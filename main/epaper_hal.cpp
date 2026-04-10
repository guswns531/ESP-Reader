#include "epaper_hal.h"

#include <cstring>

extern "C" {
#include "FreeMonoBold9pt7b.h"
}

#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_check.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace epaper {
namespace {

constexpr const char *kTag = "epaper_hal";
constexpr gpio_num_t kPinCs = GPIO_NUM_10;
constexpr gpio_num_t kPinBusy = GPIO_NUM_3;
constexpr gpio_num_t kPinRst = GPIO_NUM_46;
constexpr gpio_num_t kPinDc = GPIO_NUM_9;
constexpr gpio_num_t kPinSck = GPIO_NUM_12;
constexpr gpio_num_t kPinMosi = GPIO_NUM_11;

spi_device_handle_t s_spi;
uint8_t s_framebuffer[(kWidth * kHeight) / 8];
uint8_t s_prev_framebuffer[(kWidth * kHeight) / 8];
bool s_power_is_on;
bool s_init_display_done;
bool s_hibernating;

void setDc(int level)
{
    gpio_set_level(kPinDc, level);
}

esp_err_t spiWrite(const uint8_t *data, size_t len)
{
    if (len == 0) {
        return ESP_OK;
    }
    spi_transaction_t transaction = {};
    transaction.length = static_cast<uint32_t>(len * 8);
    transaction.tx_buffer = data;
    return spi_device_transmit(s_spi, &transaction);
}

esp_err_t sendCommand(uint8_t command)
{
    setDc(0);
    return spiWrite(&command, 1);
}

esp_err_t sendData(const uint8_t *data, size_t len)
{
    setDc(1);
    return spiWrite(data, len);
}

esp_err_t sendDataByte(uint8_t value)
{
    return sendData(&value, 1);
}

void waitReady()
{
    TickType_t start = xTaskGetTickCount();
    while (gpio_get_level(kPinBusy) == 1) {
        if ((xTaskGetTickCount() - start) > pdMS_TO_TICKS(5000)) {
            return;
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void resetPanel()
{
    gpio_set_level(kPinRst, 1);
    vTaskDelay(pdMS_TO_TICKS(10));
    gpio_set_level(kPinRst, 0);
    vTaskDelay(pdMS_TO_TICKS(2));
    gpio_set_level(kPinRst, 1);
    vTaskDelay(pdMS_TO_TICKS(10));
}

esp_err_t initSpi()
{
    spi_bus_config_t bus_config = {};
    bus_config.mosi_io_num = kPinMosi;
    bus_config.miso_io_num = -1;
    bus_config.sclk_io_num = kPinSck;
    bus_config.quadwp_io_num = -1;
    bus_config.quadhd_io_num = -1;
    bus_config.max_transfer_sz = static_cast<int>(sizeof(s_framebuffer) + 16);
    ESP_RETURN_ON_ERROR(spi_bus_initialize(SPI2_HOST, &bus_config, SPI_DMA_CH_AUTO), kTag, "spi bus");

    spi_device_interface_config_t dev_config = {};
    dev_config.clock_speed_hz = 4 * 1000 * 1000;
    dev_config.mode = 0;
    dev_config.spics_io_num = kPinCs;
    dev_config.queue_size = 1;
    return spi_bus_add_device(SPI2_HOST, &dev_config, &s_spi);
}

esp_err_t initGpio()
{
    const gpio_config_t out_config = {
        .pin_bit_mask = (1ULL << kPinDc) | (1ULL << kPinRst),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_RETURN_ON_ERROR(gpio_config(&out_config), kTag, "gpio out");

    const gpio_config_t in_config = {
        .pin_bit_mask = (1ULL << kPinBusy),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_RETURN_ON_ERROR(gpio_config(&in_config), kTag, "gpio in");

    gpio_set_level(kPinDc, 1);
    gpio_set_level(kPinRst, 1);
    return ESP_OK;
}

esp_err_t setRamArea(uint16_t x, uint16_t y, uint16_t w, uint16_t h)
{
    ESP_RETURN_ON_ERROR(sendCommand(0x11), kTag, "set ram entry mode");
    ESP_RETURN_ON_ERROR(sendDataByte(0x03), kTag, "ram entry data");

    ESP_RETURN_ON_ERROR(sendCommand(0x44), kTag, "set ram x");
    const uint8_t x_range[] = {
        static_cast<uint8_t>(x / 8),
        static_cast<uint8_t>((x + w - 1) / 8),
    };
    ESP_RETURN_ON_ERROR(sendData(x_range, sizeof(x_range)), kTag, "ram x data");

    ESP_RETURN_ON_ERROR(sendCommand(0x45), kTag, "set ram y");
    const uint8_t y_range[] = {
        static_cast<uint8_t>(y & 0xFF),
        static_cast<uint8_t>(y >> 8),
        static_cast<uint8_t>((y + h - 1) & 0xFF),
        static_cast<uint8_t>((y + h - 1) >> 8),
    };
    ESP_RETURN_ON_ERROR(sendData(y_range, sizeof(y_range)), kTag, "ram y data");

    ESP_RETURN_ON_ERROR(sendCommand(0x4E), kTag, "set x pointer");
    ESP_RETURN_ON_ERROR(sendDataByte(static_cast<uint8_t>(x / 8)), kTag, "set x pointer data");

    ESP_RETURN_ON_ERROR(sendCommand(0x4F), kTag, "set y pointer");
    const uint8_t y_pointer[] = {
        static_cast<uint8_t>(y & 0xFF),
        static_cast<uint8_t>(y >> 8),
    };
    return sendData(y_pointer, sizeof(y_pointer));
}

esp_err_t writeRegion(uint8_t command, const uint8_t *framebuffer, uint16_t x, uint16_t y, uint16_t w, uint16_t h)
{
    ESP_RETURN_ON_ERROR(setRamArea(x, y, w, h), kTag, "set partial area");
    ESP_RETURN_ON_ERROR(sendCommand(command), kTag, "write region cmd");
    setDc(1);
    for (uint16_t row = 0; row < h; ++row) {
        const size_t offset = (((static_cast<size_t>(y + row) * kWidth) + x) / 8);
        const size_t row_bytes = w / 8;
        ESP_RETURN_ON_ERROR(spiWrite(&framebuffer[offset], row_bytes), kTag, "write region data");
    }
    return ESP_OK;
}

esp_err_t powerOn()
{
    if (!s_power_is_on) {
        ESP_RETURN_ON_ERROR(sendCommand(0x22), kTag, "power on ctrl");
        ESP_RETURN_ON_ERROR(sendDataByte(0xE0), kTag, "power on ctrl data");
        ESP_RETURN_ON_ERROR(sendCommand(0x20), kTag, "power on activate");
        waitReady();
    }
    s_power_is_on = true;
    return ESP_OK;
}

esp_err_t initDisplay()
{
    if (s_hibernating) {
        resetPanel();
    } else {
        resetPanel();
    }
    vTaskDelay(pdMS_TO_TICKS(10));
    ESP_RETURN_ON_ERROR(sendCommand(0x12), kTag, "sw reset");
    vTaskDelay(pdMS_TO_TICKS(10));
    ESP_RETURN_ON_ERROR(sendCommand(0x01), kTag, "set mux");
    const uint8_t mux[] = {0x2B, 0x01, 0x00};
    ESP_RETURN_ON_ERROR(sendData(mux, sizeof(mux)), kTag, "set mux data");

    ESP_RETURN_ON_ERROR(sendCommand(0x3C), kTag, "border waveform");
    ESP_RETURN_ON_ERROR(sendDataByte(0x01), kTag, "border waveform data");

    ESP_RETURN_ON_ERROR(sendCommand(0x18), kTag, "temp sensor");
    ESP_RETURN_ON_ERROR(sendDataByte(0x80), kTag, "temp sensor data");
    ESP_RETURN_ON_ERROR(setRamArea(0, 0, kWidth, kHeight), kTag, "full ram area");
    s_init_display_done = true;
    return ESP_OK;
}

esp_err_t initPanel()
{
    s_power_is_on = false;
    s_hibernating = false;
    s_init_display_done = false;
    return initDisplay();
}

inline void setPixelRaw(int x, int y, bool black)
{
    if (x < 0 || x >= kWidth || y < 0 || y >= kHeight) {
        return;
    }
    const size_t index = ((static_cast<size_t>(y) * kWidth) + static_cast<size_t>(x)) / 8;
    const uint8_t mask = static_cast<uint8_t>(0x80 >> (x & 7));
    if (black) {
        s_framebuffer[index] &= static_cast<uint8_t>(~mask);
    } else {
        s_framebuffer[index] |= mask;
    }
}

void drawChar(int16_t x, int16_t y, unsigned char c, const GFXfont *font, bool black)
{
    if (c < font->first || c > font->last) {
        return;
    }

    const GFXglyph *glyph = &font->glyph[c - font->first];
    const uint8_t *bitmap = font->bitmap + glyph->bitmapOffset;
    uint8_t bits = 0;
    uint8_t bit_mask = 0;

    for (uint8_t yy = 0; yy < glyph->height; ++yy) {
        for (uint8_t xx = 0; xx < glyph->width; ++xx) {
            if (!(bit_mask >>= 1)) {
                bits = *bitmap++;
                bit_mask = 0x80;
            }
            if (bits & bit_mask) {
                const int16_t px = x + (glyph->xOffset + xx) * kFontScale;
                const int16_t py = y + (glyph->yOffset + yy) * kFontScale;
                drawFilledRect(px, py, kFontScale, kFontScale, black);
            }
        }
    }
}

}  // namespace

esp_err_t init()
{
    ESP_RETURN_ON_ERROR(initGpio(), kTag, "gpio");
    ESP_RETURN_ON_ERROR(initSpi(), kTag, "spi");
    ESP_RETURN_ON_ERROR(initPanel(), kTag, "panel");
    clear(false);
    std::memset(s_prev_framebuffer, 0xFF, sizeof(s_prev_framebuffer));
    return ESP_OK;
}

void clear(bool black)
{
    std::memset(s_framebuffer, black ? 0x00 : 0xFF, sizeof(s_framebuffer));
}

void drawTextLine(int x, int baseline_y, const std::string &text, bool black)
{
    const GFXfont *font = &FreeMonoBold9pt7b;
    for (char c : text) {
        const unsigned char uc = static_cast<unsigned char>(c);
        if (uc < font->first || uc > font->last) {
            x += 6 * kFontScale;
            continue;
        }
        const GFXglyph *glyph = &font->glyph[uc - font->first];
        drawChar(x, baseline_y, uc, font, black);
        x += glyph->xAdvance * kFontScale;
    }
}

void drawFilledRect(int x, int y, int w, int h, bool black)
{
    for (int yy = y; yy < y + h; ++yy) {
        for (int xx = x; xx < x + w; ++xx) {
            setPixelRaw(xx, yy, black);
        }
    }
}

void drawHLine(int x, int y, int w, bool black)
{
    drawFilledRect(x, y, w, 1, black);
}

int canvasWidth()
{
    return kWidth;
}

int canvasHeight()
{
    return kHeight;
}

int lineHeight()
{
    return FreeMonoBold9pt7b.yAdvance * kFontScale;
}

esp_err_t updateFull()
{
    if (!s_init_display_done) {
        ESP_RETURN_ON_ERROR(initDisplay(), kTag, "init display");
    }

    ESP_RETURN_ON_ERROR(powerOn(), kTag, "power on");
    ESP_RETURN_ON_ERROR(setRamArea(0, 0, kWidth, kHeight), kTag, "ram full area");

    ESP_RETURN_ON_ERROR(sendCommand(0x26), kTag, "write previous ram");
    ESP_RETURN_ON_ERROR(sendData(s_framebuffer, sizeof(s_framebuffer)), kTag, "write previous ram data");

    ESP_RETURN_ON_ERROR(setRamArea(0, 0, kWidth, kHeight), kTag, "ram full area current");
    ESP_RETURN_ON_ERROR(sendCommand(0x24), kTag, "write current ram");
    ESP_RETURN_ON_ERROR(sendData(s_framebuffer, sizeof(s_framebuffer)), kTag, "write current ram data");

    ESP_RETURN_ON_ERROR(sendCommand(0x21), kTag, "display update control");
    const uint8_t update_control[] = {0x40, 0x00};
    ESP_RETURN_ON_ERROR(sendData(update_control, sizeof(update_control)), kTag, "display update control data");

    ESP_RETURN_ON_ERROR(sendCommand(0x1A), kTag, "temperature register");
    ESP_RETURN_ON_ERROR(sendDataByte(0x6E), kTag, "temperature register data");

    ESP_RETURN_ON_ERROR(sendCommand(0x22), kTag, "update full ctrl");
    ESP_RETURN_ON_ERROR(sendDataByte(0xD7), kTag, "update full ctrl data");
    ESP_RETURN_ON_ERROR(sendCommand(0x20), kTag, "master activate");
    waitReady();
    std::memcpy(s_prev_framebuffer, s_framebuffer, sizeof(s_framebuffer));
    s_power_is_on = false;
    return ESP_OK;
}

esp_err_t updatePartial(int x, int y, int w, int h)
{
    if (!s_init_display_done) {
        ESP_RETURN_ON_ERROR(initDisplay(), kTag, "init display");
    }
    if (w <= 0 || h <= 0) {
        return ESP_OK;
    }

    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (x + w > kWidth) w = kWidth - x;
    if (y + h > kHeight) h = kHeight - y;

    x -= (x % 8);
    w -= (w % 8);
    if (w <= 0) {
        return ESP_OK;
    }

    ESP_RETURN_ON_ERROR(powerOn(), kTag, "power on partial");
    ESP_RETURN_ON_ERROR(writeRegion(0x26, s_prev_framebuffer, static_cast<uint16_t>(x), static_cast<uint16_t>(y), static_cast<uint16_t>(w), static_cast<uint16_t>(h)), kTag, "write prev region");
    ESP_RETURN_ON_ERROR(writeRegion(0x24, s_framebuffer, static_cast<uint16_t>(x), static_cast<uint16_t>(y), static_cast<uint16_t>(w), static_cast<uint16_t>(h)), kTag, "write curr region");
    ESP_RETURN_ON_ERROR(sendCommand(0x21), kTag, "partial update control");
    const uint8_t update_control[] = {0x00, 0x00};
    ESP_RETURN_ON_ERROR(sendData(update_control, sizeof(update_control)), kTag, "partial update control data");
    ESP_RETURN_ON_ERROR(sendCommand(0x22), kTag, "partial update trigger");
    ESP_RETURN_ON_ERROR(sendDataByte(0xFC), kTag, "partial update trigger data");
    ESP_RETURN_ON_ERROR(sendCommand(0x20), kTag, "partial master activate");
    waitReady();
    std::memcpy(s_prev_framebuffer, s_framebuffer, sizeof(s_framebuffer));
    s_power_is_on = true;
    return ESP_OK;
}

}  // namespace epaper
