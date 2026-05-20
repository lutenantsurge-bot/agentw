# Whisper.cpp Voice Input Integration

This adds optional voice input to llama-box-agent via whisper.cpp, with platform-specific audio backends for Linux, Termux, and Android/C4Droid.

## Files Added

- `whisper_input.h` - Header file defining WhisperConfig struct and WhisperInput class
- `whisper_input.cpp` - Implementation with platform-specific audio backends
- `WHISPER_INTERGRATION.md` - This documentation file

## Platform Backends

| Platform | Backend   | Compile Flag    | Libraries |
|----------|-----------|-----------------|-----------|
| Linux    | ALSA      | `AUDIO_ALSA`    | `-lasound` |
| Termux   | termux-api | `AUDIO_TERMUX` | none       |
| Android  | OpenSL ES | `AUDIO_OPENSL`  | `-lOpenSLES` |

Exactly one define is set at build time. All three compile cleanly.

## Build Commands

### Termux Build (Static Integration)

```bash
cd ~/Desktop/llama-box-agent
bash build.sh --termux
```

Or manually:

```bash
cd ~/Desktop/llama-box-agent
mkdir -p build && cd build
cmake .. \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_CXX_STANDARD=17 \
  -DWITH_WHISPER=ON \
  -DAUDIO_TERMUX=ON \
  -G "Unix Makefiles"
cmake --build . --config Release -j $(nproc)
```

### Linux Build (ALSA)

```bash
cd ~/Desktop/llama-box-agent
cmake -B build -DWITH_WHISPER=ON
cmake --build build
```

## Configuration

WhisperConfig structure:
```cpp
struct WhisperConfig {
    bool enabled = false;
    std::string wake_word;
    std::string model_path = "ggml-base.en.bin";
    std::string tiny_path = "ggml-tiny.en.bin";  // For wake word detection
    float vad_threshold = 0.02f;
    int sample_rate = 16000;
    int record_ms = 5000;
    int silence_ms = 800;
    std::string tmp_dir = "/tmp";
};
```

## Runtime Usage

```bash
# Always listen (VAD-triggered)
./llama-box-agent -whspr

# Wake word mode
./llama-box-agent -whspr -wake "hey llama"

# Custom model
./llama-box-agent -whspr -wake "computer" -whspr-model ggml-small.en.bin

# Custom temp directory (Termux)
./llama-box-agent -whspr -whspr-tmp /data/local/tmp
```

## Features

1. **Voice Activity Detection (VAD)** - RMS-based, near-zero CPU when idle
2. **Wake Word Detection** - Optional wake word with tiny model
3. **Audio Feedback** - Beep tones for ready/wake signals (platform-specific)
4. **Platform Abstraction** - Single codebase, three backends
5. **Static Compilation** - whisper.cpp integrated as static library

## Audio Flow

1. Record short chunk (silence_ms * 4 ms)
2. Check RMS - if below threshold, loop (Stage 1 VAD)
3. If wake_word set, run detect_wake() with tiny model
   - No match: loop
   - Match: play 520Hz 150ms beep, sleep 100ms, record full query
4. If no wake_word: use audio directly
5. Play 440Hz 200ms beep (ready signal)
6. Transcribe with full model, return text

## Termux Requirements

- Install Termux:API: `pkg install termux-api`
- Install Termux:API app from F-Droid
- Grant microphone permission in app

## Notes

- Microphone required; speaker optional (beep skipped if no speaker)
- VAD is RMS-only (no ML in listen loop)
- whisper.cpp compiled statically, integrated into llama-box-agent
- WITH_WHISPER=OFF builds identical to original behavior
- No threads, no background processes, no extra libraries
