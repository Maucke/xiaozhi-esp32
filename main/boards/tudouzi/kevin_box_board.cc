#include "ml307_board.h"
#include "audio_codecs/box_audio_codec.h"
#include "display/ssd1306_display.h"
#include "application.h"
#include "button.h"
#include "config.h"
#include "axp2101.h"
#include "iot/thing_manager.h"
#include "led/single_led.h"
#include "assets/lang_config.h"
#include "font_awesome_symbols.h"

#include <esp_log.h>
#include <driver/gpio.h>
#include <driver/i2c_master.h>
#include <esp_timer.h>
#include <esp_sleep.h>
#include <esp_pm.h>

#define TAG "KevinBoxBoard"

LV_FONT_DECLARE(font_puhui_14_1);
LV_FONT_DECLARE(font_awesome_14_1);

class KevinBoxBoard : public Ml307Board {
private:
    i2c_master_bus_handle_t display_i2c_bus_;
    i2c_master_bus_handle_t codec_i2c_bus_;
    Axp2101* axp2101_ = nullptr;
    Button boot_button_;
    Button volume_up_button_;
    Button volume_down_button_;
    esp_timer_handle_t power_save_timer_ = nullptr;
    bool show_low_power_warning_ = false;
    bool sleep_mode_enabled_ = false;
    int power_save_ticks_ = 0;

    void InitializePowerSaveTimer() {
        esp_timer_create_args_t power_save_timer_args = {
            .callback = [](void *arg) {
                auto board = static_cast<KevinBoxBoard*>(arg);
                board->PowerSaveCheck();
            },
            .arg = this,
            .dispatch_method = ESP_TIMER_TASK,
            .name = "power_save_timer",
            .skip_unhandled_events = false,
        };
        ESP_ERROR_CHECK(esp_timer_create(&power_save_timer_args, &power_save_timer_));
        ESP_ERROR_CHECK(esp_timer_start_periodic(power_save_timer_, 1000000));
    }

    void PowerSaveCheck() {
        // 电池放电模式下，如果待机超过一定时间，则进入睡眠模式
        const int seconds_to_sleep = 120;
        auto& app = Application::GetInstance();
        if (app.GetDeviceState() != kDeviceStateIdle) {
            power_save_ticks_ = 0;
            return;
        }

        if (axp2101_->IsDischarging()) {
            // 电量低于 10% 时，显示低电量警告
            if (!show_low_power_warning_ && axp2101_->GetBatteryLevel() <= 10) {
                EnableSleepMode(false);
                app.Alert(Lang::Strings::WARNING, Lang::Strings::BATTERY_LOW, "sad", Lang::Sounds::P3_VIBRATION);
                show_low_power_warning_ = true;
            }
        } else {
            if (show_low_power_warning_) {
                app.DismissAlert();
                show_low_power_warning_ = false;
            }
        }

        power_save_ticks_++;
        if (power_save_ticks_ >= seconds_to_sleep) {
            EnableSleepMode(true);
        }
    }

    void EnableSleepMode(bool enable) {
        power_save_ticks_ = 0;
        if (!sleep_mode_enabled_ && enable) {
            ESP_LOGI(TAG, "Enabling sleep mode");
            auto display = GetDisplay();
            display->SetChatMessage("system", "");
            display->SetEmotion("sleepy");
            
            auto codec = GetAudioCodec();
            codec->EnableInput(false);

            esp_pm_config_t pm_config = {
                .max_freq_mhz = 240,
                .min_freq_mhz = 40,
                .light_sleep_enable = true,
            };
            esp_pm_configure(&pm_config);
            sleep_mode_enabled_ = true;
        } else if (sleep_mode_enabled_ && !enable) {
            esp_pm_config_t pm_config = {
                .max_freq_mhz = 240,
                .min_freq_mhz = 240,
                .light_sleep_enable = false,
            };
            esp_pm_configure(&pm_config);
            ESP_LOGI(TAG, "Disabling sleep mode");

            auto codec = GetAudioCodec();
            codec->EnableInput(true);
            sleep_mode_enabled_ = false;
        }
    }

    void Enable4GModule() {
        // Make GPIO HIGH to enable the 4G module
        gpio_config_t ml307_enable_config = {
            .pin_bit_mask = (1ULL << 4),
            .mode = GPIO_MODE_OUTPUT,
            .pull_up_en = GPIO_PULLUP_DISABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type = GPIO_INTR_DISABLE,
        };
        gpio_config(&ml307_enable_config);
        gpio_set_level(GPIO_NUM_4, 1);
    }

    void InitializeDisplayI2c() {
        i2c_master_bus_config_t bus_config = {
            .i2c_port = (i2c_port_t)0,
            .sda_io_num = DISPLAY_SDA_PIN,
            .scl_io_num = DISPLAY_SCL_PIN,
            .clk_source = I2C_CLK_SRC_DEFAULT,
            .glitch_ignore_cnt = 7,
            .intr_priority = 0,
            .trans_queue_depth = 0,
            .flags = {
                .enable_internal_pullup = 1,
            },
        };
        ESP_ERROR_CHECK(i2c_new_master_bus(&bus_config, &display_i2c_bus_));
    }

    void InitializeCodecI2c() {
        // Initialize I2C peripheral
        i2c_master_bus_config_t i2c_bus_cfg = {
            .i2c_port = (i2c_port_t)1,
            .sda_io_num = AUDIO_CODEC_I2C_SDA_PIN,
            .scl_io_num = AUDIO_CODEC_I2C_SCL_PIN,
            .clk_source = I2C_CLK_SRC_DEFAULT,
            .glitch_ignore_cnt = 7,
            .intr_priority = 0,
            .trans_queue_depth = 0,
            .flags = {
                .enable_internal_pullup = 1,
            },
        };
        ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_bus_cfg, &codec_i2c_bus_));
    }

    void InitializeButtons() {
        gpio_wakeup_enable(BOOT_BUTTON_GPIO, GPIO_INTR_LOW_LEVEL);
        gpio_wakeup_enable(VOLUME_UP_BUTTON_GPIO, GPIO_INTR_LOW_LEVEL);
        gpio_wakeup_enable(VOLUME_DOWN_BUTTON_GPIO, GPIO_INTR_LOW_LEVEL);
        esp_sleep_enable_gpio_wakeup();

        boot_button_.OnPressDown([this]() {
            EnableSleepMode(false);
            Application::GetInstance().StartListening();
        });
        boot_button_.OnPressUp([this]() {
            Application::GetInstance().StopListening();
        });

        volume_up_button_.OnClick([this]() {
            auto codec = GetAudioCodec();
            auto volume = codec->output_volume() + 10;
            if (volume > 100) {
                volume = 100;
            }
            codec->SetOutputVolume(volume);
            GetDisplay()->ShowNotification(Lang::Strings::VOLUME + std::to_string(volume));
        });

        volume_up_button_.OnLongPress([this]() {
            GetAudioCodec()->SetOutputVolume(100);
            GetDisplay()->ShowNotification(Lang::Strings::MAX_VOLUME);
        });

        volume_down_button_.OnClick([this]() {
            auto codec = GetAudioCodec();
            auto volume = codec->output_volume() - 10;
            if (volume < 0) {
                volume = 0;
            }
            codec->SetOutputVolume(volume);
            GetDisplay()->ShowNotification(Lang::Strings::VOLUME + std::to_string(volume));
        });

        volume_down_button_.OnLongPress([this]() {
            GetAudioCodec()->SetOutputVolume(0);
            GetDisplay()->ShowNotification(Lang::Strings::MUTED);
        });
    }

    // 物联网初始化，添加对 AI 可见设备
    void InitializeIot() {
        auto& thing_manager = iot::ThingManager::GetInstance();
        thing_manager.AddThing(iot::CreateThing("Speaker"));
        thing_manager.AddThing(iot::CreateThing("Battery"));
    }

public:
    KevinBoxBoard() : Ml307Board(ML307_TX_PIN, ML307_RX_PIN, 4096),
        boot_button_(BOOT_BUTTON_GPIO),
        volume_up_button_(VOLUME_UP_BUTTON_GPIO),
        volume_down_button_(VOLUME_DOWN_BUTTON_GPIO) {
        InitializeDisplayI2c();
        InitializeCodecI2c();
        axp2101_ = new Axp2101(codec_i2c_bus_, AXP2101_I2C_ADDR);

        Enable4GModule();

        InitializeButtons();
        InitializePowerSaveTimer();
        InitializeIot();
    }

    virtual Led* GetLed() override {
        static SingleLed led(BUILTIN_LED_GPIO);
        return &led;
    }

    virtual AudioCodec* GetAudioCodec() override {
        static BoxAudioCodec audio_codec(codec_i2c_bus_, AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE,
            AUDIO_I2S_GPIO_MCLK, AUDIO_I2S_GPIO_BCLK, AUDIO_I2S_GPIO_WS, AUDIO_I2S_GPIO_DOUT, AUDIO_I2S_GPIO_DIN,
            AUDIO_CODEC_PA_PIN, AUDIO_CODEC_ES8311_ADDR, AUDIO_CODEC_ES7210_ADDR, AUDIO_INPUT_REFERENCE);
        return &audio_codec;
    }

    virtual Display* GetDisplay() override {
        static Ssd1306Display display(display_i2c_bus_, DISPLAY_WIDTH, DISPLAY_HEIGHT, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y,
                                    &font_puhui_14_1, &font_awesome_14_1);
        return &display;
    }

    virtual bool GetBatteryLevel(int &level, bool& charging) override {
        static int last_level = 0;
        static bool last_charging = false;
        level = axp2101_->GetBatteryLevel();
        charging = axp2101_->IsCharging();
        if (level != last_level || charging != last_charging) {
            last_level = level;
            last_charging = charging;
            ESP_LOGI(TAG, "Battery level: %d, charging: %d", level, charging);
        }
        return true;
    }
};

DECLARE_BOARD(KevinBoxBoard);