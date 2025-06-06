#ifndef _BOARD_CONFIG_H_
#define _BOARD_CONFIG_H_

#include <driver/gpio.h>

#define NTP_SERVER1           "pool.ntp.org"
#define NTP_SERVER2           "time.nist.gov"
#define DEFAULT_TIMEZONE      "CST-8"     

#define AUDIO_INPUT_SAMPLE_RATE  24000
#define AUDIO_OUTPUT_SAMPLE_RATE 24000

// 如果使用 Duplex I2S 模式，请注释下面一行
// #define AUDIO_I2S_METHOD_SIMPLEX

#ifdef AUDIO_I2S_METHOD_SIMPLEX

#define AUDIO_I2S_MIC_GPIO_WS   GPIO_NUM_4
#define AUDIO_I2S_MIC_GPIO_SCK  GPIO_NUM_5
#define AUDIO_I2S_MIC_GPIO_DIN  GPIO_NUM_6
#define AUDIO_I2S_SPK_GPIO_DOUT GPIO_NUM_7
#define AUDIO_I2S_SPK_GPIO_BCLK GPIO_NUM_15
#define AUDIO_I2S_SPK_GPIO_LRCK GPIO_NUM_16

#else

#define AUDIO_I2S_GPIO_WS GPIO_NUM_10   //以适配
#define AUDIO_I2S_GPIO_BCLK GPIO_NUM_11   //以适配
#define AUDIO_I2S_GPIO_DIN  GPIO_NUM_13   //以适配
#define AUDIO_I2S_GPIO_DOUT GPIO_NUM_12   //以适配

#endif
#define ADC_UNIT ADC_UNIT_1
#define ADC_CHANNEL ADC_CHANNEL_3 //GPIO_NUM_4
#define DEFAULT_VREF 1100 // 参考电压，单位为 mV，可根据实际情况调整

#define IIC_MASTER_NUM I2C_NUM_0
#define IIC_SDA_NUM             GPIO_NUM_3
#define IIC_SCL_NUM             GPIO_NUM_2
#define I2C_MASTER_TX_BUF_DISABLE   0                    /*!< I2C master do not need buffer */
#define I2C_MASTER_RX_BUF_DISABLE   0                    /*!< I2C master do not need buffer */

#define BUILTIN_LED_GPIO        GPIO_NUM_1   //以适配
#define BOOT_BUTTON_GPIO        GPIO_NUM_0   //以适配
// #define RESET_NVS_BUTTON_GPIO     GPIO_NUM_1
// #define RESET_FACTORY_BUTTON_GPIO GPIO_NUM_4

#define TOUCH_BUTTON_GPIO       GPIO_NUM_15   //以适配-encoder
#define VOLUME_ENCODER1_GPIO   GPIO_NUM_14    //以适配-encoder
#define VOLUME_ENCODER2_GPIO   GPIO_NUM_16  //以适配-encoder

#define LCD_HOST SPI2_HOST

#define DISPLAY_WIDTH   536
#define DISPLAY_HEIGHT  240
#define DISPLAY_MIRROR_X true
#define DISPLAY_MIRROR_Y false
#define DISPLAY_SWAP_XY true

#define DISPLAY_OFFSET_X  0
#define DISPLAY_OFFSET_Y  0

#define PIN_NUM_LCD_CS (GPIO_NUM_6)
#define PIN_NUM_LCD_PCLK (GPIO_NUM_47)
#define PIN_NUM_LCD_DATA0 (GPIO_NUM_18)
#define PIN_NUM_LCD_DATA1 (GPIO_NUM_7)
#define PIN_NUM_LCD_DATA2 (GPIO_NUM_48)
#define PIN_NUM_LCD_DATA3 (GPIO_NUM_5)
#define PIN_NUM_LCD_RST (GPIO_NUM_17)
#define PIN_NUM_BK_LIGHT (GPIO_NUM_NC)
#define PIN_NUM_LCD_POWER (GPIO_NUM_38)

#define PIN_NUM_SD_CMD (GPIO_NUM_43)
#define PIN_NUM_SD_CLK (GPIO_NUM_42)
#define PIN_NUM_SD_D0 (GPIO_NUM_41)
#define PIN_NUM_SD_D1 (GPIO_NUM_40)
#define PIN_NUM_SD_D2 (GPIO_NUM_45)
#define PIN_NUM_SD_D3 (GPIO_NUM_44)
#define PIN_NUM_SD_CDZ (GPIO_NUM_39)

#endif // _BOARD_CONFIG_H_
