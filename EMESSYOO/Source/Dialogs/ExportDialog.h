#pragma once

#include <JuceHeader.h>

//==============================================================================
/**
 * Dialog for PCM export settings.
 * Allows user to specify track number and destination.
 */
class ExportDialog : public juce::Component,
                     public juce::Button::Listener
{
public:
    //==============================================================================
    ExportDialog();
    ~ExportDialog() override;
    
    //==============================================================================
    void paint(juce::Graphics& g) override;
    void resized() override;
    void buttonClicked(juce::Button* button) override;
    
    //==============================================================================
    std::function<void(int, juce::File)> onExport;
    std::function<void()> onCancel;
    
    void setDefaultDirectory(const juce::File& directory);
    
private:
    //==============================================================================
    juce::Label titleLabel;
    juce::Label trackNumberLabel;
    juce::Label destinationLabel;
    
    juce::Slider trackNumberSlider;
    juce::TextButton browseButton;
    juce::TextButton exportButton;
    juce::TextButton cancelButton;
    
    juce::File destinationDirectory;
    std::unique_ptr<juce::FileChooser> fileChooser;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ExportDialog)
};
