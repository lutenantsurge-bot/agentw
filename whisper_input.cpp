#include "whisper_input.h"
#include <cmath>
#include <algorithm>
#include <cstring>
#include <sstream>
#include <stdexcept>
#include <unistd.h>

#ifdef AUDIO_TERMUX
    #include <cstdlib>
#endif

#ifdef AUDIO_ALSA
    #include <alsa/asoundlib.h>
#endif

#ifdef AUDIO_OPENSL
    #include <SLES/OpenSLES.h>
    #include <SLES/OpenSLES_Android.h>
#endif

extern "C" {
#include "whisper.h"
}

WhisperInput::WhisperInput(const WhisperConfig &cfg) : cfg_(cfg) {}

WhisperInput::~WhisperInput() {
    if (wctx_full_) whisper_free(wctx_full_);
    if (wctx_tiny_) whisper_free(wctx_tiny_);
}

float WhisperInput::rms(const std::vector<float> &audio) {
    if (audio.empty()) return 0.0f;
    float sum = 0.0f;
    for (float v : audio) sum += v * v;
    return std::sqrt(sum / audio.size());
}

bool WhisperInput::check_audio_devices() {
#ifdef AUDIO_ALSA
    snd_pcm_t *handle;
    if (snd_pcm_open(&handle, "default", SND_PCM_STREAM_CAPTURE, SND_PCM_NONBLOCK) == 0) {
        has_mic_ = true;
        snd_pcm_close(handle);
    }
    if (snd_pcm_open(&handle, "default", SND_PCM_STREAM_PLAYBACK, SND_PCM_NONBLOCK) == 0) {
        has_spk_ = true;
        snd_pcm_close(handle);
    }
#endif
#ifdef AUDIO_TERMUX
    has_mic_ = (system("command -v termux-microphone-record > /dev/null 2>&1") == 0);
    has_spk_ = (system("command -v termux-media-player > /dev/null 2>&1") == 0);
#endif
    return has_mic_;
}

bool WhisperInput::init() {
    if (!check_audio_devices()) {
        printf("[whspr] Warning: No microphone found\n");
        return false;
    }
    struct whisper_context_params cparams = whisper_context_default_params();
    cparams.use_gpu = false;
    wctx_full_ = whisper_init_from_file_with_params(cfg_.model_path.c_str(), cparams);
    if (!wctx_full_) {
        printf("[whspr] Error: Failed to load model %s\n", cfg_.model_path.c_str());
        return false;
    }
    if (!cfg_.wake_word.empty()) {
        wctx_tiny_ = whisper_init_from_file_with_params(cfg_.tiny_path.c_str(), cparams);
        if (!wctx_tiny_) wctx_tiny_ = wctx_full_;
    }
    printf("[whspr] Ready%s\n", cfg_.wake_word.empty() ? "" :
        (std::string(" (wake: \"") + cfg_.wake_word + "\")").c_str());
    return true;
}

void WhisperInput::write_wav(const std::string &path, const std::vector<float> &pcm) {
    FILE *f = fopen(path.c_str(), "wb");
    if (!f) return;
    int num_channels = 1, bits_per_sample = 16, sample_rate = cfg_.sample_rate;
    int byte_rate  = sample_rate * num_channels * bits_per_sample / 8;
    int block_align = num_channels * bits_per_sample / 8;
    int data_size  = (int)pcm.size() * block_align;
    int riff_size  = 36 + data_size;
    fwrite("RIFF", 1, 4, f); fwrite(&riff_size, 4, 1, f); fwrite("WAVE", 1, 4, f);
    fwrite("fmt ", 1, 4, f);
    int fmt_size = 16; fwrite(&fmt_size, 4, 1, f);
    uint16_t audio_format = 1; fwrite(&audio_format, 2, 1, f);
    fwrite(&num_channels, 2, 1, f); fwrite(&sample_rate, 4, 1, f);
    fwrite(&byte_rate, 4, 1, f);    fwrite(&block_align, 2, 1, f);
    fwrite(&bits_per_sample, 2, 1, f);
    fwrite("data", 1, 4, f); fwrite(&data_size, 4, 1, f);
    for (float v : pcm) {
        int16_t s = static_cast<int16_t>(v * 32767.0f);
        fwrite(&s, 2, 1, f);
    }
    fclose(f);
}

std::vector<float> WhisperInput::read_wav(const std::string &path) {
    std::vector<float> result;
    FILE *f = fopen(path.c_str(), "rb");
    if (!f) return result;
    char tag[4];
    fread(tag, 1, 4, f); if (strncmp(tag, "RIFF", 4) != 0) { fclose(f); return result; }
    int riff_size; fread(&riff_size, 4, 1, f);
    fread(tag, 1, 4, f); if (strncmp(tag, "WAVE", 4) != 0) { fclose(f); return result; }
    fread(tag, 1, 4, f); if (strncmp(tag, "fmt ", 4) != 0) { fclose(f); return result; }
    int fmt_size; fread(&fmt_size, 4, 1, f);
    uint16_t audio_format, num_channels; int sample_rate; uint16_t bits_per_sample;
    fread(&audio_format, 2, 1, f); fread(&num_channels, 2, 1, f);
    fread(&sample_rate, 4, 1, f);
    int skip; fread(&skip, 4, 1, f); fread(&bits_per_sample, 2, 1, f);
    fread(tag, 1, 4, f); if (strncmp(tag, "data", 4) != 0) { fclose(f); return result; }
    int data_size; fread(&data_size, 4, 1, f);
    int num_samples = data_size / (bits_per_sample / 8);
    std::vector<int16_t> pcm(num_samples);
    fread(pcm.data(), bits_per_sample / 8, num_samples, f);
    result.resize(num_samples);
    for (int i = 0; i < num_samples; i++)
        result[i] = static_cast<float>(pcm[i]) / 32768.0f;
    fclose(f);
    return result;
}

std::vector<float> WhisperInput::record_audio(int max_ms) {
    std::vector<float> result;

#ifdef AUDIO_ALSA
    snd_pcm_t *handle;
    if (snd_pcm_open(&handle, "default", SND_PCM_STREAM_CAPTURE, 0) != 0)
        return result;

    // FIX 1: use snd_pcm_uframes_t for buffer/period sizes, not int*
    snd_pcm_uframes_t buffer_size = 0, period_size = 0;
    snd_pcm_get_params(handle, &buffer_size, &period_size);

    snd_pcm_set_params(handle, SND_PCM_FORMAT_S16_LE,
                       SND_PCM_ACCESS_RW_INTERLEAVED,
                       1, cfg_.sample_rate, 1,
                       (unsigned int)(cfg_.sample_rate * max_ms / 1000));

    int max_samples = cfg_.sample_rate * max_ms / 1000;
    std::vector<int16_t> buffer(max_samples);
    int samples_read = 0;
    int samples_to_read = max_samples;

    // FIX 2: silence_count must be int not bool (bool++ is forbidden in C++17)
    int silence_count = 0;
    bool vad_triggered = false;

    while (samples_to_read > 0) {
        int n = std::min(samples_to_read, max_samples);
        int r = snd_pcm_readi(handle, buffer.data() + samples_read, n);
        if (r > 0) {
            samples_read     += r;
            samples_to_read  -= r;
        } else if (r == -EPIPE) {
            snd_pcm_prepare(handle);
        } else if (r < 0) {
            break;
        }
        int check_interval = cfg_.sample_rate * cfg_.silence_ms / 1000;
        if (samples_read >= check_interval && !vad_triggered) {
            std::vector<float> chunk(buffer.begin(), buffer.begin() + samples_read);
            float rv = rms(chunk);
            if (rv < cfg_.vad_threshold) {
                silence_count++;
                if (silence_count >= 2) { vad_triggered = true; break; }
            } else {
                silence_count = 0;
            }
        }
    }
    snd_pcm_close(handle);
    result.resize(samples_read);
    for (int i = 0; i < samples_read; i++)
        result[i] = static_cast<float>(buffer[i]) / 32768.0f;
#endif

#ifdef AUDIO_TERMUX
    std::string outfile = cfg_.tmp_dir + "/whspr_rec.wav";
    std::string cmd = "termux-microphone-record -l " +
                      std::to_string(max_ms / 1000.0f) +
                      " -f \"" + outfile + "\"";
    system(cmd.c_str());
    result = read_wav(outfile);
#endif

    size_t target = cfg_.sample_rate * max_ms / 1000;
    if (result.size() > target) result.resize(target);
    return result;
}

static std::vector<int16_t> generate_sine_wave(int hz, int ms, int sample_rate) {
    std::vector<int16_t> samples;
    int num_samples  = sample_rate * ms / 1000;
    int fade_samples = 100;
    for (int i = 0; i < num_samples; i++) {
        double t    = i / (double)sample_rate;
        double s    = sin(2.0 * M_PI * hz * t);
        double fade = 1.0;
        if (i < fade_samples)
            fade = (double)i / fade_samples;
        else if (i > num_samples - fade_samples)
            fade = 1.0 - ((double)(i - (num_samples - fade_samples)) / fade_samples);
        samples.push_back(static_cast<int16_t>(s * fade * 16000.0));
    }
    return samples;
}

void WhisperInput::play_beep(int hz, int ms) {
#ifdef AUDIO_ALSA
    if (!has_spk_) { printf("[whspr] No speaker, skipping beep\n"); return; }
    std::vector<int16_t> samples = generate_sine_wave(hz, ms, 16000);
    snd_pcm_t *handle;
    snd_pcm_open(&handle, "default", SND_PCM_STREAM_PLAYBACK, 0);
    snd_pcm_set_params(handle, SND_PCM_FORMAT_S16_LE,
                       SND_PCM_ACCESS_RW_INTERLEAVED, 1, 16000, 1, 256);
    // FIX 3: cast to const void* not const int*
    snd_pcm_writei(handle, static_cast<const void*>(samples.data()), samples.size());
    snd_pcm_drain(handle);
    snd_pcm_close(handle);
#endif

#ifdef AUDIO_TERMUX
    std::string beep_path = cfg_.tmp_dir + "/whspr_beep.wav";
    std::vector<int16_t> samples = generate_sine_wave(hz, ms, 16000);
    std::vector<float> float_samples;
    float_samples.reserve(samples.size());
    for (int16_t s : samples)
        float_samples.push_back(static_cast<float>(s) / 32768.0f);
    write_wav(beep_path, float_samples);
    system(("termux-media-player play \"" + beep_path + "\"").c_str());
    usleep(ms * 1000);
#endif
}

bool WhisperInput::detect_wake(const std::vector<float> &audio) {
    if (!wctx_tiny_ || audio.empty()) return false;
    std::string result = transcribe(wctx_tiny_, audio);
    std::string wake   = cfg_.wake_word;
    std::transform(wake.begin(),   wake.end(),   wake.begin(),   ::tolower);
    std::transform(result.begin(), result.end(), result.begin(), ::tolower);
    return result.find(wake) != std::string::npos;
}

std::string WhisperInput::transcribe(whisper_context *ctx, const std::vector<float> &audio) {
    if (!ctx || audio.empty()) return "";
    whisper_full_params wparams = whisper_full_default_params(WHISPER_SAMPLING_GREEDY);
    wparams.language         = "en";
    wparams.translate        = false;
    wparams.print_progress   = false;
    wparams.print_realtime   = false;
    wparams.print_timestamps = false;
    wparams.no_context       = true;
    wparams.single_segment   = true;
    if (whisper_full(ctx, wparams, audio.data(), (int)audio.size()) != 0) return "";
    int n = whisper_full_n_segments(ctx);
    if (n == 0) return "";
    const char *text = whisper_full_get_segment_text(ctx, n - 1);
    return text ? std::string(text) : "";
}

std::string WhisperInput::listen() {
    int chunk_ms = cfg_.silence_ms * 4;
    while (true) {
        std::vector<float> chunk = record_audio(chunk_ms);
        if (!chunk.empty() && rms(chunk) >= cfg_.vad_threshold) break;
    }
    if (!cfg_.wake_word.empty()) {
        if (!detect_wake(record_audio(chunk_ms))) return listen();
        play_beep(520, 150);
        usleep(100000);
    }
    play_beep(440, 200);
    return transcribe(wctx_full_, record_audio(cfg_.record_ms));
}
