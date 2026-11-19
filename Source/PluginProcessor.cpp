#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace kluay;

KluayProcessor::APVTS::ParameterLayout KluayProcessor::createLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> p;
    p.push_back (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "repeats", 1 }, "Repeats",
                                                               juce::StringArray { "2", "4", "8" }, 1));
    p.push_back (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "rate", 1 }, "Rate",
                                                               juce::StringArray { "1/16", "1/8" }, 1));
    p.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "fade", 1 }, "Fade In",
                                                              juce::NormalisableRange<float> (0.0f, 1.0f), 0.6f));
    p.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "pan", 1 }, "Pan",
                                                              juce::NormalisableRange<float> (0.0f, 1.0f), 0.6f));
    p.push_back (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "phone", 1 }, "Phone", true));
    p.push_back (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "chorus", 1 }, "Chorus", false));
    p.push_back (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "backverb", 1 }, "Backverb", false));
    return { p.begin(), p.end() };
}

KluayProcessor::KluayProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "KLUAY", createLayout())
{
    formatManager.registerBasicFormats();
    for (auto* id : { "repeats", "rate", "fade", "pan", "phone", "chorus", "backverb" })
        apvts.addParameterListener (id, this);
}

KluayProcessor::~KluayProcessor()
{
    for (auto* id : { "repeats", "rate", "fade", "pan", "phone", "chorus", "backverb" })
        apvts.removeParameterListener (id, this);
}

void KluayProcessor::prepareToPlay (double sampleRate, int)
{
    currentSampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;
    renderPending = true;
}

bool KluayProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

double KluayProcessor::getCurrentBpm() const
{
    const double b = hostBpm.load();
    return (b > 20.0 && b < 400.0) ? b : 140.0;
}

// ------------------------------------------------------------------ audio
void KluayProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();

    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
            if (auto bpm = pos->getBpm())
                hostBpm = *bpm;

    for (const auto metadata : midi)
        if (metadata.getMessage().isNoteOn())
        {
            playPos = 0;
            playing = true;
        }

    if (! playing.load())
        return;

    const juce::ScopedTryLock sl (renderLock);
    if (! sl.isLocked())
        return;

    const int len = current.length;
    if (len <= 0 || std::abs (current.sampleRate - currentSampleRate.load()) > 1.0)
        return;

    const auto& vox = showAfter.load() ? current.after : current.before;
    const bool beat = withBeat.load();
    const int n = buffer.getNumSamples();
    const int numCh = buffer.getNumChannels();
    int pos = playPos.load();

    for (int i = 0; i < n; ++i)
    {
        if (pos >= len) pos = 0;
        for (int ch = 0; ch < numCh; ++ch)
        {
            const int src = juce::jmin (ch, 1);
            float v = vox.getSample (src, pos);
            if (beat) v += current.beat.getSample (src, pos);
            buffer.setSample (ch, i, juce::jlimit (-1.0f, 1.0f, v));
        }
        ++pos;
    }
    playPos = pos;
}

// ------------------------------------------------------------------ vocal
bool KluayProcessor::loadVocal (const juce::File& file, bool autoSelect)
{
    std::unique_ptr<juce::AudioFormatReader> reader (formatManager.createReaderFor (file));
    if (reader == nullptr || reader->lengthInSamples <= 0 || reader->lengthInSamples > (juce::int64) (reader->sampleRate * 120.0))
        return false;

    const int len = (int) reader->lengthInSamples;
    const int ch = (int) reader->numChannels;
    juce::AudioBuffer<float> tmp (2, len);
    tmp.clear();
    reader->read (&tmp, 0, len, 0, true, true);
    if (ch == 1)
        tmp.copyFrom (1, 0, tmp, 0, 0, len);

    vocal = std::move (tmp);
    vocalRate = reader->sampleRate;
    vocalFile = file;
    analyseSegments();

    if (autoSelect)
    {
        if (! segments.empty())
        {
            selStart = segments.front().first;
            selEnd = segments.front().second;
        }
        else
        {
            selStart = 0.0;
            selEnd = juce::jmin (1.0, 0.5 * vocalRate / (double) len);
        }
    }

    vocalLoaded = true;
    ++sourceVersion;
    currentLength = 0;
    renderPending = true;
    return true;
}

double KluayProcessor::getVocalLengthSeconds() const
{
    return vocalRate > 0.0 ? (double) vocal.getNumSamples() / vocalRate : 0.0;
}

void KluayProcessor::setSelection (double a, double b)
{
    selStart = juce::jlimit (0.0, 1.0, juce::jmin (a, b));
    selEnd = juce::jlimit (0.0, 1.0, juce::jmax (a, b));
    renderPending = true;
}

void KluayProcessor::analyseSegments()
{
    segments.clear();
    const int n = vocal.getNumSamples();
    if (n < 1) return;

    const int win = juce::jmax (1, (int) (vocalRate * 0.01));
    const int nWin = n / win + 1;
    std::vector<float> env ((size_t) nWin, 0.0f);
    const float* l = vocal.getReadPointer (0);
    const float* r = vocal.getReadPointer (1);
    float maxEnv = 0.0f;
    for (int w = 0; w < nWin; ++w)
    {
        float m = 0.0f;
        const int e = juce::jmin (n, (w + 1) * win);
        for (int i = w * win; i < e; ++i)
            m = juce::jmax (m, std::abs (l[i]), std::abs (r[i]));
        env[(size_t) w] = m;
        maxEnv = juce::jmax (maxEnv, m);
    }

    const float thr = juce::jmax (maxEnv * 0.06f, 0.002f);
    const int gapMax = 12, minLen = 8;
    std::vector<std::pair<int, int>> segs;
    int start = -1, lastActive = -1;
    for (int w = 0; w < nWin; ++w)
    {
        if (env[(size_t) w] > thr)
        {
            if (start >= 0 && w - lastActive > gapMax) { segs.push_back ({ start, lastActive }); start = w; }
            else if (start < 0) start = w;
            lastActive = w;
        }
    }
    if (start >= 0) segs.push_back ({ start, lastActive });

    for (auto& s : segs)
    {
        if (s.second - s.first < minLen) continue;
        const double a = (double) (juce::jmax (0, s.first - 2) * win) / (double) n;
        const double b = (double) juce::jmin (n, (s.second + 3) * win) / (double) n;
        segments.push_back ({ a, b });
    }
}

std::pair<double, double> KluayProcessor::findSegmentAt (double x) const
{
    std::pair<double, double> best { -1.0, -1.0 };
    double bestDist = 1.0e9;
    for (auto& s : segments)
    {
        if (x >= s.first && x <= s.second) return s;
        const double d = juce::jmin (std::abs (x - s.first), std::abs (x - s.second));
        if (d < bestDist) { bestDist = d; best = s; }
    }
    return best;
}

std::vector<float> KluayProcessor::getSourcePeaks (int cols) const
{
    std::vector<float> out ((size_t) juce::jmax (1, cols), 0.0f);
    const int n = vocal.getNumSamples();
    if (n <= 0) return out;
    const float* l = vocal.getReadPointer (0);
    const float* r = vocal.getReadPointer (1);
    for (int c = 0; c < cols; ++c)
    {
        const int a = (int) ((juce::int64) n * c / cols);
        const int b = juce::jmax (a + 1, (int) ((juce::int64) n * (c + 1) / cols));
        const int step = juce::jmax (1, (b - a) / 256);
        float m = 0.0f;
        for (int i = a; i < b && i < n; i += step)
            m = juce::jmax (m, std::abs (l[i]), std::abs (r[i]));
        out[(size_t) c] = m;
    }
    return out;
}

juce::AudioBuffer<float> KluayProcessor::extractWord (double sr) const
{
    const int n = vocal.getNumSamples();
    const int a = juce::jlimit (0, n, (int) std::floor (selStart * n));
    const int b = juce::jlimit (0, n, (int) std::ceil (selEnd * n));
    if (b - a < 16) return {};

    juce::AudioBuffer<float> seg (2, b - a);
    for (int ch = 0; ch < 2; ++ch)
        seg.copyFrom (ch, 0, vocal, ch, a, b - a);

    if (std::abs (vocalRate - sr) > 1.0)
    {
        const double ratio = vocalRate / sr;
        const int outLen = (int) std::floor ((double) seg.getNumSamples() / ratio) - 2;
        if (outLen < 16) return {};
        juce::AudioBuffer<float> out (2, outLen);
        for (int ch = 0; ch < 2; ++ch)
        {
            juce::LagrangeInterpolator interp;
            interp.process (ratio, seg.getReadPointer (ch), out.getWritePointer (ch), outLen);
        }
        return out;
    }
    return seg;
}

// ------------------------------------------------------------------ preview
void KluayProcessor::renderIfNeeded()
{
    if (! vocalLoaded.load()) return;
    const bool bpmChanged = std::abs (getCurrentBpm() - lastRenderedBpm) > 0.05;
    const bool srChanged = std::abs (currentSampleRate.load() - lastRenderedSr) > 1.0;
    if (renderPending.load() || bpmChanged || srChanged || currentLength.load() == 0)
        renderNow();
}

void KluayProcessor::renderNow()
{
    renderPending = false;
    const double sr = currentSampleRate.load();
    auto word = extractWord (sr);
    if (word.getNumSamples() < 64) return;

    StutterSettings s;
    s.repeats = (int) std::lround (juce::String (apvts.getParameter ("repeats")->getCurrentValueAsText()).getIntValue());
    if (s.repeats != 2 && s.repeats != 8) s.repeats = 4;
    s.rateIndex = (int) std::lround (apvts.getRawParameterValue ("rate")->load());
    s.fadeIn = apvts.getRawParameterValue ("fade")->load();
    s.pan = apvts.getRawParameterValue ("pan")->load();
    s.phone = apvts.getRawParameterValue ("phone")->load() > 0.5f;
    s.chorus = apvts.getRawParameterValue ("chorus")->load() > 0.5f;
    s.backverb = apvts.getRawParameterValue ("backverb")->load() > 0.5f;
    s.bpm = getCurrentBpm();
    s.sampleRate = sr;

    RenderResult fresh;
    engine::render (word, s, fresh);
    const int len = fresh.length;
    {
        const juce::ScopedLock sl (renderLock);
        std::swap (current, fresh);
        currentLength = len;
    }
    lastRenderedBpm = s.bpm;
    lastRenderedSr = sr;
    ++renderVersion;
}

void KluayProcessor::startPreview()
{
    if (! vocalLoaded.load()) return;
    renderIfNeeded();
    playPos = 0;
    playing = true;
}

float KluayProcessor::getPlayProgress() const
{
    const int len = currentLength.load();
    return len > 0 ? (float) playPos.load() / (float) len : 0.0f;
}

std::vector<float> KluayProcessor::getResultPeaks (bool after, bool beat, int cols) const
{
    std::vector<float> out ((size_t) juce::jmax (1, cols), 0.0f);
    const juce::ScopedLock sl (renderLock);
    const int n = current.length;
    if (n <= 0) return out;
    const auto& vox = after ? current.after : current.before;
    for (int c = 0; c < cols; ++c)
    {
        const int a = (int) ((juce::int64) n * c / cols);
        const int b = juce::jmax (a + 1, (int) ((juce::int64) n * (c + 1) / cols));
        const int step = juce::jmax (1, (b - a) / 256);
        float m = 0.0f;
        for (int i = a; i < b && i < n; i += step)
        {
            float v = 0.5f * (vox.getSample (0, i) + vox.getSample (1, i));
            if (beat) v += 0.5f * (current.beat.getSample (0, i) + current.beat.getSample (1, i));
            m = juce::jmax (m, std::abs (v));
        }
        out[(size_t) c] = m;
    }
    return out;
}

bool KluayProcessor::exportWav (const juce::File& dest, bool includeBeat) const
{
    juce::AudioBuffer<float> mix;
    double sr = 44100.0;
    {
        const juce::ScopedLock sl (renderLock);
        if (current.length <= 0) return false;
        mix.makeCopyOf (current.after);
        if (includeBeat)
            for (int ch = 0; ch < 2; ++ch)
                mix.addFrom (ch, 0, current.beat, ch, 0, current.length);
        sr = current.sampleRate;
    }
    const float mag = mix.getMagnitude (0, mix.getNumSamples());
    if (mag > 0.98f) mix.applyGain (0.98f / mag);

    dest.getParentDirectory().createDirectory();
    dest.deleteFile();
    std::unique_ptr<juce::FileOutputStream> stream (dest.createOutputStream());
    if (stream == nullptr) return false;

    juce::WavAudioFormat wav;
    std::unique_ptr<juce::AudioFormatWriter> writer (wav.createWriterFor (stream.get(), sr, 2u, 24, juce::StringPairArray(), 0));
    if (writer == nullptr) return false;
    stream.release();
    return writer->writeFromAudioSampleBuffer (mix, 0, mix.getNumSamples());
}

// ------------------------------------------------------------------ state
void KluayProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto state = apvts.copyState();
    state.setProperty ("vocalPath", vocalFile.getFullPathName(), nullptr);
    state.setProperty ("selStart", selStart, nullptr);
    state.setProperty ("selEnd", selEnd, nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, dest);
}

void KluayProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
    {
        if (! xml->hasTagName (apvts.state.getType())) return;
        auto vt = juce::ValueTree::fromXml (*xml);
        apvts.replaceState (vt);
        const juce::File f (vt.getProperty ("vocalPath").toString());
        if (f.existsAsFile() && loadVocal (f, false) && vt.hasProperty ("selStart"))
            setSelection ((double) vt.getProperty ("selStart"), (double) vt.getProperty ("selEnd"));
    }
}

juce::AudioProcessorEditor* KluayProcessor::createEditor()
{
    return new KluayEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new KluayProcessor();
}
