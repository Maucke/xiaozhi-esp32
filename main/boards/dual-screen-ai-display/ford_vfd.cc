#include "ford_vfd.h"
#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <freertos/task.h>
#include <string.h>
#include <esp_log.h>
#include "settings.h"

#define TAG "FORD_VFD"

FORD_VFD::FORD_VFD(gpio_num_t din, gpio_num_t clk, gpio_num_t cs, spi_host_device_t spi_num) : _cs(cs)
{
	// Initialize the SPI bus configuration structure
	spi_bus_config_t buscfg = {0};

	// Set the clock and data pins for the SPI bus
	buscfg.sclk_io_num = clk;
	buscfg.data0_io_num = din;

	// Set the maximum transfer size in bytes
	buscfg.max_transfer_sz = 1024;

	// Initialize the SPI bus with the specified configuration
	ESP_ERROR_CHECK(spi_bus_initialize(spi_num, &buscfg, SPI_DMA_CH_AUTO));

	// Initialize the SPI device interface configuration structure
	spi_device_interface_config_t devcfg = {
		.mode = 0,				  // Set the SPI mode to 0
		.clock_speed_hz = 400000, // Set the clock speed to 400kHz
		.spics_io_num = cs,		  // Set the chip select pin
		.flags = 0,
		.queue_size = 7,
	};

	ESP_ERROR_CHECK(spi_bus_add_device(spi_num, &devcfg, &spi_device_));
	init();
	init_task();
	ESP_LOGI(TAG, "FORD_VFD Initalized");
}

FORD_VFD::FORD_VFD(spi_device_handle_t spi_device) : spi_device_(spi_device)
{
	init();
	init_task();
	ESP_LOGI(TAG, "FORD_VFD Initalized");
}

void FORD_VFD::write_data8(uint8_t dat)
{
	write_data8(&dat, 1);
}

void FORD_VFD::write_data8(uint8_t *dat, int len)
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

	return;
}

void FORD_VFD::setbrightness(uint8_t brightness)
{
	dimming = brightness * 127 / 100;
}

void FORD_VFD::refrash(uint8_t *gram, int size)
{
	if (dimming < 5)
		dimming = 5;
	uint8_t data[2] = {0xcf, (uint8_t)(dimming & 0xFC)};

	write_data8((uint8_t *)data, 2);
	write_data8((uint8_t *)gram, size);
}

void FORD_VFD::time_blink()
{
	static bool time_mark = true;
	time_mark = !time_mark;
	// symbolhelper(COLON1, time_mark);
	symbolhelper(COLON2, time_mark);
}

void FORD_VFD::number_show(int start, char *buf, int size, NumAni ani)
{
	for (size_t i = 0; i < size && (start + i) < NUM_COUNT; i++)
	{
		currentData[start + i].animation_type = ani;
		currentData[start + i].current_content = buf[i];
	}
}

uint8_t FORD_VFD::contentgetpart(uint8_t raw, uint8_t before_raw, uint8_t mask)
{
	return (raw & mask) | (before_raw & (~mask));
}

void FORD_VFD::contentanimate()
{
	static int64_t start_time = esp_timer_get_time() / 1000;
	int64_t current_time = esp_timer_get_time() / 1000;
	int64_t elapsed_time = current_time - start_time;

	if (elapsed_time >= 30)
		start_time = current_time;
	else
		return;

	for (int i = 0; i < NUM_COUNT; i++)
	{
		if (currentData[i].current_content != currentData[i].last_content)
		{
			uint8_t before_raw_code = find_hex_code(currentData[i].last_content);
			uint8_t raw_code = find_hex_code(currentData[i].current_content);
			uint8_t code = raw_code;
			if (currentData[i].animation_type == CLOCKWISE)
			{
				switch (currentData[i].animation_step)
				{
				case 0:
					code = contentgetpart(raw_code, before_raw_code, 1);
					break;
				case 1:
					code = contentgetpart(raw_code, before_raw_code, 3);
					break;
				case 2:
					code = contentgetpart(raw_code, before_raw_code, 0x43);
					break;
				case 3:
					code = contentgetpart(raw_code, before_raw_code, 0x47);
					break;
				case 4:
					code = contentgetpart(raw_code, before_raw_code, 0x4f);
					break;
				case 5:
					code = contentgetpart(raw_code, before_raw_code, 0x5f);
					break;
				default:
					currentData[i].animation_step = -1;
					break;
				}
			}
			else if (currentData[i].animation_type == ANTICLOCKWISE)
			{
				switch (currentData[i].animation_step)
				{
				case 0:
					code = contentgetpart(raw_code, before_raw_code, 1);
					break;
				case 1:
					code = contentgetpart(raw_code, before_raw_code, 0x21);
					break;
				case 2:
					code = contentgetpart(raw_code, before_raw_code, 0x61);
					break;
				case 3:
					code = contentgetpart(raw_code, before_raw_code, 0x71);
					break;
				case 4:
					code = contentgetpart(raw_code, before_raw_code, 0x79);
					break;
				case 5:
					code = contentgetpart(raw_code, before_raw_code, 0x7d);
					break;
				default:
					currentData[i].animation_step = -1;
					break;
				}
			}
			else if (currentData[i].animation_type == UP2DOWN)
			{
				switch (currentData[i].animation_step)
				{
				case 0:
					code = contentgetpart(raw_code, before_raw_code, 0x1);
					break;
				case 1:
					code = contentgetpart(raw_code, before_raw_code, 0x21);
					break;
				case 2:
					code = contentgetpart(raw_code, before_raw_code, 0x73);
					break;
				default:
					currentData[i].animation_step = -1;
					break;
				}
			}
			else if (currentData[i].animation_type == DOWN2UP)
			{
				switch (currentData[i].animation_step)
				{
				case 0:
					code = contentgetpart(raw_code, before_raw_code, 0x8);
					break;
				case 1:
					code = contentgetpart(raw_code, before_raw_code, 0xc);
					break;
				case 2:
					code = contentgetpart(raw_code, before_raw_code, 0x5e);
					break;
				default:
					currentData[i].animation_step = -1;
					break;
				}
			}
			else if (currentData[i].animation_type == LEFT2RT)
			{
				switch (currentData[i].animation_step)
				{
				case 0:
					code = contentgetpart(raw_code, before_raw_code, 0x10);
					break;
				case 1:
					code = contentgetpart(raw_code, before_raw_code, 0x18);
					break;
				case 2:
					code = contentgetpart(raw_code, before_raw_code, 0x7c);
					break;
				default:
					currentData[i].animation_step = -1;
					break;
				}
			}
			else if (currentData[i].animation_type == RT2LEFT)
			{
				switch (currentData[i].animation_step)
				{
				case 0:
					code = contentgetpart(raw_code, before_raw_code, 0x4);
					break;
				case 1:
					code = contentgetpart(raw_code, before_raw_code, 0xc);
					break;
				case 2:
					code = contentgetpart(raw_code, before_raw_code, 0x5e);
					break;
				default:
					currentData[i].animation_step = -1;
					break;
				}
			}
			else
				currentData[i].animation_step = -1;

			if (currentData[i].animation_step == -1)
				currentData[i].last_content = currentData[i].current_content;

			charhelper(i, code);
			currentData[i].animation_step++;
		}
	}
}

void FORD_VFD::init()
{
	write_data8(0x55);
	write_data8((uint8_t *)init_data_block1, sizeof(init_data_block1));
	write_data8((uint8_t *)init_data_block2, sizeof(init_data_block2));
	write_data8((uint8_t *)init_data_block3, sizeof(init_data_block3));
	write_data8((uint8_t *)init_data_block4, sizeof(init_data_block4));
	write_data8((uint8_t *)init_data_block5, sizeof(init_data_block5));
	write_data8((uint8_t *)gram, sizeof(gram));
	write_data8((uint8_t *)init_data_block7, sizeof(init_data_block7));
	write_data8((uint8_t *)init_data_block8, sizeof(init_data_block8));
}

void FORD_VFD::init_task()
{
	_spectrum = new SpectrumDisplay(FORD_WIDTH, FORD_HEIGHT);
	_spectrum->setDrawPointCallback([this](int x, int y, uint8_t dot)
									{ this->draw_point(x, y, dot, FFT); });
	xTaskCreate(
		[](void *arg)
		{
			FORD_VFD *vfd = static_cast<FORD_VFD *>(arg);
			vfd->symbolhelper(BT, true);
			while (true)
			{
				if (vfd->_mode == FFT)
					vfd->clear();
				vfd->_spectrum->spectrumProcess();
				vfd->refrash(vfd->gram, sizeof vfd->gram);
				vTaskDelay(pdMS_TO_TICKS(10));
				vfd->contentanimate();
			}
			vTaskDelete(NULL);
		},
		"vfd",
		4096 - 1024,
		this,
		6,
		nullptr);
}
// 合并 get_oddgroup 和 get_evengroup 函数
uint8_t FORD_VFD::get_group(int x, uint8_t dot, uint8_t group, bool isOdd)
{
	static const uint8_t oddMasks[] = {2, 4, 1};
	static const uint8_t evenMasks[] = {2, 1, 4};
	const uint8_t *masks = isOdd ? oddMasks : evenMasks;
	int remainder = x % 3;
	if (dot)
	{
		group |= masks[remainder];
	}
	else
	{
		group &= ~masks[remainder];
	}
	return group & 0x7;
}

void FORD_VFD::draw_point(int x, int y, uint8_t dot, Mode mode)
{
	if (mode != _mode)
		return;
	if (x >= FORD_WIDTH || y >= FORD_HEIGHT || x < 0 || y < 0)
		return;
	uint16_t index;
	uint8_t temp;
	y = FORD_HEIGHT - 1 - y;
	index = 2 + 16 * (x / 3) + y / 4 * 3; //+y%16
	if (index > 480)
		index += 32;
	else if (index > 256)
		index += 16;

	bool isOdd = (16 * (x / 3) / 16 % 2) == 1;
	int remainder = y % 4;
	switch (remainder)
	{
	case 0:
		if (isOdd)
		{
			temp = (get_group(x, dot, (gram[index] >> 5) & 0x7, true) << 5);
			gram[index] &= ~0xe0;
			gram[index] |= temp;
		}
		else
		{
			temp = (get_group(x, dot, (gram[index] >> 2) & 0x7, false) << 2);
			gram[index] &= ~0x1c;
			gram[index] |= temp;
		}
		break;
	case 1:
		if (isOdd)
		{
			temp = (get_group(x, dot, (gram[index] << 1) & 0x7, true) >> 1);
			gram[index] &= ~0x3;
			gram[index] |= temp;

			temp = (get_group(x, dot, (gram[index + 1] >> 7) & 0x7, true) << 7);
			gram[index + 1] &= ~0x80;
			gram[index + 1] |= temp;
		}
		else
		{
			temp = (get_group(x, dot, (gram[index + 1] >> 4) & 0x7, false) << 4);
			gram[index + 1] &= ~0x70;
			gram[index + 1] |= temp;
		}
		break;
	case 2:
		if (isOdd)
		{
			temp = (get_group(x, dot, (gram[index + 1] >> 1) & 0x7, true) << 1);
			gram[index + 1] &= ~0xe;
			gram[index + 1] |= temp;
		}
		else
		{
			temp = (get_group(x, dot, (gram[index + 1] << 2) & 0x7, false) >> 2);
			gram[index + 1] &= ~0x1;
			gram[index + 1] |= temp;
			temp = (get_group(x, dot, (gram[index + 2] >> 6) & 0x7, false) << 6);
			gram[index + 2] &= ~0xc0;
			gram[index + 2] |= temp;
		}
		break;
	case 3:
		if (isOdd)
		{
			temp = (get_group(x, dot, (gram[index + 2] >> 3) & 0x7, true) << 3);
			gram[index + 2] &= ~0x38;
			gram[index + 2] |= temp;
		}
		else
		{
			temp = (get_group(x, dot, (gram[index + 2]) & 0x7, false));
			gram[index + 2] &= ~0x7;
			gram[index + 2] |= temp;
		}
		break;
	}
}
void FORD_VFD::clear()
{
#if 1
	for (int y = 0; y < FORD_HEIGHT; ++y)
	{
		for (int x = 0; x < FORD_WIDTH; ++x)
		{
			draw_point(x, y, 0, _mode);
		}
	}
#else
	memset(&gram[2], 0, 270 - 2);
	memset(&gram[275], 0, 510 - 275);
	memset(&gram[515], 0, 814 - 515);
#endif
}
void FORD_VFD::find_enum_code(Symbols flag, int *byteIndex, int *bitMask)
{
	*byteIndex = symbolPositions[flag].byteIndex;
	*bitMask = symbolPositions[flag].bitMask;
}

void FORD_VFD::symbolhelper(Symbols symbol, bool is_on)
{
	if (symbol >= SYMBOL_MAX)
		return;

	int byteIndex, bitMask;
	find_enum_code(symbol, &byteIndex, &bitMask);

	if (is_on)
		gram[byteIndex] |= bitMask;
	else
		gram[byteIndex] &= ~bitMask;
}

uint8_t process_bit(uint8_t real, uint8_t realbitdelta, uint8_t phy, uint8_t phybitdelta)
{
	if (phy & (1 << phybitdelta))
		return real | (1 << realbitdelta);
	else
		return real & (~(1 << realbitdelta));
}

uint8_t FORD_VFD::find_hex_code(char ch)
{
	if (ch >= ' ' && ch <= 'Z')
		return hex_codes[ch - ' '];
	else if (ch >= 'a' && ch <= 'z')
		return hex_codes[ch - 'a' + 'A' - ' '];
	return 0;
}

void FORD_VFD::charhelper(int index, char ch)
{
	if (index >= 9)
		return;
	uint8_t val = find_hex_code(ch);
	charhelper(index, val);
}

void FORD_VFD::charhelper(int index, uint8_t code)
{
	if (index >= 9)
		return;
	switch (index)
	{
	case 0:
		gram[270 + 3] = process_bit(gram[270 + 3], 0, code, 2);
		break;
	case 1:
		gram[270 + 0] = process_bit(gram[270 + 0], 2, code, 3);
		gram[270 + 1] = process_bit(gram[270 + 1], 6, code, 4);
		gram[270 + 1] = process_bit(gram[270 + 1], 2, code, 2);
		gram[270 + 2] = process_bit(gram[270 + 2], 1, code, 5);
		gram[270 + 3] = process_bit(gram[270 + 3], 5, code, 1);
		gram[270 + 2] = process_bit(gram[270 + 2], 6, code, 6);
		gram[270 + 3] = process_bit(gram[270 + 3], 1, code, 0);
		break;
	case 2:
		gram[270 + 0] = process_bit(gram[270 + 0], 3, code, 3);
		gram[270 + 1] = process_bit(gram[270 + 1], 7, code, 4);
		gram[270 + 1] = process_bit(gram[270 + 1], 3, code, 2);
		gram[270 + 2] = process_bit(gram[270 + 2], 2, code, 5);
		gram[270 + 3] = process_bit(gram[270 + 3], 6, code, 1);
		gram[270 + 2] = process_bit(gram[270 + 2], 7, code, 6);
		gram[270 + 3] = process_bit(gram[270 + 3], 2, code, 0);
		break;
	case 3:
		gram[270 + 0] = process_bit(gram[270 + 0], 4, code, 3);
		gram[270 + 0] = process_bit(gram[270 + 0], 0, code, 4);
		gram[270 + 1] = process_bit(gram[270 + 1], 4, code, 2);
		gram[270 + 2] = process_bit(gram[270 + 2], 3, code, 5);
		gram[270 + 3] = process_bit(gram[270 + 3], 7, code, 1);
		gram[270 + 1] = process_bit(gram[270 + 1], 0, code, 6);
		gram[270 + 3] = process_bit(gram[270 + 3], 3, code, 0);
		break;
	case 4:
		gram[270 + 0] = process_bit(gram[270 + 0], 5, code, 3);
		gram[270 + 0] = process_bit(gram[270 + 0], 1, code, 4);
		gram[270 + 1] = process_bit(gram[270 + 1], 5, code, 2);
		gram[270 + 2] = process_bit(gram[270 + 2], 4, code, 5);
		gram[270 + 1] = process_bit(gram[270 + 1], 1, code, 6);
		gram[270 + 2] = process_bit(gram[270 + 2], 0, code, 1);
		gram[270 + 3] = process_bit(gram[270 + 3], 4, code, 0);
		break;
	case 5:
		gram[814 + 0] = process_bit(gram[814 + 0], 2, code, 3);
		gram[814 + 1] = process_bit(gram[814 + 1], 6, code, 4);
		gram[814 + 1] = process_bit(gram[814 + 1], 2, code, 2);
		gram[814 + 2] = process_bit(gram[814 + 2], 1, code, 5);
		gram[814 + 3] = process_bit(gram[814 + 3], 5, code, 1);
		gram[814 + 2] = process_bit(gram[814 + 2], 6, code, 6);
		gram[814 + 3] = process_bit(gram[814 + 3], 1, code, 0);
		break;
	case 6:
		gram[814 + 0] = process_bit(gram[814 + 0], 3, code, 3);
		gram[814 + 1] = process_bit(gram[814 + 1], 7, code, 4);
		gram[814 + 1] = process_bit(gram[814 + 1], 3, code, 2);
		gram[814 + 2] = process_bit(gram[814 + 2], 2, code, 5);
		gram[814 + 3] = process_bit(gram[814 + 3], 6, code, 1);
		gram[814 + 2] = process_bit(gram[814 + 2], 7, code, 6);
		gram[814 + 3] = process_bit(gram[814 + 3], 2, code, 0);
		break;
	case 7:
		gram[814 + 0] = process_bit(gram[814 + 0], 4, code, 3);
		gram[814 + 0] = process_bit(gram[814 + 0], 0, code, 4);
		gram[814 + 1] = process_bit(gram[814 + 1], 4, code, 2);
		gram[814 + 2] = process_bit(gram[814 + 2], 3, code, 5);
		gram[814 + 3] = process_bit(gram[814 + 3], 7, code, 1);
		gram[814 + 1] = process_bit(gram[814 + 1], 0, code, 6);
		gram[814 + 3] = process_bit(gram[814 + 3], 3, code, 0);
		break;
	case 8:
		gram[814 + 0] = process_bit(gram[814 + 0], 5, code, 3);
		gram[814 + 0] = process_bit(gram[814 + 0], 1, code, 4);
		gram[814 + 1] = process_bit(gram[814 + 1], 5, code, 2);
		gram[814 + 2] = process_bit(gram[814 + 2], 4, code, 5);
		gram[814 + 1] = process_bit(gram[814 + 1], 1, code, 6);
		gram[814 + 2] = process_bit(gram[814 + 2], 0, code, 1);
		gram[814 + 3] = process_bit(gram[814 + 3], 4, code, 0);
		break;
	default:
		break;
	}
}

void FORD_VFD::setsleep(bool en)
{
	if (en)
	{
		memset(gram, 0, sizeof gram);
		// pt6324_refrash(gram);
	}
}

void FORD_VFD::test()
{
	xTaskCreate(
		[](void *arg)
		{
			int count = 0;
			FORD_VFD *vfd = static_cast<FORD_VFD *>(arg);
			int64_t start_time = esp_timer_get_time() / 1000;
			ESP_LOGI(TAG, "FORD_VFD Test");
			while (1)
			{
				int64_t current_time = esp_timer_get_time() / 1000;
				int64_t elapsed_time = current_time - start_time;

				if (elapsed_time >= 500)
				{
					count++;
					start_time = current_time;
				}
				vfd->symbolhelper(BT, true);
				for (size_t i = 0; i < 9; i++)
				{
					vfd->charhelper(i, (char)('0' + count % 10));
				}
				for (size_t i = 0; i < FORD_WIDTH; i++)
				{
					for (size_t j = 0; j < FORD_HEIGHT; j++)
					{
						vfd->draw_point(i, FORD_HEIGHT - 1 - j, (j + i) % 2);
					}
				}
				vfd->refrash(vfd->gram, sizeof vfd->gram);

				vTaskDelay(pdMS_TO_TICKS(100));
			}
			vTaskDelete(NULL);
		},
		"vfd_test",
		4096 - 1024,
		this,
		5,
		nullptr);
}
