# MSU-1 Prep Studio - Quick Start Guide

## Installation & Setup

### Option 1: Automated Setup (Recommended)

**Windows:**
```cmd
setup.bat
```

**macOS/Linux:**
```bash
chmod +x setup.sh
./setup.sh
```

This will automatically download JUCE and prepare the build environment.

### Option 2: Manual Setup

1. Clone or download JUCE 8.0+ into the project directory:
```bash
git clone https://github.com/juce-framework/JUCE.git
```

2. Create build directory:
```bash
mkdir build
cd build
```

3. Generate build files:
```bash
cmake ..
```

4. Build the project:
```bash
cmake --build . --config Release
```

## Project Overview

### Core Components

#### Audio Pipeline
- **AudioImporter**: Converts any audio format to MSU-1 specs (44.1kHz, 16-bit, stereo)
- **AudioPlayer**: Handles playback with loop support
- **NormalizationAnalyzer**: Analyzes and normalizes audio levels

#### Export System
- **MSU1Exporter**: Creates properly formatted .pcm files with MSU-1 header
- **ManifestHandler**: Manages .msu manifest files and track naming

#### User Interface
- **WaveformView**: Visual waveform display with loop marker editing
- **TransportControls**: Playback controls (play, pause, stop, loop)
- **ToolbarPanel**: File operations and export controls
- **CustomLookAndFeel**: Dark theme with green accents

### File Structure
```
MSU1PrepStudio/
├── Source/
│   ├── Core/          # Project state and file handling
│   ├── Audio/         # Audio processing and playback
│   ├── Export/        # MSU-1 export and manifest handling
│   ├── UI/            # User interface components
│   └── Dialogs/       # Modal dialogs
├── CMakeLists.txt     # Build configuration
└── README.md          # Documentation
```

## Development Notes

### Key Classes

1. **MSUProjectState** (`Core/MSUProjectState.h`)
   - Central state management
   - Stores audio buffer, loop points, and project metadata
   - Notifies listeners of changes via ChangeBroadcaster

2. **AudioImporter** (`Audio/AudioImporter.h`)
   - Imports various audio formats
   - Automatic conversion to MSU-1 requirements
   - DC offset removal and normalization

3. **MSU1Exporter** (`Export/MSU1Exporter.h`)
   - Exports audio as MSU-1 PCM format
   - Writes proper MSU-1 header with loop points
   - Creates backup files

4. **WaveformView** (`UI/WaveformView.h`)
   - Uses JUCE AudioThumbnail for waveform rendering
   - Supports zoom and scroll
   - Interactive loop marker editing

### MSU-1 Format Specification

**Header Structure:**
```
Offset  Size  Description
0x0000  4     "MSU1" (ASCII, 0x4D 0x53 0x55 0x31)
0x0004  4     Loop point (32-bit little-endian, in samples)
0x0008  ...   PCM data (16-bit signed little-endian, stereo interleaved)
```

**Audio Requirements:**
- Sample Rate: 44,100 Hz
- Bit Depth: 16-bit
- Channels: 2 (Stereo)
- Byte Order: Little-endian
- Format: Signed PCM

### Building for Different Platforms

#### Windows (Visual Studio)
```cmd
cd build
cmake .. -G "Visual Studio 17 2022"
cmake --build . --config Release
```

#### macOS (Xcode)
```bash
cd build
cmake .. -G Xcode
cmake --build . --config Release
```

#### Linux (Make)
```bash
cd build
cmake .. -G "Unix Makefiles"
cmake --build . --config Release
```

## Next Steps

### To implement full functionality:

1. **Connect UI callbacks** in MainComponent
   - Wire up file open dialog
   - Connect export functionality
   - Link normalization dialog

2. **Audio device setup**
   - Initialize audio device manager in MainComponent
   - Connect AudioPlayer to audio output

3. **Add file I/O**
   - Implement audio file chooser
   - Add MSU manifest file picker
   - Create export destination selector

4. **Testing**
   - Test with various audio formats
   - Validate exported PCM files with emulators
   - Test loop playback functionality

### Recommended Development Order

1. Test basic audio import and playback
2. Verify waveform visualization
3. Test loop marker editing
4. Implement and test normalization
5. Test PCM export with various files
6. Validate with actual MSU-1 hardware/emulators

## Resources

- **JUCE Documentation**: https://docs.juce.com/
- **JUCE Tutorials**: https://juce.com/learn/tutorials
- **MSU-1 Specification**: https://github.com/msu1/msu1-specs
- **SD2SNES/FXPak Pro**: https://sd2snes.de/

## Troubleshooting

### Build Errors

**"JUCE not found"**
- Ensure JUCE directory exists in project root
- Run setup script or clone JUCE manually

**CMake version error**
- Upgrade CMake to 3.22 or higher

**Compiler errors**
- Ensure C++17 compatible compiler is installed
- Windows: Visual Studio 2019 or later
- macOS: Xcode 12 or later
- Linux: GCC 9+ or Clang 10+

### Runtime Issues

**No audio playback**
- Check audio device permissions
- Verify audio device manager initialization
- Ensure audio callback is properly connected

**Waveform not displaying**
- Check that audio buffer is properly loaded
- Verify AudioThumbnail initialization
- Ensure thumbnail cache is created

## Credits

Created by Scott McKay (BelmontLegon). Follow project updates at [github.com/belmontlegion/EMESSYOO](https://github.com/belmontlegion/EMESSYOO/).

## Contact & Support

For issues or questions:
- Open an issue on GitHub
- Check documentation in `MSU1_Prep_Studio_Guide.md`
- Review roadmap in `MSU1_Prep_Studio_Roadmap.md`
