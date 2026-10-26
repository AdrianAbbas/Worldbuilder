#include "Pixel.h"
#include "../Places/Place.h"
#if WB_HAS_PIXEL_ART
#include "WorldbuilderPixel.h"
#endif

// The Place icons (all 26 Places and the empty slot) as palette-indexed pixel maps: 32 x 32, one character per art pixel
// ('.' transparent, 'K' the mood's outline, the rest the colours below). Frame 0 is
// drawn in full; each further idle-animation frame lists only the pixels it changes,
// as "row column pixels" runs separated by ';'. Original art, made for this plugin: fantasy
// and nature Places drawn storybook-whimsical, modern ones clean and neutral, sci-fi ones
// crisp and neon.

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
        case 'H': return c(0xc49cff);
        case 'h': return c(0x7c5cc4);
        case 'E': return c(0x58d4c4);
        case 'e': return c(0x288a8c);
        case 'X': return c(0xffeece);
        case 'D': return c(0xeecc8c);
        case 'd': return c(0xbe925c);
        case 'I': return c(0x96d6ff);
        case 'i': return c(0x3c7cce);
        case 'J': return c(0xb25640);
        case 'j': return c(0x78342c);
        case 'k': return c(0x2c3038);
        case 'v': return c(0x96c878);
        default: return juce::Colours::transparentBlack;
    }
}

// empty: 2 frames
const char* const emptyRows[] {
    "................................",
    "................................",
    "................................",
    "................................",
    "................................",
    "................................",
    "................................",
    "................................",
    "......KKKKKKKKKKKKKKKKKK........",
    ".....KXXXXXXXXXXXXXXXXXXK.......",
    ".....KXXXXXXXXXYYXXXXXXXXK......",
    ".....KXXXXXXXXYYYYXXXXXXXXK.....",
    ".....KXXXXXXXXXYYXXXXXXXXXK.....",
    ".....KXXXXXXXXXXXXXXXXXXXK......",
    ".....KXDDDDDDDDDDDDDDDDDK.......",
    "......KKKKKKKKKWVKKKKKKK........",
    "..............KWVK..............",
    "..............KWVK..............",
    "..............KWVK..............",
    "..............KWVK..............",
    "..............KWVK..............",
    "..............KWVK..............",
    "........KKKKKKKWVKKKKKKK........",
    "......KKNNNNkkkWVkkkNNNNKK......",
    ".....KNNkkkkkkkWVkkkkkkkNNK.....",
    "....KNNkkkkkkkkWVkkkkkkkkNNK....",
    "....KNNkkkkkkkkkkkkkkkkkkNNK....",
    ".....KNNkkkkkkkkkkkkkkkkNNK.....",
    "......KKNNNNkkkkkkkkNNNNKK......",
    "........KKKKKKKKKKKKKKKK........",
    "................................",
    "................................",
};
const char* const emptyFrames[] {
    "",
    "5 28 Y;6 4 Y;10 15 ww;11 14 wwww;12 15 ww",
};
// castle: 3 frames
const char* const castleRows[] {
    "................YKKK............",
    "...............KVEEEK...........",
    "...............KVEEEEK..........",
    "..y...K........KVEeKK....K...w..",
    ".yYy.KQK.......KVKK.....KQK.....",
    "..y.KQQQK......KK......KQQQK....",
    "....KQQQHK....KHHK.....KQQQHK...",
    "...KQQQQHK....KHHK....KQQQQHK...",
    "...KQQQQQHK..KHHHHK...KQQQQQHK..",
    "..KQQQQQQHK..KHHHHK..KQQQQQQHK..",
    ".KQQQQQQQQHKKHHHHHHKKQQQQQQQQHK.",
    ".KQQQQQQQQHKKHHHHHHKKQQQQQQQQHK.",
    "KQhhhhhhhhXXKXXKXXKXXhhhhhhhhhHK",
    ".KKXXXXXXDXXKXXKXXKXXKXXXXXXDKK.",
    "..KXXXXXXDXXXXXXXXXXDDXXXXXXDK..",
    "..KXXXXXXDXXXXXXXXXXDDXXXXXXDK..",
    "..KXXXYXXDXXYYXXXXYYDDXXXYXXDK..",
    "..KXXYwYXDXXYYXXXXYYDDXXYwYXDK..",
    "..KXXYYYXDXXXXXXXXXXDDXXYYYXDK..",
    "..KXXXYXXDXXXXXXXXXXDDXXXYXXDK..",
    "..KXXXXXXDXXXXXXXXXXDDXXXXXXDK..",
    "..KXXXXXXDXXQXXVVXXXDDXXXXXXDK..",
    "..KXXXXXXDXXXXVVTVXXDDXXXXXXDK..",
    "..KXXXXXXDXXXVVVTVVXDDXXXXXXDK..",
    "..KXXXXXXDXXXVVVTVVXDDXXXXXQDK..",
    "..KXQXXXXDXXXVVVTYVXDDXXXXXXDK..",
    "KKKXXXXXXDXXXVVVTVVXDDXXXXXXDKKK",
    "GGKXXXXXXDXXXVVVTVVXDDXXXXXXDKGG",
    "GGGGGGGGGGGKKKKKKKKKKGGGGGGGGGGG",
    "GvGGGvGGGvGGGvGGGvGGGvGGGvGGGvGG",
    "gggggggggggggggggggggggggggggggg",
    "gggggggggggggggggggggggggggggggg",
};
const char* const castleFrames[] {
    "",
    "0 19 .;1 9 y;1 19 K;2 8 yYy;3 2 .;3 9 y;3 18 Ee;3 29 .;4 1 .w.;4 19 K;5 2 .",
    "0 18 ..;1 18 KK.;2 9 w;2 20 K.;2 29 y;3 2 .;3 18 EEeK;3 28 yYy;4 1 ...;4 19 KK;4 29 y;5 2 .",
};
// village: 3 frames
const char* const villageRows[] {
    "................................",
    "................................",
    "...........Aa...................",
    "................................",
    "............A...................",
    "............Aa..................",
    "................................",
    "...........AA...................",
    "...........KK...................",
    ".......KK.KbbK..................",
    "......KRRKKbbK..................",
    ".....KRRRRKbbK......KKKKKKKK....",
    "....KRRRRRrbbK....KKRRRRRwwRKK..",
    "...KRRRRRRRbbK...KRRRwwRRRRRRRK.",
    "...KRRRRRRRrrK...KRRRRRRRRRRRRK.",
    "..KRRRRRRRRRrrK.KRRwwRRRRRRwwRRK",
    ".KRRRRRRRRRRRrrKKRRRRRRRRRRRRRRK",
    "KRRRRRRRXXXXXXDK.KKKXXXXXXXDKKK.",
    ".KKXXXXXXXXXXXDK...KXXXXXYYDK...",
    "..KXYYXXXXXYYXDK...KXXXXXYYDK...",
    "..KXYYXXXXXYYXDK...KXXXXXXXDK...",
    "..KXXXXEEXXXXXDK...KXXVVVXXDK...",
    "..KXXXEEEEXXXXDK...KXXVVVXXDK...",
    "..KXXXEEEEEXXXDK...KXXVVYXXDK...",
    "K.KXXXEEEYEXXXDK..KKXXVVVXXDK...",
    "WKKXXXEEEEEXXXDK.KWKXXVVVXXDK.KK",
    "WGKXXXEEEEEXXXDK.KWKXXVVVXXDKKGG",
    "WGGGGGGGGGGKKKKKKKWKKGGGGGGGGGGG",
    "GGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGG",
    "GGHGGGGGGGGGGGGGGGQGGGGGGGGGGQGG",
    "gggggggggggggggggggggggggggggggg",
    "gggggggggggggggggggggggggggggggg",
};
const char* const villageFrames[] {
    "",
    "1 12 Aa;2 11 ..;3 11 A;4 11 Aa;5 12 ..;7 11 .;7 13 A",
    "0 11 Aa;2 11 .A;3 12 Aa;4 12 .;5 12 ..;6 11 AA;7 11 ..",
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
// factory: 3 frames
const char* const factoryRows[] {
    "......A.........................",
    ".........................a......",
    "....KAK.........................",
    "...KJJJK..................A.....",
    "...KJJJK........................",
    "...KwwwK................KAK.....",
    "...KJJJK...............KJJJK....",
    "...KJJJK...............KJJJK....",
    "...KJJJK...............KwwwK....",
    "...KJJJKK......K......KKJJJK.K..",
    "...KJJJKAK....KAK....KAKJJJKKAK.",
    "...KJJJAIK...KAIK...KAIKJJJKAIK.",
    "...KJJJAIK..KAAIK..KAAIKJJJAAIK.",
    "...KJJJAIK.KAAAIK.KAAAIKJJJAAIK.",
    "...KJJJAIKKAAAAIKKAAAAIKJJJAAIK.",
    "..KAJJJAIKAAAAAIKAAAAAIKJJJAAIK.",
    ".KaaaaaaaaaaaaaaaaaaaaaaaaaaabK.",
    ".KaaaaaaaaaaaaaaaaaaaaaaaaaaabK.",
    ".KaaaaaaaaaaaaaaaaaaaaaaaaaaabK.",
    ".KaaaaaaaaaaaaaaaaaaaaaaaaaaabK.",
    ".KaaYYYaaYYYaaYYYaaYYYaaYYYaabK.",
    ".KaaYYYaaYYYaaYYYaaYYYaaYYYaabK.",
    ".KaaYYYaaYYYaaYYYaaYYYaaYYYaabK.",
    ".KaaaaaaaaaaaaaaaaaaaaaaaaaaabK.",
    ".KaaaaaaaaaaabbbbbaaaaaaaaaaabK.",
    ".KaaaaaaaaaaabbbbbaaaaaaaaaaabK.",
    ".KaaaaaaaaaaabbbbbaaaaaaaaaaabK.",
    ".KaaaaaaaaaaabbbbbaaaaaaaaaaabK.",
    "KKaaaaaaaaaaabbbbbaaaaaaaaaaabKK",
    "kkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkk",
    "kkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkk",
    "kkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkk",
};
const char* const factoryFrames[] {
    "",
    "0 6 .;0 24 a;1 4 A;1 25 .;2 5 K;2 25 A;3 26 .;4 24 A;5 25 K",
    "0 5 A.;1 25 .A;2 5 K;3 25 A.;5 25 K",
};
// city: 3 frames
const char* const cityRows[] {
    "...............KRK..............",
    "...............KNK..............",
    ".............KKKNKK.............",
    "...w........KMMMMMNK............",
    "..www.......KMMMMMNK............",
    "............KMIMIMNK....KKKKKKK.",
    "............KMMMMMNK...KMMMMMMNK",
    ".......KKKKKKMMMMMNK...KMMMMMMNK",
    "......KIIIIINMIMIMNK...KMIMIMINK",
    "......KIIIIINMMMMMNK...KMMMMMMNK",
    "......KIwIwINMMMMMNKKKKKMMMMMMNK",
    "......KIIIIINMIMIMNiiiiNMIMIMINK",
    "......KIIIIINMMMMMNiiiiNMMMMMMNK",
    "......KIwIwINMMMMMNiwiwNMMMMMMNK",
    ".KKKKKKIIIIINMIMIMNiiiiNMIMIMINK",
    "KMMMMMNIIIIINMMMMMNiiiiNMMMMMMNK",
    "KMMMMMNIwIwINMMMMMNiwiwNMMMMMMNK",
    "KMIMIMNIIIIINMIMIMNiiiiNMIMIMINK",
    "KMMMMMNIIIIINMMMMMNiiiiNMMMMMMNK",
    "KMMMMMNIwIwINMMMMMNiwiwNMMMMMMNK",
    "KMIMIMNIIIIINMIMIMNiiiiNMIMIMINK",
    "KMMMMMNIIIIINMMMMMNiiiiNMMMMMMNK",
    "KMMMMMNIwIwINMMMMMNiwiwNMMMMMMNK",
    "KMIMIMNIIIIINMIMIMNiiiiNMIMIMINK",
    "KMllllNIIIIINMMMMMNiiiiNMMMMMMNK",
    "KMllllNIwIwINMMMMMNiwiwNMMMMMMNK",
    "KMllllNIIIIINMIMIMNiiiiNMIMIMINK",
    "KMllTlNIIIIINMMMMMNiiiiNMMMMMMNK",
    "KMMMTMNIIIIINMMMMMNiiiiNMMMMMMNK",
    "kkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkk",
    "kkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkk",
    "kkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkk",
};
const char* const cityFrames[] {
    "",
    "0 16 w;2 12 w;3 3 .;3 11 www;4 2 ...",
    "3 3 .;3 21 w;4 2 ...;4 20 www",
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
    "..KKQggggggggggggggggggggggKKK..",
    "KKggggggggHggggggggggggggggQggKK",
    "gggggggggggggggggggggggggggggggg",
};
const char* const forestFrames[] {
    "",
    "1 30 .;2 2 .;2 31 Z;3 3 Z;3 20 Z;4 11 .",
    "1 30 .;2 2 .;3 12 Z;4 2 Z;4 11 .",
    "0 30 Z;1 30 .;2 2 .;3 1 Z;4 11 .;4 21 Z",
};
// hive: 3 frames
const char* const hiveRows[] {
    "................................",
    "................................",
    "................................",
    "................................",
    "............KKKK................",
    "...........KllllK...............",
    "..........KllllllK..............",
    ".........KlZZZllllK.............",
    ".........KlZZZllllK.............",
    ".........KlZZZllllKKKKKK........",
    ".........KllllZllmppppppKK......",
    ".........KlllllZmmppppppppK.....",
    ".........KlllllmmmZZpppppppK....",
    "........KKKlllmmmZZZZppppppK....",
    "......KKllKKlmmmZZZZZZppppppK...",
    ".....KllllllKKppZZZZZZppppppK...",
    "....KllllllmmKppZZZZZZppppppK...",
    "...KlZZZZlmmmmpppZZZZppppppuK...",
    "..KllZZZZZmmmmpppppppZppppuuK...",
    "..KllZZZZZmmmmpppppppppppuuuK...",
    "..KllZZZZmmmmmppppppppppuuuuK...",
    "..KlllmmmZmmmmpppppppppuuuuuK...",
    "..KllmmmmmmmmmmpppppppuuuuppKK..",
    "..KlmmmmmmmmmmmppppppuuupZZpmpK.",
    "...KmmmmmmmmmmKKppppuuuupZZpmpK.",
    "....KmmmmmmmmK.KKKpuuuuupZZppmK.",
    "....KmmmmmmmK.KmK.KKKKKKpppZpmK.",
    "KKKKmKKKmmKKKKKmKKKKKKKKppppZumK",
    "uuumuuuuuuuuuumuuuuuuuuuppppuumu",
    "uuuuuuuuuuuuuuuuuuuuuuuuuupuuuuu",
    "uuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuu",
    "uuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuu",
};
const char* const hiveFrames[] {
    "",
    "10 14 L;11 15 L;17 20 L;18 21 L;20 8 L;21 9 L;26 27 L;27 28 L",
    "10 14 Q;11 15 P;17 20 Q;18 21 P;20 8 Q;21 9 P;26 27 Q;27 28 P",
};
// temple: 3 frames
const char* const templeRows[] {
    "...............KKK..............",
    "..............KYwYK.............",
    "..............KYYYK.............",
    "...............KYK..............",
    "...............KYK..............",
    ".............KKKYKK.............",
    "...........KKEEEEEEKK...........",
    "..........KEEEEEEEEEeK..........",
    ".........KEEEwEEEEEEeeK.........",
    "........KEEEwEEEEEEEEeeK........",
    "........KEEEEEEEEEEEEeeK........",
    ".......KEEEEEEEEEEEEEeeeK.......",
    ".....KKKEEEEEEEEEEEEEEeeKKK.....",
    "....KYYYYYYYYYYYYYYYYYYYYYYK....",
    "....KyyyyyyyyyyyyyyyyyyyyyyK....",
    ".....KKXDKKKKKKKKKKKKKKXDKK.....",
    "......KXDK............KXDK......",
    "......KXDK............KXDK......",
    "......KXDK.KK......KK.KXDK......",
    "...KK.KXDKKXDK.KK.KXDKKXDK.KK...",
    "..KOOKKXDKKXDKKXDKKXDKKXDKKOOK..",
    "..KOOKKXDKKXDKKXDKKXDKKXDKKOOK..",
    "..KOOKKXDKKXDKKXDKKXDKKXDKKOOK..",
    "...KVKKXDKKXDKKXDKKXDKKXDK.KVK..",
    "...KVKXXXXXXXXXXXXXXXXXXXXKKVK..",
    "...KVKXXXXXXXXXXXXXXXXXXXXKKVK..",
    "...KXXXXXXXXXXXXXXXXXXXXXXXXK...",
    "KK.KXXXXXXXXXXXXXXXXXXXXXXXXK.KK",
    "GGKKDDDDDDDDDDDDDDDDDDDDDDDDKKGG",
    "GGGGGGGGGGGKKKKKKKKKKGGGGGGGGGGG",
    "gggggggggggggggggggggggggggggggg",
    "gggggggggggggggggggggggggggggggg",
};
const char* const templeFrames[] {
    "",
    "0 15 w;0 17 w;1 16 Y;2 14 w;2 18 w;20 3 YY;20 27 YY;21 3 YY;21 27 YY;22 3 YY;22 27 YY",
    "1 16 Y;19 4 y;19 28 y;20 3 yy;20 27 yy;21 3 yy;21 27 yy;22 3 yy;22 27 yy",
};
// cavern: 3 frames
const char* const cavernRows[] {
    "................................",
    "................................",
    "................KK..............",
    ".............KKKAbKK............",
    "..........KKKAAAAAabKK..........",
    ".........KbAAAAAAaaaabKK........",
    "........KAAAbAAAaaaaaaabK.......",
    ".......KAAAAAAbaaaaaaaaaaK......",
    "......KAAAAAAAaabaaaaaaaaaK.....",
    ".....KAbAAAAAaaaaabaaaaaaaaK....",
    ".....KAAAbAAaaaaaaaabaaaaaaK....",
    "....KAAAAAAbaaaaaaaaaabaaaaaK...",
    "...KAAAAAAaaabaaaaaaaaaabaaaK...",
    "..KAbAAAAaaaaaabaaaaaaaaaabaaK..",
    "..KAAAbAaaaaaaaaabaaaaaaaaaabaK.",
    "..KAAAAabaaaaaaaaaabaaaaaaaaaaK.",
    "..KAAAaaaabaaaaaaaaaabaaaaaaaaK.",
    ".KAAAaaaaaaabBBBBBBaaaabaaaaaaK.",
    ".KAbaaaaaaaaBBaBBBaBaaaaabaaaaK.",
    ".KAaabaaaaBaBBaBBBaBBaaaaaabaaaK",
    ".KaaaaabaaBaBBaBBBaBBaaaaaaaabaK",
    ".KaaaaaaaBBaBBaBBBBBBaBaaaaaaaaK",
    "KaaaaaaaaBBBBBaBBBBBBaBaaaaaaaaK",
    "KabaaaaaBBBBBEEBBBBBBBBBbaaaaaaK",
    "KaaabaaaBBBBBEeBBBBBHHBBaabaaaaK",
    "KaaaaabaBwHBBEeBBBBBHhBBaaaabaaK",
    "KaaaaaaaIHhBBEeBBBBBHhBBaaaaaabK",
    "KaaaaaBBBHhBBEeBBBBBHhBEEBaaaaaa",
    "abaaaaBBBHhBBEIBBBBBHhBEeBaaaaaa",
    "aaabaaBBBHhBBEeBBBBBHhBEeBaaaaaa",
    "aaaaabBBBHhBBEeBBBBBHhBEeBabaaaa",
    "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb",
};
const char* const cavernFrames[] {
    "",
    "23 13 w;24 12 I;25 9 H;26 8 B;28 14 e;29 14 I",
    "24 20 w;25 9 H;25 19 I;26 8 B;28 14 e;30 14 I",
};
// ruins: 4 frames
const char* const ruinsRows[] {
    ".............K..................",
    ".........KKKKAKK................",
    "........KAAAAAAAK...............",
    ".......KAAaAAAAAK...............",
    "..K...KaAAAAAAAAK...............",
    ".KlK.KAAAAAAAAAAK...............",
    "..KlKAAAAAAKKKKK................",
    "...KKAAAAAK.....................",
    "...KAAAAAK......................",
    "...KAAAAAK........KKKKK.........",
    "...KAAAAAK.......KAAAAAK........",
    "...KlAAAAK.......KAAAAAK........",
    "...KlQAAAK........KAAAAK........",
    "...KAlAAAK........KAAAAK........",
    "...KAaAAAK........KAAAKK........",
    "...KlAAAAK........KAAAAaK.......",
    "...KAlAAAK..wI....KlAAAaK.......",
    "...KQAAAAK..II....KAlAAaK.......",
    "...KlAAAAK...i....KAQAAaK.......",
    "...KAAAAaK.i......KlAaAaK.......",
    "...KAAaAaK........KAAAAaK.......",
    "...KAAAAaK...KKKK.KAAAAaK.......",
    "...KAAAAlK..KAQAaKKAAAAaKKKKKK..",
    "...KAAAlaK..KAAAaKKAAAAaKAAQAAK.",
    "...KAAAAaK..KAAAaKKAAAAaKAAAAAK.",
    "KK.KAAAAaK..KAAAaKKAAAAaKAAAAAKK",
    "GGKKAAAAaKK.KAAAaKKAAAAaKaaaaaGG",
    "GGGGGGGGGGGKKKKKKKKKKGGGGGGGGGGG",
    "GGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGG",
    "GGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGG",
    "gggggggggggggggggggggggggggggggg",
    "gggggggggggggggggggggggggggggggg",
};
const char* const ruinsFrames[] {
    "",
    "14 13 wI;15 13 II;16 12 ..i;17 12 .i;18 13 .;19 11 .",
    "13 14 wI;14 14 II;15 15 i;16 12 .i;17 12 ..;18 13 .;19 11 .",
    "15 13 wI;16 12 .;16 14 I;17 12 ..i;19 11 .",
};
// spaceship: 3 frames
const char* const spaceshipRows[] {
    "................................",
    "................................",
    "................................",
    "............................Sc..",
    "....Sc........KKK...............",
    ".............KnnnK..............",
    "............KnnnK...............",
    "...........KnnnnK...............",
    "..........KnnnnnK...............",
    ".........KnnnnnK................",
    "........KnnnnnnKKKKKKKKKK.......",
    "........KMMMMMMMMMMMMMMMMK......",
    ".......KMMMMMMMMMMCCCCCCMMK.....",
    ".KK...KMMMMMMMMMMMCCwCCCCMMK....",
    "ynnK.KMMMMMMMMMMMMMCCCCCCCMMK...",
    "OnnKKMMMMMMMMMMMMMMMMMMMMMMMMK..",
    "YnnKMMMMMMMMMMMMMMMMMMMNNNNNNK..",
    "OnnMMMMMMMcccccccccccccccccNK...",
    "KnnKNNNNNNNNNNNNNNNNNNNNNNNK....",
    ".KK.KKNNNNNNNNNNNNNNNNNNNNK.....",
    "......KKNNNNNNNNNNNNNNNNNK......",
    "........KnnnnnnKKKKKKKKKK.......",
    ".........KnnnnnK................",
    "..........KnnnnnK...............",
    "...........KnnnnK...............",
    "............KnnnK...............",
    ".............KnnnK.........Sc...",
    "..............KKK...............",
    "...................Sc...........",
    "................................",
    "........Sc......................",
    "................................",
};
const char* const spaceshipFrames[] {
    "",
    "3 25 Sc;3 28 ..;4 1 Sc;4 4 ..;14 0 K;15 0 y;16 0 w;17 0 Y;26 24 Sc;26 27 ..;28 16 Sc;28 19 ..;30 5 Sc;30 8 ..",
    "3 22 Sc;3 28 ..;4 4 ..;4 30 Sc;14 0 K;16 0 O;17 0 y;26 21 Sc;26 27 ..;28 13 Sc;28 19 ..;30 2 Sc;30 8 ..",
};
// biodome: 3 frames
const char* const biodomeRows[] {
    "................................",
    "................................",
    "................................",
    "................................",
    "................................",
    "................................",
    "................................",
    "................................",
    "................................",
    "................................",
    "..................KKKK..........",
    "............KKKKKKllllKK........",
    "..........KKCCCCCCCCLlllK.......",
    "........KKCCBBBBlLLLCCllK.......",
    ".......KCCuBBBBllLLLLlCClK......",
    "......KCllllBuBllLLLLlllCK......",
    ".....KClLLlllBBllllllllllCK.....",
    "....KClLLLLlllBllllllllllBCK....",
    "....KClLLLLlllBBllllllllBBCK....",
    "...KwullLLllllBBllllTlllBBBCK...",
    "...KwBllllllllBBcBllTlcBBBBCK...",
    "..KCBBBlllTllBBBBBBBTBBBBBBBCK..",
    "..KCBBBBllTlBiiicBBBTBcBBBBBCK..",
    "..KCBBBBBBTBBiiiBBBBTBBBBBBBCK..",
    ".KKCBBBBBBTBBBBBcBBBTBcBBBBBCKK.",
    "KMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMK",
    "KNNNNNNNNNNNNNNNNNNNNNNNNZNNNNNK",
    "KNNCNNNNCNNNNCNNNNCNNNNCNNNNCNNK",
    "KnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnK",
    ".KKKKKKKKKKKKKKKKKKKKKKKKKKKKKK.",
    "................................",
    "................................",
};
const char* const biodomeFrames[] {
    "",
    "14 8 w;15 7 w;16 7 w;19 4 C;20 4 C;26 25 C",
    "12 12 ww;13 11 w;19 4 C;20 4 C",
};
// seaship: 3 frames
const char* const seashipRows[] {
    "................................",
    "...............KKK..............",
    "..............KRRRK.............",
    "..............KTKK..............",
    "..............KTK...............",
    ".............KKTXK..............",
    "............KXKTXXK...K.........",
    "...........KXXKTXXK..KTK........",
    "...........KXXKTXXXK.KTK........",
    "..........KXXXKTXXXXKKTXK.......",
    "..........KXXXKTXXXXKKTXK.......",
    ".........KXXXXKTXXXXXKTXXK......",
    "........KXXXXXKTXXXXXXTXXXK.....",
    "........KXXXXXKTXXXXXXXXXXK.....",
    ".......KXXXXXXKTXXXXXXXXXXXK....",
    ".......KXXXXXXKTXXXXXXXXXXXK....",
    "......KXXXXXXXKTXXXXXXXXXXXXK...",
    ".....KXXXXXXXXKTXXXXXXXXXXXXXK..",
    "......KKKKXXXXKTXXKKKKTXXXKKK...",
    "..........KKKKKTKK...KTKKK......",
    "....KKKKKKKKKKKTKKKKKKTKKKKK....",
    "...KVWWWWWWWWWWWWWWWWWWWWWWWK...",
    "....KVVVVVVVVVVVVVVVVVVVVVVK....",
    "KKKKKVVVVVVVVVVVVVVVVVVVVVKKKKKK",
    "wiiiiiVwwwwwwwwwwwwwwwwwwwiiiiii",
    "iwiiiiiVVwVVVVVVVwVVVVVVVwiiIiii",
    "iiiiiIiiiiiiiiIiiiiiiiiIiiiiiiii",
    "IiiiiiiiiIiiiiiiiiIiiiiiiiiIiiii",
    "iiiiIiiiiiiiiIiiiiiiiiIiiiiiiiiI",
    "iiiiiiiiIiiiiiiiiIiiiiiiiiIiiiii",
    "iiiIiiiiiiiiIiiiiiiiiIiiiiiiiiIi",
    "iiiiiiiIiiiiiiiiIiiiiiiiiIiiiiii",
};
const char* const seashipFrames[] {
    "",
    "1 15 ...;2 14 .KKK.;3 15 RRRK;4 17 K;5 13 .;5 16 K.;6 12 .K;6 17 K.;6 22 .;7 11 .K;7 21 .K.;8 18 K.;9 10 .K;9 19 K.;9 23 K.;11 9 .K;11 20 K;11 24 K.;12 8 .K;12 21 K;12 25 K.;13 22 T;14 7 .K;14 26 K.;16 6 .K;16 27 K.;17 5 .K;17 28 K.;18 5 KXXXX;18 18 XXXXX;18 26 XXXK;19 6 KKKKXXXX;19 16 XXKKK;19 23 XXXKKK;20 4 ......;20 18 ...;20 26 ..;21 3 .KKKKKKKKKKKTKKKKKKTKKKKK.;22 3 KV;22 27 VK;23 26 V;24 0 iI;24 6 wVVVVVVV;24 15 VVVVVVV;24 23 VVV;24 28 I;24 30 w;25 1 i;25 5 w;25 7 ww;25 10 wwwwwww;25 18 wwwwwww;25 28 iw;26 0 I;26 5 i;26 9 I;26 14 i;26 18 I;26 23 i;26 27 I;27 0 i;27 4 I;27 9 i;27 13 I;27 18 i;27 22 I;27 27 i;27 31 I;28 4 i;28 8 I;28 13 i;28 17 I;28 22 i;28 26 I;28 31 i;29 3 I;29 8 i;29 12 I;29 17 i;29 21 I;29 26 i;29 30 I;30 3 i;30 7 I;30 12 i;30 16 I;30 21 i;30 25 I;30 30 i;31 2 I;31 7 i;31 11 I;31 16 i;31 20 I;31 25 i;31 29 I",
    "24 0 i;24 2 w;24 26 w;25 1 I;25 3 w;25 9 V;25 11 w;25 17 V;25 19 w;25 25 i;25 27 w",
};
// suburbs: 2 frames
const char* const suburbsRows[] {
    "................................",
    "................................",
    "................................",
    "................................",
    "................................",
    "................................",
    "............KK..................",
    "...........KiBK.KK..............",
    "..........KiiiBKJJK.............",
    ".........KiiiiiBJJK.............",
    "........KiiiiiiBJJK.............",
    ".......KiiiiiiiiJJK.....KKKKKK..",
    "......KiiiiiiiiiJJBK...KllllllK.",
    ".....KiiiiiiiiiiiBBBK.KlLLLLlllK",
    "....KiiiiiiiiiiiiiBBBKKlLLLLlllK",
    "...KiiiiiiiiiiiiiiiBBBKlLLLLlllK",
    "....KXXXXXXXXXXXXXXXDKKlLLLLlllK",
    "....KXXXXXXXXXXXXXXXDKKllllllllK",
    "....KXXwIIIXXXXwIIIXDKKllllllllK",
    "....KXXIIIIXXXXIIIIXDK.KllllllK.",
    ".KK.KXXIIIIXXXXIIIIXDK..KKKTKK..",
    "KiiKKXXXXXXRRRXXXXXXDK....KTK...",
    "KiiKKXXXXXXRRRXXXXXXDK....KTK...",
    ".KkKKXXXXXXRRRXXXXXXDK.K.KKTKK.K",
    "KwkwKXXXXXXRRYXXXXXXDKKwKwKTKwKw",
    "wwkwKXXXXXXRRRXXXXXXDKwwwwwTwwww",
    "KwkwKXXXXXXRRRXXXXXXDKKwKwKTKwKw",
    "GwGwGGGGGGGGGGGGGGGGGGGwGwGwGwGw",
    "GGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGG",
    "GGGGGGIGGGIGGGGGGGGGGGGGGGGGGGGG",
    "GGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGG",
    "gggggggggggggggggggggggggggggggg",
};
const char* const suburbsFrames[] {
    "",
    "18 16 YYY;19 15 YYYY;20 15 YYYY;28 8 I;29 6 G;29 10 G",
};
// mall: 2 frames
const char* const mallRows[] {
    "................................",
    "................................",
    "................R...............",
    "...............KNK..............",
    "...............KNK..............",
    "...............KNK..............",
    "........KKKKKKKKNKKKKKKK........",
    ".......KkkkkkkkkkkkkkkkkK.......",
    ".......KkkQQkQQkkQQkQQkkK.......",
    ".......KkkQQkQQkkQQkQQkkK.......",
    ".......KkkQQkQQkkQQkQQkkK.......",
    ".......KkkkkkkkkkkkkkkkkK.......",
    ".KKKKKKKKKKKKKKKKKKKKKKKKKKKKKK.",
    "KAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAK",
    "KXXXXXXXXXXXXXXXXXXXXXXXXXXXXXDK",
    "KXXXXXXXXXXXXXXXXXXXXXXXXXXXXXDK",
    "KXXXXXXXXXXXXXXXXXXXXXXXXXXXXXDK",
    "KXXXXXXXXXXXXXXXXXXXXXXXXXXXXXDK",
    "KXXMIIIMIIIMIIIMIIIMIIIMIIIMIXDK",
    "KXXMIIIMIIIMIIIMIIIMIIIMIIIMIXDK",
    "KXXMwIIwIIwMIwIMwIIwIIwMIwIMIXDK",
    "KXXMIIIMIIIMiiiiMiiiIIIMIIIMIXDK",
    "KXXMIIIMIIIMiiiiMiiiIIIMIIIMIXDK",
    "KXXMIIIMIIIMiiiiMiiiIIIMIIIMIXDK",
    "KXXMIIIMIIIMiiiiMiiiIIIMIIIMIXDK",
    "KXXMIIIMIIIMiiiiMiiiIIIMIIIMIXDK",
    "KXXMIIIMIIIMiiiiMiiiIIIMIIIMIXDK",
    "KXXXXXXXXXXXiiiiMiiiXXXXXXXXXXDK",
    "KXXXXXXXXXXXiiiiMiiiXXXXXXXXXXDK",
    "kkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkk",
    "kkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkk",
    "kkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkk",
};
const char* const mallFrames[] {
    "",
    "2 16 w;8 10 pp;8 13 PP;8 17 pp;8 20 PP;9 10 pp;9 13 PP;9 17 pp;9 20 PP;10 10 pp;10 13 PP;10 17 pp;10 20 PP",
};
// military: 3 frames
const char* const militaryRows[] {
    "................................",
    "................................",
    "...............KKKKK............",
    "..............KNRRRRK...........",
    "..............KNwwwwK...........",
    "..............KNRRRRK...........",
    "..............KNKKKK............",
    "..............KNK...............",
    "..............KNK...............",
    "..............KNK...............",
    "..............KNK...............",
    "..............KNK...............",
    "..............KNK...............",
    "..........KK..KNK...............",
    ".........KlgK.KNK...............",
    "........KlllgKKNK...............",
    "........KlllgKKNK...............",
    ".......KllllggKNK......KK.......",
    "......KlllllgggNK.....KlgK......",
    "......KlllllgggNK....KlllgK.....",
    ".....KlllllllggNK....KlllgK.....",
    ".....KlllllllggNK...KllllggK....",
    "....KlllllmllggNgK.KllllllggK...",
    ".KKKllllllmmlggNggKlllllllgggK..",
    "KVVVVlllllmmlggNggKlllmmmmgggK..",
    "KWWWWllllmmmllgNggllllmmmmlgggK.",
    "KWWWWllllmmmmlgNglllllmmmmlggggK",
    "vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv",
    "vgvvvgvvvgvvvgvvvgvvvgvvvgvvvgvv",
    "vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv",
    "vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv",
    "gggggggggggggggggggggggggggggggg",
};
const char* const militaryFrames[] {
    "",
    "2 19 .;3 19 K;4 20 wK",
    "5 19 K.;6 19 RK;7 19 K",
};
// beach: 3 frames
const char* const beachRows[] {
    "................................",
    "................................",
    "................................",
    "................................",
    "......................KKKKKK....",
    ".....................KYYYYYYK...",
    "............K.......KYYYYYYYYK..",
    ".........KKKLKKK....KYYwYYYYYK..",
    ".......KKLLLlLLLKK..KYYYYYYYYK..",
    "......KLLLLVVlLLLLK.KYYYYYYYYK..",
    ".....KLllllVVVllllLKKYYYYYYYYK..",
    ".....KlKLKKTVKKKLKlKKYYYYYYYYK..",
    "......KKlKKTWK.KlKK..KYYYYYYK...",
    "........K.KTVK..K.....KKKKKK....",
    "..........KTVK..................",
    ".........KTWK...................",
    ".........KTVK...................",
    ".........KTVK...................",
    ".........KTWK...................",
    "........KTVK....................",
    "........KTVK....................",
    "KKKKKKKKKTWKKKKKKKKKKKKKKKKKKKKK",
    "iiiiiIiiiTViIiiiiiiIiiiiiiIiiiii",
    "wwiIiiwwTVIiwwiiiIwwiiiiwwiiiiww",
    "iIiiiiiiTWiiiiiIiiiiiiIiiiiiiIii",
    "iiiiiiIiTVDDDDiiiiiiIiiiiiiIiiii",
    "iiiDDDDDTVDDDDDDDDRiwwiRiIiiiiii",
    "DDDDDDDDDDDDDDDDDDDRRRRIiiiiiiIi",
    "DDDDDDDDDDDDDDDDDDDRRRRDDDiiIiii",
    "DDDDDDDDDDDDDDDDDDDDRDDDDDDDDDii",
    "DdDDDDdDDDDdDDDDdDDDDdDDDDdDDDDd",
    "DDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDD",
};
const char* const beachFrames[] {
    "",
    "23 0 ii;23 4 wwii;23 10 wwii;23 16 wwii;23 22 wwIi;23 28 wwiI;29 20 DR",
    "23 0 iiww;23 6 ii;23 12 iiww;23 18 iiww;23 24 Iiww;23 30 iI",
};
// desert: 3 frames
const char* const desertRows[] {
    "................................",
    "................................",
    "................................",
    "....................KKKK........",
    "....w.............KKOOOOKK......",
    ".................KOOOYYOOOK.....",
    ".................KOYYYYYYOK.....",
    "................KOOYwYYYYOOK....",
    "................KOYYYYYYYYOK....",
    "................KOYYYYYYYYOK....",
    "................KOOYYYYYYOOK....",
    "...........KK....KOYYYYYYOK.....",
    "..........KDdK...KOOOYYOOOK.....",
    ".........KDdddK...KKOOOOKK......",
    ".........KDDDdK.....KKKK..K.....",
    "........KDDDDddK........KKQK....",
    "........KdddddddK......KlKllKK..",
    ".......KDDDDDdddK......KlKllKlK.",
    "......KDDDDDDddddK.....KllllKlK.",
    "......KDdddddddddK......KKllllK.",
    ".....KDDDDDDDdddddK......KllKK..",
    ".....KDDDDDDDDddddK......KllK.KK",
    "..KKKDDDDDDDDDdddddK.....KllKKDD",
    "KKDDDDDDDDDDDDDDDdddK...KKllDDDD",
    "DDDDDDDDDDDDDDDDDDDDKKKKDDllDDDD",
    "DDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDD",
    "DDDDDDDDDDXDDDDDDDDDDDDDDDDDDDDD",
    "DDDXDDDDDDDDDDDDXDDddddDDDDDDDDD",
    "DDDDDDDDDDDDDDDDdddddddddddddDDD",
    "DDDDDDDDDDDDDDdddddddddddddddddd",
    "DDDDDDDDDDDDdddddddddddddddddddd",
    "DDDDDDDDDddddddddddddddddddddddd",
};
const char* const desertFrames[] {
    "",
    "4 4 .;5 4 w;26 10 DX;27 3 DX;27 16 DX",
    "26 10 D;26 12 X;27 3 D;27 5 X;27 16 D;27 18 X",
};
// palace: 3 frames
const char* const palaceRows[] {
    "...............KYK..............",
    "...............KYK..............",
    ".........Q.....KYK..............",
    "....K...QYQ....KQQK........K....",
    "...KwK...Q.....KQQK.......KYK...",
    "...KYK.......KKQQQQK......KYK...",
    "...KYK......KQQQQQQHK.....KYK...",
    "...KEEK....KQQQQQQQHHK....KEEK..",
    "...KEEK....KQQQQQQQHHK....KEEK..",
    "..KEEEeK...KQQQQQQQHHK...KEEEeK.",
    ".KEEEEeK...KQQQQQQQHHK..KEEEEeK.",
    ".KEEEEeK...KQQQQQQQHHK..KEEEEeK.",
    ".KEEEEeK...KQQQQQQQHHK..KEEEEeK.",
    ".KEEEEeK....KQQQQQQHK...KEEEEeK.",
    ".KXEEEXK....KXXXQXXDK...KXEEEXK.",
    ".KXXXXXK....KXXXXXXDK...KXXXXXK.",
    ".KXXXXXKKKKKKXXXXXXDKKKKKXXXXXK.",
    ".KXYYYYYYYYYYYYYYYYYYYYYYYYYYXK.",
    "..KXXXXXXXXXXXXXXXXXXXXXXXXXXK..",
    "..KXXXXXXXXXXXXXXXXXXXXXXXXXXK..",
    "..KXXXXXXXXXXXXXXXXXXXXXXXXXXK..",
    "..KXYYXXYYXXYYXXYYXXYYXXYYXXXK..",
    "..KXYYXXYYXXYYXXYYXXYYXXYYXXXK..",
    "..KXYYXXYYXXYYXVVYXXYYXXYYXXXK..",
    "..KXYYXXYYXXYYVVVVXXYYXXYYXXXK..",
    "..KXXXXXXXXXXVVVVVVXXXXXXXXXXK..",
    "..KXXXXXXXXXXVVVVVVXXXXXXXXXXK..",
    "KKKXXXXXXXXXXXVVVVVXXXXXXXXXXKKK",
    "GGKDDDDDDDDDDDVVVVVDDDDDDDDDDKGG",
    "GGGGGGGGGGGKKKKKKKKKKGGGGGGGGGGG",
    "gggggggggggggggggggggggggggggggg",
    "gggggggggggggggggggggggggggggggg",
};
const char* const palaceFrames[] {
    "",
    "0 16 w;1 23 Q;2 9 .;2 22 QYQ;3 8 ...;3 23 Q;4 4 Y;4 9 .",
    "1 2 Q;2 1 QYQ;2 9 .;3 2 Q;3 8 ...;4 4 Y;4 9 .;4 27 w",
};
// tavern: 4 frames
const char* const tavernRows[] {
    "................................",
    ".........Y...........aA.........",
    "................................",
    "...Y................KKaAK.......",
    "...................KaaaaaK......",
    "...........KKKKKKKKKKbbbK.......",
    ".........KKQQQHQQQHQKbbbK.......",
    "........KHQQQHQQQHQQQbbbK.......",
    ".......KHQQQHQQQHQQQHbbbK.......",
    "......KHQQQHQQQHQQQHQbbbK.......",
    ".....KHQQQHQQQHQQQHQQbbbK.......",
    "....KHQQQHQQQHQQQHQQQHQQQK......",
    "...KHQQQHQQQHQQQHQQQHQQQHQK.....",
    "..KHQQQHQQQHQQQHQQQHQQQHQQQKK...",
    ".KHHHHHHHHHHHHHHHHHHHHHHHHHHHK..",
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
    "0 22 aA;1 9 w;1 21 ..;2 23 aA;3 3 .;3 22 KK;4 3 Y;17 30 VK;18 26 .K;18 31 W;19 26 .KWwYW;20 26 .KW;20 30 YW;21 26 .K;21 31 W;22 27 .;22 31 K",
    "0 23 aA;1 21 ..;2 22 aA;3 22 KK;16 31 K;17 31 V;18 26 ..K;18 31 W;19 26 ..KWwY;20 26 ..KWYY;21 26 ..K;21 31 W;22 27 ..;22 31 K",
    "1 9 w;1 21 .aA;3 3 .;3 21 aAK;4 3 Y;17 30 VK;18 26 .K;18 31 W;19 26 .KWwYW;20 26 .KW;20 30 YW;21 26 .K;21 31 W;22 27 .;22 31 K",
};
// station: 3 frames
const char* const stationRows[] {
    "................................",
    "................................",
    ".KKKKKKK................KKKKKKK.",
    "KiicicicK......KK......KiicicicK",
    "KiicicicKKKKKKKNNKKKKKKKiicicicK",
    "KiicicicNNNNNNNNNNNNNNNNiicicicK",
    "KiicicicKKKKKKKNNKKKKKKKiicicicK",
    "KiicicicK.....KNNK.....KiicicicK",
    ".KKKKKKK......KNNK......KKKKKKK.",
    "..............KNNK..............",
    ".........KKKKKKNNKKKKKK.........",
    ".......KKMMMMMMNNMMMMMMKK.......",
    ".....KKMMMNKKKKNCKKKKNMMMKK.....",
    "....KMMMNKK..KMMMMK..KKNMMMK....",
    "...KMMMNK...KMwwMMMK...KNMMMK...",
    "...KMMNK....KMwwMMMK....KNMMK...",
    "...KMRNK....KMMMMMMK....KNMCK...",
    "...KMMMNK...KMMMMnMK...KNMMMK...",
    "....KMMMNKK..KMMMMK..KKNMMMK....",
    ".....KKMMMNKKKKNNKKKKNMMMKK.....",
    ".......KKMMMMMMNNMMMMMMKK.......",
    ".........KKKKKKNRKKKKKK.........",
    "..............KNNK..............",
    ".KKKKKKK......KNNK......KKKKKKK.",
    "KiicicicK.....KNNK.....KiicicicK",
    "KiicicicKKKKKKKNNKKKKKKKiicicicK",
    "KiicicicNNNNNNNNNNNNNNNNiicicicK",
    "KiicicicKKKKKKKNNKKKKKKKiicicicK",
    "KiicicicK......KK......KiicicicK",
    ".KKKKKKK................KKKKKKK.",
    "................................",
    "................................",
};
const char* const stationFrames[] {
    "",
    "12 16 R;12 30 S;16 5 C;21 16 C",
    "16 5 C;16 27 R;20 2 S;21 16 C",
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
// bridge: 3 frames
const char* const bridgeRows[] {
    "................................",
    "................................",
    "..KKKKKKKKKKKKKKKKKKKKKKKKKKKK..",
    ".KBBBBBBBBBBBBBBBBBBBBBBBBBBBBK.",
    ".KBBBBBBBBBBBBBBBBBBBBBBBBBBBBK.",
    ".KBBBBBBBBBBBBBBBBBBSBBBBBBBBBK.",
    ".KBBBScBBBBBBBBBBBBBBBBBBBBBBBK.",
    ".KBBBBBBBBBBBBBBBBBBwwHHBBBBBBK.",
    ".KBBBBBBBBBBBBBBScBBwwHHBBBBBBK.",
    ".KBBBBBBBBBBBBBBBBBBHHHHBScBBBK.",
    ".KBBBBBBBBBBBBBBBBBBHHHHBBBBBBK.",
    ".KBBBBBBBBBScBBBBBBBBBBBBBBBBBK.",
    ".KBBBBBBBBBBBBBBBBBBBBBBBBBBBBK.",
    ".KBBBBBBBSBBBBBBBBBBBBBBSBBBBBK.",
    ".KBBBBBBBBBBBBBBBBBBBBBBBBBBBBK.",
    ".KBBBBBBBBBBBBBBBBBBBBBBBBBBBBK.",
    ".KnnnnnnnnnnnnnnnnnnnnnnnnnnnnK.",
    "..KKKKKKKKKKKKKKKKKKKKKKKKKKKK..",
    "................................",
    "...KKKKKKKKKKKKKKKKKKKKKKKKKK...",
    "..KNNNNNNNNNNNNNNNNNNNNNNNNNNK..",
    "..KnnnnnnnnnnnMMMMnnnnnnnnnnnK..",
    ".KnnnkknZZnRRnkkMPPnCCnkknRRnnK.",
    ".KnnnnnnnnnnnNMMMMNnnnnnnnnnnnK.",
    ".KnnnnnnnnnnnNNNNNNnnnnnnnnnnnK.",
    "KnnnnnnnnnnnnNNNNNNnnnnnnnnnnnK.",
    "KnnnnnnnnnnnnNNNNNNnnnnnnnnnnnnK",
    "KnnnnnnnnnnnnNNNNNNnnnnnnnnnnnnK",
    "KnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnK",
    "nnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnn",
    "nnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnn",
    "KKKKKKKKKKKKKKKKKKKKKKKKKKKKKKKK",
};
const char* const bridgeFrames[] {
    "",
    "6 4 ScB;8 15 ScB;9 24 ScB;11 10 ScB;22 5 ZZ;22 8 RR;22 11 kk;22 14 PP;22 17 CC;22 20 kk;22 23 RR;22 26 YY",
    "6 3 ScBB;8 14 ScBB;9 23 ScBB;11 9 ScBB;22 5 RR;22 8 kk;22 11 PP;22 14 CC;22 17 kk;22 20 RR;22 23 YY;22 26 kk",
};
// mining: 3 frames
const char* const miningRows[] {
    "........KKKKKKKKKKKKK...........",
    ".......KYYYYYYYYYYYYYK..........",
    "........KKMMMMMMMMKKK...........",
    ".........KMMMMMMMMK.............",
    "..........KKKNNKKK..............",
    "............KNNK................",
    "............KNNK................",
    "............KNNK................",
    "............KNNK................",
    "..........KKaNNKKKKK............",
    "........KKaOOOOOObbbKKKK........",
    "......KKaaaaOOOObbbbbbbbK.......",
    ".....KaaaaaaaOObbbbbbbbbbK......",
    "....KaaaaaaOabbbbbbbbbbbbbK.....",
    "...KaaaaaaaabbwbbbkkbbbbbbbK....",
    "...KaaaaaaabObbbbbkkbbbbbbbbK...",
    "..KaaaaaaabbbbbbbbbbbbbbbbbbbK..",
    "..KaaaaaabbbbbbbbbbbbbbbbbbbbbK.",
    "..KaaaaakkkkbbbbbbbbbbbbbbbbbbK.",
    ".KaaaaabkkkkbbbbbbbbbbbbbbbbbK..",
    ".KaaaabbkkkkbbbbbbbbbbbbbbbbbK..",
    "KaaaabbbkkkkbbbbbbbbbbbbbbbbbK..",
    "KaaabbbbbbbbbbbbbbbbbkkbbbbbbK..",
    ".KabbbbbbbbbbbbbbbbbkkkkbbbbbK..",
    "..KbbbbbbbbbbbbbbbbbkkkkbbbbK...",
    "...KbbbbbbbbbbbbbbbbbkkbbbbbK...",
    "....KbbbbbbbbbbbbbbbbbbbbbbbK...",
    ".....KbbbbbbbbbbbbbbbbbbbbKK....",
    "......KKKbbbbbbbbbbbbbbKKK......",
    ".........KKKKKKbbbbbKKK.........",
    "...............KKKKK............",
    "................................",
};
const char* const miningFrames[] {
    "",
    "1 18 R;13 11 a;13 18 Y;15 12 b;15 20 Y",
    "",
};
// alienruins: 3 frames
const char* const alienruinsRows[] {
    "..............KnK...............",
    ".............KnnnK..............",
    "............KnnnnnK.............",
    "...........KnnnnnnnK............",
    "...........KnnnnkkkK............",
    "...........KnnnnkkkK............",
    "...........KnnnnkkkK............",
    "...........KnnnnkkkK............",
    "...........KnnCCkkkK............",
    "...........KnnnCkkkK............",
    ".....nnn..KnnnnnkkkkK...........",
    ".....nnn..KnnnnnkkkkK....nnn....",
    "......C...KnnnncckkkK....nnn....",
    "..........KnnnnnckkkK.....C.....",
    "..........KnnnnnkkkkK...........",
    "..........KnnnnnkkkkK...........",
    "..........KnnnccnkkkK...........",
    ".KKKKK....KnnnncnkkkK...........",
    "KnnnnkK...KnnnnnnkkkK...........",
    "KnnnnkK...KnnnnnnkkkK...........",
    "KnnnnkK...KnnnnCCkkkK.....KKKKK.",
    "KnnnnkK...KnnnnnCkkkK....KnnnnkK",
    "KnnPnkK..KnnnnnnnkkkkK...KnnnnkK",
    "KnnnnkK..KnnnnnnnkkkkK...KnnnnkK",
    "KnnnnkK..KnnnnccnkkkkK...KnnpnkK",
    "KnnnnkK..KnnnnncnkkkkK...KnnnnkK",
    "KnnnnkK..KnnnnnnnkkkkK...KnnnnkK",
    "KnnnnkKKKKnnnnnnnkkkkKKKKKnnnnkK",
    "uuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuu",
    "uuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuu",
    "uuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuu",
    "uuuuuuuuuuuuuuuuuuuuuuuuuuuuuuuu",
};
const char* const alienruinsFrames[] {
    "",
    "8 14 cc;9 5 nnn;9 15 c;11 5 .C.;11 25 ...;12 6 .;13 25 nnn;14 26 C;16 14 CC;17 15 C;20 15 cc;21 16 c;22 3 p",
    "8 14 cc;9 15 c;12 15 CC;13 16 C;20 15 cc;21 16 c;22 3 p;24 14 CC;24 28 P;25 15 C",
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
    const char* const* rows;
    const char* const* frames;
    int frameCount;
};
// In wb::PlaceType order (Empty first).
const IconDef icons[] {
        { emptyRows, emptyFrames, int(std::size(emptyFrames)) },
        { castleRows, castleFrames, int(std::size(castleFrames)) },
        { villageRows, villageFrames, int(std::size(villageFrames)) },
        { spaceportRows, spaceportFrames, int(std::size(spaceportFrames)) },
        { factoryRows, factoryFrames, int(std::size(factoryFrames)) },
        { cityRows, cityFrames, int(std::size(cityFrames)) },
        { forestRows, forestFrames, int(std::size(forestFrames)) },
        { hiveRows, hiveFrames, int(std::size(hiveFrames)) },
        { templeRows, templeFrames, int(std::size(templeFrames)) },
        { cavernRows, cavernFrames, int(std::size(cavernFrames)) },
        { ruinsRows, ruinsFrames, int(std::size(ruinsFrames)) },
        { spaceshipRows, spaceshipFrames, int(std::size(spaceshipFrames)) },
        { biodomeRows, biodomeFrames, int(std::size(biodomeFrames)) },
        { seashipRows, seashipFrames, int(std::size(seashipFrames)) },
        { suburbsRows, suburbsFrames, int(std::size(suburbsFrames)) },
        { mallRows, mallFrames, int(std::size(mallFrames)) },
        { militaryRows, militaryFrames, int(std::size(militaryFrames)) },
        { beachRows, beachFrames, int(std::size(beachFrames)) },
        { desertRows, desertFrames, int(std::size(desertFrames)) },
        { palaceRows, palaceFrames, int(std::size(palaceFrames)) },
        { tavernRows, tavernFrames, int(std::size(tavernFrames)) },
        { stationRows, stationFrames, int(std::size(stationFrames)) },
        { megacityRows, megacityFrames, int(std::size(megacityFrames)) },
        { bridgeRows, bridgeFrames, int(std::size(bridgeFrames)) },
        { miningRows, miningFrames, int(std::size(miningFrames)) },
        { alienruinsRows, alienruinsFrames, int(std::size(alienruinsFrames)) },
        { nebulaRows, nebulaFrames, int(std::size(nebulaFrames)) },
};
static_assert(std::size(icons) == std::size_t(PlaceTypeCount), "an icon for every Place");

const IconDef* iconFor(int type) { return juce::isPositiveAndBelow(type, int(PlaceTypeCount)) ? &icons[type] : nullptr; }

// Short names for small spaces (cards, chips), in wb::PlaceType order.
constexpr const char* shortNames[] { "EMPTY", "CASTLE", "VILLAGE", "SPACEPORT", "FACTORY", "CITY", "FOREST", "HIVE", "TEMPLE", "CAVERN",
                                      "RUINS", "SPACESHIP", "BIODOME", "SEA SHIP", "SUBURBS", "MALL", "MILITARY", "BEACH", "DESERT",
                                      "PALACE", "TAVERN", "STATION", "MEGACITY", "BRIDGE", "MINING", "XENO RUINS", "NEBULA" };
static_assert(std::size(shortNames) == std::size_t(PlaceTypeCount), "a short name for every Place");
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

juce::String placeShortName(int placeType) { return juce::isPositiveAndBelow(placeType, int(PlaceTypeCount)) ? shortNames[placeType] : ""; }

int placeIconFrames(int placeType) {
    const auto sheet = bundledSprite(juce::String("place-") + placeKeys[std::size_t(juce::jlimit(0, int(PlaceTypeCount) - 1, placeType))]);
    if (sheet.isValid() && sheet.getHeight() > 0) return std::max(1, sheet.getWidth() / sheet.getHeight());
    const auto* icon = iconFor(placeType);
    return icon != nullptr ? icon->frameCount : 1;
}

Canvas placeIcon(int placeType, int frame, juce::Colour outline) {
    Canvas k(iconSize, iconSize);
    if (placeType < NoPlace || placeType >= PlaceTypeCount) return k;
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
