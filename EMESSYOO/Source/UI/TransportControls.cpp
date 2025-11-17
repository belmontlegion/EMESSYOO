#include "TransportControls.h"
#include "CustomLookAndFeel.h"

//==============================================================================
TransportControls::TransportControls(MSUProjectState& state, AudioPlayer& player)
    : projectState(state),
      audioPlayer(player)
{
    // Setup buttons
    addAndMakeVisible(playButton);
    playButton.setButtonText("Play");
    playButton.addListener(this);
    
    addAndMakeVisible(pauseButton);
    pauseButton.setButtonText("Pause");
    pauseButton.addListener(this);
    
    addAndMakeVisible(stopButton);
    stopButton.setButtonText("Stop");
    stopButton.addListener(this);
    
    addAndMakeVisible(loopButton);
    loopButton.setButtonText("Loop");
    loopButton.setClickingTogglesState(true);
    loopButton.setToggleState(true, juce::dontSendNotification);
    loopButton.addListener(this);
    audioPlayer.setLooping(loopButton.getToggleState());
    
    addAndMakeVisible(autoScrollButton);
    autoScrollButton.setButtonText("Auto-Scroll");
    autoScrollButton.setClickingTogglesState(true);
    autoScrollButton.setToggleState(false, juce::dontSendNotification);
    autoScrollButton.addListener(this);
    
    addAndMakeVisible(autoTrimPadButton);
    autoTrimPadButton.setButtonText("Auto Trim/Pad");
    autoTrimPadButton.setClickingTogglesState(true);
    autoTrimPadButton.setToggleState(false, juce::dontSendNotification);
    autoTrimPadButton.addListener(this);
    
    addAndMakeVisible(padAmountLabel);
    padAmountLabel.setText("Pad Amount (In miliseconds)", juce::dontSendNotification);
    padAmountLabel.setJustificationType(juce::Justification::centredLeft);
    padAmountLabel.setColour(juce::Label::textColourId, CustomLookAndFeel::textColor);
    
    addAndMakeVisible(padAmountSlider);
    padAmountSlider.setRange(10.0, 5000.0, 10.0);
    padAmountSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    padAmountSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 60, 20);
    padAmountSlider.setColour(juce::Slider::textBoxTextColourId, CustomLookAndFeel::textColor);
    padAmountSlider.setColour(juce::Slider::textBoxOutlineColourId, CustomLookAndFeel::darkControl);
    padAmountSlider.setValue(projectState.getPadAmountMs(), juce::dontSendNotification);
    padAmountSlider.onValueChange = [this]()
    {
        int padMs = juce::roundToInt(padAmountSlider.getValue());
        projectState.setPadAmountMs(padMs);
        if (autoTrimPadButton.getToggleState())
            applyAutoTrimPad();
    };
    
    addAndMakeVisible(trimNoPadButton);
    trimNoPadButton.setButtonText("Trim (No Pad)");
    trimNoPadButton.setClickingTogglesState(true);
    trimNoPadButton.setToggleState(false, juce::dontSendNotification);
    trimNoPadButton.addListener(this);
    
    // Setup labels
    addAndMakeVisible(positionLabel);
    positionLabel.setJustificationType(juce::Justification::centred);
    positionLabel.setColour(juce::Label::textColourId, CustomLookAndFeel::textColor);
    
    addAndMakeVisible(durationLabel);
    durationLabel.setJustificationType(juce::Justification::centred);
    durationLabel.setColour(juce::Label::textColourId, CustomLookAndFeel::textColorDark);
    
    // Start timer for position updates
    startTimer(50);
    
    updatePositionDisplay();
}

TransportControls::~TransportControls()
{
    stopTimer();
}

//==============================================================================
void TransportControls::paint(juce::Graphics& g)
{
    g.fillAll(CustomLookAndFeel::darkPanel);
}

void TransportControls::resized()
{
    auto bounds = getLocalBounds().reduced(8);
    
    // Top row: time display
    auto topRow = bounds.removeFromTop(24);
    positionLabel.setBounds(topRow.removeFromLeft(120));
    topRow.removeFromLeft(8);
    durationLabel.setBounds(topRow.removeFromLeft(120));
    
    bounds.removeFromTop(8);
    
    // Bottom row: transport buttons (expanded for 7 buttons)
    auto buttonRow = bounds.removeFromTop(60);
    int buttonWidth = 80;
    int spacing = 8;
    int padControlWidth = 180;
    
    auto buttonArea = buttonRow.withSizeKeepingCentre(buttonWidth * 7 + padControlWidth + spacing * 7, 60);
    
    playButton.setBounds(buttonArea.removeFromLeft(buttonWidth));
    buttonArea.removeFromLeft(spacing);
    
    pauseButton.setBounds(buttonArea.removeFromLeft(buttonWidth));
    buttonArea.removeFromLeft(spacing);
    
    stopButton.setBounds(buttonArea.removeFromLeft(buttonWidth));
    buttonArea.removeFromLeft(spacing);
    
    loopButton.setBounds(buttonArea.removeFromLeft(buttonWidth));
    buttonArea.removeFromLeft(spacing);
    
    autoScrollButton.setBounds(buttonArea.removeFromLeft(buttonWidth));
    buttonArea.removeFromLeft(spacing);
    
    autoTrimPadButton.setBounds(buttonArea.removeFromLeft(buttonWidth));
    buttonArea.removeFromLeft(spacing);
    
    auto padArea = buttonArea.removeFromLeft(padControlWidth);
    padAmountLabel.setBounds(padArea.removeFromTop(16));
    padArea.removeFromTop(4);
    padAmountSlider.setBounds(padArea);
    buttonArea.removeFromLeft(spacing);
    
    trimNoPadButton.setBounds(buttonArea.removeFromLeft(buttonWidth));
}

void TransportControls::buttonClicked(juce::Button* button)
{
    if (button == &playButton)
    {
        audioPlayer.play();
    }
    else if (button == &pauseButton)
    {
        audioPlayer.pause();
    }
    else if (button == &stopButton)
    {
        audioPlayer.stop();
        updatePositionDisplay();
    }
    else if (button == &loopButton)
    {
        audioPlayer.setLooping(loopButton.getToggleState());
    }
    else if (button == &autoTrimPadButton)
    {
        if (autoTrimPadButton.getToggleState())
        {
            trimNoPadButton.setToggleState(false, juce::dontSendNotification);
            applyAutoTrimPad();
        }
        else if (!trimNoPadButton.getToggleState())
        {
            resetTrimAndPadding();
        }
    }
    else if (button == &trimNoPadButton)
    {
        if (trimNoPadButton.getToggleState())
        {
            autoTrimPadButton.setToggleState(false, juce::dontSendNotification);
            applyTrimNoPad();
        }
        else if (!autoTrimPadButton.getToggleState())
        {
            resetTrimAndPadding();
        }
    }
}

void TransportControls::timerCallback()
{
    updatePositionDisplay();
}

//==============================================================================
void TransportControls::updatePositionDisplay()
{
    double position = audioPlayer.getPosition();
    double duration = projectState.getLengthInSeconds();
    
    positionLabel.setText(formatTime(position), juce::dontSendNotification);
    durationLabel.setText("/ " + formatTime(duration), juce::dontSendNotification);
}

juce::String TransportControls::formatTime(double seconds) const
{
    int totalSeconds = static_cast<int>(seconds);
    int minutes = totalSeconds / 60;
    int secs = totalSeconds % 60;
    int millis = static_cast<int>((seconds - totalSeconds) * 100);
    
    return juce::String::formatted("%02d:%02d.%02d", minutes, secs, millis);
}

//==============================================================================
int64 TransportControls::detectFirstAudioSample(float thresholdDb) const
{
    if (!projectState.hasAudio())
        return 0;
    
    const auto& buffer = projectState.getAudioBuffer();
    float thresholdLinear = juce::Decibels::decibelsToGain(thresholdDb);
    
    // Scan through audio to find first sample above threshold
    for (int64 sample = 0; sample < buffer.getNumSamples(); ++sample)
    {
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            float sampleValue = std::abs(buffer.getSample(ch, static_cast<int>(sample)));
            if (sampleValue > thresholdLinear)
                return sample;
        }
    }
    
    return 0; // No audio detected, return start
}

void TransportControls::applyAutoTrimPad()
{
    if (!projectState.hasAudio())
        return;
    
    int64 firstAudioSample = detectFirstAudioSample(-60.0f);
    double sampleRate = projectState.getSampleRate();
    int padAmountMs = projectState.getPadAmountMs();
    int64 padSamples = static_cast<int64>((padAmountMs / 1000.0) * sampleRate);
    padSamples = juce::jmax<int64>(0, padSamples);
    
    if (padSamples == 0)
    {
        projectState.setTrimStart(firstAudioSample);
        projectState.setPaddingSamples(0);
        return;
    }
    
    int64 availableSilence = firstAudioSample;
    if (availableSilence >= padSamples)
    {
        // Enough lead-in silence in file, trim to leave only desired pad
        int64 trimPosition = firstAudioSample - padSamples;
        projectState.setTrimStart(trimPosition);
        projectState.setPaddingSamples(0);
    }
    else
    {
        // Not enough silence, trim to start of file and add difference as padding
        int64 neededPadding = padSamples - availableSilence;
        projectState.setTrimStart(0);
        projectState.setPaddingSamples(neededPadding);
    }
}

void TransportControls::applyTrimNoPad()
{
    if (!projectState.hasAudio())
        return;
    
    int64 firstAudioSample = detectFirstAudioSample(-60.0f);
    projectState.setTrimStart(firstAudioSample);
    projectState.setPaddingSamples(0); // No padding in trim mode
}

void TransportControls::resetTrimAndPadding()
{
    projectState.setTrimStart(0);
    projectState.setPaddingSamples(0);
}
