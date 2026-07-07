#include "ui_assets.h"

#include "LCD_1in69.h"
#include "ext_flash.h"

#include <string.h>

#define UI_ASSET_MAGIC0 'U'
#define UI_ASSET_MAGIC1 '5'
#define UI_ASSET_MAGIC2 'U'
#define UI_ASSET_MAGIC3 'I'
#define UI_ASSET_VERSION 1UL
#define UI_ASSET_COUNT 2UL
#define UI_ASSET_ENTRY_SIZE 32UL
#define UI_ASSET_HEADER_SIZE (12UL + (UI_ASSET_COUNT * UI_ASSET_ENTRY_SIZE))
#define UI_ASSET_WIDTH 240UL
#define UI_ASSET_HEIGHT 280UL
#define UI_ASSET_IMAGE_SIZE (UI_ASSET_WIDTH * UI_ASSET_HEIGHT * 2UL)
#define UI_ASSET_DRAW_ROWS 20UL

typedef struct {
    uint32_t offset;
    uint32_t size;
    uint32_t width;
    uint32_t height;
} UIAssetDesc;

static uint8_t ui_assets_ready = 0U;
static uint8_t ui_asset_loaded[UI_ASSET_COUNT] = {0U, 0U};
static uint8_t ui_asset_cache[UI_ASSET_COUNT][UI_ASSET_IMAGE_SIZE];
static const UIAssetDesc ui_asset_desc[UI_ASSET_COUNT] = {
    { UI_ASSET_HEADER_SIZE, UI_ASSET_IMAGE_SIZE, UI_ASSET_WIDTH, UI_ASSET_HEIGHT },
    { UI_ASSET_HEADER_SIZE + UI_ASSET_IMAGE_SIZE, UI_ASSET_IMAGE_SIZE, UI_ASSET_WIDTH, UI_ASSET_HEIGHT }
};

static uint32_t UIAssets_ReadU32LE(const uint8_t* data)
{
    return ((uint32_t)data[0]) |
           ((uint32_t)data[1] << 8) |
           ((uint32_t)data[2] << 16) |
           ((uint32_t)data[3] << 24);
}

uint8_t UIAssets_Init(void)
{
    uint8_t header[12];

    ui_assets_ready = 0U;

    if (ExtFlash_Init() == 0U) {
        return 0U;
    }

    if (ExtFlash_Read(EXT_FLASH_ASSET_BASE, header, sizeof(header)) == 0U) {
        return 0U;
    }

    if ((header[0] != (uint8_t)UI_ASSET_MAGIC0) ||
        (header[1] != (uint8_t)UI_ASSET_MAGIC1) ||
        (header[2] != (uint8_t)UI_ASSET_MAGIC2) ||
        (header[3] != (uint8_t)UI_ASSET_MAGIC3)) {
        return 0U;
    }

    if ((UIAssets_ReadU32LE(&header[4]) != UI_ASSET_VERSION) ||
        (UIAssets_ReadU32LE(&header[8]) != UI_ASSET_COUNT)) {
        return 0U;
    }

    ui_assets_ready = 1U;
    return 1U;
}

static uint8_t UIAssets_LoadToCache(UIAssetId asset)
{
    static uint8_t rows[UI_ASSET_WIDTH * UI_ASSET_DRAW_ROWS * 2UL];
    const UIAssetDesc* desc;
    uint32_t y;
    uint8_t* cache;

    if ((ui_assets_ready == 0U) || ((uint32_t)asset >= UI_ASSET_COUNT)) {
        return 0U;
    }

    desc = &ui_asset_desc[(uint32_t)asset];
    if ((desc->width != UI_ASSET_WIDTH) ||
        (desc->height != UI_ASSET_HEIGHT) ||
        (desc->size != UI_ASSET_IMAGE_SIZE)) {
        return 0U;
    }

    if (ui_asset_loaded[(uint32_t)asset] != 0U) {
        return 1U;
    }

    cache = ui_asset_cache[(uint32_t)asset];
    for (y = 0UL; y < UI_ASSET_HEIGHT; y += UI_ASSET_DRAW_ROWS) {
        uint32_t rows_to_draw = UI_ASSET_HEIGHT - y;
        uint32_t byte_count;
        uint32_t address;

        if (rows_to_draw > UI_ASSET_DRAW_ROWS) {
            rows_to_draw = UI_ASSET_DRAW_ROWS;
        }

        byte_count = rows_to_draw * UI_ASSET_WIDTH * 2UL;
        address = EXT_FLASH_ASSET_BASE + desc->offset + (y * UI_ASSET_WIDTH * 2UL);
        if (ExtFlash_Read(address, rows, byte_count) == 0U) {
            return 0U;
        }
        memcpy(&cache[y * UI_ASSET_WIDTH * 2UL], rows, byte_count);
    }

    ui_asset_loaded[(uint32_t)asset] = 1U;
    return 1U;
}

uint8_t UIAssets_Preload(void)
{
    uint32_t asset;
    uint8_t ok = 1U;

    for (asset = 0UL; asset < UI_ASSET_COUNT; asset++) {
        if (UIAssets_LoadToCache((UIAssetId)asset) == 0U) {
            ok = 0U;
        }
    }

    return ok;
}

uint8_t UIAssets_DrawBackground(UIAssetId asset)
{
    uint32_t y;
    const uint8_t* cache;

    if (((uint32_t)asset >= UI_ASSET_COUNT) || (UIAssets_LoadToCache(asset) == 0U)) {
        return 0U;
    }

    cache = ui_asset_cache[(uint32_t)asset];
    for (y = 0UL; y < UI_ASSET_HEIGHT; y += UI_ASSET_DRAW_ROWS) {
        uint32_t rows_to_draw = UI_ASSET_HEIGHT - y;

        if (rows_to_draw > UI_ASSET_DRAW_ROWS) {
            rows_to_draw = UI_ASSET_DRAW_ROWS;
        }

        LCD_1IN69_DrawRGB565Bytes(0U,
                                  (UWORD)y,
                                  (UWORD)UI_ASSET_WIDTH,
                                  (UWORD)rows_to_draw,
                                  &cache[y * UI_ASSET_WIDTH * 2UL]);
    }

    return 1U;
}
