/*
 * Sandshroud Project Ronin
 * Copyright (C) 2005-2008 Ascent Team <http://www.ascentemu.com/>
 * Copyright (C) 2008-2009 AspireDev <http://www.aspiredev.org/>
 * Copyright (C) 2009-2017 Sandshroud <https://github.com/Sandshroud>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "StdAfx.h"

initialiseSingleton( AddonMgr );

//#define DEBUG_PRINT_ADDON_PACKET          // Prints out Received addon packet when char logging in

static uint8 publicAddonKey[256] =
{
    0xC3, 0x5B, 0x50, 0x84, 0xB9, 0x3E, 0x32, 0x42, 0x8C, 0xD0, 0xC7, 0x48, 0xFA, 0x0E, 0x5D, 0x54,
    0x5A, 0xA3, 0x0E, 0x14, 0xBA, 0x9E, 0x0D, 0xB9, 0x5D, 0x8B, 0xEE, 0xB6, 0x84, 0x93, 0x45, 0x75,
    0xFF, 0x31, 0xFE, 0x2F, 0x64, 0x3F, 0x3D, 0x6D, 0x07, 0xD9, 0x44, 0x9B, 0x40, 0x85, 0x59, 0x34,
    0x4E, 0x10, 0xE1, 0xE7, 0x43, 0x69, 0xEF, 0x7C, 0x16, 0xFC, 0xB4, 0xED, 0x1B, 0x95, 0x28, 0xA8,
    0x23, 0x76, 0x51, 0x31, 0x57, 0x30, 0x2B, 0x79, 0x08, 0x50, 0x10, 0x1C, 0x4A, 0x1A, 0x2C, 0xC8,
    0x8B, 0x8F, 0x05, 0x2D, 0x22, 0x3D, 0xDB, 0x5A, 0x24, 0x7A, 0x0F, 0x13, 0x50, 0x37, 0x8F, 0x5A,
    0xCC, 0x9E, 0x04, 0x44, 0x0E, 0x87, 0x01, 0xD4, 0xA3, 0x15, 0x94, 0x16, 0x34, 0xC6, 0xC2, 0xC3,
    0xFB, 0x49, 0xFE, 0xE1, 0xF9, 0xDA, 0x8C, 0x50, 0x3C, 0xBE, 0x2C, 0xBB, 0x57, 0xED, 0x46, 0xB9,
    0xAD, 0x8B, 0xC6, 0xDF, 0x0E, 0xD6, 0x0F, 0xBE, 0x80, 0xB3, 0x8B, 0x1E, 0x77, 0xCF, 0xAD, 0x22,
    0xCF, 0xB7, 0x4B, 0xCF, 0xFB, 0xF0, 0x6B, 0x11, 0x45, 0x2D, 0x7A, 0x81, 0x18, 0xF2, 0x92, 0x7E,
    0x98, 0x56, 0x5D, 0x5E, 0x69, 0x72, 0x0A, 0x0D, 0x03, 0x0A, 0x85, 0xA2, 0x85, 0x9C, 0xCB, 0xFB,
    0x56, 0x6E, 0x8F, 0x44, 0xBB, 0x8F, 0x02, 0x22, 0x68, 0x63, 0x97, 0xBC, 0x85, 0xBA, 0xA8, 0xF7,
    0xB5, 0x40, 0x68, 0x3C, 0x77, 0x86, 0x6F, 0x4B, 0xD7, 0x88, 0xCA, 0x8A, 0xD7, 0xCE, 0x36, 0xF0,
    0x45, 0x6E, 0xD5, 0x64, 0x79, 0x0F, 0x17, 0xFC, 0x64, 0xDD, 0x10, 0x6F, 0xF3, 0xF5, 0xE0, 0xA6,
    0xC3, 0xFB, 0x1B, 0x8C, 0x29, 0xEF, 0x8E, 0xE5, 0x34, 0xCB, 0xD1, 0x2A, 0xCE, 0x79, 0xC3, 0x9A,
    0x0D, 0x36, 0xEA, 0x01, 0xE0, 0xAA, 0x91, 0x20, 0x54, 0xF0, 0x72, 0xD8, 0x1E, 0xC7, 0x89, 0xD2
};

AddonMgr::AddonMgr()
{

}

AddonMgr::~AddonMgr()
{
    while(KnownAddons.size())
    {
        delete KnownAddons.begin()->second;
        KnownAddons.erase(KnownAddons.begin());
    }
}

bool AddonMgr::IsAddonBanned(uint32 crc, std::string name)
{
    return false;   // bleh needs work
}

bool AddonMgr::IsAddonBanned(std::string name, uint32 crc)
{
    std::map<std::string,AddonEntry*>::iterator i = KnownAddons.find(name);
    if(i != KnownAddons.end())
    {
        if(i->second->banned)
        {
            sLog.outDebug("Addon %s is banned.", name.c_str());
            return true;
        }
    }
    else
    {
        // New addon. It'll be saved to db at server shutdown.
        AddonEntry *ent = new AddonEntry;
        ent->name = name;
        ent->crc = crc;
        ent->banned = false;    // by default.. we can change this I guess..
        ent->isNew = true;
        ent->showinlist = (crc == 0x4C1C776D ? false : true);

        sLog.outDebug("Discovered new addon %s sent by client.", name.c_str());
        KnownAddons.insert(std::make_pair(ent->name, ent));
    }

    return false;
}

bool AddonMgr::ShouldShowInList(std::string name, uint32 crc)
{
    std::map<std::string,AddonEntry*>::iterator i = KnownAddons.find(name);
    if(i != KnownAddons.end())
    {
        if(i->second->showinlist)
            return true;
        return false;
    }
    else
    {
        // New addon. It'll be saved to db at server shutdown.
        AddonEntry *ent = new AddonEntry;
        ent->name = name;
        ent->crc = crc;
        ent->banned = false;    // by default.. we can change this I guess..
        ent->isNew = true;
        ent->showinlist = (crc == 0x4C1C776D ? false : true);

        sLog.Debug("AddonMgr","Discovered new addon %s sent by client.", name.c_str());
        KnownAddons.insert(std::make_pair(ent->name, ent));
    }

    return true;
}

void AddonMgr::SendAddonInfoPacket(WorldPacket *source, WorldSession *m_session)
{
    uint32 realsize;
    try { *source >> realsize; }
    catch (ByteBufferException &)
    {
        sLog.Debug("AddonMgr","Warning, Incomplete auth session sent.");
        return;
    }
    if(realsize == 0)
    {
        // we shouldnt get here.. but just in case this will stop any crash here.
        sLog.Debug("AddonMgr","Warning, Incomplete auth session sent.");
        return;
    }
    uint32 pos = (*source).rpos();

    uLongf rsize = realsize;
    ByteBuffer unpacked(realsize);
    unpacked.resize(realsize);
    int32 result = uncompress((uint8*)unpacked.contents(), &rsize, (uint8*)(*source).contents()+pos, (uLong)((*source).size()-pos));
    if(result != Z_OK)
    {
        sLog.Debug("AddonMgr","Decompression of addon section of CMSG_AUTH_SESSION failed.");
        return;
    }

    sLog.Debug("AddonMgr","Decompression of addon section of CMSG_AUTH_SESSION succeeded.");

    uint32 addonCount;
    try { unpacked >> addonCount; }
    catch (ByteBufferException &)
    {
        sLog.Debug("AddonMgr","Warning, Incomplete auth session sent.");
        return;
    }

    std::string name;
    uint32 crc, unknown;
    uint8 Enable; // based on the parsed files from retool
    WorldPacket returnpacket(SMSG_ADDON_INFO, 200);
    for (uint32 i = 0; i < addonCount; i++)
    {
        try
        {
            unpacked >> name;
            unpacked >> Enable;
            unpacked >> crc;
            unpacked >> unknown;
        }
        catch (ByteBufferException &)
        {
            sLog.Debug("AddonMgr","Warning, Incomplete auth session sent.");
            return;
        }

        returnpacket << uint8(Enable ? 0x02 : 0x01);
        returnpacket << uint8(Enable ? 0x01 : 0x00);
        if(Enable)
        {
            uint8 unk = (crc != 0x4c1c776d);           // If addon is Standard addon CRC
            returnpacket << uint8(unk);
            if (unk) returnpacket.append(publicAddonKey, 256);
            returnpacket << uint32(0);
        }

        uint8 unk3 = (Enable ? 0 : 1);
        returnpacket << uint8(unk3);
        if (unk3)
            returnpacket << uint8(0);
    }

    uint32 timeStamp;
    unpacked >> timeStamp;
    returnpacket << uint32(0); // Banned count
    m_session->SendPacket(&returnpacket);
}

void AddonMgr::LoadFromDB()
{
    QueryResult *result = WorldDatabase.Query("SELECT * FROM clientaddons");
    if(!result)
    {
        sLog.outString("Query failed: SELECT * FROM clientaddons");
        return;
    }

    do
    {
        Field *field = result->Fetch();
        std::string name = field[1].GetString();
        if(KnownAddons.find(name) != KnownAddons.end())
            continue;

        AddonEntry *ent = new AddonEntry;
        ent->name = name;
        ent->crc = field[2].GetUInt64();
        ent->banned = (field[3].GetUInt32()>0? true:false);
        ent->isNew = false;
        if(result->GetFieldCount() == 5)                // To avoid crashes for stilly nubs who don't update table :P
            ent->showinlist = (field[4].GetUInt32()>0 ? true : false);

        KnownAddons.insert(std::make_pair(ent->name, ent));
    } while(result->NextRow());

    delete result;
}

void AddonMgr::SaveToDB()
{
    sLog.Debug("AddonMgr","Saving any new addons discovered in this session to database.");
    for(std::map<std::string, AddonEntry*>::iterator itr = KnownAddons.begin();itr!=KnownAddons.end();itr++)
    {
        if(itr->second->isNew)
        {
            sLog.outDebug("Saving new addon %s", itr->second->name.c_str());
            std::stringstream ss;
            ss << "INSERT INTO clientaddons (name, crc, banned, showinlist) VALUES(\""
                << WorldDatabase.EscapeString(itr->second->name) << "\",\""
                << itr->second->crc << "\",\""
                << itr->second->banned << "\",\""
                << itr->second->showinlist << "\");";

            WorldDatabase.Execute(ss.str().c_str());
        }
    }
}
