#include "constants/battle_constants.h"
.include "battle_commands.inc"

.data

_000:
    CheckAbility CHECK_OPCODE_HAVE, BATTLER_CATEGORY_ATTACKER, ABILITY_TIRELESS, _tireless // PMFG (REQ-TGT-ABL-001)
    UpdateMonData OPCODE_FLAG_ON, BATTLER_CATEGORY_ATTACKER, BMON_DATA_STATUS2, STATUS2_RECHARGE
    UpdateVarFromVar OPCODE_SET, BSCRIPT_VAR_ATTACKER_LOCKED_MOVE, BSCRIPT_VAR_MOVE_NO_CUR
    UpdateMonDataFromVar OPCODE_SET, BATTLER_CATEGORY_ATTACKER, BMON_DATA_RECHARGE, BSCRIPT_VAR_TOTAL_TURNS
    End
_tireless: // no recharge turn: popup only
    AbilityPopup BATTLER_CATEGORY_ATTACKER
    WaitButtonABTime 30
    End 
