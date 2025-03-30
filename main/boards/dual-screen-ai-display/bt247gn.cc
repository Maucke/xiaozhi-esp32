/*
 * Author: 施华锋
 * Date: 2025-02-12
 * Description: This file implements the methods of the BT247GN class for communicating with the BT247GN device via SPI.
 */

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "bt247gn.h"
#include "string.h"

#define TAG "BT247GN"
// 0x00-0x0D 原始数据，可带最多9*13参数，地址自增
// 0x20-0x3F 原始点阵数据，bit 4-2:x, bit 0:y 后须带5的倍数数据，地址自增
// 0x40-0x5F 原始数字数据，bit 4-0:x，地址自增
// 0x60 图标，byte1位置，共49个，地址自增
// 0x80-0x9F 数字ASCII，bit 4-1:x 地址自增
// 0xA0-0xAF 点阵ASCII，bit 4-2:x, bit 0:y ，地址自增
// 0xB0-0xBF 命令
// 0xB0 dimming: 0-100
void BT247GN::write_data8(uint8_t *dat, int len)
{
    // Create an SPI transaction structure and initialize it to zero
    spi_transaction_t t;
    memset(&t, 0, sizeof(t));

    // Set the length of the transaction in bits
    t.length = len * 8;

    // Set the pointer to the data buffer to be transmitted
    t.tx_buffer = dat;

    // Queue the SPI transaction. This function will block until the transaction can be queued.
    ESP_ERROR_CHECK(spi_device_queue_trans(spi_device_, &t, portMAX_DELAY));

    // The following code can be uncommented if you need to wait for the transaction to complete and verify the result
    spi_transaction_t *ret_trans;
    ESP_ERROR_CHECK(spi_device_get_trans_result(spi_device_, &ret_trans, portMAX_DELAY));
    assert(ret_trans == &t);
    vTaskDelay(pdMS_TO_TICKS(10));

    return;
}

BT247GN::BT247GN(gpio_num_t din, gpio_num_t clk, gpio_num_t cs, spi_host_device_t spi_num)
{
    // Initialize the SPI bus configuration structure
    spi_bus_config_t buscfg = {0};

    // Log the initialization process
    ESP_LOGI(TAG, "Initialize BT247GN SPI bus");

    // Set the clock and data pins for the SPI bus
    buscfg.sclk_io_num = clk;
    buscfg.data0_io_num = din;

    // Set the maximum transfer size in bytes
    buscfg.max_transfer_sz = 256;

    // Initialize the SPI bus with the specified configuration
    ESP_ERROR_CHECK(spi_bus_initialize(spi_num, &buscfg, SPI_DMA_CH_AUTO));

    // Initialize the SPI device interface configuration structure
    spi_device_interface_config_t devcfg = {
        .mode = 0,                  // Set the SPI mode to 3
        .clock_speed_hz = 1000000, // Set the clock speed to 1MHz
        .spics_io_num = cs,         // Set the chip select pin
        .queue_size = 7,
    };

    // Add the BT247GN device to the SPI bus with the specified configuration
    ESP_ERROR_CHECK(spi_bus_add_device(spi_num, &devcfg, &spi_device_));
    init();
    ESP_LOGI(TAG, "BT247GN Initalized");
}

BT247GN::BT247GN(spi_device_handle_t spi_device) : spi_device_(spi_device)
{
    init();
    ESP_LOGI(TAG, "BT247GN Initalized");
}

void BT247GN::init()
{
    for (size_t i = 0; i < PIXEL_COUNT; i++)
    {
        currentPixelData[i].current_content = ' ';
        currentPixelData[i].last_content = ' ';
        currentPixelData[i].animation_type = LEFT2RT;
        tempPixelData[i].current_content = ' ';
        tempPixelData[i].last_content = ' ';
        tempPixelData[i].animation_type = LEFT2RT;
    }
    for (size_t i = 0; i < PIXEL_COUNT; i++)
    {
        currentNumData[i].current_content = ' ';
        currentNumData[i].last_content = ' ';
        currentNumData[i].animation_type = LEFT2RT;
    }

    xTaskCreate(
        [](void *arg)
        {
            BT247GN *vfd = static_cast<BT247GN *>(arg);
            while (true)
            {
                vfd->refrash();
                vTaskDelay(pdMS_TO_TICKS(10));
                vfd->pixelanimate();
                vfd->numberanimate();
            }
            vTaskDelete(NULL);
        },
        "vfd",
        4096,
        this,
        6,
        nullptr);
}

void BT247GN::test()
{
    static bool face = false;
    for (size_t i = 1; i < sizeof internal_gram; i++)
    {
        if (face)
            internal_gram[i] = rand();
        else
            internal_gram[i] = 0xFF;
    }
    face = !face;
    refrash();
}

void BT247GN::setbrightness(uint8_t brightness)
{
    dimming = brightness;
}

void BT247GN::setsleep(bool en)
{
    if (en)
    {
        memset(internal_gram, 0, sizeof internal_gram);
        // refrash();
    }
}
void BT247GN::noti_show(int start, char *buf, int size, NumAni ani, int timeout)
{
    content_inhibit_time = esp_timer_get_time() / 1000 + timeout;
    for (size_t i = 0; i < size && (start + i) < PIXEL_COUNT; i++)
    {
        currentPixelData[start + i].animation_type = ani;
        currentPixelData[start + i].current_content = buf[i];
    }
}

void BT247GN::pixel_show(int start, char *buf, int size, NumAni ani)
{
    if (content_inhibit_time != 0)
    {
        for (size_t i = 0; i < size && (start + i) < PIXEL_COUNT; i++)
        {
            tempPixelData[start + i].animation_type = ani;
            tempPixelData[start + i].current_content = buf[i];
        }
        return;
    }
    for (size_t i = 0; i < size && (start + i) < PIXEL_COUNT; i++)
    {
        currentPixelData[start + i].animation_type = ani;
        currentPixelData[start + i].current_content = buf[i];
    }
}

void BT247GN::num_show(int start, char *buf, int size, NumAni ani)
{
    for (size_t i = 0; i < size && (start + i) < PIXEL_COUNT; i++)
    {
        currentNumData[start + i].animation_type = ani;
        currentNumData[start + i].current_content = buf[i];
    }
}

const uint8_t *BT247GN::find_pixel_hex_code(char ch)
{
    if (ch >= ' ' && ch <= ('~' + 1))
        return hex_codes[ch - ' '];
    return hex_codes[' '];
}

uint8_t BT247GN::find_num_hex_code(char ch)
{
    if (ch >= ' ' && ch <= 'Z')
        return num_hex_codes[ch - ' '];
    else if (ch >= 'a' && ch <= 'z')
        return num_hex_codes[ch - 'a' + 'A' - ' '];
    return 0;
}

void BT247GN::pixelanimate()
{
    static int64_t start_time = esp_timer_get_time() / 1000;
    int64_t current_time = esp_timer_get_time() / 1000;
    int64_t elapsed_time = current_time - start_time;
    uint8_t temp_code[5] = {0};

    if (elapsed_time >= 30)
        start_time = current_time;
    else
        return;

    if (content_inhibit_time != 0)
    {
        elapsed_time = current_time - content_inhibit_time;
        if (elapsed_time > 0)
        {
            for (size_t i = 0; i < PIXEL_COUNT; i++)
            {
                currentPixelData[i].last_content = currentPixelData[i].current_content;
                currentPixelData[i].animation_type = tempPixelData[i].animation_type;
                currentPixelData[i].current_content = tempPixelData[i].current_content;
            }
            content_inhibit_time = 0;
        }
    }

    for (int i = 0; i < PIXEL_COUNT; i++)
    {
        if (currentPixelData[i].current_content != currentPixelData[i].last_content)
        {
            const uint8_t *before_raw_code = find_pixel_hex_code(currentPixelData[i].last_content);
            const uint8_t *raw_code = find_pixel_hex_code(currentPixelData[i].current_content);
            if (currentPixelData[i].animation_type == UP2DOWN)
            {
                for (int j = 0; j < 5; j++)
                    temp_code[j] = (before_raw_code[i] << currentPixelData[i].animation_step) | (raw_code[i] >> (7 - currentPixelData[i].animation_step));

                if (currentPixelData[i].animation_step >= 7)
                    currentPixelData[i].animation_step = -1;
            }
            else if (currentPixelData[i].animation_type == DOWN2UP)
            {
                for (int j = 0; j < 5; j++)
                    temp_code[j] = (before_raw_code[i] >> currentPixelData[i].animation_step) | (raw_code[i] << (7 - currentPixelData[i].animation_step));

                if (currentPixelData[i].animation_step >= 7)
                    currentPixelData[i].animation_step = -1;
            }
            else if (currentPixelData[i].animation_type == LEFT2RT)
            {
                switch (currentPixelData[i].animation_step)
                {
                case 0:
                    for (int j = 0; j < 4; j++)
                        temp_code[j] = before_raw_code[j + 1];
                    for (int j = 4; j < 5; j++)
                        temp_code[j] = raw_code[j - 4];
                    break;
                case 1:
                    for (int j = 0; j < 3; j++)
                        temp_code[j] = before_raw_code[j + 2];
                    for (int j = 3; j < 5; j++)
                        temp_code[j] = raw_code[j - 3];
                    break;
                case 2:
                    for (int j = 0; j < 2; j++)
                        temp_code[j] = before_raw_code[j + 3];
                    for (int j = 2; j < 5; j++)
                        temp_code[j] = raw_code[j - 2];
                    break;
                case 3:
                    for (int j = 0; j < 1; j++)
                        temp_code[j] = before_raw_code[j + 4];
                    for (int j = 1; j < 5; j++)
                        temp_code[j] = raw_code[j - 1];
                    break;
                default:
                    currentPixelData[i].animation_step = -1;
                    break;
                }
            }
            else if (currentPixelData[i].animation_type == RT2LEFT)
            {
                switch (currentPixelData[i].animation_step)
                {
                case 0:
                    for (int j = 0; j < 4; j++)
                        temp_code[j + 1] = before_raw_code[j];
                    for (int j = 4; j < 5; j++)
                        temp_code[j - 4] = raw_code[j];
                    break;
                case 1:
                    for (int j = 0; j < 3; j++)
                        temp_code[j + 2] = before_raw_code[j];
                    for (int j = 3; j < 5; j++)
                        temp_code[j - 3] = raw_code[j];
                    break;
                case 2:
                    for (int j = 0; j < 2; j++)
                        temp_code[j + 3] = before_raw_code[j];
                    for (int j = 2; j < 5; j++)
                        temp_code[j - 2] = raw_code[j];
                    break;
                case 3:
                    for (int j = 0; j < 1; j++)
                        temp_code[j + 4] = before_raw_code[j];
                    for (int j = 1; j < 5; j++)
                        temp_code[j - 1] = raw_code[j];
                    break;
                default:
                    currentPixelData[i].animation_step = -1;
                    break;
                }
            }
            else
                currentPixelData[i].animation_step = -1;

            if (currentPixelData[i].animation_step == -1)
            {
                currentPixelData[i].last_content = currentPixelData[i].current_content;
                memcpy(temp_code, raw_code, sizeof temp_code);
            }

            pixelhelper(i, temp_code);

            currentPixelData[i].animation_step++;
        }
    }
}

uint8_t BT247GN::contentgetpart(uint8_t raw, uint8_t before_raw, uint8_t mask)
{
    return (raw & mask) | (before_raw & (~mask));
}

void BT247GN::numberanimate()
{
    static int64_t start_time = esp_timer_get_time() / 1000;
    int64_t current_time = esp_timer_get_time() / 1000;
    int64_t elapsed_time = current_time - start_time;

    if (elapsed_time >= 30)
        start_time = current_time;
    else
        return;

    for (int i = 0; i < NUM_COUNT; i++)
    {
        if (currentNumData[i].current_content != currentNumData[i].last_content)
        {
            uint8_t before_raw_code = find_num_hex_code(currentNumData[i].last_content);
            uint8_t raw_code = find_num_hex_code(currentNumData[i].current_content);
            uint8_t code = raw_code;
            if (currentNumData[i].animation_type == CLOCKWISE)
            {
                switch (currentNumData[i].animation_step)
                {
                case 0:
                    code = contentgetpart(raw_code, before_raw_code, 1);
                    break;
                case 1:
                    code = contentgetpart(raw_code, before_raw_code, 3);
                    break;
                case 2:
                    code = contentgetpart(raw_code, before_raw_code, 0x43);
                    break;
                case 3:
                    code = contentgetpart(raw_code, before_raw_code, 0x47);
                    break;
                case 4:
                    code = contentgetpart(raw_code, before_raw_code, 0x4f);
                    break;
                case 5:
                    code = contentgetpart(raw_code, before_raw_code, 0x5f);
                    break;
                default:
                    currentNumData[i].animation_step = -1;
                    break;
                }
            }
            else if (currentNumData[i].animation_type == ANTICLOCKWISE)
            {
                switch (currentNumData[i].animation_step)
                {
                case 0:
                    code = contentgetpart(raw_code, before_raw_code, 1);
                    break;
                case 1:
                    code = contentgetpart(raw_code, before_raw_code, 0x21);
                    break;
                case 2:
                    code = contentgetpart(raw_code, before_raw_code, 0x61);
                    break;
                case 3:
                    code = contentgetpart(raw_code, before_raw_code, 0x71);
                    break;
                case 4:
                    code = contentgetpart(raw_code, before_raw_code, 0x79);
                    break;
                case 5:
                    code = contentgetpart(raw_code, before_raw_code, 0x7d);
                    break;
                default:
                    currentNumData[i].animation_step = -1;
                    break;
                }
            }
            else if (currentNumData[i].animation_type == UP2DOWN)
            {
                switch (currentNumData[i].animation_step)
                {
                case 0:
                    code = contentgetpart(raw_code, before_raw_code, 0x1);
                    break;
                case 1:
                    code = contentgetpart(raw_code, before_raw_code, 0x21);
                    break;
                case 2:
                    code = contentgetpart(raw_code, before_raw_code, 0x73);
                    break;
                default:
                    currentNumData[i].animation_step = -1;
                    break;
                }
            }
            else if (currentNumData[i].animation_type == DOWN2UP)
            {
                switch (currentNumData[i].animation_step)
                {
                case 0:
                    code = contentgetpart(raw_code, before_raw_code, 0x8);
                    break;
                case 1:
                    code = contentgetpart(raw_code, before_raw_code, 0xc);
                    break;
                case 2:
                    code = contentgetpart(raw_code, before_raw_code, 0x5e);
                    break;
                default:
                    currentNumData[i].animation_step = -1;
                    break;
                }
            }
            else if (currentNumData[i].animation_type == LEFT2RT)
            {
                switch (currentNumData[i].animation_step)
                {
                case 0:
                    code = contentgetpart(raw_code, before_raw_code, 0x10);
                    break;
                case 1:
                    code = contentgetpart(raw_code, before_raw_code, 0x18);
                    break;
                case 2:
                    code = contentgetpart(raw_code, before_raw_code, 0x7c);
                    break;
                default:
                    currentNumData[i].animation_step = -1;
                    break;
                }
            }
            else if (currentNumData[i].animation_type == RT2LEFT)
            {
                switch (currentNumData[i].animation_step)
                {
                case 0:
                    code = contentgetpart(raw_code, before_raw_code, 0x4);
                    break;
                case 1:
                    code = contentgetpart(raw_code, before_raw_code, 0xc);
                    break;
                case 2:
                    code = contentgetpart(raw_code, before_raw_code, 0x5e);
                    break;
                default:
                    currentNumData[i].animation_step = -1;
                    break;
                }
            }
            else
                currentNumData[i].animation_step = -1;

            if (currentNumData[i].animation_step == -1)
                currentNumData[i].last_content = currentNumData[i].current_content;

            numhelper(i, code);
            currentNumData[i].animation_step++;
        }
    }
}
void BT247GN::pixelhelper(int index, uint8_t *code)
{
    memcpy(pixel_gram + index * 5, code, 5);
}

void BT247GN::numhelper(int index, uint8_t code)
{
    num_gram[index] = code;
}

void BT247GN::symbolhelper(Icon_e icon, bool en)
{
    icon_gram[(int)icon] = en;
}

void BT247GN::time_blink()
{
    static bool time_mark = true;
    time_mark = !time_mark;
    symbolhelper(Colon, time_mark);
}

void BT247GN::refrash() // origin
{
    static uint8_t lastdimming = 0;
    if (lastdimming != dimming)
    {
        lastdimming = dimming;
        dimming_write(dimming);
    }
    pixel_write(0, 0, pixel_gram, (sizeof pixel_gram) / 5);
    num_write(0, num_gram, sizeof num_gram);
    icon_write(0, icon_gram, sizeof icon_gram);
}

void BT247GN::pixel_write(int x, int y, uint8_t *code, int len)
{
    uint8_t temp_gram[PIXEL_COUNT * 5 + 1];
    temp_gram[0] = 0x20;
    temp_gram[0] |= (x & 0xF) << 1;
    temp_gram[0] |= y != 0;
    memcpy(temp_gram + 1, code, len * 5);
    write_data8(temp_gram, len * 5 + 1);
}

void BT247GN::pixel_write(int x, int y, char *ascii, int len)
{
    uint8_t temp_gram[PIXEL_COUNT + 1];
    temp_gram[0] = 0xA0;
    temp_gram[0] |= (x & 0xF) << 1;
    temp_gram[0] |= y != 0;
    memcpy(temp_gram + 1, ascii, len);
    write_data8(temp_gram, len + 1);
}

void BT247GN::num_write(int x, uint8_t *code, int len)
{
    uint8_t temp_gram[NUM_COUNT + 1];
    temp_gram[0] = 0x40;
    temp_gram[0] |= (x & 0x1F);
    memcpy(temp_gram + 1, code, len);
    write_data8(temp_gram, len + 1);
}

void BT247GN::num_write(int x, char *ascii, int len)
{
    uint8_t temp_gram[NUM_COUNT + 1];
    temp_gram[0] = 0x80;
    temp_gram[0] |= (x & 0x1F);
    memcpy(temp_gram + 1, ascii, len);
    write_data8(temp_gram, len + 1);
}

void BT247GN::icon_write(Icon_e icon, bool en)
{
    uint8_t temp_gram[1 + 2];
    temp_gram[0] = 0x60;
    temp_gram[1] = (uint8_t)icon;
    temp_gram[2] = en;

    write_data8(temp_gram, 3);
}

void BT247GN::icon_write(int x, uint8_t *code, int len)
{
    uint8_t temp_gram[ICON_COUNT + 2];
    temp_gram[0] = 0x60;
    temp_gram[1] = x;
    memcpy(temp_gram + 2, code, len);

    write_data8(temp_gram, len + 2);
}

void BT247GN::dimming_write(int val)
{
    uint8_t temp_gram[2];
    temp_gram[0] = 0xB0;
    temp_gram[1] = val;

    write_data8(temp_gram, 2);
}