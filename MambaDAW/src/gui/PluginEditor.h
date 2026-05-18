#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "HeaderBar.h"
#include "ContextMeter.h"
#include "ControlBar.h"
#include "SectionPanel.h"
#include "ManualControlsBar.h"
#include "ManualEQPanel.h"
#include "ManualCompPanel.h"
#include "ManualSatPanel.h"
#include "ManualRevPanel.h"
#include "MambaLookAndFeel.h"
#include <clap/clap.h>
#include <clap/ext/params.h>
#include <memory>

class PluginProcessor;

class PluginEditor : public juce::Component, private juce::Timer {
public:
    explicit PluginEditor(PluginProcessor& processor,
                          const clap_host_t* host = nullptr,
                          const clap_host_params_t* hostParams = nullptr);
    ~PluginEditor() override;

    void resized() override;
    void paint(juce::Graphics&) override;

    // Called by CLAP GUI bridge to start/stop the repaint timer.
    void startTimerHz(int hz) { juce::Timer::startTimerHz(hz); }
    void stopTimer()          { juce::Timer::stopTimer(); }

private:
    void timerCallback() override;
    void setParam(uint32_t paramId, double value);

    MambaLookAndFeel laf_;

    PluginProcessor& processor_;
    const clap_host_t*        host_       = nullptr;
    const clap_host_params_t* hostParams_ = nullptr;

    HeaderBar        headerBar_;
    ContextMeter     contextMeter_;
    ControlBar       controlBar_;

    // AI read-only display panels (top section)
    SectionPanel     eqPanel_;
    SectionPanel     compPanel_;
    SectionPanel     satPanel_;
    SectionPanel     revPanel_;

    // Manual controls section (always visible below AI display)
    ManualControlsBar manualBar_;
    ManualEQPanel     manualEQ_;
    ManualCompPanel   manualComp_;
    ManualSatPanel    manualSat_;
    ManualRevPanel    manualRev_;

    // Bottom bar controls (inline — no separate class).
    juce::Slider     ctxLenSlider_;
    juce::Label      ctxLenLabel_;
    juce::TextButton resetButton_{ "RESET" };
    juce::TextButton loadButton_{ "LOAD MODEL" };

    // Kept alive during async file-open dialog.
    std::unique_ptr<juce::FileChooser> chooser_;

    // Clip flash timer (milliseconds remaining).
    int clipFlashMs_{ 0 };
    static constexpr int kClipFlashDurationMs = 500;
    static constexpr int kTimerIntervalMs      = 33;  // ~30 Hz
};
