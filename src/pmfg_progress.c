#include "config.h"
#include "types.h"
#include "pokemon.h"
#include "battle.h"
#include "test_battle.h"
#include "save.h"
#include "bag.h"
#include "constants/item.h"
#include "constants/file.h"
#include "pmfg_innates.h"
#include "pmfg_level_caps.h"
#include "pmfg_settings.h"

// PMFG story progress (REQ-TGT-MON-001 innate unlocks, REQ-TGT-SYS-002 level caps). Flags: pret/pokeheartgold include/constants/flags.h
#define PMFG_FLAG_BEAT_RADIO_TOWER_ROCKETS 0xC6
#define PMFG_FLAG_BELL_TOWER_HO_OH_DONE    0x108 // FLAG_UNK_108: set only when the Ho-Oh battle ends without a loss (caught or KO), scr_seq_0021_D17R0110.s:82
#define PMFG_FLAG_WHIRL_ISLANDS_LUGIA_DONE 0x109 // FLAG_UNK_109: the same for Lugia, scr_seq_0104_D40R0107.s:118

static u32 BadgeCount(void)
{
#ifdef DEBUG_BATTLE_SCENARIOS
    return 0; // battle tests see the first caps (12 NORMAL, 9 HARD) whatever the test save holds
#endif
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
        case PMFG_MILESTONE_LEGENDARY_STORY: // D-046: whichever legend the player chose at the Radio Tower, as SoulGold
            // not FLAG_HIDE_BELL_TOWER_HO_OH: the new-game init script sets it
            return CheckScriptFlag(PMFG_FLAG_BELL_TOWER_HO_OH_DONE) || CheckScriptFlag(PMFG_FLAG_WHIRL_ISLANDS_LUGIA_DONE);
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

// every ability check reads innates, so the last few (species, form) rows stay in RAM instead of a NARC read each time
#define PMFG_INNATE_CACHE_SIZE 8
static struct { u32 key; u16 innates[PMFG_NUM_INNATES]; } sInnateCache[PMFG_INNATE_CACHE_SIZE];
static u8 sInnateCacheNext;

void LONG_CALL PMFG_GetSpeciesInnates(u16 species, u8 form, u16 *out)
{
    u32 key = ((u32)form << 16 | species) + 1; // + 1: a zeroed slot never matches
    u32 i;
    for (i = 0; i < PMFG_INNATE_CACHE_SIZE && sInnateCache[i].key != key; i++) {}
    if (i == PMFG_INNATE_CACHE_SIZE) {
        i = sInnateCacheNext;
        sInnateCacheNext = (sInnateCacheNext + 1) % PMFG_INNATE_CACHE_SIZE;
        ReadFromNarcMemberByIdPair(sInnateCache[i].innates, ARC_CODE_ADDONS, CODE_ADDON_PMFG_INNATES, sizeof(u16) * PMFG_NUM_INNATES * PokeOtherFormMonsNoGet(species, form), sizeof(u16) * PMFG_NUM_INNATES);
        sInnateCache[i].key = key;
    }
    for (u32 j = 0; j < PMFG_NUM_INNATES; j++) {
        out[j] = sInnateCache[i].innates[j];
    }
}

/// @brief B-019: Illusion as the party mon's ability or an unlocked innate, for battle_pokemon.c's party-level checks
/// (they run before the mon is a battler, so BattlerHasAbility can't answer)
BOOL LONG_CALL PMFG_MonHasIllusion(struct PartyPokemon *mon)
{
    if (GetMonData(mon, MON_DATA_ABILITY, NULL) == ABILITY_ILLUSION) {
        return TRUE;
    }
    u32 unlocked = PMFG_UnlockedInnateCount();
    u16 innates[PMFG_NUM_INNATES];
    if (unlocked) {
        PMFG_GetSpeciesInnates(GetMonData(mon, MON_DATA_SPECIES, NULL), GetMonData(mon, MON_DATA_FORM, NULL), innates);
    }
    for (u32 i = 0; i < unlocked; i++) {
        if (innates[i] == ABILITY_ILLUSION) {
            return TRUE;
        }
    }
    return FALSE;
}

// REQ-TGT-TRN-002 (refs/soulgold/src/level_scaling.c:32-76): the party's average level, skipping mons 7+ levels below the top
u32 LONG_CALL PMFG_PartyScalingLevel(void)
{
    struct Party *party = SaveData_GetPlayerPartyPtr(SaveBlock2_get());
    u32 levels[6], highest = 1, total = 0, count = 0;
    for (int i = 0; i < party->count; i++) {
        struct PartyPokemon *mon = Party_GetMonByIndex(party, i);
        levels[i] = GetMonData(mon, MON_DATA_IS_EGG, NULL) ? 0 : GetMonData(mon, MON_DATA_LEVEL, NULL);
        if (levels[i] > highest) {
            highest = levels[i];
        }
    }
    for (int i = 0; i < party->count; i++) {
        if (levels[i] != 0 && highest - levels[i] < 7) {
            total += levels[i];
            count++;
        }
    }
    return count ? total / count : 1;
}

// REQ-TGT-SYS-004 trainer_ivs / trainer_evs (refs/pokehns-expansion/src/battle_main.c:2145-2175,
// challenge_menu.c:2382-2426): OFF keeps the authored values; SCALE rises with the badges (at most 8, as H&S)
static const u8 sTrainerScaledIVs[] = { 7, 10, 13, 16, 19, 22, 25, 28, 31 };
static const u8 sTrainerScaledEVs[] = { 12, 24, 36, 48, 60, 72, 80, 100, 128 };

void LONG_CALL PMFG_ApplyTrainerIVsEVs(struct PartyPokemon *mon)
{
    u32 badges = BadgeCount() > 8 ? 8 : BadgeCount();
    u32 ivSetting = PMFG_GetSetting(SETTING_TRAINER_IVS), evSetting = PMFG_GetSetting(SETTING_TRAINER_EVS);
    if (ivSetting != SETTING_TRAINER_IVS_OFF) {
        u8 iv = ivSetting == SETTING_TRAINER_IVS_SCALE ? sTrainerScaledIVs[badges] : 31;
        for (int i = 0; i < 6; i++) {
            SetMonData(mon, MON_DATA_HP_IV + i, &iv);
        }
    }
    if (evSetting == SETTING_TRAINER_EVS_OFF) {
        return;
    }
    u8 ev = evSetting == SETTING_TRAINER_EVS_SCALE ? sTrainerScaledEVs[badges] : evSetting == SETTING_TRAINER_EVS_HARD ? 128 : 252;
    RecalcPartyPokemonStats(mon); // the spread follows the mon's stats with its final IVs
    BOOL physical = GetMonData(mon, MON_DATA_ATTACK, NULL) > GetMonData(mon, MON_DATA_SPECIAL_ATTACK, NULL);
    BOOL defensive = GetMonData(mon, MON_DATA_DEFENSE, NULL) > GetMonData(mon, MON_DATA_SPECIAL_DEFENSE, NULL);
    SetMonData(mon, MON_DATA_HP_EV, &ev);
    SetMonData(mon, MON_DATA_SPEED_EV, &ev);
    SetMonData(mon, physical ? MON_DATA_ATK_EV : MON_DATA_SPATK_EV, &ev);
    SetMonData(mon, defensive ? MON_DATA_DEF_EV : MON_DATA_SPDEF_EV, &ev);
}

/**
 *  @brief PMFG (REQ-TGT-MON-001, B-018): can this hg-engine entry ability (PMFG_ENTRY_* bit) still fire this switch-in?
 *  A cleared ability_activated_flag (switch-in, Skill Swap...) re-arms every bit.
 */
BOOL LONG_CALL PmfgEntryReady(struct BattleStruct *sp, int client, u32 bit)
{
    if (!sp->battlemon[client].ability_activated_flag) {
        sp->pmfgVanillaEntryDone[client] = 0;
    }
    return !(sp->pmfgVanillaEntryDone[client] & bit);
}

void LONG_CALL PmfgEntryMark(struct BattleStruct *sp, int client, u32 bit)
{
    sp->battlemon[client].ability_activated_flag = 1;
    sp->pmfgVanillaEntryDone[client] |= bit;
}

// ---- PMFG REQ-TGT-BTL-007 Terastallization (Docs/plan/phase-4-btl-007.md). Terastallizing itself: ServerBeforeAct.c ----

/**
 *  @brief a battler's mon was loaded (switch-in, faint, Transform; from ClearBattleMonFlags): Tera again if this party
 *  slot already Terastallized this battle (Tera lasts the whole battle), else not Terastallized.
 *  shortcut: once per battle per side, not per trainer (a multi battle's two trainers share it); fine while no PMFG trainer Terastallizes
 */
void LONG_CALL PMFG_TeraOnLoad(struct BattleStruct *sp, int client)
{
    BOOL tera = sp->pmfgTeraSlot[client & 1] == sp->sel_mons_no[client] + 1;
    sp->battlemon[client].is_currently_terastallized = tera;
    sp->battlemon[client].tera_type = tera ? sp->pmfgTeraType[client & 1] : 0;
    sp->pmfgZWant &= ~(1 << client); // REQ-TGT-BTL-010: a Z request of a mon that never moved doesn't pass to the next one
}

/**
 *  @brief D-061 (1a): the move menu shows the Tera button in the Mega button's place (src/battle/mega.c
 *  CheckCanDrawMegaButton, for a mon that can't Mega Evolve). Same gate as ServerBeforeAct's PMFG_CanTerastallize.
 */
BOOL LONG_CALL PMFG_CanDrawTeraButton(struct BattleStruct *sp, int client)
{
    return CheckScriptFlag(FLAG_TERASTALIZATION_ENABLED) && CheckScriptFlag(PMFG_FLAG_TERA_ORB_CHARGED) && !sp->pmfgTeraSlot[0]
        && !(client && (sp->pmfgTeraWant & 1)); // doubles: the first mon already asked to Terastallize
}

void LONG_CALL PMFG_SetTeraWant(struct BattleStruct *sp, int client, BOOL want)
{
    sp->pmfgTeraWant = want ? (sp->pmfgTeraWant | (1 << client)) : (sp->pmfgTeraWant & ~(1 << client));
}

// ---- PMFG REQ-TGT-BTL-010 Z-Moves (Docs/plan/phase-4-btl-010.md). The conversion: BattleController_BeforeMove.c ----

/// D-066 B: the player can use Z-moves with FLAG_Z_MOVE_ENABLED (set by the obtain event) and the Z-Power Ring in the bag
BOOL LONG_CALL PMFG_PlayerHasZPower(void)
{
    return CheckScriptFlag(FLAG_Z_MOVE_ENABLED) && Bag_HasItem(Sav2_Bag_get(SaveBlock2_get()), ITEM_Z_POWER_RING, 1, HEAPID_BATTLE_HEAP);
}

/// D-066 A (b): the Z button in the Mega button's place, for a Z-crystal holder (a crystal holder can't Mega Evolve or Terastallize)
BOOL LONG_CALL PMFG_CanDrawZButton(struct BattleStruct *sp, int client)
{
    return PMFG_PlayerHasZPower() && !(sp->pmfgZUsed & 1) && !sp->battlemon[client].is_currently_terastallized
        && !(client && (sp->pmfgZWant & 1)); // doubles: the first mon already asked for its Z-move
}

void LONG_CALL PMFG_SetZWant(struct BattleStruct *sp, int client, BOOL want)
{
    sp->pmfgZWant = want ? (sp->pmfgZWant | (1 << client)) : (sp->pmfgZWant & ~(1 << client));
}

u32 LONG_CALL PMFG_StabKind(struct BattleStruct *sp, int attacker, int type)
{
    struct BattlePokemon *bm = &sp->battlemon[attacker];
    // a Terastallized mon's own types can't change (hg-engine blocks Soak, Forest's Curse...), so these stay the original types
    BOOL base = bm->type1 == type || bm->type2 == type || bm->type3 == type;
    if (!bm->is_currently_terastallized) {
        return base ? 2 : 0;
    }
    if (bm->tera_type == type) {
        return base ? 3 : 2;
    }
    return base ? 1 : 0;
}
