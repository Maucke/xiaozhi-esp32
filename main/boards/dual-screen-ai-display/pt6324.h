/*
 * Author: 施华锋
 * Date: 2025-01-16
 * Description: This header file defines the PT6324 class for communicating with the PT6324 device via SPI.
 */

#ifndef _PT6324_H_
#define _PT6324_H_

#include <driver/spi_master.h>
#include <driver/gpio.h>
#include <esp_log.h>

/**
 * @class PT6324
 * @brief A class for interacting with the PT6324 device using SPI communication.
 *
 * This class provides methods to initialize the PT6324 device, write data to it, and refresh its display.
 */
class PT6324
{
public:
    PT6324(gpio_num_t din, gpio_num_t clk, gpio_num_t cs, spi_host_device_t spi_num);
    PT6324(spi_device_handle_t spi_device) : spi_device_(spi_device) {}
    void init();
    void setbrightness(uint8_t brightness);
    void setsleep(bool en);

private:
    uint8_t dimming = 0;
    spi_device_handle_t spi_device_;

    void write_data8(uint8_t *dat, int len);

protected:
    uint8_t internal_gram[48] = {0};
    void refrash(uint8_t *gram);
};

#endif