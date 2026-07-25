# U5_LIGHT 编译与烧录指南

本文档用于在一台新的 Windows 电脑上获取 `wzx` 分支、编译固件并通过
ST-LINK 烧录到 STM32U575 手表。

## 1. 准备工具

安装以下软件：

- Git
- STM32CubeCLT 或带 ARM GCC 工具链的 STM32CubeIDE
- STM32CubeProgrammer
- CMake 和 Ninja
- ST-LINK 驱动

确认下列程序可以在 PowerShell 中运行：

```powershell
git --version
arm-none-eabi-gcc --version
ninja --version
```

项目脚本会优先使用系统 `PATH` 中的 CMake 和
`STM32_Programmer_CLI.exe`，也能识别 STM32Cube 工具安装在
`%LOCALAPPDATA%\stm32cube\bundles` 下的常见目录。

## 2. 获取代码

```powershell
git clone -b wzx https://github.com/Zexuan910/U5_LIGHT.git
cd U5_LIGHT
```

已有仓库时更新代码：

```powershell
git switch wzx
git pull origin wzx
```

## 3. 连接硬件

1. 将 ST-LINK 的 `SWDIO`、`SWCLK`、`GND` 和目标板对应引脚连接好。
2. 给手表主板供电。
3. 将 ST-LINK 接入电脑。
4. 烧录期间不要断开供电或调试线。

## 4. 编译普通固件

在仓库根目录运行：

```powershell
powershell -ExecutionPolicy Bypass -File .\tools\run_cmake.ps1 --preset Debug
powershell -ExecutionPolicy Bypass -File .\tools\run_cmake.ps1 --build --preset Debug
```

成功后生成：

```text
build\Debug\U5_LIGHT.elf
build\Debug\U5_LIGHT.hex
build\Debug\U5_LIGHT.bin
```

每次构建都会读取电脑当前的本地日期和时间，并生成
`build\Debug\generated\ui_pc_time.h`。固件启动后以这个时间为初值继续走时。

## 5. 烧录普通固件

```powershell
powershell -ExecutionPolicy Bypass -File .\tools\program_flash.ps1 -Config Debug
```

脚本会先刷新电脑时间并重新配置、构建固件，再通过 SWD 连接目标板，写入
`U5_LIGHT.hex`，随后校验并复位 MCU。因此使用本脚本烧录时，不需要提前单独执行构建命令。
日志出现以下内容表示烧录成功：

```text
Download verified successfully
MCU Reset
```

首页保持显示到分钟，秒在固件内部继续累计。长按首页息屏只关闭 LCD 背光，
主循环和软件时钟不会停止；触摸唤醒后会显示已经继续走过的时间。

如果绕过本脚本，直接在 STM32CubeProgrammer 中烧录以前生成的 HEX，固件使用的仍是
该 HEX 构建时记录的旧电脑时间。需要更新时间时，应重新运行上述项目烧录命令。

如果脚本无法自动找到 STM32CubeProgrammer，可显式指定：

```powershell
powershell -ExecutionPolicy Bypass -File .\tools\program_flash.ps1 `
  -Config Debug `
  -ProgrammerPath "C:\Program Files\STMicroelectronics\STM32Cube\STM32CubeProgrammer\bin\STM32_Programmer_CLI.exe"
```

## 6. 首次写入 UI 素材

新主板或外部 Flash 中没有界面素材时，需要先执行一次本步骤。已经正常显示
表盘、导航页和运动背景的设备可以跳过。

编译素材写入固件：

```powershell
powershell -ExecutionPolicy Bypass -File .\tools\run_cmake.ps1 `
  --preset Debug -DUI_ASSET_PROGRAMMER=ON
powershell -ExecutionPolicy Bypass -File .\tools\run_cmake.ps1 `
  --build --preset Debug
powershell -ExecutionPolicy Bypass -File .\tools\program_flash.ps1 -Config Debug
```

等待屏幕显示 `FLASH OK`，期间不要断电。然后必须重新编译并烧录普通固件：

```powershell
powershell -ExecutionPolicy Bypass -File .\tools\run_cmake.ps1 `
  --preset Debug -DUI_ASSET_PROGRAMMER=OFF
powershell -ExecutionPolicy Bypass -File .\tools\run_cmake.ps1 `
  --build --preset Debug
powershell -ExecutionPolicy Bypass -File .\tools\program_flash.ps1 -Config Debug
```

## 7. 常见问题

- `arm-none-eabi-gcc` 找不到：将 ARM GCC 的 `bin` 目录加入系统 `PATH`，
  重新打开 PowerShell。
- `STM32_Programmer_CLI.exe was not found`：安装 STM32CubeProgrammer，或使用
  `-ProgrammerPath` 指定完整路径。
- 无法连接目标板：检查供电、GND、SWDIO、SWCLK，并确认没有其他调试软件占用
  ST-LINK。
- 校验失败：重新插拔 ST-LINK 和目标板电源后再次烧录。
- 背景图缺失但程序可运行：按“首次写入 UI 素材”完整执行一次。

