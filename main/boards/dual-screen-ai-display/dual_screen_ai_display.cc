#include "wifi_board.h"
#include "display/lcd_display.h"
#include "esp_lcd_sh8601.h"
#include "esp_lcd_touch_ft5x06.h"
#include "font_awesome_symbols.h"
#include "audio_codecs/no_audio_codec.h"
#include <esp_sleep.h>
#include <vector>
#include "system_reset.h"
#include "application.h"
#include "button.h"
#include <sstream>
#include "led/single_led.h"
#include "config.h"
#include "iot/thing_manager.h"

#include <wifi_station.h>
#include <esp_log.h>
#include <esp_lcd_panel_vendor.h>
#include <driver/i2c_master.h>
#include <driver/spi_master.h>
#include "driver/rtc_io.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "assets/lang_config.h"
#include "bmp280.h"
#include <esp_wifi.h>
#include "rx8900.h"
#include "esp_sntp.h"
#include "settings.h"
#if SUB_DISPLAY_EN && FORD_VFD_EN
#include "ford_vfd.h"
#elif SUB_DISPLAY_EN && HNA_16MM65T_EN
#include "hna_16mm65t.h"
#elif SUB_DISPLAY_EN && FTB_13_BT_247GN_EN
#include "futaba_bt_247gn.h"
#elif SUB_DISPLAY_EN && BOE_48_1504FN_EN
#include "boe_48_1504fn.h"
#elif SUB_DISPLAY_EN && HUV_13SS16T_EN
#include "huv_13ss16t.h"
#endif
#include "spectrumdisplay.h"
#include "mpu6050.h"
#include "ina3221.h"
#include "pcf8574.h"
#include "physics.h"
#include "power_save_timer.h"

#define TAG "DualScreenAIDisplay"

LV_FONT_DECLARE(font_awesome_16_4);
LV_FONT_DECLARE(font_puhui_16_4);
LV_FONT_DECLARE(font_puhui_14_1);

#define LCD_BIT_PER_PIXEL (16)
#define LCD_OPCODE_WRITE_CMD (0x02ULL)

#define LCD_OPCODE_READ_CMD (0x03ULL)
#define LCD_OPCODE_WRITE_COLOR (0x32ULL)

static const sh8601_lcd_init_cmd_t vendor_specific_init[] = {
    {0x11, (uint8_t[]){0x00}, 0, 120},
    // {0x44, (uint8_t []){0x01, 0xD1}, 2, 0},
    // {0x35, (uint8_t []){0x00}, 1, 0},
    {0x34, (uint8_t[]){0x00}, 0, 0},
    {0x36, (uint8_t[]){0xF0}, 1, 0},
    {0x3A, (uint8_t[]){0x55}, 1, 0}, // 16bits-RGB565
    {0x2A, (uint8_t[]){0x00, 0x00, 0x02, 0x17}, 4, 0},
    {0x2B, (uint8_t[]){0x00, 0x00, 0x00, 0xEF}, 4, 0},
    {0x29, (uint8_t[]){0x00}, 0, 10},
    {0x51, (uint8_t[]){0x00}, 1, 0},
};

class CustomLcdDisplay : public QspiLcdDisplay, public Backlight
#if SUB_DISPLAY_EN && FORD_VFD_EN
    ,
                         public Led,
                         public FORD_VFD
#elif SUB_DISPLAY_EN && HNA_16MM65T_EN
    ,
                         public Led,
                         public HNA_16MM65T
#elif SUB_DISPLAY_EN && FTB_13_BT_247GN_EN
    ,
                         public Led,
                         public FTB_BT_247GN
#elif SUB_DISPLAY_EN && BOE_48_1504FN_EN
    ,
                         public Led,
                         public BOE_48_1504FN
#elif SUB_DISPLAY_EN && HUV_13SS16T_EN
    ,
                         public Led,
                         public HUV_13SS16T
#endif
{
private:
#if SUB_DISPLAY_EN && FORD_VFD_EN
    lv_display_t *subdisplay;
    lv_obj_t *sub_status_label_;
#endif

public:
    CustomLcdDisplay(esp_lcd_panel_io_handle_t io_handle,
                     esp_lcd_panel_handle_t panel_handle,
                     esp_lcd_touch_handle_t tp_handle,
                     int width,
                     int height,
                     int offset_x,
                     int offset_y,
                     bool mirror_x,
                     bool mirror_y,
                     bool swap_xy,
                     spi_device_handle_t spidevice = nullptr)
        : QspiLcdDisplay(io_handle, panel_handle,
                         width, height, offset_x, offset_y, mirror_x, mirror_y, swap_xy,
                         {
                             .text_font = &font_puhui_16_4,
                             .icon_font = &font_awesome_16_4,
                             .emoji_font = font_emoji_32_init(),
                         },
                         tp_handle)
#if SUB_DISPLAY_EN && FORD_VFD_EN
          ,
          FORD_VFD(spidevice)
#elif SUB_DISPLAY_EN && HNA_16MM65T_EN
          ,
          HNA_16MM65T(spidevice)
#elif SUB_DISPLAY_EN && BOE_48_1504FN_EN
          ,
          BOE_48_1504FN(spidevice)
#elif SUB_DISPLAY_EN && FTB_13_BT_247GN_EN
          ,
          FTB_BT_247GN(spidevice)
#elif SUB_DISPLAY_EN && HUV_13SS16T_EN
          ,
          HUV_13SS16T(spidevice)
#endif
    {
        DisplayLockGuard lock(this);
#if SUB_DISPLAY_EN && FORD_VFD_EN
        InitializeSubScreen();
        SetupSubUI();
#endif
    }

    void SetBrightnessImpl(uint8_t brightness) override
    {
        // ESP_LOGI(TAG, "brightness: %d", brightness);
#if SUB_DISPLAY_EN
        SetSubBacklight(brightness);
#endif
        DisplayLockGuard lock(this);
        uint8_t data[1] = {((uint8_t)((255 * brightness) / 100))};
        int lcd_cmd = 0x51;
        lcd_cmd &= 0xff;
        lcd_cmd <<= 8;
        lcd_cmd |= LCD_OPCODE_WRITE_CMD << 24;
        esp_lcd_panel_io_tx_param(panel_io_, lcd_cmd, &data, sizeof(data));
    }

    void SetSleep(bool en)
    {
        DisplayLockGuard lock(this);
        ESP_LOGI(TAG, "LCD sleep");
        uint8_t data[1] = {1};
        int lcd_cmd = 0x10;
        lcd_cmd &= 0xff;
        lcd_cmd <<= 8;
        lcd_cmd |= LCD_OPCODE_WRITE_CMD << 24;
        if (en)
        {
            data[0] = 1;
            esp_lcd_panel_io_tx_param(panel_io_, lcd_cmd, &data, sizeof(data));
        }
        else
        {
            data[0] = 0;
            esp_lcd_panel_io_tx_param(panel_io_, lcd_cmd, &data, sizeof(data));
        }

#if SUB_DISPLAY_EN
        SetSubSleep(en);
#endif
    }

    virtual void SetChatMessage(const char *role, const char *content) override
    {
        if (content != nullptr && *content == '\0')
            return;
#if SUB_DISPLAY_EN && FORD_VFD_EN
        SetSubContent(content);
#elif SUB_DISPLAY_EN && FTB_13_BT_247GN_EN
        SetSubContent(role, content);
#elif SUB_DISPLAY_EN && BOE_48_1504FN_EN
        SetSubContent(content);
#endif
        QspiLcdDisplay::SetChatMessage(role, content);
    }

#if SUB_DISPLAY_EN && FORD_VFD_EN

    virtual void ShowNotification(const std::string &notification, int duration_ms = 3000) override
    {
        DisplayLockGuard lock(this);
        if (notification_label_ == nullptr)
        {
            return;
        }
        lv_label_set_text(notification_label_, notification.c_str());
        lv_obj_clear_flag(notification_label_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(status_label_, LV_OBJ_FLAG_HIDDEN);

        esp_timer_stop(notification_timer_);
        ESP_ERROR_CHECK(esp_timer_start_once(notification_timer_, duration_ms * 1000));
        SetSubContent(notification.c_str());
    }

    void SetSubBacklight(uint8_t brightness)
    {
        setbrightness(brightness);
    }

    static void sub_disp_flush(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map)
    {
        auto display = Board::GetInstance().GetDisplay();
#if 0
        int32_t x, y;
        uint8_t byte_index = 0;
        uint8_t bit_index = 0;

        for (y = area->y1; y <= area->y2; y++)
        {
            for (x = area->x1; x <= area->x2; x++)
            {
                uint8_t color = (px_map[byte_index] >> (7 - bit_index)) & 0x01;

                display->DrawPoint(x, y, color);

                bit_index++;
                if (bit_index == 8)
                {
                    bit_index = 0;
                    byte_index++;
                }
            }
        }
#else
        uint16_t *buf16 = (uint16_t *)px_map;
        int32_t x, y;
        for (y = area->y1; y <= area->y2; y++)
        {
            for (x = area->x1; x <= area->x2; x++)
            {
                display->DrawPoint(x, y, *buf16);
                buf16++;
            }
        }
#endif
        lv_display_flush_ready(disp);
    }

    void InitializeSubScreen()
    {
        // 创建显示器对象
        subdisplay = lv_display_create(FORD_WIDTH, FORD_HEIGHT);
        if (subdisplay == NULL)
        {
            ESP_LOGI(TAG, "Failed to create subdisplay");
            return;
        }
        lv_display_set_flush_cb(subdisplay, sub_disp_flush);
#if 0
        lv_display_set_color_format(subdisplay, LV_COLOR_FORMAT_I1);
        static uint16_t buf1[FORD_WIDTH * FORD_HEIGHT / 8];
        static uint16_t buf2[FORD_WIDTH * FORD_HEIGHT / 8];
        lv_display_set_buffers(subdisplay, buf1, buf2, sizeof(buf1), LV_DISPLAY_RENDER_MODE_FULL);
#else
        static uint16_t buf[FORD_WIDTH * FORD_HEIGHT];
        lv_display_set_buffers(subdisplay, buf, nullptr, sizeof(buf), LV_DISPLAY_RENDER_MODE_PARTIAL);
#endif
        LV_LOG_INFO("Subscreen initialized successfully");
    }

    void SetSubContent(const char *content)
    {
        DisplayLockGuard lock(this);
        lv_label_set_text(sub_status_label_, content);
    }

    void SetupSubUI()
    {
        DisplayLockGuard lock(this);

        ESP_LOGI(TAG, "SetupSubUI");
        auto screen = lv_disp_get_scr_act(subdisplay);

        lv_obj_set_style_text_font(screen, &font_puhui_14_1, 0);
        lv_obj_set_style_text_color(screen, lv_color_white(), 0);
        lv_obj_set_style_bg_color(screen, lv_color_black(), 0);
        lv_obj_set_style_pad_all(screen, 0, 0);
        lv_obj_set_scrollbar_mode(screen, LV_SCROLLBAR_MODE_OFF);

        lv_obj_t *sub_container_ = lv_obj_create(screen);
        lv_obj_set_style_bg_color(sub_container_, lv_color_black(), 0);
        lv_obj_set_width(sub_container_, FORD_WIDTH);
        lv_obj_set_flex_flow(sub_container_, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_style_border_width(sub_container_, 0, 0);
        lv_obj_set_style_pad_all(sub_container_, 0, 0);

        lv_obj_t *sub_status_bar_ = lv_obj_create(sub_container_);
        lv_obj_set_style_bg_color(sub_status_bar_, lv_color_black(), 0);
        lv_obj_set_width(sub_status_bar_, FORD_WIDTH);
        lv_obj_set_style_radius(sub_status_bar_, 0, 0);

        lv_obj_set_flex_flow(sub_status_bar_, LV_FLEX_FLOW_ROW);
        lv_obj_set_style_border_width(sub_status_bar_, 0, 0);
        lv_obj_set_style_pad_all(sub_status_bar_, 0, 0);

        sub_status_label_ = lv_label_create(sub_status_bar_);
        lv_obj_set_style_border_width(sub_status_label_, 0, 0);
        lv_obj_set_style_bg_color(sub_status_label_, lv_color_black(), 0);
        lv_obj_set_style_pad_all(sub_status_label_, 0, 0);
        lv_obj_set_width(sub_status_label_, FORD_WIDTH);
        lv_label_set_long_mode(sub_status_label_, LV_LABEL_LONG_SCROLL_CIRCULAR);
        lv_label_set_text(sub_status_label_, "正在初始化");
        lv_obj_set_style_text_align(sub_status_label_, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_scrollbar_mode(sub_status_label_, LV_SCROLLBAR_MODE_OFF);
    }

    virtual void DrawPoint(int x, int y, uint8_t dot) override
    {
        draw_point(x, y, dot);
    }

    virtual void OnStateChanged() override
    {
        auto &app = Application::GetInstance();
        auto device_state = app.GetDeviceState();
        symbolhelper(DAB, false);
        symbolhelper(FM, false);
        symbolhelper(BT, false);
        symbolhelper(TA, false);
        symbolhelper(CD0, false);
        symbolhelper(CD1, false);
        symbolhelper(CD2, false);
        symbolhelper(CD3, false);
        symbolhelper(IPOD, false);
        symbolhelper(UDISK, false);
        switch (device_state)
        {
        case kDeviceStateStarting:
            symbolhelper(FM, true);
            break;
        case kDeviceStateWifiConfiguring:
            symbolhelper(BT, true);
            break;
        case kDeviceStateIdle:
            setmode(FORD_VFD::FFT);
            return;
        case kDeviceStateConnecting:
            symbolhelper(TA, true);
            break;
        case kDeviceStateListening:
            if (app.IsVoiceDetected())
            {
                symbolhelper(CD0, true);
                symbolhelper(CD1, true);
                symbolhelper(CD2, true);
                symbolhelper(CD3, true);
            }
            else
            {
                symbolhelper(CD0, true);
                symbolhelper(CD1, true);
            }
            break;
        case kDeviceStateSpeaking:
            symbolhelper(IPOD, true);
            break;
        case kDeviceStateUpgrading:
            symbolhelper(UDISK, true);
            break;
        case kDeviceStateActivating:
            symbolhelper(DAB, true);
            break;
        default:
            setmode(FORD_VFD::CONTENT);
            ESP_LOGE(TAG, "Invalid led strip event: %d", device_state);
            return;
        }
        setmode(FORD_VFD::CONTENT);
    }

#if CONFIG_USE_FFT_EFFECT
    virtual void SpectrumShow(float *buf, int size) override
    {
        _spectrum->inputFFTData(buf, size);
    }
#endif
    void SetSubSleep(bool en = true)
    {
        setsleep(en);
    }
#elif SUB_DISPLAY_EN && FTB_13_BT_247GN_EN

    bool isPureEnglish(const char *content)
    {
        int len = strlen(content);
        for (int i = 0; i < len; i++)
        {
            if (!(content[i] >= ' ' && content[i] <= 'z'))
            {
                return false;
            }
        }
        return true;
    }
    void SetSubContent(const char *role, const char *content)
    {
        // if (!isPureEnglish(content))
        //     return;
        if (strcmp(role, "user") == 0)
        {
            pixel_show(1, content);
        }
        else
        {
            pixel_show(0, content);
        }
    }
#if CONFIG_USE_FFT_EFFECT
    virtual void SpectrumShow(float *buf, int size) override
    {
        spectrum_show(buf, size);
    }
#endif
public:
    virtual void Notification(const std::string &content, int timeout = 2000) override
    {
        noti_show(0, (char *)content.c_str(), content.size(), true, FTB_BT_247GN::UP2DOWN, timeout);
    }

    void SetSubSleep(bool en = true)
    {
        setsleep(en);
    }

    void SetSubBacklight(uint8_t brightness)
    {
        setbrightness(brightness);
    }

    virtual void OnStateChanged() override
    {
        auto &app = Application::GetInstance();
        auto device_state = app.GetDeviceState();
        symbolhelper(Hand, false);
        symbolhelper(Folder, false);
        symbolhelper(Aac, false);
        symbolhelper(Single, false);
        symbolhelper(Track, false);
        symbolhelper(Pause, false);
        symbolhelper(Play, false);
        symbolhelper(Cd, false);
        symbolhelper(All, false);
        switch (device_state)
        {
        case kDeviceStateStarting:
            symbolhelper(Hand, true);
            break;
        case kDeviceStateWifiConfiguring:
            symbolhelper(Folder, true);
            break;
        case kDeviceStateIdle:
            symbolhelper(Pause, true);
            break;
        case kDeviceStateConnecting:
            symbolhelper(Single, true);
            break;
        case kDeviceStateListening:
            if (app.IsVoiceDetected())
            {
                symbolhelper(Aac, true);
                symbolhelper(Track, true);
            }
            else
            {
                symbolhelper(Aac, true);
            }
            break;
        case kDeviceStateSpeaking:
            symbolhelper(Pause, false);
            symbolhelper(Play, true);
            break;
        case kDeviceStateUpgrading:
            symbolhelper(Cd, true);
            break;
        case kDeviceStateActivating:
            symbolhelper(All, true);
            break;
        default:
            ESP_LOGE(TAG, "Invalid led strip event: %d", device_state);
            return;
        }
    }
#elif SUB_DISPLAY_EN && BOE_48_1504FN_EN

    void SetSubContent(const char *content)
    {
        int len = strlen(content);
        noti_show(content, len * 300);
    }

#if CONFIG_USE_FFT_EFFECT
    virtual void SpectrumShow(float *buf, int size) override
    {
        spectrum_show(buf, size);
    }
#endif

    virtual void Notification(const std::string &content, int timeout = 2000) override
    {
        noti_show((char *)content.c_str(), timeout);
    }

    void SetSubSleep(bool en = true)
    {
        setsleep(en);
    }

    void SetSubBacklight(uint8_t brightness)
    {
        setbrightness(brightness);
    }

    virtual void OnStateChanged() override
    {
        static int count = 0;
        auto &app = Application::GetInstance();
        auto device_state = app.GetDeviceState();
        symbolhelper(LD_KARAOKE, false);
        symbolhelper(NET, false);
        symbolhelper(D_PAUSE, false);
        symbolhelper(RD_V_FADE, false);
        symbolhelper(D_OUTLINE_0, false);
        symbolhelper(D_OUTLINE_1, false);
        symbolhelper(D_OUTLINE_2, false);
        symbolhelper(D_OUTLINE_3, false);
        symbolhelper(RD_MIC, false);
        symbolhelper(RD_USB, false);
        symbolhelper(PLAY, false);
        switch (device_state)
        {
        case kDeviceStateStarting:
            symbolhelper(LD_KARAOKE, true);
            break;
        case kDeviceStateWifiConfiguring:
            symbolhelper(NET, true);
            break;
        case kDeviceStateIdle:
            symbolhelper(D_PAUSE, true);
            break;
        case kDeviceStateConnecting:
            symbolhelper(RD_V_FADE, true);
            break;
        case kDeviceStateListening:
            if (app.IsVoiceDetected())
            {
                symbolhelper(RD_MIC, true);
                count = (count + 1) % 3;
                if (count > 0)
                    symbolhelper(D_OUTLINE_0, true);
                if (count > 1)
                    symbolhelper(D_OUTLINE_1, true);
                if (count > 2)
                    symbolhelper(D_OUTLINE_2, true);
            }
            else
            {
                symbolhelper(RD_MIC, true);
            }
            break;
        case kDeviceStateSpeaking:
            symbolhelper(PLAY, true);
            break;
        case kDeviceStateUpgrading:
            symbolhelper(RD_USB, true);
            break;
        case kDeviceStateActivating:
            symbolhelper(D_OUTLINE_3, true);
            break;
        default:
            ESP_LOGE(TAG, "Invalid led strip event: %d", device_state);
            return;
        }
    }
#elif SUB_DISPLAY_EN && HUV_13SS16T_EN

#if CONFIG_USE_FFT_EFFECT
    virtual void SpectrumShow(float *buf, int size) override
    {
        spectrum_show(buf, size);
    }
#endif

    virtual void Notification(const std::string &content, int timeout = 2000) override
    {
        noti_show((char *)content.c_str(), timeout);
    }

    void SetSubSleep(bool en = true)
    {
        setsleep(en);
    }

    void SetSubBacklight(uint8_t brightness)
    {
        setbrightness(brightness);
    }

    virtual void OnStateChanged() override
    {
        auto &app = Application::GetInstance();
        auto device_state = app.GetDeviceState();
        switch (device_state)
        {
        case kDeviceStateStarting:
            break;
        case kDeviceStateWifiConfiguring:
            break;
        case kDeviceStateIdle:
            break;
        case kDeviceStateConnecting:
            break;
        case kDeviceStateListening:
            if (app.IsVoiceDetected())
            {
            }
            else
            {
            }
            break;
        case kDeviceStateSpeaking:
            break;
        case kDeviceStateUpgrading:
            break;
        case kDeviceStateActivating:
            break;
        default:
            ESP_LOGE(TAG, "Invalid led strip event: %d", device_state);
            return;
        }
    }
#elif SUB_DISPLAY_EN && HNA_16MM65T_EN
    void SetSubSleep(bool en = true)
    {
        setsleep(en);
    }

    void SetSubBacklight(uint8_t brightness)
    {
        setbrightness(brightness);
    }

    virtual void OnStateChanged() override
    {
        auto &app = Application::GetInstance();
        auto device_state = app.GetDeviceState();
        symbolhelper(GIGA, false);
        symbolhelper(MONO, false);
        symbolhelper(STEREO, false);
        symbolhelper(REC_1, false);
        symbolhelper(REC_2, false);
        symbolhelper(USB1, false);
        dotshelper(DOT_MATRIX_FILL);
        symbolhelper(LBAR_RBAR, false);
        switch (device_state)
        {
        case kDeviceStateStarting:
            symbolhelper(GIGA, true);
            break;
        case kDeviceStateWifiConfiguring:
            symbolhelper(MONO, true);
            break;
        case kDeviceStateIdle:
            break;
        case kDeviceStateConnecting:
            symbolhelper(STEREO, true);
            break;
        case kDeviceStateListening:
            if (app.IsVoiceDetected())
            {
                symbolhelper(REC_1, true);
                symbolhelper(REC_2, true);
            }
            else
            {
                symbolhelper(REC_2, true);
            }
            break;
        case kDeviceStateSpeaking:
            dotshelper(DOT_MATRIX_NEXT);
            break;
        case kDeviceStateUpgrading:
            symbolhelper(USB1, true);
            break;
        case kDeviceStateActivating:
            symbolhelper(LBAR_RBAR, true);
            break;
        default:
            ESP_LOGE(TAG, "Invalid led strip event: %d", device_state);
            return;
        }
    }

    virtual void Notification(const std::string &content, int timeout = 2000) override
    {
        noti_show(0, (char *)content.c_str(), 10, HNA_16MM65T::UP2DOWN, timeout);
    }

#if CONFIG_USE_FFT_EFFECT
    virtual void SpectrumShow(float *buf, int size) override
    {
        spectrum_show(buf, size);
    }
#endif
#endif
};

class DualScreenAIDisplay : public WifiBoard
{
private:
    Button *touch_button_ = NULL;
    CustomLcdDisplay *display_ = NULL;
    adc_oneshot_unit_handle_t adc_handle;
    adc_cali_handle_t bat_adc_cali_handle, dimm_adc_cali_handle;
    i2c_bus_handle_t i2c_bus = NULL;
    bmp280_handle_t bmp280 = NULL;
    rx8900_handle_t rx8900 = NULL;
    mpu6050_handle_t mpu6050 = NULL;
    esp_lcd_panel_io_handle_t panel_io_ = NULL;
    PowerSaveTimer *power_save_timer_;
#if ESP_DUAL_DISPLAY_V2
    PCF8574 *pcf8574 = NULL;
    INA3221 *ina3221 = NULL;
#endif
    void InitializePowerSaveTimer()
    {
        power_save_timer_ = new PowerSaveTimer(-1, 60, 300);
        power_save_timer_->OnEnterSleepMode([this]()
                                            {
        ESP_LOGI(TAG, "Enabling sleep mode");
        display_->SetChatMessage("system", "");
        display_->SetEmotion("sleepy");
#if SUB_DISPLAY_EN && HNA_16MM65T_EN
                display_->Notification("  sleepy  ", 4000);
#elif SUB_DISPLAY_EN && BOE_48_1504FN_EN
                display_->symbolhelper(BOE_48_1504FN::SLEEP, true);
                display_->Notification("  sleepy  ", 4000);
#elif SUB_DISPLAY_EN && FTB_13_BT_247GN_EN
                display_->Notification("  sleepy  ", 4000);
#elif SUB_DISPLAY_EN && HUV_13SS16T_EN
                display_->Notification("  sleepy  ", 4000);
#endif
        GetBacklight()->SetBrightness(1); });
        power_save_timer_->OnExitSleepMode([this]()
                                           {
        display_->SetChatMessage("system", "");
        display_->SetEmotion("neutral");
        GetBacklight()->RestoreBrightness(); });
        power_save_timer_->OnShutdownRequest([this]()
                                             {
        ESP_LOGI(TAG, "Shutting down");
#if ESP_DUAL_DISPLAY_V2
                pcf8574->writeGpio(MIC_EN, 0);
                pcf8574->writeGpio(OLED_EN, 0);
                pcf8574->writeGpio(VFD_EN, 0);
                pcf8574->writeGpio(SD_EN, 0);
                pcf8574->writeGpio(TPS_PS, 0);
#endif
                mpu6050_sleep(mpu6050); // deactive the mpu, because its lost power to fast
                GetBacklight()->SetBrightness(0);
                display_->SetSleep(true);
                vTaskDelay(pdMS_TO_TICKS(100));
                if (PIN_NUM_VFD_EN != GPIO_NUM_NC)
                {
                    ESP_LOGI(TAG, "Disable amoled power");
                    gpio_set_level(PIN_NUM_LCD_POWER, 0);
                }
        
                if (PIN_NUM_VFD_EN != GPIO_NUM_NC)
                {
                    ESP_LOGI(TAG, "Disable VFD power");
                    gpio_set_level(PIN_NUM_VFD_EN, 0);
                }
        
                if (PIN_NUM_POWER_EN != GPIO_NUM_NC)
                {
                    ESP_LOGI(TAG, "Disable force power");
                    gpio_set_level(PIN_NUM_POWER_EN, 0);
                }
                i2c_bus_delete(&i2c_bus);
#if ESP_DUAL_DISPLAY_V2
                rtc_gpio_pullup_en(TOUCH_INT_NUM);
                rtc_gpio_pullup_en(WAKE_INT_NUM);
                rtc_gpio_pullup_en(TOUCH_BUTTON_GPIO);
                esp_sleep_enable_ext1_wakeup((1ULL << TOUCH_INT_NUM) | (1 << WAKE_INT_NUM) | (1 << TOUCH_BUTTON_GPIO), ESP_EXT1_WAKEUP_ANY_LOW);
            // rtc_gpio_pulldown_en(PIN_NUM_VCC_DECT);
            // esp_sleep_enable_ext0_wakeup(PIN_NUM_VCC_DECT, 1);
#else
                rtc_gpio_pullup_en(TOUCH_INT_NUM);
                esp_sleep_enable_ext1_wakeup((1ULL << TOUCH_INT_NUM), ESP_EXT1_WAKEUP_ANY_LOW);
#endif
                // rtc_gpio_pullup_en(TOUCH_BUTTON_GPIO);
                // esp_sleep_enable_ext0_wakeup(TOUCH_BUTTON_GPIO, 0);
                esp_deep_sleep_start(); });
        power_save_timer_->SetEnabled(true);
    }

    void InitializeI2c()
    {
        i2c_config_t conf = {
            .mode = I2C_MODE_MASTER,
            .sda_io_num = IIC_SDA_NUM,
            .scl_io_num = IIC_SCL_NUM,
            .sda_pullup_en = GPIO_PULLUP_ENABLE,
            .scl_pullup_en = GPIO_PULLUP_ENABLE,
            .master = {0},
            .clk_flags = 0,
        };
        conf.master.clk_speed = 400000,
        i2c_bus = i2c_bus_create(IIC_MASTER_NUM, &conf);
        if (i2c_bus == nullptr)
            return;
        uint8_t device_addresses[256];
        i2c_bus_scan(i2c_bus, device_addresses, 255);

        bmp280 = bmp280_create(i2c_bus, BMP280_I2C_ADDRESS_DEFAULT);
        esp_err_t ret = bmp280_default_init(bmp280);
        ESP_LOGI(TAG, "bmp280_default_init:%d", ret);
        rx8900 = rx8900_create(i2c_bus, RX8900_I2C_ADDRESS_DEFAULT);
        ret = rx8900_default_init(rx8900);
        ESP_LOGI(TAG, "rx8900_default_init:%d", ret);
        if (ret == ESP_OK)
        {
            struct tm time_user;
            ret = rx8900_read_time(rx8900, &time_user);
            ESP_LOGI(TAG, "rx8900_read_time:%d", ret);
            if (ret == ESP_OK)
            {
                setenv("TZ", DEFAULT_TIMEZONE, 1);
                tzset();
                ESP_LOGI(TAG, "Sync system time");
                time_t timestamp = mktime(&time_user);
                struct timeval tv;
                tv.tv_sec = timestamp;
                tv.tv_usec = 0;
                if (settimeofday(&tv, NULL) == ESP_FAIL)
                {
                    ESP_LOGI(TAG, "Failed to set system time");
                    return;
                }
            }
        }

        mpu6050 = mpu6050_create(i2c_bus, MPU6050_I2C_ADDRESS);
        ESP_LOGI(TAG, "mpu6050_init:%d", mpu6050_init(mpu6050));
        mpu6050_enable_motiondetection(mpu6050, 5, 20);

#if ESP_DUAL_DISPLAY_V2
        pcf8574 = new PCF8574(i2c_bus);
        ina3221 = new INA3221(i2c_bus);
        ESP_LOGD(TAG, "ina3221 begin: %d", ina3221->begin());
        float voltage = 0.0f, current = 0.0f;
        for (size_t i = 0; i < 3; i++)
        {
            ina3221->setShuntRes(i, 10);
            voltage = ina3221->getBusVoltage(i);
            current = ina3221->getCurrent(i);
            ESP_LOGI(TAG, "channel: %d, voltage: %.2fv, current: %.2fa", i, voltage, current);
        }
#endif
    }

    void InitializeButtons()
    {
        touch_button_ = new Button(TOUCH_BUTTON_GPIO);
        // boot_button_.OnClick([this]()
        //                      {
        //     auto& app = Application::GetInstance();
        //      if (app.GetDeviceState() == kDeviceStateStarting && !WifiStation::GetInstance().IsConnected()) {
        //         ResetWifiConfiguration();
        //     }
        //     app.ToggleChatState(); });
        //         touch_button_->OnLongPress([this]
        //                                    {
        //                                     ESP_LOGI(TAG, "Button long pressed, Deep sleep");
        //                                      Sleep(); });
        //         touch_button_->OnClick([this]
        //                                {
        // #if SUB_DISPLAY_EN && FTB_13_BT_247GN_EN
        //                                    static int fonttype = 0;
        //                                    fonttype++;
        //                                    display_->set_fonttype(fonttype);
        // #endif
        //                                });

        touch_button_->OnClick([this]()
                               {
            power_save_timer_->WakeUp();
            auto& app = Application::GetInstance();
            if (app.GetDeviceState() == kDeviceStateStarting && !WifiStation::GetInstance().IsConnected()) {
                ResetWifiConfiguration();
            }
            app.ToggleChatState(); });
    }

    static void tp_interrupt_callback(esp_lcd_touch_handle_t tp)
    {
        // auto &app = Application::GetInstance();
        // app.ToggleChatState();
    }

    void InitializeDisplay()
    {
        // Initialize the SPI bus configuration structure
        spi_bus_config_t buscfg = {0};
        spi_device_handle_t spi_device = nullptr;

#if SUB_DISPLAY_EN

        // Log the initialization process
        ESP_LOGI(TAG, "Initialize VFD SPI bus");
        if (PIN_NUM_VFD_RE != GPIO_NUM_NC)
        {
            gpio_set_direction(PIN_NUM_VFD_RE, GPIO_MODE_OUTPUT);
            gpio_set_level(PIN_NUM_VFD_RE, 1);
        }
        // Set the clock and data pins for the SPI bus
        buscfg.sclk_io_num = PIN_NUM_VFD_PCLK;
        buscfg.data0_io_num = PIN_NUM_VFD_DATA0;
#endif
#if SUB_DISPLAY_EN && FORD_VFD_EN

        // Set the maximum transfer size in bytes
        buscfg.max_transfer_sz = 1024;

        // Initialize the SPI device interface configuration structure
        spi_device_interface_config_t devcfg = {
            .mode = 0,                      // Set the SPI mode to 3
            .clock_speed_hz = 600000,       // Set the clock speed to 1MHz
            .spics_io_num = PIN_NUM_VFD_CS, // Set the chip select pin
            .flags = 0,
            .queue_size = 7,
        };

        // Initialize the SPI bus with the specified configuration
        ESP_ERROR_CHECK(spi_bus_initialize(VFD_HOST, &buscfg, SPI_DMA_CH_AUTO));

        // Add the PT6324 device to the SPI bus with the specified configuration
        ESP_ERROR_CHECK(spi_bus_add_device(VFD_HOST, &devcfg, &spi_device));
#elif SUB_DISPLAY_EN && HNA_16MM65T_EN

        // Set the maximum transfer size in bytes
        buscfg.max_transfer_sz = 256;

        // Initialize the SPI device interface configuration structure
        spi_device_interface_config_t devcfg = {
            .mode = 3,                      // Set the SPI mode to 3
            .clock_speed_hz = 1000000,      // Set the clock speed to 1MHz
            .spics_io_num = PIN_NUM_VFD_CS, // Set the chip select pin
            .flags = SPI_DEVICE_BIT_LSBFIRST,
            .queue_size = 7,
        };

        // Initialize the SPI bus with the specified configuration
        ESP_ERROR_CHECK(spi_bus_initialize(VFD_HOST, &buscfg, SPI_DMA_CH_AUTO));

        // Add the PT6324 device to the SPI bus with the specified configuration
        ESP_ERROR_CHECK(spi_bus_add_device(VFD_HOST, &devcfg, &spi_device));
#elif SUB_DISPLAY_EN && FTB_13_BT_247GN_EN

        // Set the maximum transfer size in bytes
        buscfg.max_transfer_sz = 256;

        // Initialize the SPI device interface configuration structure
        spi_device_interface_config_t devcfg = {
            .mode = 0,                      // Set the SPI mode to 1
            .clock_speed_hz = 12000000,     // Set the clock speed to 1MHz
            .spics_io_num = PIN_NUM_VFD_CS, // Set the chip select pin
            .queue_size = 7,
        };

        // Initialize the SPI bus with the specified configuration
        ESP_ERROR_CHECK(spi_bus_initialize(VFD_HOST, &buscfg, SPI_DMA_CH_AUTO));

        // Add the PT6324 device to the SPI bus with the specified configuration
        ESP_ERROR_CHECK(spi_bus_add_device(VFD_HOST, &devcfg, &spi_device));
#elif SUB_DISPLAY_EN && (BOE_48_1504FN_EN || HUV_13SS16T)

        // Set the maximum transfer size in bytes
        buscfg.max_transfer_sz = 256;

        // Initialize the SPI device interface configuration structure
        spi_device_interface_config_t devcfg = {
            .mode = 3,                      // Set the SPI mode to 3
            .clock_speed_hz = 200000,       // Set the clock speed to 1MHz
            .spics_io_num = PIN_NUM_VFD_CS, // Set the chip select pin
            .flags = SPI_DEVICE_BIT_LSBFIRST,
            .queue_size = 7,
        };

        // Initialize the SPI bus with the specified configuration
        ESP_ERROR_CHECK(spi_bus_initialize(VFD_HOST, &buscfg, SPI_DMA_CH_AUTO));

        // Add the PT6324 device to the SPI bus with the specified configuration
        ESP_ERROR_CHECK(spi_bus_add_device(VFD_HOST, &devcfg, &spi_device));
#endif
#if ESP_DUAL_DISPLAY_V2
        pcf8574->writeGpio(OLED_RST, 0);
        vTaskDelay(pdMS_TO_TICKS(120));
        pcf8574->writeGpio(OLED_RST, 1);
#endif
        ESP_LOGI(TAG, "Initialize OLED SPI bus");
        buscfg.sclk_io_num = PIN_NUM_LCD_PCLK;
        buscfg.data0_io_num = PIN_NUM_LCD_DATA0;
        buscfg.data1_io_num = PIN_NUM_LCD_DATA1;
        buscfg.data2_io_num = PIN_NUM_LCD_DATA2;
        buscfg.data3_io_num = PIN_NUM_LCD_DATA3;
        buscfg.max_transfer_sz = DISPLAY_WIDTH * DISPLAY_HEIGHT * sizeof(uint16_t);
        buscfg.flags = SPICOMMON_BUSFLAG_QUAD;
        ESP_ERROR_CHECK(spi_bus_initialize(LCD_HOST, &buscfg, SPI_DMA_CH_AUTO));

        esp_lcd_panel_io_handle_t panel_io = nullptr;
        esp_lcd_panel_handle_t panel = nullptr;
        // 液晶屏控制IO初始化
        ESP_LOGD(TAG, "Install panel IO");
        esp_lcd_panel_io_spi_config_t io_config = SH8601_PANEL_IO_QSPI_CONFIG(
            PIN_NUM_LCD_CS,
            nullptr,
            nullptr);
        ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(LCD_HOST, &io_config, &panel_io));

        // 初始化液晶屏驱动芯片
        ESP_LOGD(TAG, "Install LCD driver");
        sh8601_vendor_config_t vendor_config = {
            .init_cmds = &vendor_specific_init[0],
            .init_cmds_size = sizeof(vendor_specific_init) / sizeof(sh8601_lcd_init_cmd_t),
            .flags = {
                .use_qspi_interface = 1,
            }};

        const esp_lcd_panel_dev_config_t panel_config = {
            .reset_gpio_num = PIN_NUM_LCD_RST,
            .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
            .data_endian = LCD_RGB_DATA_ENDIAN_BIG,
            .bits_per_pixel = LCD_BIT_PER_PIXEL,
            .flags = {
                .reset_active_high = 0,
            },
            .vendor_config = &vendor_config,
        };
        ESP_ERROR_CHECK(esp_lcd_new_panel_sh8601(panel_io, &panel_config, &panel));
        panel_io_ = panel_io;
        ESP_ERROR_CHECK(esp_lcd_panel_reset(panel));
        ESP_ERROR_CHECK(esp_lcd_panel_init(panel));
        esp_lcd_panel_invert_color(panel, false);
        esp_lcd_panel_swap_xy(panel, DISPLAY_SWAP_XY);
        esp_lcd_panel_mirror(panel, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y);
        esp_lcd_panel_disp_on_off(panel, true);

        esp_lcd_touch_handle_t tp = nullptr;
#if USE_TOUCH
        ESP_LOGI(TAG, "Initialize I2C bus");
        i2c_master_bus_handle_t i2c_bus_;
        i2c_master_bus_config_t i2c_bus_cfg = {
            .i2c_port = (i2c_port_t)TOUCH_MASTER_NUM,
            .sda_io_num = TOUCH_SDA_NUM,
            .scl_io_num = TOUCH_SCL_NUM,
            .clk_source = I2C_CLK_SRC_DEFAULT,
            .glitch_ignore_cnt = 7,
            .intr_priority = 0,
            .trans_queue_depth = 0,
            .flags = {
                .enable_internal_pullup = 1,
            },
        };
        ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_bus_cfg, &i2c_bus_));

        esp_lcd_panel_io_handle_t tp_io_handle = NULL;
        esp_lcd_panel_io_i2c_config_t tp_io_config = ESP_LCD_TOUCH_IO_I2C_FT5x06_CONFIG();

        tp_io_config.scl_speed_hz = 400 * 1000;
        // Attach the TOUCH to the I2C bus
        ESP_ERROR_CHECK(esp_lcd_new_panel_io_i2c_v2((i2c_master_bus_t *)i2c_bus_, &tp_io_config, &tp_io_handle));

        const esp_lcd_touch_config_t tp_cfg = {
            .x_max = DISPLAY_HEIGHT - 1,
            .y_max = DISPLAY_WIDTH - 1,
            .rst_gpio_num = GPIO_NUM_NC,
            .int_gpio_num = TOUCH_INT_NUM,
            .levels = {
                .reset = 0,
                .interrupt = 0,
            },
            .flags = {
                .swap_xy = TOUCH_SWAP_XY,
                .mirror_x = TOUCH_MIRROR_X,
                .mirror_y = TOUCH_MIRROR_Y,
            },
            .interrupt_callback = tp_interrupt_callback,
        };

        ESP_LOGI(TAG, "Initialize touch controller");
        (esp_lcd_touch_new_i2c_ft5x06(tp_io_handle, &tp_cfg, &tp)); // The first initial will be failed
        (esp_lcd_touch_new_i2c_ft5x06(tp_io_handle, &tp_cfg, &tp));
#endif
        display_ = new CustomLcdDisplay(panel_io, panel, tp,
                                        DISPLAY_WIDTH, DISPLAY_HEIGHT, DISPLAY_OFFSET_X,
                                        DISPLAY_OFFSET_Y, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y,
                                        DISPLAY_SWAP_XY, spi_device);
        GetBacklight()->SetBrightness(0);

        if (PIN_NUM_VFD_EN != GPIO_NUM_NC)
        {
            ESP_LOGI(TAG, "Enable amoled power");
            gpio_set_direction(PIN_NUM_LCD_POWER, GPIO_MODE_OUTPUT);
            gpio_set_level(PIN_NUM_LCD_POWER, 1);
        }

        if (PIN_NUM_VFD_EN != GPIO_NUM_NC)
        {
            ESP_LOGI(TAG, "Enable VFD power");
            gpio_set_direction(PIN_NUM_VFD_EN, GPIO_MODE_OUTPUT);
            gpio_set_level(PIN_NUM_VFD_EN, 1);
        }
    }

    // 物联网初始化，添加对 AI 可见设备
    void InitializeIot()
    {
        auto &thing_manager = iot::ThingManager::GetInstance();
        thing_manager.AddThing(iot::CreateThing("Speaker"));
        thing_manager.AddThing(iot::CreateThing("Barometer"));
        thing_manager.AddThing(iot::CreateThing("Screen"));
        thing_manager.AddThing(iot::CreateThing("Battery"));
        // thing_manager.AddThing(iot::CreateThing("Lamp"));
    }

    void InitializeAdc()
    {
        adc_oneshot_unit_init_cfg_t init_config = {
            .unit_id = ADC_UNIT,
        };
        ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config, &adc_handle));

#if !ESP_DUAL_DISPLAY_V2
        adc_oneshot_chan_cfg_t bat_config = {
            .atten = ADC_ATTEN_DB_12,
            .bitwidth = ADC_BITWIDTH_12,
        };
        ESP_ERROR_CHECK(adc_oneshot_config_channel(adc_handle, BAT_ADC_CHANNEL, &bat_config));

        adc_cali_curve_fitting_config_t bat_cali_config = {
            .unit_id = ADC_UNIT,
            .atten = ADC_ATTEN_DB_12,
            .bitwidth = ADC_BITWIDTH_12,
        };
        ESP_ERROR_CHECK(adc_cali_create_scheme_curve_fitting(&bat_cali_config, &bat_adc_cali_handle));
#endif
        adc_oneshot_chan_cfg_t dimm_config = {
            .atten = ADC_ATTEN_DB_12,
            .bitwidth = ADC_BITWIDTH_12,
        };
        ESP_ERROR_CHECK(adc_oneshot_config_channel(adc_handle, DIMM_ADC_CHANNEL, &dimm_config));
        adc_cali_curve_fitting_config_t dimm_cali_config = {
            .unit_id = ADC_UNIT,
            .atten = ADC_ATTEN_DB_12,
            .bitwidth = ADC_BITWIDTH_12,
        };
        ESP_ERROR_CHECK(adc_cali_create_scheme_curve_fitting(&dimm_cali_config, &dimm_adc_cali_handle));
    }
    void GetWakeupCause(void)
    {
        esp_sleep_wakeup_cause_t wakeup_cause = esp_sleep_get_wakeup_cause();
        switch (wakeup_cause)
        {
        case ESP_SLEEP_WAKEUP_UNDEFINED:
            ESP_LOGI(TAG, "Wakeup cause: Undefined");
            break;
        case ESP_SLEEP_WAKEUP_EXT0:
            ESP_LOGI(TAG, "Wakeup cause: External source 0");
            break;
        case ESP_SLEEP_WAKEUP_EXT1:
            ESP_LOGI(TAG, "Wakeup cause: External source 1");
            break;
        case ESP_SLEEP_WAKEUP_TIMER:
            ESP_LOGI(TAG, "Wakeup cause: Timer");
            break;
        case ESP_SLEEP_WAKEUP_TOUCHPAD:
            ESP_LOGI(TAG, "Wakeup cause: Touchpad");
            break;
        case ESP_SLEEP_WAKEUP_ULP:
            ESP_LOGI(TAG, "Wakeup cause: ULP");
            break;
        case ESP_SLEEP_WAKEUP_GPIO:
            ESP_LOGI(TAG, "Wakeup cause: GPIO");
            break;
        case ESP_SLEEP_WAKEUP_UART:
            ESP_LOGI(TAG, "Wakeup cause: UART");
            break;
        case ESP_SLEEP_WAKEUP_WIFI:
            ESP_LOGI(TAG, "Wakeup cause: WiFi");
            break;
        case ESP_SLEEP_WAKEUP_COCPU:
            ESP_LOGI(TAG, "Wakeup cause: Co-processor");
            break;
        default:
            ESP_LOGI(TAG, "Wakeup cause: Unknown");
            break;
        }
    }

    void test()
    {
        mpu6050_acce_value_t acce_value;
        physics_init();
        while (1)
        {
            printf("\033[2J\033[H");
            auto ret = mpu6050_get_acce(mpu6050, &acce_value);
            if (ret != ESP_OK)
            {
                ESP_LOGE(TAG, "Failed to read acceleration data: %s", esp_err_to_name(ret));
            }
            else
            {
                char acce_str[100];
                snprintf(acce_str, sizeof(acce_str), "Acceleration Data: X = %.2f, Y = %.2f, Z = %.2f",
                         (float)acce_value.acce_x, (float)acce_value.acce_y, (float)acce_value.acce_z);
                ESP_LOGI(TAG, "%s", acce_str);
                {
                    float gx = -acce_value.acce_y;
                    float gy = acce_value.acce_z;
                    physics_update(gx * 9.8 * 2, gy * 9.8 * 2, 0.1);
                }

                char grid[GRID_HEIGHT][GRID_WIDTH];
                physics_render_to_grid(grid);

                for (int y = 0; y < GRID_HEIGHT; y++)
                {
                    for (int x = 0; x < GRID_WIDTH; x++)
                    {
                        printf("%c ", grid[y][x]);
                    }
                    printf("\n");
                }
            }
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }

public:
    DualScreenAIDisplay()
    {
        if (PIN_NUM_POWER_EN != GPIO_NUM_NC)
        {
            gpio_set_direction(PIN_NUM_POWER_EN, GPIO_MODE_OUTPUT);
            gpio_set_level(PIN_NUM_POWER_EN, 1);
        }

        vTaskDelay(pdMS_TO_TICKS(120));

        InitializeAdc();
        InitializeI2c();
        // test();
#if ESP_DUAL_DISPLAY_V2
        pcf8574->writeGpio(TPS_PS, 1);
        pcf8574->writeGpio(MIC_EN, 1);
        pcf8574->writeGpio(OLED_EN, 1);
        pcf8574->writeGpio(VFD_EN, 0);
        pcf8574->writeGpio(SD_EN, 1);
        vTaskDelay(pdMS_TO_TICKS(120));
        pcf8574->writeGpio(VFD_EN, 1);
#endif

        xTaskCreate([](void *arg)
                    { sntp_set_time_sync_notification_cb([](struct timeval *t) {
                if (settimeofday(t, NULL) == ESP_FAIL) {
                    ESP_LOGE(TAG, "Failed to set system time");
                    return;
                }
                struct tm tm_info;
                localtime_r(&t->tv_sec, &tm_info);
                char time_str[50];
                strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", &tm_info);

                ESP_LOGD(TAG, "The net time is: %s", time_str);
                auto ret = Board::GetInstance().CalibrateTime(&tm_info);
                if(!ret)
                    ESP_LOGI(TAG, "Calibration Time Failed");
                    else
                    {
                        CustomLcdDisplay *display = (CustomLcdDisplay*)Board::GetInstance().GetDisplay();
                        if(display != nullptr)
                            display->Notification("SYNC TM OK", 3000);
                        } 
                    });
        esp_netif_init();
        esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
        esp_sntp_setservername(0, (char *)NTP_SERVER1);
        esp_sntp_setservername(1, (char *)NTP_SERVER2);
        esp_sntp_init();
        setenv("TZ", DEFAULT_TIMEZONE, 1);
        tzset();
        vTaskDelete(NULL); }, "timesync", 4096, NULL, 4, nullptr);

        InitializeDisplay();
        InitializeIot();
        GetBacklight()->RestoreBrightness();
        InitializePowerSaveTimer();
        GetWakeupCause();
        GetSdcard();
        InitializeButtons();
#if ESP_DUAL_DISPLAY_V2
        gpio_set_direction(PIN_NUM_VCC_DECT, GPIO_MODE_INPUT);
        gpio_set_pull_mode(PIN_NUM_VCC_DECT, GPIO_FLOATING);
        ESP_LOGI(TAG, "PIN_NUM_VCC_DECT: %d", gpio_get_level(PIN_NUM_VCC_DECT));
#endif
    }

    virtual Led *GetLed() override
    {
#if SUB_DISPLAY_EN
        return display_;
#else
        static SingleLed led(BUILTIN_LED_GPIO);
        return &led;
#endif
    }

    virtual float GetBarometer() override
    {
        float pressure = 0.0f;
        if (ESP_OK == bmp280_read_pressure(bmp280, &pressure))
        {
            return pressure;
        }
        return 0;
    }

    virtual float GetTemperature() override
    {
        float temperature = 0.0f;
        if (ESP_OK == bmp280_read_temperature(bmp280, &temperature))
        {
            return temperature;
        }
        return 0;
    }

    virtual AudioCodec *GetAudioCodec() override
    {
#ifdef AUDIO_I2S_METHOD_SIMPLEX
        static NoAudioCodec audio_codec(AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE,
                                        AUDIO_I2S_SPK_GPIO_BCLK, AUDIO_I2S_SPK_GPIO_LRCK, AUDIO_I2S_SPK_GPIO_DOUT, AUDIO_I2S_MIC_GPIO_SCK, AUDIO_I2S_MIC_GPIO_WS, AUDIO_I2S_MIC_GPIO_DIN);
#else
        static NoAudioCodecDuplex audio_codec(AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE,
                                              AUDIO_I2S_GPIO_BCLK, AUDIO_I2S_GPIO_WS, AUDIO_I2S_GPIO_DOUT, AUDIO_I2S_GPIO_DIN);
#endif
        return &audio_codec;
    }

    virtual Display *GetDisplay() override
    {
        return display_;
    }

    virtual Backlight *GetBacklight() override
    {
        return display_;
    }

    Sdcard *GetSdcard()
    {
        static Sdcard sd_card(PIN_NUM_SD_CMD, PIN_NUM_SD_CLK, PIN_NUM_SD_D0, PIN_NUM_SD_D1, PIN_NUM_SD_D2, PIN_NUM_SD_D3, PIN_NUM_SD_CDZ);
        return &sd_card;
    }

    int32_t map(int32_t x, int32_t in_min, int32_t in_max, int32_t out_min, int32_t out_max)
    {
        return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
    }

    int32_t constrain(int32_t value, int32_t min_value, int32_t max_value)
    {
        if (value < min_value)
        {
            return min_value;
        }
        else if (value > max_value)
        {
            return max_value;
        }
        return value;
    }

    esp_err_t testmpu()
    {
        mpu6050_acce_value_t acce_value;
        mpu6050_gyro_value_t gyro_value;
        complimentary_angle_t complimentary_angle;

        bool active = 0;
        mpu6050_getMotionInterruptStatus(mpu6050, &active);
        ESP_LOGD(TAG, "mpu6050_getMotionInterruptStatus: %d", active);

        auto ret = mpu6050_get_acce(mpu6050, &acce_value);
        if (ret != ESP_OK)
        {
            ESP_LOGE(TAG, "Failed to read acceleration data: %s", esp_err_to_name(ret));
        }
        else
        {
            char acce_str[100];
            snprintf(acce_str, sizeof(acce_str), "Acceleration Data: X = %.2f, Y = %.2f, Z = %.2f",
                     (float)acce_value.acce_x, (float)acce_value.acce_y, (float)acce_value.acce_z);
            ESP_LOGI(TAG, "%s", acce_str);
        }

        ret = mpu6050_get_gyro(mpu6050, &gyro_value);
        if (ret != ESP_OK)
        {
            ESP_LOGE(TAG, "Failed to read gyroscope data: %s", esp_err_to_name(ret));
        }
        else
        {
            char gyro_str[100];
            snprintf(gyro_str, sizeof(gyro_str), "Gyroscope Data: X = %.2f, Y = %.2f, Z = %.2f",
                     (float)gyro_value.gyro_x, (float)gyro_value.gyro_y, (float)gyro_value.gyro_z);
            ESP_LOGI(TAG, "%s", gyro_str);
        }
        ret = mpu6050_complimentory_filter(mpu6050, &acce_value, &gyro_value, &complimentary_angle);
        if (ret != ESP_OK)
        {
            ESP_LOGE(TAG, "Failed to apply complimentary filter: %s", esp_err_to_name(ret));
        }

        ESP_LOGI(TAG, "Roll: %.2f degrees, Pitch: %.2f degrees", complimentary_angle.roll, complimentary_angle.pitch);

        // esp_err_t ret;
        // uint8_t mpu6050_deviceid;
        // mpu6050_raw_acce_value_t acce;
        // mpu6050_raw_gyro_value_t gyro;
        // mpu6050_temp_value_t temp;
        // ret = mpu6050_get_deviceid(mpu6050, &mpu6050_deviceid);
        // ESP_LOGI(TAG, "mpu6050_deviceid:%X", mpu6050_deviceid);

        // ret = mpu6050_get_raw_acce(mpu6050, &acce);
        // ESP_LOGI(TAG, "acce_x:%d, acce_y:%d, acce_z:%d", acce.raw_acce_x, acce.raw_acce_y, acce.raw_acce_z);

        // ret = mpu6050_get_raw_gyro(mpu6050, &gyro);
        // ESP_LOGI(TAG, "gyro_x:%d, gyro_y:%d, gyro_z:%d", gyro.raw_gyro_x, gyro.raw_gyro_y, gyro.raw_gyro_z);

        // ret = mpu6050_get_temp(mpu6050, &temp);
        // ESP_LOGI(TAG, "t:%.2f", temp.temp);

        return ret;
    }

#define VCHARGE 4210
#define V1_UP 4000
#define V1_DOWN 3850
#define V2_UP 3750
#define V2_DOWN 3600
#define V3_UP 3550
#define V3_DOWN 3350
#define V4_UP 3100
#define V4_DOWN 2900

    virtual bool GetBatteryLevel(int &level, bool &charging, bool &discharging) override
    {
#if ESP_DUAL_DISPLAY_V2
        static int last_level = 0;
        int bat_v = ina3221->getBusVoltage(BAT_PW) * 1000;
        int new_level;
        if (last_level >= 100 && bat_v >= V1_DOWN)
        {
            new_level = 100;
        }
        else if (last_level < 100 && bat_v >= V1_UP)
        {
            new_level = 100;
        }
        else if (last_level >= 75 && bat_v >= V2_DOWN)
        {
            new_level = 75;
        }
        else if (last_level < 75 && bat_v >= V2_UP)
        {
            new_level = 75;
        }
        else if (last_level >= 50 && bat_v >= V3_DOWN)
        {
            new_level = 50;
        }
        else if (last_level < 50 && bat_v >= V3_UP)
        {
            new_level = 50;
        }
        else if (last_level >= 25 && bat_v >= V4_DOWN)
        {
            new_level = 25;
        }
        else if (last_level < 25 && bat_v >= V4_UP)
        {
            new_level = 25;
        }
        else
        {
            new_level = 0;
        }
        // testmpu();
        level = new_level;
        // charging = gpio_get_level(PIN_NUM_VCC_DECT);
        float crt = ina3221->getCurrent(BAT_PW);
        if (crt > 0.01)
            discharging = true;
        else
            discharging = false;
        if (crt < -0.1)
            charging = true;
        else
            charging = false;

        // float voltage = 0.0f, current = 0.0f;
        // for (size_t i = 0; i < 3; i++)
        // {
        //     voltage = ina3221->getBusVoltage(i);
        //     current = ina3221->getCurrent(i);
        //     ESP_LOGI(TAG, "channel: %s, voltage: %dmV, current: %dmA", DectectCHEnum[i], (int)(voltage * 1000), (int)(current * 1000));
        // }
#else
        static int last_level = 0;
        static bool last_charging = false;
        int bat_adc_value;
        int bat_v = 0;
        ESP_ERROR_CHECK(adc_oneshot_read(adc_handle, BAT_ADC_CHANNEL, &bat_adc_value));
        ESP_ERROR_CHECK(adc_cali_raw_to_voltage(bat_adc_cali_handle, bat_adc_value, &bat_v));
        bat_v *= 2;
        int new_level;
        if (bat_v >= VCHARGE)
        {
            new_level = last_level;
            charging = true;
        }
        else if (last_level >= 100 && bat_v >= V1_DOWN)
        {
            new_level = 100;
            charging = false;
        }
        else if (last_level < 100 && bat_v >= V1_UP)
        {
            new_level = 100;
            charging = false;
        }
        else if (last_level >= 75 && bat_v >= V2_DOWN)
        {
            new_level = 75;
            charging = false;
        }
        else if (last_level < 75 && bat_v >= V2_UP)
        {
            new_level = 75;
            charging = false;
        }
        else if (last_level >= 50 && bat_v >= V3_DOWN)
        {
            new_level = 50;
            charging = false;
        }
        else if (last_level < 50 && bat_v >= V3_UP)
        {
            new_level = 50;
            charging = false;
        }
        else if (last_level >= 25 && bat_v >= V4_DOWN)
        {
            new_level = 25;
            charging = false;
        }
        else if (last_level < 25 && bat_v >= V4_UP)
        {
            new_level = 25;
            charging = false;
        }
        else
        {
            new_level = 0;
            charging = false;
        }

        level = new_level;
        if (level != last_level || charging != last_charging)
        {
            last_level = level;
            last_charging = charging;
            // ESP_LOGI(TAG, "Battery level: %d, charging: %d", level, charging);
        }
        discharging = !charging;
#endif
        power_save_timer_->SetEnabled(discharging);
        if(discharging)
        {
            bool active;
            mpu6050_getMotionInterruptStatus(mpu6050, &active);
            if(active)
            {
                power_save_timer_->WakeUp();
                // ESP_LOGI(TAG, "WakeUp");
            }
        }
#if SUB_DISPLAY_EN && FTB_13_BT_247GN_EN
        char temp_str[11];
        snprintf(temp_str, sizeof temp_str, "%d", (int)(bat_v / 10));
        display_->num_show(14, temp_str, 3, FTB_BT_247GN::ANTICLOCKWISE);
        snprintf(temp_str, sizeof temp_str, "%4d", (int)(GetBarometer()));
        display_->num_show(3, temp_str, 4, FTB_BT_247GN::ANTICLOCKWISE);

        snprintf(temp_str, sizeof temp_str, "%3d", (int)(GetTemperature() * 10));
        display_->num_show(18, temp_str, 3, FTB_BT_247GN::CLOCKWISE);
        display_->symbolhelper(FTB_BT_247GN::Point, true);
#endif

#if SUB_DISPLAY_EN && FORD_VFD_EN
        if (!charging)
            display_->symbolhelper(FORD_VFD::AUX, false);
        else
            display_->symbolhelper(FORD_VFD::AUX, true);
#elif SUB_DISPLAY_EN && HNA_16MM65T_EN
        if (discharging)
            display_->symbolhelper(HNA_16MM65T::USB2, false);
        else
            display_->symbolhelper(HNA_16MM65T::USB2, true);
#elif SUB_DISPLAY_EN && BOE_48_1504FN_EN
        if (discharging)
            display_->symbolhelper(BOE_48_1504FN::RD_BAT, true);
        else
            display_->symbolhelper(BOE_48_1504FN::RD_BAT, false);

        if (charging)
            display_->symbolhelper(BOE_48_1504FN::D_USB, true);
        else
            display_->symbolhelper(BOE_48_1504FN::D_USB, false);
        if (!discharging && !charging)
            display_->symbolhelper(BOE_48_1504FN::D_USB_L, true);
        else
            display_->symbolhelper(BOE_48_1504FN::D_USB_L, false);
#elif SUB_DISPLAY_EN && HUV_13SS16T_EN
#elif SUB_DISPLAY_EN && FTB_13_BT_247GN_EN
        static int64_t start_time = esp_timer_get_time() / 1000;
        int64_t current_time = esp_timer_get_time() / 1000;
        int64_t elapsed_time = current_time - start_time;

        if (elapsed_time >= 500)
            start_time = current_time;
        else
            return true;
        static int count = 0;
        if (charging)
            display_->symbolhelper(FTB_BT_247GN::Usb, true);
        else
            display_->symbolhelper(FTB_BT_247GN::Usb, false);

        if (discharging)
        {
            display_->symbolhelper(FTB_BT_247GN::Bat_0, false);
            display_->symbolhelper(FTB_BT_247GN::Bat_1, false);
            display_->symbolhelper(FTB_BT_247GN::Bat_2, false);
            display_->symbolhelper(FTB_BT_247GN::Bat_3, false);
            display_->symbolhelper(FTB_BT_247GN::Bat_4, false);
            display_->symbolhelper(FTB_BT_247GN::Bat_5, false);
            display_->symbolhelper(FTB_BT_247GN::Bat_6, false);
            if (level > 0)
                display_->symbolhelper(FTB_BT_247GN::Bat_1, true);
            if (level > 25)
            {
                display_->symbolhelper(FTB_BT_247GN::Bat_2, true);
                display_->symbolhelper(FTB_BT_247GN::Bat_3, true);
            }
            if (level > 50)
            {
                display_->symbolhelper(FTB_BT_247GN::Bat_4, true);
                display_->symbolhelper(FTB_BT_247GN::Bat_5, true);
            }
            if (level > 75)
                display_->symbolhelper(FTB_BT_247GN::Bat_6, true);
            count = 0;
        }
        else
        {
            display_->symbolhelper(FTB_BT_247GN::Bat_0, true);
            display_->symbolhelper(FTB_BT_247GN::Bat_1, false);
            display_->symbolhelper(FTB_BT_247GN::Bat_2, false);
            display_->symbolhelper(FTB_BT_247GN::Bat_3, false);
            display_->symbolhelper(FTB_BT_247GN::Bat_4, false);
            display_->symbolhelper(FTB_BT_247GN::Bat_5, false);
            display_->symbolhelper(FTB_BT_247GN::Bat_6, false);
            if (count > 0)
                display_->symbolhelper(FTB_BT_247GN::Bat_1, true);
            if (count > 1)
                display_->symbolhelper(FTB_BT_247GN::Bat_2, true);
            if (count > 2)
                display_->symbolhelper(FTB_BT_247GN::Bat_3, true);
            if (count > 3)
                display_->symbolhelper(FTB_BT_247GN::Bat_4, true);
            if (count > 4)
                display_->symbolhelper(FTB_BT_247GN::Bat_5, true);
            if (count > 5)
                display_->symbolhelper(FTB_BT_247GN::Bat_6, true);
            count++;
            count = count % 7;
        }
#endif
        return true;
    }

    virtual bool CalibrateTime(struct tm *tm_info) override
    {
        if (rx8900_write_time(rx8900, tm_info) == ESP_FAIL)
            return false;
        return true;
    }

    virtual bool DimmingUpdate() override
    {
        static uint8_t last_bl = 0;
        int dimm_adc_value;
        ESP_ERROR_CHECK(adc_oneshot_read(adc_handle, DIMM_ADC_CHANNEL, &dimm_adc_value));
        uint8_t bl = constrain(100 - map(dimm_adc_value, 100, 3000, 0, 100), 0, 100);

        // ESP_LOGI(TAG, "bl: %d", bl);

#if SUB_DISPLAY_EN && FTB_13_BT_247GN_EN
        char tempstr[7];
        snprintf(tempstr, sizeof(tempstr), "%d", (bl / 10) % 10);
        display_->num_show(17, tempstr, 1, FTB_BT_247GN::ANTICLOCKWISE);
        if (bl / 100)
            display_->symbolhelper(FTB_BT_247GN::L2, true);
        else
            display_->symbolhelper(FTB_BT_247GN::L2, false);

#endif
        if (abs(bl - last_bl) > 10)
        {
            last_bl = bl;
            GetBacklight()->SetBrightness(bl);
        }

        return true;
    }

    virtual bool TimeUpdate() override
    {
        // display_->test();
#if SUB_DISPLAY_EN && FORD_VFD_EN
        static struct tm time_user;
        time_t now = time(NULL);
        time_user = *localtime(&now);
        char time_str[7];
        strftime(time_str, sizeof(time_str), "%H%M", &time_user);
        display_->number_show(5, time_str, 4);
        display_->time_blink();
        strftime(time_str, sizeof(time_str), "%m%d", &time_user);
        display_->symbolhelper(FORD_VFD::POINT1, true);
        display_->number_show(1, time_str, 4, FORD_VFD::DOWN2UP);
#elif SUB_DISPLAY_EN && HNA_16MM65T_EN
        static struct tm time_user;
        time_t now = time(NULL);
        time_user = *localtime(&now);
        char time_str[7];
        strftime(time_str, sizeof(time_str), "%H%M%S", &time_user);
        display_->content_show(4, time_str, 6);
        display_->time_blink();
        const char *weekDays[7] = {
            "SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"};
        display_->content_show(0, (char *)weekDays[time_user.tm_wday % 7], 3, HNA_16MM65T::DOWN2UP);
#elif SUB_DISPLAY_EN && BOE_48_1504FN_EN
        static struct tm time_user;
        time_t now = time(NULL);
        time_user = *localtime(&now);
        char time_str[11];
        static bool blink = false;
        if (blink)
            strftime(time_str, sizeof(time_str), "%H:%M:%S", &time_user);
        else
            strftime(time_str, sizeof(time_str), "%H %M %S", &time_user);
        blink = !blink;
        display_->content_show(1, time_str, 8, false, BOE_48_1504FN::UP2DOWN);
#elif SUB_DISPLAY_EN && HUV_13SS16T_EN
        static struct tm time_user;
        time_t now = time(NULL);
        time_user = *localtime(&now);
        char time_str[11];
        static bool blink = false;
        if (blink)
            strftime(time_str, sizeof(time_str), "%H:%M:%S", &time_user);
        else
            strftime(time_str, sizeof(time_str), "%H %M %S", &time_user);
        blink = !blink;
        display_->content_show(0, time_str, 8, false, HUV_13SS16T::UP2DOWN);
#elif SUB_DISPLAY_EN && FTB_13_BT_247GN_EN
        static struct tm time_user;
        time_t now = time(NULL);
        time_user = *localtime(&now);
        char time_str[7];
        strftime(time_str, sizeof(time_str), "%H%M%S", &time_user);
        display_->num_show(8, time_str, 6, FTB_BT_247GN::ANTICLOCKWISE);
        const char *weekDays[7] = {
            "sun", "mon", "tue", "wed", "thu", "fri", "sat"};
        display_->num_show(0, weekDays[time_user.tm_wday % 7], 3, false, FTB_BT_247GN::UP2DOWN);
        display_->time_blink();
#endif
        return true;
    }
};

DECLARE_BOARD(DualScreenAIDisplay);
