# GuitarFX

GuitarFX is a JUCE-based Windows desktop guitar effects application. It processes live audio through built-in effects, optional VST3 plugins, and a multi-lane looper.

## Features

### Audio

- Audio input and output device selection.
- Automatic preference for the `Focusrite USB ASIO` device when it is available.
- Audio device settings are restored between runs.

### Built-in effects

The Effects tab includes the following processors:

- Distortion
- Overdrive
- Noise gate
- Cabinet effect
- Delay
- Reverb

Each effect can be enabled or bypassed and exposes its relevant controls.

### VST3 plugins

The VST Plugins tab scans standard VST3 locations and lets you:

1. Select a discovered plugin.
2. Add it to the processing chain.
3. Enable or bypass it.
4. Open its editor when supported.
5. Remove it from the chain.

On Windows, the default scan locations are:

- `C:\Program Files\Common Files\VST3`
- `C:\Program Files (x86)\Common Files\VST3`
- `%LOCALAPPDATA%\VST3`

### Looper

The Looper tab supports up to ten lanes:

- Press **Space** or click **Space: Record / Arm** to start recording.
- The first recording establishes the master loop length.
- Press **Space** while playing to arm the next available lane.
- Press **Esc** or click **Stop** to stop recording/playback.
- Use **Delete Track** to clear an individual lane.
- Use **Clear All (Del)** to clear all lanes.
- Adjust lane level and mute individual lanes.
- Drag the waveform handles to trim the start and end.
- Automatically trim captured audio based on its signal level.
- Add lanes with **Add Lane**.

The application saves audio-device settings, VST3 chain state, and looper lane settings in a JUCE XML state file under the user application-data directory.

## Requirements

- Windows 10 or later
- Visual Studio 2022 with:
  - Desktop development with C++
  - MSVC v143 build tools
  - Windows 10 SDK
- JUCE with Projucer
- An audio interface is recommended for low-latency use
- Optional VST3 plugins

## Building

1. Install JUCE and note the path to its `modules` directory.
2. Open [NewProject.jucer](NewProject.jucer) in Projucer.
3. Update the JUCE module paths if they do not match your installation.
4. Save or re-export the Visual Studio 2022 project.
5. Open `Builds/VisualStudio2022/NewProject.sln` in Visual Studio.
6. Select `Debug` or `Release` and the `x64` platform.
7. Build and run the `NewProject` application.

The generated Visual Studio project currently contains machine-specific JUCE paths from the original development environment. Re-exporting from Projucer is the recommended way to update them for another machine.

## Running

1. Connect the audio interface before launching the application.
2. Open the **Audio** tab and select the desired input/output device and buffer size.
3. Enable the built-in effects you want to use.
4. Optionally add VST3 plugins from the **VST Plugins** tab.
5. Use the **Looper** tab to record and layer loops.

Use headphones or suitable monitoring to avoid feedback.

## Repository layout

```text
Source/                 Application and effect source code
Source/Effects/         Built-in audio effects
Source/Looper/          Multi-lane looper
Source/VST/             VST3 discovery and hosting
JuceLibraryCode/        JUCE generated project glue
Builds/VisualStudio2022 Visual Studio solution and project files
NewProject.jucer        Projucer project definition
```

Build products, Visual Studio caches, and user-specific settings are excluded by [.gitignore](.gitignore).

## Current limitations

- The project is currently configured around the Visual Studio 2022 exporter and x64 Windows builds.
- VST3 scanning is performed against the built-in default paths.
- Audio state is persisted locally, not committed to the repository.
