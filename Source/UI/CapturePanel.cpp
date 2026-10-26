#include "CapturePanel.h"
#include "Pixel.h"
#include "Style.h"
#include "WorldLookAndFeel.h"

namespace wb::ui {

//==============================================================================
// A pill button that can also be dragged out of the plugin as a file: a click calls
// onClick, a drag hands the files from dragFiles() to the host.
class CapturePanel::Chip final : public juce::Component, public juce::SettableTooltipClient {
public:
    juce::String text;
    Icon icon = Icon::None;
    bool arrow = false, active = true;
    float progress = -1;   // 0..1 while rendering
    std::function<void()> onClick;
    std::function<juce::StringArray()> dragFiles;

    void paint(juce::Graphics& g) override {
        // A pixel button with the chip's icon; a bar fills along its foot while rendering,
        // and a small arrow says it can be dragged out (when it has no icon).
        const auto& pal = pixel::theme().colours;
        const int w = getWidth() / pixel::unit, h = getHeight() / pixel::unit - 1;
        pixel::State s;
        s.hover = hover && active;
        s.enabled = active;
        const bool showArrow = arrow && icon == Icon::None;
        const int filled = progress >= 0.0f ? juce::roundToInt(juce::jlimit(0.0f, 1.0f, progress) * float(w - 4)) : -1;
        pixel::drawCached(g, "capture/" + text + juce::String(int(icon)) + "/" + juce::String(w) + "x" + juce::String(h) + juce::String(int(s.hover)) + juce::String(int(active))
                                 + juce::String(int(showArrow)) + "/" + juce::String(filled),
                          { -pixel::margin * pixel::unit, -pixel::margin * pixel::unit }, [&] {
                              const auto glyph = pixel::toolIcon(int(icon), 7, juce::Colours::white, juce::Colours::black);
                              auto k = pixel::button(pal, text.toUpperCase() + (showArrow ? " >" : ""), s, w, h, 0, icon != Icon::None ? &glyph : nullptr, pal.inlay);
                              if (filled >= 0) {
                                  k.fill(pixel::margin + 2, pixel::margin + h - 3, w - 4, 1, pal.well);
                                  k.fill(pixel::margin + 2, pixel::margin + h - 3, filled, 1, pal.accent);
                              }
                              return k;
                          });
    }
    void mouseEnter(const juce::MouseEvent&) override { setHover(true); }
    void mouseExit(const juce::MouseEvent&) override { setHover(false); }
    void mouseDown(const juce::MouseEvent&) override { dragged = false; }
    void mouseDrag(const juce::MouseEvent& e) override {
        if (dragged || !active || dragFiles == nullptr || e.getDistanceFromDragStart() < 6) return;
        dragged = true;
        const auto files = dragFiles();
        if (!files.isEmpty()) juce::DragAndDropContainer::performExternalDragDropOfFiles(files, false, this);
    }
    void mouseUp(const juce::MouseEvent& e) override {
        if (!dragged && active && onClick != nullptr && getLocalBounds().contains(e.getPosition())) onClick();
        dragged = false;
    }

private:
    void setHover(bool on) {
        hover = on;
        repaint();
    }
    bool hover = false, dragged = false;
};

//==============================================================================
// Renders every job (a pass of the bars, or a game file) into the work folder, one
// after the other.
class CapturePanel::Worker final : public juce::Thread {
public:
    struct Job {
        capture::Pass pass;
        capture::GameFile game;   // section -1 and kind Whole without an arrangement: the bars
        bool arranged = false;
    };
    Worker(capture::Scene s, std::vector<Job> j, int b, juce::Array<juce::File> f)
        : juce::Thread("WORLDBUILDER render"), scene(std::move(s)), jobs(std::move(j)), bars(b), files(std::move(f)) {}
    void run() override {
        for (std::size_t i = 0; i < jobs.size() && !threadShouldExit(); ++i) {
            current.store(int(i));
            partial.store(0);
            const auto& job = jobs[i];
            const bool ok = job.arranged ? capture::renderGameFileToWav(scene, job.game, job.pass, files[int(i)], &cancel, &partial)
                                         : capture::renderToWav(scene, job.pass, bars, files[int(i)], &cancel, &partial);
            if (!ok) {
                failed.store(!cancel.load());
                break;
            }
        }
        finished.store(true);
    }
    float progress() const { return (float(current.load()) + partial.load()) / float(std::max<std::size_t>(1, jobs.size())); }
    bool succeeded() const { return finished.load() && !cancel.load() && !failed.load(); }

    capture::Scene scene;
    std::vector<Job> jobs;
    int bars;
    juce::Array<juce::File> files;
    std::atomic<bool> cancel { false }, finished { false }, failed { false };
    std::atomic<int> current { 0 };
    std::atomic<float> partial { 0 };
};

//==============================================================================
CapturePanel::CapturePanel(WorldbuilderProcessor& p) : processor(p) {
    for (int b : capture::barChoices) barChoice.addItem(juce::String(b) + " bars", b);
    barChoice.addSeparator();
    barChoice.addItem("Arrangement", arrangementId);
    barChoice.setSelectedId(8, juce::dontSendNotification);
    barChoice.setTooltip("How many bars to render or export, from bar 1, at the host's tempo (4/4); or the whole Arrangement, "
                         "as the host plays it in Play Mode Arrangement.");
    addAndMakeVisible(barChoice);
    mode.addItem("Mix", MixMode);
    mode.addItem("Stems", StemsMode);
    mode.addItem("Sections", SectionsMode);
    mode.addItem("Stingers", StingersMode);
    mode.setSelectedId(MixMode, juce::dontSendNotification);
    mode.setTooltip("Mix: one WAV of the whole world. Stems: one WAV per Place, plus the environment layers, "
                    "each through the master section (they add up to about the mix). Sections: each section of the arrangement as its "
                    "own file for a game engine (Tavern_Intro.wav...), a section that repeats or loops as a seamless loop. Stingers: each "
                    "section's ending and transition as a short one-shot cue that rings out.");
    addAndMakeVisible(mode);

    midi = std::make_unique<Chip>();
    midi->text = "MIDI";
    midi->icon = Icon::Midi;
    midi->arrow = true;
    midi->setTooltip("Scene MIDI: the patterns of every active instrument, one track each (\"Place - Instrument\"), from bar 1 at the "
                     "host's tempo. Drag it onto the host's tracks, or click to save the file.");
    midi->onClick = [this] { saveMidi(); };
    midi->dragFiles = [this] {
        const auto file = tempMidiFile();
        return writeSceneMidi(file) ? juce::StringArray { file.getFullPathName() } : juce::StringArray {};
    };
    addAndMakeVisible(*midi);

    render = std::make_unique<Chip>();
    render->text = "RENDER";
    render->icon = Icon::Render;
    render->setTooltip("Renders the bars of the current world offline, faster than real time, exactly as the host plays it from bar 1 "
                       "in Transport mode. Click again to cancel. Automation is not included: for that, bounce in the host.");
    render->onClick = [this] { rendering() ? cancelRender() : startRender(); };
    addAndMakeVisible(*render);

    wav = std::make_unique<Chip>();
    wav->text = "WAV";
    wav->icon = Icon::DragOut;
    wav->arrow = true;
    wav->active = false;
    wav->setTooltip("The last render: drag it onto the host's tracks (at bar 1), or click to save it.");
    wav->onClick = [this] { saveRender(); };
    wav->dragFiles = [this] {
        juce::StringArray paths;
        for (const auto& f : rendered)
            if (f.existsAsFile()) paths.add(f.getFullPathName());
        return paths;
    };
    addAndMakeVisible(*wav);
}

CapturePanel::~CapturePanel() {
    stopTimer();
    if (worker != nullptr) {
        worker->cancel.store(true);
        worker->stopThread(10000);
    }
}

int CapturePanel::bars() const {
    const int id = barChoice.getSelectedId();
    return id <= arrangementId ? 8 : id;
}
bool CapturePanel::stems() const { return mode.getSelectedId() == StemsMode; }
void CapturePanel::setBars(int b) { barChoice.setSelectedId(b, juce::dontSendNotification); }
void CapturePanel::setStems(bool on) { mode.setSelectedId(on ? StemsMode : MixMode, juce::dontSendNotification); }
void CapturePanel::setArrangement(bool on) { barChoice.setSelectedId(on ? arrangementId : 8, juce::dontSendNotification); }
void CapturePanel::setMode(int m) { mode.setSelectedId(std::clamp(m, int(MixMode), int(StingersMode)), juce::dontSendNotification); }
int CapturePanel::currentMode() const { return std::max(int(MixMode), mode.getSelectedId()); }
bool CapturePanel::followsArrangement() const { return barChoice.getSelectedId() == arrangementId || currentMode() >= SectionsMode; }

juce::File CapturePanel::workFolder() {
    return juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("WORLDBUILDER").getChildFile("Capture");
}

juce::File CapturePanel::tempMidiFile() const {
    return workFolder().getChildFile(followsArrangement() ? juce::String("WORLDBUILDER arrangement.mid") : "WORLDBUILDER scene " + juce::String(bars()) + " bars.mid");
}

bool CapturePanel::writeSceneMidi(const juce::File& file) {
    const auto scene = processor.captureScene();
    const auto tracks = capture::sceneTracks(scene);
    if (tracks.empty()) {
        status = "No patterns playing";
        repaint();
        return false;
    }
    file.getParentDirectory().createDirectory();
    file.deleteFile();
    juce::FileOutputStream out(file);
    if (!out.openedOk()) return false;
    const bool arranged = followsArrangement() && scene.arrangement.count > 0;
    const bool ok = (arranged ? capture::arrangementMidi(scene, tracks) : capture::sceneMidi(tracks, bars(), scene.bpm)).writeTo(out, 1);
    out.flush();
    status = ok ? juce::String(int(tracks.size())) + " MIDI tracks" : juce::String("MIDI not written");
    repaint();
    return ok;
}

void CapturePanel::startRender() {
    cancelRender();
    auto scenePointer = std::make_unique<capture::Scene>(processor.captureScene());
    const auto& scene = *scenePointer;
    const bool arranged = followsArrangement();
    if (arranged && scene.arrangement.count <= 0) {
        status = "No sections to render";
        repaint();
        return;
    }
    auto list = capture::passes(scene, stems());
    if (list.empty()) {
        status = "Nothing audible";
        repaint();
        return;
    }
    const auto folder = workFolder();
    folder.createDirectory();
    for (const auto& old : folder.findChildFiles(juce::File::findFiles, false, "*.wav")) old.deleteFile();
    rendered.clear();
    wav->active = false;
    wav->repaint();
    juce::Array<juce::File> files;
    std::vector<Worker::Job> jobs;
    if (!arranged) {
        const auto stem = "WORLDBUILDER " + juce::String(bars()) + " bars";
        for (const auto& pass : list) {
            files.add(folder.getChildFile(juce::File::createLegalFileName(list.size() == 1 && pass.name == "Mix" ? stem : stem + " - " + pass.name) + ".wav"));
            jobs.push_back({ pass, {}, false });
        }
    } else if (currentMode() >= SectionsMode) {
        // Game files: the mix of each section, or each stinger.
        const auto mix = capture::passes(scene, false).front();
        for (const auto& game : capture::gameFiles(scene, currentMode() == StingersMode)) {
            files.add(folder.getChildFile(juce::File::createLegalFileName(game.name) + ".wav"));
            jobs.push_back({ mix, game, true });
        }
    } else {
        capture::GameFile whole;
        whole.name = capture::sceneName(scene) + "_Arrangement";
        for (const auto& pass : list) {
            files.add(folder.getChildFile(juce::File::createLegalFileName(list.size() == 1 && pass.name == "Mix" ? whole.name : whole.name + "_" + pass.name.removeCharacters(" ")) + ".wav"));
            jobs.push_back({ pass, whole, true });
        }
    }
    worker = std::make_unique<Worker>(std::move(*scenePointer), std::move(jobs), bars(), files);
    worker->startThread(juce::Thread::Priority::low);
    status = {};
    render->text = "CANCEL";
    render->icon = Icon::Close;
    render->progress = 0;
    render->repaint();
    startTimerHz(15);
}

void CapturePanel::cancelRender() {
    if (worker == nullptr) return;
    worker->cancel.store(true);
    worker->stopThread(10000);
    worker.reset();
    stopTimer();
    render->text = "RENDER";
    render->icon = Icon::Render;
    render->progress = -1;
    render->repaint();
    status = "Cancelled";
    repaint();
}

void CapturePanel::timerCallback() {
    if (worker == nullptr) {
        stopTimer();
        return;
    }
    if (worker->finished.load()) {
        finishRender();
        return;
    }
    render->progress = worker->progress();
    render->repaint();
}

void CapturePanel::finishRender() {
    stopTimer();
    worker->stopThread(10000);
    const bool ok = worker->succeeded();
    if (ok) rendered = worker->files;
    status = ok ? (rendered.size() == 1 ? juce::String("Rendered") : juce::String(rendered.size()) + " files") : juce::String("Render failed");
    worker.reset();
    render->text = "RENDER";
    render->icon = Icon::Render;
    render->progress = -1;
    render->repaint();
    wav->active = !rendered.isEmpty();
    wav->repaint();
    repaint();
}

bool CapturePanel::waitForRender(int timeoutMs) {
    if (worker == nullptr) return true;
    if (!worker->waitForThreadToExit(timeoutMs)) return false;
    finishRender();
    return true;
}

void CapturePanel::saveMidi() {
    chooser = std::make_unique<juce::FileChooser>("Save the scene as MIDI", juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getChildFile(tempMidiFile().getFileName()),
                                                  "*.mid");
    chooser->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting, [this](const juce::FileChooser& c) {
        if (c.getResult() != juce::File()) writeSceneMidi(c.getResult().withFileExtension("mid"));
    });
}

void CapturePanel::saveRender() {
    if (rendered.isEmpty()) return;
    const auto documents = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory);
    if (rendered.size() == 1) {
        chooser = std::make_unique<juce::FileChooser>("Save the render", documents.getChildFile(rendered[0].getFileName()), "*.wav");
        chooser->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting, [this](const juce::FileChooser& c) {
            if (c.getResult() == juce::File()) return;
            const bool ok = rendered[0].copyFileTo(c.getResult().withFileExtension("wav"));
            status = ok ? "Saved" : "Not saved";
            repaint();
        });
    } else {
        chooser = std::make_unique<juce::FileChooser>("Choose a folder for the files", documents);
        chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories, [this](const juce::FileChooser& c) {
            const auto folder = c.getResult();
            if (folder == juce::File()) return;
            bool ok = true;
            for (const auto& f : rendered) ok = f.copyFileTo(folder.getChildFile(f.getFileName())) && ok;
            status = ok ? juce::String(rendered.size()) + " files saved" : juce::String("Not saved");
            repaint();
        });
    }
}

void CapturePanel::paint(juce::Graphics& g) {
    const auto& pal = pixel::theme().colours;
    pixel::drawText(g, "CAPTURE", label.withHeight(14), pal.rim, juce::Justification::centredLeft, pixel::Font::Small);
    if (status.isNotEmpty()) pixel::drawText(g, status, label.withTrimmedLeft(70).withHeight(14), pal.inkDim, juce::Justification::centredRight, pixel::Font::Small);
}

void CapturePanel::resized() {
    auto r = getLocalBounds();
    auto top = r.removeFromTop(r.getHeight() / 2);
    // The label on the same line as the footer's other labels.
    label = top.removeFromTop(14).withHeight(11);
    const int half = (top.getWidth() - 6) / 2;
    barChoice.setBounds(top.removeFromLeft(half).withSizeKeepingCentre(half, 22));
    top.removeFromLeft(6);
    mode.setBounds(top.withSizeKeepingCentre(top.getWidth(), 22));
    auto bottom = r.withTrimmedTop(6);
    const int w = (bottom.getWidth() - 8) / 3;
    midi->setBounds(bottom.removeFromLeft(w).withSizeKeepingCentre(w, 22));
    bottom.removeFromLeft(4);
    render->setBounds(bottom.removeFromLeft(w + 2).withSizeKeepingCentre(w + 2, 22));
    bottom.removeFromLeft(4);
    wav->setBounds(bottom.withSizeKeepingCentre(bottom.getWidth(), 22));
    pixel::snapChildren(*this);
}

} // namespace wb::ui
