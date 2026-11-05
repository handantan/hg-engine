#include "constants/battle_constants.h"
.include "battle_commands.inc"

.data

// PMFG (REQ-TGT-ABL-001): Restoring: Aqua Ring on entry

_000:
    AbilityPopup BATTLER_CATEGORY_MSG_BATTLER_TEMP
    WaitButtonABTime 15
    UpdateMonData OPCODE_FLAG_ON, BATTLER_CATEGORY_MSG_BATTLER_TEMP, BMON_DATA_MOVE_EFFECT, MOVE_EFFECT_FLAG_AQUA_RING
    // {0} surrounded itself with a veil of water!
    PrintMessage 1027, TAG_NICKNAME, BATTLER_CATEGORY_MSG_BATTLER_TEMP
    Wait
    WaitButtonABTime 30
    End
