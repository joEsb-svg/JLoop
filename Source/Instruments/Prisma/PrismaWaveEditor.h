#pragma once

#include <JuceHeader.h>
#include <functional>
#include "Prisma.h"
#include "../FrenchRoulette/FrenchRouletteEditor.h"
#include "../../Platform/OpenWith.h"

//==============================================================================
/**
    "LE MIE ONDE" di Prisma: si disegnano le proprie wavetable.

    - Una tabella ha da 1 a 32 FORME CHIAVE; Prisma sfuma da una all'altra quando si muove la posizione.
    - Ogni forma si cambia in due modi: ONDA (la si disegna col dito: matita o linea) o ARMONICHE
      (una barra per armonica, da alzare e abbassare). Sotto si vede sempre l'altra vista.
    - "Da file audio": JLoop trova l'intonazione del suono e ne prende 32 cicli, dall'inizio alla fine.
    - Mentre si lavora la tabella suona gia' (l'oscillatore passa su "Mia N"): Salva la tiene,
      Annulla rimette tutto com'era.
*/
class PrismaWaveEditor : public juce::Component
{
public:
    static constexpr int N = prisma::frameSize;
    static constexpr int numBars = 32;

    PrismaWaveEditor (PrismaAudioProcessor& p, int oscIndex)
        : proc (p), osc (oscIndex), tableId (oscIndex == 0 ? "aTable" : "bTable")
    {
        auto& bank = prisma::Bank::get();
        startTable = (int) proc.apvts.getRawParameterValue (tableId)->load();
        // si parte dalla tabella "Mia" che l'oscillatore sta suonando, altrimenti dal primo posto libero
        int s = startTable - bank.userBase();
        if (s < 0 || s >= prisma::numUserTables)
        {
            s = 0;
            for (int k = 0; k < prisma::numUserTables; ++k)
                if (bank.user (k).empty()) { s = k; break; }
        }
        loadSlot (s);

        auto add = [this] (Btn& b, std::function<void()> fn) { b.onClick = std::move (fn); addAndMakeVisible (b); };
        add (cancelButton, [this] { cancel(); });
        add (saveButton, [this] { save(); });
        add (tableButton, [this] { showTableMenu(); });
        add (audioButton, [this] { chooseAudio(); });
        add (waveMode, [this] { harmonicsMode = false; refresh(); });
        add (harmMode, [this] { harmonicsMode = true; refresh(); });
        for (int i = 0; i < 4; ++i) add (tools[i], [this, i] { toolAction (i); });
        const char* starts[] { "Seno", "Sega", "Quadra", "Triang.", "Rumore" };
        for (int i = 0; i < 5; ++i) { startButtons[i].setButtonText (starts[i]); add (startButtons[i], [this, i] { startFrom (i); }); }
        add (dupButton, [this] { duplicateKey(); });
        add (delButton, [this] { deleteKey(); });
        add (clearButton, [this] { keys.assign (1, shape (0)); current = 0; commit(); });
        saveButton.on = true;

        big.owner = small.owner = strip.owner = this;
        big.editable = true;
        addAndMakeVisible (big);
        addAndMakeVisible (small);
        addAndMakeVisible (strip);
        setOpaque (true);
        refresh();
    }

    std::function<void()> onClose;

    void setHarmonicsMode (bool h)   { harmonicsMode = h; refresh(); }
    /** Per i test: carica un suono come se fosse stato scelto da file. */
    void loadAudioForTest (const juce::File& f)   { loadAudio (juce::URL (f)); }
    void fillFromTableForTest (int t)              { fillFromTable (t); }

    //==========================================================================
    void paint (juce::Graphics& g) override
    {
        g.fillAll (frui::colours::bg);
        auto r = getLocalBounds().toFloat().reduced (3.0f);
        g.setColour (frui::colours::panelBot);
        g.fillRoundedRectangle (r, 8.0f);
        g.setColour (frui::colours::edge);
        g.drawRoundedRectangle (r, 8.0f, 1.0f);
        frui::drawNeon (g, "PRISMA - LE MIE ONDE", titleArea.toFloat(), 1.5f, juce::Justification::centredLeft, frui::accent());

        g.setFont (frui::sans (9.5f));
        g.setColour (frui::colours::textDim);
        g.drawText (harmonicsMode ? "LA FORMA DELL'ONDA (SI AGGIORNA DA SOLA)" : "LE ARMONICHE DI QUESTA FORMA (SI AGGIORNANO DA SOLE)",
                    smallCaption, juce::Justification::centredLeft, true);
        g.drawText ("FORME CHIAVE (" + juce::String ((int) keys.size()) + ") - PRISMA SFUMA DA UNA ALL'ALTRA", stripCaption, juce::Justification::centredLeft, true);
        if (! startCaption.isEmpty())
            g.drawText ("PARTI DA", startCaption, juce::Justification::centredLeft, true);
        if (status.isNotEmpty())
        {
            g.setColour (frui::accent().withAlpha (0.9f));
            g.drawFittedText (status, statusArea, juce::Justification::centred, 2, 0.8f);
        }
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (10, 8);
        const bool wide = r.getWidth() > r.getHeight() * 1.2f;
        auto head = r.removeFromTop (34);
        saveButton.setBounds (head.removeFromRight (84).reduced (2));
        cancelButton.setBounds (head.removeFromRight (90).reduced (2));
        titleArea = head;
        r.removeFromTop (4);

        if (wide)
        {
            auto left = r.removeFromLeft (r.getWidth() * 58 / 100);
            r.removeFromLeft (10);
            auto modes = left.removeFromTop (32);
            waveMode.setBounds (modes.removeFromLeft (modes.getWidth() / 2).reduced (2));
            harmMode.setBounds (modes.reduced (2));
            left.removeFromTop (4);
            auto toolRow = left.removeFromBottom (34);
            const int tw = toolRow.getWidth() / 4;
            for (auto& t : tools) t.setBounds (toolRow.removeFromLeft (tw).reduced (2));
            left.removeFromBottom (4);
            big.setBounds (left);

            auto tb = r.removeFromTop (34);
            tableButton.setBounds (tb.reduced (2));
            audioButton.setBounds (r.removeFromTop (34).reduced (2));
            smallCaption = r.removeFromTop (16);
            small.setBounds (r.removeFromTop (juce::jmax (50, r.getHeight() / 4)));
            r.removeFromTop (2);
            startCaption = r.removeFromTop (16);
            auto st = r.removeFromTop (32);
            const int sw = st.getWidth() / 5;
            for (auto& b : startButtons) b.setBounds (st.removeFromLeft (sw).reduced (2));
            stripCaption = r.removeFromTop (18);
            strip.setBounds (r.removeFromTop (juce::jlimit (34, 52, r.getHeight() / 3)));
            r.removeFromTop (4);
            auto foot = r.removeFromTop (34);
            const int fw = foot.getWidth() / 3;
            dupButton.setBounds (foot.removeFromLeft (fw).reduced (2));
            delButton.setBounds (foot.removeFromLeft (fw).reduced (2));
            clearButton.setBounds (foot.reduced (2));
            statusArea = r;
        }
        else
        {
            auto tb = r.removeFromTop (34);
            audioButton.setBounds (tb.removeFromRight (juce::jmin (140, tb.getWidth() / 3)).reduced (2));
            tableButton.setBounds (tb.reduced (2));
            r.removeFromTop (2);
            auto modes = r.removeFromTop (34);
            waveMode.setBounds (modes.removeFromLeft (modes.getWidth() / 2).reduced (2));
            harmMode.setBounds (modes.reduced (2));
            r.removeFromTop (4);

            auto foot = r.removeFromBottom (36);
            const int fw = foot.getWidth() / 3;
            dupButton.setBounds (foot.removeFromLeft (fw).reduced (2));
            delButton.setBounds (foot.removeFromLeft (fw).reduced (2));
            clearButton.setBounds (foot.reduced (2));
            r.removeFromBottom (4);
            statusArea = r.removeFromBottom (status.isNotEmpty() ? 30 : 0);

            const int stripH = juce::jlimit (34, 50, r.getHeight() / 10);
            strip.setBounds (r.removeFromBottom (stripH));
            stripCaption = r.removeFromBottom (18);
            auto st = r.removeFromBottom (34);
            const int sw = st.getWidth() / 5;
            for (auto& b : startButtons) b.setBounds (st.removeFromLeft (sw).reduced (2));
            startCaption = r.removeFromBottom (16);
            auto toolRow = r.removeFromBottom (36);
            const int tw = toolRow.getWidth() / 4;
            for (auto& t : tools) t.setBounds (toolRow.removeFromLeft (tw).reduced (2));
            r.removeFromBottom (4);
            const int smallH = juce::jlimit (50, 96, r.getHeight() / 4);
            small.setBounds (r.removeFromBottom (smallH));
            smallCaption = r.removeFromBottom (16);
            r.removeFromBottom (2);
            big.setBounds (r);
        }
    }

private:
    //==========================================================================
    /** Pulsante piatto (il look & feel neon non disegna i TextButton). */
    struct Btn : public juce::Button
    {
        explicit Btn (const juce::String& t = {}) : juce::Button (t) {}
        bool on = false;
        void paintButton (juce::Graphics& g, bool over, bool down) override
        {
            auto r = getLocalBounds().toFloat().reduced (0.5f);
            const auto a = frui::accent();
            g.setColour (on ? a.withBrightness (0.16f).withSaturation (0.7f) : (down ? juce::Colour (0xff242c35) : over ? juce::Colour (0xff1f262e) : juce::Colour (0xff1a2027)));
            g.fillRoundedRectangle (r, 6.0f);
            g.setColour (on ? a : juce::Colour (0xff2c3640));
            g.drawRoundedRectangle (r, 6.0f, on ? 1.5f : 1.0f);
            g.setColour (! isEnabled() ? frui::colours::textDim.withAlpha (0.5f) : on ? a : frui::colours::text);
            g.setFont (frui::sans (juce::jlimit (10.0f, 13.5f, r.getHeight() * 0.42f)));
            g.drawFittedText (getButtonText(), getLocalBounds().reduced (4, 0), juce::Justification::centred, 1, 0.7f);
        }
    };

    //==========================================================================
    /** Il disegno di una forma: onda o barre delle armoniche. Quello grande si modifica col dito. */
    struct Plot : public juce::Component
    {
        PrismaWaveEditor* owner = nullptr;
        bool editable = false;
        bool bars() const   { return editable ? owner->harmonicsMode : ! owner->harmonicsMode; }

        void paint (juce::Graphics& g) override
        {
            auto r = getLocalBounds().toFloat();
            g.setColour (juce::Colour (0xff05080b));
            g.fillRoundedRectangle (r, 7.0f);
            g.setColour (juce::Colours::black);
            g.drawRoundedRectangle (r.reduced (0.5f), 7.0f, 1.0f);
            const auto a = frui::accent();
            const auto& x = owner->keys[(size_t) owner->current];
            if (bars())
            {
                const auto h = harmonics (x);
                const float bw = (r.getWidth() - 8.0f) / (float) numBars;
                const float top = 4.0f, bottom = r.getHeight() - (editable ? 16.0f : 4.0f);
                for (int i = 0; i < numBars; ++i)
                {
                    const float bx = 4.0f + (float) i * bw;
                    g.setColour (juce::Colour (0xff0c171d));
                    g.fillRoundedRectangle (bx + 1.0f, top, bw - 2.0f, bottom - top, 2.0f);
                    const float v = std::sqrt (juce::jlimit (0.0f, 1.0f, h[(size_t) i]));
                    const float bh = v * (bottom - top);
                    g.setColour (i == lastBar ? juce::Colours::white : a.withAlpha (0.85f));
                    g.fillRoundedRectangle (bx + 1.0f, bottom - bh, bw - 2.0f, bh, 2.0f);
                    if (editable && (i == 0 || (i + 1) % 4 == 0))
                    {
                        g.setColour (frui::colours::textDim);
                        g.setFont (frui::sans (9.0f, false));
                        g.drawText (juce::String (i + 1), juce::Rectangle<float> (bx - 6.0f, bottom + 1.0f, bw + 12.0f, 14.0f), juce::Justification::centred, false);
                    }
                }
                return;
            }
            // griglia
            g.setColour (juce::Colour (0xff0f1d24));
            for (int i = 1; i < 8; ++i) g.fillRect (r.getWidth() * (float) i / 8.0f, 0.0f, 1.0f, r.getHeight());
            for (int i = 1; i < 4; ++i) g.fillRect (0.0f, r.getHeight() * (float) i / 4.0f, r.getWidth(), i == 2 ? 1.5f : 1.0f);
            juce::Path p;
            const int pts = juce::jmax (64, (int) r.getWidth());
            for (int k = 0; k <= pts; ++k)
            {
                const int i = juce::jmin (N - 1, k * N / pts);
                const float px = r.getWidth() * (float) k / (float) pts, py = yFor (x[(size_t) i]);
                if (k == 0) p.startNewSubPath (px, py); else p.lineTo (px, py);
            }
            g.setColour (a.withAlpha (0.25f));
            g.strokePath (p, juce::PathStrokeType (editable ? 6.0f : 4.0f, juce::PathStrokeType::curved));
            g.setColour (a);
            g.strokePath (p, juce::PathStrokeType (editable ? 2.0f : 1.5f, juce::PathStrokeType::curved));
            if (editable && touch.x >= 0.0f)
            {
                g.setColour (juce::Colours::white.withAlpha (0.8f));
                g.drawEllipse (juce::Rectangle<float> (26.0f, 26.0f).withCentre (touch), 2.0f);
                g.fillEllipse (juce::Rectangle<float> (8.0f, 8.0f).withCentre (touch));
            }
        }

        float yFor (float v) const   { return (float) getHeight() * 0.5f - v * (float) getHeight() * 0.45f; }
        float valueFor (float y) const { return juce::jlimit (-1.0f, 1.0f, ((float) getHeight() * 0.5f - y) / ((float) getHeight() * 0.45f)); }
        int indexFor (float px) const  { return juce::jlimit (0, N - 1, (int) (px / (float) juce::jmax (1, getWidth()) * (float) N)); }
        int barFor (float px) const    { return juce::jlimit (0, numBars - 1, (int) ((px - 4.0f) / ((float) getWidth() - 8.0f) * (float) numBars)); }
        float barValue (float py) const
        {
            const float top = 4.0f, bottom = (float) getHeight() - 16.0f;
            const float v = juce::jlimit (0.0f, 1.0f, (bottom - py) / (bottom - top));
            return v * v;
        }

        void mouseDown (const juce::MouseEvent& e) override
        {
            if (! editable) return;
            auto& x = owner->keys[(size_t) owner->current];
            snapshot = x;
            downPos = last = e.position;
            if (bars()) { lastBar = barFor (e.position.x); setHarmonic (x, lastBar, barValue (e.position.y)); }
            else
            {
                touch = e.position;
                if (owner->tool == 0) x[(size_t) indexFor (e.position.x)] = valueFor (e.position.y);
            }
            owner->small.repaint();
            repaint();
        }

        void mouseDrag (const juce::MouseEvent& e) override
        {
            if (! editable) return;
            auto& x = owner->keys[(size_t) owner->current];
            if (bars())
            {
                const int b = barFor (e.position.x), from = barFor (last.x);
                for (int k = juce::jmin (b, from); k <= juce::jmax (b, from); ++k)
                    if (k != lastBar || k == b) setHarmonic (x, k, barValue (e.position.y));
                lastBar = b;
            }
            else
            {
                touch = e.position;
                auto line = [&] (juce::Point<float> p0, juce::Point<float> p1)
                {
                    int i0 = indexFor (p0.x), i1 = indexFor (p1.x);
                    float v0 = valueFor (p0.y), v1 = valueFor (p1.y);
                    if (i0 > i1) { std::swap (i0, i1); std::swap (v0, v1); }
                    for (int i = i0; i <= i1; ++i)
                        x[(size_t) i] = i1 == i0 ? v1 : v0 + (v1 - v0) * (float) (i - i0) / (float) (i1 - i0);
                };
                if (owner->tool == 1) { x = snapshot; line (downPos, e.position); }
                else line (last, e.position);
            }
            last = e.position;
            owner->small.repaint();
            repaint();
        }

        void mouseUp (const juce::MouseEvent&) override
        {
            if (! editable) return;
            touch = { -1.0f, -1.0f };
            lastBar = -1;
            owner->commit();
        }

        juce::Point<float> downPos, last, touch { -1.0f, -1.0f };
        std::vector<float> snapshot;
        int lastBar = -1;
    };

    //==========================================================================
    /** Le forme chiave in fila: tocca per sceglierne una, "+" ne aggiunge una (copia di quella scelta). */
    struct Strip : public juce::Component
    {
        PrismaWaveEditor* owner = nullptr;
        int cells() const               { return juce::jmax (8, (int) owner->keys.size() + ((int) owner->keys.size() < prisma::numFrames ? 1 : 0)); }
        float cellW() const             { return (float) getWidth() / (float) cells(); }

        void paint (juce::Graphics& g) override
        {
            const auto a = frui::accent();
            const int n = (int) owner->keys.size();
            const float w = cellW();
            for (int c = 0; c < cells(); ++c)
            {
                auto r = juce::Rectangle<float> ((float) c * w, 0.0f, w, (float) getHeight()).reduced (w > 20.0f ? 2.0f : 0.5f, 0.0f);
                g.setColour (juce::Colour (0xff05080b));
                g.fillRoundedRectangle (r, 4.0f);
                const bool sel = c == owner->current;
                g.setColour (sel ? a : juce::Colour (0xff1e2a36));
                g.drawRoundedRectangle (r.reduced (0.5f), 4.0f, sel ? 1.5f : 1.0f);
                if (c < n)
                {
                    juce::Path p;
                    const auto& x = owner->keys[(size_t) c];
                    for (int k = 0; k <= 32; ++k)
                    {
                        const float px = r.getX() + 2.0f + (r.getWidth() - 4.0f) * (float) k / 32.0f;
                        const float py = r.getCentreY() - x[(size_t) juce::jmin (N - 1, k * N / 32)] * r.getHeight() * 0.38f;
                        if (k == 0) p.startNewSubPath (px, py); else p.lineTo (px, py);
                    }
                    g.setColour (sel ? a : a.withAlpha (0.55f));
                    g.strokePath (p, juce::PathStrokeType (1.3f));
                    if (w > 22.0f)
                    {
                        g.setColour (frui::colours::textDim);
                        g.setFont (frui::sans (8.5f, false));
                        g.drawText (juce::String (c + 1), r.reduced (3.0f, 1.0f), juce::Justification::topLeft, false);
                    }
                }
                else if (c == n && n < prisma::numFrames)
                {
                    g.setColour (frui::colours::textDim);
                    g.setFont (frui::sans (juce::jmin (20.0f, r.getHeight() * 0.5f), false));
                    g.drawText ("+", r, juce::Justification::centred, false);
                }
            }
        }

        void mouseDown (const juce::MouseEvent& e) override
        {
            const int c = juce::jlimit (0, cells() - 1, (int) (e.position.x / cellW()));
            const int n = (int) owner->keys.size();
            if (c < n) { owner->current = c; owner->refresh(); }
            else if (c == n) owner->duplicateKey();
        }
    };

    //==========================================================================
    // Le armoniche di una forma (ampiezza: 1 = una sinusoide piena)

    static std::array<float, numBars> harmonics (const std::vector<float>& x)
    {
        std::array<float, numBars> h {};
        for (int k = 1; k <= numBars; ++k)
        {
            double re = 0.0, im = 0.0;
            for (int i = 0; i < N; ++i)
            {
                const double ph = juce::MathConstants<double>::twoPi * k * i / N;
                re += x[(size_t) i] * std::cos (ph);
                im += x[(size_t) i] * std::sin (ph);
            }
            h[(size_t) k - 1] = (float) (2.0 * std::sqrt (re * re + im * im) / N);
        }
        return h;
    }

    /** Porta l'armonica (0 = fondamentale) all'ampiezza m, tenendo la sua fase. */
    static void setHarmonic (std::vector<float>& x, int bar, float m)
    {
        const int k = bar + 1;
        double a = 0.0, b = 0.0;
        for (int i = 0; i < N; ++i)
        {
            const double ph = juce::MathConstants<double>::twoPi * k * i / N;
            a += x[(size_t) i] * std::cos (ph);
            b += x[(size_t) i] * std::sin (ph);
        }
        a *= 2.0 / N; b *= 2.0 / N;
        const double mag = std::sqrt (a * a + b * b);
        const double na = mag > 1.0e-6 ? a / mag * m : 0.0, nb = mag > 1.0e-6 ? b / mag * m : m;
        for (int i = 0; i < N; ++i)
        {
            const double ph = juce::MathConstants<double>::twoPi * k * i / N;
            x[(size_t) i] = juce::jlimit (-1.5f, 1.5f, x[(size_t) i] + (float) ((na - a) * std::cos (ph) + (nb - b) * std::sin (ph)));
        }
    }

    /** Lavora sullo spettro intero di una forma: fn (armonica, &re, &im). */
    template <typename Fn>
    static void spectral (std::vector<float>& x, Fn&& fn)
    {
        juce::dsp::FFT fft (10);
        std::vector<float> d ((size_t) N * 2, 0.0f);
        std::copy (x.begin(), x.end(), d.begin());
        fft.performRealOnlyForwardTransform (d.data(), true);
        for (int k = 0; k <= N / 2; ++k) fn (k, d[(size_t) k * 2], d[(size_t) k * 2 + 1]);
        fft.performRealOnlyInverseTransform (d.data());
        for (int i = 0; i < N; ++i) x[(size_t) i] = juce::jlimit (-1.5f, 1.5f, d[(size_t) i]);
    }

    static std::vector<float> shape (int which)
    {
        std::vector<float> x ((size_t) N);
        juce::Random r ((juce::int64) juce::Time::getMillisecondCounter());
        float prev = 0.0f;
        for (int i = 0; i < N; ++i)
        {
            const float t = (float) i / (float) N;
            float v = 0.0f;
            switch (which)
            {
                case 0: v = std::sin (juce::MathConstants<float>::twoPi * t); break;
                case 1: v = t < 0.5f ? 2.0f * t : 2.0f * t - 2.0f; break;
                case 2: v = t < 0.5f ? 0.9f : -0.9f; break;
                case 3: v = t < 0.25f ? 4.0f * t : t < 0.75f ? 2.0f - 4.0f * t : 4.0f * t - 4.0f; break;
                default: prev += 0.35f * ((r.nextFloat() * 2.0f - 1.0f) - prev); v = prev * 2.0f; break;
            }
            x[(size_t) i] = juce::jlimit (-1.0f, 1.0f, v);
        }
        return x;
    }

    //==========================================================================
    void refresh()
    {
        current = juce::jlimit (0, (int) keys.size() - 1, current);
        waveMode.on = ! harmonicsMode;
        harmMode.on = harmonicsMode;
        const char* waveTools[] { "Matita", "Linea", "Liscia", "Specchia" };
        const char* harmTools[] { "Solo dispari", "Solo pari", "Piu' scuro", "A caso" };
        for (int i = 0; i < 4; ++i)
        {
            tools[i].setButtonText (harmonicsMode ? harmTools[i] : waveTools[i]);
            tools[i].on = ! harmonicsMode && i == tool;
        }
        const auto& u = prisma::Bank::get().user (slot);
        juce::String label = "Tabella: Mia " + juce::String (slot + 1);
        if (name.isNotEmpty() && name != "Mia " + juce::String (slot + 1)) label << " - " << name;
        tableButton.setButtonText (label + (u.empty() && ! dirty ? juce::String (" (nuova)") : juce::String()) + juce::String (juce::CharPointer_UTF8 (" \xe2\x96\xbe")));
        delButton.setEnabled (keys.size() > 1);
        dupButton.setEnabled ((int) keys.size() < prisma::numFrames);
        for (auto* c : getChildren()) c->repaint();
        repaint();
    }

    void loadSlot (int s)
    {
        slot = juce::jlimit (0, prisma::numUserTables - 1, s);
        const auto& u = prisma::Bank::get().user (slot);
        original = u;
        keys.clear();
        if (u.empty()) { keys.push_back (shape (0)); name = "Mia " + juce::String (slot + 1); }
        else
        {
            name = u.name;
            for (int k = 0; k < u.keys; ++k)
            {
                std::vector<float> x ((size_t) N);
                for (int i = 0; i < N; ++i) x[(size_t) i] = (float) u.samples[(size_t) k * N + (size_t) i] / 32767.0f;
                keys.push_back (std::move (x));
            }
        }
        current = 0;
        dirty = false;
    }

    prisma::UserTable makeTable() const
    {
        prisma::UserTable t;
        t.name = name;
        t.keys = (int) keys.size();
        t.samples.resize ((size_t) t.keys * N);
        float peak = 1.0e-6f;
        for (auto& x : keys) for (float v : x) peak = juce::jmax (peak, std::abs (v));
        const float g = peak > 1.0f ? 1.0f / peak : 1.0f;
        for (int k = 0; k < t.keys; ++k)
            for (int i = 0; i < N; ++i)
                t.samples[(size_t) k * N + (size_t) i] = (juce::int16) juce::jlimit (-32767.0f, 32767.0f, keys[(size_t) k][(size_t) i] * g * 32767.0f);
        return t;
    }

    /** La tabella suona subito: l'oscillatore passa su "Mia N". */
    void commit()
    {
        dirty = true;
        auto& bank = prisma::Bank::get();
        bank.setUser (slot, makeTable(), false);
        setTableParam (bank.userBase() + slot);
        proc.updateSpectral();
        refresh();
    }

    void setTableParam (int t)
    {
        if (auto* p = proc.apvts.getParameter (tableId))
        {
            const float v = p->convertTo0to1 ((float) t);
            if (std::abs (p->getValue() - v) > 1.0e-6f)
            {
                p->beginChangeGesture();
                p->setValueNotifyingHost (v);
                p->endChangeGesture();
            }
        }
    }

    void save()
    {
        auto& bank = prisma::Bank::get();
        bank.setUser (slot, makeTable(), true);
        setTableParam (bank.userBase() + slot);
        proc.updateSpectral();
        close();
    }

    void cancel()
    {
        revert();
        setTableParam (startTable);
        proc.updateSpectral();
        close();
    }

    void revert()
    {
        if (dirty) prisma::Bank::get().setUser (slot, original, false);
        dirty = false;
    }

    void close()
    {
        if (onClose) onClose();
    }

    void duplicateKey()
    {
        if ((int) keys.size() >= prisma::numFrames) return;
        keys.insert (keys.begin() + current + 1, keys[(size_t) current]);
        ++current;
        commit();
    }

    void deleteKey()
    {
        if (keys.size() <= 1) return;
        keys.erase (keys.begin() + current);
        current = juce::jmin (current, (int) keys.size() - 1);
        commit();
    }

    void startFrom (int which)
    {
        keys[(size_t) current] = shape (which);
        commit();
    }

    void toolAction (int i)
    {
        auto& x = keys[(size_t) current];
        if (! harmonicsMode)
        {
            if (i < 2) { tool = i; refresh(); return; }
            if (i == 2)   // liscia
                for (int pass = 0; pass < 3; ++pass)
                {
                    const auto y = x;
                    for (int k = 0; k < N; ++k)
                    {
                        float s = 0.0f;
                        for (int d = -4; d <= 4; ++d) s += y[(size_t) ((k + d + N) % N)];
                        x[(size_t) k] = s / 9.0f;
                    }
                }
            else          // specchia: la seconda meta' e' la prima rovesciata
                for (int k = 0; k < N / 2; ++k) x[(size_t) (N - 1 - k)] = -x[(size_t) k];
        }
        else if (i == 0) spectral (x, [] (int k, float& re, float& im) { if (k % 2 == 0) re = im = 0.0f; });
        else if (i == 1) spectral (x, [] (int k, float& re, float& im) { if (k % 2 == 1 && k > 1) re = im = 0.0f; });
        else if (i == 2) spectral (x, [] (int k, float& re, float& im) { const float g = std::pow (0.8f, (float) juce::jmax (0, k - 1)); re *= g; im *= g; });
        else
        {
            juce::Random r ((juce::int64) juce::Time::getHighResolutionTicks());
            std::fill (x.begin(), x.end(), 0.0f);
            for (int k = 1; k <= numBars; ++k)
            {
                const float m = (k == 1 ? 0.8f : r.nextFloat() * r.nextFloat()) * std::pow ((float) k, -0.6f);
                for (int s = 0; s < N; ++s) x[(size_t) s] += m * std::sin (juce::MathConstants<float>::twoPi * (float) (k * s) / (float) N);
            }
        }
        commit();
    }

    void showTableMenu()
    {
        auto& bank = prisma::Bank::get();
        juce::PopupMenu menu;
        menu.addSectionHeader ("LE MIE TABELLE");
        for (int s = 0; s < prisma::numUserTables; ++s)
        {
            const auto& u = bank.user (s);
            juce::String t = "Mia " + juce::String (s + 1);
            if (u.empty()) t << " (vuota)";
            else if (u.name.isNotEmpty() && u.name != t) t << " - " << u.name;
            menu.addItem (s + 1, t, true, s == slot);
        }
        menu.addSeparator();
        if (startTable >= 0 && startTable < bank.userBase())
            menu.addItem (500, "Riempi con le forme di " + bank.names()[startTable]);
        juce::Component::SafePointer<PrismaWaveEditor> safe (this);
        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&tableButton), [safe] (int r)
        {
            if (safe == nullptr || r <= 0) return;
            if (r == 500) { safe->fillFromTable (safe->startTable); return; }
            if (r - 1 == safe->slot) return;
            const bool lost = safe->dirty;
            safe->revert();
            safe->loadSlot (r - 1);
            safe->status = lost ? "Le modifiche non salvate sono state lasciate" : juce::String();
            if (! prisma::Bank::get().user (safe->slot).empty())
                safe->setTableParam (prisma::Bank::get().userBase() + safe->slot);
            safe->refresh();
            safe->resized();
        });
    }

    /** 8 forme chiave prese da una tabella esistente (cosi' si parte da qualcosa e la si cambia). */
    void fillFromTable (int t)
    {
        auto& bank = prisma::Bank::get();
        bank.ensure (t);
        keys.clear();
        for (int k = 0; k < 8; ++k)
        {
            const float* f = bank.frame (t, juce::roundToInt ((float) k * (float) (prisma::numFrames - 1) / 7.0f), 0);
            keys.emplace_back (f, f + N);
        }
        name = bank.names()[t];
        current = 0;
        commit();
    }

    void chooseAudio()
    {
        chooser = std::make_unique<juce::FileChooser> ("Scegli un suono da trasformare in tabella", juce::File(), "*.wav;*.aif;*.aiff;*.flac;*.ogg;*.mp3");
        juce::Component::SafePointer<PrismaWaveEditor> safe (this);
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [safe] (const juce::FileChooser& fc)
        {
            if (safe == nullptr) return;
            const auto url = fc.getURLResult();
            if (url.isEmpty()) return;
            safe->loadAudio (url);
        });
    }

    void loadAudio (const juce::URL& url)
    {
        juce::MemoryBlock mb;
        if (auto in = OpenWith::openStream (url))
            in->readIntoMemoryBlock (mb, 60 * 1024 * 1024);
        juce::AudioFormatManager formats;
        formats.registerBasicFormats();
        std::unique_ptr<juce::AudioFormatReader> reader (mb.getSize() > 0 ? formats.createReaderFor (std::make_unique<juce::MemoryInputStream> (mb, false)) : nullptr);
        if (reader == nullptr) { status = "Non riesco a leggere questo file audio."; resized(); repaint(); return; }
        const int n = (int) juce::jmin (reader->lengthInSamples, (juce::int64) (reader->sampleRate * 12.0));
        juce::AudioBuffer<float> buf ((int) juce::jmax (1u, reader->numChannels), juce::jmax (1, n));
        reader->read (&buf, 0, n, 0, true, true);
        std::vector<float> mono ((size_t) juce::jmax (1, n), 0.0f);
        for (int c = 0; c < buf.getNumChannels(); ++c)
            juce::FloatVectorOperations::addWithMultiply (mono.data(), buf.getReadPointer (c), 1.0f / (float) buf.getNumChannels(), n);
        auto fileName = OpenWith::fileName (url).upToLastOccurrenceOf (".", false, false);
        if (fileName.isEmpty()) fileName = "Dal file";
        const auto t = prisma::tableFromAudio (mono.data(), n, reader->sampleRate, fileName.substring (0, 24));
        if (t.empty()) { status = "In questo file non trovo un suono utile (silenzio o troppo corto)."; resized(); repaint(); return; }
        keys.clear();
        for (int k = 0; k < t.keys; ++k)
        {
            std::vector<float> x ((size_t) N);
            for (int i = 0; i < N; ++i) x[(size_t) i] = (float) t.samples[(size_t) k * N + (size_t) i] / 32767.0f;
            keys.push_back (std::move (x));
        }
        name = t.name;
        current = 0;
        status = juce::String (t.keys) + " forme prese da \"" + name + "\". Muovi POSIZIONE per scorrerle.";
        commit();
        resized();
    }

    //==========================================================================
    PrismaAudioProcessor& proc;
    const int osc;
    const juce::String tableId;
    int startTable = 0, slot = 0, current = 0, tool = 0;
    bool harmonicsMode = false, dirty = false;
    juce::String name, status;
    prisma::UserTable original;
    std::vector<std::vector<float>> keys;

    Btn cancelButton { "Annulla" }, saveButton { "Salva" }, tableButton, audioButton { juce::String (juce::CharPointer_UTF8 ("Da file audio\xe2\x80\xa6")) },
        waveMode { "ONDA (disegno)" }, harmMode { "ARMONICHE (barre)" }, dupButton { "Duplica forma" }, delButton { "Elimina forma" }, clearButton { "Svuota" };
    Btn tools[4], startButtons[5];
    Plot big, small;
    Strip strip;
    juce::Rectangle<int> titleArea, statusArea;
    juce::Rectangle<int> smallCaption, stripCaption, startCaption;
    std::unique_ptr<juce::FileChooser> chooser;

    friend struct Plot;
    friend struct Strip;
};
