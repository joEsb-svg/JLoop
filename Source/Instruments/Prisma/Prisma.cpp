#include "Prisma.h"
#include "PrismaEditor.h"
#include <cstring>

namespace prisma
{
    juce::StringArray tableNames()     { return Bank::get().names(); }

    juce::StringArray sourceNames()
    {
        return { "-", "Inviluppo 2", "LFO 1", "LFO 2", "Velocity", "Nota", "Casuale", "Mod wheel" };
    }

    juce::StringArray destNames()
    {
        return { "-", "Posizione A", "Posizione B", "Warp A", "Warp B", "Cutoff", "Risonanza", "Pitch A", "Pitch B", "Volume A", "Volume B",
                 "Detune", "Pan", "Sub", "Rumore", "Drive", "Velocita' LFO 1", "Velocita' LFO 2" };
    }

    //==========================================================================
    // Le tabelle

    namespace
    {
        constexpr double twoPi = juce::MathConstants<double>::twoPi;

        /** Picchi delle formanti (per le tabelle "vocali"): guadagno a una frequenza. */
        double formant (double f, const double (&F)[3], double sharp)
        {
            double g = 0.04;
            const double bw[3] { 90.0, 120.0, 160.0 };
            for (int k = 0; k < 3; ++k)
            {
                const double x = (f - F[k]) / (bw[k] * 0.5 / sharp);
                g += (k == 0 ? 1.0 : k == 1 ? 0.7 : 0.45) / (1.0 + x * x);
            }
            return g;
        }

        void vowelAt (double t, double (&F)[3], const double table[][3], int count)
        {
            const double s = juce::jlimit (0.0, 1.0, t) * (count - 1);
            const int i = juce::jmin ((int) s, count - 2);
            const double fr = s - i;
            for (int k = 0; k < 3; ++k) F[k] = table[i][k] + (table[i + 1][k] - table[i][k]) * fr;
        }

        double basicWave (int w, double x)
        {
            switch (w)
            {
                case 0:  return std::sin (twoPi * x);
                case 1:  return x < 0.25 ? 4.0 * x : x < 0.75 ? 2.0 - 4.0 * x : 4.0 * x - 4.0;
                case 2:  return x < 0.5 ? 2.0 * x : 2.0 * x - 2.0;
                default: return x < 0.5 ? 1.0 : -1.0;
            }
        }

        // Analogico: sinusoide -> triangolo -> dente di sega -> quadra
        void tableAnalog (double t, std::vector<float>& out)
        {
            const double s = t * 3.0;
            const int k = juce::jmin ((int) s, 2);
            const double fr = s - k;
            const int N = (int) out.size();
            for (int i = 0; i < N; ++i)
            {
                const double x = (double) i / N;
                out[(size_t) i] = (float) (basicWave (k, x) * (1.0 - fr) + basicWave (k + 1, x) * fr);
            }
        }

        double vowelAmp (int h, double f0, const double* F, double sharp, int mode)
        {
            const double F3[3] { F[0], F[1], F[2] };
            const double g = formant (h * f0, F3, sharp);
            if (mode == 0) return g / h;
            return (h % 2 == 1 ? 1.0 : 0.35) * g * g / h;
        }

        // Formanti: a -> e -> i -> o -> u su un dente di sega
        void tableFormant (double t, std::vector<float>& out)
        {
            static const double v[5][3] { { 800, 1150, 2900 }, { 450, 2100, 2800 }, { 300, 2600, 3300 }, { 450, 800, 2830 }, { 330, 700, 2700 } };
            double F[3];
            vowelAt (t, F, v, 5);
            for (int h = 1; h <= 200; ++h)
            {
                const double a = vowelAmp (h, 110.0, F, 1.0, 0);
                for (size_t i = 0; i < out.size(); ++i) out[i] += (float) (a * std::sin (twoPi * h * (double) i / (double) out.size()));
            }
        }

        // Growl: "yoi" (o -> a -> i) su armonici dispari, formanti strette
        void tableGrowl (double t, std::vector<float>& out)
        {
            static const double v[3][3] { { 450, 800, 2830 }, { 800, 1150, 2900 }, { 300, 2600, 3300 } };
            double F[3];
            vowelAt (t, F, v, 3);
            for (int h = 1; h <= 300; ++h)
            {
                const double a = vowelAmp (h, 80.0, F, 1.8, 1);
                for (size_t i = 0; i < out.size(); ++i) out[i] += (float) (a * std::sin (twoPi * h * (double) i / (double) out.size()));
            }
        }

        double harmonicCountAmp (int h, double count)   { return juce::jlimit (0.0, 1.0, count - (h - 1)) / h; }

        void tableHarmonics (double t, std::vector<float>& out)
        {
            const double count = 1.0 + t * 63.0;
            for (int h = 1; h <= 64; ++h)
            {
                const double a = harmonicCountAmp (h, count);
                if (a < 1.0e-6) continue;
                for (size_t i = 0; i < out.size(); ++i) out[i] += (float) (a * std::sin (twoPi * h * (double) i / (double) out.size()));
            }
        }

        void tableOrgan (double t, std::vector<float>& out)
        {
            static const int harm[6] { 1, 2, 3, 4, 6, 8 };
            for (int k = 0; k < 6; ++k)
            {
                const double g = k == 0 ? 1.0 : juce::jlimit (0.0, 1.0, t * 6.0 - (k - 1));
                for (size_t i = 0; i < out.size(); ++i) out[i] += (float) (g * 0.8 * std::sin (twoPi * harm[k] * (double) i / (double) out.size()));
            }
        }

        /** Una forma (un periodo) della tabella 'table', alla posizione t (0..1). */
        void makeFrame (int table, double t, std::vector<float>& out)
        {
            const int N = frameSize;
            out.assign ((size_t) N, 0.0f);
            if (table == 0) { tableAnalog (t, out); return; }
            if (table == 1) { tableFormant (t, out); return; }
            if (table == 3) { tableHarmonics (t, out); return; }
            if (table == 7) { tableGrowl (t, out); return; }
            if (table == 9) { tableOrgan (t, out); return; }
            for (int i = 0; i < N; ++i)
            {
                const double x = (double) i / N;
                double y = 0.0;
                if (table == 2)        y = x < 0.5 - 0.47 * t ? 1.0 : -1.0;                                    // PWM
                else if (table == 4)   { const double r = std::fmod (x * (1.0 + t * 7.0), 1.0); y = (2.0 * r - 1.0) * (1.0 - 0.3 * x); }   // Sync
                else if (table == 5)   y = std::sin (twoPi * x + t * 6.0 * std::sin (twoPi * 2.0 * x));        // FM
                else if (table == 6)   y = std::sin ((1.0 + t * 8.0) * std::sin (twoPi * x) * juce::MathConstants<double>::halfPi);   // Wavefolder
                else                                                                                          // Bitcrush
                {
                    const double levels = std::pow (2.0, 5.0 - t * 3.5), steps = 128.0 - t * 112.0;
                    const double q = std::floor (x * steps) / steps;
                    y = std::round ((2.0 * q - 1.0) * levels) / levels;
                }
                out[(size_t) i] = (float) y;
            }
        }
    }

    Bank::Bank()
    {
        // Le tabelle libere (CC0): nome + 32 forme x 1024 campioni int16, dentro l'app
        int size = 0;
        if (const char* raw = BinaryData::getNamedResource ("PrismaTables_bin", size); raw != nullptr && size > 8
              && juce::String (juce::CharPointer_UTF8 (raw), 4) == "PRSM")
        {
            int count = 0;
            std::memcpy (&count, raw + 4, 4);
            const size_t perTable = 32 + (size_t) numFrames * frameSize * 2;
            for (int i = 0; i < count && builtin + (int) imported.size() < maxTables; ++i)
            {
                const size_t off = 8 + (size_t) i * perTable;
                if (off + perTable > (size_t) size) break;
                Imported t;
                t.name = juce::String::fromUTF8 (raw + off, (int) strnlen (raw + off, 32));
                t.samples = reinterpret_cast<const juce::int16*> (raw + off + 32);
                imported.push_back (t);
            }
        }
        ensure (0);
        for (int slot = 0; slot < numUserTables; ++slot)
        {
            juce::MemoryBlock mb;
            if (userDir().getChildFile ("mia_" + juce::String (slot + 1) + ".prismatab").loadFileAsData (mb))
                if (auto t = UserTable::fromBlock (mb.getData(), mb.getSize()); ! t.empty())
                    setUser (slot, std::move (t), false);
        }
    }

    juce::StringArray Bank::names() const
    {
        juce::StringArray n { "Analogico", "Formanti", "PWM", "Armonici", "Sync", "FM", "Wavefolder", "Growl", "Bitcrush", "Organo" };
        for (auto& t : imported) n.add (t.name);
        for (int u = 1; u <= numUserTables; ++u) n.add ("Mia " + juce::String (u));
        return n;
    }

    /** Una forma (1024 campioni) della tabella t, prima della limitazione di banda. */
    void Bank::sourceFrame (int t, int f, std::vector<float>& out) const
    {
        out.assign ((size_t) frameSize, 0.0f);
        if (t < builtin) { makeFrame (t, (double) f / (numFrames - 1), out); return; }
        if (t < userBase())
        {
            const auto* src = imported[(size_t) (t - builtin)].samples + (size_t) f * frameSize;
            for (int i = 0; i < frameSize; ++i)
            {
                juce::int16 v;
                std::memcpy (&v, src + i, 2);   // (allineamento qualsiasi)
                out[(size_t) i] = (float) v / 32767.0f;
            }
            return;
        }
        // tabella dell'utente: si sfuma tra le forme chiave
        const auto& u = users[(size_t) (t - userBase())];
        if (u.empty()) return;
        const float x = u.keys == 1 ? 0.0f : (float) f / (float) (numFrames - 1) * (float) (u.keys - 1);
        const int k0 = juce::jmin ((int) x, juce::jmax (0, u.keys - 2));
        const int k1 = juce::jmin (k0 + 1, u.keys - 1);
        const float fr = x - (float) k0;
        const auto* a = u.samples.data() + (size_t) k0 * frameSize;
        const auto* b = u.samples.data() + (size_t) k1 * frameSize;
        for (int i = 0; i < frameSize; ++i)
            out[(size_t) i] = ((float) a[i] + ((float) b[i] - (float) a[i]) * fr) / 32767.0f;
    }

    /** Costruisce in buf le 32 forme x 8 livelli di armonici della tabella t. */
    void Bank::build (float* buf, int t, bool normalise) const
    {
        juce::dsp::FFT fft (10);   // 1024
        std::vector<float> frameData ((size_t) frameSize), spec ((size_t) frameSize * 2), work ((size_t) frameSize * 2);
        float tablePeak = 1.0e-6f;
        for (int f = 0; f < numFrames; ++f)
        {
            sourceFrame (t, f, frameData);
            std::fill (spec.begin(), spec.end(), 0.0f);
            std::copy (frameData.begin(), frameData.end(), spec.begin());
            fft.performRealOnlyForwardTransform (spec.data(), true);
            for (int level = 0; level < numLevels; ++level)
            {
                const int maxHarm = (frameSize / 2) >> level;
                work = spec;
                work[0] = work[1] = 0.0f;   // via la continua
                for (int h = maxHarm + 1; h <= frameSize / 2; ++h) { work[(size_t) h * 2] = 0.0f; work[(size_t) h * 2 + 1] = 0.0f; }
                fft.performRealOnlyInverseTransform (work.data());
                float* dst = buf + (((size_t) f * numLevels + (size_t) level) * frameSize);
                for (int i = 0; i < frameSize; ++i) dst[i] = work[(size_t) i];
                if (level == 0)
                    for (int i = 0; i < frameSize; ++i) tablePeak = juce::jmax (tablePeak, std::abs (dst[i]));
            }
        }
        // picco a 0.9 su tutta la tabella (il morph non salta); quelle libere hanno gia' il loro volume
        if (normalise && tablePeak > 1.0e-4f)
            juce::FloatVectorOperations::multiply (buf, 0.9f / tablePeak, numFrames * numLevels * frameSize);
    }

    void Bank::ensure (int t)
    {
        if (t < 0 || t >= userBase() || isReady (t)) return;   // quelle dell'utente si costruiscono in setUser
        const juce::ScopedLock sl (lock);
        if (isReady (t)) return;
        auto buf = std::make_unique<float[]> ((size_t) numFrames * numLevels * frameSize);
        build (buf.get(), t, t < builtin);
        data[(size_t) t] = std::move (buf);
        ready[(size_t) t].store (true, std::memory_order_release);
    }

    //==========================================================================
    // Le tabelle dell'utente

    juce::MemoryBlock UserTable::toBlock() const
    {
        juce::MemoryOutputStream out;
        out.write ("PRUT", 4);
        out.writeInt (keys);
        out.writeString (name);
        out.write (samples.data(), (size_t) keys * frameSize * sizeof (juce::int16));
        return out.getMemoryBlock();
    }

    UserTable UserTable::fromBlock (const void* d, size_t size)
    {
        UserTable t;
        juce::MemoryInputStream in (d, size, false);
        char magic[4] {};
        if (in.read (magic, 4) != 4 || std::memcmp (magic, "PRUT", 4) != 0) return t;
        const int keys = in.readInt();
        t.name = in.readString();
        if (keys < 1 || keys > numFrames || in.getNumBytesRemaining() < (juce::int64) keys * frameSize * 2) return t;
        t.samples.resize ((size_t) keys * frameSize);
        in.read (t.samples.data(), (int) (t.samples.size() * sizeof (juce::int16)));
        t.keys = keys;
        return t;
    }

    juce::File Bank::userDir()
    {
        return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory).getChildFile ("Prisma").getChildFile ("Onde");
    }

    void Bank::setUser (int slot, UserTable table, bool saveToDisk)
    {
        if (slot < 0 || slot >= numUserTables) return;
        const juce::ScopedLock sl (lock);
        const int t = userBase() + slot;
        users[(size_t) slot] = std::move (table);
        const auto& u = users[(size_t) slot];
        if (u.empty())
            ready[(size_t) t].store (false, std::memory_order_release);   // la memoria resta: chi suona ricade sulla tabella 0
        else
        {
            const size_t count = (size_t) numFrames * numLevels * frameSize;
            auto buf = std::make_unique<float[]> (count);
            build (buf.get(), t, true);
            // la prima volta si alloca; poi si riscrive sul posto, cosi' il thread audio non perde mai la memoria che sta leggendo
            if (data[(size_t) t] == nullptr) data[(size_t) t] = std::move (buf);
            else std::memcpy (data[(size_t) t].get(), buf.get(), count * sizeof (float));
            ready[(size_t) t].store (true, std::memory_order_release);
        }
        versions[(size_t) slot].fetch_add (1);
        if (saveToDisk)
        {
            const auto file = userDir().getChildFile ("mia_" + juce::String (slot + 1) + ".prismatab");
            if (u.empty()) file.deleteFile();
            else { userDir().createDirectory(); file.replaceWithData (u.toBlock().getData(), u.toBlock().getSize()); }
        }
    }

    //==========================================================================
    // Warp spettrali: lavorano sulle armoniche (ampiezza e fase) di ogni forma

    std::shared_ptr<const SpecTable> Bank::spectral (int table, int mode)
    {
        if (mode < firstSpectralWarp || mode >= firstSpectralWarp + numSpectralWarps || table < 0 || table >= numTables())
            return {};
        ensure (table);
        const juce::ScopedLock sl (lock);
        if (! isReady (table)) table = 0;
        const int version = userVersion (table);
        for (auto it = specCache.begin(); it != specCache.end();)
        {
            auto sp = it->lock();
            if (sp == nullptr) { it = specCache.erase (it); continue; }
            if (sp->table == table && sp->mode == mode && sp->version == version) return sp;
            ++it;
        }

        auto st = std::make_shared<SpecTable>();
        st->table = table; st->mode = mode; st->version = version;
        size_t total = 0;
        for (int l = 0; l < numLevels; ++l)
        {
            st->size[l] = juce::jlimit (32, frameSize, (frameSize >> l) * 4);
            st->offset[l] = total;
            total += (size_t) st->size[l];
        }
        st->perCycle = total;
        st->data.assign ((size_t) numSpecAmounts * numFrames * total, 0.0f);

        constexpr int H = frameSize / 2;   // 512 armoniche
        juce::dsp::FFT fft (10);
        std::unique_ptr<juce::dsp::FFT> small[11];
        for (int o = 5; o <= 10; ++o) small[o] = std::make_unique<juce::dsp::FFT> (o);
        std::vector<float> spec ((size_t) frameSize * 2), work ((size_t) frameSize * 2);
        std::vector<float> mag ((size_t) H + 2), ph ((size_t) H + 2), om ((size_t) H + 2), op ((size_t) H + 2);
        juce::Random rnd (1234);
        std::vector<float> noise ((size_t) H + 2);
        for (auto& v : noise) v = rnd.nextFloat() * 2.0f - 1.0f;
        auto magAt = [&mag] (float h) { if (h < 1.0f || h > (float) H) return 0.0f; const int i = (int) h; const float f = h - (float) i; return mag[(size_t) i] + (mag[(size_t) juce::jmin (H, i + 1)] - mag[(size_t) i]) * f; };

        for (int f = 0; f < numFrames; ++f)
        {
            std::fill (spec.begin(), spec.end(), 0.0f);
            std::copy (frame (table, f, 0), frame (table, f, 0) + frameSize, spec.begin());
            fft.performRealOnlyForwardTransform (spec.data(), true);
            double srcEnergy = 0.0;
            mag[0] = ph[0] = 0.0f;
            for (int h = 1; h <= H; ++h)
            {
                const float re = spec[(size_t) h * 2], im = spec[(size_t) h * 2 + 1];
                mag[(size_t) h] = std::sqrt (re * re + im * im);
                ph[(size_t) h] = std::atan2 (im, re);
                srcEnergy += (double) mag[(size_t) h] * mag[(size_t) h];
            }
            for (int a = 0; a < numSpecAmounts; ++a)
            {
                const float amt = (float) a / (float) (numSpecAmounts - 1);
                bool keepEnergy = false;
                for (int h = 1; h <= H; ++h)
                {
                    const float fh = (float) h;
                    float m = mag[(size_t) h], p = ph[(size_t) h];
                    switch (mode - firstSpectralWarp)
                    {
                        case 0: m = magAt (1.0f + (fh - 1.0f) / (1.0f + 3.0f * amt)); keepEnergy = true; break;        // formanti su
                        case 1: m = magAt (1.0f + (fh - 1.0f) * (1.0f + 3.0f * amt)); keepEnergy = true; break;        // formanti giu'
                        case 2: { const float c = std::pow ((float) H, 1.0f - amt); if (fh > c) m *= std::exp (-(fh - c) / (0.2f * c + 0.5f)); break; }   // taglia alti
                        case 3: { const float c = std::pow (96.0f, amt); if (fh < c) m *= std::exp (-(c - fh) / (0.15f * c + 0.3f) * 2.0f); break; }      // taglia bassi
                        case 4:                                                                                         // sfoca
                        {
                            const int w = (int) std::round (amt * (2.0f + fh * 0.25f));
                            if (w > 0)
                            {
                                float sum = 0.0f; int cnt = 0;
                                for (int k = juce::jmax (1, h - w); k <= juce::jmin (H, h + w); ++k) { sum += mag[(size_t) k]; ++cnt; }
                                m = sum / (float) juce::jmax (1, cnt);
                            }
                            p += amt * noise[(size_t) h] * juce::MathConstants<float>::pi;
                            keepEnergy = true;
                            break;
                        }
                        default: p += amt * 0.12f * fh * fh; break;                                                     // disperdi le fasi
                    }
                    om[(size_t) h] = m; op[(size_t) h] = p;
                }
                if (keepEnergy)
                {
                    double e = 0.0;
                    for (int h = 1; h <= H; ++h) e += (double) om[(size_t) h] * om[(size_t) h];
                    const float gain = e > 1.0e-12 ? (float) juce::jmin (4.0, std::sqrt (srcEnergy / e)) : 1.0f;
                    for (int h = 1; h <= H; ++h) om[(size_t) h] *= gain;
                }
                for (int l = 0; l < numLevels; ++l)
                {
                    const int m = st->size[l];
                    const int maxHarm = juce::jmin (H >> l, m / 2 - 1);
                    const float scale = (float) m / (float) frameSize;
                    std::fill (work.begin(), work.begin() + (size_t) m * 2, 0.0f);
                    for (int h = 1; h <= maxHarm; ++h)
                    {
                        work[(size_t) h * 2] = om[(size_t) h] * std::cos (op[(size_t) h]) * scale;
                        work[(size_t) h * 2 + 1] = om[(size_t) h] * std::sin (op[(size_t) h]) * scale;
                    }
                    int order = 0; while ((1 << order) < m) ++order;
                    small[order]->performRealOnlyInverseTransform (work.data());
                    float* dst = st->data.data() + ((size_t) (a * numFrames + f) * total + st->offset[l]);
                    for (int i = 0; i < m; ++i) dst[i] = juce::jlimit (-1.5f, 1.5f, work[(size_t) i]);
                }
            }
        }
        specCache.push_back (st);
        return st;
    }

    //==========================================================================
    // Da un file audio a una tabella

    UserTable tableFromAudio (const float* x, int n, double sr, const juce::String& name)
    {
        UserTable t;
        t.name = name;
        if (x == nullptr || n < 256 || sr < 8000.0) return t;

        // via il silenzio in testa e in coda
        float peak = 0.0f;
        for (int i = 0; i < n; ++i) peak = juce::jmax (peak, std::abs (x[i]));
        if (peak < 1.0e-4f) return t;
        int a = 0, b = n - 1;
        while (a < b && std::abs (x[a]) < peak * 0.03f) ++a;
        while (b > a && std::abs (x[b]) < peak * 0.03f) --b;
        int len = b - a + 1;
        if (len < 512) { a = 0; len = n; }

        // intonazione: differenza media normalizzata (come YIN) su una finestra presa a un terzo del suono
        double period = 0.0;
        {
            const int minLag = juce::jmax (8, (int) (sr / 1500.0)), maxLag = juce::jmin (len / 3, (int) (sr / 35.0));
            const int win = juce::jmin (len - maxLag - 1, (int) (sr * 0.06));
            if (maxLag > minLag * 2 && win > minLag * 2)
            {
                const float* w = x + a + juce::jmax (0, juce::jmin (len - maxLag - win - 1, len / 3));
                std::vector<double> d ((size_t) maxLag + 1, 0.0);
                for (int lag = 1; lag <= maxLag; ++lag)
                {
                    double sum = 0.0;
                    for (int i = 0; i < win; ++i) { const double v = (double) w[i] - (double) w[i + lag]; sum += v * v; }
                    d[(size_t) lag] = sum;
                }
                double run = 0.0; int best = 0;
                std::vector<double> c ((size_t) maxLag + 1, 1.0);
                for (int lag = 1; lag <= maxLag; ++lag) { run += d[(size_t) lag]; c[(size_t) lag] = run > 0.0 ? d[(size_t) lag] * lag / run : 1.0; }
                for (int lag = minLag; lag < maxLag; ++lag)
                    if (c[(size_t) lag] < 0.15 && c[(size_t) lag] <= c[(size_t) lag + 1]) { best = lag; break; }
                if (best > 0)
                {
                    // minimo preciso con una parabola
                    const double y0 = c[(size_t) best - 1], y1 = c[(size_t) best], y2 = c[(size_t) best + 1];
                    const double den = y0 - 2.0 * y1 + y2;
                    period = best + (std::abs (den) > 1.0e-12 ? 0.5 * (y0 - y2) / den : 0.0);
                }
            }
        }
        const bool pitched = period > 0.0;
        if (! pitched) period = juce::jmin ((double) len * 0.5, 2048.0 * sr / 48000.0);   // rumori e batterie: fette lunghe uguali

        auto sample = [x, n] (double pos)
        {
            const int i = (int) std::floor (pos);
            const float f = (float) (pos - i);
            auto at = [x, n] (int k) { return x[juce::jlimit (0, n - 1, k)]; };
            const float y0 = at (i - 1), y1 = at (i), y2 = at (i + 1), y3 = at (i + 2);
            const float c1 = 0.5f * (y2 - y0), c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3, c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
            return ((c3 * f + c2) * f + c1) * f + y1;
        };

        std::vector<float> frames ((size_t) numFrames * frameSize);
        std::vector<float> framePeak ((size_t) numFrames, 0.0f);
        const double span = juce::jmax (0.0, (double) len - period - 2.0);
        for (int f = 0; f < numFrames; ++f)
        {
            double start = a + span * (double) f / (double) (numFrames - 1);
            if (pitched)
            {
                // parte da un passaggio per lo zero in salita: le forme restano in fase tra loro
                const int from = (int) start, lim = (int) juce::jmin ((double) (a + len - (int) period - 2), start + period);
                for (int i = from; i < lim; ++i)
                    if (x[juce::jlimit (0, n - 1, i)] <= 0.0f && x[juce::jlimit (0, n - 1, i + 1)] > 0.0f) { start = i; break; }
            }
            float* dst = frames.data() + (size_t) f * frameSize;
            double mean = 0.0;
            for (int i = 0; i < frameSize; ++i) { dst[i] = sample (start + period * (double) i / frameSize); mean += dst[i]; }
            // il ciclo deve chiudersi: si toglie la differenza tra la fine e l'inizio con una rampa
            const float gap = sample (start + period) - dst[0];
            mean /= frameSize;
            for (int i = 0; i < frameSize; ++i)
            {
                dst[i] -= (float) mean + gap * ((float) i / (float) frameSize - 0.5f);
                if (! pitched)   // fette di rumore: dissolvenza ai bordi
                    dst[i] *= juce::jmin (1.0f, (float) juce::jmin (i, frameSize - 1 - i) / 48.0f);
                framePeak[(size_t) f] = juce::jmax (framePeak[(size_t) f], std::abs (dst[i]));
            }
        }
        float maxPeak = 1.0e-6f;
        for (float v : framePeak) maxPeak = juce::jmax (maxPeak, v);
        t.samples.resize ((size_t) numFrames * frameSize);
        for (int f = 0; f < numFrames; ++f)
        {
            // le forme piu' deboli si alzano un po' (non del tutto: resta l'andamento del suono)
            const float g = framePeak[(size_t) f] > maxPeak * 0.01f ? std::pow (maxPeak / framePeak[(size_t) f], 0.7f) / maxPeak * 0.9f : 0.0f;
            for (int i = 0; i < frameSize; ++i)
                t.samples[(size_t) f * frameSize + (size_t) i] = (juce::int16) juce::jlimit (-32767.0f, 32767.0f, frames[(size_t) f * frameSize + (size_t) i] * g * 32767.0f);
        }
        t.keys = numFrames;
        return t;
    }

    Bank& Bank::get()
    {
        static Bank bank;
        return bank;
    }

    double warpPhase (int mode, float amount, double ph) noexcept
    {
        const double a = juce::jlimit (0.0f, 1.0f, amount);
        switch (mode)
        {
            case 1: { const double r = ph * (1.0 + a * 7.0); return r - std::floor (r); }
            case 2: return std::pow (ph, std::exp (a * 2.2));
            case 3: return 1.0 - std::pow (1.0 - ph, std::exp (a * 2.2));
            case 4: return ph + a * ((ph < 0.5 ? ph : 1.0 - ph) - ph);
            case 5: { const double steps = 2.0 + (1.0 - a) * 126.0; return std::floor (ph * steps) / steps; }
            case 6: { const double w = 0.5 - a * 0.47; return ph < w ? ph * 0.5 / w : 0.5 + (ph - w) * 0.5 / (1.0 - w); }
            default: return ph;
        }
    }

    float Env::next (float a, float d, float s, float r, double sr) noexcept
    {
        switch (stage)
        {
            case attack:
                value += (float) (1.0 / (juce::jmax (0.0005f, a) * sr));
                if (value >= 1.0f) { value = 1.0f; stage = decay; }
                break;
            case decay:
            {
                const float k = 1.0f - std::exp (-1.0f / (juce::jmax (0.001f, d) * 0.3f * (float) sr));
                value += (s - value) * k;
                if (std::abs (value - s) < 1.0e-4f) { value = s; stage = sustain; }
                break;
            }
            case sustain: value = s; break;
            case release:
            {
                const float k = 1.0f - std::exp (-1.0f / (juce::jmax (0.001f, r) * 0.3f * (float) sr));
                value -= value * k;
                if (value < 1.0e-4f) { value = 0.0f; stage = idle; }
                break;
            }
            case idle: default: value = 0.0f; break;
        }
        return value;
    }

    //==========================================================================
    // Parametri

    static const char* const oscIds[] { "On", "Table", "Pos", "Warp", "WarpAmt", "Oct", "Semi", "Fine", "Uni", "Detune", "Blend", "Level", "Pan" };
    enum { oOn, oTable, oPos, oWarp, oWarpAmt, oOct, oSemi, oFine, oUni, oDetune, oBlend, oLevel, oPan, numOscIds };

    static const char* const mainIds[] {
        "subOn", "subShape", "subOct", "subLevel", "noise", "noiseColor",
        "fltType", "cutoff", "res", "drive", "fltEnv", "keytrack",
        "envA", "envD", "envS", "envR", "env2A", "env2D", "env2S", "env2R",
        "l1Shape", "l1Sync", "l1Rate", "l1Retrig", "l2Shape", "l2Sync", "l2Rate", "l2Retrig",
        "voiceMode", "glide", "morph", "chorus", "dlyMix", "dlyTime", "dlyFb", "revMix", "revSize", "master" };
    enum { iSubOn, iSubShape, iSubOct, iSubLevel, iNoise, iNoiseColor, iFltType, iCutoff, iRes, iDrive, iFltEnv, iKeytrack,
           iEnvA, iEnvD, iEnvS, iEnvR, iEnv2A, iEnv2D, iEnv2S, iEnv2R,
           iL1Shape, iL1Sync, iL1Rate, iL1Retrig, iL2Shape, iL2Sync, iL2Rate, iL2Retrig,
           iVoiceMode, iGlide, iMorph, iChorus, iDlyMix, iDlyTime, iDlyFb, iRevMix, iRevSize, iMaster, numMainIds };

    juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
    {
        using namespace juce;
        std::vector<std::unique_ptr<RangedAudioParameter>> v;
        auto pct = [] (float x, int) { return String (roundToInt (x * 100.0f)) + "%"; };
        auto bip = [] (float x, int) { return (x > 0.005f ? "+" : "") + String (roundToInt (x * 100.0f)) + "%"; };
        auto secs = [] (float x, int) { return x < 1.0f ? String (roundToInt (x * 1000.0f)) + " ms" : String (x, 2) + " s"; };
        auto flt = [&v] (String id, String name, NormalisableRange<float> r, float def, std::function<String (float, int)> t)
        { v.push_back (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, r, def, AudioParameterFloatAttributes().withStringFromValueFunction (std::move (t)))); };
        auto choice = [&v] (String id, String name, StringArray o, int def) { v.push_back (std::make_unique<AudioParameterChoice> (ParameterID { id, 1 }, name, o, def)); };
        auto timeRange = [] (float lo, float hi) { NormalisableRange<float> r (lo, hi); r.setSkewForCentre (lo + (hi - lo) * 0.08f); return r; };

        for (int o = 0; o < 2; ++o)
        {
            const String P = o == 0 ? "a" : "b", N = o == 0 ? "Osc A " : "Osc B ";
            choice (P + "On", N + "acceso", { "OFF", "ON" }, o == 0 ? 1 : 0);
            choice (P + "Table", N + "tabella", tableNames(), o == 0 ? 0 : 1);
            flt (P + "Pos", N + "posizione", { 0.0f, 1.0f }, 0.0f, pct);
            choice (P + "Warp", N + "warp", { "Niente", "Sync", "Bend +", "Bend -", "Specchio", "Quantizza", "Allunga", o == 0 ? "FM da B" : "FM da A",
                                                      "Formanti su", "Formanti giu'", "Taglia alti", "Taglia bassi", "Sfoca", "Disperdi fasi" }, 0);
            flt (P + "WarpAmt", N + "quanto warp", { 0.0f, 1.0f }, 0.0f, pct);
            choice (P + "Oct", N + "ottava", { "OTT -3", "OTT -2", "OTT -1", "OTT 0", "OTT +1", "OTT +2" }, 3);
            flt (P + "Semi", N + "semitoni", { -12.0f, 12.0f, 1.0f }, 0.0f, [] (float x, int) { return (x > 0 ? "+" : "") + String (roundToInt (x)) + " st"; });
            flt (P + "Fine", N + "fine", { -100.0f, 100.0f }, 0.0f, [] (float x, int) { return (x > 0 ? "+" : "") + String (roundToInt (x)) + " ct"; });
            choice (P + "Uni", N + "unisono", { "1 voce", "2 voci", "3 voci", "4 voci", "5 voci", "7 voci" }, 0);
            flt (P + "Detune", N + "detune", { 0.0f, 1.0f }, 0.25f, pct);
            flt (P + "Blend", N + "larghezza", { 0.0f, 1.0f }, 0.7f, pct);
            flt (P + "Level", N + "volume", { 0.0f, 1.0f }, 0.75f, pct);
            flt (P + "Pan", N + "pan", { -1.0f, 1.0f }, 0.0f, bip);
        }
        choice ("subOn", "Sub acceso", { "OFF", "ON" }, 0);
        choice ("subShape", "Forma sub", { "Sinusoide", "Triangolo", "Quadra" }, 0);
        choice ("subOct", "Ottava sub", { "-1", "-2" }, 0);
        flt ("subLevel", "Volume sub", { 0.0f, 1.0f }, 0.6f, pct);
        flt ("noise", "Rumore", { 0.0f, 1.0f }, 0.0f, pct);
        flt ("noiseColor", "Colore rumore", { 0.0f, 1.0f }, 0.7f, pct);

        choice ("fltType", "Filtro", { "LP 12", "LP 24", "HP", "BP", "Notch" }, 1);
        { NormalisableRange<float> r (20.0f, 20000.0f); r.setSkewForCentre (900.0f);
          flt ("cutoff", "Cutoff", r, 20000.0f, [] (float x, int) { return x >= 1000.0f ? String (x / 1000.0f, 1) + " kHz" : String (roundToInt (x)) + " Hz"; }); }
        flt ("res", "Risonanza", { 0.0f, 1.0f }, 0.1f, pct);
        flt ("drive", "Drive", { 0.0f, 1.0f }, 0.0f, pct);
        flt ("fltEnv", "Inviluppo filtro", { -1.0f, 1.0f }, 0.0f, bip);
        flt ("keytrack", "Keytrack", { 0.0f, 1.0f }, 0.3f, pct);

        flt ("envA", "Attacco", timeRange (0.0f, 5.0f), 0.005f, secs);
        flt ("envD", "Decadimento", timeRange (0.0f, 5.0f), 0.3f, secs);
        flt ("envS", "Sustain", { 0.0f, 1.0f }, 0.8f, pct);
        flt ("envR", "Rilascio", timeRange (0.0f, 8.0f), 0.25f, secs);
        flt ("env2A", "Attacco 2", timeRange (0.0f, 5.0f), 0.005f, secs);
        flt ("env2D", "Decadimento 2", timeRange (0.0f, 5.0f), 0.4f, secs);
        flt ("env2S", "Sustain 2", { 0.0f, 1.0f }, 0.0f, pct);
        flt ("env2R", "Rilascio 2", timeRange (0.0f, 8.0f), 0.3f, secs);

        const StringArray shapes { "Sinusoide", "Triangolo", "Dente su", "Dente giu'", "Quadra", "Casuale" };
        const StringArray syncs { "Libero", "4/1", "2/1", "1/1", "1/2", "1/4", "1/8", "1/16", "1/4T", "1/8T" };
        for (int l = 1; l <= 2; ++l)
        {
            const String P = "l" + String (l), N = "LFO " + String (l) + " ";
            choice (P + "Shape", N + "forma", shapes, 0);
            choice (P + "Sync", N + "a tempo", syncs, l == 1 ? 5 : 0);
            { NormalisableRange<float> r (0.02f, 20.0f); r.setSkewForCentre (1.5f);
              flt (P + "Rate", N + "velocita'", r, 2.0f, [] (float x, int) { return String (x, x < 1.0f ? 2 : 1) + " Hz"; }); }
            choice (P + "Retrig", N + "riparte", { "No", "Si'" }, 0);
        }
        for (int s = 1; s <= numSlots; ++s)
        {
            const String P = "m" + String (s);
            choice (P + "Src", "Mod " + String (s) + " da", sourceNames(), 0);
            choice (P + "Dst", "Mod " + String (s) + " a", destNames(), 0);
            flt (P + "Amt", "Mod " + String (s) + " quanto", { -1.0f, 1.0f }, 0.5f, bip);
        }
        choice ("voiceMode", "Voci", { "Poly", "Mono", "Legato" }, 0);
        flt ("glide", "Glide", timeRange (0.0f, 1.0f), 0.0f, secs);
        flt ("morph", "Morph", { -1.0f, 1.0f }, 0.0f, bip);
        flt ("chorus", "Chorus", { 0.0f, 1.0f }, 0.0f, pct);
        flt ("dlyMix", "Delay", { 0.0f, 1.0f }, 0.0f, pct);
        choice ("dlyTime", "Tempo delay", { "1/16", "1/8", "3/16", "1/4", "3/8", "1/2" }, 2);
        flt ("dlyFb", "Feedback delay", { 0.0f, 0.9f }, 0.35f, pct);
        flt ("revMix", "Riverbero", { 0.0f, 1.0f }, 0.0f, pct);
        flt ("revSize", "Stanza", { 0.0f, 1.0f }, 0.6f, pct);
        flt ("master", "Volume", { -24.0f, 6.0f }, 0.0f, [] (float x, int) { return (x > 0.05f ? "+" : "") + String (x, 1) + " dB"; });
        return { v.begin(), v.end() };
    }

    //==========================================================================
    // Preset

    void Presets::set (const juce::String& id, float value)
    {
        if (auto* prm = apvts.getParameter (id))
            prm->setValueNotifyingHost (prm->convertTo0to1 (value));
    }

    struct Factory { const char* name; std::vector<std::pair<const char*, float>> values; };
    static const std::vector<Factory>& factory()
    {
        // Tabelle: 0 Analogico 1 Formanti 2 PWM 3 Armonici 4 Sync 5 FM 6 Wavefolder 7 Growl 8 Bitcrush 9 Organo
        // Warp: 0 niente 1 sync 2 bend+ 3 bend- 4 specchio 5 quantizza 6 allunga 7 FM | Unisono: 0..5 = 1,2,3,4,5,7 voci
        // Sorgenti: 1 env2 2 lfo1 3 lfo2 4 vel 5 nota 6 casuale 7 modwheel | Dest: 1 posA 2 posB 3 warpA 4 warpB 5 cutoff 6 res 7 pitchA 8 pitchB
        //           9 volA 10 volB 11 detune 12 pan 13 sub 14 rumore 15 drive
        // LFO sync: 0 libero 1 4/1 2 2/1 3 1/1 4 1/2 5 1/4 6 1/8 7 1/16 8 1/4T 9 1/8T | Filtro: 0 LP12 1 LP24 2 HP 3 BP 4 Notch
        static const std::vector<Factory> list
        {
            { "Init", {} },
            { "Supersaw Lead", { { "aTable", 0 }, { "aPos", 0.66f }, { "aUni", 5 }, { "aDetune", 0.35f }, { "aBlend", 0.9f },
                                 { "bOn", 1 }, { "bTable", 0 }, { "bPos", 0.66f }, { "bOct", 4 }, { "bUni", 3 }, { "bDetune", 0.2f }, { "bLevel", 0.35f },
                                 { "cutoff", 9000 }, { "res", 0.15f }, { "envA", 0.01f }, { "envR", 0.35f }, { "chorus", 0.3f }, { "revMix", 0.22f },
                                 { "dlyMix", 0.18f }, { "master", -7.0f } } },
            { "Reese Bass", { { "aTable", 0 }, { "aPos", 0.66f }, { "aOct", 1 }, { "aUni", 1 }, { "aDetune", 0.18f }, { "aBlend", 0.3f },
                              { "bOn", 1 }, { "bTable", 0 }, { "bPos", 0.66f }, { "bOct", 1 }, { "bFine", 14 }, { "bLevel", 0.7f },
                              { "subOn", 1 }, { "subLevel", 0.5f }, { "cutoff", 700 }, { "res", 0.25f }, { "drive", 0.45f },
                              { "voiceMode", 2 }, { "glide", 0.06f }, { "m1Src", 2 }, { "m1Dst", 5 }, { "m1Amt", 0.25f }, { "l1Sync", 4 }, { "master", 3.0f } } },
            { "Wobble 1/8", { { "aTable", 1 }, { "aPos", 0.0f }, { "aOct", 1 }, { "aUni", 1 }, { "subOn", 1 }, { "subLevel", 0.6f },
                              { "cutoff", 1200 }, { "res", 0.45f }, { "drive", 0.4f }, { "voiceMode", 1 }, { "l1Sync", 6 },
                              { "m1Src", 2 }, { "m1Dst", 5 }, { "m1Amt", 0.7f }, { "m2Src", 2 }, { "m2Dst", 1 }, { "m2Amt", 0.8f }, { "master", 2.0f } } },
            { "Psy Lead", { { "aTable", 4 }, { "aPos", 0.35f }, { "aUni", 2 }, { "aDetune", 0.15f }, { "cutoff", 5500 }, { "res", 0.3f },
                            { "fltEnv", 0.4f }, { "env2D", 0.25f }, { "m1Src", 1 }, { "m1Dst", 1 }, { "m1Amt", 0.45f },
                            { "dlyMix", 0.3f }, { "dlyTime", 2 }, { "dlyFb", 0.45f }, { "revMix", 0.15f }, { "master", -5.0f } } },
            { "Pluck Digitale", { { "aTable", 5 }, { "aPos", 0.2f }, { "aUni", 2 }, { "aDetune", 0.12f }, { "envA", 0.001f }, { "envD", 0.35f },
                                  { "envS", 0.0f }, { "envR", 0.3f }, { "cutoff", 2400 }, { "fltEnv", 0.55f }, { "env2D", 0.2f },
                                  { "m1Src", 1 }, { "m1Dst", 1 }, { "m1Amt", 0.5f }, { "revMix", 0.25f }, { "dlyMix", 0.2f }, { "master", -5.0f } } },
            { "Pad Formanti", { { "aTable", 1 }, { "aPos", 0.2f }, { "aUni", 4 }, { "aDetune", 0.3f }, { "aBlend", 1.0f },
                                { "bOn", 1 }, { "bTable", 3 }, { "bPos", 0.3f }, { "bOct", 4 }, { "bLevel", 0.3f },
                                { "envA", 0.9f }, { "envR", 2.0f }, { "cutoff", 5000 }, { "l1Sync", 0 }, { "l1Rate", 0.15f },
                                { "m1Src", 2 }, { "m1Dst", 1 }, { "m1Amt", 0.6f }, { "chorus", 0.5f }, { "revMix", 0.45f }, { "revSize", 0.85f }, { "master", -6.0f } } },
            { "Future Bass Chords", { { "aTable", 0 }, { "aPos", 0.66f }, { "aUni", 5 }, { "aDetune", 0.4f }, { "aBlend", 1.0f },
                                      { "bOn", 1 }, { "bTable", 2 }, { "bPos", 0.4f }, { "bUni", 2 }, { "bLevel", 0.4f },
                                      { "envA", 0.02f }, { "envR", 0.6f }, { "cutoff", 7000 }, { "chorus", 0.4f }, { "revMix", 0.3f },
                                      { "m1Src", 3 }, { "m1Dst", 9 }, { "m1Amt", -0.6f }, { "l2Shape", 3 }, { "l2Sync", 5 }, { "master", -8.0f } } },
            { "Sub Bass", { { "aTable", 0 }, { "aPos", 0.05f }, { "aOct", 2 }, { "subOn", 1 }, { "subLevel", 0.4f }, { "cutoff", 400 },
                            { "voiceMode", 2 }, { "glide", 0.04f }, { "envR", 0.15f }, { "master", 0.0f } } },
            { "Hoover Morph", { { "aTable", 2 }, { "aPos", 0.4f }, { "aUni", 5 }, { "aDetune", 0.5f }, { "aOct", 2 },
                                { "bOn", 1 }, { "bTable", 0 }, { "bPos", 0.66f }, { "bOct", 1 }, { "bUni", 3 }, { "bDetune", 0.4f }, { "bLevel", 0.5f },
                                { "voiceMode", 2 }, { "glide", 0.12f }, { "cutoff", 4500 }, { "chorus", 0.4f },
                                { "m1Src", 1 }, { "m1Dst", 7 }, { "m1Amt", -0.12f }, { "env2D", 0.4f }, { "master", 0.0f } } },
            { "Campana Organo", { { "aTable", 9 }, { "aPos", 0.8f }, { "envA", 0.002f }, { "envD", 1.2f }, { "envS", 0.2f }, { "envR", 1.0f },
                                  { "bOn", 1 }, { "bTable", 5 }, { "bPos", 0.5f }, { "bOct", 4 }, { "bLevel", 0.3f },
                                  { "revMix", 0.35f }, { "chorus", 0.2f }, { "master", -4.0f } } },
            { "Lo-fi Keys", { { "aTable", 8 }, { "aPos", 0.3f }, { "envA", 0.003f }, { "envD", 0.6f }, { "envS", 0.4f }, { "envR", 0.4f },
                              { "cutoff", 3000 }, { "res", 0.1f }, { "chorus", 0.6f }, { "revMix", 0.2f }, { "master", -4.0f } } },
            { "Trance Pluck Arp", { { "aTable", 0 }, { "aPos", 0.66f }, { "aUni", 3 }, { "aDetune", 0.25f }, { "envA", 0.001f }, { "envD", 0.25f },
                                    { "envS", 0.0f }, { "envR", 0.2f }, { "cutoff", 1200 }, { "res", 0.3f }, { "fltEnv", 0.65f }, { "env2D", 0.18f },
                                    { "dlyMix", 0.35f }, { "dlyTime", 2 }, { "dlyFb", 0.4f }, { "revMix", 0.25f }, { "master", -5.0f } } },
            { "Vocal Sweep", { { "aTable", 1 }, { "aPos", 0.0f }, { "aUni", 2 }, { "aDetune", 0.15f }, { "envA", 0.05f }, { "envR", 0.8f },
                               { "l1Sync", 3 }, { "l1Shape", 1 }, { "m1Src", 2 }, { "m1Dst", 1 }, { "m1Amt", 1.0f }, { "revMix", 0.3f },
                               { "chorus", 0.3f }, { "master", -4.0f } } },
            // --- con le tabelle libere (CC0) di Kimura Taro e WaveEdit Online
            // 10 Hardcore FM 11 Phase Dist 12 Chebyshev 13 Saw Sweep 14 OP Digitale 15 CZ Reso 16 PPG 17 Vocoder 18 Formant K
            // 19 Vocali GMa 20 Juno PWM 21 Corde 22 Aah Woh Ye 23 Parlante 24 Reso Saw 25 Supersaw W 26 Basso W
            { "Talking Bass", { { "aTable", 22 }, { "aPos", 0.0f }, { "aOct", 1 }, { "subOn", 1 }, { "subLevel", 0.5f }, { "voiceMode", 1 },
                                { "l1Sync", 5 }, { "l1Shape", 1 }, { "m1Src", 2 }, { "m1Dst", 1 }, { "m1Amt", 1.0f }, { "cutoff", 6000 }, { "master", 0.0f } } },
            { "Parlante 1/8", { { "aTable", 23 }, { "aPos", 0.2f }, { "aOct", 1 }, { "subOn", 1 }, { "subLevel", 0.5f }, { "voiceMode", 1 },
                                { "l1Sync", 6 }, { "l1Shape", 0 }, { "m1Src", 2 }, { "m1Dst", 1 }, { "m1Amt", 0.8f }, { "master", 0.0f } } },
            { "Reso Saw Bass", { { "aTable", 24 }, { "aPos", 0.3f }, { "aOct", 1 }, { "aUni", 1 }, { "aDetune", 0.1f }, { "subOn", 1 }, { "subLevel", 0.45f },
                                 { "voiceMode", 2 }, { "glide", 0.05f }, { "env2D", 0.35f }, { "m1Src", 1 }, { "m1Dst", 1 }, { "m1Amt", 0.5f },
                                 { "cutoff", 5000 }, { "master", 0.0f } } },
            { "Basso W Pluck", { { "aTable", 26 }, { "aPos", 0.1f }, { "aOct", 1 }, { "envA", 0.001f }, { "envD", 0.4f }, { "envS", 0.3f }, { "envR", 0.1f },
                                 { "env2D", 0.25f }, { "m1Src", 1 }, { "m1Dst", 1 }, { "m1Amt", 0.6f }, { "voiceMode", 1 }, { "master", 3.0f } } },
            { "Hardcore FM Stab", { { "aTable", 10 }, { "aPos", 0.15f }, { "aUni", 2 }, { "aDetune", 0.15f }, { "envA", 0.001f }, { "envD", 0.3f },
                                    { "envS", 0.2f }, { "envR", 0.2f }, { "env2D", 0.3f }, { "m1Src", 1 }, { "m1Dst", 1 }, { "m1Amt", 0.55f },
                                    { "revMix", 0.15f }, { "master", -4.0f } } },
            { "CZ Acid", { { "aTable", 15 }, { "aPos", 0.1f }, { "aOct", 2 }, { "voiceMode", 2 }, { "glide", 0.07f }, { "envA", 0.001f },
                           { "envD", 0.3f }, { "envS", 0.6f }, { "env2D", 0.2f }, { "m1Src", 1 }, { "m1Dst", 1 }, { "m1Amt", 0.7f },
                           { "m2Src", 4 }, { "m2Dst", 1 }, { "m2Amt", 0.25f }, { "dlyMix", 0.2f }, { "dlyTime", 2 }, { "master", 4.0f } } },
            { "PPG Pad", { { "aTable", 16 }, { "aPos", 0.3f }, { "aUni", 3 }, { "aDetune", 0.2f }, { "aBlend", 1.0f }, { "envA", 0.6f }, { "envR", 1.5f },
                           { "l1Sync", 0 }, { "l1Rate", 0.12f }, { "m1Src", 2 }, { "m1Dst", 1 }, { "m1Amt", 0.35f }, { "chorus", 0.5f },
                           { "revMix", 0.4f }, { "revSize", 0.8f }, { "master", -6.0f } } },
            { "Juno Strings", { { "aTable", 20 }, { "aPos", 0.4f }, { "aUni", 3 }, { "aDetune", 0.18f }, { "aBlend", 1.0f }, { "envA", 0.35f },
                                { "envR", 0.9f }, { "l1Sync", 0 }, { "l1Rate", 0.6f }, { "m1Src", 2 }, { "m1Dst", 1 }, { "m1Amt", 0.3f },
                                { "cutoff", 6000 }, { "chorus", 0.7f }, { "revMix", 0.3f }, { "master", -6.0f } } },
            { "Vocoder Lead", { { "aTable", 17 }, { "aPos", 0.3f }, { "aUni", 2 }, { "aDetune", 0.12f }, { "envA", 0.02f }, { "envR", 0.4f },
                                { "voiceMode", 2 }, { "glide", 0.06f }, { "l1Sync", 4 }, { "l1Shape", 1 }, { "m1Src", 2 }, { "m1Dst", 1 }, { "m1Amt", 0.4f },
                                { "dlyMix", 0.25f }, { "revMix", 0.2f }, { "master", 0.0f } } },
            { "Corde Pluck", { { "aTable", 21 }, { "aPos", 0.0f }, { "envA", 0.001f }, { "envD", 0.8f }, { "envS", 0.0f }, { "envR", 0.6f },
                               { "env2D", 0.6f }, { "m1Src", 1 }, { "m1Dst", 1 }, { "m1Amt", 0.8f }, { "revMix", 0.25f }, { "master", -1.0f } } },
            { "Anthem Supersaw W", { { "aTable", 25 }, { "aPos", 0.5f }, { "aUni", 5 }, { "aDetune", 0.3f }, { "aBlend", 1.0f },
                                     { "envA", 0.01f }, { "envR", 0.5f }, { "chorus", 0.3f }, { "revMix", 0.3f }, { "dlyMix", 0.15f }, { "master", -7.0f } } },
            { "Formant Choir", { { "aTable", 19 }, { "aPos", 0.2f }, { "aUni", 4 }, { "aDetune", 0.2f }, { "aBlend", 1.0f }, { "envA", 0.8f },
                                 { "envR", 2.0f }, { "l1Sync", 0 }, { "l1Rate", 0.08f }, { "m1Src", 2 }, { "m1Dst", 1 }, { "m1Amt", 0.7f },
                                 { "revMix", 0.5f }, { "revSize", 0.9f }, { "master", -1.0f } } },
            { "Sync Screamer", { { "aTable", 4 }, { "aPos", 0.2f }, { "aWarp", 1 }, { "aWarpAmt", 0.0f }, { "aUni", 2 }, { "aDetune", 0.15f },
                                 { "voiceMode", 2 }, { "glide", 0.08f }, { "m1Src", 1 }, { "m1Dst", 3 }, { "m1Amt", 0.7f }, { "env2D", 0.6f },
                                 { "m2Src", 7 }, { "m2Dst", 1 }, { "m2Amt", 0.8f }, { "drive", 0.35f }, { "dlyMix", 0.25f }, { "master", 1.0f } } },
        };
        return list;
    }

    int Presets::numFactory()                    { return (int) factory().size(); }
    juce::String Presets::factoryName (int i)    { return factory()[(size_t) juce::jlimit (0, numFactory() - 1, i)].name; }

    void Presets::loadFactory (int i)
    {
        const auto& f = factory()[(size_t) juce::jlimit (0, numFactory() - 1, i)];
        for (auto* prm : apvts.processor.getParameters())
            if (auto* r = dynamic_cast<juce::RangedAudioParameter*> (prm))
                r->setValueNotifyingHost (r->getDefaultValue());
        for (auto& [id, value] : f.values) set (id, value);
        apvts.state.setProperty ("presetName", f.name, nullptr);
    }

    juce::File Presets::userDir() const
    {
        return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory).getChildFile ("Prisma").getChildFile ("Presets");
    }
    juce::Array<juce::File> Presets::userPresets() const
    {
        auto files = userDir().findChildFiles (juce::File::findFiles, false, "*.prismapreset");
        files.sort();
        return files;
    }
    bool Presets::save (const juce::String& name)
    {
        const auto clean = juce::File::createLegalFileName (name.trim());
        if (clean.isEmpty()) return false;
        userDir().createDirectory();
        apvts.state.setProperty ("presetName", clean, nullptr);
        if (auto xml = apvts.copyState().createXml()) return xml->writeTo (userDir().getChildFile (clean + ".prismapreset"));
        return false;
    }
    bool Presets::load (const juce::File& file)
    {
        auto xml = juce::XmlDocument::parse (file);
        if (xml == nullptr || ! xml->hasTagName (apvts.state.getType())) return false;
        apvts.replaceState (juce::ValueTree::fromXml (*xml));
        apvts.state.setProperty ("presetName", file.getFileNameWithoutExtension(), nullptr);
        return true;
    }
}

//==============================================================================
struct PrismaAudioProcessor::Block
{
    struct Osc { bool on; int table, warp, oct, uni; float pos, warpAmt, semi, fine, detune, blend, level, pan; };
    Osc osc[2];
    const prisma::SpecTable* spec[2] { nullptr, nullptr };   // warp spettrale pronto (o niente)
    bool subOn; int subShape, subOct; float subLevel, noise, noiseColor;
    int fltType; float cutoff, res, drive, fltEnv, keytrack;
    float env[4], env2[4];
    int lfoShape[2], lfoSync[2]; float lfoRate[2]; bool lfoRetrig[2];
    int src[prisma::numSlots], dst[prisma::numSlots]; float amt[prisma::numSlots];
    int voiceMode; float glide, morph;
    forge::FxParams fx;
};

PrismaAudioProcessor::PrismaAudioProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "Prisma", prisma::createLayout())
{
    prisma::Bank::get();   // le tabelle si costruiscono qui (thread della UI), non nel thread audio
    for (int o = 0; o < 2; ++o)
        for (auto* id : prisma::oscIds) p.push_back (apvts.getRawParameterValue (juce::String (o == 0 ? "a" : "b") + id));
    for (auto* id : prisma::mainIds) p.push_back (apvts.getRawParameterValue (id));
    for (int s = 1; s <= prisma::numSlots; ++s)
        for (auto* suffix : { "Src", "Dst", "Amt" })
            p.push_back (apvts.getRawParameterValue ("m" + juce::String (s) + suffix));
    for (auto* x : p) jassert (x != nullptr);
    for (auto* id : { "aTable", "bTable", "aWarp", "bWarp" })
        apvts.addParameterListener (id, this);
    presets.loadFactory (0);
    ensureTables();
    updateSpectral();
}

PrismaAudioProcessor::~PrismaAudioProcessor()
{
    cancelPendingUpdate();
    for (auto* id : { "aTable", "bTable", "aWarp", "bWarp" })
        apvts.removeParameterListener (id, this);
}

/** Le tabelle scelte si preparano subito (di solito dal thread della UI: preset, scelta della tabella). */
void PrismaAudioProcessor::ensureTables()
{
    for (auto* id : { "aTable", "bTable" })
        prisma::Bank::get().ensure ((int) apvts.getRawParameterValue (id)->load());
}

void PrismaAudioProcessor::parameterChanged (const juce::String&, float)
{
    ensureTables();
    // i warp spettrali si calcolano (qualche decimo di secondo): mai nel thread audio
    if (juce::MessageManager::existsAndIsCurrentThread()) updateSpectral();
    else triggerAsyncUpdate();
}

void PrismaAudioProcessor::updateSpectral()
{
    auto& bank = prisma::Bank::get();
    const juce::ScopedLock sl (specLock);
    const auto now = juce::Time::getMillisecondCounter();
    for (int o = 0; o < 2; ++o)
    {
        const int table = (int) apvts.getRawParameterValue (o == 0 ? "aTable" : "bTable")->load();
        const int warp = (int) apvts.getRawParameterValue (o == 0 ? "aWarp" : "bWarp")->load();
        const bool spectral = warp >= prisma::firstSpectralWarp && bank.isReady (table);
        auto& held = specHeld[o];
        if (! spectral)
        {
            if (held != nullptr) { specNow[o].store (nullptr); specRetired.push_back ({ std::move (held), now }); held = nullptr; }
            continue;
        }
        if (held != nullptr && held->table == table && held->mode == warp && held->version == bank.userVersion (table))
            continue;
        auto fresh = bank.spectral (table, warp);
        specNow[o].store (fresh.get());
        if (held != nullptr) specRetired.push_back ({ std::move (held), now });
        held = std::move (fresh);
    }
    // le tabelle sostituite restano vive qualche secondo: il thread audio potrebbe stare ancora leggendo
    specRetired.erase (std::remove_if (specRetired.begin(), specRetired.end(),
                                       [now] (const auto& r) { return now - r.second > 4000u; }), specRetired.end());
}

void PrismaAudioProcessor::readBlock (Block& b)
{
    using namespace prisma;
    auto P = [this] (int i) { return p[(size_t) i]->load(); };
    static constexpr int unisonCounts[] { 1, 2, 3, 4, 5, 7 };
    for (int o = 0; o < 2; ++o)
    {
        const int base = o * numOscIds;
        auto& s = b.osc[o];
        s.on = P (base + oOn) > 0.5f; s.table = (int) P (base + oTable); s.pos = P (base + oPos); s.warp = (int) P (base + oWarp);
        s.warpAmt = P (base + oWarpAmt); s.oct = (int) P (base + oOct) - 3; s.semi = P (base + oSemi); s.fine = P (base + oFine);
        s.uni = unisonCounts[juce::jlimit (0, 5, (int) P (base + oUni))]; s.detune = P (base + oDetune); s.blend = P (base + oBlend);
        s.level = P (base + oLevel); s.pan = P (base + oPan);
        const auto* sp = s.warp >= firstSpectralWarp ? specNow[o].load() : nullptr;
        b.spec[o] = sp != nullptr && sp->mode == s.warp && sp->table == s.table ? sp : nullptr;
    }
    const int m = 2 * numOscIds;
    auto M = [&] (int i) { return P (m + i); };
    b.subOn = M (iSubOn) > 0.5f; b.subShape = (int) M (iSubShape); b.subOct = (int) M (iSubOct) + 1; b.subLevel = M (iSubLevel);
    b.noise = M (iNoise); b.noiseColor = M (iNoiseColor);
    b.fltType = (int) M (iFltType); b.cutoff = M (iCutoff); b.res = M (iRes); b.drive = M (iDrive); b.fltEnv = M (iFltEnv); b.keytrack = M (iKeytrack);
    for (int k = 0; k < 4; ++k) { b.env[k] = M (iEnvA + k); b.env2[k] = M (iEnv2A + k); }
    for (int l = 0; l < 2; ++l)
    {
        const int base = l == 0 ? iL1Shape : iL2Shape;
        b.lfoShape[l] = (int) M (base); b.lfoSync[l] = (int) M (base + 1); b.lfoRate[l] = M (base + 2); b.lfoRetrig[l] = M (base + 3) > 0.5f;
    }
    b.voiceMode = (int) M (iVoiceMode); b.glide = M (iGlide); b.morph = M (iMorph);
    static constexpr double delayBeats[] { 0.25, 0.5, 0.75, 1.0, 1.5, 2.0 };
    b.fx.chorus = M (iChorus); b.fx.delayMix = M (iDlyMix); b.fx.delayBeats = delayBeats[juce::jlimit (0, 5, (int) M (iDlyTime))];
    b.fx.delayFeedback = M (iDlyFb); b.fx.reverbMix = M (iRevMix); b.fx.reverbSize = M (iRevSize);
    b.fx.gain = juce::Decibels::decibelsToGain (M (iMaster));
    const int mm = m + numMainIds;
    for (int s = 0; s < numSlots; ++s)
    {
        b.src[s] = (int) P (mm + s * 3); b.dst[s] = (int) P (mm + s * 3 + 1); b.amt[s] = P (mm + s * 3 + 2);
    }
}

void PrismaAudioProcessor::prepareToPlay (double sampleRate, int)
{
    sr = sampleRate;
    fx.prepare (sr);
    reset();
}

void PrismaAudioProcessor::reset()
{
    for (auto& v : voices) v = {};
    held.clear();
    fx.reset();
    shownPos[0].store (-1.0f); shownPos[1].store (-1.0f);
}

void PrismaAudioProcessor::noteOn (int note, float velocity, const Block& b)
{
    using namespace prisma;
    const bool anyHeld = ! held.empty();
    held.push_back (note);
    for (int l = 0; l < 2; ++l)
        if (b.lfoRetrig[l] && ! anyHeld) lfo[l].phase = 0.0;

    auto start = [&] (Voice& v, bool retrigger)
    {
        const bool wasActive = v.active;
        v.note = note;
        v.velocity = velocity;
        v.targetPitch = note;
        if (! wasActive || b.glide <= 0.0005f) v.pitch = note;
        if (retrigger || ! wasActive)
        {
            v.active = true;
            v.random = rng.nextFloat() * 2.0f - 1.0f;
            if (! wasActive)
            {
                for (auto& o : v.phase) for (auto& ph : o) ph = rng.nextDouble();   // unisono: fasi a caso (suono piu' largo)
                v.svf = {};
                v.amp.value = 0.0f;
            }
            v.amp.start();
            v.mod.start();
            v.age = ++ageCounter;
        }
    };

    if (b.voiceMode != 0)   // mono / legato: una voce sola
    {
        auto& v = voices[0];
        start (v, b.voiceMode == 1 || ! anyHeld || v.amp.stage == Env::release || ! v.active);
        return;
    }
    // poly: voce libera, altrimenti quella piu' vecchia
    Voice* pick = nullptr;
    for (auto& v : voices) if (! v.active) { pick = &v; break; }
    if (pick == nullptr)
    {
        pick = &voices[0];
        for (auto& v : voices) if (v.age < pick->age) pick = &v;
        pick->active = false;   // riparte da zero
    }
    start (*pick, true);
}

void PrismaAudioProcessor::noteOff (int note, const Block& b)
{
    held.erase (std::remove (held.begin(), held.end(), note), held.end());
    if (b.voiceMode != 0)
    {
        auto& v = voices[0];
        if (! held.empty())
        {
            // mono/legato: si torna all'ultima nota ancora premuta
            v.note = held.back();
            v.targetPitch = held.back();
            if (b.voiceMode == 1) { v.amp.start(); v.mod.start(); }
        }
        else { v.amp.stop(); v.mod.stop(); }
        return;
    }
    for (auto& v : voices)
        if (v.active && v.note == note && v.amp.stage != prisma::Env::release) { v.amp.stop(); v.mod.stop(); }
}

void PrismaAudioProcessor::render (float* L, float* R, int n, const Block& b, double bpm, double ppq)
{
    using namespace prisma;
    const auto& bank = Bank::get();
    constexpr int chunk = 16;
    static constexpr double syncBeats[] { 0.0, 16.0, 8.0, 4.0, 2.0, 1.0, 0.5, 0.25, 2.0 / 3.0, 1.0 / 3.0 };
    auto mtof = [] (double m) { return 440.0 * std::pow (2.0, (m - 69.0) / 12.0); };
    auto lfoValue = [] (int shape, double ph, float hold)
    {
        switch (shape)
        {
            case 0:  return (float) std::sin (ph * juce::MathConstants<double>::twoPi);
            case 1:  return (float) (ph < 0.25 ? 4.0 * ph : ph < 0.75 ? 2.0 - 4.0 * ph : 4.0 * ph - 4.0);
            case 2:  return (float) (2.0 * ph - 1.0);
            case 3:  return (float) (1.0 - 2.0 * ph);
            case 4:  return ph < 0.5 ? 1.0f : -1.0f;
            default: return hold;
        }
    };
    const float voiceGain = b.voiceMode != 0 ? 0.8f : 0.4f;   // mono/legato: una voce sola, un po' piu' forte
    const float glideCoef = b.glide <= 0.0005f ? 1.0f : 1.0f - std::exp (-1.0f / (b.glide * 0.35f * (float) sr));
    float shownA = -1.0f, shownB = -1.0f;
    juce::uint32 newest = 0;

    for (int start = 0; start < n; start += chunk)
    {
        const int len = juce::jmin (chunk, n - start);
        const double chunkSec = len / sr;

        // --- LFO (uguali per tutte le voci)
        float lfoNow[2];
        for (int l = 0; l < 2; ++l)
        {
            auto& st = lfo[l];
            const double pos = ppq >= 0.0 ? ppq + bpm / 60.0 * start / sr : -1.0;
            double rate = b.lfoRate[l];
            // velocita' LFO modulata dalle sorgenti globali (l'altro LFO, la mod wheel)
            for (int s = 0; s < numSlots; ++s)
                if (b.dst[s] == (l == 0 ? dstLfo1Rate : dstLfo2Rate))
                {
                    const float srcV = b.src[s] == srcLfo1 ? lfo[0].value : b.src[s] == srcLfo2 ? lfo[1].value : b.src[s] == srcModWheel ? modWheel : 0.0f;
                    rate *= std::pow (4.0, (double) (b.amt[s] * srcV));
                }
            double newPhase;
            if (b.lfoSync[l] > 0)
            {
                const double beats = syncBeats[juce::jlimit (1, 9, b.lfoSync[l])];
                if (pos >= 0.0) newPhase = std::fmod (pos / beats, 1.0);
                else newPhase = st.phase + chunkSec * bpm / 60.0 / beats;
            }
            else newPhase = st.phase + chunkSec * rate;
            if (newPhase >= 1.0 || newPhase < st.phase) st.hold = rng.nextFloat() * 2.0f - 1.0f;   // nuovo giro: nuovo valore casuale
            st.phase = newPhase - std::floor (newPhase);
            st.value = lfoValue (b.lfoShape[l], st.phase, st.hold);
            lfoNow[l] = st.value;
        }

        for (auto& v : voices)
        {
            if (! v.active) continue;
            // --- modulazioni della voce (una volta ogni 16 campioni)
            const float env2 = v.mod.value;
            float src[numSources] { 0.0f, env2, lfoNow[0], lfoNow[1], v.velocity, (float) ((v.note - 60) / 24.0), v.random, modWheel };
            float mod[numDests] {};
            for (int s = 0; s < numSlots; ++s)
                if (b.src[s] > 0 && b.dst[s] > 0)
                    mod[b.dst[s]] += src[juce::jlimit (0, numSources - 1, b.src[s])] * b.amt[s];

            v.pitch += (v.targetPitch - v.pitch) * (1.0 - std::pow (1.0 - (double) glideCoef, (double) len));

            struct OscNow { bool on; int table, warp, level, uni; const SpecTable* spec; float pos, warpAmt, gain; double inc[maxUnison]; float gl[maxUnison], gr[maxUnison]; };
            OscNow o[2];
            for (int k = 0; k < 2; ++k)
            {
                const auto& s = b.osc[k];
                auto& on = o[k];
                on.on = s.on;
                if (! s.on) continue;
                on.table = s.table; on.warp = s.warp; on.uni = s.uni; on.spec = b.spec[k];
                on.pos = juce::jlimit (0.0f, 1.0f, s.pos + b.morph + mod[k == 0 ? dstPosA : dstPosB]);
                on.warpAmt = juce::jlimit (0.0f, 1.0f, s.warpAmt + mod[k == 0 ? dstWarpA : dstWarpB]);
                on.gain = juce::jlimit (0.0f, 1.5f, s.level * (1.0f + mod[k == 0 ? dstLevelA : dstLevelB])) / std::sqrt ((float) s.uni);
                const double midi = v.pitch + s.oct * 12 + s.semi + s.fine / 100.0 + (pitchBend * 2.0 + slideHere (start)) + mod[k == 0 ? dstPitchA : dstPitchB] * 24.0;
                const double f = mtof (midi);
                const float detune = juce::jlimit (0.0f, 1.0f, s.detune + mod[dstDetune]);
                double maxF = f;
                for (int u = 0; u < s.uni; ++u)
                {
                    const float spread = s.uni == 1 ? 0.0f : (float) u / (float) (s.uni - 1) * 2.0f - 1.0f;   // -1..1
                    const double fu = f * std::pow (2.0, spread * detune * 0.5 / 12.0);   // fino a +-50 cent
                    on.inc[u] = fu / sr;
                    maxF = juce::jmax (maxF, fu);
                    const float panU = juce::jlimit (-1.0f, 1.0f, spread * s.blend + s.pan + mod[dstPan]);
                    on.gl[u] = std::cos ((panU + 1.0f) * juce::MathConstants<float>::pi * 0.25f) * on.gain;
                    on.gr[u] = std::sin ((panU + 1.0f) * juce::MathConstants<float>::pi * 0.25f) * on.gain;
                }
                // con il warp (sync, quantizza...) servono meno armonici
                const double warpMul = s.warp == 1 ? 1.0 + on.warpAmt * 7.0 : s.warp >= 2 && s.warp <= 6 ? 1.0 + on.warpAmt * 2.0 : 1.0;
                on.level = Bank::levelFor (maxF * warpMul, sr);
            }
            if (v.age >= newest) { newest = v.age; shownA = o[0].on ? o[0].pos : -1.0f; shownB = o[1].on ? o[1].pos : -1.0f; }

            // sub e rumore
            const float subGain = b.subOn ? juce::jlimit (0.0f, 1.5f, b.subLevel + mod[dstSub]) * 0.7f : 0.0f;
            const double subInc = mtof (v.pitch - 12.0 * b.subOct + (pitchBend * 2.0 + slideHere (start))) / sr;
            const float noiseGain = juce::jlimit (0.0f, 1.0f, b.noise + mod[dstNoise]) * 0.4f;
            const float noiseK = 0.05f + 0.95f * b.noiseColor;

            // filtro
            const double cutoffHz = juce::jlimit (20.0, sr * 0.45, (double) b.cutoff * std::pow (2.0, b.fltEnv * env2 * 7.0
                                                  + b.keytrack * (v.note - 60) / 12.0 + mod[dstCutoff] * 6.0));
            const float res = juce::jlimit (0.0f, 1.0f, b.res + mod[dstRes]);
            const float g = (float) std::tan (juce::MathConstants<double>::pi * cutoffHz / sr);
            const float kq = 2.0f - 1.96f * res;
            const float a1 = 1.0f / (1.0f + g * (g + kq)), a2 = g * a1, a3 = g * a2;
            const float drive = juce::jlimit (0.0f, 1.0f, b.drive + mod[dstDrive]);
            const float driveGain = 1.0f + drive * 9.0f, driveNorm = 1.0f / (1.0f + drive * 2.5f);
            const float velGain = 0.45f + 0.55f * v.velocity;

            for (int i = start; i < start + len; ++i)
            {
                float l = 0.0f, r = 0.0f;
                float outA = 0.0f, outB = 0.0f;
                for (int k = 0; k < 2; ++k)
                {
                    auto& on = o[k];
                    if (! on.on) continue;
                    float sum = 0.0f;
                    const float other = k == 0 ? v.lastB : v.lastA;
                    for (int u = 0; u < on.uni; ++u)
                    {
                        double& ph = v.phase[(size_t) k][(size_t) u];
                        float s;
                        if (on.spec != nullptr) s = on.spec->read (on.warpAmt, on.pos, ph, on.level);
                        else
                        {
                            double rp;
                            if (on.warp == 7) { rp = ph + on.warpAmt * 0.5 * other; rp -= std::floor (rp); }
                            else rp = warpPhase (on.warp, on.warpAmt, ph);
                            s = bank.read (on.table, on.pos, rp, on.level);
                        }
                        l += s * on.gl[u];
                        r += s * on.gr[u];
                        if (u == 0) sum = s;
                        ph += on.inc[u];
                        if (ph >= 1.0) ph -= 1.0;
                    }
                    (k == 0 ? outA : outB) = sum;
                }
                v.lastA = outA; v.lastB = outB;
                if (subGain > 0.0f)
                {
                    v.subPhase += subInc;
                    if (v.subPhase >= 1.0) v.subPhase -= 1.0;
                    const double sp = v.subPhase;
                    const float s = b.subShape == 0 ? (float) std::sin (sp * juce::MathConstants<double>::twoPi)
                                  : b.subShape == 1 ? (float) (sp < 0.25 ? 4.0 * sp : sp < 0.75 ? 2.0 - 4.0 * sp : 4.0 * sp - 4.0)
                                                    : (sp < 0.5 ? 0.8f : -0.8f);
                    l += s * subGain; r += s * subGain;
                }
                if (noiseGain > 0.0f)
                {
                    const float wn = rng.nextFloat() * 2.0f - 1.0f;
                    v.noiseLp += (wn - v.noiseLp) * noiseK;
                    l += v.noiseLp * noiseGain;
                    r += v.noiseLp * noiseGain * 0.9f + wn * noiseGain * 0.1f * noiseK;
                }

                // drive + filtro SVF (TPT), 2 stadi per il 24 dB
                if (drive > 0.001f) { l = std::tanh (l * driveGain) * driveNorm; r = std::tanh (r * driveGain) * driveNorm; }
                auto svf = [&] (float x, float& ic1, float& ic2, float& hp, float& bp)
                {
                    const float v3 = x - ic2;
                    const float v1 = a1 * ic1 + a2 * v3;
                    const float v2 = ic2 + a2 * ic1 + a3 * v3;
                    ic1 = 2.0f * v1 - ic1;
                    ic2 = 2.0f * v2 - ic2;
                    bp = v1;
                    hp = x - kq * v1 - v2;
                    return v2;   // passa-basso
                };
                float ch[2] { l, r };
                for (int c = 0; c < 2; ++c)
                {
                    float hp, bp;
                    auto& st = v.svf[(size_t) c];
                    float lp = svf (ch[c], st[0], st[1], hp, bp);
                    float y;
                    switch (b.fltType)
                    {
                        case 1:  { float hp2, bp2; auto& st2 = v.svf[(size_t) (2 + c)]; y = svf (lp, st2[0], st2[1], hp2, bp2); break; }
                        case 2:  y = hp; break;
                        case 3:  y = bp * kq; break;
                        case 4:  y = lp + hp; break;
                        default: y = lp; break;
                    }
                    ch[c] = y;
                }

                const float ampEnv = v.amp.next (b.env[0], b.env[1], b.env[2], b.env[3], sr);
                if (i == start) v.mod.next (b.env2[0], b.env2[1], b.env2[2], b.env2[3], sr / len);   // inviluppo 2 a "control rate"
                const float gOut = ampEnv * velGain * voiceGain;
                L[i] += ch[0] * gOut;
                R[i] += ch[1] * gOut;
                if (v.amp.stage == Env::idle) { v.active = false; break; }
            }
        }
    }
    shownPos[0].store (shownA, std::memory_order_relaxed);
    shownPos[1].store (shownB, std::memory_order_relaxed);

    fx.process (L, R, n, b.fx, bpm);
    for (int i = 0; i < n; ++i)
    {
        auto soft = [] (float x) { const float a = std::abs (x); return a < 0.85f ? x : std::copysign (0.85f + 0.15f * std::tanh ((a - 0.85f) * 6.0f), x); };
        L[i] = soft (L[i]);
        if (R != L) R[i] = soft (R[i]);
    }
}

void PrismaAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals nd;
    buffer.clear();
    double bpm = 128.0, ppq = -1.0;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            if (auto bp = pos->getBpm()) bpm = *bp;
            if (pos->getIsPlaying()) if (auto q = pos->getPpqPosition()) ppq = *q;
        }
    Block b;
    readBlock (b);
    auto* L = buffer.getWritePointer (0);
    auto* R = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : L;
    beginSlideBlock (buffer.getNumSamples(), sr);
    int done = 0;
    for (const auto meta : midi)
    {
        const int at = juce::jlimit (0, buffer.getNumSamples(), meta.samplePosition);
        slideBase = done;
        render (L + done, R + done, at - done, b, bpm, ppq >= 0.0 ? ppq + bpm / 60.0 * done / sr : -1.0);
        done = at;
        const auto m = meta.getMessage();
        if (m.isNoteOn()) noteOn (m.getNoteNumber(), m.getFloatVelocity(), b);
        else if (m.isNoteOff()) noteOff (m.getNoteNumber(), b);
        else if (m.isPitchWheel()) pitchBend = (float) (m.getPitchWheelValue() - 8192) / 8192.0f;
        else if (m.isControllerOfType (1)) modWheel = (float) m.getControllerValue() / 127.0f;
        else if (m.isAllNotesOff() || m.isAllSoundOff())
        {
            held.clear();
            for (auto& v : voices) { v.amp.stop(); v.mod.stop(); }
        }
    }
    slideBase = done;
    render (L + done, R + done, buffer.getNumSamples() - done, b, bpm, ppq >= 0.0 ? ppq + bpm / 60.0 * done / sr : -1.0);
}

juce::AudioProcessorEditor* PrismaAudioProcessor::createEditor()   { return new PrismaEditor (*this); }

void PrismaAudioProcessor::getStateInformation (juce::MemoryBlock& d)
{
    if (auto xml = apvts.copyState().createXml())
    {
        // le tabelle "Mie" usate viaggiano col progetto: su un altro dispositivo il suono resta lo stesso
        auto& bank = prisma::Bank::get();
        for (int slot = 0; slot < prisma::numUserTables; ++slot)
        {
            bool used = false;
            for (auto* id : { "aTable", "bTable" })
                used = used || (int) apvts.getRawParameterValue (id)->load() == bank.userBase() + slot;
            if (used && ! bank.user (slot).empty())
            {
                auto* e = xml->createNewChildElement ("UserTable");
                e->setAttribute ("slot", slot);
                e->setAttribute ("data", bank.user (slot).toBlock().toBase64Encoding());
            }
        }
        copyXmlToBinary (*xml, d);
    }
}

void PrismaAudioProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto& bank = prisma::Bank::get();
            while (auto* e = xml->getChildByName ("UserTable"))
            {
                juce::MemoryBlock mb;
                const int slot = e->getIntAttribute ("slot", -1);
                if (slot >= 0 && slot < prisma::numUserTables && mb.fromBase64Encoding (e->getStringAttribute ("data")))
                    if (auto t = prisma::UserTable::fromBlock (mb.getData(), mb.getSize()); ! t.empty() && ! (t == bank.user (slot)))
                    {
                        const bool wasEmpty = bank.user (slot).empty();
                        bank.setUser (slot, std::move (t), wasEmpty);   // se qui lo spazio era libero, la tabella resta anche su questo dispositivo
                    }
                xml->removeChildElement (e, true);
            }
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
            ensureTables();
            updateSpectral();
        }
}
