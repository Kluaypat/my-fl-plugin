#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_dsp/juce_dsp.h>
#include <atomic>
#include <utility>
#include <vector>
#include "StutterEngine.h"

class KluayProcessor : public juce::AudioProcessor,
                       private juce::AudioProcessorValueTreeState::Listener
{
public:
    using APVTS = juce::AudioProcessorValueTreeState;

    KluayProcessor();
    ~KluayProcessor() override;

    // ---- AudioProcessor ----
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "KLUAY"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // ---- parameters ----
    APVTS apvts;

    // ---- vocal / word selection (message thread) ----
    bool loadVocal (const juce::File& file, bool autoSelect = true);
    bool hasVocal() const { return vocalLoaded.load(); }
    juce::String getVocalName() const { return vocalFile.getFileName(); }
    double getVocalLengthSeconds() const;
    void setSelection (double startNorm, double endNorm);
    std::pair<double, double> getSelection() const { return { selStart, selEnd }; }
    std::pair<double, double> findSegmentAt (double norm) const;
    std::vector<float> getSourcePeaks (int cols) const;
    int getSourceVersion() const { return sourceVersion.load(); }

    // ---- preview ----
    void requestRender() { renderPending = true; }
    void renderIfNeeded();
    void startPreview();
    void stopPreview() { playing = false; }
    bool isPlaying() const { return playing.load(); }
    bool hasResult() const { return currentLength.load() > 0; }
    float getPlayProgress() const;
    double getCurrentBpm() const;
    int getRenderVersion() const { return renderVersion.load(); }
    std::vector<float> getResultPeaks (bool after, bool beat, int cols) const;
    bool exportWav (const juce::File& dest, bool includeBeat) const;

    std::atomic<bool> showAfter { false };
    std::atomic<bool> withBeat { true };

private:
    void parameterChanged (const juce::String&, float) override { renderPending = true; }
    void renderNow();
    juce::AudioBuffer<float> extractWord (double sr) const;
    void analyseSegments();
    static APVTS::ParameterLayout createLayout();

    juce::AudioFormatManager formatManager;
    juce::AudioBuffer<float> vocal;
    double vocalRate = 44100.0;
    juce::File vocalFile;
    std::atomic<bool> vocalLoaded { false };
    std::atomic<int> sourceVersion { 0 };
    double selStart = 0.0, selEnd = 1.0;
    std::vector<std::pair<double, double>> segments;

    mutable juce::CriticalSection renderLock;
    kluay::RenderResult current;
    std::atomic<int> currentLength { 0 };
    std::atomic<int> renderVersion { 0 };
    std::atomic<int> playPos { 0 };
    std::atomic<bool> playing { false };
    std::atomic<bool> renderPending { true };
    std::atomic<double> currentSampleRate { 44100.0 };
    std::atomic<double> hostBpm { 0.0 };
    double lastRenderedBpm = 0.0, lastRenderedSr = 0.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (KluayProcessor)
};
