#pragma once
// Every JUCE control in the 8-bit style (Milestone 13): knobs, the Score Intensity knob,
// sliders (horizontal, vertical, value bars), buttons, toggles, combo boxes, popup
// menus, tab bars, scroll bars and tooltips, drawn from the pixel sprites in the
// current mood (pixel::theme()). Every control keeps its own behaviour (hit area, drag,
// Shift fine control, double-click reset, tooltip, automation); only its look changes.
//
// Component properties it reads: "compact" (a knob drawn as the large Score Intensity
// dial, no text), "pixelSwitch" (a ToggleButton drawn as a switch with an LED) and the
// icon set with wb::ui::setIcon.
#include "Pixel.h"

namespace wb::ui {

class PixelLookAndFeel final : public juce::LookAndFeel_V4 {
public:
    PixelLookAndFeel();
    // Sets the shared theme's mood (0 fantasy, 0.5 blend, 1 sci-fi); the editor sets the
    // whole theme itself each animation frame.
    void setMood(float sciFi);
    void setMood(const pixel::Mood& mood);
    pixel::SpriteCache& cache() { return pixel::sharedCache(); }

    // The colour a control lights in when on: the old vector style's fixed colours map
    // to the mood (brass: its accent, cyan: its inlay, danger: its peak); any other
    // colour (a Place's accent) is kept.
    static juce::Colour moodColour(juce::Colour requested);

    void drawRotarySlider(juce::Graphics&, int x, int y, int width, int height, float position, float startAngle, float endAngle,
                          juce::Slider&) override;
    void drawLinearSlider(juce::Graphics&, int x, int y, int width, int height, float position, float minPos, float maxPos,
                          juce::Slider::SliderStyle, juce::Slider&) override;
    int getSliderThumbRadius(juce::Slider&) override;
    juce::Label* createSliderTextBox(juce::Slider&) override;
    void drawButtonBackground(juce::Graphics&, juce::Button&, const juce::Colour&, bool highlighted, bool down) override;
    void drawButtonText(juce::Graphics&, juce::TextButton&, bool highlighted, bool down) override;
    void drawToggleButton(juce::Graphics&, juce::ToggleButton&, bool highlighted, bool down) override;
    void drawComboBox(juce::Graphics&, int width, int height, bool down, int buttonX, int buttonY, int buttonW, int buttonH, juce::ComboBox&) override;
    void positionComboBoxText(juce::ComboBox&, juce::Label&) override;
    juce::Font getComboBoxFont(juce::ComboBox&) override;
    void drawPopupMenuBackground(juce::Graphics&, int width, int height) override;
    void drawPopupMenuItem(juce::Graphics&, const juce::Rectangle<int>& area, bool isSeparator, bool isActive, bool isHighlighted, bool isTicked,
                           bool hasSubMenu, const juce::String& text, const juce::String& shortcutKeyText, const juce::Drawable* icon,
                           const juce::Colour* textColour) override;
    void drawPopupMenuSectionHeader(juce::Graphics&, const juce::Rectangle<int>& area, const juce::String& sectionName) override;
    void getIdealPopupMenuItemSize(const juce::String& text, bool isSeparator, int standardMenuItemHeight, int& idealWidth, int& idealHeight) override;
    int getTabButtonBestWidth(juce::TabBarButton&, int tabDepth) override;
    int getTabButtonOverlap(int) override { return 0; }
    void drawTabButton(juce::TabBarButton&, juce::Graphics&, bool isMouseOver, bool isMouseDown) override;
    void drawTabbedButtonBarBackground(juce::TabbedButtonBar&, juce::Graphics&) override;
    void drawTabAreaBehindFrontButton(juce::TabbedButtonBar&, juce::Graphics&, int width, int height) override;
    void drawScrollbar(juce::Graphics&, juce::ScrollBar&, int x, int y, int width, int height, bool isScrollbarVertical, int thumbStartPosition,
                       int thumbSize, bool isMouseOver, bool isMouseDown) override;
    void drawTooltip(juce::Graphics&, const juce::String& text, int width, int height) override;
    juce::Rectangle<int> getTooltipBounds(const juce::String& tipText, juce::Point<int> screenPos, juce::Rectangle<int> parentArea) override;
};

} // namespace wb::ui
