#ifndef PMFG_INNATES_H
#define PMFG_INNATES_H

#include "types.h"

// PMFG innates (REQ-TGT-MON-001/002): up to 3 extra always-on abilities per species, unlocked by story progress.

#define PMFG_NUM_INNATES 3
#define PMFG_FLAG_GAME_CLEAR 0x964 // pret/pokeheartgold include/constants/flags.h FLAG_GAME_CLEAR (Johto League beaten)


u32 LONG_CALL PMFG_UnlockedInnateCount(void);
u32 LONG_CALL PMFG_ProgressLevelCap(void); // REQ-TGT-SYS-002, src/pmfg_progress.c
u32 LONG_CALL PMFG_PartyScalingLevel(void); // REQ-TGT-TRN-002
void LONG_CALL PMFG_GetSpeciesInnates(u16 species, u8 form, u16 *out); // reads PMFG_NUM_INNATES from a028 member CODE_ADDON_PMFG_INNATES

#endif // PMFG_INNATES_H
