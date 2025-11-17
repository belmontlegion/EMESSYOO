#pragma once

#include <JuceHeader.h>
#include <vector>

#include "NormalizationAnalyzer.h"
#include "../Core/AudioFileHandler.h"

/**
 * Runs multi-threaded loudness analysis across MSU-1 PCM tracks and
 * calculates the target gain required to match an edited buffer.
 */
class VolumeMatchAnalyzer
{
public:
    enum class PerformanceMode
    {
        Auto = 0,
        Balanced,
        HighPerformance,
        LowPower
    };

    struct TrackResult
    {
        juce::File file;
        bool success = false;
        NormalizationAnalyzer::AudioStats stats;
        juce::String errorMessage;
    };

    struct AnalysisResult
    {
        bool success = false;
        float targetRmsDb = -96.0f;
        float averagePeakDb = -96.0f;
        int filesAnalyzed = 0;
        int filesFailed = 0;
        std::vector<TrackResult> trackDetails;
        juce::String errorMessage;
    };

    VolumeMatchAnalyzer() = default;

    AnalysisResult analyzePCMDirectory(const juce::File& directory,
                                       const juce::File& fileToExclude,
                                       PerformanceMode mode) const;

    static juce::StringArray getPerformanceModeLabels();
    static PerformanceMode performanceModeFromIndex(int index);
    static int performanceModeToIndex(PerformanceMode mode);
    static juce::String describePerformanceMode(PerformanceMode mode);

private:
    struct FileAnalyzeJob : public juce::ThreadPoolJob
    {
        FileAnalyzeJob(const juce::File& fileToAnalyze, TrackResult& output);
        JobStatus runJob() override;

        juce::File file;
        TrackResult& result;
    };

    static int getThreadCountForMode(PerformanceMode mode);
    static bool analyzePCMFile(const juce::File& file,
                               NormalizationAnalyzer::AudioStats& stats,
                               juce::String& errorMessage);
};
