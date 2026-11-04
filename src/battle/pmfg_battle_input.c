#include "config.h"
#include "types.h"

#include "constants/battle_constants.h"
#include "constants/file.h"
#include "constants/item.h"

#include "bag.h"
#include "battle.h"
#include "overlay.h"
#include "pmfg_settings.h"
#include "system.h"

// PMFG REQ-TGT-QOL-002 quick_run and ball_prompt: the battle main menu (FIGHT/BAG/POKEMON/RUN). Vanilla's command task
// (ov12_0225DAD4, state 5) polls BattleInput_CheckTouch each frame; asm/battle/battle_hooks.s PMFG_BattleInput_CheckTouch_hook
// routes that call here (0x0225DF20). Offsets into pret's struct BattleInput are read off BattleInput_CheckTouch.
#define BattleInput_CheckTouch ((int (*)(u8 *battleInput))(0x02266B78 | 1))
#define BattleCursor_Disable ((void (*)(void *cursor))(0x0226BB68 | 1))
#define BI_CUR_MENU_ID   0x68B // s8 curMenuId: BATTLE_MENU_MAIN_INITIAL 1, BATTLE_MENU_MAIN 2
#define BI_CANCEL_SHOWN  0x68F // unk66F: CANCEL instead of RUN (the second mon in a double battle)
#define BI_CURSOR        0x6D8 // BattleCursor *cursor
#define BI_MENU_CURSOR   0x6DC // BattleMenuCursor { enabled, menuY, menuX, unused }
#define BI_KEY_PRESSED   0x6E0
#define BATTLE_INPUT_BAG 2
#define BATTLE_INPUT_RUN 4
#define PAD_BUTTON_A  0x0001
#define PAD_BUTTON_B  0x0002
#define PAD_BUTTON_R  0x0100
#define PAD_BUTTON_L  0x0200
#define PAD_KEY_DOWN  0x0080
#define BTLPOCKETLIST_BALLS 2 // pret include/constants/items.h
#define BAG_CURSOR_LAST_USED_ITEM 30 // pret BagCursor: BagCursorField (20 bytes), then battle.lastUsedItem / lastUsedPocket
#define BAG_CURSOR_LAST_USED_POCKET 32

// balls the R button may throw, in tie-break order; never the Master Ball, nor contest/Safari/Park balls
static const u16 sQuickBalls[] = {
    ITEM_POKE_BALL, ITEM_GREAT_BALL, ITEM_ULTRA_BALL, ITEM_NET_BALL, ITEM_DIVE_BALL, ITEM_NEST_BALL, ITEM_REPEAT_BALL,
    ITEM_TIMER_BALL, ITEM_LUXURY_BALL, ITEM_PREMIER_BALL, ITEM_DUSK_BALL, ITEM_HEAL_BALL, ITEM_QUICK_BALL, ITEM_CHERISH_BALL,
    ITEM_FAST_BALL, ITEM_LEVEL_BALL, ITEM_LURE_BALL, ITEM_HEAVY_BALL, ITEM_LOVE_BALL, ITEM_FRIEND_BALL, ITEM_MOON_BALL,
    ITEM_DREAM_BALL, ITEM_BEAST_BALL,
};

int PCStorage_CountEmptySpotsInAllBoxes(void *storage); // src/pokemon_storage_system.c

u16 gPmfgQuickBall = ITEM_NONE; // set here, consumed by PMFG_TryQuickBallThrow on the server side
u8 gPmfgBallScoreOnly = FALSE;  // src/individual/CalculateBallShakes.c returns the ball's score instead of shakes
static u8 sRunCursorPending = FALSE;

/**
 *  @brief the best ball against the wild mon: highest (catch rate + Heavy Ball bonus) x ball multiplier, from the same
 *  code that throws it (src/individual/CalculateBallShakes.c, loaded once for all balls)
 */
static u16 BestBall(struct BattleSystem *bsys, struct BattleStruct *sp)
{
    u16 best = ITEM_NONE;
    u32 bestScore = 0;
    int item = sp->item_work, attacker = sp->attack_client, defender = sp->defence_client;
    sp->attack_client = 0;
    sp->defence_client = 1;
    gPmfgBallScoreOnly = TRUE;
    HandleLoadOverlay(OVERLAY_CALCULATEBALLSHAKES, 2);
    for (u32 i = 0; i < NELEMS(sQuickBalls); i++) {
        if (Bag_HasItem(bsys->bag, sQuickBalls[i], 1, 5)) {
            sp->item_work = sQuickBalls[i];
            u32 score = ((u32 (*)(void *, struct BattleStruct *))(0x023C0400 | 1))(bsys, sp);
            if (score > bestScore) {
                best = sQuickBalls[i];
                bestScore = score;
            }
        }
    }
    UnloadOverlayByID(OVERLAY_CALCULATEBALLSHAKES);
    gPmfgBallScoreOnly = FALSE;
    sp->item_work = item;
    sp->attack_client = attacker;
    sp->defence_client = defender;
    return best;
}

/**
 *  @brief the last ball thrown (HGSS's battle bag remembers it), else the first ball in the bag (H&S
 *  refs/pokehns-expansion/src/battle_interface.c:3094-3118)
 */
static u16 LastBall(struct BattleSystem *bsys)
{
    u8 *cursor = bsys->bagCursor;
    u16 last = *(u16 *)(cursor + BAG_CURSOR_LAST_USED_ITEM);
    if (*(u16 *)(cursor + BAG_CURSOR_LAST_USED_POCKET) == BTLPOCKETLIST_BALLS && last != ITEM_MASTER_BALL && Bag_HasItem(bsys->bag, last, 1, 5)) {
        return last;
    }
    for (u32 i = 0; i < NELEMS(sQuickBalls); i++) {
        if (Bag_HasItem(bsys->bag, sQuickBalls[i], 1, 5)) {
            return sQuickBalls[i];
        }
    }
    return ITEM_NONE;
}

static int PickedByKey(u8 *battleInput, int input)
{
    // what vanilla does once a menu entry is picked (pret battle_input.c BattleInput_CheckTouch)
    for (int i = 0; i < 4; i++) {
        battleInput[BI_MENU_CURSOR + i] = 0;
    }
    BattleCursor_Disable(*(void **)(battleInput + BI_CURSOR));
    battleInput[BI_KEY_PRESSED] = TRUE;
    return input;
}

int LONG_CALL PMFG_BattleInput_CheckTouch(u8 *battleInput)
{
    struct BattleSystem *bsys = *(struct BattleSystem **)battleInput;
    s8 menu = battleInput[BI_CUR_MENU_ID];
    u32 type = BattleTypeGet(bsys);
    if ((menu != 1 && menu != 2) || battleInput[BI_CANCEL_SHOWN]
        || (type & (BATTLE_TYPE_TRAINER | BATTLE_TYPE_LINK | BATTLE_TYPE_SAFARI | BATTLE_TYPE_FRONTIER | BATTLE_TYPE_PAL_PARK | BATTLE_TYPE_TUTORIAL | BATTLE_TYPE_BUG_CONTEST))) {
        sRunCursorPending = FALSE;
        return BattleInput_CheckTouch(battleInput);
    }
    u32 held = gSystem.heldKeys, pressed = gSystem.newKeys;
    switch (PMFG_GetSetting(SETTING_QUICK_RUN)) {
    case SETTING_QUICK_RUN_LRA: // refs/pokehns-expansion/src/battle_main.c:3825-3826, here at the menu: hold L+R, press A
        if ((held & (PAD_BUTTON_L | PAD_BUTTON_R)) == (PAD_BUTTON_L | PAD_BUTTON_R) && (pressed & PAD_BUTTON_A)) {
            return PickedByKey(battleInput, BATTLE_INPUT_RUN);
        }
        break;
    case SETTING_QUICK_RUN_B:
        if (pressed & PAD_BUTTON_B) {
            return PickedByKey(battleInput, BATTLE_INPUT_RUN);
        }
        break;
    case SETTING_QUICK_RUN_B_THEN_A: // refs/pokehns-expansion/src/battle_controller_player.c:383-391: B puts the cursor on RUN
        if (sRunCursorPending && battleInput[BI_MENU_CURSOR]) {
            // vanilla moves, draws and sounds the cursor: from FIGHT's row in RUN's column, DOWN lands on RUN
            sRunCursorPending = FALSE;
            battleInput[BI_MENU_CURSOR + 1] = 0;
            battleInput[BI_MENU_CURSOR + 2] = 1;
            gSystem.newKeys = PAD_KEY_DOWN;
            int ret = BattleInput_CheckTouch(battleInput);
            gSystem.newKeys = pressed;
            return ret;
        }
        if (pressed & PAD_BUTTON_B) {
            sRunCursorPending = TRUE; // a hidden cursor shows up on this press; it moves next frame
        }
        break;
    }
    if (PMFG_GetSetting(SETTING_BALL_PROMPT) != SETTING_BALL_PROMPT_OFF && (pressed & PAD_BUTTON_R) && !(held & PAD_BUTTON_L)
        && !(type & BATTLE_TYPE_DOUBLES) // a double wild battle can't aim a ball (vanilla's bag refuses it too)
        && (Battle_GetClientPartySize(bsys, 0) < 6 || PCStorage_CountEmptySpotsInAllBoxes(bsys->storage) > 0)) {
        // no room for a catch: R does nothing, the bag says why as in vanilla
        u16 ball = PMFG_GetSetting(SETTING_BALL_PROMPT) == SETTING_BALL_PROMPT_BEST ? BestBall(bsys, bsys->sp) : LastBall(bsys);
        if (ball != ITEM_NONE) {
            gPmfgQuickBall = ball;
            return PickedByKey(battleInput, BATTLE_INPUT_BAG);
        }
    }
    return BattleInput_CheckTouch(battleInput);
}

/**
 *  @brief server side of the quick ball: the BAG command arrives in SSI_STATE_7, where vanilla would open the bag;
 *  answer as the bag would (pret battle_controller_player.c SSI_STATE_8: playerActions[b][2] = BattleItem {id, page}),
 *  so BattleControllerPlayer_ItemInput throws it and takes it from the bag
 */
void PMFG_TryQuickBallThrow(struct BattleStruct *ctx)
{
    if (gPmfgQuickBall == ITEM_NONE) {
        return;
    }
    for (int battlerId = 0; battlerId < 4; battlerId += 2) {
        if (ctx->playerActions[battlerId][0] == CONTROLLER_COMMAND_ITEM_INPUT && ctx->com_seq_no[battlerId] == SSI_STATE_7) {
            ctx->playerActions[battlerId][2] = gPmfgQuickBall | (BTLPOCKETLIST_BALLS << 16);
            ctx->com_seq_no[battlerId] = SSI_STATE_13;
            gPmfgQuickBall = ITEM_NONE;
            return;
        }
    }
}
