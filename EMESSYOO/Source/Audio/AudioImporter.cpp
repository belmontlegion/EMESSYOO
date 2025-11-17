#include "AudioImporter.h"

//==============================================================================
AudioImporter::AudioImporter()
{
}

AudioImporter::~AudioImporter()
{
}

//==============================================================================
bool AudioImporter::importAudioFile(const juce::File& file,
                                    juce::AudioBuffer<float>& outputBuffer,
                                    bool removeDCOffsetFlag,
                                    bool normalizeOnImport)
{
    // Load the audio file
    double sampleRate;
    if (!audioFileHandler.loadAudioFile(file, outputBuffer, sampleRate))
    {
        lastError = audioFileHandler.getLastError();
        return false;
    }
    
    // Convert to MSU-1 format
    if (!convertToMSU1Format(outputBuffer, sampleRate))
    {
        return false;
    }
    
    // Remove DC offset if requested
    if (removeDCOffsetFlag)
    {
        removeDCOffset(outputBuffer);
    }
    
    // Normalize on import if requested
    if (normalizeOnImport)
    {
        normalizeToPeak(outputBuffer, -1.0f);
    }
    
    lastError.clear();
    return true;
}

bool AudioImporter::convertToMSU1Format(juce::AudioBuffer<float>& buffer, double currentSampleRate)
{
    // Resample to 44.1kHz if needed
    if (std::abs(currentSampleRate - MSU1_SAMPLE_RATE) > 0.1)
    {
        buffer = resampleBuffer(buffer, currentSampleRate, MSU1_SAMPLE_RATE);
    }
    
    // Convert to stereo if mono
    if (buffer.getNumChannels() == 1)
    {
        convertMonoToStereo(buffer);
    }
    else if (buffer.getNumChannels() > 2)
    {
        // Mix down to stereo by taking first two channels
        juce::AudioBuffer<float> stereoBuffer(2, buffer.getNumSamples());
        stereoBuffer.copyFrom(0, 0, buffer, 0, 0, buffer.getNumSamples());
        stereoBuffer.copyFrom(1, 0, buffer, 1, 0, buffer.getNumSamples());
        buffer = stereoBuffer;
    }
    
    return true;
}

juce::AudioBuffer<float> AudioImporter::resampleBuffer(const juce::AudioBuffer<float>& buffer,
                                                       double currentSampleRate,
                                                       double targetSampleRate)
{
    if (std::abs(currentSampleRate - targetSampleRate) < 0.1)
        return buffer;
    
    auto ratio = targetSampleRate / currentSampleRate;
    auto newLength = static_cast<int>(std::ceil(buffer.getNumSamples() * ratio));
    
    juce::AudioBuffer<float> resampledBuffer(buffer.getNumChannels(), newLength);
    
    // Use Lagrange interpolator for high-quality resampling
    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
    {
        juce::LagrangeInterpolator interpolator;
        interpolator.process(ratio,
                           buffer.getReadPointer(channel),
                           resampledBuffer.getWritePointer(channel),
                           newLength);
    }
    
    return resampledBuffer;
}

void AudioImporter::convertMonoToStereo(juce::AudioBuffer<float>& buffer)
{
    if (buffer.getNumChannels() != 1)
        return;
    
    juce::AudioBuffer<float> stereoBuffer(2, buffer.getNumSamples());
    
    // Duplicate mono channel to both left and right
    stereoBuffer.copyFrom(0, 0, buffer, 0, 0, buffer.getNumSamples());
    stereoBuffer.copyFrom(1, 0, buffer, 0, 0, buffer.getNumSamples());
    
    buffer = stereoBuffer;
}

void AudioImporter::removeDCOffset(juce::AudioBuffer<float>& buffer)
{
    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
    {
        // Calculate DC offset (mean value)
        float sum = 0.0f;
        auto* data = buffer.getReadPointer(channel);
        
        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
            sum += data[sample];
        
        float dcOffset = sum / buffer.getNumSamples();
        
        // Remove DC offset
        buffer.applyGain(channel, 0, buffer.getNumSamples(), 1.0f);
        buffer.addFrom(channel, 0, buffer, channel, 0, buffer.getNumSamples(), -dcOffset);
    }
}

void AudioImporter::normalizeToPeak(juce::AudioBuffer<float>& buffer, float targetDb)
{
    // Find peak level
    float maxLevel = 0.0f;
    
    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
    {
        auto level = buffer.getMagnitude(channel, 0, buffer.getNumSamples());
        maxLevel = juce::jmax(maxLevel, level);
    }
    
    if (maxLevel > 0.0f)
    {
        // Calculate gain needed to reach target
        float targetLinear = juce::Decibels::decibelsToGain(targetDb);
        float gain = targetLinear / maxLevel;
        
        // Apply gain
        buffer.applyGain(gain);
    }
}

//==============================================================================
void AudioImporter::setError(const juce::String& error)
{
    lastError = error;
    DBG("AudioImporter Error: " + error);
}
