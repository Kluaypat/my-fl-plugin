#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include <functional>
#include "PluginProcessor.h"

namespace kluay
{
namespace col
{
    inline const juce::Colour accent    { 0xff2cc3ff };
    inline const juce::Colour accent2   { 0xff1a8cff };
    inline const juce::Colour text      { 0xffeaf6ff };
    inline const juce::Colour muted     { 0xff7a93aa };
    inline const juce::Colour dim       { 0xff4d667c };
    inline const juce::Colour cardFill  { 0xff091829 };
    inline const juce::Colour cardFill2 { 0xff0c2640 };
    inline const juce::Colour border    { 0xff1b6288 };
    inline const juce::Colour borderDim { 0xff17405a };
    inline const juce::Colour dark      { 0xff040b15 };
}

inline juce::Font boldFont (float h, float kern = 0.0f)
{
    return juce::Font (h, juce::Font::bold).withExtraKerningFactor (kern);
}

inline void drawGlowRect (juce::Graphics& g, juce::Rectangle<float> r, float corner, juce::Colour fill,
                          juce::Colour border, float glowAlpha, int glowRadius)
{
    if (glowAlpha > 0.0f)
    {
        juce::Path p;
        p.addRoundedRectangle (r, corner);
        juce::DropShadow (border.withAlpha (glowAlpha), glowRadius, juce::Point<int> (0, 0)).drawForPath (g, p);
    }
    g.setColour (fill);
    g.fillRoundedRectangle (r, corner);
    g.setColour (border);
    g.drawRoundedRectangle (r, corner, 1.2f);
}

// ---------------------------------------------------------------- look and feel
class KluayLookAndFeel : public juce::LookAndFeel_V4
{
public:
    void drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height, float sliderPos,
                           float startAngle, float endAngle, juce::Slider&) override
    {
        auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) width, (float) height).reduced (4.0f);
        const float r = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
        const auto c = bounds.getCentre();
        const float angle = startAngle + sliderPos * (endAngle - startAngle);
        const float arcR = r - 2.0f;

        juce::Path track;
        track.addCentredArc (c.x, c.y, arcR, arcR, 0.0f, startAngle, endAngle, true);
        g.setColour (juce::Colour (0xff3b4b5c));
        g.strokePath (track, juce::PathStrokeType (3.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        juce::Path val;
        val.addCentredArc (c.x, c.y, arcR, arcR, 0.0f, startAngle, angle, true);
        juce::DropShadow (col::accent.withAlpha (0.6f), 6, juce::Point<int> (0, 0)).drawForPath (g, val);
        g.setColour (col::accent);
        g.strokePath (val, juce::PathStrokeType (3.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        const float kr = r - 9.0f;
        g.setColour (juce::Colour (0xff03080f));
        g.fillEllipse (c.x - kr, c.y - kr, kr * 2.0f, kr * 2.0f);
        g.setColour (juce::Colour (0xff16293d));
        g.drawEllipse (c.x - kr, c.y - kr, kr * 2.0f, kr * 2.0f, 1.0f);

        const auto p1 = c.getPointOnCircumference (kr * 0.45f, angle);
        const auto p2 = c.getPointOnCircumference (kr * 0.95f, angle);
        g.setColour (juce::Colours::white);
        g.drawLine (juce::Line<float> (p1, p2), 2.0f);
    }
};

// ---------------------------------------------------------------- pill button
class PillButton : public juce::Component
{
public:
    explicit PillButton (juce::String t) : text (std::move (t)) { setMouseCursor (juce::MouseCursor::PointingHandCursor); }
    void setActive (bool a) { if (active != a) { active = a; repaint(); } }
    std::function<void()> onClick;

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().reduced (6.0f);
        const float cr = r.getHeight() * 0.5f;
        if (active)
            drawGlowRect (g, r, cr, col::cardFill2, col::accent, 0.55f, 6);
        else
        {
            g.setColour (col::cardFill);
            g.fillRoundedRectangle (r, cr);
            g.setColour (col::borderDim);
            g.drawRoundedRectangle (r, cr, 1.0f);
        }
        g.setColour (active ? col::text : col::dim);
        g.setFont (boldFont (13.0f, 0.12f));
        g.drawText (text, r, juce::Justification::centred);
    }
    void mouseUp (const juce::MouseEvent& e) override
    {
        if (e.mouseWasClicked() && getLocalBounds().contains (e.getPosition()) && onClick) onClick();
    }

private:
    juce::String text;
    bool active = false;
};

// ---------------------------------------------------------------- switch
class SwitchComp : public juce::Component
{
public:
    SwitchComp() { setMouseCursor (juce::MouseCursor::PointingHandCursor); }
    void setOn (bool o) { if (on != o) { on = o; repaint(); } }
    bool isOn() const { return on; }
    std::function<void (bool)> onToggle;

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().withSizeKeepingCentre (52.0f, 26.0f);
        const float cr = r.getHeight() * 0.5f;
        if (on)
        {
            juce::Path p;
            p.addRoundedRectangle (r, cr);
            juce::DropShadow (col::accent.withAlpha (0.6f), 7, juce::Point<int> (0, 0)).drawForPath (g, p);
            g.setGradientFill (juce::ColourGradient (col::accent2, r.getX(), 0.0f, col::accent, r.getRight(), 0.0f, false));
            g.fillRoundedRectangle (r, cr);
        }
        else
        {
            g.setColour (juce::Colour (0xff131f2c));
            g.fillRoundedRectangle (r, cr);
            g.setColour (juce::Colour (0xff3d4f61));
            g.drawRoundedRectangle (r, cr, 1.2f);
        }
        const float d = r.getHeight() - 6.0f;
        const float tx = on ? r.getRight() - 3.0f - d : r.getX() + 3.0f;
        g.setColour (on ? juce::Colours::white : juce::Colour (0xffd7e0ea));
        g.fillEllipse (tx, r.getY() + 3.0f, d, d);
    }
    void mouseUp (const juce::MouseEvent& e) override
    {
        if (! e.mouseWasClicked() || ! getLocalBounds().contains (e.getPosition())) return;
        on = ! on;
        repaint();
        if (onToggle) onToggle (on);
    }

private:
    bool on = false;
};

// ---------------------------------------------------------------- preview button
class PreviewButton : public juce::Component
{
public:
    PreviewButton() { setMouseCursor (juce::MouseCursor::PointingHandCursor); }
    void setPlaying (bool p) { if (playing != p) { playing = p; repaint(); } }
    std::function<void()> onClick;

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().reduced (5.0f);
        const float cr = 14.0f;
        if (playing)
        {
            juce::Path p;
            p.addRoundedRectangle (r, cr);
            juce::DropShadow (col::accent.withAlpha (0.45f), 9, juce::Point<int> (0, 0)).drawForPath (g, p);
            g.setGradientFill (juce::ColourGradient (juce::Colour (0xff17508a), r.getX(), 0.0f,
                                                     juce::Colour (0xff1c7e98), r.getRight(), 0.0f, false));
            g.fillRoundedRectangle (r, cr);
            g.setColour (col::accent);
            g.drawRoundedRectangle (r, cr, 1.2f);
        }
        else
            drawGlowRect (g, r, cr, col::cardFill, col::border, 0.3f, 6);

        const juce::String label = playing ? "STOP" : "PREVIEW";
        const auto font = boldFont (15.0f, 0.2f);
        const float tw = font.getStringWidthFloat (label);
        const float iconW = 12.0f, gap = 12.0f;
        const float startX = r.getCentreX() - (iconW + gap + tw) * 0.5f;
        const float cy = r.getCentreY();

        g.setColour (col::text);
        if (playing)
            g.fillRect (startX, cy - 5.0f, 10.0f, 10.0f);
        else
        {
            juce::Path tri;
            tri.addTriangle (startX, cy - 6.0f, startX, cy + 6.0f, startX + 11.0f, cy);
            g.fillPath (tri);
        }
        g.setFont (font);
        g.drawText (label, juce::Rectangle<float> (startX + iconW + gap, r.getY(), tw + 4.0f, r.getHeight()),
                    juce::Justification::centredLeft);
    }
    void mouseUp (const juce::MouseEvent& e) override
    {
        if (e.mouseWasClicked() && getLocalBounds().contains (e.getPosition()) && onClick) onClick();
    }

private:
    bool playing = false;
};

// ---------------------------------------------------------------- segmented group
class SegmentedGroup : public juce::Component
{
public:
    explicit SegmentedGroup (juce::StringArray l) : labels (std::move (l)) { setMouseCursor (juce::MouseCursor::PointingHandCursor); }
    void setSelected (int i) { if (selected != i) { selected = i; repaint(); } }
    std::function<void (int)> onSelect;

    void paint (juce::Graphics& g) override
    {
        auto outer = getLocalBounds().toFloat().reduced (2.0f);
        g.setColour (col::cardFill);
        g.fillRoundedRectangle (outer, 12.0f);
        g.setColour (col::borderDim);
        g.drawRoundedRectangle (outer, 12.0f, 1.2f);

        auto inner = getInner();
        const float cw = inner.getWidth() / (float) labels.size();
        for (int i = 0; i < labels.size(); ++i)
        {
            auto cell = juce::Rectangle<float> (inner.getX() + cw * (float) i, inner.getY(), cw, inner.getHeight()).reduced (2.0f);
            const bool sel = (i == selected);
            if (sel)
            {
                g.setColour (juce::Colour (0xff12355a));
                g.fillRoundedRectangle (cell, 9.0f);
                g.setColour (col::accent);
                g.drawRoundedRectangle (cell, 9.0f, 1.2f);
            }
            g.setColour (sel ? col::text : col::dim);
            g.setFont (boldFont (16.0f, 0.05f));
            g.drawText (labels[i], cell, juce::Justification::centred);
        }
    }
    void mouseUp (const juce::MouseEvent& e) override
    {
        if (! e.mouseWasClicked()) return;
        auto inner = getInner();
        const float cw = inner.getWidth() / (float) labels.size();
        const int idx = juce::jlimit (0, labels.size() - 1, (int) ((e.position.x - inner.getX()) / cw));
        selected = idx;
        repaint();
        if (onSelect) onSelect (idx);
    }

private:
    juce::Rectangle<float> getInner() const { return getLocalBounds().toFloat().reduced (7.0f); }
    juce::StringArray labels;
    int selected = 0;
};

// ---------------------------------------------------------------- knob card
class KnobCard : public juce::Component
{
public:
    KnobCard (juce::String cap, juce::LookAndFeel* lnf) : caption (std::move (cap)), laf (lnf)
    {
        slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.2f, juce::MathConstants<float>::pi * 2.8f, true);
        slider.setLookAndFeel (lnf);
        addAndMakeVisible (slider);
    }
    ~KnobCard() override { slider.setLookAndFeel (nullptr); }

    void resized() override { slider.setBounds (getWidth() / 2 - 32, 10, 64, 64); }
    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().reduced (3.0f);
        g.setColour (col::cardFill);
        g.fillRoundedRectangle (r, 14.0f);
        g.setColour (col::border);
        g.drawRoundedRectangle (r, 14.0f, 1.2f);
        g.setColour (col::text);
        g.setFont (boldFont (12.0f, 0.12f));
        g.drawText (caption, getLocalBounds().withTrimmedTop (76).withTrimmedBottom (8), juce::Justification::centred);
    }

    juce::Slider slider;

private:
    juce::String caption;
    juce::LookAndFeel* laf;
};

// ---------------------------------------------------------------- effect card
class EffectCard : public juce::Component
{
public:
    explicit EffectCard (juce::String n) : name (std::move (n))
    {
        addAndMakeVisible (sw);
        sw.onToggle = [this] (bool v) { repaint(); if (onToggle) onToggle (v); };
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
    }
    void setOn (bool o) { sw.setOn (o); repaint(); }
    std::function<void (bool)> onToggle;

    void resized() override { sw.setBounds (getWidth() / 2 - 32, 14, 64, 40); }
    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().reduced (5.0f);
        const bool on = sw.isOn();
        if (on)
            drawGlowRect (g, r, 14.0f, col::cardFill2, col::accent, 0.4f, 6);
        else
        {
            g.setColour (col::cardFill);
            g.fillRoundedRectangle (r, 14.0f);
            g.setColour (col::borderDim);
            g.drawRoundedRectangle (r, 14.0f, 1.2f);
        }
        g.setColour (on ? col::text : col::dim);
        g.setFont (boldFont (12.0f, 0.12f));
        g.drawText (name, getLocalBounds().withTrimmedTop (62).withTrimmedBottom (10), juce::Justification::centred);
    }
    void mouseUp (const juce::MouseEvent& e) override
    {
        if (! e.mouseWasClicked()) return;
        const bool v = ! sw.isOn();
        sw.setOn (v);
        repaint();
        if (onToggle) onToggle (v);
    }

private:
    SwitchComp sw;
    juce::String name;
};

// ---------------------------------------------------------------- waveform panel
class WaveformPanel : public juce::Component,
                      public juce::FileDragAndDropTarget
{
public:
    enum class View { Source, Result };

    explicit WaveformPanel (KluayProcessor& p) : proc (p) {}

    void setView (View v) { view = v; repaint(); }
    View getView() const { return view; }

    std::function<void()> onRequestLoad;
    std::function<void()> onSelectionCommitted;
    std::function<void (const juce::File&)> onFileDropped;

    void tick()
    {
        const bool pl = proc.isPlaying();
        const int ver = proc.getRenderVersion();
        if (pl || pl != lastPlaying || ver != lastVersion) repaint();
        lastPlaying = pl;
        lastVersion = ver;
    }

    void resized() override
    {
        tabLoad = { 16, 8, 46, 18 };
        tabSource = { 68, 8, 62, 18 };
        tabResult = { 136, 8, 62, 18 };
    }

    juce::Rectangle<float> plotRect() const
    {
        return juce::Rectangle<float> (16.0f, 32.0f, (float) getWidth() - 32.0f, (float) getHeight() - 48.0f);
    }

    void paint (juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat().reduced (3.0f);
        drawGlowRect (g, bounds, 18.0f, col::dark, dropHover ? col::accent : col::border, 0.35f, 8);

        // faint dotted guide lines
        g.setColour (col::accent.withAlpha (0.18f));
        for (float x = 16.0f; x < (float) getWidth() - 16.0f; x += 6.0f)
        {
            g.fillRect (x, 28.0f, 1.0f, 1.0f);
            g.fillRect (x, (float) getHeight() - 14.0f, 1.0f, 1.0f);
        }

        const juce::String dot = juce::String::charToString ((juce::juce_wchar) 0xb7);
        auto tabText = [&g] (juce::Rectangle<int> r, const juce::String& t, bool active)
        {
            g.setColour (active ? col::accent : col::dim);
            g.setFont (boldFont (10.5f, 0.18f));
            g.drawText (t, r, juce::Justification::centredLeft);
        };
        tabText (tabLoad, "LOAD", false);
        tabText (tabSource, "SOURCE", view == View::Source);
        tabText (tabResult, "RESULT", view == View::Result);

        auto plot = plotRect();
        const float cy = plot.getCentreY();

        if (! proc.hasVocal())
        {
            g.setColour (col::muted);
            g.setFont (boldFont (12.0f, 0.2f));
            g.drawText ("DROP A VOCAL HERE " + dot + " OR CLICK TO LOAD", plot, juce::Justification::centred);
            return;
        }

        const int cols = juce::jmax (1, (int) (plot.getWidth() / 2.0f));
        const float colW = plot.getWidth() / (float) cols;

        if (view == View::Source)
        {
            if (srcVersion != proc.getSourceVersion() || srcCols != cols)
            {
                srcPeaks = normalise (proc.getSourcePeaks (cols));
                srcVersion = proc.getSourceVersion();
                srcCols = cols;
            }

            auto sel = dragMode != Drag::None ? dragSel : proc.getSelection();
            const float xa = plot.getX() + (float) sel.first * plot.getWidth();
            const float xb = plot.getX() + (float) sel.second * plot.getWidth();

            g.setColour (col::accent.withAlpha (0.16f));
            g.fillRect (juce::Rectangle<float> (xa, plot.getY(), xb - xa, plot.getHeight()));

            for (int c = 0; c < cols; ++c)
            {
                const float x = plot.getX() + ((float) c + 0.5f) * colW;
                const float h = juce::jmax (1.5f, srcPeaks[(size_t) c] * plot.getHeight());
                g.setColour (x >= xa && x <= xb ? col::accent : col::accent.withAlpha (0.35f));
                g.fillRect (x - 0.8f, cy - h * 0.5f, 1.6f, h);
            }
            g.setColour (juce::Colours::white);
            g.fillRect (xa - 0.75f, plot.getY(), 1.5f, plot.getHeight());
            g.fillRect (xb - 0.75f, plot.getY(), 1.5f, plot.getHeight());

            g.setColour (col::accent);
            g.setFont (boldFont (10.5f, 0.18f));
            const double secs = (sel.second - sel.first) * proc.getVocalLengthSeconds();
            g.drawText ("WORD " + dot + " " + juce::String (secs, 2) + "S " + dot + " DRAG TO PICK, DOUBLE-CLICK TO SNAP",
                        juce::Rectangle<int> (getWidth() / 2 - 20, 8, getWidth() / 2, 18), juce::Justification::centredRight);
            return;
        }

        // ---- result view
        if (! proc.hasResult())
        {
            g.setColour (col::muted);
            g.setFont (boldFont (12.0f, 0.2f));
            g.drawText ("PRESS PREVIEW", plot, juce::Justification::centred);
            return;
        }

        const bool after = proc.showAfter.load();
        const bool beat = proc.withBeat.load();
        const int key = proc.getRenderVersion() * 8 + (after ? 1 : 0) + (beat ? 2 : 0);
        if (resKey != key || resCols != cols)
        {
            resPeaks = normalise (proc.getResultPeaks (after, beat, cols));
            resKey = key;
            resCols = cols;
        }

        const bool playing = proc.isPlaying();
        const float playX = plot.getX() + proc.getPlayProgress() * plot.getWidth();
        for (int c = 0; c < cols; ++c)
        {
            const float x = plot.getX() + ((float) c + 0.5f) * colW;
            const float h = juce::jmax (1.5f, resPeaks[(size_t) c] * plot.getHeight());
            const bool played = ! playing || x <= playX;
            g.setColour (played ? juce::Colour (0xff35c8ff) : juce::Colour (0xff1c6f93).withAlpha (0.75f));
            g.fillRect (x - 0.8f, cy - h * 0.5f, 1.6f, h);
        }
        if (playing)
        {
            g.setColour (juce::Colours::white);
            g.fillRect (playX - 0.75f, plot.getY() - 2.0f, 1.5f, plot.getHeight() + 4.0f);
        }

        char bpmText[16];
        snprintf (bpmText, sizeof (bpmText), "%.0f", proc.getCurrentBpm());
        g.setColour (col::accent);
        g.setFont (boldFont (10.5f, 0.18f));
        g.drawText (juce::String (after ? "AFTER" : "BEFORE") + " " + dot + " " + juce::String (bpmText) + " BPM " + dot + " " + (beat ? "BEAT" : "VOX"),
                    juce::Rectangle<int> (getWidth() / 2, 8, getWidth() / 2 - 16, 18), juce::Justification::centredRight);

        g.setColour (col::muted);
        g.setFont (boldFont (9.0f, 0.2f));
        g.drawText ("DRAG TO DAW", juce::Rectangle<int> (getWidth() - 150, getHeight() - 26, 134, 12), juce::Justification::centredRight);
    }

    // ---- mouse
    void mouseDown (const juce::MouseEvent& e) override
    {
        dragMode = Drag::None;
        dragStarted = false;
        const auto pos = e.getPosition();
        if (tabLoad.contains (pos)) { if (onRequestLoad) onRequestLoad(); return; }
        if (tabSource.contains (pos)) { setView (View::Source); return; }
        if (tabResult.contains (pos)) { setView (View::Result); return; }
        if (! proc.hasVocal()) { if (onRequestLoad) onRequestLoad(); return; }

        if (view == View::Source)
        {
            const auto plot = plotRect();
            const auto sel = proc.getSelection();
            const float xa = plot.getX() + (float) sel.first * plot.getWidth();
            const float xb = plot.getX() + (float) sel.second * plot.getWidth();
            const float x = (float) pos.x;
            dragSel = sel;
            if (std::abs (x - xa) < 7.0f) dragMode = Drag::EdgeA;
            else if (std::abs (x - xb) < 7.0f) dragMode = Drag::EdgeB;
            else
            {
                dragMode = Drag::NewSel;
                anchor = xToNorm (x);
                dragSel = { anchor, anchor };
            }
        }
        else if (e.mods.isPopupMenu())
            showMenu();
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (view == View::Source && dragMode != Drag::None)
        {
            const double v = xToNorm ((float) e.x);
            if (dragMode == Drag::NewSel) dragSel = { juce::jmin (anchor, v), juce::jmax (anchor, v) };
            else if (dragMode == Drag::EdgeA) dragSel.first = juce::jmin (v, dragSel.second);
            else dragSel.second = juce::jmax (v, dragSel.first);
            repaint();
        }
        else if (view == View::Result && ! dragStarted && proc.hasResult() && e.getDistanceFromDragStart() > 6
                 && ! e.mods.isPopupMenu())
        {
            dragStarted = true;
            auto f = makeExportFile();
            if (proc.exportWav (f, proc.withBeat.load()))
                juce::DragAndDropContainer::performExternalDragDropOfFiles (juce::StringArray (f.getFullPathName()), false, this);
        }
    }

    void mouseUp (const juce::MouseEvent&) override
    {
        if (view == View::Source && dragMode != Drag::None)
        {
            dragMode = Drag::None;
            const double secs = (dragSel.second - dragSel.first) * proc.getVocalLengthSeconds();
            if (secs >= 0.03)
            {
                proc.setSelection (dragSel.first, dragSel.second);
                if (onSelectionCommitted) onSelectionCommitted();
            }
            repaint();
        }
    }

    void mouseDoubleClick (const juce::MouseEvent& e) override
    {
        if (view != View::Source || ! proc.hasVocal()) return;
        const auto seg = proc.findSegmentAt (xToNorm ((float) e.x));
        if (seg.first >= 0.0)
        {
            proc.setSelection (seg.first, seg.second);
            if (onSelectionCommitted) onSelectionCommitted();
            repaint();
        }
    }

    // ---- file drop
    bool isInterestedInFileDrag (const juce::StringArray& files) override
    {
        for (auto& f : files)
        {
            const auto ext = juce::File (f).getFileExtension().toLowerCase();
            if (ext == ".wav" || ext == ".aif" || ext == ".aiff" || ext == ".flac" || ext == ".mp3" || ext == ".ogg") return true;
        }
        return false;
    }
    void fileDragEnter (const juce::StringArray&, int, int) override { dropHover = true; repaint(); }
    void fileDragExit (const juce::StringArray&) override { dropHover = false; repaint(); }
    void filesDropped (const juce::StringArray& files, int, int) override
    {
        dropHover = false;
        if (files.size() > 0 && onFileDropped) onFileDropped (juce::File (files[0]));
        repaint();
    }

private:
    enum class Drag { None, NewSel, EdgeA, EdgeB };

    static std::vector<float> normalise (std::vector<float> v)
    {
        float m = 1.0e-6f;
        for (auto x : v) m = juce::jmax (m, x);
        for (auto& x : v) x = juce::jmin (1.0f, x / m * 0.95f);
        return v;
    }

    double xToNorm (float x) const
    {
        const auto plot = plotRect();
        return juce::jlimit (0.0, 1.0, (double) ((x - plot.getX()) / plot.getWidth()));
    }

    static juce::File makeExportFile()
    {
        auto dir = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("KLUAY Exports");
        return dir.getChildFile ("KLUAY_" + juce::Time::getCurrentTime().formatted ("%Y%m%d_%H%M%S") + ".wav");
    }

    void showMenu()
    {
        juce::PopupMenu m;
        m.addItem (1, "Save as WAV...");
        juce::Component::SafePointer<WaveformPanel> safe (this);
        m.showMenuAsync (juce::PopupMenu::Options(), [safe] (int r)
        {
            if (safe == nullptr || r != 1) return;
            safe->chooser = std::make_unique<juce::FileChooser> ("Save stutter", makeExportFile(), "*.wav");
            safe->chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles
                                            | juce::FileBrowserComponent::warnAboutOverwriting,
                                        [safe] (const juce::FileChooser& fc)
                                        {
                                            if (safe == nullptr) return;
                                            auto f = fc.getResult();
                                            if (f != juce::File())
                                                safe->proc.exportWav (f.withFileExtension ("wav"), safe->proc.withBeat.load());
                                        });
        });
    }

    KluayProcessor& proc;
    View view = View::Source;
    Drag dragMode = Drag::None;
    std::pair<double, double> dragSel { 0.0, 0.0 };
    double anchor = 0.0;
    bool dragStarted = false, dropHover = false, lastPlaying = false;
    int lastVersion = -1;
    juce::Rectangle<int> tabLoad, tabSource, tabResult;

    std::vector<float> srcPeaks, resPeaks;
    int srcVersion = -1, srcCols = -1, resKey = -1, resCols = -1;
    std::unique_ptr<juce::FileChooser> chooser;
};
} // namespace kluay
