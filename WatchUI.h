#ifndef WATCH_UI_H
#define WATCH_UI_H

#include <lvgl.h>
#include "Arduino_GFX_Library.h"
#include <driver/i2s_std.h>
#include "ES8311Audio.h"
#include <Preferences.h>
#include "PowerManager.h"
#include "ShawnsNetwork.h"
#include "SensorPCF85063.hpp"
#include "SensorQMI8658.hpp"

extern SensorPCF85063 rtc;
extern SensorQMI8658 qmi;

static const char* dayNames[] = {
    "Sunday","Monday","Tuesday","Wednesday","Thursday","Friday","Saturday"
};
static const char* monthNames[] = {
    "", "Jan","Feb","Mar","Apr","May","Jun",
    "Jul","Aug","Sep","Oct","Nov","Dec"
};
static int rssiToPercent(int rssi) {
    if (rssi >= -50)  return 100;
    if (rssi <= -100) return 0;
    return 2 * (rssi + 100);
}

// OTA firmware URL - change this to your server address
static constexpr const char* OTA_URL = "http://192.168.1.100/firmware.bin";

class WatchUI {
public:
    lv_obj_t *system_screen;
    lv_obj_t *menu_screen;
    lv_obj_t *network_screen;
    lv_obj_t *power_screen;
    lv_obj_t *emergency_screen;
    lv_obj_t *timeset_screen;
    lv_obj_t *alarm_screen;
    lv_obj_t *sensor_screen;
    lv_obj_t *security_screen;
    lv_obj_t *music_screen;
    lv_obj_t *fall_screen;
    lv_obj_t *camera_screen;
    lv_obj_t *ai_chat_screen;
    lv_obj_t *sleep_screen;
    lv_obj_t *ota_screen;

    lv_obj_t *time_label;
    lv_obj_t *date_label;
    lv_obj_t *day_label;
    lv_obj_t *wifi_label;
    lv_obj_t *batt_label;
    lv_obj_t *batt_icon;

    lv_obj_t *ssid_box;
    lv_obj_t *pass_box;
    lv_obj_t *keyboard;
    lv_obj_t *online_btn;

    int       set_hour   = 12;
    int       set_minute = 0;
    bool      set_ampm   = false;
    lv_obj_t *set_time_label;
    lv_obj_t *set_ampm_btn;
    lv_obj_t *set_ampm_label;

    int       alarm_hour    = 7;
    int       alarm_minute  = 0;
    bool      alarm_ampm    = false;
    bool      alarm_enabled = false;
    bool      alarm_firing  = false;
    int       alarm_volume  = 50;
    lv_obj_t *alarm_time_label;
    lv_obj_t *alarm_ampm_btn;
    lv_obj_t *alarm_ampm_label;
    lv_obj_t *alarm_toggle_btn;
    lv_obj_t *alarm_toggle_label;
    lv_obj_t *alarm_volume_label;
    lv_obj_t *alarm_volume_slider;

    lv_obj_t *ota_status_label;
    lv_obj_t *ota_progress_bar;
    lv_obj_t *ota_progress_label;
    lv_obj_t *ota_update_btn;
    lv_obj_t *wifi_status_ota;

    lv_obj_t *accel_label    = nullptr;
    lv_obj_t *gyro_label     = nullptr;
    lv_obj_t *activity_label = nullptr;
    lv_obj_t *steps_label    = nullptr;
    lv_obj_t *temp_label       = nullptr;
    lv_obj_t *temp_clock_label = nullptr;

    float imu_temp = 0;  // QMI8658 die temperature °C

    // Raw IMU values
    float imu_ax = 0, imu_ay = 0, imu_az = 0;
    float imu_gx_raw = 0, imu_gy_raw = 0, imu_gz_raw = 0;
    float imu_gx = 0, imu_gy = 0, imu_gz = 0;

    // Gyro zero offsets
    float gyro_x_off = 0, gyro_y_off = 0, gyro_z_off = 0;

    // Pedometer state
    uint32_t step_count     = 0;
    float    step_speed_mph = 0;
    float    accel_prev_mag = 1.0f;
    bool     step_above     = false;
    uint32_t last_step_ms   = 0;
    uint32_t step_interval_ms = 600; // ms between steps (init to brisk walk)
    int      last_midnight_day = -1;

    // Called from loop() — stores values + runs pedometer
    void update_sensors(float ax, float ay, float az,
                        float gx, float gy, float gz, float temp) {
        imu_ax = ax; imu_ay = ay; imu_az = az;
        imu_temp = temp;
        imu_gx_raw = gx; imu_gy_raw = gy; imu_gz_raw = gz;
        imu_gx = gx - gyro_x_off;
        imu_gy = gy - gyro_y_off;
        imu_gz = gz - gyro_z_off;

        // Pedometer — peak detection on accel magnitude
        float mag = sqrtf(ax*ax + ay*ay + az*az);
        float delta = mag - 1.0f; // deviation from 1g
        const float STEP_THRESH = 0.18f;

        if (delta > STEP_THRESH && !step_above) {
            step_above = true;
            uint32_t now = millis();
            uint32_t interval = now - last_step_ms;
            if (interval > 250 && interval < 2000) { // valid step cadence
                step_count++;
                step_interval_ms = interval;
            }
            last_step_ms = now;
        } else if (delta < STEP_THRESH * 0.5f) {
            step_above = false;
        }

        // Motion detection — reset sleep/alert timer on any significant movement
        if (fabsf(mag - 1.0f) > 0.05f) last_motion_ms = millis();

        // Speed from step cadence (stride ~2.3 ft, 1 step = 0.5 stride)
        // mph = (steps/sec) * stride_ft * 3600 / 5280
        if (step_interval_ms > 0) {
            float steps_per_sec = 1000.0f / step_interval_ms;
            float stride_ft = (steps_per_sec > 2.5f) ? 4.5f : 2.3f; // running vs walking
            step_speed_mph = steps_per_sec * stride_ft * 3600.0f / 5280.0f;
        }

        // Stop showing speed if no step for 3 seconds
        if (millis() - last_step_ms > 3000) step_speed_mph = 0;
    }

    // Called by LVGL timer every 100ms
    void refresh_sensor_labels() {
        if (!accel_label || !gyro_label) return;

        char abuf[80], gbuf[80], spdbuf[48], stpbuf[32];

        snprintf(abuf, sizeof(abuf), "ACCEL (g)\nX:%+.3f  Y:%+.3f  Z:%+.3f",
            imu_ax, imu_ay, imu_az);
        snprintf(gbuf, sizeof(gbuf), "GYRO (dps)\nX:%+.2f  Y:%+.2f  Z:%+.2f",
            imu_gx, imu_gy, imu_gz);

        // Activity mode
        const char *mode;
        uint32_t idle_ms = millis() - last_step_ms;
        float    mag_var = fabsf(sqrtf(imu_ax*imu_ax + imu_ay*imu_ay + imu_az*imu_az) - 1.0f);
        if (idle_ms < 3000 && step_speed_mph > 0) {
            float sps = 1000.0f / step_interval_ms;
            mode = (sps > 2.5f) ? "RUNNING" : "WALKING";
        } else if (mag_var > 0.3f && idle_ms > 3000) {
            mode = "DRIVING";
        } else {
            mode = "STILL";
        }

        snprintf(spdbuf, sizeof(spdbuf), "%s  |  %.1f MPH", mode, step_speed_mph);
        char tmpbuf[32];
        float tempF = imu_temp * 9.0f / 5.0f + 32.0f;
        snprintf(tmpbuf, sizeof(tmpbuf), "TEMP: %.1fF  (%.1fC)", tempF, imu_temp);
        snprintf(stpbuf, sizeof(stpbuf), "STEPS: %lu", (unsigned long)step_count);

        lv_label_set_text(accel_label, abuf);
        lv_label_set_text(gyro_label, gbuf);
        if (activity_label) lv_label_set_text(activity_label, spdbuf);
        if (steps_label)    lv_label_set_text(steps_label, stpbuf);
        if (temp_label)     lv_label_set_text(temp_label, tmpbuf);
    }

    PowerManager  *pwr;
    ShawnsNetwork *net;
    Preferences    prefs;
    Arduino_GFX   *gfx;

    // Sleep management
    bool     sleeping       = false;
    uint32_t last_motion_ms = 0;
    static const uint32_t SLEEP_TIMEOUT_MS = 60000;

    void begin(PowerManager *p_ptr, ShawnsNetwork *n_ptr, Arduino_GFX *g_ptr) {
        pwr      = p_ptr;
        net      = n_ptr;
        gfx      = g_ptr;
        keyboard = nullptr;
        last_motion_ms = millis();

        prefs.begin("wifi", false);

        sleep_screen     = lv_obj_create(NULL);
        lv_obj_set_style_bg_color(sleep_screen, lv_color_hex(0x000000), 0);
        system_screen    = lv_obj_create(NULL);
        menu_screen      = lv_obj_create(NULL);
        network_screen   = lv_obj_create(NULL);
        power_screen     = lv_obj_create(NULL);
        emergency_screen = lv_obj_create(NULL);
        timeset_screen   = lv_obj_create(NULL);
        alarm_screen     = lv_obj_create(NULL);
        sensor_screen    = lv_obj_create(NULL);
        security_screen  = lv_obj_create(NULL);
        music_screen     = lv_obj_create(NULL);
        fall_screen      = lv_obj_create(NULL);
        camera_screen    = lv_obj_create(NULL);
        ai_chat_screen   = lv_obj_create(NULL);
        ota_screen       = lv_obj_create(NULL);

        setupSystem();
        setupMenu();
        setupNetwork();
        setupPower();
        setupEmergency();
        setupTimeSet();
        setupAlarm();
        setupSensors();
        setupOTA();

        // Load alarm settings from NVS (set from web control panel)
        Preferences alarm_prefs;
        alarm_prefs.begin("alarm", true);  // read-only
        alarm_hour = alarm_prefs.getInt("hour", 7);
        alarm_minute = alarm_prefs.getInt("minute", 0);
        alarm_ampm = alarm_prefs.getBool("ampm", false);
        alarm_enabled = alarm_prefs.getBool("enabled", false);
        alarm_volume = alarm_prefs.getInt("volume", 50);
        alarm_prefs.end();
        Serial.printf("Loaded alarm from NVS: %d:%02d %s, %s, Vol:%d%%\n",
                      alarm_hour, alarm_minute, alarm_ampm ? "PM" : "AM",
                      alarm_enabled ? "ON" : "OFF", alarm_volume);
        
        // Update alarm display with loaded values
        if (alarm_time_label) {
            lv_label_set_text_fmt(alarm_time_label, "%d:%02d", alarm_hour, alarm_minute);
        }
        if (alarm_toggle_label) {
            lv_label_set_text(alarm_toggle_label, alarm_enabled ? "ON" : "OFF");
        }
        if (alarm_volume_slider) {
            lv_slider_set_value(alarm_volume_slider, alarm_volume, LV_ANIM_OFF);
        }
        if (alarm_volume_label) {
            lv_label_set_text_fmt(alarm_volume_label, "Volume: %d%%", alarm_volume);
        }

        Serial.printf("[INIT] sensor_screen=%p  accel_label=%p  gyro_label=%p\n",
            sensor_screen, accel_label, gyro_label);

        lv_timer_create([](lv_timer_t *t) {
            ((WatchUI *)lv_timer_get_user_data(t))->refresh_sensor_labels();
        }, 100, this);

        setupPlaceholder(security_screen, LV_SYMBOL_EDIT  "  Security");
        setupPlaceholder(music_screen,    LV_SYMBOL_AUDIO "  Music");
        setupPlaceholder(fall_screen,     LV_SYMBOL_CALL  "  Fall Detection");
        setupPlaceholder(camera_screen,   LV_SYMBOL_IMAGE "  Cameras");
        setupPlaceholder(ai_chat_screen,  LV_SYMBOL_CALL  "  AI Chat");

        es8311_begin();
        lv_screen_load(system_screen);
    }

    void update_ui_task(lv_timer_t */*timer*/) {
        // Check sleep timeout — use black screen overlay, NOT displayOff()
        // displayOff() kills touch controller on CO5300 AMOLED
        if (!sleeping && (millis() - last_motion_ms > SLEEP_TIMEOUT_MS)) {
            sleeping = true;
            lv_screen_load(sleep_screen);
            return;
        }
        if (sleeping) return;

        RTC_DateTime dt = rtc.getDateTime();

        int hr = dt.getHour() % 12;
        if (hr == 0) hr = 12;
        lv_label_set_text_fmt(time_label, "%02d:%02d", hr, dt.getMinute());

        uint8_t mon = dt.getMonth();
        if (mon < 1 || mon > 12) mon = 1;
        lv_label_set_text_fmt(date_label, "%s %d, %d",
                              monthNames[mon], dt.getDay(), dt.getYear());

        {
            int y = dt.getYear(), m = dt.getMonth(), d = dt.getDay();
            if (m < 3) { m += 12; y--; }
            int k = y % 100, j = y / 100;
            int dow = (d + (13*(m+1))/5 + k + k/4 + j/4 + 5*j) % 7;
            dow = ((dow + 6) % 7 + 7) % 7;
            lv_label_set_text(day_label, dayNames[dow]);
        }

        int batt = pwr->getBatteryPercent();
        lv_label_set_text_fmt(batt_label, "%d%%", batt);
        lv_color_t bc = (batt >= 50) ? lv_color_hex(0x00FF00)
                      : (batt >= 20) ? lv_color_hex(0xFFFF00)
                                     : lv_color_hex(0xFF2222);
        lv_obj_set_style_text_color(batt_icon,  bc, 0);
        lv_obj_set_style_text_color(batt_label, bc, 0);

        if (WiFi.status() == WL_CONNECTED) {
            int rssi = WiFi.RSSI();
            int pct  = rssiToPercent(rssi);
            lv_color_t wc = (pct >= 60) ? lv_color_hex(0x00FF00)
                          : (pct >= 30) ? lv_color_hex(0xFFFF00)
                                        : lv_color_hex(0xFF4444);
            lv_label_set_text_fmt(wifi_label, LV_SYMBOL_WIFI " %d%%", pct);
            lv_obj_set_style_text_color(wifi_label, wc, 0);
        } else {
            lv_label_set_text(wifi_label, LV_SYMBOL_WIFI " Off");
            lv_obj_set_style_text_color(wifi_label, lv_color_hex(0xFF4444), 0);
        }

        if (online_btn) {
            bool up = (WiFi.status() == WL_CONNECTED);
            if (!up && net->serverStarted) net->serverStarted = false;
            lv_obj_t *lbl = lv_obj_get_child(online_btn, 0);
            lv_label_set_text(lbl, up ? "GO OFFLINE" : "GO ONLINE");
            lv_obj_set_style_bg_color(online_btn,
                up ? lv_color_hex(0x1A8C1A) : lv_color_hex(0x444444), 0);
        }

        // Auto reset steps at midnight
        int today = dt.getDay();
        if (dt.getHour() == 0 && dt.getMinute() == 0 && today != last_midnight_day) {
            last_midnight_day = today;
            step_count = 0;
            Serial.println("Steps auto-reset at midnight.");
        }

        // Update temp on clock face
        if (temp_clock_label) {
            char tcbuf[20];
            float tempF = imu_temp * 9.0f / 5.0f + 32.0f;
            snprintf(tcbuf, sizeof(tcbuf), "%.0fF / %.0fC", tempF, imu_temp);
            lv_label_set_text(temp_clock_label, tcbuf);
        }

        // Update OTA WiFi status
        updateOTAWiFiStatus();

        checkAlarm(dt.getHour(), dt.getMinute(), dt.getSecond());
    }

private:
    #include "Screen_placeholder.h"
    #include "Screen_clock.h"
    #include "Screen_menu.h"
    #include "Screen_network.h"
    #include "Screen_power.h"
    #include "Screen_emergency.h"
    #include "Screen_timeset.h"
    #include "Screen_alarm.h"
    #include "Screen_sensors.h"
    #include "Screen_OTA.h"
};

// ═══════════════════════════════════════════════════════════════════════════
// OTAUpdater Implementation (must come AFTER WatchUI is fully defined)
// ═══════════════════════════════════════════════════════════════════════════

inline void OTAUpdater::updateStatus(WatchUI* ui, const char* message, uint32_t color) {
    if (!ui || !ui->ota_status_label) return;
    lv_label_set_text(ui->ota_status_label, message);
    lv_obj_set_style_text_color(ui->ota_status_label, lv_color_hex(color), 0);
}

inline void OTAUpdater::updateProgress(WatchUI* ui, int progress) {
    if (!ui || !ui->ota_progress_bar) return;
    lv_bar_set_value(ui->ota_progress_bar, progress, LV_ANIM_OFF);
    lv_label_set_text_fmt(ui->ota_progress_label, "%d%%", progress);
}

inline void OTAUpdater::performUpdate(WatchUI* ui, const char* url) {
    if (!ui) return;
    
    // Check WiFi
    if (WiFi.status() != WL_CONNECTED) {
        updateStatus(ui, "WiFi not connected!", 0xFF4444);
        return;
    }
    
    // Disable update button
    lv_obj_add_state(ui->ota_update_btn, LV_STATE_DISABLED);
    
    // Show progress elements
    lv_obj_clear_flag(ui->ota_progress_bar, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(ui->ota_progress_label, LV_OBJ_FLAG_HIDDEN);
    
    updateStatus(ui, "Connecting...", 0xFFFF00);
    updateProgress(ui, 0);
    
    HTTPClient http;
    http.begin(url);
    http.setTimeout(10000); // 10 second timeout
    
    int httpCode = http.GET();
    
    if (httpCode != HTTP_CODE_OK) {
        char buf[64];
        snprintf(buf, sizeof(buf), "HTTP error: %d", httpCode);
        updateStatus(ui, buf, 0xFF4444);
        http.end();
        lv_obj_clear_state(ui->ota_update_btn, LV_STATE_DISABLED);
        return;
    }
    
    int contentLength = http.getSize();
    if (contentLength <= 0) {
        updateStatus(ui, "Invalid file size", 0xFF4444);
        http.end();
        lv_obj_clear_state(ui->ota_update_btn, LV_STATE_DISABLED);
        return;
    }
    
    if (!Update.begin(contentLength)) {
        char buf[64];
        snprintf(buf, sizeof(buf), "Not enough space: %d bytes", contentLength);
        updateStatus(ui, buf, 0xFF4444);
        http.end();
        lv_obj_clear_state(ui->ota_update_btn, LV_STATE_DISABLED);
        return;
    }
    
    updateStatus(ui, "Downloading...", 0x00FFFF);
    
    WiFiClient* stream = http.getStreamPtr();
    uint8_t buff[512];
    int downloaded = 0;
    int lastProgress = 0;
    
    while (http.connected() && downloaded < contentLength) {
        size_t available = stream->available();
        if (available) {
            int c = stream->readBytes(buff, min(available, sizeof(buff)));
            if (Update.write(buff, c) != c) {
                updateStatus(ui, "Write failed", 0xFF4444);
                Update.abort();
                http.end();
                lv_obj_clear_state(ui->ota_update_btn, LV_STATE_DISABLED);
                return;
            }
            downloaded += c;
            
            int progress = (downloaded * 100) / contentLength;
            if (progress != lastProgress) {
                updateProgress(ui, progress);
                lastProgress = progress;
            }
        }
        delay(1);
    }
    
    if (Update.end(true)) {
        updateStatus(ui, "Update complete! Rebooting...", 0x00FF00);
        updateProgress(ui, 100);
        delay(2000);
        ESP.restart();
    } else {
        char buf[128];
        snprintf(buf, sizeof(buf), "Update failed: %s", Update.errorString());
        updateStatus(ui, buf, 0xFF4444);
        lv_obj_clear_state(ui->ota_update_btn, LV_STATE_DISABLED);
    }
    
    http.end();
}

#endif // WATCH_UI_H