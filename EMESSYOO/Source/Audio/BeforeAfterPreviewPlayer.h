#pragma once

#include <JuceHeader.h>

/**
 * Lightweight audio callback that can preview two in-memory buffers ("Before" and "After")
 * using the shared AudioDeviceManager. Designed for Audio Level Studio A/B comparisons.
 */
class BeforeAfterPreviewPlayer : public juce::AudioIODeviceCallback
{
public:
    enum class Target
    {
        Before,
        After
    };

    BeforeAfterPreviewPlayer();
    ~BeforeAfterPreviewPlayer() override = default;

    void setSourceBuffers(const juce::AudioBuffer<float>* beforeBuffer,
                          const juce::AudioBuffer<float>* afterBuffer,
                          double bufferSampleRate);

    void play(Target target, bool restartPlayback);
    void stop();

    bool isPlaying() const;
    bool hasContent(Target target) const;
    Target getActiveTarget() const { return activeTarget; }
    bool getPlaybackProgress(double& currentSeconds, double& totalSeconds) const;

    void audioDeviceIOCallbackWithContext(const float* const* inputChannelData,
                                          int numInputChannels,
                                          float* const* outputChannelData,
                                          int numOutputChannels,
                                          int numSamples,
                                          const juce::AudioIODeviceCallbackContext& context) override;

    void audioDeviceAboutToStart(juce::AudioIODevice* device) override;
    void audioDeviceStopped() override;

private:
    const juce::AudioBuffer<float>* beforeBuffer = nullptr;
    const juce::AudioBuffer<float>* afterBuffer = nullptr;
    double sourceSampleRate = 44100.0;
    double deviceSampleRate = 44100.0;
    double playbackIncrement = 1.0;

    mutable juce::CriticalSection lock;
    bool playing = false;
    double currentSample = 0.0;
    Target activeTarget = Target::Before;

    const juce::AudioBuffer<float>* getBufferFor(Target target) const;
    bool bufferHasContent(const juce::AudioBuffer<float>* buffer) const;
    void updatePlaybackIncrement();
    void writeSilence(float* const* outputChannelData, int numOutputChannels, int numSamples) const;

    static constexpr double fallbackSampleRate = 44100.0;
};
