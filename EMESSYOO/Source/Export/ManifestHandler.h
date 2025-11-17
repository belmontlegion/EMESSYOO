#pragma once

#include <JuceHeader.h>

//==============================================================================
/**
 * Handles reading and writing MSU-1 manifest (.msu) files.
 * Manages track naming and metadata.
 */
class ManifestHandler
{
public:
    //==============================================================================
    ManifestHandler();
    ~ManifestHandler();
    
    //==============================================================================
    /**
     * Parse an MSU file to extract base name.
     * @param msuFile The .msu manifest file
     * @param baseName Output base name (e.g., "zelda3")
     * @return true if successful
     */
    bool parseManifestFile(const juce::File& msuFile, juce::String& baseName);
    
    /**
     * Get the base name from an MSU file or ROM directory.
     * @param file Either an .msu file or ROM file
     * @return Base name for track naming
     */
    static juce::String getBaseName(const juce::File& file);
    
    /**
     * Generate PCM filename for a track number.
     * @param baseName Base name from manifest
     * @param trackNumber Track number (1-based)
     * @return Formatted filename (e.g., "zelda3-1.pcm")
     */
    static juce::String generatePCMFilename(const juce::String& baseName, int trackNumber);
    
    /**
     * Read manifest file contents.
     * @param msuFile The .msu manifest file
     * @param contents Output file contents
     * @return true if successful
     */
    bool readManifest(const juce::File& msuFile, juce::String& contents);
    
    /**
     * Write manifest file contents.
     * @param msuFile The .msu manifest file
     * @param contents Contents to write
     * @return true if successful
     */
    bool writeManifest(const juce::File& msuFile, const juce::String& contents);
    
    /**
     * Add or update loop information in manifest.
     * @param contents Manifest contents (will be modified)
     * @param trackNumber Track number
     * @param loopStartSample Loop start sample position
     */
    static void updateLoopInfo(juce::String& contents,
                              int trackNumber,
                              int64 loopStartSample);
    
    /**
     * Get the last error message.
     */
    juce::String getLastError() const { return lastError; }
    
private:
    //==============================================================================
    juce::String lastError;
    
    void setError(const juce::String& error);
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ManifestHandler)
};
