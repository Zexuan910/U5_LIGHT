#include "DEV_Config.h"

void DEV_SPI_WRite(UBYTE _dat)
{
    if (HAL_SPI_Transmit(&hspi1, &_dat, 1, 500) != HAL_OK)
    {
        Error_Handler();
    }
}

void DEV_SPI_WriteBuffer(const UBYTE *Data, UDOUBLE Len)
{
    while (Len > 0)
    {
        uint16_t Chunk = (Len > 65535U) ? 65535U : (uint16_t)Len;

        if (HAL_SPI_Transmit(&hspi1, (uint8_t *)Data, Chunk, HAL_MAX_DELAY) != HAL_OK)
        {
            Error_Handler();
        }

        Data += Chunk;
        Len -= Chunk;
    }
}

void DEV_SetBacklight(UWORD Value)
{
    if (Value == 0)
    {
        DEV_Digital_Write(DEV_BL_PIN, 0);
    }
    else
    {
        DEV_Digital_Write(DEV_BL_PIN, 1);
    }
}

int DEV_Module_Init(void)
{
    DEV_Digital_Write(DEV_CS_PIN, 1);
    DEV_Digital_Write(DEV_DC_PIN, 1);
    DEV_Digital_Write(DEV_RST_PIN, 1);

    DEV_SetBacklight(1000);

    return 0;
}

void DEV_Module_Exit(void)
{
    DEV_SetBacklight(0);

    DEV_Digital_Write(DEV_DC_PIN, 0);
    DEV_Digital_Write(DEV_CS_PIN, 0);
    DEV_Digital_Write(DEV_RST_PIN, 0);
}
