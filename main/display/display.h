#ifndef DISPLAY_H
#define DISPLAY_H

#include <lvgl.h>
#include <esp_timer.h>
#include <esp_log.h>
#include <esp_pm.h>

#include <string>
#include "settings.h"

struct DisplayFonts
{
    const lv_font_t *text_font = nullptr;
    const lv_font_t *icon_font = nullptr;
    const lv_font_t *emoji_font = nullptr;
};

class Display
{
private:
    bool timeOffline_ = false;
    bool autoDimming_ = false;

public:
    Display();
    virtual ~Display();

    virtual void SetStatus(const char *status);
    virtual void ShowNotification(const char *notification, int duration_ms = 3000);
    virtual void ShowNotification(const std::string &notification, int duration_ms = 3000);
    virtual void SetEmotion(const char *emotion);
    virtual void SetChatMessage(const char *role, const char *content);
    virtual void Notification(const std::string &content, int timeout);
    virtual void SetIcon(const char *icon);
#if CONFIG_USE_FFT_EFFECT
    virtual void SpectrumShow(float *buf, int size) {}
#endif
    virtual void DrawPoint(int x, int y, uint8_t dot) {}
    virtual std::string GetTheme() { return current_theme_name_; }
    virtual void SetTheme(const std::string &theme_name, bool permanent = true);

    void SetAutoDimming(bool en, bool permanent = true)
    {
        if (permanent)
        {
            Settings settings("display", true);
            settings.SetInt("auto", en);
        }
        autoDimming_ = en;
    }

    bool GetAutoDimming()
    {
        return autoDimming_;
    }

    void RestoreAutoDimming()
    {
        Settings settings("display", false);
        autoDimming_ = settings.GetInt("auto", 0);
        SetAutoDimming(autoDimming_, false);
    }

    inline int width() const { return width_; }
    inline int height() const { return height_; }

protected:
    int width_ = 0;
    int height_ = 0;
    uint8_t brightness_ = 0;

    lv_indev_t *touch_ = nullptr;

    esp_pm_lock_handle_t pm_lock_ = nullptr;
    lv_display_t *display_ = nullptr;

    lv_obj_t *emotion_label_ = nullptr;
    lv_obj_t *network_label_ = nullptr;
    lv_obj_t *status_label_ = nullptr;
    lv_obj_t *notification_label_ = nullptr;
    lv_obj_t *mute_label_ = nullptr;
    lv_obj_t *battery_label_ = nullptr;
    lv_obj_t* chat_message_label_ = nullptr;
    lv_obj_t* low_battery_popup_ = nullptr;
    lv_obj_t* low_battery_label_ = nullptr;
    
    const char* battery_icon_ = nullptr;
    const char* network_icon_ = nullptr;
    bool muted_ = false;
    std::string current_theme_name_;

    esp_timer_handle_t notification_timer_ = nullptr;
    esp_timer_handle_t update_timer_ = nullptr;

    friend class DisplayLockGuard;
    virtual bool Lock(int timeout_ms = 0) = 0;
    virtual void Unlock() = 0;

    virtual void Update();
};

class DisplayLockGuard
{
public:
    DisplayLockGuard(Display *display) : display_(display) {
        if (!display_->Lock(30000)) {
            ESP_LOGE("Display", "Failed to lock display");
        }
    }
    ~DisplayLockGuard()
    {
        display_->Unlock();
    }

private:
    Display *display_;
};

class NoDisplay : public Display
{
private:
    virtual bool Lock(int timeout_ms = 0) override
    {
        return true;
    }
    virtual void Unlock() override {}
};

#endif
