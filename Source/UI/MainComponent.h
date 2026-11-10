#pragma once

#include <JuceHeader.h>
#include <map>
#include "../Engine/SequencerEngine.h"
#include "../Engine/RenderAhead.h"
#include "../Engine/Freeze.h"
#include "../Engine/ProjectShare.h"
#include "DawLookAndFeel.h"
#include "Widgets.h"
#include "ChannelRow.h"
#include "ChannelEditor.h"
#include "PianoRoll.h"
#include "SynthPanel.h"
#include "SoundBrowser.h"
#include "Playlist.h"
#include "ExportPanel.h"
#include "MixerPanel.h"
#include "PresetBrowser.h"
#include "JLoopSplash.h"
#include "DemoProjects.h"
#include "QuickGuide.h"
#include "UndoHistory.h"
#include "DoctorPanel.h"
#include "LivePanel.h"
#include "HoloDisplay.h"
#include "Haptics.h"

//==============================================================================
/** La schermata principale: barra del trasporto + channel rack. */
class MainComponent : public juce::Component,
                      public juce::FileDragAndDropTarget,
                      private juce::Timer,
                      private juce::MidiInputCallback
{
public:
    MainComponent();
    ~MainComponent() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    /** Salva lo stato corrente nel file di autosalvataggio (chiamato anche quando l'app va in background). */
    void saveSession();

    /** Tasto Indietro di Android: chiude il pannello aperto. false = niente da chiudere. */
    bool handleBackButton();

    /** Solo per i test sul PC: "--editor N" apre l'editor del canale N, "--mode N" sceglie la modalita'. */
    void handleCommandLine (const juce::String& commandLine);
    /** PC: tasti arrivati a una finestra separata (synth): scorciatoie e tastiera-pianoforte valgono anche li'. */
    bool handleWindowKey (const juce::KeyPress& k)     { return keyPressed (k); }
    bool handleWindowKeyState (bool down)               { return keyStateChanged (down); }
    /** PC: gli strumenti si aprono in finestre separate (una per canale, anche piu' insieme). */
    std::map<int, std::unique_ptr<juce::DocumentWindow>> synthWindows;
    void closeSynthWindow (int channel);
    void closeAllSynthWindows();
    bool useSynthWindows() const;
    juce::PropertiesFile* getSettings()                 { return settings.get(); }

private:
    void timerCallback() override;
    juce::Rectangle<int> getContentArea() const;

    // Menu e finestre
    void showFileMenu();
    void showPatternMenu();
    void showKitMenu();
    void askProjectName (const juce::String& title, std::function<void (juce::String)> onName);
    void saveProjectAs (const juce::String& name, bool confirmOverwrite);
    void openProject (const juce::String& name);
    void deleteProject (const juce::String& name);
    void newProject();
    void setAudioReserve (int ms);
    void applyDefaultDrums();
    void loadDemo (int index);
    void showGuide();
    void showUndoHistory();
    void openLive();
    void jumpInHistory (int steps);
    void closeGuide (int demo);
    void showAudioInfo();
    void showAudioSetup();
    void saveAudioDevice();
    void showShortcuts();
    // --- PC: file trascinati da Esplora risorse, dimensione dell'interfaccia, suggerimenti
    bool isInterestedInFileDrag (const juce::StringArray& files) override;
    void filesDropped (const juce::StringArray& files, int x, int y) override;
    void fileDragEnter (const juce::StringArray&, int, int) override  { dropHighlight = true; repaint(); }
    void fileDragExit (const juce::StringArray&) override              { dropHighlight = false; repaint(); }
    bool dropHighlight = false;
    void paintOverChildren (juce::Graphics&) override;
    void setUiScale (float scale);
    void exportMidi (bool song);
    std::unique_ptr<juce::TooltipWindow> tooltips;
    void showMessage (const juce::String& title, const juce::String& message);

    void syncControlsFromEngine();
    void openEditor (int channel);
    void openPianoRoll (int channel);
    int getVisibleSteps() const;
    float getFirstVisibleStep() const;
    void layoutLandscape (juce::Rectangle<int> area);
    void layoutPortrait (juce::Rectangle<int> area);
    void layoutRack (juce::Rectangle<int> rack, bool portrait);

    // Campioni dell'utente
    SampleStore sampleStore;
    std::unique_ptr<juce::FileChooser> chooser;
    juce::PopupMenu buildSoundMenu (int channel);
    void handleSoundMenu (int channel, int result);
    void assignSample (int channel, const juce::String& fileName);
    void importSample (int channel);
    void resolveSamples();

    // Canali: aggiungi / clona / sposta / elimina, menu del canale, browser dei suoni
    void rebuildRows();
    void handleChannelCommand (int channel, int command);
    void openBrowser (int channel);
    void addKitChannels (int kitIndex);
    void applySoundChoice (int channel, const SoundChoice& choice);
    void askText (const juce::String& title, const juce::String& initial, std::function<void (juce::String)> onText);
    void hideOverlays();
    void openPlaylist();
    void openExport();
    void openMixer (int track = -1);
    void scrollToChannel (int channel);
    int shownStructure = -1;
    int scrollStartY = 0;

    // Impostazioni dell'app (vibrazione, buffer audio), salvate a parte rispetto ai progetti
    std::unique_ptr<juce::PropertiesFile> settings;
    void loadSettings();
    void setBufferSize (int samples);
    juce::PopupMenu createSettingsMenu();
    void handleSettingsResult (int result);
    void showPerformanceMenu();
    void updateCpuBadge();
    void restoreSession();

    // Registrazione dal vivo e tastiere MIDI
    void toggleRecording();
    void placeRecordedNotes();
    void setLiveTarget (int channel);
    void scanMidiInputs();
    void handleIncomingMidiMessage (juce::MidiInput*, const juce::MidiMessage&) override;
    std::atomic<int> liveTarget { -1 };
    std::atomic<bool> midiActivity { false };
    juce::StringArray midiDeviceNames;
    int recordQuantize = 12, midiScanCounter = 0;
    bool recordHintShown = false;
    int cpuCounter = 0, lastXRuns = 0, highLoadTicks = 0, glitchesThisSession = 0;
    juce::uint32 lastXRunTime = 0, dirtySince = 0;
    bool performanceHintShown = false, sessionDirty = false, recoveredFromBackup = false;
    void setTabletMode (bool tablet);
    // --- Tastiera del computer: scorciatoie e tasti come pianoforte
    bool keyPressed (const juce::KeyPress&) override;
    bool keyStateChanged (bool isKeyDown) override;
    bool typingInText() const;
    void saveShortcut();
    int pcOctave = 0;                         // spostamento d'ottava della tastiera-pianoforte (-3..+3)
    std::map<int, int> pcKeysDown;            // tasto -> nota suonata
    void releasePcKeys();   // tablet: pattern nuovi da 32 step
    bool loadingDemo = false;
    double tapTempo();   // ritorna i BPM battuti (0 = servono altri tocchi)
    void askBpm();

    // Undo / redo: istantanee del progetto prese automaticamente dopo ogni modifica
    juce::String takeSnapshot();
    void applySnapshot (const juce::String& snapshot);
    void checkForChanges();
    void undo();
    void redo();
    juce::StringArray undoStack, redoStack;
    juce::StringArray undoLabels, redoLabels;   // il nome di ogni passo (cronologia)
    juce::String lastSnapshot;
    int snapshotCounter = 0;

    /** Segna che l'utente ha toccato lo schermo (qualsiasi pannello o menu): allora si cercano modifiche. */
    struct TouchWatcher : public juce::MouseListener
    {
        explicit TouchWatcher (std::atomic<bool>& f) : flag (f) {}
        void mouseUp (const juce::MouseEvent&) override    { flag.store (true); }
        void mouseDrag (const juce::MouseEvent&) override  { flag.store (true); }
        std::atomic<bool>& flag;
    };
    std::atomic<bool> userTouched { true };
    TouchWatcher touchWatcher { userTouched };
    juce::Array<double> tapTimes;
    void updateViews();
    juce::String getKitLabel() const;

    SequencerEngine engine;
    RenderAhead renderAhead { engine };   // audio calcolato in anticipo (assorbe i picchi del telefono)
    std::unique_ptr<Instruments::Host> instruments;      // gli strumenti dei canali (distrutti dopo il pannello)
    std::unique_ptr<SynthPanel> synthPanel;
    std::unique_ptr<JLoopSplash> splash;
    std::unique_ptr<QuickGuide> guide;
    LivePanel livePanel { engine };
    juce::TextButton liveButton { "LIVE" };
    void openSynthPanel (int channel);
    void showPresetMenu (int channel, juce::Component& target);
    juce::AudioDeviceManager deviceManager;
    struct PerfLog
    {
        int blocks = 0, overruns = 0, lockMisses = 0, blockSize = 0, uiStalls = 0, maxVoices = 0, startXRuns = 0, reserveEmpty = 0;
        float loadSum = 0.0f, maxLoad = 0.0f;
        double startMs = 0.0, lastTimerMs = 0.0, worstUiGapMs = 0.0, playingSeconds = 0.0;
    } perfLog;
    juce::String buildPerformanceReport() const;
    void showPerformanceReport();
    void resetPerformanceLog();
    juce::OpenGLContext openGL;          // Android: lo schermo lo disegna la scheda grafica, non il processore
   #if JUCE_WINDOWS
    /** Windows: come disegnare le finestre. JUCE usa Direct2D, che su alcuni portatili lascia pezzi di schermo vecchi
        o neri quando si muove il mouse; di base si usa il disegno "software" (0), stabile. Vale per tutte le finestre,
        anche quelle aperte dopo (menu, strumenti), quindi si controlla di continuo. */
    struct WindowsRenderer : juce::Timer
    {
        int engine = 0;
        void timerCallback() override
        {
            for (int i = 0; i < juce::ComponentPeer::getNumPeers(); ++i)
                if (auto* p = juce::ComponentPeer::getPeer (i))
                    if (engine < p->getAvailableRenderingEngines().size() && p->getCurrentRenderingEngine() != engine)
                        p->setCurrentRenderingEngine (engine);
        }
    } windowsRenderer;
   #endif
    juce::String audioError;
    juce::String projectName;
    juce::String lastSavedJson;
    int autosaveCounter = 0;

    DawLookAndFeel lookAndFeel;

    IconButton fileButton { IconButton::Icon::menu };
    juce::TextButton kitButton, mixerButton { "MIXER" };
    CpuBadge cpuBadge;
    PlayButton playButton;
    RecordButton recordButton;
    LcdSelector bpmDisplay, patternDisplay;     // comandi (ora disegnati dentro il display "olografico")
    HoloDisplay holo;
    SpectrumAnalyser analyser;
    std::vector<float> scopeSamples;
    juce::Image logoImage;
    bool holoWasPlaying = false, spectrumActive = false;
    int spectrumCounter = 0;
    juce::String getKitChipText() const;
    bool hasDrumSynthChannels() const;
    SegmentedControl modeSwitch  { { "STEP", "VEL", "PROB", "ROLL", "PITCH", "PAN", "FX" } };
    int stepFxMode = 0;                 // l'ultima corsia "FX": 0 filtro, 1 riverbero, 2 delay
    int lastModeSelected = 0;
    void chooseStepFx();
    void setStepFxMode (int mode);   // 0 filtro, 1 riverbero, 2 delay, 3 SUONO
    ChannelRow::EditMode currentEditMode() const;
    IconButton undoButton { IconButton::Icon::undo }, redoButton { IconButton::Icon::redo },
               metroButton { IconButton::Icon::metronome };
    ChannelEditor editor { engine };
    PianoRoll pianoRoll { engine };
    PatternLengthChip lengthChip;

    // Congela traccia
    std::unique_ptr<ChannelFreezer> freezer;
    int freezingInstrumentId = 0, resamplePattern = -1;   // resamplePattern >= 0: il risultato va in uno Slicer
    std::vector<std::unique_ptr<SequencerEngine::FrozenAudio>> frozenStore;
    juce::Label freezeStatus;
    void startFreeze (int channel);
    void shareProject();
    void openProjectFile();
    void openProjectFromUrl (const juce::URL&);
    void checkOpenWith();   // "Apri con JLoop" da un'altra app
    juce::TimedCallback openWithPoll { [this] { checkOpenWith(); } };
    void openRiser (bool downer = false);
    void openVocals (int category = -1, int index = -1, int editChannel = -1);
    double retimeSeenBpm = -1.0, retimeChangedAt = 0.0;   // riser/downer rifatti quando cambiano i BPM
    bool retimePending = false;
    std::unique_ptr<juce::FileChooser> projectChooser;
    juce::ScopedMessageBox shareBox;
    void unfreezeChannel (int channel);
    void checkFreezer();
    void askUnfreeze (int channel, std::function<void()> then);
    void collectFrozen();
    BeatWatermark beatWatermark;
    float scrollPos = 0.0f, scrollTarget = 0.0f, scrollDragStart = 0.0f;   // primo step visibile (il rack scorre in orizzontale)
    bool scrollDragging = false;
    double lastUserScrollMs = 0.0;
    void scrollStepsBy (float dx, int phase);
    void setScroll (float pos, bool animate);
    float getMaxScroll() const;
    void applyScroll();
    juce::Slider swingKnob, masterKnob;
    LevelMeter meter;
    StepRuler ruler;
    juce::OwnedArray<ChannelRow> rows;
    juce::Viewport rackView;            // i canali scorrono in verticale quando non stanno tutti
    juce::Component rackContent;
    juce::TextButton addButton;
    SoundBrowser browser { engine, sampleStore };
    Playlist playlist { engine };
    ExportPanel exportPanel { engine, sampleStore };
    DoctorPanel doctorPanel { engine, sampleStore };   // controllo del progetto
    GuideBar guideBar;                                 // la sua guida passo passo
    IconButton doctorButton { IconButton::Icon::doctor };
    void openDoctor();
    void doctorGo (doctor::Go, const doctor::Issue&);
    void layoutGuide();
    MixerPanel mixerPanel { engine };
    // PC, "banco di lavoro": libreria dei suoni fissa a sinistra e mixer fisso sotto il channel rack
    SoundBrowser deskBrowser { engine, sampleStore };
    MixerPanel deskMixer { engine };
    bool deskMode = false, deskBrowserOpened = false;
    juce::Rectangle<int> overlayArea;   // dove si aprono playlist, piano roll... (col banco: a destra della libreria)
    PresetBrowser presetBrowser { engine };
    SegmentedControl songSwitch { { "PAT", "SONG" } };

    juce::Rectangle<float> headerArea, logoArea, swingCaption, masterCaption, kitCaption, rackArea;
    bool portraitLayout = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};
