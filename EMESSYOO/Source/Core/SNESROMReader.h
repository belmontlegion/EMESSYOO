#pragma once

#include <JuceHeader.h>

//==============================================================================
/**
 * Reader for SNES ROM files (.sfc, .smc) to extract metadata.
 * Based on the SNES ROM header format specification.
 */
class SNESROMReader
{
public:
    //==============================================================================
    SNESROMReader();
    ~SNESROMReader();
    
    //==============================================================================
    /** Load and parse a SNES ROM file */
    bool loadROMFile(const juce::File& romFile);
    
    /** Get the game title from the ROM header */
    juce::String getGameTitle() const { return gameTitle; }
    
    /** Get the ROM region/country */
    juce::String getRegion() const { return region; }
    
    /** Check if ROM data was loaded successfully */
    bool isLoaded() const { return loaded; }
    
private:
    //==============================================================================
    enum class HeaderOffset : int
    {
        LoROM = 0x7FB0,
        ExLoROM = 0x407FB0,
        HiROM = 0xFFB0,
        ExHiROM = 0x40FFB0
    };
    
    enum class HeaderValue : int
    {
        Title = 0x10,
        MapMode = 0x25,
        Type = 0x26,
        Size = 0x27,
        SRAM = 0x28,
        Country = 0x29,
        Company = 0x2A,
        Version = 0x2B,
        InverseChecksum = 0x2C,
        Checksum = 0x2E
    };
    
    //==============================================================================
    bool loaded = false;
    juce::String gameTitle;
    juce::String region;
    int headerOffset = 0;
    bool hasSMCHeader = false;
    
    //==============================================================================
    bool detectSMCHeader(const juce::MemoryBlock& romData);
    int findHeaderOffset(const juce::MemoryBlock& romData);
    int calculateMapModeScore(const juce::MemoryBlock& romData, int offset);
    juce::String extractTitle(const juce::MemoryBlock& romData, int offset);
    juce::String getRegionFromCountryCode(uint8_t countryCode);
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SNESROMReader)
};
