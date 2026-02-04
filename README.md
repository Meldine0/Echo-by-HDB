# Echo by HDB (VST3)

A simple, low-latency stereo echo/delay plugin built with JUCE for Windows 64-bit VST3 hosts (tested workflow for FL Studio).

## Features

- Stereo 2→2 delay with optional ping-pong feedback
- Delay time in milliseconds or host-tempo sync divisions
- Linear interpolation for fractional delay times
- Feedback loop filtering (LowCut/HighCut) + soft drive saturation
- Constant-power dry/wet mix
- Smoothed parameters to avoid zipper noise
- Full parameter automation and preset/state recall

## Build Requirements

- Windows 11 64-bit
- Visual Studio 2022 (MSVC)
- CMake 3.22+
- JUCE (locally cloned)

## Build Instructions (Visual Studio 2022)

1. Clone JUCE (recommended next to this repo):
   ```bash
   git clone https://github.com/juce-framework/JUCE.git
   ```
2. Configure the project with CMake (set JUCE_PATH):
   ```bash
   cmake -S . -B build -DJUCE_PATH=C:/path/to/JUCE
   ```
3. Open the generated solution:
   ```bash
   cmake --open build
   ```
4. In Visual Studio, select `Release` and build the `EchoByHDB` target.

The VST3 binary will be created at:
```
build/EchoByHDB_artefacts/Release/VST3/Echo by HDB.vst3
```

## Install (VST3)

Copy the plugin bundle to the standard VST3 location:
```
C:\Program Files\Common Files\VST3\
```

## FL Studio Scan

1. Open **Options → Manage plugins**
2. Click **Find Plugins**
3. Confirm **Echo by HDB** appears under the **HDB** manufacturer

## Parameters

| Parameter | Range | Notes |
| --- | --- | --- |
| Time | 1 → 2000 ms | Smoothed, linear interpolation |
| Sync | Off/On | Uses host tempo if available |
| Sync Division | 1/1…1/16D | Fallback to ms when tempo is unavailable |
| Feedback | 0 → 95% | Clamped below unity |
| Mix | 0 → 100% | Constant power |
| LowCut | 20 → 1000 Hz | In feedback loop |
| HighCut | 1000 → 20000 Hz | In feedback loop |
| PingPong | Off/On | L↔R feedback |
| Drive | 0 → 24 dB | Soft tanh saturation |
| Output | -24 → +6 dB | Smoothed |

## Bypass Behavior

Host bypass is handled by the DAW. The plugin keeps a short tail (2.5s) to avoid abrupt cuts.

## Test Checklist

- [ ] Automation of Time/Feedback/Mix is smooth (no zipper noise)
- [ ] Tempo sync switches between divisions correctly
- [ ] Fallback to ms when host tempo is unavailable
- [ ] Ping-pong mode alternates L/R as expected
- [ ] Verify behavior with different sample rates (44.1/48/96 kHz)
- [ ] Verify behavior with different buffer sizes
- [ ] Confirm Output and Drive do not clip unexpectedly

## Notes

- This project builds VST3 only (no VST2).
- Plugin name shown in host: **Echo by HDB**
- Manufacturer shown in host: **HDB**
