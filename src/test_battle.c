#include "../include/config.h"
#include "../include/debug.h"
#include "../include/types.h"

#ifdef DEBUG_BATTLE_SCENARIOS

#include "../include/battle.h"
#include "../include/constants/file.h"
#include "../include/constants/generated/test_battle.h"
#include "../include/constants/item.h"
#include "../include/constants/moves.h"
#include "../include/constants/species.h"
#include "../include/pokemon.h"
#include "../include/test_battle.h"

#define TEST_START_INDEX 0

static u32 gTestEndIndex = TEST_BATTLE_TOTAL_TESTS;
static BOOL overridden = FALSE;

// Layout:
//   bits 0-3:   scriptIndex[0] (0-8)
//   bits 4-7:   scriptIndex[1] (0-8)
//   bits 8-11:  scriptIndex[2] (0-8)
//   bits 12-15: scriptIndex[3] (0-8)
//   bits 16-19: expectationPassCount (0-8)
//   bit 20:     testComplete
//   bit 21:     hasMoreTests
//   bits 22-31: currentTestIndex (0-1023)
static u32 gTestBattleState = (TEST_START_INDEX << 22);

static struct TestBattleScenario *sCurrentScenario = NULL;

static int GetScriptIndex(int battler)
{
    return (gTestBattleState >> (battler * 4)) & STATE_SCRIPT_IDX_MASK;
}

static void SetScriptIndex(int battler, int value)
{
    int shift = battler * 4;
    gTestBattleState = (gTestBattleState & ~(STATE_SCRIPT_IDX_MASK << shift)) | ((value & STATE_SCRIPT_IDX_MASK) << shift);
}

static void IncrementScriptIndex(int battler)
{
    SetScriptIndex(battler, GetScriptIndex(battler) + 1);
}

static int GetCurrentTestIndex(void)
{
    return (gTestBattleState >> STATE_TEST_INDEX_SHIFT) & STATE_TEST_INDEX_MASK;
}

static void SetCurrentTestIndex(int value)
{
    gTestBattleState = (gTestBattleState & ~(STATE_TEST_INDEX_MASK << STATE_TEST_INDEX_SHIFT)) | ((value & STATE_TEST_INDEX_MASK) << STATE_TEST_INDEX_SHIFT);
}

static BOOL IsTestComplete(void)
{
    return (gTestBattleState & STATE_COMPLETE_BIT) != 0;
}

static void SetTestComplete(BOOL complete)
{
    if (complete) {
        gTestBattleState |= STATE_COMPLETE_BIT;
    } else {
        gTestBattleState &= ~STATE_COMPLETE_BIT;
    }
}

static BOOL HasMoreTests(void)
{
    return (gTestBattleState & STATE_HAS_MORE_BIT) != 0;
}

static void SetHasMoreTests(BOOL hasMore)
{
    if (hasMore) {
        gTestBattleState |= STATE_HAS_MORE_BIT;
    } else {
        gTestBattleState &= ~STATE_HAS_MORE_BIT;
    }
}

static void ResetScriptIndices(void)
{
    gTestBattleState &= ~0xFFFF; // Clear bits 0-15 (all 4 script indices)
}

static void FreeCurrentScenario(void)
{
    if (sCurrentScenario != NULL) {
        sys_FreeMemoryEz(sCurrentScenario);
        sCurrentScenario = NULL;
    }
}

static void AllocAndLoadScenario(void)
{
    FreeCurrentScenario();
    sCurrentScenario = sys_AllocMemory(HEAPID_DEFAULT, sizeof(struct TestBattleScenario));
    if (sCurrentScenario != NULL) {
        int testIndex = GetCurrentTestIndex();
        ReadFromNarcMemberByIdPair(sCurrentScenario, ARC_CODE_ADDONS, CODE_ADDON_BATTLE_TESTS, testIndex * sizeof(struct TestBattleScenario), sizeof(struct TestBattleScenario));
    }
}

// TODO: there is definitely some better way to do this, so that we don't need to worry if somehow this address is used by something else
int *g_EmulatorCommunicationSendHole = (int *)0x02FFF81C;

void LONG_CALL SendValueThroughCommunicationSendHole(int value)
{
    *g_EmulatorCommunicationSendHole = value;
}

int LONG_CALL ReadValueThroughCommunicationSendHole()
{
    return *g_EmulatorCommunicationSendHole;
}

struct TestBattleScenario *LONG_CALL TestBattle_GetCurrentScenario()
{
    return sCurrentScenario;
}

BOOL LONG_CALL TestBattle_HasMoreExpectations()
{
    if (sCurrentScenario == NULL) {
        return FALSE;
    }
    return sCurrentScenario->expectationPassCount != MAX_EXPECTATIONS && sCurrentScenario->expectations[sCurrentScenario->expectationPassCount].expectationType != 0;
}

static u8 sTeraWant; // PMFG REQ-TGT-BTL-007: No2Bit(battler) of ACTION_TERA this turn

/**
 *  @brief PMFG REQ-TGT-BTL-007: the battlers whose scripted action this turn has ACTION_TERA (then cleared); read by
 *  PMFG_TryTerastallize (src/battle/pmfg_tera.c), which can't reach this overlay's ctx-less AI path otherwise
 */
u8 LONG_CALL TestBattle_TakeTeraWants(void)
{
    u8 want = sTeraWant;
    sTeraWant = 0;
    return want;
}

static u8 sZWant; // PMFG REQ-TGT-BTL-010: No2Bit(battler) of ACTION_Z this turn; read by PMFG_TryZMove (BattleController_BeforeMove.c)
u8 LONG_CALL TestBattle_TakeZWants(void)
{
    u8 want = sZWant;
    sZWant = 0;
    return want;
}

/**
 * @brief Loads the current test's scenario and resets its state; NULL when there is none.
 * PMFG B-041: the party override that uses it lives in src/field/test_battle_party.c (overlay 129 is full)
 */
const struct TestBattleScenario *LONG_CALL TestBattle_BeginScenario(void)
{
    u32 testIndex = GetCurrentTestIndex();
    SetTestComplete(FALSE);
    SetHasMoreTests((testIndex + 1) < gTestEndIndex);

    AllocAndLoadScenario();
    if (sCurrentScenario != NULL) {
        ResetScriptIndices();
    }
    return sCurrentScenario;
}

/**
 * @brief Apply battle state from scenario to battlers and field
 *
 * This should be called after battle initialization but before the first turn.
 * It applies all status, condition, and field flags from the test scenario.
 *
 * @param bw Battle system pointer
 * @param sp Battle struct pointer
 */
void LONG_CALL TestBattle_ApplyBattleState(struct BattleStruct *sp)
{
    if (sCurrentScenario == NULL) {
        return;
    }

    // In doubles, we need to ensure both battlers are properly linked to party slots
    if (sCurrentScenario->battleType & BATTLE_TYPE_DOUBLES) {
        // Player
        sp->sel_mons_no[BATTLER_PLAYER_FIRST] = 0;
        if (sCurrentScenario->playerParty[1].species != 0) {
            sp->sel_mons_no[BATTLER_PLAYER_SECOND] = 1;
        }

        // Enemy
        sp->sel_mons_no[BATTLER_ENEMY_FIRST] = 0;
        if (sCurrentScenario->enemyParty[1].species != 0) {
            sp->sel_mons_no[BATTLER_ENEMY_SECOND] = 1;
        }
    }

    // Apply player Pokemon status and conditions (battlers 0-1 in doubles, 0 in singles)
    for (int slot = 0; slot < 2; slot++) {
        const struct TestBattlePokemon *mon = &sCurrentScenario->playerParty[slot];
        int battlerId = (slot == 0) ? BATTLER_PLAYER_FIRST : BATTLER_PLAYER_SECOND;

        if (mon->species == 0) {
            continue;
        }

        if (mon->status) {
            sp->battlemon[battlerId].condition |= mon->status;
        }

        if (mon->condition2) {
            sp->battlemon[battlerId].condition2 |= mon->condition2;

            if (mon->condition2 & STATUS2_RECHARGE) {
                sp->battlemon[battlerId].moveeffect.rechargeCount = 2; // Skip 1 turn

                // lock choice item to first move
                if (mon->item == ITEM_CHOICE_BAND || mon->item == ITEM_CHOICE_SPECS || mon->item == ITEM_CHOICE_SCARF) {
                    sp->battlemon[battlerId].moveeffect.moveNoChoice = mon->moves[0];
                }
            }
        }

        if (mon->moveEffectFlags) {
            sp->battlemon[battlerId].effect_of_moves |= mon->moveEffectFlags;
        }
    }

    // Apply enemy Pokemon status and conditions (battlers 2-3 in doubles, 1 in singles)
    for (int slot = 0; slot < 2; slot++) {
        const struct TestBattlePokemon *mon = &sCurrentScenario->enemyParty[slot];
        int battlerId = (slot == 0) ? BATTLER_ENEMY_FIRST : BATTLER_ENEMY_SECOND;

        if (mon->species == 0) {
            continue;
        }

        if (mon->status) {
            sp->battlemon[battlerId].condition |= mon->status;
        }

        if (mon->condition2) {
            sp->battlemon[battlerId].condition2 |= mon->condition2;

            if (mon->condition2 & STATUS2_RECHARGE) {
                sp->battlemon[battlerId].moveeffect.rechargeCount = 2; // Skip 1 turn

                // lock choice item to first move
                if (mon->item == ITEM_CHOICE_BAND || mon->item == ITEM_CHOICE_SPECS || mon->item == ITEM_CHOICE_SCARF) {
                    sp->battlemon[battlerId].moveeffect.moveNoChoice = mon->moves[0];
                }
            }
        }

        if (mon->moveEffectFlags) {
            sp->battlemon[battlerId].effect_of_moves |= mon->moveEffectFlags;
        }
    }

    if (sCurrentScenario->weather) {
        sp->field_condition |= sCurrentScenario->weather;
    }

    if (sCurrentScenario->fieldCondition) {
        sp->field_condition |= sCurrentScenario->fieldCondition;
    }

    sp->terrainOverlay.type = TERRAIN_NONE;
    sp->terrainOverlay.numberOfTurnsLeft = 0;

    if (sCurrentScenario->terrain != TERRAIN_NONE) {
        sp->terrainOverlay.type = sCurrentScenario->terrain;
        sp->terrainOverlay.numberOfTurnsLeft = 255;
    }
}

/**
 * @brief Check if all battler scripts have been exhausted
 *
 * @return TRUE if all active battlers have completed their scripts, FALSE otherwise
 */
static BOOL LONG_CALL TestBattle_TestComplete()
{
    if (sCurrentScenario == NULL) {
        return FALSE;
    }

    int maxBattlers = (sCurrentScenario->battleType & BATTLE_TYPE_DOUBLES) ? 4 : 2;
    for (int i = 0; i < maxBattlers; i++) {
        const struct BattleAction *script;
        int battlerIndex = (i == BATTLER_PLAYER_FIRST || i == BATTLER_ENEMY_FIRST) ? 0 : 1;

        if (i == BATTLER_PLAYER_FIRST || i == BATTLER_PLAYER_SECOND) {
            script = sCurrentScenario->playerScript[battlerIndex];
        } else {
            script = sCurrentScenario->enemyScript[battlerIndex];
        }

        int scriptIndex = GetScriptIndex(i);
        if (scriptIndex < AI_SCRIPT_MAX_MOVES && script[scriptIndex].action != ACTION_NONE) {
            return FALSE;
        }
    }

    return TRUE;
}

/**
 * @brief Check if test battle scripts are complete and update completion flag
 *
 * This should be called after each turn or after all battlers act.
 * When scripts are complete, it sets the completion flag and logs TEST_END.
 */
static void LONG_CALL TestBattle_CheckScriptCompletion()
{
    if (sCurrentScenario == NULL || IsTestComplete()) {
        return;
    }

    if (TestBattle_TestComplete()) {
        SetTestComplete(TRUE);
        // if (TestBattle_HasMoreExpectations()) {
        //     *g_EmulatorCommunicationSendHole = TEST_CASE_FAIL;
        // } else {
        //     *g_EmulatorCommunicationSendHole = TEST_CASE_PASS;
        // }
    }
}

/**
 * @brief Check if test battle is marked as complete
 *
 * @return TRUE if test battle should end
 */
BOOL LONG_CALL TestBattle_IsComplete()
{
    return IsTestComplete();
}

// PMFG: a KO win ends the battle before the next turn's completion check, which left the next test unqueued
void LONG_CALL TestBattle_MarkComplete()
{
    SetTestComplete(TRUE);
}

void LONG_CALL TestBattle_QueueNextTest()
{
    if (!overridden) {
        u32 value = ReadValueThroughCommunicationSendHole();
        u32 currentIndex = value & 0xFFFF;
        // debug_printf("First test index: %d\n", currentIndex);
        SetCurrentTestIndex(currentIndex);
        gTestEndIndex = value >> 16;
        // debug_printf("Last test index: %d\n", gTestEndIndex - 1);
        overridden = TRUE;
    }
    if (IsTestComplete()) {
        SetCurrentTestIndex(GetCurrentTestIndex() + 1);
        SetTestComplete(FALSE);
    }
    SetHasMoreTests(FALSE); // set this to false temporarily so the script only loads once
    FreeCurrentScenario();
}

/**
 * @brief Check if there are more tests to run after the current one
 *
 * @return TRUE if more tests are available and the current one is done, FALSE otherwise
 */
BOOL LONG_CALL TestBattle_HasMoreTests()
{
    return IsTestComplete() && HasMoreTests();
}

/**
 * @brief Get the next scripted move for player or enemy
 *
 * This function is called before move decision. If there's a scripted
 * move available, it returns that move and advances the script. Otherwise returns 0
 * to let normal input/AI decide.
 *
 * @param battlerId The battler making the decision (0-3)
 * @param moveSlot Output parameter - which move slot (0-3) contains the scripted move
 * @param target Output parameter - which battler to target (0-3)
 * @return Move ID to use, or 0 if no override
 */
void LONG_CALL TestBattle_GetAIScriptedMove(int battlerId, u8 *moveSlot, u8 *target)
{
    *moveSlot = (u8)0;
    *target = (u8)0;

    if (sCurrentScenario == NULL) {
        return;
    }

    if (battlerId < 0 || battlerId >= 4) {
        return;
    }

    const struct BattleAction *script;
    int battlerIndex = (battlerId == BATTLER_PLAYER_FIRST || battlerId == BATTLER_ENEMY_FIRST) ? 0 : 1;

    if (battlerId == BATTLER_PLAYER_FIRST || battlerId == BATTLER_PLAYER_SECOND) {
        script = sCurrentScenario->playerScript[battlerIndex];
    } else {
        script = sCurrentScenario->enemyScript[battlerIndex];
    }

    int scriptIndex = GetScriptIndex(battlerId);

    if (scriptIndex >= AI_SCRIPT_MAX_MOVES) {
        return;
    }

    struct BattleAction action = script[scriptIndex];

    if (action.action == ACTION_NONE) {
        return;
    }

    if ((action.action & ~ACTION_GIMMICKS) <= ACTION_MOVE_SLOT_4) {
        *moveSlot = action.action & ~ACTION_GIMMICKS;
        if (action.action & ACTION_Z) {
            sZWant |= No2Bit(battlerId);
        }
        *target = action.target;
        if (action.action & ACTION_TERA) {
            sTeraWant |= No2Bit(battlerId);
        }
    }
    IncrementScriptIndex(battlerId);
}

/**
 * @brief AI move selection for test battles
 *
 * @param bsys Battle system pointer
 * @param battler The battler making the decision (1 or 3 for enemies in doubles)
 * @return The move slot (0-3) to use
 */
u8 LONG_CALL TestBattle_AISelectMove(struct BattleSystem *bsys, int battler)
{
    u8 moveSlot = 0;
    u8 target = 0;
    TestBattle_GetAIScriptedMove(battler, &moveSlot, &target);
    bsys->sp->waza_no_pos[battler] = moveSlot;
    bsys->sp->aiWorkTable.ai_dir_select_client[battler] = target;
    return moveSlot;
}

/**
 * @brief AI command picking for test battles - decides whether to fight or switch
 *
 * @param bsys Battle system pointer
 * @param battler The battler making the decision
 * @return 1 for FIGHT, 2 for ITEM, 3 for SWITCH (matches original AI function)
 */
int LONG_CALL TestBattle_AIPickCommand(struct BattleSystem *bsys, int battler)
{
    if (battler == BATTLER_PLAYER_FIRST || battler == BATTLER_PLAYER_SECOND) {
        return 1; // FIGHT
    }

    if (sCurrentScenario == NULL) {
        return 1; // FIGHT
    }

    // Add safety check for bsys and sp
    if (bsys == NULL || bsys->sp == NULL) {
        return 1; // FIGHT
    }

    // Determine which script array and index to use
    const struct BattleAction *script;
    int battlerIndex = (battler == BATTLER_PLAYER_FIRST || battler == BATTLER_ENEMY_FIRST) ? 0 : 1;

    if (battler == BATTLER_PLAYER_FIRST || battler == BATTLER_PLAYER_SECOND) {
        script = sCurrentScenario->playerScript[battlerIndex];
    } else {
        script = sCurrentScenario->enemyScript[battlerIndex];
    }

    TestBattle_CheckScriptCompletion();

    if (IsTestComplete()) {
        return 1; // will be ignored as battle ends
    }

    int scriptIndex = GetScriptIndex(battler);

    if (scriptIndex >= AI_SCRIPT_MAX_MOVES) {
        return 1; // FIGHT
    }

    struct BattleAction action = script[scriptIndex];

    if (action.action >= ACTION_SWITCH_SLOT_0 && action.action <= ACTION_SWITCH_SLOT_5) {
        u8 partySlot = action.action - ACTION_SWITCH_SLOT_0;
        bsys->sp->ai_reshuffle_sel_mons_no[battler] = partySlot;
        IncrementScriptIndex(battler);
        return 3; // SWITCH
    }

    return 1; // FIGHT
}

// send out pokemon in order
int LONG_CALL TestBattle_PostKOSwitchIn(struct BattleSystem *bsys UNUSED, int battler UNUSED)
{
    return 6;
}

static BOOL TestBattle_IsPartySlotAlreadyActive(struct BattleSystem *bsys, struct BattleStruct *ctx, int battler, int partySlot)
{
    int maxBattlers = BattleWorkClientSetMaxGet(bsys);
    u8 side = IsClientEnemy(bsys, battler);

    for (int other = 0; other < maxBattlers; other++) {
        if (IsClientEnemy(bsys, other) != side) {
            continue;
        }
        if (ctx->sel_mons_no[other] == partySlot) {
            return TRUE;
        }

        if (other != battler && ctx->reshuffle_sel_mons_no[other] == partySlot) {
            return TRUE;
        }
    }

    return FALSE;
}

static int TestBattle_FindNextLivePartySlot(struct BattleSystem *bsys, struct BattleStruct *ctx, int battler)
{
    int partyCount = BattleWorkPokeCountGet(bsys, battler);

    for (int partySlot = 0; partySlot < partyCount; partySlot++) {
        struct PartyPokemon *mon = BattleWorkPokemonParamGet(bsys, battler, partySlot);

        if (GetMonData(mon, MON_DATA_HP, NULL) == 0) {
            continue;
        }

        if (GetMonData(mon, MON_DATA_SPECIES_OR_EGG, NULL) == SPECIES_NONE) {
            continue;
        }

        if (GetMonData(mon, MON_DATA_SPECIES_OR_EGG, NULL) == SPECIES_EGG) {
            continue;
        }

        if (TestBattle_IsPartySlotAlreadyActive(bsys, ctx, battler, partySlot)) {
            continue;
        }

        return partySlot;
    }

    return 6;
}

static BOOL TestBattle_ShouldAutoSelectKOSwitch(struct BattleSystem *bsys, struct BattleStruct *ctx, int battler)
{
    return sCurrentScenario != NULL
        //&& (battler == BATTLER_PLAYER_FIRST || battler == BATTLER_PLAYER_SECOND) // do it for all
        && (ctx->client_status[battler] & 1)
        && TestBattle_FindNextLivePartySlot(bsys, ctx, battler) != 6;
}

BOOL LONG_CALL TestBattle_ShowParty(struct BattleSystem *bsys, struct BattleStruct *ctx)
{
    int promptedBattlers = 0;
    int maxBattlers = BattleWorkClientSetMaxGet(bsys);

    IncrementBattleScriptPtr(ctx, 1);

    for (int battler = 0; battler < maxBattlers; battler++) {
        if (ctx->client_status[battler] & 1) {
            promptedBattlers |= MaskOfFlagNo(battler);
            if (!TestBattle_ShouldAutoSelectKOSwitch(bsys, ctx, battler)) {
                BattleController_EmitShowMonList(bsys, ctx, battler, 1, 0, 6);
            }
        }
    }

    for (int battler = 0; battler < maxBattlers; battler++) {
        if (BattleTypeGet(bsys) == (BATTLE_TYPE_TRAINER | BATTLE_TYPE_DOUBLES | BATTLE_TYPE_LINK)) {
            int partner = BattleWorkPartnerClientNoGet(bsys, battler);
            if (!(promptedBattlers & MaskOfFlagNo(battler)) && !(promptedBattlers & MaskOfFlagNo(partner))) {
                promptedBattlers |= MaskOfFlagNo(battler);
                BattleController_EmitShowWaitMessage(bsys, battler);
            }
        } else if (!(promptedBattlers & MaskOfFlagNo(battler))) {
            BattleController_EmitShowWaitMessage(bsys, battler);
        }
    }

    for (int battler = 0; battler < maxBattlers; battler++) {
        if (ctx->client_status[battler] & 1) {
            ctx->reshuffle_client = battler;
            break;
        }
    }

    return FALSE;
}

BOOL LONG_CALL TestBattle_WaitMonSelection(struct BattleSystem *bsys, struct BattleStruct *ctx)
{
    int maxBattlers = BattleWorkClientSetMaxGet(bsys);
    int switchCnt = 0;
    u8 autoSelected[CLIENT_MAX] = { 0 };

    for (int battler = 0; battler < maxBattlers; battler++) {
        if (ctx->client_status[battler] & 1) {
            switchCnt++;
        }
    }

    for (int battler = 0; battler < maxBattlers; battler++) {
        if (!(ctx->client_status[battler] & 1)) {
            continue;
        }

        int selectedSlot = TestBattle_FindNextLivePartySlot(bsys, ctx, battler);
        if (TestBattle_ShouldAutoSelectKOSwitch(bsys, ctx, battler)) {
            ctx->reshuffle_sel_mons_no[battler] = selectedSlot;
            autoSelected[battler] = TRUE;
            switchCnt--;
            continue;
        }

        if (BattleBuffer_GetNext(ctx, battler)) {
            ctx->reshuffle_sel_mons_no[battler] = ctx->server_buffer[battler][0] - 1;
            switchCnt--;
            if (!(ctx->server_status_flag2 & (MaskOfFlagNo(battler) << BATTLE_STATUS_FAINTED_SHIFT))) {
                ctx->server_status_flag2 |= MaskOfFlagNo(battler) << BATTLE_STATUS_FAINTED_SHIFT;
                BattleController_EmitShowWaitMessage(bsys, battler);
            }
        }
    }

    if (switchCnt == 0) {
        for (int battler = 0; battler < maxBattlers; battler++) {
            if (ctx->client_status[battler] & 1) {
                if (autoSelected[battler]) {
                    ov12_0223BDDC(bsys, battler, ctx->reshuffle_sel_mons_no[battler] + 1);
                } else if (BattleBuffer_GetNext(ctx, battler)) {
                    ov12_0223BDDC(bsys, battler, ctx->server_buffer[battler][0]);
                }
            }
        }
        ctx->server_status_flag2 &= 0xf0ffffff;
        IncrementBattleScriptPtr(ctx, 1);
    }

    ctx->battle_progress_flag = 1;

    return FALSE;
}

/**
 * @brief Auto-select moves/switches for player battlers in test scenarios
 *
 * Called from BattleContext_Main during the input phase to automatically
 * select actions for player-controlled battlers based on the test script.
 *
 * @param bsys Battle system pointer
 * @param ctx Battle struct pointer
 */
void LONG_CALL TestBattle_autoSelectPlayerMoves(struct BattleSystem *bsys, struct BattleStruct *ctx)
{
    if (ctx->server_seq_no != CONTROLLER_COMMAND_SELECTION_SCREEN_INPUT) {
        return; // Not in input phase
    }

    if (ctx->com_seq_no[0] != SSI_STATE_SELECT_COMMAND_INIT) {
        return; // Already processed or not ready
    }

    if (sCurrentScenario == NULL) {
        return;
    }

    if (GetScriptIndex(0) == 0) {
        int maxBattlers = BattleWorkClientSetMaxGet(bsys);

        for (int battlerId = 0; battlerId < maxBattlers; battlerId++) {
            int partySlot = ctx->sel_mons_no[battlerId];
            const struct TestBattlePokemon *mon;

            if (BATTLER_IS_ENEMY(battlerId)) {
                mon = &sCurrentScenario->enemyParty[partySlot];
            } else {
                mon = &sCurrentScenario->playerParty[partySlot];
            }

            if (mon->teraType || mon->isTerastallized) { // PMFG REQ-TGT-BTL-007: else keep the personality's Tera type
                ctx->battlemon[battlerId].tera_type = mon->teraType;
                ctx->battlemon[battlerId].is_currently_terastallized = mon->isTerastallized;
            }
        }
    }

    TestBattle_CheckScriptCompletion();

    const struct BattleAction *script0 = sCurrentScenario->playerScript[0];
    int scriptIndex0 = GetScriptIndex(0);

    if (scriptIndex0 < AI_SCRIPT_MAX_MOVES) {
        struct BattleAction action = script0[scriptIndex0];
        if (action.action == ACTION_NONE) {
            return;
        }

        if (action.action >= ACTION_SWITCH_SLOT_0 && action.action <= ACTION_SWITCH_SLOT_5) {
            // switch
            u8 partySlot = action.action - ACTION_SWITCH_SLOT_0;
            ctx->playerActions[0][0] = CONTROLLER_COMMAND_POKEMON_INPUT;
            ctx->playerActions[0][1] = partySlot;
            ctx->playerActions[0][2] = 0;
            ctx->playerActions[0][3] = SELECT_POKEMON_COMMAND;
            ctx->reshuffle_sel_mons_no[0] = partySlot;
            ctx->com_seq_no[0] = SSI_STATE_END;
            ctx->ret_seq_no[0] = SSI_STATE_13;
            IncrementScriptIndex(0);
        } else {
            // fight
            u8 moveSlot = action.action & ~ACTION_GIMMICKS;
            if (action.action & ACTION_Z) {
                sZWant |= No2Bit(0);
            }
            if (action.action & ACTION_TERA) {
                sTeraWant |= No2Bit(0);
            }
            u8 target = action.target;
            ctx->playerActions[0][0] = CONTROLLER_COMMAND_FIGHT_INPUT;
            ctx->playerActions[0][1] = target;
            ctx->playerActions[0][2] = moveSlot + 1;
            ctx->playerActions[0][3] = SELECT_FIGHT_COMMAND;
            ctx->waza_no_pos[0] = moveSlot;
            ctx->waza_no_select[0] = ctx->battlemon[0].move[moveSlot];
            ctx->com_seq_no[0] = SSI_STATE_END;
            ctx->ret_seq_no[0] = SSI_STATE_13;
            IncrementScriptIndex(0);
        }
    }

    if (BattleTypeGet(bsys) & BATTLE_TYPE_DOUBLES) {
        const struct BattleAction *script1 = sCurrentScenario->playerScript[1];
        int scriptIndex2 = GetScriptIndex(2);

        if (scriptIndex2 < AI_SCRIPT_MAX_MOVES) {
            struct BattleAction action = script1[scriptIndex2];
            if (action.action == ACTION_NONE) {
                return;
            }

            if (action.action >= ACTION_SWITCH_SLOT_0 && action.action <= ACTION_SWITCH_SLOT_5) {
                // switch
                u8 partySlot = action.action - ACTION_SWITCH_SLOT_0;
                ctx->playerActions[2][0] = CONTROLLER_COMMAND_POKEMON_INPUT;
                ctx->playerActions[2][1] = partySlot;
                ctx->playerActions[2][2] = 0;
                ctx->playerActions[2][3] = SELECT_POKEMON_COMMAND;
                ctx->reshuffle_sel_mons_no[2] = partySlot;
                ctx->com_seq_no[2] = SSI_STATE_END;
                ctx->ret_seq_no[2] = SSI_STATE_13;
                IncrementScriptIndex(2);
            } else {
                // fight
                u8 moveSlot = action.action & ~ACTION_GIMMICKS;
                if (action.action & ACTION_Z) {
                    sZWant |= No2Bit(2);
                }
                if (action.action & ACTION_TERA) {
                    sTeraWant |= No2Bit(2);
                }
                u8 target = action.target;
                ctx->playerActions[2][0] = CONTROLLER_COMMAND_FIGHT_INPUT;
                ctx->playerActions[2][1] = target;
                ctx->playerActions[2][2] = moveSlot + 1;
                ctx->playerActions[2][3] = SELECT_FIGHT_COMMAND;
                ctx->waza_no_pos[2] = moveSlot;
                ctx->waza_no_select[2] = ctx->battlemon[2].move[moveSlot];
                ctx->com_seq_no[2] = SSI_STATE_END;
                ctx->ret_seq_no[2] = SSI_STATE_13;
                IncrementScriptIndex(2);
            }
        }
    }
}


// ---- message expectations, called from src/battle/test_battle.c BattleMessage_ExpandPlaceholders (PMFG: moved out of the full battle overlay) ----

static void NormalizeMessage(char *s)
{
    int read = 0;
    int write = 0;

    while (s[read] == ' ' || s[read] == '\t' || s[read] == '\r' || s[read] == '\n') {
        read++;
    }

    while (s[read] != '\0') {
        s[write++] = s[read++];
    }

    while (write > 0 && (s[write - 1] == ' ' || s[write - 1] == '\t' || s[write - 1] == '\r' || s[write - 1] == '\n')) {
        write--;
    }

    s[write] = '\0';
}

static BOOL MessageContains(const char *message, const char *substring)
{
    int i = 0;

    if (substring[0] == '\0') {
        return TRUE;
    }

    for (i = 0; message[i] != '\0'; i++) {
        int j = 0;
        while (substring[j] != '\0' && message[i + j] == substring[j]) {
            j++;
        }
        if (substring[j] == '\0') {
            return TRUE;
        }
    }

    return FALSE;
}

void LONG_CALL TestBattle_CheckMessage(struct BattleSystem *battleSystem)
{
    struct TestBattleScenario *scenario = TestBattle_GetCurrentScenario();

    while (scenario != NULL && TestBattle_HasMoreExpectations()) {
        struct Expectations *expectation = &scenario->expectations[scenario->expectationPassCount];
        if (expectation->expectationType == EXPECTATION_TYPE_PARTY_FORM) {
            struct PartyPokemon *mon = BattleWorkPokemonParamGet(battleSystem, BATTLER_PLAYER_FIRST, expectation->battlerIDOrPartySlot);
            if (GetMonData(mon, MON_DATA_FORM, NULL) != expectation->expectationValue.formID) {
                break;
            }
        } else if (expectation->expectationType == EXPECTATION_TYPE_BATTLER_TYPES) {
            struct BattlePokemon *mon = &battleSystem->sp->battlemon[expectation->battlerIDOrPartySlot];
            if (mon->type1 != expectation->expectationValue.types[0]
                || mon->type2 != expectation->expectationValue.types[1]) {
                break;
            }
        } else if (expectation->expectationType == EXPECTATION_TYPE_PARTY_EV_TOTAL) {
            struct PartyPokemon *mon = BattleWorkPokemonParamGet(battleSystem, BATTLER_PLAYER_FIRST, expectation->battlerIDOrPartySlot);
            u32 total = 0;
            for (int i = 0; i < 6; i++) {
                total += GetMonData(mon, MON_DATA_HP_EV + i, NULL);
            }
            if (total != expectation->expectationValue.evTotal) {
                break;
            }
        } else if (expectation->expectationType == EXPECTATION_TYPE_SIDE_CONDITION_ABSENT) {
            if (battleSystem->sp->side_condition[expectation->battlerIDOrPartySlot]
                & expectation->expectationValue.sideConditionMask) {
                break;
            }
        } else {
            break;
        }
        scenario->expectationPassCount++;
    }

    char actualMessage[TEST_BATTLE_MESSAGE_LEN] = { 0 };
    int out = 0;

    for (int i = 0; i < battleSystem->msgBuffer->size && out < TEST_BATTLE_MESSAGE_LEN - 1; i++) {
        u32 code = battleSystem->msgBuffer->data[i];
        char character = '\0';

        switch (code) {
        case 0xFFFF:
            actualMessage[out] = '\0';
            i = battleSystem->msgBuffer->size;
            continue;
        case 0x01AC:
            character = '?';
            break;
        case 0x01A8:
            character = '$';
            break;
        case 0x01BE:
            character = '-';
            break;
        case 0x01DE:
        case 0x0001:
        case 0xE000:
        case 0x25BC:
        case 0x25BD:
            character = ' ';
            break;
        case 0x01AB:
            character = '!';
            break;
        case 0x01AD:
            character = ',';
            break;
        case 0x01AE:
            character = '.';
            break;
        case 0x01B3:
            character = '\'';
            break;
        case 0x0188:
            character = 'e';
            break;
        case 0x01B9:
            character = '(';
            break;
        case 0x01BA:
            character = ')';
            break;
        default:
            if (code <= 0x012A) {
                character = (char)(code - 0xF1);
            } else if (code <= 0x0144) {
                character = (char)(code - 234);
            } else {
                character = (char)(code - 228);
            }
            break;
        }
        if (character == '\0') {
            character = ' ';
        }
        actualMessage[out++] = character;

        switch (code) {
        case 0x01B3:
            debug_printf("’");
            break;
        case 0x0188:
            debug_printf("é");
            break;
        default:
            debug_printf("%c", character);
            break;
        }
    }
    actualMessage[out] = '\0';

    if (scenario == NULL || !TestBattle_HasMoreExpectations()) {
        debug_printf("\n");
        return;
    }

    enum ExpectationType expectationType = scenario->expectations[scenario->expectationPassCount].expectationType;
    if (expectationType != EXPECTATION_TYPE_MESSAGE
        && expectationType != EXPECTATION_TYPE_MESSAGE_CONTAINS
        && expectationType != EXPECTATION_TYPE_ATTACK_MESSAGE
        && expectationType != EXPECTATION_TYPE_MESSAGE_DOES_NOT_CONTAIN
        && expectationType != EXPECTATION_TYPE_NOT_MESSAGE) {
        debug_printf("\n");
        return;
    }

    char expectedMessage[TEST_BATTLE_MESSAGE_LEN] = { 0 };
    for (int i = 0; i < TEST_BATTLE_MESSAGE_LEN - 1; i++) {
        expectedMessage[i] = scenario->expectations[scenario->expectationPassCount].expectationValue.message[i];
        if (expectedMessage[i] == '\0') {
            break;
        }
    }

    NormalizeMessage(actualMessage);
    NormalizeMessage(expectedMessage);

    BOOL messageMatch = FALSE;
    if (expectationType == EXPECTATION_TYPE_MESSAGE_CONTAINS) {
        messageMatch = MessageContains(actualMessage, expectedMessage);
    } else if (expectationType == EXPECTATION_TYPE_MESSAGE_DOES_NOT_CONTAIN) {
        messageMatch = !MessageContains(actualMessage, expectedMessage);
    } else {
        messageMatch = TRUE;
        for (int i = 0; i < TEST_BATTLE_MESSAGE_LEN; i++) {
            if (actualMessage[i] != expectedMessage[i]) {
                messageMatch = FALSE;
                break;
            }
            if (actualMessage[i] == '\0') {
                break;
            }
        }
        if (expectationType == EXPECTATION_TYPE_NOT_MESSAGE) {
            messageMatch = !messageMatch;
        }
    }

    if (messageMatch && !scenario->markAsFail) {
        if (expectationType == EXPECTATION_TYPE_MESSAGE || EXPECTATION_TYPE_MESSAGE_CONTAINS) {
            debug_printf(" ✅");
        } else {
            debug_printf(" ✔️");
        }
        scenario->expectationPassCount++;
    } else {
        if ((expectationType == EXPECTATION_TYPE_NOT_MESSAGE || expectationType == EXPECTATION_TYPE_MESSAGE_DOES_NOT_CONTAIN) && !scenario->markAsFail) {
            debug_printf(" ❌");
            scenario->markAsFail = TRUE;
        }
    }
    debug_printf("\n");
}

#endif // DEBUG_BATTLE_SCENARIOS
