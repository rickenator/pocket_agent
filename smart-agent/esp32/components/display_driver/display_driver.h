#ifndef DISPLAY_DRIVER_H
#define DISPLAY_DRIVER_H

#include <stdint.h>
#include <string.h>
#include "esp_err.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"

// Waveshare ESP32-S3-Touch-AMOLED-1.75C Display Specifications
// Display: 1.75" Round AMOLED, 466x466 pixels
// Driver: SH8601
// Interface: QSPI
// Touch: CST816D (I2C)

#define DISPLAY_WIDTH  466
#define DISPLAY_HEIGHT 466
#define DISPLAY_CENTER_X (DISPLAY_WIDTH / 2)
#define DISPLAY_CENTER_Y (DISPLAY_HEIGHT / 2)

// Pin definitions for ESP32-S3-Touch-AMOLED-1.75C
#define DISPLAY_PIN_SCK    GPIO_NUM_47
#define DISPLAY_PIN_MOSI   GPIO_NUM_48
#define DISPLAY_PIN_MISO   GPIO_NUM_41  // For QSPI D1
#define DISPLAY_PIN_DC     GPIO_NUM_40  // Data/Command
#define DISPLAY_PIN_CS     GPIO_NUM_38  // Chip Select
#define DISPLAY_PIN_RST    GPIO_NUM_39  // Reset
#define DISPLAY_PIN_TE     GPIO_NUM_9   // Tearing Effect

// Touch panel pins (CST816D)
#define TOUCH_PIN_SDA      GPIO_NUM_8
#define TOUCH_PIN_SCL      GPIO_NUM_7
#define TOUCH_PIN_INT      GPIO_NUM_4
#define TOUCH_PIN_RST      GPIO_NUM_5

// SPI configuration
#define DISPLAY_SPI_HOST   SPI3_HOST
#define DISPLAY_SPI_FREQ   40000000  // 40MHz for QSPI

// SH8601 commands
#define SH8601_CMD_NOP     0x00
#define SH8601_CMD_SWRESET 0x01
#define SH8601_CMD_SLPIN   0x10
#define SH8601_CMD_SLPOUT  0x11
#define SH8601_CMD_DISPON  0x29
#define SH8601_CMD_DISPOFF 0x28
#define SH8601_CMD_CASET   0x2A
#define SH8601_CMD_RASET   0x2B
#define SH8601_CMD_RAMWR   0x2C

// Display colors
typedef enum {
    DISPLAY_COLOR_BLACK = 0x00,
    DISPLAY_COLOR_WHITE = 0x01,
    DISPLAY_COLOR_RED = 0x02,
    DISPLAY_COLOR_GREEN = 0x03,
    DISPLAY_COLOR_BLUE = 0x04,
    DISPLAY_COLOR_YELLOW = 0x05,
    DISPLAY_COLOR_CYAN = 0x06,
    DISPLAY_COLOR_MAGENTA = 0x07,
} display_color_t;

// Display modes
typedef enum {
    DISPLAY_SIMULATION,
    DISPLAY_HARDWARE_QSPI,  // QSPI for AMOLED
} display_mode_t;

// Display rotation
typedef enum {
    DISPLAY_ROTATION_0,
    DISPLAY_ROTATION_90,
    DISPLAY_ROTATION_180,
    DISPLAY_ROTATION_270
} display_rotation_t;

// Display driver class
class DisplayDriver {
public:
    DisplayDriver();
    ~DisplayDriver();

    esp_err_t init(display_mode_t mode);
    void clear();
    void update();

    // Drawing functions
    void drawText(uint16_t x, uint16_t y, const char* text, display_color_t color);
    void drawStatus(const char* status, display_color_t color);
    void drawPixel(uint16_t x, uint16_t y, display_color_t color);
    void drawLine(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, display_color_t color);
    void drawRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, display_color_t color);
    void fillRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, display_color_t color);
    void drawCircle(uint16_t x, uint16_t y, uint16_t r, display_color_t color);
    void fillCircle(uint16_t x, uint16_t y, uint16_t r, display_color_t color);

    // Hardware control
    void setBrightness(uint8_t brightness);  // 0-255
    void setRotation(display_rotation_t rotation);
    void displayOn();
    void displayOff();
    void sleepIn();
    void sleepOut();

    // Touch interface (CST816D)
    bool isTouchAvailable();
    bool getTouch(uint16_t& x, uint16_t& y);

private:
    display_mode_t m_mode;
    display_rotation_t m_rotation;
    uint8_t* m_buffer;  // RGB565 buffer for AMOLED
    bool m_dirty;
    uint8_t m_brightness;

    // SPI/QSPI handles
    spi_device_handle_t m_spi_dev;
    spi_bus_config_t m_bus_config;
    spi_device_interface_config_t m_dev_config;

    // Hardware functions
    esp_err_t initHardware();
    esp_err_t initSH8601();
    void writeCommand(uint8_t cmd);
    void writeData(uint8_t data);
    void writeData16(uint16_t data);
    void writeDataBuffer(const uint8_t* data, size_t len);
    void setWindow(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1);
    void sendFrameBuffer();

    // Touch functions
    esp_err_t initTouch();
    bool readTouch(uint16_t& x, uint16_t& y);
};

#endif // DISPLAY_DRIVER_H