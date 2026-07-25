# U5 Screen, UI, Touch Debug Notes

## Current State

- The ST7789 screen can light and draw the UI.
- External GD25Q128E Flash can be written by the temporary asset-programmer firmware.
- UI background assets are read from external Flash, then cached in STM32U575 RAM.
- Background redraw now outputs from RAM in small row blocks to avoid lower-screen color corruption.
- Touch can drive long-press/page interactions, but the black lock-page wakeup still has an unresolved second-wakeup failure.
- The current lock page is implemented as a pure black UI page, not a real low-power lock mode.

## Main Problems Encountered

### 1. Backlight on but no image

Early failures looked like the LCD was only lighting the backlight. The root causes were mostly code-side display initialization and transfer details:

- LCD reset, CS, DC, SPI mode, and init sequence must match the working U575 reference project.
- A visible backlight does not prove SPI display data is correct.
- Color-bar and black-white tests were useful because they separated "LCD accepts pixels" from "UI/resource loading works".

### 2. Color bars with row misalignment

The color-bar stage exposed row and address-window alignment problems.

Things to watch:

- Use consistent inclusive/exclusive rectangle coordinates.
- Be careful with one-row or partial-row writes near the bottom of the screen.
- Verify both full-screen and small-region drawing paths; one can work while the other has an off-by-one.

### 3. External Flash asset workflow

The board external Flash could not be handled reliably through a generic external loader path, so the project uses a temporary firmware mode:

- Build with `UI_ASSET_PROGRAMMER=ON`.
- The asset package is embedded into internal Flash.
- Firmware erases/programs/verifies the external GD25Q128E through GPIO bit-bang SPI.
- After `FLASH OK`, rebuild and flash the normal firmware.

Important pitfalls:

- New boards need the external Flash assets written again.
- Normal firmware is small because the images are not embedded.
- If the external Flash is blank, the UI falls back to plain color backgrounds.

### 4. Background image performance and corruption

Reading full backgrounds directly from external Flash on every page transition was slow. Caching backgrounds in RAM improved UI responsiveness.

However, pushing a whole 240x280 RGB565 image as one transfer caused lower-screen color corruption on this setup. The safer path is:

- Load image once from external Flash to RAM cache.
- Draw from RAM cache in row blocks.
- Avoid reloading background when only a small UI element changes.

### 5. START/STOP button redraw

The sport detail page originally redrew the whole page when toggling START/STOP. That reloaded/redrew the background and caused visible delay.

Fix:

- Split the START/STOP button into its own draw function.
- Toggle `exercise_running`.
- Redraw only the button rectangle.

### 6. Touch short press and coordinate handling

Short touch was unreliable when only polling `TP_INT` in the main loop. Improvements added:

- EXTI falling-edge interrupt on TP_INT.
- I2C3 CST816T coordinate reads.
- Fallback to raw TP_INT state.
- Coordinate-based tap and swipe handling.

Pitfalls:

- Do not assume the touch coordinate orientation is already 240x280.
- CST816T finger count should be masked with `0x0F`; upper bits may carry event data.
- Startup or wakeup touch residue can create false page jumps unless the code waits for release.

### 7. Lock/wake behavior

Several lock-screen bugs came from treating wake touch as a normal press/release sequence:

- Wake press could immediately become a long press and relock.
- A stale pressed state could prevent a new press edge from being seen.
- Turning the backlight off appeared to make wakeup unreliable on this board/screen combination.

Current mitigation:

- Lock is temporarily just a pure black UI page.
- Backlight stays enabled.
- The page has its own raw touch wake path.
- After wake, input is suppressed until release.

Still unresolved:

- First wake may succeed while the second wake can fail. This suggests the touch chip may be sleeping, the INT pulse may be missed, or the black page path still needs a more explicit CST816T wake/reset strategy.

## Coding Rules Learned

- Keep hardware tests incremental: backlight, black/white, color bars, full UI, external assets, touch.
- Do not redraw full pages for small state changes.
- Avoid single huge LCD transfers when smaller row blocks are stable.
- Always think about touch press lifecycle: press, hold, release, startup residue, wake residue.
- Avoid using low-power or backlight-off behavior until touch wake is proven reliable.
- New boards need both MCU firmware and external Flash assets.

## Suggested Next Steps

- Add a visible one-pixel or small text debug marker on the black page to show raw touch activity counts.
- Try resetting CST816T when entering the black page or before wake polling.
- Consider disabling CST816T auto-sleep with the exact register sequence from the H750 reference if available.
- Later, replace the software clock with RTC and add a simple time-setting page.
