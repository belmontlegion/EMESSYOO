#pragma once

#include <JuceHeader.h>
#include "../Core/MSUProjectState.h"

//==============================================================================
/**
 * Custom ScrollBar that passes mouse wheel events to parent only when NOT over itself.
 */
class PassThroughScrollBar : public juce::ScrollBar
{
public:
    PassThroughScrollBar(bool vertical) : juce::ScrollBar(vertical) {}
    
    void mouseWheelMove(const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel) override
    {
        // Always handle scroll bar's own wheel events normally
        juce::ScrollBar::mouseWheelMove(event, wheel);
    }
};

//==============================================================================
/**
 * Displays waveform with zoom and scroll capabilities.
 * Will host loop markers for visual editing.
 */
class WaveformView : public juce::Component,
                     public juce::ChangeListener,
                     public juce::ScrollBar::Listener
{
public:
    //==============================================================================
    WaveformView(MSUProjectState& state);
    ~WaveformView() override;
    
    //==============================================================================
    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& event) override;
    void mouseDrag(const juce::MouseEvent& event) override;
    void mouseUp(const juce::MouseEvent& event) override;
    void mouseMove(const juce::MouseEvent& event) override;
    void mouseWheelMove(const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel) override;
    bool keyPressed(const juce::KeyPress& key) override;
    
    //==============================================================================
    void changeListenerCallback(juce::ChangeBroadcaster* source) override;
    void scrollBarMoved(juce::ScrollBar* scrollBarThatHasMoved, double newRangeStart) override;
    
    //==============================================================================
    void setZoomLevel(double newZoom);
    double getZoomLevel() const { return zoomLevel; }
    
    void setPlayPosition(double seconds);
    void setAutoScrollEnabled(bool enabled) { autoScrollEnabled = enabled; }
    
    std::function<void(double)> onPositionClicked;
    
private:
    //==============================================================================
    MSUProjectState& projectState;
    
    juce::AudioFormatManager formatManager;
    juce::AudioThumbnailCache thumbnailCache;
    juce::AudioThumbnail thumbnail;
    PassThroughScrollBar scrollBar;
    
    double zoomLevel = 1.0;
    double visibleStart = 0.0;
    double visibleEnd = 0.0;
    
    double playPosition = 0.0;
    bool autoScrollEnabled = true;
    int64 lastNumSamples = 0;
    int64 lastEffectiveStart = 0;
    int64 lastPaddingSamples = 0;
    double lastThumbnailLengthSeconds = 0.0;
    
    enum DragMode
    {
        None,
        TrimStart,
        LoopStart,
        LoopEnd
    };
    
    DragMode currentDragMode = None;
    DragMode selectedHandle = None;  // For fine-tuning with wheel
    
    float fineTuneAccumulator = 0.0f;
    float zoomAccumulator = 0.0f;
    
    void updateThumbnail();
    void updateVisibleRange();
    double getPlaybackLengthSeconds() const;
    int64 sampleAtX(int x) const;
    int xAtSample(int64 sample) const;
    int getTrimHandleX() const;
    double timelineSecondsAtX(int x) const;
    void drawHotkeyLegend(juce::Graphics& g, const juce::Rectangle<int>& waveformBounds) const;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(WaveformView)
};
