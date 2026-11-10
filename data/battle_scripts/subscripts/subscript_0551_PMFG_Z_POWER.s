#include "constants/battle_constants.h"
.include "battle_commands.inc"

.data

// PMFG REQ-TGT-BTL-010: PMFG_TryZMove (BattleController_BeforeMove.c) swapped in the Z-move or applied the status
// Z effect; the user is MSG_BATTLER_TEMP, TEMP_DATA says what to print (PMFG_Z_MSG_*), SIDE_EFFECT_PARAM the stat raise.
// shortcut: messages only, no Z animation (D-066)
_000:
    // {0} surrounded itself with its Z-Power!
    PrintMessage 1817, TAG_NICKNAME, BATTLER_CATEGORY_MSG_BATTLER_TEMP
    Wait
    WaitButtonABTime 30
    CompareVarToValue OPCODE_EQU, BSCRIPT_VAR_TEMP_DATA, 1, _Damaging
    CompareVarToValue OPCODE_EQU, BSCRIPT_VAR_TEMP_DATA, 2, _StatUp
    CompareVarToValue OPCODE_EQU, BSCRIPT_VAR_TEMP_DATA, 3, _AllUp
    CompareVarToValue OPCODE_EQU, BSCRIPT_VAR_TEMP_DATA, 4, _Reset
    CompareVarToValue OPCODE_EQU, BSCRIPT_VAR_TEMP_DATA, 5, _Crit
    CompareVarToValue OPCODE_EQU, BSCRIPT_VAR_TEMP_DATA, 6, _Recover
    CompareVarToValue OPCODE_EQU, BSCRIPT_VAR_TEMP_DATA, 7, _FollowMe
    End

_Damaging:
    // {0} unleashes its full-force Z-Move!
    PrintMessage 1820, TAG_NICKNAME, BATTLER_CATEGORY_MSG_BATTLER_TEMP
    GoTo _Done

_StatUp:
    // {0} boosted its stats using its Z-Power!
    PrintMessage 1826, TAG_NICKNAME, BATTLER_CATEGORY_MSG_BATTLER_TEMP
    Wait
    WaitButtonABTime 30
    UpdateVar OPCODE_SET, BSCRIPT_VAR_SIDE_EFFECT_TYPE, SIDE_EFFECT_TYPE_INDIRECT
    UpdateVarFromVar OPCODE_SET, BSCRIPT_VAR_BATTLER_STAT_CHANGE, BSCRIPT_VAR_MSG_BATTLER_TEMP
    Call BATTLE_SUBSCRIPT_UPDATE_STAT_STAGE
    End

_AllUp:
    PrintMessage 1826, TAG_NICKNAME, BATTLER_CATEGORY_MSG_BATTLER_TEMP
    GoTo _Done

_Reset:
    // {0} returned its decreased stats to normal using its Z-Power!
    PrintMessage 1823, TAG_NICKNAME, BATTLER_CATEGORY_MSG_BATTLER_TEMP
    GoTo _Done

_Crit:
    // {0} boosted its critical-hit ratio using its Z-Power!
    PrintMessage 1829, TAG_NICKNAME, BATTLER_CATEGORY_MSG_BATTLER_TEMP
    GoTo _Done

_Recover:
    UpdateVar OPCODE_FLAG_ON, BSCRIPT_VAR_BATTLE_STATUS, BATTLE_STATUS_NO_BLINK
    Call BATTLE_SUBSCRIPT_UPDATE_HP
    // {0} restored its HP using its Z-Power!
    PrintMessage 1832, TAG_NICKNAME, BATTLER_CATEGORY_MSG_BATTLER_TEMP
    GoTo _Done

_FollowMe:
    // {0} became the center of attention!
    PrintMessage 484, TAG_NICKNAME, BATTLER_CATEGORY_MSG_BATTLER_TEMP

_Done:
    Wait
    WaitButtonABTime 30
    End
