#include "../include/config.h"
#include "../include/constants/file.h"
#include "../include/repel.h"
#include "../include/roamer.h"
#include "../include/pmfg_options.h"
#include "../include/script.h"
#include "../include/types.h"

#define SCRIPT_NEW_CMD_REPEL_USE 0
#define SCRIPT_NEW_CMD_PMFG_SETTINGS_VIEWER 1

// pmfg: launching an app from a script, as pret's ScrCmd_PartySelectUI does (refs/pokeheartgold/src/scrcmd_c.c:1605)
void LONG_CALL FieldSystem_LaunchApplication(FieldSystem *fsys, const OverlayManagerTemplate *template, void *args);
void LONG_CALL SetupNativeScript(SCRIPTCONTEXT *ctx, ScrCmdFunc func);
BOOL LONG_CALL ScrNative_WaitApplication(SCRIPTCONTEXT *ctx);

#define SCRIPT_NEW_CMD_MAX 256

BOOL Script_RunNewCmd(SCRIPTCONTEXT *ctx)
{
    u8 sw = ScriptReadByte(ctx);
    u16 UNUSED arg0 = ScriptReadHalfword(ctx);

    switch (sw) {
    case SCRIPT_NEW_CMD_REPEL_USE:;
#ifdef IMPLEMENT_REUSABLE_REPELS
        u16 most_recent_repel = Repel_GetMostRecent();
        SetScriptVar(arg0, most_recent_repel);
        Repel_Use(most_recent_repel, HEAPID_MAIN_HEAP);
#endif
        break;

    case SCRIPT_NEW_CMD_PMFG_SETTINGS_VIEWER: // pmfg: SYS-008
        FieldSystem_LaunchApplication(ctx->fsys, &PMFG_OptionsScreenApp, PMFG_OPTIONS_ARGS_PC);
        SetupNativeScript(ctx, ScrNative_WaitApplication);
        return TRUE;

    default:
        break;
    }

    return FALSE;
}

#ifdef EXPAND_ROAMERS
BOOL LONG_CALL ScrCmd_CreateRoamer(SCRIPTCONTEXT *ctx)
{
    u8 roamerNo = ScriptReadByte(ctx);
    Save_CreateRoamerByID(ctx->fsys->savedata, roamerNo);
    return FALSE;
}
#endif // EXPAND_ROAMERS
