#pragma once

#include <JuceHeader.h>
#include "../Export/MSUManifestUpdater.h"

class BackupMetadataStore
{
public:
    struct Record
    {
        juce::String pcmFileName;
        juce::File manifestFile;
        MSUManifestUpdater::MetadataSnapshot snapshot;
    };

    explicit BackupMetadataStore(const juce::File& backupDirectory);

    void recordSnapshot(const juce::String& pcmFileName,
                        const juce::File& manifestFile,
                        const MSUManifestUpdater::MetadataSnapshot& snapshot);

    std::vector<Record> getSnapshotsFor(const juce::String& pcmFileName) const;
    juce::StringArray listTrackedPCMFiles() const;

private:
    juce::File metadataFile;
    mutable juce::var data;
    mutable bool loaded = false;

    void loadIfNeeded() const;
    void save() const;
    juce::DynamicObject* ensureRootObject() const;
    juce::DynamicObject* ensureTracksObject() const;
    juce::DynamicObject* getRootObject() const;
    juce::DynamicObject* getTracksObject() const;
    juce::Array<juce::var>* getTrackArray(const juce::String& pcmFileName, bool createIfMissing) const;
};
