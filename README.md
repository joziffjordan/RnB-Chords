# RnB Chords by Dev Griffin

A chord-progression instrument plugin (VST3, AU on Mac, plus a standalone app).

- 339 R&B progressions in four eras: 80s, 90s, 2000s, Modern
- The whole interface changes look with the era, with a coin-flip transition
- Play button loops the selected progression; chord pads audition single chords
- MIDI Trigger: white keys play the seven chords of the chosen key, black keys play borrowed chords
- Drag MIDI to DAW drops the progression as a MIDI clip
- Built-in electric-piano sound, and the chords are also sent to the plugin's MIDI output

## Build it

You need CMake 3.22 or newer and a C++ compiler (Xcode on Mac, Visual Studio 2022 on Windows).
JUCE 8 is downloaded automatically the first time.

    cmake -B build -DCMAKE_BUILD_TYPE=Release
    cmake --build build --config Release

The plugin lands in `build/RnBChords_artefacts/Release/` (on Windows: `build/RnBChords_artefacts/Release/VST3/`).

- Mac: copy `RnB Chords.vst3` to `~/Library/Audio/Plug-Ins/VST3/` and `RnB Chords.component` to `~/Library/Audio/Plug-Ins/Components/`
- Windows: copy `RnB Chords.vst3` to `C:\Program Files\Common Files\VST3\`

Then rescan plugins in your DAW. In Ableton, load it on a MIDI track like any instrument.

If you push this folder to GitHub, the included workflow (`.github/workflows/build.yml`) builds the Mac and
Windows versions for you and attaches them to the run as downloads.

## Where things are

- `Resources/rnb_chords_db.json`: the progression database. Edit it and rebuild to add progressions.
- `Source/ChordDatabase.h`: loads the database and turns chords into notes
- `Source/PluginProcessor.*`: sound, playback, MIDI trigger, MIDI file export
- `Source/PluginEditor.*`, `Source/Theme.h`: the interface and the four era looks
