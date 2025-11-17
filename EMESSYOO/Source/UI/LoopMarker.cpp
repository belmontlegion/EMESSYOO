#include "LoopMarker.h"
#include "CustomLookAndFeel.h"

//==============================================================================
LoopMarker::LoopMarker(MarkerType type)
    : markerType(type)
{
    setSize(12, 40);
}

LoopMarker::~LoopMarker()
{
}

//==============================================================================
void LoopMarker::paint(juce::Graphics& g)
{
    auto colour = (markerType == LoopStart) 
                  ? CustomLookAndFeel::greenAccentBright 
                  : juce::Colours::orange;
    
    // Draw marker handle
    g.setColour(colour);
    
    auto bounds = getLocalBounds().toFloat();
    
    // Draw vertical line
    g.fillRect(bounds.withWidth(2.0f));
    
    // Draw handle at top
    juce::Path handle;
    handle.addTriangle(0.0f, 0.0f, 
                      bounds.getWidth(), 0.0f, 
                      bounds.getWidth() / 2, 10.0f);
    g.fillPath(handle);
}

void LoopMarker::mouseDown(const juce::MouseEvent& event)
{
    juce::ignoreUnused(event);

    // Start drag
}

void LoopMarker::mouseDrag(const juce::MouseEvent& event)
{
    if (onPositionChanged)
    {
        onPositionChanged(getX() + event.getDistanceFromDragStartX());
    }
}
