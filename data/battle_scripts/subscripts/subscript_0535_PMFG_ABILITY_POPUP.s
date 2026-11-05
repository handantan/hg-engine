#include "constants/battle_constants.h"
.include "battle_commands.inc"

.data

// PMFG (REQ-TGT-ABL-001): only the ability popup, for effects done in C (Magnify Field)

_000:
    AbilityPopup BATTLER_CATEGORY_MSG_BATTLER_TEMP
    WaitButtonABTime 30
    End
