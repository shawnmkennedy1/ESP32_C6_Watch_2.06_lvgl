/* 
 * =================================================================
 * PROJECT: KENNEDY_C6_AMOLED_LVGL
 * FILE REVISION: 1.0 (V9.3 API Fix)
 * DATE: May 01, 2026
 * AUTHOR: Shawn M. Kennedy
 * 
 * CHANGES IN THIS REV (1.0):
 * 1. LVGL v9 API: Updated disp_drv and draw_buf types.
 * 2. FLUSH: Updated signature for lv_display_t.
 * 3. DIMENSIONS: 410x502 (Offset 22).
 * =================================================================
 */

#include <Arduino.h> 
#include <Arduino_GFX_Library.h>
#include <XPowersLib.h> 
#include <Wire.h>
#include <lvgl.h>

/* Display Pins */
#define IIC_SDA         8   
#define IIC_SCL         7   
#define LCD_CS          5
#define LCD_SCLK        0
#define LCD_SDIO0       1
#define LCD_SDIO1       2
#define LCD_SDIO2       3
#define LCD_SDIO3       4
#define LCD_RESET       11
#define LCD_WIDTH       410
#define LCD_HEIGHT      502

/* LVGL Buffers */
static const uint32_t screenWidth  = LCD_WIDTH;
static const uint32_t screenHeight = LCD_HEIGHT;

// V9 CHANGE: Type is now lv_display_t and buffers are handled differently
static uint8_t buf[screenWidth * 40 * sizeof(lv_color_t)]; 

Arduino_DataBus *bus = new Arduino_ESP32QSPI(
    LCD_CS, LCD_SCLK, LCD_SDIO0, LCD_SDIO1, LCD_SDIO2, LCD_SDIO3
);

Arduino_GFX *gfx = new Arduino_CO5300(
    bus, LCD_RESET, 0, false, LCD_WIDTH, LCD_HEIGHT, 22, 0, 0
);

XPowersAXP2101 PMU;

/* V9 CHANGE: Flush function signature changed */
void my_disp_flush(lv_display_t * disp, const lv_area_t * area, uint8_t * px_map) {
    uint32_t w = (area->x2 - area->x1 + 1);
    uint32_t h = (area->y2 - area->y1 + 1);
    
    // px_map is already a pointer to the colors
    gfx->draw16bitBeRGBBitmap(area->x1, area->y1, (uint16_t *)px_map, w, h);
    
    lv_display_flush_ready(disp);
}

void setup() {
    Serial.begin(115200);
    delay(2000);
    Serial.println("\n--- STARTING LVGL REV 1.0 (V9 API) ---");

    // 1. Power Initialization
    Wire.begin(IIC_SDA, IIC_SCL);
    if (PMU.begin(Wire, 0x34, IIC_SDA, IIC_SCL)) {
        PMU.setALDO1Voltage(3300); PMU.enableALDO1();
        PMU.setALDO2Voltage(3300); PMU.enableALDO2();
        PMU.setBLDO1Voltage(1800); PMU.enableBLDO1();
        Serial.println("Power: OK");
    }

    // 2. Display Initialization
    if (!gfx->begin()) {
        Serial.println("GFX: Failed");
    }

    // 3. LVGL Initialization
    lv_init();

    // V9 CHANGE: Create a display object and set its properties
    lv_display_t * disp = lv_display_create(screenWidth, screenHeight);
    lv_display_set_flush_cb(disp, my_disp_flush);
    lv_display_set_buffers(disp, buf, NULL, sizeof(buf), LV_DISPLAY_RENDER_MODE_PARTIAL);

    /* Simple LVGL UI Test */
    lv_obj_t * label = lv_label_create(lv_screen_active());
    lv_label_set_text(label, "SHAWN: LVGL V9 ACTIVE");
    lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);
    
    Serial.println("LVGL: Setup Complete");
}

void loop() {
    // V9 CHANGE: Use lv_tick_inc to tell LVGL how much time has passed
    // or rely on a timer. For a simple loop, we'll use a millis tracker.
    static uint32_t last_tick = 0;
    if (millis() - last_tick > 5) {
        lv_tick_inc(millis() - last_tick);
        last_tick = millis();
    }

    lv_timer_handler(); 
    delay(5);
}