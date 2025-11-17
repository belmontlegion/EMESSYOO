#include "NormalizationAnalyzer.h"

//==============================================================================
NormalizationAnalyzer::NormalizationAnalyzer()
{
}

NormalizationAnalyzer::~NormalizationAnalyzer()
{
}

//==============================================================================
NormalizationAnalyzer::AudioStats NormalizationAnalyzer::analyzeBuffer(const juce::AudioBuffer<float>& buffer)
{
    AudioStats stats;
    
    if (buffer.getNumSamples() == 0)
        return stats;
    
    // Calculate peak
    float maxPeak = 0.0f;
    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
    {
        float channelPeak = buffer.getMagnitude(channel, 0, buffer.getNumSamples());
        maxPeak = juce::jmax(maxPeak, channelPeak);
    }
    
    stats.peakLinear = maxPeak;
    stats.peakDb = juce::Decibels::gainToDecibels(maxPeak, -96.0f);
    
    // Calculate RMS
    float sumSquares = 0.0f;
    int totalSamples = 0;
    
    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
    {
        const float* data = buffer.getReadPointer(channel);
        
        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
        {
            sumSquares += data[sample] * data[sample];
            totalSamples++;
        }
    }
    
    float rms = std::sqrt(sumSquares / totalSamples);
    stats.rmsLinear = rms;
    stats.rmsDb = juce::Decibels::gainToDecibels(rms, -96.0f);
    
    return stats;
}

bool NormalizationAnalyzer::analyzeDirectory(const juce::File& directory,
                                             std::map<juce::File, AudioStats>& stats)
{
    if (!directory.isDirectory())
    {
        setError("Not a valid directory: " + directory.getFullPathName());
        return false;
    }
    
    stats.clear();
    
    // Find all PCM files
    juce::Array<juce::File> pcmFiles = directory.findChildFiles(juce::File::findFiles, 
                                                                false, 
                                                                "*.pcm");
    
    if (pcmFiles.isEmpty())
    {
        setError("No PCM files found in directory");
        return false;
    }
    
    juce::AudioFormatManager formatManager;
    formatManager.registerBasicFormats();
    
    for (const auto& file : pcmFiles)
    {
        // Try to read as WAV (PCM files might be readable)
        std::unique_ptr<juce::AudioFormatReader> reader(formatManager.createReaderFor(file));
        
        if (reader != nullptr)
        {
            // Read audio data
            juce::AudioBuffer<float> buffer(static_cast<int>(reader->numChannels),
                                           static_cast<int>(reader->lengthInSamples));
            
            if (reader->read(&buffer, 0, static_cast<int>(reader->lengthInSamples), 0, true, true))
            {
                stats[file] = analyzeBuffer(buffer);
            }
        }
    }
    
    if (stats.empty())
    {
        setError("Could not analyze any PCM files");
        return false;
    }
    
    lastError.clear();
    return true;
}

float NormalizationAnalyzer::calculateAverageRMS(const std::map<juce::File, AudioStats>& stats)
{
    if (stats.empty())
        return -96.0f;
    
    float sumRms = 0.0f;
    
    for (const auto& pair : stats)
    {
        // Convert dB to linear, accumulate, then convert back
        sumRms += pair.second.rmsLinear;
    }
    
    float avgRmsLinear = sumRms / stats.size();
    return juce::Decibels::gainToDecibels(avgRmsLinear, -96.0f);
}

float NormalizationAnalyzer::calculateGainToTarget(float currentRmsDb, float targetRmsDb)
{
    return targetRmsDb - currentRmsDb;
}

void NormalizationAnalyzer::applyGain(juce::AudioBuffer<float>& buffer, float gainDb)
{
    float gainLinear = juce::Decibels::decibelsToGain(gainDb);
    buffer.applyGain(gainLinear);
}

void NormalizationAnalyzer::normalizeToRMS(juce::AudioBuffer<float>& buffer,
                                          float targetRmsDb,
                                          bool limitPeak)
{
    // Analyze current buffer
    AudioStats stats = analyzeBuffer(buffer);
    
    // Calculate required gain
    float gainDb = calculateGainToTarget(stats.rmsDb, targetRmsDb);
    
    // If limiting peak, check if resulting peak would exceed -1dB
    if (limitPeak)
    {
        float newPeakDb = stats.peakDb + gainDb;
        if (newPeakDb > -1.0f)
        {
            // Reduce gain to keep peak at -1dB
            gainDb = -1.0f - stats.peakDb;
        }
    }
    
    // Apply gain
    applyGain(buffer, gainDb);
}

//==============================================================================
void NormalizationAnalyzer::setError(const juce::String& error)
{
    lastError = error;
    DBG("NormalizationAnalyzer Error: " + error);
}
