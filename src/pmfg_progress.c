#include "config.h"
#include "types.h"
#include "pokemon.h"
#include "test_battle.h"
#include "save.h"
#include "constants/file.h"
#include "pmfg_innates.h"
#include "pmfg_level_caps.h"
#include "pmfg_settings.h"

// PMFG story progress (REQ-TGT-MON-001 innate unlocks, REQ-TGT-SYS-002 level caps). Flags: pret/pokeheartgold include/constants/flags.h
#define PMFG_FLAG_BEAT_RADIO_TOWER_ROCKETS 0xC6
#define PMFG_FLAG_CAUGHT_HO_OH             0x116
#define PMFG_FLAG_HIDE_BELL_TOWER_HO_OH    0x242

static u32 BadgeCount(void)
{
    struct PlayerProfile *profile = Sav2_PlayerData_GetProfileAddr(SaveBlock2_get());
    u32 badges = 0;
    for (int i = 0; i < 16; i++) {
        badges += PlayerProfile_TestBadgeFlag(profile, i) ? 1 : 0;
    }
    return badges;
}

// D-033 (proposal, to confirm): SoulGold's milestones mapped to HGSS events
static BOOL MilestoneReached(u8 milestone, u32 badges)
{
    switch (milestone) {
        case PMFG_MILESTONE_ROCKET_TAKEOVER:
            return CheckScriptFlag(PMFG_FLAG_BEAT_RADIO_TOWER_ROCKETS);
        case PMFG_MILESTONE_LEGENDARY_STORY: // SoulGold: Lugia chamber; HeartGold: the Bell Tower Ho-Oh event
            return CheckScriptFlag(PMFG_FLAG_CAUGHT_HO_OH) || CheckScriptFlag(PMFG_FLAG_HIDE_BELL_TOWER_HO_OH);
        case PMFG_MILESTONE_JOHTO_CHAMPION:
            return CheckScriptFlag(PMFG_FLAG_GAME_CLEAR);
        case PMFG_MILESTONE_KANTO_CHAMPION: // HGSS has no Kanto League: all 16 badges
            return badges >= 16;
        case PMFG_MILESTONE_NONE:
            return FALSE;
        default: // BADGE_n
            return badges >= milestone;
    }
}

u32 LONG_CALL PMFG_ProgressLevelCap(void)
{
    const u8 *caps = PMFG_GetSetting(SETTING_LEVEL_CAP_TABLE) == SETTING_LEVEL_CAP_TABLE_HARD ? sLevelCapTable_Hard : sLevelCapTable_Normal;
    u32 badges = BadgeCount();
    for (u32 i = 0; i < NELEMS(sLevelCapUntil); i++) {
        if (!MilestoneReached(sLevelCapUntil[i], badges)) {
            return caps[i];
        }
    }
    return 100;
}

// D-022: innate 1 at 8 badges, innate 2 after the Johto League, innate 3 at 12 badges (Docs/requirements/target/data/innate_unlocks.csv)
u32 LONG_CALL PMFG_UnlockedInnateCount(void)
{
#ifdef DEBUG_BATTLE_SCENARIOS
    return TestBattle_GetCurrentScenario()->innatesUnlocked ? PMFG_NUM_INNATES : 0; // upstream tests assume no innates
#else
    u32 badges = BadgeCount();
    if (badges >= 12) {
        return 3;
    }
    if (CheckScriptFlag(PMFG_FLAG_GAME_CLEAR)) {
        return 2;
    }
    return badges >= 8 ? 1 : 0;
#endif
}

void LONG_CALL PMFG_GetSpeciesInnates(u16 species, u8 form, u16 *out)
{
    // shortcut: reads the NARC on every ability check; cache per battler if battles feel slow
    ReadFromNarcMemberByIdPair(out, ARC_CODE_ADDONS, CODE_ADDON_PMFG_INNATES, sizeof(u16) * PMFG_NUM_INNATES * PokeOtherFormMonsNoGet(species, form), sizeof(u16) * PMFG_NUM_INNATES);
}
