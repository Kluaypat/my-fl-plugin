#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_dsp/juce_dsp.h>
#include <cmath>

namespace kluay
{
struct StutterSettings
{
    int repeats = 4;          // 2, 4, 8
    int rateIndex = 1;        // 0 = 1/16, 1 = 1/8
    float fadeIn = 0.6f;      // 0..1
    float pan = 0.6f;         // 0..1 ping-pong width
    bool phone = true;
    bool chorus = false;
    bool backverb = false;
    double bpm = 140.0;
    double sampleRate = 44100.0;
};

struct RenderResult
{
    juce::AudioBuffer<float> before, after, beat;   // all stereo, same length
    int length = 0;
    int landing = 0;
    double sampleRate = 44100.0;
    double bpm = 140.0;
};

namespace engine
{
inline void edgeFades (juce::AudioBuffer<float>& b, double sr)
{
    const int n = b.getNumSamples();
    const int fi = juce::jmin (n / 4, (int) (0.001 * sr));
    const int fo = juce::jmin (n / 4, (int) (0.004 * sr));
    for (int ch = 0; ch < b.getNumChannels(); ++ch)
    {
        auto* d = b.getWritePointer (ch);
        for (int i = 0; i < fi; ++i) d[i] *= (float) i / (float) fi;
        for (int i = 0; i < fo; ++i) d[n - 1 - i] *= (float) i / (float) fo;
    }
}

// Telephone-style band-pass + a little grit
inline void applyPhone (juce::AudioBuffer<float>& b, double sr)
{
    auto hp = juce::dsp::IIR::Coefficients<float>::makeHighPass (sr, 450.0f);
    auto lp = juce::dsp::IIR::Coefficients<float>::makeLowPass (sr, 3200.0f);
    for (int ch = 0; ch < b.getNumChannels(); ++ch)
    {
        juce::dsp::IIR::Filter<float> h1 (hp), h2 (hp), l1 (lp), l2 (lp);
        auto* d = b.getWritePointer (ch);
        for (int i = 0; i < b.getNumSamples(); ++i)
        {
            float x = l2.processSample (l1.processSample (h2.processSample (h1.processSample (d[i]))));
            d[i] = std::tanh (x * 2.2f) * 0.8f;
        }
    }
}

// Two-voice modulated delay chorus
inline void applyChorus (juce::AudioBuffer<float>& b, double sr)
{
    juce::AudioBuffer<float> src (b);
    const int n = b.getNumSamples();
    const double twoPi = juce::MathConstants<double>::twoPi;
    for (int ch = 0; ch < juce::jmin (2, b.getNumChannels()); ++ch)
    {
        const float* in = src.getReadPointer (ch);
        float* out = b.getWritePointer (ch);
        for (int i = 0; i < n; ++i)
        {
            auto tap = [&] (double base, double depth, double rate, double ph) -> float
            {
                const double d = (base + depth * std::sin (twoPi * rate * (double) i / sr + ph)) * sr;
                const double p = (double) i - d;
                if (p < 0.0) return 0.0f;
                const int i0 = (int) p;
                const float fr = (float) (p - (double) i0);
                const int i1 = juce::jmin (i0 + 1, n - 1);
                return in[i0] * (1.0f - fr) + in[i1] * fr;
            };
            const float a = tap (0.014, 0.005, 0.8, ch ? 1.7 : 0.0);
            const float c = tap (0.022, 0.006, 1.1, ch ? 0.4 : 2.2);
            out[i] = in[i] * 0.65f + (a + c) * 0.42f;
        }
    }
}

// Reverse-reverb swell that leads into the stutter
inline void buildBackverb (const juce::AudioBuffer<float>& word, int revLen, double beatLen,
                           const StutterSettings& s, juce::AudioBuffer<float>& rev)
{
    rev.setSize (2, revLen);
    rev.clear();
    const int feed = juce::jmin (word.getNumSamples(), juce::jmin ((int) (beatLen * 0.5), revLen / 2));
    if (feed < 32) return;

    // reversed word at the start of the buffer
    for (int ch = 0; ch < 2; ++ch)
        for (int i = 0; i < feed; ++i)
            rev.setSample (ch, i, word.getSample (ch, feed - 1 - i));

    juce::Reverb reverb;
    reverb.setSampleRate (s.sampleRate);
    juce::Reverb::Parameters p;
    p.roomSize = 0.85f; p.damping = 0.35f; p.wetLevel = 1.0f; p.dryLevel = 0.0f; p.width = 1.0f; p.freezeMode = 0.0f;
    reverb.setParameters (p);
    reverb.processStereo (rev.getWritePointer (0), rev.getWritePointer (1), revLen);

    for (int ch = 0; ch < 2; ++ch)
        rev.reverse (ch, 0, revLen);

    // level match + gentle swell + tiny fade at the end
    const float wordPeak = word.getMagnitude (0, word.getNumSamples());
    const float revPeak = rev.getMagnitude (0, revLen);
    if (revPeak > 1.0e-6f)
        rev.applyGain (0.55f * wordPeak / revPeak);
    const int fo = juce::jmin (revLen / 4, (int) (0.003 * s.sampleRate));
    for (int ch = 0; ch < 2; ++ch)
    {
        auto* d = rev.getWritePointer (ch);
        for (int i = 0; i < revLen; ++i)
        {
            const float t = (float) i / (float) revLen;
            d[i] *= t * t;
        }
        for (int i = 0; i < fo; ++i) d[revLen - 1 - i] *= (float) i / (float) fo;
    }
}

inline void addKick (juce::AudioBuffer<float>& b, int start, double sr)
{
    const int len = (int) (0.28 * sr);
    double phase = 0.0;
    for (int i = 0; i < len && start + i < b.getNumSamples(); ++i)
    {
        const double t = (double) i / sr;
        phase += juce::MathConstants<double>::twoPi * (46.0 + 110.0 * std::exp (-t * 30.0)) / sr;
        const float v = (float) (std::sin (phase) * std::exp (-t * 7.5)) * 0.95f;
        b.addSample (0, start + i, v);
        b.addSample (1, start + i, v);
    }
}

inline void addSnare (juce::AudioBuffer<float>& b, int start, double sr, juce::Random& rng)
{
    const int len = (int) (0.22 * sr);
    for (int i = 0; i < len && start + i < b.getNumSamples(); ++i)
    {
        const double t = (double) i / sr;
        const float noise = (rng.nextFloat() * 2.0f - 1.0f) * (float) std::exp (-t * 20.0);
        const float tone = (float) (std::sin (juce::MathConstants<double>::twoPi * 185.0 * t) * std::exp (-t * 28.0));
        const float v = noise * 0.5f + tone * 0.4f;
        b.addSample (0, start + i, v);
        b.addSample (1, start + i, v);
    }
}

inline void addHat (juce::AudioBuffer<float>& b, int start, double sr, juce::Random& rng, float level, float panL)
{
    const int len = (int) (0.07 * sr);
    float prev = 0.0f;
    for (int i = 0; i < len && start + i < b.getNumSamples(); ++i)
    {
        const double t = (double) i / sr;
        const float n = rng.nextFloat() * 2.0f - 1.0f;
        const float hp = n - prev;
        prev = n;
        const float v = hp * (float) std::exp (-t * 85.0) * level;
        b.addSample (0, start + i, v * panL);
        b.addSample (1, start + i, v * (2.0f - panL));
    }
}

inline void makeBeat (juce::AudioBuffer<float>& beat, int total, double bpm, double sr)
{
    beat.setSize (2, total);
    beat.clear();
    juce::Random rng (1234);
    const double beatLen = 60.0 * sr / bpm;
    const int nBeats = (int) std::ceil ((double) total / beatLen);
    for (int k = 0; k < nBeats; ++k)
    {
        const int pos = (int) std::lround ((double) k * beatLen);
        const int half = (int) std::lround (((double) k + 0.5) * beatLen);
        const int m = k % 4;
        if (m == 0 || m == 2) addKick (beat, pos, sr);
        if (m == 3) addKick (beat, half, sr);
        if (m == 1 || m == 3) addSnare (beat, pos, sr, rng);
        addHat (beat, pos, sr, rng, 0.22f, 0.8f);
        addHat (beat, half, sr, rng, 0.14f, 1.2f);
    }
    const float mag = beat.getMagnitude (0, total);
    if (mag > 1.0e-6f) beat.applyGain (0.35f / mag);
}

inline void render (const juce::AudioBuffer<float>& wordIn, const StutterSettings& s, RenderResult& out)
{
    const double sr = s.sampleRate;
    const double beatLen = 60.0 * sr / s.bpm;
    const double slotD = beatLen * (s.rateIndex == 0 ? 0.25 : 0.5);
    const int reps = juce::jmax (1, s.repeats);

    juce::AudioBuffer<float> word (wordIn);
    edgeFades (word, sr);
    const int W = word.getNumSamples();
    const int slot = juce::jmax (64, (int) std::lround (slotD));
    const int sliceLen = juce::jmin (slot, W);

    const double stutterLenD = slotD * reps;
    const int stutterLen = (int) std::lround (stutterLenD);
    // the word always lands on a beat, with at least one beat of lead-in
    const double landingD = std::ceil ((stutterLenD + beatLen) / beatLen - 1.0e-9) * beatLen;
    const int landing = (int) std::lround (landingD);
    const int stutterStart = landing - stutterLen;

    const double barLen = beatLen * 4.0;
    const int total = (int) std::lround (std::ceil (((double) landing + (double) W + beatLen * 0.5) / barLen) * barLen);

    out.sampleRate = sr;
    out.bpm = s.bpm;
    out.length = total;
    out.landing = landing;

    // BEFORE: the plain word on the beat
    out.before.setSize (2, total);
    out.before.clear();
    for (int ch = 0; ch < 2; ++ch)
        out.before.copyFrom (ch, landing, word, ch, 0, juce::jmin (W, total - landing));

    // stutter repeats
    juce::AudioBuffer<float> stut (2, stutterLen + 1);
    stut.clear();
    const float pi = juce::MathConstants<float>::pi;
    for (int r = 0; r < reps; ++r)
    {
        const int at = (int) std::lround ((double) r * slotD);
        const int len = juce::jmin (sliceLen, stutterLen - at);
        if (len <= 0) continue;

        const float g = 1.0f - s.fadeIn * (1.0f - (float) (r + 1) / (float) reps);
        const float pos = s.pan * ((r % 2 == 0) ? -1.0f : 1.0f);
        const float ang = (pos + 1.0f) * pi * 0.25f;
        const float gl = std::cos (ang) * 1.41421356f;
        const float gr = std::sin (ang) * 1.41421356f;
        const int fi = juce::jmax (1, juce::jmin (len / 4, (int) (0.001 * sr)));
        const int fo = juce::jmax (1, juce::jmin (len / 3, (int) (0.004 * sr)));

        for (int i = 0; i < len; ++i)
        {
            float env = g;
            if (i < fi) env *= (float) i / (float) fi;
            if (i >= len - fo) env *= (float) (len - i) / (float) fo;
            stut.addSample (0, at + i, word.getSample (0, i) * env * gl);
            stut.addSample (1, at + i, word.getSample (1, i) * env * gr);
        }
    }
    if (s.phone)  applyPhone (stut, sr);
    if (s.chorus) applyChorus (stut, sr);

    out.after.setSize (2, total);
    out.after.clear();

    if (s.backverb && stutterStart > 256)
    {
        juce::AudioBuffer<float> rev;
        buildBackverb (word, stutterStart, beatLen, s, rev);
        if (s.phone)  applyPhone (rev, sr);
        if (s.chorus) applyChorus (rev, sr);
        for (int ch = 0; ch < 2; ++ch)
            out.after.addFrom (ch, 0, rev, ch, 0, juce::jmin (stutterStart, total));
    }

    for (int ch = 0; ch < 2; ++ch)
    {
        out.after.addFrom (ch, stutterStart, stut, ch, 0, juce::jmin (stutterLen, total - stutterStart));
        out.after.addFrom (ch, landing, word, ch, 0, juce::jmin (W, total - landing));
    }

    // same gain on both so A/B level is honest
    const float peak = juce::jmax (out.before.getMagnitude (0, total), out.after.getMagnitude (0, total));
    if (peak > 1.0e-5f)
    {
        const float g = 0.7f / peak;
        out.before.applyGain (g);
        out.after.applyGain (g);
    }

    makeBeat (out.beat, total, s.bpm, sr);
}
} // namespace engine
} // namespace kluay
