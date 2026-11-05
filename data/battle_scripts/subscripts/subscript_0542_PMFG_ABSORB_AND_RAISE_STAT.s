#include "constants/battle_constants.h"
.include "battle_commands.inc"

.data

// PMFG (REQ-TGT-ABL-001): Mudslide, Smouldering. Motor Drive's absorb script with the stat change from C (addeffect_param).

_000:
    PrintAttackMessage
    Wait
    WaitButtonABTime 15
    AbilityPopup BATTLER_CATEGORY_MSG_BATTLER_TEMP
    UpdateVarFromVar OPCODE_SET, BSCRIPT_VAR_BATTLER_STAT_CHANGE, BSCRIPT_VAR_MSG_BATTLER_TEMP
    Call BATTLE_SUBSCRIPT_UPDATE_STAT_STAGE
    End
