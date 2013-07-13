//
//  SuperTuxKart - a fun racing game with go-kart
//  Copyright (C) 2013 SuperTuxKart-Team
//
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 3
//  of the License, or (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program; if not, write to the Free Software
//  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

#include "network/game_setup.hpp"

#include "utils/log.hpp"

//-----------------------------------------------------------------------------

GameSetup::GameSetup()
{
}

//-----------------------------------------------------------------------------

GameSetup::~GameSetup()
{
}

//-----------------------------------------------------------------------------

void GameSetup::addPlayer(NetworkPlayerProfile* profile)
{
    m_players.push_back(profile);
    Log::verbose("GameSetup", "New player in the game setup. Global id : %u, "
        "Race id : %d.", profile->user_profile->getUserID(), profile->race_id);
}

//-----------------------------------------------------------------------------

bool GameSetup::removePlayer(uint32_t id)
{
    for (unsigned int i = 0; i < m_players.size(); i++)
    {
        if (m_players[i]->user_profile->getUserID() == id)
        {
            delete m_players[i];
            m_players.erase(m_players.begin()+i, m_players.begin()+i+1);
            Log::verbose("GameSetup", "Removed a player from the game setup. "
                        "Remains %u.", m_players.size());
            return true;
        }
    }
    return false;
}

//-----------------------------------------------------------------------------

bool GameSetup::removePlayer(uint8_t id)
{
    for (unsigned int i = 0; i < m_players.size(); i++)
    {
        if (m_players[i]->race_id == id) // check the given id
        {
            delete m_players[i];
            m_players.erase(m_players.begin()+i, m_players.begin()+i+1);
            Log::verbose("GameSetup", "Removed a player from the game setup. "
                        "Remains %u.", m_players.size());
            return true;
        }
    }
    return false;
}

//-----------------------------------------------------------------------------

void GameSetup::setPlayerKart(uint8_t id, std::string kart_name)
{
    bool found = false;
    for (unsigned int i = 0; i < m_players.size(); i++)
    {
        if (m_players[i]->race_id == id)
        {
            m_players[i]->kart_name = kart_name;
            Log::info("GameSetup::setPlayerKart", "Player %d took kart %s", 
                        id, kart_name.c_str());
            found = true;
        }
    }
    if (!found)
    {
        Log::info("GameSetup::setPlayerKart", "The player %d was unknown.", id);
    }
}

//-----------------------------------------------------------------------------

const NetworkPlayerProfile* GameSetup::getProfile(uint32_t id)
{
    for (unsigned int i = 0; i < m_players.size(); i++)
    {
        if (m_players[i]->user_profile->getUserID() == id)
        {
            return m_players[i];
        }
    }
    return NULL;
}

//-----------------------------------------------------------------------------

const NetworkPlayerProfile* GameSetup::getProfile(uint8_t id)
{
    for (unsigned int i = 0; i < m_players.size(); i++)
    {
        if (m_players[i]->race_id == id)
        {
            return m_players[i];
        }
    }
    return NULL;
}

//-----------------------------------------------------------------------------

bool GameSetup::isKartAvailable(std::string kart_name)
{
    for (unsigned int i = 0; i < m_players.size(); i++)
    {
        if (m_players[i]->kart_name == kart_name)
            return false;
    }
    return true;
}
