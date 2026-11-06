#include "constants/battle_constants.h"
.include "battle_commands.inc"

.data

// PMFG REQ-TGT-BTL-003: Aroma Veil blocks a mental move; the holder is ABILITY_MON, the target MSG_TEMP
_000:
    PrintAttackMessage
    Wait
    WaitButtonABTime 30
    AbilityPopup BATTLER_CATEGORY_ABILITY_MON, ABILITY_AROMA_VEIL
    // {0} is protected by an aromatic veil!
    PrintMessage 1808, TAG_NICKNAME, BATTLER_CATEGORY_MSG_TEMP
    Wait
    WaitButtonABTime 30
    End
