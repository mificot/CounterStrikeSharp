/*
*  This file is part of CounterStrikeSharp.
*  CounterStrikeSharp is free software: you can redistribute it and/or modify
*  it under the terms of the GNU General Public License as published by
*  the Free Software Foundation, either version 3 of the License, or
*  (at your option) any later version.
*
*  CounterStrikeSharp is distributed in the hope that it will be useful,
*  but WITHOUT ANY WARRANTY; without even the implied warranty of
*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*  GNU General Public License for more details.
*
*  You should have received a copy of the GNU General Public License
*  along with CounterStrikeSharp.  If not, see <https://www.gnu.org/licenses/>. *
*/
#include <iterator>

#include "core/managers/entity_manager.h"

#include <public/eiface.h>
#include "scripting/callback_manager.h"

SH_DECL_HOOK6_void(ISource2GameEntities, CheckTransmit, SH_NOATTRIB, false, CCheckTransmitInfo**, int, CBitVec<16384>&, const Entity2Networkable_t **, const uint16 *, int);

namespace counterstrikesharp {

struct TransmitInfo
{
	CBitVec<16384> *m_pTransmitEdict;
	uint8_t unknown[552];
	CPlayerSlot m_nClientEntityIndex;
};

EntityManager::EntityManager() {}

EntityManager::~EntityManager() {}

void EntityManager::OnAllInitialized() {
    SH_ADD_HOOK(ISource2GameEntities, CheckTransmit, g_pSource2GameEntities, SH_MEMBER(this, &EntityManager::OnEntityCheckTransmit), true);

    on_entity_spawned_callback = globals::callbackManager.CreateCallback("OnEntitySpawned");
    on_entity_created_callback = globals::callbackManager.CreateCallback("OnEntityCreated");
    on_entity_deleted_callback = globals::callbackManager.CreateCallback("OnEntityDeleted");
    on_entity_parent_changed_callback = globals::callbackManager.CreateCallback("OnEntityParentChanged");
    on_entity_check_transmit = globals::callbackManager.CreateCallback("OnEntityCheckTransmit");
    // Listener is added in ServerStartup as entity system is not initialised at this stage.
}

void EntityManager::OnShutdown() {
    SH_REMOVE_HOOK(ISource2GameEntities, CheckTransmit, g_pSource2GameEntities, SH_MEMBER(this, &EntityManager::OnEntityCheckTransmit), true);

    globals::callbackManager.ReleaseCallback(on_entity_spawned_callback);
    globals::callbackManager.ReleaseCallback(on_entity_created_callback);
    globals::callbackManager.ReleaseCallback(on_entity_deleted_callback);
    globals::callbackManager.ReleaseCallback(on_entity_parent_changed_callback);
    globals::callbackManager.ReleaseCallback(on_entity_check_transmit);
    globals::entitySystem->RemoveListenerEntity(&entityListener);
}

void CEntityListener::OnEntitySpawned(CEntityInstance *pEntity) {
    auto callback = globals::entityManager.on_entity_spawned_callback;

    if (callback && callback->GetFunctionCount()) {
        callback->ScriptContext().Reset();
        callback->ScriptContext().Push(pEntity);
        callback->Execute();
    }
}
void CEntityListener::OnEntityCreated(CEntityInstance *pEntity) {
    auto callback = globals::entityManager.on_entity_created_callback;

    if (callback && callback->GetFunctionCount()) {
        callback->ScriptContext().Reset();
        callback->ScriptContext().Push(pEntity);
        callback->Execute();
    }
}
void CEntityListener::OnEntityDeleted(CEntityInstance *pEntity) {
    auto callback = globals::entityManager.on_entity_deleted_callback;

    if (callback && callback->GetFunctionCount()) {
        callback->ScriptContext().Reset();
        callback->ScriptContext().Push(pEntity);
        callback->Execute();
    }
}
void CEntityListener::OnEntityParentChanged(CEntityInstance *pEntity, CEntityInstance *pNewParent) {
    auto callback = globals::entityManager.on_entity_parent_changed_callback;

    if (callback && callback->GetFunctionCount()) {
        callback->ScriptContext().Reset();
        callback->ScriptContext().Push(pEntity);
        callback->ScriptContext().Push(pNewParent);
        callback->Execute();
    }
}

void EntityManager::OnEntityCheckTransmit(CCheckTransmitInfo **pInfo, int infoCount, CBitVec<16384> &, const Entity2Networkable_t **pNetworkables, const uint16 *pEntityIndicies, int nEntities) {
    auto callback = globals::entityManager.on_entity_check_transmit;

    if (callback && callback->GetFunctionCount()) {

        for (int i = 0; i < infoCount; i++)
        {
            // Cast it to our own TransmitInfo struct because CCheckTransmitInfo isn't correct.
            TransmitInfo *pTransmitInfo = reinterpret_cast<TransmitInfo*>(pInfo[i]);
            
            // Find out who this info will be sent to.
            CPlayerSlot targetSlot = pTransmitInfo->m_nClientEntityIndex;

            callback->ScriptContext().Reset();
            callback->ScriptContext().Push(targetSlot.Get());
            callback->Execute(false);
            auto bits = callback->ScriptContext().GetResult<uint32*>();

            if (bits && bits[0] > 0) {
                int size = bits[0];

                for (int i = 1; i <= size; i++) {
                    if (pTransmitInfo->m_pTransmitEdict->IsBitSet(bits[i])) {
                        pTransmitInfo->m_pTransmitEdict->Clear(bits[i]);
                    }
                }
            }
        }

        
    }
}
}  // namespace counterstrikesharp