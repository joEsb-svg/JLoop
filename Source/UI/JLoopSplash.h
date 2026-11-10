#pragma once

#include <JuceHeader.h>
#include <functional>

//==============================================================================
/**
    Schermata di presentazione all'avvio: "il neon si accende".
    1. Al buio compare il logo di metallo (J + infinito), spento.
    2. La luce arancio dentro sfarfalla come un'insegna al neon e poi resta accesa (con un leggero ronzio).
    3. Sotto compare la scritta JLOOP, poi tutto sfuma e si vede l'app.
    Il logo e' fatto di tre immagini sovrapposte (retro, luce, faccia di metallo) piu' la scritta.
    Un tocco salta alla fine.
*/
class JLoopSplash : public juce::Component,
                     private juce::Timer
{
public:
    JLoopSplash()
    {
        back  = juce::ImageCache::getFromMemory (BinaryData::splash_back_png,  BinaryData::splash_back_pngSize);
        glow  = juce::ImageCache::getFromMemory (BinaryData::splash_glow_png,  BinaryData::splash_glow_pngSize);
        front = juce::ImageCache::getFromMemory (BinaryData::splash_front_png, BinaryData::splash_front_pngSize);
        word  = juce::ImageCache::getFromMemory (BinaryData::splash_word_png,  BinaryData::splash_word_pngSize);
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

    /** Quanto e' accesa la luce al momento t (0 spenta, 1 accesa): sfarfallio da insegna, poi ronzio leggero. */
    static float neonLevel (double t)
    {
        struct Flash { double from, to; float level; };
        static constexpr Flash flicker[] {
            { 0.62, 0.67, 0.85f }, { 0.67, 0.76, 0.05f }, { 0.76, 0.79, 1.00f }, { 0.79, 0.90, 0.12f },
            { 0.90, 0.93, 0.70f }, { 0.93, 0.98, 0.25f }, { 0.98, 1.01, 0.95f }, { 1.01, 1.05, 0.45f } };
        if (t < flicker[0].from) return 0.0f;
        for (auto& f : flicker)
            if (t >= f.from && t < f.to) return f.level;
        if (t < 1.25) return 0.45f + 0.55f * (float) ((t - 1.05) / 0.20);   // si accende del tutto
        return 0.93f + 0.05f * std::sin ((float) t * 37.0f) + 0.02f * std::sin ((float) t * 91.0f);   // ronzio
    }

    void paint (juce::Graphics& g) override
    {
        const double t = elapsed();
        const float screenAlpha = t < fadeOutStart ? 1.0f : (float) juce::jlimit (0.0, 1.0, 1.0 - (t - fadeOutStart) / fadeOutTime);
        g.setColour (juce::Colours::black.withAlpha (screenAlpha));
        g.fillAll();
        if (! front.isValid())
            return;

        // 1. il metallo esce dal buio (con un leggero avvicinamento)
        const float in = (float) juce::jlimit (0.0, 1.0, (t - 0.08) / 0.5);
        const float ease = 1.0f - std::pow (1.0f - in, 3.0f);
        const float scale = 0.94f + 0.06f * ease + (t > fadeOutStart ? 0.05f * (1.0f - screenAlpha) : 0.0f);
        const float metalAlpha = ease * screenAlpha;
        const float neon = neonLevel (t) * screenAlpha;

        // dimensioni: il disegno (1000 x 860) ha il logo vero al centro (~65% della larghezza)
        const float logoW = juce::jmin ((float) getWidth() * 0.78f, (float) getHeight() * 0.55f) / 0.65f * scale;
        const float logoH = logoW * (float) front.getHeight() / (float) front.getWidth();
        const float wordW = logoW * 0.5f;
        const float wordH = word.isValid() ? wordW * (float) word.getHeight() / (float) word.getWidth() : 0.0f;
        auto centre = getLocalBounds().toFloat().getCentre().translated (0.0f, -wordH * 1.1f);
        const auto area = juce::Rectangle<float> (logoW, logoH).withCentre (centre);

        // luce calda sul "pavimento" quando il neon e' acceso
        if (neon > 0.01f)
        {
            juce::ColourGradient warm (juce::Colour (0xffff7a1a).withAlpha (0.16f * neon), area.getCentreX(), area.getCentreY(),
                                       juce::Colour (0xffff7a1a).withAlpha (0.0f), area.getCentreX() + logoW * 0.55f, area.getCentreY(), true);
            g.setGradientFill (warm);
            g.fillEllipse (area.withSizeKeepingCentre (logoW * 1.1f, logoW * 1.1f));
        }

        g.setOpacity (metalAlpha);
        g.drawImage (back, area, juce::RectanglePlacement::stretchToFit);
        if (neon > 0.0f)
        {
            g.setOpacity (juce::jmin (1.0f, neon) * ease);
            g.drawImage (glow, area, juce::RectanglePlacement::stretchToFit);
        }
        g.setOpacity (metalAlpha);
        g.drawImage (front, area, juce::RectanglePlacement::stretchToFit);

        // 3. la scritta sale e compare quando la luce e' accesa
        if (word.isValid())
        {
            const float w = (float) juce::jlimit (0.0, 1.0, (t - 1.15) / 0.4);
            const float wEase = 1.0f - std::pow (1.0f - w, 3.0f);
            if (wEase > 0.0f)
            {
                const float top = area.getCentreY() + logoH * 0.36f + (1.0f - wEase) * wordH * 0.6f;
                g.setOpacity (wEase * screenAlpha);
                g.drawImage (word, juce::Rectangle<float> (wordW, wordH).withCentre ({ area.getCentreX(), top + wordH * 0.5f }),
                             juce::RectanglePlacement::stretchToFit);
            }
        }
    }

private:
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

    static constexpr double fadeOutStart = 2.25, fadeOutTime = 0.6;
    double frozen = -1.0;
    juce::Image back, glow, front, word;
    double start = 0.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (JLoopSplash)
};
