# MSU-1 Prep Studio User Guide

_Last updated: November 2025_

> MSU-1 Prep Studio (EMESSYOO) is a JUCE-based desktop workstation for preparing audio destined for MSU-1 ROM hacks. This guide walks through installation, the UI, day-to-day workflows, and troubleshooting so you can go from a fresh install to batch-ready PCM exports with metadata backups.

## Table of Contents
1. [System Requirements & Installation](#system-requirements--installation)
2. [First Launch Checklist](#first-launch-checklist)
3. [Interface Tour](#interface-tour)
4. [Working with Audio](#working-with-audio)
5. [Managing MSU Projects](#managing-msu-projects)
6. [Audio Level Studio](#audio-level-studio)
7. [Exporting PCM Files](#exporting-pcm-files)
8. [Batch Processing](#batch-processing)
9. [Backups & Metadata Safety](#backups--metadata-safety)
10. [Settings Reference](#settings-reference)
11. [Keyboard Shortcuts](#keyboard-shortcuts)
12. [Troubleshooting & Tips](#troubleshooting--tips)
13. [Further Reading](#further-reading)

---

## System Requirements & Installation

| Platform | Minimum Requirements | Notes |
| --- | --- | --- |
| Windows | Windows 10+, Visual Studio 2019+ build tools, CMake 3.22+, 64-bit CPU | `setup.bat` automates JUCE + build folder creation. |
| macOS | macOS 11+, Xcode 12+, CMake 3.22+ | Run `chmod +x setup.sh && ./setup.sh`. |
| Linux | Modern distro with GCC 9+/Clang 10+, ALSA/JACK configured, CMake 3.22+ | Use the same setup script or follow manual steps in `QUICKSTART.md`. |

### Install Steps (all platforms)
1. Clone the repository and enter the folder:
   ```bash
   git clone https://github.com/yourusername/MSU1PrepStudio.git
   cd MSU1PrepStudio
   ```
2. Run the platform setup script **OR** manually clone JUCE 8.x into `./JUCE`.
3. Generate your preferred build files (Visual Studio, Xcode, Ninja, Makefiles) via CMake.
4. Build using `cmake --build <build-folder> --config Release`. The executable/app bundle lands inside `<build>/MSU1PrepStudio_artefacts/<Config>/`.

> _Tip:_ Keep the `build-vs`, `build-ninja`, and `build` folders around so you can swap generators without reconfiguring JUCE.

---

## First Launch Checklist

1. **Audio Device Permissions** – The app opens an audio device on startup. Grant permissions if prompted by the OS.
2. **Connect JUCE** – Ensure the `JUCE` subfolder exists next to the executable; otherwise the app cannot load.
3. **Open an MSU Project** – Use the Audio Level Studio (ALS) tab’s *Load MSU-1* button or the MSU File Browser to pick a `.msu` manifest so track metadata and backup folders are detected.
4. **Prepare the Backup Folder** – Each MSU directory should contain a `Backup/` subfolder. The app will create it automatically once backups are enabled, but pre-creating it keeps version control happy.

---

## Interface Tour

The main window is split into three areas: the toolbar, tabs, and the status bar.

### Toolbar (top)
- **Open Audio File…** – Imports MP3/WAV/FLAC/OGG/AIFF into the loop editor.
- **Export PCM** – Starts the single-track export flow (with loop/preset options).
- **Restore Backups** – Opens the restore dialog for files in the current project’s `Backup` folder.
- **Settings** – Currently houses the backup toggle (details below).
- **Title + Sample Rate Label** – Quick glance reminder that output is fixed at 44.1 kHz, 16-bit stereo.

### Tabs
1. **Loop Editor** – Waveform editing, transport controls, and the MSU File Browser live here.
2. **Audio Level Studio** – Loudness presets, manual RMS/peak overrides, previewing, and batch processing.

Switch tabs via the bar below the toolbar. The app auto-stops preview/batch playback when you leave a tab.

### Status Bar (bottom)
- Shows contextual messages (e.g., “Loaded: track name”, “Original PCM backups enabled”, “No loop data has been set…”).

---

## Working with Audio

### Importing
1. Click **Open Audio File…** and choose a source file. Supported formats: MP3, WAV, FLAC, OGG, AIFF, raw PCM.
2. The importer converts to 44.1 kHz / 16-bit stereo, detects loop markers from PCM headers when present, and fills the waveform view.

### Loop Editing & Navigation
- Drag the **Loop Start** (left handle) and **Loop End** (right handle) markers in the waveform.
- Use the mouse wheel to zoom horizontally; drag the scrollbar to pan.
- Hotkeys: `Z`/`X` jump loop markers to the cursor, and the **spacebar** toggles Play/Pause.
- The loop editor warns you if you try to export while loop start remains at 0 and loop end equals the file length (“No loop data has been set…”). Confirm to proceed or adjust markers.

### Transport Controls
- **Play / Pause / Stop** – Standard transport.
- **Loop** – Keeps playback within loop markers.
- **Auto-Scroll** – Centers the playhead as playback moves.
- **Auto Trim/Pad** – Trims leading silence, applies the configured pad (
10–5000 ms) before re-rendering.
- **Trim (No Pad)** – Removes silence without inserting the pad.
- **Pad Amount Slider** – Sets the padding value used by Auto Trim/Pad.

### Waveform Overlays & Legends
- The Loop Editor displays the hotkey legend; ALS shows an overlay comparing “Before” (green) and “After/Preview” (aqua).
- ALS only shows a playback cursor while the **Play Before** or **Play After** buttons are actively running, so the overlay stays uncluttered when idle.

### Preview Options
- **Loop Editor Playback** – Uses the global transport.
- **Track Preview (MSU File Browser)** – Each track row has a *Preview* button for auditioning the PCM currently on disk.
- **Before/After Preview (ALS)** – Dedicated buttons play the original buffer or the processed signal with the active preset/manual settings applied.

---

## Managing MSU Projects

### Loading a Manifest
- From ALS **Batch MSU Processing** or the Loop Editor’s browser, click **Load MSU-1** and select a `.msu` file. The app parses related `.pcm` files (`<basename>-NN.pcm`), populates the track list, and remembers the directory for future sessions.

### MSU File Browser Columns
| Column | Meaning |
| --- | --- |
| Track | Track number inferred from filename or manifest. |
| Title / File Name | Prefers explicit filenames; falls back to manifest title. |
| Status | `Found` (green) if the PCM exists, `Missing` (orange) otherwise. |
| Backup Exists | Displays “Yes” when `Backup/<file>.pcm` is present. |
| Preview | Toggles playback of the existing PCM. |
| Action | **Replace** button primes the export pipeline for that track. |

### Replacing a Track
1. Click **Replace** on the desired row to set it as the export target.
2. Switch to the Loop Editor to fine-tune audio and loop markers.
3. When ready, hit **Export PCM** and choose how to process (Loop + Preset, Loop Only, or Preset Only).
4. If backups are enabled, the original PCM and metadata snapshot land in `<MSU directory>/Backup/` automatically.

### Track List Refresh
- The app reloads and repaints the table whenever you:
  - Export or restore a track (with backups enabled).
  - Use the **Restore Backups** flow.
  - Toggle batch operations that touch on-disk files.
- You no longer need to click Preview to force updates.

---

## Audio Level Studio

ALS is a focused workspace for loudness matching and batch prep.

### Preset Selector
| Preset | Target | Notes |
| --- | --- | --- |
| Authentic | −20 RMS / −23 LUFS | Matches many original SNES mixes. |
| Balanced | −18 RMS / −21 LUFS | Default compromise. |
| Quieter | −23 RMS / −26 LUFS | Useful for ambience/background. |
| Louder | −16 RMS / −19 LUFS | Pushes forward-facing content. |
| Maximum | Peak −1.0 dBFS | Peak-capped, ignores RMS target. |
| Manual | Enabled automatically when manual overrides are active. |

### Metrics & Manual Controls
- **Metrics Group** – Displays measured RMS, LUFS estimate, Peak, and Headroom.
- **Manual RMS Target** – Toggle + slider to set a custom goal (display updates inline).
- **Manual Peak Ceiling** – Toggle + slider for peak limiting.
- **Advanced Help Label** – Summarizes which overrides are active.

### Waveform Overlay
- Shows Before (green) vs After/Preview (aqua) thumbnails. Use **Play Before/After** to audition with (or without) the preset.
- The message “Preview playback has preset/manual settings applied” reminds you that ALS preview always reflects your loudness choices.

### Batch MSU Processing
1. **Load MSU-1** – Same as the main browser, but scoped to ALS.
2. **Track Table** – Reorders/filters aren’t yet implemented, but you can multi-select rows.
3. **Preview** – Previews a processed version (After) alongside the raw PCM (Before).
4. **Batch Export w/ Preset/Manual Settings Applied** – Runs the export pipeline for all selected rows, baking in loop data and the active loudness preset/manual overrides.
5. **Progress Bar & Cancel** – A modal progress dialog appears; cancel gracefully ends the batch.
6. After export, backups refresh (if enabled) and manifests can be updated per track via the standard prompt.

---

## Exporting PCM Files

### Single-Track Export Flow
1. Click **Export PCM** (toolbar).
2. If loop start/end are still at default values, you’ll see a guard dialog. Confirm to proceed or adjust loops.
3. Choose one of the following in the **Select Export Options** dialog:
   - **Loop + Preset** – Applies both loop markers and the active ALS preset/manual gain.
   - **Loop Only** – Writes loop data/padding but leaves gain at unity.
   - **Preset Only** – Applies loudness preset/manual gain without modifying loop points.
4. If the export targets a specific MSU track (from the Replace workflow), the app:
   - Backs up the previous PCM (and metadata snapshot) when backups are enabled.
   - Asks whether to update any matching `.bml`/`.msu` manifests with the new loop sample index.
5. Status messages summarize success, and the MSU browser refreshes immediately.

### Manifest Updates
- When the exporter detects `.msu`/`.bml` files referencing the same basename, it lists them and offers to update loop metadata.
- Accepting creates/updates `metadata_backup.json` (if backups are on) before editing each manifest.

---

## Batch Processing

1. Navigate to the **Audio Level Studio → Batch MSU Processing** section.
2. **Load MSU-1** and wait for the table to populate.
3. Select rows (use Shift/Ctrl for ranges) and click **Batch export w/ Preset/Manual Settings applied.**
4. Confirm backups if prompted; the batch engine will:
   - Apply trim/pad rules mirrored from the Loop Editor.
   - Use the ALS preset/manual overrides at the moment you launched the batch.
   - Create backups per track (when enabled) and update metadata snapshots.
5. Watch the progress dialog; cancel if needed. Upon completion, check the log summary for failures.
6. The MSU File Browser refreshes, and ALS shows updated backup flags.

> _Note:_ Batch previewing a row locks the ALS player until you stop it, preventing accidental overlaps with single-track preview.

---

## Backups & Metadata Safety

### What Gets Backed Up?
- **PCM File** – Copied to `<MSU directory>/Backup/<track>.pcm`.
- **Metadata Snapshot** – Stored in `<MSU directory>/Backup/metadata_backup.json` with track presence, loop flags, and per-manifest loop indices.

### Enabling / Disabling Backups
1. Click **Settings** on the toolbar.
2. Toggle **Back up original PCM files** (on by default).
3. The dialog reminds you: “When enabled, each replaced PCM and its metadata are copied into the MSU-1 directory’s \Backup folder. We also maintain metadata_backup.json in that folder so the Restore Backups option can put everything back.”

### Restoring Backups
1. Click **Restore Backups** on the toolbar.
2. In the dialog, select one or more rows. Each shows file size and whether a metadata snapshot exists.
3. Click **Restore Selected**, review the confirmation prompt, and click **Restore**.
4. The app copies the backup PCM back to the project, restores any manifest metadata, deletes the backup copy (to prevent drift), and refreshes the MSU table automatically.

### Manual Cleanup
- Restored backups are removed from `Backup/`. If you want to keep an archive, copy files elsewhere before restoring.

---

## Settings Reference

| Setting | Description |
| --- | --- |
| **Back up original PCM files** | Global toggle stored in `AppData/Roaming/MSU1PrepStudio/MSU1PrepStudio.settings`. When on, every track replacement or batch export saves the original PCM plus metadata to the project’s `Backup` folder so you can restore later. |

Future versions may add more settings; the dialog will expand accordingly.

---

## Keyboard Shortcuts

| Key / Gesture | Context | Action |
| --- | --- | --- |
| Space | Loop Editor | Play/Pause transport. |
| Z / X | Loop Editor | Jump loop start / loop end to cursor. |
| Mouse wheel | Waveform | Zoom horizontally. Hold Shift for finer steps (OS-dependent). |
| Ctrl/Cmd + O | Global | Open audio file dialog. |
| Ctrl/Cmd + E | Global | Open Export dialog. |
| Escape | ALS / Preview dialogs | Stops playback or closes modal dialogs. |

(The hotkey legend inside the waveform view gives contextual reminders.)

---

## Troubleshooting & Tips

### No Audio Playback
- Verify your OS granted microphone/audio-output permissions.
- Open **Settings → Sound** (OS) to confirm the default output device isn’t exclusively locked by another DAW.
- Check the transport status bar; if stuck, hit **Stop** then **Play** again.

### “No Loop Data Has Been Set” Warning
- This appears when loop start is at 0 **and** loop end equals the full file length. Set intentional loop points or click **Yes** to continue with a full-length loop.

### Backups Not Appearing
- Ensure backups are enabled in settings.
- Confirm you’re exporting into an existing MSU directory (backups only occur when replacing a file, not when exporting standalone PCM outside the project).
- Look inside `<MSU>/Backup/` for the PCM copy and `metadata_backup.json`.

### Restore Dialog Does Nothing
- The app now runs the confirmation asynchronously; if you don’t see results, ensure you clicked **Restore** (not **Cancel**) in the second prompt. A completion dialog will list successes/failures.

### Batch Export Seems Stuck
- Watch the status label inside ALS for “Batch export cancelled/complete.”
- The progress dialog can be hidden behind other windows; check the taskbar/dock.
- Cancelling requests can take a few seconds while the current file finalizes.

### Manifest Not Updating
- Confirm the exported PCM shares the same base name as the `.msu/.bml` manifests.
- If you skipped the prompt once, re-run the export or manually run **Restore Backups** → re-export to trigger it again.

### Linking With Emulators
- After exporting, copy the PCM files and `.msu/.bml` manifests to your SD2SNES/FXPak or emulator directory. Manifest loop data is set in samples, so double-check by auditioning in-game.

---

## Further Reading
- `README.md` – Project overview and build instructions.
- `QUICKSTART.md` – Fast setup plus architecture notes.
- `MSU1_Prep_Studio_Guide.md` – Deep technical specification of the processing pipeline.
- `MSU1_Prep_Studio_Roadmap.md` – Upcoming features and milestones.

## Credits
Created and maintained by Scott McKay (BelmontLegon). Follow releases at [github.com/belmontlegion/EMESSYOO](https://github.com/belmontlegion/EMESSYOO/).

For bug reports or feature requests, open an issue on GitHub. Enjoy crafting polished MSU-1 soundtracks!
