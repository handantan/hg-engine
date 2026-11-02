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

.include "data/scr_seq/include/event_R27.inc"


// text archive to grab from: 369.txt

.data


scrdef scr_seq_R27_000
scrdef scr_seq_R27_001
scrdef scr_seq_R27_002
scrdef scr_seq_R27_003
scrdef scr_seq_R27_004
scrdef scr_seq_R27_005
scrdef_end

scr_seq_R27_003:
	goto_if_set FLAG_UNK_168, _0026
	scrcmd_804 1
	end

_0026:
	end

scr_seq_R27_000:
	simple_npc_msg 1
	end

scr_seq_R27_001:
	scrcmd_609
	lockall
	apply_movement obj_R27_gsbigman, _00A8
	wait_movement
	npc_msg 0
	closemsg
	get_player_coords VAR_TEMP_x4000, VAR_TEMP_x4001
	compare VAR_TEMP_x4001, 402
	goto_if_ne _006F
	apply_movement obj_R27_gsbigman, _00C4
	goto _0092

_006F:
	compare VAR_TEMP_x4001, 403
	goto_if_ne _008A
	apply_movement obj_R27_gsbigman, _00B0
	goto _0092

_008A:
	apply_movement obj_R27_gsbigman, _00B8
_0092:
	wait_movement
	npc_msg 1
	wait_button
	closemsg
	releaseall
	setvar VAR_UNK_408A, 1
	end

	.byte 0x00, 0x00, 0x00
	.balign 4
_00A8:

	step 75, 1
	step_end
	.balign 4
_00B0:

	step 14, 4
	step_end
	.balign 4
_00B8:

	step 13, 1
	step 14, 4
	step_end
	.balign 4
_00C4:

	step 12, 1
	step 14, 4
	step_end
scr_seq_R27_002:
	scrcmd_055 2, 0
	scrcmd_057 3
	scrcmd_058
	trainer_tips 2, VAR_SPECIAL_RESULT
	callstd std_signpost
	end

scr_seq_R27_004:
	direction_signpost 3, 1, 5, VAR_SPECIAL_RESULT
	scrcmd_057 3
	scrcmd_058
	scrcmd_060 VAR_SPECIAL_RESULT
	callstd std_signpost
	end
	.balign 4

// REQ-TGT-MAP-006: SoulGold Route Expert, battles once every trainer listed has been beaten
scr_seq_R27_005:
	play_se SEQ_SE_DP_SELECT
	lockall
	faceplayer
	goto_if_defeated TRAINER_PMFG_CHOICE_BAND_EXPERT, _PMFG_ExpertDone
	goto_if_not_defeated TRAINER_FISHERMAN_SCOTT, _PMFG_ExpertWait
	goto_if_not_defeated TRAINER_PSYCHIC_M_VERNON, _PMFG_ExpertWait
	goto_if_not_defeated TRAINER_ACE_TRAINER_F_JOYCE, _PMFG_ExpertWait
	goto_if_not_defeated TRAINER_ACE_TRAINER_M_GAVEN, _PMFG_ExpertWait
	goto_if_not_defeated TRAINER_ACE_TRAINER_M_JAKE, _PMFG_ExpertWait
	goto_if_not_defeated TRAINER_ACE_TRAINER_F_JAMIE, _PMFG_ExpertWait
	goto_if_not_defeated TRAINER_ACE_TRAINER_F_MEGAN, _PMFG_ExpertWait
	goto_if_not_defeated TRAINER_ACE_TRAINER_M_BLAKE, _PMFG_ExpertWait
	goto_if_not_defeated TRAINER_BIRD_KEEPER_GS_JOSE_2, _PMFG_ExpertWait
	goto_if_not_defeated TRAINER_ACE_TRAINER_M_BRIAN, _PMFG_ExpertWait
	goto_if_not_defeated TRAINER_PMFG_GILBERT, _PMFG_ExpertWait
	goto_if_not_defeated TRAINER_ACE_TRAINER_F_REENA, _PMFG_ExpertWait
	npc_msg 5
	closemsg
	trainer_battle TRAINER_PMFG_CHOICE_BAND_EXPERT, 0, 0, 0
	check_battle_won VAR_SPECIAL_RESULT
	compare VAR_SPECIAL_RESULT, 0
	goto_if_eq _PMFG_ExpertLost
	npc_msg 6
	giveitem_no_check ITEM_CHOICE_BAND, 1
	settrainerflag TRAINER_PMFG_CHOICE_BAND_EXPERT
_PMFG_ExpertDone:
	npc_msg 7
	wait_button
	closemsg
	releaseall
	end

_PMFG_ExpertWait:
	npc_msg 4
	wait_button
	closemsg
	releaseall
	end

_PMFG_ExpertLost:
	white_out
	releaseall
	end
	.balign 4
