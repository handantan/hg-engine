#include "types.h"
#include "constants/file.h"
#include "pmfg_options.h"

// pmfg: the options screen's app template (B-020). Overlay 36 registers it after Oak's speech instead of the field
// (repoints). It must sit in always-loaded overlay 129: the template is read before its own overlay is loaded.

#define ENTRY ((const OverlayFunc *)PMFG_OPTIONS_ENTRY)

static BOOL PMFG_OptionsScreenApp_Init(void *manager, int *state)
{
    return ENTRY[0](manager, state);
}

static BOOL PMFG_OptionsScreenApp_Main(void *manager, int *state)
{
    return ENTRY[1](manager, state);
}

static BOOL PMFG_OptionsScreenApp_Exit(void *manager, int *state)
{
    return ENTRY[2](manager, state);
}

const OverlayManagerTemplate PMFG_OptionsScreenApp = {
    PMFG_OptionsScreenApp_Init, PMFG_OptionsScreenApp_Main, PMFG_OptionsScreenApp_Exit, OVERLAY_PMFG_OPTIONS,
};
