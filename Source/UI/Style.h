#pragma once
// "Leitstand" look: a Soviet power-station control panel. Turquoise-grey enamel, cream label plates,
// bakelite knobs with aluminium caps, incandescent lamps, nixie tubes and green phosphor screens.

#include <juce_gui_extra/juce_gui_extra.h>

namespace ui
{
/** Cyrillic (or any UTF-8) literal to juce::String. */
inline juce::String cyr (const char* utf8) { return juce::String::fromUTF8 (utf8); }
/** " · " separator. */
inline juce::String dot() { return juce::String::fromUTF8 ("  \xc2\xb7  "); }

namespace col
{
const juce::Colour room        { 0xff121614 };
const juce::Colour frame       { 0xff222a27 };
const juce::Colour frameLight  { 0xff36403c };
const juce::Colour enamel      { 0xff6b8b84 };
const juce::Colour enamelLight { 0xff83a39b };
const juce::Colour enamelDark  { 0xff4c6862 };
const juce::Colour enamelDeep  { 0xff314541 };
const juce::Colour cream       { 0xffe8dec3 };
const juce::Colour creamDark   { 0xffcbbd98 };
const juce::Colour ink         { 0xff1c1a16 };
const juce::Colour inkSoft     { 0xff4a4337 };
const juce::Colour red         { 0xffb3261e };
const juce::Colour redDark     { 0xff6f150f };
const juce::Colour amber       { 0xffffb04a };
const juce::Colour amberGlow   { 0xffff7a12 };
const juce::Colour green       { 0xff8cff74 };
const juce::Colour nixie       { 0xffff7b24 };
const juce::Colour nixieCore   { 0xffffd8a6 };
const juce::Colour phosphor    { 0xff7dffa4 };
const juce::Colour phosphorDim { 0xff2f8f56 };
const juce::Colour grid        { 0xff184a2c };
const juce::Colour screen      { 0xff03100a };
const juce::Colour bakelite    { 0xff1a1613 };
const juce::Colour chrome      { 0xffd9ddd9 };
const juce::Colour chromeDark  { 0xff5f6663 };
} // namespace col

// Fonts
juce::Font logo (float height);
juce::Font label (float height, float kerning = 0.08f);   // Oswald SemiBold, for engraved captions
juce::Font regular (float height, float kerning = 0.04f); // Oswald Regular
juce::Font light (float height, float kerning = 0.06f);   // Oswald Light, for Cyrillic subtitles
juce::Font mono (float height);                           // PT Mono, for readouts
juce::Font stencil (float height);                        // station numbers

// Surfaces
void paintEnamel (juce::Graphics&, juce::Rectangle<float> r, juce::Colour base = col::enamel, float corner = 4.0f);
void paintRecess (juce::Graphics&, juce::Rectangle<float> r, float corner = 4.0f);
void paintPlate (juce::Graphics&, juce::Rectangle<float> r, juce::Colour base = col::cream, bool rivets = true);
void paintScrew (juce::Graphics&, juce::Point<float> c, float radius = 5.0f, float angle = 0.6f);
void paintTexture (juce::Graphics&, juce::Rectangle<int> r, float opacity);

// Text
void engraved (juce::Graphics&, const juce::String& text, juce::Rectangle<float> r, const juce::Font&,
               juce::Justification = juce::Justification::centred, juce::Colour = col::cream);
void inked (juce::Graphics&, const juce::String& text, juce::Rectangle<float> r, const juce::Font&,
            juce::Justification = juce::Justification::centred, juce::Colour = col::ink);
/** English caption with a smaller Cyrillic line under it, engraved on enamel. */
void caption (juce::Graphics&, juce::Rectangle<float> r, const juce::String& en, const juce::String& ru,
              juce::Colour = col::cream, float size = 13.0f);

// Indicators
void paintLamp (juce::Graphics&, juce::Point<float> c, float radius, juce::Colour glass, float brightness);
void paintNixieDigit (juce::Graphics&, juce::Rectangle<float> tube, juce::juce_wchar ch, bool lit, float glow = 1.0f);
void paintNixieTube (juce::Graphics&, juce::Rectangle<float> tube);
void paintGlowPath (juce::Graphics&, const juce::Path&, juce::Colour, float width, float glow = 1.0f);

// Small filter-shape icons for the EQ type keys
juce::Path filterIcon (int type, juce::Rectangle<float> r);

/** Caches static drawing at the physical pixel density of the target (window scale and Retina),
    so it stays sharp while only the moving parts are redrawn each frame. */
class LayerCache
{
public:
    template <typename Painter>
    void draw (juce::Graphics& g, juce::Rectangle<int> area, Painter&& painter)
    {
        const float k = std::max (0.25f, g.getInternalContext().getPhysicalPixelScaleFactor());
        const int w = std::max (1, juce::roundToInt ((float) area.getWidth() * k));
        const int h = std::max (1, juce::roundToInt ((float) area.getHeight() * k));
        if (dirty || ! image.isValid() || image.getWidth() != w || image.getHeight() != h)
        {
            image = juce::Image (juce::Image::ARGB, w, h, true);
            juce::Graphics ig (image);
            ig.addTransform (juce::AffineTransform::translation ((float) -area.getX(), (float) -area.getY())
                                 .scaled ((float) w / (float) area.getWidth(), (float) h / (float) area.getHeight()));
            painter (ig);
            dirty = false;
        }
        g.drawImage (image, area.toFloat());
    }

    void invalidate() { dirty = true; }

private:
    juce::Image image;
    bool dirty = true;
};
} // namespace ui
