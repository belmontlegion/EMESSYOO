#include "WaveformView.h"
#include "CustomLookAndFeel.h"

//==============================================================================
WaveformView::WaveformView(MSUProjectState& state)
    : projectState(state),
      thumbnailCache(5),
      thumbnail(512, formatManager, thumbnailCache),
      scrollBar(false)
{
    // Register audio formats for thumbnail
    formatManager.registerBasicFormats();
    
    // Listen to project state changes
    projectState.addChangeListener(this);
    
    // Setup scroll bar
    addAndMakeVisible(scrollBar);
    scrollBar.addListener(this);
    scrollBar.setAutoHide(false);
    
    // Enable keyboard focus for hotkeys
    setWantsKeyboardFocus(true);
    
    // Initial thumbnail update
    updateThumbnail();
}

WaveformView::~WaveformView()
{
    projectState.removeChangeListener(this);
    scrollBar.removeListener(this);
}

//==============================================================================
void WaveformView::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().reduced(4);
    
    // Draw background
    g.fillAll(CustomLookAndFeel::darkBackground);
    g.setColour(CustomLookAndFeel::darkPanel);
    g.fillRect(bounds);
    
    if (!projectState.hasAudio())
    {
        // Show "No audio loaded" message
        g.setColour(CustomLookAndFeel::textColorDark);
        g.setFont(16.0f);
        g.drawText("No audio loaded", bounds, juce::Justification::centred);
        return;
    }
    
    // Draw waveform
    g.setColour(CustomLookAndFeel::greenAccent);
    thumbnail.drawChannels(g, bounds.reduced(2),
                          visibleStart, visibleEnd,
                          1.0f);
    
    // Highlight added padding region at start if present
    if (projectState.getPaddingSamples() > 0 && projectState.getSampleRate() > 0.0)
    {
        double paddingStartTime = 0.0;
        double paddingEndTime = static_cast<double>(projectState.getPaddingSamples()) / projectState.getSampleRate();
        
        if (paddingEndTime > visibleStart && paddingStartTime < visibleEnd)
        {
            double visibleRange = visibleEnd - visibleStart;
            if (visibleRange <= 0.0)
                visibleRange = 0.000001;
            int paddingStartX = bounds.getX() + static_cast<int>(((paddingStartTime - visibleStart) / visibleRange) * bounds.getWidth());
            int paddingEndX = bounds.getX() + static_cast<int>(((paddingEndTime - visibleStart) / visibleRange) * bounds.getWidth());
            
            paddingStartX = juce::jmax(bounds.getX(), paddingStartX);
            paddingEndX = juce::jmin(bounds.getRight(), paddingEndX);
            
            if (paddingEndX > paddingStartX)
            {
                auto paddingBounds = juce::Rectangle<int>(paddingStartX, bounds.getY(), paddingEndX - paddingStartX, bounds.getHeight());
                
                g.setColour(CustomLookAndFeel::darkPanel.withAlpha(0.5f));
                g.fillRect(paddingBounds);
                
                g.setColour(juce::Colours::blue.withAlpha(0.4f));
                g.fillRect(paddingBounds);
                
                g.setColour(juce::Colours::lightblue);
                juce::String label = "PAD: " + juce::String(static_cast<int>(paddingEndTime * 1000.0)) + " ms";
                g.setFont(10.0f);
                g.drawText(label, paddingBounds.reduced(4), juce::Justification::topLeft, false);
            }
        }
    }
    
    // Draw trim start marker (drawn first, behind loop markers)
    if (projectState.hasAudio())
    {
        int trimStartX = getTrimHandleX();
        
        if (trimStartX >= bounds.getX() && trimStartX <= bounds.getRight())
        {
            // Vertical line
            g.setColour(juce::Colours::yellow);
            g.drawLine(static_cast<float>(trimStartX), static_cast<float>(bounds.getY()),
                      static_cast<float>(trimStartX), static_cast<float>(bounds.getBottom()), 2.0f);
            
            // Draw handle at top
            juce::Path handle;
            handle.addTriangle(trimStartX - 8.0f, static_cast<float>(bounds.getY()),
                             trimStartX + 8.0f, static_cast<float>(bounds.getY()),
                             static_cast<float>(trimStartX), static_cast<float>(bounds.getY()) + 12.0f);
            g.fillPath(handle);
            
            // Label
            g.setFont(11.0f);
            g.drawText("Trim", trimStartX + 4, bounds.getY() + 16, 100, 20,
                      juce::Justification::left);
        }
    }
    
    // Draw loop markers if set
    if (projectState.hasLoopPoints())
    {
        auto loopStart = projectState.getLoopStart();
        auto loopEnd = projectState.getLoopEnd();
        
        int loopStartX = xAtSample(loopStart);
        int loopEndX = xAtSample(loopEnd);
        
        // Draw loop region
        if (loopStartX < bounds.getRight() && loopEndX > bounds.getX())
        {
            auto loopBounds = bounds.withLeft(juce::jmax(bounds.getX(), loopStartX))
                                   .withRight(juce::jmin(bounds.getRight(), loopEndX));
            
            g.setColour(CustomLookAndFeel::greenAccent.withAlpha(0.1f));
            g.fillRect(loopBounds);
        }
        
        // Draw loop start marker
        if (loopStartX >= bounds.getX() && loopStartX <= bounds.getRight())
        {
            // Vertical line
            g.setColour(CustomLookAndFeel::greenAccentBright);
            g.drawLine(static_cast<float>(loopStartX), static_cast<float>(bounds.getY()),
                      static_cast<float>(loopStartX), static_cast<float>(bounds.getBottom()), 2.0f);
            
            // Draw handle at top
            juce::Path handle;
            handle.addTriangle(loopStartX - 8.0f, static_cast<float>(bounds.getY()),
                             loopStartX + 8.0f, static_cast<float>(bounds.getY()),
                             static_cast<float>(loopStartX), static_cast<float>(bounds.getY()) + 12.0f);
            g.fillPath(handle);
            
            // Label
            g.setFont(11.0f);
            g.drawText("Loop Start", loopStartX + 4, bounds.getY() + 16, 100, 20,
                      juce::Justification::left);
        }
        
        // Draw loop end marker
        if (loopEndX >= bounds.getX() && loopEndX <= bounds.getRight())
        {
            // Vertical line
            g.setColour(juce::Colours::orange);
            g.drawLine(static_cast<float>(loopEndX), static_cast<float>(bounds.getY()),
                      static_cast<float>(loopEndX), static_cast<float>(bounds.getBottom()), 2.0f);
            
            // Draw handle at top
            juce::Path handle;
            handle.addTriangle(loopEndX - 8.0f, static_cast<float>(bounds.getY()),
                             loopEndX + 8.0f, static_cast<float>(bounds.getY()),
                             static_cast<float>(loopEndX), static_cast<float>(bounds.getY()) + 12.0f);
            g.fillPath(handle);
            
            // Label - position on left side if too close to right edge
            int labelX = loopEndX + 4;
            auto labelJustification = juce::Justification::left;
            
            if (loopEndX + 80 > bounds.getRight())
            {
                labelX = loopEndX - 84;
                labelJustification = juce::Justification::right;
            }
            
            g.setFont(11.0f);
            g.drawText("Loop End", labelX, bounds.getY() + 16, 80, 20, labelJustification);
        }
    }
    
    // Draw playhead
    if (playPosition >= visibleStart && playPosition <= visibleEnd)
    {
        double ratio = (playPosition - visibleStart) / (visibleEnd - visibleStart);
        int playheadX = bounds.getX() + static_cast<int>(ratio * bounds.getWidth());
        
        g.setColour(juce::Colours::white);
        g.drawLine(static_cast<float>(playheadX), static_cast<float>(bounds.getY()),
                  static_cast<float>(playheadX), static_cast<float>(bounds.getBottom()), 1.0f);
    }
    
    // Draw time ruler at bottom
    if (projectState.hasAudio())
    {
        int rulerHeight = 20;
        auto rulerBounds = juce::Rectangle<int>(bounds.getX(), bounds.getBottom() - rulerHeight,
                                                 bounds.getWidth(), rulerHeight);
        
        // Draw ruler background
        g.setColour(CustomLookAndFeel::darkControl.withAlpha(0.8f));
        g.fillRect(rulerBounds);
        
        // Calculate appropriate time interval based on zoom level
        double visibleDuration = visibleEnd - visibleStart;
        double msPerPixel = (visibleDuration * 1000.0) / bounds.getWidth();
        
        // Determine tick interval in milliseconds
        int majorTickInterval = 1000; // 1 second
        int minorTickInterval = 100;  // 100ms
        
        if (msPerPixel > 50.0)
        {
            majorTickInterval = 10000; // 10 seconds
            minorTickInterval = 1000;  // 1 second
        }
        else if (msPerPixel > 10.0)
        {
            majorTickInterval = 5000;  // 5 seconds
            minorTickInterval = 500;   // 500ms
        }
        else if (msPerPixel < 0.5)
        {
            majorTickInterval = 100;   // 100ms
            minorTickInterval = 10;    // 10ms
        }
        else if (msPerPixel < 2.0)
        {
            majorTickInterval = 500;   // 500ms
            minorTickInterval = 50;    // 50ms
        }
        
        // Draw ticks and labels
        int startMs = static_cast<int>(visibleStart * 1000.0);
        int endMs = static_cast<int>(visibleEnd * 1000.0);
        
        // Align to tick interval
        int firstTick = (startMs / minorTickInterval) * minorTickInterval;
        
        g.setFont(9.0f);
        
        for (int ms = firstTick; ms <= endMs; ms += minorTickInterval)
        {
            double timeInSeconds = ms / 1000.0;
            if (timeInSeconds < visibleStart || timeInSeconds > visibleEnd)
                continue;
            
            double ratio = (timeInSeconds - visibleStart) / (visibleEnd - visibleStart);
            int x = bounds.getX() + static_cast<int>(ratio * bounds.getWidth());
            
            bool isMajorTick = (ms % majorTickInterval == 0);
            
            if (isMajorTick)
            {
                // Major tick
                g.setColour(CustomLookAndFeel::textColor);
                g.drawLine(static_cast<float>(x), static_cast<float>(rulerBounds.getY()),
                          static_cast<float>(x), static_cast<float>(rulerBounds.getY() + 8), 1.0f);
                
                // Label
                juce::String label;
                if (ms >= 1000)
                {
                    double seconds = ms / 1000.0;
                    if (ms % 1000 == 0)
                        label = juce::String(static_cast<int>(seconds)) + "s";
                    else
                        label = juce::String(seconds, 1) + "s";
                }
                else
                {
                    label = juce::String(ms) + "ms";
                }
                
                g.drawText(label, x - 30, rulerBounds.getY() + 8, 60, 12,
                          juce::Justification::centred, false);
            }
            else
            {
                // Minor tick
                g.setColour(CustomLookAndFeel::textColorDark);
                g.drawLine(static_cast<float>(x), static_cast<float>(rulerBounds.getY()),
                          static_cast<float>(x), static_cast<float>(rulerBounds.getY() + 4), 1.0f);
            }
        }
    }

    if (projectState.hasAudio())
        drawHotkeyLegend(g, bounds);
    
    // Draw border
    g.setColour(CustomLookAndFeel::darkControl);
    g.drawRect(bounds, 1);
}

void WaveformView::resized()
{
    auto bounds = getLocalBounds();
    scrollBar.setBounds(bounds.removeFromBottom(16).reduced(4, 2));
    
    updateVisibleRange();
}

void WaveformView::mouseDown(const juce::MouseEvent& event)
{
    if (!projectState.hasAudio())
        return;
    
    auto bounds = getLocalBounds().reduced(4);
    // Check for trim start marker first (highest priority)
    if (projectState.hasAudio())
    {
        int trimStartX = getTrimHandleX();
        bool inHandleArea = event.y < (bounds.getY() + 20);
        
        if (std::abs(event.x - trimStartX) < 10 && inHandleArea)
        {
            currentDragMode = TrimStart;
            selectedHandle = TrimStart;
            setMouseCursor(juce::MouseCursor::LeftRightResizeCursor);
            return;
        }
        
        if (std::abs(event.x - trimStartX) < 5)
        {
            currentDragMode = TrimStart;
            selectedHandle = TrimStart;
            setMouseCursor(juce::MouseCursor::LeftRightResizeCursor);
            return;
        }
    }
    
    // Check if clicking near loop markers (within 10 pixels for handle area)
    if (projectState.hasLoopPoints())
    {
        int loopStartX = xAtSample(projectState.getLoopStart());
        int loopEndX = xAtSample(projectState.getLoopEnd());
        
        // Check for handle click (top 20 pixels of bounds)
        bool inHandleArea = event.y < (bounds.getY() + 20);
        
        if (std::abs(event.x - loopStartX) < 10 && inHandleArea)
        {
            currentDragMode = LoopStart;
            selectedHandle = LoopStart;  // Select for wheel fine-tuning
            setMouseCursor(juce::MouseCursor::LeftRightResizeCursor);
            return;
        }
        
        if (std::abs(event.x - loopEndX) < 10 && inHandleArea)
        {
            currentDragMode = LoopEnd;
            selectedHandle = LoopEnd;  // Select for wheel fine-tuning
            setMouseCursor(juce::MouseCursor::LeftRightResizeCursor);
            return;
        }
        
        // Check for line click anywhere on the vertical line
        if (std::abs(event.x - loopStartX) < 5)
        {
            currentDragMode = LoopStart;
            selectedHandle = LoopStart;  // Select for wheel fine-tuning
            setMouseCursor(juce::MouseCursor::LeftRightResizeCursor);
            return;
        }
        
        if (std::abs(event.x - loopEndX) < 5)
        {
            currentDragMode = LoopEnd;
            selectedHandle = LoopEnd;  // Select for wheel fine-tuning
            setMouseCursor(juce::MouseCursor::LeftRightResizeCursor);
            return;
        }
    }
    
    // Otherwise, seek to clicked position
    selectedHandle = None;  // Deselect any handle
    if (onPositionClicked)
    {
        double timelineSeconds = timelineSecondsAtX(event.x);
        onPositionClicked(timelineSeconds);
    }
}

void WaveformView::mouseDrag(const juce::MouseEvent& event)
{
    if (!projectState.hasAudio() || currentDragMode == None)
        return;
    
    int64 dragSample = sampleAtX(event.x);
    
    if (currentDragMode == TrimStart)
    {
        projectState.setTrimStart(dragSample);
    }
    else if (currentDragMode == LoopStart)
        projectState.setLoopStart(dragSample);
    else if (currentDragMode == LoopEnd)
        projectState.setLoopEnd(dragSample);
}

void WaveformView::mouseUp(const juce::MouseEvent& event)
{
    juce::ignoreUnused(event);

    currentDragMode = None;
    setMouseCursor(juce::MouseCursor::NormalCursor);
}

void WaveformView::mouseMove(const juce::MouseEvent& event)
{
    if (!projectState.hasAudio())
        return;
    
    auto bounds = getLocalBounds().reduced(4);
    
    // Check for trim start marker hover
    if (projectState.hasAudio())
    {
        int trimStartX = getTrimHandleX();
        bool nearTrim = std::abs(event.x - trimStartX) < 10;
        bool inHandleArea = event.y < (bounds.getY() + 20);
        
        if (nearTrim && inHandleArea)
        {
            setMouseCursor(juce::MouseCursor::LeftRightResizeCursor);
            return;
        }
        
        if (std::abs(event.x - trimStartX) < 5)
        {
            setMouseCursor(juce::MouseCursor::LeftRightResizeCursor);
            return;
        }
    }
    
    // Change cursor when hovering over loop markers
    if (projectState.hasLoopPoints())
    {
        int loopStartX = xAtSample(projectState.getLoopStart());
        int loopEndX = xAtSample(projectState.getLoopEnd());
        
        bool nearStart = std::abs(event.x - loopStartX) < 10;
        bool nearEnd = std::abs(event.x - loopEndX) < 10;
        bool inHandleArea = event.y < (bounds.getY() + 20);
        
        if ((nearStart || nearEnd) && inHandleArea)
        {
            setMouseCursor(juce::MouseCursor::LeftRightResizeCursor);
            return;
        }
        
        if (std::abs(event.x - loopStartX) < 5 || std::abs(event.x - loopEndX) < 5)
        {
            setMouseCursor(juce::MouseCursor::LeftRightResizeCursor);
            return;
        }
    }
    
    setMouseCursor(juce::MouseCursor::NormalCursor);
}

bool WaveformView::keyPressed(const juce::KeyPress& key)
{
    if (!projectState.hasAudio())
        return false;
    
    // Get current mouse position relative to this component
    auto mousePos = getMouseXYRelative();
    auto bounds = getLocalBounds().reduced(4);
    
    // Clamp mouse position to valid bounds (use center if outside)
    int clampedX = mousePos.x;
    if (clampedX < bounds.getX() || clampedX > bounds.getRight())
        clampedX = bounds.getCentreX();
    
    // T: Move trim start to mouse cursor
    if (key.getKeyCode() == 'T' || key.getKeyCode() == 't')
    {
        int64 sample = sampleAtX(clampedX);
        projectState.setTrimStart(sample);
        selectedHandle = TrimStart;
        repaint();
        return true;
    }
    
    // Z: Move loop start to mouse cursor
    if (key.getKeyCode() == 'Z' || key.getKeyCode() == 'z')
    {
        int64 sample = sampleAtX(clampedX);
        projectState.setLoopStart(sample);
        selectedHandle = LoopStart;
        repaint();
        return true;
    }
    
    // X: Move loop end to mouse cursor
    if (key.getKeyCode() == 'X' || key.getKeyCode() == 'x')
    {
        int64 sample = sampleAtX(clampedX);
        projectState.setLoopEnd(sample);
        selectedHandle = LoopEnd;
        repaint();
        return true;
    }
    
    return false;
}

void WaveformView::mouseWheelMove(const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel)
{
    // Check if event originated from scroll bar area
    auto scrollBarBounds = scrollBar.getBounds();
    if (scrollBarBounds.contains(event.getEventRelativeTo(this).getPosition()))
    {
        // Scroll bar handles its own events, don't interfere
        return;
    }
    
    // Zoom with Ctrl+mouse wheel
    if (event.mods.isCtrlDown())
    {
        zoomAccumulator += wheel.deltaY;
        const float zoomThreshold = 0.1f;
        
        if (std::abs(zoomAccumulator) >= zoomThreshold)
        {
            double zoomFactor = 1.0 + (zoomAccumulator * 5.0);
            zoomFactor = juce::jlimit(0.5, 2.0, zoomFactor);
            
            // Calculate mouse position as a ratio of the visible range
            auto bounds = getLocalBounds().reduced(4);
            double mouseRatio = static_cast<double>(event.x - bounds.getX()) / bounds.getWidth();
            mouseRatio = juce::jlimit(0.0, 1.0, mouseRatio);
            
            // Calculate the time position under the mouse cursor
            double timeUnderMouse = visibleStart + (visibleEnd - visibleStart) * mouseRatio;
            
            // Apply zoom
            double newZoomLevel = zoomLevel * zoomFactor;
            newZoomLevel = juce::jlimit(0.1, 100.0, newZoomLevel);
            
            double totalLength = projectState.hasAudio() ? 
                (projectState.getAudioBuffer().getNumSamples() / projectState.getSampleRate()) : 0.0;
            double newVisibleLength = totalLength / newZoomLevel;
            
            // Clamp visible length to not exceed total length
            newVisibleLength = juce::jlimit(totalLength * 0.001, totalLength, newVisibleLength);
            
            // Calculate new visible range to keep the time under mouse at the same ratio
            double newVisibleStart = timeUnderMouse - newVisibleLength * mouseRatio;
            newVisibleStart = juce::jlimit(0.0, juce::jmax(0.0, totalLength - newVisibleLength), newVisibleStart);
            
            // Update state
            zoomLevel = newZoomLevel;
            visibleStart = newVisibleStart;
            visibleEnd = visibleStart + newVisibleLength;
            
            scrollBar.setCurrentRange(visibleStart, newVisibleLength);
            repaint();
            zoomAccumulator = 0.0f;
        }
        return;
    }
    
    // Fine-tune selected handle with Shift+scroll
    if (event.mods.isShiftDown() && selectedHandle != None && (projectState.hasLoopPoints() || projectState.hasTrimStart()))
    {
        fineTuneAccumulator += wheel.deltaY;
        const float stepThreshold = 0.01f;
        
        while (fineTuneAccumulator >= stepThreshold)
        {
            if (selectedHandle == TrimStart)
            {
                int64 newPos = projectState.getTrimStart() + 100;
                projectState.setTrimStart(newPos);
            }
            else if (selectedHandle == LoopStart)
            {
                int64 newPos = projectState.getLoopStart() + 100;
                projectState.setLoopStart(newPos);
            }
            else if (selectedHandle == LoopEnd)
            {
                int64 newPos = projectState.getLoopEnd() + 100;
                projectState.setLoopEnd(newPos);
            }
            fineTuneAccumulator -= stepThreshold;
        }
        
        while (fineTuneAccumulator <= -stepThreshold)
        {
            if (selectedHandle == TrimStart)
            {
                int64 newPos = projectState.getTrimStart() - 100;
                // Ensure it doesn't go negative
                newPos = juce::jmax(int64(0), newPos);
                projectState.setTrimStart(newPos);
            }
            else if (selectedHandle == LoopStart)
            {
                int64 newPos = projectState.getLoopStart() - 100;
                projectState.setLoopStart(newPos);
            }
            else if (selectedHandle == LoopEnd)
            {
                int64 newPos = projectState.getLoopEnd() - 100;
                projectState.setLoopEnd(newPos);
            }
            fineTuneAccumulator += stepThreshold;
        }
    }
}

//==============================================================================
void WaveformView::changeListenerCallback(juce::ChangeBroadcaster* source)
{
    if (source == &projectState)
    {
        int64 currentNumSamples = projectState.hasAudio() ? projectState.getNumSamples() : 0;
        int64 currentEffectiveStart = projectState.getEffectivePlaybackStart();
        int64 currentPadding = projectState.getPaddingSamples();
        bool audioChanged = (currentNumSamples != lastNumSamples);
        bool effectiveChanged = (currentEffectiveStart != lastEffectiveStart);
        bool paddingChanged = (currentPadding != lastPaddingSamples);
        
        if (audioChanged || effectiveChanged || paddingChanged)
        {
            updateThumbnail();
        }
        else
        {
            repaint();
        }
    }
}

void WaveformView::scrollBarMoved(juce::ScrollBar* scrollBarThatHasMoved, double newRangeStart)
{
    if (scrollBarThatHasMoved != &scrollBar || !projectState.hasAudio())
        return;
    
    const double totalLength = getPlaybackLengthSeconds();
    const double visibleLength = totalLength / zoomLevel;
    
    // Clamp start
    visibleStart = juce::jlimit(0.0, totalLength - visibleLength, newRangeStart);
    visibleEnd = visibleStart + visibleLength;
    
    // Keep scrollbar in sync (but don't recompute from itself)
    scrollBar.setCurrentRange(visibleStart, visibleLength);
    
    repaint();
}

//==============================================================================
void WaveformView::setZoomLevel(double newZoom)
{
    zoomLevel = juce::jlimit(0.1, 100.0, newZoom);
    updateVisibleRange();
    repaint();
}

void WaveformView::setPlayPosition(double seconds)
{
    double adjustedSeconds = seconds;
    if (projectState.hasAudio() && projectState.getPaddingSamples() > 0 && projectState.getSampleRate() > 0.0)
    {
        double paddingSeconds = static_cast<double>(projectState.getPaddingSamples()) / projectState.getSampleRate();
        adjustedSeconds = juce::jmax(0.0, seconds - paddingSeconds);
    }
    playPosition = adjustedSeconds;
    
    // Auto-scroll to keep playhead visible
    if (autoScrollEnabled && projectState.hasAudio())
    {
        double visibleLength = visibleEnd - visibleStart;
        
        // If playhead is outside visible range, center it
        if (adjustedSeconds < visibleStart || adjustedSeconds > visibleEnd)
        {
            double newVisibleStart = adjustedSeconds - (visibleLength * 0.5);
            double totalLength = getPlaybackLengthSeconds();
            newVisibleStart = juce::jlimit(0.0, totalLength - visibleLength, newVisibleStart);
            
            // Only update if actually changing
            if (std::abs(newVisibleStart - visibleStart) > 0.001)
            {
                visibleStart = newVisibleStart;
                visibleEnd = visibleStart + visibleLength;
                scrollBar.setCurrentRange(visibleStart, visibleLength);
            }
        }
    }
    
    repaint();
}

//==============================================================================
void WaveformView::updateThumbnail()
{
    if (!projectState.hasAudio())
    {
        thumbnail.clear();
        visibleStart = 0.0;
        visibleEnd = 0.0;
        lastNumSamples = 0;
        lastEffectiveStart = 0;
        lastPaddingSamples = 0;
        lastThumbnailLengthSeconds = 0.0;
        return;
    }
    
    const auto& sourceBuffer = projectState.getAudioBuffer();
    int64 paddingSamples = projectState.getPaddingSamples();
    int64 effectiveStart = projectState.getEffectivePlaybackStart();
    int64 totalSamples = sourceBuffer.getNumSamples();
    paddingSamples = juce::jlimit<int64>(0, totalSamples, paddingSamples);
    effectiveStart = juce::jlimit<int64>(0, totalSamples, effectiveStart);
    
    // If we have padding, create a padded buffer for the thumbnail
    if (paddingSamples > 0)
    {
        // Calculate the effective audio region (from effectiveStart onwards)
        int sourceOffset = static_cast<int>(effectiveStart);
        int sourceLength = juce::jmax(0, sourceBuffer.getNumSamples() - sourceOffset);
        int totalLength = static_cast<int>(paddingSamples) + sourceLength;
        
        // Create temporary buffer with padding
        juce::AudioBuffer<float> paddedBuffer(sourceBuffer.getNumChannels(), totalLength);
        paddedBuffer.clear();
        
        // Copy audio after padding region
        for (int ch = 0; ch < sourceBuffer.getNumChannels(); ++ch)
        {
            paddedBuffer.copyFrom(ch, static_cast<int>(paddingSamples), 
                                sourceBuffer, ch, sourceOffset, sourceLength);
        }
        
        thumbnail.reset(projectState.getNumChannels(),
                       projectState.getSampleRate(),
                       totalLength);
        
        thumbnail.addBlock(0, paddedBuffer, 0, totalLength);
    }
    else
    {
        // No padding - use original buffer from effectiveStart
        int sourceOffset = static_cast<int>(effectiveStart);
        int sourceLength = juce::jmax(0, sourceBuffer.getNumSamples() - sourceOffset);
        
        thumbnail.reset(projectState.getNumChannels(),
                       projectState.getSampleRate(),
                       sourceLength);
        
        thumbnail.addBlock(0, sourceBuffer, sourceOffset, sourceLength);
    }
    
    // Initialize visible range to show entire waveform
    const double totalLengthSeconds = getPlaybackLengthSeconds();
    visibleStart = 0.0;
    visibleEnd = totalLengthSeconds / zoomLevel;
    
    updateVisibleRange();
    
    lastNumSamples = projectState.getNumSamples();
    lastEffectiveStart = effectiveStart;
    lastPaddingSamples = paddingSamples;
    lastThumbnailLengthSeconds = totalLengthSeconds;
}

void WaveformView::updateVisibleRange()
{
    if (!projectState.hasAudio())
        return;
    
    const double totalLength = getPlaybackLengthSeconds();
    double visibleLength = totalLength / zoomLevel;
    
    // Don't let visibleLength get silly
    const double minLength = totalLength * 0.001;
    visibleLength = juce::jlimit(minLength, totalLength, visibleLength);
    
    // Clamp start so the window stays inside the file
    visibleStart = juce::jlimit(0.0, totalLength - visibleLength, visibleStart);
    visibleEnd = visibleStart + visibleLength;
    
    // Scrollbar is in seconds too - setCurrentRange(start, size)
    scrollBar.setRangeLimits(0.0, totalLength);
    scrollBar.setCurrentRange(visibleStart, visibleLength);
}

double WaveformView::getPlaybackLengthSeconds() const
{
    double totalLength = thumbnail.getTotalLength();
    if (totalLength <= 0.0 && projectState.hasAudio())
        totalLength = projectState.getLengthInSeconds();
    return totalLength;
}

int64 WaveformView::sampleAtX(int x) const
{
    auto bounds = getLocalBounds().reduced(4);
    double ratio = (x - bounds.getX()) / static_cast<double>(bounds.getWidth());
    double seconds = visibleStart + ratio * (visibleEnd - visibleStart);
    int64 thumbnailSample = static_cast<int64>(seconds * projectState.getSampleRate());
    
    // Convert from thumbnail position to projectState position
    int64 effectiveStart = projectState.getEffectivePlaybackStart();
    int64 paddingSamples = projectState.getPaddingSamples();
    int64 projectSample = effectiveStart + thumbnailSample - paddingSamples;
    projectSample = juce::jlimit<int64>(0, projectState.getNumSamples(), projectSample);
    return projectSample;
}

int WaveformView::xAtSample(int64 sample) const
{
    auto bounds = getLocalBounds().reduced(4);
    
    // Convert from projectState position to thumbnail position
    int64 effectiveStart = projectState.getEffectivePlaybackStart();
    int64 paddingSamples = projectState.getPaddingSamples();
    int64 thumbnailSample = sample - effectiveStart + paddingSamples;
    thumbnailSample = juce::jmax<int64>(0, thumbnailSample);
    
    double seconds = thumbnailSample / projectState.getSampleRate();
    double range = juce::jmax(0.000001, visibleEnd - visibleStart);
    double ratio = (seconds - visibleStart) / range;
    return bounds.getX() + static_cast<int>(ratio * bounds.getWidth());
}

int WaveformView::getTrimHandleX() const
{
    int64 visualTrimSample = projectState.getTrimStart();
    if (projectState.getPaddingSamples() > 0)
        visualTrimSample -= projectState.getPaddingSamples(); // shift handle to padding start
    return xAtSample(visualTrimSample);
}

double WaveformView::timelineSecondsAtX(int x) const
{
    auto bounds = getLocalBounds().reduced(4);
    double ratio = (x - bounds.getX()) / static_cast<double>(bounds.getWidth());
    ratio = juce::jlimit(0.0, 1.0, ratio);
    double range = visibleEnd - visibleStart;
    return visibleStart + (range * ratio);
}

void WaveformView::drawHotkeyLegend(juce::Graphics& g, const juce::Rectangle<int>& waveformBounds) const
{
    // Keep these in ASCII to avoid platform encoding surprises.
    static constexpr const char* legendLines[] = {
        "CTRL+Scroll = Zoom",
        "Z = Add Loop Start Point",
        "X = Add Loop End Point",
        "Space = Play/Pause"
    };
    constexpr int legendLineCount = static_cast<int>(sizeof(legendLines) / sizeof(legendLines[0]));
    auto legendFont = g.getCurrentFont().withHeight(11.0f);
    g.setFont(legendFont);
    int lineHeight = static_cast<int>(legendFont.getHeight()) + 2;

    int maxLineWidth = 0;
    for (int i = 0; i < legendLineCount; ++i)
        maxLineWidth = juce::jmax(maxLineWidth, legendFont.getStringWidth(legendLines[i]));

    int availableWidth = juce::jmax(80, waveformBounds.getWidth() - 20);
    int legendWidth = juce::jmin(maxLineWidth + 20, availableWidth);
    int legendHeight = lineHeight * legendLineCount + 8;

    auto legendBounds = juce::Rectangle<int>(
        waveformBounds.getRight() - legendWidth - 10,
        waveformBounds.getBottom() - legendHeight - 10,
        legendWidth,
        legendHeight);

    g.setColour(CustomLookAndFeel::darkControl.withAlpha(0.85f));
    g.fillRoundedRectangle(legendBounds.toFloat(), 6.0f);

    g.setColour(CustomLookAndFeel::textColor);
    int textY = legendBounds.getY() + 4;
    for (int i = 0; i < legendLineCount; ++i)
    {
        g.drawText(legendLines[i],
                   legendBounds.getX() + 8,
                   textY,
                   legendBounds.getWidth() - 16,
                   lineHeight,
                   juce::Justification::left,
                   false);
        textY += lineHeight;
    }
}
