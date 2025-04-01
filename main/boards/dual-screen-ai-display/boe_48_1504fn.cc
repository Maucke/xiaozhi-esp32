#include "boe_48_1504fn.h"
#include "driver/usb_serial_jtag.h"
#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <freertos/task.h>
#include <string.h>
#include <esp_log.h>

// Define the log tag
#define TAG "BOE_48_1504FN"

/**
 * @brief Constructor for the PT6302 class.
 *
 * Initializes the PT6302 object with the specified GPIO pins and SPI host device.
 *
 * @param din The GPIO pin number for the data input line.
 * @param clk The GPIO pin number for the clock line.
 * @param cs The GPIO pin number for the chip select line.
 * @param spi_num The SPI host device number to use for communication.
 */
BOE_48_1504FN::BOE_48_1504FN(gpio_num_t din, gpio_num_t clk, gpio_num_t cs, spi_host_device_t spi_num) : PT6302(din, clk, cs, spi_num)
{
    init();
    init_task();
    ESP_LOGI(TAG, "BOE_48_1504FN Initalized");
}

/**
 * @brief Constructor of the BOE_48_1504FN class.
 *
 * Initializes the PT6302 device and creates a task to refresh the display and perform animations.
 *
 * @param spi_device The SPI device handle used to communicate with the PT6302.
 */
BOE_48_1504FN::BOE_48_1504FN(spi_device_handle_t spi_device) : PT6302(spi_device)
{
    if (!spi_device)
    {
        ESP_LOGE(TAG, "VFD spi is null");
        return;
    }
    init();
    init_task();
    ESP_LOGI(TAG, "BOE_48_1504FN Initalized");
}

void BOE_48_1504FN::init_task()
{
    // noti_show("Test long string present: 0123456789");
    memset(internal_gram.symbol, 0, sizeof internal_gram.symbol);
    memset(internal_gram.cgram, 0, sizeof internal_gram.cgram);
    for (size_t i = 0; i < 10; i++)
    {
        charhelper(i, ' ');
    }
    for (int i = 0; i < DISPLAY_SIZE; i++)
    {
        currentContentData[i].animation_index = -1;
        tempContentData[i].animation_index = -1;
    }

    xTaskCreate(
        [](void *arg)
        {
            int count = 0;
            BOE_48_1504FN *vfd = static_cast<BOE_48_1504FN *>(arg);
            char tempstr[10];

            while (true)
            {
                // vfd->internal_gram.cgram[(count / 8 - 1) % 25] = 0;
                // vfd->internal_gram.cgram[count / 8 % 25] = 1 << (count % 8);

                // snprintf(tempstr, 10, "%d-%d", count / 8 % 25, count % 8);
                // vfd->content_show(0, tempstr, 4, false, BOE_48_1504FN::NONE);
                if (!((++count) % 12))
                {
                    vfd->display_buffer();
                    vfd->scroll_buffer();
                }
                vfd->refrash();
                vTaskDelay(pdMS_TO_TICKS(10));
                vfd->contentanimate();
            }
            vTaskDelete(NULL);
        },
        "vfd",
        4096 - 1024,
        this,
        6,
        nullptr);
}

void BOE_48_1504FN::charhelper(int index, char ch)
{
    if (index <= (DISPLAY_SIZE - 1))
        internal_gram.number[index] = ch;
}

void BOE_48_1504FN::charhelper(int index, int ramindex, uint8_t *code)
{
    if (index <= (DISPLAY_SIZE - 1) && ramindex <= 2)
    {
        for (size_t i = 0; i < 5; i++)
            internal_gram.cgram_number[ramindex * 5 + i] = code[i];
        internal_gram.number[index] = ramindex + SYMBOL_CGRAM_SIZE;
    }
}

const uint8_t *BOE_48_1504FN::find_content_hex_code(char ch)
{
    if (ch >= ' ' && ch <= ('~' + 1))
        return hex_codes[ch - ' '];
    return hex_codes[0];
}

int BOE_48_1504FN::get_cgram()
{
    for (size_t i = 0; i < 3; i++)
    {
        if (!cgramBusy[i])
        {
            cgramBusy[i] = true;
            return i;
        }
    }
    return -1;
}

void BOE_48_1504FN::free_cgram(int index)
{
    if (index > 2)
        return;
    cgramBusy[index] = false;
}

void BOE_48_1504FN::contentanimate()
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
            for (size_t i = 0; i < DISPLAY_SIZE; i++)
            {
                currentContentData[i].last_content = currentContentData[i].current_content;
                currentContentData[i].animation_type = tempContentData[i].animation_type;
                currentContentData[i].current_content = tempContentData[i].current_content;
                currentContentData[i].need_update = true;
            }
            content_inhibit_time = 0;
        }
    }

    for (int i = 0; i < DISPLAY_SIZE; i++)
    {

        if (currentContentData[i].current_content != currentContentData[i].last_content || currentContentData[i].need_update)
        {
            if (currentContentData[i].animation_index == -1)
            {
                currentContentData[i].animation_index = get_cgram();
                if (currentContentData[i].animation_index == -1)
                    return;
            }
            currentContentData[i].animation_step++;
            const uint8_t *before_raw_code = find_content_hex_code(currentContentData[i].last_content);
            const uint8_t *raw_code = find_content_hex_code(currentContentData[i].current_content);

            // ESP_LOGI(TAG, "%c-%c", currentContentData[i].last_content, currentContentData[i].current_content);

            if (currentContentData[i].animation_type == UP2DOWN)
            {
                for (int j = 0; j < 5; j++)
                    temp_code[j] = (before_raw_code[j] << currentContentData[i].animation_step) | (raw_code[j] >> (8 - currentContentData[i].animation_step));

                if (currentContentData[i].animation_step >= 8)
                    currentContentData[i].animation_step = -1;
            }
            else if (currentContentData[i].animation_type == DOWN2UP)
            {
                for (int j = 0; j < 5; j++)
                    temp_code[j] = (before_raw_code[j] >> currentContentData[i].animation_step) | (raw_code[j] << (8 - currentContentData[i].animation_step));

                if (currentContentData[i].animation_step >= 8)
                    currentContentData[i].animation_step = -1;
            }
            else if (currentContentData[i].animation_type == LEFT2RT)
            {
                switch (currentContentData[i].animation_step)
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
                    currentContentData[i].animation_step = -1;
                    break;
                }
            }
            else if (currentContentData[i].animation_type == RT2LEFT)
            {
                switch (currentContentData[i].animation_step)
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
                    currentContentData[i].animation_step = -1;
                    break;
                }
            }
            else
                currentContentData[i].animation_step = -1;

            if (currentContentData[i].animation_step == -1)
            {
                currentContentData[i].need_update = false;
                currentContentData[i].last_content = currentContentData[i].current_content;
                memcpy(temp_code, raw_code, sizeof temp_code);
                charhelper(i, currentContentData[i].current_content);
                free_cgram(currentContentData[i].animation_index);
                currentContentData[i].animation_index = -1;
            }
            else
            {
                charhelper(i, currentContentData[i].animation_index, temp_code);
            }
        }
    }
}

void BOE_48_1504FN::noti_show(int start, const char *buf, int size, bool forceupdate, NumAni ani, int timeout)
{
    content_inhibit_time = esp_timer_get_time() / 1000 + timeout;
    for (size_t i = 0; i < DISPLAY_SIZE; i++)
    {
        currentContentData[i].animation_type = ani;
        currentContentData[i].current_content = ' ';
        if (forceupdate)
            currentContentData[start + i].need_update = true;
    }
    for (size_t i = 0; i < size && (start + i) < DISPLAY_SIZE; i++)
    {
        currentContentData[start + i].animation_type = ani;
        currentContentData[start + i].current_content = buf[i];
        if (forceupdate)
            currentContentData[start + i].need_update = true;
    }
}

void BOE_48_1504FN::content_show(int start, const char *buf, int size, bool forceupdate, NumAni ani)
{
    if (content_inhibit_time != 0)
    {
        for (size_t i = 0; i < size && (start + i) < DISPLAY_SIZE; i++)
        {
            tempContentData[start + i].animation_type = ani;
            tempContentData[start + i].current_content = buf[i];
            if (forceupdate)
                tempContentData[start + i].need_update = true;
        }
        return;
    }
    for (size_t i = 0; i < size && (start + i) < DISPLAY_SIZE; i++)
    {
        currentContentData[start + i].animation_type = ani;
        currentContentData[start + i].current_content = buf[i];
        if (forceupdate)
            currentContentData[start + i].need_update = true;
    }
}

void BOE_48_1504FN::display_buffer()
{
    if (cb->length)
    {
        if (cb->length <= DISPLAY_SIZE)
        {
            noti_show(0, cb->buffer, DISPLAY_SIZE, false, NONE, roll_timeout_);

            // ESP_LOGI(TAG, "%s", cb->buffer);
        }
        else
        {
            // Scroll display for length > 10
            char display[DISPLAY_SIZE + 1];
            for (int i = 0; i < DISPLAY_SIZE; i++)
            {
                int pos = (cb->start_pos + i) % cb->length;
                display[i] = cb->buffer[pos];
            }
            noti_show(0, display, DISPLAY_SIZE, false, NONE, roll_timeout_);

            // ESP_LOGI(TAG, "%s", display);
        }
    }
}
void BOE_48_1504FN::scroll_buffer()
{
    if (cb->length > DISPLAY_SIZE)
    {
        cb->start_pos = (cb->start_pos + 1) % cb->length;
    }
}

void BOE_48_1504FN::noti_show(const char *str, int timeout)
{
    roll_timeout_ = timeout;
    int str_len = strlen(str);
    if (str_len > BUFFER_SIZE)
    {
        // Only keep the last BUFFER_SIZE characters if input is too long
        str += (str_len - BUFFER_SIZE);
        str_len = BUFFER_SIZE;
    }
    {
        memset(cb->buffer, 0, sizeof cb->buffer);
        cb->start_pos = 0;
        cb->length = 0;
        if (str_len + cb->length <= (BUFFER_SIZE - 2))
        {
            // Simple append
            strncpy(cb->buffer + cb->length, str, str_len);
            cb->length += str_len;
            if (str_len > DISPLAY_SIZE)
            {
                memset(cb->buffer + cb->length, ' ', 2);
                cb->length += 2;
            }
        }
        else
        {
            // Need to wrap around
            int remaining = BUFFER_SIZE - cb->length;
            strncpy(cb->buffer + cb->length, str, remaining);
            strncpy(cb->buffer, str + remaining, str_len - remaining);
            cb->length = BUFFER_SIZE;
            cb->buffer[cb->length] = '\0';
        }
    }
}