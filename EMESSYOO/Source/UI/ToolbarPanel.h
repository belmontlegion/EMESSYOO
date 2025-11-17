#pragma once

#include <JuceHeader.h>
#include "../Core/MSUProjectState.h"

//==============================================================================
/**
 * Top toolbar with file operations and export controls.
 */
class ToolbarPanel : public juce::Component,
                     public juce::Button::Listener
{
public:
    //==============================================================================
    ToolbarPanel(MSUProjectState& state);
    ~ToolbarPanel() override;
    
    //==============================================================================
    void paint(juce::Graphics& g) override;
    void resized() override;
    void buttonClicked(juce::Button* button) override;
    int getMinimumWidth() const;
    
    //==============================================================================
    // Callbacks (to be connected by MainComponent)
    std::function<void()> onOpenFile;
    std::function<void()> onExport;
    std::function<void()> onOpenSettings;
    std::function<void()> onRestoreBackups;
    
private:
    //==============================================================================
    MSUProjectState& projectState;
    
    juce::TextButton openButton;
    juce::TextButton exportButton;
    juce::TextButton restoreButton;
    juce::TextButton settingsButton;
    
    juce::Label titleLabel;
    juce::Label sampleRateLabel;
    
    inline static constexpr int buttonCount = 4;
    inline static constexpr int buttonSpacing = 8;
    inline static constexpr int buttonMinWidth = 150;
    inline static constexpr int buttonHeight = 32;
    inline static constexpr int leftSectionMinWidth = 260;
    inline static constexpr int horizontalPadding = 16; // 8px inset per side

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ToolbarPanel)
};
