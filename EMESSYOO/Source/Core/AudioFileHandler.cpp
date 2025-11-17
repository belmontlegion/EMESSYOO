#include "AudioFileHandler.h"

#include <limits>

//==============================================================================
AudioFileHandler::AudioFileHandler()
{
    // Register standard audio formats
    formatManager.registerBasicFormats();
}

AudioFileHandler::~AudioFileHandler()
{
}

//==============================================================================
bool AudioFileHandler::loadAudioFile(const juce::File& file,
                                     juce::AudioBuffer<float>& buffer,
                                     double& sampleRate,
                                     int64* loopPoint)
{
    if (!file.existsAsFile())
    {
        setError("File does not exist: " + file.getFullPathName());
        return false;
    }
    
    // Check if this is an MSU-1 PCM file
    if (file.getFileExtension().toLowerCase() == ".pcm")
    {
        return loadMSU1PCMFile(file, buffer, sampleRate, loopPoint);
    }
    
    std::unique_ptr<juce::AudioFormatReader> reader(formatManager.createReaderFor(file));
    
    if (reader == nullptr)
    {
        setError("Could not read audio file: " + file.getFullPathName());
        return false;
    }
    
    // Get audio properties
    sampleRate = reader->sampleRate;
    auto numChannels = static_cast<int>(reader->numChannels);
    auto lengthInSamples = reader->lengthInSamples;
    
    // Allocate buffer
    buffer.setSize(numChannels, static_cast<int>(lengthInSamples));
    
    // Read audio data
    if (!reader->read(&buffer, 0, static_cast<int>(lengthInSamples), 0, true, true))
    {
        setError("Failed to read audio data from file");
        return false;
    }
    
    lastError.clear();
    return true;
}

bool AudioFileHandler::saveAudioFile(const juce::File& file,
                                     const juce::AudioBuffer<float>& buffer,
                                     double sampleRate,
                                     int bitDepth)
{
    if (buffer.getNumSamples() == 0)
    {
        setError("Cannot save empty audio buffer");
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
    
    // Determine format based on file extension
    juce::WavAudioFormat wavFormat;
    
    // Create writer
    std::unique_ptr<juce::AudioFormatWriter> writer;
    writer.reset(wavFormat.createWriterFor(outputStream.get(),
                                           sampleRate,
                                           static_cast<unsigned int>(buffer.getNumChannels()),
                                           bitDepth,
                                           {},
                                           0));
    
    if (writer == nullptr)
    {
        setError("Could not create audio format writer");
        return false;
    }
    
    // Release ownership of the output stream (writer now owns it)
    outputStream.release();
    
    // Write audio data
    if (!writer->writeFromAudioSampleBuffer(buffer, 0, buffer.getNumSamples()))
    {
        setError("Failed to write audio data to file");
        return false;
    }
    
    lastError.clear();
    return true;
}

bool AudioFileHandler::getAudioFileInfo(const juce::File& file,
                                        double& sampleRate,
                                        int& numChannels,
                                        int64& lengthInSamples)
{
    if (!file.existsAsFile())
    {
        setError("File does not exist: " + file.getFullPathName());
        return false;
    }
    
    // Check if this is an MSU-1 PCM file
    if (file.getFileExtension().toLowerCase() == ".pcm")
    {
        // MSU-1 PCM files are always 44.1kHz stereo
        sampleRate = 44100.0;
        numChannels = 2;
        
        // Calculate length from file size
        // Header: 4 bytes "MSU1" + 4 bytes loop point = 8 bytes
        // Each sample: 2 channels * 2 bytes (16-bit) = 4 bytes
        auto fileSize = file.getSize();
        if (fileSize < 8)
        {
            setError("Invalid MSU-1 PCM file: too small");
            return false;
        }
        
        lengthInSamples = (fileSize - 8) / 4;
        lastError.clear();
        return true;
    }
    
    std::unique_ptr<juce::AudioFormatReader> reader(formatManager.createReaderFor(file));
    
    if (reader == nullptr)
    {
        setError("Could not read audio file: " + file.getFullPathName());
        return false;
    }
    
    sampleRate = reader->sampleRate;
    numChannels = static_cast<int>(reader->numChannels);
    lengthInSamples = reader->lengthInSamples;
    
    lastError.clear();
    return true;
}

//==============================================================================
void AudioFileHandler::setError(const juce::String& error)
{
    lastError = error;
    DBG("AudioFileHandler Error: " + error);
}

bool AudioFileHandler::loadMSU1PCMFile(const juce::File& file,
                                       juce::AudioBuffer<float>& buffer,
                                       double& sampleRate,
                                       int64* loopPoint)
{
    // Open file for reading
    juce::FileInputStream inputStream(file);
    
    if (!inputStream.openedOk())
    {
        setError("Could not open PCM file: " + file.getFullPathName());
        return false;
    }
    
    // Read and verify MSU-1 header
    char header[4];
    if (inputStream.read(header, 4) != 4)
    {
        setError("Could not read PCM file header");
        return false;
    }
    
    if (std::memcmp(header, "MSU1", 4) != 0)
    {
        setError("Invalid MSU-1 PCM file: missing MSU1 header");
        return false;
    }
    
    // Read loop point from file header
    uint32_t loopPointValue;
    if (inputStream.read(&loopPointValue, 4) != 4)
    {
        setError("Could not read loop point from PCM file");
        return false;
    }
    
    // Store loop point if requested
    if (loopPoint != nullptr)
    {
        *loopPoint = static_cast<int64>(loopPointValue);
    }
    
    // Calculate number of samples
    auto remainingBytes = inputStream.getTotalLength() - 8; // 8 bytes for header + loop
    auto numSamples64 = remainingBytes / 4; // 4 bytes per stereo sample (2 channels * 2 bytes)
    
    if (numSamples64 <= 0)
    {
        setError("PCM file contains no audio data");
        return false;
    }
    
    if (numSamples64 > std::numeric_limits<int>::max())
    {
        setError("PCM file is too large to load into memory");
        return false;
    }
    
    const auto numSamples = static_cast<int>(numSamples64);
    
    // MSU-1 format is always 44.1kHz stereo
    sampleRate = 44100.0;
    buffer.setSize(2, numSamples);
    
    // Read interleaved 16-bit PCM data
    std::vector<int16_t> int16Data(static_cast<size_t>(numSamples) * 2);
    auto bytesRead = inputStream.read(int16Data.data(), static_cast<int64>(numSamples) * 4);
    
    if (bytesRead != static_cast<int64>(numSamples) * 4)
    {
        setError("Failed to read all audio data from PCM file");
        return false;
    }
    
    // Convert int16 to float and de-interleave
    for (int i = 0; i < numSamples; ++i)
    {
        buffer.setSample(0, i, int16Data[static_cast<size_t>(i) * 2] / 32768.0f);     // Left
        buffer.setSample(1, i, int16Data[static_cast<size_t>(i) * 2 + 1] / 32768.0f); // Right
    }
    
    lastError.clear();
    return true;
}
