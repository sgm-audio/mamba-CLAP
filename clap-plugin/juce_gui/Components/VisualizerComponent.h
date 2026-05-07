#ifndef VISUALIZER_COMPONENT_H_INCLUDED
#define VISUALIZER_COMPONENT_H_INCLUDED

#include <JuceHeader.h>
#include "../MambaLookAndFeel.h"

class VisualizerComponent : public juce::Component,
                            public juce::OpenGLRenderer
{
public:
    VisualizerComponent();
    ~VisualizerComponent() override;

    // Component overrides
    void paint (juce::Graphics&) override;
    void resized() override;

    // OpenGLRenderer overrides
    void newOpenGLContextCreated() override;
    void renderOpenGL() override;
    void openGLContextClosing() override;

    // Data push methods
    void pushAudioBuffer (const float* buffer, int numSamples);
    void pushMambaState (const float* state, int stateDim);

private:
    juce::OpenGLContext openGLContext;
    
    // Shader programs and buffers for spectrogram
    std::unique_ptr<juce::OpenGLShaderProgram> shader;
    GLuint vertexBuffer, indexBuffer;
    
    // Abstract pulsing state visualizer
    float statePulseIntensity = 0.0f;
    juce::LinearSmoothedValue<float> smoothedPulse;

    void updateStateVisualization();

    JU_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VisualizerComponent)
};

#endif // VISUALIZER_COMPONENT_H_INCLUDED
