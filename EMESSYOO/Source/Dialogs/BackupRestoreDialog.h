#pragma once

#include <JuceHeader.h>
#include <vector>

class BackupRestoreDialog : public juce::Component,
                            private juce::ListBoxModel
{
public:
    struct Entry
    {
        juce::String title;
        juce::String detail;
        bool metadataAvailable = false;
    };

    explicit BackupRestoreDialog(std::vector<Entry> entries);

    using RestoreCallback = std::function<bool(const juce::Array<int>&)>;
    RestoreCallback onRestore;

    void resized() override;
    void paint(juce::Graphics& g) override;
    void dismiss();

private:
    juce::Label headingLabel;
    juce::Label descriptionLabel;
    juce::ListBox listBox;
    juce::TextButton selectAllButton { "Select All" };
    juce::TextButton restoreButton { "Restore Selected" };
    juce::TextButton cancelButton { "Cancel" };
    std::vector<Entry> items;

    int getNumRows() override;
    void paintListBoxItem(int rowNumber,
                          juce::Graphics& g,
                          int width,
                          int height,
                          bool rowIsSelected) override;

    void handleRestoreRequest();
    void closeParentDialog();
};
