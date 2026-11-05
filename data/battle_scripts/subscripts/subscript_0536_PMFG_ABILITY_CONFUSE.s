#include "constants/battle_constants.h"
.include "battle_commands.inc"

.data

// PMFG (REQ-TGT-ABL-001): Dust Devil, Mirage. C has checked Own Tempo, Safeguard and existing confusion;
// the confuse subscript has no popup for an ability, so show it first.

_000:
    AbilityPopup BATTLER_CATEGORY_MSG_BATTLER_TEMP
    Call BATTLE_SUBSCRIPT_CONFUSE
    End
