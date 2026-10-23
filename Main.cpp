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
        PinkyProcessor p;
        p.setPlayConfigDetails (2, 2, sr, 512);
        p.prepareToPlay (sr, 512);

        // every mode x quality at max drive stays finite & bounded
        bool allOk = true;
        for (int q = 0; q < 3; ++q)
            for (int m = 0; m < 8; ++m)
            {
                setParam (p, "quality", (float) q); setParam (p, "mode", (float) m);
                setParam (p, "drive", 100.0f); setParam (p, "crush_bits", 5.0f); setParam (p, "crush_rate", 7.0f);
                setParam (p, "filter_type", 3.0f); setParam (p, "reso", 95.0f);
                setParam (p, "lfo_drive", 100.0f); setParam (p, "lfo_cutoff", 100.0f); setParam (p, "lfo_crush", 100.0f);
                setParam (p, "lfo_shape", (float) (m % 6));
                auto b = makeSine (48000, 0.9f, 220.0f, sr);
                run (p, b, 480);
                allOk &= finiteAndBounded (b, 1.01f);
            }
        CHECK (allOk, "all modes/qualities finite and below the safety ceiling");

        // large host blocks (> internal chunk) work
        {
            auto b = makeSine (4096 + 77, 0.5f, 440.0f, sr);
            run (p, b, 4096 + 77);
            CHECK (finiteAndBounded (b, 1.01f), "oversized host block handled");
        }
    }

    {
        // mix = 0 keeps the level untouched (the dry path only receives an all-pass phase shift)
        PinkyProcessor p;
        p.setPlayConfigDetails (2, 2, sr, 512);
        p.prepareToPlay (sr, 512);
        setParam (p, "mix", 0.0f); setParam (p, "quality", 2.0f); setParam (p, "width", 100.0f);
        for (int i = 0; i < 8; ++i) { auto warm = makeSine (512, 0.3f, 300.0f, sr); run (p, warm, 512); }
        auto in = makeSine (16384, 0.4f, 1000.0f, sr);
        auto out = in;
        run (p, out, 512);
        double a = 0, b = 0;
        for (int i = 4096; i < 16384; ++i) { a += in.getSample (0, i) * in.getSample (0, i); b += out.getSample (0, i) * out.getSample (0, i); }
        std::cout << "      latency @4x: " << p.getLatencySamples() << " samples, level ratio " << std::sqrt (b / a) << "\n";
        CHECK (std::abs (std::sqrt (b / a) - 1.0) < 0.01, "mix=0 passes the signal at unity level");
    }

    {
        // wet path (drive 0, soft clip) lines up with the dry path -> no comb filtering when mixing
        auto render = [&] (float mix)
        {
            PinkyProcessor p;
            p.setPlayConfigDetails (2, 2, sr, 512);
            p.prepareToPlay (sr, 512);
            setParam (p, "drive", 0.0f); setParam (p, "mode", 6.0f); setParam (p, "shape", 100.0f);
            setParam (p, "mix", mix); setParam (p, "quality", 1.0f);
            auto b = makeSine (16384, 0.1f, 500.0f, sr);
            run (p, b, 512);
            return b;
        };
        auto wet = render (100.0f), dry = render (0.0f);
        double err = 0, ref = 0;
        for (int i = 4096; i < 16384; ++i)
        {
            const float d = wet.getSample (0, i) - dry.getSample (0, i);
            err += d * d; ref += dry.getSample (0, i) * dry.getSample (0, i);
        }
        std::cout << "      wet-vs-dry RMS error " << std::sqrt (err / ref) << "\n";
        CHECK (std::sqrt (err / ref) < 0.05, "drive=0 wet signal is phase-aligned with the dry path");
    }

    {
        // higher drive must actually add harmonics (output differs and is compressed)
        PinkyProcessor p;
        p.setPlayConfigDetails (2, 2, sr, 512);
        p.prepareToPlay (sr, 512);
        setParam (p, "mode", 0.0f); setParam (p, "drive", 90.0f); setParam (p, "mix", 100.0f);
        auto b = makeSine (16384, 0.5f, 200.0f, sr); run (p, b, 512);
        float pk = 0; for (int i = 8192; i < 16384; ++i) pk = jmax (pk, std::abs (b.getSample (0, i)));
        CHECK (pk > 0.05f && pk <= 1.0f, "heavy drive output is audible and limited (peak " + String (pk, 3).toStdString() + ")");
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
        CHECK (std::abs (q.apvts.getRawParameterValue ("lfo_drive")->load() - p.apvts.getRawParameterValue ("lfo_drive")->load()) < 1.0e-4f, "parameters restored");
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
        p.setCurrentProgram (5); // Glitter Pump
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
            p.setCurrentProgram (7);
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
