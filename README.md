# FZ2 Sim (VST3)

Unofficial emulation of a Super-Fuzz family octave fuzz with a three-way mode switch, in the style of the
Boss FZ-2 Hyper Fuzz. It is a behavioural model built from the few published facts about the pedal,
**not** a schematic-exact copy, and is not affiliated with any manufacturer.

Published facts used: closely related to the 1968 Univox / Shin-ei Super-Fuzz; a differential pair gives subtle
octaving under the right conditions; controls Gain, Level, Bass, Treble and a Mode switch: Fuzz I (end of the
fuzz circuit), Fuzz II (same signal through a mid scoop), Boost (taken right after the input stage, clean).
Transistor biasing, tone-stack values and the Bass/Treble corner frequencies are informed approximations.

## Signal flow
```
in -> op-amp style gain stage (Gain) -> [Boost tap]
   -> asymmetric transistor clipper -> differential pair (partial rectifier = octave)
   -> second clipper -> [Fuzz I tap] -> mid scoop -> [Fuzz II tap]
tap mix -> Bass band (about 100 Hz, half an octave wide) -> Treble shelf -> Level -> out
```

## Controls
| Control | What it does |
|---|---|
| Gain | 0 to +40 dB into the circuit. In Boost mode it acts as a volume control |
| Level | Output level (centre is about unity). In Boost mode it is a second volume control |
| Bass | Boost/cut of about +-12 dB around 100 Hz |
| Treble | Boost/cut of about +-12 dB shelf above about 2.5 kHz |
| Mode | Fuzz I, Fuzz II (mid scoop), Boost (clean), Fuzz I + II (emulates leaving the real switch between the two fuzz positions) |
| Octave (extra) | Not on the real pedal: how strongly the differential pair rectifies. 0 = almost none, 1 = strong octave up |
| ON / OFF | Footswitch, click-free crossfade |
| Input / Output | Host-side trims, -24 to +24 dB and -24 to +12 dB |

At extreme settings (everything up, +24 dB input) the output can be very loud: use the Output control.
Runs at 4x oversampling (the plugin reports the oversampler latency to the host). The analog-style octave
is monophonic and works best on single notes.

## Build
CMake 3.22+ and a C++17 compiler. JUCE 8.0.15 is downloaded automatically.
```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```
Result: `build/FZ2Sim_artefacts/Release/VST3/FZ2 Sim.vst3`

Install: Windows `C:\Program Files\Common Files\VST3\`, macOS `~/Library/Audio/Plug-Ins/VST3/`, Linux `~/.vst3/`.

No compiler? Push this folder to GitHub: `.github/workflows/build.yml` builds Windows, macOS and Linux VST3s as downloadable artifacts.

Offline test: `cmake -S . -B build -DFZ_BUILD_TESTS=ON && cmake --build build --target dsp_test && ./build/dsp_test`

JUCE is used under AGPLv3/commercial licensing: if you distribute the plugin, AGPL terms apply.
