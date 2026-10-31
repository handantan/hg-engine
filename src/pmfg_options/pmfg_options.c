#include "../../include/types.h"
#include "../../include/pmfg_options.h"

// pmfg: options screen app (B-020). Step 1 spike: shows nothing and hands over to the field at once.

static BOOL PMFG_OptionsScreen_Init(void *manager UNUSED, int *state UNUSED)
{
    return TRUE;
}

static BOOL PMFG_OptionsScreen_Main(void *manager UNUSED, int *state UNUSED)
{
    return TRUE;
}

static BOOL PMFG_OptionsScreen_Exit(void *manager UNUSED, int *state UNUSED)
{
    RegisterMainOverlay(0xFFFFFFFF, &gApplication_NewGameFieldsys); // what overlay 36 did before the hook
    return TRUE;
}

// first in the overlay (.init), so overlay 129's trampolines find it at PMFG_OPTIONS_ENTRY
__attribute__((section(".init"))) const OverlayFunc gPmfgOptionsEntry[3] = {
    PMFG_OptionsScreen_Init, PMFG_OptionsScreen_Main, PMFG_OptionsScreen_Exit,
};
