#include "constants/battle_constants.h"
.include "battle_commands.inc"

.data

// PMFG (REQ-TGT-ABL-001): Plantation. C has set the defender's Leech Seed bits.

_000:
    AbilityPopup BATTLER_CATEGORY_MSG_BATTLER_TEMP
    // {0} was seeded!
    PrintMessage 290, TAG_NICKNAME, BATTLER_CATEGORY_DEFENDER
    Wait
    WaitButtonABTime 30
    End
