#include "Style.h"
#include "BinaryData.h"

namespace ui
{
namespace
{
struct Faces
{
    juce::Typeface::Ptr logo, semibold, regular, light, mono, stencil;

    Faces()
    {
        logo     = juce::Typeface::createSystemTypefaceFor (BinaryData::RussoOneRegular_ttf, (size_t) BinaryData::RussoOneRegular_ttfSize);
        semibold = juce::Typeface::createSystemTypefaceFor (BinaryData::OswaldSemiBold_ttf, (size_t) BinaryData::OswaldSemiBold_ttfSize);
        regular  = juce::Typeface::createSystemTypefaceFor (BinaryData::OswaldRegular_ttf, (size_t) BinaryData::OswaldRegular_ttfSize);
        light    = juce::Typeface::createSystemTypefaceFor (BinaryData::OswaldLight_ttf, (size_t) BinaryData::OswaldLight_ttfSize);
        mono     = juce::Typeface::createSystemTypefaceFor (BinaryData::PTMonoRegular_ttf, (size_t) BinaryData::PTMonoRegular_ttfSize);
        stencil  = juce::Typeface::createSystemTypefaceFor (BinaryData::BigShouldersStencilExtraBold_ttf, (size_t) BinaryData::BigShouldersStencilExtraBold_ttfSize);
    }

    static Faces& get()
    {
        static Faces f;
        return f;
    }
};

juce::Font make (const juce::Typeface::Ptr& tf, float h, float kerning = 0.0f)
{
    return juce::Font (juce::FontOptions (tf).withHeight (h).withKerningFactor (kerning));
}

const juce::Image& noiseImage()
{
    static const juce::Image img = []
    {
        juce::Image im (juce::Image::ARGB, 192, 192, true);
        juce::Random rnd (1947);
        for (int y = 0; y < im.getHeight(); ++y)
            for (int x = 0; x < im.getWidth(); ++x)
            {
                const float r = (rnd.nextFloat() + rnd.nextFloat() + rnd.nextFloat()) / 3.0f - 0.5f;
                const auto base = r > 0.0f ? juce::Colours::white : juce::Colour (0xff0b1210);
                im.setPixelAt (x, y, base.withAlpha (juce::jlimit (0.0f, 1.0f, std::abs (r) * 0.5f)));
            }
        // a few soft blotches give the hammer-paint unevenness
        juce::Graphics g (im);
        for (int i = 0; i < 70; ++i)
        {
            const float cx = rnd.nextFloat() * 192.0f, cy = rnd.nextFloat() * 192.0f, rad = 3.0f + rnd.nextFloat() * 9.0f;
            const bool light = rnd.nextBool();
            juce::ColourGradient grad ((light ? juce::Colours::white : juce::Colours::black).withAlpha (0.05f), cx, cy,
                                       juce::Colours::transparentBlack, cx + rad, cy, true);
            g.setGradientFill (grad);
            g.fillEllipse (cx - rad, cy - rad, rad * 2, rad * 2);
        }
        return im;
    }();
    return img;
}

/** Unit-box (10 x 16) wire shapes of IN-14 style nixie cathodes. */
juce::Path nixiePath (juce::juce_wchar ch)
{
    juce::Path p;
    switch (ch)
    {
        case '0': p.addEllipse (1.0f, 1.0f, 8.0f, 14.0f); break;
        case '1':
            p.startNewSubPath (5.2f, 15.0f); p.lineTo (5.2f, 1.0f); p.lineTo (3.0f, 3.2f);
            break;
        case '2':
            p.startNewSubPath (1.2f, 4.4f);
            p.cubicTo (1.4f, 1.8f, 3.2f, 1.0f, 5.0f, 1.0f);
            p.cubicTo (7.2f, 1.0f, 8.8f, 2.4f, 8.8f, 4.6f);
            p.cubicTo (8.8f, 7.2f, 5.0f, 9.8f, 1.2f, 15.0f);
            p.lineTo (8.9f, 15.0f);
            break;
        case '3':
            p.startNewSubPath (1.4f, 3.4f);
            p.cubicTo (2.0f, 1.8f, 3.4f, 1.0f, 5.0f, 1.0f);
            p.cubicTo (7.2f, 1.0f, 8.6f, 2.4f, 8.6f, 4.3f);
            p.cubicTo (8.6f, 6.4f, 7.0f, 7.6f, 4.4f, 7.6f);
            p.cubicTo (7.4f, 7.6f, 9.0f, 9.4f, 9.0f, 11.5f);
            p.cubicTo (9.0f, 13.8f, 7.2f, 15.0f, 5.0f, 15.0f);
            p.cubicTo (3.0f, 15.0f, 1.6f, 14.2f, 1.0f, 12.6f);
            break;
        case '4':
            p.startNewSubPath (7.0f, 15.0f); p.lineTo (7.0f, 1.0f); p.lineTo (1.0f, 10.8f); p.lineTo (9.2f, 10.8f);
            break;
        case '5':
            p.startNewSubPath (8.6f, 1.0f); p.lineTo (2.1f, 1.0f); p.lineTo (1.6f, 7.2f);
            p.cubicTo (2.6f, 6.4f, 3.8f, 6.1f, 5.0f, 6.1f);
            p.cubicTo (7.6f, 6.1f, 9.0f, 8.0f, 9.0f, 10.6f);
            p.cubicTo (9.0f, 13.4f, 7.2f, 15.0f, 4.8f, 15.0f);
            p.cubicTo (3.0f, 15.0f, 1.6f, 14.2f, 1.0f, 12.8f);
            break;
        case '6':
            p.startNewSubPath (8.2f, 2.4f);
            p.cubicTo (7.4f, 1.4f, 6.3f, 1.0f, 5.1f, 1.0f);
            p.cubicTo (2.4f, 1.0f, 1.0f, 4.2f, 1.0f, 8.6f);
            p.cubicTo (1.0f, 12.6f, 2.6f, 15.0f, 5.0f, 15.0f);
            p.cubicTo (7.4f, 15.0f, 9.0f, 13.2f, 9.0f, 10.6f);
            p.cubicTo (9.0f, 8.0f, 7.4f, 6.4f, 5.0f, 6.4f);
            p.cubicTo (2.8f, 6.4f, 1.2f, 8.0f, 1.0f, 10.0f);
            break;
        case '7':
            p.startNewSubPath (1.0f, 1.0f); p.lineTo (9.0f, 1.0f); p.lineTo (3.4f, 15.0f);
            break;
        case '8':
            p.addEllipse (1.8f, 1.0f, 6.4f, 6.4f);
            p.addEllipse (1.0f, 7.4f, 8.0f, 7.6f);
            break;
        case '9':
            p.startNewSubPath (9.0f, 6.0f);
            p.cubicTo (8.8f, 8.0f, 7.2f, 9.6f, 5.0f, 9.6f);
            p.cubicTo (2.6f, 9.6f, 1.0f, 8.0f, 1.0f, 5.4f);
            p.cubicTo (1.0f, 2.8f, 2.6f, 1.0f, 5.0f, 1.0f);
            p.cubicTo (7.6f, 1.0f, 9.0f, 3.4f, 9.0f, 7.4f);
            p.cubicTo (9.0f, 11.8f, 7.6f, 15.0f, 4.9f, 15.0f);
            p.cubicTo (3.7f, 15.0f, 2.6f, 14.6f, 1.8f, 13.6f);
            break;
        case '-': p.startNewSubPath (2.0f, 8.2f); p.lineTo (8.0f, 8.2f); break;
        case '+': p.startNewSubPath (2.0f, 8.2f); p.lineTo (8.0f, 8.2f); p.startNewSubPath (5.0f, 5.2f); p.lineTo (5.0f, 11.2f); break;
        case '.': p.addEllipse (9.3f, 14.3f, 1.1f, 1.1f); break;
        default: break;
    }
    return p;
}
} // namespace

juce::Font logo (float h)                 { return make (Faces::get().logo, h, 0.02f); }
juce::Font label (float h, float k)       { return make (Faces::get().semibold, h, k); }
juce::Font regular (float h, float k)     { return make (Faces::get().regular, h, k); }
juce::Font light (float h, float k)       { return make (Faces::get().light, h, k); }
juce::Font mono (float h)                 { return make (Faces::get().mono, h); }
juce::Font stencil (float h)              { return make (Faces::get().stencil, h, 0.02f); }

//==============================================================================
void paintTexture (juce::Graphics& g, juce::Rectangle<int> r, float opacity)
{
    juce::Graphics::ScopedSaveState s (g);
    g.reduceClipRegion (r);
    g.setTiledImageFill (noiseImage(), 0, 0, opacity);
    g.fillRect (r);
}

void paintEnamel (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour base, float corner)
{
    g.setColour (juce::Colours::black.withAlpha (0.55f));
    g.fillRoundedRectangle (r.expanded (1.0f), corner + 1.0f);

    juce::ColourGradient grad (base.brighter (0.07f), r.getX(), r.getY(), base.darker (0.14f), r.getX(), r.getBottom(), false);
    g.setGradientFill (grad);
    g.fillRoundedRectangle (r, corner);

    {
        juce::Graphics::ScopedSaveState s (g);
        juce::Path clip;
        clip.addRoundedRectangle (r, corner);
        g.reduceClipRegion (clip);
        paintTexture (g, r.toNearestInt(), 0.32f);
    }

    g.setColour (juce::Colours::white.withAlpha (0.22f));
    g.drawLine (r.getX() + corner, r.getY() + 0.8f, r.getRight() - corner, r.getY() + 0.8f, 1.0f);
    g.drawLine (r.getX() + 0.8f, r.getY() + corner, r.getX() + 0.8f, r.getBottom() - corner, 0.8f);
    g.setColour (juce::Colours::black.withAlpha (0.30f));
    g.drawLine (r.getX() + corner, r.getBottom() - 0.8f, r.getRight() - corner, r.getBottom() - 0.8f, 1.0f);
    g.drawLine (r.getRight() - 0.8f, r.getY() + corner, r.getRight() - 0.8f, r.getBottom() - corner, 0.8f);
}

void paintRecess (juce::Graphics& g, juce::Rectangle<float> r, float corner)
{
    g.setColour (col::enamelDeep);
    g.fillRoundedRectangle (r, corner);
    juce::ColourGradient shade (juce::Colours::black.withAlpha (0.45f), r.getX(), r.getY(),
                                juce::Colours::transparentBlack, r.getX(), r.getY() + 10.0f, false);
    g.setGradientFill (shade);
    g.fillRoundedRectangle (r, corner);
    g.setColour (juce::Colours::white.withAlpha (0.16f));
    g.drawLine (r.getX() + corner, r.getBottom() + 0.5f, r.getRight() - corner, r.getBottom() + 0.5f, 1.0f);
    g.setColour (juce::Colours::black.withAlpha (0.5f));
    g.drawRoundedRectangle (r, corner, 1.0f);
}

void paintScrew (juce::Graphics& g, juce::Point<float> c, float r, float angle)
{
    g.setColour (juce::Colours::black.withAlpha (0.5f));
    g.fillEllipse (c.x - r - 1.0f, c.y - r - 0.5f, (r + 1.0f) * 2.0f, (r + 1.0f) * 2.0f);
    juce::ColourGradient grad (juce::Colour (0xffeef0ec), c.x - r * 0.5f, c.y - r * 0.6f, juce::Colour (0xff4a504d), c.x + r, c.y + r, true);
    grad.addColour (0.5, juce::Colour (0xff9aa19d));
    g.setGradientFill (grad);
    g.fillEllipse (c.x - r, c.y - r, r * 2.0f, r * 2.0f);
    const juce::Point<float> d (std::cos (angle) * (r - 1.2f), std::sin (angle) * (r - 1.2f));
    g.setColour (juce::Colours::white.withAlpha (0.35f));
    g.drawLine (c.x - d.x, c.y - d.y + 0.8f, c.x + d.x, c.y + d.y + 0.8f, 1.6f);
    g.setColour (juce::Colour (0xff2b2f2d));
    g.drawLine (c.x - d.x, c.y - d.y, c.x + d.x, c.y + d.y, 1.6f);
}

void paintPlate (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour base, bool rivets)
{
    g.setColour (juce::Colours::black.withAlpha (0.45f));
    g.fillRoundedRectangle (r.translated (0.0f, 1.5f).expanded (0.5f), 2.5f);
    juce::ColourGradient grad (base.brighter (0.06f), r.getX(), r.getY(), base.darker (0.10f), r.getX(), r.getBottom(), false);
    g.setGradientFill (grad);
    g.fillRoundedRectangle (r, 2.0f);
    {
        juce::Graphics::ScopedSaveState s (g);
        g.reduceClipRegion (r.toNearestInt());
        paintTexture (g, r.toNearestInt(), 0.18f);
    }
    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.drawRoundedRectangle (r, 2.0f, 1.0f);
    g.setColour (juce::Colours::white.withAlpha (0.35f));
    g.drawLine (r.getX() + 2.0f, r.getY() + 1.0f, r.getRight() - 2.0f, r.getY() + 1.0f, 0.8f);
    if (rivets && r.getWidth() > 30 && r.getHeight() > 16)
        for (auto p : { r.getTopLeft() + juce::Point<float> (5, 5), r.getTopRight() + juce::Point<float> (-5, 5),
                        r.getBottomLeft() + juce::Point<float> (5, -5), r.getBottomRight() + juce::Point<float> (-5, -5) })
        {
            juce::ColourGradient rg (base.brighter (0.4f), p.x - 1.0f, p.y - 1.0f, base.darker (0.5f), p.x + 2.0f, p.y + 2.0f, true);
            g.setGradientFill (rg);
            g.fillEllipse (p.x - 2.0f, p.y - 2.0f, 4.0f, 4.0f);
        }
}

//==============================================================================
void engraved (juce::Graphics& g, const juce::String& text, juce::Rectangle<float> r, const juce::Font& f,
               juce::Justification j, juce::Colour c)
{
    g.setFont (f);
    g.setColour (juce::Colours::black.withAlpha (0.45f));
    g.drawText (text, r.translated (0.0f, 1.0f), j, false);
    g.setColour (c);
    g.drawText (text, r, j, false);
}

void inked (juce::Graphics& g, const juce::String& text, juce::Rectangle<float> r, const juce::Font& f,
            juce::Justification j, juce::Colour c)
{
    g.setFont (f);
    g.setColour (juce::Colours::white.withAlpha (0.4f));
    g.drawText (text, r.translated (0.0f, 0.8f), j, false);
    g.setColour (c);
    g.drawText (text, r, j, false);
}

void caption (juce::Graphics& g, juce::Rectangle<float> r, const juce::String& en, const juce::String& ru, juce::Colour c, float size)
{
    const float h1 = size * 1.15f;
    engraved (g, en, r.removeFromTop (h1), label (size), juce::Justification::centred, c);
    if (ru.isNotEmpty())
        engraved (g, ru, r.removeFromTop (size * 0.95f), light (size * 0.72f, 0.1f), juce::Justification::centred, c.withAlpha (0.72f));
}

//==============================================================================
void paintLamp (juce::Graphics& g, juce::Point<float> c, float r, juce::Colour glass, float b)
{
    b = juce::jlimit (0.0f, 1.0f, b);
    if (b > 0.01f)
    {
        juce::ColourGradient glow (glass.withAlpha (0.45f * b), c.x, c.y, glass.withAlpha (0.0f), c.x + r * 3.2f, c.y, true);
        g.setGradientFill (glow);
        g.fillEllipse (c.x - r * 3.2f, c.y - r * 3.2f, r * 6.4f, r * 6.4f);
    }
    const float br = r * 1.38f;
    juce::ColourGradient bezel (col::chrome, c.x - br, c.y - br, col::chromeDark, c.x + br, c.y + br, false);
    g.setGradientFill (bezel);
    g.fillEllipse (c.x - br, c.y - br, br * 2.0f, br * 2.0f);
    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.drawEllipse (c.x - br, c.y - br, br * 2.0f, br * 2.0f, 0.8f);

    const auto off = glass.darker (1.6f).withMultipliedSaturation (0.7f);
    const auto on = glass.brighter (0.25f);
    const auto mid = off.interpolatedWith (on, b);
    juce::ColourGradient lens (mid.brighter (0.4f + 0.6f * b), c.x - r * 0.3f, c.y - r * 0.3f, mid.darker (0.5f), c.x + r, c.y + r, true);
    g.setGradientFill (lens);
    g.fillEllipse (c.x - r, c.y - r, r * 2.0f, r * 2.0f);
    if (b > 0.01f)
    {
        juce::ColourGradient hot (juce::Colours::white.withAlpha (0.75f * b), c.x, c.y, on.withAlpha (0.0f), c.x + r * 0.8f, c.y, true);
        g.setGradientFill (hot);
        g.fillEllipse (c.x - r * 0.8f, c.y - r * 0.8f, r * 1.6f, r * 1.6f);
    }
    g.setColour (juce::Colours::white.withAlpha (0.55f));
    g.fillEllipse (c.x - r * 0.55f, c.y - r * 0.62f, r * 0.5f, r * 0.34f);
}

void paintGlowPath (juce::Graphics& g, const juce::Path& p, juce::Colour c, float w, float glow)
{
    const juce::PathStrokeType::JointStyle j = juce::PathStrokeType::curved;
    const juce::PathStrokeType::EndCapStyle e = juce::PathStrokeType::rounded;
    if (glow > 0.0f)
    {
        g.setColour (c.withAlpha (0.08f * glow));
        g.strokePath (p, juce::PathStrokeType (w * 6.0f, j, e));
        g.setColour (c.withAlpha (0.16f * glow));
        g.strokePath (p, juce::PathStrokeType (w * 3.2f, j, e));
    }
    g.setColour (c.withAlpha (0.9f));
    g.strokePath (p, juce::PathStrokeType (w * 1.3f, j, e));
    g.setColour (c.brighter (0.8f).withAlpha (0.9f));
    g.strokePath (p, juce::PathStrokeType (w * 0.55f, j, e));
}

void paintNixieTube (juce::Graphics& g, juce::Rectangle<float> t)
{
    const float corner = t.getWidth() * 0.38f;
    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.fillRoundedRectangle (t.expanded (1.5f), corner);
    juce::ColourGradient glass (juce::Colour (0xff2a1b13), t.getX(), t.getY(), juce::Colour (0xff0d0806), t.getRight(), t.getBottom(), false);
    g.setGradientFill (glass);
    g.fillRoundedRectangle (t, corner);

    // anode mesh
    {
        juce::Graphics::ScopedSaveState s (g);
        juce::Path clip;
        clip.addRoundedRectangle (t.reduced (2.0f), corner);
        g.reduceClipRegion (clip);
        g.setColour (juce::Colour (0xffc06a30).withAlpha (0.10f));
        const float step = std::max (2.6f, t.getWidth() * 0.13f);
        for (float x = t.getX() - t.getHeight(); x < t.getRight(); x += step)
        {
            g.drawLine (x, t.getBottom(), x + t.getHeight() * 0.58f, t.getY(), 0.6f);
            g.drawLine (x + t.getHeight() * 0.58f, t.getBottom(), x, t.getY(), 0.6f);
        }
    }

    // unlit cathodes stacked behind
    const auto box = t.reduced (t.getWidth() * 0.2f, t.getHeight() * 0.13f);
    const auto tr = juce::AffineTransform::scale (box.getWidth() / 10.0f, box.getHeight() / 16.0f).translated (box.getX(), box.getY());
    for (auto ch : { '8', '0', '3' })
    {
        auto p = nixiePath (ch);
        p.applyTransform (tr);
        g.setColour (juce::Colour (0xff3d2a1f).withAlpha (0.8f));
        g.strokePath (p, juce::PathStrokeType (std::max (0.8f, t.getWidth() * 0.035f)));
    }

    // glass highlights
    g.setColour (juce::Colours::white.withAlpha (0.09f));
    g.fillRoundedRectangle (t.getX() + t.getWidth() * 0.12f, t.getY() + t.getHeight() * 0.08f, t.getWidth() * 0.12f, t.getHeight() * 0.7f, 2.0f);
    g.setColour (juce::Colours::black.withAlpha (0.7f));
    g.drawRoundedRectangle (t, corner, 1.0f);
}

void paintNixieDigit (juce::Graphics& g, juce::Rectangle<float> t, juce::juce_wchar ch, bool lit, float glow)
{
    if (! lit || ch == ' ')
        return;
    const auto box = t.reduced (t.getWidth() * 0.2f, t.getHeight() * 0.13f);
    auto p = nixiePath (ch);
    p.applyTransform (juce::AffineTransform::scale (box.getWidth() / 10.0f, box.getHeight() / 16.0f).translated (box.getX(), box.getY()));
    paintGlowPath (g, p, col::nixie, std::max (1.0f, t.getWidth() * 0.06f), glow);
}

//==============================================================================
juce::Path filterIcon (int type, juce::Rectangle<float> r)
{
    juce::Path p;
    const float x0 = r.getX(), x1 = r.getRight(), w = r.getWidth();
    const float mid = r.getCentreY(), up = r.getY() + r.getHeight() * 0.15f, dn = r.getBottom() - r.getHeight() * 0.15f;
    switch (type)
    {
        case 0: // bell
            p.startNewSubPath (x0, mid);
            p.lineTo (x0 + w * 0.25f, mid);
            p.cubicTo (x0 + w * 0.42f, mid, x0 + w * 0.40f, up, x0 + w * 0.5f, up);
            p.cubicTo (x0 + w * 0.60f, up, x0 + w * 0.58f, mid, x0 + w * 0.75f, mid);
            p.lineTo (x1, mid);
            break;
        case 1: // low shelf
            p.startNewSubPath (x0, up);
            p.lineTo (x0 + w * 0.3f, up);
            p.cubicTo (x0 + w * 0.5f, up, x0 + w * 0.5f, mid + (dn - mid) * 0.4f, x0 + w * 0.7f, mid + (dn - mid) * 0.4f);
            p.lineTo (x1, mid + (dn - mid) * 0.4f);
            break;
        case 2: // high shelf
            p.startNewSubPath (x0, mid + (dn - mid) * 0.4f);
            p.lineTo (x0 + w * 0.3f, mid + (dn - mid) * 0.4f);
            p.cubicTo (x0 + w * 0.5f, mid + (dn - mid) * 0.4f, x0 + w * 0.5f, up, x0 + w * 0.7f, up);
            p.lineTo (x1, up);
            break;
        case 3: // low cut
            p.startNewSubPath (x0 + w * 0.1f, dn);
            p.cubicTo (x0 + w * 0.3f, mid, x0 + w * 0.35f, up + (mid - up) * 0.5f, x0 + w * 0.55f, up + (mid - up) * 0.5f);
            p.lineTo (x1, up + (mid - up) * 0.5f);
            break;
        case 4: // high cut
            p.startNewSubPath (x0, up + (mid - up) * 0.5f);
            p.lineTo (x0 + w * 0.45f, up + (mid - up) * 0.5f);
            p.cubicTo (x0 + w * 0.65f, up + (mid - up) * 0.5f, x0 + w * 0.7f, mid, x0 + w * 0.9f, dn);
            break;
        case 5: // notch
            p.startNewSubPath (x0, up + (mid - up) * 0.5f);
            p.lineTo (x0 + w * 0.38f, up + (mid - up) * 0.5f);
            p.cubicTo (x0 + w * 0.47f, up + (mid - up) * 0.5f, x0 + w * 0.47f, dn, x0 + w * 0.5f, dn);
            p.cubicTo (x0 + w * 0.53f, dn, x0 + w * 0.53f, up + (mid - up) * 0.5f, x0 + w * 0.62f, up + (mid - up) * 0.5f);
            p.lineTo (x1, up + (mid - up) * 0.5f);
            break;
        default: // tilt
            p.startNewSubPath (x0, dn - (dn - mid) * 0.3f);
            p.lineTo (x0 + w * 0.2f, dn - (dn - mid) * 0.3f);
            p.cubicTo (x0 + w * 0.5f, dn - (dn - mid) * 0.3f, x0 + w * 0.5f, up + (mid - up) * 0.3f, x0 + w * 0.8f, up + (mid - up) * 0.3f);
            p.lineTo (x1, up + (mid - up) * 0.3f);
            break;
    }
    return p;
}
} // namespace ui
