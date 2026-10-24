#include "SlicerEditor.h"

namespace col = bf::ui::col;
static const juce::Colour slAccent { 0xffe8c35a };

//==============================================================================
/** Forma d'onda con i tagli: ogni pezzo ha il suo tasto (C3, C#3...). Tocca un pezzo per sentirlo. */
class SlicerEditor::WaveView : public juce::Component
{
public:
    explicit WaveView (SlicerAudioProcessor& p) : proc (p) {}

    juce::uint64 signature() const
    {
        juce::uint64 h = (juce::uint64) (juce::pointer_sized_int) proc.getSample() + (juce::uint64) proc.getNumSlices() * 7919u;
        for (int i = 0; i <= proc.getNumSlices(); ++i)
            h = h * 1000003u + (juce::uint64) proc.getSliceStart (i);
        return h * 31u + (juce::uint64) (proc.getLastSlice() + 1);
    }
    juce::uint64 shown = 0;

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().reduced (2.0f);
        g.setColour (col::bg);
        g.fillRoundedRectangle (r, 6.0f);
        g.setColour (col::edge);
        g.drawRoundedRectangle (r, 6.0f, 1.0f);

        const auto* s = proc.getSample();
        if (s == nullptr || s->audio.getNumSamples() == 0)
        {
            g.setColour (col::textDim);
            g.setFont (juce::Font (juce::FontOptions (14.0f)));
            g.drawFittedText ("Nessun suono: tocca \"Scegli suono\" (un loop o una voce)", r.toNearestInt(), juce::Justification::centred, 2);
            return;
        }

        auto area = r.reduced (6.0f, 6.0f);
        auto labels = area.removeFromBottom (16.0f);
        const int len = s->audio.getNumSamples();
        const float* x = s->audio.getReadPointer (0);
        auto xFor = [&] (int sample) { return area.getX() + area.getWidth() * (float) sample / (float) len; };
        const int count = proc.getNumSlices(), last = proc.getLastSlice();

        // Pezzi: sfondo alternato, quello appena suonato acceso
        for (int i = 0; i < count; ++i)
        {
            const float x0 = xFor (proc.getSliceStart (i)), x1 = xFor (proc.getSliceStart (i + 1));
            g.setColour (i == last ? slAccent.withAlpha (0.22f) : (i % 2 == 0 ? col::panel.withAlpha (0.5f) : col::panelHi.withAlpha (0.35f)));
            g.fillRect (juce::Rectangle<float> (x0, area.getY(), x1 - x0, area.getHeight()));
        }

        // Forma d'onda (min/max per colonna)
        const int w = juce::jmax (1, (int) area.getWidth());
        const float mid = area.getCentreY(), half = area.getHeight() * 0.48f;
        g.setColour (slAccent.withAlpha (0.85f));
        for (int px = 0; px < w; ++px)
        {
            const int a = (int) ((juce::int64) len * px / w), b = juce::jmax (a + 1, (int) ((juce::int64) len * (px + 1) / w));
            float lo = 0.0f, hi = 0.0f;
            for (int i = a; i < b && i < len; ++i) { lo = juce::jmin (lo, x[i]); hi = juce::jmax (hi, x[i]); }
            g.fillRect (area.getX() + (float) px, mid - hi * half, 1.0f, juce::jmax (1.0f, (hi - lo) * half));
        }

        // Tagli e tasti
        g.setFont (juce::Font (juce::FontOptions (10.0f, juce::Font::bold)));
        for (int i = 0; i < count; ++i)
        {
            const float x0 = xFor (proc.getSliceStart (i)), x1 = xFor (proc.getSliceStart (i + 1));
            g.setColour (juce::Colours::white.withAlpha (0.8f));
            g.fillRect (x0, area.getY(), 1.5f, area.getHeight());
            g.setColour (i == last ? slAccent : col::textDim);
            if (x1 - x0 > 16.0f)
                g.drawText (slicer::noteName (slicer::firstNote + i), juce::Rectangle<float> (x0, labels.getY(), x1 - x0, labels.getHeight()), juce::Justification::centred, false);
            if (x1 - x0 > 12.0f)
                g.drawText (juce::String (i + 1), juce::Rectangle<float> (x0 + 3.0f, area.getY() + 2.0f, 24.0f, 12.0f), juce::Justification::centredLeft, false);
        }
        g.setColour (col::textDim);
        g.drawText (juce::String (count) + " pezzi", r.reduced (8.0f, 4.0f), juce::Justification::topRight, false);
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        const auto* s = proc.getSample();
        if (s == nullptr) return;
        auto area = getLocalBounds().toFloat().reduced (8.0f, 8.0f);
        const int pos = (int) ((e.position.x - area.getX()) / area.getWidth() * (float) s->audio.getNumSamples());
        for (int i = 0; i < proc.getNumSlices(); ++i)
            if (pos >= proc.getSliceStart (i) && pos < proc.getSliceStart (i + 1))
            {
                proc.auditionSlice (i);
                break;
            }
    }

private:
    SlicerAudioProcessor& proc;
};

//==============================================================================
SlicerEditor::SlicerEditor (SlicerAudioProcessor& p)
    : ForgeEditorBase (p, p.apvts, "SLICE", "R", slAccent, { "PEZZI" }), proc (p)
{
    auto& w = addSection (0, "Campione: tocca un pezzo per sentirlo", 2.4f);
    auto button = std::make_unique<juce::TextButton> ("Scegli suono...");
    sampleButton = button.get();
    sampleButton->onClick = [this] { showSampleMenu(); };
    item (w, std::move (button));
    auto imp = std::make_unique<juce::TextButton> ("Importa un file...");
    imp->onClick = [this] { importSample(); };
    item (w, std::move (imp));
    auto view = std::make_unique<WaveView> (p);
    wave = view.get();
    setView (w, std::move (view), 0.78f);

    auto& t = addSection (0, "Taglio e suono", 1.3f);
    combo (t, "mode", "Taglio");
    knob (t, "sens", "Sensibilita'");
    combo (t, "voice", "Voce");
    knob (t, "attack", "Attacco");
    knob (t, "release", "Rilascio");
    knob (t, "pitch", "Intonazione");
    knob (t, "gain", "Volume");
    showTab (0);
    refreshViews();
}

SlicerEditor::~SlicerEditor() = default;

void SlicerEditor::refreshViews()
{
    if (wave != nullptr)
        if (const auto sig = wave->signature(); sig != wave->shown)
        {
            wave->shown = sig;
            wave->repaint();
        }
    if (sampleButton != nullptr)
    {
        const auto name = proc.getSampleName();
        sampleButton->setButtonText (name.isEmpty() ? juce::String ("Scegli suono...") : SampleStore::displayName (name) + juce::String (juce::CharPointer_UTF8 ("  \xe2\x96\xbe")));
    }
}

void SlicerEditor::showSampleMenu()
{
    // Libreria per categoria (prima i loop e le voci), poi i campioni importati
    juce::PopupMenu menu;
    std::vector<juce::String> names;
    juce::StringArray cats;
    for (auto& en : LibraryIndex::entries)
        if (! cats.contains (juce::CharPointer_UTF8 (en.category)))
            cats.add (juce::CharPointer_UTF8 (en.category));
    cats.removeString ("Loop"); cats.removeString ("Voci"); cats.removeString ("FX");
    cats.insert (0, "FX"); cats.insert (0, "Voci"); cats.insert (0, "Loop");
    const auto current = proc.getSampleName();
    for (auto& cat : cats)
    {
        juce::PopupMenu sub;
        for (auto& en : LibraryIndex::entries)
            if (juce::String (juce::CharPointer_UTF8 (en.category)) == cat)
            {
                names.push_back (SampleStore::libraryName (en.id));
                sub.addItem ((int) names.size(), juce::String (juce::CharPointer_UTF8 (en.name)), true, names.back() == current);
            }
        menu.addSubMenu (cat, sub);
    }
    juce::PopupMenu mine;
    for (auto& f : proc.getSampleStore().listSamples())
    {
        names.push_back (f);
        mine.addItem ((int) names.size(), juce::File (f).getFileNameWithoutExtension(), true, f == current);
    }
    menu.addSeparator();
    menu.addSubMenu ("Campioni importati", mine, mine.getNumItems() > 0);
    menu.addItem (100000, "Importa un file...");

    juce::Component::SafePointer<SlicerEditor> safe (this);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (sampleButton), [safe, names] (int r)
    {
        if (safe == nullptr || r <= 0) return;
        if (r == 100000) { safe->importSample(); return; }
        if (r - 1 < (int) names.size() && safe->proc.setSample (names[(size_t) (r - 1)]))
        {
            safe->proc.auditionSlice (0);
            safe->refreshViews();
        }
    });
}

void SlicerEditor::importSample()
{
    chooser = std::make_unique<juce::FileChooser> ("Scegli un loop o una voce", juce::File(), "*.wav;*.aif;*.aiff;*.flac;*.ogg;*.mp3");
    juce::Component::SafePointer<SlicerEditor> safe (this);
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                          [safe] (const juce::FileChooser& fc)
    {
        if (safe == nullptr || fc.getURLResults().isEmpty())
            return;
        juce::String error;
        const auto name = safe->proc.getSampleStore().importFromUrl (fc.getURLResults()[0], error);
        if (name.isEmpty())
        {
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Slicer", error);
            return;
        }
        safe->proc.setSample (name);
        safe->proc.auditionSlice (0);
        safe->refreshViews();
    });
}
