/***
 * Demonstrike Core
 */

#include "StdAfx.h"

void WorldSession::HandleDuelAccepted(WorldPacket & recv_data)
{
    CHECK_INWORLD_RETURN();

    if( _player->DuelingWith == NULL )
        return;

    if( _player->m_duelState != DUEL_STATE_FINISHED )
        return;

    if( _player->m_duelCountdownTimer > 0 )
        return;

    _player->m_duelStatus = DUEL_STATUS_INBOUNDS;
    _player->DuelingWith->m_duelStatus = DUEL_STATUS_INBOUNDS;

    _player->m_duelState = DUEL_STATE_STARTED;
    _player->DuelingWith->m_duelState = DUEL_STATE_STARTED;

    WorldPacket data( SMSG_DUEL_COUNTDOWN, 4 );
    data << uint32( 3000 );

    SendPacket( &data );
    _player->DuelingWith->m_session->SendPacket( &data );

    _player->m_duelCountdownTimer = 3000;
}

void WorldSession::HandleDuelCancelled(WorldPacket & recv_data)
{
    if( _player->DuelingWith ==  NULL )
        return;

    if( _player->m_duelState == DUEL_STATE_STARTED )
    {
        _player->DuelingWith->EndDuel( DUEL_WINNER_KNOCKOUT );
        return;
    }

    WorldPacket data( SMSG_DUEL_COMPLETE, 1 );
    data << uint8( 1 );

    SendPacket( &data );
    _player->DuelingWith->m_session->SendPacket( &data );

    if( GameObject* arbiter = _player->GetMapInstance() ? _player->GetMapInstance()->GetGameObject( _player->GetUInt64Value( PLAYER_DUEL_ARBITER ) ) : NULL )
    {
        arbiter->RemoveFromWorld();
        arbiter->Destruct();
        arbiter = NULL;
    }

    _player->DuelingWith->SetUInt64Value( PLAYER_DUEL_ARBITER, 0 );
    _player->SetUInt64Value( PLAYER_DUEL_ARBITER, 0 );

    _player->DuelingWith->SetUInt32Value( PLAYER_DUEL_TEAM, 0 );
    _player->SetUInt32Value( PLAYER_DUEL_TEAM, 0);

    _player->DuelingWith->m_duelState = DUEL_STATE_FINISHED;
    _player->m_duelState = DUEL_STATE_FINISHED;

    _player->DuelingWith->m_duelCountdownTimer = 0;
    _player->m_duelCountdownTimer = 0;

    _player->DuelingWith->DuelingWith = NULL;
    _player->DuelingWith = NULL;

}
