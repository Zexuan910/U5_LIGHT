# RUN/STILL NanoEdge model

Source package:
`C:\Users\35156\Desktop\libneai_project-2026-07-23-17-13_32.zip`

The generated library targets Cortex-M33 with hard-float and short enums. Its
384-value input is 64 samples of six interleaved IMU axes. Metadata orders the
two classes as `still_3m50s_20260713_s6` and `run_374m_314steps`; the compiled
API exposes those class IDs using the numeric strings `0` and `1`.

NanoEdge libraries export common runtime symbols, so the new archive cannot be
linked beside the existing WALK model unchanged. `libneai_run.a` is produced
from the package's `libneai.a` with:

```text
arm-none-eabi-objcopy --redefine-syms=symbol_map.txt libneai.a libneai_run.a
```

The symbol mapping gives every global defined by the RUN archive a
`runmodel_` prefix. Undefined C-library symbols are not renamed.
