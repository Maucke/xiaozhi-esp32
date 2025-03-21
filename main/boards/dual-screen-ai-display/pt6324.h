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
    /**
     * @brief Constructor for the PT6324 class.
     *
     * Initializes the PT6324 object with the specified GPIO pins and SPI host device.
     *
     * @param din The GPIO pin number for the data input line.
     * @param clk The GPIO pin number for the clock line.
     * @param cs The GPIO pin number for the chip select line.
     * @param spi_num The SPI host device number to use for communication.
     */
    PT6324(gpio_num_t din, gpio_num_t clk, gpio_num_t cs, spi_host_device_t spi_num);

    /**
     * @brief Constructor for the PT6324 class.
     *
     * Initializes the PT6324 object with an existing SPI device handle.
     *
     * @param spi_device The SPI device handle to use for communication.
     */
    PT6324(spi_device_handle_t spi_device) : spi_device_(spi_device) {}

    /**
     * @brief Initializes the PT6324 device.
     *
     * This method performs any necessary setup operations to prepare the PT6324 device for use.
     */
    void init();
    void setbrightness(uint8_t brightness);
    void setsleep(bool en);

private:
    uint8_t dimming = 0;
    spi_device_handle_t spi_device_;

    /**
     * @brief Writes data to the PT6324 device.
     *
     * Sends the specified data buffer of a given length to the PT6324 device via SPI.
     *
     * @param dat A pointer to the data buffer to be sent.
     * @param len The length of the data buffer in bytes.
     */
    void write_data8(uint8_t *dat, int len);

protected:
    bool dimmen = false;
    uint8_t gram[48] = {0};
    /**
     * @brief Refreshes the display of the PT6324 device.
     *
     * Updates the display of the PT6324 device with the data stored in the given graphics RAM buffer.
     *
     * @param gram A pointer to the graphics RAM buffer containing the display data.
     */
    void refrash(uint8_t *gram);
};

#endif