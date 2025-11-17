#include "ExportDialog.h"
#include "../UI/CustomLookAndFeel.h"

//==============================================================================
ExportDialog::ExportDialog()
{
    setSize(500, 250);
    
    // Setup labels
    addAndMakeVisible(titleLabel);
    titleLabel.setText("Export MSU-1 PCM", juce::dontSendNotification);
    titleLabel.setFont(juce::FontOptions(18.0f, juce::Font::bold));
    titleLabel.setJustificationType(juce::Justification::centred);
    
    addAndMakeVisible(trackNumberLabel);
    trackNumberLabel.setText("Track Number:", juce::dontSendNotification);
    trackNumberLabel.setJustificationType(juce::Justification::centredLeft);
    
    addAndMakeVisible(destinationLabel);
    destinationLabel.setText("No destination selected", juce::dontSendNotification);
    destinationLabel.setJustificationType(juce::Justification::centredLeft);
    
    // Setup track number slider
    addAndMakeVisible(trackNumberSlider);
    trackNumberSlider.setRange(1, 999, 1);
    trackNumberSlider.setValue(1);
    trackNumberSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 60, 20);
    
    // Setup buttons
    addAndMakeVisible(browseButton);
    browseButton.setButtonText("Browse...");
    browseButton.addListener(this);
    
    addAndMakeVisible(exportButton);
    exportButton.setButtonText("Export");
    exportButton.addListener(this);
    
    addAndMakeVisible(cancelButton);
    cancelButton.setButtonText("Cancel");
    cancelButton.addListener(this);
}

ExportDialog::~ExportDialog()
{
}

//==============================================================================
void ExportDialog::paint(juce::Graphics& g)
{
    g.fillAll(CustomLookAndFeel::darkBackground);
    
    g.setColour(CustomLookAndFeel::darkControl);
    g.drawRect(getLocalBounds(), 2);
}

void ExportDialog::resized()
{
    auto bounds = getLocalBounds().reduced(16);
    
    titleLabel.setBounds(bounds.removeFromTop(30));
    bounds.removeFromTop(16);
    
    trackNumberLabel.setBounds(bounds.removeFromTop(24));
    bounds.removeFromTop(8);
    trackNumberSlider.setBounds(bounds.removeFromTop(40));
    bounds.removeFromTop(16);
    
    auto destRow = bounds.removeFromTop(36);
    browseButton.setBounds(destRow.removeFromRight(100));
    destRow.removeFromRight(8);
    destinationLabel.setBounds(destRow);
    
    bounds.removeFromTop(16);
    
    auto buttonRow = bounds.removeFromBottom(36);
    int buttonWidth = 100;
    
    cancelButton.setBounds(buttonRow.removeFromLeft(buttonWidth));
    buttonRow.removeFromLeft(8);
    exportButton.setBounds(buttonRow.removeFromLeft(buttonWidth));
}

void ExportDialog::buttonClicked(juce::Button* button)
{
    if (button == &exportButton && onExport)
    {
        if (destinationDirectory.isDirectory())
        {
            onExport(static_cast<int>(trackNumberSlider.getValue()), 
                    destinationDirectory);
        }
    }
    else if (button == &cancelButton && onCancel)
    {
        onCancel();
    }
    else if (button == &browseButton)
    {
        auto chooserFlags = juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories;
        
        fileChooser = std::make_unique<juce::FileChooser>("Select destination directory", juce::File{}, "*");
        
        fileChooser->launchAsync(chooserFlags, [this](const juce::FileChooser& chooser)
        {
            auto result = chooser.getResult();
            if (result.isDirectory())
                setDefaultDirectory(result);
        });
    }
}

void ExportDialog::setDefaultDirectory(const juce::File& directory)
{
    destinationDirectory = directory;
    destinationLabel.setText(directory.getFullPathName(), juce::dontSendNotification);
}
