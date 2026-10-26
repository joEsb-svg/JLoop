#pragma once

#include <JuceHeader.h>
#include "ProjectIO.h"
#include "SampleStore.h"

//==============================================================================
/**
    Condivisione dei progetti: un unico file ".jloop" (uno zip) con dentro
    - project.json : il progetto completo (strumenti, preset, effetti, playlist...)
    - samples/...  : i campioni importati dall'utente che il progetto usa (anche dentro DrumForge e Slicer)
    Chi lo riceve lo apre con "Apri progetto da file": i campioni vanno nella sua cartella, il progetto nella lista.
*/
namespace ProjectShare
{
    inline juce::File getShareFolder()    { return ProjectIO::getRootFolder().getChildFile ("Condivisi"); }

    /** Crea il file .jloop. 'extraText' = testi in cui cercare i nomi dei campioni (stati degli strumenti). */
    inline juce::File pack (const juce::var& projectJson, const juce::String& projectName, const juce::StringArray& extraText, juce::String& error)
    {
        const auto jsonText = juce::JSON::toString (projectJson);
        juce::ZipFile::Builder zip;
        zip.addEntry (new juce::MemoryInputStream (jsonText.toRawUTF8(), jsonText.getNumBytesAsUTF8(), true), 9, "project.json", juce::Time::getCurrentTime());

        // Campioni dell'utente usati dal progetto (cercati per nome nel progetto e negli stati degli strumenti)
        const auto all = jsonText + "\n" + extraText.joinIntoString ("\n");
        for (auto& f : SampleStore::getFolder().findChildFiles (juce::File::findFiles, false))
            if (all.contains (f.getFileName()))
                zip.addFile (f, 0, "samples/" + f.getFileName());

        auto folder = getShareFolder();
        folder.createDirectory();
        for (auto& old : folder.findChildFiles (juce::File::findFiles, false, "*.jloop"))
            old.deleteFile();   // solo l'ultimo: non si accumulano copie
        auto file = folder.getChildFile (juce::File::createLegalFileName (projectName.isEmpty() ? "Progetto" : projectName) + ".jloop");
        juce::FileOutputStream out (file);
        if (! out.openedOk() || ! zip.writeToStream (out, nullptr))
        {
            error = "Non riesco a creare il file da condividere.";
            return {};
        }
        out.flush();
        return file;
    }

    /** Apre un .jloop (o un .dawfl): copia i campioni e salva il progetto nella lista. Ritorna il nome del progetto. */
    inline juce::String unpack (juce::InputStream& in, const juce::String& suggestedName, juce::String& error)
    {
        juce::MemoryBlock data;
        in.readIntoMemoryBlock (data, 300 * 1024 * 1024);
        juce::String jsonText;

        juce::MemoryInputStream zipStream (data, false);
        juce::ZipFile zip (zipStream);
        if (zip.getNumEntries() > 0)
        {
            SampleStore::getFolder().createDirectory();
            for (int i = 0; i < zip.getNumEntries(); ++i)
            {
                const auto* e = zip.getEntry (i);
                std::unique_ptr<juce::InputStream> s (zip.createStreamForEntry (i));
                if (s == nullptr) continue;
                if (e->filename == "project.json")
                    jsonText = s->readEntireStreamAsString();
                else if (e->filename.startsWith ("samples/"))
                {
                    auto dest = SampleStore::getFolder().getChildFile (juce::File::createLegalFileName (e->filename.fromFirstOccurrenceOf ("samples/", false, false)));
                    if (! dest.existsAsFile())   // un campione con lo stesso nome c'e' gia': si usa quello
                    {
                        juce::FileOutputStream out (dest);
                        if (out.openedOk()) out.writeFromInputStream (*s, -1);
                    }
                }
            }
        }
        else
            jsonText = data.toString();   // un .dawfl semplice (solo JSON)

        const auto json = juce::JSON::parse (jsonText);
        SequencerEngine probe;
        if (jsonText.isEmpty() || ! ProjectIO::fromJson (probe, json))
        {
            error = "Questo file non e' un progetto JLoop.";
            return {};
        }

        // Nome libero nella lista dei progetti
        auto base = juce::File::createLegalFileName (suggestedName.upToLastOccurrenceOf (".", false, false).trim());
        if (base.isEmpty()) base = "Progetto ricevuto";
        auto name = base;
        for (int i = 2; ProjectIO::getProjectFile (name).existsAsFile(); ++i)
            name = base + " (" + juce::String (i) + ")";
        ProjectIO::getProjectsFolder().createDirectory();
        if (! ProjectIO::getProjectFile (name).replaceWithText (jsonText))
        {
            error = "Non riesco a salvare il progetto.";
            return {};
        }
        return name;
    }
}
