/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "ea_soundtouch/include/SoundTouch.h"

//==============================================================================
/**
*/
class YTAudioProcessor  : public juce::AudioProcessor,
                            public juce::ChangeBroadcaster
{
public:
    class DownloadThread;
    //==============================================================================
    YTAudioProcessor();
    ~YTAudioProcessor() override;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

   #ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
   #endif

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    //==============================================================================
    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    //==============================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    //==============================================================================
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    //==============================================================================
    void loadFile(const juce::File& file);
    void play();
    void stop();
    void reset() override;
    void returnToBeginning(); // New method
    void downloadFile(const juce::String& url);
    void downloadFinished(bool success, const juce::File& file, const juce::String& message);

    // New playback control methods
    bool isPlaying() const;
    double getPlaybackPositionSeconds() const;
    double getTotalLengthSeconds() const;
    void setPlaybackPosition(double seconds);

    bool getPlaybackState() const; // New method for reliable state
    const juce::String& getStatusMessage() const { return statusMessage; }

    juce::AudioThumbnail& getAudioThumbnail() { return thumbnail; }

    juce::AudioProcessorValueTreeState& getValueTreeState();

    //==============================================================================
    // Settings
    juce::String getWavOutputPath();
    juce::String getYtDlpPath();
    juce::String getFfmpegPath();
    void setWavOutputPath(const juce::String& path);
    void setYtDlpPath(const juce::String& path);
    void setFfmpegPath(const juce::String& path);

private:
    //==============================================================================
    // Audio processing chain
    std::unique_ptr<juce::AudioFormatReaderSource> readerSource;
    juce::AudioTransportSource transportSource;

    // Pointer to the final source in the chain. AudioTransportSource handles any
    // sample-rate correction internally, so this always points at transportSource.
    juce::AudioSource* finalSource = nullptr;

    // One SoundTouch instance per output channel (each processes a single mono stream).
    std::vector<soundtouch::SoundTouch> soundTouchInstances;
    // Scratch buffer used to pull input from the source while feeding SoundTouch.
    juce::AudioBuffer<float> feedBuffer;

    juce::AudioProcessorValueTreeState valueTreeState;
    std::atomic<float>* pitch = nullptr;
    std::atomic<float>* speed = nullptr;


    // Waveform
    juce::AudioThumbnailCache thumbnailCache;
    juce::AudioThumbnail thumbnail;

    // Explicit playback state
    std::atomic<bool> isPlayingState {false};
    
    // Download handling
    juce::AudioFormatManager formatManager;
    juce::ReferenceCountedObjectPtr<DownloadThread> activeDownloadThread;
    juce::String statusMessage;

    // Settings
    std::unique_ptr<juce::PropertiesFile> settings;
    void initSettings();

    juce::File temporaryMonoFile;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (YTAudioProcessor)
};
