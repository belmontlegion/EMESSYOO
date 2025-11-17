#pragma once

#include <JuceHeader.h>

//==============================================================================
/**
 * Handles reading and writing audio files with format conversion.
 * Provides utilities for working with various audio formats.
 */
class AudioFileHandler
{
public:
    //==============================================================================
    AudioFileHandler();
    ~AudioFileHandler();
    
    //==============================================================================
    /**
     * Load an audio file and return its contents as an AudioBuffer.
     * @param file The audio file to load
     * @param buffer The buffer to store the audio data
     * @param sampleRate Output parameter for the file's sample rate
     * @param loopPoint Optional output parameter for loop point (MSU-1 PCM files only)
     * @return true if successful
     */
    bool loadAudioFile(const juce::File& file, 
                       juce::AudioBuffer<float>& buffer,
                       double& sampleRate,
                       int64* loopPoint = nullptr);
    
    /**
     * Save an audio buffer to a file in the specified format.
     * @param file The destination file
     * @param buffer The audio buffer to save
     * @param sampleRate The sample rate of the audio
     * @param bitDepth The bit depth (16, 24, or 32)
     * @return true if successful
     */
    bool saveAudioFile(const juce::File& file,
                       const juce::AudioBuffer<float>& buffer,
                       double sampleRate,
                       int bitDepth = 16);
    
    /**
     * Get information about an audio file without loading it.
     * @param file The audio file to query
     * @param sampleRate Output parameter for sample rate
     * @param numChannels Output parameter for channel count
     * @param lengthInSamples Output parameter for length
     * @return true if successful
     */
    bool getAudioFileInfo(const juce::File& file,
                          double& sampleRate,
                          int& numChannels,
                          int64& lengthInSamples);
    
    /**
     * Get the last error message.
     */
    juce::String getLastError() const { return lastError; }
    
private:
    //==============================================================================
    juce::AudioFormatManager formatManager;
    juce::String lastError;
    
    void setError(const juce::String& error);
    
    /**
     * Load an MSU-1 PCM file (raw 16-bit stereo PCM with MSU1 header).
     */
    bool loadMSU1PCMFile(const juce::File& file,
                          juce::AudioBuffer<float>& buffer,
                          double& sampleRate,
                          int64* loopPoint);    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioFileHandler)
};
