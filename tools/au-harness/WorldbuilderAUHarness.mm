// WorldbuilderAUHarness: a small AudioToolbox host that loads aumu/Wbld/Veil the way
// real hosts do and checks that it actually produces sound and shows its view.
//
// It deliberately does not use JUCE's hosting code. Three hosting paths run:
//
//   1. AUv2 API, out-of-process  (AudioComponentInstantiate + LoadOutOfProcess)
//   2. AUv2 API, in-process      (AudioComponentInstantiate + LoadInProcess)
//   3. AUAudioUnit API, out-of-process (what modern hosts such as GarageBand
//      and Logic use on Apple Silicon, where AUv2 plug-ins are bridged into
//      AUHostingServiceXPC)
//
// WORLDBUILDER is an instrument with no input bus. Each path renders with no
// input and no MIDI and checks that the world is audible, Master Level and Power
// changes, MIDI acceptance, factory presets, state round trip and the editor view.
//
// Usage:
//   WorldbuilderAUHarness [--strict] [--no-window] [--component aumu:Wbld:Veil]
//                 [--only v2-oop|v2-inproc|auaudiounit]
//
// Without --strict the harness reports and exits 0 (used to characterise an
// old build). With --strict any FAIL gives exit code 1. Exit code 2 means the
// component is not registered at all.

#import <AppKit/AppKit.h>
#import <AVFoundation/AVFoundation.h>
#import <AudioToolbox/AudioToolbox.h>
#import <AudioUnit/AUCocoaUIView.h>
#import <AudioUnit/AudioUnit.h>
#import <CoreAudioKit/CoreAudioKit.h>

#include <mach-o/dyld.h>
#include <mach/mach_time.h>
#include <sys/sysctl.h>
#include <unistd.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdarg>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <string>
#include <strings.h>
#include <thread>
#include <vector>

namespace {

//==============================================================================
std::string strf(const char* f, ...) {
    char buffer[2048];
    va_list args;
    va_start(args, f);
    std::vsnprintf(buffer, sizeof(buffer), f, args);
    va_end(args);
    return buffer;
}

std::string statusText(OSStatus s) {
    const auto u = static_cast<UInt32>(s);
    char code[5] = { char(u >> 24), char(u >> 16), char(u >> 8), char(u), 0 };
    bool printable = true;
    for (int i = 0; i < 4; ++i) printable &= code[i] >= 32 && code[i] < 127;
    return printable ? strf("%d '%s'", int(s), code) : strf("%d", int(s));
}

std::string toString(NSString* s) { return s != nil ? std::string(s.UTF8String) : std::string("(nil)"); }

std::string toString(CFStringRef s) {
    if (s == nullptr) return "(null)";
    char buffer[512];
    return CFStringGetCString(s, buffer, sizeof(buffer), kCFStringEncodingUTF8) ? buffer : "(unconvertible)";
}

OSType fourCC(const std::string& s) {
    if (s.size() != 4) return 0;
    return (OSType(uint8_t(s[0])) << 24) | (OSType(uint8_t(s[1])) << 16) | (OSType(uint8_t(s[2])) << 8) | OSType(uint8_t(s[3]));
}

double toDb(double rms) { return 20.0 * std::log10(std::max(rms, 1.0e-12)); }

//==============================================================================
struct Report {
    int passed = 0, failed = 0, skipped = 0;
    std::string scope = "setup";

    void line(const char* tag, const std::string& name, const std::string& detail) {
        std::printf("%-4s [%s] %s%s%s\n", tag, scope.c_str(), name.c_str(), detail.empty() ? "" : " :: ", detail.c_str());
        std::fflush(stdout);
    }
    bool check(bool okay, const std::string& name, const std::string& detail = {}) {
        okay ? ++passed : ++failed;
        line(okay ? "PASS" : "FAIL", name, detail);
        return okay;
    }
    void skip(const std::string& name, const std::string& why) { ++skipped; line("SKIP", name, why); }
    void info(const std::string& name, const std::string& detail) { line("INFO", name, detail); }
    // Progress markers around host calls, so a crash shows exactly where it happened.
    void trace(const std::string& what) { line("TRACE", what, ""); }
};

//==============================================================================
// Spins the main run loop while waiting, so completion handlers delivered on
// the main queue (and JUCE's message thread work) can run.
bool waitFor(dispatch_semaphore_t semaphore, double seconds) {
    NSDate* deadline = [NSDate dateWithTimeIntervalSinceNow: seconds];
    while (dispatch_semaphore_wait(semaphore, DISPATCH_TIME_NOW) != 0) {
        if ([deadline timeIntervalSinceNow] <= 0) return false;
        [[NSRunLoop currentRunLoop] runMode: NSDefaultRunLoopMode beforeDate: [NSDate dateWithTimeIntervalSinceNow: 0.01]];
    }
    return true;
}

void pump(double seconds) {
    NSDate* deadline = [NSDate dateWithTimeIntervalSinceNow: seconds];
    while ([deadline timeIntervalSinceNow] > 0)
        [[NSRunLoop currentRunLoop] runMode: NSDefaultRunLoopMode beforeDate: [NSDate dateWithTimeIntervalSinceNow: 0.01]];
}

// True if a Mach-O image whose path contains `needle` is mapped into this process.
bool imageLoaded(const char* needle) {
    for (uint32_t i = 0, n = _dyld_image_count(); i < n; ++i)
        if (const char* name = _dyld_get_image_name(i))
            if (std::strstr(name, needle) != nullptr) return true;
    return false;
}

std::string shell(const char* command) {
    std::string out;
    if (FILE* pipe = popen(command, "r")) {
        char buffer[512];
        while (std::fgets(buffer, sizeof(buffer), pipe) != nullptr) out += buffer;
        pclose(pipe);
    }
    while (!out.empty() && (out.back() == '\n' || out.back() == ' ')) out.pop_back();
    return out;
}

std::string hostingServices() {
    auto list = shell("/usr/bin/pgrep -l AUHostingService 2>/dev/null | /usr/bin/tr '\\n' ';'");
    return list.empty() ? "none" : list;
}

const char* harnessArchitecture() {
   #if defined(__arm64__)
    return "arm64";
   #elif defined(__x86_64__)
    int translated = 0;
    size_t size = sizeof(translated);
    if (sysctlbyname("sysctl.proc_translated", &translated, &size, nullptr, 0) == 0 && translated == 1)
        return "x86_64 (Rosetta)";
    return "x86_64";
   #else
    return "unknown";
   #endif
}

constexpr const char* pluginImageNeedle = "WORLDBUILDER.component/Contents/MacOS/";

// Out-of-process rendering goes through Apple's render pipe, which expects a host's
// timing: one block per block-duration with host time set, and playback stopped
// before the unit is torn down. Rendering flat out from the main thread and
// uninitialising immediately trapped inside AudioToolbox (caulk::semaphore).
struct Pacer {
    bool realTime = false;
    std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
    void begin() { start = std::chrono::steady_clock::now(); }
    void wait(size_t framesDone, double sampleRate) const {
        if (realTime)
            std::this_thread::sleep_until(start + std::chrono::duration_cast<std::chrono::steady_clock::duration>(
                                                      std::chrono::duration<double>(double(framesDone) / sampleRate)));
    }
};

AudioTimeStamp hostTimeStamp(Float64 sampleTime) {
    AudioTimeStamp ts {};
    ts.mFlags = kAudioTimeStampSampleTimeValid | kAudioTimeStampHostTimeValid;
    ts.mSampleTime = sampleTime;
    ts.mHostTime = mach_absolute_time();
    return ts;
}

//==============================================================================
AudioStreamBasicDescription floatFormat(double sampleRate, UInt32 channels) {
    AudioStreamBasicDescription f {};
    f.mSampleRate = sampleRate;
    f.mFormatID = kAudioFormatLinearPCM;
    f.mFormatFlags = kAudioFormatFlagsNativeFloatPacked | kAudioFormatFlagIsNonInterleaved;
    f.mBytesPerPacket = sizeof(float);
    f.mFramesPerPacket = 1;
    f.mBytesPerFrame = sizeof(float);
    f.mChannelsPerFrame = channels;
    f.mBitsPerChannel = 32;
    return f;
}

// WORLDBUILDER is an instrument: there is no input bus, so these only measure output.
struct RenderResult {
    OSStatus status = noErr;
    double outRms[2] {};
    float peak = 0;
    bool finite = true;
    UInt32 outChannels = 0;

    double rmsDb() const { return toDb(outChannels > 1 ? std::sqrt(0.5 * (outRms[0] * outRms[0] + outRms[1] * outRms[1])) : outRms[0]); }
    std::string describe() const {
        return strf("status=%s out=%.1f dBFS (L %.1f / R %.1f) peak=%.3f finite=%d",
                      statusText(status).c_str(), rmsDb(), toDb(outRms[0]), toDb(outRms[1]), double(peak), int(finite));
    }
};

struct Meter {
    double sum[2] {};
    size_t counted = 0;
    float peak = 0;
    bool finite = true;

    void add(const float* const* channels, UInt32 numChannels, UInt32 frames, size_t firstIndex, size_t skip) {
        for (UInt32 n = 0; n < frames; ++n) {
            const size_t index = firstIndex + n;
            for (UInt32 c = 0; c < numChannels; ++c) {
                const float v = channels[c][n];
                if (!std::isfinite(v)) { finite = false; continue; }
                peak = std::max(peak, std::abs(v));
                if (index >= skip && c < 2) sum[c] += double(v) * v;
            }
            if (index >= skip) ++counted;
        }
    }
    void finish(RenderResult& r, UInt32 numChannels) const {
        const double n = double(std::max<size_t>(counted, 1));
        r.outRms[0] = std::sqrt(sum[0] / n);
        r.outRms[1] = numChannels > 1 ? std::sqrt(sum[1] / n) : r.outRms[0];
        r.peak = peak;
        r.finite = finite;
        r.outChannels = numChannels;
    }
};

//==============================================================================
// AUv2 API host for an instrument (output bus only).
struct V2Host {
    AudioUnit unit = nullptr;
    Report* report = nullptr;   // optional: trace each configuration call
    bool rendered = false;      // render resources in use since the last initialise
    Pacer pacer;
    double sampleRate = 48000.0;
    UInt32 outChannels = 2, maxFrames = 512;
    Float64 sampleTime = 0;
    std::vector<std::vector<float>> outputs;

    void trace(const char* step) { if (report != nullptr) report->trace(strf("configure %.0f Hz ->%u: %s", sampleRate, unsigned(outChannels), step)); }

    // Like a host changing sample rate or channel layout: uninitialise, set formats,
    // initialise again.
    OSStatus configure(double sr, UInt32 out, UInt32 frames = 512) {
        sampleRate = sr; outChannels = out; maxFrames = frames;
        if (rendered && pacer.realTime) pump(0.2);   // playback stopped, then reconfigure
        trace("uninitialize");
        AudioUnitUninitialize(unit);
        rendered = false;
        auto outFormat = floatFormat(sr, out);
        trace("output format");
        OSStatus s = AudioUnitSetProperty(unit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Output, 0, &outFormat, sizeof(outFormat));
        if (s != noErr) return s;
        trace("maximum frames");
        s = AudioUnitSetProperty(unit, kAudioUnitProperty_MaximumFramesPerSlice, kAudioUnitScope_Global, 0, &maxFrames, sizeof(maxFrames));
        if (s != noErr) return s;
        outputs.assign(out, std::vector<float>(maxFrames, 0.0f));
        trace("initialize");
        s = AudioUnitInitialize(unit);
        trace("done");
        return s;
    }

    RenderResult render(double seconds, double skipSeconds) {
        RenderResult result;
        const auto total = size_t(seconds * sampleRate), skip = size_t(skipSeconds * sampleRate);
        std::vector<uint8_t> storage(offsetof(AudioBufferList, mBuffers) + sizeof(AudioBuffer) * outChannels);
        auto* abl = reinterpret_cast<AudioBufferList*>(storage.data());
        std::vector<const float*> channels(outChannels);
        Meter meter;
        size_t done = 0;
        rendered = true;
        pacer.begin();
        while (done < total) {
            pacer.wait(done, sampleRate);
            const auto frames = UInt32(std::min<size_t>(maxFrames, total - done));
            abl->mNumberBuffers = outChannels;
            for (UInt32 c = 0; c < outChannels; ++c) {
                std::fill(outputs[c].begin(), outputs[c].end(), 0.0f);
                abl->mBuffers[c].mNumberChannels = 1;
                abl->mBuffers[c].mDataByteSize = frames * sizeof(float);
                abl->mBuffers[c].mData = outputs[c].data();
            }
            AudioUnitRenderActionFlags flags = 0;
            const AudioTimeStamp ts = hostTimeStamp(sampleTime);
            result.status = AudioUnitRender(unit, &flags, &ts, 0, frames, abl);
            if (result.status != noErr) break;
            for (UInt32 c = 0; c < outChannels; ++c)
                channels[c] = abl->mBuffers[c].mData != nullptr ? static_cast<const float*>(abl->mBuffers[c].mData) : outputs[c].data();
            meter.add(channels.data(), outChannels, frames, done, skip);
            sampleTime += frames;
            done += frames;
        }
        meter.finish(result, outChannels);
        return result;
    }
};

//==============================================================================
struct Param {
    AudioUnitParameterID id = 0;
    std::string name;
    AudioUnitParameterInfo info {};
};

std::vector<Param> listParameters(AudioUnit unit) {
    UInt32 size = 0;
    Boolean writable = false;
    if (AudioUnitGetPropertyInfo(unit, kAudioUnitProperty_ParameterList, kAudioUnitScope_Global, 0, &size, &writable) != noErr || size == 0)
        return {};
    std::vector<AudioUnitParameterID> ids(size / sizeof(AudioUnitParameterID));
    if (AudioUnitGetProperty(unit, kAudioUnitProperty_ParameterList, kAudioUnitScope_Global, 0, ids.data(), &size) != noErr)
        return {};
    std::vector<Param> result;
    for (auto id : ids) {
        Param p;
        p.id = id;
        UInt32 infoSize = sizeof(p.info);
        if (AudioUnitGetProperty(unit, kAudioUnitProperty_ParameterInfo, kAudioUnitScope_Global, id, &p.info, &infoSize) != noErr)
            continue;
        p.name.assign(p.info.name, strnlen(p.info.name, sizeof(p.info.name)));
        if ((p.info.flags & kAudioUnitParameterFlag_HasCFNameString) != 0 && p.info.cfNameString != nullptr) {
            p.name = toString(p.info.cfNameString);
            if ((p.info.flags & kAudioUnitParameterFlag_CFNameRelease) != 0) CFRelease(p.info.cfNameString);
            p.info.cfNameString = nullptr;
        }
        result.push_back(p);
    }
    return result;
}

const Param* findParameter(const std::vector<Param>& params, std::initializer_list<const char*> names) {
    for (const char* name : names)
        for (const auto& p : params)
            if (strcasecmp(p.name.c_str(), name) == 0) return &p;
    return nullptr;
}

float getParameter(AudioUnit unit, AudioUnitParameterID id) {
    AudioUnitParameterValue v = 0;
    AudioUnitGetParameter(unit, id, kAudioUnitScope_Global, 0, &v);
    return v;
}

OSStatus setParameter(AudioUnit unit, AudioUnitParameterID id, float value) {
    return AudioUnitSetParameter(unit, id, kAudioUnitScope_Global, 0, value, 0);
}

// JUCE publishes continuous AU parameters normalised to 0...1, so a value in real
// units ("0 dB") must be converted by the plug-in, exactly as a host does for typed text.
bool valueFromString(AudioUnit unit, AudioUnitParameterID id, NSString* text, float& value) {
    AudioUnitParameterValueFromString request {};
    request.inParamID = id;
    request.inString = (__bridge CFStringRef) text;
    UInt32 size = sizeof(request);
    if (AudioUnitGetProperty(unit, kAudioUnitProperty_ParameterValueFromString, kAudioUnitScope_Global, 0, &request, &size) != noErr)
        return false;
    value = request.outValue;
    return true;
}

bool isDiscrete(const AudioUnitParameterInfo& info) {
    return info.unit == kAudioUnitParameterUnit_Indexed || info.unit == kAudioUnitParameterUnit_Boolean;
}

//==============================================================================
AudioComponentInstance instantiate(AudioComponent component, bool outOfProcess, OSStatus& status) {
    __block AudioComponentInstance instance = nullptr;
    __block OSStatus result = noErr;
    dispatch_semaphore_t done = dispatch_semaphore_create(0);
    AudioComponentInstantiate(component,
                              outOfProcess ? kAudioComponentInstantiation_LoadOutOfProcess : kAudioComponentInstantiation_LoadInProcess,
                              ^(AudioComponentInstance i, OSStatus e) {
                                  instance = i;
                                  result = e;
                                  dispatch_semaphore_signal(done);
                              });
    if (!waitFor(done, 30.0)) {
        status = kAudioUnitErr_FailedInitialization;
        return nullptr;
    }
    status = result;
    return instance;
}

//==============================================================================
// Editor via the AUv2 Cocoa view factory. Must run in the plug-in's process.
void cocoaViewTest(Report& r, AudioUnit unit, bool useWindow) {
    UInt32 size = 0;
    Boolean writable = false;
    OSStatus s = AudioUnitGetPropertyInfo(unit, kAudioUnitProperty_CocoaUI, kAudioUnitScope_Global, 0, &size, &writable);
    if (!r.check(s == noErr && size >= sizeof(AudioUnitCocoaViewInfo), "kAudioUnitProperty_CocoaUI is published",
                 strf("status=%s size=%u", statusText(s).c_str(), unsigned(size))))
        return;
    std::vector<uint8_t> storage(size, 0);
    auto* info = reinterpret_cast<AudioUnitCocoaViewInfo*>(storage.data());
    s = AudioUnitGetProperty(unit, kAudioUnitProperty_CocoaUI, kAudioUnitScope_Global, 0, info, &size);
    if (!r.check(s == noErr && info->mCocoaAUViewBundleLocation != nullptr, "kAudioUnitProperty_CocoaUI readable", statusText(s)))
        return;
    const auto classes = UInt32((size - sizeof(CFURLRef)) / sizeof(CFStringRef));
    NSURL* url = CFBridgingRelease(info->mCocoaAUViewBundleLocation);
    NSString* className = classes > 0 ? (NSString*) CFBridgingRelease(info->mCocoaAUViewClass[0]) : nil;
    for (UInt32 i = 1; i < classes; ++i) CFRelease(info->mCocoaAUViewClass[i]);
    r.info("Cocoa view factory", strf("class=%s bundle=%s", toString(className).c_str(), toString(url.path).c_str()));

    // Hosts resolve the class through the bundle the property names.
    NSBundle* bundle = url != nil ? [NSBundle bundleWithURL: url] : nil;
    Class factoryClass = bundle != nil && className != nil ? [bundle classNamed: className] : nil;
    const char* how = "[NSBundle classNamed:]";
    if (factoryClass == nil && className != nil) { factoryClass = NSClassFromString(className); how = "NSClassFromString (bundle lookup failed)"; }
    if (!r.check(factoryClass != nil, "view factory class resolves", how)) return;
    if (!r.check([factoryClass conformsToProtocol: @protocol(AUCocoaUIBase)]
                     && [factoryClass instancesRespondToSelector: @selector(uiViewForAudioUnit:withSize:)],
                 "view factory implements AUCocoaUIBase"))
        return;

    NSView* view = nil;
    @autoreleasepool {
        id<AUCocoaUIBase> factory = [[factoryClass alloc] init];
        view = [factory uiViewForAudioUnit: unit withSize: NSMakeSize(0, 0)];
    }
    if (!r.check(view != nil, "uiViewForAudioUnit:withSize: returns an NSView")) return;
    const NSSize size0 = view.frame.size;
    r.check(size0.width >= 300 && size0.height >= 200, "editor view has a sensible size", strf("%.0f x %.0f", size0.width, size0.height));

    if (!useWindow) {
        r.skip("editor hosted in a window", "--no-window");
    } else {
        NSWindow* window = [[NSWindow alloc] initWithContentRect: NSMakeRect(80, 80, size0.width, size0.height)
                                                       styleMask: NSWindowStyleMaskTitled | NSWindowStyleMaskResizable
                                                         backing: NSBackingStoreBuffered
                                                           defer: NO];
        window.releasedWhenClosed = NO;
        NSView* container = [[NSView alloc] initWithFrame: NSMakeRect(0, 0, size0.width, size0.height)];
        window.contentView = container;
        [container addSubview: view];
        [window orderFront: nil];
        pump(1.5);
        r.check(view.window == window && NSWidth(view.frame) > 0, "editor stays alive in a window for 1.5 s",
                strf("%.0f x %.0f", NSWidth(view.frame), NSHeight(view.frame)));
        [view removeFromSuperview];
        [window orderOut: nil];
        [window close];
    }
    view = nil;
    pump(0.3);   // let JUCE delete its editor before the unit goes away
}

// Editor via kAudioUnitProperty_RequestViewController (the AUv2-API route to a remote
// view). Apple does not promise this property for every AUv2 instance, so when it is
// absent that is reported, not failed; when present it must deliver a usable view.
// The AUAudioUnit suite checks the modern hosts' route, which is required.
void requestViewControllerTest(Report& r, AudioUnit unit, bool useWindow, bool required) {
    typedef void (^ViewControllerBlock)(AUViewControllerBase*);
    UInt32 size = 0;
    Boolean writable = false;
    OSStatus s = AudioUnitGetPropertyInfo(unit, kAudioUnitProperty_RequestViewController, kAudioUnitScope_Global, 0, &size, &writable);
    const bool available = s == noErr && size == sizeof(ViewControllerBlock);
    const auto detail = strf("status=%s size=%u", statusText(s).c_str(), unsigned(size));
    if (!required && !available) {
        r.info("kAudioUnitProperty_RequestViewController not offered for this instance", detail);
        return;
    }
    if (!r.check(available, "kAudioUnitProperty_RequestViewController available", detail))
        return;
    __block AUViewControllerBase* controller = nil;
    dispatch_semaphore_t done = dispatch_semaphore_create(0);
    ViewControllerBlock callback = ^(AUViewControllerBase* vc) {
        controller = vc;
        dispatch_semaphore_signal(done);
    };
    s = AudioUnitSetProperty(unit, kAudioUnitProperty_RequestViewController, kAudioUnitScope_Global, 0, &callback, sizeof(callback));
    if (!r.check(s == noErr, "view controller requested", statusText(s))) return;
    const bool answered = waitFor(done, 20.0);
    if (!r.check(answered && controller != nil, "view controller delivered", !answered ? "timed out" : (controller == nil ? "callback returned nil" : "")))
        return;
    NSView* view = controller.view;
    const NSSize preferred = controller.preferredContentSize;
    r.check(view != nil, "view controller has a view",
            strf("class=%s view=%.0f x %.0f preferred=%.0f x %.0f", toString(NSStringFromClass([controller class])).c_str(),
                   NSWidth(view.frame), NSHeight(view.frame), preferred.width, preferred.height));
    if (view != nil && useWindow) {
        const NSSize sz = NSMakeSize(std::max<CGFloat>(NSWidth(view.frame), preferred.width), std::max<CGFloat>(NSHeight(view.frame), preferred.height));
        NSWindow* window = [[NSWindow alloc] initWithContentRect: NSMakeRect(80, 80, std::max<CGFloat>(sz.width, 100), std::max<CGFloat>(sz.height, 100))
                                                       styleMask: NSWindowStyleMaskTitled | NSWindowStyleMaskResizable
                                                         backing: NSBackingStoreBuffered
                                                           defer: NO];
        window.releasedWhenClosed = NO;
        window.contentViewController = controller;
        [window orderFront: nil];
        pump(2.0);
        r.check(NSWidth(view.frame) >= 300 && NSHeight(view.frame) >= 200, "remote editor has a sensible size after 2 s",
                strf("%.0f x %.0f", NSWidth(view.frame), NSHeight(view.frame)));
        [window orderOut: nil];
        window.contentViewController = nil;
        [window close];
    }
    controller = nil;
    pump(0.3);
}

//==============================================================================
// Common AUv2 render/parameter/preset/state checks for one instance.
void v2Suite(Report& r, AudioComponent component, bool outOfProcess, bool useWindow) {
    r.scope = outOfProcess ? "v2 out-of-process" : "v2 in-process";
    const bool imageBefore = imageLoaded(pluginImageNeedle);
    OSStatus s = noErr;
    AudioComponentInstance unit = instantiate(component, outOfProcess, s);
    if (!r.check(unit != nullptr && s == noErr, "instantiate", statusText(s))) return;
    const bool imageAfter = imageLoaded(pluginImageNeedle);
    r.info("where the plug-in code runs",
           strf("plug-in image in harness process: before=%d after=%d; AUHostingService processes: %s",
                  int(imageBefore), int(imageAfter), hostingServices().c_str()));
    if (outOfProcess && !imageBefore && imageAfter)
        r.info("note", "out-of-process was requested but the plug-in loaded into this process");

    V2Host host;
    host.unit = unit;
    host.pacer.realTime = outOfProcess && !imageAfter;

    UInt32 inputElements = 0;
    UInt32 size = sizeof(inputElements);
    AudioUnitGetProperty(unit, kAudioUnitProperty_ElementCount, kAudioUnitScope_Input, 0, &inputElements, &size);
    r.info("input elements", strf("%u (an instrument has none)", unsigned(inputElements)));

    // 1. Stereo render at 48 kHz with no input and no MIDI: the world plays by itself.
    s = host.configure(48000.0, 2);
    if (r.check(s == noErr, "stereo ->2 @48k initialise", statusText(s))) {
        auto res = host.render(4.0, 2.5);
        r.check(res.status == noErr, "render calls succeed", res.describe());
        r.check(res.finite, "output is finite");
        r.check(res.rmsDb() > -45.0, "default world is audible without MIDI (> -45 dBFS)", strf("%.1f dBFS", res.rmsDb()));
        r.check(toDb(res.outRms[1]) > -50.0, "right channel is written", strf("R %.1f dBFS", toDb(res.outRms[1])));
        r.check(res.peak < 1.0f, "peak stays below 0 dBFS", strf("peak %.3f", double(res.peak)));

        const auto params = listParameters(unit);
        r.check(!params.empty(), "parameter list is published", strf("%zu parameters", params.size()));

        // 2. Master Level to minimum through AudioUnitSetParameter is audible.
        if (const auto* master = findParameter(params, { "Master Level", "Master" })) {
            const float original = getParameter(unit, master->id);
            auto before = host.render(1.5, 0.5);
            s = setParameter(unit, master->id, master->info.minValue);
            auto after = host.render(1.5, 0.5);
            r.check(s == noErr && before.rmsDb() - after.rmsDb() > 12.0, "AudioUnitSetParameter(Master Level -> minimum) is audible",
                    strf("%.1f dBFS -> %.1f dBFS", before.rmsDb(), after.rmsDb()));
            setParameter(unit, master->id, original);
        } else {
            r.check(false, "parameter named Master Level exists");
        }

        // 3. Power off fades the world out; Power on brings it back.
        if (const auto* power = findParameter(params, { "Power" })) {
            const float original = getParameter(unit, power->id);
            setParameter(unit, power->id, power->info.minValue);
            auto off = host.render(3.5, 3.0);
            r.check(off.rmsDb() < -90.0, "Power off is silent after the fade", off.describe());
            setParameter(unit, power->id, power->info.maxValue);
            auto on = host.render(3.5, 2.5);
            r.check(on.rmsDb() > -45.0, "Power on brings the world back", on.describe());
            setParameter(unit, power->id, original);
        } else {
            r.check(false, "parameter named Power exists");
        }

        // 4. MIDI notes are accepted while rendering (events arrive in a later milestone).
        s = MusicDeviceMIDIEvent(unit, 0x90, 60, 100, 0);
        auto withNote = host.render(1.0, 0.0);
        const OSStatus s2 = MusicDeviceMIDIEvent(unit, 0x80, 60, 0, 0);
        r.check(s == noErr && s2 == noErr && withNote.status == noErr && withNote.finite, "MIDI note on/off accepted", withNote.describe());

        // 4b. KEYS (Milestone 14): with a Play target, MIDI plays the instrument. The target
        // is set through the state (it is not a parameter): Place 1 (the Castle) > Horns.
        // Its patterns are switched off and the environment is down, so what is heard is
        // the note.
        {
            CFPropertyListRef info = nullptr;
            UInt32 infoSize = sizeof(info);
            bool targeted = false;
            if (AudioUnitGetProperty(unit, kAudioUnitProperty_ClassInfo, kAudioUnitScope_Global, 0, &info, &infoSize) == noErr && info != nullptr
                && CFGetTypeID(info) == CFDictionaryGetTypeID()) {
                CFMutableDictionaryRef dict = CFDictionaryCreateMutableCopy(nullptr, 0, static_cast<CFDictionaryRef>(info));
                const auto data = static_cast<CFDataRef>(CFDictionaryGetValue(dict, CFSTR("jucePluginState")));
                if (data != nullptr && CFGetTypeID(data) == CFDataGetTypeID() && CFDataGetLength(data) > 8) {
                    // JUCE's binary XML: a magic number, the text's size, the text.
                    const UInt8* bytes = CFDataGetBytePtr(data);
                    uint32_t magic = 0, length = 0;
                    std::memcpy(&magic, bytes, 4);
                    std::memcpy(&length, bytes + 4, 4);
                    if (magic == 0x21324356 && length + 8 <= UInt32(CFDataGetLength(data))) {
                        std::string xml(reinterpret_cast<const char*>(bytes + 8), strnlen(reinterpret_cast<const char*>(bytes + 8), length));
                        const auto end = xml.rfind("</");
                        if (end != std::string::npos) {
                            xml.insert(end, "<Keys slot=\"0\" element=\"2\" monitor=\"1\"/>");
                            std::vector<UInt8> out(8 + xml.size() + 1, 0);
                            const uint32_t size2 = uint32_t(xml.size() + 1);
                            std::memcpy(out.data(), &magic, 4);
                            std::memcpy(out.data() + 4, &size2, 4);
                            std::memcpy(out.data() + 8, xml.data(), xml.size());
                            CFDataRef replaced = CFDataCreate(nullptr, out.data(), CFIndex(out.size()));
                            CFDictionarySetValue(dict, CFSTR("jucePluginState"), replaced);
                            CFRelease(replaced);
                            CFPropertyListRef next = dict;
                            targeted = AudioUnitSetProperty(unit, kAudioUnitProperty_ClassInfo, kAudioUnitScope_Global, 0, &next, sizeof(next)) == noErr;
                        }
                    }
                }
                CFRelease(dict);
            }
            if (info != nullptr) CFRelease(info);
            if (r.check(targeted, "KEYS: a Play target set through the state")) {
                std::vector<std::pair<AudioUnitParameterID, float>> restore;
                for (const auto& p : params) {
                    const bool elementSwitch = p.name.rfind("Place 1 Element", 0) == 0 && p.name.size() > 3 && p.name.compare(p.name.size() - 3, 3, " On") == 0;
                    if (elementSwitch) {
                        restore.push_back({ p.id, getParameter(unit, p.id) });
                        setParameter(unit, p.id, 0.0f);
                    }
                }
                if (const auto* env = findParameter(params, { "Environment Level" })) {
                    restore.push_back({ env->id, getParameter(unit, env->id) });
                    setParameter(unit, env->id, env->info.minValue);
                }
                auto quiet = host.render(4.0, 3.0);
                const OSStatus on = MusicDeviceMIDIEvent(unit, 0x90, 57, 110, 0);
                auto played = host.render(1.0, 0.2);
                MusicDeviceMIDIEvent(unit, 0x80, 57, 0, 0);
                r.check(on == noErr && played.finite && played.rmsDb() > -60.0 && played.rmsDb() > quiet.rmsDb() + 15.0,
                        "KEYS: a MIDI note plays the target instrument (audible)", strf("%.1f dBFS vs %.1f dBFS without", played.rmsDb(), quiet.rmsDb()));
                for (const auto& [id, value] : restore) setParameter(unit, id, value);
                host.render(0.5, 0.0);
            }
        }

        // 5. Factory presets via kAudioUnitProperty_PresentPreset.
        CFArrayRef presets = nullptr;
        size = sizeof(presets);
        s = AudioUnitGetProperty(unit, kAudioUnitProperty_FactoryPresets, kAudioUnitScope_Global, 0, &presets, &size);
        if (s == noErr && presets != nullptr && CFArrayGetCount(presets) > 0) {
            const CFIndex count = CFArrayGetCount(presets);
            for (CFIndex i = 0; i < count; ++i) {
                AUPreset preset = *static_cast<const AUPreset*>(CFArrayGetValueAtIndex(presets, i));
                s = AudioUnitSetProperty(unit, kAudioUnitProperty_PresentPreset, kAudioUnitScope_Global, 0, &preset, sizeof(preset));
                auto res = host.render(2.0, 1.0);
                r.check(s == noErr && res.status == noErr && res.finite && res.rmsDb() > -45.0,
                        strf("preset %ld '%s' is audible", long(preset.presetNumber), toString(preset.presetName).c_str()),
                        strf("%.1f dBFS", res.rmsDb()));
            }
        } else {
            r.info("factory presets", strf("none published (status=%s)", statusText(s).c_str()));
        }
        if (presets != nullptr) CFRelease(presets);

        // 6. ClassInfo round trip into a fresh instance. Change a few parameters
        // (never Power) so the restore has something to prove, then put them back.
        std::vector<std::pair<AudioUnitParameterID, float>> changed;
        for (const auto& p : params) {
            if ((p.info.flags & kAudioUnitParameterFlag_IsWritable) == 0 || changed.size() >= 6) continue;
            if (strcasestr(p.name.c_str(), "power") != nullptr || strcasestr(p.name.c_str(), "bypass") != nullptr) continue;
            float v = p.info.minValue + 0.37f * (p.info.maxValue - p.info.minValue);
            if (isDiscrete(p.info)) v = std::round(v) == p.info.defaultValue ? p.info.maxValue : std::round(v);
            if (std::abs(v - p.info.defaultValue) < 1.0e-4f) continue;
            const float original = getParameter(unit, p.id);
            if (setParameter(unit, p.id, v) == noErr) changed.push_back({ p.id, original });
        }
        // Let the changes reach an out-of-process instance before saving.
        host.render(0.3, 0.0);
        pump(0.3);
        CFPropertyListRef state = nullptr;
        size = sizeof(state);
        s = AudioUnitGetProperty(unit, kAudioUnitProperty_ClassInfo, kAudioUnitScope_Global, 0, &state, &size);
        if (r.check(s == noErr && state != nullptr, "kAudioUnitProperty_ClassInfo readable", statusText(s))) {
            OSStatus s3 = noErr;
            AudioComponentInstance fresh = instantiate(component, outOfProcess, s3);
            if (r.check(fresh != nullptr, "second instance for state restore", statusText(s3))) {
                s3 = AudioUnitSetProperty(fresh, kAudioUnitProperty_ClassInfo, kAudioUnitScope_Global, 0, &state, sizeof(state));
                int mismatches = 0;
                std::string firstMismatch;
                for (const auto& p : params) {
                    const float a = getParameter(unit, p.id), b = getParameter(fresh, p.id);
                    if (std::abs(a - b) > 1.0e-3f * std::max(1.0f, std::abs(a))) {
                        if (mismatches++ == 0) firstMismatch = strf("%s: %g vs %g", p.name.c_str(), double(a), double(b));
                    }
                }
                r.check(s3 == noErr && mismatches == 0, "state restores every parameter in a fresh instance",
                        strf("set=%s changed=%zu mismatches=%d %s", statusText(s3).c_str(), changed.size(), mismatches, firstMismatch.c_str()));
                r.trace("disposing the second instance");
                AudioComponentInstanceDispose(fresh);
            }
        }
        if (state != nullptr) CFRelease(state);
        for (const auto& [id, original] : changed) setParameter(unit, id, original);
    }
    host.report = &r;

    // 7. Other rates and block sizes.
    for (double rate : { 44100.0, 96000.0 }) {
        s = host.configure(rate, 2, rate > 50000 ? 4096 : 64);
        if (r.check(s == noErr, strf("stereo @%.0f initialise (max %u frames)", rate, unsigned(host.maxFrames)), statusText(s))) {
            auto res = host.render(3.0, 2.0);
            r.check(res.status == noErr && res.finite && res.rmsDb() > -45.0, strf("stereo @%.0f is audible", rate), res.describe());
        }
    }

    // 8. Mono output (report only: hosts give instruments stereo outputs).
    s = host.configure(48000.0, 1);
    if (s == noErr) {
        auto res = host.render(3.0, 2.0);
        r.check(res.status == noErr && res.finite && res.rmsDb() > -45.0, "mono ->1 is audible", res.describe());
    } else {
        r.info("mono ->1", strf("not accepted (%s)", statusText(s).c_str()));
    }
    if (host.pacer.realTime) pump(0.2);
    AudioUnitUninitialize(unit);

    // 9. Editor.
    if (outOfProcess && !imageAfter) {
        requestViewControllerTest(r, unit, useWindow, false);
    } else {
        host.configure(48000.0, 2);
        cocoaViewTest(r, unit, useWindow);
        requestViewControllerTest(r, unit, useWindow, false);
        AudioUnitUninitialize(unit);
    }

    AudioComponentInstanceDispose(unit);
    pump(0.2);
}

//==============================================================================
// AUAudioUnit API, out-of-process: the hosting route used by modern hosts.
// Diagnostic only in CI (see docs/DEVLOG.md).
void auAudioUnitSuite(Report& r, AudioComponentDescription description, bool useWindow) {
    r.scope = "AUAudioUnit out-of-process";
    __block AUAudioUnit* unit = nil;
    __block NSError* error = nil;
    dispatch_semaphore_t done = dispatch_semaphore_create(0);
    [AUAudioUnit instantiateWithComponentDescription: description
                                             options: kAudioComponentInstantiation_LoadOutOfProcess
                                   completionHandler: ^(AUAudioUnit* u, NSError* e) {
                                       unit = u;
                                       error = e;
                                       dispatch_semaphore_signal(done);
                                   }];
    const bool answered = waitFor(done, 30.0);
    if (!r.check(answered && unit != nil, "instantiate", answered ? toString(error.description) : "timed out")) return;
    r.info("instance", strf("class=%s name=%s; plug-in image in harness process=%d; AUHostingService processes: %s",
                              toString(NSStringFromClass([unit class])).c_str(), toString(unit.audioUnitName).c_str(),
                              int(imageLoaded(pluginImageNeedle)), hostingServices().c_str()));

    AVAudioFormat* format48 = [[AVAudioFormat alloc] initStandardFormatWithSampleRate: 48000.0 channels: 2];
    NSError* e = nil;
    const bool outOkay = unit.outputBusses.count > 0 && [unit.outputBusses[0] setFormat: format48 error: &e];
    r.check(outOkay, "set stereo 48 kHz output format", toString(e.description));
    unit.maximumFramesToRender = 512;
    AURenderBlock renderBlock = unit.renderBlock;
    e = nil;
    if (!r.check([unit allocateRenderResourcesAndReturnError: &e], "allocateRenderResources", toString(e.description))) return;
    pump(0.3);

    std::vector<float> out0(512), out1(512);
    Float64 sampleTime = 0;
    bool hostOutputBuffers = true;
    auto render = [&](double seconds, double skipSeconds) {
        RenderResult result;
        const auto total = size_t(seconds * 48000.0), skip = size_t(skipSeconds * 48000.0);
        Meter meter;
        std::vector<uint8_t> storage(offsetof(AudioBufferList, mBuffers) + sizeof(AudioBuffer) * 2);
        auto* abl = reinterpret_cast<AudioBufferList*>(storage.data());
        size_t doneFrames = 0;
        Pacer pacer;
        pacer.realTime = true;
        pacer.begin();
        while (doneFrames < total) {
            pacer.wait(doneFrames, 48000.0);
            const auto frames = UInt32(std::min<size_t>(512, total - doneFrames));
            abl->mNumberBuffers = 2;
            float* outs[2] = { out0.data(), out1.data() };
            for (UInt32 c = 0; c < 2; ++c) {
                abl->mBuffers[c].mNumberChannels = 1;
                abl->mBuffers[c].mDataByteSize = frames * sizeof(float);
                abl->mBuffers[c].mData = hostOutputBuffers ? outs[c] : nullptr;
            }
            AudioUnitRenderActionFlags flags = 0;
            const AudioTimeStamp ts = hostTimeStamp(sampleTime);
            result.status = renderBlock(&flags, &ts, frames, 0, abl, nil);
            if (result.status == noErr && (abl->mBuffers[0].mData == nullptr || abl->mBuffers[1].mData == nullptr)) {
                result.status = kAudioUnitErr_FailedInitialization;   // unit left an output buffer unset
                break;
            }
            if (result.status != noErr) break;
            const float* channels[2] = { static_cast<const float*>(abl->mBuffers[0].mData), static_cast<const float*>(abl->mBuffers[1].mData) };
            meter.add(channels, 2, frames, doneFrames, skip);
            sampleTime += frames;
            doneFrames += frames;
        }
        meter.finish(result, 2);
        return result;
    };

    auto res = render(4.0, 2.5);
    if (res.status == noErr && res.peak == 0.0f) {
        r.info("host-supplied output buffers stayed silent", "retrying with unit-supplied output buffers");
        hostOutputBuffers = false;
        auto retry = render(3.0, 1.0);
        r.info("unit-supplied output buffers", retry.describe());
        if (retry.status == noErr && retry.peak > 0.0f) res = retry;
        else hostOutputBuffers = true;
    }
    r.check(res.status == noErr, "renderBlock calls succeed", res.describe());
    r.check(res.finite && res.rmsDb() > -45.0, "default world is audible", strf("%.1f dBFS", res.rmsDb()));
    r.info("parameter tree", strf("%lu parameters", (unsigned long) unit.parameterTree.allParameters.count));

    NSDictionary<NSString*, id>* state = unit.fullState;
    r.check(state != nil, "fullState readable", strf("%lu keys", (unsigned long) state.count));
    pump(0.2);
    [unit deallocateRenderResources];

    if (useWindow) {
        __block AUViewControllerBase* controller = nil;
        dispatch_semaphore_t viewDone = dispatch_semaphore_create(0);
        [unit requestViewControllerWithCompletionHandler: ^(AUViewControllerBase* vc) {
            controller = vc;
            dispatch_semaphore_signal(viewDone);
        }];
        const bool got = waitFor(viewDone, 20.0);
        if (r.check(got && controller != nil, "requestViewController delivers a controller", got ? "nil controller" : "timed out")) {
            NSWindow* window = [[NSWindow alloc] initWithContentRect: NSMakeRect(80, 80, 400, 300)
                                                           styleMask: NSWindowStyleMaskTitled | NSWindowStyleMaskResizable
                                                             backing: NSBackingStoreBuffered
                                                               defer: NO];
            window.releasedWhenClosed = NO;
            window.contentViewController = controller;
            [window orderFront: nil];
            pump(2.0);
            NSView* view = controller.view;
            r.check(view != nil && NSWidth(view.frame) >= 300 && NSHeight(view.frame) >= 200, "remote editor has a sensible size after 2 s",
                    strf("class=%s view=%.0f x %.0f", toString(NSStringFromClass([controller class])).c_str(),
                           NSWidth(view.frame), NSHeight(view.frame)));
            [window orderOut: nil];
            window.contentViewController = nil;
            [window close];
        }
        controller = nil;
    } else {
        r.skip("requestViewController delivers a controller", "--no-window");
    }
    unit = nil;
    pump(0.3);
}

//==============================================================================
void describeComponent(Report& r, AudioComponent component) {
    CFStringRef name = nullptr;
    AudioComponentCopyName(component, &name);
    UInt32 version = 0;
    AudioComponentGetVersion(component, &version);
    AudioComponentDescription d {};
    AudioComponentGetDescription(component, &d);
    r.info("component", strf("name='%s' version=0x%x flags=0x%x sandboxSafe=%d isV3=%d canLoadInProcess=%d",
                               toString(name).c_str(), unsigned(version), unsigned(d.componentFlags),
                               int((d.componentFlags & kAudioComponentFlag_SandboxSafe) != 0),
                               int((d.componentFlags & kAudioComponentFlag_IsV3AudioUnit) != 0),
                               int((d.componentFlags & kAudioComponentFlag_CanLoadInProcess) != 0)));
    if (name != nullptr) CFRelease(name);
}

} // namespace

//==============================================================================
int main(int argc, const char* argv[]) {
    @autoreleasepool {
        bool strict = false, useWindow = true;
        std::string only, componentText = "aumu:Wbld:Veil";
        for (int i = 1; i < argc; ++i) {
            const std::string a = argv[i];
            if (a == "--strict") strict = true;
            else if (a == "--no-window") useWindow = false;
            else if (a == "--only" && i + 1 < argc) only = argv[++i];
            else if (a == "--component" && i + 1 < argc) componentText = argv[++i];
            else { std::fprintf(stderr, "unknown argument %s\n", a.c_str()); return 64; }
        }
        if (componentText.size() != 14 || componentText[4] != ':' || componentText[9] != ':') {
            std::fprintf(stderr, "--component expects type:subtype:manufacturer, e.g. aumu:Wbld:Veil\n");
            return 64;
        }

        // Watchdog: a hung load must fail the job, not stall it.
        dispatch_after(dispatch_time(DISPATCH_TIME_NOW, int64_t(600) * NSEC_PER_SEC), dispatch_get_global_queue(QOS_CLASS_DEFAULT, 0), ^{
            std::printf("FAIL [watchdog] harness exceeded 600 s\n");
            std::fflush(stdout);
            _exit(3);
        });

        [NSApplication sharedApplication];
        [NSApp setActivationPolicy: NSApplicationActivationPolicyAccessory];

        Report r;
        NSOperatingSystemVersion os = [[NSProcessInfo processInfo] operatingSystemVersion];
        r.info("environment", strf("macOS %ld.%ld.%ld, harness architecture %s",
                                     long(os.majorVersion), long(os.minorVersion), long(os.patchVersion), harnessArchitecture()));

        AudioComponentDescription description {};
        description.componentType = fourCC(componentText.substr(0, 4));
        description.componentSubType = fourCC(componentText.substr(5, 4));
        description.componentManufacturer = fourCC(componentText.substr(10, 4));
        AudioComponent component = AudioComponentFindNext(nullptr, &description);
        if (!r.check(component != nullptr, "component registered", componentText)) {
            std::printf("SUMMARY passed=%d failed=%d skipped=%d\n", r.passed, r.failed, r.skipped);
            return 2;
        }
        int matches = 0;
        for (AudioComponent c = nullptr; (c = AudioComponentFindNext(c, &description)) != nullptr;) ++matches;
        r.check(matches == 1, "exactly one registered component matches", strf("%d", matches));
        describeComponent(r, component);

        // Out-of-process first, before the in-process load maps the plug-in into this process.
        if (only.empty() || only == "v2-oop") v2Suite(r, component, true, useWindow);
        if (only.empty() || only == "auaudiounit") auAudioUnitSuite(r, description, useWindow);
        if (only.empty() || only == "v2-inproc") v2Suite(r, component, false, useWindow);

        r.scope = "summary";
        std::printf("SUMMARY passed=%d failed=%d skipped=%d\n", r.passed, r.failed, r.skipped);
        std::fflush(stdout);
        return strict && r.failed > 0 ? 1 : 0;
    }
}
