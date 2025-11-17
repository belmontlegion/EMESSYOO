#pragma once

#include <JuceHeader.h>

//==============================================================================
/**
 * Visual marker for loop points in the waveform view.
 * Can be dragged to adjust loop positions.
 */
class LoopMarker : public juce::Component
{
public:
    //==============================================================================
    enum MarkerType
    {
        LoopStart,
        LoopEnd
    };
    
    //==============================================================================
    LoopMarker(MarkerType type);
    ~LoopMarker() override;
    
    //==============================================================================
    void paint(juce::Graphics& g) override;
    void mouseDown(const juce::MouseEvent& event) override;
    void mouseDrag(const juce::MouseEvent& event) override;
    
    //==============================================================================
    std::function<void(int)> onPositionChanged;
    
private:
    //==============================================================================
    MarkerType markerType;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LoopMarker)
};
