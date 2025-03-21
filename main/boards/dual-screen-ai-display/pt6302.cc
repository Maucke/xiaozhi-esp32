/**
 * @file PT6302.cpp
 * Main source file for PT6302 library.
 *
 * Arduino library for communicating with the Princeton Technology PT6302 VFD Driver/Controller IC with Character RAM
 * This library only includes the ASCII compatible characters, all others will have to be printed using the hex codes in the datasheet
 *
 * Copyright (c) 2021 Arne van Iterson
 */

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "pt6302.h"
#include "string.h"

#define TAG "PT6302"

void PT6302::write_data8(uint8_t *dat, int len)
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

PT6302::PT6302(gpio_num_t din, gpio_num_t clk, gpio_num_t cs, spi_host_device_t spi_num)
{
    // Initialize the SPI bus configuration structure
    spi_bus_config_t buscfg = {0};

    // Log the initialization process
    ESP_LOGI(TAG, "Initialize VFD SPI bus");

    // Set the clock and data pins for the SPI bus
    buscfg.sclk_io_num = clk;
    buscfg.data0_io_num = din;

    // Set the maximum transfer size in bytes
    buscfg.max_transfer_sz = 256;

    // Initialize the SPI bus with the specified configuration
    ESP_ERROR_CHECK(spi_bus_initialize(spi_num, &buscfg, SPI_DMA_CH_AUTO));

    // Initialize the SPI device interface configuration structure
    spi_device_interface_config_t devcfg = {
        .mode = 3,                 // Set the SPI mode to 3
        .clock_speed_hz = 1000000, // Set the clock speed to 1MHz
        .spics_io_num = cs,        // Set the chip select pin
        .flags = SPI_DEVICE_BIT_LSBFIRST,
        .queue_size = 7,
    };

    // Add the PT6302 device to the SPI bus with the specified configuration
    ESP_ERROR_CHECK(spi_bus_add_device(spi_num, &devcfg, &spi_device_));
}

void PT6302::init()
{
    setmode(PT6302::Mode::NORMAL);
    setgrnum(digits);
    const uint8_t values[5] = {
        0, 1, 2, 3, 4};
    write_dcram(10, (uint8_t *)values, 5);
}

void PT6302::write_dcram(int index, uint8_t *dat, int len)
{
    uint8_t *command = new uint8_t[1 + len];
    command[0] = (index & 0xF) | 0x10;
    memcpy(command + 1, dat, len);
    write_data8(command, 1 + len);
    delete[] command;
}

void PT6302::write_cgram(int index, uint8_t *dat, int len)
{
    uint8_t *command = new uint8_t[1 + len];
    command[0] = (index & 0x7) | 0x20;
    memcpy(command + 1, dat, len);
    write_data8(command, 1 + len);
    delete[] command;
}

void PT6302::write_adram(int index, uint8_t *dat, int len)
{
    uint8_t *command = new uint8_t[1 + len];
    command[0] = (index & 0xF) | 0x30;
    memcpy(command + 1, dat, len);
    write_data8(command, 1 + len);
    delete[] command;
}

void PT6302::setdimming()
{
    uint8_t command = 0x50;
    command |= dimming & 0x7;
    write_data8(&command, 1);
}

void PT6302::setgrnum(unsigned int amount)
{
    uint8_t command = 0x60;
    command |= amount & 0x7;
    write_data8(&command, 1);
}

void PT6302::setmode(Mode mode)
{
    uint8_t command = 0x7 | (uint8_t)mode;
    write_data8(&command, 1);
    return;
}

void PT6302::refrash(Gram *gram)
{
    if (gram == nullptr)
        return;
    write_dcram(0, gram->number, 10);
    write_adram(0, gram->symbol, 15);
    write_cgram(0, gram->cgram, 25);
    setdimming();
}

void PT6302::setbrightness(uint8_t brightness)
{
    dimming = brightness * 8 / 100;
    if (dimming > 7)
        dimming = 7;
    return;
}
