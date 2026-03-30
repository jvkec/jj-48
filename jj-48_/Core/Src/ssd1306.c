/**
 * original author:  Tilen Majerle<tilen@majerle.eu>
 * modification for STM32f10x: Alexander Lutsai<s.lyra@ya.ru>

   ----------------------------------------------------------------------
   	Copyright (C) Alexander Lutsai, 2016
    Copyright (C) Tilen Majerle, 2015

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
   ----------------------------------------------------------------------
 */
#include "ssd1306.h"
#include "main.h"

extern I2C_HandleTypeDef hi2c2;

/* SSD1306 data buffer. This is the buffer you must write to for setting pixel values. */
static uint8_t SSD1306_Buffer[SSD1306_WIDTH * SSD1306_HEIGHT / 8];

/* The set of registers to write to when initializing the SSD1306 screen. */
static uint8_t SSD1306_Init_Config [] = {0xAE, 0x20, 0x10, 0xB0, 0xC8, 0x00, 0x10, 0x40, 0x81, 0xFF, 0xA1, 0xA6, 0xA8, 0x3F, 0xA4, 0xD3, 0x00, 0xD5, 0xF0, 0xD9, 0x22, 0xDA, 0x12, 0xDB, 0x20, 0x8D, 0x14, 0xAF, 0x2E};

/* An array you can use for the commands to be sent for scrolling. */
static uint8_t SSD1306_Scroll_Commands [8] = {0};

/* The following functions are provided for you to use, without requiring any modifications */

void SSD1306_UpdateScreen(void) {
	uint8_t m;
	uint8_t packet[3] = {0x00, 0x00, 0x10};

	for (m = 0; m < 8; m++) {
		packet[0] = (0xB0 + m);
		ssd1306_I2C_Write(SSD1306_I2C_ADDR, 0x00, packet, 3);
		ssd1306_I2C_Write(SSD1306_I2C_ADDR, 0x40, &SSD1306_Buffer[SSD1306_WIDTH * m], SSD1306_WIDTH);
	}
}

void SSD1306_Fill(SSD1306_COLOR_t color) {
	/* Use memset to efficiently set the entire SSD1306_Buffer to a single value. */
	memset(SSD1306_Buffer, (color == SSD1306_COLOR_BLACK) ? 0x00 : 0xFF, sizeof(SSD1306_Buffer));
}

void SSD1306_Clear (void)
{
	SSD1306_Fill (0);
    SSD1306_UpdateScreen();
}

/* Start of the functions you must complete for this lab. */
void ssd1306_I2C_Write(uint8_t address, uint8_t reg, uint8_t* data, uint16_t count) {
	//Write the address of the device you want to communicate with
	//Specify which register you want to write to, followed by the data bytes

	uint8_t buf[count + 1];

    buf[0] = reg; // 0x00 or 0x40
    memcpy(&buf[1], data, count);

	if (HAL_I2C_Master_Transmit(&hi2c2, address, buf, count + 1, 10) != HAL_OK) {
		//print_msg(message);
		HAL_GPIO_WritePin(LD3_GPIO_Port, LD3_Pin, GPIO_PIN_SET);
		while(1);
	}

	//green light
	HAL_GPIO_WritePin(LD1_GPIO_Port, LD1_Pin, GPIO_PIN_SET);

}

HAL_StatusTypeDef SSD1306_Init(void) {

	/* Check if the OLED is connected to I2C */
	if (HAL_I2C_IsDeviceReady(&hi2c2, SSD1306_I2C_ADDR, 1, 20000) != HAL_OK) {
		return HAL_ERROR;
	}

    /* Keep this delay to prevent overflowing the I2C controller */
	HAL_Delay(10);

	/* Init LCD */
	ssd1306_I2C_Write(SSD1306_I2C_ADDR, 0x00, SSD1306_Init_Config, sizeof(SSD1306_Init_Config));

	/* Clear screen */
	SSD1306_Fill(SSD1306_COLOR_BLACK);
	//SSD1306_Fill(SSD1306_COLOR_WHITE);

	/* Update screen */
	SSD1306_UpdateScreen();

	/* Return OK */
	return HAL_OK;
}



HAL_StatusTypeDef SSD1306_SetPixel(uint16_t x, uint16_t y, SSD1306_COLOR_t color) {
	if (x >= SSD1306_WIDTH || y >= SSD1306_HEIGHT) {
		return HAL_ERROR;
	}

    /* Set the pixel at position (x,y) to 'color'. */

	//update  the SSD1306_Buffer
	uint16_t index = ((uint16_t)floor(y / 8.0) * 128) + x;
	uint8_t bits = y % 8;

	if(color == SSD1306_COLOR_WHITE) {
		SSD1306_Buffer[index] |= 0x01 << bits;
	} else { //black
		SSD1306_Buffer[index] &= ~(0x01 << bits);
	}

	//update screen
	//SSD1306_UpdateScreen();

	return HAL_OK;
}



void SSD1306_Scroll(SSD1306_SCROLL_DIR_t direction, uint8_t start_row, uint8_t end_row)
{
    /* TO DO */
	uint8_t scroll_arr[8];

	//scroll direction
	scroll_arr[0] = direction == SSD1306_SCROLL_LEFT ? SSD1306_LEFT_HORIZONTAL_SCROLL : SSD1306_RIGHT_HORIZONTAL_SCROLL;

	//dummy byte
	scroll_arr[1] = 0;

	//first page to scroll
	scroll_arr[2] = start_row;

	//scrolling frequency
	scroll_arr[3] = 0;

	//last page to scroll
	scroll_arr[4] = end_row;

	//dummy byte
	scroll_arr[5] = 0;
	scroll_arr[6] = 0xFF;

	//activate scrolling
	scroll_arr[7] = SSD1306_ACTIVATE_SCROLL;

	ssd1306_I2C_Write(SSD1306_I2C_ADDR, 0x00, scroll_arr, sizeof(scroll_arr));
}

void SSD1306_Stopscroll(void)
{
	/* TO DO */
	uint8_t scroll_arr[8];

	//scroll direction
	scroll_arr[0] = 0;

	//dummy byte
	scroll_arr[1] = 0;

	//first page to scroll
	scroll_arr[2] = 0;

	//scrolling frequency
	scroll_arr[3] = 0;

	//last page to scroll
	scroll_arr[4] = 0;

	//dummy byte
	scroll_arr[5] = 0;
	scroll_arr[6] = 0xFF;

	//activate scrolling
	scroll_arr[7] = SSD1306_DEACTIVATE_SCROLL;

	ssd1306_I2C_Write(SSD1306_I2C_ADDR, 0x00, scroll_arr, sizeof(scroll_arr));
}


void SSD1306_Putc(uint16_t x, uint16_t y, char ch, FontDef_t* Font) {
	uint16_t *data_ptr = Font->data;
	uint8_t rows = Font->FontHeight;
	uint8_t cols = Font->FontWidth;

	uint16_t data_start_index = (ch - 32) * rows;

	for(int i = data_start_index; i < data_start_index + rows; i++) {
		uint16_t data_row_shifted = data_ptr[i] >> 5; //remove right 5 bits
		for(int j = 0; j < cols; j++) {
			if((data_row_shifted << j) & 0x0400) { //mask with bit 10 and LSH
				SSD1306_SetPixel(x + j, y + i - data_start_index, SSD1306_COLOR_WHITE);
				/*if(SSD1306_SetPixel(x + j, y + i - data_start_index, SSD1306_COLOR_WHITE) != HAL_OK) {
					HAL_GPIO_WritePin(LD3_GPIO_Port, LD3_Pin, GPIO_PIN_SET);
					while(1);
				}*/
			}
		}
	}

}

HAL_StatusTypeDef SSD1306_Puts(char* str, FontDef_t* Font) {

	/* Loop over every character until we see \0. */
	uint16_t x_offset = DEFAULT_X_OFFSET;
	uint16_t y_offset = DEFAULT_Y_OFFSET;
	while (*str != '\0') {

        /* TODO */
		if(*str == '\n') {
			y_offset += Font->FontWidth;
			x_offset = DEFAULT_X_OFFSET;
			str++;
		}
		SSD1306_Putc(0+x_offset, 0 + y_offset, *str, Font);
		x_offset += Font->FontHeight + EXTRA_X_OFFSET;
        /* Increase string pointer */
        str++;
	}

	return HAL_OK;
}

HAL_StatusTypeDef SSD1306_Put_8x4Grid(uint8_t grid[GRID_ROWS][GRID_COLS], FontDef_t* Font) {

	// top grid
	SSD1306_Puts("12345678", Font);

	uint16_t x_offset = DEFAULT_X_OFFSET;
	uint16_t y_offset = DEFAULT_Y_OFFSET;

	char ch = '_';
	for(uint8_t i = 0; i < GRID_ROWS; i++) {
		for(uint8_t j = 0; j < GRID_COLS; j++) {
			if(grid[i][j] == NOTE_ON) {
				ch = '_';
			} else if(grid[i][j] == NOTE_OFF) {
				ch = ' ';
			} else if(grid[i][j] == NOTE_SELECT) {
				ch = '!';
			} else if(grid[i][j] == NOTE_PLAY) {
				ch = '.';
			} else {
				ch = '?'; // undefined state
			}

			SSD1306_Putc(0+x_offset, 0 + y_offset, ch, Font);
			x_offset += Font->FontHeight + EXTRA_X_OFFSET;
		}
		y_offset += Font->FontWidth + EXTRA_Y_OFFSET;
		x_offset = DEFAULT_X_OFFSET;
	}

	return HAL_OK;
}
