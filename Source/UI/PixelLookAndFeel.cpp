#include "PixelLookAndFeel.h"

namespace wb::ui {

PixelLookAndFeel::PixelLookAndFeel() {
    // The sprites draw every text themselves; JUCE's own labels stay invisible.
    setColour(juce::ComboBox::textColourId, juce::Colours::transparentBlack);
    setColour(juce::Slider::textBoxTextColourId, juce::Colours::transparentBlack);
    setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour(juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setMood(pixel::blendMood);
}

void PixelLookAndFeel::setMood(float mood) {
    // Moods change in steps of 1/64: smooth to the eye, and a bounded number of sprites.
    mood = std::round(juce::jlimit(0.0f, 1.0f, mood) * 64.0f) / 64.0f;
    if (mood == currentMood && sprites.size() > 0) return;
    currentMood = mood;
    colours = pixel::palette(mood);
    setColour(juce::PopupMenu::backgroundColourId, colours.face);
    setColour(juce::TooltipWindow::backgroundColourId, colours.face);
    setColour(juce::TooltipWindow::textColourId, colours.ink);
}

void PixelLookAndFeel::setPixelScale(int s) {
    s = juce::jlimit(1, 8, s);
    if (s == scale) return;
    scale = s;
    sprites.clear();
}

void PixelLookAndFeel::setFrame(int f) { frame = f; }

juce::String PixelLookAndFeel::moodKey() const { return juce::String(juce::roundToInt(currentMood * 64.0f)) + "/" + juce::String(scale) + "/" + juce::String(frame % 6); }

void PixelLookAndFeel::blit(juce::Graphics& g, const juce::String& key, juce::Rectangle<int> area, const std::function<pixel::Canvas()>& make, bool centreX,
                            bool centreY) {
    const auto& image = sprites.get(moodKey() + "/" + key, [&] { return make().scaled(scale); });
    // Centre in the area, on whole art pixels from its corner, so nothing is split.
    const int x = centreX ? area.getX() + (area.getWidth() - image.getWidth()) / 2 / scale * scale : area.getX();
    const int y = centreY ? area.getY() + (area.getHeight() - image.getHeight()) / 2 / scale * scale : area.getY();
    g.drawImageAt(image, x, y);
}

namespace {
pixel::State stateOf(const juce::Component& c, bool hover, bool down, bool on = false) {
    pixel::State s;
    s.hover = hover && c.isEnabled();
    s.down = down;
    s.on = on;
    s.enabled = c.isEnabled();
    return s;
}
juce::String stateKey(const pixel::State& s) { return juce::String(int(s.hover) | int(s.down) << 1 | int(s.on) << 2 | int(s.enabled) << 3); }
} // namespace

void PixelLookAndFeel::drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height, float position, float, float, juce::Slider& slider) {
    const auto s = stateOf(slider, slider.isMouseOverOrDragging(), slider.isMouseButtonDown());
    const auto label = slider.getName().toUpperCase(), value = slider.getTextFromValue(slider.getValue()).toUpperCase();
    const float v = std::round(position * 64.0f) / 64.0f;
    blit(g, "knob/" + label + "/" + value + "/" + juce::String(v) + "/" + stateKey(s), { x, y, width, height },
         [&] { return pixel::knob(colours, v, label, value, s, frame); });
}

void PixelLookAndFeel::drawLinearSlider(juce::Graphics& g, int x, int y, int width, int height, float position, float, float, juce::Slider::SliderStyle style,
                                        juce::Slider& slider) {
    if (style != juce::Slider::LinearHorizontal && style != juce::Slider::LinearBar) {
        LookAndFeel_V4::drawLinearSlider(g, x, y, width, height, position, 0, 0, style, slider);   // vertical sliders come in 13b
        return;
    }
    const auto s = stateOf(slider, slider.isMouseOverOrDragging(), slider.isMouseButtonDown());
    const auto label = slider.getName().toUpperCase(), value = slider.getTextFromValue(slider.getValue()).toUpperCase();
    const int art = std::max(8, width / scale - 2 * pixel::margin);
    const float v = slider.valueToProportionOfLength(slider.getValue());
    blit(g, "slider/" + juce::String(art) + "/" + label + "/" + value + "/" + juce::String(v) + "/" + stateKey(s), { x, y, width, height },
         [&] { return pixel::slider(colours, art, v, label, value, s, frame); });
}

juce::Label* PixelLookAndFeel::createSliderTextBox(juce::Slider& slider) {
    // Values are drawn in the sprite; the text box stays for typing a value in.
    auto* label = LookAndFeel_V4::createSliderTextBox(slider);
    label->setColour(juce::Label::textColourId, juce::Colours::transparentBlack);
    label->setColour(juce::Label::outlineColourId, juce::Colours::transparentBlack);
    return label;
}

void PixelLookAndFeel::drawButtonBackground(juce::Graphics& g, juce::Button& button, const juce::Colour&, bool highlighted, bool down) {
    const auto s = stateOf(button, highlighted, down, button.getToggleState());
    const auto text = button.getButtonText().toUpperCase();
    const int art = button.getWidth() / scale - 2 * pixel::margin;
    blit(g, "button/" + juce::String(art) + "/" + text + "/" + stateKey(s), button.getLocalBounds(), [&] { return pixel::button(colours, text, s, art, frame); });
}

void PixelLookAndFeel::drawButtonText(juce::Graphics&, juce::TextButton&, bool, bool) {}   // in the sprite

void PixelLookAndFeel::drawToggleButton(juce::Graphics& g, juce::ToggleButton& button, bool highlighted, bool down) {
    const auto s = stateOf(button, highlighted, down, button.getToggleState());
    const auto text = button.getButtonText().toUpperCase();
    blit(g, "toggle/" + text + "/" + stateKey(s), button.getLocalBounds(), [&] { return pixel::toggle(colours, text, s, frame); }, false, true);
}

void PixelLookAndFeel::drawComboBox(juce::Graphics& g, int width, int height, bool down, int, int, int, int, juce::ComboBox& box) {
    const auto s = stateOf(box, box.isMouseOver(true), down || box.isPopupActive());
    const auto text = box.getText().toUpperCase();
    const int art = width / scale - 2 * pixel::margin - 1;
    blit(g, "combo/" + juce::String(art) + "/" + text + "/" + stateKey(s), { 0, 0, width, height }, [&] { return pixel::dropdown(colours, text, art, s, nullptr, -1, frame); });
}

void PixelLookAndFeel::positionComboBoxText(juce::ComboBox& box, juce::Label& label) { label.setBounds(box.getLocalBounds()); }

void PixelLookAndFeel::drawPopupMenuBackground(juce::Graphics& g, int width, int height) {
    pixel::Canvas k((width + scale - 1) / scale, (height + scale - 1) / scale);
    k.box(0, 0, k.width(), k.height(), colours.face, colours.outline);
    k.hline(1, k.width() - 2, 1, colours.faceDark);
    pixel::draw(g, k, {}, scale);
}

void PixelLookAndFeel::drawPopupMenuItem(juce::Graphics& g, const juce::Rectangle<int>& area, bool isSeparator, bool isActive, bool isHighlighted, bool isTicked,
                                         bool, const juce::String& text, const juce::String&, const juce::Drawable*, const juce::Colour*) {
    pixel::Canvas k(area.getWidth() / scale, area.getHeight() / scale);
    if (isSeparator) {
        k.hline(2, k.width() - 3, k.height() / 2, colours.faceDark);
    } else {
        if (isHighlighted && isActive) {
            k.fill(1, 0, k.width() - 2, k.height(), colours.accent);
            k.hline(1, k.width() - 2, 0, colours.glow);
        }
        const auto ink = !isActive ? colours.inkDim : isHighlighted ? colours.inkOn : colours.ink;
        if (isTicked) {
            k.set(4, k.height() / 2 - 1, ink);
            k.hline(3, 5, k.height() / 2, ink);
            k.set(4, k.height() / 2 + 1, ink);
        }
        k.text(8, (k.height() - pixel::Canvas::textHeight) / 2, text.toUpperCase(), ink);
    }
    pixel::draw(g, k, area.getPosition().toFloat(), scale);
}

void PixelLookAndFeel::getIdealPopupMenuItemSize(const juce::String& text, bool isSeparator, int, int& idealWidth, int& idealHeight) {
    idealHeight = (isSeparator ? 5 : 10) * scale;
    idealWidth = (pixel::Canvas::textWidth(text) + 14) * scale;
}

int PixelLookAndFeel::getTabButtonBestWidth(juce::TabBarButton& button, int) { return (pixel::Canvas::textWidth(button.getButtonText()) + 13) * scale; }

void PixelLookAndFeel::drawTabButton(juce::TabBarButton& button, juce::Graphics& g, bool isMouseOver, bool) {
    // One tab of the strip: the strip's sprite for this name alone, active or not.
    const bool active = button.isFrontTab();
    const auto text = button.getButtonText().toUpperCase();
    blit(g, "tab/" + text + "/" + juce::String(int(active)) + juce::String(int(isMouseOver)), button.getLocalBounds(), [&] {
        const auto strip = pixel::tabs(colours, { text }, active ? 0 : -1, isMouseOver ? 0 : -1, frame);
        // Crop to the tab itself (no panel edge beyond it, which the bar draws).
        const auto area = pixel::tabAreas({ text })[0];
        pixel::Canvas k(area.getWidth() + 1, strip.height() - pixel::margin);
        k.paste(strip.source().getClippedImage({ area.getX(), pixel::margin, k.width(), k.height() }), 0, 0);
        return k;
    }, false, false);
}

void PixelLookAndFeel::drawTabbedButtonBarBackground(juce::TabbedButtonBar& bar, juce::Graphics& g) {
    // The panel edge under the tabs.
    pixel::Canvas k(bar.getWidth() / scale, 5);
    k.fill(0, 0, k.width(), 4, colours.face);
    k.hline(0, k.width() - 1, 0, colours.outline);
    k.hline(0, k.width() - 1, 1, colours.faceLight);
    k.hline(0, k.width() - 1, 4, colours.outline);
    pixel::draw(g, k, { 0.0f, float((bar.getHeight() / scale - 5) * scale) }, scale);
}

void PixelLookAndFeel::drawTabAreaBehindFrontButton(juce::TabbedButtonBar&, juce::Graphics&, int, int) {}

} // namespace wb::ui
