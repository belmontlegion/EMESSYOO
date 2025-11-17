#include "AudioPlayer.h"

namespace
{
    constexpr double sampleRateToleranceHz = 0.1;
}

//==============================================================================
AudioPlayer::AudioPlayer()
{
}

AudioPlayer::~AudioPlayer()
{
    if (projectState != nullptr)
        projectState->removeChangeListener(this);
}

//==============================================================================
void AudioPlayer::play()
{
    const juce::ScopedLock sl(lock);
    playing = true;
}

void AudioPlayer::pause()
{
    const juce::ScopedLock sl(lock);
    playing = false;
}

void AudioPlayer::stop()
{
    const juce::ScopedLock sl(lock);
    playing = false;
    currentPosition = 0.0;
    currentSample = 0;
    fractionalSample = 0.0;
}

void AudioPlayer::setPosition(double seconds)
{
    const juce::ScopedLock sl(lock);
    
    if (projectState == nullptr)
        return;
    
    currentPosition = juce::jlimit(0.0, projectState->getLengthInSeconds(), seconds);
    currentSample = static_cast<int>(currentPosition * projectState->getSampleRate());
    fractionalSample = static_cast<double>(currentSample);
}

//==============================================================================
void AudioPlayer::setProjectState(MSUProjectState* state)
{
    const juce::ScopedLock sl(lock);
    
    if (projectState != nullptr)
        projectState->removeChangeListener(this);
    
    projectState = state;
    
    if (projectState != nullptr)
        projectState->addChangeListener(this);
    
    stop();
    updateResamplingState();
}

//==============================================================================
void AudioPlayer::audioDeviceIOCallbackWithContext(const float* const* inputChannelData,
                                                   int numInputChannels,
                                                   float* const* outputChannelData,
                                                   int numOutputChannels,
                                                   int numSamples,
                                                   const juce::AudioIODeviceCallbackContext& context)
{
    juce::ignoreUnused(inputChannelData, numInputChannels, context);

    auto clearOutput = [&](int startSample)
    {
        if (startSample >= numSamples)
            return;

        for (int ch = 0; ch < numOutputChannels; ++ch)
        {
            if (outputChannelData[ch] != nullptr)
                juce::FloatVectorOperations::clear(outputChannelData[ch] + startSample, numSamples - startSample);
        }
    };

    // Clear buffer before generating audio
    clearOutput(0);
    
    const juce::ScopedLock sl(lock);
    
    if (!playing || projectState == nullptr || !projectState->hasAudio())
        return;
    
    const auto& buffer = projectState->getAudioBuffer();
    const int totalSamples = buffer.getNumSamples();
    const double sourceSampleRate = projectState->getSampleRate();
    
    if (sourceSampleRate <= 0.0 || deviceSampleRate <= 0.0 || totalSamples <= 0)
        return;

    updateResamplingState();
    const bool shouldResample = resamplingActive;
    
    // Get trim and padding settings
    const int64 paddingSamples = projectState->getPaddingSamples();
    const int64 effectiveStart = projectState->getEffectivePlaybackStart();
    
    // Adjust loop points to account for trim and padding
    const bool hasLoop = looping && projectState->hasLoopPoints();
    int loopStart = hasLoop ? static_cast<int>(projectState->getLoopStart() - effectiveStart) : 0;
    int loopEnd = hasLoop ? static_cast<int>(projectState->getLoopEnd() - effectiveStart) : totalSamples;
    
    // Effective total length includes padding
    int effectiveLength = totalSamples - static_cast<int>(effectiveStart);
    if (paddingSamples > 0)
        effectiveLength += static_cast<int>(paddingSamples);
    
    const int channelCount = juce::jmin(numOutputChannels, buffer.getNumChannels());
    
    if (shouldResample)
    {
        double sourcePos = fractionalSample;
        
        for (int i = 0; i < numSamples; ++i)
        {
            int sourceIndex = static_cast<int>(sourcePos);

            if (sourceIndex >= effectiveLength || (hasLoop && sourceIndex >= loopEnd))
            {
                if (hasLoop)
                {
                    sourcePos = static_cast<double>(loopStart);
                    sourceIndex = loopStart;
                }
                else
                {
                    playing = false;
                    clearOutput(i);
                    fractionalSample = sourcePos;
                    currentSample = juce::jlimit(0, effectiveLength, sourceIndex);
                    currentPosition = static_cast<double>(currentSample) / sourceSampleRate;
                    return;
                }
            }
            
            // Check if we're in the padding region
            if (paddingSamples > 0 && sourceIndex < paddingSamples)
            {
                // Output silence for padding
                for (int channel = 0; channel < channelCount; ++channel)
                {
                    if (outputChannelData[channel] != nullptr)
                        outputChannelData[channel][i] = 0.0f;
                }
            }
            else
            {
                // Calculate actual buffer position (offset by effectiveStart and padding)
                int bufferPos = static_cast<int>(effectiveStart) + sourceIndex - static_cast<int>(paddingSamples);
                bufferPos = juce::jlimit(0, totalSamples - 1, bufferPos);
                const int nextPos = juce::jmin(bufferPos + 1, totalSamples - 1);
                const double fraction = sourcePos - static_cast<double>(sourceIndex);
                
                for (int channel = 0; channel < channelCount; ++channel)
                {
                    if (outputChannelData[channel] != nullptr)
                    {
                        const float sample1 = buffer.getSample(channel, bufferPos);
                        const float sample2 = buffer.getSample(channel, nextPos);
                        outputChannelData[channel][i] = static_cast<float>(sample1 + fraction * (sample2 - sample1));
                    }
                }
            }
            
            sourcePos += resamplingRatio;
        }
        
        fractionalSample = sourcePos;
        currentSample = juce::jlimit(0, effectiveLength, static_cast<int>(fractionalSample));
        currentPosition = fractionalSample / sourceSampleRate;
    }
    else
    {
        int samplesWritten = 0;
        
        while (samplesWritten < numSamples)
        {
            if (currentSample >= effectiveLength || (hasLoop && currentSample >= loopEnd))
            {
                if (hasLoop)
                {
                    currentSample = loopStart;
                }
                else
                {
                    playing = false;
                    clearOutput(samplesWritten);
                    break;
                }
            }

            int samplesToWrite = juce::jmin(numSamples - samplesWritten,
                                            hasLoop ? juce::jmax(0, loopEnd - currentSample)
                                                    : juce::jmax(0, effectiveLength - currentSample));

            if (samplesToWrite <= 0)
            {
                if (hasLoop)
                {
                    currentSample = loopStart;
                    continue;
                }

                playing = false;
                clearOutput(samplesWritten);
                break;
            }
            
            // Check if we're in the padding region
            if (paddingSamples > 0 && currentSample < paddingSamples)
            {
                int paddingSamplesToWrite = juce::jmin(samplesToWrite, static_cast<int>(paddingSamples) - currentSample);
                
                // Output silence for padding
                for (int channel = 0; channel < channelCount; ++channel)
                {
                    if (outputChannelData[channel] != nullptr)
                        juce::FloatVectorOperations::clear(outputChannelData[channel] + samplesWritten, paddingSamplesToWrite);
                }
                
                currentSample += paddingSamplesToWrite;
                samplesWritten += paddingSamplesToWrite;
            }
            else
            {
                // Calculate actual buffer position
                int bufferPos = static_cast<int>(effectiveStart) + currentSample - static_cast<int>(paddingSamples);
                bufferPos = juce::jlimit(0, totalSamples - 1, bufferPos);
                
                int bufferSamplesToWrite = juce::jmin(samplesToWrite, totalSamples - bufferPos);
                
                for (int channel = 0; channel < channelCount; ++channel)
                {
                    if (outputChannelData[channel] != nullptr)
                        juce::FloatVectorOperations::copy(outputChannelData[channel] + samplesWritten,
                                                         buffer.getReadPointer(channel, bufferPos),
                                                         bufferSamplesToWrite);
                }
                
                currentSample += bufferSamplesToWrite;
                samplesWritten += bufferSamplesToWrite;
            }
        }
        
        fractionalSample = static_cast<double>(currentSample);
        currentPosition = currentSample / sourceSampleRate;
    }
}

void AudioPlayer::updateResamplingState()
{
    if (projectState == nullptr || !projectState->hasAudio())
    {
        resamplingActive = false;
        resamplingRatio = 1.0;
        fractionalSample = static_cast<double>(currentSample);
        return;
    }

    const double sourceRate = projectState->getSampleRate();

    if (sourceRate <= 0.0 || deviceSampleRate <= 0.0)
    {
        resamplingActive = false;
        resamplingRatio = 1.0;
        return;
    }

    resamplingActive = std::abs(sourceRate - deviceSampleRate) > sampleRateToleranceHz;
    resamplingRatio = resamplingActive ? (sourceRate / deviceSampleRate) : 1.0;
    fractionalSample = static_cast<double>(currentSample);
}

void AudioPlayer::audioDeviceAboutToStart(juce::AudioIODevice* device)
{
    const juce::ScopedLock sl(lock);

    if (device == nullptr)
    {
        deviceSampleRate = 0.0;
        resamplingActive = false;
        return;
    }

    deviceSampleRate = device->getCurrentSampleRate();
    fractionalSample = static_cast<double>(currentSample);
    updateResamplingState();
    DBG("Audio device starting: " + device->getName() + " at " + juce::String(deviceSampleRate) + " Hz");
}

void AudioPlayer::audioDeviceStopped()
{
    const juce::ScopedLock sl(lock);
    deviceSampleRate = 0.0;
    resamplingActive = false;
    DBG("Audio device stopped");
}

//==============================================================================
void AudioPlayer::changeListenerCallback(juce::ChangeBroadcaster* source)
{
    if (source == projectState)
    {
        // Project state changed, validate playback position
        const juce::ScopedLock sl(lock);
        
        if (!projectState->hasAudio())
        {
            stop();
        }
        else
        {
            // Ensure position is still valid
            int totalSamples = projectState->getNumSamples();
            if (currentSample >= totalSamples)
                currentSample = 0;
            fractionalSample = static_cast<double>(currentSample);
        }

        updateResamplingState();
    }
}
