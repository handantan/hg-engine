#include "constants/battle_constants.h"
.include "battle_commands.inc"

.data

// PMFG (REQ-TGT-ABL-001): Rooted: Ingrain on entry

_000:
    AbilityPopup BATTLER_CATEGORY_MSG_BATTLER_TEMP
    WaitButtonABTime 15
    UpdateMonData OPCODE_FLAG_ON, BATTLER_CATEGORY_MSG_BATTLER_TEMP, BMON_DATA_MOVE_EFFECT, MOVE_EFFECT_FLAG_INGRAIN
    // {0} planted its roots!
    PrintMessage 536, TAG_NICKNAME, BATTLER_CATEGORY_MSG_BATTLER_TEMP
    Wait
    WaitButtonABTime 30
    End
