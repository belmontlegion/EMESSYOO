#pragma once

#include <JuceHeader.h>
#include "../Core/MSUProjectState.h"
#include "../Audio/AudioPlayer.h"

//==============================================================================
/**
 * Transport controls for playback (play, pause, stop, loop toggle).
 */
class TransportControls : public juce::Component,
                          public juce::Button::Listener,
                          public juce::Timer
{
public:
    //==============================================================================
    TransportControls(MSUProjectState& state, AudioPlayer& player);
    ~TransportControls() override;
    
    //==============================================================================
    void paint(juce::Graphics& g) override;
    void resized() override;
    void buttonClicked(juce::Button* button) override;
    void timerCallback() override;
    
    bool isAutoScrollEnabled() const { return autoScrollButton.getToggleState(); }
    bool isAutoTrimPadEnabled() const { return autoTrimPadButton.getToggleState(); }
    bool isTrimNoPadEnabled() const { return trimNoPadButton.getToggleState(); }
    
    void applyAutoTrimPad();
    void applyTrimNoPad();
    
private:
    //==============================================================================
    MSUProjectState& projectState;
    AudioPlayer& audioPlayer;
    
    juce::TextButton playButton;
    juce::TextButton pauseButton;
    juce::TextButton stopButton;
    juce::ToggleButton loopButton;
    juce::ToggleButton autoScrollButton;
    juce::ToggleButton autoTrimPadButton;
    juce::ToggleButton trimNoPadButton;
    juce::Label padAmountLabel;
    juce::Slider padAmountSlider;
    
    juce::Label positionLabel;
    juce::Label durationLabel;
    
    int64 detectFirstAudioSample(float thresholdDb = -60.0f) const;
    void resetTrimAndPadding();
    
    void updatePositionDisplay();
    juce::String formatTime(double seconds) const;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TransportControls)
};
