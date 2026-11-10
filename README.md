# WRAITH WEAVE 0.4 — STRONG NEW WEAVE

Built on 0.3 without changing plugin manufacturer/code or parameter IDs.

## What changed

The previous NEW WEAVE mostly altered subtle pitch bias and rhythm probabilities. Version 0.4 randomizes complete sonic scenes using six strongly differentiated rotating families:

- Large pitch transpositions (about -19 to +19 semitones), plus pitch spread
- Sample focus/region and region width
- Grain duration from very short to stretched
- Density and layer rate
- Reverse-playback probability
- Half-rate movement in some scenes
- Stereo position and changed rhythm stride/gate lengths
- New 16-step rhythm groove on each press

Previous grains finish their envelopes to avoid abrupt clicks. New changes become clear as old grains fade out. The WAV sample is kept. A scene counter is displayed in the plugin.

## Installation

Replace repository contents with this folder, commit, run existing GitHub Actions Windows VST3 workflow, download artifact and replace existing plugin. Back up the working 0.3 project first.

## Testing

Record a 2-sec sample, set Mix 100%, Variation 0.5-0.8, Density 0.5, repeatedly press NEW WEAVE. Check both Texture and Rhythm. Changes can take up to the duration of active grains to become dominant.

Not yet compiled/tested on Windows or Ableton.

## 0.4.1 hotfix
Fixed MSVC C2280 in PluginProcessor.cpp (output level atomic parameter is read with load()).
