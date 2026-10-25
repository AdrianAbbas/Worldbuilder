#include "CapturePanel.h"
#include "WorldLookAndFeel.h"

namespace wb::ui {

namespace {
juce::Font font(float size, bool bold = false) { return juce::Font(juce::FontOptions(size, bold ? juce::Font::bold : juce::Font::plain)); }
}

//==============================================================================
// A pill button that can also be dragged out of the plugin as a file: a click calls
// onClick, a drag hands the files from dragFiles() to the host.
class CapturePanel::Chip final : public juce::Component, public juce::SettableTooltipClient {
public:
    juce::String text;
    bool arrow = false, active = true;
    float progress = -1;   // 0..1 while rendering
    std::function<void()> onClick;
    std::function<juce::StringArray()> dragFiles;

    void paint(juce::Graphics& g) override {
        const auto r = getLocalBounds().toFloat().reduced(1.0f);
        const auto accent = active ? colours::cyan : colours::dim;
        const float corner = r.getHeight() * 0.5f;
        juce::ColourGradient fill(accent.withAlpha(hover ? 0.30f : 0.18f), r.getX(), r.getY(), colours::panel.withAlpha(0.9f), r.getX(), r.getBottom(), false);
        g.setGradientFill(fill);
        g.fillRoundedRectangle(r, corner);
        if (progress >= 0.0f) {
            g.setColour(colours::brass.withAlpha(0.45f));
            g.saveState();
            g.reduceClipRegion(r.withWidth(r.getWidth() * juce::jlimit(0.0f, 1.0f, progress)).toNearestInt());
            g.fillRoundedRectangle(r, corner);
            g.restoreState();
        }
        g.setColour(accent.withAlpha(active ? 0.85f : 0.5f));
        g.drawRoundedRectangle(r, corner, 1.0f);
        g.setFont(font(10.5f, true));
        g.setColour(active ? colours::parchment : colours::dim);
        g.drawText(text, r.withTrimmedRight(arrow ? 10.0f : 0.0f), juce::Justification::centred, false);
        if (arrow) {
            juce::Path p;
            const float x = r.getRight() - 13.0f, y = r.getCentreY();
            p.addTriangle(x, y - 3.5f, x, y + 3.5f, x + 5.0f, y);
            g.setColour(accent);
            g.fillPath(p);
        }
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
// Renders every pass of a scene into the work folder, one after the other.
class CapturePanel::Worker final : public juce::Thread {
public:
    Worker(capture::Scene s, std::vector<capture::Pass> p, int b, juce::Array<juce::File> f)
        : juce::Thread("WORLDBUILDER render"), scene(std::move(s)), passes(std::move(p)), bars(b), files(std::move(f)) {}
    void run() override {
        for (std::size_t i = 0; i < passes.size() && !threadShouldExit(); ++i) {
            current.store(int(i));
            partial.store(0);
            if (!capture::renderToWav(scene, passes[i], bars, files[int(i)], &cancel, &partial)) {
                failed.store(!cancel.load());
                break;
            }
        }
        finished.store(true);
    }
    float progress() const { return (float(current.load()) + partial.load()) / float(std::max<std::size_t>(1, passes.size())); }
    bool succeeded() const { return finished.load() && !cancel.load() && !failed.load(); }

    capture::Scene scene;
    std::vector<capture::Pass> passes;
    int bars;
    juce::Array<juce::File> files;
    std::atomic<bool> cancel { false }, finished { false }, failed { false };
    std::atomic<int> current { 0 };
    std::atomic<float> partial { 0 };
};

//==============================================================================
CapturePanel::CapturePanel(WorldbuilderProcessor& p) : processor(p) {
    for (int b : capture::barChoices) barChoice.addItem(juce::String(b) + " bars", b);
    barChoice.setSelectedId(8, juce::dontSendNotification);
    barChoice.setTooltip("How many bars to render or export, from bar 1, at the host's tempo (4/4).");
    addAndMakeVisible(barChoice);
    mode.addItem("Mix", 1);
    mode.addItem("Stems", 2);
    mode.setSelectedId(1, juce::dontSendNotification);
    mode.setTooltip("Mix: one WAV of the whole world. Stems: one WAV per Place, plus the environment layers, "
                    "each through the master section (they add up to about the mix).");
    addAndMakeVisible(mode);

    midi = std::make_unique<Chip>();
    midi->text = "MIDI";
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
    render->setTooltip("Renders the bars of the current world offline, faster than real time, exactly as the host plays it from bar 1 "
                       "in Transport mode. Click again to cancel. Automation is not included: for that, bounce in the host.");
    render->onClick = [this] { rendering() ? cancelRender() : startRender(); };
    addAndMakeVisible(*render);

    wav = std::make_unique<Chip>();
    wav->text = "WAV";
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

int CapturePanel::bars() const { return std::max(1, barChoice.getSelectedId()); }
bool CapturePanel::stems() const { return mode.getSelectedId() == 2; }
void CapturePanel::setBars(int b) { barChoice.setSelectedId(b, juce::dontSendNotification); }
void CapturePanel::setStems(bool on) { mode.setSelectedId(on ? 2 : 1, juce::dontSendNotification); }

juce::File CapturePanel::workFolder() {
    return juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("WORLDBUILDER").getChildFile("Capture");
}

juce::File CapturePanel::tempMidiFile() const { return workFolder().getChildFile("WORLDBUILDER scene " + juce::String(bars()) + " bars.mid"); }

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
    const bool ok = capture::sceneMidi(tracks, bars(), scene.bpm).writeTo(out, 1);
    out.flush();
    status = ok ? juce::String(int(tracks.size())) + " MIDI tracks" : juce::String("MIDI not written");
    repaint();
    return ok;
}

void CapturePanel::startRender() {
    cancelRender();
    const auto scene = processor.captureScene();
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
    const auto stem = "WORLDBUILDER " + juce::String(bars()) + " bars";
    for (const auto& pass : list)
        files.add(folder.getChildFile(juce::File::createLegalFileName(list.size() == 1 && pass.name == "Mix" ? stem : stem + " - " + pass.name) + ".wav"));
    worker = std::make_unique<Worker>(scene, std::move(list), bars(), files);
    worker->startThread(juce::Thread::Priority::low);
    status = {};
    render->text = "CANCEL";
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
    status = ok ? (rendered.size() == 1 ? juce::String("Rendered") : juce::String(rendered.size()) + " stems") : juce::String("Render failed");
    worker.reset();
    render->text = "RENDER";
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
        chooser = std::make_unique<juce::FileChooser>("Choose a folder for the stems", documents);
        chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories, [this](const juce::FileChooser& c) {
            const auto folder = c.getResult();
            if (folder == juce::File()) return;
            bool ok = true;
            for (const auto& f : rendered) ok = f.copyFileTo(folder.getChildFile(f.getFileName())) && ok;
            status = ok ? juce::String(rendered.size()) + " stems saved" : juce::String("Not saved");
            repaint();
        });
    }
}

void CapturePanel::paint(juce::Graphics& g) {
    g.setFont(juce::Font(juce::FontOptions(10.5f, juce::Font::bold)).withExtraKerningFactor(0.12f));
    g.setColour(colours::brass.withAlpha(0.8f));
    g.drawText("CAPTURE", label, juce::Justification::centredLeft, false);
    if (status.isNotEmpty()) {
        g.setFont(font(10.0f));
        g.setColour(colours::dim);
        g.drawText(status, label.withTrimmedLeft(70), juce::Justification::centredRight, true);
    }
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
}

} // namespace wb::ui
