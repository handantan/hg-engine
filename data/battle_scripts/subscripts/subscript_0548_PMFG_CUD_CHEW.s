#include "constants/battle_constants.h"
.include "battle_commands.inc"

.data

// PMFG REQ-TGT-BTL-003: Cud Chew eats its last berry again; TryPluckBerry put the effect script in TEMP_DATA (as subscript 219)
_000:
    AbilityPopup BATTLER_CATEGORY_MSG_BATTLER_TEMP, ABILITY_CUD_CHEW
    WaitButtonABTime 15
    CompareVarToValue OPCODE_EQU, BSCRIPT_VAR_TEMP_DATA, 0, _end
    CallFromVar BSCRIPT_VAR_TEMP_DATA
_end:
    End
