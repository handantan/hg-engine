#include "constants/battle_constants.h"
.include "battle_commands.inc"

.data

// PMFG (REQ-TGT-ABL-001): Brand of Torment. C has set the attacker's torment bit.

_000:
    AbilityPopup BATTLER_CATEGORY_MSG_BATTLER_TEMP
    // {0} was subjected to torment!
    PrintMessage 494, TAG_NICKNAME, BATTLER_CATEGORY_ATTACKER
    Wait
    WaitButtonABTime 30
    End
