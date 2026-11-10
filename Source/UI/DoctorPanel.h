#pragma once

#include <JuceHeader.h>
#include "Theme.h"
#include "../Engine/ProjectDoctor.h"

//==============================================================================
namespace doctorui
{
    inline juce::Colour severityColour (int s)
    {
        return s == doctor::red ? juce::Colour (0xffff4d5e) : s == doctor::warn ? juce::Colour (0xffff9a3c) : Theme::cyan;
    }
    inline const char* severityName (int s)    { return s == doctor::red ? "ROSSO" : s == doctor::warn ? "ATTENZIONE" : "CONSIGLIO"; }

    /** Pulsante piatto del controllo (arancio pieno = azione principale). */
    class Btn : public juce::Button
    {
    public:
        Btn (const juce::String& text, bool primary = false) : juce::Button (text), main (primary) { setButtonText (text); }
        void paintButton (juce::Graphics& g, bool over, bool down) override
        {
            auto r = getLocalBounds().toFloat().reduced (0.5f);
            const auto base = main ? Theme::accent : Theme::bg;
            g.setColour (! isEnabled() ? base.withAlpha (0.35f) : down ? base.darker (0.25f) : over ? base.brighter (0.1f) : base);
            g.fillRoundedRectangle (r, 6.0f);
            if (! main)
            {
                g.setColour (Theme::edgeLight.withAlpha (0.6f));
                g.drawRoundedRectangle (r, 6.0f, 1.0f);
            }
            g.setColour (main ? juce::Colours::black.withAlpha (isEnabled() ? 0.9f : 0.5f) : (dim ? Theme::textDim : Theme::textBright));
            g.setFont (Theme::font (juce::jlimit (11.0f, 13.5f, r.getHeight() * 0.42f), true));
            g.drawFittedText (getButtonText(), getLocalBounds().reduced (6, 0), juce::Justification::centred, 1, 0.75f);
        }
        bool main, dim = false;
    };
}

//==============================================================================
/** Una carta: un problema, con Sistema / Ascolta / Ignora. Toccandola si apre la guida passo passo. */
class DoctorCard : public juce::Component
{
public:
    DoctorCard (const doctor::Issue& i) : issue (i)
    {
        addAndMakeVisible (fixButton);
        addAndMakeVisible (listenButton);
        addAndMakeVisible (ignoreButton);
        addAndMakeVisible (guideButton);
        ignoreButton.dim = true;
        fixButton.setEnabled (issue.canFix);
        listenButton.setVisible (issue.kind != doctor::tooQuiet && issue.kind != doctor::tooLoud);
        fixButton.onClick = [this] { if (onFix) onFix(); };
        listenButton.onClick = [this] { if (onListen) onListen(); };
        ignoreButton.onClick = [this] { if (onIgnore) onIgnore(); };
        guideButton.onClick = [this] { if (onGuide) onGuide(); };
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
    }

    std::function<void()> onFix, onListen, onIgnore, onGuide;
    doctor::Issue issue;
    bool fixed = false;

    void setFixed()
    {
        fixed = true;
        fixButton.setEnabled (false);
        fixButton.setButtonText (juce::String (juce::CharPointer_UTF8 ("Sistemato \xe2\x9c\x93")));
        repaint();
    }

    /** Altezza giusta per questa larghezza (il testo va a capo). */
    int heightFor (int width) const
    {
        juce::AttributedString a;
        a.append (issue.detail, Theme::font (12.5f), Theme::text);
        juce::TextLayout tl;
        tl.createLayout (a, (float) (width - 30));
        juce::AttributedString t;
        t.append (titleText(), Theme::font (14.5f, true), Theme::textBright);
        juce::TextLayout tt;
        tt.createLayout (t, (float) (width - 30));
        return 14 + 23 + (int) tt.getHeight() + 6 + (int) tl.getHeight() + 10 + 34 + 12;
    }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().reduced (0.5f, 3.0f);
        const auto col = doctorui::severityColour (issue.severity);
        g.setColour (isMouseOver (true) ? Theme::raisedHi : Theme::raised);
        g.fillRoundedRectangle (r, 8.0f);
        g.setColour (Theme::edgeLight.withAlpha (0.4f));
        g.drawRoundedRectangle (r, 8.0f, 1.0f);
        g.setColour (fixed ? Theme::led : col);
        g.fillRoundedRectangle (r.withWidth (4.0f), 2.0f);

        auto in = r.reduced (14.0f, 10.0f).withTrimmedLeft (2.0f);
        juce::AttributedString t;
        t.append (titleText(), Theme::font (14.5f, true), fixed ? Theme::textDim : Theme::textBright);
        juce::TextLayout tt;
        tt.createLayout (t, in.getWidth());
        // etichetta colorata prima del titolo
        const juce::String tag = fixed ? "FATTO" : doctorui::severityName (issue.severity);
        g.setFont (Theme::font (10.0f, true));
        const float tagW = juce::GlyphArrangement::getStringWidth (g.getCurrentFont(), tag) + 12.0f;
        auto tagBox = juce::Rectangle<float> (in.getX(), in.getY() + 1.0f, tagW, 17.0f);
        g.setColour (fixed ? Theme::led : col);
        g.fillRoundedRectangle (tagBox, 4.0f);
        g.setColour (juce::Colours::black.withAlpha (0.85f));
        g.drawText (tag, tagBox, juce::Justification::centred);
        in.removeFromTop (23.0f);
        tt.draw (g, in.withHeight (tt.getHeight() + 2.0f));
        in.removeFromTop (tt.getHeight() + 6.0f);

        juce::AttributedString a;
        a.append (issue.detail, Theme::font (12.5f), Theme::text.withAlpha (fixed ? 0.5f : 0.9f));
        juce::TextLayout tl;
        tl.createLayout (a, in.getWidth());
        tl.draw (g, in.withHeight (tl.getHeight() + 2.0f));
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (14, 0).withTrimmedLeft (2);
        auto row = r.removeFromBottom (46).withTrimmedBottom (12).withHeight (32);
        const bool small = row.getWidth() < 360;
        fixButton.setBounds (row.removeFromLeft (small ? 86 : 100));
        row.removeFromLeft (6);
        guideButton.setBounds (row.removeFromLeft (small ? 70 : 96));
        row.removeFromLeft (6);
        if (listenButton.isVisible()) { listenButton.setBounds (row.removeFromLeft (small ? 46 : 84)); row.removeFromLeft (6); }
        ignoreButton.setBounds (row.removeFromRight (small ? 66 : 76));
        listenButton.setButtonText (small ? juce::String (juce::CharPointer_UTF8 ("\xe2\x96\xb6")) : juce::String (juce::CharPointer_UTF8 ("\xe2\x96\xb6 Ascolta")));
        guideButton.setButtonText (small ? "Guida" : "Come si fa");
    }

    void mouseUp (const juce::MouseEvent& e) override
    {
        if (! e.mouseWasDraggedSinceMouseDown() && onGuide) onGuide();
    }
    void mouseEnter (const juce::MouseEvent&) override  { repaint(); }
    void mouseExit (const juce::MouseEvent&) override   { repaint(); }

private:
    juce::String titleText() const
    {
        return issue.title;
    }
    doctorui::Btn fixButton { "Sistema", true }, listenButton { "Ascolta" }, ignoreButton { "Ignora" }, guideButton { "Come si fa" };
};

//==============================================================================
/**
    Il pannello "Controllo del progetto": ascolta la canzone, mostra il punteggio, dove sono i problemi (linea del
    tempo) e le carte. "Sistema tutto" applica tutte le correzioni automatiche; ogni carta apre la guida passo passo.
*/
class DoctorPanel : public juce::Component,
                    private juce::Timer
{
public:
    DoctorPanel (SequencerEngine& e, SampleStore& s) : engine (e), samples (s)
    {
        addAndMakeVisible (closeButton);
        addAndMakeVisible (recheckButton);
        addAndMakeVisible (fixAllButton);
        closeButton.onClick = [this] { stop(); if (onClose) onClose(); };
        recheckButton.onClick = [this] { start(); };
        fixAllButton.onClick = [this] { fixAll(); };
        view.setViewedComponent (&content, false);
        view.setScrollBarsShown (true, false);
        view.setScrollBarThickness (6);
        addAndMakeVisible (view);
    }
    ~DoctorPanel() override     { stop(); }

    std::function<void()> onClose;
    std::function<void (const doctor::Issue&)> onGuide;
    std::function<void (const doctor::Issue&)> onListen;
    std::function<juce::var()> getProjectJson;

    /** Ascolta la canzone da capo. */
    void start()
    {
        stop();
        cards.clear();
        report = {};
        finished = false;
        if (getProjectJson == nullptr) return;
        analyzer = std::make_unique<doctor::Analyzer> (getProjectJson(), samples);
        analyzer->startThread (juce::Thread::Priority::normal);
        startTimerHz (20);
        updateLayout();
    }

    void stop()
    {
        stopTimer();
        analyzer.reset();
    }

    /** Applica la correzione di un problema (anche dalla guida). */
    bool fix (const doctor::Issue& is)
    {
        const bool ok = doctor::applyFix (engine, is);
        if (ok)
            for (auto* c : cards)
                if (sameIssue (c->issue, is)) c->setFixed();
        updateLayout();
        return ok;
    }

    void paint (juce::Graphics& g) override
    {
        Theme::drawGlassPanel (g, getLocalBounds().toFloat().reduced (2.0f));
        g.setColour (Theme::textBright);
        g.setFont (Theme::font (16.0f, true));
        g.drawFittedText ("Controllo del progetto", titleArea, juce::Justification::centredLeft, 1);

        if (! finished)
        {
            const float p = analyzer != nullptr ? analyzer->getProgress() : 0.0f;
            auto r = scoreArea.toFloat();
            g.setColour (Theme::text);
            g.setFont (Theme::font (14.0f, true));
            g.drawFittedText ("Ascolto la canzone... " + juce::String (juce::roundToInt (p * 100.0f)) + "%", r.removeFromTop (30).toNearestInt(), juce::Justification::centredLeft, 1);
            auto bar = r.removeFromTop (8.0f);
            Theme::drawSunken (g, bar, Theme::bg, 4.0f);
            g.setColour (Theme::accent);
            g.fillRoundedRectangle (bar.withWidth (bar.getWidth() * p), 4.0f);
            g.setColour (Theme::textDim);
            g.setFont (Theme::font (12.5f));
            g.drawFittedText ("JLoop suona la canzone in silenzio e controlla volume, cassa e basso, tonalita' e frequenze.",
                              r.withTrimmedTop (8.0f).toNearestInt(), juce::Justification::topLeft, 3);
            return;
        }

        // punteggio
        auto r = scoreArea.toFloat();
        const int score = report.score();
        auto ring = r.removeFromLeft (r.getHeight()).reduced (4.0f);
        const auto ringCol = score >= 85 ? Theme::led : score >= 65 ? juce::Colour (0xffff9a3c) : juce::Colour (0xffff4d5e);
        juce::Path back, arc;
        const float th = 6.0f, rad = ring.getWidth() * 0.5f - th * 0.5f;
        back.addCentredArc (ring.getCentreX(), ring.getCentreY(), rad, rad, 0.0f, 0.0f, juce::MathConstants<float>::twoPi, true);
        arc.addCentredArc (ring.getCentreX(), ring.getCentreY(), rad, rad, 0.0f, 0.0f, juce::MathConstants<float>::twoPi * (float) score / 100.0f, true);
        g.setColour (Theme::bg);
        g.strokePath (back, juce::PathStrokeType (th));
        g.setColour (ringCol);
        g.strokePath (arc, juce::PathStrokeType (th, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (Theme::textBright);
        g.setFont (Theme::font (ring.getHeight() * 0.36f, true));
        g.drawText (juce::String (score), ring, juce::Justification::centred);
        r.removeFromLeft (10.0f);
        auto textArea = r.withTrimmedRight ((float) fixAllButton.getWidth() + 8.0f);
        g.setFont (Theme::font (15.0f, true));
        g.drawFittedText (report.verdict(), textArea.removeFromTop (textArea.getHeight() * 0.55f).toNearestInt(), juce::Justification::bottomLeft, 2);
        g.setColour (Theme::textDim);
        g.setFont (Theme::font (12.0f));
        const int secs = juce::roundToInt (report.seconds);
        g.drawFittedText (juce::String (report.song ? "Ascoltata tutta la canzone (" : "Ascoltato il pattern (") + juce::String (secs / 60) + ":"
                          + juce::String (secs % 60).paddedLeft ('0', 2) + ")", textArea.toNearestInt(), juce::Justification::topLeft, 1);

        // linea del tempo: dove sono i problemi
        if (! timelineArea.isEmpty())
        {
            auto t = timelineArea.toFloat();
            Theme::drawSunken (g, t, Theme::lcdBg, 6.0f);
            const int bars = juce::jmax (1, report.bars);
            const int every = bars <= 16 ? 4 : bars <= 64 ? 8 : 16;
            g.setFont (Theme::font (9.5f));
            for (int b = 0; b < bars; b += every)
            {
                const float x = t.getX() + t.getWidth() * (float) b / (float) bars;
                g.setColour (Theme::edgeLight.withAlpha (0.35f));
                g.fillRect (x, t.getY(), 1.0f, t.getHeight());
                g.setColour (Theme::textDim);
                g.drawText (juce::String (b + 1), juce::Rectangle<float> (x + 3.0f, t.getY() + 2.0f, 30.0f, 11.0f), juce::Justification::left);
            }
            int lane = 0;
            for (auto* c : cards)
            {
                if (c->issue.fromBar < 0 || c->fixed) continue;
                const float x0 = t.getX() + t.getWidth() * (float) c->issue.fromBar / (float) bars;
                const float x1 = t.getX() + t.getWidth() * (float) (c->issue.toBar + 1) / (float) bars;
                const float y = t.getBottom() - 9.0f - (float) (lane % 3) * 9.0f;
                g.setColour (doctorui::severityColour (c->issue.severity));
                g.fillRoundedRectangle (juce::Rectangle<float> (x0 + 1.0f, y, juce::jmax (4.0f, x1 - x0 - 2.0f), 6.0f), 3.0f);
                ++lane;
            }
        }
        if (cards.isEmpty())
        {
            g.setColour (Theme::led);
            g.setFont (Theme::font (15.0f, true));
            g.drawFittedText (juce::String (juce::CharPointer_UTF8 ("\xe2\x9c\x93 Tutto a posto: nessun problema trovato.")),
                              view.getBounds().withHeight (60), juce::Justification::centred, 2);
        }
    }

    void resized() override     { updateLayout(); }

    bool isWorking() const      { return analyzer != nullptr && ! finished; }
    const doctor::Report& getReport() const   { return report; }

private:
    static bool sameIssue (const doctor::Issue& a, const doctor::Issue& b)
    {
        return a.kind == b.kind && a.track == b.track && a.track2 == b.track2 && a.channel == b.channel && a.pattern == b.pattern && a.band == b.band;
    }
    static juce::String keyOf (const doctor::Issue& a)
    {
        return juce::String ((int) a.kind) + ":" + juce::String (a.track) + ":" + juce::String (a.track2) + ":" + juce::String (a.channel) + ":" + juce::String (a.pattern) + ":" + juce::String (a.band);
    }

    void timerCallback() override
    {
        if (analyzer == nullptr) { stopTimer(); return; }
        repaint();
        if (! analyzer->isFinished()) return;
        stopTimer();
        report = analyzer->takeReport();
        analyzer.reset();
        doctor::checkNotes (engine, report);
        doctor::sortIssues (report);
        // quelli ignorati restano nascosti
        report.issues.erase (std::remove_if (report.issues.begin(), report.issues.end(), [this] (const doctor::Issue& i) { return ignored.contains (keyOf (i)); }),
                             report.issues.end());
        finished = true;
        for (auto& is : report.issues)
        {
            auto* c = cards.add (new DoctorCard (is));
            content.addAndMakeVisible (c);
            juce::Component::SafePointer<DoctorCard> sc (c);
            c->onFix = [this, sc] { if (sc != nullptr) fix (sc->issue); };
            c->onListen = [this, sc] { if (sc != nullptr && onListen) onListen (sc->issue); };
            c->onGuide = [this, sc] { if (sc != nullptr && onGuide) onGuide (sc->issue); };
            c->onIgnore = [this, sc]
            {
                if (sc == nullptr) return;
                ignored.add (keyOf (sc->issue));
                juce::MessageManager::callAsync ([safe = juce::Component::SafePointer<DoctorPanel> (this), sc]
                {
                    if (safe == nullptr || sc == nullptr) return;
                    safe->cards.removeObject (sc.getComponent());
                    safe->updateLayout();
                });
            };
        }
        updateLayout();
    }

    void fixAll()
    {
        for (auto* c : cards)
            if (! c->fixed && c->issue.canFix && doctor::applyFix (engine, c->issue))
                c->setFixed();
        updateLayout();
    }

    void updateLayout()
    {
        auto r = getLocalBounds().reduced (14, 10);
        auto head = r.removeFromTop (36);
        closeButton.setBounds (head.removeFromRight (36).reduced (2));
        head.removeFromRight (6);
        recheckButton.setVisible (finished);
        if (finished) recheckButton.setBounds (head.removeFromRight (r.getWidth() < 420 ? 44 : 120).reduced (0, 3));
        recheckButton.setButtonText (r.getWidth() < 420 ? juce::String (juce::CharPointer_UTF8 ("\xe2\x9f\xb3")) : juce::String (juce::CharPointer_UTF8 ("Ricontrolla \xe2\x9f\xb3")));
        titleArea = head;
        r.removeFromTop (6);
        const int maxW = 760;
        if (r.getWidth() > maxW) r = r.withSizeKeepingCentre (maxW, r.getHeight());
        scoreArea = r.removeFromTop (finished ? 66 : 90);
        bool anyFix = false;
        for (auto* c : cards) anyFix = anyFix || (! c->fixed && c->issue.canFix);
        fixAllButton.setVisible (finished && anyFix);
        fixAllButton.setBounds (scoreArea.withLeft (scoreArea.getRight() - (scoreArea.getWidth() < 380 ? 104 : 130)).withSizeKeepingCentre (scoreArea.getWidth() < 380 ? 104 : 130, 38).withX (scoreArea.getRight() - (scoreArea.getWidth() < 380 ? 104 : 130)));
        r.removeFromTop (8);
        bool anyBars = false;
        for (auto* c : cards) anyBars = anyBars || c->issue.fromBar >= 0;
        timelineArea = finished && anyBars ? r.removeFromTop (44) : juce::Rectangle<int>();
        if (! timelineArea.isEmpty()) r.removeFromTop (8);
        view.setBounds (r);
        const int w = r.getWidth() - 8;
        int y = 0;
        for (auto* c : cards)
        {
            const int h = c->heightFor (w);
            c->setBounds (0, y, w, h);
            y += h + 2;
        }
        content.setSize (w, juce::jmax (y, 10));
        repaint();
    }

    SequencerEngine& engine;
    SampleStore& samples;
    std::unique_ptr<doctor::Analyzer> analyzer;
    doctor::Report report;
    bool finished = false;
    juce::OwnedArray<DoctorCard> cards;
    juce::StringArray ignored;
    juce::Viewport view;
    juce::Component content;
    juce::Rectangle<int> titleArea, scoreArea, timelineArea;
    doctorui::Btn closeButton { juce::String (juce::CharPointer_UTF8 ("\xe2\x9c\x95")) }, recheckButton { "Ricontrolla" }, fixAllButton { "Sistema tutto", true };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DoctorPanel)
};

//==============================================================================
/**
    La guida passo passo: una barra in basso, sopra l'app, cosi' si fa davvero (mixer, piano roll...) mentre si legge.
    [Apri il mixer]  [Indietro] [Avanti]  ...  [Fallo per me]  [x]
*/
class GuideBar : public juce::Component
{
public:
    GuideBar()
    {
        for (auto* b : { &goButton, &backButton, &nextButton, &doItButton, &closeButton })
            addAndMakeVisible (b);
        backButton.onClick = [this] { show (step - 1); };
        nextButton.onClick = [this] { if (step + 1 < (int) steps.size()) show (step + 1); else if (onDone) onDone(); };
        goButton.onClick = [this] { if (onGo && juce::isPositiveAndBelow (step, (int) steps.size())) onGo (steps[(size_t) step].go); };
        doItButton.onClick = [this] { if (onDoIt) onDoIt(); };
        closeButton.onClick = [this] { if (onClose) onClose(); };
    }

    std::function<void (doctor::Go)> onGo;
    std::function<void()> onDone, onDoIt, onClose;

    void start (const doctor::Issue& is, std::vector<doctor::Step> s)
    {
        issue = is;
        steps = std::move (s);
        doItButton.setVisible (is.canFix);
        show (0);
    }
    const doctor::Issue& getIssue() const   { return issue; }

    /** Altezza giusta per questa larghezza. */
    int heightFor (int width) const
    {
        juce::AttributedString a;
        a.append (stepText(), Theme::font (14.0f), Theme::textBright);
        juce::TextLayout tl;
        tl.createLayout (a, (float) (width - 32));
        return 30 + (int) tl.getHeight() + 14 + (width < 520 ? 2 * 38 + 6 : 38) + 14;
    }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().reduced (2.0f);
        g.setColour (juce::Colours::black.withAlpha (0.45f));
        g.fillRoundedRectangle (r.translated (0.0f, -2.0f).expanded (2.0f), 12.0f);
        g.setColour (Theme::raised);
        g.fillRoundedRectangle (r, 10.0f);
        g.setColour (doctorui::severityColour (issue.severity));
        g.drawRoundedRectangle (r, 10.0f, 1.5f);
        auto in = r.reduced (16.0f, 10.0f);
        auto head = in.removeFromTop (20.0f);
        g.setFont (Theme::font (11.5f, true));
        g.setColour (doctorui::severityColour (issue.severity));
        g.drawFittedText ("PASSO " + juce::String (step + 1) + " DI " + juce::String ((int) steps.size()) + juce::String (juce::CharPointer_UTF8 ("  \xc2\xb7  ")) + issue.title.toUpperCase(),
                          head.withTrimmedRight (40.0f).toNearestInt(), juce::Justification::centredLeft, 1, 0.7f);
        // pallini dei passi
        in.removeFromTop (6.0f);
        juce::AttributedString a;
        a.append (stepText(), Theme::font (14.0f), Theme::textBright);
        juce::TextLayout tl;
        tl.createLayout (a, in.getWidth());
        tl.draw (g, in.withHeight (tl.getHeight() + 2.0f));
        const float dotsY = head.getCentreY();
        for (int i = 0; i < (int) steps.size(); ++i)
        {
            g.setColour (i == step ? Theme::accent : Theme::edgeLight);
            g.fillEllipse (juce::Rectangle<float> (6.0f, 6.0f).withCentre ({ r.getRight() - 52.0f - (float) ((int) steps.size() - 1 - i) * 10.0f, dotsY }));
        }
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (16, 12);
        closeButton.setBounds (r.getRight() - 30, r.getY() - 4, 30, 26);
        const bool narrow = r.getWidth() < 520;
        auto rows = r.removeFromBottom (narrow ? 2 * 38 + 6 : 38);
        auto row1 = narrow ? rows.removeFromTop (38) : rows;
        auto row2 = narrow ? rows.removeFromBottom (38) : rows;
        const bool hasGo = juce::isPositiveAndBelow (step, (int) steps.size()) && steps[(size_t) step].go != doctor::Go::nothing;
        goButton.setVisible (hasGo);
        if (hasGo) { goButton.setBounds (row1.removeFromLeft (juce::jmin (190, row1.getWidth() / (narrow ? 1 : 3)))); row1.removeFromLeft (8); }
        if (narrow)
        {
            doItButton.setBounds (row2.removeFromRight (doItButton.isVisible() ? 130 : 0));
            backButton.setBounds (row2.removeFromLeft (96)); row2.removeFromLeft (6);
            nextButton.setBounds (row2.removeFromLeft (96));
        }
        else
        {
            backButton.setBounds (row1.removeFromLeft (96)); row1.removeFromLeft (6);
            nextButton.setBounds (row1.removeFromLeft (110));
            if (doItButton.isVisible()) doItButton.setBounds (row1.removeFromRight (140));
        }
    }

private:
    juce::String stepText() const   { return juce::isPositiveAndBelow (step, (int) steps.size()) ? steps[(size_t) step].text : juce::String(); }

    void show (int i)
    {
        step = juce::jlimit (0, juce::jmax (0, (int) steps.size() - 1), i);
        backButton.setEnabled (step > 0);
        const bool last = step + 1 >= (int) steps.size();
        nextButton.setButtonText (last ? juce::String (juce::CharPointer_UTF8 ("Fatto \xe2\x9c\x93")) : juce::String (juce::CharPointer_UTF8 ("Avanti \xe2\x96\xb8")));
        if (juce::isPositiveAndBelow (step, (int) steps.size()) && steps[(size_t) step].button.isNotEmpty())
            goButton.setButtonText (steps[(size_t) step].button);
        if (auto* p = getParentComponent())
        {
            const int w = getWidth() > 0 ? getWidth() : p->getWidth();
            setSize (w, heightFor (w));
            if (onResize) onResize();
        }
        resized();
        repaint();
    }

public:
    std::function<void()> onResize;

private:
    doctor::Issue issue { doctor::clipping, doctor::tip };
    std::vector<doctor::Step> steps;
    int step = 0;
    doctorui::Btn goButton { "Mostrami", true }, backButton { juce::String (juce::CharPointer_UTF8 ("\xe2\x97\x82 Indietro")) }, nextButton { "Avanti" },
                  doItButton { "Fallo per me" }, closeButton { juce::String (juce::CharPointer_UTF8 ("\xe2\x9c\x95")) };
};
