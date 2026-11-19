#include "PluginEditor.h"

using namespace kluay;

namespace
{
constexpr int kW = 560, kH = 858, kX = 24, kCW = 512;
const char* kHintDefault = "Tap anything below. It plays the new sound straight away.";
const char* kHintNoVocal = "Drop a vocal file on the waveform, then drag over one word.";
const char* kHintPick    = "Drag over a word (or double-click to snap). It plays straight away.";
}

KluayEditor::KluayEditor (KluayProcessor& p)
    : juce::AudioProcessorEditor (&p), proc (p), wave (p),
      beforePill ("BEFORE"), afterPill ("AFTER"), voxPill ("VOX ONLY"), beatPill ("WITH BEAT"),
      repeatsSeg (juce::StringArray { "2", "4", "8" }), rateSeg (juce::StringArray { "1/16", "1/8" }),
      fadeCard ("FADE IN", &lnf), panCard ("PAN", &lnf),
      phoneCard ("PHONE"), chorusCard ("CHORUS"), backCard ("BACKVERB")
{
    setSize (kW, kH);

    for (juce::Component* c : std::initializer_list<juce::Component*> {
             &wave, &previewBtn, &beforePill, &afterPill, &voxPill, &beatPill, &abSwitch, &beatSwitch,
             &repeatsSeg, &rateSeg, &fadeCard, &panCard, &phoneCard, &chorusCard, &backCard })
        addAndMakeVisible (c);

    // ---- parameter bindings
    auto makeAtt = [this] (const char* id, std::function<void (float)> cb)
    {
        auto a = std::make_unique<juce::ParameterAttachment> (*proc.apvts.getParameter (id), std::move (cb), nullptr);
        a->sendInitialUpdate();
        return a;
    };
    repeatsAtt = makeAtt ("repeats", [this] (float v) { repeatsSeg.setSelected ((int) std::lround (v)); });
    rateAtt    = makeAtt ("rate",    [this] (float v) { rateSeg.setSelected ((int) std::lround (v)); });
    phoneAtt   = makeAtt ("phone",   [this] (float v) { phoneCard.setOn (v > 0.5f); });
    chorusAtt  = makeAtt ("chorus",  [this] (float v) { chorusCard.setOn (v > 0.5f); });
    backAtt    = makeAtt ("backverb",[this] (float v) { backCard.setOn (v > 0.5f); });

    fadeAtt = std::make_unique<juce::SliderParameterAttachment> (*proc.apvts.getParameter ("fade"), fadeCard.slider, nullptr);
    panAtt  = std::make_unique<juce::SliderParameterAttachment> (*proc.apvts.getParameter ("pan"), panCard.slider, nullptr);
    fadeCard.slider.setDoubleClickReturnValue (true, 0.6);
    panCard.slider.setDoubleClickReturnValue (true, 0.6);

    repeatsSeg.onSelect = [this] (int i) { repeatsAtt->setValueAsCompleteGesture ((float) i); tweaked (true); };
    rateSeg.onSelect    = [this] (int i) { rateAtt->setValueAsCompleteGesture ((float) i); tweaked (true); };
    phoneCard.onToggle  = [this] (bool v) { phoneAtt->setValueAsCompleteGesture (v ? 1.0f : 0.0f); tweaked (true); };
    chorusCard.onToggle = [this] (bool v) { chorusAtt->setValueAsCompleteGesture (v ? 1.0f : 0.0f); tweaked (true); };
    backCard.onToggle   = [this] (bool v) { backAtt->setValueAsCompleteGesture (v ? 1.0f : 0.0f); tweaked (true); };

    for (auto* k : { &fadeCard, &panCard })
    {
        k->slider.onDragStart = [this] { proc.showAfter = true; };
        k->slider.onDragEnd = [this] { tweaked (true); };
    }

    // ---- hear the difference
    auto ensurePlaying = [this] { if (! proc.isPlaying()) proc.startPreview(); wave.setView (WaveformPanel::View::Result); };
    beforePill.onClick = [this, ensurePlaying] { proc.showAfter = false; ensurePlaying(); };
    afterPill.onClick  = [this, ensurePlaying] { proc.showAfter = true;  ensurePlaying(); };
    voxPill.onClick    = [this, ensurePlaying] { proc.withBeat = false;  ensurePlaying(); };
    beatPill.onClick   = [this, ensurePlaying] { proc.withBeat = true;   ensurePlaying(); };
    abSwitch.onToggle   = [this, ensurePlaying] (bool on) { proc.showAfter = on; ensurePlaying(); };
    beatSwitch.onToggle = [this, ensurePlaying] (bool on) { proc.withBeat = on;  ensurePlaying(); };

    // ---- preview + waveform
    previewBtn.onClick = [this]
    {
        if (! proc.hasVocal()) { openChooser(); return; }
        if (proc.isPlaying()) proc.stopPreview();
        else { proc.startPreview(); wave.setView (WaveformPanel::View::Result); }
    };
    wave.onRequestLoad = [this] { openChooser(); };
    wave.onFileDropped = [this] (const juce::File& f) { loadFile (f); };
    wave.onSelectionCommitted = [this] { tweaked (true); };

    hint = proc.hasVocal() ? kHintDefault : kHintNoVocal;
    if (proc.hasVocal()) wave.setView (WaveformPanel::View::Source);

    startTimerHz (30);
}

KluayEditor::~KluayEditor()
{
    stopTimer();
}

void KluayEditor::tweaked (bool restart)
{
    proc.showAfter = true;
    if (restart)
    {
        proc.startPreview();
        wave.setView (WaveformPanel::View::Result);
    }
    else
        proc.requestRender();
}

void KluayEditor::openChooser()
{
    chooser = std::make_unique<juce::FileChooser> ("Choose a vocal", juce::File(), "*.wav;*.aif;*.aiff;*.flac;*.mp3;*.ogg");
    juce::Component::SafePointer<KluayEditor> safe (this);
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                          [safe] (const juce::FileChooser& fc)
                          {
                              auto f = fc.getResult();
                              if (safe != nullptr && f.existsAsFile()) safe->loadFile (f);
                          });
}

void KluayEditor::loadFile (const juce::File& f)
{
    proc.stopPreview();
    if (proc.loadVocal (f, true))
    {
        wave.setView (WaveformPanel::View::Source);
        hint = kHintPick;
    }
    else
        hint = "Could not read that file. Try WAV, AIFF, FLAC or MP3.";
    repaint();
}

void KluayEditor::timerCallback()
{
    proc.renderIfNeeded();
    previewBtn.setPlaying (proc.isPlaying());

    const bool after = proc.showAfter.load(), beat = proc.withBeat.load();
    abSwitch.setOn (after);
    beatSwitch.setOn (beat);
    beforePill.setActive (! after);
    afterPill.setActive (after);
    voxPill.setActive (! beat);
    beatPill.setActive (beat);

    wave.tick();

    juce::String h = hint;
    if (! proc.hasVocal()) h = kHintNoVocal;
    else if (h == kHintNoVocal) h = kHintDefault;
    if (h != hint) { hint = h; repaint (kX, 432, kCW, 34); }
}

void KluayEditor::resized()
{
    wave.setBounds (kX, 92, kCW, 118);
    previewBtn.setBounds (kX, 220, kCW, 50);

    const int pw = 128, sw = 64, gap = 4, rowH = 44;
    const int x0 = (kW - (pw * 2 + sw + gap * 2)) / 2;
    auto row = [&] (int y, PillButton& l, SwitchComp& s, PillButton& r)
    {
        l.setBounds (x0, y, pw, rowH);
        s.setBounds (x0 + pw + gap, y, sw, rowH);
        r.setBounds (x0 + pw + gap + sw + gap, y, pw, rowH);
    };
    row (318, beforePill, abSwitch, afterPill);
    row (364, voxPill, beatSwitch, beatPill);

    repeatsSeg.setBounds (kX, 516, 252, 54);
    rateSeg.setBounds (kX + 260, 516, 252, 54);
    fadeCard.setBounds (kX, 604, 252, 100);
    panCard.setBounds (kX + 260, 604, 252, 100);

    const int ew = 165;
    phoneCard.setBounds (kX, 742, ew, 92);
    chorusCard.setBounds (kX + ew + 9, 742, ew, 92);
    backCard.setBounds (kX + (ew + 9) * 2, 742, ew + 1, 92);
}

void KluayEditor::sectionLabel (juce::Graphics& g, const juce::String& t, int y)
{
    const auto f = boldFont (11.0f, 0.3f);
    g.setColour (col::muted);
    g.setFont (f);
    const int tw = (int) f.getStringWidthFloat (t) + 4;
    g.drawText (t, kX, y, tw, 16, juce::Justification::centredLeft);
    g.setColour (col::borderDim);
    g.fillRect ((float) (kX + tw + 12), (float) y + 8.0f, (float) (kCW - tw - 12), 1.0f);
}

void KluayEditor::paintLogo (juce::Graphics& g)
{
    const int iconX = kW / 2 - 128, iconY = 16, iconS = 60;
    juce::Rectangle<float> icon ((float) iconX, (float) iconY, (float) iconS, (float) iconS);
    drawGlowRect (g, icon, 10.0f, juce::Colour (0xff07182a), col::accent2.withAlpha (0.6f), 0.6f, 12);

    const float heights[] = { 0.30f, 0.62f, 1.0f, 0.55f, 0.82f, 0.40f, 0.22f };
    const float bw = 4.0f, bg = 3.5f;
    const float total = 7 * bw + 6 * bg;
    float bx = icon.getCentreX() - total * 0.5f;
    for (float h : heights)
    {
        const float bh = h * 38.0f;
        juce::Rectangle<float> bar (bx, icon.getCentreY() - bh * 0.5f, bw, bh);
        juce::Path p;
        p.addRoundedRectangle (bar, 2.0f);
        juce::DropShadow (col::accent.withAlpha (0.7f), 5, juce::Point<int> (0, 0)).drawForPath (g, p);
        g.setColour (col::accent);
        g.fillRoundedRectangle (bar, 2.0f);
        bx += bw + bg;
    }

    g.setColour (juce::Colours::white);
    g.setFont (boldFont (36.0f, 0.22f));
    g.drawText ("KLUAY", iconX + iconS + 16, iconY - 2, 210, 42, juce::Justification::centredLeft);
    g.setColour (col::muted);
    g.setFont (boldFont (9.5f, 0.2f));
    g.drawText ("VOCAL STUTTERS. FASTER.", iconX + iconS + 18, iconY + 40, 210, 14, juce::Justification::centredLeft);
}

void KluayEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff030812));

    auto card = juce::Rectangle<float> (8.0f, 8.0f, (float) kW - 16.0f, (float) kH - 16.0f);
    juce::Path cp;
    cp.addRoundedRectangle (card, 22.0f);
    juce::DropShadow (col::accent2.withAlpha (0.35f), 8, juce::Point<int> (0, 0)).drawForPath (g, cp);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff0c2038), 0.0f, 8.0f, juce::Colour (0xff050d1a), 0.0f, (float) kH, false));
    g.fillRoundedRectangle (card, 22.0f);
    g.setColour (col::border.withAlpha (0.8f));
    g.drawRoundedRectangle (card, 22.0f, 1.3f);

    paintLogo (g);

    // hear the difference box
    juce::Rectangle<float> box ((float) kX, 280.0f, (float) kCW, 140.0f);
    drawGlowRect (g, box, 16.0f, col::cardFill, col::border, 0.3f, 6);
    g.setColour (col::accent);
    g.setFont (boldFont (11.5f, 0.28f));
    g.drawText ("HEAR THE DIFFERENCE", kX, 290, kCW, 18, juce::Justification::centred);

    // hint bar
    juce::Rectangle<float> hb ((float) kX, 432.0f, (float) kCW, 34.0f);
    g.setColour (juce::Colour (0xff08141f));
    g.fillRoundedRectangle (hb, 10.0f);
    juce::Path hp, dashed;
    hp.addRoundedRectangle (hb, 10.0f);
    const float dashes[] = { 4.0f, 3.0f };
    juce::PathStrokeType (1.0f).createDashedStroke (dashed, hp, dashes, 2);
    g.setColour (col::border);
    g.strokePath (dashed, juce::PathStrokeType (1.0f));
    g.setColour (col::accent);
    g.fillEllipse ((float) kX + 20.0f, 432.0f + 12.0f, 10.0f, 10.0f);
    g.setColour (col::text);
    g.setFont (boldFont (12.5f, 0.02f));
    g.drawText (hint, kX + 42, 432, kCW - 54, 34, juce::Justification::centredLeft);

    sectionLabel (g, "STUTTER", 480);
    g.setColour (col::dim);
    g.setFont (boldFont (10.0f, 0.25f));
    g.drawText ("REPEATS", kX, 499, 252, 14, juce::Justification::centred);
    g.drawText ("RATE", kX + 260, 499, 252, 14, juce::Justification::centred);

    sectionLabel (g, "SHAPE", 584);
    sectionLabel (g, "EFFECTS", 720);
}
