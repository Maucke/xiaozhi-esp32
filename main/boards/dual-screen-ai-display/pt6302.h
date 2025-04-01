/*
 * Author: 施华锋
 * Date: 2025-03-21
 * Description: This header file defines the PT6324 class for communicating with the PT6324 device via SPI.
 */

#ifndef PT6302_H
#define PT6302_H

#include <driver/spi_master.h>
#include <driver/gpio.h>
#include <esp_log.h>

/**
 * @class PT6302
 * @brief A class for interacting with the PT6302 device using SPI communication.
 *
 * This class provides methods to initialize the PT6302 device, write data to it, and refresh its display.
 */
class PT6302
{
#define CGRAM_SIZE 8
#define SYMBOL_CGRAM_SIZE 5
public:
    enum class Mode
    {
        NORMAL = 0x00,
        ALLOFF = 0x01,
        ALLON = 0x02,
    };

    typedef union
    {
        struct
        {
            uint8_t number[10];
            uint8_t symbol[15];
            uint8_t cgram[SYMBOL_CGRAM_SIZE * 5];
            uint8_t cgram_number[(CGRAM_SIZE - SYMBOL_CGRAM_SIZE) * 5];
        };
        uint8_t array[10 + 15 + 5 * CGRAM_SIZE];
    } Gram;

    PT6302(gpio_num_t din, gpio_num_t clk, gpio_num_t cs, spi_host_device_t spi_num);
    PT6302(spi_device_handle_t spi_device) : spi_device_(spi_device) {}
    void init();
    void setbrightness(uint8_t brightness);
    void setsleep(bool en);
    void test();

private:
    uint8_t dimming = 7;
    spi_device_handle_t spi_device_;
    const unsigned int digits = 15;

    void write_data8(uint8_t *dat, int len);
    void write_dcram(int index, uint8_t *dat, int len);
    void write_adram(int index, uint8_t *dat, int len);
    void write_cgram(int index, uint8_t *dat, int len = 5);
    void write_dimming();
    void write_grnum(unsigned int amount = 15);
    void write_mode(PT6302::Mode mode);

protected:
    Gram internal_gram; // Display buffer 10 num + 15 ad + 5 cgram
    void refrash(Gram *gram);
    void refrash();
};

#endif
