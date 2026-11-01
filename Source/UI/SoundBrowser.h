#pragma once

#include <JuceHeader.h>
#include <functional>
#include <vector>
#include <set>
#include "../Engine/SequencerEngine.h"
#include "../Engine/SampleStore.h"
#include "../Instruments/Instruments.h"
#include "Theme.h"
#include "Widgets.h"

//==============================================================================
/** Il suono scelto nel browser. */
struct SoundChoice
{
    enum Kind { none, library, userSample, importFile, drumSynth, instrument, drumForgeKit, kitChannels };

    Kind kind = none;
    juce::String id;            // libreria: id; campione: nome del file
    juce::String name;          // nome da dare al canale
    juce::uint32 colour = 0xff8aa0b8;
    int drumType = 0, kit = 0;
};

//==============================================================================
/**
    Una lista di "piastrelle" da toccare, dentro un Viewport che scorre col dito.
    Il tocco conta solo se il dito non si e' mosso (altrimenti era uno scorrimento).
*/
class TileList : public juce::Component
{
public:
    struct Item
    {
        juce::String text, detail;
        juce::Colour colour;
        bool enabled = true;
    };

    std::function<void (int index)> onTap;

    void setItems (std::vector<Item> newItems, int newSelected)
    {
        items = std::move (newItems);
        selected = newSelected;
        updateSize();
        repaint();
    }

    void setSelected (int index)        { selected = index; repaint(); }
    int getSelected() const             { return selected; }
    const std::vector<Item>& getItems() const   { return items; }

    void setLayout (int numColumns, int rowHeight)
    {
        columns = juce::jmax (1, numColumns);
        itemHeight = rowHeight;
        updateSize();
    }

    /** Larghezza visibile (quella del Viewport): l'altezza cresce con il numero di voci. */
    void setVisibleWidth (int w)        { width = w; updateSize(); }

    juce::Rectangle<int> getItemBounds (int i) const
    {
        const int gap = 5;
        const int w = (width - gap * (columns - 1)) / columns;
        return { (i % columns) * (w + gap), (i / columns) * (itemHeight + gap), w, itemHeight };
    }

    void paint (juce::Graphics& g) override
    {
        for (int i = 0; i < (int) items.size(); ++i)
        {
            auto r = getItemBounds (i).toFloat();
            if (! g.clipRegionIntersects (r.toNearestInt()))
                continue;

            const auto& it = items[(size_t) i];
            const bool sel = i == selected;
            Theme::drawRaised (g, r, sel ? Theme::raisedHi : Theme::raised);

            if (sel)
            {
                g.setColour (Theme::accent);
                g.drawRoundedRectangle (r.reduced (1.0f), Theme::corner, 1.6f);
            }

            g.setColour (it.colour.withMultipliedAlpha (it.enabled ? 0.95f : 0.3f));
            g.fillRect (r.withWidth (3.0f).reduced (0.0f, 5.0f).translated (4.0f, 0.0f));

            auto text = r.withTrimmedLeft (13.0f).reduced (2.0f, 0.0f);
            const bool twoLines = it.detail.isNotEmpty() && r.getHeight() >= 38.0f;
            g.setColour (it.enabled ? (sel ? Theme::textBright : Theme::text) : Theme::textDim.withAlpha (0.6f));
            g.setFont (Theme::font (juce::jmin (14.0f, r.getHeight() * 0.36f), true));

            if (twoLines)
            {
                auto top = text.removeFromTop (text.getHeight() * 0.56f);
                g.drawFittedText (it.text, top.toNearestInt(), juce::Justification::bottomLeft, 1);
                g.setColour (Theme::textDim);
                g.setFont (Theme::font (11.0f));
                g.drawFittedText (it.detail, text.toNearestInt(), juce::Justification::topLeft, 1);
            }
            else
            {
                g.drawFittedText (it.text, text.toNearestInt(), juce::Justification::centredLeft, 1);
            }
        }
    }

    void mouseUp (const juce::MouseEvent& e) override
    {
        if (e.getDistanceFromDragStart() > 10 || e.getLengthOfMousePress() > 800)
            return;

        for (int i = 0; i < (int) items.size(); ++i)
            if (getItemBounds (i).contains (e.getMouseDownPosition()))
            {
                if (items[(size_t) i].enabled && onTap)
                    onTap (i);
                return;
            }
    }

private:
    void updateSize()
    {
        const int rows = ((int) items.size() + columns - 1) / columns;
        setSize (width, juce::jmax (1, rows * (itemHeight + 5)));
    }

    std::vector<Item> items;
    int selected = -1, columns = 1, itemHeight = 44, width = 100;
};

//==============================================================================
/**
    PC, banco di lavoro: la libreria come albero in una colonna sola (stile FL Studio).
    Cartelle principali -> sottocartelle -> suoni; si aprono in giu', senza allargare il pannello.
*/
class TreeList : public juce::Component
{
public:
    struct Row
    {
        int level = 0;            // 0 = cartella principale, 1 = sottocartella, 2 = suono
        bool folder = true, open = false, current = false;
        int tab = 0, category = -1, sound = -1;
        juce::String text, detail;
        juce::Colour colour;
    };

    std::function<void (int row)> onClick, onDoubleClick;

    void setRows (std::vector<Row> newRows)     { rows = std::move (newRows); updateSize(); repaint(); }
    const std::vector<Row>& getRows() const     { return rows; }
    void setSelected (int r)                    { selected = r; repaint(); }
    int getSelected() const                     { return selected; }
    void setVisibleWidth (int w)                { width = w; updateSize(); }
    juce::Rectangle<int> getRowBounds (int i) const   { return { 0, i * rowH, width, rowH }; }

    void paint (juce::Graphics& g) override
    {
        for (int i = 0; i < (int) rows.size(); ++i)
        {
            auto r = getRowBounds (i).toFloat();
            if (! g.clipRegionIntersects (r.toNearestInt())) continue;
            const auto& row = rows[(size_t) i];
            auto box = r.reduced (1.0f, 1.5f).withTrimmedLeft ((float) row.level * 14.0f);

            if (i == selected || row.current)
            {
                g.setColour (i == selected ? Theme::raisedHi : Theme::raised);
                g.fillRoundedRectangle (box, 4.0f);
                if (i == selected)
                {
                    g.setColour (Theme::accent);
                    g.drawRoundedRectangle (box.reduced (0.5f), 4.0f, 1.4f);
                }
            }
            else if (i == hover)
            {
                g.setColour (Theme::raised.withAlpha (0.6f));
                g.fillRoundedRectangle (box, 4.0f);
            }

            auto content = box.reduced (6.0f, 0.0f);
            if (row.folder)
            {
                // freccina e cartella
                auto arrow = content.removeFromLeft (12.0f);
                juce::Path p;
                const auto c = arrow.getCentre();
                if (row.open) p.addTriangle (c.x - 4.0f, c.y - 2.5f, c.x + 4.0f, c.y - 2.5f, c.x, c.y + 3.0f);
                else          p.addTriangle (c.x - 2.5f, c.y - 4.0f, c.x - 2.5f, c.y + 4.0f, c.x + 3.0f, c.y);
                g.setColour (row.open ? Theme::accent : Theme::textDim);
                g.fillPath (p);
                content.removeFromLeft (4.0f);
                auto icon = content.removeFromLeft (16.0f).withSizeKeepingCentre (15.0f, 11.0f);
                g.setColour (row.colour.withAlpha (row.open ? 0.95f : 0.7f));
                g.fillRoundedRectangle (icon.withTrimmedTop (2.0f), 2.0f);
                g.fillRoundedRectangle (icon.withHeight (4.0f).withWidth (7.0f), 1.5f);
                content.removeFromLeft (6.0f);
                g.setColour (row.level == 0 ? Theme::textBright : Theme::text);
                g.setFont (Theme::font (row.level == 0 ? 14.0f : 13.0f, true));
            }
            else
            {
                g.setColour (row.colour);
                g.fillRect (content.removeFromLeft (3.0f).reduced (0.0f, 6.0f));
                content.removeFromLeft (7.0f);
                g.setColour (i == selected ? Theme::textBright : Theme::text);
                g.setFont (Theme::font (13.0f));
            }
            if (row.detail.isNotEmpty() && ! row.folder)
            {
                const float tw = juce::GlyphArrangement::getStringWidth (g.getCurrentFont(), row.text);
                g.drawText (row.text, content, juce::Justification::centredLeft, true);
                g.setColour (Theme::textDim);
                g.setFont (Theme::font (10.5f));
                g.drawText (row.detail, content.withTrimmedLeft (tw + 8.0f), juce::Justification::centredLeft, true);
            }
            else
                g.drawText (row.text, content, juce::Justification::centredLeft, true);
        }
    }

    void mouseMove (const juce::MouseEvent& e) override     { setHover (e.y / rowH); }
    void mouseExit (const juce::MouseEvent&) override       { setHover (-1); }

    void mouseUp (const juce::MouseEvent& e) override
    {
        if (e.getDistanceFromDragStart() > 10 || e.getLengthOfMousePress() > 800) return;
        const int i = e.getMouseDownY() / rowH;
        if (i >= 0 && i < (int) rows.size() && onClick) onClick (i);
    }

    void mouseDoubleClick (const juce::MouseEvent& e) override
    {
        const int i = e.y / rowH;
        if (i >= 0 && i < (int) rows.size() && ! rows[(size_t) i].folder && onDoubleClick) onDoubleClick (i);
    }

    static constexpr int rowH = 28;

private:
    void setHover (int h)    { if (h != hover) { hover = h; repaint(); } }
    void updateSize()        { setSize (width, juce::jmax (1, (int) rows.size() * rowH)); }

    std::vector<Row> rows;
    int selected = -1, hover = -1, width = 100;
};

//==============================================================================
/**
    Il browser dei suoni: si apre sopra il channel rack per aggiungere uno strumento
    o cambiare il suono di un canale.

    - Strumenti:   pianoforti, chitarre, bassi, archi, fiati, voci, percussioni, batteria acustica...
    - Elettronica: EDM, House, Techno, Electro, Drum and Bass, Dubstep, Trap, synth, effetti
    - Sintesi:     i synth (BassForge, PolyForge) e la batteria sintetizzata di JLoop (kit per genere)
    - Campioni:    i file importati dall'utente

    Tocca un suono per ascoltarlo; "Aggiungi" (o "Usa questo suono") per confermare.
*/
class SoundBrowser : public juce::Component
{
public:
    SoundBrowser (SequencerEngine& e, SampleStore& store) : engine (e), samples (store)
    {
        for (auto* b : { &closeButton, &confirmButton })
            addAndMakeVisible (b);

        closeButton.onClick = [this] { if (onClose) onClose(); };
        confirmButton.onClick = [this] { confirm(); };

        tabs.onChange = [this] (int) { showTab (tabs.getSelected(), 0); };
        addAndMakeVisible (tabs);

        for (auto* vp : { &categoryView, &tileView })
        {
            vp->setScrollOnDragMode (juce::Viewport::ScrollOnDragMode::all);
            vp->setScrollBarsShown (true, false);
            vp->setScrollBarThickness (6);
            addAndMakeVisible (vp);
        }

        treeView.setScrollOnDragMode (juce::Viewport::ScrollOnDragMode::all);
        treeView.setScrollBarsShown (true, false);
        treeView.setScrollBarThickness (8);
        treeView.setViewedComponent (&tree, false);
        addChildComponent (treeView);
        tree.onClick = [this] (int r) { treeClick (r); };
        tree.onDoubleClick = [this] (int r) { treeClick (r); confirm(); };

        categoryView.setViewedComponent (&categoryList, false);
        tileView.setViewedComponent (&tileList, false);
        categoryList.onTap = [this] (int i) { showCategory (i); };
        tileList.onTap     = [this] (int i) { select (i); };

        buildLibraryCategories();
    }

    std::function<void (int channel, const SoundChoice&)> onChoose;
    std::function<void()> onClose;
    std::function<void()> onTap;          // per la vibrazione

    /** Apre il browser: channel = canale di cui cambiare suono, -1 = aggiungere un canale nuovo. */
    void openFor (int channel)
    {
        targetChannel = channel;
        chosen = {};
        confirmButton.setButtonText (channel < 0 ? "Aggiungi" : "Usa questo suono");

        // Parte dalla categoria del suono attuale del canale, se si sa qual e'
        int tab = lastTab, category = lastCategory[(size_t) lastTab];
        if (channel >= 0)
        {
            auto& ch = engine.getChannel (channel);
            if (engine.isSynth (channel))
            {
                tab = 2; category = 0;
            }
            else if (auto* entry = SampleStore::findLibraryEntry (ch.sampleName); entry != nullptr && SampleStore::isLibrary (ch.sampleName))
            {
                tab = juce::String (entry->group) == "Batteria" ? 0 : 1;
                category = juce::jmax (0, groups[(size_t) tab].indexOf (juce::String (juce::CharPointer_UTF8 (entry->category))));
            }
            else if (ch.sample.load() != nullptr)
            {
                tab = 3; category = 0;
            }
            else
            {
                tab = 2; category = engine.getChannelKit (channel) + firstDrumSynthCategory;
            }
        }

        tabs.setSelected (tab);
        showTab (tab, category);
        if (embedded)
            rebuildTree (false);   // all'inizio solo le cartelle principali: si aprono quando le tocchi
        repaint();
    }

    int getTargetChannel() const    { return targetChannel; }

    /** PC, banco di lavoro: la libreria resta aperta a sinistra; il suono va sul canale scelto nel rack. */
    void setEmbedded (bool e)
    {
        embedded = e;
        closeButton.setVisible (! e);
        tabs.setVisible (! e);
        categoryView.setVisible (! e);
        tileView.setVisible (! e);
        treeView.setVisible (e);
        if (e) rebuildTree (false);
        resized(); repaint();
    }
    bool isEmbedded() const         { return embedded; }
    /** Cambia il canale a cui va il suono, senza cambiare la categoria in vista. */
    void setTarget (int channel)
    {
        if (channel == targetChannel) return;
        targetChannel = channel;
        confirmButton.setButtonText (channel < 0 ? "Aggiungi" : "Usa sul canale");
        if (embedded) rebuildTree (false);
        repaint();
    }
    bool embedded = false;

    /** Solo per i test: apre una scheda e una categoria. */
    void showTabForTest (int tab, int category)    { tabs.setSelected (tab); showTab (tab, category); }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        Theme::drawGlassPanel (g, r);

        auto title = titleArea;
        g.setColour (Theme::accent);
        g.fillRect (title.removeFromLeft (4.0f).reduced (0.0f, 6.0f));
        title.removeFromLeft (8.0f);
        g.setColour (Theme::textBright);
        g.setFont (Theme::font (17.0f, true));
        const auto heading = embedded ? (targetChannel < 0 ? juce::String ("Suoni") : "Suoni  " + juce::String (juce::CharPointer_UTF8 ("\xe2\x86\x92  ")) + engine.getChannel (targetChannel).name)
                           : targetChannel < 0 ? juce::String ("Aggiungi strumento")
                                               : "Suono di \"" + engine.getChannel (targetChannel).name + "\"";
        g.drawFittedText (heading, title.toNearestInt(), juce::Justification::centredLeft, 1);

        // In basso: il suono selezionato
        g.setColour (chosen.kind == SoundChoice::none ? Theme::textDim : Theme::text);
        g.setFont (Theme::font (13.0f, chosen.kind != SoundChoice::none));
        g.drawFittedText (chosen.kind == SoundChoice::none ? juce::String ("Tocca un suono per ascoltarlo")
                                                           : juce::String (juce::CharPointer_UTF8 ("\xe2\x99\xaa  ")) + chosen.name,
                          footerText.toNearestInt(), juce::Justification::centredLeft, 1);
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (10, 6);
        auto top = r.removeFromTop (34);
        if (! embedded) closeButton.setBounds (top.removeFromRight (80).reduced (0, 2));
        titleArea = top.toFloat();

        if (embedded)
        {
            auto footer = r.removeFromBottom (68);
            footerText = footer.removeFromTop (26).toFloat();
            confirmButton.setBounds (footer.reduced (0, 3));
            r.removeFromBottom (4);
            treeView.setBounds (r);
            tree.setVisibleWidth (r.getWidth() - 10);
            return;
        }

        r.removeFromTop (4);
        tabs.setBounds (r.removeFromTop (30));
        r.removeFromTop (6);

        auto footer = r.removeFromBottom (40);
        confirmButton.setBounds (footer.removeFromRight (juce::jmin (170, footer.getWidth() / 2)).reduced (0, 3));
        footerText = footer.toFloat().withTrimmedRight (8.0f);
        r.removeFromBottom (4);

        const bool narrow = r.getWidth() < 520;
        auto left = r.removeFromLeft (juce::jlimit (110, 190, (int) (r.getWidth() * (narrow ? 0.36f : 0.26f))));
        r.removeFromLeft (8);

        categoryView.setBounds (left);
        categoryList.setLayout (1, narrow ? 40 : 36);
        categoryList.setVisibleWidth (left.getWidth() - 8);

        tileView.setBounds (r);
        const int columns = juce::jmax (1, (r.getWidth() - 8) / (narrow ? 200 : 170));
        tileList.setLayout (columns, 46);
        tileList.setVisibleWidth (r.getWidth() - 8);
    }

private:
    //==============================================================================
    void buildLibraryCategories()
    {
        for (auto& e : LibraryIndex::entries)
        {
            const int g = juce::String (e.group) == "Batteria" ? 0 : 1;
            const auto cat = juce::String (juce::CharPointer_UTF8 (e.category));
            if (! groups[(size_t) g].contains (cat))
                groups[(size_t) g].add (cat);
        }
    }

    void showTab (int tab, int category)
    {
        lastTab = tab;
        auto cats = categoriesFor (tab);
        categoryList.setItems (cats, -1);
        showCategory (juce::jlimit (0, (int) cats.size() - 1, category));
    }

    std::vector<TileList::Item> categoriesFor (int tab) const
    {
        std::vector<TileList::Item> cats;

        if (tab <= 1)
        {
            for (auto& c : groups[(size_t) tab])
                cats.push_back ({ c, {}, Theme::textDim, true });
        }
        else if (tab == 2)
        {
            cats.push_back ({ "Synth", {}, juce::Colour (0xffb07cff), true });
            cats.push_back ({ "Kit DrumForge", {}, juce::Colour (0xffff6b5b), true });
            cats.push_back ({ "Kit su canali separati", {}, juce::Colour (0xffff6b5b), true });
            for (int k = 0; k < SoundLibrary::numKits; ++k)
                cats.push_back ({ juce::String ("Batteria ") + SoundLibrary::getKitName (k), {}, Theme::textDim, true });
        }
        else
        {
            cats.push_back ({ "Campioni importati", {}, Theme::textDim, true });
        }
        return cats;
    }

    void showCategory (int index)
    {
        const int tab = tabs.getSelected();
        lastCategory[(size_t) tab] = index;
        categoryList.setSelected (index);
        std::vector<TileList::Item> tiles;
        soundsFor (tab, index, choices, tiles);

        // Evidenzia il suono attuale del canale
        int current = -1;
        if (targetChannel >= 0)
            for (int i = 0; i < (int) choices.size(); ++i)
                if (isCurrentSound (choices[(size_t) i]))
                    current = i;

        tileList.setItems (tiles, current);
        tileView.setViewPosition (0, current >= 0 ? juce::jmax (0, tileList.getItemBounds (current).getY() - 40) : 0);
        chosen = {};
        confirmButton.setEnabled (false);
        repaint();
    }

    void soundsFor (int tab, int index, std::vector<SoundChoice>& outChoices, std::vector<TileList::Item>& tiles) const
    {
        outChoices.clear();
        tiles.clear();

        if (tab <= 1)
        {
            const auto cat = groups[(size_t) tab][index];
            for (auto& e : LibraryIndex::entries)
            {
                const auto entryCat = juce::String (juce::CharPointer_UTF8 (e.category));
                const int g = juce::String (e.group) == "Batteria" ? 0 : 1;
                if (g != tab || entryCat != cat)
                    continue;

                SoundChoice c;
                c.kind = SoundChoice::library;
                c.id = e.id;
                c.name = juce::String (juce::CharPointer_UTF8 (e.name));
                c.colour = e.colour;
                outChoices.push_back (c);
                tiles.push_back ({ c.name, {}, juce::Colour (e.colour), true });
            }
        }
        else if (tab == 2 && (index == 1 || index == 2))
        {
            // I kit di DrumForge: in un solo canale (la drum machine) o un canale per suono
            for (int k = 0; k < df::Presets::numFactory(); ++k)
            {
                SoundChoice c;
                c.kind = index == 1 ? SoundChoice::drumForgeKit : SoundChoice::kitChannels;
                c.id = "drumforge";
                c.kit = k;
                c.name = "Kit " + df::Presets::factoryName (k);
                c.colour = 0xffff6b5b;
                outChoices.push_back (c);
                tiles.push_back ({ c.name, index == 1 ? juce::String ("16 pad in DrumForge") : juce::String (kitChannelPads.size()) + " canali",
                                   juce::Colour (c.colour), true });
            }
        }
        else if (tab == 2 && index > 0)
        {
            index -= firstDrumSynthCategory;   // 0 = Synth, 1-2 = kit di DrumForge, poi i kit della batteria sintetizzata
            for (int t = 0; t < SequencerEngine::numDrumTypes; ++t)
            {
                SoundChoice c;
                c.kind = SoundChoice::drumSynth;
                c.drumType = t;
                c.kit = index;
                c.name = SequencerEngine::getDrumTypeInfo (t).name;
                c.colour = SequencerEngine::getDrumTypeInfo (t).colour;
                outChoices.push_back (c);
                tiles.push_back ({ c.name, SoundLibrary::getKitName (index), juce::Colour (c.colour), true });
            }
        }
        else if (tab == 2)
        {
            for (auto& info : Instruments::list())
            {
                SoundChoice c;
                c.kind = SoundChoice::instrument;
                c.id = info.id;
                c.name = info.name;
                c.colour = info.colour;
                outChoices.push_back (c);
                tiles.push_back ({ info.name, juce::String (juce::CharPointer_UTF8 (info.detail)), juce::Colour (info.colour), true });
            }
        }
        else
        {
            SoundChoice imp;
            imp.kind = SoundChoice::importFile;
            imp.name = "Importa un file audio...";
            outChoices.push_back (imp);
            tiles.push_back ({ "+ Importa...", "WAV, AIFF, FLAC, OGG, MP3", Theme::accent, true });

            for (auto& f : samples.listSamples())
            {
                SoundChoice c;
                c.kind = SoundChoice::userSample;
                c.id = f;
                c.name = SampleStore::displayName (f);
                c.colour = 0xffc49360;
                outChoices.push_back (c);
                tiles.push_back ({ c.name, {}, juce::Colour (c.colour), true });
            }
        }
    }

    //==============================================================================
    // Albero (banco di lavoro sul PC)

    static juce::Colour tabColour (int tab)
    {
        static const juce::uint32 cols[] { 0xffff8a5b, 0xff4fe3ff, 0xffb07cff, 0xffc49360 };
        return juce::Colour (cols[juce::jlimit (0, 3, tab)]);
    }

    void rebuildTree (bool scrollToCurrent)
    {
        static const char* tabNames[] { "Batteria", "Suoni", "Sintesi", "Campioni" };
        std::vector<TreeList::Row> rows;
        treeChoices.clear();
        int selectedRow = -1, currentRow = -1;
        for (int tab = 0; tab < 4; ++tab)
        {
            TreeList::Row top;
            top.level = 0; top.tab = tab; top.open = openTabs.count (tab) > 0;
            top.text = tabNames[tab]; top.colour = tabColour (tab);
            rows.push_back (top);
            if (! top.open) continue;

            const auto cats = categoriesFor (tab);
            for (int c = 0; c < (int) cats.size(); ++c)
            {
                TreeList::Row sub;
                sub.level = 1; sub.tab = tab; sub.category = c; sub.open = openCats.count ({ tab, c }) > 0;
                sub.text = cats[(size_t) c].text;
                sub.colour = cats[(size_t) c].colour == Theme::textDim ? tabColour (tab).withMultipliedSaturation (0.5f) : cats[(size_t) c].colour;
                rows.push_back (sub);
                if (! sub.open) continue;

                std::vector<SoundChoice> ch;
                std::vector<TileList::Item> items;
                soundsFor (tab, c, ch, items);
                for (int k = 0; k < (int) ch.size(); ++k)
                {
                    TreeList::Row leaf;
                    leaf.level = 2; leaf.folder = false; leaf.tab = tab; leaf.category = c;
                    leaf.sound = (int) treeChoices.size();
                    leaf.text = items[(size_t) k].text; leaf.detail = items[(size_t) k].detail;
                    leaf.colour = items[(size_t) k].colour;
                    leaf.current = targetChannel >= 0 && isCurrentSound (ch[(size_t) k]);
                    if (leaf.current) currentRow = (int) rows.size();
                    if (sameChoice (ch[(size_t) k], chosen)) selectedRow = (int) rows.size();
                    treeChoices.push_back (ch[(size_t) k]);
                    rows.push_back (leaf);
                }
            }
        }
        tree.setRows (std::move (rows));
        tree.setSelected (selectedRow);
        if (scrollToCurrent && currentRow >= 0)
            treeView.setViewPosition (0, juce::jmax (0, currentRow * TreeList::rowH - treeView.getHeight() / 3));
    }

    static bool sameChoice (const SoundChoice& a, const SoundChoice& b)
    {
        return a.kind != SoundChoice::none && a.kind == b.kind && a.id == b.id && a.kit == b.kit && a.drumType == b.drumType;
    }

    void treeClick (int r)
    {
        const auto& rows = tree.getRows();
        if (r < 0 || r >= (int) rows.size()) return;
        const auto row = rows[(size_t) r];
        if (row.folder)
        {
            if (row.level == 0) { if (! openTabs.erase (row.tab)) openTabs.insert (row.tab); }
            else if (! openCats.erase ({ row.tab, row.category })) openCats.insert ({ row.tab, row.category });
            const int y = treeView.getViewPositionY();
            rebuildTree (false);
            treeView.setViewPosition (0, y);
            return;
        }
        chosen = treeChoices[(size_t) row.sound];
        tree.setSelected (r);
        if (onTap) onTap();
        if (chosen.kind == SoundChoice::importFile) { confirm(); return; }
        preview (chosen);
        confirmButton.setEnabled (true);
        repaint();
    }

    bool isCurrentSound (const SoundChoice& c) const
    {
        auto& ch = engine.getChannel (targetChannel);
        switch (c.kind)
        {
            case SoundChoice::library:    return ch.sampleName == SampleStore::libraryName (c.id);
            case SoundChoice::userSample: return ch.sampleName == c.id;
            case SoundChoice::drumSynth:  return engine.isDrumSynthChannel (targetChannel) && ! engine.isSynth (targetChannel)
                                                 && ch.drumType.load() == c.drumType && ch.kit.load() == c.kit;
            case SoundChoice::instrument: return engine.isSynth (targetChannel) && ch.instrumentKind == c.id;
            default: return false;
        }
    }

    void select (int index)
    {
        if (index < 0 || index >= (int) choices.size())
            return;

        tileList.setSelected (index);
        chosen = choices[(size_t) index];
        if (onTap) onTap();

        // Importa: niente anteprima, si sceglie subito il file
        if (chosen.kind == SoundChoice::importFile)
        {
            confirm();
            return;
        }

        preview (chosen);
        confirmButton.setEnabled (true);
        repaint();
    }

    void preview (const SoundChoice& c)
    {
        if (c.kind == SoundChoice::library)
        {
            if (auto* s = samples.load (SampleStore::libraryName (c.id)))
                engine.previewSample (s);
        }
        else if (c.kind == SoundChoice::userSample)
        {
            if (auto* s = samples.load (c.id))
                engine.previewSample (s);
        }
        else if (c.kind == SoundChoice::drumSynth)
        {
            engine.previewDrum (c.drumType, c.kit);
        }
        else if (c.kind == SoundChoice::drumForgeKit || c.kind == SoundChoice::kitChannels)
        {
            const auto& kit = df::factoryKits()[(size_t) juce::jlimit (0, df::Presets::numFactory() - 1, c.kit)];
            if (auto* s = samples.load (SampleStore::libraryName (kit.pads[0].sample)))
                engine.previewSample (s);
        }
        else if (c.kind == SoundChoice::instrument)
        {
            // Anteprima: se nel progetto c'e' gia' uno strumento di questo tipo, fa sentire quello
            for (int ch = 0; ch < engine.getNumChannels(); ++ch)
                if (engine.isSynth (ch) && engine.getChannel (ch).instrumentKind == c.id)
                {
                    engine.previewChannel (ch);
                    break;
                }
        }
    }

    void confirm()
    {
        if (chosen.kind == SoundChoice::none)
            return;

        const auto choice = chosen;
        const int channel = targetChannel;
        if (onChoose)
            onChoose (channel, choice);
    }

public:
    /** I pad di un kit che diventano canali ("Kit su canali separati"): cassa, rullante, clap, hat chiuso, hat aperto, rim, tom, crash. */
    static inline const std::array<int, 8> kitChannelPads { 0, 2, 3, 5, 7, 4, 9, 11 };

private:
    static constexpr int firstDrumSynthCategory = 3;
    SequencerEngine& engine;
    SampleStore& samples;
    int targetChannel = -1, lastTab = 0;
    std::array<int, 4> lastCategory {};
    std::array<juce::StringArray, 2> groups;      // categorie della libreria: Strumenti, Elettronica
    std::vector<SoundChoice> choices;
    SoundChoice chosen;

    SegmentedControl tabs { { "Batteria", "Suoni", "Sintesi", "Campioni" } };
    juce::Viewport categoryView, tileView, treeView;
    TreeList tree;
    std::set<int> openTabs;
    std::set<std::pair<int, int>> openCats;
    std::vector<SoundChoice> treeChoices;
    TileList categoryList, tileList;
    juce::TextButton closeButton { "Chiudi" }, confirmButton { "Aggiungi" };
    juce::Rectangle<float> titleArea, footerText;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SoundBrowser)
};
