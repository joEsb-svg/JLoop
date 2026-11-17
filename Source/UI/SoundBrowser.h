#pragma once

#include <JuceHeader.h>
#include <functional>
#include <vector>
#include <set>
#include <map>
#include "../Engine/SequencerEngine.h"
#include "../Engine/SampleStore.h"
#include "../Instruments/Instruments.h"
#include "Theme.h"
#include "WaveView.h"
#include "../Engine/SampleRender.h"
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
    int preset = -1;            // synth: preset di fabbrica da caricare (-1 = quello iniziale)
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
    std::function<void (int row)> onDragStart;     // il mouse ha trascinato una voce (per lasciarla su un canale)
    std::function<bool (int row)> canDrag;

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
            if (row.detail.isNotEmpty() && row.folder)
            {
                g.drawText (row.text, content, juce::Justification::centredLeft, true);
                g.setColour (Theme::textDim);
                g.setFont (Theme::font (10.5f));
                g.drawText (row.detail, content.withTrimmedRight (4.0f), juce::Justification::centredRight, true);
                continue;
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

    void mouseDown (const juce::MouseEvent&) override   { dragging = false; }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (dragging || e.getDistanceFromDragStart() < 8 || onDragStart == nullptr) return;
        const int i = e.getMouseDownY() / rowH;
        if (i >= 0 && i < (int) rows.size() && (canDrag == nullptr || canDrag (i)))
        {
            dragging = true;
            onDragStart (i);
        }
    }

    void mouseUp (const juce::MouseEvent& e) override
    {
        if (dragging) { dragging = false; return; }
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
    bool dragging = false;
};

//==============================================================================
/**
    Il browser dei suoni: si apre sopra il channel rack per aggiungere uno strumento
    o cambiare il suono di un canale.

    - Strumenti:   pianoforti, chitarre, bassi, archi, fiati, voci, percussioni, batteria acustica...
    - Elettronica: EDM, House, Techno, Electro, Drum and Bass, Dubstep, Trap, synth, effetti
    - Sintesi:     i synth (BassForge, PolyForge) e la batteria sintetizzata di AudioNaut (kit per genere)
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
        tree.onDragStart = [this] (int r) { startDrag (r); };
        tree.canDrag = [this] (int r) { const auto c = choiceForRow (r); return c.kind != SoundChoice::none && c.kind != SoundChoice::importFile; };

        categoryView.setViewedComponent (&categoryList, false);
        tileView.setViewedComponent (&tileList, false);
        categoryList.onTap = [this] (int i) { showCategory (i); };
        tileList.onTap     = [this] (int i) { select (i); };

        buildLibraryCategories();
        addAndMakeVisible (wave);
        wave.setEmptyText ("Qui vedi la forma d'onda del suono che tocchi");
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
        // sul PC il mouse trascina i suoni (la rotella scorre); col dito si scorre come prima
        treeView.setScrollOnDragMode (e ? juce::Viewport::ScrollOnDragMode::nonHover : juce::Viewport::ScrollOnDragMode::all);
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
        if (embedded)
        {
            // "SUONO DEL CANALE / Kick Complextro" (o, senza canale, come si aggiunge)
            g.setColour (Theme::textDim);
            g.setFont (Theme::font (9.5f, true).withExtraKerningFactor (0.14f));
            g.drawText (targetChannel < 0 ? juce::String ("SUONI") : juce::String ("SUONO DEL CANALE"), title.removeFromTop (15.0f), juce::Justification::bottomLeft, false);
            g.setColour (Theme::textBright);
            g.setFont (Theme::font (16.0f, true));
            g.drawFittedText (targetChannel < 0 ? juce::String ("Trascina un suono sul rack") : engine.getChannel (targetChannel).name,
                              title.toNearestInt(), juce::Justification::centredLeft, 1, 0.8f);
            g.setColour (chosen.kind == SoundChoice::none ? Theme::textDim : Theme::text);
            g.setFont (Theme::font (12.0f, chosen.kind != SoundChoice::none));
            g.drawFittedText (chosen.kind == SoundChoice::none ? juce::String ("Tocca un suono per sentirlo, poi trascinalo su un canale")
                                                               : juce::String (juce::CharPointer_UTF8 ("\xe2\x99\xaa  ")) + chosen.name
                                                                 + (chosen.preset >= 0 ? juce::String (juce::CharPointer_UTF8 (" \xc2\xb7 ")) + chosenLabel : juce::String())
                                                                 + juce::String ("   (trascinalo su un canale)"),
                              footerText.toNearestInt(), juce::Justification::centredLeft, 1, 0.8f);
            return;
        }
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
            confirmButton.setVisible (false);   // i suoni si trascinano sul rack (doppio clic: sul canale scelto)
            titleArea = titleArea.withHeight (40.0f);
            r.removeFromTop (8);
            wave.setBounds (r.removeFromBottom (juce::jlimit (70, 120, r.getHeight() / 5)));
            footerText = r.removeFromBottom (24).toFloat();
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
        wave.setBounds (r.removeFromBottom (r.getHeight() > 420 ? 72 : 52));
        r.removeFromBottom (6);

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

    // Le cartelle dell'albero: Batteria, Suoni, Synth (con i preset dentro), Kit, I miei campioni
    enum TreeTop { topDrums, topSounds, topSynth, topKits, topMine, numTops };

    static juce::Colour topColour (int top)
    {
        static const juce::uint32 cols[] { 0xffff8a5b, 0xff4fe3ff, 0xffb07cff, 0xffff6b5b, 0xffe8c35a };
        return juce::Colour (cols[juce::jlimit (0, numTops - 1, top)]);
    }

    /** Le sottocartelle di una cartella principale (nome, dettaglio a destra, colore). */
    std::vector<TreeList::Row> subFolders (int top) const
    {
        std::vector<TreeList::Row> out;
        auto add = [&out, top] (const juce::String& text, const juce::String& detail, juce::Colour c)
        {
            TreeList::Row r; r.level = 1; r.tab = top; r.category = (int) out.size(); r.text = text; r.detail = detail; r.colour = c;
            out.push_back (r);
        };
        if (top == topDrums || top == topSounds)
            for (auto& c : groups[(size_t) top]) add (c, {}, topColour (top).withMultipliedSaturation (0.5f));
        else if (top == topSynth)
            for (auto& info : Instruments::list())
            {
                const int n = Instruments::factoryPresetNames (info.id).size();
                add (info.name, n > 0 ? juce::String (n) + " preset" : juce::String(), juce::Colour (info.colour));
            }
        else if (top == topKits)
        {
            const auto cats = categoriesFor (2);
            for (int c = 1; c < (int) cats.size(); ++c)
                add (cats[(size_t) c].text, {}, cats[(size_t) c].colour == Theme::textDim ? topColour (top).withMultipliedSaturation (0.5f) : cats[(size_t) c].colour);
        }
        return out;
    }

    /** I suoni dentro una sottocartella (o dentro "I miei campioni", che non ne ha). */
    void treeSounds (int top, int sub, std::vector<SoundChoice>& ch, std::vector<TileList::Item>& items) const
    {
        ch.clear(); items.clear();
        if (top == topDrums || top == topSounds) soundsFor (top, sub, ch, items);
        else if (top == topKits)                 soundsFor (2, sub + 1, ch, items);
        else if (top == topMine)                 soundsFor (3, 0, ch, items);
        else if (top == topSynth)
        {
            const auto& list = Instruments::list();
            if (sub < 0 || sub >= (int) list.size()) return;
            const auto& info = list[(size_t) sub];
            const auto names = Instruments::factoryPresetNames (info.id);
            for (int i = 0; i < names.size(); ++i)
            {
                SoundChoice c;
                c.kind = SoundChoice::instrument;
                c.id = info.id; c.name = info.name; c.colour = info.colour; c.preset = i;
                ch.push_back (c);
                items.push_back ({ names[i], {}, juce::Colour (info.colour), true });
            }
        }
    }

    void rebuildTree (bool scrollToCurrent)
    {
        static const char* topNames[] { "Batteria", "Suoni", "Synth", "Kit", "I miei campioni" };
        std::vector<TreeList::Row> rows;
        treeChoices.clear();
        int selectedRow = -1, currentRow = -1;
        auto addLeaves = [&] (int top, int sub, int level)
        {
            std::vector<SoundChoice> ch;
            std::vector<TileList::Item> items;
            treeSounds (top, sub, ch, items);
            for (int k = 0; k < (int) ch.size(); ++k)
            {
                TreeList::Row leaf;
                leaf.level = level; leaf.folder = false; leaf.tab = top; leaf.category = sub;
                leaf.sound = (int) treeChoices.size();
                leaf.text = items[(size_t) k].text; leaf.detail = items[(size_t) k].detail;
                leaf.colour = items[(size_t) k].colour;
                leaf.current = targetChannel >= 0 && isCurrentSound (ch[(size_t) k]);
                if (leaf.current) currentRow = (int) rows.size();
                if (sameChoice (ch[(size_t) k], chosen)) selectedRow = (int) rows.size();
                treeChoices.push_back (ch[(size_t) k]);
                rows.push_back (leaf);
            }
        };
        for (int top = 0; top < numTops; ++top)
        {
            TreeList::Row t;
            t.level = 0; t.tab = top; t.open = openTabs.count (top) > 0;
            t.text = topNames[top]; t.colour = topColour (top);
            rows.push_back (t);
            if (! t.open) continue;
            if (top == topMine) { addLeaves (top, 0, 1); continue; }
            for (auto sub : subFolders (top))
            {
                sub.open = openCats.count ({ top, sub.category }) > 0;
                // il synth del canale: si vede subito quale e'
                if (top == topSynth && targetChannel >= 0 && engine.isSynth (targetChannel)
                    && engine.getChannel (targetChannel).instrumentKind == juce::String (Instruments::list()[(size_t) sub.category].id))
                    sub.current = true;
                rows.push_back (sub);
                if (sub.open) addLeaves (top, sub.category, 2);
            }
        }
        tree.setRows (std::move (rows));
        tree.setSelected (selectedRow);
        if (scrollToCurrent && currentRow >= 0)
            treeView.setViewPosition (0, juce::jmax (0, currentRow * TreeList::rowH - treeView.getHeight() / 3));
    }

    /** La voce trascinabile di una riga: un suono, oppure un synth intero (senza scegliere il preset). */
    SoundChoice choiceForRow (int r) const
    {
        const auto& rows = tree.getRows();
        if (r < 0 || r >= (int) rows.size()) return {};
        const auto& row = rows[(size_t) r];
        if (! row.folder) return treeChoices[(size_t) row.sound];
        if (row.level == 1 && row.tab == topSynth)
        {
            const auto& info = Instruments::list()[(size_t) row.category];
            SoundChoice c; c.kind = SoundChoice::instrument; c.id = info.id; c.name = info.name; c.colour = info.colour;
            return c;
        }
        return {};
    }

    void startDrag (int r)
    {
        const auto c = choiceForRow (r);
        if (c.kind == SoundChoice::none || c.kind == SoundChoice::importFile) return;
        dragged = c;
        auto* container = juce::DragAndDropContainer::findParentDragContainerFor (this);
        if (container == nullptr) return;
        // l'etichetta che segue il mouse
        const auto label = tree.getRows()[(size_t) r].text;
        const int w = juce::jlimit (90, 260, juce::GlyphArrangement::getStringWidthInt (Theme::font (13.0f, true), label) + 30);
        juce::Image img (juce::Image::ARGB, w, 28, true);
        {
            juce::Graphics g (img);
            g.setColour (Theme::raisedHi.withAlpha (0.95f));
            g.fillRoundedRectangle (img.getBounds().toFloat().reduced (1.0f), 6.0f);
            g.setColour (juce::Colour (c.colour));
            g.fillRect (8, 6, 3, 16);
            g.setColour (Theme::textBright);
            g.setFont (Theme::font (13.0f, true));
            g.drawText (label, 18, 0, w - 22, 28, juce::Justification::centredLeft, true);
        }
        container->startDragging (dragDescription, &tree, juce::ScaledImage (img), false);
    }

    static bool sameChoice (const SoundChoice& a, const SoundChoice& b)
    {
        return a.kind != SoundChoice::none && a.kind == b.kind && a.id == b.id && a.kit == b.kit && a.drumType == b.drumType && a.preset == b.preset;
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
        chosenLabel = row.text;
        tree.setSelected (r);
        if (onTap) onTap();
        if (chosen.kind == SoundChoice::importFile) { confirm(); return; }
        preview (chosen);
        showWave (chosen);
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
            case SoundChoice::instrument:
                if (! engine.isSynth (targetChannel) || ch.instrumentKind != c.id) return false;
                if (c.preset < 0) return true;
                return Instruments::presetName (engine.getInstrument (targetChannel)) == Instruments::factoryPresetNames (c.id)[c.preset];
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
        showWave (chosen);
        confirmButton.setEnabled (true);
        repaint();
    }

public:
    /** Test: come se si fosse toccato quel suono della libreria. */
    void showLibrarySoundForTest (const juce::String& id)
    {
        SoundChoice c; c.kind = SoundChoice::library; c.id = id;
        if (auto* e = SampleStore::findLibraryEntry (id)) { c.name = juce::String (juce::CharPointer_UTF8 (e->name)); c.colour = e->colour; }
        chosen = c;
        showWave (c);
        repaint();
    }
private:
    /** La forma d'onda del suono scelto (i synth non ne hanno una fissa). */
    void showWave (const SoundChoice& c)
    {
        const SampleData* s = nullptr;
        if (c.kind == SoundChoice::library)         s = samples.load (SampleStore::libraryName (c.id));
        else if (c.kind == SoundChoice::userSample) s = samples.load (c.id);
        else if (c.kind == SoundChoice::drumSynth)  s = samples.load (SampleStore::drumName (c.drumType, c.kit));
        else if (c.kind == SoundChoice::drumForgeKit || c.kind == SoundChoice::kitChannels)
        {
            const auto& kit = df::factoryKits()[(size_t) juce::jlimit (0, df::Presets::numFactory() - 1, c.kit)];
            s = samples.load (SampleStore::libraryName (kit.pads[0].sample));
        }
        if (c.kind == SoundChoice::instrument && c.preset >= 0)
            s = synthPreview (c);
        wave.setEmptyText (c.kind == SoundChoice::instrument ? (c.preset >= 0 ? "Questo preset da solo non fa suono (gli serve un campione o una voce)"
                                                                               : "Apri il synth per vedere i suoi preset")
                                                             : "Qui vedi la forma d'onda del suono che tocchi");
        wave.setSample (s, juce::Colour (c.colour));
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
        else if (c.kind == SoundChoice::instrument && c.preset >= 0)
        {
            if (auto* s = synthPreview (c))
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

    /** Un preset di un synth suonato una volta (un Do, poco piu' di un secondo), per sentirlo e vederne l'onda.
        Si fa la prima volta che lo tocchi e poi resta pronto. */
    const SampleData* synthPreview (const SoundChoice& c)
    {
        const auto key = c.id + ":" + juce::String (c.preset);
        if (auto it = previews.find (key); it != previews.end()) return it->second.get();
        auto p = Instruments::create (c.id);
        if (p == nullptr) return nullptr;
        Instruments::loadFactory (p.get(), c.preset);
        const double sr = 44100.0;
        const int block = 512, total = (int) (sr * 1.6), noteOff = (int) (sr * 0.9);
        p->setPlayConfigDetails (0, 2, sr, block);
        p->prepareToPlay (sr, block);
        const bool drums = c.id == "drumforge";
        const int note = drums ? 36 : 48;
        juce::AudioBuffer<float> buf (2, block);
        SampleData::Ptr out (new SampleData());
        out->name = key; out->sampleRate = sr;
        out->audio.setSize (1, total);
        out->audio.clear();
        for (int pos = 0; pos < total; pos += block)
        {
            const int n = juce::jmin (block, total - pos);
            juce::MidiBuffer m;
            if (pos == 0) m.addEvent (juce::MidiMessage::noteOn (1, note, 0.85f), 0);
            if (noteOff >= pos && noteOff < pos + n) m.addEvent (juce::MidiMessage::noteOff (1, note), noteOff - pos);
            buf.setSize (2, n, false, false, true);
            buf.clear();
            p->processBlock (buf, m);
            out->audio.addFrom (0, pos, buf, 0, 0, n, 0.5f);
            if (buf.getNumChannels() > 1) out->audio.addFrom (0, pos, buf, 1, 0, n, 0.5f);
        }
        p->releaseResources();
        const float peak = out->audio.getMagnitude (0, 0, total);
        if (! std::isfinite (peak) || peak < 1.0e-4f) { previews[key] = nullptr; return nullptr; }
        out->audio.applyGain (0.7f / peak);
        // via il silenzio in coda
        int end = total;
        while (end > 2000 && std::abs (out->audio.getSample (0, end - 1)) < 0.002f) --end;
        out->audio.setSize (1, end, true, true, true);
        previews[key] = out;
        return out.get();
    }

public:
    /** Il suono che si sta trascinando sul rack (vuoto se non e' un trascinamento di questo pannello). */
    SoundChoice takeDragged (const juce::var& description)
    {
        if (description.toString() != dragDescription) return {};
        return dragged;
    }
    static inline const juce::String dragDescription { "audionaut-sound" };

    /** Per le foto di prova: apre una cartella principale e una sottocartella e sceglie una voce. */
    void openTreeForTest (int top, int sub, int leaf)
    {
        openTabs.insert (top);
        if (sub >= 0) openCats.insert ({ top, sub });
        rebuildTree (false);
        const auto& rows = tree.getRows();
        int found = -1, count = 0;
        for (int i = 0; i < (int) rows.size(); ++i)
            if (! rows[(size_t) i].folder && rows[(size_t) i].tab == top && (sub < 0 || rows[(size_t) i].category == sub))
                if (count++ == leaf) found = i;
        if (found >= 0) { treeClick (found); treeView.setViewPosition (0, juce::jmax (0, found * TreeList::rowH - treeView.getHeight() / 2)); }
    }

private:
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
    WaveView wave;
    int targetChannel = -1, lastTab = 0;
    std::array<int, 4> lastCategory {};
    std::array<juce::StringArray, 2> groups;      // categorie della libreria: Strumenti, Elettronica
    std::vector<SoundChoice> choices;
    SoundChoice chosen, dragged;
    juce::String chosenLabel;
    std::map<juce::String, SampleData::Ptr> previews;

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
