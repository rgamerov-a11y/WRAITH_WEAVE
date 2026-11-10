# WRAITH WEAVE 0.1 — JUCE VST3 for Ableton Live (Windows 11)

Independent experimental plugin **inspired by the documented interaction concept** of Fieldtone Weaver. No source code, firmware, branding artwork or proprietary assets from Fieldtone are used. This is not a verified sonic replica.

## Implemented
- Live stereo/mono audio input on an Ableton **audio track**.
- RECORD 2 SEC: capture next 2 seconds of incoming audio while Ableton audio engine runs.
- RESET: discard captured audio.
- TEXTURE: overlapping windowed randomized grains with varying positions, playback rate and stereo pan.
- RHYTHM: host-tempo-guided 16th note triggers, probabilistic playback.
- NEW WEAVE: reseed and regenerate playback events (UI button rather than infinite encoder).
- Automatable Mode, Variation, Mix and Density.
- Soft clipping to prevent extreme output amplitudes.
- Restore parameters with project save. **Captured audio is not saved** in version 0.1; record again after reopening or changing sample rate.

## Automated Windows build
1. Create a NEW GitHub repository (e.g. `WRAITH_WEAVE`) and upload the **contents** of this ZIP, preserving `.github/workflows/windows-vst3.yml`.
2. In GitHub open `Actions` > `WRAITH WEAVE Windows VST3` > `Run workflow` (or push to `main`).
3. Download `WRAITH-WEAVE-0.1-Windows-VST3` from the completed job's Artifacts.
4. Extract and copy the folder `WRAITH WEAVE.vst3` into `C:\Program Files\Common Files\VST3\`.
5. Restart Ableton Live or rescan plugins. Insert the plugin on an **audio** track and play some audio through it. Click RECORD, let >2 seconds play, and select Texture or Rhythm.

## Local compilation
```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release --target WRAITH_WEAVE_VST3
```
CMake downloads JUCE 8.0.6 from GitHub during configuration. Network is needed.

## Limitations / validation
- Workflow and C++ sources are prepared, but this environment cannot run the Windows compiler or Ableton. **Build and runtime results unverified.**
- Rhythm uses the host BPM but is not phase-aligned to the host transport position; synchronization refinement is pending.
- RECORD and NEW WEAVE are not DAW-automatable yet; audio is not embedded in saved plugin state.
- No MIDI input, WAV import, advanced random rules, crossfaded state transitions or finished GUI.
- Wet signal depends on source content and may need volume matching.
- This project is a first-pass generative prototype, not a claimed exact clone of Fieldtone's algorithms.
