#include "lcd_selftest.h"

#include "DEV_Config.h"
#include "LCD_1in69.h"

void LCD_SelfTest_Run(uint16_t color)
{
    if (DEV_Module_Init() != 0) {
        Error_Handler();
    }

    LCD_1IN69_SetBackLight(1000U);
    LCD_1IN69_Init(VERTICAL);
    LCD_1IN69_Clear(color);
}
