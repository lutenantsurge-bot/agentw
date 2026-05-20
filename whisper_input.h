#ifndef WHISPER_INPUT_H
#define WHISPER_INPUT_H

#include <string>
#include <vector>
#include <memory>

// Whisper context type (forward declared to avoid full include)
struct whisper_context;

struct WhisperConfig {
    bool enabled = false;
    std::string wake_word;
    std::string model_path = "ggml-base.en.bin";
    std::string tiny_path = "ggml-tiny.en.bin";
    float vad_threshold = 0.02f;
    int sample_rate = 16000;
    int record_ms = 5000;
    int silence_ms = 800;
    std::string tmp_dir = "/tmp";
};

class WhisperInput {
public:
    explicit WhisperInput(const WhisperConfig &cfg);
    ~WhisperInput();
    
    // Initialize whisper context, check audio devices
    bool init();
    
    // Blocking listen - returns transcribed text
    std::string listen();
    
    // Play tone beep
    void play_beep(int hz, int ms);

private:
    WhisperConfig cfg_;
    
    // Audio device detection
    bool has_mic_ = false;
    bool has_spk_ = false;
    
    // Whisper contexts
    whisper_context *wctx_full_ = nullptr;
    whisper_context *wctx_tiny_ = nullptr;
    
    // Audio recording (platform-specific)
    std::vector<float> record_audio(int max_ms);
    
    // Wake word detection
    bool detect_wake(const std::vector<float> &audio);
    
    // Transcription using whisper context
    std::string transcribe(whisper_context *ctx, const std::vector<float> &audio);
    
    // Voice Activity Detection (RMS)
    float rms(const std::vector<float> &audio);
    
    // Check if audio devices available
    bool check_audio_devices();
    
    // WAV file helpers for Termux
    void write_wav(const std::string &path, const std::vector<float> &pcm);
    std::vector<float> read_wav(const std::string &path);
};

#endif // WHISPER_INPUT_H
