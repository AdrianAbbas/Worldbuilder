#pragma once
// Small real-time building blocks shared by every layer. Nothing here allocates
// after prepare().
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace wb::dsp {

constexpr float pi = 3.14159265358979f;
constexpr float twoPi = 6.28318530717959f;

// Control-rate block: layers update their parameters once per chunk.
constexpr int chunk = 32;

inline float dbToGain(float db) { return std::pow(10.0f, db * 0.05f); }
inline float clamp01(float x) { return std::clamp(x, 0.0f, 1.0f); }
inline float lerp(float a, float b, float t) { return a + (b - a) * t; }
// Exponential interpolation, for frequencies and times.
inline float lerpExp(float a, float b, float t) { return a * std::pow(b / a, t); }
inline float smoothstep(float e0, float e1, float x) {
    const float t = clamp01((x - e0) / (e1 - e0));
    return t * t * (3.0f - 2.0f * t);
}
inline float finiteOr(float x, float fallback = 0.0f) { return std::isfinite(x) ? x : fallback; }

//==============================================================================
// Randomness. Every random value is a hash of (seed, stream, index), so the world
// is a pure function of the Seed and the sample position: the same settings from
// the same start point always give the same sound, in real time or offline.
inline uint64_t mix64(uint64_t x) {
    x += 0x9E3779B97F4A7C15ull;
    x = (x ^ (x >> 30)) * 0xBF58476D1CE4E5B9ull;
    x = (x ^ (x >> 27)) * 0x94D049BB133111EBull;
    return x ^ (x >> 31);
}
inline float bipolar(uint64_t h) { return float(int64_t(h >> 40) - (int64_t(1) << 23)) * (1.0f / 8388608.0f); }   // [-1, 1)
inline float unipolar(uint64_t h) { return float(h >> 40) * (1.0f / 16777216.0f); }                               // [0, 1)

struct Stream {
    uint64_t key = 0;
    void setKey(uint32_t seed, uint32_t stream) { key = mix64((uint64_t(seed) << 32) ^ mix64(stream)); }
    float white(uint64_t index) const { return bipolar(mix64(key ^ index)); }
    float unit(uint64_t index) const { return unipolar(mix64(key ^ index)); }

    // Smooth random curve in about [-1, 1] at `rate` points per second, evaluated at
    // `seconds`. Catmull-Rom through hashed points: continuous, no state.
    float smooth(double seconds, double rate) const {
        const double t = seconds * rate;
        const double base = std::floor(t);
        const float f = float(t - base);
        const auto i = uint64_t(int64_t(base) + (int64_t(1) << 40));
        const float p0 = white(i - 1), p1 = white(i), p2 = white(i + 1), p3 = white(i + 2);
        const float a = -0.5f * p0 + 1.5f * p1 - 1.5f * p2 + 0.5f * p3;
        const float b = p0 - 2.5f * p1 + 2.0f * p2 - 0.5f * p3;
        const float c = -0.5f * p0 + 0.5f * p2;
        return ((a * f + b) * f + c) * f + p1;
    }
};

//==============================================================================
// One-pole smoother toward a target, used at audio or control rate.
struct Smoother {
    float value = 0, coeff = 1;
    void setTime(float seconds, float rate) { coeff = seconds <= 0 ? 1.0f : 1.0f - std::exp(-1.0f / (seconds * rate)); }
    float next(float target) { return value += coeff * (target - value); }
    void snap(float v) { value = v; }
};

// One-pole low-pass (and high-pass as input minus low-pass).
struct OnePole {
    float z = 0, a = 1;
    void setCutoff(float hz, float rate) { a = 1.0f - std::exp(-twoPi * std::min(hz, 0.45f * rate) / rate); }
    float lp(float x) { return z += a * (x - z); }
    float hp(float x) { return x - lp(x); }
    void reset() { z = 0; }
};

// Topology-preserving state-variable filter (Simper). Stable under fast modulation.
struct Svf {
    float ic1 = 0, ic2 = 0, a1 = 1, a2 = 0, a3 = 0, k = 1;
    struct Out { float lp, bp, hp; };
    void set(float hz, float q, float rate) {
        const float g = std::tan(pi * std::clamp(hz, 10.0f, 0.45f * rate) / rate);
        k = 1.0f / std::max(q, 0.05f);
        a1 = 1.0f / (1.0f + g * (g + k));
        a2 = g * a1;
        a3 = g * a2;
    }
    Out tick(float v0) {
        const float v3 = v0 - ic2;
        const float v1 = a1 * ic1 + a2 * v3;
        const float v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2.0f * v1 - ic1;
        ic2 = 2.0f * v2 - ic2;
        return { v2, v1, v0 - k * v1 - v2 };
    }
    // Band-pass normalised to unity gain at the centre.
    float bandpass(float x) { return k * tick(x).bp; }
    void reset() { ic1 = ic2 = 0; }
};

// Circular delay line with linear-interpolated fractional reads.
struct DelayLine {
    std::vector<float> data;
    std::size_t mask = 0, write = 0;
    void prepare(std::size_t maxSamples) {
        std::size_t size = 1;
        while (size < maxSamples + 4) size <<= 1;
        data.assign(size, 0.0f);
        mask = size - 1;
        write = 0;
    }
    void clear() { std::fill(data.begin(), data.end(), 0.0f); }
    void push(float x) { data[write] = x; write = (write + 1) & mask; }
    // delay >= 1: the sample pushed `delay` pushes ago.
    float read(float delay) const {
        const float d = std::clamp(delay, 1.0f, float(mask - 2));
        const auto whole = std::size_t(d);
        const float frac = d - float(whole);
        const float a = data[(write - whole) & mask];
        const float b = data[(write - whole - 1) & mask];
        return a + frac * (b - a);
    }
    float readInt(std::size_t delay) const { return data[(write - delay) & mask]; }
};

// Schroeder all-pass diffuser on a fixed delay.
struct Allpass {
    DelayLine line;
    std::size_t length = 1;
    float g = 0.6f;
    void prepare(std::size_t samples) { length = std::max<std::size_t>(samples, 1); line.prepare(length + 1); }
    float process(float x) {
        const float delayed = line.readInt(length);
        const float v = x + g * delayed;
        line.push(v);
        return delayed - g * v;
    }
    void clear() { line.clear(); }
};

// Complex-rotation sine oscillator: one multiply-add pair per sample. Gives sin and
// cos of the phase, so harmonics follow from the Chebyshev recurrence.
struct Phasor {
    float re = 1, im = 0, wr = 1, wi = 0;
    void setFrequency(float hz, float rate) {
        const float w = twoPi * hz / rate;
        wr = std::cos(w);
        wi = std::sin(w);
    }
    void setPhase(float radians) { re = std::cos(radians); im = std::sin(radians); }
    void step() {
        const float r = re * wr - im * wi;
        im = re * wi + im * wr;
        re = r;
    }
    // Call once per chunk: keeps the magnitude at 1 despite rounding.
    void normalise() {
        const float m = re * re + im * im;
        const float s = 1.5f - 0.5f * m;
        re *= s;
        im *= s;
    }
};

} // namespace wb::dsp
