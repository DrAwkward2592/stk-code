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


#ifndef HEADER_REGISTRATION_DIALOG_HPP
#define HEADER_REGISTRATION_DIALOG_HPP

#include <irrString.h>

#include "guiengine/modaldialog.hpp"
/**
 * \brief Dialog that allows a user to register
 * \ingroup states_screens
 */
class RegistrationDialog : public GUIEngine::ModalDialog
{
public:
    
    enum Phase
    {
        Info = 1,
        Terms = 2,
        Activation = 4
    };

    RegistrationDialog(const Phase phase = Info);
    ~RegistrationDialog();

    void onEnterPressedInternal();
    GUIEngine::EventPropagation processEvent(const std::string& eventSource);
    
    virtual void onUpdate(float dt);

private:
    Phase m_phase;
    bool m_self_destroy;
    bool m_show_registration_info;
    bool m_show_registration_terms;
    bool m_show_registration_activation;

    //Saved user input :
    irr::core::stringw m_username;
    irr::core::stringw m_password;
    irr::core::stringw m_password_confirm;
    irr::core::stringw m_email;
    irr::core::stringw m_email_confirm;
    irr::core::stringw m_registration_error;
    bool m_agreement;

    void showRegistrationInfo();
    void showRegistrationTerms();
    void showRegistrationActivation();
    bool processInfoEvent(const std::string& eventSource);
    bool processTermsEvent(const std::string& eventSource);
    bool processActivationEvent(const std::string& eventSource);

};

#endif
