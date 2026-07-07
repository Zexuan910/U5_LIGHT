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

#define LCD_1IN69_FILL_BLOCK_PIXELS 256U

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
    LCD_1IN69_CS_1;
}

static void LCD_1IN69_SendData_8Bit(UBYTE Data)
{
    LCD_1IN69_DC_1;
    LCD_1IN69_CS_0;
    DEV_SPI_WRITE(Data);
    LCD_1IN69_CS_1;
}

static void LCD_1IN69_SendData_16Bit(UWORD Data)
{
    LCD_1IN69_DC_1;
    LCD_1IN69_CS_0;
    DEV_SPI_WRITE((Data >> 8) & 0xFF);
    DEV_SPI_WRITE(Data & 0xFF);
    LCD_1IN69_CS_1;
}

static UBYTE LCD_1IN69_GetMemoryAccessReg(UBYTE Scan_dir)
{
    if (Scan_dir == HORIZONTAL) {
        return LCD_1IN69_MADCTL_HORIZONTAL;
    }

    return LCD_1IN69_MADCTL_VERTICAL;
}

#if LCD_1IN69_INIT_SEQUENCE == LCD_1IN69_INIT_SEQUENCE_LEGACY
static void LCD_1IN69_InitRegLegacy(void)
{
    LCD_1IN69_SendCommand(0x36);
    LCD_1IN69_SendData_8Bit(0x00);

    LCD_1IN69_SendCommand(0x3A);
    LCD_1IN69_SendData_8Bit(LCD_1IN69_COLMOD);

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
#endif

static void LCD_1IN69_InitRegP169H002(UBYTE MemoryAccessReg)
{
    LCD_1IN69_SendCommand(0x11);
    DEV_Delay_ms(120);

    LCD_1IN69_SendCommand(0x36);
    LCD_1IN69_SendData_8Bit(MemoryAccessReg);

    LCD_1IN69_SendCommand(0x3A);
    LCD_1IN69_SendData_8Bit(LCD_1IN69_COLMOD);

    LCD_1IN69_SendCommand(0xB2);
    LCD_1IN69_SendData_8Bit(0x0C);
    LCD_1IN69_SendData_8Bit(0x0C);
    LCD_1IN69_SendData_8Bit(0x00);
    LCD_1IN69_SendData_8Bit(0x33);
    LCD_1IN69_SendData_8Bit(0x33);

    LCD_1IN69_SendCommand(0xB7);
    LCD_1IN69_SendData_8Bit(0x35);

    LCD_1IN69_SendCommand(0xBB);
    LCD_1IN69_SendData_8Bit(0x32);

    LCD_1IN69_SendCommand(0xC2);
    LCD_1IN69_SendData_8Bit(0x01);

    LCD_1IN69_SendCommand(0xC3);
    LCD_1IN69_SendData_8Bit(0x15);

    LCD_1IN69_SendCommand(0xC4);
    LCD_1IN69_SendData_8Bit(0x20);

    LCD_1IN69_SendCommand(0xC6);
    LCD_1IN69_SendData_8Bit(0x0F);

    LCD_1IN69_SendCommand(0xD0);
    LCD_1IN69_SendData_8Bit(0xA4);
    LCD_1IN69_SendData_8Bit(0xA1);

    LCD_1IN69_SendCommand(0xE0);
    LCD_1IN69_SendData_8Bit(0xD0);
    LCD_1IN69_SendData_8Bit(0x08);
    LCD_1IN69_SendData_8Bit(0x0E);
    LCD_1IN69_SendData_8Bit(0x09);
    LCD_1IN69_SendData_8Bit(0x09);
    LCD_1IN69_SendData_8Bit(0x05);
    LCD_1IN69_SendData_8Bit(0x31);
    LCD_1IN69_SendData_8Bit(0x33);
    LCD_1IN69_SendData_8Bit(0x48);
    LCD_1IN69_SendData_8Bit(0x17);
    LCD_1IN69_SendData_8Bit(0x14);
    LCD_1IN69_SendData_8Bit(0x15);
    LCD_1IN69_SendData_8Bit(0x31);
    LCD_1IN69_SendData_8Bit(0x34);

    LCD_1IN69_SendCommand(0xE1);
    LCD_1IN69_SendData_8Bit(0xD0);
    LCD_1IN69_SendData_8Bit(0x08);
    LCD_1IN69_SendData_8Bit(0x0E);
    LCD_1IN69_SendData_8Bit(0x09);
    LCD_1IN69_SendData_8Bit(0x09);
    LCD_1IN69_SendData_8Bit(0x15);
    LCD_1IN69_SendData_8Bit(0x31);
    LCD_1IN69_SendData_8Bit(0x33);
    LCD_1IN69_SendData_8Bit(0x48);
    LCD_1IN69_SendData_8Bit(0x17);
    LCD_1IN69_SendData_8Bit(0x14);
    LCD_1IN69_SendData_8Bit(0x15);
    LCD_1IN69_SendData_8Bit(0x31);
    LCD_1IN69_SendData_8Bit(0x34);

    LCD_1IN69_SendCommand(0x21);
    LCD_1IN69_SendCommand(0x29);
}

static void LCD_1IN69_SetAttributes(UBYTE Scan_dir)
{
    LCD_1IN69.SCAN_DIR = Scan_dir;
    if (Scan_dir == HORIZONTAL) {
        LCD_1IN69.HEIGHT = LCD_1IN69_WIDTH;
        LCD_1IN69.WIDTH = LCD_1IN69_HEIGHT;
    } else {
        LCD_1IN69.HEIGHT = LCD_1IN69_HEIGHT;
        LCD_1IN69.WIDTH = LCD_1IN69_WIDTH;
    }
}

void LCD_1IN69_Init(UBYTE Scan_dir)
{
    LCD_1IN69_Reset();
    LCD_1IN69_SetAttributes(Scan_dir);

#if LCD_1IN69_INIT_SEQUENCE == LCD_1IN69_INIT_SEQUENCE_P169H002
    LCD_1IN69_InitRegP169H002(LCD_1IN69_GetMemoryAccessReg(Scan_dir));
#elif LCD_1IN69_INIT_SEQUENCE == LCD_1IN69_INIT_SEQUENCE_LEGACY
    LCD_1IN69_InitRegLegacy();
#else
#error "Unsupported LCD_1IN69_INIT_SEQUENCE"
#endif
}

void LCD_1IN69_SetWindows(UWORD Xstart, UWORD Ystart, UWORD Xend, UWORD Yend)
{
    if (LCD_1IN69.SCAN_DIR == VERTICAL) {
        LCD_1IN69_SendCommand(0x2A);
        LCD_1IN69_SendData_8Bit(Xstart >> 8);
        LCD_1IN69_SendData_8Bit(Xstart);
        LCD_1IN69_SendData_8Bit((Xend - 1U) >> 8);
        LCD_1IN69_SendData_8Bit(Xend - 1U);

        LCD_1IN69_SendCommand(0x2B);
        LCD_1IN69_SendData_8Bit((Ystart + 20U) >> 8);
        LCD_1IN69_SendData_8Bit(Ystart + 20U);
        LCD_1IN69_SendData_8Bit((Yend + 20U - 1U) >> 8);
        LCD_1IN69_SendData_8Bit(Yend + 20U - 1U);
    } else {
        LCD_1IN69_SendCommand(0x2A);
#if LCD_1IN69_INIT_SEQUENCE == LCD_1IN69_INIT_SEQUENCE_P169H002
        LCD_1IN69_SendData_8Bit(Xstart >> 8);
        LCD_1IN69_SendData_8Bit(Xstart);
        LCD_1IN69_SendData_8Bit((Xend - 1U) >> 8);
        LCD_1IN69_SendData_8Bit(Xend - 1U);
#else
        LCD_1IN69_SendData_8Bit((Xstart + 20U) >> 8);
        LCD_1IN69_SendData_8Bit(Xstart + 20U);
        LCD_1IN69_SendData_8Bit((Xend + 20U - 1U) >> 8);
        LCD_1IN69_SendData_8Bit(Xend + 20U - 1U);
#endif

        LCD_1IN69_SendCommand(0x2B);
        LCD_1IN69_SendData_8Bit(Ystart >> 8);
        LCD_1IN69_SendData_8Bit(Ystart);
        LCD_1IN69_SendData_8Bit((Yend - 1U) >> 8);
        LCD_1IN69_SendData_8Bit(Yend - 1U);
    }

    LCD_1IN69_SendCommand(0x2C);
}

static void LCD_1IN69_WriteRepeatedColor(UWORD Color, UDOUBLE PixelCount)
{
    static UBYTE Buffer[LCD_1IN69_FILL_BLOCK_PIXELS * 2U];
    UWORD i;
    UBYTE ColorHi = (UBYTE)(Color >> 8);
    UBYTE ColorLo = (UBYTE)(Color & 0xffU);

    for (i = 0U; i < LCD_1IN69_FILL_BLOCK_PIXELS; i++) {
        Buffer[i * 2U] = ColorHi;
        Buffer[i * 2U + 1U] = ColorLo;
    }

    while (PixelCount > 0U) {
        UDOUBLE CurrentPixels = PixelCount;
        if (CurrentPixels > LCD_1IN69_FILL_BLOCK_PIXELS) {
            CurrentPixels = LCD_1IN69_FILL_BLOCK_PIXELS;
        }

        DEV_SPI_WriteBuffer(Buffer, CurrentPixels * 2U);
        PixelCount -= CurrentPixels;
    }
}

void LCD_1IN69_FillRect(UWORD Xstart, UWORD Ystart, UWORD Xend, UWORD Yend, UWORD Color)
{
    UDOUBLE PixelCount;

    if ((Xstart >= LCD_1IN69.WIDTH) || (Ystart >= LCD_1IN69.HEIGHT)) {
        return;
    }

    if (Xend > LCD_1IN69.WIDTH) {
        Xend = LCD_1IN69.WIDTH;
    }

    if (Yend > LCD_1IN69.HEIGHT) {
        Yend = LCD_1IN69.HEIGHT;
    }

    if ((Xstart >= Xend) || (Ystart >= Yend)) {
        return;
    }

    PixelCount = (UDOUBLE)(Xend - Xstart) * (UDOUBLE)(Yend - Ystart);

    LCD_1IN69_SetWindows(Xstart, Ystart, Xend, Yend);
    LCD_1IN69_DC_1;
    LCD_1IN69_CS_0;
    LCD_1IN69_WriteRepeatedColor(Color, PixelCount);
    LCD_1IN69_CS_1;
}

void LCD_1IN69_FillRect_FastStatic(UWORD Xstart, UWORD Ystart, UWORD Xend, UWORD Yend, UWORD Color)
{
    if ((Xstart >= LCD_1IN69.WIDTH) || (Ystart >= LCD_1IN69.HEIGHT)) {
        return;
    }

    if (Xend >= LCD_1IN69.WIDTH) {
        Xend = (UWORD)(LCD_1IN69.WIDTH - 1U);
    }

    if (Yend >= LCD_1IN69.HEIGHT) {
        Yend = (UWORD)(LCD_1IN69.HEIGHT - 1U);
    }

    if ((Xstart > Xend) || (Ystart > Yend)) {
        return;
    }

    LCD_1IN69_FillRect(Xstart, Ystart, (UWORD)(Xend + 1U), (UWORD)(Yend + 1U), Color);
}

void LCD_1IN69_FillScreen(UWORD Color)
{
    LCD_1IN69_FillRect_FastStatic(0, 0, (UWORD)(LCD_1IN69.WIDTH - 1U), (UWORD)(LCD_1IN69.HEIGHT - 1U), Color);
}

void LCD_1IN69_DrawColorBars(void)
{
    static const UWORD Colors[] = {
        LCD_COLOR_RED,
        LCD_COLOR_GREEN,
        LCD_COLOR_BLUE,
        0xFFE0U,
        0x07FFU,
        0xF81FU,
    };
    UWORD BarCount = (UWORD)(sizeof(Colors) / sizeof(Colors[0]));
    UWORD BarWidth = (UWORD)(LCD_1IN69.WIDTH / BarCount);
    UWORD Xstart = 0U;
    UWORD i;

    for (i = 0U; i < BarCount; i++) {
        UWORD Xend = (i == (BarCount - 1U)) ? LCD_1IN69.WIDTH : (UWORD)(Xstart + BarWidth);
        LCD_1IN69_FillRect(Xstart, 0, Xend, LCD_1IN69.HEIGHT, Colors[i]);
        Xstart = Xend;
    }
}

void LCD_1IN69_Clear(UWORD Color)
{
    LCD_1IN69_FillScreen(Color);
}

void LCD_1IN69_Display(UWORD *Image)
{
    UWORD i;
    UWORD j;

    LCD_1IN69_SetWindows(0, 0, LCD_1IN69.WIDTH, LCD_1IN69.HEIGHT);
    LCD_1IN69_DC_1;
    LCD_1IN69_CS_0;

    for (i = 0U; i < LCD_1IN69_WIDTH; i++) {
        for (j = 0U; j < LCD_1IN69_HEIGHT; j++) {
            UWORD Pixel = *(Image + i * LCD_1IN69_HEIGHT + j);
            DEV_SPI_WRITE((Pixel >> 8) & 0xffU);
            DEV_SPI_WRITE(Pixel);
        }
    }

    LCD_1IN69_CS_1;
}

void LCD_1IN69_DisplayWindows(UWORD Xstart, UWORD Ystart, UWORD Xend, UWORD Yend, UWORD *Image)
{
    UDOUBLE Addr = 0U;
    UWORD i;
    UWORD j;

    LCD_1IN69_SetWindows(Xstart, Ystart, Xend, Yend);
    LCD_1IN69_DC_1;
    LCD_1IN69_CS_0;

    for (i = Ystart; i < Yend; i++) {
        Addr = Xstart + i * LCD_1IN69_WIDTH;
        for (j = Xstart; j < Xend; j++) {
            DEV_SPI_WRITE((*(Image + Addr + j) >> 8) & 0xffU);
            DEV_SPI_WRITE(*(Image + Addr + j));
        }
    }

    LCD_1IN69_CS_1;
}

void LCD_1IN69_DrawPoint(UWORD X, UWORD Y, UWORD Color)
{
    LCD_1IN69_SetWindows(X, Y, (UWORD)(X + 1U), (UWORD)(Y + 1U));
    LCD_1IN69_SendData_16Bit(Color);
}

void LCD_1IN69_FillRect(UWORD Xstart, UWORD Ystart, UWORD Width, UWORD Height, UWORD Color)
{
    uint32_t pixelCount;

    if ((Width == 0U) || (Height == 0U) ||
        (Xstart >= LCD_1IN69.WIDTH) || (Ystart >= LCD_1IN69.HEIGHT)) {
        return;
    }

    if ((UWORD)(Xstart + Width) > LCD_1IN69.WIDTH) {
        Width = (UWORD)(LCD_1IN69.WIDTH - Xstart);
    }
    if ((UWORD)(Ystart + Height) > LCD_1IN69.HEIGHT) {
        Height = (UWORD)(LCD_1IN69.HEIGHT - Ystart);
    }

    LCD_1IN69_SetWindows(Xstart, Ystart, (UWORD)(Xstart + Width), (UWORD)(Ystart + Height));
    LCD_1IN69_DC_1;
    LCD_1IN69_CS_0;
    pixelCount = (uint32_t)Width * (uint32_t)Height;
    while (pixelCount > 0U) {
        DEV_SPI_WRITE((Color >> 8) & 0xff);
        DEV_SPI_WRITE(Color & 0xff);
        pixelCount--;
    }
    LCD_1IN69_CS_1;
}

void LCD_1IN69_SetBackLight(UWORD Value)
{
    DEV_Set_PWM(Value);
}
