#include "MSU1Exporter.h"

//==============================================================================
MSU1Exporter::MSU1Exporter()
{
}

MSU1Exporter::~MSU1Exporter()
{
}

//==============================================================================
bool MSU1Exporter::exportPCM(const juce::File& file,
                             const juce::AudioBuffer<float>& buffer,
                             int64 loopStartSample,
                             bool createBackup)
{
    // Validate buffer
    juce::String errorMessage;
    if (!validateBuffer(buffer, MSU1_SAMPLE_RATE, errorMessage))
    {
        setError(errorMessage);
        return false;
    }
    
    // Move existing file into Backup folder if requested
    if (createBackup && file.existsAsFile())
    {
        if (!moveOriginalToBackupFolder(file))
            return false;
    }
    
    // Delete existing file
    if (file.existsAsFile())
        file.deleteFile();
    
    // Create output stream
    std::unique_ptr<juce::FileOutputStream> outputStream(file.createOutputStream());
    
    if (outputStream == nullptr)
    {
        setError("Could not create output file: " + file.getFullPathName());
        return false;
    }
    
    // Write MSU-1 header
    // Bytes 0-3: "MSU1" (ASCII)
    outputStream->write("MSU1", 4);
    
    // Bytes 4-7: Loop point (32-bit little-endian)
    uint32_t loopPoint = (loopStartSample >= 0) ? static_cast<uint32_t>(loopStartSample) : 0;
    outputStream->writeInt(juce::ByteOrder::swapIfBigEndian(static_cast<int>(loopPoint)));
    
    // Prepare interleaved 16-bit PCM data
    const int numSamples = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();
    
    // Process in chunks to avoid large memory allocations
    const int chunkSize = 4096;
    std::vector<int16_t> int16Buffer(chunkSize * numChannels);
    
    for (int startSample = 0; startSample < numSamples; startSample += chunkSize)
    {
        int samplesToProcess = juce::jmin(chunkSize, numSamples - startSample);
        
        // Interleave channels and convert to int16
        for (int i = 0; i < samplesToProcess; ++i)
        {
            for (int channel = 0; channel < numChannels; ++channel)
            {
                float sample = buffer.getSample(channel, startSample + i);
                
                // Clamp to [-1.0, 1.0]
                sample = juce::jlimit(-1.0f, 1.0f, sample);
                
                // Convert to 16-bit signed integer
                int16_t int16Sample = static_cast<int16_t>(sample * 32767.0f);
                int16Buffer[i * numChannels + channel] = int16Sample;
            }
        }
        
        // Write interleaved samples in little-endian format
        for (int i = 0; i < samplesToProcess * numChannels; ++i)
        {
            outputStream->writeShort(juce::ByteOrder::swapIfBigEndian(int16Buffer[i]));
        }
    }
    
    outputStream->flush();
    
    lastError.clear();
    return true;
}

bool MSU1Exporter::validateBuffer(const juce::AudioBuffer<float>& buffer,
                                  double sampleRate,
                                  juce::String& errorMessage)
{
    // Check sample rate
    if (std::abs(sampleRate - MSU1_SAMPLE_RATE) > 0.1)
    {
        errorMessage = juce::String::formatted(
            "Invalid sample rate: %.1f Hz (must be %.1f Hz)",
            sampleRate, MSU1_SAMPLE_RATE);
        return false;
    }
    
    // Check channel count
    if (buffer.getNumChannels() != MSU1_NUM_CHANNELS)
    {
        errorMessage = juce::String::formatted(
            "Invalid channel count: %d (must be %d)",
            buffer.getNumChannels(), MSU1_NUM_CHANNELS);
        return false;
    }
    
    // Check if buffer is empty
    if (buffer.getNumSamples() == 0)
    {
        errorMessage = "Buffer is empty";
        return false;
    }
    
    return true;
}

void MSU1Exporter::convertFloatToInt16(const float* floatData,
                                      int16_t* int16Data,
                                      int numSamples)
{
    for (int i = 0; i < numSamples; ++i)
    {
        float sample = juce::jlimit(-1.0f, 1.0f, floatData[i]);
        int16Data[i] = static_cast<int16_t>(sample * 32767.0f);
    }
}

//==============================================================================
void MSU1Exporter::setError(const juce::String& error)
{
    lastError = error;
    DBG("MSU1Exporter Error: " + error);
}

bool MSU1Exporter::moveOriginalToBackupFolder(const juce::File& file)
{
    if (!file.existsAsFile())
        return true;

    auto parent = file.getParentDirectory();
    if (!parent.isDirectory())
    {
        setError("Invalid output directory: " + parent.getFullPathName());
        return false;
    }

    auto backupDir = parent.getChildFile("Backup");
    if (!backupDir.isDirectory())
    {
        if (!backupDir.createDirectory())
        {
            setError("Could not create Backup folder: " + backupDir.getFullPathName());
            return false;
        }
    }

    auto destination = backupDir.getChildFile(file.getFileName());
    if (destination.existsAsFile() && !destination.deleteFile())
    {
        setError("Could not replace existing backup: " + destination.getFullPathName());
        return false;
    }

    if (!file.moveFileTo(destination))
    {
        setError("Failed to move original file into Backup folder");
        return false;
    }

    return true;
}
