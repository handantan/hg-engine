// Test: PMFG Level cap - SOFT, 1 level over the cap gives EXP / 8
// REQ-TGT-SYS-002: criterion 2 (EXP / 4 at the cap, halved per level over). Same parties as exp/x1_baseline.c. The HARD table puts the cap at 9: 180 / 8 = 22.
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
        { .expectationType = EXPECTATION_TYPE_MESSAGE_CONTAINS, .expectationValue.message = "gained a boosted 22 Exp. Points" },
    },
    .settings = { PMFG_SETTING(LEVEL_CAP, SOFT), PMFG_SETTING(LEVEL_CAP_TABLE, HARD) },
}
END_TEST
