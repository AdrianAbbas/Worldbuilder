#include "PixelLookAndFeel.h"
#include "Style.h"
#include "WorldLookAndFeel.h"

namespace wb::ui {

namespace {
const pixel::Palette& pal() { return pixel::theme().colours; }

pixel::State stateOf(const juce::Component& c, bool hover, bool down, bool on = false) {
    pixel::State s;
    s.hover = hover && c.isEnabled();
    s.down = down && c.isEnabled();
    s.on = on;
    s.enabled = c.isEnabled();
    return s;
}
juce::String stateKey(const pixel::State& s) { return juce::String(int(s.hover) | int(s.down) << 1 | int(s.on) << 2 | int(s.enabled) << 3); }
juce::String colourKey(juce::Colour c) { return juce::String::toHexString(int(c.getARGB())); }
// The animation frame, for sprites that sparkle or flicker only while hovered.
juce::String frameKey(const pixel::State& s) { return s.hover ? juce::String(pixel::theme().frame % 6) : juce::String("-"); }

// Where a sprite with a decoration margin goes so that its body fills `bounds` less one
// art pixel all round.
juce::Point<int> bodyAt(juce::Rectangle<int> bounds) {
    return { pixel::snap(bounds.getX()) - (pixel::margin - 1) * pixel::unit, pixel::snap(bounds.getY()) - (pixel::margin - 1) * pixel::unit };
}
int art(int logical) { return logical / pixel::unit; }
} // namespace

PixelLookAndFeel::PixelLookAndFeel() {
    // The sprites draw the texts; JUCE's own labels stay invisible where a sprite
    // writes over them.
    setColour(juce::ComboBox::textColourId, juce::Colours::transparentBlack);
    setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour(juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour(juce::ResizableWindow::backgroundColourId, wb::ui::colours::background);
    setColour(juce::Label::textColourId, wb::ui::colours::parchment);
    setColour(juce::TextEditor::backgroundColourId, wb::ui::colours::panel);
    setColour(juce::TextEditor::textColourId, wb::ui::colours::parchment);
    setColour(juce::TooltipWindow::textColourId, wb::ui::colours::parchment);
}

void PixelLookAndFeel::setMood(float sciFi) { setMood(pixel::Mood { sciFi, 0 }); }
void PixelLookAndFeel::setMood(const pixel::Mood& mood) { pixel::setTheme(mood, pixel::theme().frame, pixel::theme().delights); }

juce::Colour PixelLookAndFeel::moodColour(juce::Colour requested) {
    const auto opaque = requested.withAlpha(1.0f);
    if (opaque == wb::ui::colours::brass) return pal().accent;
    if (opaque == wb::ui::colours::cyan) return pal().inlay;
    if (opaque == wb::ui::colours::danger) return pal().peak;
    return requested.isTransparent() ? pal().accent : opaque;
}

// ---- Sliders -----------------------------------------------------------------------

void PixelLookAndFeel::drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height, float position, float, float, juce::Slider& slider) {
    const auto s = stateOf(slider, slider.isMouseOverOrDragging(), slider.isMouseButtonDown());
    const float v = std::round(position * 128.0f) / 128.0f;
    auto area = juce::Rectangle<int>(x, y, width, height);
    if (slider.getProperties().getWithDefault("compact", false)) {
        // The Score Intensity dial: no text (the editor labels it).
        const int dial = std::max(16, std::min(art(width), art(height)) - 2);
        const int size = (dial + 2 * pixel::margin) * pixel::unit;
        pixel::drawCached(g, "intensity/" + juce::String(dial) + "/" + juce::String(v) + "/" + stateKey(s) + frameKey(s),
                          { area.getCentreX() - size / 2, area.getCentreY() - size / 2 }, [&] { return pixel::intensityKnob(pal(), dial, v, s, pixel::theme().frame); });
        return;
    }
    // The dial on top, the name and value (pixel text) below, as the vector style did.
    constexpr int textHeight = 30;
    const int dial = std::max(10, std::min(art(width), art(height - textHeight)) - 1);
    const int size = (dial + 2 * pixel::margin) * pixel::unit;
    const auto knobArea = area.withTrimmedBottom(textHeight);
    pixel::drawCached(g, "knob/" + juce::String(dial) + "/" + juce::String(v) + "/" + stateKey(s) + frameKey(s),
                      { knobArea.getCentreX() - size / 2, knobArea.getCentreY() - size / 2 },
                      [&] { return pixel::knob(pal(), dial, v, {}, {}, s, pixel::theme().frame); });
    auto text = area.removeFromBottom(textHeight);
    const auto& p = pal();
    pixel::drawText(g, slider.getName(), text.removeFromTop(14), s.enabled ? p.ink : p.inkDim, juce::Justification::centredTop, pixel::Font::Small);
    pixel::drawText(g, slider.getTextFromValue(slider.getValue()), text.removeFromTop(14), s.enabled ? p.accent : p.inkDim, juce::Justification::centredTop,
                    pixel::Font::Small);
}

int PixelLookAndFeel::getSliderThumbRadius(juce::Slider&) { return (pixel::margin + 3) * pixel::unit; }

void PixelLookAndFeel::drawLinearSlider(juce::Graphics& g, int, int, int, int, float, float, float, juce::Slider::SliderStyle style, juce::Slider& slider) {
    const auto s = stateOf(slider, slider.isMouseOverOrDragging(), slider.isMouseButtonDown());
    const float v = std::round(float(slider.valueToProportionOfLength(slider.getValue())) * 256.0f) / 256.0f;
    // The slider's own text box (if any) keeps its side; the sprite fills the rest.
    auto bounds = slider.getLocalBounds();
    if (slider.getTextBoxPosition() == juce::Slider::TextBoxLeft) bounds.removeFromLeft(slider.getTextBoxWidth());
    else if (slider.getTextBoxPosition() == juce::Slider::TextBoxRight) bounds.removeFromRight(slider.getTextBoxWidth());
    if (style == juce::Slider::LinearBar || style == juce::Slider::LinearBarVertical) {
        const int w = art(bounds.getWidth()), h = art(bounds.getHeight());
        const auto text = slider.getTextFromValue(slider.getValue());
        pixel::drawCached(g, "bar/" + juce::String(w) + "x" + juce::String(h) + "/" + juce::String(v) + "/" + text + "/" + stateKey(s), bounds.getPosition(),
                          [&] { return pixel::bar(pal(), w, h, v, text, s); });
        return;
    }
    const bool vertical = style == juce::Slider::LinearVertical || style == juce::Slider::TwoValueVertical || style == juce::Slider::ThreeValueVertical;
    const int length = std::max(8, art(vertical ? bounds.getHeight() : bounds.getWidth()) - 2 * pixel::margin);
    const auto accent = slider.isColourSpecified(juce::Slider::trackColourId) ? moodColour(slider.findColour(juce::Slider::trackColourId)) : juce::Colour();
    const int across = (11 + 1 + 2 * pixel::margin) * pixel::unit;
    const juce::Point<int> at = vertical ? juce::Point<int>(bounds.getCentreX() - across / 2, bounds.getY())
                                         : juce::Point<int>(bounds.getX(), bounds.getCentreY() - across / 2 + pixel::unit / 2);
    pixel::drawCached(g, "slider/" + juce::String(int(vertical)) + juce::String(length) + "/" + juce::String(v) + "/" + stateKey(s) + frameKey(s) + colourKey(accent), at,
                      [&] { return pixel::slider(pal(), length, v, vertical, s, pixel::theme().frame, accent); });
}

juce::Label* PixelLookAndFeel::createSliderTextBox(juce::Slider& slider) {
    auto* label = LookAndFeel_V4::createSliderTextBox(slider);
    label->setColour(juce::Label::backgroundColourId, juce::Colours::transparentBlack);
    label->setColour(juce::Label::outlineColourId, juce::Colours::transparentBlack);
    label->setColour(juce::Label::textColourId, wb::ui::colours::parchment);
    return label;
}

// ---- Buttons -----------------------------------------------------------------------

void PixelLookAndFeel::drawButtonBackground(juce::Graphics& g, juce::Button& button, const juce::Colour&, bool highlighted, bool down) {
    const auto s = stateOf(button, highlighted, down, button.getToggleState());
    const auto accent = moodColour(button.findColour(juce::TextButton::buttonOnColourId));
    const auto text = button.getButtonText().toUpperCase();
    const int w = art(button.getWidth()) - 2, h = art(button.getHeight()) - 3;
    const auto icon = iconOf(button);
    pixel::drawCached(g, "button/" + juce::String(w) + "x" + juce::String(h) + "/" + text + "/" + juce::String(int(icon)) + "/" + stateKey(s) + frameKey(s) + colourKey(accent),
                      bodyAt(button.getLocalBounds()), [&] {
                          const int iconPx = std::max(5, std::min(h - 4, 9));
                          const auto sprite = pixel::toolIcon(int(icon), iconPx, juce::Colours::white, juce::Colours::black);
                          // The icon alone when the text does not fit beside it.
                          const auto font = h >= 13 ? pixel::Font::Large : pixel::Font::Small;
                          const bool both = icon != Icon::None && pixel::Canvas::textWidth(text, font) + sprite.width() + 9 <= w;
                          const bool iconOnly = icon != Icon::None && !both && (text.isEmpty() || pixel::Canvas::textWidth(text, font) + 4 > w);
                          return pixel::button(pal(), iconOnly ? juce::String() : text, s, w, h, pixel::theme().frame, icon != Icon::None && (both || iconOnly) ? &sprite : nullptr,
                                               accent);
                      });
}

void PixelLookAndFeel::drawButtonText(juce::Graphics&, juce::TextButton&, bool, bool) {}   // in the sprite

void PixelLookAndFeel::drawToggleButton(juce::Graphics& g, juce::ToggleButton& button, bool highlighted, bool down) {
    const auto s = stateOf(button, highlighted, down, button.getToggleState());
    const auto text = button.getButtonText().toUpperCase();
    if (button.getProperties().getWithDefault("pixelSwitch", false)) {
        const auto at = juce::Point<int>(0, button.getHeight() / 2 - (11 + 1 + 2 * pixel::margin) * pixel::unit / 2);
        pixel::drawCached(g, "switch/" + text + "/" + stateKey(s) + frameKey(s), at, [&] { return pixel::toggle(pal(), text, s, pixel::theme().frame); });
        return;
    }
    // A pill that lights in its colour when on (M, S, Power, Characters, switches).
    const auto accent = moodColour(button.findColour(juce::TextButton::buttonOnColourId));
    const int w = art(button.getWidth()) - 2, h = art(button.getHeight()) - 3;
    const auto icon = iconOf(button);
    pixel::drawCached(g, "pill/" + juce::String(w) + "x" + juce::String(h) + "/" + text + "/" + juce::String(int(icon)) + "/" + stateKey(s) + frameKey(s) + colourKey(accent),
                      bodyAt(button.getLocalBounds()), [&] {
                          const int iconPx = std::max(5, std::min(h - 4, 9));
                          const auto sprite = pixel::toolIcon(int(icon), iconPx, juce::Colours::white, juce::Colours::black);
                          const auto font = h >= 13 ? pixel::Font::Large : pixel::Font::Small;
                          const bool withIcon = icon != Icon::None && pixel::Canvas::textWidth(text, font) + sprite.width() + 9 <= w;
                          return pixel::button(pal(), text, s, w, h, pixel::theme().frame, withIcon ? &sprite : nullptr, accent);
                      });
}

// ---- Menus -------------------------------------------------------------------------

void PixelLookAndFeel::drawComboBox(juce::Graphics& g, int width, int height, bool down, int, int, int, int, juce::ComboBox& box) {
    const auto s = stateOf(box, box.isMouseOver(true), down || box.isPopupActive());
    const auto text = box.getText().toUpperCase();
    const int w = art(width) - 2, h = art(height) - 2;
    pixel::drawCached(g, "combo/" + juce::String(w) + "x" + juce::String(h) + "/" + text + "/" + stateKey(s) + frameKey(s), bodyAt({ 0, 0, width, height }),
                      [&] { return pixel::dropdown(pal(), text, w, s, nullptr, -1, pixel::theme().frame, h); });
}

void PixelLookAndFeel::positionComboBoxText(juce::ComboBox& box, juce::Label& label) {
    label.setBounds(box.getLocalBounds());
    label.setColour(juce::Label::textColourId, juce::Colours::transparentBlack);
}

juce::Font PixelLookAndFeel::getComboBoxFont(juce::ComboBox&) { return juce::Font(juce::FontOptions(13.0f)); }

void PixelLookAndFeel::drawPopupMenuBackground(juce::Graphics& g, int width, int height) {
    const auto& p = pal();
    pixel::Canvas k(art(width) + 1, art(height) + 1);
    k.box(0, 0, art(width), art(height), p.face, p.outline);
    k.hline(1, art(width) - 2, 1, p.faceLight);
    k.vline(1, 1, art(height) - 2, p.faceLight);
    pixel::draw(g, k, {});
}

void PixelLookAndFeel::drawPopupMenuItem(juce::Graphics& g, const juce::Rectangle<int>& area, bool isSeparator, bool isActive, bool isHighlighted, bool isTicked,
                                         bool hasSubMenu, const juce::String& text, const juce::String&, const juce::Drawable*, const juce::Colour*) {
    const auto& p = pal();
    pixel::Canvas k(art(area.getWidth()), art(area.getHeight()));
    if (isSeparator) {
        k.hline(2, k.width() - 3, k.height() / 2, p.faceDark);
    } else {
        if (isHighlighted && isActive) {
            k.fill(1, 0, k.width() - 2, k.height(), p.accent);
            k.hline(1, k.width() - 2, 0, p.glow);
        }
        const auto ink = !isActive ? p.inkDim : isHighlighted ? p.inkOn : p.ink;
        const int mid = k.height() / 2;
        if (isTicked) {
            k.set(4, mid - 1, ink);
            k.hline(3, 5, mid, ink);
            k.set(4, mid + 1, ink);
        }
        k.shadowText(8, (k.height() - pixel::Canvas::textHeight(pixel::Font::Large)) / 2, text.toUpperCase(), ink,
                     isHighlighted ? p.glow.withAlpha(0.5f) : p.outline, pixel::Font::Large);
        if (hasSubMenu)
            for (int r = 0; r < 3; ++r) k.vline(k.width() - 6 + r, mid - 2 + r, mid + 2 - r, ink);
    }
    pixel::draw(g, k, area.getPosition());
}

void PixelLookAndFeel::drawPopupMenuSectionHeader(juce::Graphics& g, const juce::Rectangle<int>& area, const juce::String& sectionName) {
    pixel::drawText(g, sectionName.toUpperCase(), area.reduced(8 * 2, 0), pal().accent, juce::Justification::centredLeft, pixel::Font::Small);
}

void PixelLookAndFeel::getIdealPopupMenuItemSize(const juce::String& text, bool isSeparator, int, int& idealWidth, int& idealHeight) {
    idealHeight = (isSeparator ? 5 : 11) * pixel::unit;
    idealWidth = (pixel::Canvas::textWidth(text.toUpperCase(), pixel::Font::Large) + 20) * pixel::unit;
}

// ---- Tabs and scroll bars ----------------------------------------------------------

int PixelLookAndFeel::getTabButtonBestWidth(juce::TabBarButton& button, int) {
    return (pixel::Canvas::textWidth(button.getButtonText().toUpperCase(), pixel::Font::Large) + 13) * pixel::unit;
}

void PixelLookAndFeel::drawTabButton(juce::TabBarButton& button, juce::Graphics& g, bool isMouseOver, bool) {
    const bool active = button.isFrontTab();
    const auto text = button.getButtonText().toUpperCase();
    const int w = art(button.getWidth()), h = art(button.getHeight());
    pixel::drawCached(g, "tab/" + juce::String(w) + "x" + juce::String(h) + "/" + text + juce::String(int(active)) + juce::String(int(isMouseOver)), {},
                      [&] { return pixel::tab(pal(), text, w, h, active, isMouseOver); });
}

void PixelLookAndFeel::drawTabbedButtonBarBackground(juce::TabbedButtonBar& bar, juce::Graphics& g) {
    const auto& p = pal();
    pixel::Canvas k(art(bar.getWidth()), 1);
    k.hline(0, k.width() - 1, 0, p.outline);
    pixel::draw(g, k, { 0, bar.getHeight() - pixel::unit });
}

void PixelLookAndFeel::drawTabAreaBehindFrontButton(juce::TabbedButtonBar&, juce::Graphics&, int, int) {}

void PixelLookAndFeel::drawScrollbar(juce::Graphics& g, juce::ScrollBar&, int x, int y, int width, int height, bool vertical, int thumbStart, int thumbSize,
                                     bool isMouseOver, bool isMouseDown) {
    const auto& p = pal();
    const int w = std::max(3, art(width)), h = std::max(3, art(height));
    pixel::Canvas k(w, h);
    k.box(0, 0, w, h, p.well, p.outline);
    if (thumbSize > 0) {
        const int a = art(thumbStart), l = std::max(3, art(thumbSize));
        const auto body = isMouseDown ? p.rimLight : isMouseOver ? p.rimLight.interpolatedWith(p.rim, 0.5f) : p.rim;
        if (vertical) {
            k.box(0, a, w, l, body, p.outline);
            k.vline(1, a + 1, a + l - 2, p.rimLight);
        } else {
            k.box(a, 0, l, h, body, p.outline);
            k.hline(a + 1, a + l - 2, 1, p.rimLight);
        }
    }
    pixel::draw(g, k, { x, y });
}

// ---- Tooltips ----------------------------------------------------------------------
// Tooltips are sentences: a pixel frame around clean, readable text.

void PixelLookAndFeel::drawTooltip(juce::Graphics& g, const juce::String& text, int width, int height) {
    const auto& p = pal();
    pixel::Canvas k(art(width), art(height));
    k.box(0, 0, k.width(), k.height(), p.face, p.outline);
    k.hline(1, k.width() - 2, 1, p.inlay);
    pixel::draw(g, k, {});
    g.setColour(p.ink);
    g.setFont(juce::Font(juce::FontOptions(13.0f)));
    g.drawFittedText(text, juce::Rectangle<int>(width, height).reduced(9, 5), juce::Justification::centredLeft, 6);
}

juce::Rectangle<int> PixelLookAndFeel::getTooltipBounds(const juce::String& tipText, juce::Point<int> screenPos, juce::Rectangle<int> parentArea) {
    const juce::Font font(juce::FontOptions(13.0f));
    juce::AttributedString s;
    s.append(tipText, font);
    juce::TextLayout layout;
    layout.createLayoutWithBalancedLineLengths(s, 360.0f);
    const int w = pixel::snap(int(layout.getWidth()) + 20 + pixel::unit), h = pixel::snap(int(layout.getHeight()) + 12 + pixel::unit);
    return juce::Rectangle<int>(screenPos.x > parentArea.getCentreX() ? screenPos.x - (w + 12) : screenPos.x + 24,
                                screenPos.y > parentArea.getCentreY() ? screenPos.y - (h + 6) : screenPos.y + 6, w, h)
        .constrainedWithin(parentArea);
}

} // namespace wb::ui
