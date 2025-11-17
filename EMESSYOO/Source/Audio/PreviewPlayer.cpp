#include "PreviewPlayer.h"
#include "../Core/AudioFileHandler.h"

#include <limits>

//==============================================================================
PreviewPlayer::PreviewPlayer()
{
    // Register audio formats
    formatManager.registerBasicFormats();
}

PreviewPlayer::~PreviewPlayer()
{
    stop();
}

void PreviewPlayer::setAudioDeviceManager(juce::AudioDeviceManager* manager)
{
    const juce::ScopedLock lock(callbackLock);
    audioDeviceManager = manager;
    refreshDeviceSampleRate();
}

void PreviewPlayer::refreshDeviceSampleRate()
{
    if (audioDeviceManager == nullptr)
        return;

    juce::AudioDeviceManager::AudioDeviceSetup setup;
    audioDeviceManager->getAudioDeviceSetup(setup);
    if (setup.sampleRate > 0.0)
    {
        deviceSampleRate = setup.sampleRate;
        return;
    }

    if (auto* device = audioDeviceManager->getCurrentAudioDevice())
    {
        auto rate = device->getCurrentSampleRate();
        if (rate > 0.0)
            deviceSampleRate = rate;
    }
}

//==============================================================================
bool PreviewPlayer::loadAndPlay(const juce::File& file)
{
    // Stop any current playback (this acquires the lock)
    stop();
    
    if (!file.existsAsFile())
        return false;
    
    // Check if it's a PCM file (needs special handling)
    if (file.getFileExtension().toLowerCase() == ".pcm")
    {
        // Load PCM data into buffer (avoid real-time disk I/O)
        loadPCMIntoBuffer(file);
        refreshDeviceSampleRate();
        
        if (pcmBuffer.getNumSamples() == 0)
            return false;
        
        const juce::ScopedLock lock(callbackLock);
        isPCMFile = true;
        pcmPosition = 0;
        pcmFractionalPosition = 0.0;
        playing = true;
        
        return true;
    }
    else
    {
        // Standard audio format
        currentReader.reset(formatManager.createReaderFor(file));
        
        if (currentReader == nullptr)
            return false;
        
        // Now lock and set up for playback
        const juce::ScopedLock lock(callbackLock);
        
        isPCMFile = false;
        
        // Create reader source and prepare for playback
        readerSource = std::make_unique<juce::AudioFormatReaderSource>(currentReader.get(), false);
        
        // Create resampling source to handle sample rate conversion
        // Note: deviceSampleRate is set by audioDeviceAboutToStart() when device initializes
        resamplingSource = std::make_unique<juce::ResamplingAudioSource>(readerSource.get(), false, currentReader->numChannels);
        
        // Set resampling ratio: source rate / output rate
        // e.g., 44100 / 48000 = 0.91875 (plays slower to pitch down)
        //       48000 / 44100 = 1.0884 (plays faster to pitch up)
        refreshDeviceSampleRate();

        if (deviceSampleRate > 0)
        {
            double ratio = currentReader->sampleRate / deviceSampleRate;
            DBG("Preview: File SR=" + juce::String(currentReader->sampleRate) + 
                " Device SR=" + juce::String(deviceSampleRate) + 
                " Ratio=" + juce::String(ratio));
            
            resamplingSource->setResamplingRatio(ratio);
            resamplingSource->prepareToPlay(512, deviceSampleRate);
            playing = true;
        }
        
        return true;
    }
}

void PreviewPlayer::stop()
{
    const juce::ScopedLock lock(callbackLock);
    
    playing = false;
    
    resamplingSource.reset();
    readerSource.reset();
    currentReader.reset();
    
    // Clean up PCM streaming
    pcmInputStream.reset();
    pcmBuffer.setSize(0, 0);
    pcmPosition = 0;
    pcmFractionalPosition = 0.0;
    pcmLoopPoint = 0;
    pcmTotalSamples = 0;
    isPCMFile = false;
}

double PreviewPlayer::getPosition() const
{
    if (isPCMFile)
        return pcmFractionalPosition / pcmNativeSampleRate;
    
    if (readerSource == nullptr || currentReader == nullptr)
        return 0.0;
    
    return static_cast<double>(readerSource->getNextReadPosition()) / currentReader->sampleRate;
}

double PreviewPlayer::getTotalLength() const
{
    if (isPCMFile)
        return static_cast<double>(pcmBuffer.getNumSamples()) / pcmNativeSampleRate;
    
    if (currentReader == nullptr)
        return 0.0;
    
    return static_cast<double>(currentReader->lengthInSamples) / currentReader->sampleRate;
}

//==============================================================================
void PreviewPlayer::audioDeviceAboutToStart(juce::AudioIODevice* device)
{
    deviceSampleRate = device != nullptr ? device->getCurrentSampleRate() : 0.0;
    
    if (deviceSampleRate <= 0.0)
        refreshDeviceSampleRate();
    
    DBG("Preview device starting at " + juce::String(deviceSampleRate) + " Hz");
    
    // Update resampling ratio if we have a file loaded
    if (resamplingSource != nullptr && currentReader != nullptr)
    {
        const juce::ScopedLock lock(callbackLock);
        double ratio = currentReader->sampleRate / deviceSampleRate;
        DBG("Updating resampling ratio to " + juce::String(ratio));
        resamplingSource->setResamplingRatio(ratio);
        resamplingSource->prepareToPlay(device->getCurrentBufferSizeSamples(), deviceSampleRate);
    }
}

void PreviewPlayer::audioDeviceStopped()
{
    if (resamplingSource != nullptr)
        resamplingSource->releaseResources();
    deviceSampleRate = 0.0;
}

void PreviewPlayer::audioDeviceIOCallbackWithContext(const float* const* inputChannelData,
                                                     int numInputChannels,
                                                     float* const* outputChannelData,
                                                     int numOutputChannels,
                                                     int numSamples,
                                                     const juce::AudioIODeviceCallbackContext& context)
{
    juce::ignoreUnused(inputChannelData, numInputChannels, context);
    
    const juce::ScopedLock lock(callbackLock);
    
    if (!playing)
    {
        // Clear output
        for (int i = 0; i < numOutputChannels; ++i)
            juce::zeromem(outputChannelData[i], sizeof(float) * numSamples);
        return;
    }
    
    if (isPCMFile)
    {
        auto clearTail = [&](int startSample)
        {
            if (startSample >= numSamples)
                return;
            for (int ch = 0; ch < numOutputChannels; ++ch)
            {
                if (outputChannelData[ch] != nullptr)
                    juce::FloatVectorOperations::clear(outputChannelData[ch] + startSample,
                                                       numSamples - startSample);
            }
        };

        if (pcmBuffer.getNumSamples() == 0)
        {
            clearTail(0);
            playing = false;
            return;
        }

        refreshDeviceSampleRate();
        const double playbackRate = deviceSampleRate > 0.0 ? deviceSampleRate : pcmNativeSampleRate;
        const double ratio = pcmNativeSampleRate / playbackRate;
        const int availableChannels = juce::jmin(numOutputChannels, pcmBuffer.getNumChannels());

        for (int i = 0; i < numSamples; ++i)
        {
            if (pcmFractionalPosition >= pcmBuffer.getNumSamples())
            {
                playing = false;
                clearTail(i);
                break;
            }

            const int sourceIndex = static_cast<int>(pcmFractionalPosition);
            const int nextIndex = juce::jmin(sourceIndex + 1, pcmBuffer.getNumSamples() - 1);
            const double fraction = pcmFractionalPosition - static_cast<double>(sourceIndex);

            for (int ch = 0; ch < availableChannels; ++ch)
            {
                if (outputChannelData[ch] != nullptr)
                {
                    const float sample1 = pcmBuffer.getSample(ch, sourceIndex);
                    const float sample2 = pcmBuffer.getSample(ch, nextIndex);
                    outputChannelData[ch][i] = static_cast<float>(sample1 + (sample2 - sample1) * fraction);
                }
            }

            // Ensure any extra hardware channels stay silent
            for (int ch = availableChannels; ch < numOutputChannels; ++ch)
            {
                if (outputChannelData[ch] != nullptr)
                    outputChannelData[ch][i] = 0.0f;
            }

            pcmFractionalPosition += ratio;
        }

        pcmPosition = static_cast<int64>(pcmFractionalPosition);
        return;
    }
    else
    {
        // Standard audio format with resampling
        if (resamplingSource == nullptr)
        {
            for (int i = 0; i < numOutputChannels; ++i)
                juce::zeromem(outputChannelData[i], sizeof(float) * numSamples);
            return;
        }
        
        // Create AudioBuffer wrapper for output
        juce::AudioBuffer<float> buffer(outputChannelData, numOutputChannels, numSamples);
        juce::AudioSourceChannelInfo info(&buffer, 0, numSamples);
        
        resamplingSource->getNextAudioBlock(info);
        
        // Check if we've reached the end
        if (readerSource != nullptr && readerSource->getNextReadPosition() >= readerSource->getTotalLength())
        {
            playing = false;
        }
    }
}

void PreviewPlayer::loadPCMIntoBuffer(const juce::File& file)
{
    pcmTotalSamples = 0;

    // Read loop point from companion file
    juce::File loopFile = file.withFileExtension(".pcm.loop");
    pcmLoopPoint = 0;
    
    if (loopFile.existsAsFile())
    {
        juce::FileInputStream loopStream(loopFile);
        if (loopStream.openedOk() && loopStream.getNumBytesRemaining() >= 4)
        {
            // Read 32-bit little-endian loop point (in samples)
            pcmLoopPoint = loopStream.readInt();
        }
    }
    
    // Open PCM file
    juce::FileInputStream pcmStream(file);
    if (!pcmStream.openedOk())
    {
        pcmBuffer.setSize(0, 0);
        return;
    }
    
    // Calculate total samples (PCM is 16-bit stereo @ 44100Hz)
    const int64 fileSize = pcmStream.getTotalLength();
    const int64 totalSamples = fileSize / 4; // 2 bytes per sample * 2 channels
    if (totalSamples <= 0 || totalSamples > std::numeric_limits<int>::max())
    {
        pcmBuffer.setSize(0, 0);
        return;
    }
    
    // Allocate buffer (stereo)
    pcmBuffer.setSize(2, static_cast<int>(totalSamples));
    
    // Read and convert PCM data
    juce::HeapBlock<char> rawData;
    rawData.malloc(static_cast<size_t>(fileSize));
    
    if (pcmStream.read(rawData, static_cast<int>(fileSize)) != fileSize)
    {
        pcmBuffer.setSize(0, 0);
        return;
    }
    
    // Convert 16-bit signed PCM to float
    float* leftChannel = pcmBuffer.getWritePointer(0);
    float* rightChannel = pcmBuffer.getWritePointer(1);
    
    for (int i = 0; i < static_cast<int>(totalSamples); ++i)
    {
        int16_t leftSample = *reinterpret_cast<int16_t*>(rawData.getData() + i * 4);
        int16_t rightSample = *reinterpret_cast<int16_t*>(rawData.getData() + i * 4 + 2);
        
        leftChannel[i] = leftSample / 32768.0f;
        rightChannel[i] = rightSample / 32768.0f;
    }
    
    pcmTotalSamples = totalSamples;
}
