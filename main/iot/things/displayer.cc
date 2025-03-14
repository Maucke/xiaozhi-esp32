#include "iot/thing.h"
#include "display/lcd_display.h"
#include "board.h"
#include "audio_codec.h"
#include <string>

#include <esp_log.h>
#define TAG "Displayer"

namespace iot
{

    // 这里仅定义 Speaker 的属性和方法，不包含具体的实现
    class Displayer : public Thing
    {
    public:
        Displayer() : Thing("Displayer", "当前 AI 机器人的显示器")
        {
            // 定义设备的属性
            properties_.AddNumberProperty("brightness", "当前亮度值", [this]() -> int
                                          {
                auto display = Board::GetInstance().GetDisplay();
            return display->GetBacklight(); });

            // 定义设备可以被远程执行的指令
            methods_.AddMethod("SetBrightness", "设置亮度", ParameterList({Parameter("brightness", "0到100之间的整数", kValueTypeNumber, true)}), [this](const ParameterList &parameters)
                               {
                auto display = Board::GetInstance().GetDisplay();
                auto brightness = static_cast<uint8_t>(parameters["brightness"].number());
                display->SetAutoDimming(false);
                display->SetBacklight(brightness); 
                
                char tempstr[11] = {0};
                sprintf(tempstr, "BRIGHT:%d", brightness);
                display->Notification((std::string)tempstr,2000); });

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
                auto display = Board::GetInstance().GetDisplay();
                display->SetAutoDimming(false);
                char tempstr[11] = {0};
                sprintf(tempstr, "AutoDimmOF");
                display->Notification((std::string)tempstr,2000); 
                display->SetBacklight(display->GetBacklight()); 
             });
        }
    };

} // namespace iot

DECLARE_THING(Displayer);
