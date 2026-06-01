#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include <juce_audio_formats/juce_audio_formats.h> // Include for MP3AudioFormat

class WaveformDisplay  : public juce::Component,
                         public juce::ChangeListener,
                         public juce::Timer
{
public:
    WaveformDisplay(YTAudioProcessor& p);
    ~WaveformDisplay() override;

    void paint(juce::Graphics& g) override;
    void changeListenerCallback(juce::ChangeBroadcaster* source) override;
    void timerCallback() override;
    
    void mouseDown(const juce::MouseEvent& event) override;
    void mouseDrag(const juce::MouseEvent& event) override;
    void mouseUp(const juce::MouseEvent& event) override;

private:
    YTAudioProcessor& audioProcessor;
    float dragStartX = 0.0f;
    double dragStartPlayheadPosition = 0.0;
    bool wasPlayingBeforeDrag = false;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(WaveformDisplay)
};


//==============================================================================
/**
*/
class YTAudioProcessorEditor  : public juce::AudioProcessorEditor,
                                  public juce::Button::Listener,
                                  public juce::ChangeListener,
                                  public juce::Slider::Listener,
                                  public juce::Timer
{
public:
    YTAudioProcessorEditor (YTAudioProcessor&);
    ~YTAudioProcessorEditor() override;

    //==============================================================================
    void paint (juce::Graphics&) override;
    void resized() override;
    
    void buttonClicked (juce::Button* button) override;
    void sliderValueChanged (juce::Slider* slider) override; // Added for Slider::Listener
    void timerCallback() override; // Added for Timer
    
    void changeListenerCallback (juce::ChangeBroadcaster* source) override;

private:
    // This reference is provided as a quick way for your editor to
    // access the processor object that created it.
    YTAudioProcessor& audioProcessor;
    
    juce::TextButton loadButton;
    juce::ImageButton playButton;  // Changed to ImageButton
    juce::ImageButton pauseButton; // Changed to ImageButton
    juce::ImageButton stopButton;

    juce::TextEditor urlInput;
    juce::TextButton downloadButton;
    juce::Label versionLabel;

    juce::Slider positionSlider;
    juce::Label currentPositionLabel;
    juce::Label totalLengthLabel;

    juce::Slider keySlider;
    juce::Label keyLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> keySliderAttachment;

    juce::Slider speedSlider;
    juce::Label speedLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> speedSliderAttachment;
    
    WaveformDisplay waveformDisplay;
    juce::Component horizontalLine;
    juce::Label logDisplay;

    // Settings Components
    juce::GroupComponent settingsGroup;
    juce::Label wavPathLabel;
    juce::TextEditor wavPathEditor;
    juce::TextButton wavPathBrowseButton;
    juce::Label ytDlpPathLabel;
    juce::TextEditor ytDlpPathEditor;
    juce::TextButton ytDlpPathBrowseButton;

    // Drawable objects for play icon
    std::unique_ptr<juce::Drawable> playIconDrawable;
    std::unique_ptr<juce::Drawable> playOverIconDrawable;
    std::unique_ptr<juce::Drawable> playDownIconDrawable;

    // Drawable objects for pause icon
    std::unique_ptr<juce::Drawable> pauseIconDrawable;
    std::unique_ptr<juce::Drawable> pauseOverIconDrawable;
    std::unique_ptr<juce::Drawable> pauseDownIconDrawable;

    juce::Image playIconNormalImage;
    juce::Image playIconOverImage;
    juce::Image playIconDownImage;
    juce::Image pauseIconNormalImage;
    juce::Image pauseIconOverImage;
    juce::Image pauseIconDownImage;


    // Drawable objects for stop button icons
    std::unique_ptr<juce::Drawable> stopIconDrawable;
    std::unique_ptr<juce::Drawable> stopIconOverDrawable;
    std::unique_ptr<juce::Drawable> stopIconDownDrawable;

    juce::Image stopIconNormalImage;
    juce::Image stopIconOverImage;
    juce::Image stopIconDownImage;



    std::unique_ptr<juce::FileChooser> chooser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (YTAudioProcessorEditor)
};
