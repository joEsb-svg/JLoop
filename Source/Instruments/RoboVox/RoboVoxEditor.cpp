#include "RoboVoxEditor.h"

namespace
{
    juce::String recordedPrefix()   { return "Voce registrata"; }
}

//==============================================================================
/** LIBRERIA | FILE | MICROFONO e, sotto, il nome della voce (con ◂ ▸ per scorrere le voci della libreria). */
class RoboVoxEditor::SourceView : public juce::Component
{
public:
    explicit SourceView (RoboVoxEditor& e) : editor (e) {}

    void resized() override
    {
        auto r = getLocalBounds();
        const int h = juce::jmin (30, r.getHeight() / 2 - 3);
        buttonsArea = r.removeFromTop (h).withSizeKeepingCentre (juce::jmin (r.getWidth(), 300), h);
        r.removeFromTop (juce::jmax (4, r.getHeight() - h - 2) / 2);
        nameArea = r.removeFromTop (h);
    }

    void paint (juce::Graphics& g) override
    {
        const auto accent = frui::accent();
        const auto name = editor.proc.getSampleName();
        const int active = SampleStore::isLibrary (name) ? 0 : name.startsWith (recordedPrefix()) ? 2 : 1;
        const char* labels[] { "LIBRERIA", "FILE", "MICROFONO" };

        auto bar = buttonsArea.toFloat();
        g.setColour (juce::Colour (0xff0b0d10));
        g.fillRoundedRectangle (bar, 5.0f);
        g.setColour (juce::Colours::black);
        g.drawRoundedRectangle (bar.reduced (0.5f), 5.0f, 1.0f);
        const float w = bar.getWidth() / 3.0f;
        for (int i = 0; i < 3; ++i)
        {
            auto cell = juce::Rectangle<float> (bar.getX() + (float) i * w, bar.getY(), w, bar.getHeight()).reduced (3.0f);
            if (i == active)
            {
                g.setColour (accent);
                g.fillRoundedRectangle (cell, 4.0f);
            }
            g.setColour (i == active ? juce::Colours::black : frui::colours::text);
            g.setFont (frui::sans (juce::jlimit (9.0f, 12.0f, cell.getHeight() * 0.48f)));
            g.drawText (labels[i], cell, juce::Justification::centred, false);
        }

        auto r = nameArea.toFloat().reduced (0.5f);
        g.setColour (frui::lcdBg());
        g.fillRoundedRectangle (r, 4.0f);
        g.setColour (juce::Colours::black);
        g.drawRoundedRectangle (r, 4.0f, 1.0f);
        auto inner = r.reduced (4.0f, 0.0f);
        const float aw = juce::jmin (18.0f, inner.getWidth() * 0.12f);
        auto left = inner.removeFromLeft (aw), right = inner.removeFromRight (aw);
        g.setFont (frui::mono (juce::jlimit (10.0f, 14.0f, r.getHeight() * 0.5f)));
        g.setColour (accent.withAlpha (0.7f));
        auto text = name.isEmpty() ? juce::String ("NESSUNA VOCE") : SampleStore::displayName (name).toUpperCase();
        g.drawText (juce::CharPointer_UTF8 ("\xe2\x97\x82"), left, juce::Justification::centred, false);
        g.drawText (juce::CharPointer_UTF8 ("\xe2\x96\xb8"), right, juce::Justification::centred, false);
        // il nome deve stare nel display: si rimpicciolisce il carattere (e al limite si accorcia)
        float fs = juce::jlimit (10.0f, 14.0f, r.getHeight() * 0.5f);
        auto fits = [&] { return juce::GlyphArrangement::getStringWidth (frui::mono (fs), text) + 0.3f * (float) text.length() < inner.getWidth(); };
        while (fs > 8.0f && ! fits()) fs -= 0.5f;
        while (text.length() > 4 && ! fits()) text = text.dropLastCharacters (2).trimEnd() + juce::String::charToString (0x2026);
        g.setFont (frui::mono (fs));
        frui::drawNeon (g, text, inner, 0.3f, juce::Justification::centred, frui::lcdText());
    }

    void mouseUp (const juce::MouseEvent& e) override
    {
        const auto pos = e.getPosition();
        if (buttonsArea.contains (pos))
        {
            const int i = juce::jlimit (0, 2, (pos.x - buttonsArea.getX()) * 3 / juce::jmax (1, buttonsArea.getWidth()));
            if (i == 0) editor.showLibraryMenu();
            else if (i == 1) editor.importFile();
            else editor.openRecorder();
        }
        else if (nameArea.contains (pos))
        {
            const int x = pos.x - nameArea.getX();
            if (x < nameArea.getWidth() / 5) editor.stepLibrary (-1);
            else if (x > nameArea.getWidth() * 4 / 5) editor.stepLibrary (1);
            else editor.showLibraryMenu();
        }
    }

    void refresh()
    {
        const auto name = editor.proc.getSampleName();
        if (name != lastName) { lastName = name; repaint(); }
    }

private:
    RoboVoxEditor& editor;
    juce::Rectangle<int> buttonsArea, nameArea;
    juce::String lastName;
};

//==============================================================================
/** Una barra per banda: quanto "apre" il synth la voce in quel momento. */
class RoboVoxEditor::BandsView : public juce::Component
{
public:
    explicit BandsView (RoboVoxAudioProcessor& p) : proc (p) { setInterceptsMouseClicks (false, false); }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().reduced (0.5f);
        g.setColour (juce::Colour (0xff07060a));
        g.fillRoundedRectangle (r, 6.0f);
        g.setColour (juce::Colours::black);
        g.drawRoundedRectangle (r, 6.0f, 1.0f);
        auto area = r.reduced (8.0f, 8.0f);
        const int n = juce::jmax (1, proc.getNumBands());
        const float w = area.getWidth() / (float) n;
        const auto accent = frui::accent();
        for (int b = 0; b < n; ++b)
        {
            const float lv = juce::jlimit (0.0f, 1.0f, proc.getBandLevel (b));
            auto bar = juce::Rectangle<float> (area.getX() + (float) b * w, area.getY(), w, area.getHeight()).reduced (juce::jmin (3.0f, w * 0.15f), 0.0f);
            g.setColour (accent.withAlpha (0.08f));
            g.fillRoundedRectangle (bar.withTop (bar.getBottom() - 3.0f), 1.5f);
            const float h = juce::jmax (3.0f, lv * bar.getHeight());
            auto fill = bar.withTop (bar.getBottom() - h);
            g.setGradientFill (juce::ColourGradient (accent.brighter (0.2f), 0.0f, fill.getY(), accent.darker (0.6f), 0.0f, bar.getBottom(), false));
            g.fillRoundedRectangle (fill, 2.0f);
        }
    }

    void refresh()
    {
        float sig = (float) proc.getNumBands();
        for (int b = 0; b < rv::maxBands; ++b) sig += proc.getBandLevel (b) * (float) (b + 3);
        if (std::abs (sig - last) > 0.002f) { last = sig; repaint(); }
    }

private:
    RoboVoxAudioProcessor& proc;
    float last = -1.0f;
};

//==============================================================================
/** Registra la voce dal microfono (fino a 10 secondi) e la usa come voce che parla. */
class RoboVoxEditor::Recorder : public juce::Component,
                                private juce::AudioIODeviceCallback,
                                private juce::Timer
{
public:
    explicit Recorder (RoboVoxEditor& e) : editor (e)
    {
        juce::Component::SafePointer<Recorder> safe (this);
        if (juce::RuntimePermissions::isGranted (juce::RuntimePermissions::recordAudio))
            openDevice();
        else
            juce::RuntimePermissions::request (juce::RuntimePermissions::recordAudio, [safe] (bool granted)
            {
                if (safe == nullptr) return;
                if (granted) safe->openDevice();
                else { safe->message = "Serve il permesso per il microfono"; safe->repaint(); }
            });
        startTimerHz (20);
    }

    ~Recorder() override
    {
        stopTimer();
        closeDevice();
    }

    void paint (juce::Graphics& g) override
    {
        const auto accent = frui::accent();
        g.fillAll (juce::Colour (0xf00a0b0e));
        auto r = getLocalBounds().toFloat().reduced (20.0f, 14.0f);
        g.setFont (frui::sans (juce::jlimit (12.0f, 16.0f, r.getHeight() * 0.07f)));
        frui::drawNeon (g, "REGISTRA LA TUA VOCE", r.removeFromTop (30.0f), 2.0f, juce::Justification::centred, accent);

        auto bottom = r.removeFromBottom (juce::jlimit (34.0f, 44.0f, r.getHeight() * 0.18f));
        useArea = bottom.removeFromRight (bottom.getWidth() * 0.5f).reduced (8.0f, 2.0f).toNearestInt();
        cancelArea = bottom.reduced (8.0f, 2.0f).toNearestInt();
        const bool canUse = ! recording && length > 0;
        drawButton (g, cancelArea.toFloat(), "ANNULLA", false, true);
        drawButton (g, useArea.toFloat(), "USA QUESTA VOCE", canUse, canUse);

        r.removeFromBottom (8.0f);
        auto info = r.removeFromBottom (24.0f);
        auto meter = r.removeFromBottom (10.0f).withSizeKeepingCentre (juce::jmin (r.getWidth(), 360.0f), 8.0f);
        r.removeFromBottom (8.0f);
        const float d = juce::jmin (r.getWidth(), r.getHeight()) * 0.7f;
        auto circle = r.withSizeKeepingCentre (d, d);
        recArea = circle.toNearestInt();
        g.setColour (juce::Colour (0xff16181c));
        g.fillEllipse (circle);
        g.setColour (recording ? juce::Colour (0xffff3b3b) : accent);
        g.drawEllipse (circle.reduced (2.0f), 3.0f);
        auto inner = circle.reduced (d * 0.3f);
        g.setColour (juce::Colour (0xffff3b3b));
        if (recording) g.fillRoundedRectangle (inner, 4.0f);
        else g.fillEllipse (inner);

        g.setColour (juce::Colour (0xff1c1f24));
        g.fillRoundedRectangle (meter, 3.0f);
        g.setColour (accent);
        g.fillRoundedRectangle (meter.withWidth (meter.getWidth() * juce::jlimit (0.0f, 1.0f, level.load() * 1.5f)), 3.0f);

        const double rate = deviceRate > 0.0 ? deviceRate : 48000.0;
        const auto secs = (double) (recording ? writePos.load() : length) / rate;
        g.setFont (frui::mono (13.0f));
        g.setColour (frui::colours::text);
        g.drawText (message.isNotEmpty() ? message
                                         : (recording ? "Registrazione... " : (length > 0 ? "Registrata: " : "Tocca il cerchio rosso e parla  ")) + juce::String (secs, 1) + " / 10 s",
                    info, juce::Justification::centred, false);
    }

    void mouseUp (const juce::MouseEvent& e) override
    {
        const auto pos = e.getPosition();
        if (cancelArea.contains (pos)) { editor.closeRecorder(); return; }
        if (useArea.contains (pos) && ! recording && length > 0) { save(); return; }
        if (recArea.contains (pos) && device != nullptr)
        {
            if (recording) { recording = false; length = writePos.load(); }
            else { writePos.store (0); length = 0; recording = true; }
            repaint();
        }
    }

private:
    void drawButton (juce::Graphics& g, juce::Rectangle<float> r, const juce::String& text, bool lit, bool enabled)
    {
        g.setColour (lit ? frui::accent() : juce::Colour (0xff16181c));
        g.fillRoundedRectangle (r, 5.0f);
        g.setColour (juce::Colours::black);
        g.drawRoundedRectangle (r, 5.0f, 1.0f);
        g.setColour (lit ? juce::Colours::black : frui::colours::text.withAlpha (enabled ? 1.0f : 0.35f));
        g.setFont (frui::sans (juce::jlimit (10.0f, 14.0f, r.getHeight() * 0.38f)));
        g.drawText (text, r, juce::Justification::centred, false);
    }

    void openDevice()
    {
        const auto err = manager.initialiseWithDefaultDevices (1, 0);
        device = manager.getCurrentAudioDevice();
        if (err.isNotEmpty() || device == nullptr || device->getActiveInputChannels().countNumberOfSetBits() == 0)
        {
            device = nullptr;
            message = "Microfono non disponibile";
            repaint();
            return;
        }
        deviceRate = device->getCurrentSampleRate();
        buffer.setSize (1, (int) (deviceRate * 10.0));
        manager.addAudioCallback (this);
        repaint();
    }

    void closeDevice()
    {
        manager.removeAudioCallback (this);
        manager.closeAudioDevice();
        device = nullptr;
    }

    void audioDeviceIOCallbackWithContext (const float* const* in, int numIn, float* const* out, int numOut, int n,
                                           const juce::AudioIODeviceCallbackContext&) override
    {
        for (int c = 0; c < numOut; ++c) if (out[c] != nullptr) juce::FloatVectorOperations::clear (out[c], n);
        if (numIn <= 0 || in[0] == nullptr) return;
        float pk = 0.0f;
        for (int i = 0; i < n; ++i) pk = juce::jmax (pk, std::abs (in[0][i]));
        level.store (juce::jmax (pk, level.load() * 0.9f));
        if (! recording) return;
        const int pos = writePos.load();
        const int m = juce::jmin (n, buffer.getNumSamples() - pos);
        if (m > 0)
        {
            buffer.copyFrom (0, pos, in[0], m);
            writePos.store (pos + m);
        }
    }
    void audioDeviceAboutToStart (juce::AudioIODevice*) override {}
    void audioDeviceStopped() override {}

    void timerCallback() override
    {
        if (recording && writePos.load() >= buffer.getNumSamples()) { recording = false; length = writePos.load(); }
        level.store (level.load() * 0.85f);
        repaint();
    }

    void save()
    {
        closeDevice();
        auto folder = SampleStore::getFolder();
        folder.createDirectory();
        juce::File file;
        for (int i = 1; ; ++i)
        {
            file = folder.getChildFile (recordedPrefix() + " " + juce::String (i) + ".wav");
            if (! file.exists()) break;
        }
        bool ok = false;
        {
            juce::WavAudioFormat wav;
            std::unique_ptr<juce::OutputStream> os = std::make_unique<juce::FileOutputStream> (file);
            if (auto w = wav.createWriterFor (os, juce::AudioFormatWriterOptions{}.withSampleRate (deviceRate).withNumChannels (1).withBitsPerSample (16)))
                ok = w->writeFromAudioSampleBuffer (buffer, 0, length);
        }
        if (ok && editor.proc.setSample (file.getFileName()))
            editor.closeRecorder();
        else
        {
            message = "Non riesco a salvare la registrazione";
            repaint();
        }
    }

    RoboVoxEditor& editor;
    juce::AudioDeviceManager manager;
    juce::AudioIODevice* device = nullptr;
    juce::AudioBuffer<float> buffer;
    std::atomic<int> writePos { 0 };
    std::atomic<float> level { 0.0f };
    std::atomic<bool> recording { false };
    int length = 0;
    double deviceRate = 0.0;
    juce::String message;
    juce::Rectangle<int> recArea, useArea, cancelArea;
};

//==============================================================================
RoboVoxEditor::RoboVoxEditor (RoboVoxAudioProcessor& p)
    : NeonEditor (p, p.apvts, "RoboVox", juce::Colour (0xffc46bff), { "VOCE", "PORTANTE", "FX" },
                  { "formant", "FORMANTE", juce::String::fromUTF8 ("VOCE \xe2\x86\x95 ROBOT"), {} },
                  { "mix", "MIX", {}, {} }),
      proc (p)
{
    auto& voice = section (0, "VOCE CHE PARLA", 0.8f, 2);
    source = static_cast<SourceView*> (&view (voice, std::make_unique<SourceView> (*this), 0.36f));
    knob (voice, "sibil", "SIBILANTI");
    knob (voice, "gate", "GATE");
    knob (voice, "attack", "ATTACCO");
    knob (voice, "release", "RILASCIO");
    auto& bandSec = section (0, "BANDE: LA VOCE SCOLPISCE GLI ACCORDI", 1.3f);
    bands = static_cast<BandsView*> (&view (bandSec, std::make_unique<BandsView> (p), 0.72f));
    segment (bandSec, "bands", { "8", "16", "24" });
    lcd (bandSec, "carrier", "PORTANTE: ");

    auto& carrier = section (1, "SYNTH PORTANTE (TIENI GLI ACCORDI)", 1.0f, 4);
    lcd (carrier, "carrier", "PORTANTE: ");
    lcd (carrier, "trig", "VOCE: ");
    knob (carrier, "octave", "OTTAVA");
    knob (carrier, "detune", "DETUNE");
    knob (carrier, "noise", "RUMORE");
    knob (carrier, "noteRel", "RILASCIO NOTE");

    auto& cd = section (2, "CHORUS E DELAY", 1.0f, 3);
    lcd (cd, "dlyTime", "DELAY ");
    knob (cd, "chorus", "CHORUS");
    knob (cd, "dlyMix", "DELAY");
    knob (cd, "dlyFb", "FEEDBACK");
    auto& rvb = section (2, "RIVERBERO E USCITA", 1.0f, 3);
    knob (rvb, "revMix", "RIVERBERO");
    knob (rvb, "revSize", "STANZA");
    knob (rvb, "master", "VOLUME");

    showTab (0);
}

RoboVoxEditor::~RoboVoxEditor()
{
    recorder.reset();
}

void RoboVoxEditor::resized()
{
    NeonEditor::resized();
    if (recorder != nullptr) recorder->setBounds (getLocalBounds());
}

void RoboVoxEditor::refreshViews()
{
    if (source != nullptr) source->refresh();
    if (bands != nullptr) bands->refresh();
}

void RoboVoxEditor::showLibraryMenu()
{
    juce::PopupMenu menu, mine;
    std::vector<juce::String> names;
    const auto current = proc.getSampleName();
    for (auto& id : rv::libraryVoices())
        if (SampleStore::findLibraryEntry (id) != nullptr)
        {
            names.push_back (SampleStore::libraryName (id));
            menu.addItem ((int) names.size(), SampleStore::displayName (names.back()), true, names.back() == current);
        }
    for (auto& f : proc.getSampleStore().listSamples())
    {
        names.push_back (f);
        mine.addItem ((int) names.size(), juce::File (f).getFileNameWithoutExtension(), true, f == current);
    }
    menu.addSeparator();
    menu.addSubMenu ("Voci registrate e importate", mine, mine.getNumItems() > 0);
    juce::Component::SafePointer<RoboVoxEditor> safe (this);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (source), [safe, names] (int r)
    {
        if (safe == nullptr || r <= 0 || r > (int) names.size()) return;
        safe->proc.setSample (names[(size_t) (r - 1)]);
        safe->refreshViews();
    });
}

void RoboVoxEditor::stepLibrary (int delta)
{
    juce::StringArray list;
    for (auto& id : rv::libraryVoices())
        if (SampleStore::findLibraryEntry (id) != nullptr) list.add (SampleStore::libraryName (id));
    if (list.isEmpty()) return;
    const int i = list.indexOf (proc.getSampleName());
    const int next = i < 0 ? 0 : (i + delta + list.size()) % list.size();
    proc.setSample (list[next]);
    refreshViews();
}

void RoboVoxEditor::importFile()
{
    chooser = std::make_unique<juce::FileChooser> ("Scegli una voce", juce::File(), "*.wav;*.aif;*.aiff;*.flac;*.ogg;*.mp3");
    juce::Component::SafePointer<RoboVoxEditor> safe (this);
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                          [safe] (const juce::FileChooser& fc)
    {
        if (safe == nullptr || fc.getURLResults().isEmpty())
            return;
        juce::String error;
        const auto name = safe->proc.getSampleStore().importFromUrl (fc.getURLResults()[0], error);
        if (name.isEmpty())
        {
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "RoboVox", error);
            return;
        }
        safe->proc.setSample (name);
        safe->refreshViews();
    });
}

void RoboVoxEditor::openRecorder()
{
    recorder = std::make_unique<Recorder> (*this);
    addAndMakeVisible (*recorder);
    recorder->setBounds (getLocalBounds());
}

void RoboVoxEditor::closeRecorder()
{
    juce::Component::SafePointer<RoboVoxEditor> safe (this);
    juce::MessageManager::callAsync ([safe] { if (safe != nullptr) { safe->recorder.reset(); safe->refreshViews(); } });
}
