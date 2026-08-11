#pragma once
#include <JuceHeader.h>

#if JUCE_WINDOWS
 #include <windows.h>
#endif

namespace Hosting
{
// ─────────────────────────────────────────────────────────────────────────────
// ScopedPluginDllDirectory - lets a plugin find the DLLs it ships beside itself.
//
// FOUND 2026-08-10 (QA-Cleanup) by chasing a Keyscape crash, and it was sitting
// in the CL-289 Tier-1 report as DLL-3 the whole time.
//
// THE SYMPTOM.  A plugin that ships sibling DLLs loads (the .vst3 itself is
// opened by full path), its editor begins to appear, and then the process dies
// with an access violation WRITING to null + a small member offset, inside the
// plugin, from processBlock.  A VST3 with no sibling DLLs is completely
// unaffected - which is exactly what makes it read as "that one plugin is
// broken" rather than as a host bug.
//
// THE CAUSE.  JUCE loads the module with a plain LoadLibrary on a full path
// (juce_Threads_windows.cpp:313) - NOT LoadLibraryEx with
// LOAD_WITH_ALTERED_SEARCH_PATH - and nothing in JUCE or in our source calls
// AddDllDirectory or SetDllDirectory.  Windows therefore resolves that module's
// dependencies against the standard order: OUR application directory, System32,
// the Windows directory, the current directory, then PATH.  **The plugin's own
// folder is never in that list.**  So the dependency is not found, the plugin's
// internal component is never constructed, and the first process() call
// dereferences it.
//
// WHY SetDllDirectory AND NOT SetDefaultDllDirectories.  SetDllDirectory INSERTS
// one directory into the standard order and removes nothing.
// SetDefaultDllDirectories REPLACES the order - it drops PATH and the
// application directory - which is a much bigger behavioural change and is
// exactly why the DLL-1 hardening attempt was backed out.  The two are easy to
// confuse and they are not interchangeable.
//
// SCOPE.  SetDllDirectory is process-wide, so this is deliberately an RAII scope
// held only across the load and cleared immediately after.  Both users -
// instantiation and the scanner - do their loading on a single thread at a time,
// so the window cannot overlap with another plugin's load.
// ─────────────────────────────────────────────────────────────────────────────
struct ScopedPluginDllDirectory
{
    explicit ScopedPluginDllDirectory (const juce::File& pluginPath)
    {
       #if JUCE_WINDOWS
        const auto dir = resolveDirectory (pluginPath);

        if (dir.isDirectory())
            mApplied = ::SetDllDirectoryW (dir.getFullPathName().toWideCharPointer()) != 0;
       #else
        juce::ignoreUnused (pluginPath);
       #endif
    }

    ~ScopedPluginDllDirectory()
    {
       #if JUCE_WINDOWS
        if (mApplied)
            ::SetDllDirectoryW (nullptr);
       #endif
    }

    ScopedPluginDllDirectory (const ScopedPluginDllDirectory&) = delete;
    ScopedPluginDllDirectory& operator= (const ScopedPluginDllDirectory&) = delete;

    // A VST3 "file" is usually a BUNDLE FOLDER whose real binary - and whose
    // sibling DLLs - live under Contents/<arch>-win/.  Same layout
    // PluginManager::architectureOf walks.  Pointing at the bundle's PARENT
    // would search the shared VST3 folder, which is not where they are.
    static juce::File resolveDirectory (const juce::File& pluginPath)
    {
        if (pluginPath.isDirectory())
        {
            const auto contents = pluginPath.getChildFile ("Contents");

            if (auto d = contents.getChildFile ("x86_64-win"); d.isDirectory()) return d;
            if (auto d = contents.getChildFile ("x86-win");    d.isDirectory()) return d;

            return pluginPath;
        }

        return pluginPath.getParentDirectory();
    }

    bool mApplied { false };
};
}
