#include "SNESROMReader.h"

//==============================================================================
SNESROMReader::SNESROMReader()
{
}

SNESROMReader::~SNESROMReader()
{
}

//==============================================================================
bool SNESROMReader::loadROMFile(const juce::File& romFile)
{
    loaded = false;
    gameTitle = "";
    region = "";
    
    if (!romFile.existsAsFile())
        return false;
    
    // Check file size before reading
    auto fileSize = romFile.getSize();
    if (fileSize < 0x20000 || fileSize > 0x1000000) // Min 128KB, Max 16MB
        return false;
    
    // Read ROM file into memory
    juce::FileInputStream stream(romFile);
    if (!stream.openedOk())
        return false;
    
    juce::MemoryBlock romData;
    
    try
    {
        stream.readIntoMemoryBlock(romData);
    }
    catch (...)
    {
        return false;
    }
    
    if (romData.getSize() < 0x20000 || romData.getData() == nullptr)
        return false;
    
    // Detect and skip SMC header if present
    hasSMCHeader = detectSMCHeader(romData);
    if (hasSMCHeader && romData.getSize() > 512)
    {
        // Remove 512-byte header by creating new block
        try
        {
            juce::MemoryBlock temp;
            temp.append(static_cast<const uint8_t*>(romData.getData()) + 512, romData.getSize() - 512);
            romData = temp;
        }
        catch (...)
        {
            return false;
        }
    }
    
    // Validate data pointer after potential modification
    if (romData.getData() == nullptr || romData.getSize() < 0x10000)
        return false;
    
    // Find the ROM header offset (LoROM, HiROM, ExLoROM, or ExHiROM)
    headerOffset = findHeaderOffset(romData);
    
    if (headerOffset < 0 || headerOffset >= romData.getSize())
        return false;
    
    // Extract title and region
    gameTitle = extractTitle(romData, headerOffset);
    
    // Get country code
    if (headerOffset + static_cast<int>(HeaderValue::Country) < romData.getSize())
    {
        uint8_t countryCode = static_cast<const uint8_t*>(romData.getData())[headerOffset + static_cast<int>(HeaderValue::Country)];
        region = getRegionFromCountryCode(countryCode);
    }
    
    loaded = true;
    return true;
}

//==============================================================================
bool SNESROMReader::detectSMCHeader(const juce::MemoryBlock& romData)
{
    // SMC header is 512 bytes, check if file size is N*1024 + 512
    auto size = romData.getSize();
    return (size % 1024) == 512;
}

int SNESROMReader::findHeaderOffset(const juce::MemoryBlock& romData)
{
    // Calculate map mode scores for each possible header location
    int scoreLoROM = calculateMapModeScore(romData, static_cast<int>(HeaderOffset::LoROM));
    int scoreHiROM = calculateMapModeScore(romData, static_cast<int>(HeaderOffset::HiROM));
    int scoreExLoROM = calculateMapModeScore(romData, static_cast<int>(HeaderOffset::ExLoROM));
    int scoreExHiROM = calculateMapModeScore(romData, static_cast<int>(HeaderOffset::ExHiROM));
    
    // Find the best match
    int bestScore = scoreLoROM;
    int bestOffset = static_cast<int>(HeaderOffset::LoROM);
    
    if (scoreHiROM > bestScore)
    {
        bestScore = scoreHiROM;
        bestOffset = static_cast<int>(HeaderOffset::HiROM);
    }
    
    if (scoreExLoROM > bestScore)
    {
        bestScore = scoreExLoROM;
        bestOffset = static_cast<int>(HeaderOffset::ExLoROM);
    }
    
    if (scoreExHiROM > bestScore)
    {
        bestScore = scoreExHiROM;
        bestOffset = static_cast<int>(HeaderOffset::ExHiROM);
    }
    
    return bestOffset;
}

int SNESROMReader::calculateMapModeScore(const juce::MemoryBlock& romData, int offset)
{
    if (offset < 0 || offset + 80 > romData.getSize())
        return 0;
    
    const uint8_t* data = static_cast<const uint8_t*>(romData.getData()) + offset;
    int score = 0;
    
    // Check map mode byte
    uint8_t mapMode = data[static_cast<int>(HeaderValue::MapMode)];
    if ((mapMode & 0x0F) <= 0x05) // Valid map mode
        score += 2;
    
    // Check ROM type
    uint8_t romType = data[static_cast<int>(HeaderValue::Type)];
    if (romType < 0x08 || (romType >= 0x10 && romType <= 0x36))
        score += 2;
    
    // Check ROM size
    uint8_t romSize = data[static_cast<int>(HeaderValue::Size)];
    if (romSize >= 0x07 && romSize <= 0x0E)
        score += 2;
    
    // Check country code
    uint8_t country = data[static_cast<int>(HeaderValue::Country)];
    if (country <= 0x0F)
        score += 2;
    
    // Check company code
    uint8_t company = data[static_cast<int>(HeaderValue::Company)];
    if (company == 0x33 || (company >= 0x00 && company <= 0x99))
        score += 1;
    
    // Check checksum complement
    uint16_t checksum = (data[static_cast<int>(HeaderValue::Checksum) + 1] << 8) | 
                        data[static_cast<int>(HeaderValue::Checksum)];
    uint16_t invChecksum = (data[static_cast<int>(HeaderValue::InverseChecksum) + 1] << 8) | 
                           data[static_cast<int>(HeaderValue::InverseChecksum)];
    
    if ((checksum ^ invChecksum) == 0xFFFF)
        score += 4;
    
    // Check title for printable characters
    int printableChars = 0;
    for (int i = 0; i < 21; ++i)
    {
        uint8_t c = data[static_cast<int>(HeaderValue::Title) + i];
        if ((c >= 0x20 && c <= 0x7E) || c == 0x00)
            printableChars++;
    }
    
    if (printableChars >= 15) // Most characters should be printable
        score += 2;
    
    return score;
}

juce::String SNESROMReader::extractTitle(const juce::MemoryBlock& romData, int offset)
{
    if (offset < 0 || offset + static_cast<int>(HeaderValue::Title) + 21 > romData.getSize())
        return "Unknown";
    
    const uint8_t* data = static_cast<const uint8_t*>(romData.getData()) + offset;
    const uint8_t* titleData = data + static_cast<int>(HeaderValue::Title);
    
    // Title is 21 bytes, but often padded with spaces or nulls
    char titleBuffer[22] = {0};
    int titleLength = 0;
    
    for (int i = 0; i < 21; ++i)
    {
        uint8_t c = titleData[i];
        
        // Stop at null terminator
        if (c == 0x00)
            break;
        
        // Keep printable ASCII characters
        if (c >= 0x20 && c <= 0x7E)
        {
            titleBuffer[titleLength++] = static_cast<char>(c);
        }
    }
    
    // Trim trailing spaces
    while (titleLength > 0 && titleBuffer[titleLength - 1] == ' ')
        titleLength--;
    
    titleBuffer[titleLength] = '\0';
    
    return juce::String(titleBuffer).trim();
}

juce::String SNESROMReader::getRegionFromCountryCode(uint8_t countryCode)
{
    switch (countryCode)
    {
        case 0x00: return "Japan";
        case 0x01: return "USA";
        case 0x02: return "Europe";
        case 0x03: return "Sweden";
        case 0x04: return "Finland";
        case 0x05: return "Denmark";
        case 0x06: return "France";
        case 0x07: return "Netherlands";
        case 0x08: return "Spain";
        case 0x09: return "Germany";
        case 0x0A: return "Italy";
        case 0x0B: return "China";
        case 0x0C: return "Indonesia";
        case 0x0D: return "Korea";
        case 0x0E: return "Global";
        case 0x0F: return "Canada";
        default: return "Unknown";
    }
}
