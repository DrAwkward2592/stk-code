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

#include "network/protocol_manager.hpp"

#include "network/protocol.hpp"
#include "network/network_manager.hpp"
#include "utils/log.hpp"
#include "graphics/irr_driver.hpp"

#include <assert.h>
#include <cstdlib>
#include <errno.h>
#include <typeinfo>

void* protocolManagerUpdate(void* data)
{
    ProtocolManager* manager = static_cast<ProtocolManager*>(data);
    while(!manager->exit())
    {
        manager->update();
        irr_driver->getDevice()->sleep(20);
    }
    return NULL;
}

ProtocolManager::ProtocolManager()
{
    pthread_mutex_init(&m_events_mutex, NULL);
    pthread_mutex_init(&m_protocols_mutex, NULL);
    pthread_mutex_init(&m_requests_mutex, NULL);
    pthread_mutex_init(&m_id_mutex, NULL);
    pthread_mutex_init(&m_exit_mutex, NULL);
    m_next_protocol_id = 0;


    pthread_mutex_lock(&m_exit_mutex); // will let the update function run
    m_update_thread = (pthread_t*)(malloc(sizeof(pthread_t)));
    pthread_create(m_update_thread, NULL, protocolManagerUpdate, this);
}

ProtocolManager::~ProtocolManager()
{
    pthread_mutex_unlock(&m_exit_mutex); // will stop the update function
    pthread_mutex_lock(&m_events_mutex);
    pthread_mutex_lock(&m_protocols_mutex);
    pthread_mutex_lock(&m_requests_mutex);
    pthread_mutex_lock(&m_id_mutex);
    for (unsigned int i = 0; i < m_protocols.size() ; i++)
        delete m_protocols[i].protocol;
    for (unsigned int i = 0; i < m_events_to_process.size() ; i++)
        delete m_events_to_process[i];
    m_protocols.clear();
    m_requests.clear();
    m_events_to_process.clear();
    pthread_mutex_unlock(&m_events_mutex);
    pthread_mutex_unlock(&m_protocols_mutex);
    pthread_mutex_unlock(&m_requests_mutex);
    pthread_mutex_unlock(&m_id_mutex);

    pthread_mutex_destroy(&m_events_mutex);
    pthread_mutex_destroy(&m_protocols_mutex);
    pthread_mutex_destroy(&m_requests_mutex);
    pthread_mutex_destroy(&m_id_mutex);
    pthread_mutex_destroy(&m_exit_mutex);
}

void ProtocolManager::notifyEvent(Event* event)
{
    Event* event2 = new Event(*event);
    pthread_mutex_lock(&m_events_mutex);
    m_events_to_process.push_back(event2); // add the event to the queue
    pthread_mutex_unlock(&m_events_mutex);
}

void ProtocolManager::sendMessage(Protocol* sender, const NetworkString& message)
{
    NetworkString newMessage;
    newMessage.ai8(sender->getProtocolType()); // add one byte to add protocol type
    newMessage += message;
    NetworkManager::getInstance()->sendPacket(newMessage);
}

void ProtocolManager::sendMessage(Protocol* sender, STKPeer* peer, const NetworkString& message)
{
    NetworkString newMessage;
    newMessage.ai8(sender->getProtocolType()); // add one byte to add protocol type
    newMessage += message;
    NetworkManager::getInstance()->sendPacket(peer, newMessage);
}
void ProtocolManager::sendMessageExcept(Protocol* sender, STKPeer* peer, const NetworkString& message)
{
    NetworkString newMessage;
    newMessage.ai8(sender->getProtocolType()); // add one byte to add protocol type
    newMessage += message;
    NetworkManager::getInstance()->sendPacketExcept(peer, newMessage);
}

uint32_t ProtocolManager::requestStart(Protocol* protocol)
{
    // create the request
    ProtocolRequest req;
    ProtocolInfo info;
    info.protocol = protocol;
    info.state = PROTOCOL_STATE_RUNNING;
    assignProtocolId(&info); // assign a unique id to the protocol.
    req.protocol_info = info;
    req.type = PROTOCOL_REQUEST_START;
    // add it to the request stack
    pthread_mutex_lock(&m_requests_mutex);
    m_requests.push_back(req);
    pthread_mutex_unlock(&m_requests_mutex);

    return info.id;
}

void ProtocolManager::requestStop(Protocol* protocol)
{
    // create the request
    ProtocolRequest req;
    req.protocol_info.protocol = protocol;
    req.type = PROTOCOL_REQUEST_STOP;
    // add it to the request stack
    pthread_mutex_lock(&m_requests_mutex);
    m_requests.push_back(req);
    pthread_mutex_unlock(&m_requests_mutex);
}

void ProtocolManager::requestPause(Protocol* protocol)
{
    // create the request
    ProtocolRequest req;
    req.protocol_info.protocol = protocol;
    req.type = PROTOCOL_REQUEST_PAUSE;
    // add it to the request stack
    pthread_mutex_lock(&m_requests_mutex);
    m_requests.push_back(req);
    pthread_mutex_unlock(&m_requests_mutex);
}

void ProtocolManager::requestUnpause(Protocol* protocol)
{
    // create the request
    ProtocolRequest req;
    req.protocol_info.protocol = protocol;
    req.type = PROTOCOL_REQUEST_UNPAUSE;
    // add it to the request stack
    pthread_mutex_lock(&m_requests_mutex);
    m_requests.push_back(req);
    pthread_mutex_unlock(&m_requests_mutex);
}

void ProtocolManager::requestTerminate(Protocol* protocol)
{
    // create the request
    ProtocolRequest req;
    req.protocol_info.protocol = protocol;
    req.type = PROTOCOL_REQUEST_TERMINATE;
    // add it to the request stack
    pthread_mutex_lock(&m_requests_mutex);
    m_requests.push_back(req);
    pthread_mutex_unlock(&m_requests_mutex);
}

void ProtocolManager::startProtocol(ProtocolInfo protocol)
{
    Log::info("ProtocolManager", "A %s protocol with id=%u has been started. There are %ld protocols running.", typeid(*protocol.protocol).name(), protocol.id, m_protocols.size()+1);
    // add the protocol to the protocol vector so that it's updated
    pthread_mutex_lock(&m_protocols_mutex);
    m_protocols.push_back(protocol);
    pthread_mutex_unlock(&m_protocols_mutex);
    // setup the protocol and notify it that it's started
    protocol.protocol->setListener(this);
    protocol.protocol->setup();
}
void ProtocolManager::stopProtocol(ProtocolInfo protocol)
{

}
void ProtocolManager::pauseProtocol(ProtocolInfo protocol)
{
    for (unsigned int i = 0; i < m_protocols.size(); i++)
    {
        if (m_protocols[i].protocol == protocol.protocol && m_protocols[i].state == PROTOCOL_STATE_RUNNING)
        {
            m_protocols[i].state = PROTOCOL_STATE_PAUSED;
            m_protocols[i].protocol->pause();
        }
    }
}
void ProtocolManager::unpauseProtocol(ProtocolInfo protocol)
{
    for (unsigned int i = 0; i < m_protocols.size(); i++)
    {
        if (m_protocols[i].protocol == protocol.protocol && m_protocols[i].state == PROTOCOL_STATE_PAUSED)
        {
            m_protocols[i].state = PROTOCOL_STATE_RUNNING;
            m_protocols[i].protocol->unpause();
        }
    }
}
void ProtocolManager::protocolTerminated(ProtocolInfo protocol)
{
    pthread_mutex_lock(&m_protocols_mutex); // be sure that noone accesses the protocols vector while we erase a protocol
    int offset = 0;
    std::string protocol_type = typeid(*protocol.protocol).name();
    for (unsigned int i = 0; i < m_protocols.size(); i++)
    {
        if (m_protocols[i-offset].protocol == protocol.protocol)
        {
            delete m_protocols[i].protocol;
            m_protocols.erase(m_protocols.begin()+(i-offset), m_protocols.begin()+(i-offset)+1);
            offset++;
        }
    }
    Log::info("ProtocolManager", "A %s protocol has been terminated. There are %ld protocols running.", protocol_type.c_str(), m_protocols.size());
    pthread_mutex_unlock(&m_protocols_mutex);
}

void ProtocolManager::update()
{
    // before updating, notice protocols that they have received information
    int size = m_events_to_process.size();
    for (int i = 0; i < size; i++)
    {
        pthread_mutex_lock(&m_events_mutex); // secure threads
        Event* event = m_events_to_process.back();
        m_events_to_process.pop_back();
        pthread_mutex_unlock(&m_events_mutex); // release the mutex

        PROTOCOL_TYPE searchedProtocol = PROTOCOL_NONE;
        if (event->type == EVENT_TYPE_MESSAGE)
        {
            if (event->data.size() > 0)
                searchedProtocol = (PROTOCOL_TYPE)(event->data.getAndRemoveUInt8());
        }
        if (event->type == EVENT_TYPE_CONNECTED)
        {
            searchedProtocol = PROTOCOL_CONNECTION;
        }
        for (unsigned int i = 0; i < m_protocols.size() ; i++)
        {
            if (m_protocols[i].protocol->getProtocolType() == searchedProtocol || event->type == EVENT_TYPE_DISCONNECTED) // pass data to protocols even when paused
                m_protocols[i].protocol->notifyEvent(event);
        }
        if (searchedProtocol == PROTOCOL_NONE) // no protocol was aimed, show the msg to debug
        {
            Log::debug("ProtocolManager", "Message is \"%s\"", event->data.c_str());
        }

        // because we made a copy of the event
        delete event->peer; // no more need of that
        delete event;
    }

    // now update all protocols
    pthread_mutex_lock(&m_protocols_mutex);
    for (unsigned int i = 0; i < m_protocols.size(); i++)
    {
        if (m_protocols[i].state == PROTOCOL_STATE_RUNNING)
            m_protocols[i].protocol->update();
    }
    pthread_mutex_unlock(&m_protocols_mutex);

    // process queued events for protocols
    pthread_mutex_lock(&m_requests_mutex);
    for (unsigned int i = 0; i < m_requests.size(); i++)
    {
        switch (m_requests[i].type)
        {
            case PROTOCOL_REQUEST_START:
                startProtocol(m_requests[i].protocol_info);
                break;
            case PROTOCOL_REQUEST_STOP:
                stopProtocol(m_requests[i].protocol_info);
                break;
            case PROTOCOL_REQUEST_PAUSE:
                pauseProtocol(m_requests[i].protocol_info);
                break;
            case PROTOCOL_REQUEST_UNPAUSE:
                unpauseProtocol(m_requests[i].protocol_info);
                break;
            case PROTOCOL_REQUEST_TERMINATE:
                protocolTerminated(m_requests[i].protocol_info);
                break;
        }
    }
    m_requests.clear();
    pthread_mutex_unlock(&m_requests_mutex);
}

int ProtocolManager::runningProtocolsCount()
{
    return m_protocols.size();
}

PROTOCOL_STATE ProtocolManager::getProtocolState(uint32_t id)
{
    for (unsigned int i = 0; i < m_protocols.size(); i++)
    {
        if (m_protocols[i].id == id) // we know a protocol with that id
            return m_protocols[i].state; // return its state
    }
    // the protocol isn't running right now
    for (unsigned int i = 0; i < m_requests.size(); i++)
    {
        if (m_requests[i].protocol_info.id == id) // the protocol is going to be started
            return PROTOCOL_STATE_RUNNING; // we can say it's running
    }
    return PROTOCOL_STATE_TERMINATED; // else, it's already finished
}

PROTOCOL_STATE ProtocolManager::getProtocolState(Protocol* protocol)
{
    for (unsigned int i = 0; i < m_protocols.size(); i++)
    {
        if (m_protocols[i].protocol == protocol) // the protocol is known
            return m_protocols[i].state; // return its state
    }
    for (unsigned int i = 0; i < m_requests.size(); i++)
    {
        if (m_requests[i].protocol_info.protocol == protocol) // the protocol is going to be started
            return PROTOCOL_STATE_RUNNING; // we can say it's running
    }
    return PROTOCOL_STATE_TERMINATED; // we don't know this protocol at all, it's finished
}

uint32_t ProtocolManager::getProtocolID(Protocol* protocol)
{
    for (unsigned int i = 0; i < m_protocols.size(); i++)
    {
        if (m_protocols[i].protocol == protocol)
            return m_protocols[i].id;
    }
    return 0;
}

Protocol* ProtocolManager::getProtocol(uint32_t id)
{
    for (unsigned int i = 0; i < m_protocols.size(); i++)
    {
        if (m_protocols[i].id == id)
            return m_protocols[i].protocol;
    }
    return NULL;
}

Protocol* ProtocolManager::getProtocol(PROTOCOL_TYPE type)
{
    for (unsigned int i = 0; i < m_protocols.size(); i++)
    {
        if (m_protocols[i].protocol->getProtocolType() == type)
            return m_protocols[i].protocol;
    }
    return NULL;
}

bool ProtocolManager::isServer()
{
    return NetworkManager::getInstance()->isServer();
}

int ProtocolManager::exit()
{
  switch(pthread_mutex_trylock(&m_exit_mutex)) {
    case 0: /* if we got the lock, unlock and return 1 (true) */
      pthread_mutex_unlock(&m_exit_mutex);
      return 1;
    case EBUSY: /* return 0 (false) if the mutex was locked */
      return 0;
  }
  return 1;
}

void ProtocolManager::assignProtocolId(ProtocolInfo* protocol_info)
{
    pthread_mutex_lock(&m_id_mutex);
    protocol_info->id = m_next_protocol_id;
    m_next_protocol_id++;
    pthread_mutex_unlock(&m_id_mutex);
}


