#include "config.h"
#include "types.h"
#include "save.h"
#include "pmfg_settings.h"

// PMFG settings layer (REQ-TGT-SYS-001). Each setting is a 4-bit field in the save vars from PMFG_SETTINGS_VAR_BASE,
// stored XOR its default so a zeroed var (new game, or a save from before a setting existed) reads as the default.

static const u8 sDefaults[NUM_PMFG_SETTINGS] = PMFG_SETTING_DEFAULTS;

u8 LONG_CALL PMFG_GetSetting(u8 setting)
{
#ifdef DEBUG_BATTLE_SCENARIOS
    return sDefaults[setting]; // battle tests start with no field system, so script vars can't be read
#endif
    u16 var = GetScriptVar(PMFG_SETTINGS_VAR_BASE + setting / 4);
    return ((var >> ((setting % 4) * 4)) & 0xF) ^ sDefaults[setting];
}

void LONG_CALL PMFG_SetSetting(u8 setting, u8 value)
{
    u16 varId = PMFG_SETTINGS_VAR_BASE + setting / 4;
    u16 shift = (setting % 4) * 4;
    u16 var = GetScriptVar(varId) & ~(0xF << shift);
    SetScriptVar(varId, var | (((value ^ sDefaults[setting]) & 0xF) << shift));
}
