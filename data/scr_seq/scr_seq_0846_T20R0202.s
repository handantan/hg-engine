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

.include "data/scr_seq/include/event_T20R0202.inc"


// text archive to grab from: 546.txt

.data


scrdef scr_seq_T20R0202_000
scrdef scr_seq_T20R0202_001
scrdef_end

// pmfg: SYS-008, the home PC offers the settings screen too, as H&S's bedroom PC does
// (refs/pokehns-expansion/data/maps/NewBarkTown_PlayersHouse_2F_hns/scripts.inc uses EventScript_PC)
scr_seq_T20R0202_000:
	scrcmd_609
	lockall
	play_se SEQ_SE_DP_PC_ON
	buffer_players_name 0
	npc_msg 3
	touchscreen_menu_hide
	menu_init_std_gmm 1, 1, 0, 1, VAR_SPECIAL_RESULT
	menu_item_add 73, 255, 0 // MAILBOX
	menu_item_add 477, 255, 1 // SETTINGS
	menu_item_add 66, 255, 2 // SWITCH OFF
	menu_exec
	switch VAR_SPECIAL_RESULT
	case 0, _PMFG_Mailbox
	case 1, _PMFG_Settings
	closemsg
	play_se SEQ_SE_DP_PC_LOGOFF
	touchscreen_menu_show
	releaseall
	end

_PMFG_Settings:
	play_se SEQ_SE_DP_PC_LOGIN
	closemsg
	fade_screen 6, 1, 0, RGB_BLACK
	wait_fade
	pmfg_settings_viewer
	restore_overworld
	fade_screen 6, 1, 1, RGB_BLACK
	wait_fade
	touchscreen_menu_show
	releaseall
	end

_PMFG_Mailbox:
	closemsg
	touchscreen_menu_show
	scrcmd_377 VAR_SPECIAL_RESULT
	compare VAR_SPECIAL_RESULT, 0
	goto_if_eq _004B
	fade_screen 6, 1, 0, RGB_BLACK
	wait_fade
	scrcmd_376
	restore_overworld
	fade_screen 6, 1, 1, RGB_BLACK
	wait_fade
	releaseall
	end

_004B:
	npc_msg 1
	wait_button
	closemsg
	releaseall
	end

scr_seq_T20R0202_001:
	play_se SEQ_SE_DP_SELECT
	lockall
	npc_msg 2
	wait_button
	closemsg
	releaseall
	end
	.balign 4
