// PMFG B-041: moved from src/test_battle.c (overlay 129 is full in test builds); only called from enemy_party.c here.
#include "../../include/config.h"
#include "../../include/debug.h"
#include "../../include/types.h"

#ifdef DEBUG_BATTLE_SCENARIOS

#include "../../include/battle.h"
#include "../../include/constants/item.h"
#include "../../include/constants/moves.h"
#include "../../include/constants/species.h"
#include "../../include/pokemon.h"
#include "../../include/test_battle.h"

/**
 * @brief Override a single Pokemon with test scenario data
 *
 * @param mon Pokemon to override
 * @param species Species ID
 * @param level Level
 * @param form Form number (0 = base form)
 * @param ability Ability ID
 * @param item Held item ID
 * @param moves Array of 4 move IDs
 * @param hp Custom HP (0 = use max)
 * @param status Status condition
 */
static void LONG_CALL TestBattle_OverridePokemon(struct PartyPokemon *mon, u16 species, u8 level, u8 form, u16 ability, u16 item, u16 moves[4], u16 hp, u32 status, const u16 furtherParams[NUM_FURTHER_TEST_PARAMS][2])
{
    int i;

    PokeParaSet(mon, species, level, 31, FALSE, 0, 0, 0);
    SetMonData(mon, MON_DATA_FORM, &form);
    SetMonData(mon, MON_DATA_ABILITY, &ability);
    SetMonData(mon, MON_DATA_HELD_ITEM, &item);
    SET_MON_NATURE_OVERRIDE(mon, NATURE_HARDY);

    // moves
    for (i = 0; i < 4; i++) {
        if (moves[i] != MOVE_NONE) {
            u16 move = moves[i];
            SetMonData(mon, MON_DATA_MOVE1 + i, &move);
            u32 pp = GetMoveMaxPP(move, 0);
            SetMonData(mon, MON_DATA_MOVE1MAXPP + i, &pp);
            SetMonData(mon, MON_DATA_MOVE1PP + i, &pp);
        } else {
            // PMFG: clear the slot too; a learnset move left here with 0 PP is still callable by Sleep Talk (B-032)
            u16 none = MOVE_NONE;
            SetMonData(mon, MON_DATA_MOVE1 + i, &none);
            u32 pp = 0;
            SetMonData(mon, MON_DATA_MOVE1PP + i, &pp);
            SetMonData(mon, MON_DATA_MOVE1MAXPP + i, &pp);
        }
    }

    if (hp == FULL_HP) {
        u16 maxHP = (u16)GetMonData(mon, MON_DATA_MAXHP, NULL);
        SetMonData(mon, MON_DATA_HP, &maxHP);
    } else {
        SetMonData(mon, MON_DATA_HP, &hp);
    }

    SetMonData(mon, MON_DATA_STATUS, &status);

    u8 friendship = 255;
    SetMonData(mon, MON_DATA_FRIENDSHIP, &friendship);

    for (i = 0; i < NUM_FURTHER_TEST_PARAMS; i++) {
        if (furtherParams[i][0] == 0) {
            continue;
        } else {
            u16 param = furtherParams[i][1];
            SetMonData(mon, furtherParams[i][0], &param);
        }
    }
    RecalcPartyPokemonStats(mon);
}

/**
 * @brief Helper to override a single party member
 *
 * @param bp Battle param structure
 * @param partyIndex Which party (0 = player, 1 = enemy/trainer)
 * @param slot Which slot in the party (0-5)
 * @param species Species ID
 * @param level Level
 * @param form Form number (0 = base form)
 * @param ability Ability ID
 * @param item Held item ID
 * @param moves Array of 4 move IDs
 * @param hp Custom HP (0 = use max)
 * @param status Status condition
 */
static void OverridePartySlot(struct BATTLE_PARAM *bp, int partyIndex, int slot, u16 species, u8 level, u8 form, u16 ability, u16 item, u16 moves[4], u16 hp, u32 status, const u16 furtherParams[NUM_FURTHER_TEST_PARAMS][2])
{
    if (bp->poke_party[partyIndex] == NULL) {
        return;
    }

    struct PartyPokemon *mon = Party_GetMonByIndex(bp->poke_party[partyIndex], slot);
    if (mon != NULL) {
        TestBattle_OverridePokemon(mon, species, level, form, ability, item, moves, hp, status, furtherParams);
    }
}

/**
 * @brief Overrides battle parties with test scenario
 *
 * This should be called after MakeTrainerPokemonParty creates the trainer's party.
 * It will override both player and trainer parties with test scenario data.
 *
 * @param bp Battle param structure
 */
void LONG_CALL TestBattle_OverrideParties(struct BATTLE_PARAM *bp)
{
    const struct TestBattleScenario *scenario = TestBattle_BeginScenario();
    if (scenario == NULL) {
        return;
    }

    int enemyCount = 0;
    for (int i = 0; i < 6; i++) {
        if (scenario->enemyParty[i].species != 0) {
            enemyCount++;
        }
    }

    if (scenario->battleType & BATTLE_TYPE_DOUBLES) {
        bp->fight_type = BATTLE_TYPE_TRAINER | scenario->battleType;
    }

    // Override player's Pokemon (party 0)
    int playerCount = 0;
    for (int slot = 0; slot < 6; slot++) {
        const struct TestBattlePokemon *mon = &scenario->playerParty[slot];

        if (mon->species == 0) {
            break; // Stop at first empty slot
        }

        OverridePartySlot(bp, 0, slot, mon->species, mon->level, mon->form, mon->ability, mon->item, (u16 *)mon->moves, mon->hp, mon->status, mon->furtherParams);
        playerCount++;
    }
    if (bp->poke_party[0] != NULL) {
        bp->poke_party[0]->count = playerCount;
    }

    // Override enemy's Pokemon (party 1)
    if (enemyCount > 0) {
        bp->trainer_data[1].poke_count = enemyCount;

        if (bp->poke_party[1] != NULL) {
            bp->poke_party[1]->count = enemyCount;
            for (int i = 0; i < enemyCount; i++) {
                struct PartyPokemon *mon = Party_GetMonByIndex(bp->poke_party[1], i);
                if (mon == NULL) {
                    // Slot doesn't exist, add a dummy that we'll override later
                    struct PartyPokemon tempMon;
                    PokeParaSet(&tempMon, SPECIES_BULBASAUR, 5, 0, FALSE, 0, 0, 0);
                    PokeParty_Add(bp->poke_party[1], &tempMon);
                }
            }
        }
    }
    for (int slot = 0; slot < 6; slot++) {
        const struct TestBattlePokemon *mon = &scenario->enemyParty[slot];

        if (mon->species == 0) {
            break;
        }

        OverridePartySlot(bp, 1, slot, mon->species, mon->level, mon->form, mon->ability, mon->item, (u16 *)mon->moves, mon->hp, mon->status, mon->furtherParams);
    }
}

#endif // DEBUG_BATTLE_SCENARIOS
