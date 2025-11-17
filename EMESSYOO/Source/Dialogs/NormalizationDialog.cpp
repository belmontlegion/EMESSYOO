#include "NormalizationDialog.h"
#include "../UI/CustomLookAndFeel.h"

//==============================================================================
NormalizationDialog::NormalizationDialog()
{
    setSize(400, 300);
    
    // Setup labels
    addAndMakeVisible(titleLabel);
    titleLabel.setText("Normalization", juce::dontSendNotification);
    titleLabel.setFont(juce::FontOptions(18.0f, juce::Font::bold));
    titleLabel.setJustificationType(juce::Justification::centred);
    
    addAndMakeVisible(instructionsLabel);
    instructionsLabel.setText("Adjust target RMS level:", juce::dontSendNotification);
    instructionsLabel.setJustificationType(juce::Justification::centredLeft);
    
    // Setup slider
    addAndMakeVisible(targetSlider);
    targetSlider.setRange(-24.0, 0.0, 0.1);
    targetSlider.setValue(-12.0);
    targetSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 60, 20);
    targetSlider.setTextValueSuffix(" dB");
    
    // Setup buttons
    addAndMakeVisible(analyzeButton);
    analyzeButton.setButtonText("Analyze Folder");
    analyzeButton.addListener(this);
    
    addAndMakeVisible(normalizeButton);
    normalizeButton.setButtonText("Apply Normalization");
    normalizeButton.addListener(this);
    
    addAndMakeVisible(cancelButton);
    cancelButton.setButtonText("Cancel");
    cancelButton.addListener(this);
}

NormalizationDialog::~NormalizationDialog()
{
}

//==============================================================================
void NormalizationDialog::paint(juce::Graphics& g)
{
    g.fillAll(CustomLookAndFeel::darkBackground);
    
    g.setColour(CustomLookAndFeel::darkControl);
    g.drawRect(getLocalBounds(), 2);
}

void NormalizationDialog::resized()
{
    auto bounds = getLocalBounds().reduced(16);
    
    titleLabel.setBounds(bounds.removeFromTop(30));
    bounds.removeFromTop(16);
    
    instructionsLabel.setBounds(bounds.removeFromTop(24));
    bounds.removeFromTop(8);
    
    targetSlider.setBounds(bounds.removeFromTop(40));
    bounds.removeFromTop(16);
    
    analyzeButton.setBounds(bounds.removeFromTop(36));
    bounds.removeFromTop(32);
    
    auto buttonRow = bounds.removeFromBottom(36);
    int buttonWidth = 140;
    
    cancelButton.setBounds(buttonRow.removeFromLeft(buttonWidth));
    buttonRow.removeFromLeft(8);
    normalizeButton.setBounds(buttonRow.removeFromLeft(buttonWidth));
}

void NormalizationDialog::buttonClicked(juce::Button* button)
{
    if (button == &normalizeButton && onNormalize)
    {
        onNormalize(static_cast<float>(targetSlider.getValue()));
    }
    else if (button == &cancelButton && onCancel)
    {
        onCancel();
    }
    else if (button == &analyzeButton)
    {
        // Analyze folder functionality to be implemented
    }
}
