#pragma once

#include <JuceHeader.h>
#include "WaveformView.h"
#include "TransportControls.h"
#include "MSUFileBrowser.h"

/**
 * Container component for the legacy Loop Editor UI. It simply
 * arranges the waveform, transport controls, and MSU browser within
 * the tab content area so MainComponent can host it inside a tabbed
 * layout.
 */
class LoopEditorTab : public juce::Component
{
public:
    LoopEditorTab(WaveformView& waveformView,
                  TransportControls& transportControls,
                  MSUFileBrowser& msuFileBrowser,
                  int browserHeight,
                  int transportHeight);
    ~LoopEditorTab() override = default;

    void resized() override;

private:
    WaveformView& waveformView;
    TransportControls& transportControls;
    MSUFileBrowser& msuFileBrowser;
    int browserHeight;
    int transportHeight;
};
