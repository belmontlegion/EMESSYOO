#pragma once

#include <JuceHeader.h>
#include "../Core/AudioFileHandler.h"

//==============================================================================
/**
 * Handles importing audio files with automatic conversion to MSU-1 requirements.
 * Converts to 44.1kHz, 16-bit, stereo PCM.
 */
class AudioImporter
{
public:
    //==============================================================================
    AudioImporter();
    ~AudioImporter();
    
    //==============================================================================
    /**
     * Import an audio file with automatic conversion to MSU-1 format.
     * @param file The source audio file
     * @param outputBuffer The buffer to store converted audio
     * @param removeDCOffset Whether to remove DC offset
     * @param normalizeOnImport Whether to normalize to -1dB on import
     * @return true if successful
     */
    bool importAudioFile(const juce::File& file,
                         juce::AudioBuffer<float>& outputBuffer,
                         bool removeDCOffset = true,
                         bool normalizeOnImport = false);
    
    /**
     * Convert audio buffer to MSU-1 requirements (44.1kHz, stereo).
     * @param buffer The buffer to convert (will be modified in place)
     * @param currentSampleRate The current sample rate of the buffer
     * @return true if successful
     */
    bool convertToMSU1Format(juce::AudioBuffer<float>& buffer, double currentSampleRate);
    
    /**
     * Resample audio buffer to target sample rate.
     * @param buffer The buffer to resample
     * @param currentSampleRate The current sample rate
     * @param targetSampleRate The desired sample rate
     * @return new buffer at target sample rate
     */
    juce::AudioBuffer<float> resampleBuffer(const juce::AudioBuffer<float>& buffer,
                                            double currentSampleRate,
                                            double targetSampleRate);
    
    /**
     * Convert mono to stereo by duplicating the channel.
     * @param buffer The buffer to convert (will be modified)
     */
    void convertMonoToStereo(juce::AudioBuffer<float>& buffer);
    
    /**
     * Remove DC offset from audio buffer.
     * @param buffer The buffer to process
     */
    void removeDCOffset(juce::AudioBuffer<float>& buffer);
    
    /**
     * Normalize buffer to target peak level.
     * @param buffer The buffer to normalize
     * @param targetDb The target peak level in dB
     */
    void normalizeToPeak(juce::AudioBuffer<float>& buffer, float targetDb = -1.0f);
    
    /**
     * Get the last error message.
     */
    juce::String getLastError() const { return lastError; }
    
    //==============================================================================
    // MSU-1 format constants
    static constexpr double MSU1_SAMPLE_RATE = 44100.0;
    static constexpr int MSU1_NUM_CHANNELS = 2;
    static constexpr int MSU1_BIT_DEPTH = 16;
    
private:
    //==============================================================================
    AudioFileHandler audioFileHandler;
    juce::String lastError;
    
    void setError(const juce::String& error);
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioImporter)
};
