#include "AudioPlayer.h"

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
}

void AudioPlayer::setPosition(double seconds)
{
    const juce::ScopedLock sl(lock);
    
    if (projectState == nullptr)
        return;
    
    currentPosition = juce::jlimit(0.0, projectState->getLengthInSeconds(), seconds);
    currentSample = static_cast<int>(currentPosition * projectState->getSampleRate());
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
}

//==============================================================================
void AudioPlayer::audioDeviceIOCallbackWithContext(const float* const* inputChannelData,
                                                   int numInputChannels,
                                                   float* const* outputChannelData,
                                                   int numOutputChannels,
                                                   int numSamples,
                                                   const juce::AudioIODeviceCallbackContext& context)
{
    // Clear output buffers
    for (int i = 0; i < numOutputChannels; ++i)
    {
        if (outputChannelData[i] != nullptr)
            juce::FloatVectorOperations::clear(outputChannelData[i], numSamples);
    }
    
    const juce::ScopedLock sl(lock);
    
    // Return if not playing or no project state
    if (!playing || projectState == nullptr || !projectState->hasAudio())
        return;
    
    const auto& buffer = projectState->getAudioBuffer();
    const int totalSamples = buffer.getNumSamples();
    
    // Get loop points
    int loopStart = looping && projectState->hasLoopPoints() 
                    ? static_cast<int>(projectState->getLoopStart()) 
                    : 0;
    int loopEnd = looping && projectState->hasLoopPoints() 
                  ? static_cast<int>(projectState->getLoopEnd()) 
                  : totalSamples;
    
    int samplesWritten = 0;
    
    while (samplesWritten < numSamples)
    {
        // Check if we've reached the end
        if (currentSample >= totalSamples || (looping && currentSample >= loopEnd))
        {
            if (looping && projectState->hasLoopPoints())
            {
                // Loop back to loop start
                currentSample = loopStart;
            }
            else
            {
                // Stop playback
                playing = false;
                break;
            }
        }
        
        // Calculate how many samples we can write
        int samplesToWrite = juce::jmin(numSamples - samplesWritten,
                                       looping ? loopEnd - currentSample : totalSamples - currentSample);
        
        // Copy audio data to output
        for (int channel = 0; channel < juce::jmin(numOutputChannels, buffer.getNumChannels()); ++channel)
        {
            if (outputChannelData[channel] != nullptr)
            {
                juce::FloatVectorOperations::copy(outputChannelData[channel] + samplesWritten,
                                                 buffer.getReadPointer(channel, currentSample),
                                                 samplesToWrite);
            }
        }
        
        currentSample += samplesToWrite;
        samplesWritten += samplesToWrite;
    }
    
    // Update position
    if (projectState->getSampleRate() > 0.0)
        currentPosition = currentSample / projectState->getSampleRate();
}

void AudioPlayer::audioDeviceAboutToStart(juce::AudioIODevice* device)
{
    DBG("Audio device starting: " + device->getName());
}

void AudioPlayer::audioDeviceStopped()
{
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
        }
    }
}
