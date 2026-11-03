// WORLDBUILDER performance bench (the performance pass after Milestone 17).
//
// Measures, the same way on any commit that has the processor tests (it reuses their
// helpers): the audio thread's CPU per Place (four busy copies, Standard quality), the
// editor's message-thread CPU and full-frame paint time per view (run under a display:
// xvfb-run on Linux), the plug-in's load time and memory.
//
// Usage: WorldbuilderBench audio [repeats] | editor [seconds] | load | renders <dir>
// Build: cmake -DWORLDBUILDER_BUILD_BENCH=ON (a development aid, never shipped).
#define main worldbuilderTestsMain
#include "../../Tests/WorldbuilderTests.cpp"
#undef main

#include <ctime>
#include <sys/resource.h>
#if defined(__has_include)
#if __has_include(<valgrind/callgrind.h>)
#include <valgrind/callgrind.h>
#define WB_CALLGRIND 1
#endif
#endif
#if defined(__linux__)
#include <linux/perf_event.h>
#include <sys/ioctl.h>
#include <sys/syscall.h>
#endif

namespace bench {

double threadSeconds() {
    timespec t {};
    clock_gettime(CLOCK_THREAD_CPUTIME_ID, &t);
    return double(t.tv_sec) + 1.0e-9 * double(t.tv_nsec);
}

double rssMb() {
    std::ifstream f("/proc/self/status");
    std::string line;
    while (std::getline(f, line))
        if (line.rfind("VmRSS:", 0) == 0) return std::atof(line.c_str() + 6) / 1024.0;
    return 0;
}

// User-mode instructions and cycles of this thread (Linux perf counters; 0 elsewhere):
// much steadier than time on a shared machine.
struct Counter {
    int fd = -1;
    explicit Counter(unsigned long long config) {
#if defined(__linux__)
        perf_event_attr a {};
        a.type = PERF_TYPE_HARDWARE;
        a.size = sizeof(a);
        a.config = config;
        a.disabled = 1;
        a.exclude_kernel = 1;
        a.exclude_hv = 1;
        fd = int(syscall(__NR_perf_event_open, &a, 0, -1, -1, 0));
#endif
    }
    ~Counter() {
        if (fd >= 0) close(fd);
    }
    void start() {
#if defined(__linux__)
        if (fd >= 0) {
            ioctl(fd, PERF_EVENT_IOC_RESET, 0);
            ioctl(fd, PERF_EVENT_IOC_ENABLE, 0);
        }
#endif
    }
    double stop() {
        long long v = 0;
#if defined(__linux__)
        if (fd >= 0) {
            ioctl(fd, PERF_EVENT_IOC_DISABLE, 0);
            if (read(fd, &v, sizeof(v)) != sizeof(v)) v = 0;
        }
#endif
        return double(v);
    }
};

Host busyPlaces(int type) {
    Host st(48000, 256);
    st.set("quality", 1);
    for (int w = 0; w < WorldControlCount; ++w) st.set(worldId(w), 0.7f);
    for (int slot = 0; slot < placeSlots; ++slot) {
        st.set(slotId(slot, PlType), float(type));
        st.set(slotId(slot, PlCharacter), float(slot % 3));
        st.set(slotId(slot, PlSeed), float(1 + slot));
        st.set(slotId(slot, PlActivity), 1.0f);
    }
    // WB_BENCH_INTENSITY: Score Intensity (the Drama tier: 1 plays every Place at Climax).
    if (const char* x = std::getenv("WB_BENCH_INTENSITY")) st.set("intensity", float(std::atof(x)));
    // WB_BENCH_DRAMAMASK: the Drama layer's parts that play (a bit per drama::Layer::PartId).
    if (const char* m = std::getenv("WB_BENCH_DRAMAMASK")) drama::Layer::partMask = uint32_t(std::strtoul(m, nullptr, 0));
    return st;
}

// Audio: each built Place four times at once, busy, Standard quality, 48 kHz / 256: the
// audio thread's CPU time for 10 s of audio (the least of `repeats` runs), % of one core.
int audio(int repeats) {
    samples::Bank::get().load();
    samples::Bank::get().waitUntilLoaded(60000);
    double total = 0, totalInstructions = 0, totalCycles = 0;
#if defined(__linux__)
    Counter instructions(PERF_COUNT_HW_INSTRUCTIONS), cycles(PERF_COUNT_HW_CPU_CYCLES);
#else
    Counter instructions(0), cycles(0);
#endif
    for (int type : builtPlaces()) {
        double best = 1.0e9, bestInstructions = 1.0e18, bestCycles = 1.0e18;
        for (int k = 0; k < repeats; ++k) {
            auto st = busyPlaces(type);
            st.render(1.0);
            const double t0 = threadSeconds();
            instructions.start();
            cycles.start();
            st.render(10.0);
            bestCycles = std::min(bestCycles, cycles.stop());
            bestInstructions = std::min(bestInstructions, instructions.stop());
            best = std::min(best, threadSeconds() - t0);
        }
        total += 10.0 * best;
        totalInstructions += bestInstructions;
        totalCycles += bestCycles;
        if (bestInstructions > 0.0)
            std::printf("audio %-26s %6.2f %%   %8.0f M instructions   %8.0f M cycles\n", placeKeys[std::size_t(type)], 10.0 * best, bestInstructions / 1.0e6,
                        bestCycles / 1.0e6);
        else
            std::printf("audio %-26s %6.2f %%\n", placeKeys[std::size_t(type)], 10.0 * best);
        std::fflush(stdout);
    }
    std::printf("audio %-26s %6.2f %%\n", "SUM", total);
    return 0;
}

// Exact instruction counts (callgrind, which this function bounds:
//   valgrind --tool=callgrind --instr-atstart=no --toggle-collect='bench::measured*' WorldbuilderBench count --place temple):
// 2 s of the same busy Places after a second's warm-up. Deterministic, unlike time.
// (With valgrind's headers, run with --instr-atstart=no: everything else, the bank's
// decoding above all, runs uninstrumented and many times faster.)
[[gnu::noinline]] void measured(Host& h, double seconds) {
#if WB_CALLGRIND
    CALLGRIND_START_INSTRUMENTATION;
#endif
    h.render(seconds);
#if WB_CALLGRIND
    CALLGRIND_STOP_INSTRUMENTATION;
#endif
}
int count() {
    // Under valgrind the bank takes minutes to decode: wait for it, however long (the
    // recordings must play, not the synthesised fallbacks).
    samples::Bank::get().load();
    while (samples::Bank::get().state() == samples::Bank::Loading) samples::Bank::get().waitUntilLoaded(1000);
    std::fprintf(stderr, "bank: %s\n", samples::Bank::get().message().toRawUTF8());
    if (!samples::Bank::get().ready()) return 1;
    for (int type : builtPlaces()) {
        auto st = busyPlaces(type);
        st.render(1.0);
        measured(st, 2.0);
    }
    return 0;
}

// Load: constructing the first instance and preparing it (prepareToPlay waits for the
// bank), the bank's own load time, memory.
int load() {
    const double rss0 = rssMb();
    const auto t0 = juce::Time::getMillisecondCounterHiRes();
    auto p = std::make_unique<WorldbuilderProcessor>();
    const auto t1 = juce::Time::getMillisecondCounterHiRes();
    p->setPlayConfigDetails(0, 2, 48000, 512);
    p->prepareToPlay(48000, 512);
    const auto t2 = juce::Time::getMillisecondCounterHiRes();
    samples::Bank::get().waitUntilLoaded(60000);
    const auto t3 = juce::Time::getMillisecondCounterHiRes();
    std::printf("load constructor %.1f ms\n", t1 - t0);
    std::printf("load prepareToPlay %.1f ms\n", t2 - t1);
    std::printf("load until bank ready %.1f ms\n", t3 - t0);
    std::printf("load bank %s\n", samples::Bank::get().message().toRawUTF8());
    {
        // A checksum of everything loaded (the notes' audio and mapping), to compare loaders.
        uint64_t hash = 1469598103934665603ull;
        const auto mixIn = [&hash](uint64_t v) { hash = (hash ^ v) * 1099511628211ull; };
        const auto index = juce::JSON::parse(samples::Bank::get().folder().getChildFile("index.json"));
        if (const auto* list = index["instruments"].getArray())
            for (const auto& id : *list)
                if (const auto* inst = samples::Bank::get().instrument(id.toString().toRawUTF8()))
                    for (const auto& z : inst->zones) {
                        for (int i = -2; i < z.length + 4; ++i) mixIn(uint64_t(uint16_t(z.data[i])));
                        mixIn(uint64_t(z.loopStart) ^ (uint64_t(z.loopEnd) << 20) ^ (uint64_t(z.release) << 40));
                        mixIn(uint64_t(std::llround(z.root * 1000.0f)) ^ (uint64_t(z.layer) << 32) ^ (uint64_t(z.rr) << 40));
                    }
        std::printf("load bank checksum %016llx\n", (unsigned long long) hash);
    }
    std::printf("memory RSS after one instance %.1f MB (from %.1f)\n", rssMb(), rss0);
    std::vector<std::unique_ptr<WorldbuilderProcessor>> more;
    const auto t4 = juce::Time::getMillisecondCounterHiRes();
    for (int i = 0; i < 3; ++i) {
        more.push_back(std::make_unique<WorldbuilderProcessor>());
        more.back()->setPlayConfigDetails(0, 2, 48000, 512);
        more.back()->prepareToPlay(48000, 512);
    }
    std::printf("load three more instances %.1f ms\n", juce::Time::getMillisecondCounterHiRes() - t4);
    std::printf("memory RSS after four instances %.1f MB\n", rssMb());
    // A Place change and a preset change never reload anything.
    juce::AudioBuffer<float> b(2, 512);
    juce::MidiBuffer m;
    const auto t5 = juce::Time::getMillisecondCounterHiRes();
    for (int type = NoPlace + 1; type < PlaceTypeCount; ++type) {
        if (!placeImplemented[std::size_t(type)]) continue;
        auto* param = p->parameters.getParameter(slotId(0, PlType));
        param->setValueNotifyingHost(param->convertTo0to1(float(type)));
        for (int k = 0; k < 4; ++k) {
            b.clear();
            p->processBlock(b, m);
        }
    }
    std::printf("load every Place in slot 1 (4 blocks each) %.1f ms\n", juce::Time::getMillisecondCounterHiRes() - t5);
    return 0;
}

// Editor: on screen (a display is needed), audio running on its own thread in real time,
// each view for `seconds`: the message thread's CPU (timers, layout and painting) and a
// full repaint's time.
struct AudioThread final : juce::Thread {
    WorldbuilderProcessor& p;
    explicit AudioThread(WorldbuilderProcessor& proc) : juce::Thread("bench audio"), p(proc) {}
    std::atomic<int> notes { 0 };
    void run() override {
        juce::AudioBuffer<float> b(2, 512);
        juce::MidiBuffer m;
        auto next = juce::Time::getMillisecondCounterHiRes();
        int block = 0;
        while (!threadShouldExit()) {
            b.clear();
            m.clear();
            // A chord every second (the keyboards light, the meters move).
            if (notes.load() && block % 94 == 0)
                for (int k = 0; k < 3; ++k) m.addEvent(juce::MidiMessage::noteOn(1, 48 + 4 * k + (block / 94) % 3, uint8_t(100)), 0);
            if (notes.load() && block % 94 == 60)
                for (int n = 40; n < 80; ++n) m.addEvent(juce::MidiMessage::noteOff(1, n), 0);
            p.processBlock(b, m);
            ++block;
            next += 512.0 / 48.0;
            const auto wait = next - juce::Time::getMillisecondCounterHiRes();
            if (wait > 0) juce::Thread::sleep(int(wait));
        }
    }
};

struct EditorRun final : juce::Timer {
    std::unique_ptr<WorldbuilderProcessor> p;
    std::unique_ptr<juce::AudioProcessorEditor> editor;
    std::unique_ptr<AudioThread> audioThread;
    WorldbuilderEditor* view = nullptr;
    struct Step {
        const char* name;
        std::function<void(EditorRun&)> setup;
    };
    std::vector<Step> steps;
    int step = -1;
    double seconds = 4.0, began = 0, cpu0 = 0;
    void start() {
        p = std::make_unique<WorldbuilderProcessor>();
        p->setPlayConfigDetails(0, 2, 48000, 512);
        auto set = [&](const std::string& id, float v) {
            auto* param = p->parameters.getParameter(id);
            param->setValueNotifyingHost(param->convertTo0to1(v));
        };
        set("playmode", float(PlayFree));
        set(slotId(0, PlType), float(Castle));
        set(slotId(1, PlType), float(Spaceport));
        set(slotId(2, PlType), float(Temple));
        p->prepareToPlay(48000, 512);
        audioThread = std::make_unique<AudioThread>(*p);
        audioThread->startThread(juce::Thread::Priority::highest);
        juce::Thread::sleep(500);
        editor.reset(p->createEditor());
        view = dynamic_cast<WorldbuilderEditor*>(editor.get());
        editor->setOpaque(true);
        editor->addToDesktop(0);
        editor->setVisible(true);
        startTimer(50);
        next();
    }
    void next() {
        if (step >= 0) {
            const double used = threadSeconds() - cpu0, wall = (juce::Time::getMillisecondCounterHiRes() - began) / 1000.0;
            // A full repaint, as when the window is uncovered (the mean of five).
            juce::Image img(juce::Image::RGB, editor->getWidth(), editor->getHeight(), true);
            double full = 0;
            for (int i = 0; i < 5; ++i) {
                const double a = juce::Time::getMillisecondCounterHiRes();
                juce::Graphics g(img);
                editor->paintEntireComponent(g, true);
                full += juce::Time::getMillisecondCounterHiRes() - a;
            }
            std::printf("editor %-34s message thread %6.2f %%   full repaint %6.2f ms\n", steps[std::size_t(step)].name, 100.0 * used / wall, full / 5.0);
            std::fflush(stdout);
        }
        if (++step >= int(steps.size())) {
            stopTimer();
            audioThread->stopThread(2000);
            editor.reset();
            juce::MessageManager::getInstance()->stopDispatchLoop();
            return;
        }
        steps[std::size_t(step)].setup(*this);
        // Let the view settle (layout, caches) before measuring.
        settleUntil = juce::Time::getMillisecondCounterHiRes() + 1000.0;
        measuring = false;
    }
    double settleUntil = 0;
    bool measuring = false;
    void timerCallback() override {
        const double now = juce::Time::getMillisecondCounterHiRes();
        if (!measuring) {
            if (now < settleUntil) return;
            measuring = true;
            began = now;
            cpu0 = threadSeconds();
            return;
        }
        if (now - began >= seconds * 1000.0) next();
    }
};

int editor(double seconds) {
    samples::Bank::get().load();
    samples::Bank::get().waitUntilLoaded(60000);
    EditorRun run;
    run.seconds = seconds;
    run.steps = {
        { "world: map and Place cards", [](EditorRun& r) { r.audioThread->notes = 0; } },
        { "world, notes playing", [](EditorRun& r) { r.audioThread->notes = 1; } },
        { "Place detail", [](EditorRun& r) { r.view->openDetail(0); } },
        { "Pattern Editor and KEYS", [](EditorRun& r) { r.view->closeDetail(); r.view->openPatterns(0); } },
        { "Pattern Editor, transport playing", [](EditorRun& r) { r.p->transportPlay(); } },
        { "ARRANGE", [](EditorRun& r) { r.p->transportStop(); r.view->openArrange(); } },
        { "Instrument mode", [](EditorRun& r) { r.view->closeArrange(); r.p->setInstrument({ Mall, MallPlace::EPiano }); r.p->setInstrumentMode(true); } },
        { "world, idle (no notes)", [](EditorRun& r) { r.p->setInstrumentMode(false); r.audioThread->notes = 0; } },
    };
    juce::MessageManager::callAsync([&run] { run.start(); });
    juce::MessageManager::getInstance()->runDispatchLoop();
    return 0;
}

// The listening fingerprint: every Place (four busy copies, as above) and the processor
// tests' recordings renders, written as raw float WAVs to compare before and after a change.
int renders(const juce::String& dir) {
    samples::Bank::get().load();
    samples::Bank::get().waitUntilLoaded(60000);
    juce::File out(dir);
    out.createDirectory();
    for (int type : builtPlaces()) {
        auto st = busyPlaces(type);
        const auto a = st.render(12.0);
        juce::FileOutputStream f(out.getChildFile(juce::String(placeKeys[std::size_t(type)]) + ".f32"));
        f.setPosition(0);
        f.truncate();
        for (std::size_t i = 0; i < a.l.size(); ++i) {
            f.writeFloat(a.l[i]);
            f.writeFloat(a.r[i]);
        }
        std::printf("render %s\n", placeKeys[std::size_t(type)]);
        std::fflush(stdout);
    }
    return 0;
}

// Scenes: each factory scene played in Arrangement mode for 40 s from its start (raw
// floats), to compare builds sample for sample.
int sceneRenders(const juce::String& dir) {
    samples::Bank::get().load();
    samples::Bank::get().waitUntilLoaded(60000);
    juce::File out(dir);
    out.createDirectory();
    for (int i = 0; i < scenes::factoryCount(); ++i) {
        Host h(48000, 512);
        h.p->loadScene(scenes::factory(i));
        h.set("playmode", float(PlayArrangement));
        h.playHead.playing = true;
        h.playHead.position = 0;
        const auto a = h.render(40.0);
        juce::FileOutputStream f(out.getChildFile(juce::String(i) + ".f32"));
        f.setPosition(0);
        f.truncate();
        for (std::size_t k = 0; k < a.l.size(); ++k) {
            f.writeFloat(a.l[k]);
            f.writeFloat(a.r[k]);
        }
        std::printf("scene %d %s\n", i, scenes::factory(i).name.c_str());
        std::fflush(stdout);
    }
    return 0;
}

} // namespace bench

int main(int argc, char** argv) {
    juce::ScopedJuceInitialiser_GUI gui;
    // WB_BENCH_SAMPLES: another prepared folder (an older commit measured with the sets it used).
    const char* folder = std::getenv("WB_BENCH_SAMPLES");
    samples::Bank::setFolder(juce::File(juce::String(folder != nullptr ? folder : WB_TEST_SAMPLES_DIR)));
    const juce::String mode = argc > 1 ? argv[1] : "audio";
    for (int i = 1; i + 1 < argc; ++i)
        if (juce::String(argv[i]) == "--place")
            for (const auto& key : juce::StringArray::fromTokens(argv[i + 1], ",", ""))
                for (int type = NoPlace + 1; type < PlaceTypeCount; ++type)
                    if (key == placeKeys[std::size_t(type)]) placeFilter.push_back(type);
    const juce::String arg = argc > 2 && !juce::String(argv[2]).startsWith("--") ? argv[2] : "";
    if (mode == "audio") return bench::audio(arg.isNotEmpty() ? arg.getIntValue() : 3);
    if (mode == "load") return bench::load();
    if (mode == "count") return bench::count();
    if (mode == "editor") return bench::editor(arg.isNotEmpty() ? arg.getDoubleValue() : 4.0);
    if (mode == "renders") return bench::renders(arg.isNotEmpty() ? arg : "renders");
    if (mode == "scenes") return bench::sceneRenders(arg.isNotEmpty() ? arg : "scenes");
    std::fprintf(stderr, "usage: WorldbuilderBench audio [repeats] | load | editor [seconds] | renders <dir>\n");
    return 2;
}
