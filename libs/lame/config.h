/* QA-Export (2026-07-25) -- BaySickDAW vendoring shim.
 *
 * The ONLY file added to the vendored libmp3lame 3.100 tree; everything else is
 * upstream verbatim.  LAME's sources do `#include <config.h>` under
 * HAVE_CONFIG_H, and that header is normally produced by running the autotools
 * `configure` script, which we do not run.  Upstream ships a pre-made MSVC
 * equivalent (configMS.h) for exactly this case and its own MSVC build
 * instructions say to use it as config.h -- so this forwards to it rather than
 * hand-rolling a define list.
 *
 * Note configMS.h TYPEDEFS int8_t..uint64_t itself for MSVC, which is why
 * HAVE_STDINT_H must stay undefined: letting machine.h pull in <stdint.h> as
 * well would redefine those same types.
 */
#include "configMS.h"
