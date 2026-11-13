#pragma once
#include <JuceHeader.h>
#include "ChordDatabase.h"

class RnBChordsProcessor : public juce::AudioProcessor
{
public:
    RnBChordsProcessor();
    ~RnBChordsProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override                         { return true; }
    const juce::String getName() const override             { return "RnB Chords"; }
    bool acceptsMidi() const override                       { return true; }
    bool producesMidi() const override                      { return true; }
    bool isMidiEffect() const override                      { return false; }
    double getTailLengthSeconds() const override            { return 1.0; }
    int getNumPrograms() override                           { return 1; }
    int getCurrentProgram() override                        { return 0; }
    void setCurrentProgram (int) override                   {}
    const juce::String getProgramName (int) override        { return {}; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    // ---- used by the editor -------------------------------------------------
    int getEra() const;
    int getKey() const;
    void setEra (int era);
    int getProgIndex() const                { return progIndex.load(); }
    void setProgIndex (int index);
    void stepProg (int delta);
    const Progression& currentProg() const  { return db.get (getEra(), progIndex.load()); }
    void setPlaying (bool shouldPlay)       { playing.store (shouldPlay); }
    bool isPlaying() const                  { return playing.load(); }
    int playingChord() const                { return uiChord.load(); }
    int triggeredPitchClass() const         { return uiTriggerPc.load(); }
    void auditionChord (int chordIndex, bool on);
    void keyboardClick (int pitchClass, bool on);
    juce::File writeMidiFile() const;

    ChordDatabase db;
    juce::AudioProcessorValueTreeState apvts;
    std::atomic<bool> loopOn { true };

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void handleTrigger (const juce::MidiMessage&, int samplePos, juce::MidiBuffer& out);
    void runSequencer (int numSamples, juce::MidiBuffer& out);
    void stopSequencerNotes (int samplePos, juce::MidiBuffer& out);

    juce::Synthesiser synth;
    juce::MidiMessageCollector uiMidi;
    double sr = 44100.0;

    std::atomic<bool> playing { false };
    std::atomic<int> progIndex { 0 }, uiChord { -1 }, uiTriggerPc { -1 };

    // sequencer state (audio thread only)
    bool wasPlaying = false;
    int seqChord = -1, seqEra = -1, seqProg = -1;
    double samplesUntilNext = 0.0;
    int seqNotes[8] {}; int seqNoteCount = 0;
    struct Pending { bool active = false; int countdown = 0; int note = 0; };
    Pending pending[32];

    // MIDI-trigger state (audio thread only)
    int heldNotes[128][8] {}; int heldCount[128] {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RnBChordsProcessor)
};
