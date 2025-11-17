#include "MSUFileBrowser.h"
#include "../Export/MSUManifestUpdater.h"
#include <algorithm>
MSUFileBrowser::MSUFileBrowser()
{
    addAndMakeVisible(loadButton);
    loadButton.setButtonText("Load MSU-1");
    loadButton.onClick = [this] { launchLoadDialog(); };
    
    addAndMakeVisible(gameTitleLabel);
    gameTitleLabel.setFont(juce::FontOptions(16.0f, juce::Font::bold));
    gameTitleLabel.setJustificationType(juce::Justification::centredLeft);
    gameTitleLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    gameTitleLabel.setText("No ROM loaded", juce::dontSendNotification);
    
    addAndMakeVisible(table);
    table.setModel(this);
    table.setColour(juce::ListBox::outlineColourId, juce::Colours::grey);
    table.setOutlineThickness(1);
    
    // Add columns
    table.getHeader().addColumn("Track", 1, 60, 50, 80, juce::TableHeaderComponent::notResizable);
    table.getHeader().addColumn("Title / File Name", 2, 300, 100, -1, juce::TableHeaderComponent::defaultFlags);
    table.getHeader().addColumn("Status", 3, 80, 60, 100, juce::TableHeaderComponent::notResizable);
    table.getHeader().addColumn("Backup Exists", 4, 120, 90, 160, juce::TableHeaderComponent::notResizable);
    table.getHeader().addColumn("Preview", 5, 100, 80, 120, juce::TableHeaderComponent::notResizable);
    table.getHeader().addColumn("Action", 6, 100, 80, 120, juce::TableHeaderComponent::notResizable);
}

MSUFileBrowser::~MSUFileBrowser()
{
    table.setModel(nullptr);
}

//==============================================================================
void MSUFileBrowser::paint(juce::Graphics& g)
{
    g.fillAll(getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId));
}

void MSUFileBrowser::resized()
{
    auto bounds = getLocalBounds();
    loadButton.setBounds(bounds.removeFromTop(30).reduced(4));
    gameTitleLabel.setBounds(bounds.removeFromTop(30).reduced(4));
    table.setBounds(bounds.reduced(4));
}

//==============================================================================
int MSUFileBrowser::getNumRows()
{
    return static_cast<int>(tracks.size());
}

void MSUFileBrowser::paintRowBackground(juce::Graphics& g, int rowNumber, int width, int height, bool rowIsSelected)
{
    juce::ignoreUnused(width, height);

    if (rowIsSelected)
        g.fillAll(juce::Colours::lightblue);
    else if (rowNumber % 2)
        g.fillAll(juce::Colour(0xff303030));
}

void MSUFileBrowser::paintCell(juce::Graphics& g, int rowNumber, int columnId, int width, int height, bool rowIsSelected)
{
    g.setColour(rowIsSelected ? juce::Colours::darkblue : getLookAndFeel().findColour(juce::ListBox::textColourId));
    
    if (rowNumber >= tracks.size())
        return;
    
    auto& track = tracks[rowNumber];
    juce::String text;
    
    switch (columnId)
    {
        case 1: // Track number
            text = juce::String(track.trackNumber);
            break;
        case 2: // Display the actual PCM filename to keep labels consistent
        {
            if (track.fileName.isNotEmpty())
                text = track.fileName;
            else
                text = track.title;
            break;
        }
            break;
        case 3: // Status
            text = track.exists ? "Found" : "Missing";
            g.setColour(track.exists ? juce::Colours::green : juce::Colours::orange);
            break;
        case 4: // Backup Exists
            text = track.backupExists ? "Yes" : "";
            break;
        case 5: // Preview (button is drawn separately)
        case 6: // Action (button is drawn separately)
            return;
    }
    
    g.drawText(text, 2, 0, width - 4, height, juce::Justification::centredLeft, true);
}

juce::Component* MSUFileBrowser::refreshComponentForCell(int rowNumber, int columnId, bool isRowSelected, juce::Component* existingComponentToUpdate)
{
    juce::ignoreUnused(isRowSelected);

    if (columnId == 5) // Preview column
    {
        auto* button = dynamic_cast<PreviewButton*>(existingComponentToUpdate);
        
        if (button == nullptr)
            button = new PreviewButton(*this, rowNumber);
        else
            button->setRowNumber(rowNumber);
        
        return button;
    }
    else if (columnId == 6) // Action column
    {
        auto* button = dynamic_cast<ReplaceButton*>(existingComponentToUpdate);
        
        if (button == nullptr)
            button = new ReplaceButton(*this, rowNumber);
        else
            button->setRowNumber(rowNumber);
        
        return button;
    }
    
    delete existingComponentToUpdate;
    return nullptr;
}

//==============================================================================
void MSUFileBrowser::loadMSUFile(const juce::File& msuFile)
{
    if (!msuFile.existsAsFile())
        return;

    currentMSUFile = msuFile;
    parseMSUManifest(msuFile);
    table.updateContent();
}

void MSUFileBrowser::clearTracks()
{
    tracks.clear();
    currentMSUFile = juce::File();
    gameTitle.clear();
    gameTitleLabel.setText("No ROM loaded", juce::dontSendNotification);
    table.updateContent();
    if (onTracksCleared)
        onTracksCleared();
}

void MSUFileBrowser::setInitialDirectory(const juce::File& directory)
{
    if (directory.getFullPathName().isNotEmpty())
        lastMSUDirectory = directory;
}

void MSUFileBrowser::launchLoadDialog()
{
    const auto initialDirectory = lastMSUDirectory.getFullPathName().isNotEmpty()
        ? lastMSUDirectory
        : juce::File();
    auto* chooser = new juce::FileChooser("Select MSU-1 manifest file...",
                                          initialDirectory,
                                          "*.msu");

    chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
        [this, chooser](const juce::FileChooser& fc)
        {
            auto file = fc.getResult();

            if (file != juce::File())
            {
                // Remember this directory for next time
                lastMSUDirectory = file.getParentDirectory();
                if (onDirectoryChanged)
                    onDirectoryChanged(lastMSUDirectory);
                loadMSUFile(file);
            }

            delete chooser;
        });
}

void MSUFileBrowser::parseMSUManifest(const juce::File& msuFile)
{
    tracks.clear();
    gameTitle = "";
    
    DBG("Loading MSU directory: " + msuFile.getFullPathName());
    
    // Get the base name from the MSU file (e.g., "game.msu" -> "game")
    auto baseName = msuFile.getFileNameWithoutExtension();
    auto directory = msuFile.getParentDirectory();
    
    DBG("Base name: " + baseName);
    DBG("Directory: " + directory.getFullPathName());
    
    // Try to find and load SNES ROM file for metadata
    auto romFiles = directory.findChildFiles(juce::File::findFiles, false, baseName + ".sfc;" + baseName + ".smc");
    
    if (romFiles.size() == 0)
    {
        // Try case-insensitive search
        romFiles = directory.findChildFiles(juce::File::findFiles, false, "*.sfc;*.smc");
    }
    
    if (romFiles.size() > 0)
    {
        try
        {
            SNESROMReader romReader;
            if (romReader.loadROMFile(romFiles[0]))
            {
                gameTitle = romReader.getGameTitle();
                auto region = romReader.getRegion();
                
                if (gameTitle.isNotEmpty())
                {
                    gameTitleLabel.setText(gameTitle + " (" + region + ")", juce::dontSendNotification);
                    DBG("ROM Title: " + gameTitle + " [" + region + "]");
                }
            }
        }
        catch (...)
        {
            DBG("Error reading ROM file: " + romFiles[0].getFullPathName());
            gameTitle = "";
        }
    }
    
    if (gameTitle.isEmpty())
    {
        gameTitleLabel.setText("ROM file not found - " + baseName, juce::dontSendNotification);
    }
    
    // Parse manifest files for track titles
    std::map<int, juce::String> trackTitles;
    
    // Try to read .msu manifest file
    if (msuFile.existsAsFile())
    {
        juce::StringArray lines;
        msuFile.readLines(lines);
        
        for (int i = 0; i < lines.size(); ++i)
        {
            auto line = lines[i].trim();
            auto lowerLine = line.toLowerCase();
            
            // Look for patterns like: "track 1 Title Name" or "track-1 Title Name"
            if (lowerLine.startsWith("track"))
            {
                // Extract track number
                auto parts = juce::StringArray::fromTokens(line, " \t", "\"");
                if (parts.size() >= 2)
                {
                    juce::String trackNumStr = parts[1].trimCharactersAtStart("-");
                    int trackNum = trackNumStr.getIntValue();
                    
                    if (trackNum > 0 && parts.size() >= 3)
                    {
                        // Join remaining parts as title (skip "track" and number)
                        juce::String title;
                        for (int j = 2; j < parts.size(); ++j)
                        {
                            if (!parts[j].endsWithIgnoreCase(".pcm"))
                            {
                                if (title.isNotEmpty()) title += " ";
                                title += parts[j];
                            }
                        }
                        
                        if (title.isNotEmpty())
                        {
                            trackTitles[trackNum] = title.trim();
                            DBG("Found title for track " + juce::String(trackNum) + ": " + title);
                        }
                    }
                }
            }
        }
    }
    
    // Try to read .bml manifest file
    auto bmlFiles = directory.findChildFiles(juce::File::findFiles, false, "*.bml");
    if (bmlFiles.size() > 0)
    {
        juce::String bmlContent = bmlFiles[0].loadFileAsString();
        auto lines = juce::StringArray::fromLines(bmlContent);
        
        int currentTrackNum = -1;
        for (int i = 0; i < lines.size(); ++i)
        {
            auto line = lines[i].trim();
            auto lowerLine = line.toLowerCase();
            
            // Look for "number=X" or "track X"
            if (lowerLine.contains("number=") || lowerLine.startsWith("track"))
            {
                auto numStart = lowerLine.indexOf("=");
                if (numStart >= 0)
                {
                    auto numStr = line.substring(numStart + 1).trim();
                    currentTrackNum = numStr.getIntValue();
                }
                else
                {
                    auto parts = juce::StringArray::fromTokens(line, " \t", "");
                    if (parts.size() >= 2)
                        currentTrackNum = parts[1].getIntValue();
                }
            }
            
            // Look for "title=" or "name="
            if (currentTrackNum > 0 && (lowerLine.contains("title=") || lowerLine.contains("name=")))
            {
                int titleIdx = lowerLine.indexOf("title=");
                int nameIdx = lowerLine.indexOf("name=");
                auto titleStart = juce::jmax(titleIdx, nameIdx);
                
                if (titleStart >= 0)
                {
                    auto eqPos = line.indexOf(titleStart, "=");
                    if (eqPos >= 0)
                    {
                        auto titleStr = line.substring(eqPos + 1).trim();
                        titleStr = titleStr.trimCharactersAtStart("\"").trimCharactersAtEnd("\"");
                        
                        if (titleStr.isNotEmpty())
                        {
                            trackTitles[currentTrackNum] = titleStr;
                            DBG("Found BML title for track " + juce::String(currentTrackNum) + ": " + titleStr);
                        }
                    }
                }
            }
        }
    }
    
    // Scan directory for PCM files matching the pattern: basename-N.pcm
    auto pcmFiles = directory.findChildFiles(juce::File::findFiles, false, baseName + "-*.pcm");
    
    DBG("Found " + juce::String(pcmFiles.size()) + " PCM files");
    
    for (auto& pcmFile : pcmFiles)
    {
        auto fileName = pcmFile.getFileNameWithoutExtension();
        
        // Extract track number from filename (e.g., "game-1" -> 1)
        auto lastDash = fileName.lastIndexOf("-");
        if (lastDash >= 0)
        {
            auto trackNumStr = fileName.substring(lastDash + 1);
            int trackNum = trackNumStr.getIntValue();
            
            if (trackNum > 0)
            {
                TrackInfo info;
                info.trackNumber = trackNum;
                info.fileName = pcmFile.getFileName();
                info.file = pcmFile;
                info.exists = true;
                const auto backupFile = pcmFile.getParentDirectory()
                                              .getChildFile("Backup")
                                              .getChildFile(pcmFile.getFileName());
                info.backupExists = backupFile.existsAsFile();
                
                // Check if we have a title for this track
                if (trackTitles.find(trackNum) != trackTitles.end())
                {
                    info.title = trackTitles[trackNum];
                }
                
                DBG("Added track " + juce::String(info.trackNumber) + ": " + info.fileName + 
                    (info.title.isNotEmpty() ? " (" + info.title + ")" : ""));
                
                tracks.push_back(info);
            }
        }
    }
    
    // Sort tracks by track number
    std::sort(tracks.begin(), tracks.end(), 
        [](const TrackInfo& a, const TrackInfo& b) { 
            return a.trackNumber < b.trackNumber; 
        });
    
    DBG("Total tracks loaded: " + juce::String(tracks.size()));

    if (onTracksLoaded)
        onTracksLoaded(currentMSUFile, gameTitle, tracks);
}

//==============================================================================
void ReplaceButton::buttonClicked()
{
    if (rowNumber >= ownerBrowser.tracks.size())
        return;
    
    auto& track = ownerBrowser.tracks[rowNumber];
    
    if (ownerBrowser.onReplaceTrack)
    {
        ownerBrowser.onReplaceTrack(track);
    }
}

//==============================================================================
void PreviewButton::buttonClicked()
{
    if (rowNumber >= ownerBrowser.tracks.size())
        return;
    
    // If this row is currently previewing, stop it
    if (ownerBrowser.getPreviewingRow() == rowNumber)
    {
        if (ownerBrowser.onStopPreview)
        {
            ownerBrowser.onStopPreview();
        }
        return;
    }
    
    auto& track = ownerBrowser.tracks[rowNumber];
    
    if (!track.exists)
        return; // Can't preview missing tracks
    
    if (ownerBrowser.onPreviewTrack)
    {
        ownerBrowser.onPreviewTrack(track);
    }
}
