#include "PluginEditor.h"
#include <BinaryData.h>

using namespace juce;

namespace
{
std::vector<std::byte> toBytes (const char* data, int size)
{
    std::vector<std::byte> v ((size_t) size);
    std::memcpy (v.data(), data, (size_t) size);
    return v;
}

const char* mimeFor (const String& path)
{
    if (path.endsWithIgnoreCase (".html")) return "text/html; charset=utf-8";
    if (path.endsWithIgnoreCase (".css"))  return "text/css; charset=utf-8";
    if (path.endsWithIgnoreCase (".js"))   return "text/javascript; charset=utf-8";
    if (path.endsWithIgnoreCase (".svg"))  return "image/svg+xml";
    return "application/octet-stream";
}
} // namespace

WebBrowserComponent::Options MimicEditor::makeOptions()
{
    auto base = WebBrowserComponent::Options{};
   #if JUCE_WINDOWS
    base = base.withBackend (WebBrowserComponent::Options::Backend::webview2);
   #endif
    return base
        .withWinWebView2Options (WebBrowserComponent::Options::WinWebView2{}
                                     .withUserDataFolder (File::getSpecialLocation (File::tempDirectory).getChildFile ("MIMIC_WebView2"))
                                     .withBackgroundColour (Colour (0xff07070b))
                                     .withStatusBarDisabled())
        .withNativeIntegrationEnabled()
        .withKeepPageLoadedWhenBrowserIsHidden()
        .withOptionsFrom (artistRelay)
        .withOptionsFrom (amountRelay)
        .withOptionsFrom (tuneRelay)
        .withOptionsFrom (airRelay)
        .withOptionsFrom (gritRelay)
        .withOptionsFrom (spaceRelay)
        .withOptionsFrom (widthRelay)
        .withOptionsFrom (formantRelay)
        .withOptionsFrom (keyRelay)
        .withOptionsFrom (scaleRelay)
        .withOptionsFrom (outputRelay)
        .withNativeFunction ("getArtists", [] (const Array<var>&, WebBrowserComponent::NativeFunctionCompletion complete)
                             { complete (artistTable()); })
        .withResourceProvider ([this] (const String& url) { return getResource (url); });
}

var MimicEditor::artistTable()
{
    Array<var> list;
    for (int i = 0; i < mimic::kNumArtists; ++i)
    {
        const auto& a = mimic::kArtists[i];
        auto* o = new DynamicObject();
        o->setProperty ("index", i);
        o->setProperty ("name", String::fromUTF8 (a.name));
        o->setProperty ("category", String::fromUTF8 (a.category));
        o->setProperty ("accent", String (a.accent));
        o->setProperty ("tagline", String::fromUTF8 (a.tagline));
        o->setProperty ("tune", a.tuneMix);
        o->setProperty ("grit", a.satMix);
        o->setProperty ("space", jlimit (0.0f, 1.0f, (a.revMix + a.delayMix) * 1.6f));
        o->setProperty ("width", jlimit (0.0f, 1.0f, a.width / 1.4f));
        o->setProperty ("formant", a.formant);
        list.add (var (o));
    }
    return list;
}

MimicEditor::MimicEditor (MimicProcessor& p)
    : AudioProcessorEditor (p), proc (p)
{
    auto options = makeOptions();

    if (WebBrowserComponent::areOptionsSupported (options))
    {
        browser = std::make_unique<WebBrowserComponent> (options);
        addAndMakeVisible (*browser);

        auto& st = proc.state;
        auto addSlider = [&] (const char* id, WebSliderRelay& relay)
        {
            sliderAttachments.push_back (std::make_unique<WebSliderParameterAttachment> (*st.getParameter (id), relay, nullptr));
        };
        auto addCombo = [&] (const char* id, WebComboBoxRelay& relay)
        {
            comboAttachments.push_back (std::make_unique<WebComboBoxParameterAttachment> (*st.getParameter (id), relay, nullptr));
        };
        addCombo  (ParamID::artist,  artistRelay);
        addSlider (ParamID::amount,  amountRelay);
        addSlider (ParamID::tune,    tuneRelay);
        addSlider (ParamID::air,     airRelay);
        addSlider (ParamID::grit,    gritRelay);
        addSlider (ParamID::space,   spaceRelay);
        addSlider (ParamID::width,   widthRelay);
        addSlider (ParamID::formant, formantRelay);
        addCombo  (ParamID::key,     keyRelay);
        addCombo  (ParamID::scale,   scaleRelay);
        addSlider (ParamID::output,  outputRelay);

        browser->goToURL (WebBrowserComponent::getResourceProviderRoot());
        startTimerHz (30);
    }
    else
    {
        fallback.setText (String::fromUTF8 ("MIMIC arayüzü için Microsoft Edge WebView2 Runtime gerekli.\n"
                                            "go.microsoft.com/fwlink/p/?LinkId=2124703 adresinden kurup DAW'ı yeniden başlat.\n"
                                            "(Ses motoru çalışmaya devam ediyor.)"),
                          dontSendNotification);
        fallback.setJustificationType (Justification::centred);
        fallback.setColour (Label::textColourId, Colours::white);
        addAndMakeVisible (fallback);
    }

    setResizable (true, true);
    setResizeLimits (760, 500, 1600, 1040);
    if (auto* c = getConstrainer()) c->setFixedAspectRatio (960.0 / 620.0);
    setSize (960, 620);
}

MimicEditor::~MimicEditor()
{
    stopTimer();
    sliderAttachments.clear();
    comboAttachments.clear();
}

void MimicEditor::paint (Graphics& g)
{
    g.fillAll (Colour (0xff07070b));
}

void MimicEditor::resized()
{
    if (browser) browser->setBounds (getLocalBounds());
    fallback.setBounds (getLocalBounds().reduced (24));
}

void MimicEditor::timerCallback()
{
    if (! browser) return;
    auto* o = new DynamicObject();
    o->setProperty ("in", proc.meters.inDb.load());
    o->setProperty ("out", proc.meters.outDb.load());
    o->setProperty ("comp", proc.meters.compGr.load());
    o->setProperty ("deess", proc.meters.deessGr.load());
    o->setProperty ("detected", proc.meters.detectedMidi.load());
    o->setProperty ("target", proc.meters.targetMidi.load());
    o->setProperty ("output", proc.meters.outputMidi.load());
    o->setProperty ("latencyMs", 1000.0 * proc.getLatencySamples() / jmax (1.0, proc.getSampleRate()));
    browser->emitEventIfBrowserIsVisible ("telemetry", var (o));
}

std::optional<WebBrowserComponent::Resource> MimicEditor::getResource (const String& url)
{
    // The provider receives "/", "/style.css", ... (root-relative paths)
    auto path = url;
    if (path.startsWith (WebBrowserComponent::getResourceProviderRoot()))
        path = path.substring (WebBrowserComponent::getResourceProviderRoot().length());
    path = path.upToFirstOccurrenceOf ("?", false, false).trimCharactersAtStart ("/");
    if (path.isEmpty()) path = "index.html";

    const char* name = nullptr;
    if      (path == "index.html")     name = "index_html";
    else if (path == "style.css")      name = "style_css";
    else if (path == "app.js")         name = "app_js";
    else if (path == "juce/index.js")  name = "index_js";

    if (name != nullptr)
    {
        int size = 0;
        if (auto* data = MimicUI::getNamedResource (name, size))
            return WebBrowserComponent::Resource { toBytes (data, size), mimeFor (path) };
    }
    return std::nullopt;
}
