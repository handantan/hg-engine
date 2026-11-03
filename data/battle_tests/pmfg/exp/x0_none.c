// Test: PMFG EXP - x0 gives no EXP
// REQ-TGT-SYS-003: criterion 2. Same parties as exp/x1_baseline.c.
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
        { .expectationType = EXPECTATION_TYPE_MESSAGE_DOES_NOT_CONTAIN, .expectationValue.message = "Exp. Points" },
    },
    .settings = { PMFG_SETTING(LEVEL_CAP, SOFT), PMFG_SETTING(EXP_MULTIPLIER, X0) },
}
END_TEST
