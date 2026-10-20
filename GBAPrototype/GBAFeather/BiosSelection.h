#pragma once

// The private BIOS is compiled only when explicitly selected by build.ps1.
// The existing MIT replacement and its licence are retained unchanged.
#ifdef GBA_USE_LOCAL_BIOS
#include "src/generated/LocalBios.h"
inline const auto& firmwareBiosBytes = local_gba_bios_bin;
inline constexpr char firmwareBiosStage[] = "5: LOAD LOCAL GBA BIOS";
#ifdef GBA_DEBUG_USB
inline constexpr char firmwareBootBanner[] = "BOOT v46 - SONIC / CPU FETCH TRIAL";
#else
inline constexpr char firmwareBootBanner[] = "BOOT v46 - SONIC / CORE 1 TIMINGS";
#endif
#else
#include "src/ReplacementBios.h"
inline const auto& firmwareBiosBytes = gba_bios_bin;
inline constexpr char firmwareBiosStage[] = "5: LOAD REPLACEMENT BIOS";
inline constexpr char firmwareBootBanner[] = "BOOT v46 - SONIC / CULT BIOS";
#endif
