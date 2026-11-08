//==============================================================================
#include <cstring>
#include <cstdlib>
// An old session (the company rename): a session saved by the build before the rename
// (commit 4c81d33, Tests/fixtures/old-session.wbstate, written by --write-old-session
// with that build) must load unchanged: the AU codes, the parameter IDs and the state
// format did not change, only the names hosts and users see. "Unchanged": loaded and
// saved again, it gives exactly the state that build gave when it loaded and saved it
// (old-session.wbstate.expected.xml, written with it; loading adds the properties a new
// session had not set yet, so the saved file itself is not the comparison).
namespace oldSession {
// A session with some of everything: World and Place parameters, edited patterns, a
// saved Location with a name, an arrangement and the KEYS target.
void make(Host& h) {
    h.set("temperature", 0.15f);
    h.set("weather", 0.8f);
    h.set("seed", 31337);
    h.set("root", 5);
    h.set("mood", 3);
    h.set("air_level", -4.5f);
    h.set("bed_mute", 1);
    h.set("p1_type", float(Castle));
    h.set("p1_character", 1);
    h.set("p1_tension", 0.7f);
    h.set("p2_type", float(Spaceport));
    h.set("p2_character", 2);
    h.set("p3_type", float(Village));
    h.set("p3_sync", 0);
    h.set("p4_seed", 4711);
    h.set(fxId(0, 1, FxReverb), 0.6f);
    h.p->patterns.syncTypes(h.p->slotTypes());
    h.p->patterns.setElement(0, Castle, 1, editedElement({ makeNote(0, 0, 4), makeNote(6, 2, 2, 80), makeNote(10, 4, 3, 60, true) }, 0.3f, pat::VaryVelocity, 2));
    h.p->patterns.setElement(2, Village, 0, editedElement({ makeNote(0, 7, 2, 100), makeNote(8, 9, 2, 90, false, 50) }));
    h.p->saveLocation(0);
    h.p->locations.rename(0, "Old Harbour");
    h.p->locations.setColour(0, 3);
    h.p->arrangement.set(arrangeTest::classicArc());
    h.p->keys.slot.store(1);
    h.p->keys.element.store(2);
    h.render(0.5);
}

// The state's XML text as saved (JUCE's binary XML: a magic number, the text's size, the
// text), without the diagnostic `samples` count (how many recorded notes this build has;
// never read back). The AU host harness compares the same way.
juce::String xmlOf(const juce::MemoryBlock& state) {
    if (state.getSize() <= 8) return {};
    const auto* bytes = static_cast<const char*>(state.getData());
    uint32_t magic = 0, size = 0;
    std::memcpy(&magic, bytes, 4);
    std::memcpy(&size, bytes + 4, 4);
    if (magic != 0x21324356u || size_t(size) + 8 > state.getSize()) return {};
    std::string xml(bytes + 8, size);
    while (!xml.empty() && xml.back() == '\0') xml.pop_back();
    const auto at = xml.find(" samples=\"");
    if (at != std::string::npos) {
        const auto end = xml.find('"', at + 10);
        if (end != std::string::npos) xml.erase(at, end + 1 - at);
    }
    return juce::String::fromUTF8(xml.data(), int(xml.size()));
}
juce::String xmlOf(Host& h) {
    juce::MemoryBlock state;
    h.p->getStateInformation(state);
    return xmlOf(state);
}

// Two saved states are the same: the same text, except that a number in an attribute value
// may differ in its last float digits (the parameters' float maths on another platform or
// architecture: 0.8999998569 for 0.9000000954). `where`: the first difference.
bool sameState(const std::string& a, const std::string& b, std::size_t* where = nullptr) {
    const auto split = [](const std::string& s) {
        std::vector<std::string> parts;
        std::size_t start = 0;
        for (;;) {
            const auto quote = s.find('"', start);
            if (quote == std::string::npos) { parts.push_back(s.substr(start)); break; }
            parts.push_back(s.substr(start, quote - start));
            start = quote + 1;
        }
        return parts;
    };
    const auto number = [](const std::string& s, double& value) {
        if (s.empty()) return false;
        char* end = nullptr;
        value = std::strtod(s.c_str(), &end);
        return end == s.c_str() + s.size();
    };
    const auto x = split(a), y = split(b);
    std::size_t offset = 0;
    for (std::size_t i = 0; i < std::max(x.size(), y.size()); ++i) {
        if (i >= x.size() || i >= y.size()) { if (where != nullptr) *where = offset; return false; }
        double u = 0, v = 0;
        const bool same = x[i] == y[i]
                          || (i % 2 == 1 && number(x[i], u) && number(y[i], v) && std::abs(u - v) <= 1.0e-5 * std::max(1.0, std::abs(u)));
        if (!same) {
            std::size_t k = 0;
            while (k < x[i].size() && k < y[i].size() && x[i][k] == y[i][k]) ++k;
            if (where != nullptr) *where = offset + k;
            return false;
        }
        offset += x[i].size() + 1;
    }
    return true;
}

int write(const juce::String& path) {
    Host h;
    make(h);
    juce::MemoryBlock state;
    h.p->getStateInformation(state);
    const juce::File file = juce::File::getCurrentWorkingDirectory().getChildFile(path);
    if (!file.replaceWithData(state.getData(), state.getSize())) return 1;
    std::printf("wrote %s (%d bytes)\n", file.getFullPathName().toRawUTF8(), int(state.getSize()));
    // What this build saves after loading it (the comparison for later builds).
    Host back;
    back.p->setStateInformation(state.getData(), int(state.getSize()));
    const auto expected = file.getSiblingFile(file.getFileName() + ".expected.xml");
    const auto text = xmlOf(back).toStdString();
    if (!expected.replaceWithData(text.data(), text.size())) return 1;
    std::printf("wrote %s\n", expected.getFullPathName().toRawUTF8());
    return 0;
}

// The rename's checks: the names hosts show, an old session loading unchanged, and the
// user's files copied from the old folders.
void test(Suite& s) {
    s.section("company rename");
    // What hosts list (the AU's name is "<manufacturer>: <name>"), and what stays.
    s.check(juce::String(JucePlugin_Manufacturer) + ": " + JucePlugin_Name == "Windward Audio: WORLDBUILDER", "hosts list it as Windward Audio: WORLDBUILDER",
            juce::String(JucePlugin_Manufacturer).toStdString());
#define WB_RENAME_TEXT2(x) #x
#define WB_RENAME_TEXT(x) WB_RENAME_TEXT2(x)
    const juce::String bundleId(WB_RENAME_TEXT(JucePlugin_CFBundleIdentifier));
    s.check(bundleId == "com.windwardaudio.worldbuilder", "bundle ID com.windwardaudio.worldbuilder", bundleId.toStdString());
    s.check(JucePlugin_ManufacturerCode == 0x5665696c && JucePlugin_PluginCode == 0x57626c64, "the AU codes are unchanged (manufacturer Veil, subtype Wbld)");

    // The old session: loaded, then saved again, it is the same state.
    const auto file = juce::File(WB_SOURCE_DIR).getChildFile("Tests/fixtures/old-session.wbstate");
    juce::MemoryBlock old;
    const bool readable = file.loadFileAsData(old) && old.getSize() > 8;
    s.check(readable, "the old session's file is readable", file.getFullPathName().toStdString());
    if (!readable) return;
    const auto expected = file.getSiblingFile(file.getFileName() + ".expected.xml").loadFileAsString();
    Host b;
    b.p->setStateInformation(old.getData(), int(old.getSize()));
    const auto back = xmlOf(b);
    std::size_t differ = 0;
    s.check(expected.isNotEmpty() && sameState(expected.toStdString(), back.toStdString(), &differ), "an old session loads unchanged (its state compared with the state saved again)",
            "first difference at character " + std::to_string(differ) + ": " + back.substring(std::max(0, int(differ) - 60), int(differ) + 60).toStdString());
    int mismatches = 0, found = 0;
    std::string first;
    if (const auto xml = juce::AudioProcessor::getXmlFromBinary(old.getData(), int(old.getSize())))
        for (const auto* param : xml->getChildWithTagNameIterator("PARAM")) {
            const auto id = param->getStringAttribute("id");
            if (b.p->parameters.getParameter(id) == nullptr) { if (mismatches++ == 0) first = id.toStdString() + " (no such parameter)"; continue; }
            ++found;
            if (const float saved = float(param->getDoubleAttribute("value")); std::abs(b.get(id.toRawUTF8()) - saved) > 1.0e-5f * std::max(1.0f, std::abs(saved)) && mismatches++ == 0) first = id.toStdString();
        }
    s.check(found > 0 && found == int(specs.size()) && mismatches == 0, "every parameter ID of the old session exists and has its saved value",
            std::to_string(found) + " of " + std::to_string(specs.size()) + ", " + std::to_string(mismatches) + " differ " + first);
    // The same session made with this build saves the same state (the format did not change).
    Host now;
    make(now);
    s.check(sameState(xmlOf(now).toStdString(), xmlOf(old).toStdString()), "the same session made with this build saves the same state as the build before the rename");
    s.check(measure(b.render(2.0), 0.5).rmsDb > -60.0, "the old session plays");

    // The user's files: copied once from the old folders, the originals kept.
    const auto root = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("wb-rename-test");
    root.deleteRecursively();
    const userdata::Roots roots { root.getChildFile("Library"), root.getChildFile("Documents") };
    const auto oldWorld = roots.documents.getChildFile("WORLDBUILDER/Worlds/Harbour.world");
    const auto oldPreset = roots.documents.getChildFile("WORLDBUILDER/Instrument Presets/Sub/Warm Horns.wbinst");
    const auto oldHost = roots.appData.getChildFile("Audio/Presets/Veil Audio/WORLDBUILDER/Storm.aupreset");
    for (const auto& f : { oldWorld, oldPreset, oldHost }) {
        f.getParentDirectory().createDirectory();
        f.replaceWithText("old " + f.getFileName());
    }
    // A file the user already has in the new folder is never overwritten.
    const auto existing = userdata::worlds(roots).getChildFile("Harbour.world");
    existing.getParentDirectory().createDirectory();
    existing.replaceWithText("newer");
    const auto first1 = userdata::migrate(roots);
    const auto folder = roots.appData.getChildFile("Windward Audio/WORLDBUILDER");
    s.check(userdata::folder(roots) == folder, "the user's files live in ~/Library/Windward Audio/WORLDBUILDER", userdata::folder(roots).getFullPathName().toStdString());
    s.check(first1.ran && first1.copied == 2, "the first launch copies the old folders' files (two new, one already there)", std::to_string(first1.copied) + " copied");
    s.check(folder.getChildFile("Instrument Presets/Sub/Warm Horns.wbinst").loadFileAsString() == "old Warm Horns.wbinst"
            && folder.getChildFile("Host Presets/Storm.aupreset").loadFileAsString() == "old Storm.aupreset"
            && roots.appData.getChildFile("Audio/Presets/Windward Audio/WORLDBUILDER/Storm.aupreset").existsAsFile(),
            "instrument presets and host presets arrive (the host's too under the new company name)");
    s.check(existing.loadFileAsString() == "newer", "a file already in the new folder is kept");
    s.check(oldWorld.existsAsFile() && oldPreset.existsAsFile() && oldHost.existsAsFile(), "the originals are kept");
    oldPreset.getSiblingFile("Later.wbinst").replaceWithText("later");
    const auto second = userdata::migrate(roots);
    s.check(!second.ran && !folder.getChildFile("Instrument Presets/Sub/Later.wbinst").exists(), "the copy runs once");
    s.check(userdata::worlds(roots) == folder.getChildFile("Worlds") && userdata::instrumentPresets(roots) == folder.getChildFile("Instrument Presets"),
            "SAVE .world and the instrument presets use the new folder");
    root.deleteRecursively();
}
}   // namespace oldSession
