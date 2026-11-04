#include "config.h"
#include "types.h"
#include "map_events_internal.h"
#include "rtc.h"
#include "save.h"
#include "pmfg_settings.h"

// PMFG field-side settings hooked into vanilla arm9 code (engine/hooks)

// REQ-TGT-QOL-002 autorun (refs/pokehns-expansion/src/field_player_avatar.c:905-945): ON runs unless B is held.
// hooks: replaces vanilla's run-button check (0x0205DE88, `return heldKeys & PAD_BUTTON_B`); both callers then pick the
// run movement, and only when the player has running shoes
BOOL LONG_CALL PMFG_PlayerAvatar_IsRunHeld(FIELD_PLAYER_AVATAR *avatar, u16 heldKeys)
{
    BOOL held = (heldKeys & 2) != 0; // PAD_BUTTON_B
    // avatar_sub->unk2 is pret's runningShoesLock, which already forces B in FieldInput_Update: keep running then
    if (PMFG_GetSetting(SETTING_AUTORUN) == SETTING_AUTORUN_ON && !avatar->avatar_sub->unk2) {
        return !held;
    }
    return held;
}

// REQ-TGT-QOL-002 fast_surf (refs/pokehns-expansion/src/field_player_avatar.c:933). HGSS already surfs at running speed
// (MOVEMENT_RUN_*, 16), so ON uses the bike's faster step (20, as vanilla's sub_0205DF0C). asm/general_hooks.s
// PMFG_FastSurf_hook replaces `mov r2, #0x10` in the surf branch of vanilla sub_0205D4B4 (0x0205D59E)
u32 LONG_CALL PMFG_SurfStepMovement(void)
{
    return PMFG_GetSetting(SETTING_FAST_SURF) == SETTING_FAST_SURF_ON ? 20 : 16;
}

// REQ-TGT-SYS-007 clock FAKE_RTC (refs/pokehns-expansion/include/fake_rtc.h:7-15): 1 real hour is 1 in-game day.
// The in-game clock is the real one sped up 24x, so nothing is stored: FAKE -> RTC simply steps the clock back, which
// HGSS takes like any clock change (sub_02055478 re-anchors; the 24 h penalty is only for a new console).
// shortcut: the sped-up clock wraps every 36525 in-game days (2000-2099), i.e. every ~4.2 real years
// hooks: replaces vanilla GF_RTC_UpdateOnFrame (0x0201466C), so every read after boot ends in this file's callback
// instead of vanilla's (0x02014694). The callback itself can't be hooked: NitroMain starts the first read
// (GF_InitRTCWork) before it loads overlay 129, where this code lives.
struct PmfgRtcWork { // pret src/gf_rtc.c struct GFRtcWork, up to the async copies
    BOOL getDateTimeSuccess;
    BOOL getDateTimeLock;
    s32 getDateTimeSleep;
    s32 getDateTimeErrorCode;
    struct RTCDate date;
    struct RTCTime time;
    struct RTCDate date_async;
    struct RTCTime time_async;
};

#define PMFG_DAYS_2000_TO_2099 36525 // every 4th year is a leap year in that range, 2000 included
static const u16 sDaysBeforeMonth[12] = { 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334 };

static u32 DaysBeforeMonth(u32 year, u32 month)
{
    return sDaysBeforeMonth[month - 1] + (month > 2 && year % 4 == 0);
}

static void DateFromDays(u32 days, struct RTCDate *date)
{
    date->week = (6 + days) % 7; // 2000-01-01 was a Saturday
    u32 year = 0;
    while (days >= 365u + (year % 4 == 0)) {
        days -= 365 + (year % 4 == 0);
        year++;
    }
    u32 month = 12;
    while (DaysBeforeMonth(year, month) > days) {
        month--;
    }
    date->year = year;
    date->month = month;
    date->day = days - DaysBeforeMonth(year, month) + 1;
}

static void PMFG_RTC_GetDateTime_Callback(s32 result, void *arg)
{
    struct PmfgRtcWork *work = arg;
    work->getDateTimeErrorCode = result;
    work->getDateTimeSuccess = TRUE;
    work->date = work->date_async;
    work->time = work->time_async;
    // the RTC starts before the save exists (pret main.c), and settings live in save vars
    if (SaveBlock2_get() != NULL && PMFG_GetSetting(SETTING_CLOCK) == SETTING_CLOCK_FAKE_RTC) {
        u32 days = work->date.year * 365 + (work->date.year + 3) / 4 + DaysBeforeMonth(work->date.year, work->date.month) + work->date.day - 1;
        u32 seconds = work->time.hour * 3600 + work->time.minute * 60 + work->time.second;
        DateFromDays((days * 24 + seconds / 3600) % PMFG_DAYS_2000_TO_2099, &work->date); // in u32: no 64-bit math
        seconds = seconds * 24 % 86400;
        work->time.hour = seconds / 3600;
        work->time.minute = seconds / 60 % 60;
        work->time.second = seconds % 60;
    }
    work->getDateTimeLock = FALSE;
}

#define PMFG_RTC_WORK ((struct PmfgRtcWork *)0x021D1048) // pret sRTCWork (GF_RTC_UpdateOnFrame's literal pool)
#define RTC_GetDateTimeAsync ((s32 (*)(struct RTCDate *, struct RTCTime *, void (*)(s32, void *), void *))0x020DBB80) // SDK, ARM

void LONG_CALL PMFG_RTC_UpdateOnFrame(void)
{
    struct PmfgRtcWork *work = PMFG_RTC_WORK;
    if (!work->getDateTimeLock && ++work->getDateTimeSleep > 10) { // vanilla: a new read every 10 frames
        work->getDateTimeSleep = 0;
        work->getDateTimeLock = TRUE;
        work->getDateTimeErrorCode = RTC_GetDateTimeAsync(&work->date_async, &work->time_async, PMFG_RTC_GetDateTime_Callback, work);
    }
}
