# Third-Party Licenses -- Vocal Pitch-Shift Engines

BaySickDAW is distributed under the GNU General Public License v3 (see
`LICENSE` at the repository root). The vocal pitch editor, BaySickAlign, and
real-time vocal correction (batch QA-Fe, 2026-07-13) statically link the
following vendored third-party engines. Each engine's full license text is
bundled with its source under `libs/<engine>/`.

| Engine | Upstream | License | Bundled text |
|--------|----------|---------|--------------|
| WORLD | M. Morise -- github.com/mmorise/World | Modified BSD (3-clause style) | `libs/world/LICENSE.txt` |
| Rubber Band Library (R3) | Particular Programs Ltd. -- breakfastquay.com/rubberband | GPL v2-or-later | `libs/rubberband/COPYING` |
| Signalsmith Stretch | Signalsmith Audio -- github.com/Signalsmith-Audio/signalsmith-stretch | MIT | `libs/signalsmith-stretch/LICENSE.txt` |
| Signalsmith Linear | Signalsmith Audio -- github.com/Signalsmith-Audio/linear | MIT | `libs/signalsmith-linear/LICENSE.txt` |

Rubber Band is GPL v2-or-later, compatible with this application's GPLv3
distribution. WORLD (BSD) and both Signalsmith libraries (MIT) are permissive;
their copyright notices are preserved in the bundled source above. Signalsmith
Stretch is header-only and depends on Signalsmith Linear (also header-only).

> Scope: this file covers the QA-Fe pitch-shift engine vendoring. Other
> vendored dependencies (JUCE, sfizz, NeuralAmpModelerCore, LunaSVG, Eigen,
> concurrentqueue, ASIO SDK) carry their own bundled licenses under `libs/` and
> `juce/`; a complete third-party manifest is a pre-release `/audit-licenses`
> deliverable.
