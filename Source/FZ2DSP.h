#pragma once
// FZ-2 Sim - DSP core (Super-Fuzz family octave fuzz with a three-way mode switch).
//
// Public information used: closely related to the 1968 Univox / Shin-ei Super-Fuzz; a differential pair
// gives subtle octaving "under the right conditions"; a discrete op-amp style input stage feeds the circuit;
// controls Gain, Level, Bass, Treble and a Mode switch with Fuzz I (end of the fuzz circuit), Fuzz II (same
// signal through a mid scoop) and Boost (taken right after the input stage, clean).
// Transistor biasing, exact tone-stack values and the Bass/Treble corner frequencies are NOT published here,
// so those are informed approximations. This is a behavioural model, not a schematic-exact copy.
//
//   in -> op-amp gain stage (Gain) -> [Boost tap]
//      -> asymmetric transistor clipper -> differential pair (partial rectifier = octave)
//      -> second clipper -> [Fuzz I tap] -> mid scoop -> [Fuzz II tap]
//   tap mix -> Bass band + Treble shelf -> Level -> out

#include <juce_dsp/juce_dsp.h>
#include <algorithm>
#include <array>
#include <cmath>

namespace fz2
{
constexpr float kTwoPi = 6.283185307179586f;

enum Mode { FuzzI = 0, FuzzII = 1, Boost = 2, FuzzBoth = 3 };

struct Params
{
    float inputDb  = 0.0f;   // host-side input trim, -24..+24
    float gain     = 0.6f;   // 0..1
    float level    = 0.5f;   // 0..1 (0.5 ~ unity)
    float bass     = 0.5f;   // 0..1 (0.5 flat)
    float treble   = 0.5f;   // 0..1 (0.5 flat)
    float octave   = 0.4f;   // 0..1 how strongly the differential pair rectifies (extra control)
    float outputDb = 0.0f;   // host-side trim
    int   mode     = FuzzI;
    bool  on       = true;
};

struct OnePole
{
    float a = 0.0f, z = 0.0f;
    void setCutoff (float fc, float fs) { a = 1.0f - std::exp (-kTwoPi * fc / fs); }
    float lp (float x) { z += a * (x - z); return z; }
    float hp (float x) { return x - lp (x); }
    void reset() { z = 0.0f; }
};

struct Biquad
{
    float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;

    float process (float x)
    {
        const float y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }
    void reset() { z1 = z2 = 0.0f; }

    void set (float nb0, float nb1, float nb2, float na0, float na1, float na2)
    {
        const float r = 1.0f / na0;
        b0 = nb0 * r; b1 = nb1 * r; b2 = nb2 * r; a1 = na1 * r; a2 = na2 * r;
    }
    void highShelf (float fs, float f0, float dB)
    {
        const float A = std::pow (10.0f, dB / 40.0f), w = kTwoPi * f0 / fs, c = std::cos (w);
        const float al = std::sin (w) * 0.5f * std::sqrt (2.0f), t = 2.0f * std::sqrt (A) * al;
        set (A * ((A + 1) + (A - 1) * c + t), -2 * A * ((A - 1) + (A + 1) * c), A * ((A + 1) + (A - 1) * c - t),
             (A + 1) - (A - 1) * c + t, 2 * ((A - 1) - (A + 1) * c), (A + 1) - (A - 1) * c - t);
    }
    void peak (float fs, float f0, float dB, float Q)
    {
        const float A = std::pow (10.0f, dB / 40.0f), w = kTwoPi * f0 / fs, c = std::cos (w);
        const float al = std::sin (w) / (2.0f * Q);
        set (1 + al * A, -2 * c, 1 - al * A, 1 + al / A, -2 * c, 1 - al / A);
    }
};

struct Coeffs
{
    float inGain = 1, g1 = 1, opA = 0.5f, octave = 0.4f, volGain = 1, outGain = 1, onMix = 1;
    float wClean = 0, wF1 = 1, wF2 = 0;
};

// +-4.5 V supply rails of the 9 V single-supply op-amp style stage (soft corner)
inline float railSoft (float x)
{
    const float s = x * (1.0f / 4.5f), s2 = s * s;
    return 4.5f * s / std::sqrt (std::sqrt (1.0f + s2 * s2));
}

class Channel
{
public:
    void prepare (float fsOS)
    {
        fs = fsOS;
        dcIn.setCutoff (10.0f, fs);
        inHP.setCutoff (25.0f, fs);
        fuzzHP.setCutoff (90.0f, fs);
        s1LP.setCutoff (9000.0f, fs);
        octHP.setCutoff (60.0f, fs);
        octLP.setCutoff (7000.0f, fs);
        fuzzLP.setCutoff (6500.0f, fs);
        outHP.setCutoff (20.0f, fs);
        scoop.peak (fs, 650.0f, -12.0f, 0.7f);
        reset();
    }

    void reset()
    {
        for (auto* f : { &dcIn, &inHP, &opLP, &fuzzHP, &s1LP, &octHP, &octLP, &fuzzLP, &outHP })
            f->reset();
        for (auto* b : { &scoop, &bassBand, &trebleShelf })
            b->reset();
    }

    void setTone (float bass, float treble)
    {
        bassBand.peak (fs, 100.0f, -12.0f + 24.0f * bass, 2.87f);       // about half an octave wide
        trebleShelf.highShelf (fs, 2500.0f, -12.0f + 24.0f * treble);
    }

    float process (float x, const Coeffs& c)
    {
        const float in = dcIn.hp (x);

        // ---- input stage (discrete op-amp style): gain, bandwidth limit, rails ----
        float v = inHP.hp (in * c.inGain) * c.g1;
        v = opLP.lp (v);
        v = railSoft (v);
        const float clean = v * 0.4f;                       // Boost tap: right after the input stage

        // ---- Super-Fuzz style fuzz section ----
        float f = fuzzHP.hp (v);
        f = s1LP.lp (f);
        f = std::tanh (1.8f * f + 0.35f) - kTanhBias;       // asymmetric transistor clipper (even harmonics)

        // differential pair: linear + rectified outputs summed. Rectified part is sqrt(v^2+e^2): a pure
        // 2nd-harmonic term for small signals and a true full-wave rectifier (octave up) for large ones.
        const float e = 0.12f;
        const float rect = std::sqrt (f * f + e * e) - e;
        float o = f * (1.0f - 0.5f * c.octave) + rect * (1.6f * c.octave);
        o = octLP.lp (octHP.hp (o));

        o = std::tanh (3.0f * o);                           // second clipping stage
        o = fuzzLP.lp (o);

        const float fuzz1 = o * 0.6f;                       // Fuzz I tap
        const float fuzz2 = scoop.process (o) * 0.9f;       // Fuzz II tap: through the mid scoop

        float y = c.wClean * clean + c.wF1 * fuzz1 + c.wF2 * fuzz2;
        y = bassBand.process (trebleShelf.process (y));
        y = outHP.hp (y) * c.volGain * c.outGain;

        const float out = in + c.onMix * (y - in);          // footswitch crossfade
        if (! std::isfinite (out))
        {
            reset();
            return 0.0f;
        }
        return std::clamp (out, -4.0f, 4.0f);
    }

    OnePole opLP; // coefficient set per chunk (bandwidth falls as gain rises)

private:
    static inline const float kTanhBias = std::tanh (0.35f);
    float fs = 176400.0f;
    OnePole dcIn, inHP, fuzzHP, s1LP, octHP, octLP, fuzzLP, outHP;
    Biquad scoop, bassBand, trebleShelf;
};

class Engine
{
public:
    static constexpr int kChunk = 32;

    void prepare (double sampleRate)
    {
        fsOS = (float) sampleRate * 4.0f;
        os.reset();
        os.initProcessing ((size_t) kChunk);
        for (auto& c : channels)
            c.prepare (fsOS);
        for (auto* s : { &smIn, &smGain, &smLevel, &smBass, &smTreble, &smOct, &smOut, &smOn, &smWc, &smW1, &smW2 })
            s->reset (sampleRate, 0.03);
        snap = true;
    }

    void reset()
    {
        os.reset();
        for (auto& c : channels) c.reset();
        snap = true;
    }

    int getLatencySamples() const { return (int) std::lround (os.getLatencyInSamples()); }

    void process (juce::AudioBuffer<float>& buffer, const Params& p)
    {
        const int numCh = std::min (buffer.getNumChannels(), 2);
        const int n = buffer.getNumSamples();
        if (numCh == 0 || n == 0) return;

        auto set = [this] (juce::SmoothedValue<float>& s, float v)
        {
            if (snap) s.setCurrentAndTargetValue (v); else s.setTargetValue (v);
        };

        float wc = 0, w1 = 1, w2 = 0;
        switch (p.mode)
        {
            case FuzzII:   wc = 0; w1 = 0;    w2 = 1;    break;
            case Boost:    wc = 1; w1 = 0;    w2 = 0;    break;
            case FuzzBoth: wc = 0; w1 = 0.6f; w2 = 0.6f; break; // switch left between the two fuzz positions
            default:       wc = 0; w1 = 1;    w2 = 0;    break;
        }

        set (smIn, p.inputDb);  set (smGain, p.gain);   set (smLevel, p.level);
        set (smBass, p.bass);   set (smTreble, p.treble); set (smOct, p.octave);
        set (smOut, p.outputDb); set (smOn, p.on ? 1.0f : 0.0f);
        set (smWc, wc); set (smW1, w1); set (smW2, w2);
        snap = false;

        for (int pos = 0; pos < n; pos += kChunk)
        {
            const int len = std::min (kChunk, n - pos);

            Coeffs c;
            c.inGain = std::pow (10.0f, smIn.skip (len) / 20.0f);
            c.g1 = std::pow (10.0f, smGain.skip (len) * 40.0f / 20.0f);       // 0..+40 dB
            const float fcOp = std::min (1.5e6f / c.g1, 0.45f * fsOS);       // gain-bandwidth product
            c.opA = 1.0f - std::exp (-kTwoPi * fcOp / fsOS);
            const float lv = smLevel.skip (len) * 2.0f;
            c.volGain = lv * lv;
            c.octave = smOct.skip (len);
            c.outGain = std::pow (10.0f, smOut.skip (len) / 20.0f);
            c.onMix = smOn.skip (len);
            c.wClean = smWc.skip (len); c.wF1 = smW1.skip (len); c.wF2 = smW2.skip (len);
            const float bass = smBass.skip (len), treble = smTreble.skip (len);

            for (int ch = 0; ch < numCh; ++ch)
            {
                channels[(size_t) ch].opLP.a = c.opA;
                channels[(size_t) ch].setTone (bass, treble);
            }

            juce::dsp::AudioBlock<float> block (buffer.getArrayOfWritePointers(), (size_t) numCh, (size_t) pos, (size_t) len);
            auto up = os.processSamplesUp (block);
            for (int ch = 0; ch < numCh; ++ch)
            {
                float* d = up.getChannelPointer ((size_t) ch);
                for (size_t i = 0; i < up.getNumSamples(); ++i)
                    d[i] = channels[(size_t) ch].process (d[i], c);
            }
            os.processSamplesDown (block);
        }
    }

private:
    float fsOS = 176400.0f;
    bool snap = true;
    std::array<Channel, 2> channels;
    juce::dsp::Oversampling<float> os { 2, 2, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, true };
    juce::SmoothedValue<float> smIn, smGain, smLevel, smBass, smTreble, smOct, smOut, smOn, smWc, smW1, smW2;
};
} // namespace fz2
