#include "MSUManifestUpdater.h"

namespace
{
    juce::String extractTrackNumber(const juce::String& pcmFileName)
    {
        auto baseName = pcmFileName.upToLastOccurrenceOf(".pcm", false, true);
        auto lastDash = baseName.lastIndexOf("-");
        if (lastDash >= 0)
            return baseName.substring(lastDash + 1);
        return {};
    }

    juce::String extractPCMBaseName(const juce::File& pcmFile)
    {
        auto fileName = pcmFile.getFileNameWithoutExtension();
        auto lastDash = fileName.lastIndexOf("-");
        return (lastDash >= 0) ? fileName.substring(0, lastDash) : fileName;
    }
}

//==============================================================================
MSUManifestUpdater::MSUManifestUpdater()
{
}

MSUManifestUpdater::~MSUManifestUpdater()
{
}

//==============================================================================
bool MSUManifestUpdater::updateManifest(const juce::File& manifestFile,
                                       const juce::String& pcmFileName,
                                       int64 loopStartSample,
                                       int64 loopEndSample)
{
    auto extension = manifestFile.getFileExtension().toLowerCase();

    if (extension == ".bml")
        return updateBMLManifest(manifestFile, pcmFileName, loopStartSample, loopEndSample);

    return updateMSUTextManifest(manifestFile, pcmFileName, loopStartSample, loopEndSample);
}

bool MSUManifestUpdater::updateMSUTextManifest(const juce::File& msuFile,
                                               const juce::String& pcmFileName,
                                               int64 loopStartSample,
                                               int64 loopEndSample)
{
    if (!msuFile.existsAsFile())
    {
        lastError = "MSU file does not exist: " + msuFile.getFullPathName();
        return false;
    }

    juce::StringArray lines;
    msuFile.readLines(lines);

    juce::String trackNumber = extractTrackNumber(pcmFileName);
    if (trackNumber.isEmpty())
        trackNumber = "unknown";

    bool foundTrack = false;
    juce::String trackMarker = "track " + trackNumber;
    juce::String loopMarker = "loop";

    for (int i = 0; i < lines.size(); ++i)
    {
        auto line = lines[i].toLowerCase().trim();

        if (line.contains(trackMarker) || line.contains(pcmFileName.toLowerCase()))
        {
            foundTrack = true;

            for (int j = i; j < juce::jmin(i + 5, lines.size()); ++j)
            {
                auto nextLine = lines[j].toLowerCase();
                if (nextLine.contains(loopMarker))
                {
                    juce::String loopLine = "  loop " + juce::String(loopStartSample);
                    if (loopEndSample >= 0)
                        loopLine += " " + juce::String(loopEndSample);
                    lines.set(j, loopLine);

                    if (msuFile.replaceWithText(lines.joinIntoString("\n")))
                        return true;

                    lastError = "Failed to write to MSU file";
                    return false;
                }
            }

            juce::String loopLine = "  loop " + juce::String(loopStartSample);
            if (loopEndSample >= 0)
                loopLine += " " + juce::String(loopEndSample);
            lines.insert(i + 1, loopLine);

            if (msuFile.replaceWithText(lines.joinIntoString("\n")))
                return true;

            lastError = "Failed to write to MSU file";
            return false;
        }
    }

    if (!foundTrack)
    {
    lines.add("");
        lines.add("track " + trackNumber + " " + pcmFileName);
        juce::String loopLine = "  loop " + juce::String(loopStartSample);
        if (loopEndSample >= 0)
            loopLine += " " + juce::String(loopEndSample);
        lines.add(loopLine);

        if (msuFile.replaceWithText(lines.joinIntoString("\n")))
            return true;

        lastError = "Failed to write to MSU file";
        return false;
    }

    return true;
}

bool MSUManifestUpdater::updateBMLManifest(const juce::File& bmlFile,
                                           const juce::String& pcmFileName,
                                           int64 loopStartSample,
                                           int64 loopEndSample)
{
    if (!bmlFile.existsAsFile())
    {
        lastError = "BML file does not exist: " + bmlFile.getFullPathName();
        return false;
    }

    juce::String contents = bmlFile.loadFileAsString();
    auto lowerContents = contents.toLowerCase();
    auto searchName = pcmFileName.toLowerCase();
    auto trackNumber = extractTrackNumber(pcmFileName);

    auto formatLoopValue = [&]() -> juce::String
    {
        juce::String value = juce::String(loopStartSample);
        if (loopEndSample >= 0)
            value += " " + juce::String(loopEndSample);
        return value;
    };

    auto writeUpdatedContents = [&](const juce::String& updated)
    {
        if (bmlFile.replaceWithText(updated))
            return true;

        lastError = "Failed to write to BML file";
        return false;
    };

    int trackStart = lowerContents.indexOf(searchName);

    auto tryFindTrackToken = [&](const juce::String& token)
    {
        if (trackStart < 0)
            trackStart = lowerContents.indexOf(token.toLowerCase());
    };

    if (trackStart < 0 && trackNumber.isNotEmpty())
    {
        tryFindTrackToken("number=" + trackNumber);
        tryFindTrackToken("track-" + trackNumber);

        if (trackNumber.getIntValue() > 0)
        {
            auto padded = juce::String::formatted("%02d", trackNumber.getIntValue());
            tryFindTrackToken("track " + padded);
            tryFindTrackToken("number=" + padded);
        }
    }

    auto insertLoopLine = [&](int blockEndIndex, juce::String updatedContents)
    {
        juce::String indent("    ");
        bool needsLeadingNewline = blockEndIndex > 0 && updatedContents[blockEndIndex - 1] != '\n';
        juce::String insertion = (needsLeadingNewline ? "\n" : "") + indent + "loop=" + formatLoopValue() + "\n";
        updatedContents = updatedContents.replaceSection(blockEndIndex, 0, insertion);
        return writeUpdatedContents(updatedContents);
    };

    if (trackStart >= 0)
    {
        int blockStart = contents.substring(0, trackStart).lastIndexOfChar('{');
        if (blockStart < 0)
            blockStart = trackStart;
        int blockEnd = contents.indexOfChar(trackStart, '}');
        if (blockEnd < 0)
            blockEnd = contents.length();

        int loopIndex = -1;
        if (trackStart < lowerContents.length())
        {
            auto searchSection = lowerContents.substring(trackStart, blockEnd);
            int relativeIndex = searchSection.indexOf("loop");
            if (relativeIndex >= 0)
                loopIndex = trackStart + relativeIndex;
        }

        if (loopIndex >= 0 && loopIndex < blockEnd)
        {
            auto prefix = contents.substring(0, loopIndex);
            int lineStart = prefix.lastIndexOfChar('\n');
            if (lineStart < 0)
                lineStart = 0;
            else
                ++lineStart;

            int lineEnd = contents.indexOfChar(loopIndex, '\n');
            bool hasNewline = (lineEnd >= 0);
            if (hasNewline)
                ++lineEnd;
            else
                lineEnd = contents.length();

            juce::String indent;
            for (int idx = lineStart; idx < loopIndex; ++idx)
            {
                auto ch = contents[idx];
                if (ch == ' ' || ch == '\t')
                    indent << ch;
                else
                    break;
            }

            if (indent.isEmpty())
                indent = "    ";

            juce::String replacement = indent + "loop=" + formatLoopValue();
            if (hasNewline)
                replacement += "\n";

            contents = contents.replaceSection(lineStart, lineEnd - lineStart, replacement);
            return writeUpdatedContents(contents);
        }

        return insertLoopLine(blockEnd, contents);
    }

    juce::String newTrackBlock = "\n  track\n  {\n";
    if (trackNumber.isNotEmpty())
        newTrackBlock += "    number=" + trackNumber + "\n";
    newTrackBlock += "    filename=\"" + pcmFileName + "\"\n";
    newTrackBlock += "    loop=" + formatLoopValue() + "\n";
    newTrackBlock += "  }\n";

    int insertPos = contents.lastIndexOfChar('}');
    if (insertPos < 0)
        insertPos = contents.length();

    contents = contents.replaceSection(insertPos, 0, newTrackBlock);
    return writeUpdatedContents(contents);
}

bool MSUManifestUpdater::readLoopPoint(const juce::File& msuFile,
                                      const juce::String& pcmFileName,
                                      int64& loopStartSample,
                                      int64* loopEndSample)
{
    if (!msuFile.existsAsFile())
    {
        lastError = "MSU file does not exist: " + msuFile.getFullPathName();
        return false;
    }
    
    // Read manifest content
    juce::StringArray lines;
    msuFile.readLines(lines);
    
    DBG("Reading MSU manifest: " + msuFile.getFullPathName());
    DBG("Looking for PCM file: " + pcmFileName);
    DBG("Total lines in manifest: " + juce::String(lines.size()));
    
    // Extract track number from PCM filename (e.g., "game-1.pcm" -> "1")
    juce::String trackNumber;
    auto baseName = pcmFileName.upToLastOccurrenceOf(".pcm", false, true);
    auto lastDash = baseName.lastIndexOf("-");
    if (lastDash >= 0)
        trackNumber = baseName.substring(lastDash + 1);
    else
        trackNumber = "unknown";
    
    DBG("Extracted track number: " + trackNumber);
    
    // Look for track entry
    juce::String trackMarker = "track " + trackNumber;
    juce::String loopMarker = "loop";
    
    for (int i = 0; i < lines.size(); ++i)
    {
        auto line = lines[i].toLowerCase().trim();
        
        DBG("Checking line " + juce::String(i) + ": " + line);
        
        // Check if this line references our track
        if (line.contains(trackMarker) || line.contains(pcmFileName.toLowerCase()))
        {
            DBG("Found track reference at line " + juce::String(i));
            
            // Look ahead for loop info in the next few lines
            for (int j = i; j < juce::jmin(i + 5, lines.size()); ++j)
            {
                auto nextLine = lines[j].trim();
                DBG("  Checking for loop in line " + juce::String(j) + ": " + nextLine);
                
                if (nextLine.toLowerCase().contains(loopMarker))
                {
                    // Extract loop value(s) - format: "loop START [END]"
                    auto loopLine = nextLine.fromFirstOccurrenceOf("loop", false, true).trim();
                    auto tokens = juce::StringArray::fromTokens(loopLine, " \t", "");
                    
                    if (tokens.size() >= 1)
                    {
                        loopStartSample = tokens[0].getLargeIntValue();
                        DBG("Found loop start: " + juce::String(loopStartSample));
                        
                        if (tokens.size() >= 2 && loopEndSample != nullptr)
                        {
                            *loopEndSample = tokens[1].getLargeIntValue();
                            DBG("Found loop end: " + juce::String(*loopEndSample));
                        }
                        
                        return true;
                    }
                }
            }
        }
    }
    
    lastError = "Loop point not found for " + pcmFileName + " in MSU file";
    return false;
}

juce::File MSUManifestUpdater::findMSUFile(const juce::File& pcmFile)
{
    auto manifests = findRelatedManifestFiles(pcmFile);
    for (auto& manifest : manifests)
    {
        if (manifest.hasFileExtension(".msu"))
            return manifest;
    }

    return juce::File();
}

juce::Array<juce::File> MSUManifestUpdater::findRelatedManifestFiles(const juce::File& pcmFile)
{
    juce::Array<juce::File> results;

    auto directory = pcmFile.getParentDirectory();
    if (!directory.isDirectory())
        return results;

    const auto baseName = extractPCMBaseName(pcmFile);

    auto addIfValid = [&results](const juce::File& file)
    {
        if (file.existsAsFile() && !results.contains(file))
            results.add(file);
    };

    if (baseName.isNotEmpty())
    {
        addIfValid(directory.getChildFile(baseName + ".msu"));
        addIfValid(directory.getChildFile(baseName + ".bml"));
    }

    auto msuFiles = directory.findChildFiles(juce::File::findFiles, false, "*.msu");
    for (auto& file : msuFiles)
        addIfValid(file);

    auto bmlFiles = directory.findChildFiles(juce::File::findFiles, false, "*.bml");
    for (auto& file : bmlFiles)
        addIfValid(file);

    return results;
}

bool MSUManifestUpdater::captureMetadataSnapshot(const juce::File& manifestFile,
                                                 const juce::String& pcmFileName,
                                                 MetadataSnapshot& snapshot)
{
    if (!manifestFile.existsAsFile())
        return false;

    snapshot = MetadataSnapshot{};
    auto extension = manifestFile.getFileExtension().toLowerCase();
    if (extension == ".bml")
        return captureBMLMetadata(manifestFile, pcmFileName, snapshot);

    return captureMSUMetadata(manifestFile, pcmFileName, snapshot);
}

bool MSUManifestUpdater::restoreMetadataSnapshot(const juce::File& manifestFile,
                                                 const juce::String& pcmFileName,
                                                 const MetadataSnapshot& snapshot)
{
    if (!manifestFile.existsAsFile())
    {
        lastError = "Manifest file does not exist: " + manifestFile.getFullPathName();
        return false;
    }

    auto extension = manifestFile.getFileExtension().toLowerCase();
    if (extension == ".bml")
        return restoreBMLMetadata(manifestFile, pcmFileName, snapshot);

    return restoreMSUMetadata(manifestFile, pcmFileName, snapshot);
}

bool MSUManifestUpdater::captureMSUMetadata(const juce::File& msuFile,
                                            const juce::String& pcmFileName,
                                            MetadataSnapshot& snapshot)
{
    if (!msuFile.existsAsFile())
        return false;

    juce::StringArray lines;
    msuFile.readLines(lines);

    auto pcmLower = pcmFileName.toLowerCase();
    auto trackNumber = extractTrackNumber(pcmFileName);
    juce::String trackMarker = trackNumber.isNotEmpty()
        ? ("track " + trackNumber)
        : juce::String();

    for (int i = 0; i < lines.size(); ++i)
    {
        auto trimmed = lines[i].trim();
        auto lower = trimmed.toLowerCase();
        if ((trackMarker.isNotEmpty() && lower.contains(trackMarker)) || lower.contains(pcmLower))
        {
            snapshot.trackExisted = true;

            for (int j = i; j < juce::jmin(i + 5, lines.size()); ++j)
            {
                auto nextLine = lines[j].trim();
                auto nextLower = nextLine.toLowerCase();
                if (nextLower.contains("loop"))
                {
                    auto loopSection = nextLine.fromFirstOccurrenceOf("loop", false, true).trim();
                    auto tokens = juce::StringArray::fromTokens(loopSection, " \t", "");
                    if (tokens.size() >= 1)
                    {
                        snapshot.loopExisted = true;
                        snapshot.loopStart = tokens[0].getLargeIntValue();
                        if (tokens.size() >= 2)
                            snapshot.loopEnd = tokens[1].getLargeIntValue();
                    }
                    break;
                }
            }
            break;
        }
    }

    return true;
}

bool MSUManifestUpdater::captureBMLMetadata(const juce::File& bmlFile,
                                            const juce::String& pcmFileName,
                                            MetadataSnapshot& snapshot)
{
    if (!bmlFile.existsAsFile())
        return false;

    auto contents = bmlFile.loadFileAsString();
    auto lower = contents.toLowerCase();
    auto searchName = juce::String("filename=\"") + pcmFileName.toLowerCase() + "\"";
    int blockRef = lower.indexOf(searchName);

    auto trackNumber = extractTrackNumber(pcmFileName);
    if (blockRef < 0 && trackNumber.isNotEmpty())
    {
        blockRef = lower.indexOf("number=" + trackNumber.toLowerCase());
        if (blockRef < 0)
        {
            auto padded = juce::String::formatted("%02d", trackNumber.getIntValue());
            blockRef = lower.indexOf("number=" + padded.toLowerCase());
        }
    }

    if (blockRef < 0)
        return true;

    snapshot.trackExisted = true;

    int blockStart = -1;
    for (int i = blockRef; i >= 0; --i)
    {
        if (contents[i] == '{')
        {
            blockStart = i;
            break;
        }
    }

    if (blockStart < 0)
        return true;

    int depth = 0;
    int blockEnd = -1;
    for (int i = blockStart; i < contents.length(); ++i)
    {
        auto ch = contents[i];
        if (ch == '{')
            ++depth;
        else if (ch == '}')
        {
            --depth;
            if (depth == 0)
            {
                blockEnd = i;
                break;
            }
        }
    }

    if (blockEnd < 0)
        return true;

    auto block = contents.substring(blockStart, blockEnd);
    auto blockLower = block.toLowerCase();
    int loopIndex = blockLower.indexOf("loop=");
    if (loopIndex >= 0)
    {
        snapshot.loopExisted = true;
        int valueStart = loopIndex + juce::String("loop=").length();

        int valueEnd = blockLower.indexOfChar(loopIndex, '\n');
        if (valueEnd < 0)
            valueEnd = block.length();

        auto valueText = contents.substring(blockStart + valueStart, blockStart + valueEnd).trim();
        auto tokens = juce::StringArray::fromTokens(valueText, " \t", "");
        if (tokens.size() >= 1)
        {
            snapshot.loopStart = tokens[0].getLargeIntValue();
            if (tokens.size() >= 2)
                snapshot.loopEnd = tokens[1].getLargeIntValue();
        }
    }

    return true;
}

bool MSUManifestUpdater::restoreMSUMetadata(const juce::File& msuFile,
                                            const juce::String& pcmFileName,
                                            const MetadataSnapshot& snapshot)
{
    if (!snapshot.trackExisted)
    {
        juce::StringArray lines;
        msuFile.readLines(lines);
        auto trackNumber = extractTrackNumber(pcmFileName);
        if (removeTrackEntryFromMSU(lines, pcmFileName, trackNumber))
            return msuFile.replaceWithText(lines.joinIntoString("\n"));
        return true;
    }

    if (snapshot.loopExisted)
        return updateMSUTextManifest(msuFile, pcmFileName, snapshot.loopStart, snapshot.loopEnd);

    juce::StringArray lines;
    msuFile.readLines(lines);
    auto trackNumber = extractTrackNumber(pcmFileName);
    if (removeLoopEntryFromMSU(lines, pcmFileName, trackNumber))
        return msuFile.replaceWithText(lines.joinIntoString("\n"));

    return true;
}

bool MSUManifestUpdater::restoreBMLMetadata(const juce::File& bmlFile,
                                            const juce::String& pcmFileName,
                                            const MetadataSnapshot& snapshot)
{
    auto contents = bmlFile.loadFileAsString();
    auto lower = contents.toLowerCase();
    auto searchName = juce::String("filename=\"") + pcmFileName.toLowerCase() + "\"";
    int filenameIndex = lower.indexOf(searchName);

    if (filenameIndex < 0)
    {
        if (!snapshot.trackExisted)
            return true;
        if (snapshot.loopExisted)
            return updateBMLManifest(bmlFile, pcmFileName, snapshot.loopStart, snapshot.loopEnd);
        return true;
    }

    int blockStart = -1;
    for (int i = filenameIndex; i >= 0; --i)
    {
        if (contents[i] == '{')
        {
            blockStart = i;
            break;
        }
    }

    if (blockStart < 0)
        return true;

    int depth = 0;
    int blockEnd = -1;
    for (int i = blockStart; i < contents.length(); ++i)
    {
        auto ch = contents[i];
        if (ch == '{')
            ++depth;
        else if (ch == '}')
        {
            --depth;
            if (depth == 0)
            {
                blockEnd = i;
                break;
            }
        }
    }

    if (blockEnd < 0)
        return true;

    if (!snapshot.trackExisted)
    {
        if (removeTrackEntryFromBML(contents, pcmFileName))
            return bmlFile.replaceWithText(contents);
        return true;
    }

    if (snapshot.loopExisted)
        return updateBMLManifest(bmlFile, pcmFileName, snapshot.loopStart, snapshot.loopEnd);

    if (removeLoopEntryFromBML(contents, pcmFileName, blockStart, blockEnd))
        return bmlFile.replaceWithText(contents);

    return true;
}

bool MSUManifestUpdater::removeLoopEntryFromMSU(juce::StringArray& lines,
                                                const juce::String& pcmFileName,
                                                const juce::String& trackNumber)
{
    auto pcmLower = pcmFileName.toLowerCase();
    auto trackMarker = trackNumber.isNotEmpty() ? ("track " + trackNumber) : juce::String();

    for (int i = 0; i < lines.size(); ++i)
    {
        auto lower = lines[i].toLowerCase();
        if ((trackMarker.isNotEmpty() && lower.contains(trackMarker)) || lower.contains(pcmLower))
        {
            for (int j = i; j < juce::jmin(i + 5, lines.size()); ++j)
            {
                auto nextLower = lines[j].toLowerCase();
                if (nextLower.contains("loop"))
                {
                    lines.remove(j);
                    return true;
                }
                if (nextLower.startsWith("track ") && j > i)
                    break;
            }
            break;
        }
    }
    return false;
}

bool MSUManifestUpdater::removeTrackEntryFromMSU(juce::StringArray& lines,
                                                 const juce::String& pcmFileName,
                                                 const juce::String& trackNumber)
{
    auto pcmLower = pcmFileName.toLowerCase();
    auto trackMarker = trackNumber.isNotEmpty() ? ("track " + trackNumber) : juce::String();

    for (int i = 0; i < lines.size(); ++i)
    {
        auto lower = lines[i].toLowerCase();
        if ((trackMarker.isNotEmpty() && lower.contains(trackMarker)) || lower.contains(pcmLower))
        {
            int start = i;
            if (start > 0 && lines[start - 1].trim().isEmpty())
                --start;

            int end = i;
            for (int j = i + 1; j < lines.size(); ++j)
            {
                auto nextLower = lines[j].toLowerCase();
                if (nextLower.startsWith("track "))
                    break;
                end = j;
                if (lines[j].trim().isEmpty())
                    break;
            }

            lines.removeRange(start, end - start + 1);
            return true;
        }
    }
    return false;
}

bool MSUManifestUpdater::removeLoopEntryFromBML(juce::String& contents,
                                                const juce::String& pcmFileName,
                                                int blockStart,
                                                int blockEnd)
{
    juce::ignoreUnused(pcmFileName);
    auto block = contents.substring(blockStart, blockEnd);
    auto blockLower = block.toLowerCase();
    int loopPos = blockLower.indexOf("loop=");
    if (loopPos < 0)
        return false;

    int absoluteLoopPos = blockStart + loopPos;
    int lineStart = absoluteLoopPos;
    while (lineStart > 0 && contents[lineStart - 1] != '\n')
        --lineStart;

    int lineEnd = absoluteLoopPos;
    while (lineEnd < contents.length() && contents[lineEnd] != '\n')
        ++lineEnd;
    if (lineEnd < contents.length())
        ++lineEnd;

    contents = contents.replaceSection(lineStart, lineEnd - lineStart, juce::String());
    return true;
}

bool MSUManifestUpdater::removeTrackEntryFromBML(juce::String& contents,
                                                 const juce::String& pcmFileName)
{
    auto lower = contents.toLowerCase();
    auto searchName = juce::String("filename=\"") + pcmFileName.toLowerCase() + "\"";
    int filenameIndex = lower.indexOf(searchName);
    if (filenameIndex < 0)
        return false;

    int blockStart = -1;
    for (int i = filenameIndex; i >= 0; --i)
    {
        if (contents[i] == '{')
        {
            blockStart = i;
            break;
        }
    }

    if (blockStart < 0)
        return false;

    int trackLabelStart = blockStart;
    for (int i = blockStart; i >= 0; --i)
    {
        if (contents[i] == '\n')
        {
            auto line = contents.substring(i + 1, blockStart).trimStart();
            if (line.startsWith("track"))
            {
                trackLabelStart = i + 1;
                break;
            }
        }
    }

    int depth = 0;
    int blockEnd = -1;
    for (int i = blockStart; i < contents.length(); ++i)
    {
        auto ch = contents[i];
        if (ch == '{')
            ++depth;
        else if (ch == '}')
        {
            --depth;
            if (depth == 0)
            {
                blockEnd = i;
                break;
            }
        }
    }

    if (blockEnd < 0)
        return false;

    int removalEnd = blockEnd + 1;
    while (removalEnd < contents.length() && contents[removalEnd] != '\n')
        ++removalEnd;
    if (removalEnd < contents.length())
        ++removalEnd;

    contents = contents.replaceSection(trackLabelStart, removalEnd - trackLabelStart, juce::String());
    return true;
}
