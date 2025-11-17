#include "ManifestHandler.h"

//==============================================================================
ManifestHandler::ManifestHandler()
{
}

ManifestHandler::~ManifestHandler()
{
}

//==============================================================================
bool ManifestHandler::parseManifestFile(const juce::File& msuFile, juce::String& baseName)
{
    if (!msuFile.existsAsFile())
    {
        setError("Manifest file does not exist: " + msuFile.getFullPathName());
        return false;
    }
    
    baseName = getBaseName(msuFile);
    
    if (baseName.isEmpty())
    {
        setError("Could not extract base name from manifest file");
        return false;
    }
    
    lastError.clear();
    return true;
}

juce::String ManifestHandler::getBaseName(const juce::File& file)
{
    juce::String filename = file.getFileNameWithoutExtension();
    
    // Remove common suffixes
    filename = filename.replace("-msu1", "")
                      .replace("_msu1", "")
                      .replace(".msu1", "");
    
    return filename;
}

juce::String ManifestHandler::generatePCMFilename(const juce::String& baseName, int trackNumber)
{
    return baseName + "-" + juce::String(trackNumber) + ".pcm";
}

//==============================================================================
bool ManifestHandler::readManifest(const juce::File& msuFile, juce::String& contents)
{
    if (!msuFile.existsAsFile())
    {
        setError("Manifest file does not exist: " + msuFile.getFullPathName());
        return false;
    }
    
    contents = msuFile.loadFileAsString();
    
    lastError.clear();
    return true;
}

bool ManifestHandler::writeManifest(const juce::File& msuFile, const juce::String& contents)
{
    if (!msuFile.replaceWithText(contents))
    {
        setError("Failed to write manifest file: " + msuFile.getFullPathName());
        return false;
    }
    
    lastError.clear();
    return true;
}

void ManifestHandler::updateLoopInfo(juce::String& contents,
                                    int trackNumber,
                                    int64 loopStartSample)
{
    // Format: track-X loop=NNNNN
    juce::String trackPrefix = "track-" + juce::String(trackNumber);
    juce::String loopInfo = " loop=" + juce::String(loopStartSample);
    
    // Find if this track already has loop info
    int trackIndex = contents.indexOf(trackPrefix);
    
    if (trackIndex >= 0)
    {
        // Find end of line
        int lineEnd = contents.indexOfChar(trackIndex, '\n');
        if (lineEnd < 0)
            lineEnd = contents.length();
        
        // Check if loop info already exists
        int loopIndex = contents.indexOf(trackIndex, "loop=");
        
        if (loopIndex >= 0 && loopIndex < lineEnd)
        {
            // Update existing loop info
            int loopEnd = contents.indexOfChar(loopIndex, ' ');
            if (loopEnd < 0 || loopEnd > lineEnd)
                loopEnd = lineEnd;
            
            contents = contents.replaceSection(loopIndex, loopEnd - loopIndex, loopInfo.substring(1));
        }
        else
        {
            // Append loop info to line
            contents = contents.replaceSection(lineEnd, 0, loopInfo);
        }
    }
    else
    {
        // Add new track entry
        if (!contents.isEmpty() && !contents.endsWithChar('\n'))
            contents += "\n";
        
        contents += trackPrefix + loopInfo + "\n";
    }
}

//==============================================================================
void ManifestHandler::setError(const juce::String& error)
{
    lastError = error;
    DBG("ManifestHandler Error: " + error);
}
