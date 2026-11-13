#include "PluginEditor.h"

namespace
{
using Rect = juce::Rectangle<int>;
const Rect eraTile { 24, 96, 300, 232 }, listPanel { 24, 344, 300, 312 }, nowPanel { 340, 96, 676, 140 },
           midiPanel { 340, 468, 676, 188 }, voiceBar { 340, 252, 540, 40 };
const char* const inversionNames[6] = { "Root", "1st inv", "2nd inv", "3rd inv", "4th inv", "5th inv" };
const char* const noteNames[12] = { "C", "Db", "D", "Eb", "E", "F", "Gb", "G", "Ab", "A", "Bb", "B" };
const int whitePc[7] = { 0, 2, 4, 5, 7, 9, 11 };
const int blackPc[5] = { 1, 3, 6, 8, 10 };
const int blackAfterWhite[5] = { 1, 2, 4, 5, 6 };

juce::String chordName (const Chord& c, int key)
{
    juce::String s (noteNames[(key + c.root) % 12]);
    if (c.quality != "maj") s << c.quality;
    if (c.bass >= 0) s << "/" << noteNames[(key + c.bass) % 12];
    return s;
}

float textWidth (const juce::Font& f, const juce::String& text)
{
    juce::GlyphArrangement ga;
    ga.addLineOfText (f, text, 0.0f, 0.0f);
    return ga.getBoundingBox (0, -1, true).getWidth();
}

void blob (juce::Graphics& g, float cx, float cy, float radius, juce::Colour c)
{
    g.setGradientFill (juce::ColourGradient (c, cx, cy, c.withAlpha (0.0f), cx + radius, cy, true));
    g.fillEllipse (cx - radius, cy - radius, radius * 2.0f, radius * 2.0f);
}

void sparkle (juce::Graphics& g, float cx, float cy, float r)
{
    juce::Path p;
    p.startNewSubPath (cx, cy - r);
    p.quadraticTo (cx, cy, cx + r, cy);
    p.quadraticTo (cx, cy, cx, cy + r);
    p.quadraticTo (cx, cy, cx - r, cy);
    p.quadraticTo (cx, cy, cx, cy - r);
    g.setColour (juce::Colours::white);
    g.fillPath (p);
    g.setColour (juce::Colour (0xff8088c8));
    g.strokePath (p, juce::PathStrokeType (0.8f));
}
}

//==============================================================================
void GlossButton::paintButton (juce::Graphics& g, bool over, bool down)
{
    auto r = getLocalBounds().toFloat().reduced (4.0f);
    const bool active = lit || (getClickingTogglesState() && getToggleState());

    if (shape == capsule)
    {
        gloss::capsule (g, r, theme, active);
        if (down || over)
        {
            g.setColour ((down ? juce::Colours::black : juce::Colours::white).withAlpha (0.10f));
            g.fillRoundedRectangle (r, r.getHeight() * 0.5f);
        }
        g.setColour (active ? theme.actText : theme.orbText);
        g.setFont (gloss::font (fontSize));
        g.drawText (label, r, juce::Justification::centred, false);
        return;
    }

    if (shape == toggle)
    {
        r = r.withTrimmedBottom (2.0f);
        gloss::panel (g, r, r.getHeight() * 0.5f, theme);
        auto dot = r.removeFromLeft (r.getHeight()).reduced (6.0f);
        gloss::orb (g, dot, theme, active);
        auto text = r.reduced (6.0f, 4.0f);
        g.setColour (theme.text);
        g.setFont (gloss::font (14.0f));
        g.drawText (label, text.removeFromTop (text.getHeight() * 0.55f), juce::Justification::bottomLeft, true);
        g.setColour (theme.mut);
        g.setFont (gloss::font (12.0f));
        g.drawText (active ? "On" : "Off", text, juce::Justification::topLeft, true);
        return;
    }

    gloss::orb (g, r, theme, active);
    if (down || over)
    {
        g.setColour ((down ? juce::Colours::black : juce::Colours::white).withAlpha (0.10f));
        g.fillEllipse (r);
    }
    g.setColour (active ? theme.actText : theme.orbText);

    const float cx = r.getCentreX(), cy = r.getCentreY(), u = r.getWidth() * 0.16f;
    juce::Path p;
    switch (icon)
    {
        case play: p.addTriangle (cx - u * 0.8f, cy - u * 1.2f, cx - u * 0.8f, cy + u * 1.2f, cx + u * 1.3f, cy); break;
        case stop: p.addRoundedRectangle (cx - u, cy - u, u * 2.0f, u * 2.0f, 3.0f); break;
        case prev: p.addTriangle (cx, cy - u, cx, cy + u, cx - u * 1.4f, cy);
                   p.addTriangle (cx + u * 1.4f, cy - u, cx + u * 1.4f, cy + u, cx, cy); break;
        case next: p.addTriangle (cx, cy - u, cx, cy + u, cx + u * 1.4f, cy);
                   p.addTriangle (cx - u * 1.4f, cy - u, cx - u * 1.4f, cy + u, cx, cy); break;
        case loop:
        {
            juce::Path arc;
            arc.addCentredArc (cx, cy, u * 1.3f, u * 1.3f, 0.0f, 0.9f, 5.6f, true);
            g.strokePath (arc, juce::PathStrokeType (2.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            auto end = arc.getCurrentPosition();
            p.addTriangle (end.x - u * 0.7f, end.y - u * 0.2f, end.x + u * 0.5f, end.y - u * 0.7f, end.x + u * 0.3f, end.y + u * 0.6f);
            break;
        }
        case none:
            g.setFont (gloss::font (fontSize));
            g.drawText (label, r, juce::Justification::centred, false);
            break;
    }
    if (! p.isEmpty()) g.fillPath (p);
}

//==============================================================================
juce::Rectangle<float> PadStrip::padBounds (int index, int count) const
{
    const float gap = count > 6 ? 6.0f : 12.0f;
    const float w = ((float) getWidth() - gap * (float) (count - 1)) / (float) count;
    return { (float) index * (w + gap), 0.0f, w, (float) getHeight() - 8.0f };
}

juce::Rectangle<float> PadStrip::arrowBounds (juce::Rectangle<float> pad, bool right) const
{
    const float size = juce::jmin (26.0f, pad.getWidth() * 0.3f);
    const float inset = juce::jmin (10.0f, pad.getWidth() * 0.1f);
    return { right ? pad.getRight() - inset - size : pad.getX() + inset, pad.getBottom() - 12.0f - size, size, size };
}

void PadStrip::paint (juce::Graphics& g)
{
    auto& prog = proc.currentProg();
    Voiced voiced[ChordDatabase::maxChords];
    const int count = proc.voiceCurrent (voiced);
    const int key = proc.getKey();
    const bool perChord = proc.getVoicingMode() == voicingInversion;

    for (int i = 0; i < count; ++i)
    {
        auto r = padBounds (i, count).withTrimmedRight (theme.hardShadow ? 5.0f : 0.0f);
        const bool on = (i == active || i == pressed);
        gloss::panel (g, r, juce::jmin (34.0f, r.getWidth() * 0.3f), theme, on);
        auto inner = r.reduced (juce::jmin (12.0f, r.getWidth() * 0.12f), 12.0f);
        auto& c = prog.chords[(size_t) i];

        // top line: the numeral, then the notes from bottom to top
        juce::String notes;
        for (int n = 1; n < voiced[i].count; ++n) notes << noteNames[voiced[i].notes[n] % 12] << " ";
        g.setColour (on ? theme.actText : theme.mut);
        g.setFont (gloss::font (13.0f));
        g.drawFittedText (c.symbol, inner.removeFromTop (18.0f).toNearestInt(), juce::Justification::centredLeft, 1, 0.6f);
        g.setFont (gloss::font (11.0f));
        g.drawFittedText (notes.trim(), inner.removeFromTop (16.0f).toNearestInt(), juce::Justification::centredLeft, 1, 0.6f);

        // bottom line: which inversion, with arrows when you can change it
        auto bottom = inner.removeFromBottom (26.0f);
        const auto tag = juce::String (inversionNames[juce::jlimit (0, 5, voiced[i].inversion)]);
        g.setFont (gloss::font (12.0f));
        if (perChord)
        {
            for (bool right : { false, true })
            {
                auto a = arrowBounds (r, right);
                gloss::orb (g, a, theme, false);
                juce::Path chev;
                const float cx = a.getCentreX(), cy = a.getCentreY(), u = a.getWidth() * 0.16f, dir = right ? 1.0f : -1.0f;
                chev.startNewSubPath (cx - dir * u, cy - u * 1.6f);
                chev.lineTo (cx + dir * u, cy);
                chev.lineTo (cx - dir * u, cy + u * 1.6f);
                g.setColour (theme.orbText);
                g.strokePath (chev, juce::PathStrokeType (2.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            }
            g.setColour (on ? theme.actText : theme.text);
            g.drawFittedText (tag, bottom.toNearestInt(), juce::Justification::centred, 1, 0.6f);
        }
        else
        {
            g.setColour (on ? theme.actText : theme.mut);
            g.drawFittedText (tag, bottom.toNearestInt(), juce::Justification::centredLeft, 1, 0.6f);
        }

        g.setColour (on ? theme.actText : theme.text);
        g.setFont (gloss::font (count > 6 ? 16.0f : 24.0f));
        g.drawFittedText (chordName (c, key), inner.toNearestInt(), juce::Justification::centredLeft, 1, 0.55f);
    }
}

void PadStrip::mouseDown (const juce::MouseEvent& e)
{
    Voiced voiced[ChordDatabase::maxChords];
    const int count = proc.voiceCurrent (voiced);
    for (int i = 0; i < count; ++i)
    {
        auto r = padBounds (i, count).withTrimmedRight (theme.hardShadow ? 5.0f : 0.0f);
        if (! r.contains (e.position)) continue;

        if (proc.getVoicingMode() == voicingInversion)
            for (bool right : { false, true })
                if (arrowBounds (r, right).expanded (4.0f).contains (e.position))
                {
                    const int tones = juce::jmax (1, voiced[i].count - 1);
                    proc.setChordInversion (i, (proc.getChordInversion (i) % tones + (right ? 1 : tones - 1)) % tones);
                    repaint();
                    return;
                }

        pressed = i; proc.auditionChord (i, true); repaint();
        return;
    }
}

void PadStrip::mouseUp (const juce::MouseEvent&)
{
    if (pressed >= 0) { proc.auditionChord (pressed, false); pressed = -1; repaint(); }
}

//==============================================================================
int KeyStrip::keyAt (juce::Point<float> pos) const
{
    const float gap = 6.0f, ww = ((float) getWidth() - gap * 6.0f) / 7.0f;
    for (int b = 0; b < 5; ++b)
    {
        const float x = (float) blackAfterWhite[b] * (ww + gap) - gap * 0.5f - 17.0f;
        if (juce::Rectangle<float> (x, 0.0f, 34.0f, 88.0f).contains (pos)) return blackPc[b];
    }
    const int w = juce::jlimit (0, 6, (int) (pos.x / (ww + gap)));
    return whitePc[w];
}

void KeyStrip::paint (juce::Graphics& g)
{
    const float gap = 6.0f, ww = ((float) getWidth() - gap * 6.0f) / 7.0f, h = (float) getHeight() - 6.0f;
    const int lit = pressed >= 0 ? pressed : active;

    for (int i = 0; i < 7; ++i)
    {
        juce::Rectangle<float> r ((float) i * (ww + gap), 0.0f, ww, h);
        const bool on = lit == whitePc[i];
        juce::Path p;
        p.addRoundedRectangle (r.getX(), r.getY(), r.getWidth(), r.getHeight(), 16.0f, 16.0f, false, false, true, true);
        if (theme.hardShadow) { g.setColour (theme.panelBorder); g.fillPath (p, juce::AffineTransform::translation (3.0f, 3.0f)); }
        g.setGradientFill (juce::ColourGradient (on ? theme.actA : theme.wkA, 0.0f, r.getY(), on ? theme.actB : theme.wkB, 0.0f, r.getBottom(), false));
        g.fillPath (p);
        g.setColour (theme.hardShadow ? theme.panelBorder : juce::Colours::white.withAlpha (0.9f));
        g.strokePath (p, juce::PathStrokeType (theme.hardShadow ? 3.0f : 1.5f));

        g.setColour (on ? theme.actText : theme.wkText);
        g.setFont (gloss::font (16.0f));
        g.drawText (proc.db.whiteLabels[i], r.withTrimmedBottom (26.0f).removeFromBottom (20.0f), juce::Justification::centred, false);
        g.setFont (gloss::font (11.0f));
        g.drawText (noteNames[whitePc[i]], r.withTrimmedBottom (10.0f).removeFromBottom (14.0f), juce::Justification::centred, false);
    }

    for (int b = 0; b < 5; ++b)
    {
        juce::Rectangle<float> r ((float) blackAfterWhite[b] * (ww + gap) - gap * 0.5f - 17.0f, 0.0f, 34.0f, 88.0f);
        const bool on = lit == blackPc[b];
        juce::Path p;
        p.addRoundedRectangle (r.getX(), r.getY(), r.getWidth(), r.getHeight(), 10.0f, 10.0f, false, false, true, true);
        g.setGradientFill (juce::ColourGradient (on ? theme.actA : theme.bkA, 0.0f, r.getY(), on ? theme.actB : theme.bkB, 0.0f, r.getBottom(), false));
        g.fillPath (p);
        g.setColour (juce::Colours::white.withAlpha (0.35f));
        g.strokePath (p, juce::PathStrokeType (1.0f));
        g.setColour (on ? theme.actText : juce::Colours::white);
        g.setFont (gloss::font (10.0f));
        g.drawText (proc.db.blackLabels[b], r.withTrimmedBottom (8.0f).removeFromBottom (14.0f), juce::Justification::centred, false);
    }
}

void KeyStrip::mouseDown (const juce::MouseEvent& e) { pressed = keyAt (e.position); proc.keyboardClick (pressed, true); repaint(); }
void KeyStrip::mouseUp (const juce::MouseEvent&)     { if (pressed >= 0) { proc.keyboardClick (pressed, false); pressed = -1; repaint(); } }

//==============================================================================
void DragPill::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (4.0f).withTrimmedBottom (2.0f);
    gloss::panel (g, r, r.getHeight() * 0.5f, theme);
    g.setColour (theme.text);
    g.setFont (gloss::font (14.0f));
    g.drawText ("Drag MIDI to DAW", r, juce::Justification::centred, false);
}

void DragPill::mouseDrag (const juce::MouseEvent& e)
{
    if (dragging || e.getDistanceFromDragStart() < 5) return;
    dragging = true;
    auto file = proc.writeMidiFile();
    if (file.existsAsFile())
        juce::DragAndDropContainer::performExternalDragDropOfFiles ({ file.getFullPathName() }, false, this);
}

//==============================================================================
void FlipOverlay::start (juce::Image from, juce::Image to)
{
    oldImg = from; newImg = to; progress = 0.0f;
    startMs = juce::Time::getMillisecondCounterHiRes();
    setVisible (true);
    toFront (false);
    startTimerHz (60);
}

void FlipOverlay::timerCallback()
{
    progress = (float) ((juce::Time::getMillisecondCounterHiRes() - startMs) / 840.0);
    if (progress >= 1.0f)
    {
        stopTimer();
        setVisible (false);
        oldImg = {}; newImg = {};
    }
    repaint();
}

// Top-to-bottom coin flip: the old face squashes to a line, then the new face opens out.
void FlipOverlay::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff0b0b0d));
    const float eased = 0.5f - 0.5f * std::cos (juce::jlimit (0.0f, 1.0f, progress) * juce::MathConstants<float>::pi);
    const float s = std::abs (std::cos (eased * juce::MathConstants<float>::pi));
    auto& img = eased < 0.5f ? oldImg : newImg;
    if (img.isNull()) return;
    const float shrink = 1.0f - 0.16f * (1.0f - s);
    const float cx = (float) getWidth() * 0.5f, cy = (float) getHeight() * 0.5f;
    const float sx = shrink * (float) getWidth() / (float) img.getWidth();
    const float sy = juce::jmax (0.004f, s * shrink) * (float) getHeight() / (float) img.getHeight();
    g.drawImageTransformed (img, juce::AffineTransform::scale (sx, sy)
                                     .translated (cx - cx * shrink, cy - cy * juce::jmax (0.004f, s * shrink)));
    g.setColour (juce::Colours::black.withAlpha (0.45f * (1.0f - s)));
    g.fillRect (getLocalBounds());
}

//==============================================================================
void GlossLookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float minPos, float maxPos,
                                         juce::Slider::SliderStyle style, juce::Slider& slider)
{
    if (style == juce::Slider::LinearVertical)
    {
        auto r = slider.getLocalBounds().toFloat().reduced (2.0f).withTrimmedBottom (6.0f);
        const float radius = r.getWidth() * 0.5f;
        const float prop = (float) slider.valueToProportionOfLength (slider.getValue());
        juce::Path capsule;
        capsule.addRoundedRectangle (r, radius);

        if (theme.hardShadow) { g.setColour (theme.panelBorder); g.fillPath (capsule, juce::AffineTransform::translation (4.0f, 4.0f)); }
        g.setGradientFill (juce::ColourGradient (theme.capBgA, 0.0f, r.getY(), theme.capBgB, 0.0f, r.getBottom(), false));
        g.fillPath (capsule);
        {
            juce::Graphics::ScopedSaveState save (g);
            g.reduceClipRegion (capsule);
            auto fill = r.withTop (r.getBottom() - prop * r.getHeight());
            g.setGradientFill (juce::ColourGradient (theme.actA, 0.0f, fill.getY(), theme.actB, 0.0f, r.getBottom(), false));
            g.fillRect (fill);
            g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (theme.sheen * 0.6f), 0.0f, r.getY(),
                                                     juce::Colours::transparentWhite, 0.0f, r.getY() + 44.0f, false));
            g.fillEllipse (r.getX() + 6.0f, r.getY() + 4.0f, r.getWidth() - 12.0f, 40.0f);
        }
        g.setColour (theme.hardShadow ? theme.panelBorder : theme.panelBorder.withAlpha (0.7f));
        g.strokePath (capsule, juce::PathStrokeType (theme.borderW));
        g.setColour (prop > 0.14f ? theme.actText : theme.text);
        g.setFont (gloss::font (10.0f));
        g.drawText (slider.getName(), r.withTrimmedBottom (10.0f).removeFromBottom (14.0f), juce::Justification::centred, false);
        return;
    }
    if (style == juce::Slider::LinearBar)
    {
        auto r = slider.getLocalBounds().toFloat().reduced (4.0f).withTrimmedBottom (2.0f);
        gloss::panel (g, r, r.getHeight() * 0.5f, theme);
        return;
    }
    juce::LookAndFeel_V4::drawLinearSlider (g, x, y, w, h, pos, minPos, maxPos, style, slider);
}

void GlossLookAndFeel::drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox&)
{
    auto r = juce::Rectangle<float> (0.0f, 0.0f, (float) w, (float) h).reduced (4.0f).withTrimmedBottom (2.0f);
    gloss::panel (g, r, r.getHeight() * 0.5f, theme);
    juce::Path arrow;
    const float ax = r.getRight() - 22.0f, ay = r.getCentreY() - 2.0f;
    arrow.startNewSubPath (ax, ay); arrow.lineTo (ax + 5.0f, ay + 5.0f); arrow.lineTo (ax + 10.0f, ay);
    g.setColour (theme.text);
    g.strokePath (arrow, juce::PathStrokeType (2.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

//==============================================================================
RnBChordsEditor::RnBChordsEditor (RnBChordsProcessor& p) : juce::AudioProcessorEditor (p), proc (p), theme (makeTheme (p.getEra()))
{
    setLookAndFeel (&lnf);

    GlossButton* eras[5] = { &eraSoul, &era80, &era90, &era00, &eraNow };
    for (int i = 0; i < 5; ++i)
    {
        addAndMakeVisible (eras[i]);
        eras[i]->fontSize = 16.0f;
        eras[i]->onClick = [this, i] { proc.setEra (i); };
    }

    GlossButton* voicings[3] = { &voiceRoot, &voiceClosest, &voiceInv };
    for (int i = 0; i < 3; ++i)
    {
        addAndMakeVisible (voicings[i]);
        voicings[i]->fontSize = 13.0f;
        voicings[i]->onClick = [this, i] { proc.setVoicingMode (i); };
    }

    for (auto* b : { &prevBtn, &playBtn, &nextBtn, &loopBtn, &triggerBtn }) addAndMakeVisible (b);
    playBtn.lit = true;
    prevBtn.onClick = [this] { proc.stepProg (-1); };
    nextBtn.onClick = [this] { proc.stepProg (1); };
    playBtn.onClick = [this] { proc.setPlaying (! proc.isPlaying()); };
    loopBtn.onClick = [this] { proc.loopOn.store (! proc.loopOn.load()); };
    triggerBtn.setClickingTogglesState (true);
    triggerAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, "trigger", triggerBtn);

    list.setModel (this);
    list.setRowHeight (52);
    list.setOutlineThickness (0);
    list.setColour (juce::ListBox::backgroundColourId, juce::Colours::transparentBlack);
    addAndMakeVisible (list);

    addAndMakeVisible (pads);
    addAndMakeVisible (keys);
    addAndMakeVisible (dragPill);

    for (auto* s : { &volume, &strum })
    {
        s->setSliderStyle (juce::Slider::LinearVertical);
        s->setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
        addAndMakeVisible (s);
    }
    volume.setName ("VOL");
    strum.setName ("STRUM");
    tempo.setSliderStyle (juce::Slider::LinearBar);
    tempo.setTextValueSuffix (" BPM");
    addAndMakeVisible (tempo);
    volumeAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, "volume", volume);
    strumAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, "strum", strum);
    tempoAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, "tempo", tempo);

    for (int i = 0; i < 12; ++i) keyBox.addItem (juce::String ("Key ") + noteNames[i], i + 1);
    keyBox.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (keyBox);
    keyAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, "key", keyBox);

    addChildComponent (flip);

    setSize (1040, 680);
    applyTheme (proc.getEra());
    shownEra = proc.getEra();
    startTimerHz (30);
}

RnBChordsEditor::~RnBChordsEditor()
{
    stopTimer();
    list.setModel (nullptr);
    setLookAndFeel (nullptr);
}

void RnBChordsEditor::applyTheme (int era)
{
    theme = makeTheme (era);
    eraSoul.lit = era == 0; era80.lit = era == 1; era90.lit = era == 2; era00.lit = era == 3; eraNow.lit = era == 4;
    keyBox.setColour (juce::ComboBox::textColourId, theme.text);
    tempo.setColour (juce::Slider::textBoxTextColourId, theme.text);
    tempo.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    tempo.setColour (juce::Slider::trackColourId, juce::Colours::transparentBlack);
    tempo.setColour (juce::Slider::backgroundColourId, juce::Colours::transparentBlack);
    list.updateContent();
    list.selectRow (proc.getProgIndex());
    repaint();
}

void RnBChordsEditor::resized()
{
    GlossButton* eras[5] = { &eraSoul, &era80, &era90, &era00, &eraNow };
    for (int i = 0; i < 5; ++i) eras[i]->setBounds (42, 112 + i * 40, 264, 40);   // stacked pills inside the square tile
    voiceRoot.setBounds (420, 253, 76, 38); voiceClosest.setBounds (494, 253, 92, 38); voiceInv.setBounds (584, 253, 104, 38);
    list.setBounds (36, 380, 276, 264);
    prevBtn.setBounds (734, 136, 60, 60); playBtn.setBounds (798, 126, 80, 80);
    nextBtn.setBounds (882, 136, 60, 60); loopBtn.setBounds (946, 136, 60, 60);
    pads.setBounds (340, 304, 540, 156);
    volume.setBounds (892, 252, 56, 208); strum.setBounds (960, 252, 56, 208);
    triggerBtn.setBounds (354, 482, 200, 60);
    dragPill.setBounds (354, 590, 200, 54);
    keys.setBounds (570, 486, 428, 158);
    keyBox.setBounds (752, 28, 128, 52); tempo.setBounds (888, 28, 132, 52);
    flip.setBounds (getLocalBounds());
}

void RnBChordsEditor::timerCallback()
{
    const int era = proc.getEra();
    if (era != shownEra)
    {
        auto before = createComponentSnapshot (getLocalBounds());
        applyTheme (era);
        shownEra = era; shownProg = -1;
        auto after = createComponentSnapshot (getLocalBounds());
        flip.start (before, after);
    }

    const int prog = proc.getProgIndex() * 16 + proc.getKey();
    if (prog != shownProg)
    {
        shownProg = prog;
        list.selectRow (proc.getProgIndex());
        pads.repaint();
        repaint (nowPanel);
    }

    const int voicing = proc.getVoicingMode();
    int signature = voicing * 100000;
    for (int i = 0; i < ChordDatabase::maxChords; ++i) signature += proc.getChordInversion (i) * (i + 1) * 7;
    if (signature != shownVoicing)
    {
        shownVoicing = signature;
        voiceRoot.lit = voicing == 0; voiceClosest.lit = voicing == 1; voiceInv.lit = voicing == 2;
        voiceRoot.repaint(); voiceClosest.repaint(); voiceInv.repaint();
        pads.repaint();
        repaint (voiceBar);
    }

    playBtn.setIcon (proc.isPlaying() ? GlossButton::stop : GlossButton::play);
    if (loopBtn.lit != proc.loopOn.load()) { loopBtn.lit = proc.loopOn.load(); loopBtn.repaint(); }
    if (pads.active != proc.playingChord()) { pads.active = proc.playingChord(); pads.repaint(); }
    if (keys.active != proc.triggeredPitchClass()) { keys.active = proc.triggeredPitchClass(); keys.repaint(); }
}

int RnBChordsEditor::getNumRows() { return proc.db.numInEra (proc.getEra()); }

void RnBChordsEditor::selectedRowsChanged (int row)
{
    if (row >= 0 && row != proc.getProgIndex()) proc.setProgIndex (row);
}

void RnBChordsEditor::paintListBoxItem (int row, juce::Graphics& g, int width, int height, bool selected)
{
    if (row < 0 || row >= getNumRows()) return;
    auto& prog = proc.db.get (proc.getEra(), row);
    auto r = juce::Rectangle<float> (0.0f, 0.0f, (float) width, (float) height).reduced (2.0f, 3.0f).withTrimmedRight (10.0f);
    if (selected)
    {
        g.setGradientFill (juce::ColourGradient (theme.actA, 0.0f, r.getY(), theme.actB, 0.0f, r.getBottom(), false));
        g.fillRoundedRectangle (r, r.getHeight() * 0.5f);
        if (theme.hardShadow) { g.setColour (theme.panelBorder); g.drawRoundedRectangle (r.reduced (1.0f), r.getHeight() * 0.5f, 2.0f); }
    }
    auto chip = r.removeFromLeft (r.getHeight()).reduced (6.0f);
    g.setColour (selected ? juce::Colours::white.withAlpha (0.85f) : theme.orbB.withAlpha (0.55f));
    g.fillEllipse (chip);
    g.setColour (selected ? theme.actText : theme.orbText);
    g.setFont (gloss::font (12.0f));
    g.drawText (juce::String (row + 1), chip, juce::Justification::centred, false);

    r.removeFromLeft (4.0f); r.removeFromRight (10.0f);
    g.setColour (selected ? theme.actText : theme.text);
    g.setFont (gloss::font (14.0f));
    g.drawText (prog.name, r.removeFromTop (r.getHeight() * 0.52f), juce::Justification::bottomLeft, true);
    g.setColour (selected ? theme.actText : theme.mut);
    g.setFont (gloss::font (12.0f, false));
    g.drawText (prog.roman, r, juce::Justification::topLeft, true);
}

//==============================================================================
void RnBChordsEditor::paintBackground (juce::Graphics& g)
{
    using C = juce::Colour;
    const float W = (float) getWidth(), H = (float) getHeight();
    g.fillAll (theme.bg);

    if (theme.era == 0)   // Soul: warm stage glow on deep brown, gold rule under the title
    {
        g.setGradientFill (juce::ColourGradient (C (0xff3b2015), 0.0f, 0.0f, C (0xff24120b), 0.0f, H, false));
        g.fillRect (0.0f, 0.0f, W, H);
        blob (g, W * 0.50f, H * 0.62f, 400.0f, C (0x4de0a526));
        blob (g, W * 0.10f, H * 0.88f, 320.0f, C (0x612c7a86));
        blob (g, W * 0.92f, H * 0.30f, 300.0f, C (0x61b5482a));
        g.setColour (C (0xffe0a526)); g.fillRect (0.0f, 85.0f, W, 4.0f);
        g.setColour (C (0x99f0b93a)); g.fillRect (0.0f, 91.0f, W, 1.5f);
    }
    else if (theme.era == 1)   // neon sunset over a grid
    {
        juce::ColourGradient sky (C (0xff0b0424), 0.0f, 0.0f, C (0xffff7a3d), 0.0f, 400.0f, false);
        sky.addColour (0.5, C (0xff3a0f7a)); sky.addColour (0.85, C (0xffc2187a));
        g.setGradientFill (sky);
        g.fillRect (0.0f, 0.0f, W, 400.0f);
        {
            juce::Graphics::ScopedSaveState save (g);
            g.reduceClipRegion (0, 0, getWidth(), 400);
            g.setGradientFill (juce::ColourGradient (C (0xffffe14d), 0.0f, 90.0f, C (0xffff2ba6), 0.0f, 510.0f, false));
            g.fillEllipse (W * 0.5f - 210.0f, 90.0f, 420.0f, 420.0f);
            g.setColour (C (0xff8a1668));
            const float stripes[4][2] = { { 300.0f, 3.0f }, { 322.0f, 5.0f }, { 346.0f, 7.0f }, { 372.0f, 9.0f } };
            for (auto& s : stripes) g.fillRect (W * 0.5f - 220.0f, s[0], 440.0f, s[1]);
        }
        g.setColour (C (0xff07021a));
        g.fillRect (0.0f, 400.0f, W, H - 400.0f);
        for (int i = -14; i <= 14; ++i)
        {
            g.setColour (i % 2 == 0 ? C (0xffff2bd6) : C (0xff6a7bff));
            g.drawLine (W * 0.5f, 400.0f, W * 0.5f + (float) i * 150.0f, H, 1.5f);
        }
        g.setColour (C (0xffff2bd6));
        for (float yy : { 404.0f, 414.0f, 430.0f, 452.0f, 480.0f, 516.0f, 560.0f, 612.0f, 672.0f }) g.fillRect (0.0f, yy, W, 1.5f);
        g.setColour (C (0xffff9bea));
        g.fillRect (0.0f, 398.0f, W, 4.0f);
    }
    else if (theme.era == 2)   // checkerboard and colour blocks
    {
        g.setColour (C (0xff222222));
        for (int yy = 0; yy < getHeight(); yy += 48)
            for (int xx = ((yy / 48) % 2) * 48; xx < getWidth(); xx += 96)
                g.fillRect (xx, yy, 48, 48);
        g.setColour (C (0xff2fb5a6)); g.fillRect (0, 0, 452, 96);
        g.setColour (C (0xff222222)); g.fillRect (452, 0, 3, 96); g.fillRect (0, 96 - 3, 455, 3);
        g.setColour (C (0xfff2c230)); g.fillRect (getWidth() - 70, 0, 70, 200);
        g.setColour (C (0xff6fa3e6)); g.fillRect (getWidth() - 120, getHeight() - 150, 120, 150);
        g.setColour (C (0xfff25cb6)); g.fillRect (0, getHeight() - 110, 150, 110);
    }
    else if (theme.era == 3)   // holographic pastel haze
    {
        blob (g, W * 0.15f, H * 0.20f, 380.0f, C (0xffbfe9ff));
        blob (g, W * 0.88f, H * 0.80f, 400.0f, C (0xffc7b5ff));
        blob (g, W * 0.70f, H * 0.10f, 320.0f, C (0xffffc8f2));
        blob (g, W * 0.25f, H * 0.95f, 300.0f, C (0xffb8ffe6));
        sparkle (g, 420.0f, 30.0f, 13.0f); sparkle (g, 444.0f, 60.0f, 7.0f);
    }
    else   // Modern: dusty maroon with oxblood shadows and a blush glow
    {
        blob (g, W * 0.14f, H * 0.16f, 380.0f, C (0xd9220c0f));
        blob (g, W * 0.90f, H * 0.90f, 380.0f, C (0xcc220c0f));
        blob (g, W * 0.74f, H * 0.14f, 300.0f, C (0x6bf2b8b5));
        blob (g, W * 0.24f, H * 0.92f, 280.0f, C (0x61e2463f));
    }
}

void RnBChordsEditor::paintDecor (juce::Graphics& g, juce::Rectangle<float> r)
{
    juce::Graphics::ScopedSaveState save (g);
    juce::Path clip; clip.addRoundedRectangle (r, 34.0f);
    g.reduceClipRegion (clip);
    const float x = r.getX(), y = r.getY();
    if (theme.era == 4)   // a soft calla lily
    {
        const auto at = juce::AffineTransform::translation (x + 236.0f, y - 14.0f);
        g.setColour (juce::Colour (0xff66733f));
        g.strokePath (juce::Drawable::parseSVGPath ("M78 78 C84 110 96 140 124 172"),
                      juce::PathStrokeType (5.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded), at);
        g.setColour (juce::Colour (0xfff2b8b5));
        g.fillPath (juce::Drawable::parseSVGPath ("M40 22 C58 4 96 8 104 34 C110 56 96 80 78 84 C70 70 50 60 44 44 C41 36 38 28 40 22 Z"), at);
        g.setColour (juce::Colour (0xfffbdad6));
        g.fillPath (juce::Drawable::parseSVGPath ("M48 26 C62 14 90 18 96 36 C100 52 90 68 78 74 C72 62 58 54 52 42 C50 36 47 30 48 26 Z"), at);
        g.setColour (juce::Colour (0xffe79a94));
        g.strokePath (juce::Drawable::parseSVGPath ("M40 22 C52 40 66 60 78 84"), juce::PathStrokeType (2.0f), at);
        g.setColour (juce::Colour (0xfff4c56e));
        g.strokePath (juce::Drawable::parseSVGPath ("M66 40 C70 50 74 60 77 70"),
                      juce::PathStrokeType (5.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded), at);
    }
    else if (theme.era == 0)   // a record
    {
        const float cx = x + 316.0f, cy = y + 70.0f;
        g.setColour (juce::Colour (0xff150a05)); g.fillEllipse (cx - 58.0f, cy - 58.0f, 116.0f, 116.0f);
        g.setColour (juce::Colour (0x80f0b93a)); g.drawEllipse (cx - 58.0f, cy - 58.0f, 116.0f, 116.0f, 1.5f);
        g.setColour (juce::Colour (0x29ffecbe));
        for (float rr : { 30.0f, 36.0f, 42.0f, 48.0f, 54.0f }) g.drawEllipse (cx - rr, cy - rr, rr * 2.0f, rr * 2.0f, 1.0f);
        g.setColour (juce::Colour (0xffb5482a)); g.fillEllipse (cx - 23.5f, cy - 23.5f, 47.0f, 47.0f);
        g.setColour (juce::Colour (0xffe0a526)); g.fillEllipse (cx - 18.5f, cy - 18.5f, 37.0f, 37.0f);
        g.setColour (juce::Colour (0xff150a05)); g.fillEllipse (cx - 3.5f, cy - 3.5f, 7.0f, 7.0f);
    }
    else if (theme.era == 3)
    {
        sparkle (g, x + 300.0f, y + 30.0f, 14.0f); sparkle (g, x + 350.0f, y + 100.0f, 9.0f); sparkle (g, x + 262.0f, y + 110.0f, 6.0f);
    }
    else if (theme.era == 2)
    {
        g.setColour (juce::Colour (0xfff25cb6)); g.fillEllipse (x + 258.0f, y - 34.0f, 104.0f, 104.0f);
        juce::Path sq;
        sq.startNewSubPath (x + 206.0f, y + 128.0f);
        sq.cubicTo (x + 230.0f, y + 48.0f, x + 250.0f, y + 48.0f, x + 262.0f, y + 98.0f);
        sq.cubicTo (x + 274.0f, y + 148.0f, x + 296.0f, y + 138.0f, x + 312.0f, y + 78.0f);
        sq.cubicTo (x + 328.0f, y + 18.0f, x + 350.0f, y + 38.0f, x + 370.0f, y + 88.0f);
        g.setColour (juce::Colour (0xfff2c230));
        g.strokePath (sq, juce::PathStrokeType (9.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
}

void RnBChordsEditor::paint (juce::Graphics& g)
{
    paintBackground (g);

    // title: "RnB Chords" with a slightly smaller "by Dev Griffin"
    const auto titleFont = gloss::font (36.0f), byFont = gloss::font (22.0f);
    g.setColour (theme.title);
    g.setFont (titleFont);
    g.drawSingleLineText ("RnB Chords", 24, 66);
    g.setFont (byFont);
    g.drawSingleLineText ("by Dev Griffin", 24 + (int) textWidth (titleFont, "RnB Chords") + 10, 66);

    for (auto r : { eraTile, listPanel, nowPanel, midiPanel })
        gloss::panel (g, r.toFloat(), 34.0f, theme);

    // voicing bar above the pads
    gloss::panel (g, voiceBar.toFloat(), 20.0f, theme);
    g.setColour (theme.mut);
    g.setFont (gloss::font (11.0f));
    g.drawText ("VOICING", voiceBar.getX() + 16, voiceBar.getY(), 64, voiceBar.getHeight(), juce::Justification::centredLeft, false);
    static const char* const hints[3] = { "Root note at the bottom", "Smallest move between chords", "Set each chord on its pad" };
    g.setFont (gloss::font (12.0f, false));
    g.drawText (hints[juce::jlimit (0, 2, proc.getVoicingMode())], voiceBar.getX() + 352, voiceBar.getY(), 176, voiceBar.getHeight(),
                juce::Justification::centredRight, true);

    auto& prog = proc.currentProg();
    g.setColour (theme.mut);
    g.setFont (gloss::font (11.0f));
    g.drawText ((proc.db.eraNames[proc.getEra()] + " progressions").toUpperCase(), 46, 356, 250, 18, juce::Justification::centredLeft, true);

    paintDecor (g, nowPanel.toFloat());
    auto text = nowPanel.reduced (22, 16).withWidth (380);
    g.setColour (theme.mut);
    g.setFont (gloss::font (11.0f));
    g.drawText (("Now playing  |  " + prog.style).toUpperCase(), text.removeFromTop (16), juce::Justification::centredLeft, true);
    g.setColour (theme.text);
    g.setFont (gloss::font (32.0f));
    g.drawFittedText (prog.name, text.removeFromTop (42), juce::Justification::centredLeft, 1, 0.6f);
    g.setFont (gloss::font (15.0f));
    g.drawText (prog.roman, text.removeFromTop (22), juce::Justification::centredLeft, true);
    if (prog.songs.isNotEmpty())
    {
        g.setColour (theme.mut);
        g.setFont (gloss::font (12.0f, false));
        g.drawText ("Heard in: " + prog.songs, text.removeFromTop (20), juce::Justification::centredLeft, true);
    }

    g.setColour (theme.mut);
    g.setFont (gloss::font (12.0f, false));
    g.drawFittedText ("White keys play chords in key. Black keys play borrowed chords.", 362, 544, 190, 40, juce::Justification::centredLeft, 2);
}
