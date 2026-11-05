#include "constants/battle_constants.h"
.include "battle_commands.inc"

.data

// PMFG (REQ-TGT-ABL-001): Shockwiring: Charge on entry (as Wind Power's HANDLE_CHARGE_BOOST)

_000:
    AbilityPopup BATTLER_CATEGORY_MSG_BATTLER_TEMP
    WaitButtonABTime 15
    UpdateMonData OPCODE_FLAG_ON, BATTLER_CATEGORY_MSG_BATTLER_TEMP, BMON_DATA_MOVE_EFFECT, MOVE_EFFECT_FLAG_CHARGE
    UpdateMonData OPCODE_SET, BATTLER_CATEGORY_MSG_BATTLER_TEMP, BMON_DATA_CHARGED_TURNS, 2
    // {0} began charging power!
    PrintMessage 487, TAG_NICKNAME, BATTLER_CATEGORY_MSG_BATTLER_TEMP
    Wait
    WaitButtonABTime 30
    End
