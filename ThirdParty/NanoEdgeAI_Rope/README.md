# ROPE/STILL NanoEdge model

Source package:
`C:\Users\35156\Desktop\libneai_project-2026-07-24-19-29_21.zip`

The generated library targets Cortex-M33 with hard-float and short enums. Its
384-value input is 64 samples of six interleaved IMU axes. The two classes are
`still_1m40s_20260713` and `rope_105count`.

NanoEdge libraries export common runtime symbols, so this archive cannot be
linked beside the existing WALK and RUN models unchanged. `libneai_rope.a` is
produced from the package's `libneai.a` with:

```text
arm-none-eabi-objcopy --redefine-syms=symbol_map.txt libneai.a libneai_rope.a
```

The symbol mapping gives every global defined by the ROPE archive a
`ropemodel_` prefix. Undefined C-library symbols are not renamed.
