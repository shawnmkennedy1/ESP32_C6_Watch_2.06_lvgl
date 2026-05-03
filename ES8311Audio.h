#pragma once
// ES8311Audio.h — ES8311 codec init via Arduino Wire + IDF I2S v5 beep player
// For ESP32-C6-Touch-AMOLED-2.06 (Waveshare)
//
// USAGE:
//   1. Call es8311_begin() from setup() after Wire.begin()
//   2. Call playAlarmBeeps() from the FreeRTOS alarm task (I2S only, no Wire)
//   3. Call stopAlarm() from any thread to cancel mid-beep
//
// Pins:  I2S MCLK=19  BCLK=20  LRCK=22  DOUT=23   PA_CTRL=GPIO6

#include <Wire.h>
#include <driver/i2s_std.h>
#include <math.h>

#define ES8311_ADDR      0x18

// Register map
#define ES_REG00_RESET   0x00
#define ES_REG01_CLK     0x01
#define ES_REG02_CLK     0x02
#define ES_REG03_CLK     0x03
#define ES_REG04_CLK     0x04
#define ES_REG05_CLK     0x05
#define ES_REG06_CLK     0x06
#define ES_REG07_CLK     0x07
#define ES_REG08_CLK     0x08
#define ES_REG09_SDPIN   0x09
#define ES_REG0A_SDPOUT  0x0A
#define ES_REG0D_SYS     0x0D
#define ES_REG0E_SYS     0x0E
#define ES_REG12_SYS     0x12
#define ES_REG13_SYS     0x13
#define ES_REG14_SYS     0x14
#define ES_REG16_ADC     0x16
#define ES_REG17_ADC     0x17
#define ES_REG1C_ADC     0x1C
#define ES_REG31_DAC     0x31
#define ES_REG32_DAC     0x32
#define ES_REG37_DAC     0x37

static bool          _es8311_ready = false;
static volatile bool _alarm_stop   = false;  // set true to cancel alarm early

static bool _es_write(uint8_t reg, uint8_t val) {
    Wire.beginTransmission(ES8311_ADDR);
    Wire.write(reg);
    Wire.write(val);
    return Wire.endTransmission() == 0;
}

// ── I2C bus scan helper ───────────────────────────────────────────────────────
static void i2c_scan() {
    Serial.println("=== I2C scan ===");
    int found = 0;
    for (uint8_t addr = 1; addr < 127; addr++) {
        Wire.beginTransmission(addr);
        if (Wire.endTransmission() == 0) {
            Serial.printf("  device at 0x%02X\n", addr);
            found++;
        }
    }
    if (!found) Serial.println("  none found");
    Serial.println("================");
}

// ── One-time codec init — call from setup() / begin() on main thread ──────────
static bool es8311_begin() {
    Serial.println("ES8311: probing...");
    Serial.flush();
    Wire.setTimeOut(1000);
    i2c_scan();

    Wire.beginTransmission(ES8311_ADDR);
    uint8_t err = Wire.endTransmission();
    if (err != 0) {
        Serial.printf("ES8311: NOT found at 0x%02X (err=%d)\n", ES8311_ADDR, err);
        return false;
    }
    Serial.println("ES8311: found");

    _es_write(ES_REG00_RESET, 0x1F); delay(10);
    _es_write(ES_REG00_RESET, 0x00); delay(10);

    _es_write(ES_REG01_CLK,  0x3F);
    _es_write(ES_REG02_CLK,  0x00);
    _es_write(ES_REG03_CLK,  0x20);
    _es_write(ES_REG04_CLK,  0x20);
    _es_write(ES_REG05_CLK,  0x00);
    _es_write(ES_REG06_CLK,  0x07);
    _es_write(ES_REG07_CLK,  0x00);
    _es_write(ES_REG08_CLK,  0xFF);

    _es_write(ES_REG09_SDPIN,  0x0C);
    _es_write(ES_REG0A_SDPOUT, 0x0C);

    _es_write(ES_REG0D_SYS, 0x01);
    _es_write(ES_REG0E_SYS, 0x02);
    _es_write(ES_REG12_SYS, 0x00);
    _es_write(ES_REG13_SYS, 0x10);
    _es_write(ES_REG1C_ADC, 0x6A);
    _es_write(ES_REG37_DAC, 0x08);
    _es_write(ES_REG14_SYS, 0x1A);
    _es_write(ES_REG16_ADC, 0x00);
    _es_write(ES_REG17_ADC, 0xC8);
    _es_write(ES_REG32_DAC, 0xBF);
    _es_write(ES_REG31_DAC, 0x00);
    _es_write(ES_REG00_RESET, 0x80); delay(20);

    _es8311_ready = true;
    Serial.println("ES8311: ready");
    return true;
}

// ── Cancel a running alarm from any thread ────────────────────────────────────
static void stopAlarm() {
    _alarm_stop = true;
}

// ── Alarm beeps — runs for 60 seconds or until stopAlarm() called ─────────────
// I2S only — no Wire — safe to call from FreeRTOS task
static void playAlarmBeeps() {
    if (!_es8311_ready) {
        Serial.println("ES8311: not ready");
        return;
    }

    _alarm_stop = false;  // reset stop flag

    const int   SAMPLE_RATE    = 16000;
    const float FREQ_HZ        = 880.0f;
    const int   AMPLITUDE      = 8000;
    const int   ON_SAMP        = SAMPLE_RATE / 5;   // 200 ms on
    const int   OFF_SAMP       = SAMPLE_RATE / 10;  // 100 ms off
    const int   PA_PIN         = 6;                  // PA_CTRL = GPIO6
    const uint32_t ALARM_MS    = 60000;              // 1 minute total

    i2s_chan_handle_t tx;
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_AUTO, I2S_ROLE_MASTER);
    if (i2s_new_channel(&chan_cfg, &tx, NULL) != ESP_OK) {
        Serial.println("ES8311: i2s_new_channel failed"); return;
    }

    i2s_std_clk_config_t clk;
    clk.sample_rate_hz = SAMPLE_RATE;
    clk.clk_src        = I2S_CLK_SRC_DEFAULT;
    clk.mclk_multiple  = I2S_MCLK_MULTIPLE_256;

    i2s_std_slot_config_t slot =
        I2S_STD_PHILIP_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO);

    i2s_std_gpio_config_t gpio_cfg;
    memset(&gpio_cfg, 0, sizeof(gpio_cfg));
    gpio_cfg.mclk = (gpio_num_t)19;
    gpio_cfg.bclk = (gpio_num_t)20;
    gpio_cfg.ws   = (gpio_num_t)22;
    gpio_cfg.dout = (gpio_num_t)23;
    gpio_cfg.din  = I2S_GPIO_UNUSED;

    i2s_std_config_t std_cfg = { clk, slot, gpio_cfg };
    if (i2s_channel_init_std_mode(tx, &std_cfg) != ESP_OK) {
        Serial.println("ES8311: init_std_mode failed");
        i2s_del_channel(tx); return;
    }
    i2s_channel_enable(tx);

    pinMode(PA_PIN, OUTPUT);
    digitalWrite(PA_PIN, HIGH);
    delay(20);

    const TickType_t WTO = pdMS_TO_TICKS(200);
    int16_t buf[128];
    size_t  written;
    uint32_t startMs = millis();

    // Loop beep pattern for ALARM_MS milliseconds or until stopped
    while (!_alarm_stop && (millis() - startMs < ALARM_MS)) {
        // 200 ms tone
        for (int s = 0; s < ON_SAMP && !_alarm_stop; s += 128) {
            int n = ((ON_SAMP - s) < 128) ? (ON_SAMP - s) : 128;
            for (int i = 0; i < n; i++)
                buf[i] = (int16_t)(AMPLITUDE * sinf(2.f * M_PI * FREQ_HZ * (s+i) / SAMPLE_RATE));
            i2s_channel_write(tx, buf, n * 2, &written, WTO);
        }
        // 100 ms silence
        memset(buf, 0, sizeof(buf));
        for (int s = 0; s < OFF_SAMP && !_alarm_stop; s += 128) {
            int n = ((OFF_SAMP - s) < 128) ? (OFF_SAMP - s) : 128;
            i2s_channel_write(tx, buf, n * 2, &written, WTO);
        }
        // 400 ms pause between groups of 3
        static int beepCount = 0;
        if (++beepCount % 3 == 0) {
            vTaskDelay(pdMS_TO_TICKS(400));
        }
    }

    digitalWrite(PA_PIN, LOW);
    i2s_channel_disable(tx);
    i2s_del_channel(tx);
    _alarm_stop = false;
    Serial.println("ES8311: alarm ended");
}