#pragma once
// Real JUCE controls in the 8-bit style (Milestone 13a): knobs, sliders, buttons,
// toggles, combo boxes, popup menus and tab bars drawn from the pixel sprites at an
// integer scale, in the current mood. Every control keeps its own behaviour (hit area,
// drag, double-click reset, tooltip, automation); only its look changes.
#include "Pixel.h"

namespace wb::ui {

class PixelLookAndFeel final : public juce::LookAndFeel_V4 {
public:
    PixelLookAndFeel();
    // 0 fantasy, 0.5 blend, 1 sci-fi (see pixel::moodOf).
    void setMood(float mood);
    float mood() const { return currentMood; }
    // Screen pixels per art pixel (2 at 100 % UI scale, 3 at 150 %, 4 at 200 %).
    void setPixelScale(int scale);
    int pixelScale() const { return scale; }
    // Advances the decoration animation (twinkles, scanlines).
    void setFrame(int frame);
    pixel::SpriteCache& cache() { return sprites; }

    void drawRotarySlider(juce::Graphics&, int x, int y, int width, int height, float position, float startAngle, float endAngle,
                          juce::Slider&) override;
    void drawLinearSlider(juce::Graphics&, int x, int y, int width, int height, float position, float minPos, float maxPos,
                          juce::Slider::SliderStyle, juce::Slider&) override;
    juce::Label* createSliderTextBox(juce::Slider&) override;
    void drawButtonBackground(juce::Graphics&, juce::Button&, const juce::Colour&, bool highlighted, bool down) override;
    void drawButtonText(juce::Graphics&, juce::TextButton&, bool highlighted, bool down) override;
    void drawToggleButton(juce::Graphics&, juce::ToggleButton&, bool highlighted, bool down) override;
    void drawComboBox(juce::Graphics&, int width, int height, bool down, int buttonX, int buttonY, int buttonW, int buttonH, juce::ComboBox&) override;
    void positionComboBoxText(juce::ComboBox&, juce::Label&) override;
    void drawPopupMenuBackground(juce::Graphics&, int width, int height) override;
    void drawPopupMenuItem(juce::Graphics&, const juce::Rectangle<int>& area, bool isSeparator, bool isActive, bool isHighlighted, bool isTicked,
                           bool hasSubMenu, const juce::String& text, const juce::String& shortcutKeyText, const juce::Drawable* icon,
                           const juce::Colour* textColour) override;
    void getIdealPopupMenuItemSize(const juce::String& text, bool isSeparator, int standardMenuItemHeight, int& idealWidth, int& idealHeight) override;
    int getTabButtonBestWidth(juce::TabBarButton&, int tabDepth) override;
    int getTabButtonOverlap(int) override { return 0; }
    void drawTabButton(juce::TabBarButton&, juce::Graphics&, bool isMouseOver, bool isMouseDown) override;
    void drawTabbedButtonBarBackground(juce::TabbedButtonBar&, juce::Graphics&) override;
    void drawTabAreaBehindFrontButton(juce::TabbedButtonBar&, juce::Graphics&, int width, int height) override;

private:
    // Draws a sprite (made once per key and scale) in `area`, centred or at its left / top,
    // snapped to whole art pixels.
    void blit(juce::Graphics& g, const juce::String& key, juce::Rectangle<int> area, const std::function<pixel::Canvas()>& make, bool centreX = true,
              bool centreY = true);
    juce::String moodKey() const;

    float currentMood = pixel::blendMood;
    pixel::Palette colours = pixel::palette(pixel::blendMood);
    int scale = 2, frame = 0;
    pixel::SpriteCache sprites;
};

} // namespace wb::ui
