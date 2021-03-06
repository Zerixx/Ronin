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

initialiseSingleton(MailSystem);

bool MailMessage::LoadFromDB(Field * fields)
{
    uint32 i;
    char * str;
    char * p;
    uint64 itemguid;

    // Create message struct
    i = 0;
    items.clear();
    message_id = fields[i++].GetUInt32();
    message_type = fields[i++].GetUInt32();
    player_guid = fields[i++].GetUInt32();
    sender_guid = fields[i++].GetUInt32();
    subject = fields[i++].GetString();
    body = fields[i++].GetString();
    money = fields[i++].GetUInt32();
    str = (char*)fields[i++].GetString();
    p = strchr(str, ',');
    if( p == NULL )
    {
        itemguid = atoi(str);
        if( itemguid != 0 )
            items.push_back( itemguid );
    }
    else
    {
        while( p )
        {
            *p = 0;
            p++;
            itemguid = atol( str );
            if( itemguid != 0 )
                items.push_back( itemguid );
            str = p;
            p = strchr( str, ',' );
        }
    }

    cod = fields[i++].GetUInt32();
    stationary = fields[i++].GetUInt32();
    expire_time = fields[i++].GetUInt32();
    delivery_time = fields[i++].GetUInt32();
    copy_made = fields[i++].GetBool();
    read_flag = fields[i++].GetBool();
    deleted_flag = fields[i++].GetBool();
    returned_flag = fields[i++].GetBool();

    return (!deleted_flag || copy_made);
}

void MailMessage::SaveToDB()
{
    std::stringstream ss;
    std::vector< uint64 >::iterator itr;
    ss << "REPLACE INTO mailbox VALUES("
        << message_id << ","
        << message_type << ","
        << player_guid << ","
        << sender_guid << ",\""
        << CharacterDatabase.EscapeString(subject) << "\",\""
        << CharacterDatabase.EscapeString(body) << "\","
        << money << ",'";

    for( itr = items.begin( ); itr != items.end( ); itr++ )
        ss << (*itr) << ",";

    ss << "',"
        << cod << ","
        << stationary << ","
        << expire_time << ","
        << delivery_time << ","
        << copy_made << ","
        << read_flag << ","
        << deleted_flag << ","
        << returned_flag << ")";
    if(message_id == 0)
    {
        AsyncQuery * q = new AsyncQuery( new SQLClassCallbackP0<MailMessage>(this, &MailMessage::SaveToDBCallBack) );
        q->AddQuery(ss.str().c_str());
        q->AddQuery("SELECT LAST_INSERT_ID()");
        CharacterDatabase.QueueAsyncQuery(q);
    } else
    {
        CharacterDatabase.WaitExecute(ss.str().c_str());
    }
}

void MailMessage::SaveToDBCallBack(QueryResultVector & results)
{
    QueryResult *result = results[1].result;
    if(result)
    {
        Field *fields = result->Fetch();
        message_id = fields[0].GetUInt32();
    }
}

bool MailMessage::Expired()
{
    return (expire_time && expire_time < (uint32)UNIXTIME);
}
void Mailbox::AddMessage(MailMessage* Message)
{
    Messages[Message->message_id] = *Message;
}

void Mailbox::DeleteMessage(MailMessage* Message)
{
    if(Message->copy_made)
    {
        // we have the message as a copy on the item. we can't delete it or this item will no longer function.
        // deleted_flag prevents it from being shown in the mail list.
        Message->deleted_flag = true;
        Message->SaveToDB();
    }
    else
    {
        // delete the message, there are no other references to it.
        CharacterDatabase.WaitExecute("DELETE FROM mailbox WHERE message_id = %u", Message->message_id);
        Messages.erase(Message->message_id);
    }
}

void Mailbox::Load(QueryResult * result)
{
    if(!result)
        return;

    MailMessage msg;
    do
    {
        if (msg.LoadFromDB(result->Fetch()))
        {
            AddMessage(&msg);// Add to the mailbox
        }
    } while(result->NextRow());
}

void Mailbox::MailboxListingPacket(WorldPacket *packet)
{
    packet->Initialize(SMSG_MAIL_LIST_RESULT, 500);
    MessageMap::iterator itr;
    uint32 realcount = 0;
    uint32 count = 0;
    *packet << uint32(0);  // realcount - this can be used to tell the client we have more mail than that fits into this packet
    *packet << uint8(0);   // size placeholder

    for(itr = Messages.begin(); itr != Messages.end(); itr++)
    {
        if(uint8 msgCount = AddMessageToListingPacket(*packet, &itr->second))
        {
            if(msgCount == 2) ++count;
            ++realcount;
        }
    }

    packet->put<uint32>(0, realcount);
    packet->put<uint8>(4, count);

    // do cleanup on request mail
    //CleanupExpiredMessages();
}

uint8 Mailbox::AddMessageToListingPacket(WorldPacket& data,MailMessage *msg)
{
    // add stuff
    if(msg->deleted_flag || msg->Expired() || (uint32)UNIXTIME < msg->delivery_time)
        return 0;

    uint8 guidsize = msg->message_type ? 4 : 8;
    size_t msize = 2 + 4 + 1 + guidsize + 7 * 4 + ( msg->subject.size() + 1 ) + ( msg->body.size() + 1 ) + 1 + ( msg->items.size() * ( 1 + 2*4 + 7 * ( 3*4 ) + 6*4 + 1 ) );
    if(data.wpos()+msize >= 0x7FFF)
        return 1;

    data << uint16(msize);
    data << msg->message_id;
    data << uint8(msg->message_type);
    if(msg->message_type)
        data << uint32(msg->sender_guid);
    else data << msg->sender_guid;

    data << msg->cod;
    data << uint32(0);
    data << msg->stationary;
    data << msg->money; // money
    data << uint32(0x10);
    data << float((msg->expire_time - uint32(UNIXTIME)) / 86400.0f);
    data << uint32( 0 );    // mail template
    data << msg->subject;
    data << msg->body; // subjectbody
    size_t pos = data.wpos();
    data << uint8(0);       // item count

    if( !msg->items.empty( ) )
    {
        uint8 count = 0;
        std::vector<uint64>::iterator itr;
        for( itr = msg->items.begin( ); itr != msg->items.end( ); itr++ )
        {

        }
        data.put< uint8 >( pos, count );
    }

    return 2;
}

void Mailbox::MailboxTimePacket(WorldPacket *packet)
{
    packet->Initialize(MSG_QUERY_NEXT_MAIL_TIME, 100);
    if(Messages.size())
    {
        uint32 count = 0;
        *packet << uint32(0) << uint32(0);
        for(MessageMap::iterator iter = Messages.begin(); iter != Messages.end(); iter++ )
            if(AddMessageToTimePacket(* packet, &iter->second))
                if(++count == 2)
                    break;

        packet->put<uint32>(0, count);
    } else *packet << uint32(0xC7A8C000) << uint32(0);
}

bool Mailbox::AddMessageToTimePacket(WorldPacket& data,MailMessage *msg)
{
    if ( msg->deleted_flag == 1 || msg->read_flag == 1  || msg->Expired() || (uint32)UNIXTIME < msg->delivery_time )
        return false;

    data << uint64(msg->sender_guid);
    data << uint32(0);
    data << uint32(0);// money or smth?
    data << uint32(msg->stationary);
    data << uint32(0xC6000000); // float unk, time or something
    return true;
}

void Mailbox::OnMessageCopyDeleted(uint32 msg_id)
{
    MailMessage * msg = GetMessage(msg_id);
    if(msg == 0) return;

    msg->copy_made = false;

    if(msg->deleted_flag)   // we've deleted from inbox
        DeleteMessage(msg);   // wipe the message
    else
        msg->SaveToDB();
}
void MailSystem::StartMailSystem()
{

}

void MailSystem::DeliverMessage(MailMessage* message)
{
    message->SaveToDB();

    Player* plr = objmgr.GetPlayer((uint32)message->player_guid);
    if(plr != NULL)
    {
        plr->m_mailBox->AddMessage(message);
        if((uint32)UNIXTIME >= message->delivery_time)
        {
            uint32 v = 0;
            plr->GetSession()->OutPacket(SMSG_RECEIVED_MAIL, 4, &v);
        }
    }
}

void MailSystem::ReturnToSender(MailMessage* message)
{
    MailMessage msg = *message;

    // re-assign the owner/sender
    msg.player_guid = message->sender_guid;
    msg.sender_guid = message->player_guid;

    // remove the old message
    Player* plr = objmgr.GetPlayer((uint32)message->player_guid);
    if(plr == NULL)
    {
        if(message->copy_made)
        {
            message->deleted_flag = true;
            message->SaveToDB();
        }
        else
            CharacterDatabase.WaitExecute("DELETE FROM mailbox WHERE message_id = %u", message->message_id);
    }
    else plr->m_mailBox->DeleteMessage(message);

    // return mail
    msg.message_id = 0;
    msg.read_flag = false;
    msg.deleted_flag = false;
    msg.copy_made = false;
    msg.returned_flag = true;
    msg.cod = 0;
    msg.delivery_time = msg.items.empty() ? (uint32)UNIXTIME : (uint32)UNIXTIME + 3600;
    // returned mail's don't expire
    msg.expire_time = 0;
    // add to the senders mailbox
    sMailSystem.DeliverMessage(&msg);
}

void MailSystem::DeliverMessage(uint32 type, uint64 sender, uint64 receiver, std::string subject, std::string body,
                                      uint32 money, uint32 cod, WoWGuid item_guid, uint32 stationary, bool returned)
{
    // This is for sending automated messages, for example from an auction house.
    MailMessage msg;
    msg.message_id = 0;
    msg.message_type = type;
    msg.sender_guid = sender;
    msg.player_guid = receiver;
    msg.subject = subject;
    msg.body = body;
    msg.money = money;
    msg.cod = cod;
    if( !item_guid.empty() )
        msg.items.push_back(item_guid);

    msg.stationary = stationary;
    msg.delivery_time = (uint32)UNIXTIME;
    msg.expire_time = 0;
    msg.read_flag = false;
    msg.copy_made = false;
    msg.deleted_flag = false;
    msg.returned_flag = returned;

    // Send the message.
    DeliverMessage(&msg);
}

void MailSystem::UpdateMessages(uint32 diff)
{
    if(update_timer > diff)
    {
        update_timer -= diff;
        return;
    }
    else update_timer = 1200;

    QueryResult *result = CharacterDatabase.Query("SELECT * FROM mailbox WHERE expiry_time > 0 and expiry_time <= %u and deleted_flag = 0",(uint32)UNIXTIME);
    if(!result)
        return;

    MailMessage msg;
    do
    {
        if (msg.LoadFromDB(result->Fetch()))
        {
            if (/*msg.Expired() &&*/ msg.items.size() == 0 && msg.money == 0)
            {
                if(msg.copy_made)
                {
                    msg.deleted_flag = true;
                    msg.SaveToDB();
                } else
                {
                    CharacterDatabase.WaitExecute("DELETE FROM mailbox WHERE message_id = %u", msg.message_id);
                }
            }
            else
                ReturnToSender(&msg);
        }
    } while(result->NextRow());
    delete result;
}

void WorldSession::HandleSendMail(WorldPacket & recv_data )
{

}

void WorldSession::HandleMarkAsRead(WorldPacket & recv_data )
{
    uint64 mailbox;
    uint32 message_id;
    recv_data >> mailbox >> message_id;

    MailMessage * message = _player->m_mailBox->GetMessage(message_id);
    if(message == nullptr || message->Expired()) return;

    message->read_flag = true;
    if(!message->returned_flag)
    {
        // mail now has a 3 day expiry time
        if(!sMailSystem.MailOption(MAIL_FLAG_NO_EXPIRY))
            message->expire_time = (uint32)UNIXTIME + (TIME_DAY * 3);
    }
    message->SaveToDB();
}

void WorldSession::HandleMailDelete(WorldPacket & recv_data )
{
    uint64 mailbox;
    uint32 message_id;
    uint32 mailtemplet;
    recv_data >> mailbox >> message_id >> mailtemplet;

    WorldPacket data(SMSG_SEND_MAIL_RESULT, 12);
    data << message_id << uint32(MAIL_RES_DELETED);

    MailMessage * message = _player->m_mailBox->GetMessage(message_id);
    if(message == nullptr || message->Expired())
    {
        data << uint32(MAIL_ERR_INTERNAL_ERROR);
        SendPacket(&data);
        return;
    }

    _player->m_mailBox->DeleteMessage(message);

    data << uint32(MAIL_OK);
    SendPacket(&data);
}

void WorldSession::HandleTakeItem(WorldPacket & recv_data )
{

}

void WorldSession::HandleTakeMoney(WorldPacket & recv_data )
{
    uint64 mailbox;
    uint32 message_id;
    recv_data >> mailbox >> message_id;

    WorldPacket data(SMSG_SEND_MAIL_RESULT, 12);
    data << message_id << uint32(MAIL_RES_MONEY_TAKEN);

    MailMessage * message = _player->m_mailBox->GetMessage(message_id);
    if(message == 0 || message->Expired() || !message->money)
    {
        data << uint32(MAIL_ERR_INTERNAL_ERROR);
        SendPacket(&data);

        return;
    }

    // add the money to the player
    if((_player->GetUInt32Value(PLAYER_FIELD_COINAGE) + message->money) >= PLAYER_MAX_GOLD )
    {
        data << uint32(MAIL_ERR_INTERNAL_ERROR);
        SendPacket(&data);
        return;
    } else _player->ModUnsigned32Value(PLAYER_FIELD_COINAGE, message->money);

    // force save
    _player->SaveToDB(false);

    // message no longer has any money
    message->money = 0;

    if ((message->items.size() == 0) && (message->money == 0))
    {
        // mail now has a 3 day expiry time
        if(!sMailSystem.MailOption(MAIL_FLAG_NO_EXPIRY))
            message->expire_time = (uint32)UNIXTIME + (TIME_DAY * 3);
    }
    // update in sql!
    message->SaveToDB();

    // send result
    data << uint32(MAIL_OK);
    SendPacket(&data);
}

void WorldSession::HandleReturnToSender(WorldPacket & recv_data )
{
    uint64 mailbox;
    uint32 message_id;
    uint64 returntoguid;
    recv_data >> mailbox >> message_id >> returntoguid;

    WorldPacket data(SMSG_SEND_MAIL_RESULT, 12);
    data << message_id << uint32(MAIL_RES_RETURNED_TO_SENDER);

    MailMessage * msg = _player->m_mailBox->GetMessage(message_id);
    if(msg == 0 || msg->Expired())
    {
        data << uint32(MAIL_ERR_INTERNAL_ERROR);
        SendPacket(&data);

        return;
    }
    if(msg->returned_flag)
    {
        data << uint32(MAIL_ERR_INTERNAL_ERROR);
        SendPacket(&data);

        return;
    }

    sMailSystem.ReturnToSender(msg);

    // finish the packet
    data << uint32(MAIL_OK);
    SendPacket(&data);
}

void WorldSession::HandleMailCreateTextItem(WorldPacket & recv_data )
{

}

void WorldSession::HandleItemTextQuery(WorldPacket & recv_data)
{

}

void WorldSession::HandleMailTime(WorldPacket & recv_data)
{
    WorldPacket data;
    _player->m_mailBox->MailboxTimePacket(&data);
    SendPacket(&data);
}

void WorldSession::SendMailError(uint32 error, uint32 extra)
{
    WorldPacket data(SMSG_SEND_MAIL_RESULT, 16);
    data << uint32(0);
    data << uint32(MAIL_RES_MAIL_SENT);
    data << error;
    data << extra;
    SendPacket(&data);
}

void WorldSession::HandleGetMail(WorldPacket & recv_data )
{
    uint64 mailbox;
    recv_data >> mailbox;

    WorldPacket data;
    _player->m_mailBox->MailboxListingPacket(&data);
    SendPacket(&data);
}
