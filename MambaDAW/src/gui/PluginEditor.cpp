#include "PluginEditor.h"
#include "../plugin_processor.h"
#include "MambaColors.h"
#include <juce_gui_basics/juce_gui_basics.h>

using namespace juce;

// ---------------------------------------------------------------------------
// Layout constants (fixed 700×520 px window)
// ---------------------------------------------------------------------------
static constexpr int kW          = 700;
static constexpr int kHeaderH    = 40;
static constexpr int kContextH   = 36;
static constexpr int kControlH   = 52;
static constexpr int kAISectH    = 126;  // AI read-only section
static constexpr int kManDivH    = 22;   // "MANUAL OVERRIDES" divider bar
static constexpr int kManSectH   = 200;  // manual control panels
static constexpr int kBottomH    = 44;

// y-offsets
static constexpr int kHeaderY    = 0;
static constexpr int kContextY   = kHeaderH;
static constexpr int kControlY   = kContextY  + kContextH;
static constexpr int kAISectY    = kControlY  + kControlH;
static constexpr int kManDivY    = kAISectY   + kAISectH;
static constexpr int kManSectY   = kManDivY   + kManDivH;
static constexpr int kBottomY    = kManSectY  + kManSectH;
static constexpr int kTotalH     = kBottomY   + kBottomH;  // 520

// Effect section widths — must sum to kW.
static constexpr int kEqW   = 282;
static constexpr int kCompW = 220;
static constexpr int kSatW  = 110;
static constexpr int kRevW  = kW - kEqW - kCompW - kSatW;  // 88

// AI param index ranges (per EffectParamBlock layout).
static constexpr int kEqStart   = 0;
static constexpr int kCompStart = 6;
static constexpr int kSatStart  = 12;
static constexpr int kRevStart  = 15;

// ---------------------------------------------------------------------------

PluginEditor::PluginEditor(PluginProcessor& p,
                           const clap_host_t* host,
                           const clap_host_params_t* hostParams)
    : processor_(p)
    , host_(host)
    , hostParams_(hostParams)
    , controlBar_([this](uint32_t id, double v) { setParam(id, v); })
    , eqPanel_  ("EQ",   {"Lo", "Mid", "Hi", "LoHz", "HiHz", "Wet"}, {"dB","dB","dB","Hz","Hz",""})
    , compPanel_("COMP", {"Thr", "Rat", "Atk", "Rel", "Mkp", "Wet"}, {"dB","x","ms","ms","dB",""})
    , satPanel_ ("SAT",  {"Drv", "Tn", "Wet"}, {"","",""})
    , revPanel_ ("REV",  {"Dcy", "Pre", "Dif", "Wet"}, {"s","ms","","" })
    , manualEQ_  ([this](uint32_t id, double v) { setParam(id, v); })
    , manualComp_([this](uint32_t id, double v) { setParam(id, v); })
    , manualSat_ ([this](uint32_t id, double v) { setParam(id, v); })
    , manualRev_ ([this](uint32_t id, double v) { setParam(id, v); })
    , ctxLenSlider_(Slider::LinearHorizontal, Slider::NoTextBox)
{
    setSize(kW, kTotalH);

    addAndMakeVisible(headerBar_);
    addAndMakeVisible(contextMeter_);
    addAndMakeVisible(controlBar_);
    addAndMakeVisible(eqPanel_);
    addAndMakeVisible(compPanel_);
    addAndMakeVisible(satPanel_);
    addAndMakeVisible(revPanel_);
    addAndMakeVisible(manualBar_);
    addAndMakeVisible(manualEQ_);
    addAndMakeVisible(manualComp_);
    addAndMakeVisible(manualSat_);
    addAndMakeVisible(manualRev_);

    // Bottom bar
    ctxLenSlider_.setRange(10.0, 120.0, 1.0);
    ctxLenSlider_.setValue(60.0, dontSendNotification);
    ctxLenSlider_.onValueChange = [this] {
        setParam(5, ctxLenSlider_.getValue());
        contextMeter_.setContextLengthSeconds(static_cast<float>(ctxLenSlider_.getValue()));
    };
    addAndMakeVisible(ctxLenSlider_);

    ctxLenLabel_.setText("Ctx: 60s", dontSendNotification);
    ctxLenLabel_.setColour(Label::textColourId, MambaColors::textSecondary);
    ctxLenLabel_.setFont(Font(FontOptions(11.f)));
    ctxLenLabel_.setJustificationType(Justification::centredRight);
    addAndMakeVisible(ctxLenLabel_);

    resetButton_.onClick = [this] { setParam(13, 1.0); };
    addAndMakeVisible(resetButton_);

    loadButton_.onClick = [this] {
        chooser_ = std::make_unique<FileChooser>(
            "Load Mamba Weights", File::getSpecialLocation(File::userHomeDirectory),
            "*.mambaweights");
        chooser_->launchAsync(FileBrowserComponent::openMode |
                              FileBrowserComponent::canSelectFiles,
            [this](const FileChooser& fc) {
                const auto results = fc.getResults();
                if (results.isEmpty()) return;
                processor_.loadModel(results[0].getFullPathName().toStdString());
            });
    };
    addAndMakeVisible(loadButton_);

    setLookAndFeel(&laf_);
    // Timer is started by gui_bridge::gui_set_parent when the window is attached.
}

PluginEditor::~PluginEditor() {
    juce::Timer::stopTimer();
    setLookAndFeel(nullptr);
}

void PluginEditor::resized() {
    headerBar_   .setBounds(0, kHeaderY,  kW, kHeaderH);
    contextMeter_.setBounds(0, kContextY, kW, kContextH);
    controlBar_  .setBounds(0, kControlY, kW, kControlH);

    int x = 0;
    eqPanel_  .setBounds(x, kAISectY, kEqW,   kAISectH); x += kEqW;
    compPanel_.setBounds(x, kAISectY, kCompW,  kAISectH); x += kCompW;
    satPanel_ .setBounds(x, kAISectY, kSatW,   kAISectH); x += kSatW;
    revPanel_ .setBounds(x, kAISectY, kRevW,   kAISectH);

    manualBar_.setBounds(0, kManDivY, kW, kManDivH);

    x = 0;
    manualEQ_  .setBounds(x, kManSectY, kEqW,   kManSectH); x += kEqW;
    manualComp_.setBounds(x, kManSectY, kCompW,  kManSectH); x += kCompW;
    manualSat_ .setBounds(x, kManSectY, kSatW,   kManSectH); x += kSatW;
    manualRev_ .setBounds(x, kManSectY, kRevW,   kManSectH);

    // Bottom bar layout
    const int by  = kBottomY;
    const int bh  = kBottomH;
    const int pad = 8;
    const int btnW = 80;
    const int lblW = 50;
    const int slW  = kW - pad * 4 - lblW - btnW * 2;

    ctxLenLabel_ .setBounds(pad, by, lblW, bh);
    ctxLenSlider_.setBounds(pad + lblW, by + 8, slW, bh - 16);
    resetButton_ .setBounds(pad + lblW + slW + pad, by + 8, btnW, bh - 16);
    loadButton_  .setBounds(pad + lblW + slW + pad + btnW + pad, by + 8, btnW, bh - 16);
}

void PluginEditor::paint(Graphics& g) {
    g.fillAll(MambaColors::background);

    // Bottom bar divider
    g.setColour(MambaColors::border);
    g.fillRect(0, kBottomY, kW, 1);
}

void PluginEditor::timerCallback() {
    const float dt = kTimerIntervalMs / 1000.f;

    // 1. Context meter
    contextMeter_.setFillRatio(processor_.getContextFillRatio());
    contextMeter_.setContextLengthSeconds(
        static_cast<float>(processor_.getParamValue(5)));
    contextMeter_.tick(dt);
    contextMeter_.repaint();

    // 2. Header state
    const bool active   = processor_.isInferenceActive();
    const bool loaded   = processor_.isModelLoaded();
    const bool degraded = processor_.isDegraded();
    headerBar_.setInferenceActive(active);
    headerBar_.setModelLoaded(loaded);
    headerBar_.setDegraded(degraded);
    contextMeter_.setInferenceActive(active);

    // 3. Level meters
    controlBar_.setPeakL(processor_.getPeakL());
    controlBar_.setPeakR(processor_.getPeakR());
    controlBar_.tickMeter(dt);

    // 4. Clip flash
    if (processor_.clipDetected()) {
        clipFlashMs_ = kClipFlashDurationMs;
        processor_.clearClipFlag();
    }
    if (clipFlashMs_ > 0)
        clipFlashMs_ -= kTimerIntervalMs;

    // 5. AI param values → section panels (read-only display)
    float vals[N_EFFECT_PARAMS];
    for (int i = 0; i < N_EFFECT_PARAMS; ++i)
        vals[i] = processor_.getAIParamValue(i);

    const bool manualMode = processor_.getParamValue(1) >= 0.5;
    eqPanel_  .updateAIValues(vals + kEqStart,   6, manualMode);
    compPanel_.updateAIValues(vals + kCompStart,  6, manualMode);
    satPanel_ .updateAIValues(vals + kSatStart,   3, manualMode);
    revPanel_ .updateAIValues(vals + kRevStart,   4, manualMode);

    // 6. Wet sliders sync to params 6-9
    eqPanel_  .wetSlider().setValue(processor_.getParamValue(6), dontSendNotification);
    compPanel_.wetSlider().setValue(processor_.getParamValue(7), dontSendNotification);
    satPanel_ .wetSlider().setValue(processor_.getParamValue(8), dontSendNotification);
    revPanel_ .wetSlider().setValue(processor_.getParamValue(9), dontSendNotification);

    // 7. Manual panels — sync slider positions from processor
    const bool eqManual   = processor_.getParamValue(14) >= 0.5;
    const bool compManual = processor_.getParamValue(15) >= 0.5;
    const bool satManual  = processor_.getParamValue(16) >= 0.5;
    const bool revManual  = processor_.getParamValue(17) >= 0.5;

    manualEQ_.syncFromProcessor(eqManual,
        processor_.getParamValue(18),  // lo_gain
        processor_.getParamValue(19),  // lo_hz
        processor_.getParamValue(20),  // mid_gain
        processor_.getParamValue(21),  // mid_hz
        processor_.getParamValue(22),  // mid_q
        processor_.getParamValue(23),  // hi_gain
        processor_.getParamValue(24)); // hi_hz

    manualComp_.syncFromProcessor(compManual,
        processor_.getParamValue(10),  // thresh
        processor_.getParamValue(25),  // ratio
        processor_.getParamValue(26),  // attack
        processor_.getParamValue(27),  // release
        processor_.getParamValue(28)); // makeup

    manualSat_.syncFromProcessor(satManual,
        processor_.getParamValue(29),  // drive
        processor_.getParamValue(30)); // tone

    manualRev_.syncFromProcessor(revManual,
        processor_.getParamValue(11),  // decay
        processor_.getParamValue(31),  // predelay
        processor_.getParamValue(32)); // diffusion

    // 8. Ctx length label
    const double ctxSecs = processor_.getParamValue(5);
    ctxLenLabel_.setText(String("Ctx: ") + String(static_cast<int>(ctxSecs)) + String("s"),
                         dontSendNotification);

    repaint();
}

void PluginEditor::setParam(uint32_t paramId, double value) {
    processor_.setParamValue(paramId, value);
    // Notify host so automation lanes record the change.
    if (hostParams_ && host_)
        hostParams_->request_flush(host_);
}
