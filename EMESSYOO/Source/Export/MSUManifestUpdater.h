#pragma once

#include <JuceHeader.h>

//==============================================================================
/**
 * Updates MSU-1 manifest files with track loop information.
 * Supports legacy .msu manifests as well as modern .bml manifests.
 * Manifest files typically contain track metadata and loop points.
 */
class MSUManifestUpdater
{
public:
    struct MetadataSnapshot
    {
        bool trackExisted = false;
        bool loopExisted = false;
        int64 loopStart = 0;
        int64 loopEnd = -1;
    };

    //==============================================================================
    MSUManifestUpdater();
    ~MSUManifestUpdater();
    
    //==============================================================================
    /**
     * Update MSU manifest file with loop point for a specific track.
     * @param msuFile Path to .msu manifest file
     * @param pcmFileName Name of the PCM file (e.g., "game-1.pcm")
     * @param loopStartSample Loop start position in samples
     * @return true if successful
     */
    bool updateManifest(const juce::File& manifestFile,
                       const juce::String& pcmFileName,
                       int64 loopStartSample,
                       int64 loopEndSample = -1);
    
    /**
     * Read loop point from MSU manifest file for a specific PCM file.
     * @param msuFile Path to .msu manifest file
     * @param pcmFileName Name of the PCM file (e.g., "game-1.pcm")
     * @param loopStartSample Output loop start position in samples
     * @return true if loop point was found
     */
    bool readLoopPoint(const juce::File& msuFile,
                      const juce::String& pcmFileName,
                      int64& loopStartSample,
                      int64* loopEndSample = nullptr);
    
    /**
     * Find MSU file in the same directory as a PCM file.
     * Looks for .msu files with matching base name.
     * @param pcmFile The PCM file
     * @return MSU file if found, invalid File if not
     */
    static juce::File findMSUFile(const juce::File& pcmFile);

    /**
     * Find all manifest files (.msu and .bml) that reference a PCM file.
     * @param pcmFile The PCM file
     * @return Array of manifest files (may be empty)
     */
    static juce::Array<juce::File> findRelatedManifestFiles(const juce::File& pcmFile);

    static bool captureMetadataSnapshot(const juce::File& manifestFile,
                                        const juce::String& pcmFileName,
                                        MetadataSnapshot& snapshot);

    bool restoreMetadataSnapshot(const juce::File& manifestFile,
                                 const juce::String& pcmFileName,
                                 const MetadataSnapshot& snapshot);
    
    /**
     * Get last error message.
     */
    juce::String getLastError() const { return lastError; }
    
private:
    //==============================================================================
    juce::String lastError;

    bool updateMSUTextManifest(const juce::File& msuFile,
                               const juce::String& pcmFileName,
                               int64 loopStartSample,
                               int64 loopEndSample);

    bool updateBMLManifest(const juce::File& bmlFile,
                           const juce::String& pcmFileName,
                           int64 loopStartSample,
                           int64 loopEndSample);

    static bool captureMSUMetadata(const juce::File& msuFile,
                                   const juce::String& pcmFileName,
                                   MetadataSnapshot& snapshot);

    static bool captureBMLMetadata(const juce::File& bmlFile,
                                   const juce::String& pcmFileName,
                                   MetadataSnapshot& snapshot);

    bool restoreMSUMetadata(const juce::File& msuFile,
                            const juce::String& pcmFileName,
                            const MetadataSnapshot& snapshot);

    bool restoreBMLMetadata(const juce::File& bmlFile,
                            const juce::String& pcmFileName,
                            const MetadataSnapshot& snapshot);

    static bool removeLoopEntryFromMSU(juce::StringArray& lines,
                                       const juce::String& pcmFileName,
                                       const juce::String& trackNumber);

    static bool removeTrackEntryFromMSU(juce::StringArray& lines,
                                        const juce::String& pcmFileName,
                                        const juce::String& trackNumber);

    bool removeLoopEntryFromBML(juce::String& contents,
                                const juce::String& pcmFileName,
                                int blockStart,
                                int blockEnd);

    bool removeTrackEntryFromBML(juce::String& contents,
                                 const juce::String& pcmFileName);
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MSUManifestUpdater)
};
