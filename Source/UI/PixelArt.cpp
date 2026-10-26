#include "Pixel.h"
#include "../Places/Place.h"
#if WB_HAS_PIXEL_ART
#include "WorldbuilderPixel.h"
#endif

// The Place icons as palette-indexed pixel maps: 32 x 32, one character per art pixel
// ('.' transparent, 'K' the mood's outline, the rest the colours below). Frame 0 is
// drawn in full; each further idle-animation frame lists only the pixels it changes,
// as "row column pixels" runs separated by ';'. Original art, made for this plugin.

namespace wb::ui::pixel {

namespace {
juce::Colour c(uint32_t rgb) { return juce::Colour(0xff000000 | rgb); }

juce::Colour colourOf(char key, juce::Colour outline) {
    switch (key) {
        case 'K': return outline;
        case 'w': return c(0xfffae8);
        case 'A': return c(0xc4beb2);
        case 'a': return c(0x968e88);
        case 'b': return c(0x625c60);
        case 'R': return c(0xd64840);
        case 'r': return c(0x922834);
        case 'Y': return c(0xffd65c);
        case 'y': return c(0xec8c34);
        case 'F': return c(0x5a8cec);
        case 'f': return c(0x3854b4);
        case 'G': return c(0x6ebe50);
        case 'g': return c(0x3c8040);
        case 'W': return c(0xc4844e);
        case 'V': return c(0x804e32);
        case 'L': return c(0x84ce5c);
        case 'l': return c(0x469c4a);
        case 'm': return c(0x266040);
        case 'T': return c(0x704630);
        case 'M': return c(0xb0bcce);
        case 'N': return c(0x6e7a92);
        case 'n': return c(0x3e4660);
        case 'C': return c(0x60f0ff);
        case 'c': return c(0x2096c4);
        case 'P': return c(0xff5cd6);
        case 'p': return c(0xa830a0);
        case 'U': return c(0x7a4ac8);
        case 'u': return c(0x422a80);
        case 'S': return c(0xffffff);
        case 'O': return c(0xffaa3c);
        case 'o': return c(0xe65428);
        case 'Z': return c(0xdcff78);
        case 'B': return c(0x281e50);
        case 'Q': return c(0xff96c8);
        default: return juce::Colours::transparentBlack;
    }
}

// castle: 3 frames
const char* const castleRows[] {
    ".................KKKKK..........",
    "................KFFFFFK.........",
    "...............KVFFFFFFK........",
    "...............KVFFFfKK.........",
    "...............KVfKKK...........",
    "......K........KVK.......K......",
    ".....KRK.......KVK......KRK.....",
    "....KRRrK......KK......KRRrK....",
    "....KRRRrK....KRRK.....KRRRrK...",
    "...KRRRRRrK..KRRRRK...KRRRRRrK..",
    "..KRRRRRRrK..KRRRRK..KRRRRRRrK..",
    ".KRRRRRRRRrKKRRRRRRKKRRRRRRRRrK.",
    ".KRRRRRRRRRrKRRRRRRKKRRRRRRRRRrK",
    "KRrrrrrrrrAArAAKAAKAArrrrrrrrrRr",
    ".KKAAAAAAaAAKAAKAAKAAKAAAAAAaKKK",
    "..KAAAAAAaAAAAAAAAAAaaAAAAAAaK..",
    "..KAAAAAAaAAAAAAAAAAaaAaAAAAaK..",
    "..KAAAAAAaAAYYAAAAYYaaAAAAAAaK..",
    "..KAAyYAAaAaYYAAAAYYaaAAyYAAaK..",
    "..KAAYYAAaAAAAAAAAAAaaAAYYAAaK..",
    "..KAAYYAAaAAAAAAAAAAaaAAYYAAaK..",
    "..KAAAAAAaAAAAAAAaAAaaAAAAAAaK..",
    "..KAAAAAAaAAAAVVVVAAaaAAAAAaaK..",
    "..KAAAAAAaAAAbVWTVbAaaAAAAAAaK..",
    "..KAaAAAAaAAAVVWTVVAaaAAAAAAaK..",
    "..KAAAAAAaAAAVVWTVVAaaAAAAAAaK..",
    "..KAAAAAAaAAAVVWTVVAaaAAAAAAaK..",
    "..KAAAAAAaAAAVVWTVVaaaAAAAAAaK..",
    "..KAAAAAAaAAAVVWTVVAaaAAAAAAaK..",
    ".KGAAAAAAaAAAVVWTVVAaaAAAAAAaGK.",
    "KGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGK",
    "gggGGGgGGGGGgGGGGGgGGGGGgGGGGggg",
};
const char* const castleFrames[] {
    "",
    "0 21 .;1 21 K;3 20 FFfK;4 17 K;4 21 KK",
    "0 20 ..;1 20 KK.;2 22 K.;3 20 FFfK;4 17 K;4 21 KfK;5 22 K",
};
// tavern: 4 frames
const char* const tavernRows[] {
    "................................",
    ".....................aA.........",
    "................................",
    "....................KKaAK.......",
    "...................KaaaaaK......",
    "...........KKKKKKKKKKbbbK.......",
    ".........KKyyyoyyyoyKbbbK.......",
    "........KoyyyoyyyoyyybbbK.......",
    ".......KoyyyoyyyoyyyobbbK.......",
    "......KoyyyoyyyoyyyoybbbK.......",
    ".....KoyyyoyyyoyyyoyybbbK.......",
    "....KoyyyoyyyoyyyoyyyoyyyK......",
    "...KoyyyoyyyoyyyoyyyoyyyoyK.....",
    "..KoyyyoyyyoyyyoyyyoyyyoyyyKK...",
    ".KoooooooooooooooooooooooooooK..",
    "..KKKVWWWWWVWWWWWWVWWWWWVKKKKK..",
    "....KVWWWWWVWWWWWWVWWWWWVVVVVVK.",
    "....KVWYYYWVWWWWWWVWYYYWVKKKKVK.",
    "....KVWYyYWVWWWWWWVWYyYWVKKWWWWK",
    "....KVWYYYWVWWWWWWVWYYYWVKKWwYWK",
    "....KVWWWWWVWWWWWWVWWWWWVKKWYYWK",
    "....KVVVVVVVVVVVVVVVVVVVVKKWWWWK",
    "....KVWWWWWVWVVVVWVWWWWWVK.KKKK.",
    "....KVWWWWWVWVVVVWVWWWWWVK......",
    "....KVWWWWWVWVVVVWVWWWWWVK......",
    "....KVWWWWWVWVVVYWVWWWWWVK......",
    "....KVWWWWWVWVVVVWVWWWWWVK......",
    "....KVWWWWWVWVVVVWVWWWWWVK......",
    ".....KKKKGGGGGGGGGGGGGGKKKK.....",
    "..KKKGGGGGGGGGGGGGGGGGGGGGGKKK..",
    "KKGGGGGGGGGGGGGGGGGGGGGGGGGGGGKK",
    "GGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGG",
};
const char* const tavernFrames[] {
    "",
    "0 22 aA;1 21 ..;2 23 aA;3 22 KK;17 30 VK;18 26 .K;18 31 W;19 26 .KWwYW;20 26 .KW;20 30 YW;21 26 .K;21 31 W;22 27 .;22 31 K",
    "0 23 aA;1 21 ..;2 22 aA;3 22 KK;16 31 K;17 31 V;18 26 ..K;18 31 W;19 26 ..KWwY;20 26 ..KWYY;21 26 ..K;21 31 W;22 27 ..;22 31 K",
    "1 21 .aA;3 21 aAK;17 30 VK;18 26 .K;18 31 W;19 26 .KWwYW;20 26 .KW;20 30 YW;21 26 .K;21 31 W;22 27 .;22 31 K",
};
// forest: 4 frames
const char* const forestRows[] {
    "................................",
    "..............................Z.",
    "..Z.............................",
    ".........................KKK....",
    "...........Z............KLlmK...",
    ".....KKK...............KlllmmK..",
    "....KLlmK..............KlllmmK..",
    "...KlllmmK............KlLLlmmmK.",
    "...KlllmmK..KKKKKKKK..KllllmmmK.",
    "..KlLLlmmmKKLLLLLlllK.KllllmmmK.",
    "..KllllmmmKLLLLwLLmllKlLLLlmmmmK",
    "..KllllmmmLLwLLLLLLllllllllmmmmK",
    ".KlLLLlmmlLLLLLLLLLllllllllmmmmK",
    ".KlllllmmlLLLLLLLLLllllLLLlmmmmm",
    ".KlllllmmlLLLLLLLLLllmlllllmmmmm",
    "KlLLLLlmmllLLLLLLLlllllllllmmmmm",
    "KllllllmmlllLmLLLllllllLLLlmmmmm",
    "Kllllllmmllllllllllmlllllllmmmmm",
    "lLLLLLlmmmlmlllllllllllllllmmmmm",
    "lllllllmmmmllllllllllLLLLLlmmmmm",
    "lllllllmmmmmlllllllllllllllmmmmm",
    "LLLLLLlmmmmmmmTTTVKllllllllmmmmm",
    "lllllllmmmmmmmTTTVlLLLLLLLlmmmmm",
    "lllllllmmmmmmmTTTVlllllllllmmmmm",
    "lllllllmmmmmmmTTTVlllllllllmmmmm",
    "KKKKKTTKKKKKKKTTTVlllRwRlllmmmmm",
    "....KTTK.....KTTTVKKKKwKKTTKKKKK",
    "....KTTK.KKKKKTTTVKKKKwKKTTK....",
    "....KTTKKggggggggggggggKKTTK....",
    "..KKKggggggggggggggggggggggKKK..",
    "KKggggggggggggggggggggggggggggKK",
    "gggggggggggggggggggggggggggggggg",
};
const char* const forestFrames[] {
    "",
    "1 30 .;2 2 .;2 31 Z;3 3 Z;3 20 Z;4 11 .",
    "1 30 .;2 2 .;3 12 Z;4 2 Z;4 11 .",
    "0 30 Z;1 30 .;2 2 .;3 1 Z;4 11 .;4 21 Z",
};
// spaceport: 3 frames
const char* const spaceportRows[] {
    "................................",
    ".................K..............",
    "................KRK.............",
    "...............KRMK.............",
    "...............KMMK.............",
    "..............KMMMMK............",
    ".....K........KMMMMK............",
    "....KRK......KMMMMMMK...........",
    "...KnnnK.....KMMMMMMK...........",
    "...KNNNK.....KMMMMNnK...........",
    "...KnnnK.....KMMCCNnK...........",
    "...KnnnKKKKK.KMCwCCnK...........",
    "...KNNNNNNNNKKMCCCCnK...........",
    "...KnnnKKKKK.KMMCCNnK...........",
    "...KnnnK.....KMMMMNnK...........",
    "...KNNNK.....KMMMMNnK...........",
    "...KnnnK.....KMMMMNnK...........",
    "...KnnnKKKKK.KccccccK...........",
    "...KNNNNNNNNKRMMMMNnRK..........",
    "...KnnnKKKKKRRMMMMNnRRK.........",
    "...KnnnK...KRRMMMMNnRRK.........",
    "...KNNNK...KRRMMMMNnRRK.........",
    "...KnnnK...KRRMMMMNnRRK.........",
    "...KnnnK....KKKnnnnKKK..........",
    "...KNNNK.......OOOO.............",
    "...KnnnK.......yOOy.............",
    "..KKnnnKKKKKKKKKooKKKKKKKKKKKK..",
    ".KMMMMMMMMMMMMMMMMMMMMMMMMMMMMK.",
    ".KNNCNNNNNCNNNNNCNNNNNCNNNNNCNK.",
    ".KnnnnnnnnnnnnnnnnnnnnnnnnnnnnK.",
    "..KKKKKKKKKKKKKKKKKKKKKKKKKKKK..",
    "................................",
};
const char* const spaceportFrames[] {
    "",
    "24 16 YY;25 15 O;25 18 O;26 16 yy;27 16 o",
    "25 15 .;25 18 .;26 17 K",
};
// megacity: 4 frames
const char* const megacityRows[] {
    "............KKKNKKK.............",
    "...........KnnnnnnBK.c..........",
    "...c.......KnnnnnnBK............",
    "...........KnnnnnnBK....KKKKKKK.",
    "..........cKnnnnnnBK...KnnnnnnBK",
    "......KKKKKKnYnnnnBK...KnnnnnnBK",
    ".....KNNNNNBnnnnnnBK...KnCnnnCBK",
    ".....KNNNNNBnnnnnnBK...KnnnnnnBK",
    ".....KNCNCNBnnnnnnBKKKKKnnnnnCBK",
    ".....KNNNNNBnnnnnYBNNNNBnnnnnnBK",
    ".....KNNNCNBnnnnnnBNNNNBnCnnnnBK",
    ".....KNNNNNBnnnnnnBNYNNBnnnnnnBK",
    ".....KNPPPNBnnnnnnBNNNNBnnnCnYBK",
    ".KKKKKNPNNNBnCnnnCBNNNNBnnnnnnBK",
    "KnnnnnNPNYNBnnnnnnBNpppBnnnnnCBK",
    "KnnnnnNPNNNBnCnYnCBNpppBnnnnnnBK",
    "KnCnnnNPNNNBnnnnnnBNNNNBnnnnnnBK",
    "KnnnnnNPNNNBnYnCnnBNCNCBnnnnnnBK",
    "KnCnCnNNNYNBnnnnnnBNNNNBnnnnnnBK",
    "KnnnnnNNNNNBnnnnnnBNCNCBnnnnnnBK",
    "KnnnCnNNNCNBnnnnnnBNNNNBnCnCnCBK",
    "KnnnnnNNNNNBnCnYnnBNCNYBnnnnnnBK",
    "KnCnYnNNNNNBnnnnnnBNNNNBnYnnnYBK",
    "KnnnnnNNNNNBnCnCnnBNNNCBnnnnnnBK",
    "KnCnnnNNNYNBnnnnnnBNNNNBnnnnnCBK",
    "KnnnnnNNNNNBnnnCnCBNCNYBnnnnnnBK",
    "KnnnnnNCNNNBnnnnnnBNNNNBnCnYnnBK",
    "KnnnnnNNNNNBnnnnnnBNNNNBnnnnnnBK",
    "KnYnYnNCNYNBnnnnnnBNNNNBnnnnnnBK",
    "KnnnnnNNNNNBnnnnnnBNNNCBnnnnnnBK",
    "KnnnnnNNNNNBnnnnnnBNNNNBnnnnnnBK",
    ".KKKKKKKKKKKKKKKKKKKKKKKKKKKKKK.",
};
const char* const megacityFrames[] {
    "",
    "1 21 .;2 3 .;3 21 c;4 3 c;4 10 .;14 20 PPP;15 20 PPP",
    "0 10 c;1 21 .;2 3 .;4 10 .;5 21 c;6 3 c;12 7 ppp;13 7 p;14 7 p;15 7 p;16 7 p;17 7 p",
    "0 3 c;1 21 .;1 29 c;2 3 .;2 10 c;4 10 .;7 21 c;14 20 PPP;15 20 PPP",
};
// nebula: 3 frames
const char* const nebulaRows[] {
    "................................",
    "................................",
    "............u.u.........S.......",
    "...c.....u.u.u.u................",
    "..cCc...u.u.u.u.u...............",
    "...c...u.u.u.u.u............S...",
    "......u.u.u.u.u.................",
    ".....u.u.u.u.u.u.u.u.u..........",
    "....u.u.u.u.u.u.u.u.u.u.u.......",
    "...u.u.u.u.UUUUUUUUUUu.u.u......",
    "..u.u.u.uUUUUUUU...UUUU.u.u.....",
    ".u.u.u..UUUUUU..PPUU..UU.u.u....",
    "..u.u.u.UUUP...PPPPPPU.UU.u.u...",
    ".u.u.u.UUUP...PQQPPPPPU..u.u.u..",
    "..u.u..UUUP...QwQQ.PPPUU.Uu.u.u.",
    ".u.u.u.UUUP..QwSSQQ..PUU...u.u..",
    "..u.u...UUP..QQSSwQ..PUUU.u.u.u.",
    ".uSu.uU.UUPPP.QQwQ...PUUU..u.u..",
    "..u.u.u..UPPPPPQQP...PUUU.u.u.u.",
    "...u.u.UU.UPPPPPP...PUUU.u.u.u..",
    "....u.u.UU..UUPP..UUUUUU..u.u.u.",
    ".....u.u.UUUU...UUUUUUUu.u.u.u..",
    "......u.u.uUUUUUUUUUU.u.u.u.u...",
    ".......u.u.u.u.u.u.u.u.u.u.u....",
    "..........u.u.u.u.u.u.u.u.u.....",
    ".................u.u.u.u.u......",
    "................u.u.u.u.u..c....",
    ".....S.........u.u.u.u.u..cCc...",
    "................u.u.u.u....c....",
    ".................u.u............",
    "................................",
    "................................",
};
const char* const nebulaFrames[] {
    "",
    "3 3 .;4 2 .S.;5 3 .;13 15 C;15 13 C;16 18 C;17 2 C;18 16 C;26 5 c;26 27 .;27 4 cCc;27 26 .S.;28 5 c;28 27 .",
    "1 24 c;2 23 cCc;3 3 .;3 24 c;4 2 .S.;4 28 c;5 3 .;5 27 cCc;6 28 c;26 27 .;27 26 .S.;28 27 .",
};

struct IconDef {
    int type;
    const char* key;
    const char* const* rows;
    const char* const* frames;
    int frameCount;
};
const IconDef icons[] {
        { Castle, "castle", castleRows, castleFrames, int(std::size(castleFrames)) },
        { Tavern, "tavern", tavernRows, tavernFrames, int(std::size(tavernFrames)) },
        { Forest, "forest", forestRows, forestFrames, int(std::size(forestFrames)) },
        { Spaceport, "spaceport", spaceportRows, spaceportFrames, int(std::size(spaceportFrames)) },
        { Megacity, "megacity", megacityRows, megacityFrames, int(std::size(megacityFrames)) },
        { Nebula, "nebula", nebulaRows, nebulaFrames, int(std::size(nebulaFrames)) },
};

const IconDef* iconFor(int type) {
    for (const auto& icon : icons)
        if (icon.type == type) return &icon;
    return nullptr;
}
} // namespace

juce::Image bundledSprite(const juce::String& name) {
#if WB_HAS_PIXEL_ART
    const juce::String file = name + ".png";
    for (int i = 0; i < WorldbuilderPixel::namedResourceListSize; ++i)
        if (file == WorldbuilderPixel::getNamedResourceOriginalFilename(WorldbuilderPixel::namedResourceList[i])) {
            int size = 0;
            if (const char* data = WorldbuilderPixel::getNamedResource(WorldbuilderPixel::namedResourceList[i], size))
                return juce::ImageCache::getFromMemory(data, size);
        }
#else
    juce::ignoreUnused(name);
#endif
    return {};
}

bool hasPlaceIcon(int placeType) { return iconFor(placeType) != nullptr; }

int placeIconFrames(int placeType) {
    const auto sheet = bundledSprite(juce::String("place-") + placeKeys[std::size_t(juce::jlimit(0, int(PlaceTypeCount) - 1, placeType))]);
    if (sheet.isValid() && sheet.getHeight() > 0) return std::max(1, sheet.getWidth() / sheet.getHeight());
    const auto* icon = iconFor(placeType);
    return icon != nullptr ? icon->frameCount : 1;
}

Canvas placeIcon(int placeType, int frame, juce::Colour outline) {
    Canvas k(iconSize, iconSize);
    if (placeType <= NoPlace || placeType >= PlaceTypeCount) return k;
    // A hand-drawn replacement wins: frames side by side, each as tall as the sheet.
    if (const auto sheet = bundledSprite(juce::String("place-") + placeKeys[std::size_t(placeType)]); sheet.isValid() && sheet.getHeight() > 0) {
        const int size = sheet.getHeight(), count = std::max(1, sheet.getWidth() / size);
        k.paste(sheet.getClippedImage({ (frame % count) * size, 0, size, size }), (iconSize - size) / 2, (iconSize - size) / 2);
        return k;
    }
    const auto* icon = iconFor(placeType);
    if (icon == nullptr) return k;
    for (int y = 0; y < iconSize; ++y)
        for (int x = 0; x < iconSize; ++x) k.set(x, y, colourOf(icon->rows[y][x], outline));
    // Apply the frame's changes over frame 0.
    const juce::StringArray runs = juce::StringArray::fromTokens(icon->frames[std::abs(frame) % icon->frameCount], ";", "");
    for (const auto& run : runs) {
        const auto parts = juce::StringArray::fromTokens(run, " ", "");
        if (parts.size() < 2) continue;
        const int y = parts[0].getIntValue(), x = parts[1].getIntValue();
        const auto pixels = parts.size() > 2 ? parts[2] : juce::String();
        for (int i = 0; i < pixels.length(); ++i) k.set(x + i, y, colourOf(char(pixels[i]), outline));
    }
    return k;
}

} // namespace wb::ui::pixel
