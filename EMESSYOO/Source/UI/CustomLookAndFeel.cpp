#include "CustomLookAndFeel.h"

//==============================================================================
// Color definitions
const juce::Colour CustomLookAndFeel::darkBackground    = juce::Colour(0xff1a1a1a);
const juce::Colour CustomLookAndFeel::darkPanel         = juce::Colour(0xff252525);
const juce::Colour CustomLookAndFeel::darkControl       = juce::Colour(0xff333333);
const juce::Colour CustomLookAndFeel::greenAccent       = juce::Colour(0xff00cc66);
const juce::Colour CustomLookAndFeel::greenAccentBright = juce::Colour(0xff00ff80);
const juce::Colour CustomLookAndFeel::textColor         = juce::Colour(0xffe0e0e0);
const juce::Colour CustomLookAndFeel::textColorDark     = juce::Colour(0xff808080);

//==============================================================================
CustomLookAndFeel::CustomLookAndFeel()
{
    // Set color scheme
    setColour(juce::ResizableWindow::backgroundColourId, darkBackground);
    setColour(juce::DocumentWindow::backgroundColourId, darkBackground);
    
    // Text colors
    setColour(juce::Label::textColourId, textColor);
    setColour(juce::TextButton::textColourOffId, textColor);
    setColour(juce::TextButton::textColourOnId, textColor);
    
    // Button colors
    setColour(juce::TextButton::buttonColourId, darkControl);
    setColour(juce::TextButton::buttonOnColourId, greenAccent);
    
    // Slider colors
    setColour(juce::Slider::thumbColourId, greenAccent);
    setColour(juce::Slider::trackColourId, greenAccent);
    setColour(juce::Slider::backgroundColourId, darkControl);
    
    // Panel colors
    setColour(juce::GroupComponent::outlineColourId, darkControl);
    setColour(juce::GroupComponent::textColourId, textColor);
}

CustomLookAndFeel::~CustomLookAndFeel()
{
}

//==============================================================================
void CustomLookAndFeel::drawButtonBackground(juce::Graphics& g,
                                            juce::Button& button,
                                            const juce::Colour& backgroundColour,
                                            bool shouldDrawButtonAsHighlighted,
                                            bool shouldDrawButtonAsDown)
{
    auto bounds = button.getLocalBounds().toFloat().reduced(0.5f, 0.5f);
    auto baseColour = backgroundColour;
    
    if (button.getToggleState())
        baseColour = greenAccent;
    else if (shouldDrawButtonAsDown)
        baseColour = baseColour.darker(0.2f);
    else if (shouldDrawButtonAsHighlighted)
        baseColour = baseColour.brighter(0.1f);
    
    // Draw rounded rectangle
    g.setColour(baseColour);
    g.fillRoundedRectangle(bounds, 4.0f);
    
    // Draw border
    g.setColour(baseColour.brighter(0.2f));
    g.drawRoundedRectangle(bounds, 4.0f, 1.0f);
}

void CustomLookAndFeel::drawLinearSlider(juce::Graphics& g,
                                        int x, int y,
                                        int width, int height,
                                        float sliderPos,
                                        float minSliderPos,
                                        float maxSliderPos,
                                        juce::Slider::SliderStyle style,
                                        juce::Slider& slider)
{
    juce::ignoreUnused(minSliderPos, maxSliderPos, style);

    if (slider.isBar())
    {
        g.setColour(slider.findColour(juce::Slider::trackColourId));
        g.fillRect(slider.isHorizontal() ? juce::Rectangle<float>(static_cast<float>(x), (float)y + 0.5f, sliderPos - (float)x, (float)height - 1.0f)
                                         : juce::Rectangle<float>((float)x + 0.5f, sliderPos, (float)width - 1.0f, (float)y + ((float)height - sliderPos)));
    }
    else
    {
        auto isHorizontal = slider.isHorizontal();
        auto trackWidth = juce::jmin(6.0f, isHorizontal ? (float)height * 0.25f : (float)width * 0.25f);
        
        juce::Point<float> startPoint(isHorizontal ? (float)x : (float)x + (float)width * 0.5f,
                                     isHorizontal ? (float)y + (float)height * 0.5f : (float)(height + y));
        
        juce::Point<float> endPoint(isHorizontal ? (float)(width + x) : startPoint.x,
                                   isHorizontal ? startPoint.y : (float)y);
        
        juce::Path backgroundTrack;
        backgroundTrack.startNewSubPath(startPoint);
        backgroundTrack.lineTo(endPoint);
        g.setColour(slider.findColour(juce::Slider::backgroundColourId));
        g.strokePath(backgroundTrack, {trackWidth, juce::PathStrokeType::curved, juce::PathStrokeType::rounded});
        
        juce::Path valueTrack;
        juce::Point<float> minPoint, maxPoint, thumbPoint;
        
        if (isHorizontal)
        {
            minPoint = startPoint;
            maxPoint = {sliderPos, (float)y + (float)height * 0.5f};
            thumbPoint = maxPoint;
        }
        else
        {
            minPoint = {(float)x + (float)width * 0.5f, sliderPos};
            maxPoint = endPoint;
            thumbPoint = minPoint;
        }
        
        valueTrack.startNewSubPath(minPoint);
        valueTrack.lineTo(maxPoint);
        g.setColour(slider.findColour(juce::Slider::trackColourId));
        g.strokePath(valueTrack, {trackWidth, juce::PathStrokeType::curved, juce::PathStrokeType::rounded});
        
        // Draw thumb
        auto thumbWidth = juce::jmax(trackWidth * 1.5f, 12.0f);
        g.setColour(slider.findColour(juce::Slider::thumbColourId));
        g.fillEllipse(juce::Rectangle<float>(thumbWidth, thumbWidth).withCentre(thumbPoint));
    }
}
