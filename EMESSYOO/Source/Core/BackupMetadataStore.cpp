#include "BackupMetadataStore.h"

namespace
{
    constexpr const char* metadataFileName = "metadata_backup.json";
    constexpr const char* versionKey = "version";
    constexpr const char* tracksKey = "tracks";
    constexpr const char* manifestPathKey = "manifestPath";
    constexpr const char* trackExistedKey = "trackExisted";
    constexpr const char* loopExistedKey = "loopExisted";
    constexpr const char* loopStartKey = "loopStart";
    constexpr const char* loopEndKey = "loopEnd";
    constexpr const char* timestampKey = "timestamp";
    constexpr int metadataVersion = 1;
}

BackupMetadataStore::BackupMetadataStore(const juce::File& backupDirectory)
{
    auto directory = backupDirectory;
    if (!directory.isDirectory())
        directory.createDirectory();
    metadataFile = directory.getChildFile(metadataFileName);
}

void BackupMetadataStore::recordSnapshot(const juce::String& pcmFileName,
                                         const juce::File& manifestFile,
                                         const MSUManifestUpdater::MetadataSnapshot& snapshot)
{
    if (pcmFileName.isEmpty() || !manifestFile.existsAsFile())
        return;

    auto* entries = getTrackArray(pcmFileName, true);
    if (entries == nullptr)
        return;

    for (int i = entries->size(); --i >= 0;)
    {
        auto* obj = (*entries)[i].getDynamicObject();
        if (obj != nullptr && obj->getProperty(manifestPathKey).toString() == manifestFile.getFullPathName())
            entries->remove(i);
    }

    auto* entry = new juce::DynamicObject();
    entry->setProperty(manifestPathKey, manifestFile.getFullPathName());
    entry->setProperty(trackExistedKey, snapshot.trackExisted);
    entry->setProperty(loopExistedKey, snapshot.loopExisted);
    entry->setProperty(loopStartKey, snapshot.loopStart);
    entry->setProperty(loopEndKey, snapshot.loopEnd);
    entry->setProperty(timestampKey, juce::Time::getCurrentTime().toISO8601(true));
    entries->add(juce::var(entry));

    save();
}

std::vector<BackupMetadataStore::Record> BackupMetadataStore::getSnapshotsFor(const juce::String& pcmFileName) const
{
    std::vector<Record> results;
    auto* entries = getTrackArray(pcmFileName, false);
    if (entries == nullptr)
        return results;

    for (const auto& entryVar : *entries)
    {
        if (auto* obj = entryVar.getDynamicObject())
        {
            Record record;
            record.pcmFileName = pcmFileName;
            record.manifestFile = juce::File(obj->getProperty(manifestPathKey).toString());
            record.snapshot.trackExisted = obj->getProperty(trackExistedKey);
            record.snapshot.loopExisted = obj->getProperty(loopExistedKey);
            record.snapshot.loopStart = static_cast<int64>(obj->getProperty(loopStartKey));
            record.snapshot.loopEnd = static_cast<int64>(obj->getProperty(loopEndKey));
            results.push_back(record);
        }
    }

    return results;
}

juce::StringArray BackupMetadataStore::listTrackedPCMFiles() const
{
    juce::StringArray files;
    if (auto* tracks = getTracksObject())
    {
        for (const auto& pair : tracks->getProperties())
        {
            if (pair.value.isArray())
                files.add(pair.name.toString());
        }
    }
    return files;
}

void BackupMetadataStore::loadIfNeeded() const
{
    if (loaded)
        return;

    loaded = true;
    if (metadataFile.existsAsFile())
    {
        auto content = metadataFile.loadFileAsString();
        if (content.isNotEmpty())
        {
            auto parsed = juce::JSON::parse(content);
            if (parsed.isObject())
            {
                data = parsed;
                auto* root = getRootObject();
                if (root != nullptr)
                {
                    if (!root->hasProperty(tracksKey) || !root->getProperty(tracksKey).isObject())
                        root->setProperty(tracksKey, juce::var(new juce::DynamicObject()));
                    if (!root->hasProperty(versionKey))
                        root->setProperty(versionKey, metadataVersion);
                    return;
                }
            }
        }
    }

    auto* root = new juce::DynamicObject();
    root->setProperty(versionKey, metadataVersion);
    root->setProperty(tracksKey, juce::var(new juce::DynamicObject()));
    data = juce::var(root);
}

void BackupMetadataStore::save() const
{
    loadIfNeeded();
    auto* root = getRootObject();
    if (root == nullptr)
        return;

    auto directory = metadataFile.getParentDirectory();
    if (!directory.isDirectory())
        directory.createDirectory();

    auto json = juce::JSON::toString(data, false);
    metadataFile.replaceWithText(json);
}

juce::DynamicObject* BackupMetadataStore::ensureRootObject() const
{
    loadIfNeeded();
    auto* root = getRootObject();
    if (root == nullptr)
    {
        root = new juce::DynamicObject();
        root->setProperty(versionKey, metadataVersion);
        root->setProperty(tracksKey, juce::var(new juce::DynamicObject()));
        data = juce::var(root);
    }
    if (!root->hasProperty(tracksKey) || !root->getProperty(tracksKey).isObject())
        root->setProperty(tracksKey, juce::var(new juce::DynamicObject()));
    if (!root->hasProperty(versionKey))
        root->setProperty(versionKey, metadataVersion);
    return root;
}

juce::DynamicObject* BackupMetadataStore::ensureTracksObject() const
{
    auto* root = ensureRootObject();
    auto tracksVar = root->getProperty(tracksKey);
    if (!tracksVar.isObject())
    {
        auto* tracks = new juce::DynamicObject();
        root->setProperty(tracksKey, juce::var(tracks));
        return tracks;
    }
    return tracksVar.getDynamicObject();
}

juce::DynamicObject* BackupMetadataStore::getRootObject() const
{
    loadIfNeeded();
    return data.getDynamicObject();
}

juce::DynamicObject* BackupMetadataStore::getTracksObject() const
{
    auto* root = getRootObject();
    if (root == nullptr)
        return nullptr;
    auto tracksVar = root->getProperty(tracksKey);
    return tracksVar.isObject() ? tracksVar.getDynamicObject() : nullptr;
}

juce::Array<juce::var>* BackupMetadataStore::getTrackArray(const juce::String& pcmFileName,
                                                           bool createIfMissing) const
{
    auto* tracks = createIfMissing ? ensureTracksObject() : getTracksObject();
    if (tracks == nullptr)
        return nullptr;

    auto existing = tracks->getProperty(pcmFileName);
    if (!existing.isArray())
    {
        if (!createIfMissing)
            return nullptr;
        tracks->setProperty(pcmFileName, juce::var(juce::Array<juce::var>()));
        existing = tracks->getProperty(pcmFileName);
    }
    return existing.getArray();
}
