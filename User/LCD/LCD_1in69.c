/*****************************************************************************
* | File        :   LCD_1in69.c
* | Author      :   Waveshare team
* | Function    :   Hardware underlying interface
*
******************************************************************************/
#include "LCD_1in69.h"
#include "DEV_Config.h"

#include <stdlib.h>
#include <stdio.h>

LCD_1IN69_ATTRIBUTES LCD_1IN69;

static void LCD_1IN69_Reset(void)
{
    LCD_1IN69_RST_1;
    DEV_Delay_ms(100);
    LCD_1IN69_RST_0;
    DEV_Delay_ms(100);
    LCD_1IN69_RST_1;
    DEV_Delay_ms(100);
}

static void LCD_1IN69_SendCommand(UBYTE Reg)
{
    LCD_1IN69_DC_0;
    LCD_1IN69_CS_0;
    DEV_SPI_WRITE(Reg);
}

static void LCD_1IN69_SendData_8Bit(UBYTE Data)
{
    LCD_1IN69_DC_1;
    DEV_SPI_WRITE(Data);
}

static void LCD_1IN69_SendData_16Bit(UWORD Data)
{
    LCD_1IN69_DC_1;
    LCD_1IN69_CS_0;
    DEV_SPI_WRITE((Data >> 8) & 0xFF);
    DEV_SPI_WRITE(Data & 0xFF);
    LCD_1IN69_CS_1;
}

static void LCD_1IN69_InitReg(void)
{
    LCD_1IN69_SendCommand(0x36);
    LCD_1IN69_SendData_8Bit(0x00);

    LCD_1IN69_SendCommand(0x3A);
    LCD_1IN69_SendData_8Bit(0x05);

    LCD_1IN69_SendCommand(0xB2);
    LCD_1IN69_SendData_8Bit(0x0B);
    LCD_1IN69_SendData_8Bit(0x0B);
    LCD_1IN69_SendData_8Bit(0x00);
    LCD_1IN69_SendData_8Bit(0x33);
    LCD_1IN69_SendData_8Bit(0x35);

    LCD_1IN69_SendCommand(0xB7);
    LCD_1IN69_SendData_8Bit(0x11);

    LCD_1IN69_SendCommand(0xBB);
    LCD_1IN69_SendData_8Bit(0x35);

    LCD_1IN69_SendCommand(0xC0);
    LCD_1IN69_SendData_8Bit(0x2C);

    LCD_1IN69_SendCommand(0xC2);
    LCD_1IN69_SendData_8Bit(0x01);

    LCD_1IN69_SendCommand(0xC3);
    LCD_1IN69_SendData_8Bit(0x0D);

    LCD_1IN69_SendCommand(0xC4);
    LCD_1IN69_SendData_8Bit(0x20);

    LCD_1IN69_SendCommand(0xC6);
    LCD_1IN69_SendData_8Bit(0x13);

    LCD_1IN69_SendCommand(0xD0);
    LCD_1IN69_SendData_8Bit(0xA4);
    LCD_1IN69_SendData_8Bit(0xA1);

    LCD_1IN69_SendCommand(0xD6);
    LCD_1IN69_SendData_8Bit(0xA1);

    LCD_1IN69_SendCommand(0xE0);
    LCD_1IN69_SendData_8Bit(0xF0);
    LCD_1IN69_SendData_8Bit(0x06);
    LCD_1IN69_SendData_8Bit(0x0B);
    LCD_1IN69_SendData_8Bit(0x0A);
    LCD_1IN69_SendData_8Bit(0x09);
    LCD_1IN69_SendData_8Bit(0x26);
    LCD_1IN69_SendData_8Bit(0x29);
    LCD_1IN69_SendData_8Bit(0x33);
    LCD_1IN69_SendData_8Bit(0x41);
    LCD_1IN69_SendData_8Bit(0x18);
    LCD_1IN69_SendData_8Bit(0x16);
    LCD_1IN69_SendData_8Bit(0x15);
    LCD_1IN69_SendData_8Bit(0x29);
    LCD_1IN69_SendData_8Bit(0x2D);

    LCD_1IN69_SendCommand(0xE1);
    LCD_1IN69_SendData_8Bit(0xF0);
    LCD_1IN69_SendData_8Bit(0x04);
    LCD_1IN69_SendData_8Bit(0x08);
    LCD_1IN69_SendData_8Bit(0x08);
    LCD_1IN69_SendData_8Bit(0x07);
    LCD_1IN69_SendData_8Bit(0x03);
    LCD_1IN69_SendData_8Bit(0x28);
    LCD_1IN69_SendData_8Bit(0x32);
    LCD_1IN69_SendData_8Bit(0x40);
    LCD_1IN69_SendData_8Bit(0x3B);
    LCD_1IN69_SendData_8Bit(0x19);
    LCD_1IN69_SendData_8Bit(0x18);
    LCD_1IN69_SendData_8Bit(0x2A);
    LCD_1IN69_SendData_8Bit(0x2E);

    LCD_1IN69_SendCommand(0xE4);
    LCD_1IN69_SendData_8Bit(0x25);
    LCD_1IN69_SendData_8Bit(0x00);
    LCD_1IN69_SendData_8Bit(0x00);

    LCD_1IN69_SendCommand(0x21);

    LCD_1IN69_SendCommand(0x11);
    DEV_Delay_ms(120);
    LCD_1IN69_SendCommand(0x29);
}

static void LCD_1IN69_SetAttributes(UBYTE Scan_dir)
{
    UBYTE MemoryAccessReg = 0x00;

    LCD_1IN69.SCAN_DIR = Scan_dir;
    if (Scan_dir == HORIZONTAL) {
        LCD_1IN69.HEIGHT = LCD_1IN69_WIDTH;
        LCD_1IN69.WIDTH = LCD_1IN69_HEIGHT;
        MemoryAccessReg = 0x70;
    } else {
        LCD_1IN69.HEIGHT = LCD_1IN69_HEIGHT;
        LCD_1IN69.WIDTH = LCD_1IN69_WIDTH;
        MemoryAccessReg = 0x00;
    }

    LCD_1IN69_SendCommand(0x36);
    LCD_1IN69_SendData_8Bit(MemoryAccessReg);
}

void LCD_1IN69_Init(UBYTE Scan_dir)
{
    LCD_1IN69_Reset();
    LCD_1IN69_SetAttributes(Scan_dir);
    LCD_1IN69_InitReg();
}

void LCD_1IN69_SetWindows(UWORD Xstart, UWORD Ystart, UWORD Xend, UWORD Yend)
{
    if (LCD_1IN69.SCAN_DIR == VERTICAL) {
        LCD_1IN69_SendCommand(0x2A);
        LCD_1IN69_SendData_8Bit(Xstart >> 8);
        LCD_1IN69_SendData_8Bit(Xstart);
        LCD_1IN69_SendData_8Bit((Xend - 1) >> 8);
        LCD_1IN69_SendData_8Bit(Xend - 1);

        LCD_1IN69_SendCommand(0x2B);
        LCD_1IN69_SendData_8Bit((Ystart + 20) >> 8);
        LCD_1IN69_SendData_8Bit(Ystart + 20);
        LCD_1IN69_SendData_8Bit((Yend + 20 - 1) >> 8);
        LCD_1IN69_SendData_8Bit(Yend + 20 - 1);
    } else {
        LCD_1IN69_SendCommand(0x2A);
        LCD_1IN69_SendData_8Bit((Xstart + 20) >> 8);
        LCD_1IN69_SendData_8Bit(Xstart + 20);
        LCD_1IN69_SendData_8Bit((Xend + 20 - 1) >> 8);
        LCD_1IN69_SendData_8Bit(Xend + 20 - 1);

        LCD_1IN69_SendCommand(0x2B);
        LCD_1IN69_SendData_8Bit(Ystart >> 8);
        LCD_1IN69_SendData_8Bit(Ystart);
        LCD_1IN69_SendData_8Bit((Yend - 1) >> 8);
        LCD_1IN69_SendData_8Bit(Yend - 1);
    }

    LCD_1IN69_SendCommand(0x2C);
}

void LCD_1IN69_Clear(UWORD Color)
{
    UWORD i;
    UWORD j;

    LCD_1IN69_SetWindows(0, 0, LCD_1IN69.WIDTH, LCD_1IN69.HEIGHT);
    DEV_Digital_Write(DEV_DC_PIN, 1);

    for (i = 0; i < LCD_1IN69_WIDTH; i++) {
        for (j = 0; j < LCD_1IN69_HEIGHT; j++) {
            DEV_SPI_WRITE((Color >> 8) & 0xff);
            DEV_SPI_WRITE(Color);
        }
    }
}

void LCD_1IN69_Display(UWORD *Image)
{
    UWORD i;
    UWORD j;

    LCD_1IN69_SetWindows(0, 0, LCD_1IN69.WIDTH, LCD_1IN69.HEIGHT);
    DEV_Digital_Write(DEV_DC_PIN, 1);

    for (i = 0; i < LCD_1IN69_WIDTH; i++) {
        for (j = 0; j < LCD_1IN69_HEIGHT; j++) {
            UWORD Pixel = *(Image + i * LCD_1IN69_HEIGHT + j);
            DEV_SPI_WRITE((Pixel >> 8) & 0xff);
            DEV_SPI_WRITE(Pixel);
        }
    }
}

void LCD_1IN69_DisplayWindows(UWORD Xstart, UWORD Ystart, UWORD Xend, UWORD Yend, UWORD *Image)
{
    UDOUBLE Addr = 0;
    UWORD i;
    UWORD j;

    LCD_1IN69_SetWindows(Xstart, Ystart, Xend, Yend);
    LCD_1IN69_DC_1;

    for (i = Ystart; i < Yend; i++) {
        Addr = Xstart + i * LCD_1IN69_WIDTH;
        for (j = Xstart; j < Xend; j++) {
            DEV_SPI_WRITE((*(Image + Addr + j) >> 8) & 0xff);
            DEV_SPI_WRITE(*(Image + Addr + j));
        }
    }
}

void LCD_1IN69_DrawPoint(UWORD X, UWORD Y, UWORD Color)
{
    LCD_1IN69_SetWindows(X, Y, X + 1, Y + 1);
    LCD_1IN69_SendData_16Bit(Color);
}

void LCD_1IN69_SetBackLight(UWORD Value)
{
    DEV_Set_PWM(Value);
}
