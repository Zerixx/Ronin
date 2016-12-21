/*
 * Copyright (C) 2005-2011 MaNGOS <http://getmangos.com/>
 * Copyright (C) 2008-2014 TrinityCore <http://www.trinitycore.org/>
 * Copyright (C) 2014-2017 Sandshroud <https://github.com/Sandshroud>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

namespace VMAP
{
    /**
    This Class is used to convert raw vector data into balanced BSP-Trees.
    To start the conversion call convertWorld().
    */
    //===============================================

    class ModelPosition
    {
        private:
            G3D::Matrix3 iRotation;
        public:
            ModelPosition(): iScale(0.0f) { }
            G3D::Vector3 iPos;
            G3D::Vector3 iDir;
            float iScale;
            void init()
            {
                iRotation = G3D::Matrix3::fromEulerAnglesZYX(G3D::pi()*iDir.y/180.f, G3D::pi()*iDir.x/180.f, G3D::pi()*iDir.z/180.f);
            }
            G3D::Vector3 transform(const G3D::Vector3& pIn) const;
            void moveToBasePos(const G3D::Vector3& pBasePos) { iPos -= pBasePos; }
    };

    //===============================================

    struct GroupModel_Raw
    {
        G3D::uint32 mogpflags;
        G3D::uint32 GroupWMOID;

        G3D::AABox bounds;
        G3D::uint32 liquidflags;
        std::vector<MeshTriangle> triangles;
        std::vector<G3D::Vector3> vertexArray;
        class WmoLiquid* liquid;

        GroupModel_Raw() : mogpflags(0), GroupWMOID(0), liquidflags(0),
            liquid(NULL) { }
        ~GroupModel_Raw();

        bool Read(FILE* f);
    };

    struct WorldModel_Raw
    {
        G3D::uint32 RootWMOID;
        std::vector<GroupModel_Raw> groupsArray;

        bool Read(const char * path);
    };

    class TileAssembler
    {
        private:
            std::string iTileDestDir;
            std::string iObjDestDir;
            std::string iSrcDir;

            bool (*iFilterMethod)(char *pName);
            G3D::Table<std::string, unsigned int > iUniqueNameIds;
            unsigned int iCurrentUniqueNameId;
            std::set<std::string> spawnedModelFiles;

        public:
            TileAssembler(const std::string& pSrcDirName, const std::string& pTileDestDirName, const std::string &pObjectDestDirName);
            virtual ~TileAssembler();

            bool convertWorld3(FILE *mapFile, uint32 mapId, ModelSpawnMap* spawnMap, TiledModelSpawnMap* tileSpawnMap);
            bool convertWorldObjects();

            bool calculateTransformedBound(ModelSpawn &spawn);
            void exportGameobjectModels();

            bool convertRawFile(const std::string& pModelFilename);
            void setModelNameFilterMethod(bool (*pFilterMethod)(char *pName)) { iFilterMethod = pFilterMethod; }
            std::string getDirEntryNameFromModName(unsigned int pMapId, const std::string& pModPosName);
    };

}                                                           // VMAP
