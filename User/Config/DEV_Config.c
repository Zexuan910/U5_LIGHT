/*****************************************************************************
* | File        :   DEV_Config.c
* | Author      :   Waveshare team
* | Function    :   Hardware underlying interface
*
******************************************************************************/
#include "DEV_Config.h"

void DEV_SPI_WRite(UBYTE _dat)
{
    HAL_SPI_Transmit(&hspi1, (uint8_t *)&_dat, 1, 500);
}

void DEV_SetBacklight(UWORD Value)
{
    DEV_Digital_Write(DEV_BL_PIN, Value == 0U ? 0 : 1);
}

int DEV_Module_Init(void)
{
    DEV_Digital_Write(DEV_DC_PIN, 1);
    DEV_Digital_Write(DEV_CS_PIN, 1);
    DEV_Digital_Write(DEV_RST_PIN, 1);
    DEV_SetBacklight(DEV_BL_PWM_MAX);

    return 0;
}

void DEV_Module_Exit(void)
{
    DEV_SetBacklight(0);
    DEV_Digital_Write(DEV_DC_PIN, 0);
    DEV_Digital_Write(DEV_CS_PIN, 0);
    DEV_Digital_Write(DEV_RST_PIN, 0);
}
