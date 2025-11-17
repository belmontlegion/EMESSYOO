#pragma once

#include <JuceHeader.h>
#include "../Audio/NormalizationAnalyzer.h"

//==============================================================================
/**
 * Dialog for normalization options.
 * Allows user to analyze and normalize audio.
 */
class NormalizationDialog : public juce::Component,
                            public juce::Button::Listener
{
public:
    //==============================================================================
    NormalizationDialog();
    ~NormalizationDialog() override;
    
    //==============================================================================
    void paint(juce::Graphics& g) override;
    void resized() override;
    void buttonClicked(juce::Button* button) override;
    
    //==============================================================================
    std::function<void(float)> onNormalize;
    std::function<void()> onCancel;
    
private:
    //==============================================================================
    juce::Label titleLabel;
    juce::Label instructionsLabel;
    juce::Label currentLevelLabel;
    juce::Label targetLevelLabel;
    
    juce::Slider targetSlider;
    juce::TextButton analyzeButton;
    juce::TextButton normalizeButton;
    juce::TextButton cancelButton;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NormalizationDialog)
};
