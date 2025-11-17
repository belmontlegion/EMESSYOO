#pragma once

#include <JuceHeader.h>
#include <functional>
#include <vector>
#include "Core/MSUProjectState.h"
#include "Core/AudioFileHandler.h"
#include "Audio/AudioImporter.h"
#include "UI/WaveformView.h"
#include "UI/TransportControls.h"
#include "UI/ToolbarPanel.h"
#include "UI/MSUFileBrowser.h"
#include "UI/CustomLookAndFeel.h"
#include "UI/LoopEditorTab.h"
#include "UI/AudioLevelStudioComponent.h"
#include "Audio/AudioPlayer.h"
#include "Audio/PreviewPlayer.h"
#include "Audio/BeforeAfterPreviewPlayer.h"
#include "Audio/VolumeMatchAnalyzer.h"
#include "Export/MSU1Exporter.h"
#include "Export/MSUManifestUpdater.h"
#include "Export/ManifestHandler.h"
#include "Dialogs/BackupRestoreDialog.h"

//==============================================================================
/**
 * Main application component for MSU-1 Prep Studio.
 * Coordinates all UI components and manages the application state.
 */
class MainComponent : public juce::Component,
                      public juce::ChangeListener,
                      public juce::Timer
{
public:
    inline static constexpr int kToolbarHeight = 60;
    inline static constexpr int kStatusBarHeight = 24;
    inline static constexpr int kBrowserHeight = 300;
    inline static constexpr int kTransportHeight = 80;
    inline static constexpr int kWaveformMinHeight = 220;
    inline static constexpr int kMinimumHeight = kToolbarHeight + kStatusBarHeight + kBrowserHeight + kTransportHeight + kWaveformMinHeight;
    inline static constexpr int kDefaultMinimumWidth = 1100;
    inline static constexpr int kPreferredWidth = 1300;
    inline static constexpr int kPreferredHeight = 820;

    //==============================================================================
    MainComponent();
    ~MainComponent() override;

    //==============================================================================
    void paint(juce::Graphics&) override;
    void resized() override;
    void changeListenerCallback(juce::ChangeBroadcaster* source) override;
    void timerCallback() override;
    bool keyPressed(const juce::KeyPress& key) override;

    int getMinimumWindowWidth() const;
    int getPreferredWindowWidth() const;
    int getMinimumWindowHeight() const { return kMinimumHeight; }
    int getPreferredWindowHeight() const { return juce::jmax(kPreferredHeight, kMinimumHeight); }

private:
    //==============================================================================
    // Core components
    MSUProjectState projectState;
    AudioPlayer audioPlayer;
    PreviewPlayer previewPlayer;
    BeforeAfterPreviewPlayer beforeAfterPreviewPlayer;
    juce::AudioDeviceManager audioDeviceManager;
    
    // UI components
    CustomLookAndFeel customLookAndFeel;
    ToolbarPanel toolbar;
    juce::TabbedComponent mainTabs;
    int audioLevelStudioTabIndex = -1;
    int loopEditorTabIndex = -1;
    int lastMainTabIndex = -1;
    WaveformView waveformView;
    TransportControls transportControls;
    MSUFileBrowser msuFileBrowser;
    LoopEditorTab loopEditorTab;
    AudioLevelStudioComponent audioLevelStudio;

    juce::Label statusLabel;
    
    // File chooser
    std::unique_ptr<juce::FileChooser> fileChooser;
    
    // Last used directories
    juce::File lastAudioDirectory;
    juce::File lastMSUDirectory;
    std::unique_ptr<juce::PropertiesFile> settings;
    bool backupOriginalsEnabled = true;
    
    // Callbacks
    void openAudioFile();
    void exportPCM();
    void updateStatus(const juce::String& message);
    void handleReplaceTrack(const MSUFileBrowser::TrackInfo& track);
    void handlePreviewTrack(const MSUFileBrowser::TrackInfo& track);
    void handleStopPreview();
    void checkPreviewState();
    void loadPersistedDirectories();
    void saveLastAudioDirectory(const juce::File& directory);
    void saveLastMSUDirectory(const juce::File& directory);
    void showSettingsDialog();
    void showRestoreBackupsDialog();
    void beginManualExportFlow();
    bool shouldWarnAboutMissingLoopData() const;
    enum class ExportProcessingOption
    {
        LoopAndPreset,
        LoopOnly,
        PresetOnly
    };

    void promptExportProcessingOptions(std::function<void(ExportProcessingOption)> onSelection,
                                       std::function<void()> onCancel = nullptr);
    void prepareExportForOption(ExportProcessingOption option,
                                std::function<void(ExportProcessingOption)> onReady,
                                std::function<void()> onCancel = nullptr);

    struct ManifestUpdateResult
    {
        bool attempted = false;
        bool success = true;
        juce::String summary;
    };

    struct MetadataBackupEntry
    {
        juce::File manifestFile;
        juce::String pcmFileName;
        MSUManifestUpdater::MetadataSnapshot snapshot;
    };

    struct RestoreCandidateInfo
    {
        juce::File backupFile;
        juce::File targetFile;
        int64 fileSize = 0;
        std::vector<MetadataBackupEntry> metadataEntries;
    };

    ManifestUpdateResult applyManifestUpdates(const juce::Array<juce::File>& manifests,
                                              const juce::File& exportedFile,
                                              int64 loopStartSamples,
                                              bool backupMetadata,
                                              juce::File backupDirectory);
    void promptToUpdateManifests(const juce::File& exportedFile,
                                 int64 loopStartSamples,
                                 bool backupMetadata,
                                 juce::File backupDirectory);
    void refreshTrackListIfBackupsEnabled();
    void scheduleMSUTrackReload();
    void reloadMSUTracks();
    bool handleBackupRestoreSelection(const std::vector<RestoreCandidateInfo>& candidates,
                                      const juce::Array<int>& selectedRows,
                                      juce::Component::SafePointer<BackupRestoreDialog> dialogToClose);
    void executeBackupRestoreSelection(const std::vector<const RestoreCandidateInfo*>& selection,
                                       juce::StringArray& successes,
                                       juce::StringArray& failures);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};
