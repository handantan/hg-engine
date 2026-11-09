#include "types.h"

#include "test_battle.h"

#include "battle.h"
#include "message.h"

void LONG_CALL BattleMessage_ExpandPlaceholders(struct BattleSystem *battleSystem, MsgData *data, BattleMessage *msg)
{
    String *str = NewString_ReadMsgData(data, msg->id);
    StringExpandPlaceholders(battleSystem->msgFormat, battleSystem->msgBuffer, str);
    String_Delete(str);

#ifdef DEBUG_BATTLE_SCENARIOS
    TestBattle_CheckMessage(battleSystem); // PMFG: moved to src/test_battle.c, the battle overlay is full
#endif // DEBUG_BATTLE_SCENARIOS
}
