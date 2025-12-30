# VST3 YouTube Player Development Plan

This document outlines the step-by-step development plan for the VST3 YouTube Player plugin. Each step is designed to be a verifiable milestone, resulting in a buildable and testable state.

## Phase 1: Basic Audio Engine

### Step 1: Local Audio File Playback
- **Goal:** Implement the core functionality to load and play a local audio file.
- **Tasks:**
    1.  Add a "Load File" button to the `PluginEditor` UI.
    2.  Implement the `juce::FileChooser` to allow users to select a `.wav` or `.mp3` file.
    3.  In `PluginProcessor`, set up `juce::AudioFormatManager` and `juce::AudioTransportSource`.
    4.  Create a `loadFile` method in `PluginProcessor` that loads the selected audio file into the `AudioTransportSource`.
    5.  Add "Play" and "Stop" buttons to the UI to control the `AudioTransportSource`.
    6.  Route the audio from the `AudioTransportSource` to the plugin's output in `processBlock`.
- **Verification:**
    - Build the VST3 plugin.
    - Load it in a DAW.
    - Click "Load File", select a local audio file.
    - Click "Play" and confirm audio playback.
    - Click "Stop" and confirm audio stops.

## Phase 2: YouTube Audio Download

### Step 2: Synchronous YouTube Download
- **Goal:** Download audio from a YouTube URL using `yt-dlp`. The download will be synchronous for this step.
- **Tasks:**
    1.  Add a `juce::TextEditor` to the UI for entering a YouTube URL.
    2.  Add a "Download" button.
    3.  When "Download" is clicked, use `juce::ChildProcess` to execute the `yt-dlp` command. The command should download the audio in WAV format to a temporary directory.
    4.  For now, the UI will freeze during download.
- **Verification:**
    - Build the plugin.
    - Enter a valid YouTube URL.
    - Click "Download".
    - Check the designated temporary directory to confirm that a `.wav` file has been created.

### Step 3: Asynchronous Download & Integration
- **Goal:** Prevent the UI from freezing during download and automatically load the audio after download.
- **Tasks:**
    1.  Create a `DownloadThread` class that inherits from `juce::Thread`.
    2.  Move the `juce::ChildProcess` logic for `yt-dlp` into the `run()` method of the `DownloadThread`.
    3.  When the "Download" button is clicked, create and start an instance of this thread.
    4.  Use a listener or callback mechanism (`juce::ChangeListener`) to notify the `PluginProcessor` when the download is complete.
    5.  Upon notification, call the `loadFile` method from Step 1 to load the newly downloaded file into the `AudioTransportSource`.
- **Verification:**
    - Build the plugin.
    - Enter a YouTube URL and click "Download".
    - The UI should remain responsive.
    - After the download finishes, clicking "Play" should play the audio from the YouTube video.

## Phase 3: UI/UX Refinement

### Step 4: Status Display and Controls
- **Goal:** Provide feedback to the user about the plugin's state.
- **Tasks:**
    1.  Add a `juce::Label` to the UI to act as a status bar.
    2.  Update the label's text to indicate the current state: "Ready", "Downloading...", "Download Complete", "Loading Audio...", "Ready to Play", "Error: ...".
    3.  Implement a "Reset" button that stops playback, unloads the audio file, and resets the plugin to its initial state.
- **Verification:**
    - Build the plugin.
    - Observe the status label as you download and play a file.
    - Use the "Reset" button to confirm the plugin state is cleared.

## Phase 4: Final Polish

### Step 5: Robustness and Reliability
- **Goal:** Handle potential errors and edge cases gracefully.
- **Tasks:**
    1.  **Sample Rate Conversion:** In the `loadFile` method, check if the sample rate of the source file differs from the DAW's sample rate. If so, wrap the `AudioFormatReaderSource` in a `juce::ResamplingAudioSource`.
    2.  **Error Handling:** Add checks and display appropriate error messages in the status label for:
        - Invalid YouTube URL.
        - Network connection errors during download.
        - `yt-dlp` or `ffmpeg` not being found in the system's PATH.
- **Verification:**
    - Build the plugin.
    - Test with audio files of different sample rates.
    - Test with invalid URLs and by disabling the network connection.
    - If `yt-dlp` is in the PATH, temporarily rename it to test the "not found" error case.
