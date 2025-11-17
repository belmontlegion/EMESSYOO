#pragma once

#include <JuceHeader.h>

//==============================================================================
/**
 * Analyzes audio for normalization purposes.
 * Calculates RMS, peak levels, and applies gain adjustments.
 */
class NormalizationAnalyzer
{
public:
    //==============================================================================
    struct AudioStats
    {
        float peakDb = -96.0f;
        float rmsDb = -96.0f;
        float peakLinear = 0.0f;
        float rmsLinear = 0.0f;
    };
    
    //==============================================================================
    NormalizationAnalyzer();
    ~NormalizationAnalyzer();
    
    //==============================================================================
    /**
     * Analyze an audio buffer and return statistics.
     * @param buffer The buffer to analyze
     * @return Statistics structure
     */
    static AudioStats analyzeBuffer(const juce::AudioBuffer<float>& buffer);
    
    /**
     * Analyze all PCM files in a directory.
     * @param directory The directory to scan
     * @param stats Output map of file->statistics
     * @return true if successful
     */
    bool analyzeDirectory(const juce::File& directory,
                          std::map<juce::File, AudioStats>& stats);
    
    /**
     * Calculate the average RMS across multiple files.
     * @param stats Map of file statistics
     * @return Average RMS in dB
     */
    static float calculateAverageRMS(const std::map<juce::File, AudioStats>& stats);
    
    /**
     * Calculate gain needed to match a target RMS level.
     * @param currentRmsDb Current RMS in dB
     * @param targetRmsDb Target RMS in dB
     * @return Gain in dB needed
     */
    static float calculateGainToTarget(float currentRmsDb, float targetRmsDb);
    
    /**
     * Apply gain to a buffer.
     * @param buffer The buffer to modify
     * @param gainDb Gain in decibels
     */
    static void applyGain(juce::AudioBuffer<float>& buffer, float gainDb);
    
    /**
     * Normalize buffer to target RMS level.
     * @param buffer The buffer to normalize
     * @param targetRmsDb Target RMS in dB
     * @param limitPeak If true, ensure peak doesn't exceed -1dB
     */
    static void normalizeToRMS(juce::AudioBuffer<float>& buffer,
                              float targetRmsDb,
                              bool limitPeak = true);
    
    /**
     * Get the last error message.
     */
    juce::String getLastError() const { return lastError; }
    
private:
    //==============================================================================
    juce::String lastError;
    
    void setError(const juce::String& error);
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NormalizationAnalyzer)
};
