/*
 * Author: 施华锋
 * Date: 2025-01-16
 * Description: This header file defines the BT247GN class for communicating with the BT247GN device via SPI.
 */

#ifndef _BT247GN_H_
#define _BT247GN_H_

#include <driver/spi_master.h>
#include <driver/gpio.h>
#include <esp_log.h>

/**
 * @class BT247GN
 * @brief A class for interacting with the BT247GN device using SPI communication.
 *
 * This class provides methods to initialize the BT247GN device, write data to it, and refresh its display.
 */
class BT247GN
{
public:
    BT247GN(gpio_num_t din, gpio_num_t clk, gpio_num_t cs, spi_host_device_t spi_num);
    BT247GN(spi_device_handle_t spi_device) : spi_device_(spi_device) {}
    void init();
    void test();
    void setbrightness(uint8_t brightness);
    void setsleep(bool en);

private:
    uint8_t dimming = 0;
    spi_device_handle_t spi_device_;

    void write_data8(uint8_t *dat, int len);

protected:
    uint8_t internal_gram[13][10] = {0};
    void refrash();
};

#endif