#include "BassSynth.h"
#include <cmath>

// ── Parameter definitions ─────────────────────────────────────────────────────
static const BassParamDef kSub808Params[]       = {{"Pitch",&BassSoundParams::pitch},{"Decay",&BassSoundParams::decay},{"Level",&BassSoundParams::level}};
static const BassParamDef kKickBass909Params[]  = {{"Pitch",&BassSoundParams::pitch},{"Punch",&BassSoundParams::punch},{"Decay",&BassSoundParams::decay},{"Level",&BassSoundParams::level}};
static const BassParamDef kTrapSubParams[]      = {{"Pitch",&BassSoundParams::pitch},{"Decay",&BassSoundParams::decay},{"Tone",&BassSoundParams::tone},{"Level",&BassSoundParams::level}};
static const BassParamDef kJazzParams[]         = {{"Pitch",&BassSoundParams::pitch},{"Attack",&BassSoundParams::attack},{"Release",&BassSoundParams::release_},{"Tone",&BassSoundParams::tone},{"Level",&BassSoundParams::level}};
static const BassParamDef kIndustrialParams[]   = {{"Pitch",&BassSoundParams::pitch},{"Drive",&BassSoundParams::drive},{"Decay",&BassSoundParams::decay},{"Tone",&BassSoundParams::tone},{"Level",&BassSoundParams::level}};
static const BassParamDef kAnalogSineParams[]   = {{"Pitch",&BassSoundParams::pitch},{"Attack",&BassSoundParams::attack},{"Release",&BassSoundParams::release_},{"Level",&BassSoundParams::level}};
static const BassParamDef kMoogParams[]         = {{"Pitch",&BassSoundParams::pitch},{"Cutoff",&BassSoundParams::cutoff},{"Resonance",&BassSoundParams::resonance},{"Drive",&BassSoundParams::drive},{"Level",&BassSoundParams::level}};
static const BassParamDef kAcidParams[]         = {{"Pitch",&BassSoundParams::pitch},{"Cutoff",&BassSoundParams::cutoff},{"Resonance",&BassSoundParams::resonance},{"Decay",&BassSoundParams::decay},{"Level",&BassSoundParams::level}};
static const BassParamDef kGrowlParams[]        = {{"Pitch",&BassSoundParams::pitch},{"FM Ratio",&BassSoundParams::fmRatio},{"FM Index",&BassSoundParams::fmIndex},{"Decay",&BassSoundParams::decay},{"Level",&BassSoundParams::level}};
static const BassParamDef kRubberParams[]       = {{"Pitch",&BassSoundParams::pitch},{"Decay",&BassSoundParams::decay},{"Punch",&BassSoundParams::punch},{"Level",&BassSoundParams::level}};
static const BassParamDef kHipHopParams[]       = {{"Pitch",&BassSoundParams::pitch},{"Punch",&BassSoundParams::punch},{"Tone",&BassSoundParams::tone},{"Decay",&BassSoundParams::decay},{"Level",&BassSoundParams::level}};
static const BassParamDef kPopSustainParams[]   = {{"Pitch",&BassSoundParams::pitch},{"Tone",&BassSoundParams::tone},{"Attack",&BassSoundParams::attack},{"Release",&BassSoundParams::release_},{"Level",&BassSoundParams::level}};
static const BassParamDef kRnBParams[]          = {{"Pitch",&BassSoundParams::pitch},{"Tone",&BassSoundParams::tone},{"Attack",&BassSoundParams::attack},{"Release",&BassSoundParams::release_},{"Level",&BassSoundParams::level}};
static const BassParamDef kBoomBapParams[]      = {{"Pitch",&BassSoundParams::pitch},{"Punch",&BassSoundParams::punch},{"Decay",&BassSoundParams::decay},{"Level",&BassSoundParams::level}};
static const BassParamDef kFunkSlapParams[]     = {{"Pitch",&BassSoundParams::pitch},{"Punch",&BassSoundParams::punch},{"Tone",&BassSoundParams::tone},{"Decay",&BassSoundParams::decay},{"Level",&BassSoundParams::level}};

const BassParamDef* getBassParamDefs(BassSoundType type, int& outCount)
{
    switch(type) {
        case BassSoundType::Sub808:          outCount=3; return kSub808Params;
        case BassSoundType::KickBass909:     outCount=4; return kKickBass909Params;
        case BassSoundType::TrapSub:         outCount=4; return kTrapSubParams;
        case BassSoundType::JazzWalking:     outCount=5; return kJazzParams;
        case BassSoundType::IndustrialGrind: outCount=5; return kIndustrialParams;
        case BassSoundType::AnalogSine:      outCount=4; return kAnalogSineParams;
        case BassSoundType::MoogStyle:       outCount=5; return kMoogParams;
        case BassSoundType::AcidBass:        outCount=5; return kAcidParams;
        case BassSoundType::GrowlBass:       outCount=5; return kGrowlParams;
        case BassSoundType::RubberBass:      outCount=4; return kRubberParams;
        case BassSoundType::HipHopThump:     outCount=5; return kHipHopParams;
        case BassSoundType::PopSustain:      outCount=5; return kPopSustainParams;
        case BassSoundType::RnBSmooth:       outCount=5; return kRnBParams;
        case BassSoundType::BoomBap:         outCount=4; return kBoomBapParams;
        case BassSoundType::FunkSlap:        outCount=5; return kFunkSlapParams;
        default:                             outCount=0; return nullptr;
    }
}

// ── BassSynthVoice ────────────────────────────────────────────────────────────
float BassSynthVoice::midiToHz(float note) const
{
    return 440.f * std::pow(2.f, (note - 69.f) / 12.f);
}

// state is float& -- no type mismatch with private float members
float BassSynthVoice::onePoleLP(float in, float& state, float cutoff01)
{
    float c = juce::jlimit(0.001f, 0.999f, cutoff01);
    state = state + c * (in - state);
    return state;
}

void BassSynthVoice::trigger(double sr)
{
    mSampleRate  = sr;
    mSamplePos   = 0;
    mPhase       = 0.0;
    mPhase2      = 0.0;
    mEnvAmp      = 1.0;
    mEnvPitch    = 1.0;
    mFilterState  = 0.f;
    mFilterState2 = 0.f;
    mReleasing   = false;
    active       = true;
}

void BassSynthVoice::release_v()  { mReleasing = true; }

void BassSynthVoice::resetVoice() { active = false; mSamplePos = 0; mReleasing = false; }

float BassSynthVoice::process()
{
    if (!active) return 0.f;
    float out = 0.f;
    switch (type) {
        case BassSoundType::Sub808:          out = processSub808();          break;
        case BassSoundType::KickBass909:     out = processKickBass909();     break;
        case BassSoundType::TrapSub:         out = processTrapSub();         break;
        case BassSoundType::JazzWalking:     out = processJazzWalking();     break;
        case BassSoundType::IndustrialGrind: out = processIndustrialGrind(); break;
        case BassSoundType::AnalogSine:      out = processAnalogSine();      break;
        case BassSoundType::MoogStyle:       out = processMoogStyle();       break;
        case BassSoundType::AcidBass:        out = processAcidBass();        break;
        case BassSoundType::GrowlBass:       out = processGrowlBass();       break;
        case BassSoundType::RubberBass:      out = processRubberBass();      break;
        case BassSoundType::HipHopThump:     out = processHipHopThump();     break;
        case BassSoundType::PopSustain:      out = processPopSustain();      break;
        case BassSoundType::RnBSmooth:       out = processRnBSmooth();       break;
        case BassSoundType::BoomBap:         out = processBoomBap();         break;
        case BassSoundType::FunkSlap:        out = processFunkSlap();        break;
        default: break;
    }
    ++mSamplePos;
    return out * params.level;
}

// ── Sound implementations ─────────────────────────────────────────────────────
float BassSynthVoice::processSub808()
{
    float decayTime = 0.3f + params.decay * 1.5f;
    int   decSamp   = (int)(decayTime * mSampleRate);
    float t  = (float)mSamplePos / juce::jmax(1, decSamp);
    float amp = std::exp(-4.f * t);
    float baseHz  = midiToHz(params.pitch);
    float freq = baseHz + (baseHz * 3.f - baseHz) * std::exp(-8.f * t);
    mPhase += (double)freq / mSampleRate;
    if (mPhase >= 1.0) mPhase -= 1.0;
    float out = std::sin((float)(juce::MathConstants<double>::twoPi * mPhase)) * amp;
    if (t >= 1.f) active = false;
    return out;
}

float BassSynthVoice::processKickBass909()
{
    float decayTime = 0.15f + params.decay * 0.8f;
    int   decSamp   = (int)(decayTime * mSampleRate);
    float t   = (float)mSamplePos / juce::jmax(1, decSamp);
    float amp = std::exp(-6.f * t);
    float clickDur = (float)(mSampleRate * 0.004);
    float click = (mSamplePos < (int)clickDur)
                  ? (1.f - (float)mSamplePos / clickDur) * params.punch * 0.8f : 0.f;
    float freq = midiToHz(params.pitch) * (1.f + std::exp(-20.f * t));
    mPhase += (double)freq / mSampleRate;
    if (mPhase >= 1.0) mPhase -= 1.0;
    float out = std::sin((float)(juce::MathConstants<double>::twoPi * mPhase)) * amp + click;
    if (t >= 1.f) active = false;
    return out;
}

float BassSynthVoice::processTrapSub()
{
    float decayTime = 0.4f + params.decay * 2.0f;
    int   decSamp   = (int)(decayTime * mSampleRate);
    float t   = (float)mSamplePos / juce::jmax(1, decSamp);
    float amp = std::exp(-3.5f * t);
    float freq = midiToHz(params.pitch) * (1.f + 3.f * std::exp(-15.f * t));
    mPhase += (double)freq / mSampleRate;
    if (mPhase >= 1.0) mPhase -= 1.0;
    float raw = std::sin((float)(juce::MathConstants<double>::twoPi * mPhase));
    float brightness = 0.1f + params.tone * 0.4f;
    float out = onePoleLP(raw, mFilterState, brightness) * amp;
    if (t >= 1.f) active = false;
    return out;
}

float BassSynthVoice::processJazzWalking()
{
    double freq = midiToHz(params.pitch);
    mPhase += freq / mSampleRate;
    if (mPhase >= 1.0) mPhase -= 1.0;
    float t = (float)mSamplePos / (float)mSampleRate;
    float atkT = 0.005f + params.attack * 0.1f;
    float relT = 0.2f   + params.release_ * 1.0f;
    float amp;
    if (t < atkT)          amp = t / atkT;
    else if (!mReleasing)  amp = 1.f;
    else { amp = 1.f - (t - atkT) / relT; if (amp <= 0.f) { active=false; return 0.f; } }
    float raw = std::sin((float)(juce::MathConstants<double>::twoPi * mPhase));
    return onePoleLP(raw, mFilterState, 0.1f + params.tone * 0.6f) * amp;
}

float BassSynthVoice::processIndustrialGrind()
{
    double freq = midiToHz(params.pitch);
    mPhase += freq / mSampleRate;
    if (mPhase >= 1.0) mPhase -= 1.0;
    float t   = (float)mSamplePos / (float)mSampleRate;
    float amp = mReleasing ? std::max(0.f, 1.f - t * 4.f) : 1.f;
    if (amp <= 0.f) { active=false; return 0.f; }
    float saw    = 2.f * (float)mPhase - 1.f;
    float driven = softClip(saw * (1.f + params.drive * 8.f));
    float coeff  = 1.f - std::exp(-2.f * juce::MathConstants<float>::pi
                       * (200.f + params.tone * 3000.f) / (float)mSampleRate);
    return onePoleLP(driven, mFilterState, coeff) * amp;
}

float BassSynthVoice::processAnalogSine()
{
    double freq = midiToHz(params.pitch);
    mPhase += freq / mSampleRate;
    if (mPhase >= 1.0) mPhase -= 1.0;
    float t = (float)mSamplePos / (float)mSampleRate;
    float atkT = 0.005f + params.attack * 0.15f;
    float relT = 0.1f   + params.release_ * 1.5f;
    float amp;
    if (t < atkT)         amp = t / atkT;
    else if (!mReleasing) amp = 1.f;
    else { amp = 1.f - (t - atkT) / relT; if (amp <= 0.f) { active=false; return 0.f; } }
    return std::sin((float)(juce::MathConstants<double>::twoPi * mPhase)) * amp;
}

float BassSynthVoice::processMoogStyle()
{
    double freq = midiToHz(params.pitch);
    mPhase += freq / mSampleRate;
    if (mPhase >= 1.0) mPhase -= 1.0;
    float t   = (float)mSamplePos / (float)mSampleRate;
    float amp = mReleasing ? std::max(0.f, 1.f - t * 3.f) : 1.f;
    if (amp <= 0.f) { active=false; return 0.f; }
    float saw    = 2.f * (float)mPhase - 1.f;
    float driven = softClip(saw * (1.f + params.drive * 4.f));
    float c      = 0.01f + params.cutoff * 0.6f;
    float res    = params.resonance * 0.9f;
    float lp1    = onePoleLP(driven + res * mFilterState2, mFilterState,  c);
    float lp2    = onePoleLP(lp1,                          mFilterState2, c);
    return lp2 * amp;
}

float BassSynthVoice::processAcidBass()
{
    double freq = midiToHz(params.pitch);
    mPhase += freq / mSampleRate;
    if (mPhase >= 1.0) mPhase -= 1.0;
    float decayTime = 0.1f + params.decay * 0.8f;
    float t         = (float)mSamplePos / (float)((int)(decayTime * mSampleRate));
    float envCut    = params.cutoff * std::exp(-4.f * t);
    float amp       = std::exp(-3.f * t);
    float sq        = ((float)mPhase < 0.5) ? 1.f : -1.f;
    float c         = 0.005f + envCut * 0.7f;
    float res       = params.resonance * 0.95f;
    float lp1       = onePoleLP(sq + res * mFilterState2, mFilterState,  c);
    float lp2       = onePoleLP(lp1,                      mFilterState2, c);
    if (t >= 1.f && mReleasing) active = false;
    return lp2 * amp;
}

float BassSynthVoice::processGrowlBass()
{
    double cFreq = midiToHz(params.pitch);
    double mFreq = cFreq * (double)params.fmRatio;
    mPhase  += cFreq / mSampleRate;
    mPhase2 += mFreq / mSampleRate;
    if (mPhase  >= 1.0) mPhase  -= 1.0;
    if (mPhase2 >= 1.0) mPhase2 -= 1.0;
    float t   = (float)mSamplePos / (float)mSampleRate;
    float amp = mReleasing ? std::max(0.f, 1.f - t * 4.f) : 1.f;
    if (amp <= 0.f) { active=false; return 0.f; }
    float mod = std::sin((float)(juce::MathConstants<double>::twoPi * mPhase2)) * params.fmIndex;
    float out = std::sin((float)(juce::MathConstants<double>::twoPi * mPhase) + mod);
    return softClip(out * 1.2f) * amp;
}

float BassSynthVoice::processRubberBass()
{
    float decayTime = 0.08f + params.decay * 0.4f;
    int   decSamp   = (int)(decayTime * mSampleRate);
    float t   = (float)mSamplePos / juce::jmax(1, decSamp);
    float amp = std::exp(-8.f * t);
    float freq = midiToHz(params.pitch) * (1.f + params.punch * 2.f * std::exp(-20.f * t));
    mPhase += (double)freq / mSampleRate;
    if (mPhase >= 1.0) mPhase -= 1.0;
    float out = std::sin((float)(juce::MathConstants<double>::twoPi * mPhase)) * amp;
    if (t >= 1.f) active = false;
    return out;
}

float BassSynthVoice::processHipHopThump()
{
    float decayTime = 0.2f + params.decay * 0.8f;
    int   decSamp   = (int)(decayTime * mSampleRate);
    float t     = (float)mSamplePos / juce::jmax(1, decSamp);
    float amp   = std::exp(-5.f * t);
    float cDur  = (float)(mSampleRate * 0.006);
    float click = (mSamplePos < (int)cDur) ? params.punch * 0.6f * (1.f - (float)mSamplePos / cDur) : 0.f;
    float freq  = midiToHz(params.pitch) * (1.f + 1.5f * std::exp(-15.f * t));
    mPhase += (double)freq / mSampleRate;
    if (mPhase >= 1.0) mPhase -= 1.0;
    float sine = std::sin((float)(juce::MathConstants<double>::twoPi * mPhase));
    float out  = onePoleLP(sine + click, mFilterState, 0.1f + params.tone * 0.5f) * amp;
    if (t >= 1.f) active = false;
    return out;
}

float BassSynthVoice::processPopSustain()
{
    double freq = midiToHz(params.pitch);
    mPhase += freq / mSampleRate;
    if (mPhase >= 1.0) mPhase -= 1.0;
    float t = (float)mSamplePos / (float)mSampleRate;
    float atkT = 0.002f + params.attack * 0.08f;
    float relT = 0.1f   + params.release_ * 1.0f;
    float amp;
    if (t < atkT)         amp = t / atkT;
    else if (!mReleasing) amp = 1.f;
    else { amp = 1.f - (t - atkT) / relT; if (amp <= 0.f) { active=false; return 0.f; } }
    float saw   = 2.f * (float)mPhase - 1.f;
    float sine  = std::sin((float)(juce::MathConstants<double>::twoPi * mPhase));
    float blend = params.tone;
    float mixed = saw * (1.f - blend) + sine * blend;
    return onePoleLP(mixed, mFilterState, 0.2f + params.tone * 0.6f) * amp;
}

float BassSynthVoice::processRnBSmooth()
{
    double freq = midiToHz(params.pitch);
    mPhase += freq / mSampleRate;
    if (mPhase >= 1.0) mPhase -= 1.0;
    float t = (float)mSamplePos / (float)mSampleRate;
    float atkT = 0.02f + params.attack * 0.3f;
    float relT = 0.3f  + params.release_ * 2.0f;
    float amp;
    if (t < atkT)         amp = t / atkT;
    else if (!mReleasing) amp = 1.f;
    else { amp = 1.f - (t - atkT) / relT; if (amp <= 0.f) { active=false; return 0.f; } }
    float sine = std::sin((float)(juce::MathConstants<double>::twoPi * mPhase));
    return onePoleLP(sine, mFilterState, 0.08f + params.tone * 0.3f) * amp;
}

float BassSynthVoice::processBoomBap()
{
    float decayTime = 0.25f + params.decay * 1.0f;
    int   decSamp   = (int)(decayTime * mSampleRate);
    float t     = (float)mSamplePos / juce::jmax(1, decSamp);
    float amp   = std::exp(-4.5f * t);
    float cDur  = (float)(mSampleRate * 0.005);
    float click = (mSamplePos < (int)cDur) ? params.punch * 0.7f * (1.f - (float)mSamplePos / cDur) : 0.f;
    float freq  = midiToHz(params.pitch) * (1.f + 2.f * std::exp(-12.f * t));
    mPhase += (double)freq / mSampleRate;
    if (mPhase >= 1.0) mPhase -= 1.0;
    float out = std::sin((float)(juce::MathConstants<double>::twoPi * mPhase)) * amp + click;
    if (t >= 1.f) active = false;
    return out;
}

float BassSynthVoice::processFunkSlap()
{
    float decayTime = 0.04f + params.decay * 0.2f;
    int   decSamp   = (int)(decayTime * mSampleRate);
    float t     = (float)mSamplePos / juce::jmax(1, decSamp);
    float amp   = std::exp(-12.f * t);
    float cDur  = (float)(mSampleRate * 0.003);
    float click = (mSamplePos < (int)cDur) ? params.punch * (1.f - (float)mSamplePos / cDur) : 0.f;
    double freq = midiToHz(params.pitch);
    mPhase += freq / mSampleRate;
    if (mPhase >= 1.0) mPhase -= 1.0;
    float sine = std::sin((float)(juce::MathConstants<double>::twoPi * mPhase));
    float mid  = onePoleLP(sine, mFilterState, 0.15f + params.tone * 0.5f);
    float out  = mid * amp + click;
    if (t >= 1.f) active = false;
    return out;
}

// ── BassSynth ─────────────────────────────────────────────────────────────────
BassSynth::BassSynth()
{
    for (auto& v : mVoices) { v.type = mCurrentType; v.params = mParams; }
}

void BassSynth::prepare(double sampleRate, int)
{
    mSampleRate = sampleRate;
    for (auto& v : mVoices) { v.active = false; v.type = mCurrentType; }
}

void BassSynth::setSoundType(BassSoundType type)
{
    mCurrentType = type;
    for (auto& v : mVoices) v.type = type;
}

void BassSynth::applyParamsToVoice(BassSynthVoice& v)
{
    v.type   = mCurrentType;
    v.params = mParams;
}

int BassSynth::findFreeVoice() const
{
    for (int i = 0; i < MAX_POLY; ++i)
        if (!mVoices[i].active) return i;
    return 0;
}

void BassSynth::noteOn(int midiNote, float velocity)
{
    int vi = findFreeVoice();
    mParams.pitch = (float)midiNote;
    mParams.level = velocity * 0.9f;
    applyParamsToVoice(mVoices[vi]);
    mVoices[vi].trigger(mSampleRate);
}

void BassSynth::noteOff()
{
    for (auto& v : mVoices)
        if (v.active && !v.mReleasing) v.release_v();
}

void BassSynth::renderNextBlock(juce::AudioBuffer<float>& buffer,
                                 int startSample, int numSamples)
{
    int nCh = buffer.getNumChannels();
    for (int s = 0; s < numSamples; ++s)
    {
        float mixed = 0.f;
        for (auto& v : mVoices)
            if (v.active) mixed += v.process();
        mixed = std::tanh(mixed * 0.5f) * 1.5f;
        for (int ch = 0; ch < nCh; ++ch)
            buffer.addSample(ch, startSample + s, mixed);
    }
}

void BassSynth::reset()
{
    for (auto& v : mVoices) v.resetVoice();
}
