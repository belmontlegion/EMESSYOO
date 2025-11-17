#pragma once

#include <JuceHeader.h>

//==============================================================================
/**
 * Lightweight audio player for previewing tracks without loading waveforms.
 * Designed for quick MSU-1 track audition in the file browser.
 */
class PreviewPlayer : public juce::AudioIODeviceCallback
{
public:
    //==============================================================================
    PreviewPlayer();
    ~PreviewPlayer() override;
    
    //==============================================================================
    /** Load and play an audio file */
    bool loadAndPlay(const juce::File& file);
    
    /** Stop playback and unload */
    void stop();
    
    /** Check if currently playing */
    bool isPlaying() const { return playing; }
    
    /** Get current playback position in seconds */
    double getPosition() const;
    
    /** Get total length in seconds */
    double getTotalLength() const;
    
    //==============================================================================
    // AudioIODeviceCallback overrides
    void audioDeviceIOCallbackWithContext(const float* const* inputChannelData,
                                         int numInputChannels,
                                         float* const* outputChannelData,
                                         int numOutputChannels,
                                         int numSamples,
                                         const juce::AudioIODeviceCallbackContext& context) override;
    
    void audioDeviceAboutToStart(juce::AudioIODevice* device) override;
    void audioDeviceStopped() override;

    void setAudioDeviceManager(juce::AudioDeviceManager* manager);
    
private:
    //==============================================================================
    juce::AudioFormatManager formatManager;
    std::unique_ptr<juce::AudioFormatReaderSource> readerSource;
    std::unique_ptr<juce::ResamplingAudioSource> resamplingSource;
    std::unique_ptr<juce::AudioFormatReader> currentReader;
    
    // PCM-specific streaming with pre-buffering
    std::unique_ptr<juce::FileInputStream> pcmInputStream;
    juce::AudioBuffer<float> pcmBuffer;  // Pre-loaded PCM data
    int64 pcmPosition = 0;
    double pcmFractionalPosition = 0.0;
    int64 pcmLoopPoint = 0;
    int64 pcmTotalSamples = 0;
    bool isPCMFile = false;
    static constexpr double pcmNativeSampleRate = 44100.0;
    
    bool playing = false;
    double deviceSampleRate = 0.0;
    juce::AudioDeviceManager* audioDeviceManager = nullptr;
    juce::CriticalSection callbackLock;
    
    void loadPCMIntoBuffer(const juce::File& file);
    void refreshDeviceSampleRate();
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PreviewPlayer)
};
