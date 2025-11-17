#pragma once

#include <JuceHeader.h>
#include "../Core/MSUProjectState.h"

//==============================================================================
/**
 * Handles audio playback with looping support.
 * Manages the audio transport and playback state.
 */
class AudioPlayer : public juce::AudioIODeviceCallback,
                    public juce::ChangeListener
{
public:
    //==============================================================================
    AudioPlayer();
    ~AudioPlayer() override;
    
    //==============================================================================
    // Playback control
    void play();
    void pause();
    void stop();
    bool isPlaying() const { return playing; }
    
    void setLooping(bool shouldLoop) { looping = shouldLoop; }
    bool isLooping() const { return looping; }
    
    //==============================================================================
    // Position control
    void setPosition(double seconds);
    double getPosition() const { return currentPosition; }
    
    //==============================================================================
    // Project state
    void setProjectState(MSUProjectState* state);
    
    //==============================================================================
    // AudioIODeviceCallback implementation
    void audioDeviceIOCallbackWithContext(const float* const* inputChannelData,
                                         int numInputChannels,
                                         float* const* outputChannelData,
                                         int numOutputChannels,
                                         int numSamples,
                                         const juce::AudioIODeviceCallbackContext& context) override;
    
    void audioDeviceAboutToStart(juce::AudioIODevice* device) override;
    void audioDeviceStopped() override;
    
    //==============================================================================
    // ChangeListener implementation
    void changeListenerCallback(juce::ChangeBroadcaster* source) override;
    
private:
    //==============================================================================
    MSUProjectState* projectState = nullptr;
    
    bool playing = false;
    bool looping = false;
    double currentPosition = 0.0;
    int currentSample = 0;
    double fractionalSample = 0.0;
    
    double deviceSampleRate = 0.0;
    double resamplingRatio = 1.0;
    bool resamplingActive = false;
    
    juce::CriticalSection lock;

    void updateResamplingState();
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioPlayer)
};
