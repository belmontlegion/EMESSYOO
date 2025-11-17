#include "BeforeAfterPreviewPlayer.h"

BeforeAfterPreviewPlayer::BeforeAfterPreviewPlayer()
{
    updatePlaybackIncrement();
}

void BeforeAfterPreviewPlayer::setSourceBuffers(const juce::AudioBuffer<float>* before,
                                                const juce::AudioBuffer<float>* after,
                                                double bufferSampleRate)
{
    const juce::ScopedLock sl(lock);
    beforeBuffer = before;
    afterBuffer = after;
    sourceSampleRate = bufferSampleRate > 0.0 ? bufferSampleRate : fallbackSampleRate;
    currentSample = 0.0;
    playing = false;
    updatePlaybackIncrement();
}

void BeforeAfterPreviewPlayer::play(Target target, bool restartPlayback)
{
    const juce::ScopedLock sl(lock);
    const auto* buffer = getBufferFor(target);
    if (!bufferHasContent(buffer))
        return;

    if (playing && !restartPlayback)
    {
        activeTarget = target;
        currentSample = juce::jlimit(0.0, static_cast<double>(buffer->getNumSamples()), currentSample);
        return;
    }

    activeTarget = target;
    currentSample = 0.0;
    playing = true;
}

void BeforeAfterPreviewPlayer::stop()
{
    const juce::ScopedLock sl(lock);
    playing = false;
    currentSample = 0.0;
}

bool BeforeAfterPreviewPlayer::isPlaying() const
{
    const juce::ScopedLock sl(lock);
    return playing;
}

bool BeforeAfterPreviewPlayer::hasContent(Target target) const
{
    const juce::ScopedLock sl(lock);
    return bufferHasContent(getBufferFor(target));
}

bool BeforeAfterPreviewPlayer::getPlaybackProgress(double& currentSeconds, double& totalSeconds) const
{
    const juce::ScopedLock sl(lock);
    const auto* buffer = getBufferFor(activeTarget);

    if (!playing || !bufferHasContent(buffer) || sourceSampleRate <= 0.0)
    {
        currentSeconds = 0.0;
        totalSeconds = 0.0;
        return false;
    }

    totalSeconds = static_cast<double>(buffer->getNumSamples()) / sourceSampleRate;
    currentSeconds = currentSample / sourceSampleRate;
    return totalSeconds > 0.0;
}

void BeforeAfterPreviewPlayer::audioDeviceIOCallbackWithContext(const float* const* inputChannelData,
                                                                int numInputChannels,
                                                                float* const* outputChannelData,
                                                                int numOutputChannels,
                                                                int numSamples,
                                                                const juce::AudioIODeviceCallbackContext& context)
{
    juce::ignoreUnused(inputChannelData, numInputChannels, context);

    const juce::ScopedLock sl(lock);
    auto* buffer = getBufferFor(activeTarget);
    if (!playing || !bufferHasContent(buffer))
    {
        playing = false;
        writeSilence(outputChannelData, numOutputChannels, numSamples);
        return;
    }

    auto totalSamples = buffer->getNumSamples();
    auto channels = juce::jmax(1, buffer->getNumChannels());
    double position = currentSample;

    for (int sample = 0; sample < numSamples; ++sample)
    {
        auto index = static_cast<int>(position);
        if (index >= totalSamples)
        {
            playing = false;
            writeSilence(outputChannelData, numOutputChannels, numSamples - sample);
            break;
        }

        auto nextIndex = juce::jmin(index + 1, totalSamples - 1);
        auto fraction = position - static_cast<double>(index);

        for (int ch = 0; ch < numOutputChannels; ++ch)
        {
            if (outputChannelData[ch] == nullptr)
                continue;

            auto sourceChannel = ch % channels;
            auto sampleA = buffer->getSample(sourceChannel, index);
            auto sampleB = buffer->getSample(sourceChannel, nextIndex);
            outputChannelData[ch][sample] = sampleA + static_cast<float>(fraction) * (sampleB - sampleA);
        }

        position += playbackIncrement;
    }

    currentSample = position;
    if (!playing)
        currentSample = 0.0;
}

void BeforeAfterPreviewPlayer::audioDeviceAboutToStart(juce::AudioIODevice* device)
{
    const juce::ScopedLock sl(lock);
    deviceSampleRate = (device != nullptr && device->getCurrentSampleRate() > 0.0)
        ? device->getCurrentSampleRate()
        : fallbackSampleRate;
    updatePlaybackIncrement();
}

void BeforeAfterPreviewPlayer::audioDeviceStopped()
{
    const juce::ScopedLock sl(lock);
    deviceSampleRate = fallbackSampleRate;
    updatePlaybackIncrement();
}

const juce::AudioBuffer<float>* BeforeAfterPreviewPlayer::getBufferFor(Target target) const
{
    return target == Target::Before ? beforeBuffer : afterBuffer;
}

bool BeforeAfterPreviewPlayer::bufferHasContent(const juce::AudioBuffer<float>* buffer) const
{
    return buffer != nullptr && buffer->getNumSamples() > 0 && buffer->getNumChannels() > 0;
}

void BeforeAfterPreviewPlayer::updatePlaybackIncrement()
{
    if (deviceSampleRate <= 0.0)
        deviceSampleRate = fallbackSampleRate;

    playbackIncrement = sourceSampleRate <= 0.0
        ? 1.0
        : (sourceSampleRate / deviceSampleRate);

    if (playbackIncrement <= 0.0)
        playbackIncrement = 1.0;
}

void BeforeAfterPreviewPlayer::writeSilence(float* const* outputChannelData,
                                            int numOutputChannels,
                                            int numSamples) const
{
    for (int ch = 0; ch < numOutputChannels; ++ch)
    {
        if (outputChannelData[ch] != nullptr)
            juce::FloatVectorOperations::clear(outputChannelData[ch], numSamples);
    }
}
