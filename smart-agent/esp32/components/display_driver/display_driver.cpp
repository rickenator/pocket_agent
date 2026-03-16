#include "display_driver.h"
#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "driver/i2c.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char* TAG = "DisplayDriver";

// CST816D Touch controller definitions
#define CST816D_ADDR        0x15
#define CST816D_I2C_NUM     I2C_NUM_0
#define CST816D_I2C_FREQ    400000

DisplayDriver::DisplayDriver()
    : m_mode(DISPLAY_SIMULATION)
    , m_rotation(DISPLAY_ROTATION_0)
    , m_buffer(nullptr)
    , m_dirty(false)
    , m_brightness(128)
    , m_spi_dev(nullptr)
{
}

DisplayDriver::~DisplayDriver()
{
    if (m_buffer != nullptr) {
        free(m_buffer);
    }
}

esp_err_t DisplayDriver::init(display_mode_t mode)
{
    m_mode = mode;

    if (m_mode == DISPLAY_HARDWARE_QSPI) {
        // Allocate framebuffer for RGB565 (2 bytes per pixel)
        size_t buffer_size = DISPLAY_WIDTH * DISPLAY_HEIGHT * 2;
        m_buffer = (uint8_t*)heap_caps_malloc(buffer_size, MALLOC_CAP_DMA);
        if (m_buffer == nullptr) {
            ESP_LOGE(TAG, "Failed to allocate display buffer");
            return ESP_ERR_NO_MEM;
        }

        esp_err_t ret = initHardware();
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Hardware initialization failed");
            return ret;
        }

        ret = initTouch();
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "Touch initialization failed");
            // Continue without touch
        }
    } else {
        // Simulation mode - smaller buffer for testing
        m_buffer = (uint8_t*)malloc(DISPLAY_WIDTH * DISPLAY_HEIGHT / 8);
        if (m_buffer == nullptr) {
            return ESP_ERR_NO_MEM;
        }
    }

    clear();
    ESP_LOGI(TAG, "Display initialized in %s mode",
              m_mode == DISPLAY_HARDWARE_QSPI ? "QSPI" : "SIMULATION");
    return ESP_OK;
}

esp_err_t DisplayDriver::initHardware()
{
    ESP_LOGI(TAG, "Initializing SH8601 AMOLED display");

    // Configure SPI bus
    spi_bus_config_t buscfg = {
        .mosi_io_num = DISPLAY_PIN_MOSI,
        .miso_io_num = DISPLAY_PIN_MISO,
        .sclk_io_num = DISPLAY_PIN_SCK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = DISPLAY_WIDTH * DISPLAY_HEIGHT * 2 + 8,
        .flags = SPICOMMON_BUSFLAG_MASTER,
        .intr_flags = 0
    };

    esp_err_t ret = spi_bus_initialize(DISPLAY_SPI_HOST, &buscfg, SPI_DMA_CH_AUTO);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SPI bus initialization failed");
        return ret;
    }

    // Configure SPI device
    spi_device_interface_config_t devcfg = {
        .command_bits = 0,
        .address_bits = 0,
        .dummy_bits = 0,
        .mode = 0,
        .duty_cycle_pos = 0,
        .cs_ena_pretrans = 0,
        .cs_ena_posttrans = 0,
        .clock_speed_hz = DISPLAY_SPI_FREQ,
        .input_delay_ns = 0,
        .spics_io_num = DISPLAY_PIN_CS,
        .flags = SPI_DEVICE_NO_DUMMY,
        .queue_size = 7,
        .pre_cb = nullptr,
        .post_cb = nullptr
    };

    ret = spi_bus_add_device(DISPLAY_SPI_HOST, &devcfg, &m_spi_dev);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SPI device add failed");
        return ret;
    }

    // Configure control pins
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << DISPLAY_PIN_DC) | (1ULL << DISPLAY_PIN_RST),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&io_conf);

    // Hardware reset
    gpio_set_level(DISPLAY_PIN_RST, 0);
    vTaskDelay(pdMS_TO_TICKS(10));
    gpio_set_level(DISPLAY_PIN_RST, 1);
    vTaskDelay(pdMS_TO_TICKS(120));

    return initSH8601();
}

esp_err_t DisplayDriver::initSH8601()
{
    ESP_LOGI(TAG, "Configuring SH8601 controller");

    // Software reset
    writeCommand(SH8601_CMD_SWRESET);
    vTaskDelay(pdMS_TO_TICKS(120));

    // Sleep out
    writeCommand(SH8601_CMD_SLPOUT);
    vTaskDelay(pdMS_TO_TICKS(120));

    // Display on
    writeCommand(SH8601_CMD_DISPON);
    vTaskDelay(pdMS_TO_TICKS(20));

    // Set brightness
    setBrightness(m_brightness);

    ESP_LOGI(TAG, "SH8601 initialization complete");
    return ESP_OK;
}

void DisplayDriver::writeCommand(uint8_t cmd)
{
    gpio_set_level(DISPLAY_PIN_DC, 0);  // Command mode
    spi_transaction_t t = {
        .length = 8,
        .tx_buffer = &cmd,
        .rx_buffer = nullptr
    };
    spi_device_transmit(m_spi_dev, &t);
}

void DisplayDriver::writeData(uint8_t data)
{
    gpio_set_level(DISPLAY_PIN_DC, 1);  // Data mode
    spi_transaction_t t = {
        .length = 8,
        .tx_buffer = &data,
        .rx_buffer = nullptr
    };
    spi_device_transmit(m_spi_dev, &t);
}

void DisplayDriver::writeData16(uint16_t data)
{
    uint8_t buf[2] = {(uint8_t)(data >> 8), (uint8_t)(data & 0xFF)};
    gpio_set_level(DISPLAY_PIN_DC, 1);
    spi_transaction_t t = {
        .length = 16,
        .tx_buffer = buf,
        .rx_buffer = nullptr
    };
    spi_device_transmit(m_spi_dev, &t);
}

void DisplayDriver::writeDataBuffer(const uint8_t* data, size_t len)
{
    gpio_set_level(DISPLAY_PIN_DC, 1);
    spi_transaction_t t = {
        .length = len * 8,
        .tx_buffer = data,
        .rx_buffer = nullptr
    };
    spi_device_transmit(m_spi_dev, &t);
}

void DisplayDriver::setWindow(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1)
{
    // Column address set
    writeCommand(SH8601_CMD_CASET);
    writeData16(x0);
    writeData16(x1);

    // Row address set
    writeCommand(SH8601_CMD_RASET);
    writeData16(y0);
    writeData16(y1);
}

void DisplayDriver::sendFrameBuffer()
{
    if (m_mode != DISPLAY_HARDWARE_QSPI || m_buffer == nullptr) {
        return;
    }

    setWindow(0, 0, DISPLAY_WIDTH - 1, DISPLAY_HEIGHT - 1);
    writeCommand(SH8601_CMD_RAMWR);
    writeDataBuffer(m_buffer, DISPLAY_WIDTH * DISPLAY_HEIGHT * 2);
}

void DisplayDriver::clear()
{
    if (m_buffer == nullptr) {
        return;
    }

    if (m_mode == DISPLAY_HARDWARE_QSPI) {
        // Clear to black (RGB565: 0x0000)
        memset(m_buffer, 0, DISPLAY_WIDTH * DISPLAY_HEIGHT * 2);
    } else {
        memset(m_buffer, 0, DISPLAY_WIDTH * DISPLAY_HEIGHT / 8);
    }
    m_dirty = true;
}

void DisplayDriver::update()
{
    if (!m_dirty) {
        return;
    }

    switch (m_mode) {
        case DISPLAY_SIMULATION:
            printf("[DISPLAY] Update simulated display:\n");
            for (int y = 0; y < DISPLAY_HEIGHT; y += 8) {
                for (int x = 0; x < DISPLAY_WIDTH; x += 8) {
                    int idx = y * DISPLAY_WIDTH / 8 + x / 8;
                    if (idx < DISPLAY_WIDTH * DISPLAY_HEIGHT / 8) {
                        printf("%02X ", m_buffer[idx]);
                    }
                }
                printf("\n");
            }
            break;

        case DISPLAY_HARDWARE_QSPI:
            sendFrameBuffer();
            break;
    }

    m_dirty = false;
}

// Convert color to RGB565
static inline uint16_t colorToRGB565(display_color_t color) {
    switch (color) {
        case DISPLAY_COLOR_BLACK:   return 0x0000;
        case DISPLAY_COLOR_WHITE:   return 0xFFFF;
        case DISPLAY_COLOR_RED:     return 0xF800;
        case DISPLAY_COLOR_GREEN:   return 0x07E0;
        case DISPLAY_COLOR_BLUE:    return 0x001F;
        case DISPLAY_COLOR_YELLOW:  return 0xFFE0;
        case DISPLAY_COLOR_CYAN:    return 0x07FF;
        case DISPLAY_COLOR_MAGENTA: return 0xF81F;
        default:                    return 0x0000;
    }
}

void DisplayDriver::drawPixel(uint8_t x, uint8_t y, display_color_t color)
{
    if (x >= DISPLAY_WIDTH || y >= DISPLAY_HEIGHT) {
        return;
    }

    if (m_mode == DISPLAY_HARDWARE_QSPI) {
        uint16_t rgb565 = colorToRGB565(color);
        size_t idx = (y * DISPLAY_WIDTH + x) * 2;
        m_buffer[idx] = rgb565 >> 8;
        m_buffer[idx + 1] = rgb565 & 0xFF;
    } else {
        int buffer_idx = y * DISPLAY_WIDTH / 8 + x / 8;
        int bit_idx = 7 - (x % 8);
        if (color != DISPLAY_COLOR_BLACK) {
            m_buffer[buffer_idx] |= (1 << bit_idx);
        } else {
            m_buffer[buffer_idx] &= ~(1 << bit_idx);
        }
    }
    m_dirty = true;
}

void DisplayDriver::drawText(uint8_t x, uint8_t y, const char* text, display_color_t color)
{
    if (y >= DISPLAY_HEIGHT || x >= DISPLAY_WIDTH) {
        return;
    }

    // Simple text rendering - draw rectangles for characters
    int len = strlen(text);
    for (int i = 0; i < len && (x + i * 8) < DISPLAY_WIDTH; i++) {
        // Draw a simple block for each character
        for (int row = 0; row < 8 && (y + row) < DISPLAY_HEIGHT; row++) {
            for (int col = 0; col < 6 && (x + i * 8 + col) < DISPLAY_WIDTH; col++) {
                drawPixel(x + i * 8 + col, y + row, color);
            }
        }
    }
}

void DisplayDriver::drawStatus(const char* status, display_color_t color)
{
    clear();
    // Center the text
    int len = strlen(status);
    int x = DISPLAY_CENTER_X - (len * 3);
    if (x < 0) x = 0;
    drawText(x, DISPLAY_CENTER_Y - 4, status, color);
    m_dirty = true;
}

void DisplayDriver::drawLine(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, display_color_t color)
{
    int dx = abs((int)x1 - (int)x0);
    int dy = abs((int)y1 - (int)y0);
    int sx = (x0 < x1) ? 1 : -1;
    int sy = (y0 < y1) ? 1 : -1;
    int err = dx - dy;

    while (true) {
        drawPixel(x0, y0, color);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 > -dy) {
            err -= dy;
            x0 += sx;
        }
        if (e2 < dx) {
            err += dx;
            y0 += sy;
        }
    }
}

void DisplayDriver::drawRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, display_color_t color)
{
    drawLine(x, y, x + w - 1, y, color);
    drawLine(x, y + h - 1, x + w - 1, y + h - 1, color);
    drawLine(x, y, x, y + h - 1, color);
    drawLine(x + w - 1, y, x + w - 1, y + h - 1, color);
}

void DisplayDriver::fillRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, display_color_t color)
{
    for (uint16_t row = y; row < y + h && row < DISPLAY_HEIGHT; row++) {
        for (uint16_t col = x; col < x + w && col < DISPLAY_WIDTH; col++) {
            drawPixel(col, row, color);
        }
    }
}

void DisplayDriver::drawCircle(uint16_t x0, uint16_t y0, uint16_t r, display_color_t color)
{
    int x = r;
    int y = 0;
    int err = 0;

    while (x >= y) {
        drawPixel(x0 + x, y0 + y, color);
        drawPixel(x0 + y, y0 + x, color);
        drawPixel(x0 - y, y0 + x, color);
        drawPixel(x0 - x, y0 + y, color);
        drawPixel(x0 - x, y0 - y, color);
        drawPixel(x0 - y, y0 - x, color);
        drawPixel(x0 + y, y0 - x, color);
        drawPixel(x0 + x, y0 - y, color);

        if (err <= 0) {
            y += 1;
            err += 2 * y + 1;
        }
        if (err > 0) {
            x -= 1;
            err -= 2 * x + 1;
        }
    }
}

void DisplayDriver::fillCircle(uint16_t x0, uint16_t y0, uint16_t r, display_color_t color)
{
    for (int y = -r; y <= r; y++) {
        for (int x = -r; x <= r; x++) {
            if (x * x + y * y <= r * r) {
                drawPixel(x0 + x, y0 + y, color);
            }
        }
    }
}

void DisplayDriver::setBrightness(uint8_t brightness)
{
    m_brightness = brightness;
    if (m_mode == DISPLAY_HARDWARE_QSPI) {
        // SH8601 brightness command (0x51)
        writeCommand(0x51);
        writeData(brightness);
    }
}

void DisplayDriver::setRotation(display_rotation_t rotation)
{
    m_rotation = rotation;
    // SH8601 doesn't have a simple rotation command
    // Would need to adjust coordinate mapping in drawPixel
}

void DisplayDriver::displayOn()
{
    if (m_mode == DISPLAY_HARDWARE_QSPI) {
        writeCommand(SH8601_CMD_DISPON);
    }
}

void DisplayDriver::displayOff()
{
    if (m_mode == DISPLAY_HARDWARE_QSPI) {
        writeCommand(SH8601_CMD_DISPOFF);
    }
}

void DisplayDriver::sleepIn()
{
    if (m_mode == DISPLAY_HARDWARE_QSPI) {
        writeCommand(SH8601_CMD_SLPIN);
    }
}

void DisplayDriver::sleepOut()
{
    if (m_mode == DISPLAY_HARDWARE_QSPI) {
        writeCommand(SH8601_CMD_SLPOUT);
    }
}

// Touch controller implementation (CST816D)
esp_err_t DisplayDriver::initTouch()
{
    ESP_LOGI(TAG, "Initializing CST816D touch controller");

    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = TOUCH_PIN_SDA,
        .scl_io_num = TOUCH_PIN_SCL,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master = {.clk_speed = CST816D_I2C_FREQ},
        .clk_flags = 0
    };

    esp_err_t ret = i2c_param_config(CST816D_I2C_NUM, &conf);
    if (ret != ESP_OK) {
        return ret;
    }

    ret = i2c_driver_install(CST816D_I2C_NUM, conf.mode, 0, 0, 0);
    if (ret != ESP_OK) {
        return ret;
    }

    // Reset touch controller
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << TOUCH_PIN_RST),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&io_conf);

    gpio_set_level(TOUCH_PIN_RST, 0);
    vTaskDelay(pdMS_TO_TICKS(10));
    gpio_set_level(TOUCH_PIN_RST, 1);
    vTaskDelay(pdMS_TO_TICKS(100));

    ESP_LOGI(TAG, "CST816D touch controller initialized");
    return ESP_OK;
}

bool DisplayDriver::isTouchAvailable()
{
    // Check interrupt pin or poll I2C
    return true;  // Simplified
}

bool DisplayDriver::getTouch(uint16_t& x, uint16_t& y)
{
    if (m_mode == DISPLAY_SIMULATION) {
        return false;
    }

    return readTouch(x, y);
}

bool DisplayDriver::readTouch(uint16_t& x, uint16_t& y)
{
    uint8_t data[6];
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();

    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (CST816D_ADDR << 1) | I2C_MASTER_READ, true);
    i2c_master_read(cmd, data, 6, I2C_MASTER_LAST_NACK);
    i2c_master_stop(cmd);

    esp_err_t ret = i2c_master_cmd_begin(CST816D_I2C_NUM, cmd, pdMS_TO_TICKS(100));
    i2c_cmd_link_delete(cmd);

    if (ret != ESP_OK) {
        return false;
    }

    // Parse touch data
    uint8_t gesture = data[0];
    uint8_t points = data[1];

    if (points == 0) {
        return false;
    }

    x = ((data[2] & 0x0F) << 8) | data[3];
    y = ((data[4] & 0x0F) << 8) | data[5];

    return true;
}
