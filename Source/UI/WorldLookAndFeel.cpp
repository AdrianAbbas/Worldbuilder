#include "WorldLookAndFeel.h"
#include "Style.h"

namespace wb::ui {

WorldLookAndFeel::WorldLookAndFeel() {
    setColour(juce::ResizableWindow::backgroundColourId, colours::background);
    setColour(juce::PopupMenu::backgroundColourId, colours::panel);
    setColour(juce::PopupMenu::textColourId, colours::parchment);
    setColour(juce::PopupMenu::highlightedBackgroundColourId, colours::cyan.withAlpha(0.25f));
    setColour(juce::PopupMenu::highlightedTextColourId, colours::parchment);
    setColour(juce::ComboBox::textColourId, colours::parchment);
    setColour(juce::Label::textColourId, colours::parchment);
    setColour(juce::TooltipWindow::backgroundColourId, colours::panel);
    setColour(juce::TooltipWindow::textColourId, colours::parchment);
    setColour(juce::TooltipWindow::outlineColourId, colours::brass.withAlpha(0.5f));
    setColour(juce::TextButton::textColourOffId, colours::parchment);
    setColour(juce::TextButton::textColourOnId, colours::background);
    setColour(juce::Slider::textBoxTextColourId, colours::parchment);
    setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour(juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour(juce::TextEditor::backgroundColourId, colours::panel);
    setColour(juce::TextEditor::textColourId, colours::parchment);
}

// Knob: a machined dial in a brass bezel, over a tick ring, with a glowing value arc;
// name and value below.
void WorldLookAndFeel::drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height, float position,
                                        float startAngle, float endAngle, juce::Slider& slider) {
    auto area = juce::Rectangle<int>(x, y, width, height).toFloat();
    // A compact knob (the header's Score Intensity) draws no text: the editor labels it.
    const bool compact = slider.getProperties().getWithDefault("compact", false);
    const float textHeight = compact ? 0.0f : 28.0f;
    const auto knobArea = area.withTrimmedBottom(textHeight).reduced(4.0f);
    const float radius = std::min(knobArea.getWidth(), knobArea.getHeight()) * 0.5f;
    const auto centre = knobArea.getCentre();
    const float angle = startAngle + position * (endAngle - startAngle);
    const bool enabled = slider.isEnabled(), hover = slider.isMouseOverOrDragging() && enabled;
    const float arcRadius = radius * 0.92f;

    // Ticks around the dial.
    if (radius > 16.0f) {
        g.setColour(colours::parchment.withAlpha(0.18f));
        for (int k = 0; k <= 10; ++k) {
            const float a = startAngle + float(k) / 10.0f * (endAngle - startAngle);
            const float r0 = radius * 1.0f, r1 = radius * (k % 5 == 0 ? 1.1f : 1.05f);
            g.drawLine(centre.x + std::sin(a) * r0, centre.y - std::cos(a) * r0, centre.x + std::sin(a) * r1, centre.y - std::cos(a) * r1, 1.0f);
        }
    }
    // The groove and the value arc, with a glow.
    juce::Path track, value;
    track.addCentredArc(centre.x, centre.y, arcRadius, arcRadius, 0, startAngle, endAngle, true);
    g.setColour(juce::Colours::black.withAlpha(0.45f));
    g.strokePath(track, juce::PathStrokeType(4.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour(colours::panelEdge.withAlpha(0.8f));
    g.strokePath(track, juce::PathStrokeType(2.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    if (position > 0.001f) {
        value.addCentredArc(centre.x, centre.y, arcRadius, arcRadius, 0, startAngle, angle, true);
        const auto arc = enabled ? colours::cyan : colours::dim;
        if (enabled) {
            g.setColour(arc.withAlpha(hover ? 0.3f : 0.18f));
            g.strokePath(value, juce::PathStrokeType(8.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
        const auto p0 = value.getPointAlongPath(0.0f), p1 = value.getPointAlongPath(value.getLength());
        g.setGradientFill(juce::ColourGradient(arc.darker(0.3f), p0.x, p0.y, arc.brighter(0.35f), p1.x, p1.y, false));
        g.strokePath(value, juce::PathStrokeType(3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // The dial: shadow, brass bezel, machined face.
    const auto face = juce::Rectangle<float>(radius * 2, radius * 2).withCentre(centre).reduced(radius * 0.2f);
    g.setColour(juce::Colours::black.withAlpha(0.45f));
    g.fillEllipse(face.translated(0.0f, 2.0f).expanded(1.0f));
    g.setGradientFill(juce::ColourGradient(colours::brass.brighter(0.5f).withAlpha(enabled ? 0.95f : 0.4f), face.getX(), face.getY(),
                                           colours::brass.darker(0.8f).withAlpha(enabled ? 0.95f : 0.4f), face.getRight(), face.getBottom(), false));
    g.fillEllipse(face);
    const auto inner = face.reduced(std::max(1.6f, radius * 0.07f));
    juce::ColourGradient body(colours::panelEdge.brighter(hover ? 0.55f : 0.4f), inner.getX() + inner.getWidth() * 0.3f, inner.getY() + inner.getHeight() * 0.2f,
                              colours::background.darker(0.3f), inner.getRight(), inner.getBottom(), true);
    g.setGradientFill(body);
    g.fillEllipse(inner);
    // A soft sheen on the upper half.
    g.setGradientFill(juce::ColourGradient(juce::Colours::white.withAlpha(0.13f), 0.0f, inner.getY(), juce::Colours::white.withAlpha(0.0f), 0.0f, inner.getCentreY(), false));
    g.fillEllipse(inner.reduced(inner.getWidth() * 0.08f, 0.0f).withHeight(inner.getHeight() * 0.55f));
    g.setColour(juce::Colours::black.withAlpha(0.35f));
    g.drawEllipse(inner, 1.0f);

    // Pointer: an engraved line ending in a lit dot.
    const auto along = [&](float k) { return juce::Point<float>(centre.x + std::sin(angle) * radius * k, centre.y - std::cos(angle) * radius * k); };
    g.setColour(juce::Colours::black.withAlpha(0.4f));
    g.drawLine({ along(0.2f).translated(0.0f, 1.0f), along(0.62f).translated(0.0f, 1.0f) }, 2.4f);
    g.setColour(colours::parchment.withAlpha(enabled ? 1.0f : 0.4f));
    g.drawLine({ along(0.2f), along(0.62f) }, 2.0f);
    if (enabled) {
        const auto tip = along(0.62f);
        g.setColour(colours::cyan.withAlpha(0.35f));
        g.fillEllipse(juce::Rectangle<float>(7.0f, 7.0f).withCentre(tip));
        g.setColour(colours::cyan.brighter(0.4f));
        g.fillEllipse(juce::Rectangle<float>(3.0f, 3.0f).withCentre(tip));
    }

    if (compact) {
        // A glow that grows with the value: the cue heating up.
        g.setColour(colours::cyan.withAlpha(0.12f * position));
        g.fillEllipse(juce::Rectangle<float>(radius * 2.2f, radius * 2.2f).withCentre(centre));
        return;
    }
    // Name and value.
    auto text = area.removeFromBottom(textHeight);
    g.setFont(juce::Font(juce::FontOptions(12.5f, juce::Font::bold)));
    g.setColour(colours::parchment.withAlpha(enabled ? 0.95f : 0.4f));
    g.drawText(slider.getName(), text.removeFromTop(14.0f), juce::Justification::centred, false);
    g.setFont(juce::Font(juce::FontOptions(11.5f)));
    g.setColour(colours::cyan.withAlpha(enabled ? 0.85f : 0.3f));
    g.drawText(slider.getTextFromValue(slider.getValue()), text, juce::Justification::centred, false);
}

// Faders: a recessed groove, a lit brass fill and a machined thumb. Value bars (Seed):
// a recessed field filled to the value.
void WorldLookAndFeel::drawLinearSlider(juce::Graphics& g, int x, int y, int width, int height, float position, float,
                                        float, juce::Slider::SliderStyle style, juce::Slider& slider) {
    const auto area = juce::Rectangle<int>(x, y, width, height).toFloat();
    const bool enabled = slider.isEnabled(), hover = slider.isMouseOverOrDragging() && enabled;
    if (style == juce::Slider::LinearBar) {
        drawWell(g, area, 4.0f, colours::brass, hover);
        const auto fill = area.reduced(1.5f).withRight(std::max(area.getX() + 1.5f, position));
        g.setGradientFill(juce::ColourGradient(colours::brass.withAlpha(0.55f), 0.0f, fill.getY(), colours::brass.darker(0.6f).withAlpha(0.45f), 0.0f, fill.getBottom(), false));
        g.fillRoundedRectangle(fill, 3.0f);
        g.setColour(colours::parchment);
        g.setFont(juce::Font(juce::FontOptions(13.0f, juce::Font::bold)));
        g.drawText(slider.getTextFromValue(slider.getValue()), area, juce::Justification::centred, false);
        return;
    }
    const auto groove = area.withSizeKeepingCentre(area.getWidth(), 5.0f);
    drawWell(g, groove, 2.5f, colours::panelEdge.brighter(0.3f));
    const auto accent = enabled ? colours::brass : colours::dim;
    const auto filled = groove.reduced(1.0f).withRight(std::max(groove.getX() + 1.0f, position));
    if (enabled && hover) {
        g.setColour(accent.withAlpha(0.25f));
        g.fillRoundedRectangle(filled.expanded(0.0f, 2.0f), 3.0f);
    }
    g.setGradientFill(juce::ColourGradient(accent.brighter(0.35f), 0.0f, filled.getY(), accent.darker(0.35f), 0.0f, filled.getBottom(), false));
    g.fillRoundedRectangle(filled, 1.5f);
    // The thumb.
    const auto thumb = juce::Rectangle<float>(12.0f, 12.0f).withCentre({ position, groove.getCentreY() });
    g.setColour(juce::Colours::black.withAlpha(0.45f));
    g.fillEllipse(thumb.translated(0.0f, 1.2f));
    if (hover) {
        g.setColour(colours::cyan.withAlpha(0.35f));
        g.fillEllipse(thumb.expanded(3.0f));
    }
    g.setGradientFill(juce::ColourGradient(enabled ? colours::parchment : colours::dim, thumb.getX() + 3.0f, thumb.getY() + 2.0f,
                                           (enabled ? colours::brass : colours::dim).darker(0.6f), thumb.getRight(), thumb.getBottom(), true));
    g.fillEllipse(thumb);
    g.setColour(juce::Colours::black.withAlpha(0.4f));
    g.drawEllipse(thumb, 0.8f);
    g.setColour((hover ? colours::cyan : colours::background).withAlpha(0.9f));
    g.fillEllipse(juce::Rectangle<float>(3.0f, 3.0f).withCentre(thumb.getCentre()));
}

// Toggles: raised pills that light up in their colour when on (M, S, Power, Characters,
// instrument switches), with an optional icon.
void WorldLookAndFeel::drawToggleButton(juce::Graphics& g, juce::ToggleButton& button, bool highlighted, bool down) {
    const auto area = button.getLocalBounds().toFloat().reduced(2.0f);
    const auto accent = button.findColour(juce::TextButton::buttonOnColourId);
    SurfaceState st { highlighted && button.isEnabled(), down, button.getToggleState(), button.isEnabled() };
    const float corner = std::min(area.getHeight() * 0.5f, 9.0f);
    drawSurface(g, area, corner, colours::panel.brighter(0.08f), accent, st);
    const auto ink = inkFor(st, accent);
    auto text = area;
    const float fontSize = std::min(13.0f, area.getHeight() * 0.62f);
    juce::Font font(juce::FontOptions(fontSize, juce::Font::bold));
    const float iconSize = std::min(area.getHeight() - 6.0f, 15.0f);
    const float textWidth = button.getButtonText().isEmpty() ? 0.0f : juce::GlyphArrangement::getStringWidth(font, button.getButtonText()) + 5.0f;
    // The icon beside the text, when there is room for both (else the text alone).
    if (const auto icon = iconOf(button); icon != Icon::None && (textWidth == 0.0f || iconSize + textWidth + 8.0f <= area.getWidth())) {
        auto row = juce::Rectangle<float>(iconSize + textWidth, area.getHeight()).withCentre(area.getCentre());
        drawIcon(g, icon, row.removeFromLeft(iconSize), ink);
        text = row.withTrimmedLeft(5.0f);
    }
    g.setColour(ink);
    g.setFont(font);
    g.drawFittedText(button.getButtonText(), text.toNearestInt(), juce::Justification::centred, 1, 0.75f);
}

void WorldLookAndFeel::drawButtonBackground(juce::Graphics& g, juce::Button& button, const juce::Colour&, bool highlighted,
                                            bool down) {
    const auto area = button.getLocalBounds().toFloat().reduced(1.5f);
    // A pressed-in (toggled) button is lit in its "on" colour; otherwise its own colour if set.
    auto base = colours::panel.brighter(0.08f);
    if (button.isColourSpecified(juce::TextButton::buttonColourId)) base = colours::panel.interpolatedWith(button.findColour(juce::TextButton::buttonColourId).withAlpha(1.0f), 0.55f);
    const auto accent = button.getToggleState() ? button.findColour(juce::TextButton::buttonOnColourId) : colours::brass;
    SurfaceState st { highlighted && button.isEnabled(), down, button.getToggleState(), button.isEnabled() };
    drawSurface(g, area, std::min(6.0f, area.getHeight() * 0.5f), base, accent, st);
}

void WorldLookAndFeel::drawButtonText(juce::Graphics& g, juce::TextButton& button, bool highlighted, bool down) {
    const auto area = button.getLocalBounds().toFloat().reduced(4.0f, 2.0f).translated(0.0f, down ? 0.5f : 0.0f);
    const auto accent = button.getToggleState() ? button.findColour(juce::TextButton::buttonOnColourId) : colours::brass;
    const SurfaceState st { highlighted, down, button.getToggleState(), button.isEnabled() };
    const auto ink = inkFor(st, accent);
    const auto font = getTextButtonFont(button, button.getHeight());
    auto text = area;
    const float iconSize = std::min(area.getHeight() - 2.0f, 15.0f);
    const float textWidth = button.getButtonText().isEmpty() ? 0.0f : juce::GlyphArrangement::getStringWidth(font, button.getButtonText()) + 4.0f;
    // The icon beside the text, when there is room for both (else the text alone).
    if (const auto icon = iconOf(button); icon != Icon::None && (textWidth == 0.0f || iconSize + textWidth + 4.0f <= area.getWidth())) {
        auto row = juce::Rectangle<float>(iconSize + textWidth, area.getHeight()).withCentre(area.getCentre());
        drawIcon(g, icon, row.removeFromLeft(iconSize), ink);
        text = row.withTrimmedLeft(4.0f);
    }
    g.setColour(ink);
    g.setFont(font);
    g.drawFittedText(button.getButtonText(), text.toNearestInt(), juce::Justification::centred, 1, 0.7f);
}

// Menus: a recessed field with a raised cap holding the chevron.
void WorldLookAndFeel::drawComboBox(juce::Graphics& g, int width, int height, bool down, int, int, int, int, juce::ComboBox& box) {
    const auto area = juce::Rectangle<float>(0, 0, float(width), float(height)).reduced(1.0f);
    const bool hover = box.isMouseOver(true) && box.isEnabled();
    if (hover) drawGlow(g, area, 5.0f, colours::cyan.withAlpha(0.5f), 3.0f);
    drawWell(g, area, 5.0f, hover ? colours::cyan : colours::brass, hover);
    const auto cap = area.withLeft(area.getRight() - std::min(20.0f, area.getHeight())).reduced(2.0f);
    drawSurface(g, cap, 3.5f, colours::panel.brighter(0.1f), colours::brass, SurfaceState { hover, down, false, box.isEnabled() });
    juce::Path chevron;
    const auto c = cap.getCentre();
    chevron.startNewSubPath(c.x - 3.5f, c.y - 1.5f);
    chevron.lineTo(c.x, c.y + 2.0f);
    chevron.lineTo(c.x + 3.5f, c.y - 1.5f);
    g.setColour(hover ? colours::cyan : colours::brass.brighter(0.2f));
    g.strokePath(chevron, juce::PathStrokeType(1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

void WorldLookAndFeel::positionComboBoxText(juce::ComboBox& box, juce::Label& label) {
    label.setBounds(1, 1, box.getWidth() - std::min(20, box.getHeight()) - 2, box.getHeight() - 2);
    label.setFont(getComboBoxFont(box));
}

void WorldLookAndFeel::drawPopupMenuBackground(juce::Graphics& g, int width, int height) {
    const auto r = juce::Rectangle<float>(0, 0, float(width), float(height));
    g.setGradientFill(juce::ColourGradient(colours::panel.brighter(0.1f), 0.0f, 0.0f, colours::background, 0.0f, float(height), false));
    g.fillRect(r);
    g.setColour(colours::brass.withAlpha(0.55f));
    g.drawRect(r, 1.0f);
}

void WorldLookAndFeel::drawTooltip(juce::Graphics& g, const juce::String& text, int width, int height) {
    const auto r = juce::Rectangle<float>(0, 0, float(width), float(height));
    g.setGradientFill(juce::ColourGradient(colours::panel.brighter(0.12f), 0.0f, 0.0f, colours::background, 0.0f, float(height), false));
    g.fillRect(r);
    g.setColour(colours::brass.withAlpha(0.55f));
    g.drawRect(r, 1.0f);
    g.setColour(colours::parchment);
    g.setFont(juce::Font(juce::FontOptions(12.5f)));
    g.drawFittedText(text, r.reduced(8.0f, 4.0f).toNearestInt(), juce::Justification::centredLeft, 6);
}

juce::Font WorldLookAndFeel::getComboBoxFont(juce::ComboBox&) { return juce::Font(juce::FontOptions(13.0f)); }
juce::Font WorldLookAndFeel::getTextButtonFont(juce::TextButton&, int height) { return juce::Font(juce::FontOptions(height < 18 ? 9.5f : 12.5f, juce::Font::bold)).withExtraKerningFactor(0.04f); }

juce::Label* WorldLookAndFeel::createSliderTextBox(juce::Slider& slider) {
    auto* label = LookAndFeel_V4::createSliderTextBox(slider);
    label->setColour(juce::Label::backgroundColourId, juce::Colours::transparentBlack);
    label->setColour(juce::Label::outlineColourId, juce::Colours::transparentBlack);
    label->setColour(juce::Label::textColourId, colours::parchment);
    return label;
}

} // namespace wb::ui
