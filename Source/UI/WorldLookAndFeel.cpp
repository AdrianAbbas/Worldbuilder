#include "WorldLookAndFeel.h"

namespace wb::ui {

WorldLookAndFeel::WorldLookAndFeel() {
    setColour(juce::ResizableWindow::backgroundColourId, colours::background);
    setColour(juce::PopupMenu::backgroundColourId, colours::panel);
    setColour(juce::PopupMenu::textColourId, colours::parchment);
    setColour(juce::PopupMenu::highlightedBackgroundColourId, colours::brass.withAlpha(0.35f));
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

// Knob: brass-rimmed dial with a cyan value arc; name and value below.
void WorldLookAndFeel::drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height, float position,
                                        float startAngle, float endAngle, juce::Slider& slider) {
    auto area = juce::Rectangle<int>(x, y, width, height).toFloat();
    const float textHeight = 28.0f;
    const auto knobArea = area.withTrimmedBottom(textHeight).reduced(4.0f);
    const float radius = std::min(knobArea.getWidth(), knobArea.getHeight()) * 0.5f;
    const auto centre = knobArea.getCentre();
    const float angle = startAngle + position * (endAngle - startAngle);
    const bool enabled = slider.isEnabled();

    // Body: dark disc with a soft highlight, like a brass-rimmed instrument dial.
    juce::ColourGradient body(colours::panelEdge.brighter(0.25f), centre.x - radius * 0.5f, centre.y - radius * 0.6f,
                              colours::background, centre.x + radius * 0.6f, centre.y + radius, true);
    g.setGradientFill(body);
    g.fillEllipse(juce::Rectangle<float>(radius * 2, radius * 2).withCentre(centre).reduced(radius * 0.2f));
    g.setColour(colours::brass.withAlpha(enabled ? 0.55f : 0.2f));
    g.drawEllipse(juce::Rectangle<float>(radius * 2, radius * 2).withCentre(centre).reduced(radius * 0.2f), 1.2f);

    // Track and value arc.
    juce::Path track, value;
    const float arcRadius = radius * 0.92f;
    track.addCentredArc(centre.x, centre.y, arcRadius, arcRadius, 0, startAngle, endAngle, true);
    g.setColour(colours::panelEdge);
    g.strokePath(track, juce::PathStrokeType(3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    value.addCentredArc(centre.x, centre.y, arcRadius, arcRadius, 0, startAngle, angle, true);
    g.setColour(enabled ? colours::cyan : colours::dim);
    g.strokePath(value, juce::PathStrokeType(3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // Pointer.
    const juce::Point<float> tip(centre.x + std::sin(angle) * radius * 0.62f, centre.y - std::cos(angle) * radius * 0.62f);
    const juce::Point<float> base(centre.x + std::sin(angle) * radius * 0.25f, centre.y - std::cos(angle) * radius * 0.25f);
    g.setColour(colours::parchment.withAlpha(enabled ? 1.0f : 0.4f));
    g.drawLine({ base, tip }, 2.2f);

    // Name and value.
    auto text = area.removeFromBottom(textHeight);
    g.setFont(juce::Font(juce::FontOptions(12.5f, juce::Font::bold)));
    g.setColour(colours::parchment.withAlpha(enabled ? 0.95f : 0.4f));
    g.drawText(slider.getName(), text.removeFromTop(14.0f), juce::Justification::centred, false);
    g.setFont(juce::Font(juce::FontOptions(11.5f)));
    g.setColour(colours::cyan.withAlpha(enabled ? 0.85f : 0.3f));
    g.drawText(slider.getTextFromValue(slider.getValue()), text, juce::Justification::centred, false);
}

// Fader for the layer mixer: thin track, brass fill, round cap.
void WorldLookAndFeel::drawLinearSlider(juce::Graphics& g, int x, int y, int width, int height, float position, float,
                                        float, juce::Slider::SliderStyle style, juce::Slider& slider) {
    const auto area = juce::Rectangle<int>(x, y, width, height).toFloat();
    if (style == juce::Slider::LinearBar) {
        g.setColour(colours::panelEdge);
        g.fillRoundedRectangle(area, 4.0f);
        g.setColour(colours::brass.withAlpha(0.35f));
        g.fillRoundedRectangle(area.withRight(std::max(area.getX(), position)), 4.0f);
        g.setColour(colours::parchment);
        g.setFont(juce::Font(juce::FontOptions(13.0f, juce::Font::bold)));
        g.drawText(slider.getTextFromValue(slider.getValue()), area, juce::Justification::centred, false);
        return;
    }
    const auto track = area.withSizeKeepingCentre(area.getWidth(), 4.0f);
    g.setColour(colours::panelEdge);
    g.fillRoundedRectangle(track, 2.0f);
    const bool enabled = slider.isEnabled();
    g.setColour((enabled ? colours::brass : colours::dim).withAlpha(0.9f));
    g.fillRoundedRectangle(track.withRight(position), 2.0f);
    g.setColour(enabled ? colours::parchment : colours::dim);
    g.fillEllipse(juce::Rectangle<float>(10.0f, 10.0f).withCentre({ position, track.getCentreY() }));
}

// Toggle as a small pill with its text (M, S, Power).
void WorldLookAndFeel::drawToggleButton(juce::Graphics& g, juce::ToggleButton& button, bool highlighted, bool) {
    const auto area = button.getLocalBounds().toFloat().reduced(1.5f);
    const bool on = button.getToggleState();
    const auto accent = button.findColour(juce::TextButton::buttonOnColourId);
    g.setColour(on ? accent : colours::panel.brighter(highlighted ? 0.15f : 0.0f));
    g.fillRoundedRectangle(area, area.getHeight() * 0.5f);
    g.setColour(on ? accent.brighter(0.3f) : colours::panelEdge.brighter(0.3f));
    g.drawRoundedRectangle(area, area.getHeight() * 0.5f, 1.0f);
    g.setColour(on ? colours::background : colours::parchment.withAlpha(button.isEnabled() ? 0.8f : 0.3f));
    g.setFont(juce::Font(juce::FontOptions(std::min(13.0f, area.getHeight() * 0.62f), juce::Font::bold)));
    g.drawText(button.getButtonText(), area, juce::Justification::centred, false);
}

void WorldLookAndFeel::drawButtonBackground(juce::Graphics& g, juce::Button& button, const juce::Colour&, bool highlighted,
                                            bool down) {
    const auto area = button.getLocalBounds().toFloat().reduced(1.0f);
    g.setColour(colours::panel.brighter(down ? 0.3f : (highlighted ? 0.15f : 0.0f)));
    g.fillRoundedRectangle(area, 5.0f);
    g.setColour(colours::brass.withAlpha(0.6f));
    g.drawRoundedRectangle(area, 5.0f, 1.0f);
}

void WorldLookAndFeel::drawComboBox(juce::Graphics& g, int width, int height, bool, int, int, int, int, juce::ComboBox&) {
    const auto area = juce::Rectangle<float>(0, 0, float(width), float(height)).reduced(1.0f);
    g.setColour(colours::panel);
    g.fillRoundedRectangle(area, 5.0f);
    g.setColour(colours::brass.withAlpha(0.6f));
    g.drawRoundedRectangle(area, 5.0f, 1.0f);
    juce::Path arrow;
    const float cx = area.getRight() - 12.0f, cy = area.getCentreY();
    arrow.addTriangle(cx - 4.0f, cy - 2.0f, cx + 4.0f, cy - 2.0f, cx, cy + 3.0f);
    g.setColour(colours::brass);
    g.fillPath(arrow);
}

juce::Font WorldLookAndFeel::getComboBoxFont(juce::ComboBox&) { return juce::Font(juce::FontOptions(13.0f)); }
juce::Font WorldLookAndFeel::getTextButtonFont(juce::TextButton&, int) { return juce::Font(juce::FontOptions(13.0f, juce::Font::bold)); }

juce::Label* WorldLookAndFeel::createSliderTextBox(juce::Slider& slider) {
    auto* label = LookAndFeel_V4::createSliderTextBox(slider);
    label->setColour(juce::Label::backgroundColourId, juce::Colours::transparentBlack);
    label->setColour(juce::Label::outlineColourId, juce::Colours::transparentBlack);
    label->setColour(juce::Label::textColourId, colours::parchment);
    return label;
}

} // namespace wb::ui
