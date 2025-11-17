#pragma once

#include <JuceHeader.h>
#include "../Core/SNESROMReader.h"

//==============================================================================
/**
 * Browser component for MSU-1 track management.
 * Displays tracks from an MSU manifest and allows replacing individual tracks.
 */
class MSUFileBrowser : public juce::Component,
                       public juce::TableListBoxModel
{
public:
    //==============================================================================
    MSUFileBrowser();
    ~MSUFileBrowser() override;
    
    //==============================================================================
    void paint(juce::Graphics& g) override;
    void resized() override;
    
    //==============================================================================
    // TableListBoxModel overrides
    int getNumRows() override;
    void paintRowBackground(juce::Graphics& g, int rowNumber, int width, int height, bool rowIsSelected) override;
    void paintCell(juce::Graphics& g, int rowNumber, int columnId, int width, int height, bool rowIsSelected) override;
    juce::Component* refreshComponentForCell(int rowNumber, int columnId, bool isRowSelected, juce::Component* existingComponentToUpdate) override;
    
    //==============================================================================
    struct TrackInfo
    {
        int trackNumber = 0;
        juce::String fileName;
        juce::String title;
        juce::File file;
        bool exists = false;
        bool backupExists = false;
    };
    
    void loadMSUFile(const juce::File& msuFile);
    void clearTracks();
    void refreshTable() { table.updateContent(); }
    void setInitialDirectory(const juce::File& directory);
    juce::File getCurrentMSUFile() const { return currentMSUFile; }
    const std::vector<TrackInfo>& getTracks() const { return tracks; }
    juce::String getGameTitle() const { return gameTitle; }
    juce::File getCurrentDirectory() const
    {
        if (currentMSUFile.existsAsFile())
            return currentMSUFile.getParentDirectory();
        return lastMSUDirectory;
    }
    void launchLoadDialog();
    
    void setPreviewingRow(int row) 
    { 
        if (currentPreviewRow >= 0 && currentPreviewRow != row)
            table.repaintRow(currentPreviewRow); // Repaint old row
        currentPreviewRow = row; 
        table.repaintRow(row); // Repaint new row
    }
    void clearPreviewingRow() { if (currentPreviewRow >= 0) { int oldRow = currentPreviewRow; currentPreviewRow = -1; table.repaintRow(oldRow); } }
    int getPreviewingRow() const { return currentPreviewRow; }
    
    std::function<void(const TrackInfo&)> onReplaceTrack;
    std::function<void(const TrackInfo&)> onPreviewTrack;
    std::function<void()> onStopPreview;
    std::function<void(const juce::File&)> onDirectoryChanged;
    std::function<void(const juce::File&, const juce::String&, const std::vector<TrackInfo>&)> onTracksLoaded;
    std::function<void()> onTracksCleared;
    
    juce::TableListBox table;
    std::vector<TrackInfo> tracks;
    
private:
    //==============================================================================
    juce::TextButton loadButton;
    juce::Label gameTitleLabel;
    
    juce::File currentMSUFile;
    juce::String gameTitle;
    int currentPreviewRow = -1;
    juce::File lastMSUDirectory;
    
    void parseMSUManifest(const juce::File& msuFile);
    
    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MSUFileBrowser)
};

//==============================================================================
/**
 * Button component for the "Replace" action in table cells.
 */
class ReplaceButton : public juce::Component
{
public:
    ReplaceButton(MSUFileBrowser& owner, int row)
        : ownerBrowser(owner), rowNumber(row)
    {
        addAndMakeVisible(button);
        button.setButtonText("Replace");
        button.onClick = [this] { buttonClicked(); };
    }
    
    void resized() override
    {
        button.setBounds(getLocalBounds().reduced(2));
    }
    
    void setRowNumber(int newRow)
    {
        rowNumber = newRow;
    }
    
private:
    MSUFileBrowser& ownerBrowser;
    juce::TextButton button;
    int rowNumber;
    
    void buttonClicked();
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ReplaceButton)
};

//==============================================================================
/**
 * Button component for the "Preview" action in table cells.
 */
class PreviewButton : public juce::Component
{
public:
    PreviewButton(MSUFileBrowser& owner, int row)
        : ownerBrowser(owner), rowNumber(row)
    {
        addAndMakeVisible(button);
        button.setButtonText("Preview");
        button.onClick = [this] { buttonClicked(); };
    }
    
    void resized() override
    {
        button.setBounds(getLocalBounds().reduced(2));
    }
    
    void setRowNumber(int newRow)
    {
        rowNumber = newRow;
        updateButtonState();
    }
    
    void updateButtonState()
    {
        bool isPreviewing = (ownerBrowser.getPreviewingRow() == rowNumber);
        button.setButtonText(isPreviewing ? "Stop" : "Preview");
        button.setColour(juce::TextButton::buttonColourId, 
                        isPreviewing ? juce::Colours::darkred : juce::Colours::darkgrey);
    }
    
    void paint(juce::Graphics& g) override
    {
        juce::ignoreUnused(g);

        updateButtonState();
    }
    
private:
    MSUFileBrowser& ownerBrowser;
    juce::TextButton button;
    int rowNumber;
    
    void buttonClicked();
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PreviewButton)
};
