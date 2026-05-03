#pragma once

/* 
 * =================================================================
 * PROJECT: KENNEDY_C6_AMOLED_LVGL
 * FILE: pin_config.h
 * AUTHOR: Shawn M. Kennedy
 * =================================================================
 */

#define XPOWERS_CHIP_AXP2101

/* Display Dimensions */
#define LCD_WIDTH       410
#define LCD_HEIGHT      502
#define LCD_OFFSET_Y    22

/* QSPI Display Pins */
#define LCD_CS          5
#define LCD_SCLK        0
#define LCD_SDIO0       1
#define LCD_SDIO1       2
#define LCD_SDIO2       3
#define LCD_SDIO3       4
#define LCD_RESET       11

/* Power Management (PMU) I2C Pins */
#define IIC_SDA         8   
#define IIC_SCL         7   

/* Touch Controller I2C & Control Pins */
#define TP_IIC_SDA      15
#define TP_IIC_SCL      14
#define TP_INT          38
#define TP_RESET        9

/* SD Card Pins */
const int SDMMC_CLK  = 2;
const int SDMMC_CMD  = 1;
const int SDMMC_DATA = 3;
const int SDMMC_CS   = 17;