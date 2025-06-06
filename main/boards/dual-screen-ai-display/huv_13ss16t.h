/**
 * @file HUV_13SS16T.h
 * @brief This header file defines the HUV_13SS16T class, which is used to control the display of a specific device and inherits from the PT6302 class.
 *
 * This class provides a series of methods for displaying spectrum, numbers, symbols, dot matrix and other information, and also supports animation effects.
 *
 * @author Shihua Feng
 * @date February 18, 2025
 */

#ifndef _HUV_13SS16T_H_
#define _HUV_13SS16T_H_

#include "pt6302.h"
#include <cmath>
#include <esp_wifi.h>

// Define the number of characters
#define CHAR_SIZE (95 + 1)

/**
 * @class HUV_13SS16T
 * @brief This class inherits from the PT6302 class and is used to control the display of a specific device.
 *
 * It provides methods for displaying spectrum, numbers, symbols, dot matrix and other information, and also supports animation effects.
 */
class HUV_13SS16T : protected PT6302
{
#define BUFFER_SIZE 256
#define DISPLAY_SIZE 8
#define GR_COUNT 13
#define SYMBOL_CGRAM_SIZE 5
#define MAX_X 10
#define MAX_Y 9
public:
    typedef struct
    {
        int byteIndex; // Byte index
        int bitMask;   // Bit index
    } SymbolPosition;

    typedef enum
    {
        NONE = -1,
        UP2DOWN,
        DOWN2UP,
        LEFT2RT,
        RT2LEFT,
        MAX
    } NumAni;

private:
    typedef struct
    {
        char current_content;
        char last_content;
        int animation_step;
        int animation_index;
        NumAni animation_type;
        bool need_update;
    } ContentData;

    typedef struct
    {
        char buffer[BUFFER_SIZE + 1]; // +1 for null terminator
        int start_pos;
        int length;
    } CircularBuffer;

    // Hexadecimal code corresponding to each character
    // !"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[]^_`abcdefghijklmnopqrstuvwxyz
    const uint8_t hex_codes[CHAR_SIZE][5] = {
        {0x00, 0x00, 0x00, 0x00, 0x00}, /*" ", 32(ASCII)*/
        {0x00, 0x00, 0x4F, 0x00, 0x00}, /*"!", 33(ASCII)*/
        {0x00, 0x03, 0x00, 0x03, 0x00}, /*""", 34(ASCII)*/
        {0x22, 0x7F, 0x22, 0x7F, 0x22}, /*"#", 35(ASCII)*/
        {0x24, 0x2A, 0x7F, 0x2A, 0x12}, /*"$", 36(ASCII)*/
        {0x23, 0x13, 0x08, 0x64, 0x62}, /*"%", 37(ASCII)*/
        {0x36, 0x49, 0x56, 0x20, 0x58}, /*"&", 38(ASCII)*/
        {0x00, 0x02, 0x01, 0x00, 0x00}, /*"'", 39(ASCII)*/
        {0x00, 0x1C, 0x22, 0x41, 0x00}, /*"(", 40(ASCII)*/
        {0x00, 0x41, 0x22, 0x1C, 0x00}, /*")", 41(ASCII)*/
        {0x14, 0x08, 0x3E, 0x08, 0x14}, /*"*", 42(ASCII)*/
        {0x08, 0x08, 0x3E, 0x08, 0x08}, /*"+", 43(ASCII)*/
        {0x00, 0x40, 0x20, 0x00, 0x00}, /*",", 44(ASCII)*/
        {0x08, 0x08, 0x08, 0x08, 0x08}, /*"-", 45(ASCII)*/
        {0x00, 0x40, 0x00, 0x00, 0x00}, /*".", 46(ASCII)*/
        {0x20, 0x10, 0x08, 0x04, 0x02}, /*"/", 47(ASCII)*/
        {0x3E, 0x41, 0x41, 0x41, 0x3E}, /*"0", 48(ASCII)*/
        {0x00, 0x02, 0x02, 0x7F, 0x00}, /*"1", 49(ASCII)*/
        {0x62, 0x51, 0x49, 0x49, 0x46}, /*"2", 50(ASCII)*/
        {0x22, 0x41, 0x49, 0x49, 0x36}, /*"3", 51(ASCII)*/
        {0x18, 0x14, 0x12, 0x7F, 0x10}, /*"4", 52(ASCII)*/
        {0x27, 0x45, 0x45, 0x45, 0x39}, /*"5", 53(ASCII)*/
        {0x3C, 0x4A, 0x49, 0x49, 0x30}, /*"6", 54(ASCII)*/
        {0x01, 0x01, 0x71, 0x0D, 0x03}, /*"7", 55(ASCII)*/
        {0x36, 0x49, 0x49, 0x49, 0x36}, /*"8", 56(ASCII)*/
        {0x06, 0x49, 0x49, 0x29, 0x1E}, /*"9", 57(ASCII)*/
        {0x00, 0x00, 0x44, 0x00, 0x00}, /*":", 58(ASCII)*/
        {0x00, 0x40, 0x24, 0x00, 0x00}, /*";", 59(ASCII)*/
        {0x00, 0x08, 0x14, 0x22, 0x41}, /*"<", 60(ASCII)*/
        {0x14, 0x14, 0x14, 0x14, 0x14}, /*"=", 61(ASCII)*/
        {0x41, 0x22, 0x14, 0x08, 0x00}, /*">", 62(ASCII)*/
        {0x02, 0x01, 0x51, 0x09, 0x06}, /*"?", 63(ASCII)*/
        {0x3E, 0x41, 0x19, 0x25, 0x3E}, /*"@", 64(ASCII)*/
        {0x7C, 0x12, 0x11, 0x12, 0x7C}, /*"A", 65(ASCII)*/
        {0x41, 0x7F, 0x49, 0x49, 0x36}, /*"B", 66(ASCII)*/
        {0x3E, 0x41, 0x41, 0x41, 0x22}, /*"C", 67(ASCII)*/
        {0x41, 0x7F, 0x41, 0x41, 0x3E}, /*"D", 68(ASCII)*/
        {0x7F, 0x49, 0x49, 0x49, 0x41}, /*"E", 69(ASCII)*/
        {0x7F, 0x09, 0x09, 0x09, 0x01}, /*"F", 70(ASCII)*/
        {0x3E, 0x41, 0x49, 0x49, 0x3A}, /*"G", 71(ASCII)*/
        {0x7F, 0x08, 0x08, 0x08, 0x7F}, /*"H", 72(ASCII)*/
        {0x00, 0x41, 0x7F, 0x41, 0x00}, /*"I", 73(ASCII)*/
        {0x20, 0x40, 0x41, 0x7F, 0x01}, /*"J", 74(ASCII)*/
        {0x7F, 0x08, 0x14, 0x22, 0x41}, /*"K", 75(ASCII)*/
        {0x7F, 0x40, 0x40, 0x40, 0x40}, /*"L", 76(ASCII)*/
        {0x7F, 0x02, 0x0C, 0x02, 0x7F}, /*"M", 77(ASCII)*/
        {0x7F, 0x04, 0x08, 0x10, 0x7F}, /*"N", 78(ASCII)*/
        {0x3E, 0x41, 0x41, 0x41, 0x3E}, /*"O", 79(ASCII)*/
        {0x7F, 0x09, 0x09, 0x09, 0x06}, /*"P", 80(ASCII)*/
        {0x3E, 0x41, 0x51, 0x21, 0x5E}, /*"Q", 81(ASCII)*/
        {0x7F, 0x09, 0x19, 0x29, 0x46}, /*"R", 82(ASCII)*/
        {0x26, 0x49, 0x49, 0x49, 0x32}, /*"S", 83(ASCII)*/
        {0x01, 0x01, 0x7F, 0x01, 0x01}, /*"T", 84(ASCII)*/
        {0x3F, 0x40, 0x40, 0x40, 0x3F}, /*"U", 85(ASCII)*/
        {0x0F, 0x30, 0x40, 0x30, 0x0F}, /*"V", 86(ASCII)*/
        {0x7F, 0x20, 0x18, 0x20, 0x7F}, /*"W", 87(ASCII)*/
        {0x63, 0x14, 0x08, 0x14, 0x63}, /*"X", 88(ASCII)*/
        {0x07, 0x08, 0x70, 0x08, 0x07}, /*"Y", 89(ASCII)*/
        {0x61, 0x51, 0x49, 0x45, 0x43}, /*"Z", 90(ASCII)*/
        {0x00, 0x7F, 0x41, 0x41, 0x00}, /*"[", 91(ASCII)*/
        {0x02, 0x04, 0x08, 0x10, 0x20}, /*"\", 92(ASCII)*/
        {0x00, 0x41, 0x41, 0x7F, 0x00}, /*"]", 93(ASCII)*/
        {0x00, 0x02, 0x01, 0x02, 0x00}, /*"^", 94(ASCII)*/
        {0x40, 0x40, 0x40, 0x40, 0x40}, /*"_", 95(ASCII)*/
        {0x00, 0x00, 0x01, 0x02, 0x00}, /*"`", 96(ASCII)*/
        {0x20, 0x54, 0x54, 0x54, 0x78}, /*"a", 97(ASCII)*/
        {0x7F, 0x44, 0x44, 0x44, 0x38}, /*"b", 98(ASCII)*/
        {0x38, 0x44, 0x44, 0x44, 0x44}, /*"c", 99(ASCII)*/
        {0x38, 0x44, 0x44, 0x44, 0x7F}, /*"d", 100(ASCII)*/
        {0x38, 0x54, 0x54, 0x54, 0x58}, /*"e", 101(ASCII)*/
        {0x04, 0x04, 0x7E, 0x05, 0x05}, /*"f", 102(ASCII)*/
        {0x4C, 0x52, 0x52, 0x3C, 0x02}, /*"g", 103(ASCII)*/
        {0x7F, 0x08, 0x04, 0x04, 0x78}, /*"h", 104(ASCII)*/
        {0x00, 0x44, 0x7D, 0x40, 0x00}, /*"i", 105(ASCII)*/
        {0x20, 0x40, 0x44, 0x3D, 0x00}, /*"j", 106(ASCII)*/
        {0x00, 0x7F, 0x10, 0x28, 0x44}, /*"k", 107(ASCII)*/
        {0x00, 0x41, 0x7F, 0x40, 0x00}, /*"l", 108(ASCII)*/
        {0x7C, 0x04, 0x78, 0x04, 0x78}, /*"m", 109(ASCII)*/
        {0x7C, 0x08, 0x04, 0x04, 0x78}, /*"n", 110(ASCII)*/
        {0x38, 0x44, 0x44, 0x44, 0x38}, /*"o", 111(ASCII)*/
        {0x7C, 0x24, 0x24, 0x24, 0x18}, /*"p", 112(ASCII)*/
        {0x18, 0x24, 0x24, 0x24, 0x7C}, /*"q", 113(ASCII)*/
        {0x00, 0x7C, 0x08, 0x04, 0x04}, /*"r", 114(ASCII)*/
        {0x48, 0x54, 0x54, 0x54, 0x24}, /*"s", 115(ASCII)*/
        {0x00, 0x04, 0x3F, 0x44, 0x44}, /*"t", 116(ASCII)*/
        {0x3C, 0x40, 0x40, 0x20, 0x7C}, /*"u", 117(ASCII)*/
        {0x0C, 0x30, 0x40, 0x30, 0x0C}, /*"v", 118(ASCII)*/
        {0x7C, 0x20, 0x18, 0x20, 0x7C}, /*"w", 119(ASCII)*/
        {0x44, 0x28, 0x10, 0x28, 0x44}, /*"x", 120(ASCII)*/
        {0x0C, 0x50, 0x50, 0x50, 0x3C}, /*"y", 121(ASCII)*/
        {0x44, 0x64, 0x54, 0x4C, 0x44}, /*"z", 122(ASCII)*/
        {0x00, 0x08, 0x36, 0x41, 0x00}, /*"{", 123(ASCII)*/
        {0x00, 0x00, 0x7F, 0x00, 0x00}, /*"|", 124(ASCII)*/
        {0x00, 0x41, 0x36, 0x08, 0x00}, /*"}", 125(ASCII)*/
        {0x10, 0x08, 0x18, 0x10, 0x08}, /*"~", 126(ASCII)*/
        {0x7F, 0x7F, 0x7F, 0x7F, 0x7F}, /*"", 127(ASCII)*/

    };

    typedef enum
    {
        Dot1_1_1,
        Dot1_2_2_A,
        Dot1_3_3_A,
        Dot_NONE0,
        Dot1_1_6,
        Dot1_2_7_A,
        Dot1_3_8_A,

        Dot1_2_1_A,
        Dot1_3_2_A,
        Dot_NONE1,
        Dot1_1_5,
        Dot1_2_6_A,
        Dot1_3_7_A,
        Dot_NONE2,

        Dot1_3_1_A,
        Dot_NONE3,
        Dot1_1_4,
        Dot1_2_5_A,
        Dot1_3_6_A,
        Dot_NONE4,
        Dot1_1_9,

        Dot_NONE5,
        Dot1_1_3,
        Dot1_2_4_A,
        Dot1_3_5_A,
        Dot_NONE6,
        Dot1_1_8,
        Dot1_2_9_A,

        Dot1_1_2,
        Dot1_2_3_A,
        Dot1_3_4_A,
        Dot_NONE7,
        Dot1_1_7,
        Dot1_2_8_A,
        Dot1_3_9_A,

        Dot_NONE8, // G1

        Dot2_1_1_A,
        Dot1_2_2_B,
        Dot1_3_3_B,
        Dot1_4_4_A,
        Dot2_1_6_A,
        Dot1_2_7_B,
        Dot1_3_8_B,

        Dot1_2_1_B,
        Dot1_3_2_B,
        Dot1_4_3_A,
        Dot2_1_5_A,
        Dot1_2_6_B,
        Dot1_3_7_B,
        Dot1_4_8_A,

        Dot1_3_1_B,
        Dot1_4_2_A,
        Dot2_1_4_A,
        Dot1_2_5_B,
        Dot1_3_6_B,
        Dot1_4_7_A,
        Dot2_1_9_A,

        Dot1_4_1_A,
        Dot2_1_3_A,
        Dot1_2_4_B,
        Dot1_3_5_B,
        Dot1_4_6_A,
        Dot2_1_8_A,
        Dot1_2_9_B,

        Dot2_1_2_A,
        Dot1_2_3_B,
        Dot1_3_4_B,
        Dot1_4_5_A,
        Dot2_1_7_A,
        Dot1_2_8_B,
        Dot1_3_9_B,

        Dot1_4_9_A, // G2

        Dot2_1_1_B,
        Dot2_2_2_A,
        Dot2_3_3_A,
        Dot1_4_4_B,
        Dot2_1_6_B,
        Dot2_2_7_A,
        Dot2_3_8_A,

        Dot2_2_1_A,
        Dot2_3_2_A,
        Dot1_4_3_B,
        Dot2_1_5_B,
        Dot2_2_6_A,
        Dot2_3_7_A,
        Dot1_4_8_B,

        Dot2_3_1_A,
        Dot1_4_2_B,
        Dot2_1_4_B,
        Dot2_2_5_A,
        Dot2_3_6_A,
        Dot1_4_7_B,
        Dot2_1_9_B,

        Dot1_4_1_B,
        Dot2_1_3_B,
        Dot2_2_4_A,
        Dot2_3_5_A,
        Dot1_4_6_B,
        Dot2_1_8_B,
        Dot2_2_9_A,

        Dot2_1_2_B,
        Dot2_2_3_A,
        Dot2_3_4_A,
        Dot1_4_5_B,
        Dot2_1_7_B,
        Dot2_2_8_A,
        Dot2_3_9_A,

        Dot1_4_9_B, // G3

        Dot3_1_1_A,
        Dot2_2_2_B,
        Dot2_3_3_B,
        Dot2_4_4_A,
        Dot3_1_6_A,
        Dot2_2_7_B,
        Dot2_3_8_B,

        Dot2_2_1_B,
        Dot2_3_2_B,
        Dot2_4_3_A,
        Dot3_1_5_A,
        Dot2_2_6_B,
        Dot2_3_7_B,
        Dot2_4_8_A,

        Dot2_3_1_B,
        Dot2_4_2_A,
        Dot3_1_4_A,
        Dot2_2_5_B,
        Dot2_3_6_B,
        Dot2_4_7_A,
        Dot3_1_9_A,

        Dot2_4_1_A,
        Dot3_1_3_A,
        Dot2_2_4_B,
        Dot2_3_5_B,
        Dot2_4_6_A,
        Dot3_1_8_A,
        Dot2_2_9_B,

        Dot3_1_2_A,
        Dot2_2_3_B,
        Dot2_3_4_B,
        Dot2_4_5_A,
        Dot3_1_7_A,
        Dot2_2_8_B,
        Dot2_3_9_B,

        Dot2_4_9_A, // G4

        Dot3_1_1_B,
        Dot3_2_2,
        Dot_NONE9,
        Dot2_4_4_B,
        Dot3_1_6_B,
        Dot3_2_7,
        Dot_NONE10,

        Dot3_2_1,
        Dot_NONE11,
        Dot2_4_3_B,
        Dot3_1_5_B,
        Dot3_2_6,
        Dot_NONE12,
        Dot2_4_8_B,

        Dot_NONE13,
        Dot2_4_2_B,
        Dot3_1_4_B,
        Dot3_2_5,
        Dot_NONE14,
        Dot2_4_7_B,
        Dot3_1_9_B,

        Dot2_4_1_B,
        Dot3_1_3_B,
        Dot3_2_4,
        Dot_NONE15,
        Dot2_4_6_B,
        Dot3_1_8_B,
        Dot3_2_9,

        Dot3_1_2_B,
        Dot3_2_3,
        Dot_NONE16,
        Dot2_4_5_B,
        Dot3_1_7_B,
        Dot3_2_8,
        Dot_NONE17,

        Dot2_4_9_B, // G5
        SYMBOL_MAX
    } Symbols;

public:
    HUV_13SS16T(gpio_num_t din, gpio_num_t clk, gpio_num_t cs, spi_host_device_t spi_num);
    HUV_13SS16T(spi_device_handle_t spi_device);
    void noti_show(const char *str, int timeout = 8000);
    void spectrum_show(float *buf, int size);
    void content_show(int start, const char *buf, int size, bool forceupdate = false, NumAni ani = LEFT2RT);

    void draw_point(int x, int y, uint8_t dot);
    Symbols pixelMap[MAX_Y][MAX_X][2] = {
        {
            {Dot1_1_1, SYMBOL_MAX},
            {Dot1_2_1_A, Dot1_2_1_B},
            {Dot1_3_1_A, Dot1_3_1_B},
            {Dot1_4_1_A, Dot1_4_1_B},
            {Dot2_1_1_A, Dot2_1_1_B},
            {Dot2_2_1_A, Dot2_2_1_B},
            {Dot2_3_1_A, Dot2_3_1_B},
            {Dot2_4_1_A, Dot2_4_1_B},
            {Dot3_1_1_A, Dot3_1_1_B},
            {Dot3_2_1, SYMBOL_MAX},
        },
        {
            {Dot1_1_2, SYMBOL_MAX},
            {Dot1_2_2_A, Dot1_2_2_B},
            {Dot1_3_2_A, Dot1_3_2_B},
            {Dot1_4_2_A, Dot1_4_2_B},
            {Dot2_1_2_A, Dot2_1_2_B},
            {Dot2_2_2_A, Dot2_2_2_B},
            {Dot2_3_2_A, Dot2_3_2_B},
            {Dot2_4_2_A, Dot2_4_2_B},
            {Dot3_1_2_A, Dot3_1_2_B},
            {Dot3_2_2, SYMBOL_MAX},
        },
        {
            {Dot1_1_3, SYMBOL_MAX},
            {Dot1_2_3_A, Dot1_2_3_B},
            {Dot1_3_3_A, Dot1_3_3_B},
            {Dot1_4_3_A, Dot1_4_3_B},
            {Dot2_1_3_A, Dot2_1_3_B},
            {Dot2_2_3_A, Dot2_2_3_B},
            {Dot2_3_3_A, Dot2_3_3_B},
            {Dot2_4_3_A, Dot2_4_3_B},
            {Dot3_1_3_A, Dot3_1_3_B},
            {Dot3_2_3, SYMBOL_MAX},
        },
        {
            {Dot1_1_4, SYMBOL_MAX},
            {Dot1_2_4_A, Dot1_2_4_B},
            {Dot1_3_4_A, Dot1_3_4_B},
            {Dot1_4_4_A, Dot1_4_4_B},
            {Dot2_1_4_A, Dot2_1_4_B},
            {Dot2_2_4_A, Dot2_2_4_B},
            {Dot2_3_4_A, Dot2_3_4_B},
            {Dot2_4_4_A, Dot2_4_4_B},
            {Dot3_1_4_A, Dot3_1_4_B},
            {Dot3_2_4, SYMBOL_MAX},
        },
        {
            {Dot1_1_5, SYMBOL_MAX},
            {Dot1_2_5_A, Dot1_2_5_B},
            {Dot1_3_5_A, Dot1_3_5_B},
            {Dot1_4_5_A, Dot1_4_5_B},
            {Dot2_1_5_A, Dot2_1_5_B},
            {Dot2_2_5_A, Dot2_2_5_B},
            {Dot2_3_5_A, Dot2_3_5_B},
            {Dot2_4_5_A, Dot2_4_5_B},
            {Dot3_1_5_A, Dot3_1_5_B},
            {Dot3_2_5, SYMBOL_MAX},
        },
        {
            {Dot1_1_6, SYMBOL_MAX},
            {Dot1_2_6_A, Dot1_2_6_B},
            {Dot1_3_6_A, Dot1_3_6_B},
            {Dot1_4_6_A, Dot1_4_6_B},
            {Dot2_1_6_A, Dot2_1_6_B},
            {Dot2_2_6_A, Dot2_2_6_B},
            {Dot2_3_6_A, Dot2_3_6_B},
            {Dot2_4_6_A, Dot2_4_6_B},
            {Dot3_1_6_A, Dot3_1_6_B},
            {Dot3_2_6, SYMBOL_MAX},
        },
        {
            {Dot1_1_7, SYMBOL_MAX},
            {Dot1_2_7_A, Dot1_2_7_B},
            {Dot1_3_7_A, Dot1_3_7_B},
            {Dot1_4_7_A, Dot1_4_7_B},
            {Dot2_1_7_A, Dot2_1_7_B},
            {Dot2_2_7_A, Dot2_2_7_B},
            {Dot2_3_7_A, Dot2_3_7_B},
            {Dot2_4_7_A, Dot2_4_7_B},
            {Dot3_1_7_A, Dot3_1_7_B},
            {Dot3_2_7, SYMBOL_MAX},
        },
        {
            {Dot1_1_8, SYMBOL_MAX},
            {Dot1_2_8_A, Dot1_2_8_B},
            {Dot1_3_8_A, Dot1_3_8_B},
            {Dot1_4_8_A, Dot1_4_8_B},
            {Dot2_1_8_A, Dot2_1_8_B},
            {Dot2_2_8_A, Dot2_2_8_B},
            {Dot2_3_8_A, Dot2_3_8_B},
            {Dot2_4_8_A, Dot2_4_8_B},
            {Dot3_1_8_A, Dot3_1_8_B},
            {Dot3_2_8, SYMBOL_MAX},
        },
        {
            {Dot1_1_9, SYMBOL_MAX},
            {Dot1_2_9_A, Dot1_2_9_B},
            {Dot1_3_9_A, Dot1_3_9_B},
            {Dot1_4_9_A, Dot1_4_9_B},
            {Dot3_1_9_A, Dot2_1_9_B},
            {Dot2_2_9_A, Dot2_2_9_B},
            {Dot2_3_9_A, Dot2_3_9_B},
            {Dot2_4_9_A, Dot2_4_9_B},
            {Dot3_1_9_A, Dot3_1_9_B},
            {Dot3_2_9, SYMBOL_MAX},
        },
    };

private:
    bool cgramBusy[3] = {0};
    ContentData currentContentData[DISPLAY_SIZE] = {0};
    ContentData tempContentData[DISPLAY_SIZE] = {0};
    CircularBuffer *cb = new CircularBuffer();
    int64_t content_inhibit_time = 0;
    void init_task();
    void contentanimate();
    int get_cgram();
    void free_cgram(int *index);
    void display_buffer();
    void scroll_buffer();
    const uint8_t *find_content_hex_code(char ch);

    void refrash(Gram *gram) override;
    void find_enum_code(Symbols flag, int *byteIndex, int *bitMask);
    void draw_code(Symbols flag, uint8_t dot);

protected:
    void charhelper(int index, char ch);                     // write to dcram
    void charhelper(int index, int ramindex, uint8_t *code); // write to cgram, then index from the dcram
    void noti_show(int start, const char *buf, int size, bool forceupdate = false, NumAni ani = LEFT2RT, int timeout = 2000);
};

#endif // _HUV_13SS16T_H_
