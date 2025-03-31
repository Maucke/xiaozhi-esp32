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
 * @brief Performs the wave animation process.
 *
 * Calculates the elapsed time, resets wave data if necessary, updates the current value of each wave point
 * based on the animation progress, and calls helper functions to update the display.
 * Also calculates the sum of left and right wave values and calls the core wave helper function.
 */
void BOE_48_1504FN::waveanimate()
{
    int left_sum = 0, right_sum = 0;
    int64_t current_time = esp_timer_get_time() / 1000;
    int64_t elapsed_time = current_time - wave_start_time;

    if (elapsed_time >= 220)
    {
        wave_start_time = current_time;
        for (size_t i = 0; i < FFT_SIZE; i++)
        {
            waveData[i].last_value = waveData[i].target_value;
            waveData[i].target_value = 0;
            waveData[i].animation_step = 0;
        }
    }
    for (int i = 0; i < FFT_SIZE; i++)
    {
        if (waveData[i].animation_step < wave_total_steps)
        {
            float progress = static_cast<float>(waveData[i].animation_step) / wave_total_steps;
            float factor = 1 - std::exp(-3 * progress);
            waveData[i].current_value = waveData[i].last_value + static_cast<int>((waveData[i].target_value - waveData[i].last_value) * factor);
            wavehelper(i, waveData[i].current_value * 8 / 90);
            waveData[i].animation_step++;
        }
        else
        {
            waveData[i].last_value = waveData[i].target_value;
            wavehelper(i, waveData[i].target_value * 8 / 90);
        }
        if (i < 6)
            left_sum += waveData[i].current_value;
        else
            right_sum += waveData[i].current_value;
    }
    corewavehelper(left_sum * 8 / 90 / 4, right_sum * 8 / 90 / 4);
}

/**
 * @brief Gets a part of the content by combining raw and previous raw data according to a mask.
 *
 * Combines the raw data and the previous raw data based on the given mask,
 * where the bits in the mask determine which parts come from the raw data and which come from the previous raw data.
 *
 * @param raw The current raw data.
 * @param before_raw The previous raw data.
 * @param mask The mask used to determine the combination method.
 * @return The combined data.
 */
uint32_t BOE_48_1504FN::contentgetpart(uint32_t raw, uint32_t before_raw, uint32_t mask)
{
    return (raw & mask) | (before_raw & (~mask));
}

/**
 * @brief Performs the content animation process.
 *
 * Checks the elapsed time, updates the content data if the inhibition time has passed.
 * For each content item with a different current and last content,
 * it selects the appropriate animation step based on the animation type and updates the display.
 */
void BOE_48_1504FN::contentanimate()
{
    static int64_t start_time = esp_timer_get_time() / 1000;
    int64_t current_time = esp_timer_get_time() / 1000;
    int64_t elapsed_time = current_time - start_time;

    if (elapsed_time >= 30)
        start_time = current_time;
    else
        return;

    if (content_inhibit_time != 0)
    {
        elapsed_time = current_time - content_inhibit_time;
        if (elapsed_time > 0)
        {
            for (size_t i = 0; i < CONTENT_SIZE; i++)
            {
                currentData[i].last_content = currentData[i].current_content;
                currentData[i].animation_type = tempData[i].animation_type;
                currentData[i].current_content = tempData[i].current_content;
            }
            content_inhibit_time = 0;
        }
    }

    for (int i = 0; i < CONTENT_SIZE; i++)
    {
        if (currentData[i].current_content != currentData[i].last_content)
        {
            uint32_t before_raw_code = find_hex_code(currentData[i].last_content);
            uint32_t raw_code = find_hex_code(currentData[i].current_content);
            uint32_t code = raw_code;
            if (currentData[i].animation_type == HNA_CLOCKWISE)
            {
                switch (currentData[i].animation_step)
                {
                case 0:
                    code = contentgetpart(raw_code, before_raw_code, 0x080000 | 0x800000);
                    break;
                case 1:
                    code = contentgetpart(raw_code, before_raw_code, 0x4C0000 | 0x800000);
                    break;
                case 2:
                    code = contentgetpart(raw_code, before_raw_code, 0x6e0000 | 0x800000);
                    break;
                case 3:
                    code = contentgetpart(raw_code, before_raw_code, 0x6f6000 | 0x800000);
                    break;
                case 4:
                    code = contentgetpart(raw_code, before_raw_code, 0x6f6300 | 0x800000);
                    break;
                case 5:
                    code = contentgetpart(raw_code, before_raw_code, 0x6f6770 | 0x800000);
                    break;
                case 6:
                    code = contentgetpart(raw_code, before_raw_code, 0x6f6ff0 | 0x800000);
                    break;
                case 7:
                    code = contentgetpart(raw_code, before_raw_code, 0x6ffff0 | 0x800000);
                    break;
                default:
                    currentData[i].animation_step = -1;
                    break;
                }
            }
            else if (currentData[i].animation_type == HNA_ANTICLOCKWISE)
            {
                switch (currentData[i].animation_step)
                {
                case 0:
                    code = contentgetpart(raw_code, before_raw_code, 0x004880);
                    break;
                case 1:
                    code = contentgetpart(raw_code, before_raw_code, 0x004ca0);
                    break;
                case 2:
                    code = contentgetpart(raw_code, before_raw_code, 0x004ef0);
                    break;
                case 3:
                    code = contentgetpart(raw_code, before_raw_code, 0x006ff0);
                    break;
                case 4:
                    code = contentgetpart(raw_code, before_raw_code, 0x036ff0);
                    break;
                case 5:
                    code = contentgetpart(raw_code, before_raw_code, 0x676ff0);
                    break;
                case 6:
                    code = contentgetpart(raw_code, before_raw_code, 0xef6ff0);
                    break;
                case 7:
                    code = contentgetpart(raw_code, before_raw_code, 0xffeff0);
                    break;
                default:
                    currentData[i].animation_step = -1;
                    break;
                }
            }
            else if (currentData[i].animation_type == HNA_UP2DOWN)
            {
                switch (currentData[i].animation_step)
                {
                case 0:
                    code = contentgetpart(raw_code, before_raw_code, 0xe00000);
                    break;
                case 1:
                    code = contentgetpart(raw_code, before_raw_code, 0xff0000);
                    break;
                case 2:
                    code = contentgetpart(raw_code, before_raw_code, 0xffe000);
                    break;
                case 3:
                    code = contentgetpart(raw_code, before_raw_code, 0xffff00);
                    break;
                default:
                    currentData[i].animation_step = -1;
                    break;
                }
            }
            else if (currentData[i].animation_type == HNA_DOWN2UP)
            {
                switch (currentData[i].animation_step)
                {
                case 0:
                    code = contentgetpart(raw_code, before_raw_code, 0x0000f0);
                    break;
                case 1:
                    code = contentgetpart(raw_code, before_raw_code, 0x001ff0);
                    break;
                case 2:
                    code = contentgetpart(raw_code, before_raw_code, 0x00fff0);
                    break;
                case 3:
                    code = contentgetpart(raw_code, before_raw_code, 0x1ffff0);
                    break;
                default:
                    currentData[i].animation_step = -1;
                    break;
                }
            }
            else if (currentData[i].animation_type == HNA_LEFT2RT)
            {
                switch (currentData[i].animation_step)
                {
                case 0:
                    code = contentgetpart(raw_code, before_raw_code, 0x901080);
                    break;
                case 1:
                    code = contentgetpart(raw_code, before_raw_code, 0xd89880);
                    break;
                case 2:
                    code = contentgetpart(raw_code, before_raw_code, 0xdcdce0);
                    break;
                case 3:
                    code = contentgetpart(raw_code, before_raw_code, 0xdefee0);
                    break;
                default:
                    currentData[i].animation_step = -1;
                    break;
                }
            }
            else if (currentData[i].animation_type == HNA_RT2LEFT)
            {
                switch (currentData[i].animation_step)
                {
                case 0:
                    code = contentgetpart(raw_code, before_raw_code, 0x210110);
                    break;
                case 1:
                    code = contentgetpart(raw_code, before_raw_code, 0x632310);
                    break;
                case 2:
                    code = contentgetpart(raw_code, before_raw_code, 0x676770);
                    break;
                case 3:
                    code = contentgetpart(raw_code, before_raw_code, 0x6fef70);
                    break;
                default:
                    currentData[i].animation_step = -1;
                    break;
                }
            }
            else
                currentData[i].animation_step = -1;

            if (currentData[i].animation_step == -1)
                currentData[i].last_content = currentData[i].current_content;

            charhelper(i, code);
            currentData[i].animation_step++;
        }
    }
}

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
    test();

    ESP_LOGI(TAG, "LCD BOE_48_1504FN");
    // xTaskCreate(
    //     [](void *arg)
    //     {
    //         BOE_48_1504FN *vfd = static_cast<BOE_48_1504FN *>(arg);
    //         vfd->symbolhelper(LBAR_RBAR, true);
    //         while (true)
    //         {
    //             vfd->refrash(vfd->gram);
    //             vfd->contentanimate();
    //             vfd->waveanimate();
    //             vTaskDelay(pdMS_TO_TICKS(10));
    //         }
    //         vTaskDelete(NULL);
    //     },
    //     "vfd",
    //     4096 - 1024,
    //     this,
    //     6,
    //     nullptr);
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
    test();
        ESP_LOGI(TAG, "LCD BOE_48_1504FN");
    // xTaskCreate(
    //     [](void *arg)
    //     {
    //         BOE_48_1504FN *vfd = static_cast<BOE_48_1504FN *>(arg);
    //         vfd->symbolhelper(LBAR_RBAR, true);
    //         while (true)
    //         {
    //             vfd->refrash(vfd->gram);
    //             vfd->contentanimate();
    //             vfd->waveanimate();
    //             vTaskDelay(pdMS_TO_TICKS(10));
    //         }
    //         vTaskDelete(NULL);
    //     },
    //     "vfd",
    //     4096 - 1024,
    //     this,
    //     6,
    //     nullptr);
}

void BOE_48_1504FN::charhelper(int index, char ch)
{
}
