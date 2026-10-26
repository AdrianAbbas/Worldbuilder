#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

namespace wb::ui {

// Palette: deep navy and charcoal, parchment and brass, cool sci-fi cyan accents.
namespace colours {
inline const juce::Colour background { 0xff0d1420 };
inline const juce::Colour panel { 0xff152031 };
inline const juce::Colour panelEdge { 0xff26364d };
inline const juce::Colour brass { 0xffc9a45c };
inline const juce::Colour parchment { 0xffe8dcc0 };
inline const juce::Colour cyan { 0xff5fd4e6 };
inline const juce::Colour dim { 0xff6c7a8f };
inline const juce::Colour danger { 0xffd0604a };
}

class WorldLookAndFeel final : public juce::LookAndFeel_V4 {
public:
    WorldLookAndFeel();
    void drawRotarySlider(juce::Graphics&, int x, int y, int width, int height, float position, float startAngle,
                          float endAngle, juce::Slider&) override;
    void drawLinearSlider(juce::Graphics&, int x, int y, int width, int height, float position, float minPos, float maxPos,
                          juce::Slider::SliderStyle, juce::Slider&) override;
    void drawToggleButton(juce::Graphics&, juce::ToggleButton&, bool highlighted, bool down) override;
    void drawButtonBackground(juce::Graphics&, juce::Button&, const juce::Colour&, bool highlighted, bool down) override;
    void drawComboBox(juce::Graphics&, int width, int height, bool down, int buttonX, int buttonY, int buttonW, int buttonH,
                      juce::ComboBox&) override;
    void drawButtonText(juce::Graphics&, juce::TextButton&, bool highlighted, bool down) override;
    void positionComboBoxText(juce::ComboBox&, juce::Label&) override;
    void drawPopupMenuBackground(juce::Graphics&, int width, int height) override;
    void drawTooltip(juce::Graphics&, const juce::String& text, int width, int height) override;
    juce::Font getComboBoxFont(juce::ComboBox&) override;
    juce::Font getTextButtonFont(juce::TextButton&, int buttonHeight) override;
    juce::Label* createSliderTextBox(juce::Slider&) override;
};

} // namespace wb::ui
