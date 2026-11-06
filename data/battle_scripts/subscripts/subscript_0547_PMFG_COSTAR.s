#include "constants/battle_constants.h"
.include "battle_commands.inc"

.data

// PMFG REQ-TGT-BTL-003: Costar copied the ally's stat stages (SwitchInAbilityCheck.c); the ally is ABILITY_MON
_000:
    AbilityPopup BATTLER_CATEGORY_MSG_BATTLER_TEMP, ABILITY_COSTAR
    WaitButtonABTime 15
    // {0} copied {1}’s stat changes!
    PrintMessage 452, TAG_NICKNAME_NICKNAME, BATTLER_CATEGORY_MSG_BATTLER_TEMP, BATTLER_CATEGORY_ABILITY_MON
    Wait
    WaitButtonABTime 30
    End
