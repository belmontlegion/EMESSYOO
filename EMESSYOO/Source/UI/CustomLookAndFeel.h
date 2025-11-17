#pragma once

#include <JuceHeader.h>

//==============================================================================
/**
 * Custom dark theme with green accent for MSU-1 Prep Studio.
 */
class CustomLookAndFeel : public juce::LookAndFeel_V4
{
public:
    //==============================================================================
    CustomLookAndFeel();
    ~CustomLookAndFeel() override;
    
    //==============================================================================
    // Color scheme
    static const juce::Colour darkBackground;
    static const juce::Colour darkPanel;
    static const juce::Colour darkControl;
    static const juce::Colour greenAccent;
    static const juce::Colour greenAccentBright;
    static const juce::Colour textColor;
    static const juce::Colour textColorDark;
    
    //==============================================================================
    void drawButtonBackground(juce::Graphics& g,
                            juce::Button& button,
                            const juce::Colour& backgroundColour,
                            bool shouldDrawButtonAsHighlighted,
                            bool shouldDrawButtonAsDown) override;
    
    void drawLinearSlider(juce::Graphics& g,
                         int x, int y,
                         int width, int height,
                         float sliderPos,
                         float minSliderPos,
                         float maxSliderPos,
                         juce::Slider::SliderStyle style,
                         juce::Slider& slider) override;
    
private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CustomLookAndFeel)
};
