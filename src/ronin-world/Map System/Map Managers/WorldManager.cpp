/***
 * Demonstrike Core
 */

//
// WorldCreator.cpp
//

#include "StdAfx.h"

SERVER_DECL WorldManager sWorldMgr;

static const uint32 MapInstanceUpdatePeriod = 50;

WorldManager::WorldManager()
{

}

bool WorldManager::ValidateMapId(uint32 mapId)
{
    if(ContinentManagerExists(mapId))
    {
        ContinentManager *mgr = GetContinentManager(mapId);
        if(mgr->GetContinent()->IsPreloading())
            return false;
        return true;
    }

    return m_maps.find(mapId) != m_maps.end();
}

void WorldManager::Load(TaskList * l)
{
    new InstanceManager();
    new WorldStateTemplateManager();
    sWorldStateTemplateManager.LoadFromDB();

    sInstanceMgr.Prepare();

    // create maps for any we don't have yet.
    std::deque<MapEntry*> m_continent, m_otherMaps;
    for(uint32 i = 0; i < dbcMap.GetNumRows(); i++)
    {
        MapEntry *map = dbcMap.LookupRow(i);
        if(map == NULL)
            continue;
        if(m_maps.find(map->MapID) != m_maps.end())
            continue;
        if(!map->IsContinent() && !map->Instanceable())
            continue;

        if(map->IsContinent())
            m_continent.push_back(map);
        else m_otherMaps.push_back(map);
    }

    while(!m_continent.empty())
    {
        MapEntry *entry = m_continent.front();
        m_continent.pop_front();
        l->AddTask(new Task(new CallbackP1<WorldManager, MapEntry*>(this, &WorldManager::_CreateMap, entry)));
    }
    l->wait();

    while(!m_otherMaps.empty())
    {
        MapEntry *entry = m_otherMaps.front();
        m_otherMaps.pop_front();
        l->AddTask(new Task(new CallbackP1<WorldManager, MapEntry*>(this, &WorldManager::_CreateMap, entry)));
    }
    l->wait();

    for(auto itr = m_continentManagement.begin(); itr != m_continentManagement.end(); itr++)
        itr->second->SetThreadState(THREADSTATE_AWAITING);

    // load saved instances
    _LoadInstances();

    sInstanceMgr.Launch();
}

WorldManager::~WorldManager()
{
    delete WorldStateTemplateManager::getSingletonPtr();
}

void WorldManager::Shutdown()
{
    // Map manager threads are self cleanup
    m_continentManagement.clear();
    m_maps.clear();
}

uint32 WorldManager::PreTeleport(uint32 mapId, Player* plr, uint32 &instanceid)
{
    // preteleport is where all the magic happens :P instance creation, etc.
    MapEntry *map = dbcMap.LookupEntry(mapId);
    if(map == NULL) //is the map vaild?
        return INSTANCE_ABORT_NOT_FOUND;

    // main continent check.
    if(map->IsContinent())
    {
        // we can check if the destination world server is online or not and then cancel them before they load.
        if(ContinentManagerExists(mapId))
            return INSTANCE_OK;
        return INSTANCE_ABORT_NOT_FOUND;
    }

    // shouldn't happen
    if(map->IsBattleGround() || map->IsBattleArena())
        return INSTANCE_ABORT_NOT_FOUND;

    if(map->IsRaid()) // check that heroic mode is available if the player has requested it.
    {
        if(plr->iRaidType > 1 && (map->mapFlags & 0x100) == 0)
            return INSTANCE_ABORT_HEROIC_MODE_NOT_AVAILABLE;
    }
    else if(plr->iInstanceType && (map->mapFlags & 0x100) == 0)
        return INSTANCE_ABORT_HEROIC_MODE_NOT_AVAILABLE;

    //do we need addition raid/heroic checks?
    Group * pGroup = plr->GetGroup();
    if( !plr->triggerpass_cheat )
    {
        // players without groups cannot enter raid instances (no soloing them:P)
        if( pGroup == NULL && (map->IsRaid() || (map->mapFlags & 0x100)))
            return INSTANCE_ABORT_NOT_IN_RAID_GROUP;

        //and has the required level
        if( plr->getLevel() < 80)
        {
            if(uint32 instanceType = map->IsRaid() ? plr->iRaidType : plr->iInstanceType)
            {
                if(map->addon == 3 && plr->getLevel() < 90)
                    return INSTANCE_ABORT_HEROIC_MODE_NOT_AVAILABLE;
                else if(map->addon == 2 && plr->getLevel() < 80)
                    return INSTANCE_ABORT_HEROIC_MODE_NOT_AVAILABLE;
                else if(map->addon == 1 && plr->getLevel() < 70)
                    return INSTANCE_ABORT_HEROIC_MODE_NOT_AVAILABLE;
                else if(map->addon)
                    return INSTANCE_ABORT_HEROIC_MODE_NOT_AVAILABLE;
            }

            //Instance keys
        }
    }

    // if we are here, it means:
    // 1) we're a non-raid instance
    // 2) we're a raid instance, and the person is in a group.
    // so, first we have to check if they have an instance on this map already, if so, allow them to teleport to that.
    // next we check if there is a saved instance belonging to him.
    // otherwise, we can create them a new one.
    if(instanceid == 0)
        instanceid = plr->GetLinkedInstanceID(map);

    uint32 res = sInstanceMgr.PreTeleportInstanceCheck(plr->GetGUID(), mapId, instanceid, plr->CanCreateNewDungeon(mapId));
    if(res == INSTANCE_ABORT_CREATE_NEW_INSTANCE)
    {
        instanceid = 0;
        return INSTANCE_OK;
    }

    return res;
}

const uint32 GetBGForMapID(uint32 type)
{
    switch(type)
    {
    case 30:
        return BATTLEGROUND_ALTERAC_VALLEY;
    case 489:
        return BATTLEGROUND_WARSONG_GULCH;
    case 529:
        return BATTLEGROUND_ARATHI_BASIN;
    case 566:
        return BATTLEGROUND_EYE_OF_THE_STORM;
    case 607:
        return BATTLEGROUND_STRAND_OF_THE_ANCIENTS;
    case 628:
        return BATTLEGROUND_ISLE_OF_CONQUEST;
    case 559:
    case 562:
    case 572:
    case 617:
    case 618:
        return BATTLEGROUND_ARENA;
    }
    return 0;
};

bool WorldManager::PushToWorldQueue(WorldObject *obj)
{
    if(MapInstance* mapInstance = GetInstance(obj))
    {
        if(Player* p = obj->IsPlayer() ? castPtr<Player>(obj) : NULL)
        {
            // battleground checks
            if( p->m_bg == NULL && mapInstance->m_battleground != NULL )
            {
                // player hasn't been registered in the battleground, ok.
                // that means we re-logged into one. if it's an arena, don't allow it!
                // also, don't allow them in if the bg is full.
                if( !mapInstance->m_battleground->CanPlayerJoin(p) && !p->bGMTagOn)
                    return false;
            }

            // players who's group disbanded cannot remain in a raid instances alone(no soloing them:P)
            if( !p->triggerpass_cheat && p->GetGroup()== NULL && (mapInstance->IsRaid() || mapInstance->GetdbcMap()->IsMultiDifficulty()))
                return false;

            p->m_beingPushed = true;
            if(WorldSession *sess = p->GetSession())
                sess->SetEventInstanceId(mapInstance->GetInstanceID());

            if(!mapInstance->IsRaid()) p->LinkToInstance(mapInstance);
        } else if(Creature *c = obj->IsCreature() ? castPtr<Creature>(obj) : NULL)
            if(!c->CanAddToWorld())
                return false;

        mapInstance->AddObject(obj);
        return true;
    }
    return false;
}

MapInstance *WorldManager::GetInstance(WorldObject* obj)
{
    if(ContinentManager *manager = GetContinentManager(obj->GetMapId()))
        return manager->GetContinent();
    /*else if(BattleGroundManager *manager = GetBattleGroundManager(obj->GetMapId()))
        return manager->GetBattleground(obj->GetBGInstanceID());*/
    else if(MapInstance *instance = sInstanceMgr.GetInstanceForObject(obj))
        return instance;
    return NULL;
}

void WorldManager::_CreateMap(MapEntry *mapEntry)
{
    m_mapLock.Acquire();
    Map *map = new Map(mapEntry, mapEntry->name);
    m_maps.insert(std::make_pair(mapEntry->MapID, map));
    m_mapLock.Release();
    if(mapEntry->IsContinent())
        _InitializeContinent(mapEntry, map);
    else if(mapEntry->IsBattleGround() || mapEntry->IsBattleArena())
        _InitializeBattleGround(mapEntry, map);
    else if(mapEntry->IsRaid() || mapEntry->IsDungeon())
        _InitializeInstance(mapEntry, map);
    else { m_mapLock.Acquire(); m_maps.erase(mapEntry->MapID); delete map; m_mapLock.Release(); }
}

void WorldManager::BuildXMLStats(char * m_file)
{
    for(auto itr = m_continentManagement.begin(); itr != m_continentManagement.end(); itr++)
        ;//itr->second->BuildStats(m_file);

    sLog.Debug("WorldManager", "Dumping XML stats...");
}

void WorldManager::_LoadInstances()
{
    // clear any instances that have expired.
    sLog.Notice("WorldManager", "Deleting Expired Instances...");
    CharacterDatabase.WaitExecute("DELETE FROM instances WHERE expiration <= %u", UNIXTIME);

    // load saved instances
    if(QueryResult *result = CharacterDatabase.Query("SELECT * FROM instances"))
    {
        uint32 count = 0;
        do
        {
            //if(LoadInstance(result->Fetch()))
                count++;
        } while(result->NextRow());
        delete result;

        sLog.Success("WorldManager", "Loaded %u saved instance(s)." , count);
    } else sLog.Debug("WorldManager", "No saved instances found.");
}

void WorldManager::ResetHeroicInstances()
{

}

bool WorldManager::_DeleteInstance(MapInstance * in, bool ForcePlayersOut, bool atSelfEnd)
{
    m_mapLock.Acquire();

    m_mapLock.Release();
    return true;
}

void WorldManager::CheckForExpiredInstances()
{

}

void WorldManager::BuildSavedInstancesForPlayer(Player* plr)
{
    std::set<uint32> mapIds;

    WorldPacket data(SMSG_UPDATE_INSTANCE_OWNERSHIP, 4);
    data << uint32(mapIds.size() ? 0x01 : 0x00);
    plr->GetSession()->SendPacket(&data);

    for(std::set<uint32>::iterator itr = mapIds.begin(); itr != mapIds.end(); itr++)
        plr->GetSession()->OutPacket(SMSG_UPDATE_LAST_INSTANCE, 4, ((uint8*)&(*itr)));
}

void WorldManager::BuildSavedRaidInstancesForPlayer(Player* plr)
{
    uint32 counter = 0;
    WorldPacket data(SMSG_RAID_INSTANCE_INFO, 200);
    data << counter;
    *(uint32*)&data.contents()[0] = counter;
    plr->GetSession()->SendPacket(&data);
}

void WorldManager::PlayerLeftGroup(Group * pGroup, Player* pPlayer)
{

}

MapInstance* WorldManager::CreateBattlegroundInstance(uint32 mapid)
{
    return NULL;
}

void WorldManager::DeleteBattlegroundInstance(uint32 mapid, uint32 instanceid)
{
    m_mapLock.Acquire();
    printf("Could not delete battleground instance!\n");
    m_mapLock.Release();
}

void WorldManager::_InitializeContinent(MapEntry *mapEntry, Map *map)
{
    ContinentManager *mgr = new ContinentManager(mapEntry, map);
    if(!mgr->Initialize()) // Initialize the manager before starting the thread
    {
        delete mgr;
        return;
    }

    map->Initialize();

    // Store the manager in the worldManager thread
    m_continentManagement.insert(std::make_pair(mapEntry->MapID, mgr));

    // Set manager to start it's internal thread
    ThreadPool.ExecuteTask(format("ContinentMgr - M%u", mapEntry->MapID).c_str(), mgr);
}

void WorldManager::_InitializeBattleGround(MapEntry *mapEntry, Map *map)
{
    // Temp: clean up the map for now
    m_mapLock.Acquire();
    m_maps.erase(mapEntry->MapID); delete map;
    m_mapLock.Release();
}

void WorldManager::_InitializeInstance(MapEntry *mapEntry, Map *map)
{
    // Initialize the map data first
    map->Initialize();
    // Push map data to instance manager
    sInstanceMgr.AddMapData(mapEntry, map);
}
