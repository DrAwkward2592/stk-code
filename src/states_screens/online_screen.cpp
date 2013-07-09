//  SuperTuxKart - a fun racing game with go-kart
//  Copyright (C) 2013 Glenn De Jonghe
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

#define DEBUG_MENU_ITEM 0

#include "states_screens/online_screen.hpp"

#include <string>
#include <iostream>

#include "challenges/game_slot.hpp"
#include "challenges/unlock_manager.hpp"
#include "graphics/irr_driver.hpp"
#include "guiengine/scalable_font.hpp"
#include "input/device_manager.hpp"
#include "input/input_manager.hpp"
#include "io/file_manager.hpp"
#include "main_loop.hpp"
#include "states_screens/online_screen.hpp"
#include "states_screens/state_manager.hpp"
#include "states_screens/dialogs/message_dialog.hpp"
#include "states_screens/dialogs/login_dialog.hpp"
#include "states_screens/dialogs/registration_dialog.hpp"
#include "states_screens/networking_lobby.hpp"
#include "states_screens/networking_lobby_settings.hpp"
#include "modes/demo_world.hpp"
#include "utils/translation.hpp"

#include "online/current_online_user.hpp"

#include "network/protocol_manager.hpp"
#include "network/protocols/connect_to_server.hpp"


using namespace GUIEngine;

DEFINE_SCREEN_SINGLETON( OnlineScreen );

// ----------------------------------------------------------------------------

OnlineScreen::OnlineScreen() : Screen("online/main.stkgui")
{
    m_recorded_state = Not;
}   // OnlineScreen

// ----------------------------------------------------------------------------

void OnlineScreen::loadedFromFile()
{
    //Box ? FIXME
    m_top_menu_widget = getWidget<RibbonWidget>("menu_toprow");
    assert(m_top_menu_widget != NULL);
    m_quick_play_widget = (IconButtonWidget *) m_top_menu_widget->findWidgetNamed("quick_play");
    assert(m_quick_play_widget != NULL);
    m_find_server_widget = (IconButtonWidget *) m_top_menu_widget->findWidgetNamed("find_server");
    assert(m_find_server_widget != NULL);
    m_create_server_widget = (IconButtonWidget *) m_top_menu_widget->findWidgetNamed("create_server");
    assert(m_create_server_widget != NULL);

    m_online_status_widget = getWidget<LabelWidget>("online_status");
    assert(m_online_status_widget != NULL);

    m_bottom_menu_widget = getWidget<RibbonWidget>("menu_bottomrow");
    assert(m_bottom_menu_widget != NULL);
    m_sign_in_widget = (IconButtonWidget *) m_bottom_menu_widget->findWidgetNamed("sign_in");
    assert(m_sign_in_widget != NULL);
    m_register_widget = (IconButtonWidget *) m_bottom_menu_widget->findWidgetNamed("register");
    assert(m_register_widget != NULL);
    m_sign_out_widget = (IconButtonWidget *) m_bottom_menu_widget->findWidgetNamed("sign_out");
    assert(m_sign_out_widget != NULL);

}   // loadedFromFile

// ----------------------------------------------------------------------------
bool OnlineScreen::hasStateChanged()
{
    State previous_state = m_recorded_state;
    if(CurrentOnlineUser::get()->isSignedIn())
    {
        if(CurrentOnlineUser::get()->isGuest())
            m_recorded_state = Guest;
        else
            m_recorded_state = Registered;
    }
    else
        m_recorded_state = Not;
    if (previous_state != m_recorded_state)
        return true;
    return false;
}

// ----------------------------------------------------------------------------
void OnlineScreen::beforeAddingWidget()
{
    //Set everything that could be set invisible or deactivated, to active and visible
    m_bottom_menu_widget->setVisible(true);
    m_top_menu_widget->setVisible(true);

    hasStateChanged();
    if (m_recorded_state == Registered)
    {
        m_register_widget->setVisible(false);
        m_sign_in_widget->setVisible(false);
    }
    else if (m_recorded_state == Not)
    {
        m_quick_play_widget->setDeactivated();
        m_find_server_widget->setDeactivated();
        m_create_server_widget->setDeactivated();
        m_sign_out_widget->setVisible(false);
    }
    else if (m_recorded_state == Guest)
    {
        m_find_server_widget->setDeactivated();
        m_create_server_widget->setDeactivated();
        m_sign_in_widget->setVisible(false);
    }

} // beforeAddingWidget



// ----------------------------------------------------------------------------
void OnlineScreen::init()
{
    Screen::init();
    setInitialFocus();
    DemoWorld::resetIdleTime(); //FIXME : what's this?
    m_online_status_widget->setText(irr::core::stringw(_("Signed in as : ")) + CurrentOnlineUser::get()->getUserName() + ".", false);
}   // init

// ----------------------------------------------------------------------------
void OnlineScreen::onUpdate(float delta,  irr::video::IVideoDriver* driver)
{
    if (hasStateChanged())
        GUIEngine::reshowCurrentScreen();
}   // onUpdate

// ----------------------------------------------------------------------------

void OnlineScreen::eventCallback(Widget* widget, const std::string& name, const int playerID)
{
    if (name == "back")
    {
        StateManager::get()->escapePressed();
    }

    RibbonWidget* ribbon = dynamic_cast<RibbonWidget*>(widget);
    if (ribbon == NULL) return; // what's that event??
    std::string selection = ribbon->getSelectionIDString(PLAYER_ID_GAME_MASTER);

    if (selection == "sign_in")
    {
        new LoginDialog(LoginDialog::Normal);
    }
    else if (selection == "sign_out")
    {
        if (CurrentOnlineUser::get()->signOut())
        {
            new MessageDialog( _("Signed out successfully.") );
            //GUIEngine::reshowCurrentScreen();
        }
        else
            new MessageDialog( _("An error occured while signing out.") );
    }
    else if (selection == "register")
    {
        new RegistrationDialog(0.8f, 0.9f);
    }
    else if (selection == "find_server")
    {
        //if (m_recorded_state == Registered)
            new MessageDialog("Coming soon!");
    }
    else if (selection == "create_server")
    {
        //if (m_recorded_state == Registered)
        StateManager::get()->pushScreen(NetworkingLobbySettings::getInstance());
    }
    else if (selection == "quick_play")
    {
        //if (m_recorded_state == Registered || m_recorded_state == Guest) FIXME
        StateManager::get()->pushScreen(NetworkingLobby::getInstance());
        ProtocolManager::getInstance()->requestStart(new ConnectToServer()); // quick join
    }

}   // eventCallback

// ----------------------------------------------------------------------------

void OnlineScreen::tearDown()
{
}   // tearDown

// ----------------------------------------------------------------------------
void OnlineScreen::onDisabledItemClicked(const std::string& item)
{
    if (item == "find_server")
    {
        new LoginDialog(LoginDialog::Registration_Required);
    }
    else if (item =="create_server")
    {
        StateManager::get()->pushScreen(NetworkingLobbySettings::getInstance());
        // FIXME temporary; new LoginDialog(LoginDialog::Registration_Required);
    }
    else if (item == "quick_play")
    {
        StateManager::get()->pushScreen(NetworkingLobby::getInstance());
        // FIXME temporary; new LoginDialog(LoginDialog::Signing_In_Required);
    }
}   // onDisabledItemClicked

// ----------------------------------------------------------------------------
void OnlineScreen::setInitialFocus()
{
    if(m_recorded_state == Not)
        m_bottom_menu_widget->setFocusForPlayer(PLAYER_ID_GAME_MASTER);
    else
        m_top_menu_widget->setFocusForPlayer(PLAYER_ID_GAME_MASTER);
}   // setInitialFocus

// ----------------------------------------------------------------------------
void OnlineScreen::onDialogClose()
{
    if (hasStateChanged())
        GUIEngine::reshowCurrentScreen();
    else
        setInitialFocus();
}   // onDialogClose()
