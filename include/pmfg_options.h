#ifndef PMFG_OPTIONS_H
#define PMFG_OPTIONS_H

#include "types.h"

// pmfg: native options screen (PMFG B-020), shown between Oak's speech and the field.
// The screen lives in overlay OVERLAY_PMFG_OPTIONS; its Init/Main/Exit table sits first in it, at this address
// (ORIGIN in src/pmfg_options/linker.ld), because overlay 129 is linked before it and can't name its symbols.
#define PMFG_OPTIONS_ENTRY 0x021E5900

typedef BOOL (*OverlayFunc)(void *manager, int *state);

typedef struct OverlayManagerTemplate {
    OverlayFunc init;
    OverlayFunc exec;
    OverlayFunc exit;
    u32 ovyId;
} OverlayManagerTemplate;

// the PC's launch args (SYS-008, script command NEW_COMMAND_PMFG_SETTINGS_VIEWER); the new game's main-overlay args
// are a real pointer, so this marks PC mode
#define PMFG_OPTIONS_ARGS_PC ((void *)1)

extern const OverlayManagerTemplate gApplication_NewGameFieldsys;
extern const OverlayManagerTemplate PMFG_OptionsScreenApp;
void LONG_CALL RegisterMainOverlay(u32 ovyId, const OverlayManagerTemplate *template);
void *LONG_CALL OverlayManager_GetArgs(void *manager);

#endif // PMFG_OPTIONS_H
