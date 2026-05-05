/* =========================================================================================

   This is an auto-generated file: Any edits you make may be overwritten!

*/

#pragma once

namespace BaySickAssets
{
    extern const char*   BaySickDAWLogo_png;
    const int            BaySickDAWLogo_pngSize = 957688;

    extern const char*   big_rusty_drums_svg;
    const int            big_rusty_drums_svgSize = 8743;

    extern const char*   big_rusty_drums_png;
    const int            big_rusty_drums_pngSize = 3656182;

    extern const char*   control_tab_png;
    const int            control_tab_pngSize = 304558;

    // Number of elements in the namedResourceList and originalFileNames arrays.
    const int namedResourceListSize = 4;

    // Points to the start of a list of resource names.
    extern const char* namedResourceList[];

    // Points to the start of a list of resource filenames.
    extern const char* originalFilenames[];

    // If you provide the name of one of the binary resource variables above, this function will
    // return the corresponding data and its size (or a null pointer if the name isn't found).
    const char* getNamedResource (const char* resourceNameUTF8, int& dataSizeInBytes);

    // If you provide the name of one of the binary resource variables above, this function will
    // return the corresponding original, non-mangled filename (or a null pointer if the name isn't found).
    const char* getNamedResourceOriginalFilename (const char* resourceNameUTF8);
}
