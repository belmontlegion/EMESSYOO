#include "MainComponent.h"
#include "Audio/NormalizationAnalyzer.h"
#include "Core/BackupMetadataStore.h"
#include "Dialogs/BackupRestoreDialog.h"

#include <cmath>
#include <memory>

namespace
{
    constexpr auto audioDirectoryKey = "lastAudioDirectory";
    constexpr auto msuDirectoryKey = "lastMSUDirectory";
    constexpr auto backupOriginalsKey = "backupOriginalPCM";

    juce::PropertiesFile::Options createSettingsOptions()
    {
        juce::PropertiesFile::Options options;
        options.applicationName = "MSU1PrepStudio";
        options.filenameSuffix = "settings";
        options.folderName = "MSU1PrepStudio";
        options.osxLibrarySubFolder = "Application Support";
        options.storageFormat = juce::PropertiesFile::storeAsXML;
        return options;
    }
}

//==============================================================================
MainComponent::MainComponent()
        : toolbar(projectState),
            mainTabs(juce::TabbedButtonBar::TabsAtTop),
            waveformView(projectState),
            transportControls(projectState, audioPlayer),
            loopEditorTab(waveformView, transportControls, msuFileBrowser, kBrowserHeight, kTransportHeight),
            audioLevelStudio(projectState, beforeAfterPreviewPlayer)
{
    // Set custom look and feel
    setLookAndFeel(&customLookAndFeel);
    settings = std::make_unique<juce::PropertiesFile>(createSettingsOptions());
    
    // Initialize audio device with default settings (will match source when file is loaded)
    audioDeviceManager.initialiseWithDefaultDevices(0, 2);
    previewPlayer.setAudioDeviceManager(&audioDeviceManager);
    audioDeviceManager.addAudioCallback(&audioPlayer);
    audioDeviceManager.addAudioCallback(&previewPlayer);
    audioDeviceManager.addAudioCallback(&beforeAfterPreviewPlayer);
    audioPlayer.setProjectState(&projectState);
    
    // Wire up toolbar callbacks
    toolbar.onOpenFile = [this] { openAudioFile(); };
    toolbar.onExport = [this] { exportPCM(); };
    toolbar.onRestoreBackups = [this] { showRestoreBackupsDialog(); };
    toolbar.onOpenSettings = [this] { showSettingsDialog(); };
    
    // Wire up waveform callbacks
    waveformView.onPositionClicked = [this](double seconds) {
        double targetSeconds = seconds;
        if (projectState.hasAudio() && projectState.getPaddingSamples() > 0 && projectState.getSampleRate() > 0.0)
        {
            double paddingSeconds = static_cast<double>(projectState.getPaddingSamples()) / projectState.getSampleRate();
            targetSeconds += paddingSeconds;
        }
        audioPlayer.setPosition(targetSeconds);
    };
    
    // Add components
    addAndMakeVisible(toolbar);
    addAndMakeVisible(statusLabel);
    addAndMakeVisible(mainTabs);
    mainTabs.setOutline(0);
    mainTabs.addTab("Loop Editor", juce::Colours::transparentBlack, &loopEditorTab, false);
    mainTabs.addTab("Audio Level Studio", juce::Colours::transparentBlack, &audioLevelStudio, false);
    loopEditorTabIndex = 0;
    audioLevelStudioTabIndex = mainTabs.getNumTabs() - 1;
    lastMainTabIndex = mainTabs.getCurrentTabIndex();
    
    // Configure status label
    statusLabel.setText("Ready", juce::dontSendNotification);
    statusLabel.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
    statusLabel.setJustificationType(juce::Justification::centredLeft);
    
    // Setup MSU file browser callback
    msuFileBrowser.onReplaceTrack = [this](const MSUFileBrowser::TrackInfo& track) {
        handleReplaceTrack(track);
    };
    
    msuFileBrowser.onPreviewTrack = [this](const MSUFileBrowser::TrackInfo& track) {
        handlePreviewTrack(track);
    };
    
    msuFileBrowser.onStopPreview = [this]() {
        handleStopPreview();
    };

    msuFileBrowser.onDirectoryChanged = [this](const juce::File& directory)
    {
        saveLastMSUDirectory(directory);
    };

    msuFileBrowser.onTracksLoaded = [this](const juce::File& msuFile,
                                            const juce::String& gameTitle,
                                            const std::vector<MSUFileBrowser::TrackInfo>& tracks)
    {
        audioLevelStudio.setMSUContext(msuFile, gameTitle, tracks);
    };

    msuFileBrowser.onTracksCleared = [this]()
    {
        audioLevelStudio.clearMSUContext();
    };

    audioLevelStudio.setMSULoadCallback([this]
    {
        msuFileBrowser.launchLoadDialog();
    });
    audioLevelStudio.setBackupPreference(backupOriginalsEnabled);
    audioLevelStudio.setTrackReplacementCallback([this](const MSUFileBrowser::TrackInfo& track)
    {
        handleReplaceTrack(track);
    });

    audioLevelStudio.setTrackListRefreshCallback([this]
    {
        refreshTrackListIfBackupsEnabled();
    });

    audioLevelStudio.setPlaybackStopper([this]
    {
        if (previewPlayer.isPlaying())
            handleStopPreview();

        if (audioPlayer.isPlaying())
            audioPlayer.stop();
    });

    loadPersistedDirectories();
    if (msuFileBrowser.getCurrentMSUFile().existsAsFile() && !msuFileBrowser.getTracks().empty())
    {
        audioLevelStudio.setMSUContext(msuFileBrowser.getCurrentMSUFile(),
                                       msuFileBrowser.getGameTitle(),
                                       msuFileBrowser.getTracks());
    }
    audioLevelStudio.refreshFromProjectState();
    
    // Listen to project state changes
    projectState.addChangeListener(this);
    
    // Enable keyboard input
    setWantsKeyboardFocus(true);
    
    // Start timer for playback updates
    startTimer(50);
    
    // Set initial size large enough to keep every control visible
    setSize(getPreferredWindowWidth(), getPreferredWindowHeight());
}

MainComponent::~MainComponent()
{
    stopTimer();
    audioDeviceManager.removeAudioCallback(&beforeAfterPreviewPlayer);
    audioDeviceManager.removeAudioCallback(&previewPlayer);
    audioDeviceManager.removeAudioCallback(&audioPlayer);
    setLookAndFeel(nullptr);
    projectState.removeChangeListener(this);
    if (settings != nullptr)
        settings->saveIfNeeded();
}

//==============================================================================
void MainComponent::paint(juce::Graphics& g)
{
    g.fillAll(getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId));
}

void MainComponent::resized()
{
    auto bounds = getLocalBounds();
    
    // Top toolbar
    toolbar.setBounds(bounds.removeFromTop(kToolbarHeight));
    
    // Bottom status bar
    auto statusBounds = bounds.removeFromBottom(kStatusBarHeight);
    statusLabel.setBounds(statusBounds.reduced(8, 4));

    // Tab content fills remaining space
    mainTabs.setBounds(bounds);
}

int MainComponent::getMinimumWindowWidth() const
{
    return juce::jmax(kDefaultMinimumWidth, toolbar.getMinimumWidth());
}

int MainComponent::getPreferredWindowWidth() const
{
    return juce::jmax(kPreferredWidth, getMinimumWindowWidth());
}

void MainComponent::changeListenerCallback(juce::ChangeBroadcaster* source)
{
    if (source == &projectState)
    {
        audioLevelStudio.refreshFromProjectState();
        // Update UI based on project state changes
        repaint();
    }
}

void MainComponent::timerCallback()
{
    const int currentTabIndex = mainTabs.getCurrentTabIndex();
    if (currentTabIndex != lastMainTabIndex)
    {
        if (audioLevelStudioTabIndex >= 0 && lastMainTabIndex == audioLevelStudioTabIndex)
            audioLevelStudio.stopPreviewPlayback();

        if (loopEditorTabIndex >= 0 && lastMainTabIndex == loopEditorTabIndex)
        {
            if (audioPlayer.isPlaying())
                audioPlayer.stop();

            if (previewPlayer.isPlaying() || msuFileBrowser.getPreviewingRow() >= 0)
                handleStopPreview();
        }

        lastMainTabIndex = currentTabIndex;
    }

    // Update waveform with current playback position
    if (projectState.hasAudio())
    {
        waveformView.setAutoScrollEnabled(transportControls.isAutoScrollEnabled());
        waveformView.setPlayPosition(audioPlayer.getPosition());
    }
    
    // Check preview playback state
    checkPreviewState();
}

bool MainComponent::keyPressed(const juce::KeyPress& key)
{
    // Spacebar toggles play/pause
    if (key.isKeyCode(juce::KeyPress::spaceKey))
    {
        if (loopEditorTabIndex >= 0 && mainTabs.getCurrentTabIndex() == loopEditorTabIndex)
        {
            if (audioPlayer.isPlaying())
                audioPlayer.pause();
            else
                audioPlayer.play();
            return true;
        }
        return false;
    }
    
    // Pass Z/X keys to waveform view for loop point hotkeys
    if (key.getKeyCode() == 'Z' || key.getKeyCode() == 'z' ||
        key.getKeyCode() == 'X' || key.getKeyCode() == 'x')
    {
        return waveformView.keyPressed(key);
    }
    
    return false;
}

void MainComponent::openAudioFile()
{
    const auto initialAudioDir = lastAudioDirectory.getFullPathName().isNotEmpty()
        ? lastAudioDirectory
        : juce::File();

    fileChooser = std::make_unique<juce::FileChooser>(
        "Select an audio file to open...",
        initialAudioDir,
        "*.mp3;*.wav;*.flac;*.ogg;*.aiff;*.pcm");
    
    auto chooserFlags = juce::FileBrowserComponent::openMode | 
                        juce::FileBrowserComponent::canSelectFiles;
    
    fileChooser->launchAsync(chooserFlags, [this](const juce::FileChooser& chooser)
    {
        auto file = chooser.getResult();
        
            if (file.existsAsFile())
        {
            // Remember this directory for next time
            saveLastAudioDirectory(file.getParentDirectory());
            
            updateStatus("Loading: " + file.getFileName());
            
            // Import audio file
            AudioImporter importer;
            juce::AudioBuffer<float> buffer;
            
            if (importer.importAudioFile(file, buffer, true, false))
            {
                // Store the ORIGINAL sample rate and audio (no conversion yet)
                double originalSampleRate = 0.0;
                AudioFileHandler fileHandler;
                
                // Get original sample rate from file
                int numChannels;
                int64 lengthInSamples;
                if (fileHandler.getAudioFileInfo(file, originalSampleRate, numChannels, lengthInSamples))
                {
                    // Set default loop points
                    int64 loopStart = 0;
                    int64 loopEnd = buffer.getNumSamples();
                    
                    // For PCM files, get loop point from file header
                    int64 pcmLoopPoint = 0;
                    int64* loopPointPtr = file.getFileExtension().toLowerCase() == ".pcm" ? &pcmLoopPoint : nullptr;
                    
                    // Reload without conversion
                    if (fileHandler.loadAudioFile(file, buffer, originalSampleRate, loopPointPtr))
                    {
                        projectState.setAudioBuffer(buffer, originalSampleRate);
                        projectState.setSourceFile(file);
                        
                        loopEnd = buffer.getNumSamples();
                        
                        // Use loop point from PCM file if available
                        if (loopPointPtr != nullptr && pcmLoopPoint > 0 && pcmLoopPoint < loopEnd)
                        {
                            loopStart = pcmLoopPoint;
                        }
                        
                        projectState.setLoopStart(loopStart);
                        projectState.setLoopEnd(loopEnd);
                        
                        // Apply auto trim/pad if enabled
                        if (transportControls.isAutoTrimPadEnabled())
                            transportControls.applyAutoTrimPad();
                        else if (transportControls.isTrimNoPadEnabled())
                            transportControls.applyTrimNoPad();

                        audioLevelStudio.refreshFromProjectState();
                        
                        // Don't change device sample rate - let AudioPlayer handle resampling
                        DBG("Loaded audio at " + juce::String(originalSampleRate) + " Hz");
                        
                        juce::String statusMsg = "Loaded: " + file.getFileName() + 
                                   " (" + juce::String(originalSampleRate, 1) + " Hz, " +
                                   juce::String(buffer.getNumChannels()) + " ch, " +
                                   juce::String(buffer.getNumSamples()) + " samples)";
                        
                        if (loopPointPtr != nullptr && pcmLoopPoint > 0)
                        {
                            statusMsg += " [Loop at " + juce::String(pcmLoopPoint) + "]";
                        }
                        
                        updateStatus(statusMsg);
                    }
                }
            }
            else
            {
                updateStatus("Error: " + importer.getLastError());
                juce::AlertWindow::showMessageBoxAsync(
                    juce::MessageBoxIconType::WarningIcon,
                    "Import Error",
                    "Failed to import audio file:\n" + importer.getLastError());
            }
        }
    });
}

void MainComponent::exportPCM()
{
    if (!projectState.hasAudio())
    {
        juce::AlertWindow::showMessageBoxAsync(
            juce::MessageBoxIconType::WarningIcon,
            "No Audio",
            "Please load an audio file first.");
        return;
    }

    if (shouldWarnAboutMissingLoopData())
    {
        auto* warningWindow = new juce::AlertWindow(
            "No Loop Data",
            "No loop data has been set, are you sure you want to export without loop markers?",
            juce::MessageBoxIconType::WarningIcon);
        warningWindow->addButton("Yes", 1);
        warningWindow->addButton("Cancel", 0);
        warningWindow->centreAroundComponent(this, warningWindow->getWidth(), warningWindow->getHeight());

        auto safeThis = juce::Component::SafePointer<MainComponent>(this);
        warningWindow->enterModalState(true,
            juce::ModalCallbackFunction::create([safeThis, warningWindow](int result)
            {
                if (result == 1)
                {
                    if (auto* comp = safeThis.getComponent())
                        comp->beginManualExportFlow();
                }

                delete warningWindow;
            }),
            true);
        return;
    }

    beginManualExportFlow();
}

void MainComponent::beginManualExportFlow()
{
    if (!projectState.hasAudio())
        return;
    
    // Check if we have a target export file (from Replace workflow)
    if (projectState.hasTargetExportFile())
    {
        auto file = projectState.getTargetExportFile();

        auto performReplaceExport = [this, file](ExportProcessingOption option)
        {
            const bool applyLoopData = (option != ExportProcessingOption::PresetOnly);
            const bool applyPresetGain = (option != ExportProcessingOption::LoopOnly);
            const bool backupsEnabled = backupOriginalsEnabled;
            const bool willBackup = backupsEnabled && file.existsAsFile();
            const auto backupFile = file.getParentDirectory()
                                         .getChildFile("Backup")
                                         .getChildFile(file.getFileName());
            const bool backupAlreadyExists = willBackup && backupFile.existsAsFile();

            auto safeThis = juce::Component::SafePointer<MainComponent>(this);
            std::function<void()> executeExport = [safeThis,
                                                   file,
                                                   applyLoopData,
                                                   applyPresetGain,
                                                   backupsEnabled,
                                                   willBackup,
                                                   backupFile]() mutable
            {
                if (auto* comp = safeThis.getComponent())
                {
                    auto& projectState = comp->projectState;
                    AudioImporter importer;
                    auto exportBuffer = projectState.getAudioBuffer();

                    if (!importer.convertToMSU1Format(exportBuffer, projectState.getSampleRate()))
                    {
                        comp->updateStatus("Conversion failed: " + importer.getLastError());
                        return;
                    }

                    int64 exportLoopStart = -1;
                    int64 exportLoopEnd = 0;

                    if (applyLoopData)
                    {
                        const double sourceRate = projectState.getSampleRate();
                        const double ratio = (sourceRate > 0.0)
                            ? AudioImporter::MSU1_SAMPLE_RATE / sourceRate
                            : 1.0;

                        int64 exportTrimStart = projectState.hasTrimStart()
                            ? static_cast<int64>(projectState.getTrimStart() * ratio)
                            : 0;
                        exportLoopStart = static_cast<int64>(projectState.getLoopStart() * ratio);
                        exportLoopEnd = static_cast<int64>(projectState.getLoopEnd() * ratio);
                        int64 exportPaddingSamples = static_cast<int64>(projectState.getPaddingSamples() * ratio);

                        if (exportTrimStart > 0 && exportTrimStart < exportBuffer.getNumSamples())
                        {
                            int trimmedLength = exportBuffer.getNumSamples() - static_cast<int>(exportTrimStart);
                            juce::AudioBuffer<float> trimmedBuffer(exportBuffer.getNumChannels(), trimmedLength);

                            for (int ch = 0; ch < exportBuffer.getNumChannels(); ++ch)
                                trimmedBuffer.copyFrom(ch, 0, exportBuffer, ch, static_cast<int>(exportTrimStart), trimmedLength);

                            exportBuffer = trimmedBuffer;
                            exportLoopStart -= exportTrimStart;
                            exportLoopEnd -= exportTrimStart;
                        }

                        if (exportPaddingSamples > 0)
                        {
                            int paddedLength = exportBuffer.getNumSamples() + static_cast<int>(exportPaddingSamples);
                            juce::AudioBuffer<float> paddedBuffer(exportBuffer.getNumChannels(), paddedLength);
                            paddedBuffer.clear();

                            for (int ch = 0; ch < exportBuffer.getNumChannels(); ++ch)
                                paddedBuffer.copyFrom(ch,
                                                      static_cast<int>(exportPaddingSamples),
                                                      exportBuffer,
                                                      ch,
                                                      0,
                                                      exportBuffer.getNumSamples());

                            exportBuffer = paddedBuffer;
                            exportLoopStart += exportPaddingSamples;
                            exportLoopEnd += exportPaddingSamples;
                        }

                        if (exportLoopEnd > 0 && exportLoopEnd < exportBuffer.getNumSamples())
                        {
                            juce::AudioBuffer<float> trimmedBuffer(exportBuffer.getNumChannels(), static_cast<int>(exportLoopEnd));
                            for (int ch = 0; ch < exportBuffer.getNumChannels(); ++ch)
                                trimmedBuffer.copyFrom(ch, 0, exportBuffer, ch, 0, static_cast<int>(exportLoopEnd));
                            exportBuffer = trimmedBuffer;
                        }
                    }

                    const float exportGainDb = applyPresetGain ? projectState.getNormalizationGain() : 0.0f;
                    if (applyPresetGain && std::isfinite(exportGainDb) && std::abs(exportGainDb) > 0.01f)
                        NormalizationAnalyzer::applyGain(exportBuffer, exportGainDb);

                    MSU1Exporter exporter;
                    comp->updateStatus("Exporting to " + file.getFileName() + "...");

                    if (exporter.exportPCM(file, exportBuffer, exportLoopStart, backupsEnabled))
                    {
                        comp->updateStatus("Exported: " + file.getFileName() + " (44.1kHz, 16-bit stereo)");

                        auto msuDir = file.getParentDirectory();
                        auto msuFiles = msuDir.findChildFiles(juce::File::findFiles, false, "*.msu");
                        if (msuFiles.size() > 0)
                            comp->msuFileBrowser.loadMSUFile(msuFiles[0]);

                        projectState.setTargetExportFile(juce::File());
                        comp->refreshTrackListIfBackupsEnabled();

                        juce::String metadataSummary;
                        if (applyLoopData)
                        {
                            auto manifests = MSUManifestUpdater::findRelatedManifestFiles(file);
                            auto manifestResult = comp->applyManifestUpdates(manifests,
                                                                              file,
                                                                              exportLoopStart,
                                                                              willBackup,
                                                                              backupFile.getParentDirectory());
                            if (manifestResult.attempted)
                                metadataSummary = "\n\nMetadata:\n" + manifestResult.summary;
                        }

                        juce::String alertBody = "Successfully replaced track:\n" + file.getFullPathName() +
                            "\n\nConverted to MSU-1 format: 44.1kHz, 16-bit stereo";
                        alertBody += applyLoopData
                            ? "\nLoop start: " + juce::String(exportLoopStart) + " samples"
                            : "\nLoop data not applied";
                        if (willBackup && backupFile.existsAsFile())
                            alertBody += "\nBackup saved to: " + backupFile.getFullPathName();

                        auto* callback = (willBackup && applyLoopData)
                            ? juce::ModalCallbackFunction::create([safeThis,
                                                                   file,
                                                                   exportLoopStart,
                                                                   willBackup,
                                                                   backupDir = backupFile.getParentDirectory()](int)
                              {
                                  if (auto* compInner = safeThis.getComponent())
                                      compInner->promptToUpdateManifests(file,
                                                                          exportLoopStart,
                                                                          willBackup,
                                                                          backupDir);
                              })
                            : nullptr;

                        juce::AlertWindow::showMessageBoxAsync(
                            juce::MessageBoxIconType::InfoIcon,
                            "Track Replaced",
                            alertBody + metadataSummary,
                            "OK",
                            nullptr,
                            callback);
                    }
                    else
                    {
                        comp->updateStatus("Export failed: " + exporter.getLastError());
                        juce::AlertWindow::showMessageBoxAsync(
                            juce::MessageBoxIconType::WarningIcon,
                            "Export Error",
                            "Failed to export track:\n" + exporter.getLastError());
                    }
                }
            };

            if (backupAlreadyExists)
            {
                juce::String overwriteMessage =
                    "A backup already exists for one or more files. Would you like to overwrite the backup?";
                overwriteMessage << "\n\nExisting backup: " << backupFile.getFullPathName();

                auto options = juce::MessageBoxOptions()
                    .withIconType(juce::MessageBoxIconType::WarningIcon)
                    .withTitle("Overwrite Backup?")
                    .withMessage(overwriteMessage)
                    .withButton("Yes")
                    .withButton("Cancel")
                    .withAssociatedComponent(this);

                juce::AlertWindow::showAsync(
                    options,
                    juce::ModalCallbackFunction::create([safeThis, executeExport](int result) mutable
                    {
                        if (result == 1)
                        {
                            executeExport();
                        }
                        else if (auto* comp = safeThis.getComponent())
                        {
                            comp->updateStatus("Export cancelled");
                        }
                    }));
                return;
            }

            executeExport();
        };

        promptExportProcessingOptions(
            [this, performReplaceExport](ExportProcessingOption option)
            {
                prepareExportForOption(option,
                                       [performReplaceExport](ExportProcessingOption preparedOption)
                                       {
                                           performReplaceExport(preparedOption);
                                       },
                                       nullptr);
            },
            [this]()
            {
                updateStatus("Export cancelled");
            });
        return;
    }
    
    // Normal export workflow (no target file)
    updateStatus("Choose export location...");
    
    // Open file dialog to choose destination
    auto exportFileChooser = std::make_shared<juce::FileChooser>(
        "Export MSU-1 PCM file as...",
        juce::File(),
        "*.pcm");

    exportFileChooser->launchAsync(juce::FileBrowserComponent::saveMode |
                              juce::FileBrowserComponent::canSelectFiles |
                              juce::FileBrowserComponent::warnAboutOverwriting,
        [this, exportFileChooser](const juce::FileChooser& chooser)
        {
            juce::ignoreUnused(exportFileChooser);
            auto file = chooser.getResult();

            if (file == juce::File())
            {
                updateStatus("Export cancelled");
                return;
            }

            // Ensure .pcm extension
            if (file.getFileExtension() != ".pcm")
                file = file.withFileExtension(".pcm");

            auto targetFile = file;

            auto proceedWithExport = [this, targetFile](ExportProcessingOption option) mutable
            {
                juce::File file = targetFile;
                const bool applyLoopData = (option != ExportProcessingOption::PresetOnly);
                const bool applyPresetGain = (option != ExportProcessingOption::LoopOnly);

                const bool willBackup = backupOriginalsEnabled && file.existsAsFile();
                const auto backupFile = file.getParentDirectory()
                                             .getChildFile("Backup")
                                             .getChildFile(file.getFileName());

                AudioImporter importer;
                auto exportBuffer = projectState.getAudioBuffer();

                if (!importer.convertToMSU1Format(exportBuffer, projectState.getSampleRate()))
                {
                    updateStatus("Conversion failed: " + importer.getLastError());
                    return;
                }

                int64 exportLoopStart = -1;
                int64 exportLoopEnd = 0;

                if (applyLoopData)
                {
                    const double sourceRate = projectState.getSampleRate();
                    const double ratio = (sourceRate > 0.0)
                        ? AudioImporter::MSU1_SAMPLE_RATE / sourceRate
                        : 1.0;

                    int64 exportTrimStart = projectState.hasTrimStart()
                        ? static_cast<int64>(projectState.getTrimStart() * ratio)
                        : 0;
                    exportLoopStart = static_cast<int64>(projectState.getLoopStart() * ratio);
                    exportLoopEnd = static_cast<int64>(projectState.getLoopEnd() * ratio);
                    int64 exportPaddingSamples = static_cast<int64>(projectState.getPaddingSamples() * ratio);

                    if (exportTrimStart > 0 && exportTrimStart < exportBuffer.getNumSamples())
                    {
                        int trimmedLength = exportBuffer.getNumSamples() - static_cast<int>(exportTrimStart);
                        juce::AudioBuffer<float> trimmedBuffer(exportBuffer.getNumChannels(), trimmedLength);

                        for (int ch = 0; ch < exportBuffer.getNumChannels(); ++ch)
                            trimmedBuffer.copyFrom(ch, 0, exportBuffer, ch, static_cast<int>(exportTrimStart), trimmedLength);

                        exportBuffer = trimmedBuffer;
                        exportLoopStart -= exportTrimStart;
                        exportLoopEnd -= exportTrimStart;
                    }

                    if (exportPaddingSamples > 0)
                    {
                        int paddedLength = exportBuffer.getNumSamples() + static_cast<int>(exportPaddingSamples);
                        juce::AudioBuffer<float> paddedBuffer(exportBuffer.getNumChannels(), paddedLength);
                        paddedBuffer.clear();

                        for (int ch = 0; ch < exportBuffer.getNumChannels(); ++ch)
                            paddedBuffer.copyFrom(ch,
                                                  static_cast<int>(exportPaddingSamples),
                                                  exportBuffer,
                                                  ch,
                                                  0,
                                                  exportBuffer.getNumSamples());

                        exportBuffer = paddedBuffer;
                        exportLoopStart += exportPaddingSamples;
                        exportLoopEnd += exportPaddingSamples;
                    }

                    if (exportLoopEnd > 0 && exportLoopEnd < exportBuffer.getNumSamples())
                    {
                        juce::AudioBuffer<float> trimmedBuffer(exportBuffer.getNumChannels(), static_cast<int>(exportLoopEnd));
                        for (int ch = 0; ch < exportBuffer.getNumChannels(); ++ch)
                            trimmedBuffer.copyFrom(ch, 0, exportBuffer, ch, 0, static_cast<int>(exportLoopEnd));
                        exportBuffer = trimmedBuffer;
                    }
                }

                const float exportGainDb = applyPresetGain ? projectState.getNormalizationGain() : 0.0f;
                if (applyPresetGain && std::isfinite(exportGainDb) && std::abs(exportGainDb) > 0.01f)
                    NormalizationAnalyzer::applyGain(exportBuffer, exportGainDb);

                MSU1Exporter exporter;
                updateStatus("Exporting to " + file.getFileName() + "...");

                if (exporter.exportPCM(file,
                                      exportBuffer,
                                      exportLoopStart,
                                      backupOriginalsEnabled))
                {
                    updateStatus("Exported: " + file.getFileName() + " (44.1kHz, 16-bit stereo)");
                    refreshTrackListIfBackupsEnabled();

                    juce::String successMessage = "PCM file exported successfully to:\n" + file.getFullPathName() +
                        "\n\nConverted to MSU-1 format: 44.1kHz, 16-bit stereo";
                    successMessage += applyLoopData
                        ? "\nLoop start: " + juce::String(exportLoopStart) + " samples" +
                          "\nTrimmed at: " + juce::String(exportLoopEnd) + " samples"
                        : "\nLoop data not applied";

                    if (willBackup && backupFile.existsAsFile())
                        successMessage += "\n\nBackup saved to: " + backupFile.getFullPathName();

                    auto* callback = (willBackup && applyLoopData)
                        ? juce::ModalCallbackFunction::create([this,
                                                               file,
                                                               exportLoopStart,
                                                               willBackup,
                                                               backupDir = backupFile.getParentDirectory()](int)
                          {
                              promptToUpdateManifests(file,
                                                      exportLoopStart,
                                                      willBackup,
                                                      backupDir);
                          })
                        : nullptr;

                    juce::AlertWindow::showMessageBoxAsync(
                        juce::MessageBoxIconType::InfoIcon,
                        "Export Success",
                        successMessage,
                        "OK",
                        nullptr,
                        callback);
                }
                else
                {
                    updateStatus("Export failed: " + exporter.getLastError());
                    juce::AlertWindow::showMessageBoxAsync(
                        juce::MessageBoxIconType::WarningIcon,
                        "Export Error",
                        "Failed to export PCM file:\n" + exporter.getLastError());
                }
            };

            promptExportProcessingOptions(
                [this, proceedWithExport = std::move(proceedWithExport)](ExportProcessingOption option) mutable
                {
                    prepareExportForOption(option,
                                           [proceedWithExport](ExportProcessingOption preparedOption) mutable
                                           {
                                               proceedWithExport(preparedOption);
                                           },
                                           nullptr);
                },
                [this]()
                {
                    updateStatus("Export cancelled");
                });
        });
}

void MainComponent::promptExportProcessingOptions(std::function<void(ExportProcessingOption)> onSelection,
                                                  std::function<void()> onCancel)
{
    if (!projectState.hasAudio())
    {
        if (onCancel)
            onCancel();
        return;
    }

    const juce::String presetLabel = audioLevelStudio.getActivePresetDisplayName();
    const juce::String loopPresetText = "Export With Loop Data and Audio Level Preset " + presetLabel + " applied";
    const juce::String loopOnlyText = "Export With Loop Data applied only";
    const juce::String presetOnlyText = "Export with Audio Level Preset " + presetLabel + " only applied";

    juce::String message = "Choose how this export should be processed. Loop data applies trim, padding, and loop markers.\n\n";
    constexpr const char* bulletPrefix = "- ";
    message << bulletPrefix << loopPresetText << "\n";
    message << bulletPrefix << loopOnlyText << "\n";
    message << bulletPrefix << presetOnlyText;

    auto* dialog = new juce::AlertWindow("Select Export Options",
                                         message,
                                         juce::MessageBoxIconType::QuestionIcon);
    dialog->addButton("Loop + Preset", 1);
    dialog->addButton("Loop Only", 2);
    dialog->addButton("Preset Only", 3);
    dialog->addButton("Cancel", 0);
    constexpr int expandedDialogWidth = 660;
    const int dialogHeight = dialog->getHeight();
    dialog->setSize(expandedDialogWidth, dialogHeight);
    dialog->centreAroundComponent(this, expandedDialogWidth, dialogHeight);

    auto safeThis = juce::Component::SafePointer<MainComponent>(this);
    dialog->enterModalState(true,
        juce::ModalCallbackFunction::create([safeThis,
                                             onSelection = std::move(onSelection),
                                             onCancel = std::move(onCancel)](int result) mutable
        {
            if (auto* comp = safeThis.getComponent())
            {
                switch (result)
                {
                    case 1:
                        if (onSelection)
                            onSelection(ExportProcessingOption::LoopAndPreset);
                        break;
                    case 2:
                        if (onSelection)
                            onSelection(ExportProcessingOption::LoopOnly);
                        break;
                    case 3:
                        if (onSelection)
                            onSelection(ExportProcessingOption::PresetOnly);
                        break;
                    default:
                        if (onCancel)
                            onCancel();
                        break;
                }
            }
        }),
        true);
}

void MainComponent::prepareExportForOption(ExportProcessingOption option,
                                           std::function<void(ExportProcessingOption)> onReady,
                                           std::function<void()> onCancel)
{
    if (option == ExportProcessingOption::LoopOnly)
    {
        projectState.setNormalizationGain(0.0f);
        updateStatus("Exporting with loop data only");
        if (onReady)
            onReady(option);
        return;
    }

    float gainDb = 0.0f;
    juce::String description;
    if (!audioLevelStudio.calculateActivePresetGain(gainDb, description))
    {
        juce::AlertWindow::showMessageBoxAsync(
            juce::MessageBoxIconType::WarningIcon,
            "Preset Error",
            "Could not calculate gain for the \"" + audioLevelStudio.getActivePresetDisplayName() + "\" preset.");
        updateStatus("Preset unavailable for export");
        if (onCancel)
            onCancel();
        return;
    }

    projectState.setNormalizationGain(gainDb);
    juce::String status;
    status << "Applying " << audioLevelStudio.getActivePresetDisplayName()
           << " preset (" << juce::String(gainDb, 2) << " dB)";
    updateStatus(status);
    if (onReady)
        onReady(option);
}

bool MainComponent::shouldWarnAboutMissingLoopData() const
{
    if (!projectState.hasAudio())
        return false;

    const auto& buffer = projectState.getAudioBuffer();
    const int numSamples = buffer.getNumSamples();
    if (numSamples <= 0)
        return false;

    const auto loopStart = projectState.getLoopStart();
    const auto loopEnd = projectState.getLoopEnd();

    if (!projectState.hasLoopPoints())
        return true;

    const bool loopStartDefault = loopStart <= 0;
    const bool loopEndDefault = loopEnd >= numSamples;
    return loopStartDefault && loopEndDefault;
}

void MainComponent::updateStatus(const juce::String& message)
{
    statusLabel.setText(message, juce::dontSendNotification);
}

void MainComponent::checkPreviewState()
{
    // Check if preview has stopped playing
    if (msuFileBrowser.getPreviewingRow() >= 0 && !previewPlayer.isPlaying())
    {
        msuFileBrowser.clearPreviewingRow();
    }
}

void MainComponent::loadPersistedDirectories()
{
    if (settings == nullptr)
        return;

    const auto audioPath = settings->getValue(audioDirectoryKey);
    if (audioPath.isNotEmpty())
        lastAudioDirectory = juce::File(audioPath);

    const auto msuPath = settings->getValue(msuDirectoryKey);
    if (msuPath.isNotEmpty())
    {
        lastMSUDirectory = juce::File(msuPath);
        msuFileBrowser.setInitialDirectory(lastMSUDirectory);
    }

    backupOriginalsEnabled = settings->getBoolValue(backupOriginalsKey, true);
}

void MainComponent::saveLastAudioDirectory(const juce::File& directory)
{
    if (directory.getFullPathName().isEmpty())
        return;

    lastAudioDirectory = directory;
    if (settings != nullptr)
    {
        settings->setValue(audioDirectoryKey, directory.getFullPathName());
        settings->saveIfNeeded();
    }
}

void MainComponent::saveLastMSUDirectory(const juce::File& directory)
{
    if (directory.getFullPathName().isEmpty())
        return;

    lastMSUDirectory = directory;
    msuFileBrowser.setInitialDirectory(directory);

    if (settings != nullptr)
    {
        settings->setValue(msuDirectoryKey, directory.getFullPathName());
        settings->saveIfNeeded();
    }
}

void MainComponent::showSettingsDialog()
{
    auto* settingsWindow = new juce::AlertWindow(
        "Settings",
        "Configure how MSU-1 backups behave.",
        juce::MessageBoxIconType::NoIcon);

    auto backupToggle = std::make_unique<juce::ToggleButton>("Back up original PCM files");
    backupToggle->setToggleState(backupOriginalsEnabled, juce::dontSendNotification);
    backupToggle->setSize(260, 24);
    settingsWindow->addTextBlock("When enabled, each replaced PCM and its metadata are copied into the MSU-1 directory's \\Backup folder. We also maintain metadata_backup.json in that folder so the Restore Backups option can put everything back.");
    settingsWindow->addCustomComponent(backupToggle.get());

    settingsWindow->addTextBlock("Created by Scott McKay (BelmontLegon). Source and updates:");
    auto creditsLink = std::make_unique<juce::HyperlinkButton>(
        "github.com/belmontlegion/EMESSYOO",
        juce::URL("https://github.com/belmontlegion/EMESSYOO/"));
    creditsLink->setSize(280, 24);
    creditsLink->setJustificationType(juce::Justification::centredLeft);
    settingsWindow->addCustomComponent(creditsLink.get());

    settingsWindow->addButton("Save", 1, juce::KeyPress(juce::KeyPress::returnKey));
    settingsWindow->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));

    settingsWindow->enterModalState(true,
        juce::ModalCallbackFunction::create([this, settingsWindow, backupToggle = std::move(backupToggle), creditsLink = std::move(creditsLink)](int result) mutable
        {
            if (result == 1)
            {
                if (backupToggle != nullptr)
                {
                    backupOriginalsEnabled = backupToggle->getToggleState();
                    audioLevelStudio.setBackupPreference(backupOriginalsEnabled);
                    if (settings != nullptr)
                    {
                        settings->setValue(backupOriginalsKey, backupOriginalsEnabled ? 1 : 0);
                        settings->saveIfNeeded();
                    }
                    updateStatus(backupOriginalsEnabled
                        ? "Original PCM backups enabled"
                        : "Original PCM backups disabled");
                }
            }

            delete settingsWindow;
        }),
        true);

}


void MainComponent::showRestoreBackupsDialog()
{
    auto msuDir = msuFileBrowser.getCurrentDirectory();
    if (!msuDir.isDirectory())
    {
        juce::AlertWindow::showMessageBoxAsync(
            juce::MessageBoxIconType::WarningIcon,
            "No Project Loaded",
            "Load an MSU project before restoring backups.");
        return;
    }

    auto backupDir = msuDir.getChildFile("Backup");
    if (!backupDir.isDirectory())
    {
        juce::AlertWindow::showMessageBoxAsync(
            juce::MessageBoxIconType::InfoIcon,
            "No Backups Found",
            "This project does not have a Backup folder yet.");
        return;
    }

    auto backupFiles = backupDir.findChildFiles(juce::File::findFiles, false, "*.pcm");
    if (backupFiles.isEmpty())
    {
        juce::AlertWindow::showMessageBoxAsync(
            juce::MessageBoxIconType::InfoIcon,
            "No PCM Backups",
            "There are no PCM backups to restore in this project.");
        return;
    }

    BackupMetadataStore metadataStore(backupDir);
    std::vector<RestoreCandidateInfo> candidateInfos;
    candidateInfos.reserve(static_cast<size_t>(backupFiles.size()));
    std::vector<BackupRestoreDialog::Entry> dialogEntries;
    dialogEntries.reserve(static_cast<size_t>(backupFiles.size()));

    for (auto file : backupFiles)
    {
        RestoreCandidateInfo info;
        info.backupFile = file;
        info.targetFile = msuDir.getChildFile(file.getFileName());
        info.fileSize = file.getSize();

        auto snapshots = metadataStore.getSnapshotsFor(file.getFileName());
        info.metadataEntries.reserve(snapshots.size());
        for (const auto& record : snapshots)
        {
            MetadataBackupEntry entry;
            entry.manifestFile = record.manifestFile;
            entry.pcmFileName = record.pcmFileName.isNotEmpty() ? record.pcmFileName : file.getFileName();
            entry.snapshot = record.snapshot;
            info.metadataEntries.push_back(entry);
        }

        BackupRestoreDialog::Entry row;
        row.title = file.getFileName();
        juce::String detail = juce::File::descriptionOfSizeInBytes(file.getSize());
        detail << (info.metadataEntries.empty() ? " • No metadata snapshot" : " • Metadata snapshot ready");
        if (!info.targetFile.existsAsFile())
            detail << " • Current track missing";
        row.detail = detail;
        row.metadataAvailable = !info.metadataEntries.empty();

        candidateInfos.push_back(std::move(info));
        dialogEntries.push_back(row);
    }

    auto sharedCandidates = std::make_shared<std::vector<RestoreCandidateInfo>>(std::move(candidateInfos));
    auto dialog = std::make_unique<BackupRestoreDialog>(std::move(dialogEntries));
    juce::Component::SafePointer<BackupRestoreDialog> dialogSafe(dialog.get());
    dialog->onRestore = [this, sharedCandidates, dialogSafe](const juce::Array<int>& selectedRows)
    {
        return handleBackupRestoreSelection(*sharedCandidates, selectedRows, dialogSafe);
    };

    juce::DialogWindow::LaunchOptions options;
    options.content.setOwned(dialog.release());
    options.dialogTitle = "Restore Backups";
    options.dialogBackgroundColour = getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId);
    options.useNativeTitleBar = true;
    options.escapeKeyTriggersCloseButton = true;
    options.resizable = true;
    options.content->setSize(640, 480);
    options.launchAsync();
}

bool MainComponent::handleBackupRestoreSelection(const std::vector<RestoreCandidateInfo>& candidates,
                                                 const juce::Array<int>& selectedRows,
                                                 juce::Component::SafePointer<BackupRestoreDialog> dialogToClose)
{
    if (selectedRows.isEmpty())
    {
        juce::AlertWindow::showMessageBoxAsync(
            juce::MessageBoxIconType::WarningIcon,
            "No Selection",
            "Select at least one backup to restore.");
        return false;
    }

    std::vector<const RestoreCandidateInfo*> selection;
    selection.reserve(selectedRows.size());
    for (int index : selectedRows)
    {
        if (index >= 0 && index < static_cast<int>(candidates.size()))
            selection.push_back(&candidates[static_cast<size_t>(index)]);
    }

    if (selection.empty())
    {
        juce::AlertWindow::showMessageBoxAsync(
            juce::MessageBoxIconType::WarningIcon,
            "Invalid Selection",
            "Unable to determine which backups to restore.");
        return false;
    }

    juce::StringArray summaryLines;
    for (auto* candidate : selection)
        summaryLines.add("- " + candidate->backupFile.getFileName());

    juce::String confirmMessage = "The selected backups will overwrite the current PCM files and cannot be undone.\n\n" +
                                   summaryLines.joinIntoString("\n") +
                                   "\n\nContinue?";

    auto selectedPointers = std::make_shared<std::vector<const RestoreCandidateInfo*>>(std::move(selection));

    auto performRestore = [this,
                           selectedPointers,
                           dialogToClose]()
    {
        juce::StringArray successes;
        juce::StringArray failures;
        executeBackupRestoreSelection(*selectedPointers, successes, failures);

        juce::String resultMessage;
        if (!successes.isEmpty())
        {
            resultMessage << "Restored " << successes.size() << " track" << (successes.size() == 1 ? "" : "s") << ".";
        }

        if (!failures.isEmpty())
        {
            if (resultMessage.isNotEmpty())
                resultMessage << "\n\n";
            resultMessage << "Issues:\n" << failures.joinIntoString("\n");
        }

        const bool success = failures.isEmpty();
        juce::AlertWindow::showMessageBoxAsync(success ? juce::MessageBoxIconType::InfoIcon
                                                       : juce::MessageBoxIconType::WarningIcon,
                                               success ? "Restore Complete" : "Restore Completed With Issues",
                                               resultMessage.isNotEmpty() ? resultMessage
                                                                          : juce::String("Restore finished."));

        scheduleMSUTrackReload();

        updateStatus("Restored " + juce::String(successes.size()) + " backup" + (successes.size() == 1 ? "" : "s"));

        if (dialogToClose != nullptr)
            dialogToClose->dismiss();
    };

    auto* confirmWindow = new juce::AlertWindow("Confirm Restore",
                                                confirmMessage,
                                                juce::MessageBoxIconType::WarningIcon);
    confirmWindow->addButton("Restore", 1);
    confirmWindow->addButton("Cancel", 0);
    confirmWindow->centreAroundComponent(this, confirmWindow->getWidth(), confirmWindow->getHeight());
    confirmWindow->enterModalState(true,
        juce::ModalCallbackFunction::create([performRestore, confirmWindow](int result)
        {
            if (result == 1)
                performRestore();

            delete confirmWindow;
        }));

    return false;
}

void MainComponent::executeBackupRestoreSelection(const std::vector<const RestoreCandidateInfo*>& selection,
                                                  juce::StringArray& successes,
                                                  juce::StringArray& failures)
{
    MSUManifestUpdater updater;
    bool reloadedTracks = false;

    for (const auto* candidate : selection)
    {
        if (candidate == nullptr)
            continue;

        if (!candidate->backupFile.existsAsFile())
        {
            failures.add("- Missing backup: " + candidate->backupFile.getFileName());
            continue;
        }

        auto parent = candidate->targetFile.getParentDirectory();
        if (!parent.isDirectory() && !parent.createDirectory())
        {
            failures.add("- Cannot create target directory for " + candidate->targetFile.getFileName());
            continue;
        }

        if (candidate->targetFile.existsAsFile() && !candidate->targetFile.deleteFile())
        {
            failures.add("- Could not overwrite " + candidate->targetFile.getFileName());
            continue;
        }

        if (!candidate->backupFile.copyFileTo(candidate->targetFile))
        {
            failures.add("- Copy failed for " + candidate->targetFile.getFileName());
            continue;
        }

        bool metadataSuccess = true;
        for (const auto& entry : candidate->metadataEntries)
        {
            if (!entry.manifestFile.existsAsFile())
                continue;

            if (!updater.restoreMetadataSnapshot(entry.manifestFile,
                                                  entry.pcmFileName.isNotEmpty() ? entry.pcmFileName
                                                                                : candidate->targetFile.getFileName(),
                                                  entry.snapshot))
            {
                metadataSuccess = false;
                failures.add("- Metadata restore failed for " + entry.manifestFile.getFileName() +
                             ": " + updater.getLastError());
            }
        }

        juce::String successLine = candidate->targetFile.getFileName();
        if (!candidate->metadataEntries.empty())
            successLine += metadataSuccess ? " (metadata restored)" : " (metadata issues)";
        successes.add(successLine);

        if (!candidate->backupFile.deleteFile())
            failures.add("- Restored " + candidate->targetFile.getFileName() + " but could not delete backup copy");
        reloadedTracks = true;
    }

    if (reloadedTracks)
        scheduleMSUTrackReload();
}
MainComponent::ManifestUpdateResult MainComponent::applyManifestUpdates(const juce::Array<juce::File>& manifests,
                                                                        const juce::File& exportedFile,
                                                                        int64 loopStartSamples,
                                                                        bool backupMetadata,
                                                                        juce::File backupDirectory)
{
    ManifestUpdateResult result;
    if (manifests.isEmpty())
        return result;

    result.attempted = true;

    MSUManifestUpdater updater;
    juce::StringArray lines;
    std::unique_ptr<BackupMetadataStore> metadataStore;

    if (backupMetadata)
    {
        if (!backupDirectory.isDirectory())
            backupDirectory.createDirectory();
        if (backupDirectory.isDirectory())
            metadataStore = std::make_unique<BackupMetadataStore>(backupDirectory);
    }

    for (auto manifest : manifests)
    {
        if (metadataStore)
        {
            MSUManifestUpdater::MetadataSnapshot snapshot;
            if (MSUManifestUpdater::captureMetadataSnapshot(manifest,
                                                            exportedFile.getFileName(),
                                                            snapshot))
            {
                metadataStore->recordSnapshot(exportedFile.getFileName(), manifest, snapshot);
            }
        }

        if (updater.updateManifest(manifest, exportedFile.getFileName(), loopStartSamples))
        {
            lines.add("- " + manifest.getFileName() + " updated");
        }
        else
        {
            result.success = false;
            lines.add("- Failed " + manifest.getFileName() + ": " + updater.getLastError());
        }
    }

    result.summary = lines.joinIntoString("\n");

    if (result.success)
        updateStatus("Metadata updated in " + juce::String(manifests.size()) + " manifest file(s)");
    else
        updateStatus("Metadata update issues detected");

    return result;
}

void MainComponent::promptToUpdateManifests(const juce::File& exportedFile,
                                            int64 loopStartSamples,
                                            bool backupMetadata,
                                            juce::File backupDirectory)
{
    auto manifests = MSUManifestUpdater::findRelatedManifestFiles(exportedFile);
    if (manifests.isEmpty())
        return;

    juce::StringArray manifestLines;
    for (auto manifest : manifests)
        manifestLines.add("- " + manifest.getFileName());

    auto message = juce::String("Detected the following manifest files for ") + exportedFile.getFileName() + "\n\n" +
                   manifestLines.joinIntoString("\n") +
                   "\n\nUpdate loop metadata to sample " + juce::String(loopStartSamples) + "?";

    juce::AlertWindow::showOkCancelBox(
        juce::MessageBoxIconType::QuestionIcon,
        "Update Metadata?",
        message,
        "Update",
        "Skip",
        nullptr,
        juce::ModalCallbackFunction::create([this,
                                             manifests,
                                             exportedFile,
                                             loopStartSamples,
                                             backupMetadata,
                                             backupDirectory](int result)
        {
            if (result == 1)
            {
                auto summary = applyManifestUpdates(manifests,
                                                    exportedFile,
                                                    loopStartSamples,
                                                    backupMetadata,
                                                    backupDirectory);
                if (summary.attempted)
                {
                    juce::AlertWindow::showMessageBoxAsync(
                        summary.success ? juce::MessageBoxIconType::InfoIcon
                                        : juce::MessageBoxIconType::WarningIcon,
                        summary.success ? "Metadata Updated" : "Metadata Update Issues",
                        summary.summary);
                }
            }
        }));
}

void MainComponent::refreshTrackListIfBackupsEnabled()
{
    if (!backupOriginalsEnabled)
        return;
    scheduleMSUTrackReload();
}

void MainComponent::scheduleMSUTrackReload()
{
    juce::Component::SafePointer<MainComponent> safeThis(this);
    juce::MessageManager::callAsync([safeThis]
    {
        if (auto* comp = safeThis.getComponent())
            comp->reloadMSUTracks();
    });
}

void MainComponent::reloadMSUTracks()
{
    const auto currentMSU = msuFileBrowser.getCurrentMSUFile();
    if (currentMSU.existsAsFile())
        msuFileBrowser.loadMSUFile(currentMSU);
    else
        msuFileBrowser.refreshTable();

    msuFileBrowser.table.updateContent();
    msuFileBrowser.table.repaint();
}

void MainComponent::handleStopPreview()
{
    previewPlayer.stop();
    msuFileBrowser.clearPreviewingRow();
    updateStatus("Preview stopped");
}

void MainComponent::handlePreviewTrack(const MSUFileBrowser::TrackInfo& track)
{
    if (!track.exists || !track.file.existsAsFile())
    {
        updateStatus("Cannot preview missing track");
        return;
    }
    
    // Stop main player if playing
    audioPlayer.stop();
    
    // Load and play with preview player (it will stop any previous preview internally)
    if (previewPlayer.loadAndPlay(track.file))
    {
        // Update browser to show this row is previewing
        for (size_t i = 0; i < msuFileBrowser.tracks.size(); ++i)
        {
            if (msuFileBrowser.tracks[i].trackNumber == track.trackNumber)
            {
                msuFileBrowser.setPreviewingRow(static_cast<int>(i));
                break;
            }
        }
        
        updateStatus("Previewing: " + track.fileName + " (Track " + juce::String(track.trackNumber) + ")");
    }
    else
    {
        updateStatus("Failed to preview: " + track.fileName);
        juce::AlertWindow::showMessageBoxAsync(
            juce::MessageBoxIconType::WarningIcon,
            "Preview Error",
            "Failed to load track for preview:\n" + track.fileName);
    }
}

void MainComponent::handleReplaceTrack(const MSUFileBrowser::TrackInfo& track)
{
    // Stop preview if active
    if (msuFileBrowser.getPreviewingRow() >= 0)
    {
        previewPlayer.stop();
        msuFileBrowser.clearPreviewingRow();
    }
    
    // Store the original track filename for later use
    juce::File originalTrackFile = track.file;
    juce::String originalFileName = track.file.getFileName();
    
    // Prompt user to select an audio file
    const auto initialReplaceDir = lastAudioDirectory.getFullPathName().isNotEmpty()
        ? lastAudioDirectory
        : juce::File();

    auto* chooser = new juce::FileChooser(
        "Select audio file to replace track " + juce::String(track.trackNumber) + "...",
        initialReplaceDir,
        "*.wav;*.mp3;*.flac;*.ogg;*.aiff");
    
    chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
        [this, track, chooser, originalTrackFile, originalFileName](const juce::FileChooser& fc)
        {
            auto sourceFile = fc.getResult();
            
            if (sourceFile != juce::File())
            {
                // Remember this directory for next time
                saveLastAudioDirectory(sourceFile.getParentDirectory());
                
                // Load the file and display waveform in main editor
                AudioFileHandler fileHandler;
                juce::AudioBuffer<float> buffer;
                double sampleRate = 0.0;
                int64 loopPoint = 0;
                
                if (fileHandler.loadAudioFile(sourceFile, buffer, sampleRate, &loopPoint))
                {
                    // Load into main waveform view
                    projectState.setAudioBuffer(buffer, sampleRate);
                    projectState.setSourceFile(sourceFile);
                    
                    // Set loop points
                    int64 loopStart = 0;
                    int64 loopEnd = buffer.getNumSamples();
                    
                    if (loopPoint > 0 && loopPoint < loopEnd)
                        loopStart = loopPoint;
                    
                    projectState.setLoopStart(loopStart);
                    projectState.setLoopEnd(loopEnd);
                    
                    // Apply auto trim/pad if enabled
                    if (transportControls.isAutoTrimPadEnabled())
                        transportControls.applyAutoTrimPad();
                    else if (transportControls.isTrimNoPadEnabled())
                        transportControls.applyTrimNoPad();
                    
                    updateStatus("Loaded: " + sourceFile.getFileName() + 
                               " - Will replace: " + originalFileName + " when exported");
                    
                    // Store the target filename in project state for export
                    projectState.setTargetExportFile(originalTrackFile);
                }
                else
                {
                    updateStatus("Failed to load: " + sourceFile.getFileName());
                    juce::AlertWindow::showMessageBoxAsync(
                        juce::MessageBoxIconType::WarningIcon,
                        "Load Error",
                        "Failed to load audio file:\n" + fileHandler.getLastError());
                }
            }
            
            delete chooser;
        });
}
