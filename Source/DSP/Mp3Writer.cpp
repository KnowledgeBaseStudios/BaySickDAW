#include "Mp3Writer.h"

#if BAYSICK_HAS_LAME
 #include <lame.h>
#endif

Mp3Writer::~Mp3Writer()
{
    close();
}

#if ! BAYSICK_HAS_LAME

// LAME absent: the export dialog hides MP3, but keep the type linkable so
// callers do not need their own #if around every use.
bool Mp3Writer::open (const juce::File&, double, int, int, juce::String& outErr)
{
    outErr = "MP3 export is not available in this build.";
    return false;
}
bool Mp3Writer::write (const float* const*, int) { return false; }
bool Mp3Writer::close()                          { return true;  }

#else

bool Mp3Writer::open (const juce::File& destination,
                      double            sampleRate,
                      int               numChannels,
                      int               bitrateKbps,
                      juce::String&     outErr)
{
    close();

    if (numChannels < 1 || numChannels > 2)
    {
        outErr = "MP3 export supports mono or stereo only.";
        return false;
    }

    destination.deleteFile();
    auto stream = std::make_unique<juce::FileOutputStream> (destination);
    if (! stream->openedOk())
    {
        outErr = "Could not write to " + destination.getFullPathName();
        return false;
    }

    auto* flags = lame_init();
    if (flags == nullptr)
    {
        outErr = "Could not initialise the MP3 encoder.";
        return false;
    }

    lame_set_in_samplerate  (flags, (int) std::lround (sampleRate));
    lame_set_num_channels   (flags, numChannels);
    lame_set_mode           (flags, numChannels == 1 ? MONO : JOINT_STEREO);
    lame_set_brate          (flags, bitrateKbps);
    lame_set_VBR            (flags, vbr_off);            // dialog offers CBR only
    lame_set_quality        (flags, 2);                  // 0 best .. 9 worst; 2 is
                                                         // the usual high-quality
                                                         // setting, and we are
                                                         // offline so speed is free
    lame_set_bWriteVbrTag   (flags, 1);                  // Xing/LAME header so
                                                         // players seek correctly

    if (lame_init_params (flags) < 0)
    {
        lame_close (flags);
        outErr = "The MP3 encoder rejected these settings ("
               + juce::String (bitrateKbps) + " kbps at "
               + juce::String (sampleRate, 0) + " Hz).";
        return false;
    }

    mFlags       = flags;
    mStream      = std::move (stream);
    mFile        = destination;
    mNumChannels = numChannels;
    return true;
}

bool Mp3Writer::write (const float* const* channelData, int numSamples)
{
    if (mFlags == nullptr || mStream == nullptr || numSamples <= 0) return false;

    auto* flags = static_cast<lame_global_flags*> (mFlags);

    // lame's documented worst case for its buffer.
    const size_t needed = (size_t) (1.25 * (double) numSamples) + 7200;
    if (mMp3Buffer.size() < needed)
        mMp3Buffer.resize (needed);

    // lame_encode_buffer_ieee_float takes -1..+1 planar floats directly, which
    // is exactly JUCE's layout -- no interleave or integer conversion needed.
    // Mono still wants a valid second pointer, so feed it the same channel.
    const float* left  = channelData[0];
    const float* right = (mNumChannels == 2) ? channelData[1] : channelData[0];

    const int written = lame_encode_buffer_ieee_float (
        flags, left, right, numSamples, mMp3Buffer.data(), (int) mMp3Buffer.size());

    if (written < 0) return false;
    if (written > 0) return mStream->write (mMp3Buffer.data(), (size_t) written);
    return true;   // encoder buffering this block; not an error
}

bool Mp3Writer::close()
{
    if (mFlags == nullptr)
    {
        mStream.reset();
        return true;
    }

    auto* flags = static_cast<lame_global_flags*> (mFlags);
    bool  ok    = true;

    if (mStream != nullptr)
    {
        if (mMp3Buffer.size() < 7200) mMp3Buffer.resize (7200);
        const int tail = lame_encode_flush (flags, mMp3Buffer.data(),
                                            (int) mMp3Buffer.size());
        if (tail > 0)
            ok = mStream->write (mMp3Buffer.data(), (size_t) tail);

        mStream->flush();
        mStream.reset();

        // The Xing/LAME header is a placeholder until the stream length is known,
        // so it can only be written after the file is closed.  Without this the
        // file still plays but seeking and duration display are wrong.
        if (ok && mFile.existsAsFile())
        {
            if (auto* f = std::fopen (mFile.getFullPathName().toRawUTF8(), "r+b"))
            {
                lame_mp3_tags_fid (flags, f);
                std::fclose (f);
            }
        }
    }

    lame_close (flags);
    mFlags = nullptr;
    return ok;
}

#endif  // BAYSICK_HAS_LAME
