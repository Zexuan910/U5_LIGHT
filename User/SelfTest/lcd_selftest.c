#include "lcd_selftest.h"

#include "DEV_Config.h"
#include "LCD_1in69.h"

#define RGB565_WHITE 0xFFFFU

void LCD_SelfTest_Run(void)
{
    if (DEV_Module_Init() != 0) {
        Error_Handler();
    }

    LCD_1IN69_SetBackLight(DEV_BL_PWM_MAX);
    LCD_1IN69_Init(VERTICAL);

    LCD_1IN69_Clear(RGB565_WHITE);
    DEV_Delay_ms(100);
    LCD_1IN69_Clear(RGB565_WHITE);
}
