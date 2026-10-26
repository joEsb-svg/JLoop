#pragma once

#include <JuceHeader.h>
#include <functional>
#include "Theme.h"

//==============================================================================
/**
    Guida rapida: poche schede che mostrano dove sta tutto. Si apre solo da MENU > Guida rapida
    (non piu' da sola al primo avvio). L'ultima scheda propone le demo.
    Si sfoglia con i pulsanti o trascinando il dito a destra/sinistra.
*/
class QuickGuide : public juce::Component
{
public:
    explicit QuickGuide (juce::StringArray demoNames)
    {
        // pic: il disegnino della scheda (vedi drawPicture)
        cards =
        {
            { "Guida rapida di JLoop", "Uno studio musicale in tasca, pensato come FL Studio. Sfoglia le schede (pulsanti o dito a destra/sinistra) "
                                       "e chiudi quando vuoi: la ritrovi sempre in MENU > Guida rapida. "
                                       "Sbagliato qualcosa? Tieni premuta la freccia dell'undo per la cronologia delle modifiche.", picLogo },
            { "Il channel rack", "Ogni riga e' un suono: tocca le caselle per accendere i colpi e premi Play. "
                                 "Tieni premuta una casella per riempire la riga con un ritmo pronto. STEP / VEL / PROB / ROLL / PITCH / PAN "
                                 "cambiano cosa modifichi (forza, probabilita', rullate, altezza e panorama di ogni colpo).", picSteps },
            { "Lunghezza del pattern", "Il display con le frecce dice quanti quarti dura il pattern (1 quarto = 4 step, 4 quarti = 1 battuta), "
                                       "da 1 a 64. Le frecce aggiungono o tolgono un quarto; tocca il numero per scriverlo. "
                                       "I pattern lunghi scorrono col dito; i numeri grandi in trasparenza contano i quarti.", picLength },
            { "Suoni e strumenti", "Tocca il nome di un canale per cambiare suono o aprire lo strumento; il + aggiunge un canale. "
                                   "BassForge (bassi), PolyForge (pad, lead, accordi), FMForge (piani elettrici, campane), DrumForge "
                                   "(drum machine a 16 pad) e Slicer (taglia un loop o una voce: un pezzo per tasto).", picKeys },
            { "I synth neon", "French Roulette: bassi choppati con step matrix. MegaSaw: supersaw, hoover e reese. "
                              "WubForge: wobble con 4 forme di LFO da disegnare col dito e una sequenza per mezza battuta. "
                              "RoboVox: vocoder, una voce (anche registrata col microfono) fa cantare i tuoi accordi.", picNeon },
            { "Il piano roll", "Per melodie e accordi. Scegli una scala (le note giuste si illuminano), accordi con un tocco, "
                               "progressioni pronte, note fantasma degli altri canali e corsia dei volumi. "
                               "Nel pannello dello strumento, Arp accende l'arpeggiatore.", picRoll },
            { "Pattern e playlist", "99 pattern per progetto. Nella playlist li metti in fila per costruire la canzone; tocca il righello "
                                    "per partire da una battuta (riser, loop e voci lunghe riprendono dal punto giusto). "
                                    "Le automazioni muovono volume, filtri ed effetti; LIVE lancia i pattern al volo; REC registra.", picPlaylist },
            { "Riser automatico", "MENU > Riser automatico: scegli quante battute (1-16) e gli ingredienti: rumore e nota che salgono, "
                                  "rullata che accelera, impatto, filtro sul master. JLoop lo crea a tempo e lo mette nella playlist "
                                  "in modo che finisca proprio sul drop.", picRiser },
            { "Il mixer", "Ogni canale ha la sua traccia con EQ e fino a 6 effetti: compressore, OTT, sidechain, Glitch, "
                          "distorsione trash, filtro a vocali, delay, riverbero e altri. "
                          "Un synth pesante? Menu del canale > Congela traccia: suona uguale e non consuma piu' processore.", picMixer },
            { "Esporta e condividi", "MENU > Esporta audio: il mastering automatico rende il brano pronto (WAV o OGG) e puoi ritoccarlo. "
                                     "MENU > Condividi progetto crea un file .jloop con dentro anche i tuoi campioni: "
                                     "chi lo riceve lo apre con \"Apri progetto da file\".", picShare },
            { "Se l'audio scatta", "Tocca l'indicatore CPU sopra i canali: \"Risparmio\" alleggerisce i synth, \"Riserva audio\" "
                                   "calcola il suono in anticipo (contro gli scatti), \"Ferma tutti i suoni\" e' il panico. "
                                   "Congelare le tracce dei synth aiuta molto.", picCpu },
            { "Prova una demo", "Il modo piu' veloce per capire: apri una canzone di esempio, premi Play e smonta tutto. "
                                "Oppure chiudi e parti da zero.", picNone },
        };

        for (auto* b : { &backButton, &nextButton, &skipButton })
            addAndMakeVisible (b);
        backButton.onClick = [this] { show (page - 1); };
        nextButton.onClick = [this] { if (page == (int) cards.size() - 1) finish (-1); else show (page + 1); };
        skipButton.onClick = [this] { finish (-1); };

        for (int i = 0; i < demoNames.size(); ++i)
        {
            auto* b = demoButtons.add (new juce::TextButton (demoNames[i]));
            b->onClick = [this, i] { finish (i); };
            addChildComponent (b);
        }
        show (0);
    }

    /** Chiusa: -1 = senza demo, altrimenti l'indice della demo scelta. */
    std::function<void (int demo)> onFinished;

    void show (int newPage)
    {
        page = juce::jlimit (0, (int) cards.size() - 1, newPage);
        const bool last = page == (int) cards.size() - 1;
        backButton.setEnabled (page > 0);
        nextButton.setButtonText (last ? "Parti da zero" : "Avanti");
        skipButton.setVisible (! last);
        for (auto* b : demoButtons)
            b->setVisible (last);
        resized();
        repaint();
    }

    int getPage() const     { return page; }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (juce::Colours::black.withAlpha (0.7f));
        Theme::drawRaised (g, card.toFloat(), Theme::panel, 8.0f);

        g.setColour (Theme::accent);
        g.setFont (Theme::font (11.0f, true));
        g.drawText (juce::String (page + 1) + " / " + juce::String ((int) cards.size()), counterArea, juce::Justification::centredLeft);

        g.setColour (Theme::textBright);
        g.setFont (Theme::font (titleArea.getHeight() > 30 ? 24.0f : 19.0f, true));
        g.drawFittedText (cards[(size_t) page].title, titleArea, juce::Justification::centredLeft, 1);

        g.setColour (Theme::text);
        g.setFont (Theme::font (15.0f));
        g.drawFittedText (cards[(size_t) page].body, bodyArea, juce::Justification::topLeft, 8, 1.0f);

        drawPicture (g, pictureArea.toFloat());

        // Pallini delle pagine
        const float d = 7.0f, gap = 7.0f;
        const float total = (float) cards.size() * d + (float) (cards.size() - 1) * gap;
        float x = (float) dotsArea.getCentreX() - total * 0.5f;
        for (int i = 0; i < (int) cards.size(); ++i, x += d + gap)
        {
            g.setColour (i == page ? Theme::accent : Theme::raisedHi);
            g.fillEllipse (x, (float) dotsArea.getCentreY() - d * 0.5f, d, d);
        }
    }

    void resized() override
    {
        auto r = getLocalBounds();
        const int w = juce::jmin (640, r.getWidth() - 24);
        const int h = juce::jmin (440, r.getHeight() - 24);
        card = juce::Rectangle<int> (w, h).withCentre (r.getCentre());

        auto c = card.reduced (18, 14);
        auto bottom = c.removeFromBottom (40);
        skipButton.setBounds (bottom.removeFromLeft (90).reduced (0, 3));
        nextButton.setBounds (bottom.removeFromRight (130).reduced (0, 3));
        bottom.removeFromRight (8);
        backButton.setBounds (bottom.removeFromRight (90).reduced (0, 3));
        dotsArea = c.removeFromBottom (18);     // i pallini sopra ai pulsanti (sul telefono non c'e' spazio di fianco)
        c.removeFromBottom (4);

        counterArea = c.removeFromTop (16);
        titleArea = c.removeFromTop (h > 380 ? 36 : 28);
        c.removeFromTop (6);

        const bool wide = c.getWidth() > 460;
        const bool last = page == (int) cards.size() - 1;
        if (last)
        {
            bodyArea = c.removeFromTop (60);
            c.removeFromTop (6);
            const int cols = wide ? 3 : 2;
            const int rows = (demoButtons.size() + cols - 1) / cols;
            const int bh = juce::jmin (48, c.getHeight() / juce::jmax (1, rows));
            for (int i = 0; i < demoButtons.size(); ++i)
                demoButtons[i]->setBounds (juce::Rectangle<int> (c.getX() + (i % cols) * c.getWidth() / cols, c.getY() + (i / cols) * bh,
                                                                 c.getWidth() / cols, bh).reduced (4));
            pictureArea = {};
        }
        else if (wide)
        {
            pictureArea = c.removeFromRight (c.getWidth() * 2 / 5).reduced (6);
            c.removeFromRight (10);
            bodyArea = c;
        }
        else
        {
            pictureArea = c.removeFromBottom (c.getHeight() / 2).reduced (10, 4);
            bodyArea = c;
        }
    }

    void mouseDown (const juce::MouseEvent& e) override    { dragStartX = e.x; }

    void mouseUp (const juce::MouseEvent& e) override
    {
        const int dx = e.x - dragStartX;
        if (std::abs (dx) > 60)
            show (page + (dx < 0 ? 1 : -1));
    }

private:
    enum Picture { picNone, picLogo, picSteps, picLength, picKeys, picNeon, picRoll, picPlaylist, picRiser, picMixer, picShare, picCpu };
    struct Card { juce::String title, body; Picture pic = picNone; };

    void finish (int demo)
    {
        if (onFinished) onFinished (demo);
    }

    /** Un disegnino per ogni scheda, fatto di forme semplici (niente immagini da caricare). */
    void drawPicture (juce::Graphics& g, juce::Rectangle<float> r)
    {
        if (r.isEmpty())
            return;
        g.setColour (Theme::bg);
        g.fillRoundedRectangle (r, 6.0f);
        auto a = r.reduced (12.0f);

        switch (cards[(size_t) page].pic)
        {
            case picLogo:
            {
                g.setColour (Theme::accent);
                g.setFont (Theme::font (juce::jmin (a.getHeight() * 0.35f, 46.0f), true));
                g.drawText ("JLoop", a, juce::Justification::centred);
                break;
            }
            case picSteps:
            {
                static const char* rows[] = { "x...x...x...x...", "....x.......x...", "..x...x...x...x.", "x.x.x.x.x.x.x.x." };
                const float rh = a.getHeight() / 4.0f, sw = a.getWidth() / 16.0f;
                for (int rr = 0; rr < 4; ++rr)
                    for (int s = 0; s < 16; ++s)
                    {
                        const bool on = rows[rr][s] == 'x';
                        g.setColour (on ? Theme::stepOn : ((s / 4) % 2 == 0 ? Theme::stepOffA : Theme::stepOffB));
                        g.fillRoundedRectangle (a.getX() + (float) s * sw + 1.0f, a.getY() + (float) rr * rh + 2.0f, sw - 2.0f, rh - 4.0f, 2.0f);
                    }
                break;
            }
            case picKeys:
            {
                const int keys = 10;
                const float kw = a.getWidth() / (float) keys;
                for (int k = 0; k < keys; ++k)
                {
                    g.setColour (k == 4 ? Theme::accent : Theme::text);
                    g.fillRect (a.getX() + (float) k * kw + 1.0f, a.getY() + a.getHeight() * 0.4f, kw - 2.0f, a.getHeight() * 0.6f);
                }
                static const bool black[] = { true, true, false, true, true, true, false, true, true };
                for (int k = 0; k < keys - 1; ++k)
                    if (black[k])
                    {
                        g.setColour (Theme::border);
                        g.fillRect (a.getX() + (float) (k + 1) * kw - kw * 0.3f, a.getY() + a.getHeight() * 0.4f, kw * 0.6f, a.getHeight() * 0.36f);
                    }
                g.setColour (Theme::raisedHi);
                for (int i = 0; i < 4; ++i)
                    g.fillEllipse (a.getX() + (float) i * a.getWidth() / 4.0f + 6.0f, a.getY(), a.getHeight() * 0.28f, a.getHeight() * 0.28f);
                break;
            }
            case picRoll:
            {
                const float rh = a.getHeight() / 8.0f;
                for (int rr = 0; rr < 8; ++rr)
                {
                    g.setColour (rr % 3 == 1 ? Theme::raised : Theme::stepOffA);
                    g.fillRect (a.getX(), a.getY() + (float) rr * rh, a.getWidth(), rh - 1.0f);
                }
                struct N { float x, w; int row; };
                static const N notes[] = { { 0.0f, 0.25f, 5 }, { 0.0f, 0.25f, 3 }, { 0.0f, 0.25f, 1 }, { 0.3f, 0.2f, 6 }, { 0.3f, 0.2f, 4 },
                                           { 0.3f, 0.2f, 2 }, { 0.55f, 0.12f, 1 }, { 0.7f, 0.12f, 2 }, { 0.85f, 0.15f, 0 } };
                for (auto& n : notes)
                {
                    g.setColour (n.x < 0.5f ? Theme::accent : Theme::led);
                    g.fillRoundedRectangle (a.getX() + n.x * a.getWidth(), a.getY() + (float) n.row * rh + 1.0f, n.w * a.getWidth() - 2.0f, rh - 3.0f, 2.0f);
                }
                break;
            }
            case picPlaylist:
            {
                const float rh = a.getHeight() / 4.0f, bw = a.getWidth() / 8.0f;
                static const char* lanes[] = { "..xxxxxx", "..xxxx.x", "xxxxxxxx", "....xxxx" };
                const juce::Colour colours[] = { juce::Colour (0xffe8664a), juce::Colour (0xff4aa3e8), juce::Colour (0xff9c6ae8), juce::Colour (0xff59c27a) };
                for (int l = 0; l < 4; ++l)
                    for (int b = 0; b < 8; ++b)
                        if (lanes[l][b] == 'x')
                        {
                            g.setColour (colours[l].withAlpha (0.85f));
                            g.fillRoundedRectangle (a.getX() + (float) b * bw + 1.0f, a.getY() + (float) l * rh + 2.0f, bw - 2.0f, rh - 4.0f, 3.0f);
                        }
                juce::Path p;
                p.startNewSubPath (a.getX(), a.getBottom() - 4.0f);
                p.lineTo (a.getX() + a.getWidth() * 0.35f, a.getY() + 6.0f);
                p.lineTo (a.getRight(), a.getY() + 6.0f);
                g.setColour (Theme::textBright.withAlpha (0.8f));
                g.strokePath (p, juce::PathStrokeType (2.0f));
                break;
            }
            case picMixer:
            {
                const int strips = 6;
                const float sw = a.getWidth() / (float) strips;
                static const float levels[] = { 0.8f, 0.6f, 0.7f, 0.45f, 0.65f, 0.9f };
                for (int s = 0; s < strips; ++s)
                {
                    const float x = a.getX() + (float) s * sw + sw * 0.5f;
                    g.setColour (Theme::raised);
                    g.fillRect (x - 2.0f, a.getY(), 4.0f, a.getHeight());
                    const float y = a.getBottom() - levels[s] * a.getHeight();
                    g.setColour (Theme::led.withAlpha (0.7f));
                    g.fillRect (x + 6.0f, y, 4.0f, a.getBottom() - y);
                    g.setColour (s == strips - 1 ? Theme::accent : Theme::text);
                    g.fillRoundedRectangle (x - sw * 0.3f, y - 5.0f, sw * 0.6f, 10.0f, 2.0f);
                }
                break;
            }
            case picLength:
            {
                // il display ◂ 16q ▸ e sotto i quarti che diventano battute
                auto chip = a.removeFromTop (a.getHeight() * 0.4f).withSizeKeepingCentre (juce::jmin (a.getWidth(), 150.0f), 34.0f);
                Theme::drawSunken (g, chip, Theme::panel.darker (0.3f));
                g.setColour (Theme::accent);
                g.setFont (Theme::font (18.0f, true));
                g.drawText (juce::String (juce::CharPointer_UTF8 ("\xe2\x97\x82   16q   \xe2\x96\xb8")), chip, juce::Justification::centred);
                a.removeFromTop (10.0f);
                const float qw = a.getWidth() / 16.0f;
                for (int q = 0; q < 16; ++q)
                {
                    g.setColour ((q / 4) % 2 == 0 ? Theme::stepOffA.brighter (0.3f) : Theme::stepOffB.brighter (0.3f));
                    g.fillRoundedRectangle (a.getX() + (float) q * qw + 1.0f, a.getY(), qw - 2.0f, a.getHeight() * 0.45f, 2.0f);
                }
                g.setColour (Theme::textDim);
                g.setFont (Theme::font (12.0f, true));
                for (int b = 0; b < 4; ++b)
                    g.drawText ("battuta " + juce::String (b + 1), juce::Rectangle<float> (a.getX() + (float) b * 4.0f * qw, a.getY() + a.getHeight() * 0.5f, 4.0f * qw, 18.0f), juce::Justification::centred);
                break;
            }
            case picNeon:
            {
                const juce::Colour cols[] { juce::Colour (0xffff3030), juce::Colour (0xffffb020), juce::Colour (0xff8cff3a), juce::Colour (0xffc46bff) };
                const float w = a.getWidth() / 4.0f, d = juce::jmin (w * 0.8f, a.getHeight() * 0.7f);
                for (int i = 0; i < 4; ++i)
                {
                    auto k = juce::Rectangle<float> (a.getX() + (float) i * w, a.getY(), w, a.getHeight()).withSizeKeepingCentre (d, d);
                    g.setColour (juce::Colour (0xff1a1d21));
                    g.fillEllipse (k.reduced (d * 0.12f));
                    juce::Path arc;
                    arc.addCentredArc (k.getCentreX(), k.getCentreY(), d * 0.45f, d * 0.45f, 0.0f, -2.4f, -2.4f + 3.2f + 0.4f * (float) i, true);
                    g.setColour (cols[i].withAlpha (0.3f));
                    g.strokePath (arc, juce::PathStrokeType (7.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
                    g.setColour (cols[i]);
                    g.strokePath (arc, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
                }
                break;
            }
            case picRiser:
            {
                juce::Path p;
                p.startNewSubPath (a.getX(), a.getBottom());
                for (int i = 0; i <= 40; ++i)
                {
                    const float x = (float) i / 40.0f;
                    p.lineTo (a.getX() + x * a.getWidth() * 0.85f, a.getBottom() - x * x * a.getHeight());
                }
                g.setColour (Theme::cyan.withAlpha (0.15f));
                auto fill = p; fill.lineTo (a.getX() + a.getWidth() * 0.85f, a.getBottom()); fill.closeSubPath();
                g.fillPath (fill);
                g.setColour (Theme::cyan);
                g.strokePath (p, juce::PathStrokeType (2.5f));
                g.setColour (juce::Colour (0xffff6a5a));
                static const float hits[] { 0.0f, 0.25f, 0.5f, 0.625f, 0.75f, 0.8125f, 0.875f, 0.9063f, 0.9375f, 0.9688f };
                for (float h : hits)
                    g.fillRect (a.getX() + h * a.getWidth() * 0.85f, a.getBottom() - 12.0f, 2.0f, 12.0f);
                g.setColour (Theme::textBright);
                g.fillRect (a.getX() + a.getWidth() * 0.85f, a.getY(), 4.0f, a.getHeight());
                g.setFont (Theme::font (13.0f, true));
                g.drawText ("DROP", juce::Rectangle<float> (a.getX() + a.getWidth() * 0.85f - 50.0f, a.getY(), 46.0f, 20.0f), juce::Justification::centredRight);
                break;
            }
            case picShare:
            {
                auto f = a.withSizeKeepingCentre (juce::jmin (a.getWidth() * 0.4f, 90.0f), juce::jmin (a.getHeight() * 0.8f, 110.0f));
                g.setColour (Theme::raisedHi);
                g.fillRoundedRectangle (f, 6.0f);
                g.setColour (Theme::accent);
                g.setFont (Theme::font (16.0f, true));
                g.drawText (".jloop", f, juce::Justification::centred);
                juce::Path arrow;
                const float y = f.getCentreY();
                arrow.addArrow ({ f.getRight() + 10.0f, y, juce::jmin (a.getRight(), f.getRight() + 70.0f), y }, 4.0f, 14.0f, 12.0f);
                g.setColour (Theme::cyan);
                g.fillPath (arrow);
                break;
            }
            case picCpu:
            {
                auto m = a.withSizeKeepingCentre (juce::jmin (a.getWidth(), 220.0f), 40.0f);
                Theme::drawSunken (g, m);
                g.setColour (Theme::led);
                g.fillRoundedRectangle (m.reduced (6.0f).withWidth ((m.getWidth() - 12.0f) * 0.35f), 3.0f);
                g.setColour (Theme::textBright);
                g.setFont (Theme::font (15.0f, true));
                g.drawText ("CPU 35%", m, juce::Justification::centred);
                break;
            }
            default: break;
        }
    }

    std::vector<Card> cards;
    int page = 0, dragStartX = 0;
    juce::TextButton backButton { "Indietro" }, nextButton { "Avanti" }, skipButton { "Chiudi" };
    juce::OwnedArray<juce::TextButton> demoButtons;
    juce::Rectangle<int> card, counterArea, titleArea, bodyArea, pictureArea, dotsArea;
};
