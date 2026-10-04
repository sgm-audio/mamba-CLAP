#include "PluginEditor.h"
#include "../plugin_processor.h"
#include "MambaColors.h"
#include <juce_gui_basics/juce_gui_basics.h>

using namespace juce;

// ---------------------------------------------------------------------------
// Layout constants (fixed 700x520 px window)
// ---------------------------------------------------------------------------
static constexpr int kW = 700;
static constexpr int kHeaderH = 40;
static constexpr int kContextH = 36;
static constexpr int kControlH = 52;
static constexpr int kAISectH = 126;
static constexpr int kManDivH = 22;
static constexpr int kManSectH = 200;
static constexpr int kBottomH = 44;

// y-offsets
static constexpr int kHeaderY = 0;
static constexpr int kContextY = kHeaderH;
static constexpr int kControlY = kContextY + kContextH;
static constexpr int kAISectY = kControlY + kControlH;
static constexpr int kManDivY = kAISectY + kAISectH;
static constexpr int kManSectY = kManDivY + kManDivH;
static constexpr int kBottomY = kManSectY + kManSectH;
static constexpr int kTotalH = kBottomY + kBottomH;

// Effect section widths — must sum to kW.
static constexpr int kEqW = 282;
static constexpr int kCompW = 220;
static constexpr int kSatW = 110;
static constexpr int kRevW = kW - kEqW - kCompW - kSatW;

// AI param index ranges (per EffectParamBlock layout).
static constexpr int kEqStart = 0;
static constexpr int kCompStart = 6;
static constexpr int kSatStart = 12;
static constexpr int kRevStart = 15;

// ---------------------------------------------------------------------------

PluginEditor::PluginEditor(PluginProcessor& p,
                           const clap_host_t* host,
                           const clap_host_params_t* hostParams)
    : processor_(p)
    , host_(host)
    , hostParams_(hostParams)
    , controlBar_([this](uint32_t id, double v) { setParam(id, v); })
    , eqPanel_ ("EQ", {"Lo", "Mid", "Hi", "LoHz", "HiHz", "Wet"}, {"dB","dB","dB","Hz","Hz",""}, 6, [this](uint32_t id, double v) { setParam(id, v); })
    , compPanel_("COMP", {"Thr", "Rat", "Atk", "Rel", "Mkp", "Wet"}, {"dB","x","ms","ms","dB",""}, 7, [this](uint32_t id, double v) { setParam(id, v); })
    , satPanel_ ("SAT", {"Drv", "Wet", "Tn"}, {"","",""}, 8, [this](uint32_t id, double v) { setParam(id, v); })
    , revPanel_ ("REV", {"Wet", "Dcy", "Pre", "Dif"}, {"","s","ms",""}, 9, [this](uint32_t id, double v) { setParam(id, v); })
    , manualEQ_ ([this](uint32_t id, double v) { setParam(id, v); })
    , manualComp_([this](uint32_t id, double v) { setParam(id, v); })
    , manualSat_ ([this](uint32_t id, double v) { setParam(id, v); })
    , manualRev_ ([this](uint32_t id, double v) { setParam(id, v); })
    , ctxLenSlider_(Slider::LinearHorizontal, Slider::NoTextBox)
{
  setSize(kW, kTotalH);

  juce::LookAndFeel::setDefaultLookAndFeel(&laf_);

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
        const auto seconds = ctxLenSlider_.getValue();
        setParam(5, seconds);
        ctxLenLabel_.setText("Ctx target: " + String(static_cast<int>(seconds)) + "s", dontSendNotification);
        contextMeter_.setContextLengthSeconds(static_cast<float>(seconds));
    };
    addAndMakeVisible(ctxLenSlider_);

    ctxLenLabel_.setText("Ctx target: 60s", dontSendNotification);
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
}

PluginEditor::~PluginEditor() {
    juce::Timer::stopTimer();
    setLookAndFeel(nullptr);
}

void PluginEditor::resized() {
    headerBar_ .setBounds(0, kHeaderY, kW, kHeaderH);
    contextMeter_.setBounds(0, kContextY, kW, kContextH);
    controlBar_ .setBounds(0, kControlY, kW, kControlH);

    int x = 0;
    eqPanel_ .setBounds(x, kAISectY, kEqW, kAISectH); x += kEqW;
    compPanel_.setBounds(x, kAISectY, kCompW, kAISectH); x += kCompW;
    satPanel_ .setBounds(x, kAISectY, kSatW, kAISectH); x += kSatW;
    revPanel_ .setBounds(x, kAISectY, kRevW, kAISectH);

    manualBar_.setBounds(0, kManDivY, kW, kManDivH);

    x = 0;
    manualEQ_ .setBounds(x, kManSectY, kEqW, kManSectH); x += kEqW;
    manualComp_.setBounds(x, kManSectY, kCompW, kManSectH); x += kCompW;
    manualSat_ .setBounds(x, kManSectY, kSatW, kManSectH); x += kSatW;
    manualRev_ .setBounds(x, kManSectY, kRevW, kManSectH);

    // Bottom bar layout
    const int by = kBottomY;
    const int bh = kBottomH;
    const int pad = 8;
    const int btnW = 80;
    const int lblW = 50;
    const int slW = kW - pad * 4 - lblW - btnW * 2;

    ctxLenLabel_ .setBounds(pad, by, lblW, bh);
    ctxLenSlider_.setBounds(pad + lblW, by + 8, slW, bh - 16);
    resetButton_ .setBounds(pad + lblW + slW + pad, by + 8, btnW, bh - 16);
    loadButton_ .setBounds(pad + lblW + slW + pad + btnW + pad, by + 8, btnW, bh - 16);
}

void PluginEditor::paint(Graphics& g) {
    g.fillAll(MambaColors::background);

    g.setColour(MambaColors::border);
    g.fillRect(0, kBottomY, kW, 1);

    // Model-not-loaded error overlay
    if (showModelOverlay_) {
        g.setColour(MambaColors::background.withAlpha(0.85f));
        g.fillRect(getLocalBounds());

        const int cx = kW / 2;
        const int cy = kTotalH / 2;

        g.setColour(MambaColors::red);
        g.setFont(Font(FontOptions(16.f).withStyle("Bold")));
        g.drawText("NO MODEL LOADED", cx - 140, cy - 40, 280, 24, Justification::centred);

 g.setColour(MambaColors::textSecondary);
 g.setFont(Font(FontOptions(12.f)));
 g.drawText("Set MAMBA_MODEL_PATH env var or click LOAD MODEL. Switch to Manual mode for DSP without model.",
 cx - 240, cy - 14, 480, 18, Justification::centred);
    }
}

void PluginEditor::timerCallback() {
  const float dt = kTimerIntervalMs / 1000.f;

  processor_.tickParamGuiSuppress();

  // 1. Context meter
  contextMeter_.setFillRatio(processor_.getContextFillRatio());
  contextMeter_.setContextLengthSeconds(
    static_cast<float>(processor_.getParamValue(5)));
  contextMeter_.tick(dt);
  contextMeter_.repaint();

  // 2. Header state
  const bool active = processor_.isInferenceActive();
  const bool loaded = processor_.isModelLoaded();
  const bool degraded = processor_.isDegraded();
  const bool cpuOver = processor_.isCpuOverloaded();
  headerBar_.setInferenceActive(active);
  headerBar_.setModelLoaded(loaded);
  headerBar_.setDegraded(degraded);
  headerBar_.setCpuOverloaded(cpuOver);
  contextMeter_.setInferenceActive(active);

  // 3. Level meters
  controlBar_.setPeakL(processor_.getPeakL());
  controlBar_.setPeakR(processor_.getPeakR());
  controlBar_.tickMeter(dt);

  // 3b. ControlBar sync — suppress params user just changed
  controlBar_.setModeValue(static_cast<int>(processor_.getParamValue(1)));
  controlBar_.setEffectTypeValue(static_cast<int>(processor_.getParamValue(12)));
  controlBar_.setBypass(processor_.getParamValue(2) >= 0.5);
  if (paramSuppress_[0] == 0) controlBar_.setDryWet(processor_.getParamValue(0));
  if (paramSuppress_[3] == 0) controlBar_.setInputGain(processor_.getParamValue(3));
  if (paramSuppress_[4] == 0) controlBar_.setOutputGain(processor_.getParamValue(4));

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
  eqPanel_ .updateAIValues(vals + kEqStart, 6, manualMode);
  compPanel_.updateAIValues(vals + kCompStart, 6, manualMode);
  satPanel_ .updateAIValues(vals + kSatStart, 3, manualMode);
  revPanel_ .updateAIValues(vals + kRevStart, 4, manualMode);

  // 6. Wet sliders sync — suppress if user just changed
  if (paramSuppress_[6] == 0) eqPanel_.wetSlider().setValue(processor_.getParamValue(6), dontSendNotification);
  if (paramSuppress_[7] == 0) compPanel_.wetSlider().setValue(processor_.getParamValue(7), dontSendNotification);
  if (paramSuppress_[8] == 0) satPanel_.wetSlider().setValue(processor_.getParamValue(8), dontSendNotification);
  if (paramSuppress_[9] == 0) revPanel_.wetSlider().setValue(processor_.getParamValue(9), dontSendNotification);

  // 7. Manual panels — sync slider positions from processor (suppress user-touched)
  const bool eqManual = processor_.getParamValue(14) >= 0.5;
  const bool compManual = processor_.getParamValue(15) >= 0.5;
  const bool satManual = processor_.getParamValue(16) >= 0.5;
  const bool revManual = processor_.getParamValue(17) >= 0.5;

  manualEQ_.syncFromProcessor(eqManual,
    processor_.getParamValue(18),
    processor_.getParamValue(19),
    processor_.getParamValue(20),
    processor_.getParamValue(21),
    processor_.getParamValue(22),
    processor_.getParamValue(23),
    processor_.getParamValue(24),
    paramSuppress_);

  manualComp_.syncFromProcessor(compManual,
    processor_.getParamValue(10),
    processor_.getParamValue(25),
    processor_.getParamValue(26),
    processor_.getParamValue(27),
    processor_.getParamValue(28),
    paramSuppress_);

  manualSat_.syncFromProcessor(satManual,
    processor_.getParamValue(29),
    processor_.getParamValue(30),
    paramSuppress_);

  manualRev_.syncFromProcessor(revManual,
    processor_.getParamValue(11),
    processor_.getParamValue(31),
    processor_.getParamValue(32),
    paramSuppress_);

  // 8. Ctx length label
  const double ctxSecs = processor_.getParamValue(5);
  if (paramSuppress_[5] == 0)
    ctxLenSlider_.setValue(ctxSecs, dontSendNotification);
  ctxLenLabel_.setText(String("Ctx: ") + String(static_cast<int>(ctxSecs)) + String("s"),
    dontSendNotification);

  // 9. Model overlay visibility — only show in AI mode without a model
  const bool wasShowing = showModelOverlay_;
  showModelOverlay_ = !loaded && !manualMode;
  if (showModelOverlay_ != wasShowing)
    repaint();

  repaint();
}

void PluginEditor::setParam(uint32_t paramId, double value) {
  processor_.setParamValue(paramId, value);
  processor_.markParamGuiOrigin(paramId);
  if (hostParams_ && host_)
    hostParams_->request_flush(host_);
}
