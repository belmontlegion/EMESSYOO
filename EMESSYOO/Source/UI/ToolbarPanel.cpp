#include "ToolbarPanel.h"
#include "CustomLookAndFeel.h"

//==============================================================================
ToolbarPanel::ToolbarPanel(MSUProjectState& state)
    : projectState(state)
{
    // Setup buttons
    addAndMakeVisible(openButton);
    openButton.setButtonText("Open Audio File...");
    openButton.addListener(this);
    
    addAndMakeVisible(exportButton);
    exportButton.setButtonText("Export PCM");
    exportButton.addListener(this);

    addAndMakeVisible(restoreButton);
    restoreButton.setButtonText("Restore Backups");
    restoreButton.addListener(this);
    
    addAndMakeVisible(settingsButton);
    settingsButton.setButtonText("Settings");
    settingsButton.addListener(this);
    
    // Setup labels
    addAndMakeVisible(titleLabel);
    titleLabel.setText("EMESSYOO", juce::dontSendNotification);
    titleLabel.setFont(juce::FontOptions(20.0f, juce::Font::bold));
    titleLabel.setColour(juce::Label::textColourId, CustomLookAndFeel::greenAccent);
    titleLabel.setJustificationType(juce::Justification::centredLeft);
    
    addAndMakeVisible(sampleRateLabel);
    sampleRateLabel.setText("44.1 kHz / 16-bit Stereo", juce::dontSendNotification);
    sampleRateLabel.setFont(juce::FontOptions(12.0f));
    sampleRateLabel.setColour(juce::Label::textColourId, CustomLookAndFeel::textColorDark);
    sampleRateLabel.setJustificationType(juce::Justification::centredLeft);
}

ToolbarPanel::~ToolbarPanel()
{
}

//==============================================================================
void ToolbarPanel::paint(juce::Graphics& g)
{
    g.fillAll(CustomLookAndFeel::darkPanel);
    
    // Draw bottom border
    g.setColour(CustomLookAndFeel::darkControl);
    g.drawLine(0.0f, static_cast<float>(getHeight()), 
               static_cast<float>(getWidth()), static_cast<float>(getHeight()), 1.0f);
}

void ToolbarPanel::resized()
{
    auto bounds = getLocalBounds().reduced(8);

    const int buttonRowMinWidth = buttonCount * buttonMinWidth + buttonSpacing * (buttonCount - 1);
    const int availableWidth = bounds.getWidth();
    const int leftWidth = juce::jmax(leftSectionMinWidth, availableWidth - buttonRowMinWidth);

    // Left side: title
    auto leftSection = bounds.removeFromLeft(leftWidth);
    titleLabel.setBounds(leftSection.removeFromTop(28));
    sampleRateLabel.setBounds(leftSection);

    // Right side: buttons
    auto buttonRow = bounds;
    const int totalSpacing = buttonSpacing * (buttonCount - 1);
    int buttonWidth = juce::jmax(buttonMinWidth, (buttonRow.getWidth() - totalSpacing) / buttonCount);
    const int totalButtonWidth = buttonWidth * buttonCount + totalSpacing;
    buttonRow.setWidth(totalButtonWidth);
    buttonRow.setHeight(buttonHeight);
    buttonRow.setY(bounds.getCentreY() - buttonHeight / 2);
    buttonRow.setX(bounds.getRight() - totalButtonWidth);

    auto placeButton = [&](juce::Button& button)
    {
        button.setBounds(buttonRow.removeFromLeft(buttonWidth));
        buttonRow.removeFromLeft(buttonSpacing);
    };

    placeButton(openButton);
    placeButton(exportButton);
    placeButton(restoreButton);
    settingsButton.setBounds(buttonRow.removeFromLeft(buttonWidth));
}

int ToolbarPanel::getMinimumWidth() const
{
    const int buttonRowMinWidth = buttonCount * buttonMinWidth + buttonSpacing * (buttonCount - 1);
    return leftSectionMinWidth + buttonRowMinWidth + horizontalPadding;
}

void ToolbarPanel::buttonClicked(juce::Button* button)
{
    if (button == &openButton && onOpenFile)
    {
        onOpenFile();
    }
    else if (button == &exportButton && onExport)
    {
        onExport();
    }
    else if (button == &restoreButton && onRestoreBackups)
    {
        onRestoreBackups();
    }
    else if (button == &settingsButton && onOpenSettings)
    {
        onOpenSettings();
    }
}
