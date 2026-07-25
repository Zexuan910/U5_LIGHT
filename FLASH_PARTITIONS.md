# External Flash partitions

The 16 MB GD25Q128E is divided into independent UI and motion regions.

| Address range | Size | Purpose |
| --- | ---: | --- |
| `0x000000-0x0FFFFF` | 1 MB | UI asset package |
| `0x100000-0x100FFF` | 4 KB | Motion metadata copy A |
| `0x101000-0x101FFF` | 4 KB | Motion metadata copy B |
| `0x102000-0x10FFFF` | 56 KB | Reserved metadata expansion |
| `0x110000-0x90FFFF` | 8 MB | Persistent 6-axis motion samples |
| `0x910000-0xFFFFFF` | 6.94 MB | Reserved for future features |

Motion samples keep the NanoEdge export layout: one 16-byte record contains a
32-bit millisecond timestamp followed by three raw accelerometer and three raw
gyroscope `int16_t` values. The partition stores 524288 records, or about
2 hours 55 minutes at 50 Hz.

The session directory is stored as a CRC-protected snapshot. Session start and
stop alternate between metadata copies A and B, so a power loss cannot erase
the last valid directory while a replacement is being written. If power is
lost during an active session, startup scans the unfinished tail, closes the
session, and publishes it through the existing ST-LINK export interface.

UI asset programming may erase only the first 1 MB and therefore does not
touch persistent motion data. Normal internal-MCU firmware flashing also does
not erase the external Flash partitions.

After all required sessions have been exported, the persistent motion-data
directory can be cleared explicitly with:

```powershell
powershell -ExecutionPolicy Bypass -File .\tools\export_walk_dataset.ps1 `
  -Label CLEAR -EraseStoredData
```

This is a logical format. Data sectors are erased lazily as new samples reuse
them, reducing unnecessary erase cycles.
