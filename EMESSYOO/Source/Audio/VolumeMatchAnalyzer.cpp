#include "VolumeMatchAnalyzer.h"

namespace
{
    constexpr float kSilenceDb = -96.0f;
}

VolumeMatchAnalyzer::FileAnalyzeJob::FileAnalyzeJob(const juce::File& fileToAnalyze,
                                                    TrackResult& output)
    : juce::ThreadPoolJob("Analyze " + fileToAnalyze.getFileName()),
      file(fileToAnalyze),
      result(output)
{
    result.file = fileToAnalyze;
}

juce::ThreadPoolJob::JobStatus VolumeMatchAnalyzer::FileAnalyzeJob::runJob()
{
    NormalizationAnalyzer::AudioStats stats;
    juce::String error;

    if (VolumeMatchAnalyzer::analyzePCMFile(file, stats, error))
    {
        result.stats = stats;
        result.success = true;
        result.errorMessage.clear();
    }
    else
    {
        result.success = false;
        result.errorMessage = error.isNotEmpty() ? error : juce::String("Unable to analyze file");
    }

    return juce::ThreadPoolJob::jobHasFinished;
}

VolumeMatchAnalyzer::AnalysisResult VolumeMatchAnalyzer::analyzePCMDirectory(
    const juce::File& directory,
    const juce::File& fileToExclude,
    PerformanceMode mode) const
{
    AnalysisResult result;

    if (!directory.isDirectory())
    {
        result.errorMessage = "Volume match requires a loaded MSU directory";
        return result;
    }

    juce::Array<juce::File> pcmFiles = directory.findChildFiles(juce::File::findFiles, false, "*.pcm");

    if (fileToExclude != juce::File())
        pcmFiles.removeAllInstancesOf(fileToExclude);

    if (pcmFiles.isEmpty())
    {
        result.errorMessage = "No PCM tracks found to analyze";
        return result;
    }

    const int threadCount = juce::jmax(1, getThreadCountForMode(mode));
    juce::ThreadPool pool(threadCount);

    std::vector<TrackResult> trackResults(static_cast<size_t>(pcmFiles.size()));
    juce::OwnedArray<FileAnalyzeJob> jobs;

    for (int i = 0; i < pcmFiles.size(); ++i)
    {
        auto* job = new FileAnalyzeJob(pcmFiles[i], trackResults[static_cast<size_t>(i)]);
        jobs.add(job);
        pool.addJob(job, false);
    }

    for (auto* job : jobs)
        pool.waitForJobToFinish(job, -1);

    double sumRmsLinear = 0.0;
    double sumPeakLinear = 0.0;
    int successCount = 0;
    int failureCount = 0;

    for (auto& track : trackResults)
    {
        if (track.success)
        {
            sumRmsLinear += track.stats.rmsLinear;
            sumPeakLinear += track.stats.peakLinear;
            ++successCount;
        }
        else
        {
            ++failureCount;
        }
    }

    if (successCount == 0)
    {
        result.errorMessage = "Unable to analyze the PCM library";
        result.filesFailed = failureCount;
        return result;
    }

    result.filesAnalyzed = successCount;
    result.filesFailed = failureCount;
    result.trackDetails = std::move(trackResults);

    float avgRmsLinear = static_cast<float>(sumRmsLinear / successCount);
    float avgPeakLinear = static_cast<float>(sumPeakLinear / successCount);

    result.targetRmsDb = juce::Decibels::gainToDecibels(avgRmsLinear, kSilenceDb);
    result.averagePeakDb = juce::Decibels::gainToDecibels(avgPeakLinear, kSilenceDb);
    result.success = true;

    return result;
}

int VolumeMatchAnalyzer::getThreadCountForMode(PerformanceMode mode)
{
    const int cpuCount = juce::jmax(1, juce::SystemStats::getNumCpus());

    switch (mode)
    {
        case PerformanceMode::LowPower:
            return 1;
        case PerformanceMode::Balanced:
            return juce::jmax(1, cpuCount / 2);
        case PerformanceMode::HighPerformance:
            return cpuCount;
        case PerformanceMode::Auto:
        default:
            return juce::jlimit(1, cpuCount, cpuCount - 1 > 0 ? cpuCount - 1 : cpuCount);
    }
}

bool VolumeMatchAnalyzer::analyzePCMFile(const juce::File& file,
                                          NormalizationAnalyzer::AudioStats& stats,
                                          juce::String& errorMessage)
{
    AudioFileHandler fileHandler;
    juce::AudioBuffer<float> buffer;
    double sampleRate = 0.0;

    if (!fileHandler.loadAudioFile(file, buffer, sampleRate, nullptr))
    {
        errorMessage = fileHandler.getLastError();
        return false;
    }

    stats = NormalizationAnalyzer::analyzeBuffer(buffer);
    return true;
}

juce::StringArray VolumeMatchAnalyzer::getPerformanceModeLabels()
{
    return { "Auto", "Balanced", "High Performance", "Low Power" };
}

VolumeMatchAnalyzer::PerformanceMode VolumeMatchAnalyzer::performanceModeFromIndex(int index)
{
    switch (index)
    {
        case 1: return PerformanceMode::Balanced;
        case 2: return PerformanceMode::HighPerformance;
        case 3: return PerformanceMode::LowPower;
        case 0:
        default: return PerformanceMode::Auto;
    }
}

int VolumeMatchAnalyzer::performanceModeToIndex(PerformanceMode mode)
{
    switch (mode)
    {
        case PerformanceMode::Balanced: return 1;
        case PerformanceMode::HighPerformance: return 2;
        case PerformanceMode::LowPower: return 3;
        case PerformanceMode::Auto:
        default: return 0;
    }
}

juce::String VolumeMatchAnalyzer::describePerformanceMode(PerformanceMode mode)
{
    switch (mode)
    {
        case PerformanceMode::Auto:
            return "Auto chooses threads based on available CPU cores.";
        case PerformanceMode::Balanced:
            return "Balanced uses roughly half of your CPU threads to keep the UI responsive.";
        case PerformanceMode::HighPerformance:
            return "High Performance uses all CPU threads for the fastest possible analysis.";
        case PerformanceMode::LowPower:
            return "Low Power runs analysis on a single thread to minimize system impact.";
    }

    return {};
}
