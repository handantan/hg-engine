#include "config.h"
#include "types.h"
#include "pokemon.h"
#include "pokemon_storage_system.h"
#include "save.h"
#include "pmfg_options.h"

// PMFG REQ-TGT-QOL-003 criterion 1 (D-063 c+b): trade and tutor scripts pick a party or PC box mon (pmfg_choose_mon).
// A box pick is moved into the party, so HGSS's own commands (NPCTradeExec, MoveTutorInit...) run unchanged on that
// slot; pmfg_return_box_mon moves whatever is in that slot then (the traded-in mon, the tutored mon) back to the box.
// Until then the mon is only in the party: an interrupted script loses nothing (it is as if the player withdrew it).

#define SAVE_PCSTORAGE 41 // pret include/constants/save_arrays.h
void *SaveArray_Get(SaveData *saveData, int id); // src/save.c

static struct {
    u8 active, box, slot, partySlot, swapped;
} sTaken;

static PCStorage *Storage(void)
{
    return SaveArray_Get(SaveBlock2_get(), SAVE_PCSTORAGE);
}

u16 LONG_CALL PMFG_ChooserTake(u16 pick)
{
    if (pick < 0x400) {
        return pick; // a party slot or PMFG_CHOOSER_NONE
    }
    struct Party *party = SaveData_GetPlayerPartyPtr(SaveBlock2_get());
    PCStorage *storage = Storage();
    struct PartyPokemon mon;
    sTaken.box = (pick >> 5) & 0x1F;
    sTaken.slot = pick & 0x1F;
    CopyBoxPokemonToPokemon(&storage->boxes[sTaken.box].mons[sTaken.slot], &mon);
    sTaken.swapped = party->count >= 6;
    if (sTaken.swapped) {
        // shortcut: a full party trades places with its last mon, which goes through the box (full HP, no status, as a
        // PC deposit); upgrade if a party-slot-only flow ever needs that mon untouched
        sTaken.partySlot = 5;
        PCStorage_PlaceMonInBoxByIndexPair(storage, sTaken.box, sTaken.slot, (struct BoxPokemon *)Party_GetMonByIndex(party, 5));
        *Party_GetMonByIndex(party, 5) = mon;
    } else {
        sTaken.partySlot = party->count;
        PokeParty_Add(party, &mon);
        PCStorage_DeleteBoxMonByIndexPair(storage, sTaken.box, sTaken.slot);
    }
    sTaken.active = TRUE;
    return sTaken.partySlot;
}

void LONG_CALL PMFG_ChooserReturn(void)
{
    if (!sTaken.active) {
        return;
    }
    sTaken.active = FALSE;
    struct Party *party = SaveData_GetPlayerPartyPtr(SaveBlock2_get());
    PCStorage *storage = Storage();
    struct PartyPokemon *taken = Party_GetMonByIndex(party, sTaken.partySlot), bumped;
    if (sTaken.swapped) {
        CopyBoxPokemonToPokemon(&storage->boxes[sTaken.box].mons[sTaken.slot], &bumped);
        PCStorage_PlaceMonInBoxByIndexPair(storage, sTaken.box, sTaken.slot, (struct BoxPokemon *)taken);
        *taken = bumped;
    } else {
        PCStorage_PlaceMonInBoxByIndexPair(storage, sTaken.box, sTaken.slot, (struct BoxPokemon *)taken);
        PokeParty_Delete(party, sTaken.partySlot);
    }
}
