#include "constants/battle_constants.h"
.include "battle_commands.inc"

.data

// PMFG (REQ-TGT-ABL-001): Windburst: Tailwind on entry (TAILWIND_START without the move)

_000:
    AbilityPopup BATTLER_CATEGORY_MSG_BATTLER_TEMP
    WaitButtonABTime 15
    SetTailwindCounter BATTLER_CATEGORY_MSG_BATTLER_TEMP
    // A tailwind started blowing on your side!
    PrintMessage 1230, TAG_NONE_SIDE, BATTLER_CATEGORY_MSG_BATTLER_TEMP
    Wait
    WaitButtonABTime 30
    End
