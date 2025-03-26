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
        .mode = 2,                 // Set the SPI mode to 3
        .clock_speed_hz = 1000000, // Set the clock speed to 1MHz
        .spics_io_num = cs,        // Set the chip select pin
        .queue_size = 7,
    };

    // Add the BT247GN device to the SPI bus with the specified configuration
    ESP_ERROR_CHECK(spi_bus_add_device(spi_num, &devcfg, &spi_device_));
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

void BT247GN::init()
{
    // uint8_t val = 0xFF;
    // write_data8(&val, sizeof val);
    refrash();
}

void BT247GN::setbrightness(uint8_t brightness)
{
    dimming = brightness * 8 / 100;
    if (dimming > 7)
        dimming = 7;
    // ESP_LOGI(TAG, "dimming %d", dimming);
}

void BT247GN::setsleep(bool en)
{
    if (en)
    {
        memset(internal_gram, 0, sizeof internal_gram);refrash();
    }
}

void BT247GN::refrash()
{
    internal_gram[0] = 0;
    write_data8(internal_gram, sizeof internal_gram);
}