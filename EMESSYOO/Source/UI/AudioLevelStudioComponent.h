#pragma once

#include <JuceHeader.h>
#include <atomic>
#include <functional>
#include <limits>
#include <memory>
#include <optional>
#include <thread>
#include "../Core/MSUProjectState.h"
#include "../Audio/NormalizationAnalyzer.h"
#include "../Audio/BeforeAfterPreviewPlayer.h"
#include "MSUFileBrowser.h"

class BatchPreviewButton;
class BatchReplaceButton;
class BatchProgressView;

class WaveformOverlayView : public juce::Component
{
public:
    WaveformOverlayView(juce::AudioThumbnail& beforeThumb,
                        juce::AudioThumbnail& afterThumb);
    void paint(juce::Graphics& g) override;
    void setColours(juce::Colour before, juce::Colour after);
    void setPlaybackCursorRatio(double ratio);

private:
    juce::AudioThumbnail& beforeThumbnail;
    juce::AudioThumbnail& afterThumbnail;
    juce::Colour beforeColour { juce::Colours::green.withAlpha(0.7f) };
    juce::Colour afterColour { juce::Colours::aqua.withAlpha(0.7f) };
    double playbackCursorRatio = std::numeric_limits<double>::quiet_NaN();
};

/**
 * Audio Level Studio tab. Hosts loudness presets, stats, and waveform overlays.
 */
class AudioLevelStudioComponent : public juce::Component,
                                  private juce::Timer,
                                  private juce::TableListBoxModel
{
public:
    AudioLevelStudioComponent(MSUProjectState& projectState,
                              BeforeAfterPreviewPlayer& previewPlayer);
    ~AudioLevelStudioComponent() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

    void refreshFromProjectState();
    void generatePresetPreview();
    void stopPreviewPlayback();
    void setPlaybackStopper(std::function<void()> stopper) { requestPlaybackStop = std::move(stopper); }
    void setMSUContext(const juce::File& msuFile,
                       const juce::String& gameTitle,
                       const std::vector<MSUFileBrowser::TrackInfo>& tracks);
    void clearMSUContext();
    juce::String getActivePresetDisplayName() const;
    bool hasManualOverridesActive() const;
    bool calculateActivePresetGain(float& gainDb, juce::String& description);
    void setMSULoadCallback(std::function<void()> loader)
    {
        requestExternalMSULoad = std::move(loader);
        updateBatchButtons();
    }
    void setBackupPreference(bool enabled) { backupsEnabled = enabled; }
    void setTrackReplacementCallback(std::function<void(const MSUFileBrowser::TrackInfo&)> replacer)
    {
        requestTrackReplacement = std::move(replacer);
    }
    void setTrackListRefreshCallback(std::function<void()> refresher)
    {
        requestTrackListRefresh = std::move(refresher);
    }

private:
    friend class BatchPreviewButton;
    friend class BatchReplaceButton;
    struct PresetSettings
    {
        int presetId = 1;
        bool manualTargetEnabled = false;
        float manualTargetRmsDb = -18.0f;
        bool manualPeakEnabled = false;
        float manualPeakDbfs = -1.0f;
    };

    struct BatchTrackEntry
    {
        int trackNumber = 0;
        juce::String songTitle;
        juce::String suggestedName;
        juce::File pcmFile;
        bool backupExists = false;
    };

    juce::String formatLengthString(double seconds) const;
    juce::String formatLoopRange(int64 loopStart, int64 loopEnd, double sampleRate) const;
    juce::String formatDbValue(float value) const;
    juce::String formatLufsEstimate(float rmsDb) const;
    juce::String formatHeadroom(float peakDb) const;
    bool calculatePresetGain(float& gainDb, juce::String& description,
                             std::optional<NormalizationAnalyzer::AudioStats> statsOverride = std::nullopt,
                             const PresetSettings* presetOverride = nullptr) const;
    static bool calculatePresetGainForSettings(const PresetSettings& settings,
                                               const NormalizationAnalyzer::AudioStats& stats,
                                               float& gainDb,
                                               juce::String& description);
    float getSelectedPresetTargetRms() const;
    static float getSelectedPresetTargetRms(const PresetSettings& settings);
    PresetSettings getCurrentPresetSettings() const;
    void applyGainNonDestructively(float gainDb);
    void updateWaveformThumbnails(bool forceReferenceReset = false);
    void refreshReferenceBuffer(const juce::AudioBuffer<float>& buffer,
                                double sampleRate,
                                const juce::File& sourceFile = {});
    void clearWaveformData();
    void clearPreview();
    void updateWaveformLegend();
    void applyPreviewBuffer(const juce::AudioBuffer<float>& buffer,
                            double sampleRate,
                            const juce::String& description,
                            float gainDb,
                            bool updateProjectState = true);
    void handlePreviewButtonPress(BeforeAfterPreviewPlayer::Target target);
    void updatePreviewPlaybackButtons();
    void syncBeforeAfterBuffers();
    void timerCallback() override;
    void rebuildTrimPadBuffer(const juce::AudioBuffer<float>& source,
                              juce::AudioBuffer<float>& destination,
                              int64 trimStart,
                              int64 paddingSamples,
                              int64 loopEndSample);
    void rebuildProcessedBuffers();
    void syncAdvancedControls();
    void updateManualTargetValueLabel();
    void updatePeakTargetValueLabel();
    int getNumRows() override;
    void paintRowBackground(juce::Graphics& g, int rowNumber, int width, int height, bool rowIsSelected) override;
    void paintCell(juce::Graphics& g, int rowNumber, int columnId, int width, int height, bool rowIsSelected) override;
    juce::Component* refreshComponentForCell(int rowNumber,
                                             int columnId,
                                             bool isRowSelected,
                                             juce::Component* existingComponentToUpdate) override;
    void mouseWheelMove(const juce::MouseEvent& event,
                        const juce::MouseWheelDetails& wheel) override;
    void selectedRowsChanged(int lastRowSelected) override;
    void updateBatchSelectionSummary();
    void updateBatchButtons();
    void updateBatchPresetNoteText();
    bool hasManualOverridesEnabled() const;
    juce::String describeManualOverrides(bool includeValues) const;
    void runBatchOperation(bool exportMode);
    void beginBatchOperation(bool exportMode,
                             std::vector<BatchTrackEntry> entries,
                             PresetSettings presetSettings);
    void showBatchProgressDialog(bool exportMode, int totalTracks);
    void updateBatchProgressDialog(int processed, int total, const juce::String& latestLine);
    void finalizeBatchProgressDialog(const juce::String& finalSummary);
    void postBatchProgressUpdate(int processed, int total, const juce::String& latestLine);
    void rebuildBatchTrackList(const std::vector<MSUFileBrowser::TrackInfo>& tracks);
    juce::String formatSuggestedTrackName(int trackNumber, const juce::String& songTitle) const;
    juce::Array<int> getSelectedBatchRows() const;
    juce::String getSelectedPresetLabel() const;
    const BatchTrackEntry* getBatchEntry(int rowNumber) const;
    void handleTrackPreviewToggle(int rowNumber);
    void handleTrackReplaceRequest(int rowNumber);
    bool previewBatchTrack(int rowNumber);
    bool loadTrackPreviewData(const BatchTrackEntry& entry,
                              juce::AudioBuffer<float>& sourceBuffer,
                              juce::AudioBuffer<float>& processedBuffer,
                              double& sampleRate);
    void configureBatchPreviewPlayback(const juce::AudioBuffer<float>& sourceBuffer,
                                       const juce::AudioBuffer<float>& processedBuffer,
                                       double sampleRate);
    void stopActiveTrackPreview();
    void refreshBatchTableComponents();
    MSUFileBrowser::TrackInfo makeTrackInfo(const BatchTrackEntry& entry) const;
    void startBatchWorker(bool exportMode,
                          std::vector<BatchTrackEntry> entries,
                          PresetSettings presetSettings);
    bool hasExistingBackups(const std::vector<BatchTrackEntry>& entries) const;
    void promptBackupOverwriteConfirmation(std::function<void()> onConfirm,
                                           std::function<void()> onCancel);
    void handleBatchCompletion(bool exportMode,
                               int processed,
                               int failures,
                               juce::StringArray logLines,
                               bool cancelled);
    void cancelBatchOperation();

    MSUProjectState& projectState;
    BeforeAfterPreviewPlayer& beforeAfterPlayer;
    std::function<void()> requestPlaybackStop;
    std::function<void()> requestExternalMSULoad;
    std::function<void(const MSUFileBrowser::TrackInfo&)> requestTrackReplacement;
    std::function<void()> requestTrackListRefresh;

    juce::Viewport contentViewport;
    juce::Component contentHolder;
    juce::Label headerLabel;
    juce::Label descriptionLabel;
    juce::ComboBox presetSelector;
    juce::Label presetLabel;
    juce::TextButton playBeforeButton { "Play Before" };
    juce::TextButton playAfterButton { "Play After" };

    juce::GroupComponent fileInfoGroup { "fileInfo", "Current Selection" };
    juce::Label fileNameLabel;
    juce::Label fileDetailLabel;
    juce::Label loopInfoLabel;

    juce::GroupComponent metricsGroup { "metrics", "Current Levels" };
    juce::Label rmsLabel;
    juce::Label lufsLabel;
    juce::Label peakLabel;
    juce::Label headroomLabel;
    juce::Label statsHintLabel;

    juce::GroupComponent waveformGroup { "waveform", "Waveform Preview" };
    juce::Label waveformPlaceholder;
    juce::Label waveformLegendLabel;

    juce::GroupComponent advancedGroup { "advanced", "Manual Settings" };
    juce::ToggleButton manualTargetToggle { "Manual RMS target" };
    juce::Slider manualTargetSlider;
    juce::Label manualTargetValueLabel;
    juce::ToggleButton peakCeilingToggle { "Manual peak ceiling" };
    juce::Slider peakCeilingSlider;
    juce::Label peakCeilingValueLabel;
    juce::Label advancedHelpLabel;
    juce::GroupComponent batchGroup { "batchGroup", "Batch MSU Processing" };
    juce::Label batchStatusLabel;
    juce::TextButton loadMSUButton { "Load MSU-1" };
    juce::TableListBox batchTrackTable;
    juce::Label batchPresetNote;
    juce::Label batchPreviewNote;
    juce::TextButton batchExportButton { "Batch Export" };
    juce::TextButton batchCancelButton { "Cancel Batch" };
    double batchProgressValue = 0.0;
    juce::ProgressBar batchProgressBar { batchProgressValue };

    NormalizationAnalyzer::AudioStats latestStats;
    bool hasStats = false;
    bool backupsEnabled = true;
    juce::AudioBuffer<float> previewBuffer;
    bool previewValid = false;
    float pendingPreviewGainDb = 0.0f;
    juce::String pendingPresetDescription;
    double previewSampleRate = 0.0;

    juce::AudioFormatManager formatManager;
    juce::AudioThumbnailCache thumbnailCache;
    juce::AudioThumbnail beforeThumbnail;
    juce::AudioThumbnail afterThumbnail;
    std::unique_ptr<WaveformOverlayView> waveformOverlay;
    juce::AudioBuffer<float> referenceBuffer;
    juce::AudioBuffer<float> beforeProcessedBuffer;
    juce::AudioBuffer<float> afterProcessedBuffer;
    bool referenceValid = false;
    juce::File referenceSourceFile;
    double referenceSampleRate = 44100.0;
    bool manualTargetEnabled = false;
    float manualTargetRmsDb = -18.0f;
    bool manualPeakEnabled = false;
    float manualPeakDbfs = -1.0f;
    bool updatingAdvancedControls = false;
    juce::File currentMSUFile;
    juce::String currentGameTitle;

    std::vector<BatchTrackEntry> batchTracks;
    std::unique_ptr<std::thread> batchWorker;
    std::atomic<double> batchProgressPending { 0.0 };
    std::atomic<bool> batchInProgress { false };
    std::atomic<bool> batchCancelRequested { false };
    int activeBatchPreviewRow = -1;
    juce::AudioBuffer<float> batchPreviewBeforeBuffer;
    juce::AudioBuffer<float> batchPreviewAfterBuffer;
    double batchPreviewSampleRate = 44100.0;
    bool batchPreviewActive = false;
    static constexpr int minContentHeight = 2100;
    juce::Component::SafePointer<juce::DialogWindow> batchProgressDialog;
    juce::Component::SafePointer<BatchProgressView> batchProgressView;
    bool batchProgressExportMode = true;
    int batchProgressTotalTracks = 0;

    bool isBatchBusy() const { return batchInProgress.load(); }
    bool isRowPreviewing(int rowNumber) const { return activeBatchPreviewRow == rowNumber; }
    bool canReplaceTracks() const { return requestTrackReplacement != nullptr; }
};
