# EMESSYOO

A professional desktop application for preparing, normalizing, and exporting high-quality MSU-1-compatible PCM audio files for SNES hacks and projects.

## Features

- **Audio Import**: Support for MP3, WAV, FLAC, OGG, and AIFF formats
- **Automatic Conversion**: Converts to 44.1kHz, 16-bit stereo PCM
- **Visual Loop Editor**: DAW-style waveform view with draggable loop markers (or droppable to mouse cursor's location via hotkey)
- **Normalization**: RMS and peak-based normalization
- **MSU-1 Export**: Proper PCM format with MSU-1 header and loop points
- **Manifest Integration**: Automatic track naming and .msu file management

## Building from Source

### Prerequisites

1. **CMake** (version 3.22 or higher)
2. **C++17 compatible compiler**
   - Windows: Visual Studio 2019 or later
   - macOS: Xcode 12 or later
   - Linux: GCC 9+ or Clang 10+
3. **JUCE Framework** (version 8.x)

### Setup

1. Clone this repository:
```bash
git clone https://github.com/yourusername/MSU1PrepStudio.git
cd MSU1PrepStudio
```

2. Download or clone JUCE into the project directory:
```bash
git clone https://github.com/juce-framework/JUCE.git
```

3. Create build directory:
```bash
mkdir build
cd build
```

4. Generate build files:
```bash
cmake ..
```

5. Build the project:
```bash
cmake --build . --config Release
```

### Windows

After building, the executable will be in:
```
build/MSU1PrepStudio_artefacts/Release/MSU1PrepStudio.exe
```

### macOS

After building, the app bundle will be in:
```
build/MSU1PrepStudio_artefacts/Release/MSU1PrepStudio.app
```

### Linux

After building, the executable will be in:
```
build/MSU1PrepStudio_artefacts/Release/MSU1PrepStudio
```

## Usage

### Basic Workflow

1. **Open Audio File**: Click "Open Audio File..." and select your audio file
2. **Set Loop Points**: Click and drag on the waveform to set loop start and end
3. **Normalize** (Optional): Use "Normalize..." to match RMS levels
4. **Export**: Click "Export PCM" and specify track number and destination

### Keyboard Shortcuts

- **Space**: Play/Pause
- **Escape**: Stop
- **Ctrl+O**: Open file
- **Ctrl+E**: Export
- **Mouse Wheel**: Zoom in/out on waveform

### MSU-1 Format

Exported PCM files include:
- Bytes 0-3: "MSU1" (ASCII header)
- Bytes 4-7: Loop start position (32-bit little-endian)
- Bytes 8+: Interleaved stereo 16-bit PCM data (little-endian)

## Project Structure

```
MSU1PrepStudio/
├── Source/
│   ├── Main.cpp
│   ├── MainComponent.h/cpp
│   ├── Core/
│   │   ├── MSUProjectState.h/cpp
│   │   └── AudioFileHandler.h/cpp
│   ├── Audio/
│   │   ├── AudioImporter.h/cpp
│   │   ├── AudioPlayer.h/cpp
│   │   └── NormalizationAnalyzer.h/cpp
│   ├── Export/
│   │   ├── MSU1Exporter.h/cpp
│   │   └── ManifestHandler.h/cpp
│   ├── UI/
│   │   ├── WaveformView.h/cpp
│   │   ├── TransportControls.h/cpp
│   │   ├── ToolbarPanel.h/cpp
│   │   ├── LoopMarker.h/cpp
│   │   └── CustomLookAndFeel.h/cpp
│   └── Dialogs/
│       ├── NormalizationDialog.h/cpp
│       └── ExportDialog.h/cpp
├── CMakeLists.txt
└── README.md
```

## Development Roadmap

See `MSU1_Prep_Studio_Roadmap.md` for detailed development milestones.

## Technical Specification

See `MSU1_Prep_Studio_Guide.md` for complete technical details.

## License

MIT License - See LICENSE file for details

## Contributing

Contributions are welcome! Please feel free to submit pull requests or open issues for bugs and feature requests.

## Acknowledgments

- Built with [JUCE Framework](https://juce.com/)
- MSU-1 specification by byuu
- Inspired by the SNES homebrew and ROM hacking community

## Support

For questions or support, please open an issue on GitHub.
