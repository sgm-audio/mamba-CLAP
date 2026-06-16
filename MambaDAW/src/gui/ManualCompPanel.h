#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <cstdint>
#include <functional>

class ManualCompPanel : public juce::Component {
public:
    using ParamCallback = std::function<void(uint32_t, double)>;

    explicit ManualCompPanel(ParamCallback cb);

  void syncFromProcessor(bool enabled, double thresh, double ratio,
  double attack, double release, double makeup,
  const int* paramSuppress = nullptr);

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    ParamCallback onParam_;
    juce::TextButton toggleBtn_;

    struct Row {
        juce::Label  label;
        juce::Slider slider{ juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight };
    };
    Row rows_[5];  // thresh, ratio, attack, release, makeup

    bool enabled_ = false;
    void updateToggleLabel();
};
