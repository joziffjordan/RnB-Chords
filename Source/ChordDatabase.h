#pragma once
#include <JuceHeader.h>
#include <algorithm>
#include <cmath>
#include <map>
#include <vector>

// Loads the progression database (Resources/rnb_chords_db.json) that is baked into the plugin.
struct Chord
{
    int root = 0;          // semitones above the tonic
    int bass = -1;         // semitones above the tonic, -1 = same as root
    int beats = 4;
    juce::String quality, symbol;
};

// One chord turned into MIDI notes. notes[0] is the bass note, the rest are the upper notes, low to high.
struct Voiced
{
    int notes[8] {};
    int count = 0;
    int inversion = 0;
};

enum VoicingMode { voicingRoot = 0, voicingClosest = 1, voicingInversion = 2 };

struct Progression
{
    juce::String id, era, style, name, mode, roman, songs;
    std::vector<Chord> chords;
    int tempoLo = 80, tempoHi = 100;
};

class ChordDatabase
{
public:
    static constexpr int numEras = 5;
    static constexpr int maxChords = 16;

    ChordDatabase()
    {
        auto text = juce::String::fromUTF8 (BinaryData::rnb_chords_db_json, BinaryData::rnb_chords_db_jsonSize);
        auto root = juce::JSON::parse (text);

        if (auto* q = root["qualities"].getDynamicObject())
            for (auto& p : q->getProperties())
            {
                std::vector<int> iv;
                if (auto* arr = p.value.getArray())
                    for (auto& v : *arr) iv.push_back ((int) v);
                qualities[p.name.toString()] = iv;
            }

        if (auto* eras = root["eras"].getArray())
            for (auto& e : *eras) eraNames.add (e.toString());

        auto trig = root["midi_trigger"];
        readStrings (trig["white_keys"]["degrees"], whiteLabels);
        readStrings (trig["black_keys"]["degrees"], blackLabels);
        for (int e = 0; e < eraNames.size() && e < numEras; ++e)
        {
            auto perEra = trig["quality_by_era"][juce::Identifier (eraNames[e])];
            readStrings (perEra["white"], whiteQuality[e]);
            readStrings (perEra["black"], blackQuality[e]);
        }

        if (auto* arr = root["progressions"].getArray())
            for (auto& v : *arr)
            {
                Progression p;
                p.id = v["id"].toString();       p.era = v["era"].toString();
                p.style = v["style"].toString(); p.name = v["name"].toString();
                p.mode = v["mode"].toString();   p.roman = v["roman"].toString();
                if (auto* t = v["tempo"].getArray(); t != nullptr && t->size() == 2)
                {
                    p.tempoLo = (int) (*t)[0]; p.tempoHi = (int) (*t)[1];
                }
                if (auto* refs = v["song_refs"].getArray())
                {
                    juce::StringArray names;
                    for (auto& r : *refs)
                        names.add (r["title"].toString() + " (" + r["artist"].toString() + ", " + r["year"].toString() + ")");
                    p.songs = names.joinIntoString ("; ");
                }
                if (auto* cs = v["chords"].getArray())
                    for (auto& c : *cs)
                    {
                        Chord ch;
                        ch.root = (int) c["root"];
                        ch.bass = c.hasProperty ("bass") ? (int) c["bass"] : -1;
                        ch.beats = juce::jmax (1, (int) c["beats"]);
                        ch.quality = c["quality"].toString();
                        ch.symbol = c["symbol"].toString();
                        p.chords.push_back (ch);
                    }
                if (p.chords.empty()) continue;
                const int e = eraNames.indexOf (p.era);
                if (e >= 0 && e < numEras)
                {
                    byEra[e].push_back ((int) progressions.size());
                    progressions.push_back (std::move (p));
                }
            }
    }

    const std::vector<int>& intervals (const juce::String& quality) const
    {
        static const std::vector<int> triad { 0, 4, 7 };
        auto it = qualities.find (quality);
        return it != qualities.end() ? it->second : triad;
    }

    // Writes MIDI note numbers for a chord (bass note first). Returns how many notes were written.
    int buildChord (int key, int rootSemis, const juce::String& quality, int bassSemis, int* out, int maxNotes) const
    {
        auto place = [] (int pc, int base) { return base + pc - (pc > 6 ? 12 : 0); };
        const int rootNote = place ((key + rootSemis) % 12, 48);
        const int bassNote = bassSemis >= 0 ? place ((key + bassSemis) % 12, 36) : rootNote - 12;
        int n = 0;
        out[n++] = bassNote;
        for (int iv : intervals (quality))
            if (n < maxNotes) out[n++] = rootNote + 12 + iv;
        return n;
    }

    // Voices one chord. inversion >= 0 picks that inversion; inversion < 0 picks whichever
    // inversion sits closest to the previous chord (or root position if there is none).
    Voiced voiceChord (int key, int rootSemis, const juce::String& quality, int bassSemis,
                       int inversion, const Voiced* previous, int octaveShift = 0) const
    {
        auto place = [] (int pc, int base) { return base + pc - (pc > 6 ? 12 : 0); };
        const int rootNote = place ((key + rootSemis) % 12, 48) + octaveShift;
        const int bassNote = bassSemis >= 0 ? place ((key + bassSemis) % 12, 36) + octaveShift : rootNote - 12;
        auto& iv = intervals (quality);
        const int n = juce::jlimit (1, 7, (int) iv.size());

        auto build = [&] (int k, int shift, int* out)
        {
            for (int i = 0; i < n; ++i) out[i] = rootNote + 12 + iv[(size_t) i] + shift;
            std::sort (out, out + n);
            for (int i = 0; i < k; ++i) { out[0] += 12; std::sort (out, out + n); }
        };

        int best[7] {}, bestInv = 0;
        if (inversion >= 0 || previous == nullptr || previous->count < 2)
        {
            bestInv = inversion >= 0 ? inversion % n : 0;
            build (bestInv, 0, best);
            if (best[0] >= rootNote + 12 + 7)            // higher inversions sit under the root, not above it
                for (int i = 0; i < n; ++i) best[i] -= 12;
        }
        else
        {
            const int* prev = previous->notes + 1;
            const int prevN = previous->count - 1;
            double bestCost = 1.0e9;
            for (int k = 0; k < n; ++k)
                for (int shift : { 0, -12 })
                {
                    int cand[7];
                    build (k, shift, cand);
                    double cost = 0.5 * std::abs (cand[n - 1] - prev[prevN - 1]);
                    for (int i = 0; i < n; ++i)
                    {
                        int nearest = 127;
                        for (int j = 0; j < prevN; ++j) nearest = juce::jmin (nearest, std::abs (cand[i] - prev[j]));
                        cost += nearest;
                    }
                    if (cand[0] < 50 + octaveShift) cost += 8.0;
                    if (cand[n - 1] > 81 + octaveShift) cost += 8.0;
                    if (cost < bestCost) { bestCost = cost; bestInv = k; std::copy (cand, cand + n, best); }
                }
        }

        Voiced v;
        v.inversion = bestInv;
        v.notes[v.count++] = bassNote;
        for (int i = 0; i < n; ++i) v.notes[v.count++] = best[i];
        return v;
    }

    // Voices a whole progression into out[]. Returns the number of chords written.
    int voiceProgression (const Progression& p, int key, int mode, const int* inversions, Voiced* out, int maxOut) const
    {
        const int n = juce::jmin (maxOut, (int) p.chords.size());
        for (int i = 0; i < n; ++i)
        {
            auto& c = p.chords[(size_t) i];
            const int inv = mode == voicingInversion ? inversions[i] : (mode == voicingClosest && i > 0 ? -1 : 0);
            out[i] = voiceChord (key, c.root, c.quality, c.bass, inv, i > 0 ? &out[i - 1] : nullptr);
        }
        return n;
    }

    int numInEra (int era) const                       { return (int) byEra[juce::jlimit (0, numEras - 1, era)].size(); }
    const Progression& get (int era, int index) const
    {
        auto& list = byEra[juce::jlimit (0, numEras - 1, era)];
        return progressions[(size_t) list[(size_t) juce::jlimit (0, (int) list.size() - 1, index)]];
    }

    juce::StringArray eraNames, whiteLabels, blackLabels;
    juce::StringArray whiteQuality[numEras], blackQuality[numEras];
    std::vector<Progression> progressions;

private:
    static void readStrings (const juce::var& v, juce::StringArray& dest)
    {
        if (auto* arr = v.getArray())
            for (auto& s : *arr) dest.add (s.toString());
    }

    std::map<juce::String, std::vector<int>> qualities;
    std::vector<int> byEra[numEras];
};
