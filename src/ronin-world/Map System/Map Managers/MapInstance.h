/***
 * Demonstrike Core
 */

//
// MapInstance->h
//

#pragma once

class MapCell;
class MapManager;
class WorldObject;
class WorldSession;
class GameObject;
class Creature;
class Player;
class Pet;
class Transporter;
class Corpse;
class CBattleground;
class Transporter;

enum ObjectActiveState
{
    OBJECT_STATE_NONE    = 0,
    OBJECT_STATE_INACTIVE = 1,
    OBJECT_STATE_ACTIVE   = 2,
};

static const uint32 MaxObjectViewDistance = 8100;
static const uint32 MaxPlayerViewDistance = 38000;

#define MAX_TRANSPORTERS_PER_MAP 25
#define RESERVE_EXPAND_SIZE 1024

#define TRIGGER_INSTANCE_EVENT( Mgr, Func )

/// Storage pool used to store and dynamically update objects in our map
template < class T > class StoragePool
{
public:
    StoragePool() : mPoolCounter(0), mPoolAddCounter(0), mPoolSize(0) { mPoolStack = NULL; mPoolLastUpdateStack = NULL; }

    void Initialize(uint32 poolSize)
    {
        mPoolSize = poolSize;
        poolIterator = mPool.end();
        mPoolCounter = mPoolAddCounter = 0;
        if(mPoolSize == 1) // Don't initialize the single stack
            return;
        mPoolLastUpdateStack = new uint32[mPoolSize];
        mPoolStack = new std::set<T*>[mPoolSize];
    }

    void Cleanup()
    {
        delete [] mPoolLastUpdateStack;
        delete [] mPoolStack;
    }

    void Update(uint32 msTime, uint32 pDiff)
    {
        T* ptr = NULL;
        uint32 diff = pDiff;
        std::set<T*> *targetPool;
        if(mPoolStack == NULL) // No stack so use our main pool
            targetPool = &mPool;
        else
        {
            // Select our next pool to update in the sequence
            if(++mPoolCounter == mPoolSize)
                mPoolCounter = 0;
            // Recalculate the diff from the last time we updated this pool
            diff = msTime - mPoolLastUpdateStack[mPoolCounter];
            mPoolLastUpdateStack[mPoolCounter] = msTime;
            // Set the target pool pointer
            targetPool = &mPoolStack[mPoolCounter];
        }

        if(!targetPool->empty())
        {
            poolIterator = targetPool->begin();
            while(poolIterator != targetPool->end())
            {
                ptr = *poolIterator;
                ++poolIterator;

                if(ptr->IsActiveObject() && !ptr->IsActivated())
                    ptr->InactiveUpdate(msTime, diff);
                else ptr->Update(msTime, diff);
            }
        }
    }

    void ResetTime(uint32 msTime)
    {
        // Times have to be reset for our pools so we don't have massive differences from currentms-0
        for(uint32 i = 0; i < mPoolSize; i++)
            mPoolLastUpdateStack[i] = msTime;
    }

    uint8 Add(T *obj)
    {
        uint8 pool = 0xFF;
        mPool.insert(obj);
        if(mPoolStack)
        {
            pool = mPoolAddCounter++;
            if(mPoolAddCounter == mPoolSize)
                mPoolAddCounter = 0;
            mPoolStack[pool].insert(obj);
        }
        return pool;
    }

    void Remove(T *obj, uint8 poolId)
    {
        std::set<T*>::iterator itr;
        if((itr = mPool.find(obj)) != mPool.end())
        {
            // check iterator
            if( poolIterator == itr )
                poolIterator = mPool.erase(itr);
            else mPool.erase(itr);
        }

        if(mPoolStack && poolId != 0xFF)
        {
            if((itr = mPoolStack[poolId].find(obj)) != mPoolStack[poolId].end())
            {
                // check iterator
                if( poolIterator == itr )
                    poolIterator = mPoolStack[poolId].erase(itr);
                else mPoolStack[poolId].erase(itr);
            }
        }
    }

    typename std::set<T*>::iterator begin() { return mPool.begin(); };
    typename std::set<T*>::iterator end() { return mPool.end(); };

private:

    std::set<T*> mPool, *mPoolStack;
    typename std::set<T*>::iterator poolIterator;
    uint32 mPoolCounter, mPoolAddCounter, mPoolSize, *mPoolLastUpdateStack;
};

/// Map instance class for processing different map instances(duh)
class SERVER_DECL MapInstance : public CellHandler <MapCell>, public EventableObject
{
    friend class UpdateObjectThread;
    friend class ObjectUpdaterThread;
    friend class MapCell;
public:

    typedef std::set<WorldObject*> ObjectSet;
    typedef std::set<Player*> PlayerSet;
    typedef std::set<Creature*> CreatureSet;
    typedef std::set<GameObject*> GameObjectSet;
    typedef std::set<uint64> CombatProgressMap;
    typedef Loki::AssocVector<uint32, Creature*> CreatureSqlIdMap;
    typedef Loki::AssocVector<uint32, GameObject* > GameObjectSqlIdMap;

    //This will be done in regular way soon
    Mutex m_objectinsertlock;
    ObjectSet m_objectinsertpool;
    void AddObject(WorldObject*);
    WorldObject* GetObjectClosestToCoords(uint32 entry, float x, float y, float z, float ClosestDist, int32 forcedtype = -1);

////////////////////////////////////////////////////////
// Local (MapInstance) storage/generation of GameObjects
/////////////////////////////////////////////
    typedef Loki::AssocVector<WoWGuid, GameObject* > GameObjectMap;
    GameObjectMap m_gameObjectStorage;
    uint32 m_GOHighGuid;
    GameObject* CreateGameObject(uint32 entry);

    RONIN_INLINE uint32 GenerateGameobjectGuid()
    {
        m_GOHighGuid &= 0x00FFFFFF;
        return ++m_GOHighGuid;
    }

    RONIN_INLINE GameObject* GetGameObject(WoWGuid guid)
    {
        ASSERT(guid.getHigh() == HIGHGUID_TYPE_GAMEOBJECT);
        GameObjectMap::iterator itr = m_gameObjectStorage.find(guid);
        return (itr != m_gameObjectStorage.end()) ? itr->second : NULL;
    }

/////////////////////////////////////////////////////////
// Local (MapInstance) storage/generation of Creatures
/////////////////////////////////////////////
    uint32 m_CreatureHighGuid;
    typedef Loki::AssocVector<WoWGuid, Creature*> CreatureStorageMap;
    CreatureStorageMap m_CreatureStorage;
    Creature* CreateCreature(uint32 entry);

    RONIN_INLINE Creature* GetCreature(WoWGuid guid)
    {
        ASSERT(guid.getHigh() == HIGHGUID_TYPE_UNIT || guid.getHigh() == HIGHGUID_TYPE_VEHICLE);
        CreatureStorageMap::iterator itr = m_CreatureStorage.find(guid);
        return ((itr != m_CreatureStorage.end()) ? itr->second : NULL);
    }

    // Use a creature guid to create our summon.
    Summon* CreateSummon(uint32 entry);
//////////////////////////////////////////////////////////
// Local (MapInstance) storage/generation of DynamicObjects
////////////////////////////////////////////
    uint32 m_DynamicObjectHighGuid;
    typedef Loki::AssocVector<WoWGuid, DynamicObject*> DynamicObjectStorageMap;
    DynamicObjectStorageMap m_DynamicObjectStorage;
    DynamicObject* CreateDynamicObject();

    RONIN_INLINE DynamicObject* GetDynamicObject(WoWGuid guid)
    {
        DynamicObjectStorageMap::iterator itr = m_DynamicObjectStorage.find(guid);
        return ((itr != m_DynamicObjectStorage.end()) ? itr->second : NULL);
    }

//////////////////////////////////////////////////////////
// Local (MapInstance) storage of players for faster lookup
////////////////////////////////
    typedef Loki::AssocVector<WoWGuid, Player*> PlayerStorageMap;
    PlayerStorageMap m_PlayerStorage;
    RONIN_INLINE Player* GetPlayer(WoWGuid guid)
    {
        ASSERT(guid.getHigh() == HIGHGUID_TYPE_PLAYER);
        PlayerStorageMap::iterator itr = m_PlayerStorage.find(guid);
        return (itr != m_PlayerStorage.end()) ? m_PlayerStorage[guid] : NULL;
    }

//////////////////////////////////////////////////////////
// Local (MapInstance) storage of combats in progress
////////////////////////////////
    CombatProgressMap _combatProgress;
    void AddCombatInProgress(uint64 guid)
    {
        _combatProgress.insert(guid);
    }
    void RemoveCombatInProgress(uint64 guid)
    {
        _combatProgress.erase(guid);
    }
    RONIN_INLINE bool IsCombatInProgress()
    {
        //if all players are out, list should be empty.
        if(!HasPlayers())
            _combatProgress.clear();
        return (_combatProgress.size() > 0);
    }

//////////////////////////////////////////////////////////
// Lookup Wrappers
///////////////////////////////////
    Unit* GetUnit(WoWGuid guid);
    WorldObject* _GetObject(WoWGuid guid);

    MapInstance(Map *map, uint32 mapid, uint32 instanceid);
    ~MapInstance();

    void Preload();
    void Init();
    void Destruct();

    void EventPushObjectToSelf(WorldObject *obj);

    void PushObject(WorldObject* obj);
    void RemoveObject(WorldObject* obj);

    void ChangeObjectLocation(WorldObject* obj); // update inrange lists
    void ChangeFarsightLocation(Player* plr, Unit* farsight, bool apply);
    void ChangeFarsightLocation(Player* plr, float X, float Y, bool apply);
    bool IsInRange(float fRange, WorldObject* obj, WorldObject* currentobj);

    //! Mark object as updated
    void ObjectUpdated(WorldObject* obj);
    void UpdateCellActivity(uint32 x, uint32 y, int radius);

    // Call down to base map for cell activity
    RONIN_INLINE void CellLoaded(uint32 x, uint32 y) { _map->CellLoaded(x,y); }
    RONIN_INLINE void CellUnloaded(uint32 x,uint32 y) { _map->CellUnloaded(x,y); }

    // Terrain Functions
    void GetWaterData(float x, float y, float z, float &outHeight, uint16 &outType);
    float GetLandHeight(float x, float y);
    uint8 GetWalkableState(float x, float y);
    uint16 GetAreaID(float x, float y, float z = 0.0f);
    float GetWaterHeight(float x, float y, float z)
    {
        uint16 waterType = 0;
        float res = NO_WATER_HEIGHT;
        GetWaterData(x, y, z, res, waterType);
        return res;
    }

    RONIN_INLINE uint32 GetMapId() { return _mapId; }
    void AddForcedCell(MapCell * c, uint32 range = 1);
    void RemoveForcedCell(MapCell * c, uint32 range = 1);

    void PushToProcessed(Player* plr);

    RONIN_INLINE bool HasPlayers() { return (m_PlayerStorage.size() > 0); }
    void TeleportPlayers();

    RONIN_INLINE virtual bool IsInstance() { return false; }
    RONIN_INLINE uint32 GetInstanceID() { return m_instanceID; }
    RONIN_INLINE MapEntry *GetdbcMap() { return pdbcMap; }
    bool CanUseCollision(WorldObject* obj);

    virtual int32 event_GetMapID() { return _mapId; }

    void UpdateAllCells(bool apply, uint32 areamask = 0);
    RONIN_INLINE size_t GetPlayerCount() { return m_PlayerStorage.size(); }

    void _ProcessInputQueue();
    void _PerformPlayerUpdates(uint32 msTime, uint32 uiDiff);
    void _PerformCreatureUpdates(uint32 msTime, uint32 uiDiff);
    void _PerformObjectUpdates(uint32 msTime, uint32 uiDiff);
    void _PerformDynamicObjectUpdates(uint32 msTime, uint32 uiDiff);
    void _PerformSessionUpdates();
    void _PerformPendingUpdates();

    void EventCorpseDespawn(uint64 guid);

    time_t InactiveMoveTime;
    uint32 iInstanceMode;

    RONIN_INLINE void AddSpawn(uint32 x, uint32 y, CreatureSpawn * sp)
    {
        GetBaseMap()->GetSpawnsListAndCreate(x, y)->CreatureSpawns.push_back(sp);
    }

    RONIN_INLINE void AddGoSpawn(uint32 x, uint32 y, GOSpawn * gs)
    {
        GetBaseMap()->GetSpawnsListAndCreate(x, y)->GOSpawns.push_back(gs);
    }

    void UnloadCell(uint32 x,uint32 y);
    void EventRespawnCreature(Creature* ctr, MapCell * c);
    void SendMessageToCellPlayers(WorldObject* obj, WorldPacket * packet, uint32 cell_radius = 2);
    void SendChatMessageToCellPlayers(WorldObject* obj, WorldPacket * packet, uint32 cell_radius, uint32 langpos, uint32 guidPos, int32 lang, WorldSession * originator);
    void BeginInstanceExpireCountdown();
    void HookOnAreaTrigger(Player* plr, uint32 id);

protected:
    //! Collect and send updates to clients
    void _UpdateObjects();

    //! Objects that exist on map
    uint32 _mapId;

    // In this zone, we always show these objects
    Loki::AssocVector<WorldObject*, uint32> m_rangelessObjects;
    Loki::AssocVector<uint32, std::vector<WorldObject*>> m_zoneRangelessObjects;

    bool _CellActive(uint32 x, uint32 y);
    void UpdateInRangeSet(WorldObject* obj, Player* plObj, MapCell* cell);

    void ObjectMovingCells(WorldObject *obj, MapCell *oldCell, MapCell *newCell);
    void UpdateObjectVisibility(Player *plObj, WorldObject *curObj);

public:
    void UpdateInrangeSetOnCells(WorldObject* obj, uint32 startX, uint32 endX, uint32 startY, uint32 endY);

    // Map preloading to push back updating inrange objects
    bool m_mapPreloading;

    bool IsRaid() { return pdbcMap ? pdbcMap->IsRaid() : false; }
    bool IsContinent() { return pdbcMap ? pdbcMap->IsContinent() : true; }
protected:
    /* Map Information */
    MapEntry* pdbcMap;
    uint32 m_instanceID;

    /* Update System */
    Mutex m_updateMutex;
    ObjectSet _updates;
    PlayerSet _processQueue;

    /* Sessions */
    SessionSet MapSessions;

public:
    Mutex m_poolLock;
    StoragePool<Creature> mCreaturePool;
    StoragePool<GameObject> mGameObjectPool;
    StoragePool<DynamicObject> mDynamicObjectPool;

    CBattleground* m_battleground;
    std::vector<Corpse* > m_corpses;
    CreatureSqlIdMap _sqlids_creatures;
    GameObjectSqlIdMap _sqlids_gameobjects;

    Creature* GetSqlIdCreature(uint32 sqlid);
    GameObject* GetSqlIdGameObject(uint32 sqlid);

    // world state manager stuff
    WorldStateManager* m_stateManager;

    // bytebuffer caching
    ByteBuffer m_createBuffer, m_updateBuffer;

public:
    void ClearCorpse(Corpse* remove) { std::vector<Corpse* >::iterator itr; if((itr = std::find(m_corpses.begin(), m_corpses.end(), remove)) != m_corpses.end()) m_corpses.erase(itr); };

    // get!
    RONIN_INLINE WorldStateManager& GetStateManager() { return *m_stateManager; }

    // send packet functions for state manager
    void SendPacketToPlayers(int32 iZoneMask, int32 iFactionMask, WorldPacket *pData);
    void SendPvPCaptureMessage(int32 iZoneMask, uint32 ZoneId, const char * Format, ...);

    // auras :< (world pvp)
    void RemoveAuraFromPlayers(int32 iFactionMask, uint32 uAuraId);
    void RemovePositiveAuraFromPlayers(int32 iFactionMask, uint32 uAuraId);
    void CastSpellOnPlayers(int32 iFactionMask, uint32 uSpellId);

public:

    // stored iterators for safe checking
    PlayerStorageMap::iterator __player_iterator;
};
