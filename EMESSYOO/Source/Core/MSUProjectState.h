#pragma once

#include <JuceHeader.h>

//==============================================================================
/**
 * Manages the current state of an MSU-1 preparation project.
 * Stores audio data, loop points, normalization settings, and project metadata.
 */
class MSUProjectState : public juce::ChangeBroadcaster
{
public:
    //==============================================================================
    MSUProjectState();
    ~MSUProjectState() override;
    
    //==============================================================================
    // Audio data management
    void setAudioBuffer(const juce::AudioBuffer<float>& newBuffer, double sampleRate);
    const juce::AudioBuffer<float>& getAudioBuffer() const { return audioBuffer; }
    juce::AudioBuffer<float>& getAudioBuffer() { return audioBuffer; }
    double getSampleRate() const { return projectSampleRate; }
    int getNumChannels() const { return audioBuffer.getNumChannels(); }
    int getNumSamples() const { return audioBuffer.getNumSamples(); }
    double getLengthInSeconds() const;
    
    //==============================================================================
    // Loop point management
    void setLoopStart(int64 samplePosition);
    void setLoopEnd(int64 samplePosition);
    int64 getLoopStart() const { return loopStartSample; }
    int64 getLoopEnd() const { return loopEndSample; }
    bool hasLoopPoints() const { return loopStartSample >= 0 && loopEndSample > loopStartSample; }
    
    // Trim start management (for removing silence at beginning)
    void setTrimStart(int64 samplePosition);
    int64 getTrimStart() const { return trimStartSample; }
    bool hasTrimStart() const { return hasAudio(); }
    
    // Get effective playback start (accounting for padding)
    int64 getEffectivePlaybackStart() const
    {
        if (paddingSamples > 0)
            return juce::jmax<int64>(0, trimStartSample - paddingSamples);
        return trimStartSample;
    }
    
    // Padding management (for adding silence at beginning)
    void setPaddingSamples(int64 samples) { paddingSamples = samples; sendChangeMessage(); }
    int64 getPaddingSamples() const { return paddingSamples; }
    bool hasPadding() const { return paddingSamples > 0; }
    
    void setPadAmountMs(int milliseconds);
    int getPadAmountMs() const { return padAmountMs; }
    
    //==============================================================================
    // File information
    void setSourceFile(const juce::File& file);
    juce::File getSourceFile() const { return sourceFile; }
    juce::String getSourceFileName() const { return sourceFile.getFileName(); }
    
    void setTargetExportFile(const juce::File& file) { targetExportFile = file; }
    juce::File getTargetExportFile() const { return targetExportFile; }
    bool hasTargetExportFile() const { return targetExportFile != juce::File(); }
    
    //==============================================================================
    // Project state
    bool hasAudio() const { return audioBuffer.getNumSamples() > 0; }
    bool isModified() const { return modified; }
    void setModified(bool isModified);
    
    //==============================================================================
    // Normalization settings
    void setTargetRMS(float rmsDb) { targetRMSDb = rmsDb; }
    float getTargetRMS() const { return targetRMSDb; }
    void setNormalizationGain(float gainDb) { normalizationGainDb = gainDb; }
    float getNormalizationGain() const { return normalizationGainDb; }
    
    //==============================================================================
    // Reset project
    void reset();
    
private:
    //==============================================================================
    juce::AudioBuffer<float> audioBuffer;
    double projectSampleRate = 44100.0;
    
    int64 loopStartSample = -1;
    int64 loopEndSample = -1;
    int64 trimStartSample = 0;  // Sample position where track actually starts
    int64 paddingSamples = 0;   // Number of silence samples to add at beginning
    int padAmountMs = 200;      // Desired pad length in milliseconds
    
    juce::File sourceFile;
    juce::File targetExportFile;  // Track file to replace when exporting
    bool modified = false;
    
    float targetRMSDb = -12.0f;
    float normalizationGainDb = 0.0f;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MSUProjectState)
};
