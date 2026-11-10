#include "../../include/types.h"
#include "../../include/system.h"
#include "../../include/window.h"
#include "../../include/message.h"
#include "../../include/sound.h"
#include "../../include/constants/buttons.h"
#include "../../include/constants/sndseq.h"
#include "../../include/pmfg_options.h"
#include "../../include/pmfg_settings.h"
#include "../../include/pmfg_options_text.h"
#include "../../include/pokemon.h"
#include "../../include/pokemon_storage_system.h"
#include "../../include/save.h"

// pmfg: options screen app (B-020), shown once after Oak's speech. First asks RECOMMENDED / CUSTOM; CUSTOM lists the
// settings of Docs/requirements/target/data/settings.csv a page at a time, over the HGSS Options app's backgrounds
// (BG setup and graphics as pret src/options_app.c:448-458,548-657): its touch-screen gradient behind framed menu
// windows on both screens, the description on the touch screen. The top-screen Options art isn't used: its HGSS option
// buttons are drawn into the background.
// From the PC (SYS-008, args PMFG_OPTIONS_ARGS_PC) it opens on the pages, ease-only settings can't go above their
// current value, and B leaves without saving.
// REQ-TGT-QOL-003 (D-063 c): the same screen also lists party and PC box mons for trade/tutor scripts (STATE_CHOOSER).

#define HEAP_ID 0x4B // overlay 36's heap, destroyed before this app starts
#define TEXT_SPEED_NOTRANSFER 0xFF
#define COLOR(fg, shadow, bg) (((fg) << 16) | ((shadow) << 8) | (bg))
#define COLOR_NORMAL COLOR(1, 2, 15)
#define COLOR_SELECTED COLOR(3, 4, 15)
#define COLOR_LOCKED COLOR(2, 15, 15)
#define FONT_PAL 13
#define FRAME_PAL 2 // after the gradient's two palettes
#define FRAME_TILE 600 // after the windows' tiles (title 1-60, rows 61-540)

// the ROM's own save-error screen display modes (arm9 rodata), also used by src/save.c
#define ROM_GRAPHICS_MODES ((void *)0x02108530)

// BG layers (pret GF_BG_LYR_*) and the Options app's graphics archive a/0/7/2 (pret NARC_a_0_7_2)
enum { BG_TEXT = 1, BG_BACK = 2, BG_SUB_BACK = 4, BG_SUB_TEXT = 5 };
#define NARC_OPTIONS_GFX 72
#define PAL_LOCATION_MAIN_BG 0
#define PAL_LOCATION_SUB_BG 4

// pret include/bg_window.h; hg-engine declares the BG calls with void *
typedef struct {
    u32 x, y, bufferSize, baseTile;
    u8 size, colorMode, screenBase, charBase, bgExtPltt, priority, areaOver, dummy;
    u32 mosaic;
} BgTemplate;

// raw NitroSDK values: size 1 = 256x256, screenBase n = 0xn*0x800, charBase n = n*0x4000
#define BG_TEMPLATE(scr, chr, prio) { .bufferSize = 0x800, .size = 1, .screenBase = (scr), .charBase = (chr), .priority = (prio) }
static const BgTemplate sBgTemplates[] = {
    [0] = BG_TEMPLATE(30, 1, 1), // BG_TEXT: screen 0xF000, chars 0x4000
    [1] = BG_TEMPLATE(29, 0, 2), // BG_BACK: screen 0xE800, chars 0x0000
    [2] = BG_TEMPLATE(31, 0, 1), // BG_SUB_BACK: screen 0xF800, chars 0x0000
    [3] = BG_TEMPLATE(30, 2, 0), // BG_SUB_TEXT: screen 0xF000, chars 0x8000
};
static const u8 sBgLayers[] = { BG_TEXT, BG_BACK, BG_SUB_BACK, BG_SUB_TEXT };
static const u32 sGraphicsBanks[10] = { [0] = 0x01, [2] = 0x04 }; // pret GraphicsBanks: bg GX_VRAM_BG_128_A, subbg GX_VRAM_SUB_BG_128_C

void LONG_CALL Heap_Create(u32 parent, u32 heapId, u32 size);
void LONG_CALL Heap_Destroy(u32 heapId);
void LONG_CALL FreeBgTilemapBuffer(void *bgConfig, u32 layer);
u32 LONG_CALL GfGfxLoader_LoadCharData(u32 narcId, s32 memberNo, void *bgConfig, u32 layer, u32 tileStart, u32 szByte, BOOL isCompressed, u32 heapId);
void LONG_CALL GfGfxLoader_LoadScrnData(u32 narcId, s32 memberNo, void *bgConfig, u32 layer, u32 tileStart, u32 szByte, BOOL isCompressed, u32 heapId);
void LONG_CALL GfGfxLoader_GXLoadPal(u32 narcId, s32 memberNo, u32 location, u32 palSlotOffset, u32 szByte, u32 heapId);

enum { STATE_PROMPT, STATE_PAGES, STATE_CONFIRM, STATE_CHOOSER, STATE_DONE };
enum { WIN_TITLE, WIN_ROWS, WIN_DESC, WIN_COUNT };

static const WindowTemplate sWindows[WIN_COUNT] = {
    [WIN_TITLE] = { .bgId = BG_TEXT, .left = 1, .top = 1, .width = 30, .height = 2, .palette = FONT_PAL, .baseTile = 1 },
    [WIN_ROWS] = { .bgId = BG_TEXT, .left = 1, .top = 5, .width = 30, .height = 16, .palette = FONT_PAL, .baseTile = 61 },
    [WIN_DESC] = { .bgId = BG_SUB_TEXT, .left = 1, .top = 16, .width = 30, .height = 6, .palette = FONT_PAL, .baseTile = 1 },
};

static struct {
    void *bgConfig;
    struct Window windows[WIN_COUNT];
    String *string;
    u8 state;
    u8 cursor; // prompt choice, or row on the page
    u8 page;
    BOOL pc;
    u8 values[NUM_PMFG_SETTINGS];
    u8 limit[NUM_PMFG_SETTINGS]; // number of values the player may pick, from index 0
} sApp;

// chooser pages: the party, then each box that holds a mon; a page lists its mons, 8 rows at a time
#define SAVE_PCSTORAGE 41 // pret include/constants/save_arrays.h
#define CHOOSER_PARTY_PAGE 0xFF
#define CHOOSER_ROWS 8
void *LONG_CALL SaveArray_Get(SaveData *saveData, int id); // src/save.c, overlay 129 (LONG_CALL: a plain bl gets an ARM veneer)

static struct {
    u16 *result;
    struct Party *party;
    PCStorage *storage;
    u8 pages[1 + NUM_PC_BOXES]; // CHOOSER_PARTY_PAGE or a box
    u8 numPages, page, count, top;
    u8 slots[MONS_PER_BOX]; // this page's party or box slots
    struct PartyPokemon mon; // a box mon as a party mon, for its level
} sChooser;

static void Print(u32 win, const u16 *text, u32 x, u32 y, u32 color)
{
    CopyU16ArrayToString(sApp.string, (u16 *)text);
    // + 4: glyphs drawn at x = 0 lose their first pixel column
    AddTextPrinterParameterizedWithColor(&sApp.windows[win], 0, sApp.string, x + 4, y, TEXT_SPEED_NOTRANSFER, color, NULL);
}

static u8 RowSetting(u8 row)
{
    return sPmfgOptionsPageRows[sApp.page][row];
}

static void ChooserDraw(void);

static void SettingsDraw(void)
{
    Print(WIN_TITLE, sPmfgOptionsText[PMFG_OPTIONS_TEXT_TITLE], 0, 0, COLOR_NORMAL);
    if (sApp.state == STATE_PROMPT) {
        Print(WIN_ROWS, sPmfgOptionsText[PMFG_OPTIONS_TEXT_PROMPT], 0, 0, COLOR_NORMAL);
        Print(WIN_ROWS, sPmfgOptionsText[PMFG_OPTIONS_TEXT_RECOMMENDED], 16, 32, sApp.cursor == 0 ? COLOR_SELECTED : COLOR_NORMAL);
        Print(WIN_ROWS, sPmfgOptionsText[PMFG_OPTIONS_TEXT_CUSTOM], 16, 48, sApp.cursor == 1 ? COLOR_SELECTED : COLOR_NORMAL);
    } else {
        Print(WIN_TITLE, sPmfgOptionsPageTitle[sApp.page], 136, 0, COLOR_NORMAL);
        for (u8 row = 0; row < PMFG_OPTIONS_ROWS && RowSetting(row) != PMFG_OPTIONS_NO_ROW; row++) {
            u8 s = RowSetting(row);
            u32 color = row == sApp.cursor ? COLOR_SELECTED : sApp.limit[s] == 1 ? COLOR_LOCKED : COLOR_NORMAL;
            Print(WIN_ROWS, sPmfgOptionsLabel[s], 0, row * 16, color);
            Print(WIN_ROWS, sPmfgOptionsValue[s][sApp.values[s]], 160, row * 16, color);
        }
        Print(WIN_DESC, sPmfgOptionsDesc[RowSetting(sApp.cursor)], 0, 0, COLOR_NORMAL);
    }
    if (sApp.state == STATE_PAGES || sApp.state == STATE_CONFIRM) { // third line, under the description
        u32 help = sApp.state == STATE_CONFIRM ? PMFG_OPTIONS_TEXT_CONFIRM : sApp.pc ? PMFG_OPTIONS_TEXT_HELP_PC : PMFG_OPTIONS_TEXT_HELP;
        Print(WIN_DESC, sPmfgOptionsText[help], 0, 32, sApp.state == STATE_CONFIRM ? COLOR_SELECTED : COLOR_LOCKED);
    }
}

static void Draw(void)
{
    for (int i = 0; i < WIN_COUNT; i++) {
        FillWindowPixelBuffer(&sApp.windows[i], 15);
    }
    if (sApp.state == STATE_CHOOSER) {
        ChooserDraw();
    } else {
        SettingsDraw();
    }
    for (int i = 0; i < WIN_COUNT; i++) {
        CopyWindowToVram(&sApp.windows[i]);
    }
}

static struct PartyPokemon *ChooserMon(u8 row)
{
    u8 box = sChooser.pages[sChooser.page], slot = sChooser.slots[row];
    if (box == CHOOSER_PARTY_PAGE) {
        return Party_GetMonByIndex(sChooser.party, slot);
    }
    CopyBoxPokemonToPokemon(&sChooser.storage->boxes[box].mons[slot], &sChooser.mon);
    return &sChooser.mon;
}

static void ChooserLoadPage(void)
{
    u8 box = sChooser.pages[sChooser.page];
    sChooser.count = sChooser.top = sApp.cursor = 0;
    for (u8 slot = 0; slot < (box == CHOOSER_PARTY_PAGE ? sChooser.party->count : MONS_PER_BOX); slot++) {
        if (box == CHOOSER_PARTY_PAGE || GetBoxMonData(&sChooser.storage->boxes[box].mons[slot], MON_DATA_SPECIES, NULL)) {
            sChooser.slots[sChooser.count++] = slot;
        }
    }
}

static void ChooserDraw(void)
{
    u16 text[12];
    u8 box = sChooser.pages[sChooser.page];
    Print(WIN_TITLE, sPmfgOptionsText[PMFG_OPTIONS_TEXT_CHOOSER_TITLE], 0, 0, COLOR_NORMAL);
    Print(WIN_TITLE, box == CHOOSER_PARTY_PAGE ? sPmfgOptionsText[PMFG_OPTIONS_TEXT_CHOOSER_PARTY] : sChooser.storage->box_names[box], 152, 0, COLOR_NORMAL);
    for (u8 row = sChooser.top; row < sChooser.count && row < sChooser.top + CHOOSER_ROWS; row++) {
        struct PartyPokemon *mon = ChooserMon(row);
        BOOL egg = GetMonData(mon, MON_DATA_IS_EGG, NULL);
        u32 color = row == sApp.cursor ? COLOR_SELECTED : egg ? COLOR_LOCKED : COLOR_NORMAL, y = (row - sChooser.top) * 16;
        GetMonData(mon, MON_DATA_NICKNAME, text);
        Print(WIN_ROWS, text, 0, y, color);
        if (!egg) {
            u32 level = GetMonData(mon, MON_DATA_LEVEL, NULL), n = 0;
            for (u32 div = level >= 100 ? 100 : level >= 10 ? 10 : 1; div; div /= 10) {
                text[n++] = 0x121 + level / div % 10; // the game's digits (engine/charmap.txt)
            }
            text[n] = 0xFFFF;
            Print(WIN_ROWS, sPmfgOptionsText[PMFG_OPTIONS_TEXT_CHOOSER_LV], 160, y, color);
            Print(WIN_ROWS, text, 184, y, color);
        }
    }
    Print(WIN_DESC, sPmfgOptionsText[PMFG_OPTIONS_TEXT_CHOOSER_HELP], 0, 32, COLOR_LOCKED);
}

static void ChooserInput(u32 keys)
{
    if (keys & (PAD_KEY_UP | PAD_KEY_DOWN)) {
        sApp.cursor = (sApp.cursor + ((keys & PAD_KEY_DOWN) ? 1 : sChooser.count - 1)) % sChooser.count;
        sChooser.top = sApp.cursor < sChooser.top ? sApp.cursor : sApp.cursor >= sChooser.top + CHOOSER_ROWS ? sApp.cursor - CHOOSER_ROWS + 1 : sChooser.top;
    } else if (keys & (PAD_BUTTON_R | PAD_BUTTON_L)) {
        sChooser.page = (sChooser.page + ((keys & PAD_BUTTON_R) ? 1 : sChooser.numPages - 1)) % sChooser.numPages;
        ChooserLoadPage();
    } else if ((keys & PAD_BUTTON_A) && !GetMonData(ChooserMon(sApp.cursor), MON_DATA_IS_EGG, NULL)) {
        u8 box = sChooser.pages[sChooser.page], slot = sChooser.slots[sApp.cursor];
        *sChooser.result = box == CHOOSER_PARTY_PAGE ? slot : PMFG_CHOOSER_BOX(box, slot);
        sApp.state = STATE_DONE;
    } else if (keys & PAD_BUTTON_B) {
        *sChooser.result = PMFG_CHOOSER_NONE;
        sApp.state = STATE_DONE;
    } else {
        return;
    }
    PlaySE(sApp.state == STATE_DONE ? SEQ_SE_DP_DECIDE : SEQ_SE_DP_SELECT);
    if (sApp.state != STATE_DONE) {
        Draw();
    }
}

static void HandleInput(void)
{
    u32 keys = gSystem.newKeys;
    if (sApp.state == STATE_CHOOSER) {
        ChooserInput(keys);
        return;
    }
    if (sApp.state == STATE_PROMPT) {
        if (keys & (PAD_KEY_UP | PAD_KEY_DOWN)) {
            sApp.cursor ^= 1;
        } else if (keys & PAD_BUTTON_A) {
            sApp.state = sApp.cursor == 0 ? STATE_DONE : STATE_PAGES;
            sApp.cursor = 0;
        } else {
            return;
        }
    } else if (sApp.state == STATE_CONFIRM) { // B-024: START (or A) saves, B goes back to the page
        if (keys & (PAD_BUTTON_START | PAD_BUTTON_A)) {
            for (u8 i = 0; i < NUM_PMFG_SETTINGS; i++) {
                PMFG_SetSetting(i, sApp.values[i]);
            }
            sApp.state = STATE_DONE;
        } else if (keys & PAD_BUTTON_B) {
            sApp.state = STATE_PAGES;
        } else {
            return;
        }
    } else {
        u8 s = RowSetting(sApp.cursor), rows = 0;
        while (rows < PMFG_OPTIONS_ROWS && RowSetting(rows) != PMFG_OPTIONS_NO_ROW) {
            rows++;
        }
        if (keys & PAD_KEY_UP) {
            sApp.cursor = (sApp.cursor + rows - 1) % rows;
        } else if (keys & PAD_KEY_DOWN) {
            sApp.cursor = (sApp.cursor + 1) % rows;
        } else if ((keys & (PAD_KEY_RIGHT | PAD_KEY_LEFT)) && sApp.limit[s] > 1) {
            sApp.values[s] = (sApp.values[s] + ((keys & PAD_KEY_RIGHT) ? 1 : sApp.limit[s] - 1)) % sApp.limit[s];
        } else if (keys & (PAD_BUTTON_R | PAD_BUTTON_L)) {
            sApp.page = (sApp.page + ((keys & PAD_BUTTON_R) ? 1 : PMFG_OPTIONS_PAGES - 1)) % PMFG_OPTIONS_PAGES;
            sApp.cursor = 0;
        } else if (keys & PAD_BUTTON_START) {
            sApp.state = STATE_CONFIRM;
        } else if (keys & PAD_BUTTON_B) { // PC: leave without saving; new game: back to RECOMMENDED / CUSTOM (B-024)
            sApp.state = sApp.pc ? STATE_DONE : STATE_PROMPT;
            sApp.cursor = 1;
        } else {
            return;
        }
    }
    PlaySE(sApp.state == STATE_DONE ? SEQ_SE_DP_DECIDE : SEQ_SE_DP_SELECT);
    if (sApp.state != STATE_DONE) {
        Draw();
    }
}

static void OpenScreens(void);

static BOOL PMFG_OptionsScreen_Init(void *manager, int *state UNUSED)
{
    Heap_Create(3, HEAP_ID, 0x20000);
    sApp.pc = OverlayManager_GetArgs(manager) == PMFG_OPTIONS_ARGS_PC;
    sApp.state = sApp.pc ? STATE_PAGES : STATE_PROMPT;
    sApp.cursor = sApp.page = 0;
    for (u8 i = 0; i < NUM_PMFG_SETTINGS; i++) {
        sApp.values[i] = PMFG_GetSetting(i); // a new game reads as all defaults
        sApp.limit[i] = sApp.pc && sPmfgOptionsEaseOnly[i] ? sApp.values[i] + 1 : sPmfgOptionsNumValues[i];
    }
    OpenScreens();
    return TRUE;
}

static BOOL PMFG_Chooser_Init(void *manager, int *state UNUSED)
{
    Heap_Create(3, HEAP_ID, 0x20000);
    sApp.pc = TRUE; // back to the field on exit, as from the PC
    sApp.state = STATE_CHOOSER;
    sChooser.result = OverlayManager_GetArgs(manager);
    sChooser.party = SaveData_GetPlayerPartyPtr(SaveBlock2_get());
    sChooser.storage = SaveArray_Get(SaveBlock2_get(), SAVE_PCSTORAGE);
    sChooser.pages[0] = CHOOSER_PARTY_PAGE;
    sChooser.numPages = 1;
    for (u8 box = 0; box < NUM_PC_BOXES; box++) {
        sChooser.page = sChooser.numPages;
        sChooser.pages[sChooser.page] = box;
        ChooserLoadPage();
        sChooser.numPages += sChooser.count != 0;
    }
    sChooser.page = 0;
    ChooserLoadPage();
    OpenScreens();
    return TRUE;
}

static void OpenScreens(void)
{
    Main_SetVBlankIntrCB(NULL, NULL);
    GfGfx_DisableEngineAPlanes();
    GfGfx_DisableEngineBPlanes();
    GfGfx_SetBanks((void *)sGraphicsBanks);
    sApp.bgConfig = BgConfig_Alloc(HEAP_ID);
    SetBothScreensModesAndDisable(ROM_GRAPHICS_MODES);
    for (int i = 0; i < (int)NELEMS(sBgLayers); i++) {
        InitBgFromTemplate(sApp.bgConfig, sBgLayers[i], (void *)&sBgTemplates[i], 0);
        BgClearTilemapBufferAndCommit(sApp.bgConfig, sBgLayers[i]);
    }
    BG_ClearCharDataRange(BG_TEXT, 0x20, 0, HEAP_ID);
    BG_ClearCharDataRange(BG_SUB_TEXT, 0x20, 0, HEAP_ID);
    for (int sub = 0; sub < 2; sub++) {
        GfGfxLoader_GXLoadPal(NARC_OPTIONS_GFX, 3, sub ? PAL_LOCATION_SUB_BG : PAL_LOCATION_MAIN_BG, 0, 0x40, HEAP_ID);
        GfGfxLoader_LoadCharData(NARC_OPTIONS_GFX, 8, sApp.bgConfig, sub ? BG_SUB_BACK : BG_BACK, 0, 0, FALSE, HEAP_ID);
        GfGfxLoader_LoadScrnData(NARC_OPTIONS_GFX, 19, sApp.bgConfig, sub ? BG_SUB_BACK : BG_BACK, 0, 0, FALSE, HEAP_ID);
        LoadFontPal0(sub ? PAL_LOCATION_SUB_BG : PAL_LOCATION_MAIN_BG, FONT_PAL * 0x20, HEAP_ID);
        LoadUserFrameGfx1(sApp.bgConfig, sub ? BG_SUB_TEXT : BG_TEXT, FRAME_TILE, FRAME_PAL, 0, HEAP_ID);
    }

    sApp.string = String_New(64, HEAP_ID);
    ResetAllTextPrinters();
    for (int i = 0; i < WIN_COUNT; i++) {
        AddWindow(sApp.bgConfig, &sApp.windows[i], (void *)&sWindows[i]);
        DrawFrameAndWindow1(&sApp.windows[i], FALSE, FRAME_TILE, FRAME_PAL);
    }
    Draw();
    GfGfx_BothDispOn();
    SetMasterBrightnessNeutral(0);
    SetMasterBrightnessNeutral(1);
}

static BOOL PMFG_OptionsScreen_Main(void *manager UNUSED, int *state UNUSED)
{
    HandleInput();
    return sApp.state == STATE_DONE;
}

static BOOL PMFG_OptionsScreen_Exit(void *manager UNUSED, int *state UNUSED)
{
    sub_0200FBF4(0, 0); // black, as Oak's speech left the screens for the field to fade in
    sub_0200FBF4(1, 0);
    for (int i = 0; i < WIN_COUNT; i++) {
        RemoveWindow(&sApp.windows[i]);
    }
    String_Delete(sApp.string);
    for (int i = 0; i < (int)NELEMS(sBgLayers); i++) {
        FreeBgTilemapBuffer(sApp.bgConfig, sBgLayers[i]);
    }
    sys_FreeMemoryEz(sApp.bgConfig);
    GfGfx_DisableEngineAPlanes();
    GfGfx_DisableEngineBPlanes();
    Heap_Destroy(HEAP_ID);
    if (!sApp.pc) {
        RegisterMainOverlay(0xFFFFFFFF, &gApplication_NewGameFieldsys); // what overlay 36 did before the hook
    }
    return TRUE;
}

// first in the overlay (.init), so overlay 129's trampolines find it at PMFG_OPTIONS_ENTRY
__attribute__((section(".init"))) const OverlayFunc gPmfgOptionsEntry[4] = {
    PMFG_OptionsScreen_Init, PMFG_OptionsScreen_Main, PMFG_OptionsScreen_Exit, PMFG_Chooser_Init,
};
