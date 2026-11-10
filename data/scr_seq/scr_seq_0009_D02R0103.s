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

.include "data/scr_seq/include/event_D02R0103.inc"


// text archive to grab from: 050.txt

.data


scrdef scr_seq_D02R0103_000
scrdef scr_seq_D02R0103_001
scrdef scr_seq_D02R0103_002
scrdef scr_seq_D02R0103_003
scrdef scr_seq_D02R0103_004
scrdef_end

scr_seq_D02R0103_002:
	// pmfg: REQ-TGT-MAP-013: Jirachi only shows once the White Rock summoned it
	goto_if_set FLAG_PMFG_JIRACHI_SUMMONED, _PMFG_JirachiShown
	hide_person obj_D02R0103_pmfg_jirachi
_PMFG_JirachiShown:
	make_object_visible obj_D02R0103_stop
	scrcmd_379 VAR_TEMP_x4001
	compare VAR_TEMP_x4001, 3
	goto_if_ne _003D
	move_person_facing obj_D02R0103_stop, 18, 0, 10, DIR_SOUTH
	move_warp 2, 20, 3
	goto _005E

_003D:
	compare VAR_TEMP_x4001, 4
	goto_if_ne _005E
	move_person_facing obj_D02R0103_stop, 18, 0, 10, DIR_SOUTH
	move_warp 2, 20, 3
_005E:
	end

scr_seq_D02R0103_000:
	scrcmd_055 2, 0
	scrcmd_057 3
	scrcmd_058
	trainer_tips 0, VAR_SPECIAL_RESULT
	callstd std_signpost
	end

scr_seq_D02R0103_001:
	simple_npc_msg 1
	end

// pmfg: REQ-TGT-MAP-013 (H&S REQ-HNS-MAP-014, MtMoon_Outside_hns/scripts.inc:22-137): the White Rock summons Jirachi at night
scr_seq_D02R0103_003:
	play_se SEQ_SE_DP_SELECT
	lockall
	npc_msg 2
	yesno VAR_SPECIAL_RESULT
	compare VAR_SPECIAL_RESULT, 1
	goto_if_eq _PMFG_RockEnd
	goto_if_set FLAG_PMFG_JIRACHI_SUMMONED, _PMFG_RockNothing
	scrcmd_379 VAR_TEMP_x4001
	compare VAR_TEMP_x4001, 3
	goto_if_eq _PMFG_RockNight
	compare VAR_TEMP_x4001, 4
	goto_if_eq _PMFG_RockNight
_PMFG_RockNothing:
	npc_msg 3
	wait_button
_PMFG_RockEnd:
	closemsg
	releaseall
	end

_PMFG_RockNight:
	npc_msg 4
	wait_button
	closemsg
	play_se SEQ_SE_DP_SELECT
	fade_screen 4, 4, 0, RGB_WHITE
	wait_fade
	setflag FLAG_PMFG_JIRACHI_SUMMONED
	show_person obj_D02R0103_pmfg_jirachi
	fade_screen 4, 4, 1, RGB_WHITE
	wait_fade
	play_cry SPECIES_JIRACHI, 0
	wait_cry
	releaseall
	end

scr_seq_D02R0103_004:
	play_se SEQ_SE_DP_SELECT
	lockall
	faceplayer
	play_cry SPECIES_JIRACHI, 0
	npc_msg 5
	wait_cry
	closemsg
	wild_battle SPECIES_JIRACHI, 50, 0
	check_battle_won VAR_SPECIAL_RESULT
	compare VAR_SPECIAL_RESULT, 0
	goto_if_eq _PMFG_JirachiLost
	get_static_encounter_outcome VAR_TEMP_x4002
	compare VAR_TEMP_x4002, 3
	goto_if_eq _PMFG_JirachiEnd
	compare VAR_TEMP_x4002, 4
	call_if_eq _PMFG_JirachiCaught
	setflag FLAG_PMFG_JIRACHI_GONE
	hide_person obj_D02R0103_pmfg_jirachi
_PMFG_JirachiEnd:
	releaseall
	end

_PMFG_JirachiLost:
	white_out
	releaseall
	end

_PMFG_JirachiCaught:
	setflag FLAG_PMFG_CAUGHT_JIRACHI
	return
	.balign 4
