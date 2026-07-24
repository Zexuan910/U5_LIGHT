#include "ui_asset_programmer.h"

#include "ext_flash.h"
#include "ui_asset_package.h"

#define UI_ASSET_BASE 0x00000000UL
#define UI_ASSET_SECTOR_SIZE 4096UL
#define UI_ASSET_PAGE_SIZE 256UL

#if UI_ASSET_PROGRAMMER
uint8_t UIAssetProgrammer_Run(uint32_t* written_bytes)
{
    uint8_t verify[UI_ASSET_PAGE_SIZE];
    uint32_t offset = 0UL;
    uint32_t sector_count;
    uint32_t sector;

    if (written_bytes != 0) {
        *written_bytes = 0UL;
    }

    if (ExtFlash_Init() == 0U) {
        return 0U;
    }

    if (g_ui_asset_package_size > EXT_FLASH_ASSET_PARTITION_BYTES) {
        return 0U;
    }

    sector_count = (g_ui_asset_package_size + UI_ASSET_SECTOR_SIZE - 1UL) / UI_ASSET_SECTOR_SIZE;
    for (sector = 0UL; sector < sector_count; sector++) {
        if (ExtFlash_Erase4K(UI_ASSET_BASE + (sector * UI_ASSET_SECTOR_SIZE)) == 0U) {
            return 0U;
        }
    }

    while (offset < g_ui_asset_package_size) {
        uint32_t chunk = g_ui_asset_package_size - offset;
        uint32_t i;

        if (chunk > UI_ASSET_PAGE_SIZE) {
            chunk = UI_ASSET_PAGE_SIZE;
        }

        if (ExtFlash_PageProgram(UI_ASSET_BASE + offset, &g_ui_asset_package[offset], chunk) == 0U) {
            return 0U;
        }

        if (ExtFlash_Read(UI_ASSET_BASE + offset, verify, chunk) == 0U) {
            return 0U;
        }

        for (i = 0UL; i < chunk; i++) {
            if (verify[i] != g_ui_asset_package[offset + i]) {
                return 0U;
            }
        }

        offset += chunk;
        if (written_bytes != 0) {
            *written_bytes = offset;
        }
    }

    return 1U;
}
#else
uint8_t UIAssetProgrammer_Run(uint32_t* written_bytes)
{
    if (written_bytes != 0) {
        *written_bytes = 0UL;
    }
    return 0U;
}
#endif
