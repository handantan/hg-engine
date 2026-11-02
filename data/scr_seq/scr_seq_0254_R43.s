.include "asm/include/interop_macros.inc"

.include "asm/include/scriptmacros.inc"
.include "asm/include/flags.inc"
.include "asm/include/soundeffects.inc"
.include "asm/include/vars.inc"

.include "asm/include/events.inc"
.include "asm/include/game_stats.inc"
.include "asm/include/maps.inc"
.include "asm/include/map_sections.inc"
.include "asm/include/movements.inc"
.include "asm/include/rankings.inc"
.include "asm/include/spawns.inc"
.include "asm/include/std_scripts.inc"
.include "asm/include/trainers.inc"

#include "constants/item.h"
#include "constants/moves.h"
#include "constants/species.h"

.include "data/scr_seq/include/event_R43.inc"


// text archive to grab from: 401.txt

.data


scrdef scr_seq_R43_000
scrdef scr_seq_R43_001
scrdef scr_seq_R43_002
scrdef scr_seq_R43_003
scrdef scr_seq_R43_004
scrdef_end

scr_seq_R43_000:
	end

scr_seq_R43_001:
	direction_signpost 18, 1, 3, VAR_SPECIAL_RESULT
	scrcmd_057 3
	scrcmd_058
	scrcmd_060 VAR_SPECIAL_RESULT
	callstd std_signpost
	end

scr_seq_R43_002:
	scrcmd_055 3, 0
	scrcmd_057 3
	scrcmd_058
	trainer_tips 19, VAR_SPECIAL_RESULT
	callstd std_signpost
	end

scr_seq_R43_003:
	direction_signpost 20, 1, 6, VAR_SPECIAL_RESULT
	scrcmd_057 3
	scrcmd_058
	scrcmd_060 VAR_SPECIAL_RESULT
	callstd std_signpost
	end
	.balign 4

// REQ-TGT-MAP-006: SoulGold Route Expert, battles once every trainer listed has been beaten
scr_seq_R43_004:
	play_se SEQ_SE_DP_SELECT
	lockall
	faceplayer
	goto_if_defeated TRAINER_PMFG_HERB_EXPERT, _PMFG_ExpertDone
	goto_if_not_defeated TRAINER_POKE_MANIAC_BRENT, _PMFG_ExpertWait
	goto_if_not_defeated TRAINER_POKE_MANIAC_RON, _PMFG_ExpertWait
	goto_if_not_defeated TRAINER_POKE_MANIAC_BECKETT, _PMFG_ExpertWait
	goto_if_not_defeated TRAINER_FISHERMAN_MARVIN, _PMFG_ExpertWait
	goto_if_not_defeated TRAINER_CAMPER_SPENCER, _PMFG_ExpertWait
	goto_if_not_defeated TRAINER_PICNICKER_TIFFANY, _PMFG_ExpertWait
	goto_if_not_defeated TRAINER_FISHERMAN_ANDRE, _PMFG_ExpertWait
	goto_if_not_defeated TRAINER_FISHERMAN_RAYMOND, _PMFG_ExpertWait
	goto_if_not_defeated TRAINER_ACE_TRAINER_M_ALTON, _PMFG_ExpertWait
	goto_if_not_defeated TRAINER_ACE_TRAINER_F_LOIS, _PMFG_ExpertWait
	npc_msg 22
	closemsg
	trainer_battle TRAINER_PMFG_HERB_EXPERT, 0, 0, 0
	check_battle_won VAR_SPECIAL_RESULT
	compare VAR_SPECIAL_RESULT, 0
	goto_if_eq _PMFG_ExpertLost
	npc_msg 23
	giveitem_no_check ITEM_POWER_HERB, 1
	giveitem_no_check ITEM_WHITE_HERB, 1
	settrainerflag TRAINER_PMFG_HERB_EXPERT
_PMFG_ExpertDone:
	npc_msg 24
	wait_button
	closemsg
	releaseall
	end

_PMFG_ExpertWait:
	npc_msg 21
	wait_button
	closemsg
	releaseall
	end

_PMFG_ExpertLost:
	white_out
	releaseall
	end
	.balign 4
