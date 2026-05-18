#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

// Thin divider bar between the AI read-only section and the manual controls section.
class ManualControlsBar : public juce::Component {
public:
    ManualControlsBar();

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    juce::Label titleLabel_;
    juce::Label hintLabel_;
};
