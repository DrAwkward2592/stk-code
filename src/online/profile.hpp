//
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

#ifndef HEADER_ONLINE_PROFILE_HPP
#define HEADER_ONLINE_PROFILE_HPP

#include "online/request_manager.hpp"
#include "online/xml_request.hpp"
#include "utils/types.hpp"
#include "utils/ptr_vector.hpp"


#include <irrString.h>

#include <string>

namespace Online
{
/** Class that represents an online profile. It manages the online profile
 *  for any user on this system, but also for users for which information
 *  is requested (e.g. to see the achievements of friends). All those profiles
 *  are managed by the ProfileManager.
 * \ingroup online
 */
class Profile
{
public:
    enum ConstructorType
    {
        C_DEFAULT = 1,
        C_RELATION_INFO
    };

    // ========================================================================
    class RelationInfo
    {
    private:
        bool m_is_online;
        bool m_is_pending;
        bool m_is_asker;
        irr::core::stringw m_date;
    public:
        RelationInfo(const irr::core::stringw & date, bool is_online, 
                     bool is_pending, bool is_asker = false);
        void setOnline(bool online);
        // --------------------------------------------------------------------
        bool isPending() const { return m_is_pending; }
        // --------------------------------------------------------------------
        bool isAsker()  const { return m_is_asker; }
        // --------------------------------------------------------------------
        const irr::core::stringw & getDate() const { return m_date; }
        // --------------------------------------------------------------------
        bool isOnline() const { return m_is_online; }
    };  // class RelationInfo
    // ========================================================================

    typedef std::vector<uint32_t> IDList;
private:

    /** The profile can either be fetching data, or be ready. */
    enum State
    {
        S_FETCHING = 1,
        S_READY
    };

    State                           m_state;
    bool                            m_is_current_user;
    uint32_t                        m_id;
    irr::core::stringw              m_username;
    /** information about the relation with the current user */
    RelationInfo *                  m_relation_info;
    /** Whether or not the user of this profile, is a friend of the current user */
    bool                            m_is_friend;

    bool                            m_has_fetched_friends;

    /** List of user id's that are friends with the user of this profile.
     * In case this profile is of the current user, this list also contains 
     * any id's of users that still have a friend request pending. */
    std::vector<uint32_t>           m_friends;

    bool                            m_has_fetched_achievements;
    std::vector<uint32_t>           m_achievements;

    bool                            m_cache_bit;

    void storeFriends(const XMLNode * input);
    void storeAchievements(const XMLNode * input);

public:
    Profile(const uint32_t           & userid,
            const irr::core::stringw & username,
            bool is_current_user = false       );
    Profile(const XMLNode * xml,
            ConstructorType type = C_DEFAULT);
    ~Profile();
    void fetchFriends();
    const IDList&   getFriends();
    void fetchAchievements();
    void removeFriend(const uint32_t id);
    void addFriend(const uint32_t id);
    void deleteRelationalInfo();
    const IDList&   getAchievements();
    // ------------------------------------------------------------------------
    /** Returns true if the achievements for this profile have been fetched. */
    bool hasFetchedAchievements() const { return m_has_fetched_achievements; }
    // ------------------------------------------------------------------------
    /** Returns true if the friend list for this profile has been fetched. */
    bool hasFetchedFriends() const { return m_has_fetched_friends; }
    // ------------------------------------------------------------------------
    /** True if the profile is not fetching data atm. */
    bool isReady() const { return m_state == S_READY; }
    // ------------------------------------------------------------------------
    bool isCurrentUser() const { return m_is_current_user; }
    // ------------------------------------------------------------------------
    bool isFriend() const                 { return m_is_friend; }
    // ------------------------------------------------------------------------
    void setFriend()                      { m_is_friend = true; }
    // ------------------------------------------------------------------------
    RelationInfo* getRelationInfo()                { return m_relation_info; }
    // ------------------------------------------------------------------------
    void setRelationInfo(RelationInfo * r){ delete m_relation_info; m_relation_info = r; }

    void setCacheBit(bool cache_bit)      { m_cache_bit = cache_bit; }
    bool getCacheBit() const              { return m_cache_bit; }

    uint32_t getID() const                    { return m_id; }
    const irr::core::stringw &      getUserName() const              { return m_username; }

    void                            merge(Profile * profile);


};   // class Profile

} // namespace Online

#endif

/*EOF*/
