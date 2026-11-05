#include "constants/battle_constants.h"
.include "battle_commands.inc"

.data

// PMFG (REQ-TGT-ABL-001): Lifesteal, Spectral Drain, Consume. C has set hp_calc_work (> 0) and battlerIdTemp.

_000:
    AbilityPopup BATTLER_CATEGORY_MSG_BATTLER_TEMP
    UpdateVar OPCODE_FLAG_ON, BSCRIPT_VAR_BATTLE_STATUS, BATTLE_STATUS_NO_BLINK
    Call BATTLE_SUBSCRIPT_UPDATE_HP
    // {0} had its HP restored.
    PrintMessage 1396, TAG_NICKNAME_ABILITY, BATTLER_CATEGORY_MSG_BATTLER_TEMP, BATTLER_CATEGORY_MSG_BATTLER_TEMP
    Wait
    WaitButtonABTime 30
    End
