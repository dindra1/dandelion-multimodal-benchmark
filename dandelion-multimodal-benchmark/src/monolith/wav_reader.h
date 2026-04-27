#pragma once
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>
#include <string>

// Minimal WAV reader — handles 16-bit PCM mono/stereo, downmixes to mono float.
// Returns false on any parse error.
inline bool wav_read_mono_f32(const std::string& path,
                               std::vector<float>& out_samples,
                               int& out_sample_rate) {
    FILE* f = fopen(path.c_str(), "rb");
    if (!f) return false;

    auto read_u16 = [&](uint16_t& v) { return fread(&v, 2, 1, f) == 1; };
    auto read_u32 = [&](uint32_t& v) { return fread(&v, 4, 1, f) == 1; };

    char id[4];
    uint32_t chunk_size;

    // RIFF header
    if (fread(id, 1, 4, f) != 4 || memcmp(id, "RIFF", 4) != 0) { fclose(f); return false; }
    read_u32(chunk_size);
    if (fread(id, 1, 4, f) != 4 || memcmp(id, "WAVE", 4) != 0) { fclose(f); return false; }

    uint16_t audio_format = 0, num_channels = 0, bits_per_sample = 0;
    uint32_t sample_rate = 0;
    bool got_fmt = false, got_data = false;

    while (!got_data && fread(id, 1, 4, f) == 4) {
        read_u32(chunk_size);
        if (memcmp(id, "fmt ", 4) == 0) {
            uint32_t sr; uint16_t nc, bps, af;
            read_u16(af); read_u16(nc);
            read_u32(sr);
            uint32_t byte_rate; read_u32(byte_rate);
            uint16_t block_align; read_u16(block_align);
            read_u16(bps);
            if (chunk_size > 16) fseek(f, (long)(chunk_size - 16), SEEK_CUR);
            audio_format  = af;
            num_channels  = nc;
            sample_rate   = sr;
            bits_per_sample = bps;
            got_fmt = true;
        } else if (memcmp(id, "data", 4) == 0 && got_fmt) {
            out_sample_rate = (int)sample_rate;
            size_t bytes_per_sample = bits_per_sample / 8;
            size_t total_samples    = chunk_size / bytes_per_sample;
            size_t mono_samples     = total_samples / num_channels;
            out_samples.resize(mono_samples);

            if (bits_per_sample == 16 && audio_format == 1) {
                std::vector<int16_t> raw(total_samples);
                fread(raw.data(), 2, total_samples, f);
                for (size_t i = 0; i < mono_samples; i++) {
                    // Mix channels to mono
                    float s = 0.f;
                    for (int c = 0; c < num_channels; c++)
                        s += raw[i * num_channels + c] / 32768.0f;
                    out_samples[i] = s / num_channels;
                }
            } else if (bits_per_sample == 32 && audio_format == 3) {
                // 32-bit float WAV
                fread(out_samples.data(), 4, mono_samples, f);
            } else {
                fclose(f); return false; // unsupported format
            }
            got_data = true;
        } else {
            fseek(f, (long)chunk_size, SEEK_CUR);
        }
    }
    fclose(f);
    return got_fmt && got_data;
}
