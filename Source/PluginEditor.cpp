#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
// WaveformDisplay のメソッド実装
WaveformDisplay::WaveformDisplay(YTAudioProcessor& p) : audioProcessor(p)
{
    audioProcessor.getAudioThumbnail().addChangeListener(this);
    startTimerHz(30);
}

WaveformDisplay::~WaveformDisplay()
{
    audioProcessor.getAudioThumbnail().removeChangeListener(this);
    stopTimer();
}

void WaveformDisplay::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colours::darkgrey);
    g.setColour(juce::Colours::lightgrey);

    auto& thumbnail = audioProcessor.getAudioThumbnail();
    if (thumbnail.getTotalLength() > 0.0)
    {
        auto waveformArea = getLocalBounds();
        auto playheadPosition = audioProcessor.getPlaybackPositionSeconds();
        // auto totalLength = thumbnail.getTotalLength(); // Unused variable
        
        const float playheadX = 60.0f;
        const float visibleDuration = 10.0f; // 10 seconds of waveform visible

        float pixelsPerSecond = (float)waveformArea.getWidth() / visibleDuration;
        
        // Calculate the start time for the visible waveform section
        double startTime = playheadPosition - (playheadX / pixelsPerSecond);
        double endTime = startTime + visibleDuration;
        
        g.setColour(juce::Colour(75, 161, 171));
        thumbnail.drawChannels(g, waveformArea, startTime, endTime, 1.0f);
        
        // Draw a playhead marker
        g.setColour(juce::Colours::red);
        g.drawVerticalLine((int)playheadX, waveformArea.getY(), waveformArea.getBottom());
    }
    else
    {
        g.setFont(14.0f);
        g.drawText("No audio loaded", getLocalBounds(), juce::Justification::centred, true);
    }
}

void WaveformDisplay::changeListenerCallback(juce::ChangeBroadcaster* source)
{
    if (source == &audioProcessor.getAudioThumbnail())
    {
        repaint();
    }
}

void WaveformDisplay::timerCallback()
{
    repaint();
}

void WaveformDisplay::mouseDown(const juce::MouseEvent& event)
{
    dragStartX = event.x;
    dragStartPlayheadPosition = audioProcessor.getPlaybackPositionSeconds();
    wasPlayingBeforeDrag = audioProcessor.isPlaying();
    if (wasPlayingBeforeDrag)
    {
        audioProcessor.stop();
    }
}

void WaveformDisplay::mouseDrag(const juce::MouseEvent& event)
{
    if (auto* thumbnail = &audioProcessor.getAudioThumbnail())
    {
        if (thumbnail->getTotalLength() > 0.0)
        {
            const float visibleDuration = 10.0f;
            float pixelsPerSecond = (float)getWidth() / visibleDuration;
            float deltaX = event.x - dragStartX;
            double deltaTime = deltaX / pixelsPerSecond;
            double newPlayheadPosition = dragStartPlayheadPosition - deltaTime;
            
            newPlayheadPosition = juce::jlimit(0.0, thumbnail->getTotalLength(), newPlayheadPosition);
            
            audioProcessor.setPlaybackPosition(newPlayheadPosition);
        }
    }
}

void WaveformDisplay::mouseUp(const juce::MouseEvent& event)
{
    if (wasPlayingBeforeDrag)
    {
        audioProcessor.play();
    }
}


//==============================================================================
// YTAudioProcessorEditor のメソッド実装
YTAudioProcessorEditor::YTAudioProcessorEditor (YTAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p), waveformDisplay(p)
{
    audioProcessor.addChangeListener(this);

    addAndMakeVisible(urlInput);
    urlInput.setTextToShowWhenEmpty("Enter YouTube URL", juce::Colours::grey);

    downloadButton.setButtonText("Download");
    downloadButton.addListener(this);
    addAndMakeVisible(downloadButton);
    
    loadButton.setButtonText("Load File"); // Corrected text
    loadButton.addListener(this);
    addAndMakeVisible(loadButton);

    // Initialize Drawable objects for play icon (white triangle)
    playIconDrawable = juce::Drawable::createFromSVG(*juce::XmlDocument::parse("<svg height='24' width='24' viewBox='0 0 24 24'><polygon points='5,3 19,12 5,21' fill='white'/></svg>"));
    playOverIconDrawable = juce::Drawable::createFromSVG(*juce::XmlDocument::parse("<svg height='24' width='24' viewBox='0 0 24 24'><polygon points='5,3 19,12 5,21' fill='lightgrey'/></svg>"));
    playDownIconDrawable = juce::Drawable::createFromSVG(*juce::XmlDocument::parse("<svg height='24' width='24' viewBox='0 0 24 24'><polygon points='5,3 19,12 5,21' fill='darkgrey'/></svg>"));
    
    // Create Images from Drawables for play icon
    if (playIconDrawable)
    {
        playIconNormalImage = juce::Image (juce::Image::ARGB, 24, 24, true);
        juce::Graphics g (playIconNormalImage);
        g.fillAll (juce::Colours::transparentBlack); // Clear with transparency
        playIconDrawable->draw (g, 1.0f); // Draw with full opacity, rely on Drawable scaling
    }
    if (playOverIconDrawable)
    {
        playIconOverImage = juce::Image (juce::Image::ARGB, 24, 24, true);
        juce::Graphics g (playIconOverImage);
        g.fillAll (juce::Colours::transparentBlack);
        playOverIconDrawable->draw (g, 1.0f);
    }
    if (playDownIconDrawable)
    {
        playIconDownImage = juce::Image (juce::Image::ARGB, 24, 24, true);
        juce::Graphics g (playIconDownImage);
        g.fillAll (juce::Colours::transparentBlack);
        playDownIconDrawable->draw (g, 1.0f);
    }

    playButton.addListener(this);
    addAndMakeVisible(playButton);
    playButton.setImages(false, true, true, // Not toggleable
                         playIconNormalImage, 1.0f, juce::Colours::transparentWhite,
                         playIconOverImage, 1.0f, juce::Colours::transparentWhite,
                         playIconDownImage, 1.0f, juce::Colours::transparentWhite);


    // Initialize Drawable objects for pause icon (white two vertical bars)
    pauseIconDrawable = juce::Drawable::createFromSVG(*juce::XmlDocument::parse("<svg height='24' width='24' viewBox='0 0 24 24'><rect x='5' y='3' width='6' height='18' fill='white'/><rect x='13' y='3' width='6' height='18' fill='white'/></svg>"));
    pauseOverIconDrawable = juce::Drawable::createFromSVG(*juce::XmlDocument::parse("<svg height='24' width='24' viewBox='0 0 24 24'><rect x='5' y='3' width='6' height='18' fill='lightgrey'/><rect x='13' y='3' width='6' height='18' fill='lightgrey'/></svg>"));
    pauseDownIconDrawable = juce::Drawable::createFromSVG(*juce::XmlDocument::parse("<svg height='24' width='24' viewBox='0 0 24 24'><rect x='5' y='3' width='6' height='18' fill='darkgrey'/><rect x='13' y='3' width='6' height='18' fill='darkgrey'/></svg>"));

    // Create Images from Drawables for pause icon
    if (pauseIconDrawable)
    {
        pauseIconNormalImage = juce::Image (juce::Image::ARGB, 24, 24, true);
        juce::Graphics g (pauseIconNormalImage);
        g.fillAll (juce::Colours::transparentBlack);
        pauseIconDrawable->draw (g, 1.0f);
    }
    if (pauseOverIconDrawable)
    {
        pauseIconOverImage = juce::Image (juce::Image::ARGB, 24, 24, true);
        juce::Graphics g (pauseIconOverImage);
        g.fillAll (juce::Colours::transparentBlack);
        pauseOverIconDrawable->draw (g, 1.0f);
    }
    if (pauseDownIconDrawable)
    {
        pauseIconDownImage = juce::Image (juce::Image::ARGB, 24, 24, true);
        juce::Graphics g (pauseIconDownImage);
        g.fillAll (juce::Colours::transparentBlack);
        pauseDownIconDrawable->draw (g, 1.0f);
    }

    pauseButton.addListener(this);
    addAndMakeVisible(pauseButton);
    pauseButton.setImages(false, true, true, // Not toggleable
                         pauseIconNormalImage, 1.0f, juce::Colours::transparentWhite,
                         pauseIconOverImage, 1.0f, juce::Colours::transparentWhite,
                         pauseIconDownImage, 1.0f, juce::Colours::transparentWhite);
    
    // Set initial button states
    playButton.setEnabled(true);
    pauseButton.setEnabled(false);
    stopButton.setEnabled(false);

    // Initialize Drawable objects for stop button (white square)
    stopIconDrawable = juce::Drawable::createFromSVG(*juce::XmlDocument::parse("<svg height='24' width='24' viewBox='0 0 24 24'><rect x='5' y='3' width='14' height='18' fill='white'/></svg>"));
    stopIconOverDrawable = juce::Drawable::createFromSVG(*juce::XmlDocument::parse("<svg height='24' width='24' viewBox='0 0 24 24'><rect x='5' y='3' width='14' height='18' fill='lightgrey'/></svg>"));
    stopIconDownDrawable = juce::Drawable::createFromSVG(*juce::XmlDocument::parse("<svg height='24' width='24' viewBox='0 0 24 24'><rect x='5' y='3' width='14' height='18' fill='darkgrey'/></svg>"));

    // Create Images from Drawables for stop icon
    if (stopIconDrawable)
    {
        stopIconNormalImage = juce::Image (juce::Image::ARGB, 24, 24, true);
        juce::Graphics g (stopIconNormalImage);
        g.fillAll (juce::Colours::transparentBlack);
        stopIconDrawable->draw (g, 1.0f);
    }
    if (stopIconOverDrawable)
    {
        stopIconOverImage = juce::Image (juce::Image::ARGB, 24, 24, true);
        juce::Graphics g (stopIconOverImage);
        g.fillAll (juce::Colours::transparentBlack);
        stopIconOverDrawable->draw (g, 1.0f);
    }
    if (stopIconDownDrawable)
    {
        stopIconDownImage = juce::Image (juce::Image::ARGB, 24, 24, true);
        juce::Graphics g (stopIconDownImage);
        g.fillAll (juce::Colours::transparentBlack);
        stopIconDownDrawable->draw (g, 1.0f);
    }
    
    stopButton.addListener(this);
    addAndMakeVisible(stopButton);
    stopButton.setImages(false, true, true, // Stop button is not toggleable
                         stopIconNormalImage, 1.0f, juce::Colours::transparentWhite,
                         stopIconOverImage, 1.0f, juce::Colours::transparentWhite,
                         stopIconDownImage, 1.0f, juce::Colours::transparentWhite);

    addAndMakeVisible(waveformDisplay);

    addAndMakeVisible(horizontalLine);
    horizontalLine.setColour(juce::ComboBox::backgroundColourId, juce::Colours::lightgrey); // A subtle line

    versionLabel.setText("v0.5", juce::dontSendNotification); // Updated version
    versionLabel.setJustificationType(juce::Justification::bottomRight);
    addAndMakeVisible(versionLabel);

    positionSlider.setRange(0.0, 1.0, 0.0);
    positionSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    positionSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0); // Hide the text box
    positionSlider.addListener(this);
    addAndMakeVisible(positionSlider);
    
    logDisplay.setText("", juce::dontSendNotification); // Initialize with empty text
    logDisplay.setJustificationType(juce::Justification::centredLeft);
    logDisplay.setColour(juce::Label::backgroundColourId, juce::Colours::darkgrey.withAlpha(0.5f));
    logDisplay.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
    addAndMakeVisible(logDisplay);

    currentPositionLabel.setText("00:00", juce::dontSendNotification); // Reintroduced
    currentPositionLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(currentPositionLabel);

    totalLengthLabel.setText("00:00", juce::dontSendNotification); // Reintroduced
    totalLengthLabel.setJustificationType(juce::Justification::centredRight);
    addAndMakeVisible(totalLengthLabel);

    keyLabel.setText("Key", juce::dontSendNotification);
    keyLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(keyLabel);

    addAndMakeVisible(keySlider);
    keySlider.setSliderStyle(juce::Slider::LinearHorizontal);
    keySlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 50, 20);
    keySliderAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(audioProcessor.getValueTreeState(), "pitch", keySlider);

    speedLabel.setText("Speed", juce::dontSendNotification);
    speedLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(speedLabel);

    addAndMakeVisible(speedSlider);
    speedSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    speedSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 50, 20);
    speedSlider.setTextValueSuffix("x");
    speedSliderAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(audioProcessor.getValueTreeState(), "speed", speedSlider);

    // --- Settings UI Initialization ---
    settingsGroup.setText("Settings");
    addAndMakeVisible(settingsGroup);

    wavPathLabel.setText("WAV Output Path:", juce::dontSendNotification);
    wavPathLabel.attachToComponent(&wavPathEditor, true);
    settingsGroup.addAndMakeVisible(wavPathLabel);

    wavPathEditor.setReadOnly(true);
    wavPathEditor.setText(audioProcessor.getWavOutputPath());
    settingsGroup.addAndMakeVisible(wavPathEditor);

    wavPathBrowseButton.setButtonText("Browse...");
    wavPathBrowseButton.addListener(this);
    settingsGroup.addAndMakeVisible(wavPathBrowseButton);

    ytDlpPathLabel.setText("yt-dlp Path:", juce::dontSendNotification);
    ytDlpPathLabel.attachToComponent(&ytDlpPathEditor, true);
    settingsGroup.addAndMakeVisible(ytDlpPathLabel);

    ytDlpPathEditor.setReadOnly(true);
    ytDlpPathEditor.setText(audioProcessor.getYtDlpPath());
    settingsGroup.addAndMakeVisible(ytDlpPathEditor);

    ytDlpPathBrowseButton.setButtonText("Browse...");
    ytDlpPathBrowseButton.addListener(this);
    settingsGroup.addAndMakeVisible(ytDlpPathBrowseButton);

    ffmpegPathLabel.setText("ffmpeg Path:", juce::dontSendNotification);
    ffmpegPathLabel.attachToComponent(&ffmpegPathEditor, true);
    settingsGroup.addAndMakeVisible(ffmpegPathLabel);

    ffmpegPathEditor.setReadOnly(true);
    ffmpegPathEditor.setText(audioProcessor.getFfmpegPath());
    settingsGroup.addAndMakeVisible(ffmpegPathEditor);

    ffmpegPathBrowseButton.setButtonText("Browse...");
    ffmpegPathBrowseButton.addListener(this);
    settingsGroup.addAndMakeVisible(ffmpegPathBrowseButton);

    firstSetupButton.setButtonText("First-time Setup (download yt-dlp / ffmpeg)");
    firstSetupButton.addListener(this);
    settingsGroup.addAndMakeVisible(firstSetupButton);

    updateSettingsUiMode();
    // --- End Settings UI ---

    setSize (800, 730); // Height accommodates Key + Speed sliders and the settings panel

    startTimerHz(30); // Start timer for UI updates
}

YTAudioProcessorEditor::~YTAudioProcessorEditor()
{
    audioProcessor.removeChangeListener(this);
    stopTimer(); // Stop timer when editor is destroyed
}

//==============================================================================
void YTAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));
}

void YTAudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds();
    const int padding = 10;
    const int largeButtonHeight = 50;
    const int inputHeight = 25;
    const int sliderHeight = 20;
    const int labelHeight = 15;
    const int waveformHeight = 160;
    const int horizontalLineHeight = 2;
    const int logDisplayHeight = 20; // New: Height for the log display (one line)
    const int bottomRowHeight = 25; // For version label

    // 1. Load File button (large)
    loadButton.setBounds(bounds.removeFromTop(largeButtonHeight + padding).reduced(padding, padding / 2));

    // 2. URL Input and Download Button
    auto urlDownloadArea = bounds.removeFromTop(inputHeight + padding);
    urlInput.setBounds(urlDownloadArea.removeFromLeft(urlDownloadArea.getWidth() - 100 - padding).reduced(padding, 0));
    downloadButton.setBounds(urlDownloadArea.reduced(padding, 0));

    // 3. Horizontal Line
    horizontalLine.setBounds(bounds.removeFromTop(horizontalLineHeight + padding).reduced(padding, padding / 2));

    // 4. Log Display (new position)
    logDisplay.setBounds(bounds.removeFromTop(logDisplayHeight + padding).reduced(padding, 0));

    // 5. Waveform display
    waveformDisplay.setBounds(bounds.removeFromTop(waveformHeight + padding).reduced(padding, padding / 2));

    // 6. Seekbar
    positionSlider.setBounds(bounds.removeFromTop(sliderHeight + padding).reduced(padding, padding / 1));
    
    // 7. Current Playback Time | Total Playback Time
    auto timeLabelsArea = bounds.removeFromTop(labelHeight + padding);
    currentPositionLabel.setBounds(timeLabelsArea.removeFromLeft(timeLabelsArea.getWidth() / 2).reduced(padding, 0));
    totalLengthLabel.setBounds(timeLabelsArea.reduced(padding, 0));

    // 8. Transport buttons (play / pause / stop), centred
    const int buttonWidth = 60;
    const int buttonHeight = 36;
    const int buttonSpacing = 5;
    const int totalButtonsWidth = (buttonWidth * 3) + (buttonSpacing * 2);

    auto controlsArea = bounds.removeFromTop(buttonHeight + padding);
    auto buttonsArea = controlsArea.withSizeKeepingCentre(totalButtonsWidth, buttonHeight);

    playButton.setBounds(buttonsArea.removeFromLeft(buttonWidth));
    buttonsArea.removeFromLeft(buttonSpacing);
    pauseButton.setBounds(buttonsArea.removeFromLeft(buttonWidth));
    buttonsArea.removeFromLeft(buttonSpacing);
    stopButton.setBounds(buttonsArea.removeFromLeft(buttonWidth));

    // 9. Key and Speed sliders (label on top, slider with text box below)
    auto slidersRow = bounds.removeFromTop(labelHeight + sliderHeight + 25 + padding);
    auto keyArea   = slidersRow.removeFromLeft(slidersRow.getWidth() / 2).reduced(padding, 0);
    auto speedArea = slidersRow.reduced(padding, 0);

    keyLabel.setBounds(keyArea.removeFromTop(labelHeight));
    keySlider.setBounds(keyArea);
    speedLabel.setBounds(speedArea.removeFromTop(labelHeight));
    speedSlider.setBounds(speedArea);

    // Settings Group
    auto settingsArea = bounds.removeFromBottom(200); // Space for three settings rows
    settingsGroup.setBounds(settingsArea.reduced(padding, 0));

    auto settingsInnerArea = settingsGroup.getLocalBounds().reduced(padding * 2, padding * 3);
    settingsInnerArea.removeFromTop(5); // Top padding inside group box

    auto row1 = settingsInnerArea.removeFromTop(30);
    wavPathLabel.setBounds(row1.removeFromLeft(120));
    wavPathBrowseButton.setBounds(row1.removeFromRight(80));
    wavPathEditor.setBounds(row1.reduced(5, 0));

    settingsInnerArea.removeFromTop(padding); // Spacer

    auto row2 = settingsInnerArea.removeFromTop(30);
    ytDlpPathLabel.setBounds(row2.removeFromLeft(120));
    ytDlpPathBrowseButton.setBounds(row2.removeFromRight(80));
    ytDlpPathEditor.setBounds(row2.reduced(5, 0));

    settingsInnerArea.removeFromTop(padding); // Spacer

    auto row3 = settingsInnerArea.removeFromTop(30);
    ffmpegPathLabel.setBounds(row3.removeFromLeft(120));
    ffmpegPathBrowseButton.setBounds(row3.removeFromRight(80));
    ffmpegPathEditor.setBounds(row3.reduced(5, 0));

    // First-time setup button occupies the centre of the group (shown only when
    // tools are missing; overlaps the path rows, which are hidden in that mode).
    auto fsArea = settingsGroup.getLocalBounds().reduced(padding * 2, padding * 3);
    firstSetupButton.setBounds(fsArea.withSizeKeepingCentre(juce::jmin(440, fsArea.getWidth()), 40));

    // Bottom row: Version display
    auto bottomArea = bounds.removeFromBottom(bottomRowHeight + padding).reduced(padding, 0);
    versionLabel.setBounds(bottomArea.removeFromRight(80));
}

void YTAudioProcessorEditor::buttonClicked(juce::Button* button)
{
    if (button == &downloadButton)
    {
        audioProcessor.downloadFile(urlInput.getText());
    }
    else if (button == &loadButton)
    {
        chooser = std::make_unique<juce::FileChooser>("Select a WAV or MP3 file...",
                                                      juce::File{},
                                                      "*.wav;*.mp3");
        auto flags = juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles;

        chooser->launchAsync(flags, [this](const juce::FileChooser& fc)
        {
            if (fc.getResults().isEmpty())
                return;

            audioProcessor.loadFile(fc.getResult());
        });
    }
    else if (button == &playButton)
    {
        audioProcessor.play();
    }
    else if (button == &pauseButton)
    {
        audioProcessor.stop();
    }
    else if (button == &stopButton) // Handle stop button click
    {
        audioProcessor.stop();
        audioProcessor.returnToBeginning();
    }
    else if (button == &wavPathBrowseButton)
    {
        chooser = std::make_unique<juce::FileChooser>("Select WAV Output Directory...",
                                                      juce::File(audioProcessor.getWavOutputPath()),
                                                      "");
        auto flags = juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories;

        chooser->launchAsync(flags, [this](const juce::FileChooser& fc)
        {
            if (fc.getResults().isEmpty())
                return;

            auto path = fc.getResult().getFullPathName();
            audioProcessor.setWavOutputPath(path);
            wavPathEditor.setText(path, juce::dontSendNotification);
        });
    }
    else if (button == &ytDlpPathBrowseButton)
    {
        chooser = std::make_unique<juce::FileChooser>("Select yt-dlp Executable...",
                                                      juce::File(audioProcessor.getYtDlpPath()),
                                                      "");
        auto flags = juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles;

        chooser->launchAsync(flags, [this](const juce::FileChooser& fc)
        {
            if (fc.getResults().isEmpty())
                return;

            auto path = fc.getResult().getFullPathName();
            audioProcessor.setYtDlpPath(path);
            ytDlpPathEditor.setText(path, juce::dontSendNotification);
        });
    }
    else if (button == &ffmpegPathBrowseButton)
    {
        chooser = std::make_unique<juce::FileChooser>("Select ffmpeg Executable...",
                                                      juce::File(audioProcessor.getFfmpegPath()),
                                                      "");
        auto flags = juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles;

        chooser->launchAsync(flags, [this](const juce::FileChooser& fc)
        {
            if (fc.getResults().isEmpty())
                return;

            auto path = fc.getResult().getFullPathName();
            audioProcessor.setFfmpegPath(path);
            ffmpegPathEditor.setText(path, juce::dontSendNotification);
        });
    }
    else if (button == &firstSetupButton)
    {
        firstSetupButton.setEnabled(false);
        audioProcessor.startToolSetup();
    }
}

void YTAudioProcessorEditor::sliderValueChanged(juce::Slider* slider)
{
    if (slider == &positionSlider)
    {
        if (positionSlider.isMouseButtonDown()) // Only seek if the slider is being dragged by the user
        {
            double newPositionProportion = positionSlider.getValue();
            double totalLength = audioProcessor.getTotalLengthSeconds();
            if (totalLength > 0)
            {
                audioProcessor.setPlaybackPosition(newPositionProportion * totalLength);
            }
        }
    }
}

void YTAudioProcessorEditor::timerCallback()
{
    // Update labels and slider for playback position
    double currentPositionSeconds = audioProcessor.getPlaybackPositionSeconds();
    double totalLengthSeconds = audioProcessor.getTotalLengthSeconds();

    int currentMinutes = (int)currentPositionSeconds / 60;
    int currentSeconds = (int)currentPositionSeconds % 60;
    currentPositionLabel.setText(juce::String::formatted("%02d:%02d", currentMinutes, currentSeconds), juce::dontSendNotification);

    int totalMinutes = (int)totalLengthSeconds / 60;
    int totalSeconds = (int)totalLengthSeconds % 60;
    totalLengthLabel.setText(juce::String::formatted("%02d:%02d", totalMinutes, totalSeconds), juce::dontSendNotification);

    // Only update slider if not currently being dragged
    if (! positionSlider.isMouseButtonDown())
    {
        if (totalLengthSeconds > 0)
            positionSlider.setValue(currentPositionSeconds / totalLengthSeconds, juce::dontSendNotification);
        else
            positionSlider.setValue(0.0, juce::dontSendNotification);
    }

    // Update play/pause button enabled state based on playback state
    const bool isPlaying = audioProcessor.getPlaybackState();
    playButton.setEnabled(!isPlaying);
    pauseButton.setEnabled(isPlaying);
    
    // Stop button is enabled if a file is loaded
    const bool isFileLoaded = audioProcessor.getTotalLengthSeconds() > 0;
    stopButton.setEnabled(isFileLoaded);
}

void YTAudioProcessorEditor::changeListenerCallback(juce::ChangeBroadcaster* source)
{
    if (source == &audioProcessor)
    {
        logDisplay.setText(audioProcessor.getStatusMessage(), juce::dontSendNotification);
        updateSettingsUiMode();
    }
}

void YTAudioProcessorEditor::updateSettingsUiMode()
{
    const bool ready = audioProcessor.areExternalToolsReady();
    const bool busy  = audioProcessor.isToolSetupBusy();

    auto setVisible = [ready] (juce::Component& c) { c.setVisible(ready); };
    setVisible(wavPathLabel);    setVisible(wavPathEditor);    setVisible(wavPathBrowseButton);
    setVisible(ytDlpPathLabel);  setVisible(ytDlpPathEditor);  setVisible(ytDlpPathBrowseButton);
    setVisible(ffmpegPathLabel); setVisible(ffmpegPathEditor); setVisible(ffmpegPathBrowseButton);

    firstSetupButton.setVisible(! ready);
    firstSetupButton.setEnabled(! busy);

    if (ready)
    {
        // Refresh in case a just-finished setup populated the paths.
        wavPathEditor.setText(audioProcessor.getWavOutputPath(), juce::dontSendNotification);
        ytDlpPathEditor.setText(audioProcessor.getYtDlpPath(), juce::dontSendNotification);
        ffmpegPathEditor.setText(audioProcessor.getFfmpegPath(), juce::dontSendNotification);
    }
}