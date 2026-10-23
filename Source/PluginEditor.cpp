#include "PluginEditor.h"

namespace
{
const juce::Colour kBody   { 0xff22262c };
const juce::Colour kBody2  { 0xff15181c };
const juce::Colour kOrange { 0xffff7a1a };
const juce::Colour kLight  { 0xffe6e6e0 };
}

FZLookAndFeel::FZLookAndFeel()
{
    setColour (juce::Label::textColourId, kLight);
    setColour (juce::ComboBox::backgroundColourId, kBody2);
    setColour (juce::ComboBox::textColourId, kLight);
    setColour (juce::ComboBox::outlineColourId, kOrange.withAlpha (0.7f));
    setColour (juce::ComboBox::arrowColourId, kOrange);
    setColour (juce::PopupMenu::backgroundColourId, kBody2);
    setColour (juce::PopupMenu::textColourId, kLight);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, kOrange.withAlpha (0.35f));
    setColour (juce::Slider::trackColourId, kOrange);
    setColour (juce::Slider::thumbColourId, kLight);
    setColour (juce::Slider::backgroundColourId, kBody2);
    setColour (juce::TextEditor::textColourId, kLight);
    setColour (juce::TextEditor::backgroundColourId, kBody2);
}

void FZLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                      float startAngle, float endAngle, juce::Slider&)
{
    const auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (6.0f);
    const float radius = std::min (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto centre = bounds.getCentre();
    const float angle = startAngle + pos * (endAngle - startAngle);

    juce::Path track, value;
    track.addCentredArc (centre.x, centre.y, radius, radius, 0.0f, startAngle, endAngle, true);
    value.addCentredArc (centre.x, centre.y, radius, radius, 0.0f, startAngle, angle, true);
    g.setColour (kLight.withAlpha (0.18f));
    g.strokePath (track, juce::PathStrokeType (3.0f));
    g.setColour (kOrange);
    g.strokePath (value, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    const float kr = radius - 9.0f;
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff4a4f57), centre.x - kr, centre.y - kr,
                                             juce::Colour (0xff0e1013), centre.x + kr, centre.y + kr, false));
    g.fillEllipse (centre.x - kr, centre.y - kr, kr * 2.0f, kr * 2.0f);
    g.setColour (juce::Colour (0xff6a707a));
    g.drawEllipse (centre.x - kr, centre.y - kr, kr * 2.0f, kr * 2.0f, 1.5f);

    juce::Path ptr;
    ptr.addRoundedRectangle (-2.0f, -kr + 3.0f, 4.0f, kr * 0.5f, 1.5f);
    g.setColour (kOrange);
    g.fillPath (ptr, juce::AffineTransform::rotation (angle).translated (centre.x, centre.y));
}

void FZLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool highlighted, bool)
{
    const auto area = b.getLocalBounds().toFloat();
    const float d = std::min (area.getWidth() * 0.6f, area.getHeight() - 30.0f);
    const auto sw = juce::Rectangle<float> (d, d).withCentre ({ area.getCentreX(), area.getY() + 22.0f + d * 0.5f });

    const bool on = b.getToggleState();
    const juce::Rectangle<float> led (area.getCentreX() - 6.0f, area.getY() + 2.0f, 12.0f, 12.0f);
    if (on)
    {
        g.setColour (kOrange.withAlpha (0.35f));
        g.fillEllipse (led.expanded (5.0f));
    }
    g.setColour (on ? kOrange : juce::Colour (0xff4a2a12));
    g.fillEllipse (led);

    g.setGradientFill (juce::ColourGradient (juce::Colour (0xffd2d2d2), sw.getX(), sw.getY(),
                                             juce::Colour (0xff6c6c6c), sw.getRight(), sw.getBottom(), false));
    g.fillEllipse (sw);
    g.setColour (highlighted ? kOrange : juce::Colour (0xff1d1d1d));
    g.drawEllipse (sw, 2.0f);

    g.setColour (kLight);
    g.setFont (juce::FontOptions (13.0f, juce::Font::bold));
    g.drawText (b.getButtonText(), b.getLocalBounds().removeFromBottom (18), juce::Justification::centred);
}

FZ2Editor::FZ2Editor (FZ2AudioProcessor& p)
    : juce::AudioProcessorEditor (&p), proc (p)
{
    setLookAndFeel (&lnf);
    setSize (640, 470);

    addKnob (gain,   "gain",   "GAIN");
    addKnob (bass,   "bass",   "BASS");
    addKnob (treble, "treble", "TREBLE");
    addKnob (level,  "level",  "LEVEL");
    addKnob (octave, "octave", "OCTAVE (extra)");

    addAndMakeVisible (onSwitch);
    onAtt = std::make_unique<ButtonAtt> (proc.apvts, "on", onSwitch);

    modeBox.addItemList ({ "Fuzz I", "Fuzz II", "Boost", "Fuzz I + II" }, 1);
    addAndMakeVisible (modeBox);
    modeAtt = std::make_unique<ComboAtt> (proc.apvts, "mode", modeBox);

    for (auto* l : { &modeLabel, &inLabel, &outLabel })
    {
        l->setJustificationType (juce::Justification::centredLeft);
        l->setFont (juce::FontOptions (12.0f, juce::Font::bold));
        addAndMakeVisible (l);
    }
    modeLabel.setText ("MODE", juce::dontSendNotification);
    inLabel.setText ("INPUT", juce::dontSendNotification);
    outLabel.setText ("OUTPUT", juce::dontSendNotification);

    for (auto* s : { &inSlider, &outSlider })
    {
        s->setSliderStyle (juce::Slider::LinearHorizontal);
        s->setTextBoxStyle (juce::Slider::TextBoxRight, false, 60, 20);
        s->setTextValueSuffix (" dB");
        addAndMakeVisible (s);
    }
    inAtt  = std::make_unique<SliderAtt> (proc.apvts, "input", inSlider);
    outAtt = std::make_unique<SliderAtt> (proc.apvts, "output", outSlider);
}

FZ2Editor::~FZ2Editor() { setLookAndFeel (nullptr); }

void FZ2Editor::addKnob (Knob& k, const juce::String& id, const juce::String& text)
{
    k.slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    k.slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    k.slider.setRotaryParameters (juce::degreesToRadians (225.0f), juce::degreesToRadians (495.0f), true);
    k.slider.setDoubleClickReturnValue (true, proc.apvts.getParameter (id)->convertFrom0to1 (
                                                  proc.apvts.getParameter (id)->getDefaultValue()));
    k.label.setText (text, juce::dontSendNotification);
    k.label.setJustificationType (juce::Justification::centred);
    k.label.setFont (juce::FontOptions (12.5f, juce::Font::bold));
    addAndMakeVisible (k.slider);
    addAndMakeVisible (k.label);
    k.att = std::make_unique<SliderAtt> (proc.apvts, id, k.slider);
}

void FZ2Editor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff0b0c0e));
    const auto body = getLocalBounds().toFloat().reduced (8.0f);
    g.setGradientFill (juce::ColourGradient (kBody.brighter (0.06f), 0, 0, kBody2, 0, (float) getHeight(), false));
    g.fillRoundedRectangle (body, 12.0f);
    g.setColour (kOrange);
    g.drawRoundedRectangle (body, 12.0f, 2.5f);

    g.setColour (kLight);
    g.setFont (juce::FontOptions (30.0f, juce::Font::bold));
    g.drawText ("FZ-2 SIM", getLocalBounds().removeFromTop (62).withTrimmedTop (14), juce::Justification::centred);
    g.setColour (kOrange);
    g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
    g.drawText ("octave fuzz  -  Fuzz I / Fuzz II / Boost", getLocalBounds().removeFromTop (82).withTrimmedTop (62),
                juce::Justification::centred);
    g.setColour (kLight.withAlpha (0.5f));
    g.setFont (juce::FontOptions (11.0f));
    g.drawText ("unofficial emulation, not affiliated with any manufacturer",
                getLocalBounds().removeFromBottom (22), juce::Justification::centred);
}

void FZ2Editor::resized()
{
    auto r = getLocalBounds().reduced (24);
    r.removeFromTop (76);

    auto row = r.removeFromTop (140);
    const int w = row.getWidth() / 5;
    for (auto* k : { &gain, &bass, &treble, &level, &octave })
    {
        auto cell = row.removeFromLeft (w);
        k->label.setBounds (cell.removeFromBottom (20));
        k->slider.setBounds (cell);
    }

    r.removeFromTop (6);
    auto mid = r.removeFromTop (104);
    auto left = mid.removeFromLeft (mid.getWidth() / 2);
    {
        auto m = left.withSizeKeepingCentre (200, 30);
        modeLabel.setBounds (m.removeFromLeft (50));
        modeBox.setBounds (m);
    }
    onSwitch.setBounds (mid.withSizeKeepingCentre (130, 104));

    r.removeFromTop (6);
    auto bottom = r.removeFromTop (30);
    auto half = bottom.removeFromLeft (bottom.getWidth() / 2);
    inLabel.setBounds (half.removeFromLeft (52));
    inSlider.setBounds (half.reduced (4, 0));
    outLabel.setBounds (bottom.removeFromLeft (60));
    outSlider.setBounds (bottom);
}
