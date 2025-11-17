#include "LoopEditorTab.h"

LoopEditorTab::LoopEditorTab(WaveformView& waveform,
                                                         TransportControls& transport,
                                                         MSUFileBrowser& browser,
                                                         int browserHeightIn,
                                                         int transportHeightIn)
        : waveformView(waveform),
            transportControls(transport),
            msuFileBrowser(browser),
            browserHeight(browserHeightIn),
            transportHeight(transportHeightIn)
{
    addAndMakeVisible(waveformView);
    addAndMakeVisible(transportControls);
    addAndMakeVisible(msuFileBrowser);
}

void LoopEditorTab::resized()
{
    auto bounds = getLocalBounds();

    // Reserve space for the MSU browser at the bottom.
    msuFileBrowser.setBounds(bounds.removeFromBottom(browserHeight));

    // Transport controls sit above the browser.
    transportControls.setBounds(bounds.removeFromBottom(transportHeight));

    // Waveform view occupies the remaining space.
    waveformView.setBounds(bounds);
}
