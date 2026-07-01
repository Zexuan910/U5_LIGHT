/*****************************************************************************
* | File        :   DEV_Config.h
* | Author      :   Waveshare team
* | Function    :   Hardware underlying interface
*
******************************************************************************/
#ifndef _DEV_CONFIG_H_
#define _DEV_CONFIG_H_

#include "stm32u5xx_hal.h"
#include "main.h"
#include "Debug.h"
#include <stdint.h>
#include <stdio.h>

#define UBYTE   uint8_t
#define UWORD   uint16_t
#define UDOUBLE uint32_t

extern SPI_HandleTypeDef hspi1;

#define DEV_RST_PIN     LCD_RST_GPIO_Port, LCD_RST_Pin
#define DEV_DC_PIN      LCD_DC_GPIO_Port, LCD_DC_Pin
#define DEV_CS_PIN      LCD_CS_GPIO_Port, LCD_CS_Pin
#define DEV_BL_PIN      LCD_BLK_GPIO_Port, LCD_BLK_Pin
#define DEV_BL_PWM_MAX  1000U

#define DEV_Digital_Write(_pin, _value) HAL_GPIO_WritePin(_pin, (_value) == 0 ? GPIO_PIN_RESET : GPIO_PIN_SET)
#define DEV_Digital_Read(_pin) HAL_GPIO_ReadPin(_pin)

#define DEV_SPI_WRITE(_dat) DEV_SPI_WRite(_dat)
#define DEV_Delay_ms(__xms) HAL_Delay(__xms)
#define DEV_Set_PWM(_Value) DEV_SetBacklight(_Value)

void DEV_SPI_WRite(UBYTE _dat);
void DEV_SetBacklight(UWORD Value);
int DEV_Module_Init(void);
void DEV_Module_Exit(void);

#endif
