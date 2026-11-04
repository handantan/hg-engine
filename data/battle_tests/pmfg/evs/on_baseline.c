// Test: PMFG EVs - ON: the KO gives Magikarp's 1 Speed EV
// REQ-TGT-SYS-004: criterion 3. Parties as exp/x1_baseline.c (level_cap set so the test battle gives EXP, and EVs with it); the EV check runs at the message after the EXP step (battle messages are expanded inside that step); on_baseline.c shows EVs are given by then.
#include "../../battle_tests.h"
BEGIN_TEST
{
    .battleType = BATTLE_TYPE_TRAINER,
    .weather = FIELD_CONDITION_NONE,
    .fieldCondition = 0,
    .terrain = TERRAIN_NONE,
    .playerParty = {
        {
            .species = SPECIES_SNORLAX,
            .level = 10,
            .form = 0,
            .ability = ABILITY_THICK_FAT,
            .item = ITEM_NONE,
            .moves = { MOVE_TACKLE, MOVE_NONE, MOVE_NONE, MOVE_NONE },
            .hp = FULL_HP,
            .status = 0,
            .condition2 = 0,
            .moveEffectFlags = 0,
        },
        { .species = SPECIES_NONE },
        { .species = SPECIES_NONE },
        { .species = SPECIES_NONE },
        { .species = SPECIES_NONE },
        { .species = SPECIES_NONE },
    },
    .enemyParty = {
        {
            .species = SPECIES_MAGIKARP,
            .level = 10,
            .form = 0,
            .ability = ABILITY_SWIFT_SWIM,
            .item = ITEM_NONE,
            .moves = { MOVE_SPLASH, MOVE_NONE, MOVE_NONE, MOVE_NONE },
            .hp = 1,
            .status = 0,
            .condition2 = 0,
            .moveEffectFlags = 0,
        },
        { .species = SPECIES_NONE },
        { .species = SPECIES_NONE },
        { .species = SPECIES_NONE },
        { .species = SPECIES_NONE },
        { .species = SPECIES_NONE },
    },
    .playerScript = {
        {
            { ACTION_MOVE_SLOT_1, BATTLER_ENEMY_FIRST },
            { ACTION_NONE, 0 },
        },
        {
            { ACTION_NONE, 0 },
        },
    },
    .enemyScript = {
        {
            { ACTION_MOVE_SLOT_1, BATTLER_PLAYER_FIRST },
            { ACTION_NONE, 0 },
        },
        {
            { ACTION_NONE, 0 },
        },
    },
    .expectations = {
        { .expectationType = EXPECTATION_TYPE_MESSAGE_CONTAINS, .expectationValue.message = "Magikarp fainted" },
        { .expectationType = EXPECTATION_TYPE_MESSAGE_CONTAINS, .expectationValue.message = "Exp. Points" },
        { .expectationType = EXPECTATION_TYPE_PARTY_EV_TOTAL, .battlerIDOrPartySlot = 0, .expectationValue.evTotal = 1 },
        { .expectationType = EXPECTATION_TYPE_MESSAGE_CONTAINS, .expectationValue.message = "You defeated" },
    },
    .settings = { PMFG_SETTING(LEVEL_CAP, SOFT) },
}
END_TEST
