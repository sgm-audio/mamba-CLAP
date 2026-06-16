#include "ARAIntelligenceDocumentController.h"
#include <juce_gui_basics/juce_gui_basics.h>

namespace ARAIntelligence {

juce::AudioProcessorEditor* PluginProcessor::createEditor()
{
    return new juce::GenericAudioProcessorEditor(*this);
}

} // namespace ARAIntelligence
