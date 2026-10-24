#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

namespace wb::ui {

// Original vector illustrations of each Place (wb::PlaceType), drawn in `area`:
// filled with a soft `fill` and outlined in `ink`.
void drawPlaceIcon(juce::Graphics& g, int type, juce::Rectangle<float> area, juce::Colour ink, juce::Colour fill);

} // namespace wb::ui
