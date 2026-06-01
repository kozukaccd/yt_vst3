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

        outputDir = juce::File(wavOutputPath);
        tempFile = outputDir.getChildFile("downloaded_audio.wav"); // Initial placeholder name
    }

    void run() override
    {
        // 1. Create the output directory if it doesn't exist
        if (! outputDir.exists())
        {
            if (! outputDir.createDirectory())
            {
                // Failed to create directory, notify and abort.
                auto self = juce::ReferenceCountedObjectPtr<DownloadThread>(this);
                juce::MessageManager::callAsync([self] {
                    self->processor.downloadFinished(false, {}, "Error: Could not create output directory: " + self->wavOutputPath);
                });
                return;
            }
        }

        // 2. Set up the debug log file within the output directory
        auto logFile = outputDir.getChildFile("yt-dlp-debug.log");
        if (logFile.existsAsFile())
            logFile.deleteFile();

        // 3. Extract title and ID using yt-dlp --print
        juce::File cookieFile = outputDir.getChildFile("www.youtube.com_cookies.txt");
        juce::String cookieOption;
        if (cookieFile.existsAsFile())
            cookieOption = " --cookies " + cookieFile.getFullPathName().quoted();

        juce::String titleIdCommand = ytDlpPath.quoted() + cookieOption + " --print title --print id --no-warnings --skip-download ";
        titleIdCommand += url.quoted();
        titleIdCommand += " > " + logFile.getFullPathName().quoted() + " 2>&1";

        juce::StringArray titleIdArgs;
        titleIdArgs.add("/bin/sh");
        titleIdArgs.add("-c");
        titleIdArgs.add(titleIdCommand);

        juce::ChildProcess titleIdProcess;
        juce::String titleIdOutput;
        juce::String videoTitle;
        juce::String videoId;

        if (titleIdProcess.start(titleIdArgs))
        {
            if (titleIdProcess.waitForProcessToFinish(30000)) // 30 seconds for metadata
            {
                titleIdOutput = logFile.loadFileAsString();
                juce::StringArray lines;
                lines.addTokens(titleIdOutput.replace("---", ""), "\n", ""); // yt-dlp --print adds "---" between outputs
                
                if (lines.size() >= 2)
                {
                    videoTitle = lines[0].trim();
                    videoId = lines[1].trim();
                } else if (!titleIdOutput.contains("ERROR")) {
                    videoTitle = "unknown_title";
                    videoId = juce::String(juce::Time::getCurrentTime().toMilliseconds());
                }
            }
            else
            {
                titleIdProcess.kill();
                videoTitle = "timed_out_title";
                videoId = juce::String(juce::Time::getCurrentTime().toMilliseconds());
            }
        }
        else
        {
            videoTitle = "process_start_fail_title";
            videoId = juce::String(juce::Time::getCurrentTime().toMilliseconds());
        }

        videoTitle = videoTitle.replaceCharacters(" /\\?:*\"<>|", "_");
        videoId = videoId.replaceCharacters(" /\\?:*\"<>|", "_");

        // 4. Construct final tempFile path
        tempFile = outputDir.getChildFile(videoTitle + "_" + videoId + ".wav");
        
        if (tempFile.existsAsFile())
            tempFile.deleteFile();

        // 5. Build and execute the main download command
        juce::String downloadCommand = ytDlpPath.quoted() + cookieOption + " --ffmpeg-location \"/opt/homebrew/bin/ffmpeg\" -x --audio-format wav -o ";
        downloadCommand += tempFile.getFullPathName().quoted();
        downloadCommand += " ";
        downloadCommand += url.quoted();
        downloadCommand += " > " + logFile.getFullPathName().quoted() + " 2>&1";

        juce::StringArray downloadArgs;
        downloadArgs.add("/bin/sh");
        downloadArgs.add("-c");
        downloadArgs.add(downloadCommand);

        juce::ChildProcess process;
        bool success = false;
        juce::String output;

        if (process.start(downloadArgs))
        {
            if (process.waitForProcessToFinish(60000))
            {
                if (tempFile.existsAsFile())
                {
                    success = true;
                }
            }
            else
            {
                process.kill();
                output += "\n\nProcess timed out.";
            }
            output = logFile.loadFileAsString();
        }
        else
        {
            output = "Could not start shell process for download. Check if yt-dlp path is correct in settings.";
        }
        
        if (success)
            logFile.deleteFile();

        auto self = juce::ReferenceCountedObjectPtr<DownloadThread>(this);
        juce::MessageManager::callAsync([self, success, output] {
            self->processor.downloadFinished(success, self->tempFile, output);
        });
    }

private:
    YTAudioProcessor& processor;
    juce::String url;
    juce::File tempFile;
    juce::File outputDir;
    
    // Settings cache
    juce::String wavOutputPath;
    juce::String ytDlpPath;
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
              std::make_unique<juce::AudioParameterFloat>("pitch", "Pitch", juce::NormalisableRange<float>(-12.0f, 12.0f, 1.0f), 0.0f)
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
}

juce::AudioProcessorValueTreeState& YTAudioProcessor::getValueTreeState()
{
    return valueTreeState;
}

YTAudioProcessor::~YTAudioProcessor()
{
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
        st.setSampleRate(sampleRate);
        st.setChannels(1); // SoundTouch processes one channel at a time
        st.setPitchSemiTones(0.0f);
        st.flush();
    }

    outputFifos.resize(getTotalNumOutputChannels());
    for (auto& fifo : outputFifos)
    {
        fifo.clear();
    }

    tempBuffer.setSize(1, samplesPerBlock * 2);
    inputCopyBuffer.setSize(getTotalNumOutputChannels(), samplesPerBlock);
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

    if (transportSource.isPlaying())
    {
        auto totalNumInputChannels  = getTotalNumInputChannels();
        auto totalNumOutputChannels = getTotalNumOutputChannels();

        for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
            buffer.clear (i, 0, buffer.getNumSamples());

        if (finalSource != nullptr)
            finalSource->getNextAudioBlock(juce::AudioSourceChannelInfo(buffer));

        auto pitchValue = pitch->load();

        if (pitchValue != 0.0f)
        {
            inputCopyBuffer.makeCopyOf(buffer);
            buffer.clear();

            for (int channel = 0; channel < totalNumOutputChannels; ++channel)
            {
                soundTouchInstances[channel].setPitchSemiTones(pitchValue);
                soundTouchInstances[channel].putSamples(inputCopyBuffer.getReadPointer(channel), inputCopyBuffer.getNumSamples());
                soundTouchInstances[channel].receiveSamples(buffer.getWritePointer(channel), buffer.getNumSamples());
            }
        }
        else
        {
            // If pitch is zero, make sure SoundTouch and FIFOs are flushed
            for(auto& st : soundTouchInstances)
                st.clear();
            for (auto& fifo : outputFifos)
                fifo.clear();
        }
    }
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

    auto* originalReader = formatManager.createReaderFor(file);
    if (originalReader == nullptr)
    {
        statusMessage = "Error: Could not create reader for file " + file.getFileName();
        sendChangeMessage();
        return;
    }

    // Always attempt to create a mono version to simplify logic and ensure single channel for thumbnail
    temporaryMonoFile = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("temp_mono_audio_" + juce::String(juce::Time::getCurrentTime().toMilliseconds()) + ".wav");

    if (temporaryMonoFile.existsAsFile())
        temporaryMonoFile.deleteFile();

    // Create a buffer to hold the audio data (enough for all original channels)
    juce::AudioBuffer<float> tempBuffer(originalReader->numChannels, (int) originalReader->lengthInSamples);
    
    // Read all channels from the original reader into tempBuffer
    // This is the read(AudioBuffer<float>* buffer, int startSampleInDestBuffer, int numSamples, int64 readerStartSample, bool useReaderLeftChan, bool useReaderRightChan) overload
    // where useReaderLeftChan and useReaderRightChan control which source channels are read into the destination buffer,
    // and if destBuffer is mono, it will sum them or pick one.
    originalReader->read(&tempBuffer, 0, (int) originalReader->lengthInSamples, 0, true, true);

    // Create a mono buffer and copy the left channel (channel 0) from tempBuffer
    juce::AudioBuffer<float> monoBuffer(1, tempBuffer.getNumSamples());
    monoBuffer.copyFrom(0, 0, tempBuffer, 0, 0, tempBuffer.getNumSamples()); // Copy from original channel 0

    // Create a writer for the temporary mono WAV file
    if (auto writer = std::unique_ptr<juce::AudioFormatWriter>(juce::WavAudioFormat().createWriterFor(new juce::FileOutputStream(temporaryMonoFile),
                                                                                                      originalReader->sampleRate,
                                                                                                      1, // 1 channel
                                                                                                      originalReader->bitsPerSample,
                                                                                                      {}, // Empty metadata
                                                                                                      0))) // Default quality
    {
        writer->writeFromAudioSampleBuffer(monoBuffer, 0, monoBuffer.getNumSamples());
        writer->flush(); // Ensure data is written to disk
    }
    else
    {
        statusMessage = "Error: Could not create temporary mono file writer.";
        sendChangeMessage();
        delete originalReader; // Clean up original reader
        return;
    }

    // Now, set the thumbnail and transport source to use the temporary mono file
    thumbnail.setSource(new juce::FileInputSource(temporaryMonoFile));
    readerSource = std::make_unique<juce::AudioFormatReaderSource>(formatManager.createReaderFor(temporaryMonoFile), true);
    delete originalReader; // Clean up original reader
    
    // Continue with common setup for transportSource.
    if (readerSource->getAudioFormatReader() != nullptr)
    {
        // The 4th argument (sourceSampleRateToCorrectFor) tells AudioTransportSource to
        // internally resample from the file's sample rate to the host's prepared sample
        // rate. This handles all sample-rate conversion, so no external ResamplingAudioSource
        // is needed (and adding one here would double-resample and, if left unprepared,
        // write out of bounds).
        transportSource.setSource(readerSource.get(), 0, nullptr, readerSource->getAudioFormatReader()->sampleRate);
        finalSource = &transportSource;

        statusMessage = "Loaded file (mono conversion applied): " + temporaryMonoFile.getFileName();
    }
    else
    {
        statusMessage = "Error: Could not set up transport source with " + temporaryMonoFile.getFileName();
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

    if (settings->getValue(SettingKeys::wavOutputPath).isEmpty())
    {
        // Set default values if they don't exist
        settings->setValue(SettingKeys::wavOutputPath, juce::File::getSpecialLocation(juce::File::userHomeDirectory).getChildFile("yt-wav").getFullPathName());
        settings->setValue(SettingKeys::ytDlpPath, "yt-dlp");
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