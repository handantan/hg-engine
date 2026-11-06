#include "constants/battle_constants.h"
.include "battle_commands.inc"

.data

// PMFG REQ-TGT-BTL-003: Opportunist already copied the stages (ServerDoPostMoveEffects.c); the holder is MSG_BATTLER_TEMP
_000:
    AbilityPopup BATTLER_CATEGORY_MSG_BATTLER_TEMP, ABILITY_OPPORTUNIST
    WaitButtonABTime 15
    // {0} copied its opponent’s stat changes!
    PrintMessage 1811, TAG_NICKNAME, BATTLER_CATEGORY_MSG_BATTLER_TEMP
    Wait
    WaitButtonABTime 30
    End
