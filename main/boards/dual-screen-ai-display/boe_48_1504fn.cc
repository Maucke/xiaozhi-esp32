#include "boe_48_1504fn.h"
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

void BOE_48_1504FN::refrash(Gram *gram)
{
    if (gram == nullptr)
        return;
    write_cgram(0, gram->cgram, CGRAM_SIZE * 5);
    write_dcram(0, gram->number, DISPLAY_SIZE);
    write_adram(0, gram->symbol, GR_COUNT);
    write_dimming();
}

void BOE_48_1504FN::init_task()
{
    write_grnum(GR_COUNT);
    const uint8_t values[5] = {
        0, 1, 2, 3, 4};
    write_dcram(10, (uint8_t *)values, 5);
    // noti_show("Test long string present: 0123456789", 5000);
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

    symbolhelper(L_OUTLINE, true);
    symbolhelper(R_OUTLINE, true);
    symbolhelper(LD_LINE, true);
    symbolhelper(RD_LINE, true);
    symbolhelper(D_OUTLINE, true);
    symbolhelper(L_LINE, true);
    symbolhelper(R_LINE, true);

    refrash(&internal_gram);
    xTaskCreate(
        [](void *arg)
        {
            int count = 0;
            BOE_48_1504FN *vfd = static_cast<BOE_48_1504FN *>(arg);
            // char tempstr[10];

            while (true)
            {
                // vfd->internal_gram.cgram[(count / 8 - 1) % 25] = 0;
                // vfd->internal_gram.cgram[count / 8 % 25] = 1 << (count % 8);

                // snprintf(tempstr, 10, "%d-%d", count / 8 % 25, count % 8);
                // vfd->content_show(0, tempstr, 4, false, BOE_48_1504FN::NONE);
                // vfd->symbolhelper((Symbols)((count / 10) % SYMBOL_COUNT), false);
                if (!((++count) % 12))
                {
                    vfd->display_buffer();
                    vfd->scroll_buffer();
                }
                // vfd->symbolhelper((Symbols)((count / 10) % SYMBOL_COUNT), true);
                vfd->refrash(&vfd->internal_gram);
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
            internal_gram.cgram[5 * SYMBOL_CGRAM_SIZE + ramindex * 5 + i] = code[i];
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

void BOE_48_1504FN::free_cgram(int *index)
{
    if (*index > 2 || *index < 0)
        return;
    cgramBusy[*index] = false;
    *index = -1;
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
                currentContentData[i].animation_type = NONE;
                currentContentData[i].current_content = tempContentData[i].current_content;
                currentContentData[i].need_update = true;
                free_cgram(&currentContentData[i].animation_index);
            }

            content_inhibit_time = 0;
        }
    }

    for (int i = 0; i < DISPLAY_SIZE; i++)
    {
        if (currentContentData[i].current_content != currentContentData[i].last_content || currentContentData[i].need_update)
        {
            if (currentContentData[i].animation_type == NONE)
            {
                currentContentData[i].need_update = false;
                currentContentData[i].last_content = currentContentData[i].current_content;
                if (currentContentData[i].current_content < 8)
                    charhelper(i, ' ');
                else
                    charhelper(i, currentContentData[i].current_content);
                free_cgram(&currentContentData[i].animation_index);

                continue;
            }

            if (currentContentData[i].animation_index == -1)
            {
                currentContentData[i].animation_index = get_cgram();
                if (currentContentData[i].animation_index == -1)
                {
                    // ESP_LOGI(TAG, "currentContentData[%d].animation_index = -1", i);
                    return;
                }
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
                if (currentContentData[i].current_content < 8)
                    charhelper(i, ' ');
                else
                    charhelper(i, currentContentData[i].current_content);
                free_cgram(&currentContentData[i].animation_index);
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
            currentContentData[i].need_update = true;
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

void BOE_48_1504FN::find_enum_code(Symbols flag, int *byteIndex, int *bitMask)
{
    // ESP_LOGI(TAG, "byteIndex: %d, flag: %d", *byteIndex, flag);
    *byteIndex = symbolPositions[flag].byteIndex;
    *bitMask = symbolPositions[flag].bitMask;
}

void BOE_48_1504FN::symbolhelper(Symbols symbol, bool is_on)
{
    if (symbol >= SYMBOL_MAX)
        return;

    int byteIndex, bitMask;
    find_enum_code(symbol, &byteIndex, &bitMask);

    if (byteIndex < 25)
    {
        // ESP_LOGI(TAG, "byteIndex: %d", byteIndex);
        if (is_on)
            internal_gram.cgram[byteIndex] |= bitMask;
        else
            internal_gram.cgram[byteIndex] &= ~bitMask;
    }
    else
    {
        if (is_on)
            internal_gram.symbol[byteIndex - 25] |= bitMask;
        else
            internal_gram.symbol[byteIndex - 25] &= ~bitMask;
    }
}

void BOE_48_1504FN::display_buffer()
{
    int64_t current_time = esp_timer_get_time() / 1000;
    if (content_inhibit_time != 0)
    {
        int64_t elapsed_time = current_time - content_inhibit_time;
        if (elapsed_time > 0)
        {
            memset(cb->buffer, 0, sizeof cb->buffer);
            cb->start_pos = 0;
            cb->length = 0;
            content_inhibit_time = 0;
        }
    }
    else
        return;
    if (cb->length)
    {
        if (cb->length <= DISPLAY_SIZE)
        {
            for (size_t i = 0; i < DISPLAY_SIZE; i++)
            {
                currentContentData[i].animation_type = NONE;
                currentContentData[i].current_content = cb->buffer[i];
            }
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
            for (size_t i = 0; i < DISPLAY_SIZE; i++)
            {
                currentContentData[i].animation_type = NONE;
                currentContentData[i].current_content = display[i];
            }

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
    content_inhibit_time = esp_timer_get_time() / 1000 + timeout;
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

void BOE_48_1504FN::spectrum_show(float *buf, int size)
{
    symbolhelper(LD_0_0, false);
    symbolhelper(LD_1_0, false);
    symbolhelper(LD_2_0, false);
    symbolhelper(LD_3_0, false);
    symbolhelper(LD_4_0, false);
    symbolhelper(RD_0_0, false);
    symbolhelper(RD_1_0, false);
    symbolhelper(RD_2_0, false);
    symbolhelper(RD_3_0, false);
    symbolhelper(RD_4_0, false);

    int fft_level_l = 0, fft_level_r = 0;
    for (int i = size / 4; i < size * 3 / 4; i++)
    {
        if (i % 2)
            fft_level_l += buf[i];
        else
            fft_level_r += buf[i];
    }
    fft_level_l /= (size / 4);
    fft_level_r /= (size / 4);

    if (fft_level_l > 5)
        symbolhelper(RD_0_0, true);
    if (fft_level_l > 25)
        symbolhelper(RD_1_0, true);
    if (fft_level_l > 45)
        symbolhelper(RD_2_0, true);
    if (fft_level_l > 65)
        symbolhelper(RD_3_0, true);
    if (fft_level_l > 85)
        symbolhelper(RD_4_0, true);

    if (fft_level_r > 5)
        symbolhelper(LD_0_0, true);
    if (fft_level_r > 25)
        symbolhelper(LD_1_0, true);
    if (fft_level_r > 45)
        symbolhelper(LD_2_0, true);
    if (fft_level_r > 65)
        symbolhelper(LD_3_0, true);
    if (fft_level_r > 85)
        symbolhelper(LD_4_0, true);

    symbolhelper(L_0_0, false);
    symbolhelper(L_1_0, false);
    symbolhelper(R_0_0, false);
    symbolhelper(R_1_0, false);

    for (size_t i = 0; i < 14; i++)
    {
        symbolhelper(bar_L_3[i], false);
        symbolhelper(bar_L_4[i], false);
        symbolhelper(bar_R_3[i], false);
        symbolhelper(bar_R_4[i], false);
    }
    int len_single = (size / 28);
    static int count = 0;
    for (int i = 0; i < 28; i++)
    {
        int level_sum = 0;
        for (int j = 0; j < len_single; j++)
        {
            level_sum += buf[i * len_single + j];
        }
        level_sum /= len_single;
        if (i % 2)
        {
            if (level_sum > 10)
                symbolhelper(L_0_0, true);
            if (level_sum > 30)
                symbolhelper(L_1_0, true);
            if (level_sum > 50)
                symbolhelper(bar_L_3[(i + count) % 14], true);
            if (level_sum > 75)
                symbolhelper(bar_L_4[(i + count) % 14], true);
        }
        else
        {
            if (level_sum > 10)
                symbolhelper(R_0_0, true);
            if (level_sum > 30)
                symbolhelper(R_1_0, true);
            if (level_sum > 50)
                symbolhelper(bar_R_3[(i + count) % 14], true);
            if (level_sum > 75)
                symbolhelper(bar_R_4[(i + count) % 14], true);
        }
    }
    count++;
}