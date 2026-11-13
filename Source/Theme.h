#pragma once
#include <JuceHeader.h>

// One colour set per era. Era index: 0 = 80s, 1 = 90s, 2 = 2000s, 3 = Modern (Pink Jelly).
struct Theme
{
    int era = 3;
    juce::Colour bg, text, mut;
    juce::Colour panelA, panelB, panelBorder;
    float borderW = 1.5f;
    bool hardShadow = false;
    juce::Colour orbA, orbB, orbText, orbBorder;
    juce::Colour actA, actB, actText;
    juce::Colour wkA, wkB, wkText, bkA, bkB;
    juce::Colour capBgA, capBgB;
    float sheen = 0.6f;
};

inline Theme makeTheme (int era)
{
    using C = juce::Colour;
    Theme t; t.era = era;
    switch (era)
    {
        case 0: // 80s neon sunset
            t.bg = C (0xff12062e); t.text = juce::Colours::white; t.mut = C (0xfff3c6ff);
            t.panelA = C (0xb32c0e60); t.panelB = C (0xd60a041e); t.panelBorder = C (0xe6ff4fdc);
            t.orbA = C (0xff7658e6); t.orbB = C (0xff0a0424); t.orbText = C (0xff8fecff); t.orbBorder = C (0xcc6ed2ff);
            t.actA = C (0xffffe14d); t.actB = C (0xffff3fa8); t.actText = C (0xff1a0b3b);
            t.wkA = C (0xfff6e9ff); t.wkB = C (0xffd9b8ff); t.wkText = C (0xff1a0b3b); t.bkA = C (0xff3a1f8a); t.bkB = C (0xff0a0424);
            t.capBgA = C (0xff2a1266); t.capBgB = C (0xff0a0424); t.sheen = 0.5f;
            break;
        case 1: // 90s Memphis
            t.bg = juce::Colours::white; t.text = C (0xff222222); t.mut = C (0xff3d3d3d);
            t.panelA = juce::Colours::white; t.panelB = C (0xfff1edf8); t.panelBorder = C (0xff222222);
            t.borderW = 3.0f; t.hardShadow = true;
            t.orbA = C (0xff8fe6db); t.orbB = C (0xff1f9488); t.orbText = C (0xff222222); t.orbBorder = C (0xff222222);
            t.actA = C (0xffffa6db); t.actB = C (0xffdd44a0); t.actText = C (0xff222222);
            t.wkA = juce::Colours::white; t.wkB = C (0xffefeaf7); t.wkText = C (0xff222222); t.bkA = C (0xff5a5a5a); t.bkB = C (0xff222222);
            t.capBgA = juce::Colours::white; t.capBgB = C (0xfff1edf8); t.sheen = 0.7f;
            break;
        case 2: // 2000s Y2K chrome
            t.bg = C (0xffd5d9f2); t.text = C (0xff14184a); t.mut = C (0xff333a78);
            t.panelA = C (0xbdffffff); t.panelB = C (0x5cffffff); t.panelBorder = C (0xf2ffffff);
            t.orbA = juce::Colours::white; t.orbB = C (0xff98a2c6); t.orbText = C (0xff14184a); t.orbBorder = C (0x8c5a64aa);
            t.actA = C (0xff9beeff); t.actB = C (0xffffaceb); t.actText = C (0xff14184a);
            t.wkA = juce::Colours::white; t.wkB = C (0xffdde2f6); t.wkText = C (0xff14184a); t.bkA = C (0xff5a6290); t.bkB = C (0xff14184a);
            t.capBgA = C (0xbdffffff); t.capBgB = C (0x5cffffff); t.sheen = 0.8f;
            break;
        default: // Modern: Pink Jelly
            t.bg = C (0xffb23a6e); t.text = juce::Colours::white; t.mut = C (0xffffcbe2);
            t.panelA = C (0xa82c081a); t.panelB = C (0xc712030b); t.panelBorder = C (0xccffd6ea);
            t.orbA = C (0xff55555e); t.orbB = C (0xff050506); t.orbText = C (0xffff6db3); t.orbBorder = C (0x4dffffff);
            t.actA = C (0xffff9ccb); t.actB = C (0xffd4177a); t.actText = C (0xff14060c);
            t.wkA = C (0xfffff2f8); t.wkB = C (0xffffc4df); t.wkText = C (0xff14060c); t.bkA = C (0xff4a4a52); t.bkB = C (0xff050506);
            t.capBgA = C (0xff2a2a30); t.capBgB = C (0xff050506); t.sheen = 0.6f;
            break;
    }
    return t;
}

namespace gloss
{
inline juce::Font font (float size, bool bold = true)
{
    return juce::Font (juce::FontOptions (size, bold ? juce::Font::bold : juce::Font::plain));
}

// A rounded glass panel or pill, with a glossy highlight along the top.
inline void panel (juce::Graphics& g, juce::Rectangle<float> r, float radius, const Theme& t, bool active = false)
{
    if (t.hardShadow)
    {
        g.setColour (t.panelBorder);
        g.fillRoundedRectangle (r.translated (5.0f, 5.0f), radius);
    }
    else
    {
        g.setColour (juce::Colours::black.withAlpha (0.22f));
        g.fillRoundedRectangle (r.translated (0.0f, 6.0f).expanded (1.0f), radius);
    }
    g.setGradientFill (juce::ColourGradient (active ? t.actA : t.panelA, r.getX(), r.getY(),
                                             active ? t.actB : t.panelB, r.getX() + r.getWidth() * 0.3f, r.getBottom(), false));
    g.fillRoundedRectangle (r, radius);

    auto shine = r.reduced (radius * 0.35f, 3.0f).withHeight (juce::jmin (r.getHeight() * 0.42f, 46.0f));
    g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (t.sheen * (active ? 0.75f : 0.32f)), 0.0f, shine.getY(),
                                             juce::Colours::transparentWhite, 0.0f, shine.getBottom(), false));
    g.fillRoundedRectangle (shine, juce::jmin (radius * 0.8f, shine.getHeight() * 0.5f));

    g.setColour (t.panelBorder);
    g.drawRoundedRectangle (r.reduced (t.borderW * 0.5f), radius, t.borderW);
}

// A glossy sphere button.
inline void orb (juce::Graphics& g, juce::Rectangle<float> r, const Theme& t, bool active)
{
    if (t.hardShadow) { g.setColour (t.panelBorder); g.fillEllipse (r.translated (3.0f, 3.0f)); }
    else              { g.setColour (juce::Colours::black.withAlpha (0.28f)); g.fillEllipse (r.translated (0.0f, 4.0f)); }

    const auto a = active ? t.actA : t.orbA, b = active ? t.actB : t.orbB;
    g.setGradientFill (juce::ColourGradient (a, r.getX() + r.getWidth() * 0.35f, r.getY() + r.getHeight() * 0.2f,
                                             b, r.getCentreX(), r.getBottom() + r.getHeight() * 0.15f, true));
    g.fillEllipse (r);

    auto shine = juce::Rectangle<float> (r.getX() + r.getWidth() * 0.16f, r.getY() + r.getHeight() * 0.06f,
                                         r.getWidth() * 0.68f, r.getHeight() * 0.38f);
    g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (t.sheen), 0.0f, shine.getY(),
                                             juce::Colours::transparentWhite, 0.0f, shine.getBottom(), false));
    g.fillEllipse (shine);

    g.setColour (t.orbBorder);
    g.drawEllipse (r.reduced (t.hardShadow ? 1.5f : 0.5f), t.hardShadow ? 3.0f : 1.0f);
}
}
