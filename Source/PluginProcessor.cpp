#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
constexpr int directChannel = 16;   // editor -> synth, bypasses the chord trigger
constexpr float chordVelocity = 0.72f;

struct EPSound : juce::SynthesiserSound
{
    bool appliesToNote (int) override    { return true; }
    bool appliesToChannel (int) override { return true; }
};

// A simple electric-piano style voice: a soft FM tone plus a short bell "tine".
struct EPVoice : juce::SynthesiserVoice
{
    bool canPlaySound (juce::SynthesiserSound* s) override { return dynamic_cast<EPSound*> (s) != nullptr; }

    void startNote (int midiNote, float velocity, juce::SynthesiserSound*, int) override
    {
        phase = 0.0;
        delta = juce::MathConstants<double>::twoPi * juce::MidiMessage::getMidiNoteInHertz (midiNote) / getSampleRate();
        level = 0.16f * (0.4f + 0.6f * velocity);
        bell = 1.0f;
        bellDecay = (float) std::exp (-1.0 / (0.25 * getSampleRate()));
        tineOk = juce::MidiMessage::getMidiNoteInHertz (midiNote) * 14.0 < getSampleRate() * 0.45;
        adsr.setSampleRate (getSampleRate());
        adsr.setParameters ({ 0.004f, 1.6f, 0.30f, 0.35f });
        adsr.noteOn();
    }

    void stopNote (float, bool allowTailOff) override
    {
        if (allowTailOff) adsr.noteOff();
        else { adsr.reset(); clearCurrentNote(); }
    }

    void pitchWheelMoved (int) override {}
    void controllerMoved (int, int) override {}

    void renderNextBlock (juce::AudioBuffer<float>& buffer, int start, int num) override
    {
        if (! adsr.isActive()) return;
        for (int i = 0; i < num; ++i)
        {
            const float env = adsr.getNextSample();
            float s = (float) std::sin (phase + 0.55 * bell * std::sin (phase));
            if (tineOk) s += 0.10f * bell * (float) std::sin (phase * 14.0);
            s *= env * level;
            for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
                buffer.addSample (ch, start + i, s);
            phase += delta;
            if (phase > juce::MathConstants<double>::twoPi * 1000.0) phase -= juce::MathConstants<double>::twoPi * 1000.0;
            bell *= bellDecay;
        }
        if (! adsr.isActive()) clearCurrentNote();
    }

    double phase = 0.0, delta = 0.0;
    float level = 0.0f, bell = 0.0f, bellDecay = 0.999f;
    bool tineOk = true;
    juce::ADSR adsr;
};
}

juce::AudioProcessorValueTreeState::ParameterLayout RnBChordsProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "era", 1 }, "Era", StringArray { "Soul", "80s", "90s", "2000s", "Modern" }, 4));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "voicing", 1 }, "Voicing", StringArray { "Root", "Closest", "Inversion" }, 1));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "key", 1 }, "Key",
                StringArray { "C", "Db", "D", "Eb", "E", "F", "Gb", "G", "Ab", "A", "Bb", "B" }, 0));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "tempo", 1 }, "Tempo", NormalisableRange<float> (50.0f, 160.0f, 1.0f), 92.0f));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "volume", 1 }, "Volume", 0.0f, 1.0f, 0.7f));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "strum", 1 }, "Strum", 0.0f, 1.0f, 0.25f));
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { "trigger", 1 }, "MIDI Trigger", true));
    return layout;
}

RnBChordsProcessor::RnBChordsProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "state", createLayout())
{
    for (auto& v : chordInv) v.store (0);
    for (int i = 0; i < 32; ++i) synth.addVoice (new EPVoice());
    synth.addSound (new EPSound());
}

bool RnBChordsProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono();
}

void RnBChordsProcessor::prepareToPlay (double sampleRate, int)
{
    sr = sampleRate;
    synth.setCurrentPlaybackSampleRate (sampleRate);
    uiMidi.reset (sampleRate);
    wasPlaying = false; seqChord = -1; seqNoteCount = 0; samplesUntilNext = 0.0;
    for (auto& p : pending) p.active = false;
    for (auto& c : heldCount) c = 0;
    lastTrigger = {};
}

int RnBChordsProcessor::getVoicingMode() const { return juce::roundToInt (apvts.getRawParameterValue ("voicing")->load()); }

void RnBChordsProcessor::setVoicingMode (int mode)
{
    if (auto* p = apvts.getParameter ("voicing"))
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->convertTo0to1 ((float) mode));
        p->endChangeGesture();
    }
}

void RnBChordsProcessor::setChordInversion (int chord, int inversion)
{
    if (chord >= 0 && chord < ChordDatabase::maxChords) chordInv[chord].store (juce::jmax (0, inversion));
}

void RnBChordsProcessor::resetInversions()
{
    for (auto& v : chordInv) v.store (0);
}

int RnBChordsProcessor::voiceCurrent (Voiced* out) const
{
    int invs[ChordDatabase::maxChords];
    for (int i = 0; i < ChordDatabase::maxChords; ++i) invs[i] = chordInv[i].load();
    return db.voiceProgression (currentProg(), getKey(), getVoicingMode(), invs, out, ChordDatabase::maxChords);
}

int RnBChordsProcessor::getEra() const { return juce::roundToInt (apvts.getRawParameterValue ("era")->load()); }
int RnBChordsProcessor::getKey() const { return juce::roundToInt (apvts.getRawParameterValue ("key")->load()); }

void RnBChordsProcessor::setEra (int era)
{
    if (auto* p = apvts.getParameter ("era"))
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->convertTo0to1 ((float) era));
        p->endChangeGesture();
    }
    progIndex.store (0);
    resetInversions();
}

void RnBChordsProcessor::setProgIndex (int index)
{
    const int n = db.numInEra (getEra());
    if (n <= 0) return;
    const int wrapped = ((index % n) + n) % n;
    if (wrapped != progIndex.load()) resetInversions();   // inversions belong to one progression
    progIndex.store (wrapped);
}

void RnBChordsProcessor::stepProg (int delta) { setProgIndex (progIndex.load() + delta); }

void RnBChordsProcessor::auditionChord (int chordIndex, bool on)
{
    const double now = juce::Time::getMillisecondCounterHiRes() * 0.001;
    auto send = [&] (bool noteOn)
    {
        for (int i = 0; i < auditionCount; ++i)
        {
            auto m = noteOn ? juce::MidiMessage::noteOn (directChannel, auditionNotes[i], chordVelocity)
                            : juce::MidiMessage::noteOff (directChannel, auditionNotes[i]);
            m.setTimeStamp (now);
            uiMidi.addMessageToQueue (m);
        }
    };

    if (! on) { send (false); auditionCount = 0; return; }

    Voiced voiced[ChordDatabase::maxChords];
    const int total = voiceCurrent (voiced);
    if (chordIndex < 0 || chordIndex >= total) return;
    send (false);
    auditionCount = voiced[chordIndex].count;
    std::copy (voiced[chordIndex].notes, voiced[chordIndex].notes + auditionCount, auditionNotes);
    send (true);
}

void RnBChordsProcessor::keyboardClick (int pitchClass, bool on)
{
    auto m = on ? juce::MidiMessage::noteOn (15, 60 + pitchClass, chordVelocity) : juce::MidiMessage::noteOff (15, 60 + pitchClass);
    m.setTimeStamp (juce::Time::getMillisecondCounterHiRes() * 0.001);
    uiMidi.addMessageToQueue (m);
}

// Turns one incoming key into a chord: white keys = the 7 chords of the key, black keys = borrowed chords.
void RnBChordsProcessor::handleTrigger (const juce::MidiMessage& m, int pos, juce::MidiBuffer& out)
{
    static const int whitePc[7] = { 0, 2, 4, 5, 7, 9, 11 };
    static const int blackPc[5] = { 1, 3, 6, 8, 10 };
    const int note = m.getNoteNumber();
    if (note < 0 || note > 127) return;

    if (m.isNoteOn())
    {
        if (heldCount[note] > 0) return;
        const int pc = note % 12, era = getEra();
        juce::String quality = "maj";
        for (int i = 0; i < 7; ++i) if (whitePc[i] == pc) quality = db.whiteQuality[era][i];
        for (int i = 0; i < 5; ++i) if (blackPc[i] == pc) quality = db.blackQuality[era][i];
        const int shift = (note / 12 - 5) * 12;
        // "closest" follows the last chord you played, unless you have moved to another octave
        const bool closest = getVoicingMode() == voicingClosest && shift == lastTriggerShift;
        lastTriggerShift = shift;
        const auto voiced = db.voiceChord (getKey(), pc, quality, -1, closest ? -1 : 0, closest ? &lastTrigger : nullptr, shift);
        lastTrigger = voiced;
        int count = 0;
        for (int i = 0; i < voiced.count; ++i)
        {
            const int v = voiced.notes[i];
            if (v < 0 || v > 127) continue;
            heldNotes[note][count++] = v;
            out.addEvent (juce::MidiMessage::noteOn (1, v, m.getFloatVelocity()), pos);
        }
        heldCount[note] = count;
        uiTriggerPc.store (pc);
    }
    else if (m.isNoteOff())
    {
        for (int i = 0; i < heldCount[note]; ++i)
            out.addEvent (juce::MidiMessage::noteOff (1, heldNotes[note][i]), pos);
        heldCount[note] = 0;
        if (uiTriggerPc.load() == note % 12) uiTriggerPc.store (-1);
    }
}

void RnBChordsProcessor::stopSequencerNotes (int pos, juce::MidiBuffer& out)
{
    for (int i = 0; i < seqNoteCount; ++i)
        out.addEvent (juce::MidiMessage::noteOff (1, seqNotes[i]), pos);
    seqNoteCount = 0;
    for (auto& p : pending) p.active = false;
}

void RnBChordsProcessor::runSequencer (int numSamples, juce::MidiBuffer& out)
{
    const bool play = playing.load();
    const int era = getEra(), prog = progIndex.load();

    if (! play)
    {
        if (wasPlaying) { stopSequencerNotes (0, out); uiChord.store (-1); }
        wasPlaying = false;
        return;
    }

    if (! wasPlaying || era != seqEra || prog != seqProg)
    {
        stopSequencerNotes (0, out);
        seqChord = -1; samplesUntilNext = 0.0; seqEra = era; seqProg = prog; wasPlaying = true;
    }

    // strummed notes left over from the previous block
    for (auto& p : pending)
        if (p.active)
        {
            if (p.countdown < numSamples) { out.addEvent (juce::MidiMessage::noteOn (1, p.note, chordVelocity), p.countdown); p.active = false; }
            else p.countdown -= numSamples;
        }

    auto& chords = db.get (era, prog).chords;
    const double bpm = apvts.getRawParameterValue ("tempo")->load();
    const double samplesPerBeat = sr * 60.0 / juce::jmax (20.0, bpm);
    const int strumStep = (int) (apvts.getRawParameterValue ("strum")->load() * 0.035f * (float) sr);

    int pos = 0;
    double remain = samplesUntilNext;
    while (remain <= (double) (numSamples - pos))
    {
        pos = juce::jmin (numSamples - 1, pos + (int) remain);
        for (int i = 0; i < seqNoteCount; ++i)
            out.addEvent (juce::MidiMessage::noteOff (1, seqNotes[i]), pos);
        seqNoteCount = 0;

        ++seqChord;
        if (seqChord >= (int) chords.size())
        {
            if (! loopOn.load()) { playing.store (false); wasPlaying = false; uiChord.store (-1); return; }
            seqChord = 0;
        }

        auto& c = chords[(size_t) juce::jmin (seqChord, (int) chords.size() - 1)];
        Voiced voiced[ChordDatabase::maxChords];
        const int total = voiceCurrent (voiced);
        const auto& v = voiced[juce::jlimit (0, juce::jmax (0, total - 1), seqChord)];
        seqNoteCount = v.count;
        std::copy (v.notes, v.notes + v.count, seqNotes);
        for (int i = 0; i < seqNoteCount; ++i)
        {
            const int at = pos + i * strumStep;
            if (at < numSamples) out.addEvent (juce::MidiMessage::noteOn (1, seqNotes[i], chordVelocity), at);
            else
                for (auto& p : pending)
                    if (! p.active) { p.active = true; p.countdown = at - numSamples; p.note = seqNotes[i]; break; }
        }
        uiChord.store (seqChord);
        remain = c.beats * samplesPerBeat;
        if (remain < 1.0) remain = 1.0;
    }
    samplesUntilNext = remain - (double) (numSamples - pos);
}

void RnBChordsProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int numSamples = buffer.getNumSamples();
    buffer.clear();

    juce::MidiBuffer out, fromUi;
    uiMidi.removeNextBlockOfMessages (fromUi, numSamples);
    const bool trigger = apvts.getRawParameterValue ("trigger")->load() > 0.5f;

    for (const auto meta : midi)
    {
        auto m = meta.getMessage();
        if (m.isNoteOnOrOff())
        {
            if (trigger) handleTrigger (m, meta.samplePosition, out);
            else out.addEvent (m, meta.samplePosition);
        }
    }
    for (const auto meta : fromUi)
    {
        auto m = meta.getMessage();
        if (m.getChannel() == directChannel) { m.setChannel (1); out.addEvent (m, meta.samplePosition); }
        else handleTrigger (m, meta.samplePosition, out);
    }

    runSequencer (numSamples, out);

    synth.renderNextBlock (buffer, out, 0, numSamples);
    buffer.applyGain (apvts.getRawParameterValue ("volume")->load() * 1.4f);

    midi.swapWith (out);   // the chord notes also go to the plugin's MIDI output
}

juce::File RnBChordsProcessor::writeMidiFile() const
{
    auto& prog = currentProg();
    const int ppq = 480;
    juce::MidiMessageSequence seq;
    const double bpm = apvts.getRawParameterValue ("tempo")->load();
    seq.addEvent (juce::MidiMessage::tempoMetaEvent ((int) (60000000.0 / juce::jmax (20.0, bpm))), 0.0);

    Voiced voiced[ChordDatabase::maxChords];
    const int total = voiceCurrent (voiced);
    double t = 0.0;
    for (int ci = 0; ci < total; ++ci)
    {
        const double len = prog.chords[(size_t) ci].beats * ppq;
        for (int i = 0; i < voiced[ci].count; ++i)
        {
            seq.addEvent (juce::MidiMessage::noteOn (1, voiced[ci].notes[i], (juce::uint8) 92), t);
            seq.addEvent (juce::MidiMessage::noteOff (1, voiced[ci].notes[i]), t + len - 1.0);
        }
        t += len;
    }
    seq.updateMatchedPairs();

    juce::MidiFile file;
    file.setTicksPerQuarterNote (ppq);
    file.addTrack (seq);

    auto safeName = juce::File::createLegalFileName ("RnB Chords - " + prog.name + ".mid");
    auto target = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile (safeName);
    target.deleteFile();
    if (auto stream = target.createOutputStream())
        file.writeTo (*stream);
    return target;
}

void RnBChordsProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto state = apvts.copyState();
    state.setProperty ("progIndex", progIndex.load(), nullptr);
    state.setProperty ("loop", loopOn.load(), nullptr);
    juce::StringArray invs;
    for (auto& v : chordInv) invs.add (juce::String (v.load()));
    state.setProperty ("inversions", invs.joinIntoString (","), nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, dest);
}

void RnBChordsProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
    {
        auto state = juce::ValueTree::fromXml (*xml);
        if (state.isValid())
        {
            apvts.replaceState (state);
            setProgIndex ((int) state.getProperty ("progIndex", 0));
            loopOn.store ((bool) state.getProperty ("loop", true));
            auto invs = juce::StringArray::fromTokens (state.getProperty ("inversions", "").toString(), ",", "");
            for (int i = 0; i < ChordDatabase::maxChords; ++i)
                chordInv[i].store (i < invs.size() ? juce::jmax (0, invs[i].getIntValue()) : 0);
        }
    }
}

juce::AudioProcessorEditor* RnBChordsProcessor::createEditor() { return new RnBChordsEditor (*this); }
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()       { return new RnBChordsProcessor(); }
