// The Drama tier (after the performance pass): Calm -> Tension -> Action -> Climax on every
// Place, the shared harmony's gears, the arrangement's arcs, Inspire's moods, the dramatic
// factory scenes; old sessions and scenes unchanged; listening renders.
// (Included from WorldbuilderTests.cpp after the recordings' tests: it uses their helpers.)

namespace dramaTest {

// Transient energy per second: the positive spectral flux (log magnitudes of 1024-point
// frames every 256 samples, the signal first brought to unit RMS so loudness alone does
// not count), summed per second: what drums, ostinatos and stabs add and a held bed does
// not.
double onsetRate(const Audio& a, double from, double to = 1.0e9) {
    const auto begin = std::size_t(from * a.rate), end = std::min(a.l.size(), std::size_t(to * a.rate));
    constexpr std::size_t n = 1024, hop = 256;
    if (end <= begin + 4 * n) return 0;
    double power = 0;
    for (std::size_t i = begin; i < end; ++i) power += 0.25 * (double(a.l[i]) + double(a.r[i])) * (double(a.l[i]) + double(a.r[i]));
    const double norm = 1.0 / std::sqrt(power / double(end - begin) + 1.0e-18);
    std::vector<double> window(n), last(n / 2 + 1, 0.0);
    for (std::size_t i = 0; i < n; ++i) window[i] = 0.5 - 0.5 * std::cos(2.0 * M_PI * double(i) / double(n - 1));
    std::vector<std::complex<double>> frame(n);
    double flux = 0;
    bool first = true;
    for (std::size_t at = begin; at + n <= end; at += hop) {
        for (std::size_t i = 0; i < n; ++i) frame[i] = 0.5 * (double(a.l[at + i]) + double(a.r[at + i])) * norm * window[i];
        fft(frame);
        for (std::size_t k = 0; k <= n / 2; ++k) {
            const double v = std::log1p(10.0 * std::abs(frame[k]));
            if (!first) flux += std::max(0.0, v - last[k]);
            last[k] = v;
        }
        first = false;
    }
    return flux / (double(end - begin) / a.rate) / 1000.0;
}

// Energy at the minor and the major third above the key's root (four octaves): which the
// harmony leans to.
std::pair<double, double> thirds(const Audio& a, int root, double from) {
    const auto begin = std::size_t(from * a.rate);
    const auto goertzel = [&](double hz) {
        const double c = 2.0 * std::cos(2.0 * M_PI * hz / a.rate);
        double s1 = 0, s2 = 0;
        for (std::size_t i = begin; i < a.l.size(); ++i) {
            const double s0 = 0.5 * (double(a.l[i]) + double(a.r[i])) + c * s1 - s2;
            s2 = s1;
            s1 = s0;
        }
        return s1 * s1 + s2 * s2 - c * s1 * s2;
    };
    double minor = 0, major = 0;
    for (double octave : { 0.5, 1.0, 2.0, 4.0 }) {
        minor += goertzel(double(music::rootHz(root)) * octave * std::exp2(3.0 / 12.0));
        major += goertzel(double(music::rootHz(root)) * octave * std::exp2(4.0 / 12.0));
    }
    return { minor, major };
}

uint32_t dramaNotes(const Host& h) {
    uint32_t n = 0;
    for (int slot = 0; slot < placeSlotCount; ++slot) n += h.p->engine.placeSlot(slot).drama().started();
    return n;
}

struct Feel {
    double rms = -240, low = -240, onsets = 0, events = 0, peak = 0;
    bool finite = true;
    int tier = 0;
    Audio audio;
};

// A Place alone at its defaults (as its card sets it), at an intensity and a Drama amount.
Host placeHost(int type, float intensity, float dramaAmount, int style = drama::StyleAuto) {
    auto h = intensityWorld({ type }, intensity);
    h.set("drama", dramaAmount);
    h.set("dramastyle", float(style));
    return h;
}

Feel feel(Host& h, double seconds = 12.0, double from = 4.0) {
    Feel f;
    const uint32_t before = allEvents(h) + dramaNotes(h);
    f.audio = h.render(seconds);
    const auto st = measure(f.audio, from);
    f.rms = st.rmsDb;
    f.low = st.lowDb;
    f.peak = st.peak;
    f.finite = st.finite;
    f.onsets = onsetRate(f.audio, from);
    f.events = double(allEvents(h) + dramaNotes(h) - before) / seconds;
    f.tier = h.p->engine.placeSlot(0).drama().tier();
    return f;
}

// A FLAC for listening (16-bit stereo, kept at its level unless it would clip) in the
// renders folder.
bool writeFlac(const juce::String& name, const std::vector<float>& l, const std::vector<float>& r, double rate) {
    auto file = recTest::rendersFolder().getChildFile(name);
    file.deleteFile();
    std::unique_ptr<juce::FileOutputStream> out(file.createOutputStream());
    if (out == nullptr) return false;
    juce::FlacAudioFormat flac;
    std::unique_ptr<juce::AudioFormatWriter> w(flac.createWriterFor(out.get(), rate, 2, 16, {}, 0));
    if (w == nullptr) return false;
    out.release();
    juce::AudioBuffer<float> b(2, int(l.size()));
    for (std::size_t i = 0; i < l.size(); ++i) {
        b.setSample(0, int(i), std::clamp(l[i], -1.0f, 1.0f));
        b.setSample(1, int(i), std::clamp(r[i], -1.0f, 1.0f));
    }
    return w->writeFromAudioSampleBuffer(b, 0, b.getNumSamples());
}
void appendPart(std::vector<float>& l, std::vector<float>& r, const Audio& a, double from, double seconds) {
    const auto b = std::size_t(from * a.rate), e = std::min(a.l.size(), b + std::size_t(seconds * a.rate));
    if (b >= e) return;
    l.insert(l.end(), a.l.begin() + long(b), a.l.begin() + long(e));
    r.insert(r.end(), a.r.begin() + long(b), a.r.begin() + long(e));
    l.insert(l.end(), std::size_t(0.6 * a.rate), 0.0f);
    r.insert(r.end(), std::size_t(0.6 * a.rate), 0.0f);
}

Audio renderFile(capture::Scene& scene, const capture::GameFile& file) {
    Audio out;
    out.rate = scene.sampleRate;
    const auto pass = capture::passes(scene, false).front();
    capture::renderGameFile(scene, file, pass, [&](const float* l, const float* r, int n) {
        out.l.insert(out.l.end(), l, l + n);
        out.r.insert(out.r.end(), r, r + n);
        return true;
    });
    return out;
}

std::string fileKey(const std::string& name) {
    std::string out;
    for (char c : name) out += std::isalnum(static_cast<unsigned char>(c)) ? char(std::tolower(static_cast<unsigned char>(c))) : '-';
    return out;
}

} // namespace dramaTest

void testDrama(Suite& s) {
    using namespace dramaTest;
    s.section("drama: the parameters, old sessions and scenes unchanged");
    {
        const auto& d = specs[std::size_t(PDrama)];
        const auto& st = specs[std::size_t(PDramaStyle)];
        s.check(std::string(d.id) == "drama" && d.def == 1.0f && d.min == 0.0f && d.max == 1.0f && std::string(st.id) == "dramastyle" && st.max == 5.0f,
                "Drama (0-100 %, 100 % for a new instance) and Drama Style are host parameters");
        s.check(moveOf(PDrama) == Move::Live && moveOf(PDramaStyle) == Move::Live, "journeys between Locations leave the Drama tier as set");
        // A project saved before the Drama tier (no drama parameters in its state): Classic.
        Host a;
        a.set("p1_type", float(Castle));
        a.set("intensity", 0.95f);
        juce::MemoryBlock block;
        a.p->getStateInformation(block);
        auto xml = juce::AudioProcessor::getXmlFromBinary(block.getData(), int(block.getSize()));
        for (auto* child = xml->getFirstChildElement(); child != nullptr;) {
            auto* next = child->getNextElement();
            const auto id = child->getStringAttribute("id");
            if (id == "drama" || id == "dramastyle") xml->removeChildElement(child, true);
            child = next;
        }
        juce::MemoryBlock old;
        juce::AudioProcessor::copyXmlToBinary(*xml, old);
        Host b, c;
        b.p->setStateInformation(old.getData(), int(old.getSize()));
        c.p->setStateInformation(old.getData(), int(old.getSize()));
        c.set("drama", 0.0f);
        const auto rb = b.render(10.0), rc = c.render(10.0);
        bool same = rb.l.size() == rc.l.size();
        for (std::size_t i = 0; same && i < rb.l.size(); ++i) same = rb.l[i] == rc.l[i] && rb.r[i] == rc.r[i];
        s.check(b.get("drama") == 0.0f && same && dramaNotes(b) == 0 && !b.p->engine.placeSlot(0).drama().active() && b.p->engine.frame().harmony == 0,
                "a project from before it opens on the Classic score: Drama 0, the layer never plays, the harmony as before",
                "drama " + fmt(b.get("drama")) + ", " + std::to_string(dramaNotes(b)) + " drama notes");
        // The factory scenes from before it keep the Classic score and their arrangements.
        int classic = 0, dramatic = 0, tiered = 0;
        for (int i = 0; i < scenes::factoryCount(); ++i) {
            const auto& sc = scenes::factory(i);
            const auto ar = scenes::arrangement(sc);
            bool anyTier = false;
            for (int k = 0; k < ar.count; ++k) anyTier |= ar.sections[std::size_t(k)].drama >= 0;
            if (sc.drama <= 0.0f) {
                ++classic;
                tiered += anyTier ? 1 : 0;
            } else ++dramatic;
        }
        s.check(classic == 24 && tiered == 0, "the 24 factory scenes from before it play the Classic score (Drama 0, no section tiers)",
                std::to_string(classic) + " classic, " + std::to_string(tiered) + " with tiers");
        s.check(dramatic >= 15, "at least 15 dramatic factory scenes", std::to_string(dramatic));
        // At or below the middle of Score Intensity nothing changes at all, Drama or not.
        bool untouched = true;
        for (int type : { Castle, Spaceport, Forest, Factory, Ruins }) {
            auto x = placeHost(type, 0.5f, 1.0f), y = placeHost(type, 0.5f, 0.0f);
            const auto ax = x.render(6.0), ay = y.render(6.0);
            for (std::size_t i = 0; untouched && i < ax.l.size(); ++i) untouched = ax.l[i] == ay.l[i] && ax.r[i] == ay.r[i];
            untouched = untouched && dramaNotes(x) == 0 && !x.p->engine.placeSlot(0).drama().active();
        }
        s.check(untouched, "at 50 % Score Intensity a dramatic world is bit for bit the Classic one, and the layer costs nothing");
        auto z = placeHost(Castle, 1.0f, 0.0f);
        z.render(8.0);
        s.check(dramaNotes(z) == 0 && z.p->engine.frame().harmony == 0, "Drama 0 at 100 %: no layer, the Classic harmony");
    }

    s.section("drama: the harmony changes gear");
    {
        bool classic = true, action = true, climax = true, tension = true, major = true;
        for (double b = 0; b < 256; b += 1.0)
            for (float danger : { 0.2f, 0.8f }) classic = classic && music::chordDegree(b, danger, 0, 0) == music::chordDegree(b, danger);
        std::set<int> minorAction, minorClimax, majorAction;
        for (double b = 0; b < 64; b += 4.0) {
            minorAction.insert(music::chordDegree(b, 0.2f, 2, 0));
            minorClimax.insert(music::chordDegree(b, 0.2f, 3, 0));
            majorAction.insert(music::chordDegree(b, 0.2f, 2, 1));
            action = action && music::chordDegree(b, 0.2f, 2, 0) == music::chordDegree(std::floor(b / 8.0) * 8.0, 0.2f, 2, 0);
            tension = tension && (music::chordDegree(b, 0.2f, 1, 0) == 0 || music::chordDegree(b, 0.2f, 1, 0) == 5);
        }
        action = action && minorAction == std::set<int> { 0, 5, 6 };
        climax = minorClimax.size() == 4;
        major = majorAction == std::set<int> { 0, 3, 4 };
        s.check(classic, "the Classic harmony is exactly the shared clock as before");
        s.check(tension && action && climax && major,
                "Tension holds a tonic pedal, Action moves twice as fast rising into the tonic (i VI VII / I IV V), Climax steps wider");
        // The world's tier for the harmony is latched on the four-bar lines.
        auto h = placeHost(Castle, 0.5f, 1.0f);
        h.render(1.0);
        h.set("intensity", 1.0f);
        bool onLines = true;
        int last = h.p->engine.frame().harmony, changes = 0;
        for (int k = 0; k < 2600; ++k) {
            h.processOne(256);
            const auto& f = h.p->engine.frame();
            if (f.harmony != last) {
                ++changes;
                onLines = onLines && std::fmod(f.beat, 16.0) < 0.2;
                last = f.harmony;
            }
        }
        s.check(changes >= 1 && onLines && last == drama::Climax, "the harmony changes gear on the four-bar lines only",
                std::to_string(changes) + " changes, now " + std::to_string(last));
    }

    s.section("drama: every Place, Calm -> Tension -> Action -> Climax");
    std::vector<float> l, r;
    std::vector<double> climaxes;
    const auto places = builtPlaces();
    for (int type : places) {
        const std::string name = placeNames[std::size_t(type)];
        auto hc = placeHost(type, 0.35f, 1.0f), ht = placeHost(type, 0.62f, 1.0f), ha = placeHost(type, 0.8f, 1.0f), hx = placeHost(type, 1.0f, 1.0f),
             hk = placeHost(type, 1.0f, 0.0f);
        const auto calm = feel(hc), tense = feel(ht), action = feel(ha), climax = feel(hx), classic = feel(hk);
        climaxes.push_back(climax.rms);
        const std::string text = "calm " + fmt(calm.rms) + " dB " + fmt(calm.onsets) + " transients low " + fmt(calm.low) + "; tension " + fmt(tense.rms) + "; action "
                                 + fmt(action.rms) + " " + fmt(action.onsets) + "/s; climax " + fmt(climax.rms) + " dB " + fmt(climax.onsets) + "/s low "
                                 + fmt(climax.low) + " notes " + fmt(climax.events) + "/s (calm " + fmt(calm.events) + "); today's maximum " + fmt(classic.rms)
                                 + " dB " + fmt(classic.onsets) + "/s low " + fmt(classic.low) + " notes " + fmt(classic.events) + "/s";
        s.check(calm.tier == drama::Calm && tense.tier == drama::Tension && action.tier == drama::Action && climax.tier == drama::Climax,
                name + ": the tiers in order (35 %, 62 %, 80 %, 100 %)",
                std::to_string(calm.tier) + std::to_string(tense.tier) + std::to_string(action.tier) + std::to_string(climax.tier));
        // The Drama loudness pass: the tiers are about equally loud (the "loudness" group
        // measures them); intensity comes from the notes, the transients and the parts.
        s.check(climax.onsets > 1.4 * calm.onsets && climax.events > 2.5 * calm.events,
                name + ": Calm against Climax differ strongly (transients, notes per second), not in loudness", text);
        s.check(tense.events > calm.events && action.events > tense.events && climax.events > action.events, name + ": busier at every tier", text);
        // More intense than today's maximum: at least as transient (nearly: some Places'
        // Classic maximum is already a snare or shaker groove) and with half as many notes
        // again or more, without being louder.
        s.check(climax.onsets >= 0.9 * classic.onsets && climax.events > 1.5 * classic.events && climax.rms < classic.rms + 1.0,
                name + ": the Climax is more intense than today's maximum (Classic at 100 %), not louder", text);
        s.check(climax.finite && climax.peak <= 0.8913 && action.peak <= 0.8913, name + ": finite and never clipping at Climax",
                fmt(toDb(climax.peak)) + " dBFS peak");
        s.metric(name + " Climax over Calm", climax.rms - calm.rms, " dB");
        s.metric(name + " Climax over today's maximum", climax.rms - classic.rms, " dB");
        s.metric(name + " Climax transients", climax.onsets, " /s");
        // The listening render: Calm, Action, Climax (6 s each, as played).
        std::vector<float> pl, pr;
        appendPart(pl, pr, calm.audio, 5.0, 6.0);
        appendPart(pl, pr, action.audio, 5.0, 6.0);
        appendPart(pl, pr, climax.audio, 5.0, 6.0);
        writeFlac("drama-" + juce::String(placeKeys[std::size_t(type)]) + "-calm-action-climax.flac", pl, pr, 48000);
    }
    if (!climaxes.empty()) {
        auto sorted = climaxes;
        std::sort(sorted.begin(), sorted.end());
        const double median = sorted[sorted.size() / 2];
        double worst = 0;
        for (double c : climaxes) worst = std::max(worst, std::abs(c - median));
        // (Each Place's Climax now sits at its own Calm level: the Places differ as they do at Calm.)
        s.metric("Climax levels: the median", median, " dB");
        s.metric("Climax levels: the furthest from the median", worst, " dB");
    }

    s.section("drama: gear changes, styles, the arrangement");
    {
        // A tier changes on a bar line (the drums, ostinato and hits change gear together).
        auto h = placeHost(Megacity, 0.5f, 1.0f);
        h.render(1.3);
        h.set("intensity", 1.0f);
        bool bars = true;
        int last = h.p->engine.placeSlot(0).drama().tier(), changes = 0;
        for (int k = 0; k < 2000; ++k) {
            h.processOne(128);
            const int now = h.p->engine.placeSlot(0).drama().tier();
            if (now != last) {
                ++changes;
                bars = bars && std::fmod(h.p->engine.placeSlot(0).beat(), 4.0) < 0.1;
                last = now;
            }
        }
        s.check(changes >= 1 && bars && last == drama::Climax, "a jump in Score Intensity changes gear on the next bar line", std::to_string(changes) + " changes");
        // No jump overshoots the climax's own level.
        auto steady = placeHost(Castle, 1.0f, 1.0f);
        const auto st = steady.render(12.0);
        auto jump = placeHost(Castle, 0.0f, 1.0f);
        jump.render(4.0);
        jump.set("intensity", 1.0f);
        const auto after = jump.render(8.0);
        double top = -240, jumpTop = -240;
        for (double e : envelopeDb(st, 0.2, 4.0)) top = std::max(top, e);
        for (double e : envelopeDb(after, 0.2)) jumpTop = std::max(jumpTop, e);
        s.check(jumpTop < top + 1.5, "a jump to the Climax never overshoots the Climax's own level", fmt(jumpTop) + " vs " + fmt(top) + " dB");
    }
    {
        // The styles feel different on one Place.
        std::array<Feel, drama::StyleCount> by {};
        std::string text;
        for (int style = drama::StyleDriving; style < drama::StyleCount; ++style) {
            auto h = placeHost(Castle, 1.0f, 1.0f, style);
            by[std::size_t(style)] = feel(h, 10.0, 3.0);
            text += std::string(juce::StringArray::fromTokens(drama::styleChoices, "|", "")[style].toRawUTF8()) + " " + fmt(by[std::size_t(style)].rms) + " dB "
                    + fmt(by[std::size_t(style)].events) + " notes/s; ";
        }
        const auto& drive = by[drama::StyleDriving];
        s.check(by[drama::StyleEpic].events < drive.events && by[drama::StyleTragic].events < 0.75 * drive.events && by[drama::StyleSuspense].events < drive.events,
                "Drama Style: Driving drives hardest; Epic is broader, Tragic sparse, Suspense held back", text);
    }
    {
        // A dramatic arrangement: arcs on the tiers, the transitions a score changes gear with.
        int withArcs = 0, dramaticScenes = 0;
        for (int i = 0; i < scenes::factoryCount(); ++i) {
            const auto& sc = scenes::factory(i);
            if (sc.drama <= 0.0f) continue;
            ++dramaticScenes;
            const auto a = scenes::arrangement(sc);
            bool calm = false, top = false, riser = false, drop = false;
            for (int k = 0; k < a.count; ++k) {
                const auto& sec = a.sections[std::size_t(k)];
                calm |= sec.drama == drama::Calm;
                top |= sec.drama >= drama::Action;
                riser |= sec.transition == arr::TransRiser || sec.transition == arr::TransDrumFill;
                drop |= sec.transition == arr::TransDrop || sec.transition == arr::TransFilter;
            }
            withArcs += calm && top && riser && drop ? 1 : 0;
        }
        s.check(withArcs == dramaticScenes, "every dramatic scene's arrangement climbs and falls on the tiers (Calm and Action or Climax; a Riser or Drum fill in, a Drop out)",
                std::to_string(withArcs) + " of " + std::to_string(dramaticScenes));
        // Played: the climaxes louder than the calm sections, the drop silent.
        Host h;
        h.p->loadScene(scenes::factory(scenes::factoryCount() - 17));   // Battle at the Castle Gate
        const auto a = h.p->arrangement.get();
        arrangeTest::play(h);
        const double bpm = h.playHead.bpm, secondsPerBar = 240.0 / bpm;
        // (The Drama loudness pass: the fights are as loud as the calm sections, denser: more
        // notes per second. Played section by section to count them.)
        Audio audio;
        audio.rate = 48000;
        std::vector<double> notesPerSecond;
        for (int k = 0; k < a.count; ++k) {
            const double seconds = double(a.sections[std::size_t(k)].span()) * secondsPerBar;
            const uint32_t before = allEvents(h) + dramaNotes(h);
            const auto part = h.render(seconds);
            notesPerSecond.push_back(double(allEvents(h) + dramaNotes(h) - before) / seconds);
            audio.l.insert(audio.l.end(), part.l.begin(), part.l.end());
            audio.r.insert(audio.r.end(), part.r.begin(), part.r.end());
        }
        double calmTop = -240, peakLow = 240, calmOnsets = 0, fightOnsets = 1.0e9;
        for (int k = 0; k < a.count; ++k) {
            const auto& sec = a.sections[std::size_t(k)];
            const double from = double(a.startOf(k)) * secondsPerBar + secondsPerBar, to = double(a.startOf(k) + sec.span()) * secondsPerBar - secondsPerBar;
            if (to <= from || sec.transition == arr::TransFilter) continue;
            const double level = measure(audio, from, to).rmsDb, onsets = notesPerSecond[std::size_t(k)];
            if (sec.drama == drama::Calm) {
                calmTop = std::max(calmTop, level);
                calmOnsets = std::max(calmOnsets, onsets);
            }
            if (sec.drama >= drama::Action) {
                peakLow = std::min(peakLow, level);
                fightOnsets = std::min(fightOnsets, onsets);
            }
        }
        const auto whole = measure(audio, 0.5);
        s.check(fightOnsets > 1.5 * calmOnsets && peakLow < calmTop + 2.0 && whole.finite && whole.peak <= 0.8913,
                "played, its fights are denser than its calm sections (notes), not louder, and never clip",
                "quietest fight " + fmt(peakLow) + " dB, loudest calm " + fmt(calmTop) + " dB; notes " + fmt(fightOnsets) + " / " + fmt(calmOnsets) + " per s");
        std::vector<float> pl, pr;
        appendPart(pl, pr, audio, 0.0, std::min(90.0, double(audio.l.size()) / 48000.0));
        writeFlac("drama-scene-battle-at-the-castle-gate.flac", pl, pr, 48000);
    }
    {
        // A loop at Tension: its end joins its start without a click.
        Host h;
        arr::Arrangement a;
        a.sections[0] = arr::makeSection("Intro", 2, 0, 0.3f);
        a.sections[1] = arr::makeSection("Loop", 2, 1, 0.7f);
        a.sections[1].repeats = 2;
        a.sections[2] = arr::makeSection("Outro", 2, 2, 0.2f);
        a.count = 3;
        h.p->arrangement.set(a);
        h.render(0.05);
        auto scene = h.p->captureScene();
        const auto files = capture::gameFiles(scene, false);
        const auto loop = renderFile(scene, files[1]);
        const std::size_t n = loop.l.size();
        double inside = 0;
        for (std::size_t i = 1; i < n; ++i) inside = std::max(inside, double(std::max(std::abs(loop.l[i] - loop.l[i - 1]), std::abs(loop.r[i] - loop.r[i - 1]))));
        const double seam = std::max(std::abs(loop.l[0] - loop.l[n - 1]), std::abs(loop.r[0] - loop.r[n - 1]));
        s.check(n > 0 && seam <= inside, "a loop with the Drama tier's downbeats joins without a click", fmt(toDb(seam)) + " dB at the seam, " + fmt(toDb(inside)) + " inside");
    }

    s.section("drama: Inspire's moods");
    {
        std::array<int, scenes::InspireMoodCount> counts {};
        for (uint32_t seed = 1; seed <= 400; ++seed) ++counts[std::size_t(scenes::moodFor(seed))];
        const int dramaticMoods = counts[scenes::MoodTension] + counts[scenes::MoodBattle] + counts[scenes::MoodEpic] + counts[scenes::MoodTragic]
                                  + counts[scenes::MoodTriumphant];
        bool every = true;
        for (int m = 1; m < scenes::InspireMoodCount; ++m) every = every && counts[std::size_t(m)] >= 20;
        s.check(dramaticMoods >= 200 && every && counts[scenes::MoodCalm] < 100, "Any picks the dramatic moods regularly (at least half), every mood often",
                std::to_string(dramaticMoods) + " of 400 dramatic, calm " + std::to_string(counts[scenes::MoodCalm]));
        bool recipes = true;
        std::string wrong;
        for (int m = 1; m < scenes::InspireMoodCount; ++m)
            for (uint32_t seed = 1; seed <= 40; ++seed) {
                const auto sc = scenes::inspire(seed, m);
                const bool minorKey = sc.mood == 0 || sc.mood == 2 || sc.mood == 3;
                bool ok = sc.drama == 1.0f && sc.inspired == m && sc.slots[0].type != NoPlace;
                switch (m) {
                    case scenes::MoodBattle: ok = ok && sc.intensity >= 0.85f && minorKey && sc.dramaStyle == drama::StyleDriving && sc.arcs; break;
                    case scenes::MoodEpic: ok = ok && sc.intensity >= 0.85f && sc.dramaStyle == drama::StyleEpic; break;
                    case scenes::MoodTragic: ok = ok && sc.mood == 0 && sc.dramaStyle == drama::StyleTragic; break;
                    case scenes::MoodTriumphant: ok = ok && music::majorMood(sc.mood) && sc.dramaStyle == drama::StyleTriumphant; break;
                    case scenes::MoodCalm: ok = ok && sc.intensity <= 0.45f && !sc.arcs; break;
                    case scenes::MoodTension: ok = ok && sc.intensity >= 0.6f && sc.intensity < 0.7f; break;
                    default: break;
                }
                if (!ok) wrong += std::string(scenes::inspireMoodNames[std::size_t(m)]) + "/" + std::to_string(seed) + " ";
                recipes = recipes && ok;
            }
        s.check(recipes, "each mood's scene asks for its mood (Battle: Action or Climax, minor, driving; Epic: big; Tragic: minor; Triumphant: major...)", wrong);
    }
    {
        // Measured: three scenes per mood, as they play (Free: the world as set).
        struct Measured {
            double rms = 0, onsets = 0, low = 0, events = 0, minorShare = 0;
            int tierSum = 0;
        };
        std::array<Measured, scenes::InspireMoodCount> by {};
        std::string text;
        for (int m = 1; m < scenes::InspireMoodCount; ++m) {
            for (uint32_t seed : { 11u, 222u, 3333u }) {
                const auto sc = scenes::inspire(seed, m);
                Host h;
                h.p->loadScene(sc);
                const auto f = feel(h, 12.0, 4.0);
                auto& x = by[std::size_t(m)];
                x.rms += f.rms / 3.0;
                x.onsets += f.onsets / 3.0;
                x.low += f.low / 3.0;
                x.events += f.events / 3.0;
                const auto [minor, major] = thirds(f.audio, sc.root, 4.0);
                x.minorShare += minor / (minor + major + 1.0e-12) / 3.0;
                x.tierSum += f.tier;
                if (seed == 11u) {
                    std::vector<float> pl, pr;
                    appendPart(pl, pr, f.audio, 2.0, 10.0);
                    writeFlac("drama-inspire-" + juce::String(fileKey(scenes::inspireMoodNames[std::size_t(m)])) + ".flac", pl, pr, 48000);
                }
            }
            const auto& x = by[std::size_t(m)];
            text += std::string(scenes::inspireMoodNames[std::size_t(m)]) + " " + fmt(x.rms) + " dB " + fmt(x.onsets) + "/s " + fmt(x.events) + " notes/s low "
                    + fmt(x.low) + " minor " + fmt(x.minorShare) + " tiers " + std::to_string(x.tierSum) + "; ";
        }
        for (int m = 1; m < scenes::InspireMoodCount; ++m) {
            const auto& x = by[std::size_t(m)];
            const std::string mood = scenes::inspireMoodNames[std::size_t(m)];
            s.metric("Inspire " + mood + ": level", x.rms, " dBFS");
            s.metric("Inspire " + mood + ": transients", x.onsets, " /s");
            s.metric("Inspire " + mood + ": notes", x.events, " /s");
            s.metric("Inspire " + mood + ": low end", x.low, " dB");
            s.metric("Inspire " + mood + ": minor-third share", x.minorShare);
            s.metric("Inspire " + mood + ": tiers (three scenes)", double(x.tierSum));
        }
        const auto& calm = by[scenes::MoodCalm];
        const auto& battle = by[scenes::MoodBattle];
        const auto& epic = by[scenes::MoodEpic];
        const auto& tragic = by[scenes::MoodTragic];
        const auto& triumph = by[scenes::MoodTriumphant];
        const auto& tension = by[scenes::MoodTension];
        const auto& mystery = by[scenes::MoodMystery];
        const auto& adventure = by[scenes::MoodAdventure];
        s.check(battle.events > 3.0 * calm.events && battle.onsets > calm.onsets && battle.tierSum >= 6,
                "Battle: Action or Climax, driving (dense, transient against Calm; the loudness group keeps it as loud)", text);
        s.check(epic.events > 1.5 * calm.events && epic.tierSum >= 6 && epic.events < battle.events, "Epic: big (dense, at Climax) but broader than Battle", text);
        s.check(tragic.events < 0.8 * battle.events && tragic.minorShare > 0.5, "Tragic: slower, sparser, minor", text);
        s.check(triumph.events > 1.5 * calm.events && triumph.tierSum >= 6 && triumph.minorShare < battle.minorShare, "Triumphant: big (dense, at Climax) and major", text);
        s.check(tension.tierSum >= 3 && tension.tierSum <= 4 && tension.events > calm.events,
                "Tension: the Tension tier (in two scenes of three at least), above Calm", text);
        s.check(calm.tierSum == 0 && calm.events < 0.5 * battle.events && mystery.tierSum == 0 && mystery.events < 0.5 * battle.events,
                "Calm and Mystery stay below the Drama tier", text);
        s.check(adventure.events > calm.events, "Adventure: livelier than Calm", text);
        s.check(battle.minorShare > 0.5, "Battle is minor", text);
    }
    {
        // Played through its arrangement, an inspired Battle stays in the loudness window.
        Host h;
        h.p->setInspireMood(scenes::MoodBattle);
        h.p->inspire();
        arrangeTest::play(h);
        const auto st = measure(h.render(16.0), 2.0);
        s.check(st.finite && st.rmsDb > -40.0 && st.rmsDb < -6.0 && st.peak <= 0.8913 && h.p->inspireMood() == scenes::MoodBattle,
                "INSPIRE with Battle plays in Arrangement mode within the loudness window", fmt(st.rmsDb) + " dB");
    }

    s.section("drama: CPU only while it plays");
    {
        const auto cost = [](float intensity, float amount) {
            auto h = placeHost(Castle, intensity, amount);
            h.render(2.0);
            const auto t0 = juce::Time::getMillisecondCounterHiRes();
            h.render(10.0);
            return (juce::Time::getMillisecondCounterHiRes() - t0) / 10000.0 * 100.0;
        };
        const double classic = cost(1.0f, 0.0f), climax = cost(1.0f, 1.0f), calm = cost(0.5f, 1.0f), calmClassic = cost(0.5f, 0.0f);
        s.metric("CPU, the Castle at 100 % on the Classic score (report only)", classic, " %");
        s.metric("CPU, the Castle at Climax (report only)", climax, " %");
        s.metric("CPU, the Castle at 50 %, Drama on / Classic (report only)", calm, " %");
        s.metric("CPU, the Castle at 50 %, Classic (report only)", calmClassic, " %");
    }
}
