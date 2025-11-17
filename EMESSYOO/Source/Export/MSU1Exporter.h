#pragma once

#include <JuceHeader.h>

//==============================================================================
/**
 * Exports audio buffers to MSU-1 compatible PCM format.
 * Handles MSU-1 header, loop points, and proper byte ordering.
 */
class MSU1Exporter
{
public:
    //==============================================================================
    MSU1Exporter();
    ~MSU1Exporter();
    
    //==============================================================================
    /**
     * Export audio buffer as MSU-1 PCM file.
     * @param file Output file path
     * @param buffer Audio buffer (must be 44.1kHz stereo)
     * @param loopStartSample Loop start position in samples (-1 for no loop)
     * @param createBackup Create .bak file if file exists
     * @return true if successful
     */
    bool exportPCM(const juce::File& file,
                   const juce::AudioBuffer<float>& buffer,
                   int64 loopStartSample = -1,
                   bool createBackup = true);
    
    /**
     * Validate buffer meets MSU-1 requirements.
     * @param buffer Buffer to validate
     * @param sampleRate Sample rate of the buffer
     * @param errorMessage Output error message if validation fails
     * @return true if valid
     */
    bool validateBuffer(const juce::AudioBuffer<float>& buffer,
                       double sampleRate,
                       juce::String& errorMessage);
    
    /**
     * Convert float samples to 16-bit signed integer PCM.
     * @param floatData Input float samples
     * @param int16Data Output int16 buffer
     * @param numSamples Number of samples to convert
     */
    static void convertFloatToInt16(const float* floatData,
                                   int16_t* int16Data,
                                   int numSamples);
    
    /**
     * Get the last error message.
     */
    juce::String getLastError() const { return lastError; }
    
    //==============================================================================
    // MSU-1 format constants
    static constexpr uint32_t MSU1_MAGIC = 0x3153554D; // "MSU1" in little-endian
    static constexpr int MSU1_HEADER_SIZE = 8;
    static constexpr double MSU1_SAMPLE_RATE = 44100.0;
    static constexpr int MSU1_NUM_CHANNELS = 2;
    static constexpr int MSU1_BIT_DEPTH = 16;
    
private:
    //==============================================================================
    juce::String lastError;
    
    void setError(const juce::String& error);
    bool moveOriginalToBackupFolder(const juce::File& file);
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MSU1Exporter)
};
