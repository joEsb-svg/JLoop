#pragma once

#include <atomic>
#include <cmath>

//==============================================================================
/**
    Note scivolo del piano roll: il sequencer chiede a tutto lo strumento di spostare l'altezza delle
    note che stanno suonando di un certo numero di semitoni (anche piu' di un'ottava, in su o in giu').

    Gli strumenti di AudioNaut ereditano questa piccola classe: il sequencer chiama setSlideSemitones()
    a ogni tick (piu' o meno ogni 10 ms), lo strumento all'inizio di ogni blocco legge slideForBlock() e
    lo somma all'altezza (in semitoni) di tutte le voci. Il passaggio da un valore all'altro e' ammorbidito
    (pochi ms), cosi' la scivolata e' continua e senza scalini.
*/
class SlideInput
{
public:
    virtual ~SlideInput() = default;

    /** Dal sequencer (thread audio): di quanti semitoni spostare le note adesso. */
    void setSlideSemitones (float semis) noexcept   { slideTarget.store (semis, std::memory_order_relaxed); }

    /** Nota nuova: si riparte da 0 subito (senza la piccola scivolata di ammorbidimento). */
    void resetSlide() noexcept                      { slideTarget.store (0.0f, std::memory_order_relaxed); slideJump.store (true, std::memory_order_relaxed); }

protected:
    /** All'inizio di processBlock: prepara lo spostamento per questo blocco e restituisce quello di fine blocco.
        Dentro il blocco slideAt (0..1) da' il valore che passa in linea retta dall'inizio alla fine. */
    float beginSlideBlock (int numSamples, double sampleRate) noexcept
    {
        const float target = slideTarget.load (std::memory_order_relaxed);
        if (slideJump.exchange (false, std::memory_order_relaxed)) slideNow = target;
        slideStart = slideNow;
        slideLen = numSamples > 0 ? numSamples : 1;
        const float a = 1.0f - std::exp (-(float) numSamples / (float) (0.006 * (sampleRate > 0.0 ? sampleRate : 48000.0)));
        slideNow += (target - slideNow) * a;
        if (std::abs (target - slideNow) < 0.001f) slideNow = target;
        return slideNow;
    }

    float slideAt (float frac) const noexcept      { return slideStart + (slideNow - slideStart) * frac; }
    float slideAtSample (int s) const noexcept     { return slideAt ((float) s / (float) slideLen); }   // s = campione dentro il blocco
    float slideHere (int i) const noexcept         { return slideAtSample (slideBase + i); }            // i = campione dentro il pezzo che parte a slideBase
    bool sliding() const noexcept               { return slideNow != 0.0f || slideStart != 0.0f; }

    float slideNow = 0.0f, slideStart = 0.0f;
    int slideLen = 1, slideBase = 0;   // slideBase: chi suona il blocco a pezzi (tra un evento MIDI e l'altro) lo mette all'inizio di ogni pezzo

private:
    std::atomic<float> slideTarget { 0.0f };
    std::atomic<bool> slideJump { false };
};
