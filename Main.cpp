// Headless checks for Pinky Lite 2: DSP sanity, state round-trip and UI snapshots.
// Usage: PinkyTests [output-dir-for-png-snapshots]
// Author: seoulbby
#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace juce;

static int failures = 0;
#define CHECK(cond, msg) do { if (! (cond)) { ++failures; std::cout << "FAIL: " << msg << "\n"; } else std::cout << "ok:   " << msg << "\n"; } while (0)

static void setParam (PinkyProcessor& p, const char* id, float actual)
{
    auto* prm = p.apvts.getParameter (id);
    prm->setValueNotifyingHost (prm->convertTo0to1 (actual));
}

static AudioBuffer<float> makeSine (int n, float amp, float hz, double sr)
{
    AudioBuffer<float> b (2, n);
    for (int i = 0; i < n; ++i)
        for (int c = 0; c < 2; ++c) b.setSample (c, i, amp * std::sin (MathConstants<float>::twoPi * hz * (float) i / (float) sr));
    return b;
}

static void run (PinkyProcessor& p, AudioBuffer<float>& b, int block)
{
    MidiBuffer m;
    for (int pos = 0; pos < b.getNumSamples(); pos += block)
    {
        const int len = jmin (block, b.getNumSamples() - pos);
        AudioBuffer<float> sub (b.getArrayOfWritePointers(), b.getNumChannels(), pos, len);
        p.processBlock (sub, m);
    }
}

static bool finiteAndBounded (const AudioBuffer<float>& b, float limit)
{
    for (int c = 0; c < b.getNumChannels(); ++c)
        for (int i = 0; i < b.getNumSamples(); ++i)
        {
            const float v = b.getSample (c, i);
            if (! std::isfinite (v) || std::abs (v) > limit) return false;
        }
    return true;
}

int main (int argc, char** argv)
{
    ScopedJuceInitialiser_GUI gui;
    const double sr = 48000.0;

    {
        // everything cranked: finite, bounded, for every glitch type and shimmer interval
        PinkyProcessor p;
        p.setPlayConfigDetails (2, 2, sr, 512);
        p.prepareToPlay (sr, 512);
        bool allOk = true;
        for (int t = 0; t < 5; ++t)
            for (float st : { -12.0f, 0.0f, 7.0f, 12.0f })
            {
                setParam (p, "gl_type", (float) t); setParam (p, "gl_amt", 100.0f); setParam (p, "gl_chance", 100.0f);
                setParam (p, "shim_amt", 100.0f); setParam (p, "shim_pitch", st); setParam (p, "shim_decay", 100.0f);
                setParam (p, "glass_amt", 100.0f); setParam (p, "glass_decay", 100.0f); setParam (p, "wob_depth", 100.0f);
                setParam (p, "wob_rate", 10.0f); setParam (p, "crush_bits", 5.0f); setParam (p, "crush_rate", 7.0f);
                setParam (p, "filter_type", 3.0f); setParam (p, "reso", 95.0f);
                setParam (p, "lfo_wob", 100.0f); setParam (p, "lfo_shim", 100.0f); setParam (p, "lfo_glass", 100.0f); setParam (p, "lfo_cutoff", 100.0f);
                setParam (p, "lfo_shape", (float) ((t + (int) st + 12) % 6));
                auto b = makeSine (48000, 0.9f, 220.0f, sr);
                run (p, b, 480);
                allOk &= finiteAndBounded (b, 1.01f);
            }
        CHECK (allOk, "all glitch types and shimmer intervals stay finite and below the safety ceiling");

        {
            auto b = makeSine (4096 + 77, 0.5f, 440.0f, sr);
            run (p, b, 4096 + 77);
            CHECK (finiteAndBounded (b, 1.01f), "oversized host block handled");
        }
    }

    {
        // shimmer feedback must decay after the input stops (no runaway), even at maximum settings
        PinkyProcessor p;
        p.setPlayConfigDetails (2, 2, sr, 512);
        p.prepareToPlay (sr, 512);
        setParam (p, "shim_amt", 100.0f); setParam (p, "shim_pitch", 12.0f); setParam (p, "shim_decay", 100.0f);
        setParam (p, "wob_depth", 0.0f); setParam (p, "glass_amt", 0.0f); setParam (p, "mix", 100.0f);
        auto burst = makeSine (24000, 0.6f, 440.0f, sr); run (p, burst, 512);
        float early = 0, late = 0;
        for (int blk = 0; blk < 60; ++blk)
        {
            AudioBuffer<float> silence (2, 48000 / 6); silence.clear(); run (p, silence, 512);
            float pk = 0; for (int i = 0; i < silence.getNumSamples(); ++i) pk = jmax (pk, std::abs (silence.getSample (0, i)));
            if (blk == 1) early = pk;
            if (blk == 59) late = pk;
        }
        std::cout << "      tail peak after 0.3s: " << early << "  after 10s: " << late << "\n";
        CHECK (early > 0.001f, "shimmer produces a tail");
        CHECK (late < early * 0.3f, "shimmer tail decays (stable feedback)");
    }

    {
        // glass resonators ring on an in-key note but much less on an out-of-key note
        auto ringEnergy = [&] (float hz)
        {
            PinkyProcessor p;
            p.setPlayConfigDetails (2, 2, sr, 512);
            p.prepareToPlay (sr, 512);
            setParam (p, "wob_depth", 0.0f); setParam (p, "shim_amt", 0.0f); setParam (p, "glass_amt", 100.0f);
            setParam (p, "glass_decay", 80.0f); setParam (p, "glass_key", 0.0f); setParam (p, "glass_scale", 0.0f);
            setParam (p, "mix", 100.0f);
            auto burst = makeSine (4800, 0.3f, hz, sr); run (p, burst, 512);
            AudioBuffer<float> tail (2, 24000); tail.clear(); run (p, tail, 512);
            double e = 0; for (int i = 4800; i < 24000; ++i) e += tail.getSample (0, i) * tail.getSample (0, i);
            return e;
        };
        const double inKey = ringEnergy (392.0f);     // G4, in C major
        const double outKey = ringEnergy (369.99f);   // F#4, not in C major
        std::cout << "      glass ring energy in-key " << inKey << " out-of-key " << outKey << "\n";
        CHECK (inKey > outKey * 3.0, "glass rings much longer on in-scale notes");
    }

    {
        // repeatable glitches: two identical runs give identical audio
        auto render = [&]
        {
            PinkyProcessor p;
            p.setPlayConfigDetails (2, 2, sr, 512);
            p.prepareToPlay (sr, 512);
            p.settings.set ("repeat", true);
            setParam (p, "wob_depth", 0.0f); setParam (p, "shim_amt", 0.0f); setParam (p, "glass_amt", 0.0f);
            setParam (p, "gl_amt", 100.0f); setParam (p, "gl_chance", 60.0f); setParam (p, "gl_div", 2.0f); setParam (p, "gl_type", 4.0f);
            auto b = makeSine (96000, 0.4f, 330.0f, sr); run (p, b, 512); return b;
        };
        auto a = render(), b = render();
        float maxDiff = 0; for (int i = 0; i < a.getNumSamples(); ++i) maxDiff = jmax (maxDiff, std::abs (a.getSample (0, i) - b.getSample (0, i)));
        CHECK (maxDiff == 0.0f, "repeatable glitches are deterministic");

        // and glitching actually changes the signal
        PinkyProcessor p;
        p.setPlayConfigDetails (2, 2, sr, 512);
        p.prepareToPlay (sr, 512);
        setParam (p, "wob_depth", 0.0f); setParam (p, "shim_amt", 0.0f); setParam (p, "glass_amt", 0.0f); setParam (p, "gl_amt", 0.0f);
        auto clean = makeSine (96000, 0.4f, 330.0f, sr); auto src = clean; run (p, clean, 512);
        float dif = 0; for (int i = 0; i < a.getNumSamples(); ++i) dif = jmax (dif, std::abs (a.getSample (0, i) - clean.getSample (0, i)));
        CHECK (dif > 0.05f, "glitch audibly alters the signal");
        float cleanErr = 0; for (int i = 4800; i < src.getNumSamples(); ++i) cleanErr = jmax (cleanErr, std::abs (src.getSample (0, i) - clean.getSample (0, i)));
        std::cout << "      all modules off, max deviation from input " << cleanErr << "\n";
        CHECK (cleanErr < 1.0e-3f, "all modules off is transparent");
    }

    {
        // wobble moves pitch: zero crossings per second change with depth but stay in a sane range
        PinkyProcessor p;
        p.setPlayConfigDetails (2, 2, sr, 512);
        p.prepareToPlay (sr, 512);
        setParam (p, "shim_amt", 0.0f); setParam (p, "glass_amt", 0.0f); setParam (p, "gl_amt", 0.0f);
        setParam (p, "wob_depth", 100.0f); setParam (p, "wob_rate", 2.0f);
        auto b = makeSine (96000, 0.4f, 440.0f, sr); run (p, b, 512);
        CHECK (finiteAndBounded (b, 1.01f), "wobble output sane");
        int crossings = 0; for (int i = 5000; i < 96000; ++i) if ((b.getSample (0, i - 1) < 0) != (b.getSample (0, i) < 0)) ++crossings;
        const double hz = crossings / 2.0 / ((96000 - 5000) / sr);
        std::cout << "      mean frequency under wobble: " << hz << " Hz (input 440)\n";
        CHECK (hz > 380.0 && hz < 500.0, "wobble keeps the mean pitch");
    }

    {
        // presets + state round trip incl. settings
        PinkyProcessor p;
        p.setPlayConfigDetails (2, 2, sr, 512);
        p.prepareToPlay (sr, 512);
        bool ok = true;
        for (int i = 0; i < p.getNumPrograms(); ++i)
        {
            p.setCurrentProgram (i);
            auto b = makeSine (4800, 0.7f, 330.0f, sr);
            run (p, b, 480);
            ok &= finiteAndBounded (b, 1.01f);
        }
        CHECK (ok, "all " + std::to_string (p.getNumPrograms()) + " presets render cleanly");

        p.setCurrentProgram (5);
        p.settings.set ("accent", 2);
        p.settings.set ("theme", 1);
        MemoryBlock mb; p.getStateInformation (mb);

        PinkyProcessor q;
        q.settings.set ("accent", 0); q.settings.set ("theme", 0);
        q.setStateInformation (mb.getData(), (int) mb.getSize());
        CHECK (q.getCurrentProgram() == 5, "program index restored");
        CHECK (q.settings.accent() == 2 && q.settings.theme() == 1, "settings restored from state");
        CHECK (std::abs (q.apvts.getRawParameterValue ("lfo_shim")->load() - p.apvts.getRawParameterValue ("lfo_shim")->load()) < 1.0e-4f, "parameters restored");
        p.settings.resetToDefaults();
    }

    // UI snapshots
    if (argc > 1)
    {
        File dir (argv[1]);
        dir.createDirectory();
        PNGImageFormat png;

        auto snap = [&] (PinkyEditor& ed, const String& name)
        {
            auto img = ed.createComponentSnapshot (ed.getLocalBounds(), true, 1.0f);
            FileOutputStream os (dir.getChildFile (name));
            os.setPosition (0); os.truncate();
            png.writeImageToStream (img, os);
            std::cout << "wrote " << name << " (" << img.getWidth() << "x" << img.getHeight() << ")\n";
        };

        PinkyProcessor p;
        p.setPlayConfigDetails (2, 2, sr, 512);
        p.prepareToPlay (sr, 512);
        p.settings.resetToDefaults();
        p.setCurrentProgram (9); // Pink Heaven
        setParam (p, "filter_type", 1.0f);

        for (int i = 0; i < 4; ++i) { auto b = makeSine (4096, 0.5f, 140.0f, sr); run (p, b, 512); }

        {
            auto ed = std::unique_ptr<PinkyEditor> (static_cast<PinkyEditor*> (p.createEditor()));
            ed->advanceForTest (1.0f, 60);
            snap (*ed, "ui_light.png");

            ed->setSettingsOpen (true);
            ed->advanceForTest (1.0f, 60);
            snap (*ed, "ui_light_settings.png");
        }
        {
            p.settings.set ("theme", 1); p.settings.set ("accent", 2);
            auto ed = std::unique_ptr<PinkyEditor> (static_cast<PinkyEditor*> (p.createEditor()));
            p.setCurrentProgram (4);
            ed->advanceForTest (1.0f, 60);
            snap (*ed, "ui_night_lilac.png");
            ed->setSettingsOpen (true);
            ed->advanceForTest (1.0f, 60);
            snap (*ed, "ui_night_settings.png");
        }
        {
            p.settings.resetToDefaults();
            p.settings.set ("accent", 3);
            p.settings.set ("uiScale", 3);
            auto ed = std::unique_ptr<PinkyEditor> (static_cast<PinkyEditor*> (p.createEditor()));
            p.setCurrentProgram (3);
            ed->advanceForTest (1.0f, 60);
            snap (*ed, "ui_denim_150.png");
        }
        p.settings.resetToDefaults();
    }

    std::cout << (failures == 0 ? "ALL PASSED\n" : "SOME FAILED\n");
    return failures == 0 ? 0 : 1;
}
