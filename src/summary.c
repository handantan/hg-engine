#include "../include/summary.h"

#include "../include/battle.h"
#include "../include/types.h"
#include "../include/message.h"
#include "../include/pmfg_innates.h"
#include "../include/script.h"
#include "../include/sound.h"
#include "../include/constants/file.h"

// file is from LheaRachel on github who adapted it from Bubble
// i just adapted it to hgss and added the +/- handling and such

static void UpdatePokemonData(struct SummaryState *summary, u8 mode)
{
    void *potentialBoxMon = Summary_GetPokemonData(summary);
    void *pokemon;

    // Use a different data accessor for summary screens from a PC box
    if (summary->baseData->dataType == 2) {
        pokemon = AllocMonZeroed(19); // heap id 19
        CopyBoxPokemonToPokemon(potentialBoxMon, pokemon);
    } else {
        pokemon = potentialBoxMon;
    }

    int paramStart = MON_DATA_MAXHP;

    // Pokemon struct orders current HP before max HP, so need to handle mode == 0 as a special case
    if (mode == 0) {
        summary->pokemonData.hp = (u16)GetMonData(pokemon, MON_DATA_HP, NULL);
    } else {
        if (mode == 1) {
            paramStart = MON_DATA_HP_EV;
        } else {
            paramStart = MON_DATA_HP_IV;
        }

        summary->pokemonData.hp = (u16)GetMonData(pokemon, paramStart, NULL);
    }

    // Pokemon struct orders all data parameters as such:
    // Attack -> Defense -> Speed -> SpAttack -> SpDefense
    summary->pokemonData.attack = (u16)GetMonData(pokemon, paramStart + 1, NULL);
    summary->pokemonData.defense = (u16)GetMonData(pokemon, paramStart + 2, NULL);
    summary->pokemonData.speed = (u16)GetMonData(pokemon, paramStart + 3, NULL);
    summary->pokemonData.spAttack = (u16)GetMonData(pokemon, paramStart + 4, NULL);
    summary->pokemonData.spDefense = (u16)GetMonData(pokemon, paramStart + 5, NULL);

    if (summary->baseData->dataType == 2) { // free it if it was allocated
        sys_FreeMemoryEz(pokemon);
    }
}

// Components here are indices into the system font palette.
//   l == index of the color to use for the letter itself
//   s == index of the color to use for the shadow of the letter
//   g == index of the color to use for the background of the letter (0 == transparent)
#define COLOR(l, s, g) ((u32)(((l & 0xFF) << 16) | ((s & 0xFF) << 8) | ((g & 0xFF) << 0)))
#define BLACK          (COLOR(1, 2, 0))
#define BLUE_INVERT    (COLOR(4, 3, 0))
#define RED_INVERT     (COLOR(6, 5, 0))
#define BLUE           (COLOR(3, 4, 0))
#define RED            (COLOR(5, 6, 0))
#define PINK           (COLOR(7, 8, 0))
#define GREEN          (COLOR(9, 10, 0))
#define WHITE          (COLOR(0xE, 0xF, 0))

static s8 sNatureStatEffects[25][6] = {
    // atk, def, spatk, spdef, speed
    { 0, 0, 0, 0, 0, 0 }, // Hardy
    { 0, 1, -1, 0, 0, 0 }, // Lonely
    { 0, 1, 0, 0, 0, -1 }, // Brave
    { 0, 1, 0, -1, 0, 0 }, // Adamant
    { 0, 1, 0, 0, -1, 0 }, // Naughty
    { 0, -1, 1, 0, 0, 0 }, // Bold
    { 0, 0, 0, 0, 0, 0 }, // Docile
    { 0, 0, 1, 0, 0, -1 }, // Relaxed
    { 0, 0, 1, -1, 0, 0 }, // Impish
    { 0, 0, 1, 0, -1, 0 }, // Lax
    { 0, -1, 0, 0, 0, 1 }, // Timid
    { 0, 0, -1, 0, 0, 1 }, // Hasty
    { 0, 0, 0, 0, 0, 0 }, // Serious
    { 0, 0, 0, -1, 0, 1 }, // Jolly
    { 0, 0, 0, 0, -1, 1 }, // Naive
    { 0, -1, 0, 1, 0, 0 }, // Modest
    { 0, 0, -1, 1, 0, 0 }, // Mild
    { 0, 0, 0, 1, 0, -1 }, // Quiet
    { 0, 0, 0, 0, 0, 0 }, // Bashful
    { 0, 0, 0, 1, -1, 0 }, // Rash
    { 0, -1, 0, 0, 1, 0 }, // Calm
    { 0, 0, -1, 0, 1, 0 }, // Gentle
    { 0, 0, 0, 0, 1, -1 }, // Sassy
    { 0, 0, 0, -1, 1, 0 }, // Careful
    { 0, 0, 0, 0, 0, 0 }, // Quirky
};

static void PrintStatNumberWithColor(struct SummaryState *summary, u8 windowIdx, u32 justify)
{
    u32 nature = GetBoxMonNatureCountMints(Summary_GetPokemonData(summary));
    u32 color = BLACK;
    if (sNatureStatEffects[nature][windowIdx] > 0) {
        color = RED;
    } else if (sNatureStatEffects[nature][windowIdx] < 0) {
        color = BLUE;
    }

    Summary_PrintString(summary, &summary->addlWindows[windowIdx], color, justify);
}

void Summary_ColorizeStatScreen(struct SummaryState *summary, u32 mode)
{
    u32 nature = GetBoxMonNatureCountMints(Summary_GetPokemonData(summary));
    Summary_NumberToString(summary, 120, summary->pokemonData.attack, 3, 0);
    PrintStatNumberWithColor(summary, 1, JUSTIFY_RIGHT);
    Summary_NumberToString(summary, 121, summary->pokemonData.defense, 3, 0);
    PrintStatNumberWithColor(summary, 2, JUSTIFY_RIGHT);
    Summary_NumberToString(summary, 122, summary->pokemonData.spAttack, 3, 0);
    PrintStatNumberWithColor(summary, 3, JUSTIFY_RIGHT);
    Summary_NumberToString(summary, 123, summary->pokemonData.spDefense, 3, 0);
    PrintStatNumberWithColor(summary, 4, JUSTIFY_RIGHT);
    Summary_NumberToString(summary, 124, summary->pokemonData.speed, 3, 0);
    PrintStatNumberWithColor(summary, 5, JUSTIFY_RIGHT);

    for (int i = 0; i < 6; i++) {
        FillWindowPixelBuffer(&summary->defnWindows[0xF + i], 0);
        if (i != 0) // print a possibly colored text and append +/-
        {
            u32 msgId = 110;
            u32 color = WHITE;
            if (sNatureStatEffects[nature][i] > 0) {
                msgId = 196 - 1; // Stat+
                color = RED_INVERT;
            } else if (sNatureStatEffects[nature][i] < 0) {
                msgId = 201 - 1; // Stat-
                color = BLUE_INVERT;
            }

            // Summary_PrintStatStringAccountForStat(summary, 0xF+i, msgId+i, i-1, JUSTIFY_LEFT);
            Summary_PrintStringGeneric(summary, 0xF + i, msgId + i, color, JUSTIFY_LEFT);
        } else if (mode == 0) { // raw stat
            Summary_PrintStringGeneric(summary, 0xF, 110, WHITE, JUSTIFY_LEFT);
        } else if (mode == 1) { // ev's
            Summary_PrintStringGeneric(summary, 0xF, 206, WHITE, JUSTIFY_LEFT);
        } else { // iv's
            Summary_PrintStringGeneric(summary, 0xF, 207, WHITE, JUSTIFY_LEFT);
        }
        CopyWindowToVram(&summary->defnWindows[0xF + i]);
    }
}

static u8 sAbilitySlot; // PMFG (REQ-TGT-MON-003): 0 = main ability, 1-3 = innate shown in the Skills page ability box

void Summary_ColorizeStatScreen_Wrap(struct SummaryState *summary)
{
    sAbilitySlot = 0; // page (re)drawn: vanilla prints the main ability
    Summary_ColorizeStatScreen(summary, 0);
}

void Summary_ChangeStatScreenState(struct SummaryState *summary, u8 mode)
{
    for (int i = 0; i < 6; i++) {
        FillWindowPixelBuffer(&summary->addlWindows[i], 0);
    }

    UpdatePokemonData(summary, mode);

    if (mode) {
        // Print IVs or EVs
        Summary_NumberToString(summary, 119, summary->pokemonData.hp, 3, 1);
        PrintStatNumberWithColor(summary, 0, JUSTIFY_CENTER);
    } else {
        // Print cur and max
        u8 xsize = summary->addlWindows[0].sizx * 8;
        Summary_PrintCurrentOverMax(
            summary,
            0,
            117,
            119,
            118,
            summary->pokemonData.hp,
            summary->pokemonData.maxHP,
            3,
            xsize / 2,
            0);
    }

    Summary_ColorizeStatScreen(summary, mode);

    for (int i = 0; i < 6; i++) {
        CopyWindowToVram(&summary->addlWindows[i]);
    }

    UpdatePokemonData(summary, 0); // Recover old data for page change
}

// change this to possibly take a BoxPokemon structure but be compatible with vanilla handling just in case
u16 ModifyStatByNature(u32 nature, u16 n, u8 statIndex)
{
    u32 retVal;

    // Dont modify HP, Accuracy, or Evasion by nature
    if (statIndex < STAT_ATTACK || statIndex > STAT_SPECIAL_DEFENSE) {
        return n;
    }

    if (nature & 0x02000000) {
        nature = GetBoxMonNatureCountMints(&((struct PartyPokemon *)nature)->box);
    }

    // thanks to dray for this fix!
    if (statIndex == STAT_SPEED) // have to convert to window index to use the sNatureStatEffects table
    {
        statIndex = 5;
    } else if (statIndex > STAT_SPEED) {
        statIndex--;
    }

    switch (sNatureStatEffects[nature][statIndex]) {
    case 1:
        // NOTE: will overflow for n > 595 because the intermediate value is cast to u16 before the division.
        retVal = n * 110;
        retVal /= 100;
        break;
    case -1:
        // NOTE: will overflow for n > 728, see above
        retVal = n * 90;
        retVal /= 100;
        break;
    default:
        retVal = n;
        break;
    }
    return retVal;
}

// PMFG (REQ-TGT-MON-003, D-059 a): START on the Skills page cycles the ability box through the main ability and the innates.
// Offsets from vanilla's Skills page print (0x0208CB9C, 0x0208D3xx): msgData (archive 302) +0x7A0, msgFmt +0x7A8, string buffer +0x7AC;
// addl windows 6/7 = ability name/description, defn window 0x15 = "Ability" label.
#define SUMMARY_MSGDATA(s) (*(MsgData **)((u8 *)(s) + 0x7A0))
#define SUMMARY_MSGFMT(s) (*(MessageFormat **)((u8 *)(s) + 0x7A8))
#define SUMMARY_STRBUF(s) (*(String **)((u8 *)(s) + 0x7AC))
#define SUMMARY_MSG_ABILITY_LABEL 116
#define SUMMARY_MSG_ABILITY_NAME 125
#define SUMMARY_MSG_INNATE_LABEL 208 // 208-210 "Innate N", 211-213 unlock milestones (data/text/302.txt)

static void PrintMsg(struct SummaryState *summary, GF_BGL_BMPWIN *win, MsgData *md, u32 msgId, u32 color)
{
    FillWindowPixelBuffer(win, 0);
    ReadMsgDataIntoString(md, msgId, SUMMARY_STRBUF(summary));
    AddTextPrinterParameterizedWithColor(win, 0, SUMMARY_STRBUF(summary), 3, 0, 0xFF, color, NULL); // x = 3 as vanilla
    CopyWindowToVram(win);
}

void LONG_CALL PMFG_Summary_CycleAbility(struct SummaryState *summary)
{
    u16 innates[PMFG_NUM_INNATES];
    PMFG_GetSpeciesInnates(summary->pokemonData.species, summary->pokemonData.form, innates);
    do {
        sAbilitySlot = (sAbilitySlot + 1) % (PMFG_NUM_INNATES + 1);
    } while (sAbilitySlot != 0 && innates[sAbilitySlot - 1] == 0);

    u32 ability = sAbilitySlot ? innates[sAbilitySlot - 1] : summary->pokemonData.ability;
    // innate N unlocks at the milestone PMFG_UnlockedInnateCount counts (8 badges / Johto League / 12 badges); texts 211-213 must match it
    BOOL locked = sAbilitySlot > PMFG_UnlockedInnateCount();
    GF_BGL_BMPWIN *nameWin = &summary->addlWindows[6];

    PrintMsg(summary, &summary->defnWindows[0x15], SUMMARY_MSGDATA(summary),
        sAbilitySlot ? SUMMARY_MSG_INNATE_LABEL + sAbilitySlot - 1 : SUMMARY_MSG_ABILITY_LABEL, WHITE);

    FillWindowPixelBuffer(nameWin, 0);
    WORDSET_RegisterAbilityName(SUMMARY_MSGFMT(summary), 0, ability);
    String *tmpl = NewString_ReadMsgData(SUMMARY_MSGDATA(summary), SUMMARY_MSG_ABILITY_NAME);
    StringExpandPlaceholders(SUMMARY_MSGFMT(summary), SUMMARY_STRBUF(summary), tmpl);
    String_Delete(tmpl);
    Summary_PrintString(summary, nameWin, BLACK, JUSTIFY_LEFT);
    CopyWindowToVram(nameWin);

    if (locked) {
        PrintMsg(summary, &summary->addlWindows[7], SUMMARY_MSGDATA(summary), SUMMARY_MSG_INNATE_LABEL + PMFG_NUM_INNATES + sAbilitySlot - 1, BLACK);
    } else {
        MsgData *md = NewMsgDataFromNarc(1, ARC_MSG_DATA, 722, 0x13); // ability descriptions, as vanilla loads them
        PrintMsg(summary, &summary->addlWindows[7], md, ability, BLACK);
        DestroyMsgData(md);
    }
    PlaySE(0x5E1);
}
