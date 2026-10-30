#include "config.h"
#include "debug.h"
#include "types.h"

#include "constants/ability.h"
#include "constants/file.h"
#include "constants/game.h"
#include "constants/hold_item_effects.h"
#include "constants/item.h"
#include "constants/moves.h"
#include "constants/species.h"
#include "constants/weather_numbers.h"

#include "bag.h"
#include "battle.h"
#include "pokemon.h"
#include "rtc.h"
#include "save.h"
#include "script.h"
#include "trainer_data.h"
#include "pmfg_settings.h"
#include "pmfg_innates.h"

#ifdef DEBUG_BATTLE_SCENARIOS
#include "test_battle.h"
#endif // DEBUG_BATTLE_SCENARIOS

struct BattleSetup LONG_CALL *BattleSetup_New_Tutorial(u32 heapID, FieldSystem *fieldSystem);
int LONG_CALL BattleSetup_GetWildTransitionEffect(struct BattleSetup *setup);
int LONG_CALL BattleSetup_GetWildBattleMusic(struct BattleSetup *setup);
void LONG_CALL *Encounter_New(struct BattleSetup *setup, s32 effect, s32 bgm, u32 *winFlag);

/**
 *  @brief swap two integer values with each other given pointers
 *
 *  @param a first to swap
 *  @param b second to swap
 */
void swap(int *a, int *b)
{
    int temp = *a;
    *a = *b;
    *b = temp;
}

/**
 *  @brief randomize the order of an array size n
 *
 *  @param arr array to randomize
 *  @param n size of array
 */
void randomize(int arr[], int n)
{
    int i;
    for (i = n - 1; i > 0; i--) {
        int j = gf_rand() % (i + 1);
        swap(&arr[i], &arr[j]);
    }
}

extern u32 gLastPokemonLevelForMoneyCalc;

/**
 *  @brief REQ-TGT-TRN-002: highest authored level in a trpoke buffer. Walks the records with the same field sizes
 *         MakeTrainerPokemonParty reads below; keep the two in step.
 */
static u32 TrainerPartyHighestLevel(const u8 *buf, u8 dataType, u8 pokecount)
{
    u32 offset = 0, highest = 1;
    for (int i = 0; i < pokecount; i++) {
        u32 level = buf[offset + 2] | (buf[offset + 3] << 8);
        if (level > highest) {
            highest = level;
        }
        offset += 6; // ivs, ability slot, level, species
        offset += (dataType & TRAINER_DATA_TYPE_ITEMS) ? 2 : 0;
        offset += (dataType & TRAINER_DATA_TYPE_MOVES) ? 8 : 0;
        offset += (dataType & TRAINER_DATA_TYPE_ABILITY) ? 2 : 0;
        offset += (dataType & TRAINER_DATA_TYPE_BALL) ? 2 : 0;
        offset += (dataType & TRAINER_DATA_TYPE_IV_EV_SET) ? 12 : 0;
        offset += (dataType & TRAINER_DATA_TYPE_NATURE_SET) ? 1 : 0;
        offset += (dataType & TRAINER_DATA_TYPE_SHINY_LOCK) ? 1 : 0;
        if (dataType & TRAINER_DATA_TYPE_ADDITIONAL_FLAGS) {
            u32 flags = buf[offset] | (buf[offset + 1] << 8) | (buf[offset + 2] << 16) | (buf[offset + 3] << 24);
            offset += 4;
            offset += (flags & TRAINER_DATA_EXTRA_TYPE_STATUS) ? 4 : 0;
            for (u32 f = TRAINER_DATA_EXTRA_TYPE_HP; f <= TRAINER_DATA_EXTRA_TYPE_SP_DEF; f <<= 1) {
                offset += (flags & f) ? 2 : 0;
            }
            offset += (flags & TRAINER_DATA_EXTRA_TYPE_PP_COUNTS) ? 4 : 0;
            offset += (flags & TRAINER_DATA_EXTRA_TYPE_NICKNAME) ? 22 : 0;
        }
        offset += 2; // ball seal
    }
    return highest;
}

/**
 *  @brief create the trainer Party from the trainer data file and trainer party file
 *
 *  @param bp battle param
 *  @param num trainer index to read from both ARC_TRAINER_DATA and ARC_TRAINER_PARTY_DATA
 *  @param heapID heap to use for memory usage
 */
void MakeTrainerPokemonParty(struct BATTLE_PARAM *bp, int num, int heapID)
{
    u8 *buf;
    int i, j;
    u32 rnd_tmp, rnd, seed_tmp;
    u8 pow;

    seed_tmp = gf_get_seed();

    PokeParty_Init(bp->poke_party[num], 6);

    buf = (u8 *)sys_AllocMemory(heapID, sizeof(struct FULL_TRAINER_MON_DATA_STRUCTURE) * 6);

    TT_TrainerPokeDataGet(bp->trainer_id[num], buf);

    if (TT_TrainerTypeSexGet(bp->trainer_data[num].tr_type) == 1) // if trainer is female
    {
        rnd_tmp = 120;
    } else {
        rnd_tmp = 136;
    }

    u8 pokecount = bp->trainer_data[num].poke_count;
    u8 randomorder_flag = pokecount & TRAINER_DATA_RANDOM_PARTY_ORDER;
    pokecount &= 0x7f;

    // goal:  get rid of massive switch statement with each individual byte.  make the trainer type a bitfield
    u32 id;
    u16 species = 0, adjustedSpecies = 0, item = 0, ability = 0, level = 0, ball = 0, hp = 0, atk = 0, def = 0, speed = 0, spatk = 0, spdef = 0, ab1 = 0, ab2 = 0;
    u16 offset = 0;
    u16 moves[4];
    u8 ivnums[6];
    u8 evnums[6];
    u8 ppcounts[4];
    u16 *nickname = sys_AllocMemory(heapID, 11 * sizeof(u16));
    u8 form_no = 0, abilityslot = 0, nature = 0, ballseal = 0, shinylock = 0, status = 0;
    u32 additionalflags = 0;

    int partyOrder[pokecount];
    if (randomorder_flag) {
        if (gf_rand() % 2 == 0) {
            for (i = 0; i < pokecount; i++) {
                partyOrder[i] = pokecount - 1 - i;
            }
        } else {
            for (i = 0; i < pokecount; i++) {
                partyOrder[i] = i;
            }
        }
    } else {
        for (i = 0; i < pokecount; i++) {
            partyOrder[i] = i;
        }
    }

    if (randomorder_flag && pokecount > 1) {
        int numtimes = gf_rand() % 6 + 1;
        for (i = 0; i < numtimes; i++) {
            randomize(partyOrder, pokecount);
        }
    }

    struct PartyPokemon *mons[pokecount];

    // REQ-TGT-TRN-002 (refs/soulgold/src/battle_main.c:2010-2085, level_scaling.c:179-195,404-430,483-508): party
    // average + 1 - a per-save, per-trainer 0..1, minus each mon's authored gap below the top (at most 2)
    u32 scaledPartyLevel = 0, highestLevel = 0;
    if (PMFG_GetSetting(SETTING_TRAINER_LEVEL_SCALING) == SETTING_TRAINER_LEVEL_SCALING_ON) {
        u32 id = bp->trainer_id[num];
        highestLevel = TrainerPartyHighestLevel(buf, bp->trainer_data[num].data_type, pokecount);
        scaledPartyLevel = PMFG_PartyScalingLevel();
        if (id != 2 && id != 3 && id != 265) { // the first rival battles get no +1 and no variation (SoulGold)
            u32 seed = PlayerProfile_GetTrainerID_VisibleHalf(Sav2_PlayerData_GetProfileAddr(SaveBlock2_get())) ^ (id * 1103515245u);
            seed ^= seed >> 16;
            seed *= 2246822519u;
            seed ^= seed >> 13;
            scaledPartyLevel += 1 - seed % 2;
        }
    }

    for (i = 0; i < pokecount; i++) {
        mons[i] = AllocMonZeroed(heapID);
        // ivs field
        pow = buf[offset];
        offset++;

        // abilityslot field
        abilityslot = buf[offset];
        offset++;

        // level field
        level = buf[offset] | (buf[offset + 1] << 8);
        gLastPokemonLevelForMoneyCalc = level; // ends up being the last level at the end of the loop that we use for the money calc loop default case
        offset += 2;

        // species field
        species = buf[offset] | (buf[offset + 1] << 8);
        offset += 2;
        form_no = (species & 0xF800) >> 11;
        species &= 0x07FF;

        // item field - conditional
        if (bp->trainer_data[num].data_type & TRAINER_DATA_TYPE_ITEMS) {
            item = buf[offset] | (buf[offset + 1] << 8);
            offset += 2;
        }

        // moves field - conditional
        if (bp->trainer_data[num].data_type & TRAINER_DATA_TYPE_MOVES) {
            for (j = 0; j < 4; j++) {
                moves[j] = buf[offset] | (buf[offset + 1] << 8);
                offset += 2;
            }
        }

        // ability field
        if (bp->trainer_data[num].data_type & TRAINER_DATA_TYPE_ABILITY) {
            ability = buf[offset] | (buf[offset + 1] << 8);
            offset += 2;
        }

        // custom ball field
        if (bp->trainer_data[num].data_type & TRAINER_DATA_TYPE_BALL) {
            ball = buf[offset] | (buf[offset + 1] << 8);
            offset += 2;
        }

        // ivs and evs fields
        if (bp->trainer_data[num].data_type & TRAINER_DATA_TYPE_IV_EV_SET) {
            for (j = 0; j < 6; j++) {
                ivnums[j] = buf[offset];
                if (ivnums[j] > 31) {
                    ivnums[j] = 31;
                }
                offset++;
            }

            for (j = 0; j < 6; j++) {
                evnums[j] = buf[offset];
                offset++;
            }
        }

        // nature field
        if (bp->trainer_data[num].data_type & TRAINER_DATA_TYPE_NATURE_SET) {
            nature = buf[offset];
            offset++;
        }

        // shiny lock field
        if (bp->trainer_data[num].data_type & TRAINER_DATA_TYPE_SHINY_LOCK) {
            shinylock = buf[offset];
            offset++;
        }

        // reads extra flags from the trainer pokemon file
        if (bp->trainer_data[num].data_type & TRAINER_DATA_TYPE_ADDITIONAL_FLAGS) {
            additionalflags = buf[offset] | (buf[offset + 1] << 8) | (buf[offset + 2] << 16) | (buf[offset + 3] << 24);
            offset += 4;

            // status pre-set field
            if (additionalflags & TRAINER_DATA_EXTRA_TYPE_STATUS) {
                status = buf[offset] | (buf[offset + 1] << 8) | (buf[offset + 2] << 16) | (buf[offset + 3] << 24);
                offset += 4;
            }

            // custom hp stat field
            if (additionalflags & TRAINER_DATA_EXTRA_TYPE_HP) {
                hp = buf[offset] | (buf[offset + 1] << 8);
                offset += 2;
            }

            // custom atk stat field
            if (additionalflags & TRAINER_DATA_EXTRA_TYPE_ATK) {
                atk = buf[offset] | (buf[offset + 1] << 8);
                offset += 2;
            }

            // custom def stat field
            if (additionalflags & TRAINER_DATA_EXTRA_TYPE_DEF) {
                def = buf[offset] | (buf[offset + 1] << 8);
                offset += 2;
            }

            // custom speed stat field
            if (additionalflags & TRAINER_DATA_EXTRA_TYPE_SPEED) {
                speed = buf[offset] | (buf[offset + 1] << 8);
                offset += 2;
            }

            // custom spatk stat field
            if (additionalflags & TRAINER_DATA_EXTRA_TYPE_SP_ATK) {
                spatk = buf[offset] | (buf[offset + 1] << 8);
                offset += 2;
            }

            // custom spdef stat field
            if (additionalflags & TRAINER_DATA_EXTRA_TYPE_SP_DEF) {
                spdef = buf[offset] | (buf[offset + 1] << 8);
                offset += 2;
            }

            // move PP counts field
            if (additionalflags & TRAINER_DATA_EXTRA_TYPE_PP_COUNTS) {
                for (j = 0; j < 4; j++) {
                    ppcounts[j] = buf[offset];
                    offset++;
                }
            }

            // nickname field
            if (additionalflags & TRAINER_DATA_EXTRA_TYPE_NICKNAME) {
                for (j = 0; j < 11; j++) {
                    nickname[j] = buf[offset] | (buf[offset + 1] << 8);
                    offset += 2;
                }
            }
        }

        // ball seal field
        ballseal = buf[offset] | (buf[offset + 1] << 8);
        offset += 2;

        // now set mon data
        try_force_gender_maybe(species, form_no, abilityslot, &rnd_tmp);
        rnd = pow + level + species + bp->trainer_id[num];
        gf_srand(rnd);
        for (j = 0; j < bp->trainer_data[num].tr_type; j++) {
            rnd = gf_rand();
        }
        rnd = (rnd << 8) + rnd_tmp;
        pow = pow * 31 / 255;
        if (scaledPartyLevel) {
            u32 gap = highestLevel - level > 2 ? 2 : highestLevel - level;
            level = scaledPartyLevel > gap ? scaledPartyLevel - gap : 1;
            level = level > 100 ? 100 : level;
        }
        PokeParaSet(mons[i], species, level, pow, 1, rnd, 2, 0);
        SetMonData(mons[i], MON_DATA_FORM, &form_no);

        // set default abilities
        adjustedSpecies = PokeOtherFormMonsNoGet(species, form_no);
        ab1 = PokePersonalParaGet(adjustedSpecies, PERSONAL_ABILITY_1);
        ab2 = PokePersonalParaGet(adjustedSpecies, PERSONAL_ABILITY_2);
        if (ab2 != 0) {
            if (abilityslot & 1 || abilityslot == TRAINER_POKEMON_ABILITY_2) {
                SetMonData(mons[i], MON_DATA_ABILITY, (u16 *)&ab2);
            } else {
                SetMonData(mons[i], MON_DATA_ABILITY, (u16 *)&ab1);
            }
        } else {
            SetMonData(mons[i], MON_DATA_ABILITY, (u16 *)&ab1);
        }

        // if abilityslot is 2 force hidden ability with the bit set.  this specifically to cover darmanitan with zen mode switching between forms and such.
        if (abilityslot == TRAINER_POKEMON_ABILITY_HIDDEN) {
            u16 hiddenability = GetMonHiddenAbility(species, form_no);
            SET_MON_HIDDEN_ABILITY_BIT(mons[i]);
            SetMonData(mons[i], MON_DATA_ABILITY, (u16 *)&hiddenability);
        }

        if (bp->trainer_data[num].data_type & TRAINER_DATA_TYPE_ITEMS) {
            SetMonData(mons[i], MON_DATA_HELD_ITEM, &item);
        }
        if (bp->trainer_data[num].data_type & TRAINER_DATA_TYPE_MOVES) {
            for (j = 0; j < 4; j++) {
#ifdef BLOCK_LEARNING_UNIMPLEMENTED_MOVES
                if (IsMoveUnimplemented(moves[j])) {
                    moves[j] = MOVE_NONE;
                }
#endif
                SetPartyPokemonMoveAtPos(mons[i], moves[j], j);
            }
        }
        TrainerCBSet(ballseal, mons[i], heapID);
        if (bp->trainer_data[num].data_type & TRAINER_DATA_TYPE_ABILITY) {
            SetMonData(mons[i], MON_DATA_ABILITY, &ability);
        }
        if (bp->trainer_data[num].data_type & TRAINER_DATA_TYPE_BALL) {
            SetMonData(mons[i], MON_DATA_POKEBALL, &ball);
        }
        if (bp->trainer_data[num].data_type & TRAINER_DATA_TYPE_IV_EV_SET) {
            for (j = 0; j < 6; j++) {
                SetMonData(mons[i], MON_DATA_HP_IV + j, &ivnums[j]);
            }

            for (j = 0; j < 6; j++) {
                SetMonData(mons[i], MON_DATA_HP_EV + j, &evnums[j]);
            }
        }
        if (bp->trainer_data[num].data_type & TRAINER_DATA_TYPE_NATURE_SET) {
            u32 pid = GetMonData(mons[i], MON_DATA_PERSONALITY, NULL);
            u8 currentNature = pid % 25;
            pid = pid + nature - currentNature;
            SetMonData(mons[i], MON_DATA_PERSONALITY, &pid);
        }
        if (bp->trainer_data[num].data_type & TRAINER_DATA_TYPE_SHINY_LOCK) {
            u32 pid = GetMonData(mons[i], MON_DATA_PERSONALITY, NULL);
            if (shinylock != 0) {
                do {
                    id = (gf_rand() | (gf_rand() << 16));
                } while (!SHINY_CHECK(id, pid));
                SetMonData(mons[i], MON_DATA_OTID, &id);
            }
        }

        ChangeToBattleForm(mons[i]);

        RecalcPartyPokemonStats(mons[i]); // recalculate stats here

        if (bp->trainer_data[num].data_type & TRAINER_DATA_TYPE_ADDITIONAL_FLAGS) {
            if (additionalflags & TRAINER_DATA_EXTRA_TYPE_STATUS) {
                SetMonData(mons[i], MON_DATA_STATUS, &status);
            }
            if (additionalflags & TRAINER_DATA_EXTRA_TYPE_HP) {
                SetMonData(mons[i], MON_DATA_MAXHP, &hp);
                SetMonData(mons[i], MON_DATA_HP, &hp);
            }
            if (additionalflags & TRAINER_DATA_EXTRA_TYPE_ATK) {
                SetMonData(mons[i], MON_DATA_ATTACK, &atk);
            }
            if (additionalflags & TRAINER_DATA_EXTRA_TYPE_DEF) {
                SetMonData(mons[i], MON_DATA_DEFENSE, &def);
            }
            if (additionalflags & TRAINER_DATA_EXTRA_TYPE_SPEED) {
                SetMonData(mons[i], MON_DATA_SPEED, &speed);
            }
            if (additionalflags & TRAINER_DATA_EXTRA_TYPE_SP_ATK) {
                SetMonData(mons[i], MON_DATA_SPECIAL_ATTACK, &spatk);
            }
            if (additionalflags & TRAINER_DATA_EXTRA_TYPE_SP_DEF) {
                SetMonData(mons[i], MON_DATA_SPECIAL_DEFENSE, &spdef);
            }
            if (additionalflags & TRAINER_DATA_EXTRA_TYPE_PP_COUNTS) {
                for (j = 0; j < 4; j++) {
                    SetMonData(mons[i], MON_DATA_MOVE1PP + j, &ppcounts[j]);
                }
            }
            if (additionalflags & TRAINER_DATA_EXTRA_TYPE_NICKNAME) {
                u32 one = 1;

                SetMonData(mons[i], MON_DATA_HAS_NICKNAME, &one);
                SetMonData(mons[i], MON_DATA_NICKNAME, nickname);
            }
        }
        TrainerMonHandleFrustration(mons[i]);
    }

    for (i = 0; i < pokecount; i++) {
        PokeParty_Add(bp->poke_party[num], mons[partyOrder[i]]);
        sys_FreeMemoryEz(mons[i]);
    }

    sys_FreeMemoryEz(buf);
    sys_FreeMemoryEz(nickname);

    gf_srand(seed_tmp);

#ifdef DEBUG_BATTLE_SCENARIOS
    // Override parties with test scenario if enabled
    TestBattle_OverrideParties(bp);
#endif

    // Change battle forms for player party
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 6; j++) {
            struct PartyPokemon *mon = Party_GetMonByIndex(bp->poke_party[i], j);
            if (mon != NULL) {
                ChangeToBattleForm(mon);
                RecalcPartyPokemonStats(mon);
            }
        }
    }
}

extern u32 space_for_setmondata;

/**
 *  @brief REQ-TGT-SYS-007: the shiny_odds setting (1/8192..1/256) as one extra roll on top of the base 1/8192, so the
 *         total is ~1/N. Wild mons only, like H&S; SHINY_ODDS itself stays, as changing it would flip existing mons.
 *
 *  @param mon wild PartyPokemon whose personality may be rerolled to a shiny one
 */
static void TryApplyShinyOddsSetting(struct PartyPokemon *mon)
{
    static const u16 sOneIn[] = { 8192, 4096, 2048, 1024, 512, 256 }; // SETTING_SHINY_ODDS_* order
    u32 otId = GetMonData(mon, MON_DATA_OTID, NULL), pid = GetMonData(mon, MON_DATA_PERSONALITY, NULL);
    if (CalcShininessByOtIdAndPersonality(otId, pid) || gf_rand() % 8192 >= 8192 / sOneIn[PMFG_GetSetting(SETTING_SHINY_ODDS)] - 1) {
        return;
    }
    pid = GenerateShinyPIDKeepSubstructuresIntact(otId, pid);
    SetMonData(mon, MON_DATA_PERSONALITY, &pid);
    RecalcPartyPokemonStats(mon);
    ResetPartyPokemonAbility(mon);
}

/**
 *  @brief REQ-TGT-TRN-002 (refs/soulgold/src/level_scaling.c:542-558, include/config/level_scaling.h:52-58): wild
 *         level = party average - 8 - 0..2, never below 3.
 *         shortcut: the rolled level is the floor (SoulGold floors at the slot minimum, which this hook can't see), so
 *         scaling only raises levels; scripted wild battles through this hook scale too. Revisit if statics feel off.
 *
 *  @param mon wild PartyPokemon to re-level
 *  @param fightType battle type flags (the Bug-Catching Contest never scales, as in SoulGold)
 */
static void TryApplyWildLevelScaling(struct PartyPokemon *mon, u32 fightType)
{
    if (PMFG_GetSetting(SETTING_WILD_LEVEL_SCALING) != SETTING_WILD_LEVEL_SCALING_ON || (fightType & BATTLE_TYPE_BUG_CONTEST)) {
        return;
    }
    s32 level = (s32)PMFG_PartyScalingLevel() - 8 - gf_rand() % 3;
    if (level <= (s32)GetMonData(mon, MON_DATA_LEVEL, NULL)) {
        return;
    }
    u32 exp = GetExpByGrowthRateAndLevel(PokePersonalParaGet(GetMonData(mon, MON_DATA_SPECIES, NULL), PERSONAL_EXP_GROUP), level);
    SetMonData(mon, MON_DATA_EXPERIENCE, &exp);
    SetMonData(mon, MON_DATA_LEVEL, &level);
    RecalcPartyPokemonStats(mon);
    InitBoxMonMoveset(&mon->box);
}

/**
 *  @brief add a PartyPokemon to the "wild battler"'s party
 *
 *  @param inTarget battler whose party to add to
 *  @param encounterInfo various encounter information structure
 *  @param encounterPartyPokemon PartyPokemon to modify and add
 *  @param encounterBattleParam battle param
 *  @return TRUE if PokeParty_Add was successful
 */
BOOL LONG_CALL AddWildPartyPokemon(int inTarget, EncounterInfo *encounterInfo, struct PartyPokemon *encounterPartyPokemon, struct BATTLE_PARAM *encounterBattleParam)
{
    int range = 0;
    u8 change_form = 0;
    u8 form_no;
    u16 species;

    if (encounterInfo->isEgg == 0 && encounterInfo->ability == ABILITY_COMPOUND_EYES) {
        range = 1;
    }

    species = GetMonData(encounterPartyPokemon, MON_DATA_SPECIES, NULL);

    if (space_for_setmondata != 0) {
        change_form = 1;
        form_no = space_for_setmondata; //(species & 0xF800) >> 11;
        space_for_setmondata = 0;
    }

    WildMonSetRandomHeldItem(encounterPartyPokemon, encounterBattleParam->fight_type, range);

    if (species == SPECIES_UNOWN) {
        change_form = 1;
        form_no = GrabAndRegisterUnownForm(encounterInfo);
    } else if (species == SPECIES_DEERLING || species == SPECIES_SAWSBUCK) {
        UpdatePassiveForms(encounterPartyPokemon);
    }

    if (CheckScriptFlag(HIDDEN_ABILITIES_FLAG) == 1) {
        SET_MON_HIDDEN_ABILITY_BIT(encounterPartyPokemon)
        ClearScriptFlag(HIDDEN_ABILITIES_FLAG);
        ResetPartyPokemonAbility(encounterPartyPokemon);
    }

    if (change_form) {
        SetMonData(encounterPartyPokemon, MON_DATA_FORM, (u8 *)&form_no);
        RecalcPartyPokemonStats(encounterPartyPokemon);
        ResetPartyPokemonAbility(encounterPartyPokemon);
        InitBoxMonMoveset(&encounterPartyPokemon->box);
    }

    TryApplyWildLevelScaling(encounterPartyPokemon, encounterBattleParam->fight_type);
    TryApplyShinyOddsSetting(encounterPartyPokemon);
    ChangeToBattleForm(encounterPartyPokemon);

    return PokeParty_Add(encounterBattleParam->poke_party[inTarget], encounterPartyPokemon);
}

void LONG_CALL SetupAndStartTutorialBattle(TaskManager *taskManager)
{
    struct BattleSetup *setup = BattleSetup_New_Tutorial(11, taskManager->fieldSystem);

    struct PartyPokemon *marill = Party_GetMonByIndex(setup->party[BATTLER_PLAYER], 0);

    // move slot 1 is tackle
    u16 data = MOVE_TACKLE;
    SetMonData(marill, MON_DATA_MOVE1, &data);
    data = GetMoveMaxPP(data, 0);
    SetMonData(marill, MON_DATA_MOVE1PP, &data);
    data = 0;
    SetMonData(marill, MON_DATA_MOVE1PPUP, &data);

    // move slot 2 is tail whip
    data = MOVE_TAIL_WHIP;
    SetMonData(marill, MON_DATA_MOVE2, &data);
    data = GetMoveMaxPP(data, 0);
    SetMonData(marill, MON_DATA_MOVE2PP, &data);
    data = 0;
    SetMonData(marill, MON_DATA_MOVE2PPUP, &data);

    // move slot 3 and 4 none
    data = MOVE_NONE;
    SetMonData(marill, MON_DATA_MOVE3, &data);
    SetMonData(marill, MON_DATA_MOVE4, &data);

    void *encounter = Encounter_New(setup, BattleSetup_GetWildTransitionEffect(setup), BattleSetup_GetWildBattleMusic(setup), NULL);

    TaskManager_Call(taskManager, Task_TutorialBattle, encounter);
}
