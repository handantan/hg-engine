#include "constants/battle_constants.h"
.include "battle_commands.inc"

.data

// PMFG REQ-TGT-BTL-007: PMFG_TryTerastallize (src/pmfg_progress.c) set the flag; the battler is MSG_BATTLER_TEMP, its Tera type MSG_TEMP
// shortcut: message only, no Tera animation or palette tint; upgrade with art (D-061)
_000:
    // {0} terastallized into the {1} type!
    PrintMessage 1814, TAG_NICKNAME_TYPE, BATTLER_CATEGORY_MSG_BATTLER_TEMP, BATTLER_CATEGORY_MSG_TEMP
    Wait
    WaitButtonABTime 30
    End
