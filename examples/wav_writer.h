/*
    wav_writer.h — Minimal WAV writer (16-bit PCM, stereo)
    Zero external dependencies; utility for example code
*/
#pragma once

#include <cstdint>
#include <cstdio>
#include <vector>
#include <string>
#include <algorithm>
#include <cmath>

namespace wav {

inline bool write(const std::string& path,
                  const std::vector<std::vector<float>>& channels,
                  int sampleRate)
{
    if (channels.empty() || channels[0].empty()) return false;

    const int nCh = (int)channels.size();
    const int nSamples = (int)channels[0].size();
    const int bitsPerSample = 16;
    const int byteRate = sampleRate * nCh * (bitsPerSample / 8);
    const int blockAlign = nCh * (bitsPerSample / 8);
    const int dataSize = nSamples * blockAlign;

    FILE* f = fopen(path.c_str(), "wb");
    if (!f) return false;

    auto write16 = [&](uint16_t v) { fwrite(&v, 2, 1, f); };
    auto write32 = [&](uint32_t v) { fwrite(&v, 4, 1, f); };

    // RIFF header
    fwrite("RIFF", 1, 4, f);
    write32((uint32_t)(36 + dataSize));
    fwrite("WAVE", 1, 4, f);

    // fmt chunk
    fwrite("fmt ", 1, 4, f);
    write32(16);
    write16(1); // PCM
    write16((uint16_t)nCh);
    write32((uint32_t)sampleRate);
    write32((uint32_t)byteRate);
    write16((uint16_t)blockAlign);
    write16((uint16_t)bitsPerSample);

    // data chunk
    fwrite("data", 1, 4, f);
    write32((uint32_t)dataSize);

    for (int i = 0; i < nSamples; ++i)
    {
        for (int ch = 0; ch < nCh; ++ch)
        {
            float s = std::max(-1.0f, std::min(1.0f, channels[(size_t)ch][(size_t)i]));
            int16_t v = (int16_t)(s * 32767.0f);
            fwrite(&v, 2, 1, f);
        }
    }

    fclose(f);
    return true;
}

// Test signal: short sine burst (ping-pong)
inline std::vector<float> sineBurst(int sampleRate, float freqHz,
                                     float durationSec, float amplitude = 0.8f,
                                     float burstMs = 20.0f, float periodMs = 400.0f)
{
    const int total = (int)(sampleRate * durationSec);
    const int burstLen = (int)(sampleRate * burstMs / 1000.0f);
    const int periodLen = (int)(sampleRate * periodMs / 1000.0f);
    std::vector<float> out((size_t)total, 0.0f);

    for (int i = 0; i < total; ++i)
    {
        int posInPeriod = i % periodLen;
        if (posInPeriod < burstLen)
        {
            float env = 0.5f * (1.0f - std::cos(3.14159265f * (float)posInPeriod / (float)burstLen));
            out[(size_t)i] = amplitude * env *
                std::sin(2.0f * 3.14159265f * freqHz * (float)i / (float)sampleRate);
        }
    }
    return out;
}

} // namespace wav
