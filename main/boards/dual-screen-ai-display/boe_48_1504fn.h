/**
 * @file BOE_48_1504FN.h
 * @brief This header file defines the BOE_48_1504FN class, which is used to control the display of a specific device and inherits from the PT6302 class.
 *
 * This class provides a series of methods for displaying spectrum, numbers, symbols, dot matrix and other information, and also supports animation effects.
 *
 * @author Shihua Feng
 * @date February 18, 2025
 */

#ifndef _BOE_48_1504FN_H_
#define _BOE_48_1504FN_H_

#include "pt6302.h"
#include <cmath>
#include <esp_wifi.h>

// Define the number of characters
#define CHAR_SIZE (95 + 1)

/**
 * @class BOE_48_1504FN
 * @brief This class inherits from the PT6302 class and is used to control the display of a specific device.
 *
 * It provides methods for displaying spectrum, numbers, symbols, dot matrix and other information, and also supports animation effects.
 */
class BOE_48_1504FN : protected PT6302
{
#define BUFFER_SIZE 256
#define DISPLAY_SIZE 10
#define SYMBOL_COUNT 162
#define SYMBOL_CGRAM_SIZE 5
#define GR_COUNT 15
public:
    typedef struct
    {
        int byteIndex; // Byte index
        int bitMask;   // Bit index
    } SymbolPosition;

    typedef enum
    {
        L_0_0,
        L_3_9,
        L_4_7,
        L_3_4,
        L_4_2,
        L_3_13,
        L_4_11,
        L_1_0,
        L_4_9,
        L_3_6,
        L_4_4,
        L_3_1,
        L_4_13,
        L_OUTLINE,
        L_2_0,
        L_3_8,
        L_4_6,
        L_3_3,
        L_4_1,
        L_3_12,
        L_3_10,
        L_4_8,
        L_3_5,
        L_4_3,
        L_3_0,
        L_4_12,
        L_4_10,
        L_3_7,
        L_4_5,
        L_3_2,
        L_4_0,
        L_3_11,
        LD_0_0,
        LD_LINE,
        LD_6,
        LD_4,
        LD_1,
        LD_1_0,
        LD_MUTE,
        LD_DTS,
        LD_IPOD,
        LD_2_0,
        LD_REC,
        LD_II,
        LD_KARAOKE,
        LD_3_0,
        LD_CLOCK,
        LD_DB_PL,
        LD_3,
        LD_4_0,
        LD_DB_D,
        LD_5,
        LD_2,
        D_DIVX,
        D_CD,
        D_DVD_LINE,
        D_XDSS,
        D_2_L,
        D_CCN_L,
        D_OUTLINE_3,
        D_USB,
        D_DIVX_L,
        D_CD_L,
        D_USER,
        D_CCN_R,
        D_1_OUTLINE,
        D_OUTLINE_2,
        D_AUX,
        D_USB_L,
        D_OUTLINE,
        D_2_OUTLINE,
        D_CCN_D,
        D_1_R,
        D_OUTLINE_1,
        D_MUSIC,
        D_AUX_L,
        D_MP3OPT,
        D_2_R,
        D_CCN,
        D_1,
        D_OUTLINE_0,
        D_DVD,
        D_MUSIC_L,
        D_VSM,
        D_2,
        D_CCN_U,
        D_1_L,
        D_PAUSE,
        RD_0_0,
        RD_LINE,
        RD_1,
        RD_USB,
        RD_2_CIRCLE,
        RD_1_NUM,
        RD_1_0,
        RD_DUBB,
        RD_1_P2,
        RD_3_CIRCLE,
        RD_2,
        RD_SURR,
        RD_2_0,
        RD_MIC,
        RD_1_P1,
        RD_3,
        RD_2_P1,
        RD_BAT,
        RD_3_0,
        RD_V_FADE,
        RD_ECHO,
        RD_3_P1,
        RD_2_P2,
        RD_4_0,
        RD_1_CIRCLE,
        RD_REC,
        RD_3_P2,
        RD_2_NUM,
        R_0_0,
        R_3_9,
        R_4_7,
        R_3_4,
        R_4_2,
        R_3_13,
        R_4_11,
        R_1_0,
        R_4_9,
        R_3_6,
        R_4_4,
        R_3_1,
        R_4_13,
        R_OUTLINE,
        R_2_0,
        R_3_8,
        R_4_6,
        R_3_3,
        R_4_1,
        R_3_12,
        R_3_10,
        R_4_8,
        R_3_5,
        R_4_3,
        R_3_0,
        R_4_12,
        R_4_10,
        R_3_7,
        R_4_5,
        R_3_2,
        R_4_0,
        R_3_11,
        L_LINE,
        CHP_TRK,
        EQ,
        PROG,
        RANDOM,
        REPEAT,
        DISC,
        ALL,
        SLEEP,
        SYNC,
        NET,
        R_LINE,
        RDS,
        PLAY,
        SYMBOL_MAX
    } Symbols;

    SymbolPosition symbolPositions[SYMBOL_COUNT] = {
        {0, 1},       // L_0_0
        {0, 2},       // L_3_9
        {0, 4},       // L_4_7
        {0, 8},       // L_3_4
        {0, 0x10},    // L_4_2
        {0, 0x20},    // L_3_13
        {0, 0x40},    // L_4_11
        {1, 1},       // L_1_0
        {1, 2},       // L_4_9
        {1, 4},       // L_3_6
        {1, 8},       // L_4_4
        {1, 0x10},    // L_3_1
        {1, 0x20},    // L_4_13
        {1, 0x40},    // L_OUTLINE
        {2, 1},       // L_2_0
        {2, 2},       // L_3_8
        {2, 4},       // L_4_6
        {2, 8},       // L_3_3
        {2, 0x10},    // L_4_1
        {2, 0x20},    // L_3_12
        {3, 1},       // L_3_10
        {3, 2},       // L_4_8
        {3, 4},       // L_3_5
        {3, 8},       // L_4_3
        {3, 0x10},    // L_3_0
        {3, 0x20},    // L_4_12
        {4, 1},       // L_4_10
        {4, 2},       // L_3_7
        {4, 4},       // L_4_5
        {4, 8},       // L_3_2
        {4, 0x10},    // L_4_0
        {4, 0x20},    // L_3_11
        {5, 1},       // LD_0_0
        {5, 2},       // LD_LINE
        {5, 4},       // LD_6
        {5, 8},       // LD_4
        {5, 0x10},    // LD_1
        {6, 1},       // LD_1_0
        {6, 2},       // LD_MUTE
        {6, 4},       // LD_DTS
        {6, 8},       // LD_IPOD
        {7, 1},       // LD_2_0
        {7, 2},       // LD_REC
        {7, 4},       // LD_II
        {7, 8},       // LD_KARAOKE
        {8, 1},       // LD_3_0
        {8, 2},       // LD_CLOCK
        {8, 4},       // LD_DB_PL
        {8, 8},       // LD_3
        {9, 1},       // LD_4_0
        {9, 2},       // LD_DB_D
        {9, 4},       // LD_5
        {9, 8},       // LD_2
        {10, 1},      // D_DIVX
        {10, 2},      // D_CD
        {10, 4},      // D_DVD_LINE
        {10, 8},      // D_XDSS
        {10, 0x10},   // D_2_L
        {10, 0x20},   // D_CCN_L
        {10, 0x40},   // D_OUTLINE_3
        {11, 1},      // D_USB
        {11, 2},      // D_DIVX_L
        {11, 4},      // D_CD_L
        {11, 8},      // D_USER
        {11, 0x10},   // D_CCN_R
        {11, 0x20},   // D_1_OUTLINE
        {11, 0x40},   // D_OUTLINE_2
        {12, 1},      // D_AUX
        {12, 2},      // D_USB_L
        {12, 4},      // D_OUTLINE
        {12, 8},      // D_2_OUTLINE
        {12, 0x10},   // D_CCN_D
        {12, 0x20},   // D_1_R
        {12, 0x40},   // D_OUTLINE_1
        {13, 1},      // D_MUSIC
        {13, 2},      // D_AUX_L
        {13, 4},      // D_MP3OPT
        {13, 8},      // D_2_R
        {13, 0x10},   // D_CCN
        {13, 0x20},   // D_1
        {13, 0x40},   // D_OUTLINE_0
        {14, 1},      // D_DVD
        {14, 2},      // D_MUSIC_L
        {14, 4},      // D_VSM
        {14, 8},      // D_2
        {14, 0x10},   // D_CCN_U
        {14, 0x20},   // D_1_L
        {14, 0x40},   // D_PAUSE
        {15, 1},      // RD_0_0
        {15, 2},      // RD_LINE
        {15, 4},      // RD_1
        {15, 8},      // RD_USB
        {15, 0x10},   // RD_2_CIRCLE
        {15, 0x20},   // RD_1_NUM
        {16, 1},      // RD_1_0
        {16, 2},      // RD_DUBB
        {16, 4},      // RD_1_P0
        {16, 8},      // RD_3_CIRCLE
        {16, 0x10},   // RD_2
        {16, 0x20},   // RD_SURR
        {17, 1},      // RD_2_0
        {17, 2},      // RD_MIC
        {17, 4},      // RD_1_P1
        {17, 8},      // RD_3
        {17, 0x10},   // RD_2_P1
        {17, 0x20},   // RD_BAT
        {18, 1},      // RD_3_0
        {18, 2},      // RD_V_FADE
        {18, 4},      // RD_ECHO
        {18, 8},      // RD_3_P1
        {18, 0x10},   // RD_2_P2
        {19, 1},      // RD_4_0
        {19, 2},      // RD_1_CIRCLE
        {19, 4},      // RD_REC
        {19, 8},      // RD_3_P2
        {19, 0x10},   // RD_2_NUM
        {20, 1},      // R_0_0
        {20, 2},      // R_3_9
        {20, 4},      // R_4_7
        {20, 8},      // R_3_4
        {20, 0x10},   // R_4_2
        {20, 0x20},   // R_3_13
        {20, 0x40},   // R_4_11
        {21, 1},      // R_1_0
        {21, 2},      // R_4_9
        {21, 4},      // R_3_6
        {21, 8},      // R_4_4
        {21, 0x10},   // R_3_1
        {21, 0x20},   // R_4_13
        {21, 0x40},   // R_OUTLINE
        {22, 1},      // R_2_0
        {22, 2},      // R_3_8
        {22, 4},      // R_4_6
        {22, 8},      // R_3_3
        {22, 0x10},   // R_4_1
        {22, 0x20},   // R_3_12
        {23, 1},      // R_3_10
        {23, 2},      // R_4_8
        {23, 4},      // R_3_5
        {23, 8},      // R_4_3
        {23, 0x10},   // R_3_0
        {23, 0x20},   // R_4_12
        {24, 1},      // R_4_10
        {24, 2},      // R_3_7
        {24, 4},      // R_4_5
        {24, 8},      // R_3_2
        {24, 0x10},   // R_4_0
        {24, 0x20},   // R_3_11
        {25 + 0, 1},  // L_LINE
        {25 + 0, 2},  // CHP/TRK
        {25 + 1, 2},  // EQ
        {25 + 2, 2},  // PROG
        {25 + 3, 2},  // RANDOM
        {25 + 4, 2},  // REPEAT
        {25 + 5, 1},  // DISC
        {25 + 5, 2},  // ALL
        {25 + 6, 2},  // SLEEP
        {25 + 7, 2},  // SYNC
        {25 + 8, 2},  // NET
        {25 + 9, 1},  // R_LINE
        {25 + 9, 2},  // RDS
        {25 + 12, 1}, // PLAY
    };

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

public:
    BOE_48_1504FN(gpio_num_t din, gpio_num_t clk, gpio_num_t cs, spi_host_device_t spi_num);
    BOE_48_1504FN(spi_device_handle_t spi_device);
    void noti_show(const char *str, int timeout = 8000);
    void spectrum_show(float *buf, int size);
    void content_show(int start, const char *buf, int size, bool forceupdate = false, NumAni ani = LEFT2RT);
    void symbolhelper(Symbols symbol, bool is_on);

private:
    const unsigned int digits = 15;
    bool cgramBusy[3] = {0};
    ContentData currentContentData[DISPLAY_SIZE] = {0};
    ContentData tempContentData[DISPLAY_SIZE] = {0};
    CircularBuffer *cb = new CircularBuffer();
    int64_t content_inhibit_time = 0;
    Symbols bar_L_3[14] = {L_3_0, L_3_1, L_3_2, L_3_3, L_3_4, L_3_5, L_3_6, L_3_7, L_3_8, L_3_9, L_3_10, L_3_11, L_3_12, L_3_13};
    Symbols bar_L_4[14] = {L_4_0, L_4_1, L_4_2, L_4_3, L_4_4, L_4_5, L_4_6, L_4_7, L_4_8, L_4_9, L_4_10, L_4_11, L_4_12, L_4_13};
    Symbols bar_R_3[14] = {R_3_0, R_3_1, R_3_2, R_3_3, R_3_4, R_3_5, R_3_6, R_3_7, R_3_8, R_3_9, R_3_10, R_3_11, R_3_12, R_3_13};
    Symbols bar_R_4[14] = {R_4_0, R_4_1, R_4_2, R_4_3, R_4_4, R_4_5, R_4_6, R_4_7, R_4_8, R_4_9, R_4_10, R_4_11, R_4_12, R_4_13};
    void init_task();
    void contentanimate();
    int get_cgram();
    void free_cgram(int *index);
    void display_buffer();
    void scroll_buffer();
    const uint8_t *find_content_hex_code(char ch);
    void find_enum_code(Symbols flag, int *byteIndex, int *bitMask);

    void refrash(Gram *gram) override;
protected:
    void charhelper(int index, char ch);                     // write to dcram
    void charhelper(int index, int ramindex, uint8_t *code); // write to cgram, then index from the dcram
    void noti_show(int start, const char *buf, int size, bool forceupdate = false, NumAni ani = LEFT2RT, int timeout = 2000);
};

#endif // _BOE_48_1504FN_H_
