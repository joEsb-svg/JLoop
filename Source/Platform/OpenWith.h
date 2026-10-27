#pragma once

#include <JuceHeader.h>

/** File aperti da altre app (WhatsApp, gestore file...) e lettura sicura dei content:// di Android. */
namespace OpenWith
{
    /** Se l'app e' stata aperta con "Apri con JLoop", ritorna l'indirizzo del file (una volta sola), altrimenti vuoto. */
    juce::String takePendingUri();

    /** Apre un file scelto dall'utente. Su Android un content:// si legge sempre dal ContentResolver:
        il percorso "vero" (/storage/...) di solito non e' leggibile dall'app (scoped storage). */
    inline std::unique_ptr<juce::InputStream> openStream (const juce::URL& url)
    {
        if (url.getScheme() == "content")
        {
            auto ws = std::make_unique<juce::WebInputStream> (url, false);
            if (ws->connect (nullptr) && ! ws->isError())
                return ws;
        }
        return url.createInputStream (juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inAddress));
    }

    /** Nome del file da mostrare (per i content:// lo chiede ad Android). */
    inline juce::String fileName (const juce::URL& url)
    {
        auto n = url.getFileName();
        if (n.isEmpty() || n.contains ("/"))
            n = juce::URL::removeEscapeChars (url.toString (false)).fromLastOccurrenceOf ("/", false, false);
        return n.fromLastOccurrenceOf ("/", false, false);
    }
}
