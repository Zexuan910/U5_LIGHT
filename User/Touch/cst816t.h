#ifndef CST816T_H
#define CST816T_H

#include "main.h"
#include "DEV_Config.h"

#define CST816T_WIDTH  240U
#define CST816T_HEIGHT 280U

void CST816T_Init(void);
UBYTE CST816T_IsConnected(void);
void CST816T_KeepAwake(void);
UBYTE CST816T_ReadTouch(UWORD* x, UWORD* y);
UBYTE CST816T_ReadTouchLoose(UWORD* x, UWORD* y);
UBYTE CST816T_ReadGesture(UBYTE* gesture);
UBYTE CST816T_ReadActivity(void);
UBYTE CST816T_ReadDebug(UBYTE* chip_id, UBYTE* finger, UWORD* x, UWORD* y);

#endif
