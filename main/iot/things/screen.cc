#include "iot/thing.h"
#include "board.h"
#include "display/lcd_display.h"
#include "settings.h"

#include <esp_log.h>
#include <string>

#define TAG "Screen"

namespace iot
{

    // 这里仅定义 Screen 的属性和方法，不包含具体的实现
    class Screen : public Thing
    {
    public:
        Screen() : Thing("Screen", "这是一个屏幕，可设置主题和亮度")
        {
            // 定义设备的属性
            properties_.AddStringProperty("theme", "主题", [this]() -> std::string
                                          {
            auto theme = Board::GetInstance().GetDisplay()->GetTheme();
            return theme; });

            properties_.AddNumberProperty("brightness", "当前亮度百分比", [this]() -> int
                                          {
            // 这里可以添加获取当前亮度的逻辑
            auto backlight = Board::GetInstance().GetBacklight();
            return backlight ? backlight->brightness() : 100; });

            // 定义设备可以被远程执行的指令
            methods_.AddMethod("SetTheme", "设置屏幕主题", ParameterList({Parameter("theme_name", "主题模式, light 或 dark", kValueTypeString, true)}), [this](const ParameterList &parameters)
                               {
            std::string theme_name = static_cast<std::string>(parameters["theme_name"].string());
            auto display = Board::GetInstance().GetDisplay();
            auto backlight = Board::GetInstance().GetBacklight();
            if (display) {
                display->SetAutoDimming(false);
                display->SetTheme(theme_name);
                backlight->SetBrightness(backlight->brightness()); 
                display->Notification("Theme" + theme_name, 2000); 
            } });

            methods_.AddMethod("SetBrightness", "设置亮度", ParameterList({Parameter("brightness", "0到100之间的整数", kValueTypeNumber, true)}), [this](const ParameterList &parameters)
                               {
            uint8_t brightness = static_cast<uint8_t>(parameters["brightness"].number());
            auto backlight = Board::GetInstance().GetBacklight();
            auto display = Board::GetInstance().GetDisplay();
            if (backlight) {
                backlight->SetBrightness(brightness, true);
                display->Notification("Dimm:" + brightness, 2000); 
                display->SetAutoDimming(false);
            } });

            properties_.AddBooleanProperty("autodimming", "自动亮度是否打开", [this]() -> bool
                                           {
            auto display = Board::GetInstance().GetDisplay();
            return display->GetAutoDimming(); });

            methods_.AddMethod("TurnOn", "打开自动亮度", ParameterList(), [this](const ParameterList &parameters)
                               {
            auto display = Board::GetInstance().GetDisplay();
            display->SetAutoDimming(true);
            char tempstr[11] = {0};
            sprintf(tempstr, "AutoDimmON");
            display->Notification((std::string)tempstr,2000); });

            methods_.AddMethod("TurnOff", "关闭自动亮度", ParameterList(), [this](const ParameterList &parameters)
                               {
            auto backlight = Board::GetInstance().GetBacklight();
            auto display = Board::GetInstance().GetDisplay();
            display->SetAutoDimming(false);
            char tempstr[11] = {0};
            sprintf(tempstr, "AutoDimmOF");
            display->Notification((std::string)tempstr,2000); 
            backlight->SetBrightness(backlight->brightness()); });
        }
    };

} // namespace iot

DECLARE_THING(Screen);