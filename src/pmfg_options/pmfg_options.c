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

// pmfg: options screen app (B-020), shown once after Oak's speech. First asks RECOMMENDED / CUSTOM; CUSTOM lists the
// settings of Docs/requirements/target/data/settings.csv a page at a time. Text only, one BG layer, laid out like
// hg-engine's crash screen (src/save.c), whose ROM graphics templates it reuses.

#define HEAP_ID 0x4B // overlay 36's heap, destroyed before this app starts
#define TEXT_SPEED_NOTRANSFER 0xFF
#define COLOR(fg, shadow, bg) (((fg) << 16) | ((shadow) << 8) | (bg))
#define COLOR_NORMAL COLOR(1, 2, 15)
#define COLOR_SELECTED COLOR(3, 4, 15)

// the ROM's own save-error screen setup (arm9 rodata), also used by src/save.c
#define ROM_GRAPHICS_BANKS ((void *)0x0210855C)
#define ROM_GRAPHICS_MODES ((void *)0x02108530)
#define ROM_BG_TEMPLATE ((void *)0x02108540)

void LONG_CALL Heap_Create(u32 parent, u32 heapId, u32 size);
void LONG_CALL Heap_Destroy(u32 heapId);
void LONG_CALL FreeBgTilemapBuffer(void *bgConfig, u32 layer);

enum { STATE_PROMPT, STATE_PAGES, STATE_DONE };
enum { WIN_TITLE, WIN_ROWS, WIN_DESC, WIN_COUNT };

static const WindowTemplate sWindows[WIN_COUNT] = {
    [WIN_TITLE] = { .bgId = 0, .left = 1, .top = 0, .width = 30, .height = 2, .palette = 1, .baseTile = 1 },
    [WIN_ROWS] = { .bgId = 0, .left = 1, .top = 2, .width = 30, .height = 16, .palette = 1, .baseTile = 61 },
    [WIN_DESC] = { .bgId = 0, .left = 1, .top = 18, .width = 30, .height = 6, .palette = 1, .baseTile = 541 },
};

static struct {
    void *bgConfig;
    struct Window windows[WIN_COUNT];
    String *string;
    u8 state;
    u8 cursor; // prompt choice, or row on the page
    u8 page;
    u8 values[NUM_PMFG_SETTINGS];
} sApp;

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

static void Draw(void)
{
    for (int i = 0; i < WIN_COUNT; i++) {
        FillWindowPixelBuffer(&sApp.windows[i], 15);
    }
    Print(WIN_TITLE, sPmfgOptionsText[PMFG_OPTIONS_TEXT_TITLE], 0, 0, COLOR_NORMAL);
    if (sApp.state == STATE_PROMPT) {
        Print(WIN_ROWS, sPmfgOptionsText[PMFG_OPTIONS_TEXT_PROMPT], 0, 0, COLOR_NORMAL);
        Print(WIN_ROWS, sPmfgOptionsText[PMFG_OPTIONS_TEXT_RECOMMENDED], 16, 32, sApp.cursor == 0 ? COLOR_SELECTED : COLOR_NORMAL);
        Print(WIN_ROWS, sPmfgOptionsText[PMFG_OPTIONS_TEXT_CUSTOM], 16, 48, sApp.cursor == 1 ? COLOR_SELECTED : COLOR_NORMAL);
    } else {
        Print(WIN_TITLE, sPmfgOptionsPageTitle[sApp.page], 136, 0, COLOR_NORMAL);
        for (u8 row = 0; row < PMFG_OPTIONS_ROWS && RowSetting(row) != PMFG_OPTIONS_NO_ROW; row++) {
            u8 s = RowSetting(row);
            u32 color = row == sApp.cursor ? COLOR_SELECTED : COLOR_NORMAL;
            Print(WIN_ROWS, sPmfgOptionsLabel[s], 0, row * 16, color);
            Print(WIN_ROWS, sPmfgOptionsValue[s][sApp.values[s]], 160, row * 16, color);
        }
        Print(WIN_DESC, sPmfgOptionsDesc[RowSetting(sApp.cursor)], 0, 0, COLOR_NORMAL);
    }
    if (sApp.state == STATE_PAGES) {
        Print(WIN_DESC, sPmfgOptionsText[PMFG_OPTIONS_TEXT_HELP], 0, 32, COLOR(2, 15, 15)); // third line, under the description
    }
    for (int i = 0; i < WIN_COUNT; i++) {
        CopyWindowToVram(&sApp.windows[i]);
    }
}

static void HandleInput(void)
{
    u32 keys = gSystem.newKeys;
    if (sApp.state == STATE_PROMPT) {
        if (keys & (PAD_KEY_UP | PAD_KEY_DOWN)) {
            sApp.cursor ^= 1;
        } else if (keys & PAD_BUTTON_A) {
            sApp.state = sApp.cursor == 0 ? STATE_DONE : STATE_PAGES;
            sApp.cursor = 0;
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
        } else if (keys & PAD_KEY_RIGHT) {
            sApp.values[s] = (sApp.values[s] + 1) % sPmfgOptionsNumValues[s];
        } else if (keys & PAD_KEY_LEFT) {
            sApp.values[s] = (sApp.values[s] + sPmfgOptionsNumValues[s] - 1) % sPmfgOptionsNumValues[s];
        } else if (keys & (PAD_BUTTON_R | PAD_BUTTON_L)) {
            sApp.page = (sApp.page + ((keys & PAD_BUTTON_R) ? 1 : PMFG_OPTIONS_PAGES - 1)) % PMFG_OPTIONS_PAGES;
            sApp.cursor = 0;
        } else if (keys & PAD_BUTTON_START) {
            for (u8 i = 0; i < NUM_PMFG_SETTINGS; i++) {
                PMFG_SetSetting(i, sApp.values[i]);
            }
            sApp.state = STATE_DONE;
        } else {
            return;
        }
    }
    PlaySE(sApp.state == STATE_DONE ? SEQ_SE_DP_DECIDE : SEQ_SE_DP_SELECT);
    if (sApp.state != STATE_DONE) {
        Draw();
    }
}

static BOOL PMFG_OptionsScreen_Init(void *manager UNUSED, int *state UNUSED)
{
    Heap_Create(3, HEAP_ID, 0x20000);
    sApp.state = STATE_PROMPT;
    sApp.cursor = sApp.page = 0;
    for (u8 i = 0; i < NUM_PMFG_SETTINGS; i++) {
        sApp.values[i] = PMFG_GetSetting(i); // a new game reads as all defaults
    }

    Main_SetVBlankIntrCB(NULL, NULL);
    GfGfx_DisableEngineAPlanes();
    GfGfx_DisableEngineBPlanes();
    GfGfx_SetBanks(ROM_GRAPHICS_BANKS);
    sApp.bgConfig = BgConfig_Alloc(HEAP_ID);
    SetBothScreensModesAndDisable(ROM_GRAPHICS_MODES);
    InitBgFromTemplate(sApp.bgConfig, 0, ROM_BG_TEMPLATE, 0);
    BgClearTilemapBufferAndCommit(sApp.bgConfig, 0);
    LoadFontPal0(0, 0x20, HEAP_ID);
    BG_ClearCharDataRange(0, 0x20, 0, HEAP_ID);
    BG_SetMaskColor(0, 0);
    BG_SetMaskColor(4, 0);

    sApp.string = String_New(64, HEAP_ID);
    ResetAllTextPrinters();
    for (int i = 0; i < WIN_COUNT; i++) {
        AddWindow(sApp.bgConfig, &sApp.windows[i], (void *)&sWindows[i]);
    }
    Draw();
    GfGfx_BothDispOn();
    SetMasterBrightnessNeutral(0);
    SetMasterBrightnessNeutral(1);
    return TRUE;
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
    FreeBgTilemapBuffer(sApp.bgConfig, 0);
    sys_FreeMemoryEz(sApp.bgConfig);
    GfGfx_DisableEngineAPlanes();
    GfGfx_DisableEngineBPlanes();
    Heap_Destroy(HEAP_ID);
    RegisterMainOverlay(0xFFFFFFFF, &gApplication_NewGameFieldsys); // what overlay 36 did before the hook
    return TRUE;
}

// first in the overlay (.init), so overlay 129's trampolines find it at PMFG_OPTIONS_ENTRY
__attribute__((section(".init"))) const OverlayFunc gPmfgOptionsEntry[3] = {
    PMFG_OptionsScreen_Init, PMFG_OptionsScreen_Main, PMFG_OptionsScreen_Exit,
};
