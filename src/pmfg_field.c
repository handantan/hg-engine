#include "config.h"
#include "types.h"
#include "map_events_internal.h"
#include "pmfg_settings.h"

// PMFG field-side settings hooked into vanilla arm9 code (engine/hooks)

// REQ-TGT-QOL-002 autorun (refs/pokehns-expansion/src/field_player_avatar.c:905-945): ON runs unless B is held.
// hooks: replaces vanilla's run-button check (0x0205DE88, `return heldKeys & PAD_BUTTON_B`); both callers then pick the
// run movement, and only when the player has running shoes
BOOL LONG_CALL PMFG_PlayerAvatar_IsRunHeld(FIELD_PLAYER_AVATAR *avatar, u16 heldKeys)
{
    BOOL held = (heldKeys & 2) != 0; // PAD_BUTTON_B
    // avatar_sub->unk2 is pret's runningShoesLock, which already forces B in FieldInput_Update: keep running then
    if (PMFG_GetSetting(SETTING_AUTORUN) == SETTING_AUTORUN_ON && !avatar->avatar_sub->unk2) {
        return !held;
    }
    return held;
}

// REQ-TGT-QOL-002 fast_surf (refs/pokehns-expansion/src/field_player_avatar.c:933). HGSS already surfs at running speed
// (MOVEMENT_RUN_*, 16), so ON uses the bike's faster step (20, as vanilla's sub_0205DF0C). asm/general_hooks.s
// PMFG_FastSurf_hook replaces `mov r2, #0x10` in the surf branch of vanilla sub_0205D4B4 (0x0205D59E)
u32 LONG_CALL PMFG_SurfStepMovement(void)
{
    return PMFG_GetSetting(SETTING_FAST_SURF) == SETTING_FAST_SURF_ON ? 20 : 16;
}
