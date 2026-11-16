#pragma once

#include <JuceHeader.h>
#include <functional>

//==============================================================================
/**
    Schermata di presentazione all'avvio di AudioNaut: "l'onda e poi il casco".
    1. Nel buio si accendono le stelle e un'onda luminosa attraversa lo schermo da sinistra a destra.
    2. L'onda si stringe al centro e il casco dell'astronauta le compare intorno: l'onda diventa
       quella della visiera.
    3. Un bagliore dietro al casco, sotto sale la scritta AUDIONAUT, poi tutto sfuma e si vede l'app.
    Silenziosa. Un tocco salta alla fine.
*/
class JLoopSplash : public juce::Component,
                     private juce::Timer
{
public:
    JLoopSplash()
    {
        helmet = juce::ImageCache::getFromMemory (BinaryData::splash_front_png, BinaryData::splash_front_pngSize);
        word   = juce::ImageCache::getFromMemory (BinaryData::splash_word_png,  BinaryData::splash_word_pngSize);
        juce::Random r (4242);
        for (auto& s : stars)
            s = { r.nextFloat(), r.nextFloat(), 0.5f + r.nextFloat() * 1.3f, r.nextFloat() * 6.28f, 0.25f + r.nextFloat() * 0.6f };
        setOpaque (false);
        setInterceptsMouseClicks (true, false);
        start = juce::Time::getMillisecondCounterHiRes();
        startTimerHz (60);
    }

    std::function<void()> onFinished;

    /** Per le foto di prova: ferma l'animazione al secondo t. */
    void freezeAt (double t)    { frozen = t; stopTimer(); repaint(); }

    void mouseDown (const juce::MouseEvent&) override
    {
        // Salta: passa subito alla dissolvenza finale
        const double now = elapsed();
        if (now < fadeOutStart)
            start -= (fadeOutStart - now) * 1000.0;
    }

    void paint (juce::Graphics& g) override
    {
        const double t = elapsed();
        const float screen = t < fadeOutStart ? 1.0f : (float) juce::jlimit (0.0, 1.0, 1.0 - (t - fadeOutStart) / fadeOutTime);
        const auto b = getLocalBounds().toFloat();

        // fondo: blu notte, piu' chiaro dietro al casco
        const float wordH = word.isValid() ? juce::jmin (b.getWidth() * 0.62f, 420.0f) * (float) word.getHeight() / (float) word.getWidth() : 0.0f;
        const float hs = juce::jmin (b.getWidth() * 0.62f, b.getHeight() * 0.42f, 440.0f);
        const auto centre = b.getCentre().translated (0.0f, -wordH * 1.2f);
        {
            juce::ColourGradient bg (juce::Colour (0xff141a3c).withMultipliedAlpha (screen), centre,
                                     juce::Colour (0xff04050b).withMultipliedAlpha (screen), centre.translated (b.getWidth() * 0.75f, b.getHeight() * 0.5f), true);
            g.setGradientFill (bg);
            g.fillAll();
        }

        // stelle che brillano piano
        const float starsIn = ease ((t - 0.0) / 0.6) * screen;
        for (auto& s : stars)
        {
            const float tw = 0.55f + 0.45f * std::sin ((float) t * 2.3f + s.phase);
            g.setColour (juce::Colour (0xffcfe6ff).withAlpha (starsIn * s.bright * tw));
            g.fillEllipse (juce::Rectangle<float> (s.size, s.size).withCentre ({ s.x * b.getWidth(), s.y * b.getHeight() }));
        }

        // 2. il casco compare intorno all'onda (e si avvicina un poco)
        const float helmetIn = ease ((t - 0.75) / 0.55);
        const float scale = 1.15f - 0.15f * helmetIn + (t > fadeOutStart ? 0.04f * (1.0f - screen) : 0.0f);
        const auto area = juce::Rectangle<float> (hs * scale, hs * scale).withCentre (centre);
        const auto visor = area.getCentre().translated (0.0f, -0.035f * area.getHeight());

        // bagliore dietro al casco
        if (t > 1.2)
        {
            const float pulse = ease ((t - 1.2) / 0.5) * (0.85f + 0.15f * std::sin ((float) t * 3.2f)) * screen;
            juce::ColourGradient halo (juce::Colour (0xff5ad6ff).withAlpha (0.28f * pulse), visor,
                                       juce::Colour (0xff8a5cff).withAlpha (0.0f), visor.translated (hs * 0.78f, 0.0f), true);
            halo.addColour (0.55, juce::Colour (0xff8a5cff).withAlpha (0.12f * pulse));
            g.setGradientFill (halo);
            g.fillEllipse (area.withSizeKeepingCentre (hs * 1.6f, hs * 1.6f));
        }

        if (helmet.isValid() && helmetIn > 0.0f)
        {
            g.setOpacity (helmetIn * screen);
            g.drawImage (helmet, area, juce::RectanglePlacement::centred);
        }

        // 1. l'onda: attraversa lo schermo, poi si stringe nella visiera e sfuma in quella del casco
        const float draw = ease ((t - 0.05) / 0.7);
        const float shrink = ease ((t - 0.6) / 0.55);
        const float waveAlpha = (1.0f - (float) juce::jlimit (0.0, 1.0, (t - 0.95) / 0.35)) * screen;
        if (draw > 0.0f && waveAlpha > 0.0f)
        {
            const float span = juce::jmap (shrink, b.getWidth() * 0.9f, area.getWidth() * 0.68f);
            const float amp = juce::jmap (shrink, juce::jmin (b.getWidth(), b.getHeight()) * 0.11f, area.getHeight() * 0.15f);
            const float x0 = visor.x - span * 0.5f;
            juce::Path p;
            const int pts = 160;
            for (int i = 0; i <= pts; ++i)
            {
                const float u = (float) i / (float) pts;
                if (u > draw) break;
                const float env = std::pow (juce::jmax (0.0f, std::sin (juce::MathConstants<float>::pi * u)), 1.3f);   // (sin di pi esce appena negativo: niente NaN)
                const float y = visor.y - amp * env * std::sin (juce::MathConstants<float>::twoPi * (2.5f * u - 0.25f) + (float) t * 2.0f);
                if (i == 0) p.startNewSubPath (x0 + span * u, y); else p.lineTo (x0 + span * u, y);
            }
            // le code dell'onda sfumano nel buio (niente righe dritte ai lati)
            juce::ColourGradient grad (juce::Colour (0x004fe3ff), x0, 0.0f, juce::Colour (0x00c06bff), x0 + span, 0.0f, false);
            grad.addColour (0.14, juce::Colour (0xff4fe3ff));
            grad.addColour (0.5, juce::Colour (0xff7d8bff));
            grad.addColour (0.86, juce::Colour (0xffc06bff));
            const float thick = juce::jlimit (2.0f, 5.0f, hs / 110.0f);
            for (auto [w, a] : { std::pair<float, float> { thick * 5.0f, 0.08f }, { thick * 2.6f, 0.18f }, { thick, 1.0f } })
            {
                auto gg = grad;
                gg.multiplyOpacity (a * waveAlpha);
                g.setGradientFill (gg);
                g.strokePath (p, juce::PathStrokeType (w, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            }
            // la "testa" luminosa mentre l'onda avanza
            if (draw < 1.0f && p.getLength() > 0.0f)
            {
                const auto head = p.getCurrentPosition();
                g.setColour (juce::Colours::white.withAlpha (0.9f * waveAlpha));
                g.fillEllipse (juce::Rectangle<float> (thick * 2.4f, thick * 2.4f).withCentre (head));
            }
        }

        // 3. la scritta sale sotto il casco
        if (word.isValid())
        {
            const float w = ease ((t - 1.45) / 0.45);
            if (w > 0.0f)
            {
                const float ww = juce::jmin (b.getWidth() * 0.62f, 420.0f);
                const float top = centre.y + hs * 0.56f + (1.0f - w) * wordH * 0.8f;
                g.setOpacity (w * screen);
                g.drawImage (word, juce::Rectangle<float> (ww, wordH).withCentre ({ centre.x, top + wordH * 0.5f }),
                             juce::RectanglePlacement::stretchToFit);
            }
        }
    }

private:
    static float ease (double x)   { const float v = (float) juce::jlimit (0.0, 1.0, x); return 1.0f - std::pow (1.0f - v, 3.0f); }
    double elapsed() const   { return frozen >= 0.0 ? frozen : (juce::Time::getMillisecondCounterHiRes() - start) / 1000.0; }

    void timerCallback() override
    {
        repaint();
        if (elapsed() >= fadeOutStart + fadeOutTime)
        {
            stopTimer();
            setVisible (false);
            if (onFinished)
                juce::MessageManager::callAsync (onFinished);   // chi la possiede la elimina fuori da questo callback
        }
    }

    struct Star { float x, y, size, phase, bright; };

    static constexpr double fadeOutStart = 2.5, fadeOutTime = 0.6;
    double frozen = -1.0;
    juce::Image helmet, word;
    std::array<Star, 80> stars {};
    double start = 0.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (JLoopSplash)
};
