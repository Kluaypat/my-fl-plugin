#pragma once
#include "PluginProcessor.h"
#include "UIComponents.h"

class KluayEditor : public juce::AudioProcessorEditor,
                    private juce::Timer
{
public:
    explicit KluayEditor (KluayProcessor&);
    ~KluayEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void openChooser();
    void loadFile (const juce::File&);
    void tweaked (bool restart);
    void paintLogo (juce::Graphics&);
    void sectionLabel (juce::Graphics&, const juce::String&, int y);

    KluayProcessor& proc;
    kluay::KluayLookAndFeel lnf;

    kluay::WaveformPanel wave;
    kluay::PreviewButton previewBtn;
    kluay::PillButton beforePill, afterPill, voxPill, beatPill;
    kluay::SwitchComp abSwitch, beatSwitch;
    kluay::SegmentedGroup repeatsSeg, rateSeg;
    kluay::KnobCard fadeCard, panCard;
    kluay::EffectCard phoneCard, chorusCard, backCard;

    std::unique_ptr<juce::ParameterAttachment> repeatsAtt, rateAtt, phoneAtt, chorusAtt, backAtt;
    std::unique_ptr<juce::SliderParameterAttachment> fadeAtt, panAtt;
    std::unique_ptr<juce::FileChooser> chooser;

    juce::String hint;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (KluayEditor)
};
