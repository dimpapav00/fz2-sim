// Offline sanity test for the FZ-2 Sim DSP core (no GUI needed).
#include "FZ2DSP.h"
#include <cstdio>

static double goertzel (const std::vector<float>& x, double f, double fs)
{
    const double w = 2.0 * M_PI * f / fs;
    double s1 = 0, s2 = 0, c = 2.0 * std::cos (w);
    for (float v : x) { double s0 = v + c * s1 - s2; s2 = s1; s1 = s0; }
    const double p = s1 * s1 + s2 * s2 - c * s1 * s2;
    return 2.0 * std::sqrt (std::max (0.0, p)) / (double) x.size();
}

struct Result { double rms, peak, dc, fund, h2, thd; bool finite; };

static Result run (fz2::Params p, double amp, double f0, double fs = 48000.0)
{
    fz2::Engine eng;
    eng.prepare (fs);
    const int N = (int) fs, block = 512;
    juce::AudioBuffer<float> buf (2, block);
    std::vector<float> out;
    for (int pos = 0; pos < N; pos += block)
    {
        for (int i = 0; i < block; ++i)
        {
            float v = (float) (amp * std::sin (2.0 * M_PI * f0 * (pos + i) / fs));
            buf.setSample (0, i, v);
            buf.setSample (1, i, v);
        }
        eng.process (buf, p);
        for (int i = 0; i < block; ++i) out.push_back (buf.getSample (0, i));
    }
    std::vector<float> tail (out.begin() + N / 2, out.end());
    Result r { 0, 0, 0, 0, 0, 0, true };
    for (float v : tail)
    {
        if (! std::isfinite (v)) r.finite = false;
        r.rms += v * v; r.peak = std::max (r.peak, (double) std::fabs (v)); r.dc += v;
    }
    r.rms = std::sqrt (r.rms / tail.size()); r.dc /= tail.size();
    r.fund = goertzel (tail, f0, fs);
    r.h2 = goertzel (tail, 2 * f0, fs);
    double h = 0;
    for (int k = 2; k <= 7; ++k)
        if (k * f0 < fs * 0.45) { const double a = goertzel (tail, k * f0, fs); h += a * a; }
    r.thd = r.fund > 1e-9 ? std::sqrt (h) / r.fund : 0;
    return r;
}

static void show (const char* name, const Result& r)
{
    std::printf ("%-40s rms=%.3f peak=%.3f dc=%+.4f fund=%.3f h2=%.3f thd=%5.1f%% %s\n", name, r.rms, r.peak, r.dc,
                 r.fund, r.h2, r.thd * 100.0, r.finite ? "ok" : "NaN/Inf!");
}

int main()
{
    int failures = 0;
    auto check = [&failures] (bool c, const char* m) { if (! c) { std::printf ("FAIL: %s\n", m); ++failures; } };

    fz2::Params base;

    // --- bypass
    fz2::Params off = base; off.on = false;
    auto r0 = run (off, 0.1, 330.0);
    show ("bypass (0.1)", r0);
    check (std::fabs (r0.fund - 0.1) < 0.01, "bypass is transparent");

    // --- stability for every mode and input level
    const char* modeNames[4] = { "Fuzz I", "Fuzz II", "Boost", "Fuzz I+II" };
    for (int m = 0; m < 4; ++m)
        for (double a : { 0.01, 0.1, 0.5, 1.0 })
        {
            fz2::Params p = base; p.mode = m;
            char n[64]; std::snprintf (n, sizeof n, "%s, 330 Hz, in=%.2f", modeNames[m], a);
            auto r = run (p, a, 330.0); show (n, r);
            check (r.finite && r.peak < 4.01 && std::fabs (r.dc) < 0.05, "stable and DC free");
        }

    // --- Boost mode: clean at low gain, gain acts as volume
    fz2::Params bo = base; bo.mode = fz2::Boost; bo.gain = 0.2f;
    auto rbo = run (bo, 0.05, 330.0); show ("Boost, gain 0.2, in=0.05", rbo);
    check (rbo.thd < 0.03, "Boost at low gain is clean");
    fz2::Params bo2 = bo; bo2.gain = 0.8f;
    auto rb2lo = run (bo, 0.02, 330.0), rb2hi = run (bo2, 0.02, 330.0);
    show ("Boost gain 0.2, in=0.02", rb2lo); show ("Boost gain 0.8, in=0.02", rb2hi);
    check (rb2hi.fund > rb2lo.fund * 4.0, "Gain raises Boost level");

    // --- Fuzz: gain drives distortion; heavily compressed
    fz2::Params fl = base, fh = base; fl.gain = 0.1f; fh.gain = 0.9f;
    auto rfl = run (fl, 0.05, 330.0), rfh = run (fh, 0.05, 330.0);
    show ("Fuzz I gain 0.1, in=0.05", rfl); show ("Fuzz I gain 0.9, in=0.05", rfh);
    check (rfh.thd > rfl.thd, "more Gain = more distortion");
    auto rq = run (fh, 0.02, 330.0), rl = run (fh, 0.5, 330.0);
    show ("Fuzz I gain 0.9, in=0.02", rq); show ("Fuzz I gain 0.9, in=0.5", rl);
    check (rl.fund < rq.fund * 6.0, "fuzz is heavily compressed (25x input -> <6x output)");

    // --- Fuzz II: mid scoop relative to Fuzz I
    fz2::Params f1 = base, f2 = base; f2.mode = fz2::FuzzII;
    auto r650a = run (f1, 0.1, 650.0), r650b = run (f2, 0.1, 650.0);
    show ("Fuzz I  @ 650 Hz", r650a); show ("Fuzz II @ 650 Hz", r650b);
    check (r650b.fund < r650a.fund * 0.8, "Fuzz II scoops the mids");

    // --- Bass and Treble (measured in clean Boost mode)
    fz2::Params eq = bo; eq.gain = 0.2f; eq.bass = 0.0f;
    auto bLo = run (eq, 0.02, 100.0); eq.bass = 1.0f; auto bHi = run (eq, 0.02, 100.0);
    show ("Bass 0 @ 100 Hz", bLo); show ("Bass 1 @ 100 Hz", bHi);
    check (bHi.fund > bLo.fund * 4.0, "Bass knob works");
    eq.bass = 0.5f; eq.treble = 0.0f; auto tLo = run (eq, 0.02, 6000.0);
    eq.treble = 1.0f; auto tHi = run (eq, 0.02, 6000.0);
    show ("Treble 0 @ 6 kHz", tLo); show ("Treble 1 @ 6 kHz", tHi);
    check (tHi.fund > tLo.fund * 3.0, "Treble knob works");

    // --- Octave: more rectification = more 2nd harmonic relative to the fundamental
    fz2::Params o0 = base, o1 = base; o0.gain = 0.4f; o1.gain = 0.4f; o0.octave = 0.0f; o1.octave = 1.0f;
    auto ro0 = run (o0, 0.05, 330.0), ro1 = run (o1, 0.05, 330.0);
    show ("Octave 0, 330 Hz", ro0); show ("Octave 1, 330 Hz", ro1);
    check (ro1.h2 / std::max (ro1.fund, 1e-9) > 1.3 * ro0.h2 / std::max (ro0.fund, 1e-9), "Octave raises 2nd harmonic");

    // --- extremes, all modes
    for (int m = 0; m < 4; ++m)
    {
        fz2::Params mx = base; mx.mode = m; mx.gain = 1.0f; mx.level = 1.0f; mx.bass = 1.0f; mx.treble = 1.0f;
        mx.octave = 1.0f; mx.inputDb = 24.0f;
        for (double f : { 82.0, 880.0, 4000.0 })
        {
            char n[64]; std::snprintf (n, sizeof n, "max, %s @ %.0f Hz", modeNames[m], f);
            auto r = run (mx, 1.0, f); show (n, r);
            // internal clamp is +-4; the oversampler's down-sampling filter can overshoot it a little
            check (r.finite && r.peak < 6.0 && std::fabs (r.dc) < 0.1, "max settings stable (bounded, no runaway)");
        }
    }

    for (double fs : { 44100.0, 96000.0 })
    {
        auto r = run (base, 0.2, 330.0, fs);
        char n[64]; std::snprintf (n, sizeof n, "default @ %.0f Hz", fs);
        show (n, r);
        check (r.finite, "finite at other sample rates");
    }

    std::printf (failures == 0 ? "\nALL CHECKS PASSED\n" : "\n%d CHECK(S) FAILED\n", failures);
    return failures;
}
