#include "AudioLevelStudioComponent.h"

#include <algorithm>
#include <cmath>
#include <limits>

#include "../Audio/AudioImporter.h"
#include "../Core/AudioFileHandler.h"
#include "../Export/MSU1Exporter.h"

WaveformOverlayView::WaveformOverlayView(juce::AudioThumbnail& beforeThumb,
                                         juce::AudioThumbnail& afterThumb)
    : beforeThumbnail(beforeThumb), afterThumbnail(afterThumb)
{
}

void WaveformOverlayView::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colours::black.withAlpha(0.35f));
    auto area = getLocalBounds();

    const double totalLength = juce::jmax(beforeThumbnail.getTotalLength(),
                                          afterThumbnail.getTotalLength());

    if (totalLength <= 0.0)
    {
        g.setColour(juce::Colours::white.withAlpha(0.6f));
        g.drawText("Waveforms will appear once audio is analyzed.", area, juce::Justification::centred);
        return;
    }

    g.setColour(juce::Colours::dimgrey);
    g.drawRect(area);

    if (beforeThumbnail.getTotalLength() > 0.0)
    {
        g.setColour(beforeColour);
        beforeThumbnail.drawChannels(g, area, 0.0, totalLength, 1.0f);
    }

    if (afterThumbnail.getTotalLength() > 0.0)
    {
        g.setColour(afterColour);
        afterThumbnail.drawChannels(g, area, 0.0, totalLength, 1.0f);
    }

    if (std::isfinite(playbackCursorRatio) && totalLength > 0.0)
    {
        const int cursorX = area.getX() + static_cast<int>(playbackCursorRatio * area.getWidth());
        g.setColour(juce::Colours::yellow.withAlpha(0.85f));
        g.drawLine(static_cast<float>(cursorX), static_cast<float>(area.getY()),
                   static_cast<float>(cursorX), static_cast<float>(area.getBottom()), 1.5f);
    }
}

void WaveformOverlayView::setColours(juce::Colour before, juce::Colour after)
{
    beforeColour = before;
    afterColour = after;
    repaint();
}

void WaveformOverlayView::setPlaybackCursorRatio(double ratio)
{
    const double newRatio = std::isfinite(ratio)
        ? juce::jlimit(0.0, 1.0, ratio)
        : std::numeric_limits<double>::quiet_NaN();

    if ((std::isfinite(newRatio) && (!std::isfinite(playbackCursorRatio)
         || std::abs(newRatio - playbackCursorRatio) > 1e-4))
        || (!std::isfinite(newRatio) && std::isfinite(playbackCursorRatio)))
    {
        playbackCursorRatio = newRatio;
        repaint();
    }
}

namespace
{
    constexpr std::pair<int, const char*> presetDefinitions[] = {
        { 1, "Authentic (-20 RMS / -23 LUFS)" },
        { 2, "Balanced (-18 RMS / -21 LUFS)" },
        { 3, "Quieter (-23 RMS / -26 LUFS)" },
        { 4, "Louder (-16 RMS / -19 LUFS)" },
        { 5, "Maximum (Peak -1.0 dBFS)" }
    };

    enum BatchColumnIds
    {
        trackCol = 1,
        titleCol = 2,
        statusCol = 3,
        backupCol = 4,
        previewCol = 5,
        actionCol = 6
    };
}

class BatchPreviewButton : public juce::Component
{
public:
    explicit BatchPreviewButton(AudioLevelStudioComponent& ownerRef)
        : owner(ownerRef)
    {
        addAndMakeVisible(button);
        button.onClick = [this]
        {
            if (rowIndex >= 0)
                owner.handleTrackPreviewToggle(rowIndex);
        };
    }

    void setRowNumber(int newRow)
    {
        rowIndex = newRow;
        updateState();
    }

    void resized() override
    {
        button.setBounds(getLocalBounds().reduced(2));
    }

    void paint(juce::Graphics& g) override
    {
        juce::ignoreUnused(g);
        updateState();
    }

private:
    void updateState()
    {
        const auto* entry = owner.getBatchEntry(rowIndex);
        const bool valid = (entry != nullptr) && entry->pcmFile.existsAsFile();
        const bool busy = owner.isBatchBusy();
        button.setEnabled(valid && !busy);
        const bool previewing = owner.isRowPreviewing(rowIndex);
        button.setButtonText(previewing ? "Stop" : "Preview");
        button.setColour(juce::TextButton::buttonColourId,
                         previewing ? juce::Colours::darkred
                                    : juce::Colours::darkgrey);
    }

    AudioLevelStudioComponent& owner;
    juce::TextButton button { "Preview" };
    int rowIndex = -1;
};

class BatchReplaceButton : public juce::Component
{
public:
    explicit BatchReplaceButton(AudioLevelStudioComponent& ownerRef)
        : owner(ownerRef)
    {
        addAndMakeVisible(button);
        button.onClick = [this]
        {
            if (rowIndex >= 0)
                owner.handleTrackReplaceRequest(rowIndex);
        };
    }

    void setRowNumber(int newRow)
    {
        rowIndex = newRow;
        updateState();
    }

    void resized() override
    {
        button.setBounds(getLocalBounds().reduced(2));
    }

    void paint(juce::Graphics& g) override
    {
        juce::ignoreUnused(g);
        updateState();
    }

private:
    void updateState()
    {
        const bool valid = owner.getBatchEntry(rowIndex) != nullptr;
        button.setEnabled(valid && owner.canReplaceTracks() && !owner.isBatchBusy());
    }

    AudioLevelStudioComponent& owner;
    juce::TextButton button { "Replace" };
    int rowIndex = -1;
};

class BatchProgressView : public juce::Component
{
public:
    explicit BatchProgressView(std::function<void()> cancelHandler)
        : progressBar(progressValue), cancelCallback(std::move(cancelHandler))
    {
        addAndMakeVisible(summaryLabel);
        summaryLabel.setJustificationType(juce::Justification::centredLeft);
        summaryLabel.setColour(juce::Label::textColourId, juce::Colours::white);
        summaryLabel.setFont(juce::FontOptions(16.0f, juce::Font::bold));

        addAndMakeVisible(progressBar);
        progressBar.setPercentageDisplay(true);

        addAndMakeVisible(detailEditor);
        detailEditor.setMultiLine(true);
        detailEditor.setReadOnly(true);
        detailEditor.setScrollbarsShown(true);
        detailEditor.setCaretVisible(false);
        detailEditor.setColour(juce::TextEditor::backgroundColourId, juce::Colours::black.withAlpha(0.4f));
        detailEditor.setColour(juce::TextEditor::textColourId, juce::Colours::lightgrey);

        addAndMakeVisible(cancelButton);
        cancelButton.setButtonText("Cancel Batch");
        cancelButton.onClick = [this]
        {
            if (completionMode)
            {
                if (closeCallback)
                    closeCallback();
                return;
            }

            if (cancelCallback)
                cancelCallback();
        };
    }

    void setSummary(const juce::String& text)
    {
        summaryLabel.setText(text, juce::dontSendNotification);
    }

    void appendDetailLine(const juce::String& line)
    {
        if (line.isEmpty())
            return;

        detailLines.add(line);
        while (detailLines.size() > maxDetailLines)
            detailLines.remove(0);

        detailEditor.setText(detailLines.joinIntoString("\n"), juce::dontSendNotification);
        detailEditor.setCaretPosition(detailEditor.getTotalNumChars());
    }

    void setProgress(double ratio)
    {
        progressValue = juce::jlimit(0.0, 1.0, ratio);
        progressBar.repaint();
    }

    void setCancelEnabled(bool enabled)
    {
        if (completionMode)
            return;

        cancelButton.setEnabled(enabled);
    }

    void enterCompletionMode(std::function<void()> closeHandler)
    {
        completionMode = true;
        closeCallback = std::move(closeHandler);
        cancelButton.setButtonText("Close");
        cancelButton.setEnabled(true);
    }

    void resized() override
    {
        auto bounds = getLocalBounds().reduced(12);
        auto summaryArea = bounds.removeFromTop(32);
        summaryLabel.setBounds(summaryArea);

        bounds.removeFromTop(4);
        progressBar.setBounds(bounds.removeFromTop(24));

        bounds.removeFromTop(8);
        auto bottomArea = bounds.removeFromBottom(36);
        cancelButton.setBounds(bottomArea.removeFromRight(140));
        bottomArea.removeFromRight(8);
        bounds.removeFromBottom(8);
        detailEditor.setBounds(bounds);
    }

private:
    juce::Label summaryLabel;
    double progressValue = 0.0;
    juce::ProgressBar progressBar;
    juce::TextEditor detailEditor;
    juce::TextButton cancelButton { "Cancel" };
    juce::StringArray detailLines;
    std::function<void()> cancelCallback;
    std::function<void()> closeCallback;
    bool completionMode = false;
    static constexpr int maxDetailLines = 6;
};

AudioLevelStudioComponent::AudioLevelStudioComponent(MSUProjectState& state,
                                                                                                         BeforeAfterPreviewPlayer& previewPlayer)
        : projectState(state),
            beforeAfterPlayer(previewPlayer),
            thumbnailCache(8),
      beforeThumbnail(512, formatManager, thumbnailCache),
      afterThumbnail(512, formatManager, thumbnailCache)
{
    formatManager.registerBasicFormats();
    waveformOverlay = std::make_unique<WaveformOverlayView>(beforeThumbnail, afterThumbnail);
    waveformOverlay->setInterceptsMouseClicks(false, false);

    addAndMakeVisible(contentViewport);
    contentViewport.setViewedComponent(&contentHolder, false);
    contentViewport.setScrollBarsShown(true, false);
    contentViewport.setScrollBarThickness(12);

    headerLabel.setText("Audio Level Studio", juce::dontSendNotification);
    headerLabel.setFont(juce::FontOptions(20.0f, juce::Font::bold));

    descriptionLabel.setText(
        "Analyze loudness, apply presets, and prepare PCM batches with non-destructive backups.",
        juce::dontSendNotification);
    descriptionLabel.setColour(juce::Label::textColourId, juce::Colours::lightgrey);

    presetLabel.setText("Preset", juce::dontSendNotification);
    presetLabel.setJustificationType(juce::Justification::centredLeft);

    for (const auto& preset : presetDefinitions)
        presetSelector.addItem(preset.second, preset.first);
    presetSelector.setSelectedId(1);
    presetSelector.onChange = [this]
    {
        generatePresetPreview();
    };

    waveformPlaceholder.setText("Waveform overlay will appear once audio loads.", juce::dontSendNotification);
    waveformPlaceholder.setJustificationType(juce::Justification::centred);
    waveformLegendLabel.setJustificationType(juce::Justification::centredRight);
    waveformLegendLabel.setText("Before = Green, After = Aqua", juce::dontSendNotification);
    waveformLegendLabel.setColour(juce::Label::textColourId, juce::Colours::lightgrey);

    contentHolder.addAndMakeVisible(headerLabel);
    contentHolder.addAndMakeVisible(descriptionLabel);
    contentHolder.addAndMakeVisible(presetLabel);
    contentHolder.addAndMakeVisible(presetSelector);
    contentHolder.addAndMakeVisible(playBeforeButton);
    contentHolder.addAndMakeVisible(playAfterButton);

    contentHolder.addAndMakeVisible(fileInfoGroup);
    fileInfoGroup.addAndMakeVisible(fileNameLabel);
    fileInfoGroup.addAndMakeVisible(fileDetailLabel);
    fileInfoGroup.addAndMakeVisible(loopInfoLabel);

    auto styleMetricLabel = [](juce::Label& label, const juce::String& text)
    {
        label.setText(text, juce::dontSendNotification);
        label.setJustificationType(juce::Justification::centredLeft);
    };

    contentHolder.addAndMakeVisible(metricsGroup);
    metricsGroup.addAndMakeVisible(rmsLabel);
    metricsGroup.addAndMakeVisible(lufsLabel);
    metricsGroup.addAndMakeVisible(peakLabel);
    metricsGroup.addAndMakeVisible(headroomLabel);
    metricsGroup.addAndMakeVisible(statsHintLabel);
    styleMetricLabel(rmsLabel, "RMS: --");
    styleMetricLabel(lufsLabel, "LUFS (est): --");
    styleMetricLabel(peakLabel, "Peak: --");
    styleMetricLabel(headroomLabel, "Headroom: --");
    statsHintLabel.setJustificationType(juce::Justification::centredLeft);
    statsHintLabel.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
    statsHintLabel.setText("Load audio to begin.", juce::dontSendNotification);

    contentHolder.addAndMakeVisible(waveformGroup);
    waveformGroup.addAndMakeVisible(*waveformOverlay);
    waveformGroup.addAndMakeVisible(waveformPlaceholder);
    waveformGroup.addAndMakeVisible(waveformLegendLabel);
    updateWaveformLegend();

    contentHolder.addAndMakeVisible(advancedGroup);

    manualTargetSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    manualTargetSlider.setRange(-30.0, -10.0, 0.1);
    manualTargetSlider.setTextBoxStyle(juce::Slider::NoTextBox, true, 0, 0);
    manualTargetSlider.setEnabled(false);
    manualTargetSlider.onValueChange = [this]
    {
        manualTargetRmsDb = static_cast<float>(manualTargetSlider.getValue());
        updateManualTargetValueLabel();
        updateBatchPresetNoteText();
        if (!updatingAdvancedControls && manualTargetEnabled)
            generatePresetPreview();
    };

    manualTargetToggle.onClick = [this]
    {
        manualTargetEnabled = manualTargetToggle.getToggleState();
        manualTargetSlider.setEnabled(manualTargetEnabled);
        manualTargetValueLabel.setEnabled(manualTargetEnabled);
        updateManualTargetValueLabel();
        updateBatchPresetNoteText();
        if (!updatingAdvancedControls)
            generatePresetPreview();
    };
    manualTargetValueLabel.setJustificationType(juce::Justification::centredRight);
    manualTargetValueLabel.setColour(juce::Label::textColourId, juce::Colours::lightgrey);

    peakCeilingSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    peakCeilingSlider.setRange(-12.0, -0.1, 0.1);
    peakCeilingSlider.setTextBoxStyle(juce::Slider::NoTextBox, true, 0, 0);
    peakCeilingSlider.setEnabled(false);
    peakCeilingSlider.setValue(manualPeakDbfs);
    peakCeilingSlider.onValueChange = [this]
    {
        manualPeakDbfs = static_cast<float>(peakCeilingSlider.getValue());
        updatePeakTargetValueLabel();
        updateBatchPresetNoteText();
        if (!updatingAdvancedControls && manualPeakEnabled)
            generatePresetPreview();
    };

    peakCeilingToggle.onClick = [this]
    {
        manualPeakEnabled = peakCeilingToggle.getToggleState();
        peakCeilingSlider.setEnabled(manualPeakEnabled);
        peakCeilingValueLabel.setEnabled(manualPeakEnabled);

        updatePeakTargetValueLabel();
        updateBatchPresetNoteText();

        if (!updatingAdvancedControls)
            generatePresetPreview();
    };
    peakCeilingValueLabel.setJustificationType(juce::Justification::centredRight);
    peakCeilingValueLabel.setColour(juce::Label::textColourId, juce::Colours::lightgrey);

    advancedHelpLabel.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
    advancedHelpLabel.setJustificationType(juce::Justification::centredLeft);
    advancedHelpLabel.setText("Manual RMS or peak targets override presets during export.",
                              juce::dontSendNotification);

    advancedGroup.addAndMakeVisible(manualTargetToggle);
    advancedGroup.addAndMakeVisible(manualTargetSlider);
    advancedGroup.addAndMakeVisible(manualTargetValueLabel);
    advancedGroup.addAndMakeVisible(peakCeilingToggle);
    advancedGroup.addAndMakeVisible(peakCeilingSlider);
    advancedGroup.addAndMakeVisible(peakCeilingValueLabel);
    advancedGroup.addAndMakeVisible(advancedHelpLabel);

    contentHolder.addAndMakeVisible(batchGroup);
    batchGroup.addAndMakeVisible(batchStatusLabel);
    batchGroup.addAndMakeVisible(loadMSUButton);
    batchGroup.addAndMakeVisible(batchTrackTable);
    batchGroup.addAndMakeVisible(batchPresetNote);
    batchGroup.addAndMakeVisible(batchPreviewNote);
    batchGroup.addAndMakeVisible(batchExportButton);
    batchGroup.addAndMakeVisible(batchProgressBar);
    batchGroup.addAndMakeVisible(batchCancelButton);

    batchStatusLabel.setFont(juce::FontOptions(16.0f, juce::Font::bold));
    batchStatusLabel.setJustificationType(juce::Justification::centredLeft);
    batchStatusLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    batchStatusLabel.setText("No ROM loaded", juce::dontSendNotification);

    loadMSUButton.onClick = [this]
    {
        if (requestExternalMSULoad)
            requestExternalMSULoad();
    };

    batchTrackTable.setModel(this);
    batchTrackTable.setMultipleSelectionEnabled(true);
    batchTrackTable.setColour(juce::ListBox::outlineColourId, juce::Colours::grey);
    batchTrackTable.setOutlineThickness(1);
    auto& header = batchTrackTable.getHeader();
    header.addColumn("Track", BatchColumnIds::trackCol, 60, 50, 80, juce::TableHeaderComponent::notResizable);
    header.addColumn("Title / File Name", BatchColumnIds::titleCol, 300, 150, -1, juce::TableHeaderComponent::defaultFlags);
    header.addColumn("Status", BatchColumnIds::statusCol, 80, 60, 120, juce::TableHeaderComponent::notResizable);
    header.addColumn("Backup Exists", BatchColumnIds::backupCol, 120, 90, 160, juce::TableHeaderComponent::notResizable);
    header.addColumn("Preview", BatchColumnIds::previewCol, 100, 80, 140, juce::TableHeaderComponent::notResizable);
    header.addColumn("Action", BatchColumnIds::actionCol, 100, 80, 140, juce::TableHeaderComponent::notResizable);

    batchPresetNote.setJustificationType(juce::Justification::centredLeft);
    batchPresetNote.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
    updateBatchPresetNoteText();

    batchPreviewNote.setJustificationType(juce::Justification::centred);
    batchPreviewNote.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
    batchPreviewNote.setText("Preview playback has preset/manual settings applied", juce::dontSendNotification);

    batchProgressBar.setVisible(false);
    batchProgressBar.setPercentageDisplay(true);

    batchCancelButton.setVisible(false);
    batchCancelButton.onClick = [this]
    {
        cancelBatchOperation();
    };

    batchExportButton.setButtonText("Batch export w/ Preset/Manual Settings applied.");
    batchExportButton.onClick = [this]
    {
        runBatchOperation(true);
    };

    playBeforeButton.onClick = [this]
    {
        handlePreviewButtonPress(BeforeAfterPreviewPlayer::Target::Before);
    };

    playAfterButton.onClick = [this]
    {
        handlePreviewButtonPress(BeforeAfterPreviewPlayer::Target::After);
    };

    refreshFromProjectState();
    syncBeforeAfterBuffers();
    syncAdvancedControls();
    updateBatchSelectionSummary();
    updateBatchButtons();
    startTimerHz(10);
}

AudioLevelStudioComponent::~AudioLevelStudioComponent()
{
    stopTimer();
    stopPreviewPlayback();
    if (batchWorker && batchWorker->joinable())
        batchWorker->join();
    batchTrackTable.setModel(nullptr);
}

void AudioLevelStudioComponent::resized()
{
    auto area = getLocalBounds();
    contentViewport.setBounds(area);

    const int contentWidth = area.getWidth();
    const int contentHeight = juce::jmax(area.getHeight(), minContentHeight);
    contentHolder.setSize(contentWidth, contentHeight);
    contentHolder.setTopLeftPosition(0, 0);

    auto bounds = contentHolder.getLocalBounds().reduced(16);

    headerLabel.setBounds(bounds.removeFromTop(28));
    descriptionLabel.setBounds(bounds.removeFromTop(24));

    auto presetRow = bounds.removeFromTop(32);
    presetLabel.setBounds(presetRow.removeFromLeft(120));
    presetSelector.setBounds(presetRow.removeFromLeft(200));
    presetRow.removeFromLeft(8);
    playBeforeButton.setBounds(presetRow.removeFromLeft(140));
    presetRow.removeFromLeft(8);
    playAfterButton.setBounds(presetRow.removeFromLeft(140));

    bounds.removeFromTop(12);
    auto infoArea = bounds.removeFromTop(90);
    fileInfoGroup.setBounds(infoArea);
    auto infoBounds = fileInfoGroup.getLocalBounds().reduced(12);
    fileNameLabel.setBounds(infoBounds.removeFromTop(20));
    fileDetailLabel.setBounds(infoBounds.removeFromTop(18));
    loopInfoLabel.setBounds(infoBounds.removeFromTop(18));

    bounds.removeFromTop(12);
    auto metricsArea = bounds.removeFromTop(110);
    metricsGroup.setBounds(metricsArea);
    auto metricsBounds = metricsGroup.getLocalBounds().reduced(12);
    auto metricsRow1 = metricsBounds.removeFromTop(24);
    rmsLabel.setBounds(metricsRow1.removeFromLeft(metricsRow1.getWidth() / 2));
    peakLabel.setBounds(metricsRow1);

    auto metricsRow2 = metricsBounds.removeFromTop(24);
    lufsLabel.setBounds(metricsRow2.removeFromLeft(metricsRow2.getWidth() / 2));
    headroomLabel.setBounds(metricsRow2);

    statsHintLabel.setBounds(metricsBounds.removeFromTop(20));

    bounds.removeFromTop(12);
    const int waveformHeight = juce::jmin(360, bounds.getHeight());
    auto waveformArea = bounds.removeFromTop(waveformHeight);
    waveformGroup.setBounds(waveformArea);
    auto waveformGroupBounds = waveformGroup.getLocalBounds();
    auto legendArea = waveformGroupBounds.removeFromBottom(20);
    auto waveformBounds = waveformGroupBounds.reduced(12);
    if (waveformOverlay != nullptr)
        waveformOverlay->setBounds(waveformBounds);
    waveformPlaceholder.setBounds(waveformBounds);
    waveformLegendLabel.setBounds(legendArea.reduced(8, 0));

    bounds.removeFromTop(12);
    constexpr int manualSettingsSectionHeight = 192;
    const int advancedHeight = juce::jmin(manualSettingsSectionHeight, bounds.getHeight());
    auto advancedArea = bounds.removeFromTop(advancedHeight);
    advancedGroup.setBounds(advancedArea);
    auto advancedBounds = advancedGroup.getLocalBounds().reduced(16);
    auto manualRow = advancedBounds.removeFromTop(24);
    manualTargetToggle.setBounds(manualRow.removeFromLeft(200));
    manualRow.removeFromLeft(8);
    manualTargetValueLabel.setBounds(manualRow.removeFromRight(120));
    manualRow = manualRow.withTrimmedRight(8);
    manualTargetSlider.setBounds(advancedBounds.removeFromTop(36));

    advancedBounds.removeFromTop(8);
    auto peakRow = advancedBounds.removeFromTop(24);
    peakCeilingToggle.setBounds(peakRow.removeFromLeft(200));
    peakRow.removeFromLeft(8);
    peakCeilingValueLabel.setBounds(peakRow.removeFromRight(120));
    peakRow = peakRow.withTrimmedRight(8);
    peakCeilingSlider.setBounds(advancedBounds.removeFromTop(36));

    advancedBounds.removeFromTop(8);
    auto helpRow = advancedBounds.removeFromTop(24);
    advancedHelpLabel.setBounds(helpRow);

    bounds.removeFromTop(12);
    const int minBatchHeight = 620;
    if (bounds.getHeight() < minBatchHeight)
    {
        const int extra = minBatchHeight - bounds.getHeight();
        contentHolder.setSize(contentHolder.getWidth(), contentHolder.getHeight() + extra);
        bounds.setHeight(minBatchHeight);
    }
    batchGroup.setBounds(bounds);
    auto batchBounds = batchGroup.getLocalBounds().reduced(16);
    auto loadRow = batchBounds.removeFromTop(32);
    loadMSUButton.setBounds(loadRow.reduced(4));

    auto exportRow = batchBounds.removeFromTop(36);
    batchExportButton.setBounds(exportRow.reduced(4));

    batchBounds.removeFromTop(4);
    auto previewNoteRow = batchBounds.removeFromTop(24);
    batchPreviewNote.setBounds(previewNoteRow.reduced(4));

    batchBounds.removeFromTop(4);
    auto statusRow = batchBounds.removeFromTop(32);
    batchStatusLabel.setBounds(statusRow.reduced(4));

    batchBounds.removeFromTop(8);
    auto listArea = batchBounds;
    const int footerHeight = 140;
    listArea.removeFromBottom(footerHeight);
    batchTrackTable.setBounds(listArea.reduced(4));

    auto footerBounds = batchBounds.removeFromBottom(footerHeight);
    auto selectRow = footerBounds.removeFromTop(24);
    batchCancelButton.setBounds(selectRow.removeFromRight(140));

    footerBounds.removeFromTop(8);
    auto progressRow = footerBounds.removeFromTop(24);
    batchProgressBar.setBounds(progressRow);

    footerBounds.removeFromTop(8);
    auto noteRow = footerBounds.removeFromTop(32);
    batchPresetNote.setBounds(noteRow);
}

void AudioLevelStudioComponent::refreshFromProjectState()
{
    if (!projectState.hasAudio())
    {
        stopActiveTrackPreview();
        clearPreview();
        clearWaveformData();
        hasStats = false;
        stopPreviewPlayback();

        fileNameLabel.setText("No audio loaded", juce::dontSendNotification);
        fileDetailLabel.setText("Load a PCM/MSU/SPC file to begin.", juce::dontSendNotification);
        loopInfoLabel.setText("Loop points: --", juce::dontSendNotification);
        rmsLabel.setText("RMS: --", juce::dontSendNotification);
        lufsLabel.setText("LUFS (est): --", juce::dontSendNotification);
        peakLabel.setText("Peak: --", juce::dontSendNotification);
        headroomLabel.setText("Headroom: --", juce::dontSendNotification);
        statsHintLabel.setText("Load audio and choose a preset to generate a preview.", juce::dontSendNotification);
        syncBeforeAfterBuffers();
        return;
    }

    const auto sourceFile = projectState.getSourceFile();
    if (sourceFile.existsAsFile())
        fileNameLabel.setText(sourceFile.getFileName(), juce::dontSendNotification);
    else
        fileNameLabel.setText("Unsaved buffer", juce::dontSendNotification);

    const auto lengthSeconds = projectState.getLengthInSeconds();
    juce::String details;
    details << projectState.getNumChannels() << " ch | "
            << formatLengthString(lengthSeconds) << " | "
            << juce::String(juce::roundToInt(projectState.getSampleRate())) << " Hz";
    fileDetailLabel.setText(details, juce::dontSendNotification);

    if (projectState.hasLoopPoints())
        loopInfoLabel.setText(
            formatLoopRange(projectState.getLoopStart(), projectState.getLoopEnd(), projectState.getSampleRate()),
            juce::dontSendNotification);
    else
        loopInfoLabel.setText("Loop points: --", juce::dontSendNotification);

    updateWaveformThumbnails(true);
    generatePresetPreview();

    if (hasStats)
    {
        rmsLabel.setText("RMS: " + formatDbValue(latestStats.rmsDb), juce::dontSendNotification);
        lufsLabel.setText("LUFS (est): " + formatLufsEstimate(latestStats.rmsDb), juce::dontSendNotification);
        peakLabel.setText("Peak: " + formatDbValue(latestStats.peakDb), juce::dontSendNotification);
        headroomLabel.setText("Headroom: " + formatHeadroom(latestStats.peakDb), juce::dontSendNotification);
    }
    else
    {
        rmsLabel.setText("RMS: --", juce::dontSendNotification);
        lufsLabel.setText("LUFS (est): --", juce::dontSendNotification);
        peakLabel.setText("Peak: --", juce::dontSendNotification);
        headroomLabel.setText("Headroom: --", juce::dontSendNotification);
    }

    syncBeforeAfterBuffers();
    syncAdvancedControls();
}

void AudioLevelStudioComponent::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colours::black.withAlpha(0.2f));
}
juce::String AudioLevelStudioComponent::formatLengthString(double seconds) const
{
    if (seconds <= 0.0 || !std::isfinite(seconds))
        return "--";

    const auto mins = static_cast<int>(seconds / 60.0);
    const auto secs = static_cast<int>(std::round(std::fmod(seconds, 60.0)));
    juce::String lengthString;
    lengthString << mins << "m " << secs << "s";
    return lengthString;
}

juce::String AudioLevelStudioComponent::formatLoopRange(int64 loopStart,
                                                        int64 loopEnd,
                                                        double sampleRate) const
{
    if (loopStart < 0 || loopEnd <= loopStart || sampleRate <= 0.0)
        return "Loop points: --";

    const auto startSeconds = loopStart / sampleRate;
    const auto endSeconds = loopEnd / sampleRate;
    juce::String text = "Loop points: ";
    text << formatLengthString(startSeconds) << " → " << formatLengthString(endSeconds);
    return text;
}

juce::String AudioLevelStudioComponent::formatDbValue(float value) const
{
    if (!std::isfinite(value))
        return "--";

    return juce::String(value, 1) + " dB";
}

juce::String AudioLevelStudioComponent::formatLufsEstimate(float rmsDb) const
{
    if (!std::isfinite(rmsDb))
        return "--";

    // Simple heuristic: LUFS is typically a few dB lower than RMS for music content.
    const float lufsEstimate = rmsDb - 3.0f;
    return juce::String(lufsEstimate, 1) + " LUFS";
}

juce::String AudioLevelStudioComponent::formatHeadroom(float peakDb) const
{
    if (!std::isfinite(peakDb))
        return "--";

    const float headroom = -1.0f - peakDb;
    juce::String text;
    text << juce::String(headroom, 1) << " dB to -1 dBFS";
    if (headroom < 0.0f)
        text << " (CLIPPING)";
    return text;
}

void AudioLevelStudioComponent::updateWaveformThumbnails(bool forceReferenceReset)
{
    const bool hasProjectAudio = projectState.hasAudio();
    const bool hasReferenceAudio = referenceValid && referenceBuffer.getNumSamples() > 0 && referenceSampleRate > 0.0;

    if (!hasProjectAudio && !hasReferenceAudio)
    {
        clearWaveformData();
        return;
    }

    const juce::AudioBuffer<float>* projectBuffer = hasProjectAudio ? &projectState.getAudioBuffer() : nullptr;
    const double projectSampleRate = hasProjectAudio ? projectState.getSampleRate() : 0.0;

    if (projectBuffer != nullptr)
    {
        if (projectSampleRate <= 0.0 || projectBuffer->getNumSamples() == 0)
        {
            clearWaveformData();
            return;
        }

        if (forceReferenceReset || !referenceValid)
            refreshReferenceBuffer(*projectBuffer, projectSampleRate, projectState.getSourceFile());
    }
    else if (!hasReferenceAudio)
    {
        clearWaveformData();
        return;
    }

    juce::AudioBuffer<float> beforeDisplayBuffer;
    juce::AudioBuffer<float> afterDisplayBuffer;
    const int64 trimStart = projectState.getTrimStart();
    const int64 paddingSamples = projectState.getPaddingSamples();
    const int64 loopEnd = projectState.hasLoopPoints() ? projectState.getLoopEnd() : -1;

    if (referenceValid && referenceBuffer.getNumSamples() > 0)
        rebuildTrimPadBuffer(referenceBuffer, beforeDisplayBuffer, trimStart, paddingSamples, loopEnd);
    else
        beforeDisplayBuffer.setSize(0, 0);

    const juce::AudioBuffer<float>* afterSource = nullptr;
    double afterSampleRate = 0.0;
    if (previewValid && previewBuffer.getNumSamples() > 0)
    {
        afterSource = &previewBuffer;
        afterSampleRate = previewSampleRate;
    }
    else if (projectBuffer != nullptr)
    {
        afterSource = projectBuffer;
        afterSampleRate = projectSampleRate;
    }

    if (afterSource != nullptr)
        rebuildTrimPadBuffer(*afterSource, afterDisplayBuffer, trimStart, paddingSamples, loopEnd);
    else
        afterDisplayBuffer.setSize(0, 0);

    if (beforeDisplayBuffer.getNumSamples() > 0 && referenceSampleRate > 0.0)
    {
        beforeThumbnail.reset(beforeDisplayBuffer.getNumChannels(), referenceSampleRate);
        beforeThumbnail.addBlock(0, beforeDisplayBuffer, 0, beforeDisplayBuffer.getNumSamples());
    }
    else
    {
        const double fallbackRate = referenceSampleRate > 0.0 ? referenceSampleRate : projectSampleRate;
        beforeThumbnail.reset(0, fallbackRate > 0.0 ? fallbackRate : 44100.0);
    }

    const double resolvedAfterRate = afterSampleRate > 0.0
        ? afterSampleRate
        : (projectSampleRate > 0.0 ? projectSampleRate : referenceSampleRate);

    if (afterDisplayBuffer.getNumSamples() > 0 && resolvedAfterRate > 0.0)
    {
        afterThumbnail.reset(afterDisplayBuffer.getNumChannels(), resolvedAfterRate);
        afterThumbnail.addBlock(0, afterDisplayBuffer, 0, afterDisplayBuffer.getNumSamples());
    }
    else
    {
        const double fallbackRate = resolvedAfterRate > 0.0 ? resolvedAfterRate : 44100.0;
        afterThumbnail.reset(0, fallbackRate);
    }

    const bool hasWaveform = beforeThumbnail.getTotalLength() > 0.0 || afterThumbnail.getTotalLength() > 0.0;
    waveformPlaceholder.setVisible(!hasWaveform);
    waveformLegendLabel.setVisible(hasWaveform);
    if (waveformOverlay != nullptr)
        waveformOverlay->repaint();
}

void AudioLevelStudioComponent::refreshReferenceBuffer(const juce::AudioBuffer<float>& buffer,
                                                       double sampleRate,
                                                       const juce::File& sourceFile)
{
    referenceBuffer.makeCopyOf(buffer);
    referenceSampleRate = sampleRate;
    referenceValid = true;
    if (sourceFile.existsAsFile())
        referenceSourceFile = sourceFile;
    else
        referenceSourceFile = projectState.getSourceFile();

    beforeThumbnail.reset(buffer.getNumChannels(), sampleRate);
    beforeThumbnail.addBlock(0, referenceBuffer, 0, referenceBuffer.getNumSamples());
}

void AudioLevelStudioComponent::clearWaveformData()
{
    clearPreview();
    beforeThumbnail.reset(0, 44100.0);
    afterThumbnail.reset(0, 44100.0);
    referenceBuffer.setSize(0, 0);
    beforeProcessedBuffer.setSize(0, 0);
    afterProcessedBuffer.setSize(0, 0);
    referenceValid = false;
    waveformPlaceholder.setVisible(true);
    waveformLegendLabel.setVisible(false);
    if (waveformOverlay != nullptr)
    {
        waveformOverlay->setPlaybackCursorRatio(std::numeric_limits<double>::quiet_NaN());
        waveformOverlay->repaint();
    }
}

void AudioLevelStudioComponent::clearPreview()
{
    previewBuffer.setSize(0, 0);
    previewValid = false;
    pendingPreviewGainDb = 0.0f;
    pendingPresetDescription.clear();
    previewSampleRate = 0.0;
    projectState.setNormalizationGain(0.0f);
    updateWaveformLegend();
    syncBeforeAfterBuffers();
}

void AudioLevelStudioComponent::updateWaveformLegend()
{
    waveformLegendLabel.setText(
        previewValid ? "Before = Green, Preview = Aqua"
                     : "Before = Green, After = Aqua",
        juce::dontSendNotification);
}

juce::String AudioLevelStudioComponent::getActivePresetDisplayName() const
{
    return hasManualOverridesEnabled() ? juce::String("Manual") : getSelectedPresetLabel();
}

bool AudioLevelStudioComponent::hasManualOverridesActive() const
{
    return hasManualOverridesEnabled();
}

bool AudioLevelStudioComponent::calculateActivePresetGain(float& gainDb, juce::String& description)
{
    if (!projectState.hasAudio())
        return false;

    if (!hasStats)
    {
        latestStats = NormalizationAnalyzer::analyzeBuffer(projectState.getAudioBuffer());
        hasStats = true;
    }

    if (!calculatePresetGain(gainDb, description))
        return false;

    projectState.setNormalizationGain(gainDb);
    const float targetRms = getSelectedPresetTargetRms();
    if (std::isfinite(targetRms))
        projectState.setTargetRMS(targetRms);
    pendingPresetDescription = description;
    pendingPreviewGainDb = gainDb;
    return true;
}

bool AudioLevelStudioComponent::calculatePresetGain(float& gainDb,
                                                    juce::String& description,
                                                    std::optional<NormalizationAnalyzer::AudioStats> statsOverride,
                                                    const PresetSettings* presetOverride) const
{
    const bool statsAvailable = statsOverride.has_value() || hasStats;
    if (!statsAvailable)
        return false;

    const auto stats = statsOverride.value_or(latestStats);
    const auto settings = presetOverride != nullptr ? *presetOverride : getCurrentPresetSettings();
    return calculatePresetGainForSettings(settings, stats, gainDb, description);
}

bool AudioLevelStudioComponent::calculatePresetGainForSettings(const PresetSettings& settings,
                                                               const NormalizationAnalyzer::AudioStats& stats,
                                                               float& gainDb,
                                                               juce::String& description)
{
    const bool presetIsPeakOnly = (settings.presetId == 5 && !settings.manualTargetEnabled);

    float rmsGain = std::numeric_limits<float>::quiet_NaN();
    juce::String rmsDescription;
    if (settings.manualTargetEnabled || !presetIsPeakOnly)
    {
        const float targetRms = getSelectedPresetTargetRms(settings);
        if (!std::isfinite(targetRms))
            return false;

        rmsGain = NormalizationAnalyzer::calculateGainToTarget(stats.rmsDb, targetRms);
        if (!std::isfinite(rmsGain))
            return false;

        rmsDescription = juce::String(targetRms, 1) + " dB RMS";
    }

    float peakGain = std::numeric_limits<float>::quiet_NaN();
    juce::String peakDescription;
    if (settings.manualPeakEnabled || presetIsPeakOnly)
    {
        const float peakTarget = settings.manualPeakEnabled ? settings.manualPeakDbfs : -1.0f;
        peakGain = peakTarget - stats.peakDb;
        if (!std::isfinite(peakGain))
            return false;

        peakDescription = "Peak " + juce::String(peakTarget, 1) + " dBFS";
    }

    if (!std::isfinite(rmsGain) && !std::isfinite(peakGain))
        return false;

    if (std::isfinite(rmsGain) && std::isfinite(peakGain))
    {
        const bool cappedByPeak = rmsGain > peakGain;
        gainDb = cappedByPeak ? peakGain : rmsGain;
        description = cappedByPeak
            ? rmsDescription + " (capped by " + peakDescription + ")"
            : rmsDescription + " & " + peakDescription;
        return std::isfinite(gainDb);
    }

    if (std::isfinite(rmsGain))
    {
        gainDb = rmsGain;
        description = rmsDescription;
        return true;
    }

    gainDb = peakGain;
    description = peakDescription;
    return std::isfinite(gainDb);
}

void AudioLevelStudioComponent::generatePresetPreview()
{
    clearPreview();

    if (!projectState.hasAudio())
        return;

    latestStats = NormalizationAnalyzer::analyzeBuffer(projectState.getAudioBuffer());
    hasStats = true;

    float gainDb = 0.0f;
    juce::String description;
    if (!calculatePresetGain(gainDb, description))
    {
        statsHintLabel.setText("Unable to calculate preset gain.", juce::dontSendNotification);
        applyPreviewBuffer(projectState.getAudioBuffer(), projectState.getSampleRate(), description, 0.0f);
        return;
    }

    if (!std::isfinite(gainDb) || std::abs(gainDb) < 0.05f)
    {
        statsHintLabel.setText("Preset already matches the current level.", juce::dontSendNotification);
        applyPreviewBuffer(projectState.getAudioBuffer(), projectState.getSampleRate(), description, 0.0f);
        return;
    }

    previewBuffer.makeCopyOf(projectState.getAudioBuffer());
    NormalizationAnalyzer::applyGain(previewBuffer, gainDb);
    applyPreviewBuffer(previewBuffer, projectState.getSampleRate(), description, gainDb);
}

void AudioLevelStudioComponent::applyPreviewBuffer(const juce::AudioBuffer<float>& buffer,
                                                   double sampleRate,
                                                   const juce::String& description,
                                                   float gainDb,
                                                   bool updateProjectState)
{
    previewBuffer.makeCopyOf(buffer);
    pendingPreviewGainDb = gainDb;
    pendingPresetDescription = description;
    previewValid = true;
    previewSampleRate = sampleRate;
    if (updateProjectState)
        projectState.setNormalizationGain(gainDb);
    updateWaveformLegend();
    updateWaveformThumbnails(false);
    waveformPlaceholder.setVisible(false);
    waveformLegendLabel.setVisible(true);
    if (waveformOverlay != nullptr)
        waveformOverlay->repaint();

    juce::String message;
        message << "Previewing " << juce::String(pendingPreviewGainDb, 2) << " dB toward "
            << pendingPresetDescription << ". This gain will be applied automatically when exporting.";
    statsHintLabel.setText(message, juce::dontSendNotification);

    syncBeforeAfterBuffers();
}

AudioLevelStudioComponent::PresetSettings AudioLevelStudioComponent::getCurrentPresetSettings() const
{
    PresetSettings settings;
    settings.presetId = presetSelector.getSelectedId();
    settings.manualTargetEnabled = manualTargetEnabled;
    settings.manualTargetRmsDb = manualTargetRmsDb;
    settings.manualPeakEnabled = manualPeakEnabled;
    settings.manualPeakDbfs = manualPeakDbfs;
    return settings;
}

float AudioLevelStudioComponent::getSelectedPresetTargetRms() const
{
    return getSelectedPresetTargetRms(getCurrentPresetSettings());
}

float AudioLevelStudioComponent::getSelectedPresetTargetRms(const PresetSettings& settings)
{
    if (settings.manualTargetEnabled)
        return settings.manualTargetRmsDb;

    switch (settings.presetId)
    {
        case 1: return -20.0f; // Authentic
        case 2: return -18.0f; // Balanced
        case 3: return -23.0f; // Quieter
        case 4: return -16.0f; // Louder
        default: break;
    }
    return std::numeric_limits<float>::quiet_NaN();
}

void AudioLevelStudioComponent::applyGainNonDestructively(float gainDb)
{
    if (!std::isfinite(gainDb) || juce::approximatelyEqual(gainDb, 0.0f))
        return;

    auto& buffer = projectState.getAudioBuffer();
    NormalizationAnalyzer::applyGain(buffer, gainDb);
    projectState.setModified(true);
    latestStats = NormalizationAnalyzer::analyzeBuffer(buffer);
    hasStats = true;
    rmsLabel.setText("RMS: " + formatDbValue(latestStats.rmsDb), juce::dontSendNotification);
    lufsLabel.setText("LUFS (est): " + formatLufsEstimate(latestStats.rmsDb), juce::dontSendNotification);
    peakLabel.setText("Peak: " + formatDbValue(latestStats.peakDb), juce::dontSendNotification);
    headroomLabel.setText("Headroom: " + formatHeadroom(latestStats.peakDb), juce::dontSendNotification);
    updateWaveformThumbnails(false);
    syncBeforeAfterBuffers();
}

void AudioLevelStudioComponent::rebuildTrimPadBuffer(const juce::AudioBuffer<float>& source,
                                                     juce::AudioBuffer<float>& destination,
                                                     int64 trimStart,
                                                     int64 paddingSamples,
                                                     int64 loopEndSample)
{
    const int numChannels = source.getNumChannels();
    const int sourceSamples = source.getNumSamples();

    if (numChannels <= 0)
    {
        destination.setSize(0, 0);
        return;
    }

    const int startSample = static_cast<int>(juce::jlimit<int64>(0, sourceSamples, trimStart));
    int64 effectiveLoopEnd = sourceSamples;
    if (loopEndSample > 0)
        effectiveLoopEnd = juce::jlimit<int64>(startSample, sourceSamples, loopEndSample);

    const int trimmedSamples = static_cast<int>(juce::jmax<int64>(0, effectiveLoopEnd - startSample));
    const int padding = static_cast<int>(juce::jlimit<int64>(0, std::numeric_limits<int>::max(), paddingSamples));
    const int totalSamples = padding + trimmedSamples;

    destination.setSize(numChannels, totalSamples, false, false, true);
    destination.clear();

    if (trimmedSamples <= 0)
        return;

    for (int ch = 0; ch < numChannels; ++ch)
        destination.copyFrom(ch, padding, source, ch, startSample, trimmedSamples);
}

void AudioLevelStudioComponent::rebuildProcessedBuffers()
{
    beforeProcessedBuffer.setSize(0, 0);
    afterProcessedBuffer.setSize(0, 0);

    const int64 trimStart = projectState.getTrimStart();
    const int64 paddingSamples = projectState.getPaddingSamples();
    const int64 loopEnd = projectState.hasLoopPoints() ? projectState.getLoopEnd() : -1;

    if (referenceValid && referenceBuffer.getNumSamples() > 0)
        rebuildTrimPadBuffer(referenceBuffer, beforeProcessedBuffer, trimStart, paddingSamples, loopEnd);

    if (previewValid && previewBuffer.getNumSamples() > 0)
    {
        rebuildTrimPadBuffer(previewBuffer, afterProcessedBuffer, trimStart, paddingSamples, loopEnd);
    }
    else if (projectState.hasAudio())
    {
        rebuildTrimPadBuffer(projectState.getAudioBuffer(), afterProcessedBuffer, trimStart, paddingSamples, loopEnd);
    }
}

void AudioLevelStudioComponent::syncAdvancedControls()
{
    const juce::ScopedValueSetter<bool> guard(updatingAdvancedControls, true);

    manualTargetToggle.setToggleState(manualTargetEnabled, juce::dontSendNotification);
    manualTargetSlider.setEnabled(manualTargetEnabled);
    manualTargetValueLabel.setEnabled(manualTargetEnabled);
    manualTargetSlider.setValue(manualTargetRmsDb, juce::dontSendNotification);
    updateManualTargetValueLabel();

    peakCeilingToggle.setToggleState(manualPeakEnabled, juce::dontSendNotification);
    peakCeilingSlider.setEnabled(manualPeakEnabled);
    peakCeilingValueLabel.setEnabled(manualPeakEnabled);
    peakCeilingSlider.setValue(manualPeakDbfs, juce::dontSendNotification);
    updatePeakTargetValueLabel();
    updateBatchPresetNoteText();
}

void AudioLevelStudioComponent::updateManualTargetValueLabel()
{
    const juce::String text = manualTargetEnabled
        ? juce::String(manualTargetRmsDb, 1) + " dB"
        : "Preset (RMS)";
    manualTargetValueLabel.setText(text, juce::dontSendNotification);
}

void AudioLevelStudioComponent::updatePeakTargetValueLabel()
{
    const juce::String text = manualPeakEnabled
        ? juce::String(manualPeakDbfs, 1) + " dBFS"
        : "Preset (-1 dBFS)";
    peakCeilingValueLabel.setText(text, juce::dontSendNotification);
}

bool AudioLevelStudioComponent::hasManualOverridesEnabled() const
{
    return manualTargetEnabled || manualPeakEnabled;
}

juce::String AudioLevelStudioComponent::describeManualOverrides(bool includeValues) const
{
    juce::StringArray parts;

    if (manualTargetEnabled)
    {
        juce::String part = "manual RMS target";
        if (includeValues)
            part << " (" << juce::String(manualTargetRmsDb, 1) << " dB)";
        parts.add(part);
    }

    if (manualPeakEnabled)
    {
        juce::String part = "manual peak ceiling";
        if (includeValues)
            part << " (" << juce::String(manualPeakDbfs, 1) << " dBFS)";
        parts.add(part);
    }

    if (parts.isEmpty())
        return {};

    if (parts.size() == 1)
        return parts[0];

    return parts[0] + " and " + parts[1];
}

void AudioLevelStudioComponent::updateBatchPresetNoteText()
{
    juce::String text = "Batch exports apply the selected preset";

        if (hasManualOverridesEnabled())
        text << " plus your " << describeManualOverrides(false);

    text << " to every track.";
    batchPresetNote.setText(text, juce::dontSendNotification);
}

void AudioLevelStudioComponent::showBatchProgressDialog(bool exportMode, int totalTracks)
{
    batchProgressExportMode = exportMode;
    batchProgressTotalTracks = totalTracks;

    batchProgressDialog = nullptr;
    batchProgressView = nullptr;

    auto view = std::make_unique<BatchProgressView>([this]
    {
        cancelBatchOperation();
    });
    view->setSize(480, 300);
    view->setSummary("Preparing batch...");
    view->appendDetailLine("Waiting for first track...");

    juce::DialogWindow::LaunchOptions options;
    options.content.setOwned(view.release());
    options.dialogTitle = exportMode ? "Exporting Tracks" : "Previewing Tracks";
    options.useNativeTitleBar = true;
    options.resizable = false;
    options.escapeKeyTriggersCloseButton = false;
    options.componentToCentreAround = this;
    options.dialogBackgroundColour = getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId);

    if (auto* window = options.launchAsync())
    {
        batchProgressDialog = window;
        batchProgressView = dynamic_cast<BatchProgressView*>(window->getContentComponent());
    }
}

void AudioLevelStudioComponent::updateBatchProgressDialog(int processed, int total, const juce::String& latestLine)
{
    auto* view = batchProgressView.getComponent();
    if (view == nullptr)
        return;

    total = total > 0 ? total : batchProgressTotalTracks;
    const int clampedTotal = juce::jmax(1, total);
    processed = juce::jlimit(0, clampedTotal, processed);

    juce::String summary;
        summary << (batchProgressExportMode ? "Exporting " : "Previewing ")
            << processed << " / " << clampedTotal << " track" << (clampedTotal == 1 ? "" : "s") << "...";
    view->setSummary(summary);

    const double progress = clampedTotal > 0 ? static_cast<double>(processed) / clampedTotal : 0.0;
    view->setProgress(progress);

    if (latestLine.isNotEmpty())
        view->appendDetailLine(latestLine);

    const bool cancelEnabled = batchInProgress.load() && !batchCancelRequested.load();
    view->setCancelEnabled(cancelEnabled);
}

void AudioLevelStudioComponent::finalizeBatchProgressDialog(const juce::String& finalSummary)
{
    auto* view = batchProgressView.getComponent();
    if (view != nullptr)
    {
        if (finalSummary.isNotEmpty())
        {
            view->setSummary(finalSummary);
            view->appendDetailLine(finalSummary);
        }

        auto safeWindow = batchProgressDialog;
        view->enterCompletionMode([safeWindow]() mutable
        {
            if (auto* window = safeWindow.getComponent())
                window->exitModalState(0);
        });
    }

    batchProgressTotalTracks = 0;
}

void AudioLevelStudioComponent::postBatchProgressUpdate(int processed, int total, const juce::String& latestLine)
{
    auto safeThis = juce::Component::SafePointer<AudioLevelStudioComponent>(this);
    juce::MessageManager::callAsync([safeThis, processed, total, latestLine]
    {
        if (auto* comp = safeThis.getComponent())
            comp->updateBatchProgressDialog(processed, total, latestLine);
    });
}

int AudioLevelStudioComponent::getNumRows()
{
    return static_cast<int>(batchTracks.size());
}

void AudioLevelStudioComponent::paintRowBackground(juce::Graphics& g,
                                                   int rowNumber,
                                                   int width,
                                                   int height,
                                                   bool rowIsSelected)
{
    juce::ignoreUnused(width, height);

    if (rowIsSelected)
        g.fillAll(juce::Colours::lightblue);
    else if (rowNumber % 2 != 0)
        g.fillAll(juce::Colour(0xff303030));
}

void AudioLevelStudioComponent::paintCell(juce::Graphics& g,
                                          int rowNumber,
                                          int columnId,
                                          int width,
                                          int height,
                                          bool rowIsSelected)
{
    g.setColour(rowIsSelected ? juce::Colours::darkblue
                              : getLookAndFeel().findColour(juce::ListBox::textColourId));

    if (rowNumber < 0 || rowNumber >= static_cast<int>(batchTracks.size()))
        return;

    const auto& entry = batchTracks[static_cast<size_t>(rowNumber)];
    juce::String text;

    switch (columnId)
    {
        case BatchColumnIds::trackCol:
            text = juce::String(entry.trackNumber);
            break;
        case BatchColumnIds::titleCol:
            text = entry.songTitle.isNotEmpty() ? entry.songTitle : entry.pcmFile.getFileName();
            break;
        case BatchColumnIds::statusCol:
        {
            const bool exists = entry.pcmFile.existsAsFile();
            text = exists ? "Found" : "Missing";
            g.setColour(exists ? juce::Colours::green : juce::Colours::orange);
            break;
        }
        case BatchColumnIds::backupCol:
        {
            text = entry.backupExists ? "Yes" : "";
            break;
        }
        case BatchColumnIds::previewCol:
        case BatchColumnIds::actionCol:
            return;
        default:
            break;
    }

    g.drawText(text, 2, 0, width - 4, height, juce::Justification::centredLeft, true);
}

juce::Component* AudioLevelStudioComponent::refreshComponentForCell(int rowNumber,
                                                                     int columnId,
                                                                     bool isRowSelected,
                                                                     juce::Component* existingComponentToUpdate)
{
    juce::ignoreUnused(isRowSelected);

    if (columnId == BatchColumnIds::previewCol)
    {
        auto* button = dynamic_cast<BatchPreviewButton*>(existingComponentToUpdate);
        if (button == nullptr)
            button = new BatchPreviewButton(*this);

        button->setRowNumber(rowNumber);
        return button;
    }

    if (columnId == BatchColumnIds::actionCol)
    {
        auto* button = dynamic_cast<BatchReplaceButton*>(existingComponentToUpdate);
        if (button == nullptr)
            button = new BatchReplaceButton(*this);

        button->setRowNumber(rowNumber);
        return button;
    }

    delete existingComponentToUpdate;
    return nullptr;
}

void AudioLevelStudioComponent::mouseWheelMove(const juce::MouseEvent& event,
                                               const juce::MouseWheelDetails& wheel)
{
    auto forwardToContentViewport = [this, &event, &wheel]
    {
        auto relEvent = event.getEventRelativeTo(&contentViewport);
        contentViewport.mouseWheelMove(relEvent, wheel);
    };

    if (auto* tableViewport = batchTrackTable.getViewport())
    {
        auto belongsToTable = [this, tableViewport](const juce::Component* component)
        {
            while (component != nullptr)
            {
                if (component == &batchTrackTable || component == tableViewport)
                    return true;

                component = component->getParentComponent();
            }

            return false;
        };

        if (belongsToTable(event.originalComponent) || belongsToTable(event.eventComponent))
        {
            const auto shouldForwardImmediately = [tableViewport, &wheel]()
            {
                if (const auto* viewed = tableViewport->getViewedComponent())
                {
                    const int visibleHeight = tableViewport->getMaximumVisibleHeight();
                    const int contentHeight = viewed->getHeight();
                    if (contentHeight <= visibleHeight)
                        return true;

                    const int maxViewY = juce::jmax(0, contentHeight - visibleHeight);
                    const int currentY = tableViewport->getViewPositionY();
                    const bool scrollingUp = wheel.deltaY > 0.0f;
                    const bool scrollingDown = wheel.deltaY < 0.0f;

                    if (scrollingUp && currentY <= 0)
                        return true;

                    if (scrollingDown && currentY >= maxViewY)
                        return true;
                }

                return false;
            };

            if (!shouldForwardImmediately())
            {
                auto viewportEvent = event.getEventRelativeTo(tableViewport);
                if (tableViewport->useMouseWheelMoveIfNeeded(viewportEvent, wheel))
                    return;
            }

            forwardToContentViewport();
            return;
        }
    }

    forwardToContentViewport();
}

void AudioLevelStudioComponent::selectedRowsChanged(int)
{
    updateBatchSelectionSummary();
    updateBatchButtons();
}

void AudioLevelStudioComponent::setMSUContext(const juce::File& msuFile,
                                              const juce::String& gameTitle,
                                              const std::vector<MSUFileBrowser::TrackInfo>& tracks)
{
    currentMSUFile = msuFile;
    currentGameTitle = gameTitle.isNotEmpty() ? gameTitle : msuFile.getFileNameWithoutExtension();
    rebuildBatchTrackList(tracks);
}

void AudioLevelStudioComponent::clearMSUContext()
{
    stopActiveTrackPreview();
    currentMSUFile = juce::File();
    currentGameTitle.clear();
    batchTracks.clear();
    batchTrackTable.updateContent();
    batchTrackTable.deselectAllRows();
    updateBatchSelectionSummary();
    updateBatchButtons();
    refreshBatchTableComponents();
}

void AudioLevelStudioComponent::rebuildBatchTrackList(const std::vector<MSUFileBrowser::TrackInfo>& tracks)
{
    stopActiveTrackPreview();
    batchTracks.clear();
    batchTracks.reserve(tracks.size());

    for (const auto& track : tracks)
    {
        if (!track.exists)
            continue;

        BatchTrackEntry entry;
        entry.trackNumber = track.trackNumber;
        entry.songTitle = track.title.isNotEmpty() ? track.title : track.fileName;
        entry.pcmFile = track.file;
        entry.backupExists = track.backupExists;
        entry.suggestedName = formatSuggestedTrackName(entry.trackNumber, entry.songTitle);
        batchTracks.push_back(std::move(entry));
    }

    std::sort(batchTracks.begin(), batchTracks.end(),
        [](const BatchTrackEntry& a, const BatchTrackEntry& b)
        {
            return a.trackNumber < b.trackNumber;
        });

    batchTrackTable.updateContent();
    batchTrackTable.deselectAllRows();
    updateBatchSelectionSummary();
    updateBatchButtons();
    refreshBatchTableComponents();
}

juce::String AudioLevelStudioComponent::formatSuggestedTrackName(int trackNumber,
                                                                 const juce::String& songTitle) const
{
    juce::String baseTitle = currentGameTitle;
    if (baseTitle.isEmpty())
        baseTitle = currentMSUFile.existsAsFile() ? currentMSUFile.getFileNameWithoutExtension() : juce::String("Track");

    const int digits = trackNumber >= 100 ? 3 : 2;
    juce::String numberText = juce::String(trackNumber).paddedLeft('0', digits);
    return baseTitle + " - " + numberText + " - " + songTitle;
}

juce::Array<int> AudioLevelStudioComponent::getSelectedBatchRows() const
{
    juce::Array<int> rows;
    const auto selected = batchTrackTable.getSelectedRows();
    for (int i = 0; i < selected.getNumRanges(); ++i)
    {
        auto range = selected.getRange(i);
        for (int row = range.getStart(); row <= range.getEnd(); ++row)
            rows.add(row);
    }
    return rows;
}

void AudioLevelStudioComponent::updateBatchSelectionSummary()
{
    juce::String status;

    if (!currentMSUFile.existsAsFile())
    {
        status = "No ROM loaded";
    }
    else
    {
        const int totalTracks = static_cast<int>(batchTracks.size());
        status = (currentGameTitle.isNotEmpty() ? currentGameTitle : currentMSUFile.getFileNameWithoutExtension());
        status << ": " << totalTracks << " track" << (totalTracks == 1 ? "" : "s");
    }

    batchStatusLabel.setText(status, juce::dontSendNotification);
}

void AudioLevelStudioComponent::updateBatchButtons()
{
    const bool hasTracks = !batchTracks.empty();
    const bool busy = batchInProgress.load();

    loadMSUButton.setEnabled(requestExternalMSULoad != nullptr && !busy);
    batchExportButton.setEnabled(hasTracks && !busy);
    batchTrackTable.setEnabled(!busy);
    batchCancelButton.setVisible(busy);
    batchCancelButton.setEnabled(busy && !batchCancelRequested.load());
    batchProgressBar.setVisible(busy);
    if (!busy)
        batchProgressValue = 0.0;
    refreshBatchTableComponents();
}

juce::String AudioLevelStudioComponent::getSelectedPresetLabel() const
{
    auto text = presetSelector.getText();
    return text.isNotEmpty() ? text : juce::String("Preset");
}

const AudioLevelStudioComponent::BatchTrackEntry* AudioLevelStudioComponent::getBatchEntry(int rowNumber) const
{
    if (rowNumber < 0 || rowNumber >= static_cast<int>(batchTracks.size()))
        return nullptr;

    return &batchTracks[static_cast<size_t>(rowNumber)];
}

void AudioLevelStudioComponent::handleTrackPreviewToggle(int rowNumber)
{
    if (isBatchBusy())
        return;

    if (rowNumber == activeBatchPreviewRow)
    {
        stopActiveTrackPreview();
        return;
    }

    if (previewBatchTrack(rowNumber))
    {
        activeBatchPreviewRow = rowNumber;
        refreshBatchTableComponents();
    }
    else
    {
        refreshBatchTableComponents();
    }
}

void AudioLevelStudioComponent::handleTrackReplaceRequest(int rowNumber)
{
    if (!requestTrackReplacement)
        return;

    if (auto* entry = getBatchEntry(rowNumber))
        requestTrackReplacement(makeTrackInfo(*entry));
}

bool AudioLevelStudioComponent::previewBatchTrack(int rowNumber)
{
    const auto* entry = getBatchEntry(rowNumber);
    if (entry == nullptr)
        return false;

    stopActiveTrackPreview();

    juce::AudioBuffer<float> sourceBuffer;
    juce::AudioBuffer<float> processedBuffer;
    double sampleRate = 0.0;

    if (!loadTrackPreviewData(*entry, sourceBuffer, processedBuffer, sampleRate))
        return false;

    configureBatchPreviewPlayback(sourceBuffer, processedBuffer, sampleRate);

    if (requestPlaybackStop)
        requestPlaybackStop();

    beforeAfterPlayer.play(BeforeAfterPreviewPlayer::Target::After, true);
    updatePreviewPlaybackButtons();

    juce::String status;
        status << "Previewing " << entry->suggestedName
            << " - " << getSelectedPresetLabel();
    batchStatusLabel.setText(status, juce::dontSendNotification);
    return true;
}

bool AudioLevelStudioComponent::loadTrackPreviewData(const BatchTrackEntry& entry,
                                                     juce::AudioBuffer<float>& sourceBuffer,
                                                     juce::AudioBuffer<float>& processedBuffer,
                                                     double& sampleRate)
{
    if (!entry.pcmFile.existsAsFile())
    {
        batchStatusLabel.setText("Missing file for " + entry.suggestedName + ".", juce::dontSendNotification);
        return false;
    }

    AudioFileHandler handler;
    double loadedSampleRate = 0.0;
    int64 loopPoint = -1;
    if (!handler.loadAudioFile(entry.pcmFile, sourceBuffer, loadedSampleRate, &loopPoint))
    {
        batchStatusLabel.setText("Unable to load " + entry.suggestedName + ": " + handler.getLastError(),
                                 juce::dontSendNotification);
        return false;
    }

    auto stats = NormalizationAnalyzer::analyzeBuffer(sourceBuffer);
    float gainDb = 0.0f;
    juce::String description;
    if (!calculatePresetGain(gainDb, description, stats))
    {
        batchStatusLabel.setText("Preset unavailable for " + entry.suggestedName + ".", juce::dontSendNotification);
        return false;
    }

    processedBuffer.makeCopyOf(sourceBuffer);
    if (std::abs(gainDb) > 0.05f)
        NormalizationAnalyzer::applyGain(processedBuffer, gainDb);

    juce::ignoreUnused(description);
    sampleRate = loadedSampleRate;
    return true;
}

void AudioLevelStudioComponent::configureBatchPreviewPlayback(const juce::AudioBuffer<float>& sourceBuffer,
                                                              const juce::AudioBuffer<float>& processedBuffer,
                                                              double sampleRate)
{
    const int64 trimStart = projectState.getTrimStart();
    const int64 paddingSamples = projectState.getPaddingSamples();
    const int64 loopEndSample = projectState.hasLoopPoints() ? projectState.getLoopEnd() : -1;

    rebuildTrimPadBuffer(sourceBuffer, batchPreviewBeforeBuffer, trimStart, paddingSamples, loopEndSample);
    rebuildTrimPadBuffer(processedBuffer, batchPreviewAfterBuffer, trimStart, paddingSamples, loopEndSample);

    batchPreviewSampleRate = sampleRate > 0.0 ? sampleRate : projectState.getSampleRate();
    batchPreviewActive = true;

    const auto* beforePtr = batchPreviewBeforeBuffer.getNumSamples() > 0 ? &batchPreviewBeforeBuffer : nullptr;
    const auto* afterPtr = batchPreviewAfterBuffer.getNumSamples() > 0 ? &batchPreviewAfterBuffer : nullptr;
    const double sourceRate = batchPreviewSampleRate > 0.0 ? batchPreviewSampleRate : previewSampleRate;
    beforeAfterPlayer.setSourceBuffers(beforePtr, afterPtr, sourceRate);
    updatePreviewPlaybackButtons();
}

void AudioLevelStudioComponent::stopActiveTrackPreview()
{
    if (activeBatchPreviewRow < 0 && !batchPreviewActive)
        return;

    beforeAfterPlayer.stop();
    activeBatchPreviewRow = -1;
    batchPreviewActive = false;
    batchPreviewBeforeBuffer.setSize(0, 0);
    batchPreviewAfterBuffer.setSize(0, 0);
    syncBeforeAfterBuffers();
    updateBatchSelectionSummary();
    refreshBatchTableComponents();
}
 
void AudioLevelStudioComponent::refreshBatchTableComponents()
{
    const int totalRows = getNumRows();
    for (int row = 0; row < totalRows; ++row)
        batchTrackTable.repaintRow(row);
}

MSUFileBrowser::TrackInfo AudioLevelStudioComponent::makeTrackInfo(const BatchTrackEntry& entry) const
{
    MSUFileBrowser::TrackInfo info;
    info.trackNumber = entry.trackNumber;
    info.file = entry.pcmFile;
    info.exists = entry.pcmFile.existsAsFile();
    info.fileName = entry.pcmFile.getFileName();
    info.title = entry.songTitle;
    info.backupExists = entry.backupExists;
    return info;
}

bool AudioLevelStudioComponent::hasExistingBackups(const std::vector<BatchTrackEntry>& entries) const
{
    return std::any_of(entries.begin(), entries.end(),
        [](const BatchTrackEntry& entry)
        {
            return entry.backupExists;
        });
}

void AudioLevelStudioComponent::promptBackupOverwriteConfirmation(std::function<void()> onConfirm,
                                                                  std::function<void()> onCancel)
{
    auto safeThis = juce::Component::SafePointer<AudioLevelStudioComponent>(this);
    auto options = juce::MessageBoxOptions()
        .withIconType(juce::MessageBoxIconType::WarningIcon)
        .withTitle("Overwrite Backup?")
        .withMessage("A backup already exists for one or more files. Would you like to overwrite the backup?")
        .withButton("Yes")
        .withButton("Cancel")
        .withAssociatedComponent(this);

    juce::AlertWindow::showAsync(
        options,
        juce::ModalCallbackFunction::create([safeThis,
                                             onConfirm = std::move(onConfirm),
                                             onCancel = std::move(onCancel)](int result) mutable
        {
            if (result == 1)
            {
                if (onConfirm)
                    onConfirm();
            }
            else
            {
                if (onCancel)
                    onCancel();
            }
        }));
}

void AudioLevelStudioComponent::runBatchOperation(bool exportMode)
{
    if (batchInProgress.load())
        return;

    if (!exportMode)
        return;

    if (activeBatchPreviewRow >= 0)
        stopActiveTrackPreview();

    if (batchTracks.empty())
    {
        batchStatusLabel.setText("Load an MSU manifest to enable batch processing.", juce::dontSendNotification);
        return;
    }

    auto entries = std::make_shared<std::vector<BatchTrackEntry>>(batchTracks.begin(), batchTracks.end());
    const size_t trackCount = entries->size();
    const bool confirmOverwriteBackups = exportMode && backupsEnabled && hasExistingBackups(*entries);

    juce::String message;
    const bool manualOverrides = hasManualOverridesEnabled();
    message << "Apply \"" << getSelectedPresetLabel() << "\"";
    if (manualOverrides)
        message << " with manual overrides";
    message << " to all " << trackCount << " track" << (trackCount == 1 ? "" : "s") << "?\n"
            << "Original files are moved into the MSU-1 directory's \\Backup folder before exporting. This can be disabled in Settings.";

    if (manualOverrides)
        message << "\nManual overrides: " << describeManualOverrides(true) << ".";

    const auto presetSettings = getCurrentPresetSettings();
    auto safeThis = juce::Component::SafePointer<AudioLevelStudioComponent>(this);
    auto options = juce::MessageBoxOptions()
        .withIconType(juce::MessageBoxIconType::WarningIcon)
        .withTitle("Batch Export")
        .withMessage(message)
        .withButton("Export")
        .withButton("Cancel");

    juce::AlertWindow::showAsync(options, [safeThis,
                                           entries,
                                           presetSettings,
                                           exportMode,
                                           confirmOverwriteBackups](int result) mutable
    {
        if (result != 1)
            return;

        if (auto* comp = safeThis.getComponent())
        {
            auto startBatch = [safeThis, entries, presetSettings, exportMode]() mutable
            {
                if (auto* compInner = safeThis.getComponent())
                    compInner->beginBatchOperation(exportMode, std::move(*entries), presetSettings);
            };

            if (confirmOverwriteBackups)
            {
                comp->promptBackupOverwriteConfirmation(
                    [startBatch]() mutable
                    {
                        startBatch();
                    },
                    [safeThis]()
                    {
                        if (auto* compInner = safeThis.getComponent())
                        {
                            compInner->batchStatusLabel.setText("Batch export cancelled.", juce::dontSendNotification);
                            compInner->updateBatchButtons();
                        }
                    });
            }
            else
            {
                startBatch();
            }
        }
    });
}

void AudioLevelStudioComponent::beginBatchOperation(bool exportMode,
                                                    std::vector<BatchTrackEntry> entries,
                                                    PresetSettings presetSettings)
{
    if (entries.empty())
        return;

    showBatchProgressDialog(exportMode, static_cast<int>(entries.size()));

    startBatchWorker(exportMode, std::move(entries), presetSettings);
}

void AudioLevelStudioComponent::startBatchWorker(bool exportMode,
                                                 std::vector<BatchTrackEntry> entries,
                                                 PresetSettings presetSettings)
{
    if (entries.empty())
        return;

    if (batchWorker && batchWorker->joinable())
    {
        batchWorker->join();
        batchWorker.reset();
    }

    batchInProgress = true;
    batchCancelRequested = false;
    batchProgressPending.store(0.0);
    batchProgressValue = 0.0;
    batchProgressBar.setVisible(true);
    batchCancelButton.setVisible(true);
    batchCancelButton.setEnabled(true);
    updateBatchButtons();

    auto safeComponent = juce::Component::SafePointer<AudioLevelStudioComponent>(this);
    const bool backups = backupsEnabled;

    batchWorker = std::make_unique<std::thread>(
        [safeComponent, exportMode, entries = std::move(entries), presetSettings, backups]() mutable
        {
            AudioFileHandler handler;
            MSU1Exporter exporter;
            juce::StringArray logLines;
            int processed = 0;
            int failures = 0;
            const int total = static_cast<int>(entries.size());
            bool cancelled = false;

            for (int i = 0; i < total; ++i)
            {
                auto* component = safeComponent.getComponent();
                if (component == nullptr)
                    return;

                if (component->batchCancelRequested.load())
                {
                    cancelled = true;
                    juce::String cancelMsg = "Batch cancelled with " + juce::String(total - i) + " track(s) remaining.";
                    logLines.add(cancelMsg);
                    component->postBatchProgressUpdate(i, total, cancelMsg);
                    break;
                }

                const auto& entry = entries[static_cast<size_t>(i)];
                juce::String iterationLog;
                juce::AudioBuffer<float> buffer;
                double sampleRate = 0.0;
                int64 loopPoint = -1;

                if (!entry.pcmFile.existsAsFile())
                {
                    iterationLog = "Missing file for " + entry.suggestedName;
                    logLines.add(iterationLog);
                    ++failures;
                }
                else if (!handler.loadAudioFile(entry.pcmFile, buffer, sampleRate, &loopPoint))
                {
                    iterationLog = "Failed " + entry.suggestedName + ": " + handler.getLastError();
                    logLines.add(iterationLog);
                    ++failures;
                }
                else
                {
                    auto stats = NormalizationAnalyzer::analyzeBuffer(buffer);
                    float gainDb = 0.0f;
                    juce::String description;
                    if (!calculatePresetGainForSettings(presetSettings, stats, gainDb, description))
                    {
                        iterationLog = "Skipped " + entry.suggestedName + ": preset unavailable";
                        logLines.add(iterationLog);
                        ++failures;
                    }
                    else if (exportMode)
                    {
                        juce::AudioBuffer<float> processedBuffer(buffer);
                        if (std::abs(gainDb) > 0.01f)
                            NormalizationAnalyzer::applyGain(processedBuffer, gainDb);

                        if (!exporter.exportPCM(entry.pcmFile,
                                                processedBuffer,
                                                loopPoint >= 0 ? loopPoint : -1,
                                                backups))
                        {
                            iterationLog = "Failed to write " + entry.suggestedName + ": " + exporter.getLastError();
                            logLines.add(iterationLog);
                            ++failures;
                        }
                        else
                        {
                            iterationLog = "OK " + entry.suggestedName + ": " + juce::String(gainDb, 2) +
                                           " dB (" + description + ")";
                            logLines.add(iterationLog);
                            ++processed;
                        }
                    }
                    else
                    {
                        iterationLog = entry.suggestedName + ": " + juce::String(gainDb, 2) +
                                       " dB toward " + description;
                        logLines.add(iterationLog);
                        ++processed;
                    }
                }

                component->batchProgressPending.store(total > 0 ? static_cast<double>(i + 1) / total : 1.0);
                if (component != nullptr)
                    component->postBatchProgressUpdate(i + 1, total, iterationLog);
            }

            if (auto* component = safeComponent.getComponent())
            {
                juce::MessageManager::callAsync(
                    [safeComponent, exportMode, processed, failures, logs = std::move(logLines), cancelled]() mutable
                    {
                        if (auto* comp = safeComponent.getComponent())
                            comp->handleBatchCompletion(exportMode, processed, failures, std::move(logs), cancelled);
                    });
            }
        });
}

void AudioLevelStudioComponent::handleBatchCompletion(bool exportMode,
                                                      int processed,
                                                      int failures,
                                                      juce::StringArray logLines,
                                                      bool cancelled)
{
    juce::ignoreUnused(logLines);
    if (batchWorker && batchWorker->joinable())
    {
        batchWorker->join();
        batchWorker.reset();
    }

    batchInProgress = false;
    batchProgressBar.setVisible(false);
    batchProgressPending.store(0.0);
    batchProgressValue = 0.0;
    batchCancelButton.setVisible(false);
    batchCancelButton.setEnabled(true);
    batchCancelRequested = false;

    juce::String summary;
    if (logLines.isEmpty())
        summary = exportMode ? "No tracks exported." : "No tracks previewed.";
    else
    {
        summary << (exportMode ? "Exported " : "Previewed ")
                << processed << " track" << (processed == 1 ? "" : "s");
        if (failures > 0)
            summary << " - " << failures << " failed";
        if (cancelled)
            summary << " (cancelled)";
    }

    if (cancelled && logLines.isEmpty())
        summary = "Batch cancelled by user.";

    if (exportMode && backupsEnabled && processed > 0 && !cancelled && requestTrackListRefresh)
        requestTrackListRefresh();

    finalizeBatchProgressDialog(summary);
    updateBatchSelectionSummary();
    updateBatchButtons();
}

void AudioLevelStudioComponent::cancelBatchOperation()
{
    if (!batchInProgress.load() || batchCancelRequested.load())
        return;

    batchCancelRequested = true;
    batchCancelButton.setEnabled(false);
    updateBatchProgressDialog(0, batchProgressTotalTracks, "Cancelling batch...");
}

void AudioLevelStudioComponent::handlePreviewButtonPress(BeforeAfterPreviewPlayer::Target target)
{
    if (batchPreviewActive)
        stopActiveTrackPreview();

    updatePreviewPlaybackButtons();

    if (!beforeAfterPlayer.hasContent(target))
        return;

    const bool isTargetAlreadyPlaying = beforeAfterPlayer.isPlaying()
        && beforeAfterPlayer.getActiveTarget() == target;

    if (isTargetAlreadyPlaying)
    {
        beforeAfterPlayer.stop();
    }
    else
    {
        const bool wasPlaying = beforeAfterPlayer.isPlaying();

        if (!wasPlaying && requestPlaybackStop)
            requestPlaybackStop();

        const bool restartPlayback = !wasPlaying;
        beforeAfterPlayer.play(target, restartPlayback);
    }

    updatePreviewPlaybackButtons();
}

void AudioLevelStudioComponent::updatePreviewPlaybackButtons()
{
    const bool beforeHas = beforeAfterPlayer.hasContent(BeforeAfterPreviewPlayer::Target::Before);
    const bool afterHas = beforeAfterPlayer.hasContent(BeforeAfterPreviewPlayer::Target::After);

    playBeforeButton.setEnabled(beforeHas);
    playAfterButton.setEnabled(afterHas);

    if (!beforeAfterPlayer.isPlaying())
    {
        playBeforeButton.setButtonText("Play Before");
        playAfterButton.setButtonText("Play After");
        return;
    }

    const auto active = beforeAfterPlayer.getActiveTarget();
    playBeforeButton.setButtonText(active == BeforeAfterPreviewPlayer::Target::Before ? "Stop" : "Play Before");
    playAfterButton.setButtonText(active == BeforeAfterPreviewPlayer::Target::After ? "Stop" : "Play After");
}

void AudioLevelStudioComponent::syncBeforeAfterBuffers()
{
    const bool previewLocked = batchPreviewActive;
    const bool wasPlaying = beforeAfterPlayer.isPlaying() && !previewLocked;
    const auto previousTarget = beforeAfterPlayer.getActiveTarget();

    if (wasPlaying)
        beforeAfterPlayer.stop();

    rebuildProcessedBuffers();

    if (previewLocked)
    {
        updatePreviewPlaybackButtons();
        return;
    }

    const juce::AudioBuffer<float>* beforeBufferPtr = beforeProcessedBuffer.getNumSamples() > 0
        ? &beforeProcessedBuffer
        : nullptr;
    const juce::AudioBuffer<float>* afterBufferPtr = afterProcessedBuffer.getNumSamples() > 0
        ? &afterProcessedBuffer
        : nullptr;

    double afterRate = 0.0;
    if (previewValid && previewSampleRate > 0.0)
        afterRate = previewSampleRate;
    else if (projectState.hasAudio())
        afterRate = projectState.getSampleRate();

    const double beforeRate = referenceValid ? referenceSampleRate : afterRate;
    const double sourceRate = afterRate > 0.0 ? afterRate : beforeRate;

    beforeAfterPlayer.setSourceBuffers(beforeBufferPtr, afterBufferPtr, sourceRate);

    if (wasPlaying)
    {
        auto resumeTarget = previousTarget;
        if (!beforeAfterPlayer.hasContent(resumeTarget))
        {
            resumeTarget = beforeAfterPlayer.hasContent(BeforeAfterPreviewPlayer::Target::After)
                ? BeforeAfterPreviewPlayer::Target::After
                : BeforeAfterPreviewPlayer::Target::Before;
        }

        if (beforeAfterPlayer.hasContent(resumeTarget))
            beforeAfterPlayer.play(resumeTarget, true);
    }

    updatePreviewPlaybackButtons();
}

void AudioLevelStudioComponent::stopPreviewPlayback()
{
    beforeAfterPlayer.stop();
    updatePreviewPlaybackButtons();
    if (waveformOverlay != nullptr)
        waveformOverlay->setPlaybackCursorRatio(std::numeric_limits<double>::quiet_NaN());
}

void AudioLevelStudioComponent::timerCallback()
{
    updatePreviewPlaybackButtons();

    double currentSeconds = 0.0;
    double totalSeconds = 0.0;
    const bool hasProgress = beforeAfterPlayer.getPlaybackProgress(currentSeconds, totalSeconds)
        && totalSeconds > 0.0;
    const bool shouldShowWaveformCursor = !batchPreviewActive && beforeAfterPlayer.isPlaying();

    if (waveformOverlay != nullptr)
    {
        if (shouldShowWaveformCursor && hasProgress)
        {
            const double ratio = juce::jlimit(0.0, 1.0, currentSeconds / totalSeconds);
            waveformOverlay->setPlaybackCursorRatio(ratio);
        }
        else
        {
            waveformOverlay->setPlaybackCursorRatio(std::numeric_limits<double>::quiet_NaN());
        }
    }

    if (batchInProgress.load())
    {
        const double pending = batchProgressPending.load();
        if (std::abs(pending - batchProgressValue) > 0.001)
        {
            batchProgressValue = pending;
            batchProgressBar.repaint();
        }
    }
}
