#pragma once
#include <JuceHeader.h>

// ─────────────────────────────────────────────────────────────────────────────
// SafeXml - the ONLY way this app parses XML.
//
// SECURITY (QA-Cleanup 2026-08-10, CL-289 Tier-1 part 4 - save-file format).
//
// Every project, preset, template, rack chain, key-binding file and settings
// file is XML, and a project is a FOLDER USERS TRADE.  Two things made that
// dangerous, and both are closed here rather than at ~68 individual call sites.
//
// 1. EXTERNAL ENTITIES (XXE).  juce::XmlDocument honours
//    `<!DOCTYPE name SYSTEM "path">` and goes and READS that path when it
//    expands an entity (juce_XmlDocument.cpp:812-820 -> getFileContents at
//    :184-190 -> inputSource->createInputStreamFor).  When the document was
//    parsed from a FILE, JUCE attaches a FileInputSource, and that resolves the
//    path with getSiblingFile - which passes an ABSOLUTE path through verbatim,
//    including a `\\server\share` UNC path.
//
//    On Windows, opening a UNC path makes the OS offer the user's account name
//    and NTLM password hash to that server automatically.  So a shared project
//    whose project.xml began with a DOCTYPE pointing at an attacker's share
//    leaked the user's credentials the instant the project was opened, before
//    any of our code saw a single element, with nothing displayed.  The local
//    variant (`SYSTEM "C:/.../something.txt"`) pulls that file's TEXT into the
//    loaded document, where it travels onward if the project is re-shared.
//
//    Parsing from a STRING is the fix: the String constructor leaves
//    `inputSource` null, so getFileContents returns nothing and the external
//    fetch cannot happen at all.  The explicit DOCTYPE rejection below is belt
//    and braces - we never write one, so anything carrying one is not ours.
//
// 2. STACK EXHAUSTION.  juce_XmlDocument recurses once per nesting level with
//    no depth limit (:562, :619), on the way in AND when freeing the tree.
//    ~200k nested tags is ~600 KB of text - unremarkable for a project file -
//    and kills the thread.
//
// Note what is NOT relied on: JUCE's nested-entity expansion currently fails to
// blow up exponentially (billion-laughs) only because :865-884 uses the wrong
// variable as its search position.  That is a bug that happens to prevent an
// attack, not a defence, and it would come back if upstream ever fixed the
// indexing.  Rejecting DOCTYPE outright means we do not care either way.
// ─────────────────────────────────────────────────────────────────────────────
namespace SafeXml
{
    // Generous: the deepest thing we write is a project tree, nowhere near this.
    inline constexpr int kMaxDepth = 512;

    // Deliberately a whole-document check rather than a prolog-only one.  We
    // never write a DOCTYPE, so its presence anywhere means the file is not
    // ours; and failing closed on a false positive costs a refused load, while
    // failing open costs the credential leak described above.
    inline bool hasDoctype (const juce::String& text)
    {
        return text.containsIgnoreCase ("<!DOCTYPE");
    }

    inline bool exceedsDepth (const juce::String& text, int maxDepth)
    {
        int depth = 0;
        auto p = text.getCharPointer();   // CharPointerType by value, not a raw pointer

        while (! p.isEmpty())
        {
            if (p.getAndAdvance() != '<')
                continue;

            const juce::juce_wchar next = *p;

            if (next == '/')                    { --depth; continue; }
            if (next == '?' || next == '!')     { continue; }   // prolog / comment / CDATA

            if (++depth > maxDepth)
                return true;

            // A self-closing tag opens and closes in one go: walk to the '>'
            // and undo the increment if the tag ended "/>".
            juce::juce_wchar prev = 0;

            while (! p.isEmpty())
            {
                const juce::juce_wchar d = p.getAndAdvance();

                if (d == '>')
                {
                    if (prev == '/') --depth;
                    break;
                }

                prev = d;
            }
        }

        return false;
    }

    inline std::unique_ptr<juce::XmlElement> parse (const juce::String& text)
    {
        if (text.isEmpty())                       return {};
        if (hasDoctype (text))                    return {};
        if (exceedsDepth (text, kMaxDepth))       return {};

        // String overload on purpose: no input source, so no external fetch.
        return juce::parseXML (text);
    }

    inline std::unique_ptr<juce::XmlElement> parse (const juce::File& f)
    {
        if (! f.existsAsFile()) return {};
        return parse (f.loadFileAsString());
    }

    // Every engine's setStateInformation, every preset restore and every rack
    // chain goes through juce::AudioProcessor::getXmlFromBinary, which parses
    // from a String - so XXE is already closed on that path - but applies NO
    // depth limit.  That matters because the blob is base64 INSIDE the project
    // XML: the outer parse never sees the inner document's nesting, so a
    // hostile project can carry a 200k-deep tree in a blob and blow the stack
    // even though the file that contained it passed the cap.
    //
    // Byte layout mirrored from juce_AudioProcessor.cpp:946-980 - a 4-byte
    // magic number, a 4-byte length, then UTF-8.  Kept in step with it by the
    // magic constant: if JUCE ever changes the framing, this stops matching and
    // returns null rather than mis-parsing.
    inline std::unique_ptr<juce::XmlElement> parseBinaryBlob (const void* data, int sizeInBytes)
    {
        constexpr juce::uint32 kMagicXmlNumber = 0x21324356;

        if (data == nullptr || sizeInBytes <= 8)
            return {};

        if (juce::ByteOrder::littleEndianInt (data) != kMagicXmlNumber)
            return {};

        const int stringLength = (int) juce::ByteOrder::littleEndianInt (
                                     juce::addBytesToPointer (data, 4));

        if (stringLength <= 0)
            return {};

        return parse (juce::String::fromUTF8 (static_cast<const char*> (data) + 8,
                                              juce::jmin (sizeInBytes - 8, stringLength)));
    }
}
