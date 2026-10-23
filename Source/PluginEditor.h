#pragma once
#include "PluginProcessor.h"

class FZLookAndFeel : public juce::LookAndFeel_V4
{
public:
    FZLookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool highlighted, bool down) override;
};

class FZ2Editor : public juce::AudioProcessorEditor
{
public:
    explicit FZ2Editor (FZ2AudioProcessor&);
    ~FZ2Editor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    using SliderAtt = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAtt = juce::AudioProcessorValueTreeState::ButtonAttachment;
    using ComboAtt  = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    struct Knob
    {
        juce::Slider slider;
        juce::Label label;
        std::unique_ptr<SliderAtt> att;
    };

    void addKnob (Knob&, const juce::String& id, const juce::String& text);

    FZ2AudioProcessor& proc;
    FZLookAndFeel lnf;

    Knob gain, bass, treble, level, octave;
    juce::ToggleButton onSwitch { "ON / OFF" };
    juce::ComboBox modeBox;
    juce::Label modeLabel, inLabel, outLabel;
    juce::Slider inSlider, outSlider;

    std::unique_ptr<ButtonAtt> onAtt;
    std::unique_ptr<ComboAtt> modeAtt;
    std::unique_ptr<SliderAtt> inAtt, outAtt;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FZ2Editor)
};
