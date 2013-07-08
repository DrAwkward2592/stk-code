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

#ifndef CONNECT_TO_SERVER_HPP
#define CONNECT_TO_SERVER_HPP

#include "network/protocol.hpp"
#include "network/types.hpp"
#include <string>

class ConnectToServer : public Protocol, public CallbackObject
{
    public:
        ConnectToServer(uint32_t server_id);
        virtual ~ConnectToServer();
        
        virtual void notifyEvent(Event* event);
        virtual void setup();
        virtual void update();
        
    protected:
        TransportAddress m_server_address;
        TransportAddress m_public_address;
        uint32_t m_server_id;
        uint32_t m_current_protocol_id;
        
        enum STATE
        {
            NONE,
            WAITING_SELF_ADDRESS,
            SELF_ADDRESS_KNOWN,
            PEER_ADDRESS_KNOWN,
            SELF_ADDRESS_SHOWN,
            CONNECTING,
            CONNECTED,
            HIDING_ADDRESS,
            DONE
        };
        STATE m_state;
};

#endif // CONNECT_TO_SERVER_HPP