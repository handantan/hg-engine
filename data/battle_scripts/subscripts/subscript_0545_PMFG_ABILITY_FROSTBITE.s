#include "constants/battle_constants.h"
.include "battle_commands.inc"

.data

// PMFG (REQ-TGT-ABL-001): Permafrost, Blizzard Heart. C has checked that the target can get frostbite.

_000:
    AbilityPopup BATTLER_CATEGORY_MSG_BATTLER_TEMP
    Call BATTLE_SUBSCRIPT_FREEZE
    End
