#pragma once
#include <JuceHeader.h>

// ─────────────────────────────────────────────────────────────────────────────
// SafeNamModel - sanity gate on a .nam capture before it reaches nam::get_dsp.
//
// SECURITY (QA-Cleanup 2026-08-10, CL-289 Tier-1 part 2 - file-parser audit).
//
// A .nam is JSON: a `config` block describing a neural network's shape, and a
// flat `weights` array.  Amp captures circulate freely - ToneHunt, forums,
// Discord - so this is a file a user will happily accept from a stranger.
//
// TWO PROBLEMS IN THE VENDORED LOADER, NEITHER OF WHICH OUR try/catch CATCHES.
//
// 1. OVER-READ (NeuralAmpModelerCore lstm.cpp:9-28, wavenet/model.cpp:623-645,
//    conv1d.cpp:9-52).  The loader walks the declared shape and pulls one
//    number per connection through a RAW POINTER that is never checked against
//    the end of the weights list:
//
//        for (int i = 0; i < this->_w.rows(); i++)
//          for (int j = 0; j < this->_w.cols(); j++)
//            this->_w(i, j) = *(weights++);
//
//    LSTM's only check is `assert(it == weights.end())` - compiled out in
//    Release.  WaveNet's runs AFTER every read has happened.  ConvNet has a
//    function for exactly this, `_verify_weights`, and its body is `// TODO`.
//    Declare hidden_size 4096 x 8 layers, supply ten weights, and it reads
//    ~2.2 GB of adjacent memory.  Where that does not crash outright, unrelated
//    process memory becomes the model's coefficients and therefore the SOUND -
//    render that track and the user has shipped a slice of their own memory.
//
// 2. PREWARM HANG (dsp.cpp:30-64, fed by lstm.cpp:125-131 and
//    wavenet/model.cpp:616-620).  After building, NAM pushes silence through
//    until the network settles, and HOW MUCH is decided by the file: half
//    `sample_rate` for LSTM, the sum of `dilations` for WaveNet.  Neither is
//    range-checked.  `"sample_rate": 2000000000` is ~2 million full network
//    evaluations on the message thread - the UI freezes solid and never
//    returns.  Nothing throws; the call simply never comes back.
//
// WHY `catch (...)` IS NOT A DEFENCE, since both load sites have one and it
// reads like protection: it catches C++ exceptions.  Reading past the end of an
// array does not throw one, and on Windows an access violation is a Structured
// Exception, which MSVC's default /EHsc does NOT route to `catch (...)`.  A
// hang throws nothing at all.
//
// WHAT THIS CLOSES AND WHAT IT DOES NOT.  Bounding the declared dimensions
// removes the large over-reads and every prewarm hang.  It does NOT close a
// SMALL mismatch - a model needing 100 weights that supplies 90 still reads 40
// bytes past the end.  Closing that properly needs a bounds-checked iterator
// inside the vendored library, or feeding NAM a weights array we have already
// padded.  Recorded rather than papered over.
// ─────────────────────────────────────────────────────────────────────────────
namespace SafeNamModel
{
    // Generous against real captures: published NAM models sit in the low
    // hundreds for channels and single digits for layers.
    inline constexpr int    kMaxDimension   = 8192;
    inline constexpr int    kMaxLayers      = 64;
    inline constexpr int    kMaxDilation    = 1 << 20;
    inline constexpr double kMinSampleRate  = 8000.0;
    inline constexpr double kMaxSampleRate  = 768000.0;

    // Any numeric var as a double.  juce::JSON hands back an INT64 var for
    // integers past INT_MAX (juce_JSON.cpp asserts exactly that), so testing
    // only isDouble() / isInt() let `"sample_rate": 3000000000` through the
    // gate untouched and straight into the prewarm loop this file exists to
    // bound.  A first version did precisely that.
    inline bool numericValue (const juce::var& v, double& out)
    {
        if (v.isDouble() || v.isInt() || v.isInt64())
        {
            out = (double) v;
            return true;
        }

        return false;
    }

    // Forward: a model spec can contain whole other model specs.
    inline juce::String checkModelSpec (const juce::var& spec, int depth);

    // A `config` object plus whatever specs nest inside it.
    inline juce::String checkConfig (const juce::var& config, int depth)
    {
        auto* obj = config.getDynamicObject();
        if (obj == nullptr) return {};

        static const char* kDimFields[] =
            { "hidden_size", "channels", "input_size", "head_size",
              "output_size", "kernel_size", "condition_size" };

        for (const auto* name : kDimFields)
        {
            const auto v = obj->getProperty (juce::Identifier (name));
            double n = 0.0;
            if (! numericValue (v, n)) continue;

            if (n < 0.0 || n > (double) kMaxDimension)
                return juce::String ("its '") + name + "' value is out of range ("
                     + juce::String (n, 0) + ")";
        }

        double layers = 0.0;
        if (numericValue (obj->getProperty (juce::Identifier ("num_layers")), layers))
            if (layers < 0.0 || layers > (double) kMaxLayers)
                return "it declares too many layers (" + juce::String (layers, 0) + ")";

        const auto dil = obj->getProperty (juce::Identifier ("dilations"));
        if (auto* arr = dil.getArray())
        {
            if (arr->size() > kMaxLayers)
                return "it declares too many dilations (" + juce::String (arr->size()) + ")";

            double total = 0.0;
            for (const auto& d : *arr)
            {
                double n = 0.0;
                if (! numericValue (d, n)) continue;

                if (n < 0.0 || n > (double) kMaxDilation)
                    return "one of its dilation values is out of range";

                total += n;
            }

            // The WaveNet prewarm length IS this sum.
            if (total > (double) kMaxDilation)
                return "its dilations would take too long to warm up";
        }

        // WaveNet layer blocks.
        if (auto* arr = obj->getProperty (juce::Identifier ("layers")).getArray())
        {
            if (arr->size() > kMaxLayers)
                return "it declares too many layer blocks";

            for (const auto& entry : *arr)
                if (auto why = checkConfig (entry, depth + 1); why.isNotEmpty())
                    return why;
        }

        // NESTED MODEL SPECS.  Both of these are fed straight back into
        // nam::get_dsp by the vendored loader, so a hostile file can hide its
        // big number one level down and walk past a gate that only reads the
        // top-level config:
        //   container.cpp    - architecture "SlimmableContainer",
        //                      config.submodels[N].model is a full spec
        //   wavenet/model.cpp - config.condition_dsp is a full spec
        if (auto* subs = obj->getProperty (juce::Identifier ("submodels")).getArray())
        {
            if (subs->size() > kMaxLayers)
                return "it declares too many submodels";

            for (const auto& s : *subs)
                if (auto* so = s.getDynamicObject())
                    if (auto why = checkModelSpec (so->getProperty (juce::Identifier ("model")),
                                                   depth + 1);
                        why.isNotEmpty())
                        return why;
        }

        if (auto why = checkModelSpec (obj->getProperty (juce::Identifier ("condition_dsp")),
                                       depth + 1);
            why.isNotEmpty())
            return why;

        return {};
    }

    inline juce::String checkModelSpec (const juce::var& spec, int depth)
    {
        if (depth > 8)               return "its model definition is nested too deeply";
        if (! spec.isObject())       return {};

        auto* obj = spec.getDynamicObject();
        if (obj == nullptr)          return {};

        double sr = 0.0;
        if (numericValue (obj->getProperty (juce::Identifier ("sample_rate")), sr))
            if (sr < kMinSampleRate || sr > kMaxSampleRate)
                return "it declares an impossible sample rate (" + juce::String (sr, 0) + ")";

        return checkConfig (obj->getProperty (juce::Identifier ("config")), depth);
    }

    // Empty return = accept.  Non-empty = human-readable reason to refuse.
    inline juce::String rejectReason (const juce::File& f)
    {
        if (! f.existsAsFile())
            return "the file does not exist";

        const auto text = f.loadFileAsString();
        if (text.isEmpty())
            return "the file is empty";

        auto root = juce::JSON::parse (text);

        // Deliberately permissive: if OUR parser cannot read it, we do not know
        // enough to judge it, and NAM's own loader will reject it through the
        // path that already works.  Refusing here would block valid captures
        // over a JSON dialect difference.
        if (! root.isObject())
            return {};

        return checkModelSpec (root, 0);
    }
}
