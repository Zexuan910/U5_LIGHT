#ifndef UI_ASSETS_H
#define UI_ASSETS_H

#include <stdint.h>

typedef enum {
    UI_ASSET_WATCH_BG = 0,
    UI_ASSET_SPORT_BG = 1
} UIAssetId;

uint8_t UIAssets_Init(void);
uint8_t UIAssets_Preload(void);
uint8_t UIAssets_DrawBackground(UIAssetId asset);

#endif
