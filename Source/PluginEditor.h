#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "Theme.h"

class RnBChordsEditor;

// Round or pill-shaped glossy button. Draws text or a simple transport icon.
class GlossButton : public juce::Button
{
public:
    enum Icon { none, prev, play, stop, next, loop };
    GlossButton (const Theme& t, const juce::String& buttonText, Icon i = none, bool pill = false)
        : juce::Button (buttonText), theme (t), label (buttonText), icon (i), isPill (pill) {}
    void paintButton (juce::Graphics&, bool, bool) override;
    void setIcon (Icon i)                 { if (icon != i) { icon = i; repaint(); } }
    void setLabel (const juce::String& s) { if (label != s) { label = s; repaint(); } }
    bool lit = false;
    float fontSize = 22.0f;
private:
    const Theme& theme; juce::String label; Icon icon; bool isPill;
};

// The chord pads for the progression that is loaded.
class PadStrip : public juce::Component
{
public:
    PadStrip (RnBChordsProcessor& p, const Theme& t) : proc (p), theme (t) {}
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    int active = -1;
private:
    juce::Rectangle<float> padBounds (int index, int count) const;
    RnBChordsProcessor& proc; const Theme& theme; int pressed = -1;
};

// One octave of keys. Each key shows the chord it plays.
class KeyStrip : public juce::Component
{
public:
    KeyStrip (RnBChordsProcessor& p, const Theme& t) : proc (p), theme (t) {}
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    int active = -1;
private:
    int keyAt (juce::Point<float>) const;
    RnBChordsProcessor& proc; const Theme& theme; int pressed = -1;
};

// Drag this into the DAW to drop the progression as a MIDI clip.
class DragPill : public juce::Component
{
public:
    DragPill (RnBChordsProcessor& p, const Theme& t) : proc (p), theme (t) {}
    void paint (juce::Graphics&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override { dragging = false; }
private:
    RnBChordsProcessor& proc; const Theme& theme; bool dragging = false;
};

// Plays the coin flip between two snapshots of the interface.
class FlipOverlay : public juce::Component, private juce::Timer
{
public:
    void start (juce::Image from, juce::Image to);
    void paint (juce::Graphics&) override;
private:
    void timerCallback() override;
    juce::Image oldImg, newImg; double startMs = 0.0; float progress = 1.0f;
};

class GlossLookAndFeel : public juce::LookAndFeel_V4
{
public:
    explicit GlossLookAndFeel (const Theme& t) : theme (t) {}
    void drawLinearSlider (juce::Graphics&, int, int, int, int, float, float, float, juce::Slider::SliderStyle, juce::Slider&) override;
    void drawComboBox (juce::Graphics&, int, int, bool, int, int, int, int, juce::ComboBox&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override { return gloss::font (15.0f); }
    juce::Font getLabelFont (juce::Label&) override      { return gloss::font (15.0f); }
private:
    const Theme& theme;
};

class RnBChordsEditor : public juce::AudioProcessorEditor,
                        public juce::DragAndDropContainer,
                        private juce::ListBoxModel,
                        private juce::Timer
{
public:
    explicit RnBChordsEditor (RnBChordsProcessor&);
    ~RnBChordsEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    int getNumRows() override;
    void paintListBoxItem (int row, juce::Graphics&, int width, int height, bool selected) override;
    void selectedRowsChanged (int lastRowSelected) override;
    void timerCallback() override;
    void applyTheme (int era);
    void paintBackground (juce::Graphics&);
    void paintDecor (juce::Graphics&, juce::Rectangle<float> nowPanel);

    RnBChordsProcessor& proc;
    Theme theme;
    GlossLookAndFeel lnf { theme };

    GlossButton era80 { theme, "80s" }, era90 { theme, "90s" }, era00 { theme, "00s" }, eraNow { theme, "Now" };
    GlossButton prevBtn { theme, "Previous", GlossButton::prev }, playBtn { theme, "Play", GlossButton::play },
                nextBtn { theme, "Next", GlossButton::next }, loopBtn { theme, "Loop", GlossButton::loop };
    GlossButton triggerBtn { theme, "MIDI Trigger", GlossButton::none, true };
    juce::ListBox list;
    PadStrip pads { proc, theme };
    KeyStrip keys { proc, theme };
    DragPill dragPill { proc, theme };
    juce::Slider volume, strum, tempo;
    juce::ComboBox keyBox;
    FlipOverlay flip;

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> volumeAtt, strumAtt, tempoAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> keyAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> triggerAtt;

    int shownEra = -1, shownProg = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RnBChordsEditor)
};
