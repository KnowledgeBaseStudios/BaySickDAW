#pragma once
#include <JuceHeader.h>

#if BAYSICK_HAS_LAME
 #include <lame.h>
#endif

// ─────────────────────────────────────────────────────────────────────────────
// MpglibAudioFormat - MP3 DECODING through the vendored LAME tree.
//
// SECURITY (QA-Cleanup 2026-08-11, CL-289 Tier-1 follow-on).
//
// We already vendor LAME for MP3 EXPORT (Mp3Writer).  Import went through a
// completely different decoder - JUCE's `JUCE_USE_MP3AUDIOFORMAT`, a ~3,185-line
// hand-port whose own header describes it as a conversion of a decompiler's
// output, and which has never been audited.  That is a second, redundant parser
// of untrusted input for a format we already ship a maintained decoder for.
//
// `libs/lame/mpglib/*.c` is ALREADY compiled into BaySickLame (CMakeLists globs
// it and defines HAVE_MPGLIB), so this costs no new dependency - only the
// AudioFormat wrapper below.  JUCE's decoder is switched off in CMakeLists.
//
// FULLY DECODED ON OPEN, deliberately.  MP3 has no reliable random access
// without building a frame index, and every consumer here needs seeking
// (AudioClipStreamer plays clips from arbitrary positions).  Decoding once into
// memory is also what the app ALREADY does with MP3: AudioClipStreamer RAM-loads
// them up to 100 MB precisely because "MP3 decode is ~10x slower per chunk" made
// streamed decode miss the audio deadline.  So this matches the existing
// behaviour rather than introducing a new cost, and it removes per-seek decode
// entirely.
//
// A file that decodes to more than the ceiling below is refused rather than
// allowed to exhaust memory - the declared length in an MP3 header is attacker
// controlled, and this runs on a background/message thread with no ceiling of
// its own otherwise.
// ─────────────────────────────────────────────────────────────────────────────
namespace MpglibAudio
{
    // ~22 minutes of 48 kHz stereo, chosen from the MEMORY it costs rather than
    // from a duration that sounded generous: decoded stereo float at this length
    // is ~512 MB, and the intermediate vectors are still alive alongside it when
    // the AudioBuffer is filled, so the real peak is roughly double.
    //
    // An earlier value of 2 hours was not a ceiling in any useful sense - it
    // permitted a ~5.5 GB peak, which on a 32-bit-ish allocation failure path
    // gives a null buffer rather than an exception (juce::AudioBuffer::setSize
    // uses HeapBlock without throwOnFailure).  Past any clip anyone drops on a
    // timeline.
    inline constexpr juce::int64 kMaxDecodedSamples = 48000LL * 60 * 22;

    // Cheap pre-check before the file is read into memory at all: MP3 compresses
    // ~10x at worst-case-useful bitrates, so anything past this cannot decode to
    // less than the ceiling above and there is no reason to buffer it first.
    inline constexpr juce::int64 kMaxEncodedBytes = 512 * 1024 * 1024;
}

#if BAYSICK_HAS_LAME

class MpglibReader final : public juce::AudioFormatReader
{
public:
    MpglibReader (juce::InputStream* sourceStream)
        : juce::AudioFormatReader (sourceStream, "MP3")
    {
        usesFloatingPointData = true;
        bitsPerSample         = 32;

        if (input->getTotalLength() > MpglibAudio::kMaxEncodedBytes)
            return;

        juce::MemoryBlock encoded;
        input->readIntoMemoryBlock (encoded);

        if (encoded.getSize() == 0)
            return;

        hip_t hip = hip_decode_init();

        if (hip == nullptr)
            return;

        // ONE FRAME PER CALL, then drain the decoder with len = 0.
        //
        // hip_decode_headers is NOT a one-frame call, whatever its name suggests:
        // it loops internally (mpglib_interface.c:400-418) writing at
        // `pcm_l + totsize` until the decoder is empty, so ITS output buffer must
        // hold every frame contained in the pushed chunk.  A 32 kbps 48 kHz file
        // has 96-byte frames, so a 1024-byte push decodes ten of them - 11,520
        // samples - and a 24 kbps MPEG-2 file is worse.  That is an ordinary
        // low-bitrate MP3 overrunning the heap, not a crafted one.
        //
        // hip_decode1_headers returns AT MOST one frame (lame.h says so), which
        // is what makes the buffer below sufficient rather than hopeful.
        constexpr int kMaxFrame = 1152;
        constexpr int kPcmSlack = kMaxFrame * 8;

        std::vector<short> pcmL ((size_t) kPcmSlack), pcmR ((size_t) kPcmSlack);
        std::vector<float> outL, outR;

        mp3data_struct info {};
        auto* src = static_cast<unsigned char*> (encoded.getData());
        const auto total = encoded.getSize();
        size_t pos = 0;
        bool overflowed = false;
        bool failed = false;

        while (pos < total && ! overflowed && ! failed)
        {
            const int chunk = (int) juce::jmin ((size_t) 1024, total - pos);

            int got = hip_decode1_headers (hip, src + pos, (size_t) chunk,
                                           pcmL.data(), pcmR.data(), &info);
            pos += (size_t) chunk;

            while (got != 0)
            {
                if (got < 0)                // malformed stream: keep what decoded
                { failed = true; break; }

                if ((juce::int64) outL.size() + got > MpglibAudio::kMaxDecodedSamples)
                { overflowed = true; break; }

                outL.insert (outL.end(), pcmL.begin(), pcmL.begin() + got);
                if (info.stereo > 1)
                    outR.insert (outR.end(), pcmR.begin(), pcmR.begin() + got);

                // len = 0 flushes what the chunk above already buffered.
                got = hip_decode1_headers (hip, src + pos, 0,
                                           pcmL.data(), pcmR.data(), &info);
            }
        }

        hip_decode_exit (hip);

        if (overflowed || outL.empty() || info.samplerate <= 0)
            return;

        numChannels    = (info.stereo > 1) ? 2u : 1u;
        sampleRate     = (double) info.samplerate;
        lengthInSamples = (juce::int64) outL.size();

        mDecoded.setSize ((int) numChannels, (int) outL.size());

        // setSize allocates through HeapBlock WITHOUT throwOnFailure, so a failed
        // allocation leaves a null buffer that setSample below would write
        // through.  Fail the reader instead.
        if (mDecoded.getNumSamples() != (int) outL.size())
        {
            lengthInSamples = 0;
            return;
        }

        // int16 -> float, the scale JUCE's float readers use.
        constexpr float kScale = 1.0f / 32768.0f;

        for (size_t i = 0; i < outL.size(); ++i)
            mDecoded.setSample (0, (int) i, (float) outL[i] * kScale);

        if (numChannels > 1)
            for (size_t i = 0; i < outL.size(); ++i)
                mDecoded.setSample (1, (int) i,
                                    (i < outR.size() ? (float) outR[i] : 0.0f) * kScale);
    }

    bool readSamples (int* const* destChannels, int numDestChannels,
                      int startOffsetInDestBuffer, juce::int64 startSampleInFile,
                      int numSamples) override
    {
        // Past the end reads as silence, which is the contract every JUCE reader
        // follows and what a clip streamer relies on at a loop boundary.
        for (int ch = 0; ch < numDestChannels; ++ch)
        {
            if (destChannels[ch] == nullptr)
                continue;

            auto* dest = reinterpret_cast<float*> (destChannels[ch]) + startOffsetInDestBuffer;
            const int src = juce::jmin (ch, mDecoded.getNumChannels() - 1);

            for (int i = 0; i < numSamples; ++i)
            {
                const auto pos = startSampleInFile + i;

                dest[i] = (src >= 0 && pos >= 0 && pos < lengthInSamples)
                            ? mDecoded.getSample (src, (int) pos)
                            : 0.0f;
            }
        }

        return true;
    }

private:
    juce::AudioBuffer<float> mDecoded;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MpglibReader)
};

#endif // BAYSICK_HAS_LAME

class MpglibAudioFormat final : public juce::AudioFormat
{
public:
    MpglibAudioFormat()
        : juce::AudioFormat ("MP3 file", ".mp3") {}

    juce::Array<int> getPossibleSampleRates() override    { return {}; }
    juce::Array<int> getPossibleBitDepths()   override    { return {}; }
    bool canDoStereo() override                           { return true; }
    bool canDoMono()   override                           { return true; }
    bool isCompressed() override                          { return true; }

    juce::AudioFormatReader* createReaderFor (juce::InputStream* sourceStream,
                                              bool deleteStreamIfOpeningFails) override
    {
       #if BAYSICK_HAS_LAME
        std::unique_ptr<MpglibReader> r (new MpglibReader (sourceStream));

        if (r->sampleRate > 0 && r->lengthInSamples > 0)
            return r.release();

        // The reader owns the stream on success only; on failure JUCE's contract
        // is that we delete it unless asked not to.
        r->input = nullptr;

        if (! deleteStreamIfOpeningFails)
            return nullptr;
       #else
        juce::ignoreUnused (deleteStreamIfOpeningFails);
       #endif

        delete sourceStream;
        return nullptr;
    }

    // Encoding stays with Mp3Writer, which talks to LAME directly and is wired
    // into the export dialog.  A writer here would be a second path to the same
    // encoder with none of that plumbing.
    //
    // JUCE 8 signature: the OutputStream*/sampleRate/... form is a DEPRECATED
    // non-virtual wrapper, and this options-taking one is the pure virtual.
    std::unique_ptr<juce::AudioFormatWriter>
        createWriterFor (std::unique_ptr<juce::OutputStream>&,
                         const juce::AudioFormatWriterOptions&) override
    {
        return nullptr;
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MpglibAudioFormat)
};
