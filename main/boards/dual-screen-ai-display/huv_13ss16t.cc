/*
 * Author: 施华锋
 * Date: 2025-02-12
 * Description: This file implements the methods of the HUV_13SS16T class for communicating with the HUV_13SS16T device via SPI.
 */

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "huv_13ss16t.h"
#include "string.h"
#include "math.h"

#define TAG "HUV_13SS16T"
// 0x00-0x0D 原始数据，可带最多9*13参数，地址自增
// 0x20-0x3F 原始点阵数据，bit 4-2:x, bit 0:y 后须带5的倍数数据，地址自增
// 0x40-0x5F 原始数字数据，bit 4-0:x，地址自增
// 0x60 图标，byte1位置，共49个，地址自增
// 0x80-0x9F 数字ASCII，bit 4-1:x 地址自增
// 0xA0-0xAF 点阵ASCII，bit 4-2:x, bit 0:y ，地址自增
// 0xB0-0xBF 命令
// 0xB0 dimming: 0-100
void HUV_13SS16T::write_data8(uint8_t *dat, int len)
{
    // Create an SPI transaction structure and initialize it to zero
    spi_transaction_t t;
    memset(&t, 0, sizeof(t));

    // Set the length of the transaction in bits
    t.length = len * 8;

    // Set the pointer to the data buffer to be transmitted
    t.tx_buffer = dat;

    // Queue the SPI transaction. This function will block until the transaction can be queued.
    ESP_ERROR_CHECK(spi_device_queue_trans(spi_device_, &t, portMAX_DELAY));

    // The following code can be uncommented if you need to wait for the transaction to complete and verify the result
    spi_transaction_t *ret_trans;
    ESP_ERROR_CHECK(spi_device_get_trans_result(spi_device_, &ret_trans, portMAX_DELAY));
    assert(ret_trans == &t);
    // vTaskDelay(pdMS_TO_TICKS(10));

    return;
}

HUV_13SS16T::HUV_13SS16T(gpio_num_t din, gpio_num_t clk, gpio_num_t cs, spi_host_device_t spi_num)
{
    // Initialize the SPI bus configuration structure
    spi_bus_config_t buscfg = {0};

    // Log the initialization process
    ESP_LOGI(TAG, "Initialize HUV_13SS16T SPI bus");

    // Set the clock and data pins for the SPI bus
    buscfg.sclk_io_num = clk;
    buscfg.data0_io_num = din;

    // Set the maximum transfer size in bytes
    buscfg.max_transfer_sz = 256;

    // Initialize the SPI bus with the specified configuration
    ESP_ERROR_CHECK(spi_bus_initialize(spi_num, &buscfg, SPI_DMA_CH_AUTO));

    // Initialize the SPI device interface configuration structure
    spi_device_interface_config_t devcfg = {
        .mode = 0,                 // Set the SPI mode to 3
        .clock_speed_hz = 1000000, // Set the clock speed to 1MHz
        .spics_io_num = cs,        // Set the chip select pin
        .queue_size = 7,
    };

    // Add the HUV_13SS16T device to the SPI bus with the specified configuration
    ESP_ERROR_CHECK(spi_bus_add_device(spi_num, &devcfg, &spi_device_));
    init_task();
    ESP_LOGI(TAG, "HUV_13SS16T Initalized");
}

HUV_13SS16T::HUV_13SS16T(spi_device_handle_t spi_device) : spi_device_(spi_device)
{
    init_task();
    ESP_LOGI(TAG, "HUV_13SS16T Initalized");
}

void HUV_13SS16T::init_task()
{
    xTaskCreate(
        [](void *arg)
        {
            int count = 0;
            int16_t raw_acce_x;
            int16_t raw_acce_y;
            int16_t raw_acce_z;
            HUV_13SS16T *vfd = static_cast<HUV_13SS16T *>(arg);
            vfd->initialize_points();
            while (true)
            {
                if (vfd->acceCallback != nullptr)
                {
                    vfd->acceCallback(vfd->handle_, &raw_acce_x, &raw_acce_y, &raw_acce_z);
                    vfd->liquid_pixels(raw_acce_x, raw_acce_y, raw_acce_z);
                }
                // vfd->clear_point();
                // for (size_t i = 0; i < MAX_X; i++)
                // {
                //     for (size_t j = 0; j < MAX_Y; j++)
                //     {
                //         vfd->draw_point(i, j, (j + i + count) % 2);
                //     }
                // }

                if (!((++count) % 12))
                {
                    vfd->display_buffer();
                    vfd->scroll_buffer();
                }
                vfd->refrash();
                vTaskDelay(pdMS_TO_TICKS(30));
                vfd->pixelanimate();
            }
            vTaskDelete(NULL);
        },
        "vfd",
        4096,
        this,
        6,
        nullptr);
}

void HUV_13SS16T::test()
{
    static bool face = false;
    for (size_t i = 0; i < sizeof pixel_gram; i++)
    {
        if (face)
            pixel_gram[i] = rand();
        else
            pixel_gram[i] = 0xFF;
    }
    face = !face;
    refrash();
}

void HUV_13SS16T::setbrightness(uint8_t brightness)
{
    dimming = brightness;
    if (dimming < 5)
        dimming = 5;
}

void HUV_13SS16T::setsleep(bool en)
{
    if (en)
    {
        memset(pixel_gram, 0, sizeof pixel_gram);
    }
}

void HUV_13SS16T::noti_show(int start, const char *buf, int size, bool forceupdate, NumAni ani, int timeout)
{
    content_inhibit_time = esp_timer_get_time() / 1000 + timeout;
    for (size_t i = 0; i < PIXEL_COUNT; i++)
    {
        currentPixelData[i].animation_type = ani;
        currentPixelData[i].current_content = ' ';
        if (forceupdate)
            currentPixelData[i].need_update = true;
    }
    for (size_t i = 0; i < size && (start + i) < PIXEL_COUNT; i++)
    {
        currentPixelData[start + i].animation_type = ani;
        currentPixelData[start + i].current_content = buf[i];
        if (forceupdate)
            currentPixelData[start + i].need_update = true;
    }
}

void HUV_13SS16T::pixel_show(int start, const char *buf, int size, bool forceupdate, NumAni ani)
{
    if (content_inhibit_time != 0)
    {
        for (size_t i = 0; i < size && (start + i) < PIXEL_COUNT; i++)
        {
            tempPixelData[start + i].animation_type = ani;
            tempPixelData[start + i].current_content = buf[i];
            if (forceupdate)
                tempPixelData[start + i].need_update = true;
        }
        return;
    }
    for (size_t i = 0; i < size && (start + i) < PIXEL_COUNT; i++)
    {
        currentPixelData[start + i].animation_type = ani;
        currentPixelData[start + i].current_content = buf[i];
        if (forceupdate)
            currentPixelData[start + i].need_update = true;
    }
}

const uint8_t *HUV_13SS16T::find_pixel_hex_code(char ch)
{
    if (ch >= ' ' && ch <= ('~' + 1))
        return hex_codes[ch - ' '];
    return hex_codes[0];
}

uint8_t HUV_13SS16T::find_num_hex_code(char ch)
{
    if (ch >= ' ' && ch <= 'Z')
        return num_hex_codes[ch - ' '];
    else if (ch >= 'a' && ch <= 'z')
        return num_hex_codes[ch - 'a' + 'A' - ' '];
    return 0;
}

void HUV_13SS16T::pixelanimate()
{
    static int64_t start_time = esp_timer_get_time() / 1000;
    int64_t current_time = esp_timer_get_time() / 1000;
    int64_t elapsed_time = current_time - start_time;
    uint8_t temp_code[5] = {0};

    if (elapsed_time >= 30)
        start_time = current_time;
    else
        return;

    if (content_inhibit_time != 0)
    {
        elapsed_time = current_time - content_inhibit_time;
        if (elapsed_time > 0)
        {
            for (size_t i = 0; i < PIXEL_COUNT; i++)
            {
                currentPixelData[i].last_content = currentPixelData[i].current_content;
                currentPixelData[i].animation_type = tempPixelData[i].animation_type;
                currentPixelData[i].current_content = tempPixelData[i].current_content;
                currentPixelData[i].need_update = true;
            }
            content_inhibit_time = 0;
        }
    }

    for (int i = 0; i < PIXEL_COUNT; i++)
    {
        if (currentPixelData[i].current_content != currentPixelData[i].last_content || currentPixelData[i].need_update)
        {
            currentPixelData[i].animation_step++;
            const uint8_t *before_raw_code = find_pixel_hex_code(currentPixelData[i].last_content);
            const uint8_t *raw_code = find_pixel_hex_code(currentPixelData[i].current_content);
            if (currentPixelData[i].animation_type == UP2DOWN)
            {
                for (int j = 0; j < 5; j++)
                    temp_code[j] = (before_raw_code[j] << currentPixelData[i].animation_step) | (raw_code[j] >> (8 - currentPixelData[i].animation_step));

                if (currentPixelData[i].animation_step >= 8)
                    currentPixelData[i].animation_step = -1;
            }
            else if (currentPixelData[i].animation_type == DOWN2UP)
            {
                for (int j = 0; j < 5; j++)
                    temp_code[j] = (before_raw_code[j] >> currentPixelData[i].animation_step) | (raw_code[j] << (8 - currentPixelData[i].animation_step));

                if (currentPixelData[i].animation_step >= 8)
                    currentPixelData[i].animation_step = -1;
            }
            else if (currentPixelData[i].animation_type == LEFT2RT)
            {
                switch (currentPixelData[i].animation_step)
                {
                case 0:
                    for (int j = 0; j < 4; j++)
                        temp_code[j] = before_raw_code[j + 1];
                    for (int j = 4; j < 5; j++)
                        temp_code[j] = raw_code[j - 4];
                    break;
                case 1:
                    for (int j = 0; j < 3; j++)
                        temp_code[j] = before_raw_code[j + 2];
                    for (int j = 3; j < 5; j++)
                        temp_code[j] = raw_code[j - 3];
                    break;
                case 2:
                    for (int j = 0; j < 2; j++)
                        temp_code[j] = before_raw_code[j + 3];
                    for (int j = 2; j < 5; j++)
                        temp_code[j] = raw_code[j - 2];
                    break;
                case 3:
                    for (int j = 0; j < 1; j++)
                        temp_code[j] = before_raw_code[j + 4];
                    for (int j = 1; j < 5; j++)
                        temp_code[j] = raw_code[j - 1];
                    break;
                default:
                    currentPixelData[i].animation_step = -1;
                    break;
                }
            }
            else if (currentPixelData[i].animation_type == RT2LEFT)
            {
                switch (currentPixelData[i].animation_step)
                {
                case 0:
                    for (int j = 0; j < 4; j++)
                        temp_code[j + 1] = before_raw_code[j];
                    for (int j = 4; j < 5; j++)
                        temp_code[j - 4] = raw_code[j];
                    break;
                case 1:
                    for (int j = 0; j < 3; j++)
                        temp_code[j + 2] = before_raw_code[j];
                    for (int j = 3; j < 5; j++)
                        temp_code[j - 3] = raw_code[j];
                    break;
                case 2:
                    for (int j = 0; j < 2; j++)
                        temp_code[j + 3] = before_raw_code[j];
                    for (int j = 2; j < 5; j++)
                        temp_code[j - 2] = raw_code[j];
                    break;
                case 3:
                    for (int j = 0; j < 1; j++)
                        temp_code[j + 4] = before_raw_code[j];
                    for (int j = 1; j < 5; j++)
                        temp_code[j - 1] = raw_code[j];
                    break;
                default:
                    currentPixelData[i].animation_step = -1;
                    break;
                }
            }
            else
                currentPixelData[i].animation_step = -1;

            if (currentPixelData[i].animation_step == -1)
            {
                currentPixelData[i].need_update = false;
                currentPixelData[i].last_content = currentPixelData[i].current_content;
                memcpy(temp_code, raw_code, sizeof temp_code);
            }

            pixelhelper(i, temp_code);
        }
    }
}

uint8_t HUV_13SS16T::contentgetpart(uint8_t raw, uint8_t before_raw, uint8_t mask)
{
    return (raw & mask) | (before_raw & (~mask));
}

void HUV_13SS16T::pixelhelper(int index, uint8_t *code)
{
    memcpy(pixel_gram + index * 5, code, 5);
}

void HUV_13SS16T::time_blink()
{
    static bool time_mark = true;
    time_mark = !time_mark;
}

void HUV_13SS16T::set_fonttype(int index)
{
#if USE_MUTI_FONTS
    ESP_LOGI(TAG, "fonttype: %d", index % 6);
    hex_codes = hex_codes_map[index % 6];
#endif
}

void HUV_13SS16T::clear_point()
{
    memset(matrix_gram, 0, sizeof matrix_gram);
}

void HUV_13SS16T::draw_point(int x, int y, uint8_t dot)
{
    if (x >= MAX_X)
        return;
    if (y >= MAX_Y)
        return;
    matrix_gram[x][y] = dot;
}

void HUV_13SS16T::refrash() // origin
{
    static uint8_t lastdimming = 0;
    if (lastdimming != dimming)
    {
        lastdimming = dimming;
        dimming_write(dimming);
    }
    pixel_write(0, 0, pixel_gram, (sizeof pixel_gram) / 5);
    vTaskDelay(pdMS_TO_TICKS(10));
    matrix_write(&matrix_gram[0][0]);
    // num_write(0, num_gram, sizeof num_gram);
    // icon_write(0, icon_gram, sizeof icon_gram);
}

void HUV_13SS16T::pixel_write(int x, int y, const uint8_t *code, int len)
{
    uint8_t temp_gram[PIXEL_COUNT * 5 + 1];
    temp_gram[0] = 0x20;
    temp_gram[0] |= (x & 0xF) << 1;
    temp_gram[0] |= y != 0;
    memcpy(temp_gram + 1, code, len * 5);
    write_data8(temp_gram, len * 5 + 1);
}

void HUV_13SS16T::pixel_write(int x, int y, const char *ascii, int len)
{
    uint8_t temp_gram[PIXEL_COUNT + 1];
    temp_gram[0] = 0xA0;
    temp_gram[0] |= (x & 0xF) << 1;
    temp_gram[0] |= y != 0;
    memcpy(temp_gram + 1, ascii, len);
    write_data8(temp_gram, len + 1);
}

void HUV_13SS16T::matrix_write(const uint8_t *code)
{
    uint8_t temp_gram[MAX_X * MAX_Y + 1];
    temp_gram[0] = 0x40;
    for (size_t j = 0; j < MAX_Y; j++)
        for (size_t i = 0; i < MAX_X; i++)
            temp_gram[j * MAX_X + i + 1] = matrix_gram[i][j];
    write_data8(temp_gram, MAX_X * MAX_Y + 1);
}

void HUV_13SS16T::icon_write(Symbols icon, bool en)
{
    uint8_t temp_gram[1 + 2];
    temp_gram[0] = 0x60;
    temp_gram[1] = (uint8_t)icon;
    temp_gram[2] = en;

    write_data8(temp_gram, 3);
}

void HUV_13SS16T::dimming_write(int val)
{
    uint8_t temp_gram[2];
    temp_gram[0] = 0xB0;
    temp_gram[1] = val;

    write_data8(temp_gram, 2);
}


void HUV_13SS16T::display_buffer()
{
    int64_t current_time = esp_timer_get_time() / 1000;
    if (content_inhibit_time != 0)
    {
        int64_t elapsed_time = current_time - content_inhibit_time;
        if (elapsed_time > 0)
        {
            memset(cb->buffer, 0, sizeof cb->buffer);
            cb->start_pos = 0;
            cb->length = 0;
            content_inhibit_time = 0;
        }
    }
    else
        return;
    if (cb->length)
    {
        if (cb->length <= DISPLAY_SIZE)
        {
            for (size_t i = 0; i < DISPLAY_SIZE; i++)
            {
                currentPixelData[i].current_content = cb->buffer[i];
            }
            // ESP_LOGI(TAG, "%s", cb->buffer);
        }
        else
        {
            // Scroll display for length > 10
            char display[DISPLAY_SIZE + 1];
            for (int i = 0; i < DISPLAY_SIZE; i++)
            {
                int pos = (cb->start_pos + i) % cb->length;
                display[i] = cb->buffer[pos];
            }
            for (size_t i = 0; i < DISPLAY_SIZE; i++)
            {
                currentPixelData[i].current_content = display[i];
            }

            // ESP_LOGI(TAG, "%s", display);
        }
    }
}
void HUV_13SS16T::scroll_buffer()
{
    if (cb->length > DISPLAY_SIZE)
    {
        cb->start_pos = (cb->start_pos + 1) % cb->length;
    }
}

void HUV_13SS16T::noti_show(const char *str, int timeout)
{
    content_inhibit_time = esp_timer_get_time() / 1000 + timeout;
    int str_len = strlen(str);
    if (str_len > BUFFER_SIZE)
    {
        // Only keep the last BUFFER_SIZE characters if input is too long
        str += (str_len - BUFFER_SIZE);
        str_len = BUFFER_SIZE;
    }
    {
        memset(cb->buffer, 0, sizeof cb->buffer);
        cb->start_pos = 0;
        cb->length = 0;
        if (str_len + cb->length <= (BUFFER_SIZE - 2))
        {
            // Simple append
            strncpy(cb->buffer + cb->length, str, str_len);
            cb->length += str_len;
            if (str_len > DISPLAY_SIZE)
            {
                memset(cb->buffer + cb->length, ' ', 2);
                cb->length += 2;
            }
        }
        else
        {
            // Need to wrap around
            int remaining = BUFFER_SIZE - cb->length;
            strncpy(cb->buffer + cb->length, str, remaining);
            strncpy(cb->buffer, str + remaining, str_len - remaining);
            cb->length = BUFFER_SIZE;
            cb->buffer[cb->length] = '\0';
        }
    }
}

float FunctionsPitchRoll(float A, float B, float C)
{
    float DatoA, DatoB, Value;
    DatoA = A;
    DatoB = (B * B) + (C * C);
    DatoB = sqrt(DatoB);

    Value = atan2(DatoA, DatoB);
    Value = Value * 180 / 3.14;

    return Value;
}

void HUV_13SS16T::liquid_pixels(float AcX, float AcY, float AcZ)
{
    // Wire.beginTransmission(MPU);
    // Wire.write(0x3B); // starting with register 0x3B (ACCEL_XOUT_H)
    // Wire.endTransmission(false);
    // Wire.requestFrom((uint8_t)MPU,(size_t)14,true); // request a total of 14 registers
    // AcX=Wire.read()<<8|Wire.read(); // 0x3B (ACCEL_XOUT_H) & 0x3C (ACCEL_XOUT_L)
    // AcY=Wire.read()<<8|Wire.read(); // 0x3D (ACCEL_YOUT_H) & 0x3E (ACCEL_YOUT_L)
    // AcZ=Wire.read()<<8|Wire.read(); // 0x3F (ACCEL_ZOUT_H) & 0x40 (ACCEL_ZOUT_L)
    // Tmp=Wire.read()<<8|Wire.read(); // 0x41 (TEMP_OUT_H) & 0x42 (TEMP_OUT_L)
    // GyX=Wire.read()<<8|Wire.read(); // 0x43 (GYRO_XOUT_H) & 0x44 (GYRO_XOUT_L)
    // GyY=Wire.read()<<8|Wire.read(); // 0x45 (GYRO_YOUT_H) & 0x46 (GYRO_YOUT_L)
    // GyZ=Wire.read()<<8|Wire.read(); // 0x47 (GYRO_ZOUT_H) & 0x48 (GYRO_ZOUT_L)

    // calibration
    // Roll:3.58  Pitch: -4.30

    int Roll = int(FunctionsPitchRoll(AcX, AcY, AcZ) - 3.58);  // Calcolo angolo Roll
    int Pitch = int(FunctionsPitchRoll(AcY, AcX, AcZ) + 4.30); // Calcolo angolo Pitch
    // Serial.println("Roll:" + (String)Roll +"  Pitch: " + (String)Pitch) ;
    // byte btn = digitalRead(BTN_PIN);

    // Calcolo dei vettori di movimento dovuti all'inclinazione del dispositivo
    float dx = -sin(Roll * PI / 180.0);
    float dy = sin(Pitch * PI / 180.0);

    // matrix.fillScreen(0);
    clear_point();

    for (int i = 0; i < DOTS; i++)
    {

        float tx = points[i].x + points[i].vx;
        float ty = points[i].y + points[i].vy;

        // new position to check
        int ntx = round(tx);
        int nty = round(ty);

        if ((-1 == occupancy[ntx][nty] || occupancy[ntx][nty] == i) && ntx >= 0 && ntx < 10 && nty >= 0 && nty < 10)
        {
            occupancy[points[i].px][points[i].py] = -1;
            // free, new position is available
            points[i].x = tx;
            points[i].y = ty;
            points[i].px = ntx;
            points[i].py = nty;
            points[i].vx = (points[i].vx + dx) * FRICTION;
            points[i].vy = (points[i].vy + dy) * FRICTION;
            occupancy[ntx][nty] = i;
            // Serial.println(" FREE");
        }
        else
        {

            // forse questo bloco va fatto prima e poi
            // va fatto il riposizionamento.
            // in entrambi i casi devo ricalcolare posizioni, non ha senso che qui faccio solo vettori
            // (o forse ha senso per risparmiare calcoli)

            if (ntx < 0 || ntx > 9 || nty < 0 || nty > 9)
            {

                if (ntx > 9)
                {
                    points[i].vx = -points[i].vx * FRICTION;
                }
                if (ntx < 0)
                {
                    points[i].vx = -points[i].vx * FRICTION;
                }

                if (nty > 9)
                {
                    points[i].vy = -points[i].vy * FRICTION;
                }
                if (nty < 0.0)
                {
                    points[i].vy = -points[i].vy * FRICTION;
                }
            }
            else
            {

                int j = occupancy[ntx][nty];
                // CALC relative velocity for collision
                float Dvx = points[j].vx - points[i].vx;
                float Dvy = points[j].vy - points[i].vy;

                points[i].vx = (points[i].vx + Dvx) * FRICTION;
                points[i].vy = (points[i].vy + Dvy) * FRICTION;

                points[j].vx = (points[j].vx - Dvx) * FRICTION;
                points[j].vy = (points[j].vy - Dvy) * FRICTION;
            }
        }
    }

    for (int i = 0; i < DOTS; i++)
    {
        draw_point(points[i].px, points[i].py, 1);
    }
}