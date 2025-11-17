#include "MSUProjectState.h"

void MSUProjectState::setPadAmountMs(int milliseconds)
{
    int clamped = juce::jlimit(10, 5000, milliseconds); // allow between 10ms and 5s
    if (padAmountMs == clamped)
        return;
    padAmountMs = clamped;
    sendChangeMessage();
}

//==============================================================================
MSUProjectState::MSUProjectState()
{
}

MSUProjectState::~MSUProjectState()
{
}

//==============================================================================
void MSUProjectState::setAudioBuffer(const juce::AudioBuffer<float>& newBuffer, double sampleRate)
{
    audioBuffer = newBuffer;
    projectSampleRate = sampleRate;
    modified = true;
    sendChangeMessage();
}

double MSUProjectState::getLengthInSeconds() const
{
    if (projectSampleRate <= 0.0 || audioBuffer.getNumSamples() <= 0)
        return 0.0;
    
    return audioBuffer.getNumSamples() / projectSampleRate;
}

//==============================================================================
void MSUProjectState::setLoopStart(int64 samplePosition)
{
    loopStartSample = juce::jlimit<int64>(0, audioBuffer.getNumSamples() - 1, samplePosition);
    
    // Ensure loop end is after loop start
    if (loopEndSample >= 0 && loopEndSample <= loopStartSample)
        loopEndSample = juce::jmin<int64>(loopStartSample + 1, audioBuffer.getNumSamples());
    
    modified = true;
    sendChangeMessage();
}

void MSUProjectState::setTrimStart(int64 samplePosition)
{
    trimStartSample = juce::jlimit<int64>(0, audioBuffer.getNumSamples(), samplePosition);
    modified = true;
    sendChangeMessage();
}

void MSUProjectState::setLoopEnd(int64 samplePosition)
{
    loopEndSample = juce::jlimit<int64>(0, audioBuffer.getNumSamples(), samplePosition);
    
    // Ensure loop start is before loop end
    if (loopStartSample >= 0 && loopStartSample >= loopEndSample)
        loopStartSample = juce::jmax<int64>(loopEndSample - 1, 0);
    
    modified = true;
    sendChangeMessage();
}

//==============================================================================
void MSUProjectState::setSourceFile(const juce::File& file)
{
    sourceFile = file;
    sendChangeMessage();
}

void MSUProjectState::setModified(bool isModified)
{
    modified = isModified;
    sendChangeMessage();
}

//==============================================================================
void MSUProjectState::reset()
{
    audioBuffer.setSize(0, 0);
    projectSampleRate = 44100.0;
    loopStartSample = -1;
    loopEndSample = -1;
    trimStartSample = 0;
    paddingSamples = 0;
    padAmountMs = 200;
    sourceFile = juce::File();
    targetExportFile = juce::File();
    modified = false;
    targetRMSDb = -12.0f;
    normalizationGainDb = 0.0f;
    
    sendChangeMessage();
}
