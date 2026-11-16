#pragma once

#include <JuceHeader.h>
#include <functional>
#include <vector>
#include "Theme.h"

//==============================================================================
/**
    Il menu principale di AudioNaut: un pannello che esce da sinistra (tocca fuori per chiudere).

    A sinistra le sezioni (Progetto, Crea, Esporta, Impostazioni, Aiuto); a destra le voci della
    sezione scelta, a due colonne quando c'e' spazio. Le voci con un elenco (progetti, demo, elimina)
    aprono una pagina interna con "Indietro".

    Il pannello non sa fare niente da solo: ogni voce ha un numero di comando (gli stessi del vecchio
    menu a tendina) e lo passa a onCommand. Gli interruttori e le scelte lasciano il pannello aperto
    e si aggiornano subito; tutte le altre voci chiudono il pannello e poi eseguono il comando.
*/
class MainMenuPanel : public juce::Component,
                      private juce::Timer
{
public:
    /** Quello che il pannello deve mostrare: lo prepara chi lo apre (e lo rifa' dopo ogni scelta). */
    struct State
    {
        juce::String projectName, version;
        struct Project { juce::String name, when; };
        std::vector<Project> projects;            // dal piu' recente
        juce::StringArray demos;
        bool hasSong = false;
        bool haptics = true, lowPower = false, desk = true, synthWindows = false, direct2D = false;
        int quantize = 12;                          // 0 libera, 6 = 1/32, 12 = 1/16, 24 = 1/8
        std::vector<int> buffers;                   // dimensioni possibili
        int buffer = 0;
        double sampleRate = 48000.0;
        int uiScalePct = 100;
        juce::StringArray midiDevices;
        bool bluetooth = false;
        bool isDesktop = false, isWindows = false;
    };

    std::function<State()> getState;
    std::function<void (int)> onCommand;
    std::function<void()> onClosed;
    juce::Image logo;

    MainMenuPanel()
    {
        setWantsKeyboardFocus (true);
        viewport.setViewedComponent (&content, false);
        viewport.setScrollBarsShown (true, false);
        viewport.setScrollBarThickness (6);
        viewport.setScrollOnDragMode (juce::Viewport::ScrollOnDragMode::all);
        addAndMakeVisible (viewport);
    }

    void open()
    {
        refresh();
        showSection (section);
        shown = 0.0f;
        closing = false;
        startTimerHz (60);
        grabKeyboardFocus();
    }

    void close()
    {
        if (closing) return;
        closing = true;
        startTimerHz (60);
    }

    /** Per le foto di prova: mostra subito una sezione (0..4) gia' aperta. */
    void showForTest (int s)
    {
        refresh();
        section = juce::jlimit (0, numSections - 1, s);
        showSection (section);
        shown = 1.0f;
        resized();
    }

    //==========================================================================
    void paint (juce::Graphics& g) override
    {
        g.fillAll (juce::Colours::black.withAlpha (0.62f * shown));

        const auto d = drawerBounds().toFloat();
        g.setColour (juce::Colours::black.withAlpha (0.5f * shown));
        g.fillRect (d.withX (d.getRight()).withWidth (14.0f));   // ombra
        g.setColour (cPanel);
        g.fillRect (d);
        g.setColour (cLine);
        g.drawVerticalLine ((int) d.getRight() - 1, d.getY(), d.getBottom());

        // la colonna delle sezioni
        auto rail = railBounds().toFloat();
        g.setColour (cRail);
        g.fillRect (rail);
        g.setColour (juce::Colour (0xff1b2733));
        g.drawVerticalLine ((int) rail.getRight() - 1, rail.getY(), rail.getBottom());

        auto brand = rail.reduced (8.0f, 10.0f).removeFromTop (34.0f);
        if (narrow())
        {
            if (logo.isValid())
                g.drawImage (logo, brand.withSizeKeepingCentre (34.0f, 34.0f), juce::RectanglePlacement::centred);
        }
        else
        {
            if (logo.isValid())
                g.drawImage (logo, brand.removeFromLeft (34.0f).reduced (1.0f), juce::RectanglePlacement::centred);
            brand.removeFromLeft (6.0f);
            g.setColour (cCyan);
            g.setFont (Theme::font (12.0f, true).withExtraKerningFactor (0.12f));
            g.drawText ("AUDIONAUT", brand.removeFromTop (18.0f), juce::Justification::bottomLeft, false);
            g.setColour (cDim);
            g.setFont (Theme::font (9.5f));
            g.drawText ("versione " + state.version, brand, juce::Justification::topLeft, false);
        }

        for (int i = 0; i < numSections; ++i)
        {
            const auto r = sectionRect (i).toFloat();
            const bool sel = i == section;
            if (sel)
            {
                g.setColour (cCyanBg);
                g.fillRoundedRectangle (r, 8.0f);
                g.setColour (cCyan.withAlpha (0.35f));
                g.drawRoundedRectangle (r.reduced (0.5f), 8.0f, 1.0f);
            }
            const auto col = sel ? cCyan : cText;
            if (narrow())
            {
                auto ir = r.reduced (2.0f, 6.0f);
                drawSectionIcon (g, i, ir.removeFromTop (20.0f).withSizeKeepingCentre (18.0f, 18.0f), col);
                g.setColour (col);
                g.setFont (Theme::font (10.0f, true));
                g.drawFittedText (sectionNames()[i], ir.toNearestInt(), juce::Justification::centred, 1, 0.7f);
                continue;
            }
            auto ir = r.reduced (12.0f, 0.0f);
            drawSectionIcon (g, i, ir.removeFromLeft (17.0f).withSizeKeepingCentre (17.0f, 17.0f), col);
            ir.removeFromLeft (10.0f);
            g.setColour (col);
            g.setFont (Theme::font (13.5f, true));
            g.drawFittedText (sectionNames()[i], ir.toNearestInt(), juce::Justification::centredLeft, 1, 0.8f);
        }

        // "tocca qui fuori per chiudere"
        auto outside = getLocalBounds().withLeft ((int) d.getRight() + 14);
        if (outside.getWidth() > 70 && shown > 0.9f)
        {
            g.setColour (juce::Colour (0xff9aa6b2));
            g.setFont (Theme::font (10.5f));
            g.drawFittedText ("tocca qui fuori per chiudere", outside.removeFromBottom (44).reduced (6, 0), juce::Justification::centred, 2, 0.8f);
        }
    }

    void resized() override
    {
        viewport.setBounds (pageBounds());
        layoutContent();
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        const auto p = e.getPosition();
        if (! drawerBounds().contains (p)) { close(); return; }
        for (int i = 0; i < numSections; ++i)
            if (sectionRect (i).contains (p))
            {
                section = i;
                subPage = SubPage::none;
                showSection (i);
                repaint();
                return;
            }
    }

    bool keyPressed (const juce::KeyPress& k) override
    {
        if (k == juce::KeyPress::escapeKey || k == juce::KeyPress::backspaceKey)
        {
            if (subPage != SubPage::none) { subPage = SubPage::none; showSection (section); }
            else close();
            return true;
        }
        return false;
    }

private:
    //==========================================================================
    static constexpr int numSections = 5;
    inline static const juce::Colour cPanel { 0xff0f161e }, cRail { 0xff0b1117 }, cCard { 0xff141c25 }, cLine { 0xff233140 },
                                     cText { 0xffdfe6ee }, cDim { 0xff7f8c99 }, cCyan { 0xff46d9f0 }, cCyanBg { 0xff0d2229 }, cOrange { 0xfff0922b };

    static juce::StringArray sectionNames()   { return { "Progetto", "Crea", "Esporta", "Impostazioni", "Aiuto" }; }

    enum class SubPage { none, allProjects, demos, deleteProject };

    /** Una voce del menu: si disegna da sola. */
    struct Item : public juce::Component
    {
        enum Kind { action, toggle, segment, project, header, save, info, about, back };
        Kind kind = action;
        juce::String text, sub, badge;
        int command = 0;
        bool on = false, chevron = false, wide = false, enabled = true, stacked = false;
        struct Choice { juce::String label; int command; bool on; };
        std::vector<Choice> choices;
        MainMenuPanel* owner = nullptr;
        juce::Image image;

        int preferredHeight() const
        {
            switch (kind)
            {
                case header:  return 20;
                case project: return 42;
                case save:    return 44;
                case about:   return 64;
                case segment: return stacked ? 66 : 38;
                default:      return 38;
            }
        }

        void paint (juce::Graphics& g) override
        {
            auto r = getLocalBounds().toFloat();
            if (kind == header)
            {
                g.setColour (cDim);
                g.setFont (Theme::font (9.5f, true).withExtraKerningFactor (0.14f));
                g.drawText (text.toUpperCase(), r, juce::Justification::bottomLeft, true);
                return;
            }
            if (kind == save)
            {
                auto left = r.withTrimmedRight (110.0f);
                g.setColour (cDim);
                g.setFont (Theme::font (9.5f, true).withExtraKerningFactor (0.14f));
                g.drawText ("PROGETTO APERTO", left.removeFromTop (16.0f), juce::Justification::bottomLeft, true);
                g.setColour (cText);
                g.setFont (Theme::font (16.0f, true));
                g.drawFittedText (text, left.toNearestInt(), juce::Justification::centredLeft, 1, 0.7f);
                const auto b = saveRect().toFloat();
                g.setColour (isMouseButtonDown() ? cOrange.darker (0.2f) : cOrange);
                g.fillRoundedRectangle (b, 8.0f);
                g.setColour (juce::Colour (0xff1a1006));
                g.setFont (Theme::font (13.5f, true));
                g.drawText ("Salva", b, juce::Justification::centred, false);
                return;
            }
            if (kind == about)
            {
                g.setColour (cLine);
                g.drawRoundedRectangle (r.reduced (0.5f), 10.0f, 1.0f);
                auto in = r.reduced (10.0f);
                if (image.isValid())
                    g.drawImage (image, in.removeFromLeft (44.0f).withSizeKeepingCentre (44.0f, 44.0f), juce::RectanglePlacement::centred);
                in.removeFromLeft (12.0f);
                g.setColour (cCyan);
                g.setFont (Theme::font (14.0f, true).withExtraKerningFactor (0.15f));
                g.drawText ("AUDIONAUT", in.removeFromTop (in.getHeight() * 0.5f), juce::Justification::bottomLeft, false);
                g.setColour (juce::Colour (0xff9aa6b2));
                g.setFont (Theme::font (11.0f));
                g.drawText ("versione " + text, in, juce::Justification::topLeft, false);
                return;
            }

            const bool down = isMouseButtonDown() && kind != segment && kind != info;
            g.setColour (kind == back ? juce::Colours::transparentBlack : down ? juce::Colour (0xff1c2733) : cCard);
            g.fillRoundedRectangle (r.reduced (0.5f), 8.0f);
            if (kind != back)
            {
                g.setColour (cLine);
                g.drawRoundedRectangle (r.reduced (0.5f), 8.0f, 1.0f);
            }
            auto in = r.reduced (12.0f, 0.0f);
            const float alpha = enabled ? 1.0f : 0.4f;

            // a destra: interruttore, scelte, freccia, etichetta
            if (kind == toggle)
            {
                auto sw = in.removeFromRight (34.0f).withSizeKeepingCentre (34.0f, 19.0f);
                g.setColour (on ? cCyan : juce::Colour (0xff2a3644));
                g.fillRoundedRectangle (sw, 9.5f);
                g.setColour (juce::Colours::white);
                g.fillEllipse ((on ? sw.removeFromRight (19.0f) : sw.removeFromLeft (19.0f)).reduced (2.0f));
                in.removeFromRight (6.0f);
            }
            if (kind == segment)
            {
                const auto seg = segmentArea();
                g.setColour (juce::Colour (0xff2a3948));
                g.drawRoundedRectangle (seg.toFloat().reduced (0.5f), 6.0f, 1.0f);
                for (size_t i = 0; i < choices.size(); ++i)
                {
                    const auto cr = choiceRect ((int) i).toFloat();
                    if (choices[i].on)
                    {
                        g.setColour (cCyanBg);
                        g.fillRoundedRectangle (cr.reduced (1.0f), 5.0f);
                    }
                    g.setColour (choices[i].on ? cCyan : juce::Colour (0xff9aa6b2));
                    g.setFont (Theme::font (11.0f, choices[i].on));
                    g.drawFittedText (choices[i].label, cr.toNearestInt(), juce::Justification::centred, 1, 0.7f);
                }
                if (stacked) in = in.withHeight (32.0f);
                else in.setRight ((float) seg.getX() - 6.0f);
            }
            if (chevron)
            {
                g.setColour (cDim);
                g.setFont (Theme::font (15.0f));
                g.drawText (juce::String (juce::CharPointer_UTF8 ("\xe2\x80\xba")), in.removeFromRight (12.0f), juce::Justification::centredRight, false);
            }
            if (badge.isNotEmpty())
            {
                g.setFont (Theme::font (8.5f, true));
                const float bw = (float) juce::GlyphArrangement::getStringWidthInt (g.getCurrentFont(), badge) + 10.0f;
                auto br = in.removeFromRight (bw).withSizeKeepingCentre (bw, 15.0f);
                g.setColour (juce::Colour (0xff3a4a5a));
                g.drawRoundedRectangle (br, 4.0f, 1.0f);
                g.setColour (juce::Colour (0xff9fb0c0));
                g.drawText (badge, br, juce::Justification::centred, false);
                in.removeFromRight (6.0f);
            }

            if (kind == project)
            {
                g.setColour (cText);
                g.setFont (Theme::font (13.0f, true));
                g.drawFittedText (text, in.removeFromTop (in.getHeight() * 0.58f).toNearestInt(), juce::Justification::bottomLeft, 1, 0.8f);
                g.setColour (cDim);
                g.setFont (Theme::font (9.5f));
                g.drawText (sub, in, juce::Justification::topLeft, true);
                return;
            }
            if (kind == back)
            {
                g.setColour (cCyan);
                g.setFont (Theme::font (13.0f, true));
                g.drawText (juce::String (juce::CharPointer_UTF8 ("\xe2\x80\xb9  Indietro   ")) + text, in, juce::Justification::centredLeft, true);
                return;
            }

            g.setColour (cText.withAlpha (alpha));
            g.setFont (Theme::font (13.0f, true));
            if (sub.isNotEmpty() && kind != info && getWidth() >= 300)
            {
                const auto main = Theme::font (13.0f, true);
                const float tw = (float) juce::GlyphArrangement::getStringWidthInt (main, text);
                g.drawFittedText (text, in.removeFromLeft (juce::jmin (tw + 2.0f, in.getWidth())).toNearestInt(), juce::Justification::centredLeft, 1, 0.8f);
                in.removeFromLeft (8.0f);
                g.setColour (cDim);
                g.setFont (Theme::font (9.5f));
                g.drawFittedText (sub, in.toNearestInt(), juce::Justification::centredLeft, 1, 0.8f);
            }
            else if (kind == info)
            {
                g.drawFittedText (text, in.toNearestInt(), juce::Justification::centredLeft, 1, 0.8f);
                g.setColour (cDim);
                g.setFont (Theme::font (10.0f));
                g.drawFittedText (sub, in.toNearestInt(), juce::Justification::centredRight, 1, 0.8f);
            }
            else
                g.drawFittedText (text, in.toNearestInt(), juce::Justification::centredLeft, 1, 0.75f);
        }

        juce::Rectangle<int> saveRect() const   { return getLocalBounds().removeFromRight (100).withSizeKeepingCentre (100, 36); }

        juce::Rectangle<int> segmentArea() const
        {
            if (stacked) return getLocalBounds().reduced (8, 0).withTrimmedTop (32).withTrimmedBottom (7);
            const int each = choices.size() > 4 ? 38 : 44;
            const int w = juce::jmin (getWidth() - 110, each * (int) choices.size());
            return getLocalBounds().reduced (8, 7).removeFromRight (w);
        }
        juce::Rectangle<int> choiceRect (int i) const
        {
            const auto a = segmentArea();
            const int n = juce::jmax (1, (int) choices.size());
            return { a.getX() + a.getWidth() * i / n, a.getY(), a.getWidth() / n, a.getHeight() };
        }

        void mouseDown (const juce::MouseEvent&) override   { repaint(); }
        void mouseUp (const juce::MouseEvent& e) override
        {
            repaint();
            if (! enabled || e.getDistanceFromDragStart() > 8 || ! getLocalBounds().contains (e.getPosition())) return;   // era uno scorrimento
            // tutto quello che segue rifa' le voci (e quindi elimina questa): si fa dopo, fuori dal tocco
            auto later = [this] (std::function<void (MainMenuPanel&)> fn)
            {
                juce::Component::SafePointer<MainMenuPanel> safe (owner);
                juce::MessageManager::callAsync ([safe, fn] { if (safe != nullptr) fn (*safe); });
            };
            if (kind == segment)
            {
                for (size_t i = 0; i < choices.size(); ++i)
                    if (choiceRect ((int) i).contains (e.getPosition()))
                    {
                        const int cmd = choices[i].command;
                        later ([cmd] (MainMenuPanel& m) { m.run (cmd, true); });
                    }
                return;
            }
            if (kind == save && ! saveRect().contains (e.getPosition())) return;
            if (kind == header || kind == info || kind == about) return;
            const int cmd = command;
            if (kind == back)        later ([] (MainMenuPanel& m) { m.subPage = SubPage::none; m.showSection (m.section); });
            else if (kind == toggle) later ([cmd] (MainMenuPanel& m) { m.run (cmd, true); });
            else if (cmd == -10)     later ([] (MainMenuPanel& m) { m.openSubPage (SubPage::allProjects); });
            else if (cmd == -11)     later ([] (MainMenuPanel& m) { m.openSubPage (SubPage::demos); });
            else if (cmd == -12)     later ([] (MainMenuPanel& m) { m.openSubPage (SubPage::deleteProject); });
            else                     later ([cmd] (MainMenuPanel& m) { m.run (cmd, false); });
        }
    };

    //==========================================================================
    bool narrow() const                     { return drawerBounds().getWidth() < 560; }
    juce::Rectangle<int> drawerBounds() const
    {
        const int w = juce::jmin (740, getWidth() - (getWidth() < 600 ? 40 : 100));
        const int x = (int) std::round (-(float) w * (1.0f - easeOut (shown)));
        return { x, 0, w, getHeight() };
    }
    juce::Rectangle<int> railBounds() const { return drawerBounds().removeFromLeft (narrow() ? 80 : 150); }
    juce::Rectangle<int> pageBounds() const { return drawerBounds().withTrimmedLeft (railBounds().getWidth()).reduced (12, 10); }
    juce::Rectangle<int> sectionRect (int i) const
    {
        auto r = railBounds().reduced (8, 0);
        r.removeFromTop (58);
        const int h = narrow() ? juce::jmin (58, (getHeight() - 70) / numSections) : juce::jmin (42, (getHeight() - 70) / numSections);
        return r.withY (r.getY() + i * (h + 2)).withHeight (h);
    }
    static float easeOut (float x)          { return 1.0f - std::pow (1.0f - juce::jlimit (0.0f, 1.0f, x), 3.0f); }

    static void drawSectionIcon (juce::Graphics& g, int i, juce::Rectangle<float> r, juce::Colour c)
    {
        juce::Path p;
        const float x = r.getX(), y = r.getY(), s = r.getWidth();
        auto P = [x, y, s] (float u, float v) { return juce::Point<float> (x + u * s, y + v * s); };
        switch (i)
        {
            case 0:   // cartella
                p.startNewSubPath (P (0.08f, 0.25f)); p.lineTo (P (0.4f, 0.25f)); p.lineTo (P (0.5f, 0.36f)); p.lineTo (P (0.92f, 0.36f));
                p.lineTo (P (0.92f, 0.82f)); p.lineTo (P (0.08f, 0.82f)); p.closeSubPath();
                break;
            case 1:   // scintilla
                p.startNewSubPath (P (0.5f, 0.05f)); p.lineTo (P (0.6f, 0.4f)); p.lineTo (P (0.95f, 0.5f)); p.lineTo (P (0.6f, 0.6f));
                p.lineTo (P (0.5f, 0.95f)); p.lineTo (P (0.4f, 0.6f)); p.lineTo (P (0.05f, 0.5f)); p.lineTo (P (0.4f, 0.4f)); p.closeSubPath();
                break;
            case 2:   // freccia in giu'
                p.startNewSubPath (P (0.5f, 0.1f)); p.lineTo (P (0.5f, 0.66f));
                p.startNewSubPath (P (0.25f, 0.42f)); p.lineTo (P (0.5f, 0.66f)); p.lineTo (P (0.75f, 0.42f));
                p.startNewSubPath (P (0.15f, 0.9f)); p.lineTo (P (0.85f, 0.9f));
                break;
            case 3:   // ingranaggio
                p.addEllipse (r.reduced (s * 0.3f));
                for (int k = 0; k < 8; ++k)
                {
                    const float a = juce::MathConstants<float>::twoPi * (float) k / 8.0f;
                    p.startNewSubPath (r.getCentre() + juce::Point<float> (std::cos (a), std::sin (a)) * s * 0.3f);
                    p.lineTo (r.getCentre() + juce::Point<float> (std::cos (a), std::sin (a)) * s * 0.48f);
                }
                break;
            default:  // punto di domanda nel cerchio
                p.addEllipse (r.reduced (1.0f));
                break;
        }
        g.setColour (c);
        g.strokePath (p, juce::PathStrokeType (1.7f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        if (i == 4)
        {
            g.setFont (Theme::font (s * 0.7f, true));
            g.drawText ("?", r, juce::Justification::centred, false);
        }
    }

    //==========================================================================
    void refresh()
    {
        if (getState) state = getState();
    }

    void run (int command, bool staysOpen)
    {
        if (staysOpen)
        {
            if (onCommand) onCommand (command);
            refresh();
            const auto pos = viewport.getViewPosition();
            if (subPage == SubPage::none) showSection (section); else openSubPage (subPage);
            viewport.setViewPosition (pos);
            return;
        }
        pending = command;
        close();
    }

    Item& add (Item::Kind kind, const juce::String& text, int command = 0)
    {
        auto* it = items.add (new Item());
        it->kind = kind;
        it->text = text;
        it->command = command;
        it->owner = this;
        content.addAndMakeVisible (it);
        return *it;
    }

    void clearItems()
    {
        items.clear();
    }

    void showSection (int s)
    {
        clearItems();
        const bool pc = state.isDesktop;
        auto& st = state;
        switch (s)
        {
            case 0:
            {
                add (Item::save, st.projectName, 2).wide = true;
                add (Item::action, juce::String (juce::CharPointer_UTF8 ("\xef\xbc\x8b  Nuovo progetto")), 1);
                add (Item::action, juce::String (juce::CharPointer_UTF8 ("Salva con nome\xe2\x80\xa6")), 3);
                add (Item::action, "Progetti demo", -11).chevron = true;
                add (Item::action, juce::String (juce::CharPointer_UTF8 ("Apri progetto da file\xe2\x80\xa6")), 12);
                add (Item::action, pc ? juce::String (juce::CharPointer_UTF8 ("Esporta progetto (.jloop)\xe2\x80\xa6"))
                                      : juce::String (juce::CharPointer_UTF8 ("Condividi progetto\xe2\x80\xa6")), 11);
                {
                    auto& del = add (Item::action, "Elimina un progetto", -12);
                    del.chevron = true;
                    del.enabled = ! st.projects.empty();
                }
                add (Item::header, "Apri (i tuoi progetti salvati)").wide = true;
                if (st.projects.empty())
                {
                    auto& none = add (Item::info, "Nessun progetto salvato");
                    none.sub = "usa Salva";
                    none.wide = true;
                }
                for (int i = 0; i < (int) st.projects.size() && i < 4; ++i)
                {
                    auto& p = add (Item::project, st.projects[(size_t) i].name, 1000 + i);
                    p.sub = st.projects[(size_t) i].when;
                }
                if (st.projects.size() > 4)
                {
                    auto& all = add (Item::action, "Tutti i progetti (" + juce::String ((int) st.projects.size()) + ")", -10);
                    all.chevron = true;
                    all.wide = true;
                }
                break;
            }
            case 1:
                add (Item::header, "Apri").wide = true;
                add (Item::action, "Mixer", 7);
                add (Item::action, "Live (lancia i pattern)", 10);
                add (Item::header, "Fatto da AudioNaut").wide = true;
                add (Item::action, juce::String (juce::CharPointer_UTF8 ("Riser automatico\xe2\x80\xa6")), 13);
                add (Item::action, juce::String (juce::CharPointer_UTF8 ("Downer automatico\xe2\x80\xa6")), 14);
                add (Item::action, juce::String (juce::CharPointer_UTF8 ("Vocals a tempo\xe2\x80\xa6")), 22);
                break;
            case 2:
                add (Item::header, "Audio").wide = true;
                add (Item::action, juce::String (juce::CharPointer_UTF8 ("Esporta audio (WAV / OGG)\xe2\x80\xa6")), 6);
                add (Item::header, "MIDI (.mid)").wide = true;
                add (Item::action, "Pattern attuale", 2101);
                add (Item::action, "Canzone (playlist)", 2102).enabled = st.hasSong;
                add (Item::header, "Progetto").wide = true;
                add (Item::action, pc ? juce::String (juce::CharPointer_UTF8 ("Esporta progetto (.jloop)\xe2\x80\xa6"))
                                      : juce::String (juce::CharPointer_UTF8 ("Condividi progetto\xe2\x80\xa6")), 11);
                break;
            case 3:
            {
                if (pc) add (Item::action, juce::String (juce::CharPointer_UTF8 ("Scheda audio\xe2\x80\xa6")), 15);
                add (Item::action, "Info audio", 4);
                if (! st.buffers.empty())
                {
                    auto& lat = add (Item::segment, "Latenza");
                    lat.wide = true;
                    lat.sub = juce::String (1000.0 * st.buffer / juce::jmax (1.0, st.sampleRate), 1) + " ms";
                    for (int b : st.buffers)
                        lat.choices.push_back ({ juce::String (b), 5000 + b, b == st.buffer });
                }
                {
                    auto& q = add (Item::segment, "Registra note");
                    q.wide = narrow();
                    for (auto [label, cmd, val] : { std::tuple<const char*, int, int> { "Libera", 5200, 0 }, { "1/32", 5201, 6 }, { "1/16", 5202, 12 }, { "1/8", 5203, 24 } })
                        q.choices.push_back ({ label, cmd, st.quantize == val });
                }
                if (pc)
                {
                    auto& sz = add (Item::segment, "Dimensione");
                    sz.wide = true;
                    for (int pct : { 75, 85, 100, 110, 125, 150 })
                        sz.choices.push_back ({ juce::String (pct) + "%", 1700 + pct, pct == st.uiScalePct });
                }
                {
                    auto& m = add (Item::info, "Tastiere MIDI");
                    m.sub = st.midiDevices.isEmpty() ? juce::String ("nessuna collegata") : st.midiDevices.joinIntoString (", ");
                }
                if (st.bluetooth) add (Item::action, juce::String (juce::CharPointer_UTF8 ("Collega tastiera Bluetooth\xe2\x80\xa6")), 5300);
                auto toggle = [this] (const juce::String& t, const juce::String& sub, int cmd, bool on, const juce::String& badge = {})
                {
                    auto& it = add (Item::toggle, t, cmd);
                    it.sub = sub; it.on = on; it.badge = badge;
                };
                toggle ("Vibrazione al tocco", {}, 5000, st.haptics);
                toggle ("Risparmio CPU", "meno voci nei synth", 5100, st.lowPower);
                if (pc)
                {
                    toggle ("Banco di lavoro", "libreria a sinistra", 18, st.desk);
                    toggle ("Synth in finestre", {}, 19, st.synthWindows);
                }
                if (st.isWindows) toggle ("Grafica Direct2D", "se sfarfalla, spenta", 5104, st.direct2D);
                break;
            }
            default:
                add (Item::action, "Guida rapida", 8);
                if (pc) add (Item::action, "Tasti rapidi", 16);
                add (Item::action, juce::String (juce::CharPointer_UTF8 ("Cronologia delle modifiche\xe2\x80\xa6")), 9);
                add (Item::action, "Info audio", 4);
                {
                    auto& a = add (Item::about, st.version);
                    a.image = logo;
                    a.wide = true;
                }
                break;
        }
        layoutContent();
        viewport.setViewPosition (0, 0);
    }

    void openSubPage (SubPage p)
    {
        subPage = p;
        clearItems();
        const auto& st = state;
        if (p == SubPage::allProjects)
        {
            add (Item::back, "Tutti i progetti").wide = true;
            for (int i = 0; i < (int) st.projects.size(); ++i)
                add (Item::project, st.projects[(size_t) i].name, 1000 + i).sub = st.projects[(size_t) i].when;
        }
        else if (p == SubPage::demos)
        {
            add (Item::back, "Progetti demo").wide = true;
            for (int i = 0; i < st.demos.size(); ++i)
            {
                auto& d = add (Item::project, st.demos[i].upToFirstOccurrenceOf ("  -  ", false, false), 3000 + i);
                d.sub = st.demos[i].fromFirstOccurrenceOf ("  -  ", false, false);
            }
        }
        else if (p == SubPage::deleteProject)
        {
            add (Item::back, "Elimina un progetto").wide = true;
            for (int i = 0; i < (int) st.projects.size(); ++i)
                add (Item::project, st.projects[(size_t) i].name, 2000 + i).sub = juce::String ("tocca per eliminare (ti chiede conferma)");
        }
        layoutContent();
        viewport.setViewPosition (0, 0);
    }

    void layoutContent()
    {
        const int w = viewport.getWidth() - (viewport.canScrollVertically() ? 8 : 0);
        const int cols = w >= 420 ? 2 : 1, gap = 6;
        const int cw = (w - gap * (cols - 1)) / cols;
        int x = 0, y = 0, rowH = 0;
        for (auto* it : items)
        {
            if (it->kind == Item::segment)
                it->stacked = (it->wide || cols == 1 ? w : cw) < 30 + 80 + 40 * (int) it->choices.size();
            const int h = it->preferredHeight();
            if (it->wide || cols == 1)
            {
                if (x > 0) { y += rowH + gap; x = 0; rowH = 0; }
                it->setBounds (0, y, w, h);
                y += h + gap;
                continue;
            }
            it->setBounds (x * (cw + gap), y, cw, h);
            rowH = juce::jmax (rowH, h);
            if (++x == cols) { x = 0; y += rowH + gap; rowH = 0; }
        }
        if (x > 0) y += rowH + gap;
        content.setSize (juce::jmax (1, w), juce::jmax (1, y));
    }

    void timerCallback() override
    {
        const float step = 1.0f / (0.18f * 60.0f);
        shown = juce::jlimit (0.0f, 1.0f, shown + (closing ? -step : step));
        resized();
        repaint();
        if (! closing && shown >= 1.0f) stopTimer();
        if (closing && shown <= 0.0f)
        {
            stopTimer();
            const int cmd = pending;
            pending = 0;
            auto done = onClosed;
            auto command = onCommand;
            // il pannello sparisce prima del comando (che puo' aprire finestre e dialoghi)
            juce::MessageManager::callAsync ([done, command, cmd]
            {
                if (done) done();
                if (cmd != 0 && command) command (cmd);
            });
        }
    }

    State state;
    int section = 0, pending = 0;
    SubPage subPage = SubPage::none;
    float shown = 0.0f;
    bool closing = false;
    juce::Viewport viewport;
    juce::Component content;
    juce::OwnedArray<Item> items;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainMenuPanel)
};
