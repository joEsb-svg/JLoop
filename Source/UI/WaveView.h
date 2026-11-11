#pragma once

#include <JuceHeader.h>
#include "Theme.h"
#include "../Engine/SampleStore.h"

//==============================================================================
/** La forma d'onda di un suono (come nella finestra del campione di FL Studio). */
class WaveView : public juce::Component
{
public:
    void setSample (const SampleData* s, juce::Colour c = Theme::accent)
    {
        colour = c;
        peaks.clear();
        seconds = 0.0;
        if (s != nullptr && s->audio.getNumSamples() > 0)
        {
            const int n = s->audio.getNumSamples();
            seconds = n / s->sampleRate;
            const float* d = s->audio.getReadPointer (0);
            constexpr int columns = 400;
            for (int c2 = 0; c2 < columns; ++c2)
            {
                const int a = (int) ((juce::int64) n * c2 / columns), b = juce::jmax (a + 1, (int) ((juce::int64) n * (c2 + 1) / columns));
                float lo = 0.0f, hi = 0.0f;
                for (int i = a; i < juce::jmin (b, n); ++i) { lo = juce::jmin (lo, d[i]); hi = juce::jmax (hi, d[i]); }
                peaks.push_back ({ lo, hi });
            }
        }
        repaint();
    }

    void setEmptyText (const juce::String& t)   { emptyText = t; repaint(); }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        Theme::drawSunken (g, r, Theme::lcdBg, 6.0f);
        r = r.reduced (6.0f, 4.0f);
        if (peaks.empty())
        {
            g.setColour (Theme::textDim);
            g.setFont (Theme::font (12.0f));
            g.drawFittedText (emptyText, r.toNearestInt(), juce::Justification::centred, 2);
            return;
        }
        g.setColour (Theme::edgeLight.withAlpha (0.35f));
        g.fillRect (r.getX(), r.getCentreY(), r.getWidth(), 1.0f);
        juce::Path p;
        const int n = (int) peaks.size();
        const float half = r.getHeight() * 0.48f;
        for (int i = 0; i < n; ++i)
        {
            const float x = r.getX() + r.getWidth() * (float) i / (float) (n - 1);
            const float y = r.getCentreY() - peaks[(size_t) i].second * half;
            if (i == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
        }
        for (int i = n - 1; i >= 0; --i)
            p.lineTo (r.getX() + r.getWidth() * (float) i / (float) (n - 1), r.getCentreY() - peaks[(size_t) i].first * half);
        p.closeSubPath();
        g.setGradientFill (juce::ColourGradient (colour.brighter (0.3f), 0.0f, r.getY(), colour.darker (0.4f), 0.0f, r.getBottom(), false));
        g.fillPath (p);
        g.setColour (Theme::textDim);
        g.setFont (Theme::font (10.5f));
        g.drawText (seconds < 1.0 ? juce::String (juce::roundToInt (seconds * 1000.0)) + " ms" : juce::String (seconds, 2).replace (".", ",") + " s",
                    r.removeFromTop (12.0f), juce::Justification::topRight, false);
    }

private:
    std::vector<std::pair<float, float>> peaks;
    juce::Colour colour = Theme::accent;
    double seconds = 0.0;
    juce::String emptyText;
};
