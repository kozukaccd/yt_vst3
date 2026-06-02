/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
class YTAudioProcessor::DownloadThread : public juce::Thread,
                                         public juce::ReferenceCountedObject
{
public:
    DownloadThread(YTAudioProcessor& p, const juce::String& u)
        : juce::Thread("YouTube Downloader"), processor(p), url(u)
    {
        // Get settings from processor
        wavOutputPath = processor.getWavOutputPath();
        ytDlpPath = processor.getYtDlpPath();
        ffmpegPath = processor.getFfmpegPath();

        outputDir = juce::File(wavOutputPath);
        tempFile = outputDir.getChildFile("downloaded_audio.wav"); // Initial placeholder name
    }

    void run() override
    {
        // 1. Create the output directory if it doesn't exist
        if (! outputDir.exists() && ! outputDir.createDirectory())
        {
            // Failed to create directory, notify and abort.
            auto self = juce::ReferenceCountedObjectPtr<DownloadThread>(this);
            juce::MessageManager::callAsync([self] {
                self->processor.downloadFinished(false, {}, "Error: Could not create output directory: " + self->wavOutputPath);
            });
            return;
        }

        // Debug log (written only if the download fails, to help diagnose).
        auto logFile = outputDir.getChildFile("yt-dlp-debug.log");
        if (logFile.existsAsFile())
            logFile.deleteFile();

        juce::File cookieFile = outputDir.getChildFile("www.youtube.com_cookies.txt");

        // yt-dlp writes the final output file path here (after extraction/move).
        auto pathFile = outputDir.getChildFile(".yt-dlp-last-output.txt");
        if (pathFile.existsAsFile())
            pathFile.deleteFile();

        // Let yt-dlp build and sanitize the filename itself. --restrict-filenames keeps
        // it ASCII-safe, which avoids invalid filenames on Windows for non-ASCII titles
        // (the previous manual naming broke on e.g. Japanese titles -> "Invalid argument").
        const auto outputTemplate = outputDir.getFullPathName()
                                   + juce::File::getSeparatorString()
                                   + "%(title)s-%(id)s.%(ext)s";

        // Download + extract to WAV, and record the resulting path via --print-to-file.
        juce::StringArray dlArgs;
        dlArgs.add(ytDlpPath);
        if (cookieFile.existsAsFile())
        {
            dlArgs.add("--cookies");
            dlArgs.add(cookieFile.getFullPathName());
        }
        if (ffmpegPath.isNotEmpty())
        {
            dlArgs.add("--ffmpeg-location");
            dlArgs.add(ffmpegPath);
        }
        dlArgs.add("--no-playlist");
        dlArgs.add("--restrict-filenames");
        dlArgs.add("-x");
        dlArgs.add("--audio-format"); dlArgs.add("wav");
        dlArgs.add("-o"); dlArgs.add(outputTemplate);
        dlArgs.add("--print-to-file"); dlArgs.add("after_move:filepath"); dlArgs.add(pathFile.getFullPathName());
        dlArgs.add("--no-simulate"); // required so --print-to-file does not imply simulate
        dlArgs.add(url);

        juce::String output;
        const bool started = runProcess(dlArgs, 600000, output); // up to 10 minutes

        // Determine the actual output file from what yt-dlp reported.
        juce::File downloadedFile;
        if (started && pathFile.existsAsFile())
        {
            juce::StringArray lines;
            lines.addLines(pathFile.loadFileAsString());
            lines.removeEmptyStrings();
            if (! lines.isEmpty())
                downloadedFile = juce::File(lines[lines.size() - 1].trim());
            pathFile.deleteFile();
        }

        // Fallback: scan the captured output for yt-dlp's final destination line.
        if (started && (downloadedFile == juce::File() || ! downloadedFile.existsAsFile()))
        {
            juce::StringArray outLines;
            outLines.addLines(output);
            for (int i = outLines.size(); --i >= 0;)
            {
                const auto& line = outLines.getReference(i);
                if (line.contains("Destination:") && line.containsIgnoreCase(".wav"))
                {
                    juce::File cand(line.fromFirstOccurrenceOf("Destination:", false, false).trim());
                    if (cand.existsAsFile()) { downloadedFile = cand; break; }
                }
            }
        }

        const bool success = downloadedFile != juce::File() && downloadedFile.existsAsFile();
        if (success)
        {
            tempFile = downloadedFile;
            logFile.deleteFile();
        }
        else
        {
            if (output.isEmpty())
                output = "Could not start yt-dlp. Check the yt-dlp path in Settings.";
            logFile.replaceWithText(output);
        }

        auto self = juce::ReferenceCountedObjectPtr<DownloadThread>(this);
        juce::MessageManager::callAsync([self, success, output] {
            self->processor.downloadFinished(success, self->tempFile, output);
        });
    }

private:
    // Runs an executable with the given arguments (no shell), capturing merged
    // stdout+stderr. Returns false on launch failure or timeout. Cooperatively
    // aborts if the thread is asked to exit.
    bool runProcess(const juce::StringArray& args, int timeoutMs, juce::String& outputResult)
    {
        outputResult.clear();

        juce::ChildProcess proc;
        if (! proc.start(args)) // default stream flags merge stdout + stderr
            return false;

        juce::MemoryOutputStream collected;
        char buffer[4096];
        const auto startTime = juce::Time::getMillisecondCounter();

        while (proc.isRunning())
        {
            if (threadShouldExit())
            {
                proc.kill();
                outputResult = collected.toString();
                return false;
            }

            auto numRead = proc.readProcessOutput(buffer, (int) sizeof(buffer));
            if (numRead > 0)
                collected.write(buffer, (size_t) numRead);
            else
                juce::Thread::sleep(20);

            if ((int) (juce::Time::getMillisecondCounter() - startTime) > timeoutMs)
            {
                proc.kill();
                collected << "\n\nProcess timed out.";
                outputResult = collected.toString();
                return false;
            }
        }

        // Drain any output still buffered after the process has exited.
        for (;;)
        {
            auto numRead = proc.readProcessOutput(buffer, (int) sizeof(buffer));
            if (numRead <= 0)
                break;
            collected.write(buffer, (size_t) numRead);
        }

        outputResult = collected.toString();
        return true;
    }

    YTAudioProcessor& processor;
    juce::String url;
    juce::File tempFile;
    juce::File outputDir;
    
    // Settings cache
    juce::String wavOutputPath;
    juce::String ytDlpPath;
    juce::String ffmpegPath;
};

//==============================================================================
YTAudioProcessor::YTAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
     : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                      #if ! JucePlugin_IsSynth
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      #endif
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                     #endif
                       ),
      valueTreeState(*this, nullptr, "PARAMETERS",
          {
              std::make_unique<juce::AudioParameterFloat>("pitch", "Pitch", juce::NormalisableRange<float>(-12.0f, 12.0f, 1.0f), 0.0f),
              std::make_unique<juce::AudioParameterFloat>("speed", "Speed", juce::NormalisableRange<float>(0.5f, 2.0f, 0.01f), 1.0f)
          }),
      thumbnailCache(5),
      thumbnail(512, formatManager, thumbnailCache)
#endif
{
    formatManager.registerBasicFormats();
    formatManager.registerFormat(new juce::MP3AudioFormat(), true);
    initSettings();
    statusMessage = "Ready.";
    finalSource = &transportSource;
    pitch = valueTreeState.getRawParameterValue("pitch");
    speed = valueTreeState.getRawParameterValue("speed");
}

juce::AudioProcessorValueTreeState& YTAudioProcessor::getValueTreeState()
{
    return valueTreeState;
}

YTAudioProcessor::~YTAudioProcessor()
{
    if (toolManager != nullptr)
        toolManager->clearListener(this);

    if (activeDownloadThread)
        activeDownloadThread->stopThread(1000);

    if (temporaryMonoFile.existsAsFile())
    {
        temporaryMonoFile.deleteFile();
    }
}

//==============================================================================
const juce::String YTAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool YTAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool YTAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool YTAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double YTAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int YTAudioProcessor::getNumPrograms()
{
    return 1;
}

int YTAudioProcessor::getCurrentProgram()
{
    return 0;
}

void YTAudioProcessor::setCurrentProgram (int index)
{
}

const juce::String YTAudioProcessor::getProgramName (int index)
{
    return {};
}

void YTAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
}

//==============================================================================
void YTAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    transportSource.prepareToPlay(samplesPerBlock, sampleRate);

    soundTouchInstances.resize(getTotalNumOutputChannels());
    for (auto& st : soundTouchInstances)
    {
        st.setSampleRate((unsigned int) sampleRate);
        st.setChannels(1); // SoundTouch processes one channel at a time
        st.setPitchSemiTones(0.0);
        st.setTempo(1.0);
        st.clear();
    }

    feedBuffer.setSize(getTotalNumOutputChannels(), samplesPerBlock);
}

void YTAudioProcessor::releaseResources()
{
    transportSource.releaseResources();
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool YTAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
  #if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
  #else
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

   #if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
   #endif

    return true;
  #endif
}
#endif

void YTAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();

    if (! transportSource.isPlaying() || finalSource == nullptr)
        return;

    const int numOutputChannels = getTotalNumOutputChannels();
    const int numSamples        = buffer.getNumSamples();
    const auto pitchValue       = pitch->load();
    const auto speedValue       = speed->load();

    // Fast path: no pitch shift and normal speed -> pass the source straight through.
    if (pitchValue == 0.0f && speedValue == 1.0f)
    {
        finalSource->getNextAudioBlock(juce::AudioSourceChannelInfo(buffer));

        // Keep SoundTouch empty so re-engaging it later starts from a clean state.
        for (auto& st : soundTouchInstances)
            st.clear();

        return;
    }

    // Time-stretch / pitch-shift path.
    // SoundTouch decouples the number of input and output samples, so we must pull
    // input from the source until enough output samples are available to fill the block.
    for (int ch = 0; ch < numOutputChannels && ch < (int) soundTouchInstances.size(); ++ch)
    {
        soundTouchInstances[ch].setPitchSemiTones((double) pitchValue);
        soundTouchInstances[ch].setTempo((double) speedValue);
    }

    const int feedChunk = juce::jmin(numSamples, feedBuffer.getNumSamples());

    // Safety cap to guarantee the feed loop always terminates.
    const int maxIterations = 64;
    int iterations = 0;

    while ((int) soundTouchInstances[0].numSamples() < numSamples && iterations++ < maxIterations)
    {
        if (transportSource.hasStreamFinished())
        {
            // Drain remaining buffered samples once the source is exhausted.
            for (auto& st : soundTouchInstances)
                st.flush();
            break;
        }

        feedBuffer.clear();
        juce::AudioSourceChannelInfo info(&feedBuffer, 0, feedChunk);
        finalSource->getNextAudioBlock(info);

        for (int ch = 0; ch < numOutputChannels && ch < (int) soundTouchInstances.size(); ++ch)
            soundTouchInstances[ch].putSamples(feedBuffer.getReadPointer(ch), (unsigned int) feedChunk);
    }

    for (int ch = 0; ch < numOutputChannels && ch < (int) soundTouchInstances.size(); ++ch)
        soundTouchInstances[ch].receiveSamples(buffer.getWritePointer(ch), (unsigned int) numSamples);
}

//==============================================================================
bool YTAudioProcessor::hasEditor() const
{
    return true;
}

juce::AudioProcessorEditor* YTAudioProcessor::createEditor()
{
    return new YTAudioProcessorEditor (*this);
}

//==============================================================================
void YTAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
}

void YTAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
}

//==============================================================================
void YTAudioProcessor::loadFile(const juce::File& file)
{
    reset(); // Reset everything, including deleting any old temporary mono file

    auto* reader = formatManager.createReaderFor(file);
    if (reader == nullptr)
    {
        statusMessage = "Error: Could not create reader for file " + file.getFileName();
        sendChangeMessage();
        return;
    }

    // Play the original file as-is so stereo is preserved. (The waveform display
    // intentionally draws only the left channel; playback uses all channels.)
    thumbnail.setSource(new juce::FileInputSource(file));
    readerSource = std::make_unique<juce::AudioFormatReaderSource>(reader, true); // takes ownership

    if (readerSource->getAudioFormatReader() != nullptr)
    {
        // The 4th argument (sourceSampleRateToCorrectFor) tells AudioTransportSource to
        // internally resample from the file's sample rate to the host's prepared sample
        // rate. This handles all sample-rate conversion, so no external ResamplingAudioSource
        // is needed (and adding one here would double-resample and, if left unprepared,
        // write out of bounds).
        transportSource.setSource(readerSource.get(), 0, nullptr, readerSource->getAudioFormatReader()->sampleRate);
        finalSource = &transportSource;

        statusMessage = "Loaded file: " + file.getFileName();
    }
    else
    {
        statusMessage = "Error: Could not set up transport source with " + file.getFileName();
    }

    sendChangeMessage();
}

void YTAudioProcessor::downloadFile(const juce::String& url)
{
    if (url.isEmpty())
    {
        statusMessage = "Please enter a URL.";
        sendChangeMessage();
        return;
    }

    if (activeDownloadThread)
    {
        statusMessage = "Already downloading.";
        sendChangeMessage();
        return;
    }

    statusMessage = "Downloading... " + url;
    sendChangeMessage();

    activeDownloadThread = new DownloadThread(*this, url); // Changed from std::make_unique to direct new
    activeDownloadThread->startThread();
}

void YTAudioProcessor::downloadFinished(bool success, const juce::File& file, const juce::String& message)
{
    activeDownloadThread = nullptr; // Safely decrements reference count and allows deletion when appropriate

    if (success)
    {
        loadFile(file);
    }
    else
    {
        statusMessage = "Error: Download failed.\n\n" + message;
        sendChangeMessage();
    }
}

void YTAudioProcessor::play()
{
    transportSource.start();
    isPlayingState = true;
}

void YTAudioProcessor::stop()
{
    transportSource.stop();
    isPlayingState = false;
}

void YTAudioProcessor::reset()
{
    transportSource.stop(); // Stop playback
    transportSource.setSource(nullptr); // Unload the file
    readerSource.reset();
    finalSource = &transportSource;
    isPlayingState = false;
    thumbnail.clear();

    if (temporaryMonoFile.existsAsFile())
    {
        temporaryMonoFile.deleteFile();
    }
    
    statusMessage = "Ready.";
    sendChangeMessage();
}

void YTAudioProcessor::returnToBeginning()
{
    transportSource.setPosition(0.0);
}

//==============================================================================
namespace SettingKeys
{
    static const juce::String wavOutputPath { "wavOutputPath" };
    static const juce::String ytDlpPath     { "ytDlpPath" };
    static const juce::String ffmpegPath    { "ffmpegPath" };
}

static juce::String getDefaultFfmpegPath()
{
   #if JUCE_MAC
    // DAW-launched plugins often have a minimal PATH that excludes Homebrew's bin.
    return "/opt/homebrew/bin/ffmpeg";
   #else
    // On Windows / Linux, rely on ffmpeg being resolvable via PATH by default.
    return "ffmpeg";
   #endif
}

void YTAudioProcessor::initSettings()
{
    juce::PropertiesFile::Options options;
    options.applicationName     = JucePlugin_Name;
    options.filenameSuffix      = ".settings";
    options.folderName          = juce::String ("YourCompanyName/") + JucePlugin_Name;
    options.osxLibrarySubFolder = "Application Support";
    options.commonToAllUsers    = false;

    settings = std::make_unique<juce::PropertiesFile>(options);

    // Managed tools live in a "bin" folder next to the settings file.
    auto binDir = settings->getFile().getParentDirectory().getChildFile("bin");
    toolManager = std::make_unique<ToolManager>(binDir);

    if (settings->getValue(SettingKeys::wavOutputPath).isEmpty())
    {
        // Set default values if they don't exist
        settings->setValue(SettingKeys::wavOutputPath, juce::File::getSpecialLocation(juce::File::userHomeDirectory).getChildFile("yt-wav").getFullPathName());
        settings->setValue(SettingKeys::ytDlpPath, "yt-dlp");
        settings->setValue(SettingKeys::ffmpegPath, getDefaultFfmpegPath());
    }
}

juce::String YTAudioProcessor::getWavOutputPath()
{
    return settings->getValue(SettingKeys::wavOutputPath, juce::File::getSpecialLocation(juce::File::userHomeDirectory).getChildFile("yt-wav").getFullPathName());
}

juce::String YTAudioProcessor::getYtDlpPath()
{
    return settings->getValue(SettingKeys::ytDlpPath, "yt-dlp");
}

void YTAudioProcessor::setWavOutputPath(const juce::String& path)
{
    settings->setValue(SettingKeys::wavOutputPath, path);
    settings->saveIfNeeded();
}

void YTAudioProcessor::setYtDlpPath(const juce::String& path)
{
    settings->setValue(SettingKeys::ytDlpPath, path);
    settings->saveIfNeeded();
}

juce::String YTAudioProcessor::getFfmpegPath()
{
    return settings->getValue(SettingKeys::ffmpegPath, getDefaultFfmpegPath());
}

void YTAudioProcessor::setFfmpegPath(const juce::String& path)
{
    settings->setValue(SettingKeys::ffmpegPath, path);
    settings->saveIfNeeded();
}

//==============================================================================
bool YTAudioProcessor::areExternalToolsReady()
{
    auto isFile = [] (const juce::String& p)
    {
        return p.isNotEmpty() && juce::File::isAbsolutePath(p) && juce::File(p).existsAsFile();
    };

    // Ready if the configured paths point at real executables, or the managed
    // bin/ copies exist.
    const bool ytOk = isFile(getYtDlpPath()) || toolManager->getYtDlpFile().existsAsFile();
    const bool ffOk = isFile(getFfmpegPath()) || toolManager->getFfmpegFile().existsAsFile();
    return ytOk && ffOk;
}

bool YTAudioProcessor::isToolSetupBusy() const
{
    return toolManager != nullptr && toolManager->isBusy();
}

void YTAudioProcessor::startToolSetup()
{
    if (toolManager == nullptr || toolManager->isBusy())
        return;

    statusMessage = "Starting setup...";
    sendChangeMessage();
    toolManager->startSetup(this);
}

void YTAudioProcessor::toolSetupProgress(const juce::String& message, double /*progress01*/)
{
    statusMessage = message;
    sendChangeMessage();
}

void YTAudioProcessor::toolSetupFinished(bool success, const juce::String& message)
{
    if (success)
    {
        // Point the settings at the freshly downloaded, managed binaries.
        setYtDlpPath(toolManager->getYtDlpFile().getFullPathName());
        setFfmpegPath(toolManager->getFfmpegFile().getFullPathName());
    }

    statusMessage = message;
    sendChangeMessage();
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new YTAudioProcessor();
}

//==============================================================================
// New playback control methods implementation
bool YTAudioProcessor::isPlaying() const
{
    return transportSource.isPlaying();
}

bool YTAudioProcessor::getPlaybackState() const
{
    return isPlayingState;
}

double YTAudioProcessor::getPlaybackPositionSeconds() const
{
    return transportSource.getCurrentPosition();
}

double YTAudioProcessor::getTotalLengthSeconds() const
{
    if (readerSource != nullptr && readerSource->getAudioFormatReader() != nullptr)
        return (double)readerSource->getAudioFormatReader()->lengthInSamples / readerSource->getAudioFormatReader()->sampleRate;
    return 0.0;
}

void YTAudioProcessor::setPlaybackPosition(double seconds)
{
    transportSource.setPosition(seconds);
}