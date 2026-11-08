#pragma once
// Where WORLDBUILDER keeps the user's own files (the company rename): saved worlds,
// instrument presets and a copy of the host's presets, all in
//   ~/Library/Windward Audio/WORLDBUILDER/  (macOS; elsewhere the user's application
//                                            data folder, e.g. ~/.config on Linux)
// On the first launch after the rename, everything in the folders older versions used is
// copied there (never moved: the old files stay as they were):
//   ~/Documents/WORLDBUILDER/Worlds              -> Worlds
//   ~/Documents/WORLDBUILDER/Instrument Presets  -> Instrument Presets
//   ~/Library/Audio/Presets/Veil Audio/WORLDBUILDER (the host's .aupreset files, saved
//       under the old company name)              -> Host Presets, and into the host's
//       folder for the new name (~/Library/Audio/Presets/Windward Audio/WORLDBUILDER) so
//       the host's preset menu still lists them.
// A file that already exists at the destination is never overwritten. A marker file
// (.migrated) records that the copy ran.
#include <juce_core/juce_core.h>
#include <mutex>

namespace userdata {
inline constexpr const char* company = "Windward Audio";
inline constexpr const char* product = "WORLDBUILDER";
// The company name before the rename: only to find the old folders.
inline constexpr const char* oldCompany = "Veil Audio";

// The folders the paths start from (the tests use a temporary folder).
struct Roots {
    juce::File appData;     // ~/Library on macOS (JUCE's userApplicationDataDirectory)
    juce::File documents;   // ~/Documents
    static Roots system() {
        return { juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory),
                 juce::File::getSpecialLocation(juce::File::userDocumentsDirectory) };
    }
};

inline juce::File folder(const Roots& r = Roots::system()) { return r.appData.getChildFile(company).getChildFile(product); }
inline juce::File worlds(const Roots& r = Roots::system()) { return folder(r).getChildFile("Worlds"); }
inline juce::File instrumentPresets(const Roots& r = Roots::system()) { return folder(r).getChildFile("Instrument Presets"); }
inline juce::File hostPresets(const Roots& r = Roots::system()) { return folder(r).getChildFile("Host Presets"); }
inline juce::File marker(const Roots& r = Roots::system()) { return folder(r).getChildFile(".migrated"); }

// Copies every file under `from` to the same place under `to`, keeping files already
// there. Returns how many it copied.
inline int copyTree(const juce::File& from, const juce::File& to) {
    if (!from.isDirectory()) return 0;
    int copied = 0;
    for (const auto& entry : juce::RangedDirectoryIterator(from, true, "*", juce::File::findFiles)) {
        const auto& source = entry.getFile();
        const auto target = to.getChildFile(source.getRelativePathFrom(from));
        if (target.exists()) continue;
        if (target.getParentDirectory().createDirectory() && source.copyFileTo(target)) ++copied;
    }
    return copied;
}

struct Migration {
    bool ran = false;   // false: it had run before (the marker was there)
    int copied = 0;     // files copied
};

// The one-time copy from the old folders (see the top of this file).
inline Migration migrate(const Roots& r = Roots::system()) {
    Migration m;
    if (marker(r).existsAsFile()) return m;
    if (!folder(r).createDirectory()) return m;
    m.ran = true;
    const auto oldDocuments = r.documents.getChildFile(product);
    m.copied += copyTree(oldDocuments.getChildFile("Worlds"), worlds(r));
    m.copied += copyTree(oldDocuments.getChildFile("Instrument Presets"), instrumentPresets(r));
    const auto hostRoot = r.appData.getChildFile("Audio").getChildFile("Presets");
    const auto oldHost = hostRoot.getChildFile(oldCompany).getChildFile(product);
    m.copied += copyTree(oldHost, hostPresets(r));
    copyTree(oldHost, hostRoot.getChildFile(company).getChildFile(product));
    marker(r).replaceWithText("Copied " + juce::String(m.copied) + " files from the folders of older versions on "
                              + juce::Time::getCurrentTime().toISO8601(true) + ". The originals were kept.\n");
    return m;
}

// Once per process (the plug-in's first instance).
inline void migrateOnce() {
    static std::once_flag once;
    std::call_once(once, [] { migrate(); });
}
}   // namespace userdata
