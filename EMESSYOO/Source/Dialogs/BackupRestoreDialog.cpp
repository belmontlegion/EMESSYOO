#include "BackupRestoreDialog.h"

BackupRestoreDialog::BackupRestoreDialog(std::vector<Entry> entries)
    : items(std::move(entries))
{
    addAndMakeVisible(headingLabel);
    headingLabel.setText("Restore Backups", juce::dontSendNotification);
    headingLabel.setFont(juce::FontOptions(20.0f, juce::Font::bold));
    headingLabel.setColour(juce::Label::textColourId, juce::Colours::white);

    addAndMakeVisible(descriptionLabel);
    descriptionLabel.setText(
        "Choose the backups you want to restore. The selected PCM files will replace the current copies.",
        juce::dontSendNotification);
    descriptionLabel.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
    descriptionLabel.setJustificationType(juce::Justification::centredLeft);

    addAndMakeVisible(listBox);
    listBox.setModel(this);
    listBox.setMultipleSelectionEnabled(true);
    listBox.setRowHeight(52);

    addAndMakeVisible(selectAllButton);
    selectAllButton.onClick = [this]
    {
        const int totalRows = getNumRows();
        for (int row = 0; row < totalRows; ++row)
            listBox.selectRow(row, false, false);
    };

    addAndMakeVisible(restoreButton);
    restoreButton.onClick = [this]
    {
        handleRestoreRequest();
    };

    addAndMakeVisible(cancelButton);
    cancelButton.onClick = [this]
    {
        closeParentDialog();
    };
}

void BackupRestoreDialog::resized()
{
    auto bounds = getLocalBounds().reduced(16);
    auto headingArea = bounds.removeFromTop(34);
    headingLabel.setBounds(headingArea);
    descriptionLabel.setBounds(bounds.removeFromTop(40));
    bounds.removeFromTop(8);

    auto buttonRow = bounds.removeFromBottom(44);
    const int spacing = 12;
    int buttonWidth = juce::jmax(140, (buttonRow.getWidth() - 2 * spacing) / 3);
    juce::Rectangle<int> selectArea = buttonRow.removeFromLeft(buttonWidth);
    buttonRow.removeFromLeft(spacing);
    juce::Rectangle<int> restoreArea = buttonRow.removeFromLeft(buttonWidth);
    buttonRow.removeFromLeft(spacing);
    juce::Rectangle<int> cancelArea = buttonRow.removeFromLeft(buttonWidth);

    selectAllButton.setBounds(selectArea);
    restoreButton.setBounds(restoreArea);
    cancelButton.setBounds(cancelArea);

    bounds.removeFromBottom(8);
    listBox.setBounds(bounds);
}

void BackupRestoreDialog::paint(juce::Graphics& g)
{
    g.fillAll(findColour(juce::ResizableWindow::backgroundColourId));
}

void BackupRestoreDialog::dismiss()
{
    closeParentDialog();
}

int BackupRestoreDialog::getNumRows()
{
    return static_cast<int>(items.size());
}

void BackupRestoreDialog::paintListBoxItem(int rowNumber,
                                           juce::Graphics& g,
                                           int width,
                                           int height,
                                           bool rowIsSelected)
{
    if (rowNumber < 0 || rowNumber >= getNumRows())
        return;

    if (rowIsSelected)
        g.fillAll(juce::Colours::darkgrey.withAlpha(0.6f));

    const auto& entry = items[(size_t)rowNumber];
    g.setColour(juce::Colours::white);
    g.setFont(15.0f);
    g.drawText(entry.title,
               12,
               4,
               width - 24,
               height / 2,
               juce::Justification::centredLeft);

    juce::Colour detailColour = entry.metadataAvailable ? juce::Colours::lightgreen : juce::Colours::lightgrey;
    g.setColour(detailColour);
    g.setFont(13.0f);
    g.drawText(entry.detail,
               12,
               height / 2,
               width - 24,
               height / 2,
               juce::Justification::centredLeft);
}

void BackupRestoreDialog::handleRestoreRequest()
{
    if (!onRestore)
        return;

    juce::Array<int> selectedRows;
    auto sparse = listBox.getSelectedRows();
    for (int i = 0; i < sparse.size(); ++i)
        selectedRows.add(sparse[i]);

    if (onRestore(selectedRows))
        closeParentDialog();
}

void BackupRestoreDialog::closeParentDialog()
{
    if (auto* window = findParentComponentOfClass<juce::DialogWindow>())
    {
        window->exitModalState(0);
        window->setVisible(false);
    }
}
