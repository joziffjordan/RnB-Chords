#pragma once
#include <JuceHeader.h>
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

struct Progression
{
    juce::String id, era, style, name, mode, roman, songs;
    std::vector<Chord> chords;
    int tempoLo = 80, tempoHi = 100;
};

class ChordDatabase
{
public:
    static constexpr int numEras = 4;

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
