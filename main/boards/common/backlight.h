#pragma once

#include <cstdint>
#include <functional>

#include <driver/gpio.h>
#include <esp_timer.h>
#include <esp_lcd_panel_io.h>

class Backlight
{
public:
    Backlight();
    ~Backlight();

    void RestoreBrightness();
    void SetBrightness(uint8_t brightness, bool permanent = false);
    inline uint8_t brightness() const { return brightness_; }

protected:
    void OnTransitionTimer();
    virtual void SetBrightnessImpl(uint8_t brightness) = 0;

    esp_timer_handle_t transition_timer_ = nullptr;
    uint8_t brightness_ = 0;
    uint8_t target_brightness_ = 0;
    uint8_t step_ = 1;
};

class PwmBacklight : public Backlight
{
public:
    PwmBacklight(gpio_num_t pin, bool output_invert = false);
    ~PwmBacklight();

    void SetBrightnessImpl(uint8_t brightness) override;
};

class OledBacklight : public Backlight
{
#define LCD_OPCODE_WRITE_CMD (0x02ULL)
    esp_lcd_panel_io_handle_t panel_io_;

public:
    OledBacklight(esp_lcd_panel_io_handle_t panel_io);
    ~OledBacklight();

    void SetBrightnessImpl(uint8_t brightness) override;
};
