// $Id: fuzzy_ai_controller.cpp 10066 2011-10-30 20:37:14Z auria $
// TODO ID KINSU
//
//  SuperTuxKart - a fun racing game with go-kart
//  Copyright (C) 2004-2005 Steve Baker <sjbaker1@airmail.net>
//  Copyright (C) 2006-2007 Eduardo Hernandez Munoz
//  Copyright (C) 2008      Joerg Henrichs
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


//The AI debugging works best with just 1 AI kart, so set the number of karts
//to 2 in main.cpp with quickstart and run supertuxkart with the arg -N.

//#undef AI_DEBUG
#define AI_DEBUG

// Fuzzy variable designation TODO put them in a specific constants file
#define PLAYER_RANK	0 //  1st variable
#define PLAYER_CRASHES	1 //  2nd variable

#include "io/file_manager.hpp"
#include "karts/controller/fuzzy_ai_controller.hpp"
#include "karts/controller/fuzzy_data_manager.hpp"
#include "tracks/fuzzy_ai_path_tree.hpp"

#ifdef AI_DEBUG
#  include "irrlicht.h"
   using namespace irr;
#endif

#include <cstdlib>
#include <ctime>
#include <cstdio>
#include <iostream>
#include <vector>

#ifdef AI_DEBUG
#  include "graphics/irr_driver.hpp"
#endif

#include "ffll/FFLLAPI.h"

#include "graphics/slip_stream.hpp"
#include "modes/linear_world.hpp"
#include "network/network_manager.hpp"
#include "race/race_manager.hpp" // TODO : check if really necessary
#include "items/item.hpp"
#include "items/item_manager.hpp"
#include "tracks/quad_graph.hpp"
#include "tracks/track.hpp"
#include "utils/constants.hpp"

using namespace std;

FuzzyAIController::FuzzyAIController(Kart *kart) : AIBaseController(kart)
{
    reset();

    switch( race_manager->getDifficulty())
    {
    case RaceManager::RD_EASY:
        m_wait_for_players        = true;
        m_make_use_of_slipstream  = false;
        m_max_handicap_speed      = 0.9f;
        m_item_tactic             = IT_TEN_SECONDS;
        m_false_start_probability = 0.08f;
        m_min_start_delay         = 0.3f;
        m_max_start_delay         = 0.5f;
        m_min_steps               = 1;
        m_nitro_level             = NITRO_NONE;
        m_handle_bomb             = false;
        setSkiddingFraction(4.0f);
        break;
    case RaceManager::RD_MEDIUM:
        m_wait_for_players        = true;
        m_make_use_of_slipstream  = false;
        m_max_handicap_speed      = 0.95f;
        m_item_tactic             = IT_CALCULATE;
        m_false_start_probability = 0.04f;
        m_min_start_delay         = 0.25f;
        m_max_start_delay         = 0.4f;
        m_min_steps               = 1;
        m_nitro_level             = NITRO_SOME;
        m_handle_bomb             = true;
        setSkiddingFraction(3.0f);
        break;
    case RaceManager::RD_HARD:
        m_wait_for_players        = false;
        m_make_use_of_slipstream  = true;
        m_max_handicap_speed      = 1.0f;
        m_item_tactic             = IT_CALCULATE;
        m_false_start_probability = 0.01f;
        // See http://www.humanbenchmark.com/tests/reactiontime/stats.php
        // Average reaction time is around 0.215 s, so using .15 as minimum
        // gives an AI average slightly above the human average
        m_min_start_delay         = 0.15f;
        m_max_start_delay         = 0.28f;
        m_min_steps               = 2;
        m_nitro_level             = NITRO_ALL;
        m_handle_bomb             = true;
        setSkiddingFraction(2.0f);

        // -- Fuzzy controller params --
        // Player evaluation
        m_timer                   = 0.0f;
        
                
        break;
    }

#ifdef AI_DEBUG
    m_debug_sphere = irr_driver->getSceneManager()->addSphereSceneNode(1);
#endif
}   // FuzzyAIController

//-----------------------------------------------------------------------------
/** The destructor deletes the shared TrackInfo objects if no more FuzzyAIController
 *  instances are around.
 */
FuzzyAIController::~FuzzyAIController()
{
#ifdef AI_DEBUG
    irr_driver->removeNode(m_debug_sphere);
#endif
}   // ~FuzzyAIController

//-----------------------------------------------------------------------------
void FuzzyAIController::reset()
{
    m_time_since_last_shot       = 0.0f;
    m_start_kart_crash_direction = 0;
    m_curve_target_speed         = m_kart->getCurrentMaxSpeed();
    m_curve_angle                = 0.0;
    m_start_delay                = -1.0f;
    m_crash_time                 = 0.0f;
    m_collided                   = false;
    m_time_since_stuck           = 0.0f;
    m_kart_ahead                 = NULL;
    m_distance_ahead             = 0.0f;
    m_kart_behind                = NULL;
    m_distance_behind            = 0.0f;

    AIBaseController::reset();
    m_track_node               = QuadGraph::UNKNOWN_SECTOR;
    QuadGraph::get()->findRoadSector(m_kart->getXYZ(), &m_track_node);
    if(m_track_node==QuadGraph::UNKNOWN_SECTOR)
    {
        fprintf(stderr, 
                "Invalid starting position for '%s' - not on track"
                " - can be ignored.\n",
                m_kart->getIdent().c_str());
        m_track_node = QuadGraph::get()->findOutOfRoadSector(m_kart->getXYZ());
    }

}   // reset

//-----------------------------------------------------------------------------
const irr::core::stringw& FuzzyAIController::getNamePostfix() const 
{
    // Static to avoid returning the address of a temporary stringq
    static irr::core::stringw name="(fuzzy)";
    return name;
}   // getNamePostfix

//-----------------------------------------------------------------------------
/** Returns the pre-computed successor of a graph node.
 *  \parameter index The index of the graph node for which the successor
 *              is searched.
 */
unsigned int FuzzyAIController::getNextSector(unsigned int index)
{
    return m_successor_index[index];
}   // getNextSector

//-----------------------------------------------------------------------------
//TODO: if the AI is crashing constantly, make it move backwards in a straight
//line, then move forward while turning.
void FuzzyAIController::update(float dt)
{
    // This is used to enable firing an item backwards.
    m_controls->m_look_back = false;
    m_controls->m_nitro     = false;

    // Having a non-moving AI can be useful for debugging, e.g. aiming
    // or slipstreaming.
#undef AI_DOES_NOT_MOVE_FOR_DEBUGGING
#ifdef AI_DOES_NOT_MOVE_FOR_DEBUGGING
    m_controls->m_accel     = 0;
    m_controls->m_steer     = 0;
    return;
#endif

    // The client does not do any AI computations.
    if(network_manager->getMode()==NetworkManager::NW_CLIENT) 
    {
        AIBaseController::update(dt);
        return;
    }

    if( m_world->isStartPhase() )
    {
        handleRaceStart();
        AIBaseController::update(dt);
        return;
    }

    /*Get information that is needed by more than 1 of the handling funcs*/
    //Detect if we are going to crash with the track and/or kart
    int steps = 0;

    steps = calcSteps();

    computeNearestKarts();
    checkCrashes( steps, m_kart->getXYZ() );
    findCurve();

    // Special behaviour if we have a bomb attach: try to hit the kart ahead 
    // of us.
    bool commands_set = false;
    if(m_handle_bomb && 
        m_kart->getAttachment()->getType()==Attachment::ATTACH_BOMB && 
        m_kart_ahead )
    {
        // Use nitro if the kart is far ahead, or faster than this kart
        m_controls->m_nitro = m_distance_ahead>10.0f || 
                             m_kart_ahead->getSpeed() > m_kart->getSpeed();
        // If we are close enough, try to hit this kart
        if(m_distance_ahead<=10)
        {
            Vec3 target = m_kart_ahead->getXYZ();

            // If we are faster, try to predict the point where we will hit
            // the other kart
            if(m_kart_ahead->getSpeed() < m_kart->getSpeed())
            {
                float time_till_hit = m_distance_ahead
                                    / (m_kart->getSpeed()-m_kart_ahead->getSpeed());
                target += m_kart_ahead->getVelocity()*time_till_hit;
            }
            float steer_angle = steerToPoint(m_kart_ahead->getXYZ());
            setSteering(steer_angle, dt);
            commands_set = true;
        }
        handleRescue(dt);
    }
    if(!commands_set)
    {
        /*Response handling functions*/
        handleAcceleration(dt);
        handleSteering(dt);
        handleItems(dt);
        handleRescue(dt);
        handleBraking();
        // If a bomb is attached, nitro might already be set.
        if(!m_controls->m_nitro)
            handleNitroAndZipper();
    }
    // If we are supposed to use nitro, but have a zipper, 
    // use the zipper instead
    if(m_controls->m_nitro && 
        m_kart->getPowerup()->getType()==PowerupManager::POWERUP_ZIPPER && 
        m_kart->getSpeed()>1.0f && 
        m_kart->getSpeedIncreaseTimeLeft(MaxSpeed::MS_INCREASE_ZIPPER)<=0)
    {
        // Make sure that not all AI karts use the zipper at the same
        // time in time trial at start up, so during the first 5 seconds
        // this is done at random only.
        if(race_manager->getMinorMode()!=RaceManager::MINOR_MODE_TIME_TRIAL ||
            (m_world->getTime()<3.0f && rand()%50==1) )
        {
            m_controls->m_nitro = false;
            m_controls->m_fire  = true;
        }
    }

    /*And obviously general kart stuff*/
    AIBaseController::update(dt);
    m_collided = false;
    
    //==========================================================================
    // -- Fuzzy controller code --


    // path fork detection
    vector<unsigned int> nextNodes;
    QuadGraph::get()->getSuccessors(m_track_node, nextNodes, true);
    const vector<vector<PathData*>*>* pathData = NULL;
    if(nextNodes.size() > 1)
    {
        cout << "foooooooooooooooooooooooooooooooooooooooork !" << endl;
        pathData = fuzzy_data_manager->getPathData(m_track_node);
#ifdef AI_DEBUG
        if(pathData)
        {
            cout << "PATH DATA Debug : " << endl;
            for(unsigned int i=0; i<pathData->size() ; i++)
            {
                cout << "choice " << i << ":" << endl;
                for(unsigned int j=0; j<pathData->at(i)->size() ; j++)
                {
                    cout << "pathData " << j << " = ";
                    cout << pathData->at(i)->at(j)->pathLength << endl;
                }
            }
        }

#endif
    }

    m_timer += dt;
    if(m_timer >= 1.0f)        // every second, do
    {
        m_timer -= 1.0f;
        // Item position & type
        vector<Item*> nitro_b = item_manager->getCloseItems(m_kart, 40, Item::ITEM_NITRO_BIG);
        vector<Item*> nitro_s = item_manager->getCloseItems(m_kart, 40, Item::ITEM_NITRO_SMALL);
        vector<Item*> maluses = item_manager->getCloseItems(m_kart, 40, Item::ITEM_BANANA);
        vector<Item*> bubgums = item_manager->getCloseItems(m_kart, 40, Item::ITEM_BUBBLEGUM);
        maluses.insert(maluses.end(), bubgums.begin(), bubgums.end());
        vector<Item*> boxes = item_manager->getCloseItems(m_kart, 40, Item::ITEM_BONUS_BOX);
        vector<Item*> allItems = item_manager->getCloseItems(m_kart, 40, Item::ITEM_NONE);

        // Player evaluation
        // TODO : take in account the distance the player has reached ? So that on a 2 karts race, the player can be evaluated as good even if he is just behind the AI kart but has always been 2nd (so last), and can also be evaluated as bad...
        // see m_kart_info[kart_id].getSector()->getDistanceFromStart()

        float av_rank = fuzzy_data_manager->getPlayerAverageRank();
        int   crash_c = fuzzy_data_manager->getPlayerCrashCount();


        //Get total number of karts for normalization
		World *world = World::getWorld();

		int number_of_karts = world->getNumKarts();



        int eval = (int)computePlayerEvaluation("player_evaluation.fcl",number_of_karts, av_rank, crash_c);


        //Choose the driving style : competitiveness and agressiveness

        //Get the current ranking
        int current_ranking = m_kart->getPosition();


        //TODO: Kart classes (heavy..) are not implemented. We use a test value.

        int kart_class = 1;

        int competitiveness = computeCompetitiveness("driving_style_competitiveness.fcl",number_of_karts,eval,current_ranking);
        int agressiveness = computeAgressiveness("driving_style_agressiveness.fcl",number_of_karts,kart_class,current_ranking);


    
        //Decide the best path to take !
        if(pathData)
        {
            computePathChooser("path_chooser.fcl",pathData,competitiveness);
        }



        //Decide if it is interesting or not to use the current possessed weapon

        //Get current powerup
        const Powerup* current_powerup = m_kart->getPowerup();
        PowerupManager::PowerupType possessed_item = current_powerup->getType();
        //TODO m_distance_ahead only refreshed when a position change.

        int weapon_interest = 0;

       if(possessed_item != 0)
        {
            weapon_interest = computeHitEstimation("weapon_hit_estimation.fcl",possessed_item,m_distance_ahead);
        }

  
#ifdef AI_DEBUG
        cout << "----------------------------------------" << endl;
        // Player evaluation
        cout << m_kart->getIdent() << " : player crashes = " << fuzzy_data_manager->getPlayerCrashCount() << endl;
        cout << m_kart->getIdent() << " : player av.rank = " << fuzzy_data_manager->getPlayerAverageRank() << endl;
        cout << m_kart->getIdent() << " : player evaluation = ";
        switch(eval)
        {
            case (1): cout << "Good!" << endl;   break;
            case (2): cout << "Average" << endl; break;
            case (3): cout << "Bad!" << endl;    break;
            default : cout << "unexpected value : " << eval << endl;
        } // end switch
        
        // Items
        cout << m_kart->getIdent() << " : items = " << allItems.size();
        for(int i=0; i < nitro_b.size(); i++)
            cout << ", big nitro : " << (nitro_b[i])->getXYZ()[1];
        for(int i=0; i < nitro_s.size(); i++)
            cout << ", smallnitro : " << (nitro_s[i])->getXYZ()[1];
        for(int i=0; i < maluses.size(); i++)
            cout << ", malus : " << (maluses[i])->getXYZ()[1];
        for(int i=0; i < boxes.size(); i++)
            cout << ", boxes : " << (boxes[i])->getXYZ()[1];
        cout << endl;
        
        // Agent nitro gauge
        cout << m_kart->getIdent() << " : nitro gauge = " << m_kart->getEnergy() << endl;
        // Agent powerup (possessed item/weapon)
        cout << m_kart->getIdent() << " : powerup count = " << m_kart->getNumPowerup() << endl;
        // Agent speed
        cout << m_kart->getIdent() << " : speed = " << m_kart->getSpeed() << endl;
        // Agent competitiveness
        cout << m_kart->getIdent() << " : agent current ranking = ";
        cout << current_ranking << endl;
        cout << m_kart->getIdent() << " : agent competitiveness = ";
         switch(competitiveness)
        {
            case (1): cout << "Competitive" << endl;   break;
            case (2): cout << "Not competitive" << endl; break;
            default : cout << "unexpected value : " << eval << endl;
        } // end switch
        cout << m_kart->getIdent() << " : agent agressiveness = ";
        switch(agressiveness)
        {
            case (1): cout << "Agressive" << endl;   break;
            case (2): cout << "Neutral" << endl; break;
            case (3): cout << "Careful" << endl; break;
            default : cout << "unexpected value : " << eval << endl;
        } // end switch

        //Agent hit estimation
        cout << m_kart->getIdent() << " : agent current powerup type = ";
        cout << possessed_item << endl;
        cout << m_kart->getIdent() << " : agent distance from ahead kart = ";
        cout << m_distance_ahead << endl;
        cout << m_kart->getIdent() << " : agent weapon hit difficulty = ";
        cout << weapon_interest << endl;


#endif

    }

}   // update

//------------------------------------------------------------------------------
/** Player evaluation computation method. Simply call computeFuzzyModel with the
 *  right parameters.
 *  TODO : make this comment doxygen compliant
 */

int FuzzyAIController::computePlayerEvaluation( const char* file_name,
	                                            int number_of_players,
                                                float player_average_rank,
                                                int   player_crash_count )

{
	//The rank of the player need to be normalized before computing

    float normalized_player_average_rank;

	if(number_of_players > 0)
	{
      normalized_player_average_rank = (player_average_rank*10)/number_of_players;
	}

    vector<float> evaluationParameters;
    evaluationParameters.push_back(normalized_player_average_rank);
    evaluationParameters.push_back(player_crash_count);


    return  (int)computeFuzzyModel(file_name, evaluationParameters);

}

//------------------------------------------------------------------------------
/** Driving style computation methods for competitiveness and agressiveness.
 *  Simply call computeFuzzyModel with the right parameters.
 *  TODO : make this comment doxygen compliant
 */

int FuzzyAIController::computeCompetitiveness(const char*    file_name, 
		                                   int   number_of_players,
                                           int player_level,
                                           int   current_ranking)

{
	//The rank need to be normalized before computing

    float normalized_current_ranking;

	if(number_of_players > 0)
	{
      normalized_current_ranking = (current_ranking*10)/number_of_players;
	}

    vector<float> evaluationParameters;
    evaluationParameters.push_back(player_level);
    evaluationParameters.push_back(current_ranking);

    return  (int)computeFuzzyModel(file_name, evaluationParameters);
}


int FuzzyAIController::computeAgressiveness(const char*    file_name, 
		                                   int   number_of_players,
                                           int kart_class,
                                           int   current_ranking)

{
	//The rank need to be normalized before computing

    float normalized_current_ranking;

	if(number_of_players > 0)
	{
      normalized_current_ranking = (current_ranking*10)/number_of_players;
	}

    vector<float> evaluationParameters;
    evaluationParameters.push_back(normalized_current_ranking);
    evaluationParameters.push_back(kart_class);

    return  (int)computeFuzzyModel(file_name, evaluationParameters);
}

//------------------------------------------------------------------------------
/** Path chooser computation methods when there are multiple path to choose. Simply call computeFuzzyModel with the
 *  right parameters.
 *  TODO : make this comment doxygen compliant
 *         Use the number of turns and the kart class.
 */


  PathData*  FuzzyAIController::computePathChooser(const char*    file_name, 
		                          const vector<vector<PathData*>*>* pathData,
                                  float   competitiveness)

 {
    float length;
    float bonuscount;
    vector<float> PathParameters;
    float interest;
    float best_interest = 0;
    PathData* bestPath;

    //Checking for each path.
   
    for (unsigned int i=0; i < pathData->size(); i++)
	{
         for (unsigned int j=0; j < pathData->at(i)->size(); j++)
	      {
              //Getting the length and bonus count

              length = pathData->at(i)->at(j)->pathLength;
              bonuscount = pathData->at(i)->at(j)->bonusCount;

              PathParameters.push_back(length);
              PathParameters.push_back(bonuscount);
              PathParameters.push_back(competitiveness);

              //Computing the interest for the path

              interest = computeFuzzyModel(file_name, PathParameters);

              PathParameters.clear();

              //Comparing the interest with the best interest

              if(interest > best_interest)
              {
                  //If the path has a better interest we store it

                  best_interest = interest;
                  bestPath = *&(pathData->at(i)->at(j));

              }

	      }
	}

    return  (bestPath);

 }

//------------------------------------------------------------------------------
/** Module to compute the difficulty to reach an object. Simply call computeFuzzyModel with the
 *  right parameters.
 *  TODO : make this comment doxygen compliant
 *         Use the direction?
 */


 float  FuzzyAIController::difficultyTagging(const char*    file_name, 
		                          float   distance,
                                  float   angle)

 {

    vector<float> ObjectParameters;
    ObjectParameters.push_back(distance);
    ObjectParameters.push_back(angle);

    return  computeFuzzyModel(file_name, ObjectParameters);

 }

//------------------------------------------------------------------------------
/** Module to compute the difficulty hit an opponent with a weapon. Simply call computeFuzzyModel with the
 *  right parameters.
 *  TODO : make this comment doxygen compliant
 *  Fuzzy model for each weapon?
 */

  int  FuzzyAIController::computeHitEstimation(const char* file_name,
                                               int          possessed_item_type,
                                               float        next_kart_distance)
  {

    float normalized_distance;

    //Check if the distance is too important for fuzzyfication.

    if (next_kart_distance > 30)
    {
        normalized_distance = 30;
    }
    else
    {
        normalized_distance = next_kart_distance;
    }


    int type;

    switch(possessed_item_type)
    {
        // TODO : use constant name like "ITEM_BANANA" instead of hard-coded values. (see items.hpp)
        
        
        //Weapon that don't need an opponent close.
        case 1 : //POWERUP_BUBBLEGUM
        case 4 : //POWERUP_ZIPPER
        case 6 : //POWERUP_SWITCH
        case 9 : //POWERUP_PARACHUTE
        case 10 : //POWERUP_ANVIL
        case 7 : //POWERUP_SWATTER

            type = 1;
            break;

        //Weapon that need a short distance for better result.
        case 2 : //POWERUP_CAKE
        case 3 : //POWERUP_BOWLING
        case 5 : //POWERUP_PLUNGER
        case 8 : //POWERUP_RUBBERBALL

            type = 2;
            break;

        default :

            type = 0;
    }

    vector<float> HitEstimation;
    HitEstimation.push_back(type);
    HitEstimation.push_back(normalized_distance);

     return  (int)computeFuzzyModel(file_name, HitEstimation);
  }

//------------------------------------------------------------------------------
/** Generic method to interface with FFLL and compute an output using fuzzy
 *  logic. The first given parameter is the .fcl file that FFLL has to use for
 *  the computation.
 *  The next parameters are the values that correspond to the parameters
 *  declared in the .fcl file (in the same order !).
 *  TODO : make this comment doxygen compliant
 */


float FuzzyAIController::computeFuzzyModel( const char*    file_name,
                                          vector<float>  parameters )
{
    // Create FFLL model. TODO : make this model a class static variable
	int model = ffll_new_model();

    string full_name = file_manager->getFclFile(file_name);
    // Load .fcl file
	int ret_val = (int) ffll_load_fcl_file(model, full_name.c_str());

    // If ffll_load_fcl_file returns an error
	if(ret_val < 0)
	{
		cout << "FFLL : Error opening .fcl file '" << file_name << "'" << endl; // TODO use fprintf(stderr, msg);
		return ret_val;
	}
    
    // Create a child FFLL model
	int child = ffll_new_child(model);

    // Set parameters value.
	for (size_t i=0, size=parameters.size(); i < size; i++)
	{
		ffll_set_value(model, child, i, parameters[i]); 
	}

    // Compute and return output
	return (float) ffll_get_output_value(model, child);
}

//-----------------------------------------------------------------------------
void FuzzyAIController::handleBraking()
{
    // In follow the leader mode, the kart should brake if they are ahead of
    // the leader (and not the leader, i.e. don't have initial position 1)
    if(race_manager->getMinorMode() == RaceManager::MINOR_MODE_FOLLOW_LEADER &&
        m_kart->getPosition() < m_world->getKart(0)->getPosition()           &&
        m_kart->getInitialPosition()>1                                         )
    {
        m_controls->m_brake = true;
        return;
    }
        
    const float MIN_SPEED = 5.0f;
    //We may brake if we are about to get out of the road, but only if the
    //kart is on top of the road, and if we won't slow down below a certain
    //limit.
    if (m_crashes.m_road && m_kart->getVelocityLC().getZ() > MIN_SPEED && 
        m_world->isOnRoad(m_kart->getWorldKartId()) )
    {
        float kart_ang_diff = 
            QuadGraph::get()->getAngleToNext(m_track_node,
                                         m_successor_index[m_track_node])
          - m_kart->getHeading();
        kart_ang_diff = normalizeAngle(kart_ang_diff);
        kart_ang_diff = fabsf(kart_ang_diff);

        // FIXME: The original min_track_angle value of 20 degrees
        // resulted in way too much braking. Is this test
        // actually necessary at all???
        const float MIN_TRACK_ANGLE = DEGREE_TO_RAD*60.0f;
        const float CURVE_INSIDE_PERC = 0.25f;

        //Brake only if the road does not goes somewhat straight.
        if(m_curve_angle > MIN_TRACK_ANGLE) //Next curve is left
        {
            //Avoid braking if the kart is in the inside of the curve, but
            //if the curve angle is bigger than what the kart can steer, brake
            //even if we are in the inside, because the kart would be 'thrown'
            //out of the curve.
            if(!(m_world->getDistanceToCenterForKart(m_kart->getWorldKartId()) 
                 > QuadGraph::get()->getNode(m_track_node).getPathWidth() *
                 -CURVE_INSIDE_PERC || 
                 m_curve_angle > RAD_TO_DEGREE*m_kart->getMaxSteerAngle()) )
            {
                m_controls->m_brake = false;
                return;
            }
        }
        else if( m_curve_angle < -MIN_TRACK_ANGLE ) //Next curve is right
        {
            if(!(m_world->getDistanceToCenterForKart( m_kart->getWorldKartId() ) 
                < QuadGraph::get()->getNode(m_track_node).getPathWidth() *
                 CURVE_INSIDE_PERC ||
                 m_curve_angle < -RAD_TO_DEGREE*m_kart->getMaxSteerAngle()))
            {
                m_controls->m_brake = false;
                return;
            }
        }

        //Brake if the kart's speed is bigger than the speed we need
        //to go through the curve at the widest angle, or if the kart
        //is not going straight in relation to the road.
        if(m_kart->getVelocityLC().getZ() > m_curve_target_speed ||
           kart_ang_diff          > MIN_TRACK_ANGLE         )
        {
#ifdef AI_DEBUG
        std::cout << "BRAKING" << std::endl;
#endif
            m_controls->m_brake = true;
            return;
        }

    }

    m_controls->m_brake = false;
}   // handleBraking

//-----------------------------------------------------------------------------
void FuzzyAIController::handleSteering(float dt)
{
    const int next = m_next_node_index[m_track_node];
    
    float steer_angle = 0.0f;

    /*The AI responds based on the information we just gathered, using a
     *finite state machine.
     */
    //Reaction to being outside of the road
    if( fabsf(m_world->getDistanceToCenterForKart( m_kart->getWorldKartId() ))  >
       0.5f* QuadGraph::get()->getNode(m_track_node).getPathWidth()+0.5f )
    {
        steer_angle = steerToPoint(QuadGraph::get()->getQuadOfNode(next)
                                                    .getCenter());

#ifdef AI_DEBUG
        m_debug_sphere->setPosition(QuadGraph::get()->getQuadOfNode(next)
                       .getCenter().toIrrVector());
        std::cout << "- Outside of road: steer to center point." <<
            std::endl;
#endif
    }
    //If we are going to crash against a kart, avoid it if it doesn't
    //drives the kart out of the road
    else if( m_crashes.m_kart != -1 && !m_crashes.m_road )
    {
        //-1 = left, 1 = right, 0 = no crash.
        if( m_start_kart_crash_direction == 1 )
        {
            steer_angle = steerToAngle(next, -M_PI*0.5f );
            m_start_kart_crash_direction = 0;
        }
        else if(m_start_kart_crash_direction == -1)
        {
            steer_angle = steerToAngle(next, M_PI*0.5f);
            m_start_kart_crash_direction = 0;
        }
        else
        {
            if(m_world->getDistanceToCenterForKart( m_kart->getWorldKartId() ) >
               m_world->getDistanceToCenterForKart( m_crashes.m_kart ))
            {
                steer_angle = steerToAngle(next, -M_PI*0.5f );
                m_start_kart_crash_direction = 1;
            }
            else
            {
                steer_angle = steerToAngle(next, M_PI*0.5f );
                m_start_kart_crash_direction = -1;
            }
        }

#ifdef AI_DEBUG
        std::cout << "- Velocity vector crashes with kart and doesn't " <<
            "crashes with road : steer 90 degrees away from kart." <<
            std::endl;
#endif

    }
    else
    {
        m_start_kart_crash_direction = 0;
        Vec3 straight_point;
        findNonCrashingPoint(&straight_point);
#ifdef AI_DEBUG
        m_debug_sphere->setPosition(straight_point.toIrrVector());
#endif
        steer_angle = steerToPoint(straight_point);
    }

    setSteering(steer_angle, dt);
}   // handleSteering

//-----------------------------------------------------------------------------
/** Handle all items depending on the chosen strategy: Either (low level AI)
 *  just use an item after 10 seconds, or do a much better job on higher level
 *  AI - e.g. aiming at karts ahead/behind, wait an appropriate time before 
 *  using multiple items etc.
 */
void FuzzyAIController::handleItems(const float dt)
{
    m_controls->m_fire = false;
    if(m_kart->playingEmergencyAnimation() || 
        m_kart->getPowerup()->getType() == PowerupManager::POWERUP_NOTHING ) 
        return;

    m_time_since_last_shot += dt;

    // Tactic 1: wait ten seconds, then use item
    // -----------------------------------------
    if(m_item_tactic==IT_TEN_SECONDS)
    {
        if( m_time_since_last_shot > 10.0f )
        {
            m_controls->m_fire = true;
            m_time_since_last_shot = 0.0f;
        }
        return;
    }

    // Tactic 2: calculate
    // -------------------
    switch( m_kart->getPowerup()->getType() )
    {
    case PowerupManager::POWERUP_BUBBLEGUM:
        // Avoid dropping all bubble gums one after another
        if( m_time_since_last_shot <3.0f) break;

        // Either use the bubble gum after 10 seconds, or if the next kart 
        // behind is 'close' but not too close (too close likely means that the
        // kart is not behind but more to the side of this kart and so won't 
        // be hit by the bubble gum anyway). Should we check the speed of the
        // kart as well? I.e. only drop if the kart behind is faster? Otoh 
        // this approach helps preventing an overtaken kart to overtake us 
        // again.
        m_controls->m_fire = (m_distance_behind < 15.0f &&
                              m_distance_behind > 3.0f    );
        break;   // POWERUP_BUBBLEGUM

    // All the thrown/fired items might be improved by considering the angle
    // towards m_kart_ahead.
    case PowerupManager::POWERUP_CAKE:
        {
            // Leave some time between shots
            if(m_time_since_last_shot<3.0f) break;
            // Since cakes can be fired all around, just use a sane distance
            // with a bit of extra for backwards, as enemy will go towards cake
            bool fire_backwards = (m_kart_behind && m_kart_ahead &&
                                   m_distance_behind < m_distance_ahead) ||
                                  !m_kart_ahead;
            float distance = fire_backwards ? m_distance_behind
                                            : m_distance_ahead;
            m_controls->m_fire = (fire_backwards && distance < 25.0f)  ||
                                 (!fire_backwards && distance < 20.0f);
            if(m_controls->m_fire)
                m_controls->m_look_back = fire_backwards;
            break;
        }   // POWERUP_CAKE

    case PowerupManager::POWERUP_BOWLING:
        {
            // Leave more time between bowling balls, since they are 
            // slower, so it should take longer to hit something which
            // can result in changing our target.
            if(m_time_since_last_shot < 5.0f) break;
            // Bowling balls are slower, so only fire on closer karts - but when
            // firing backwards, the kart can be further away, since the ball
            // acts a bit like a mine (and the kart is racing towards it, too)
            bool fire_backwards = (m_kart_behind && m_kart_ahead && 
                                   m_distance_behind < m_distance_ahead) ||
                                  !m_kart_ahead;
            float distance = fire_backwards ? m_distance_behind 
                                            : m_distance_ahead;
            m_controls->m_fire = ( (fire_backwards && distance < 30.0f)  ||
                                   (!fire_backwards && distance <10.0f)    ) &&
                                m_time_since_last_shot > 3.0f;
            if(m_controls->m_fire)
                m_controls->m_look_back = fire_backwards;
            break;
        }   // POWERUP_BOWLING

    case PowerupManager::POWERUP_ZIPPER:
        // Do nothing. Further up a zipper is used if nitro should be selected,
        // saving the (potential more valuable nitro) for later
        break;   // POWERUP_ZIPPER

    case PowerupManager::POWERUP_PLUNGER:
        {
            // Leave more time after a plunger, since it will take some
            // time before a plunger effect becomes obvious.
            if(m_time_since_last_shot < 5.0f) break;

            // Plungers can be fired backwards and are faster,
            // so allow more distance for shooting.
            bool fire_backwards = (m_kart_behind && m_kart_ahead && 
                                   m_distance_behind < m_distance_ahead) ||
                                  !m_kart_ahead;
            float distance      = fire_backwards ? m_distance_behind 
                                                 : m_distance_ahead;
            m_controls->m_fire  = distance < 30.0f                 || 
                                  m_time_since_last_shot > 10.0f;
            if(m_controls->m_fire)
                m_controls->m_look_back = fire_backwards;
            break;
        }   // POWERUP_PLUNGER

    case PowerupManager::POWERUP_SWITCH:
        // For now don't use a switch if this kart is first (since it's more 
        // likely that this kart then gets a good iteam), otherwise use it 
        // after a waiting an appropriate time
        if(m_kart->getPosition()>1 && 
            m_time_since_last_shot > stk_config->m_item_switch_time+2.0f)
            m_controls->m_fire = true;
        break;   // POWERUP_SWITCH

    case PowerupManager::POWERUP_PARACHUTE:
        // Wait one second more than a previous parachute
        if(m_time_since_last_shot > stk_config->m_parachute_time_other+1.0f)
            m_controls->m_fire = true;
        break;   // POWERUP_PARACHUTE

    case PowerupManager::POWERUP_ANVIL:
        // Wait one second more than a previous anvil
        if(m_time_since_last_shot < stk_config->m_anvil_time+1.0f) break;

        if(race_manager->getMinorMode()==RaceManager::MINOR_MODE_FOLLOW_LEADER)
        {
            m_controls->m_fire = m_world->getTime()<1.0f && 
                                 m_kart->getPosition()>2;
        }
        else
        {
            m_controls->m_fire = m_time_since_last_shot > 3.0f && 
                                 m_kart->getPosition()>1;
        }
        break;   // POWERUP_ANVIL

    case PowerupManager::POWERUP_SWATTER:
        {
            // Squared distance for which the swatter works
            float d2 = m_kart->getKartProperties()->getSwatterDistance2();
            // Fire if the closest kart ahead or to the back is not already 
            // squashed and close enough.
            // FIXME: this can be improved on, since more than one kart might 
            //        be hit, and a kart ahead might not be at an angle at 
            //        which the glove can be used.
            if(  ( m_kart_ahead && !m_kart_ahead->isSquashed()             &&
                    (m_kart_ahead->getXYZ()-m_kart->getXYZ()).length2()<d2 &&
                    m_kart_ahead->getSpeed() < m_kart->getSpeed()            ) ||
                 ( m_kart_behind && !m_kart_behind->isSquashed() &&
                    (m_kart_behind->getXYZ()-m_kart->getXYZ()).length2()<d2) )
                    m_controls->m_fire = true;
            break;
        }
    case PowerupManager::POWERUP_RUBBERBALL:
        // Perhaps some more sophisticated algorithm might be useful.
        // For now: fire if there is a kart ahead (which means that
        // this kart is certainly not the first kart)
        m_controls->m_fire = m_kart_ahead != NULL;
        break;
    default:
        printf("Invalid or unhandled powerup '%d' in fuzzy AI.\n",
                m_kart->getPowerup()->getType());
        assert(false);
    }
    if(m_controls->m_fire)  m_time_since_last_shot = 0.0f;
}   // handleItems

//-----------------------------------------------------------------------------
/** Determines the closest karts just behind and in front of this kart. The
 *  'closeness' is for now simply based on the position, i.e. if a kart is
 *  more than one lap behind or ahead, it is not considered to be closest.
 */
void FuzzyAIController::computeNearestKarts()
{
    bool need_to_check = false;
    int my_position    = m_kart->getPosition();
    // See if the kart ahead has changed:
    if( ( m_kart_ahead && m_kart_ahead->getPosition()+1!=my_position ) ||
        (!m_kart_ahead && my_position>1                              )    )
       need_to_check = true;
    // See if the kart behind has changed:
    if( ( m_kart_behind && m_kart_behind->getPosition()-1!=my_position   ) ||
        (!m_kart_behind && my_position<(int)m_world->getCurrentNumKarts())    )
        need_to_check = true;
    if(!need_to_check) return;

    m_kart_behind    = m_kart_ahead      = NULL;
    m_distance_ahead = m_distance_behind = 9999999.9f;
    float my_dist = m_world->getDistanceDownTrackForKart(m_kart->getWorldKartId());
    for(unsigned int i=0; i<m_world->getNumKarts(); i++)
    {
        Kart *k = m_world->getKart(i);
        if(k->isEliminated() || k->hasFinishedRace() || k==m_kart) continue;
        if(k->getPosition()==my_position+1) 
        {
            m_kart_behind = k;
            m_distance_behind = my_dist - m_world->getDistanceDownTrackForKart(i);
            if(m_distance_behind<0.0f)
                m_distance_behind += m_track->getTrackLength();
        }
        else 
            if(k->getPosition()==my_position-1)
            {
                m_kart_ahead = k;
                m_distance_ahead = m_world->getDistanceDownTrackForKart(i) - my_dist;
                if(m_distance_ahead<0.0f)
                    m_distance_ahead += m_track->getTrackLength();
            }
    }   // for i<world->getNumKarts()
}   // computeNearestKarts

//-----------------------------------------------------------------------------
void FuzzyAIController::handleAcceleration( const float dt)
{
    //Do not accelerate until we have delayed the start enough
    if( m_start_delay > 0.0f )
    {
        m_start_delay -= dt;
        m_controls->m_accel = 0.0f;
        return;
    }

    if( m_controls->m_brake == true )
    {
        m_controls->m_accel = 0.0f;
        return;
    }

    if(m_kart->hasViewBlockedByPlunger())
    {
        if(!(m_kart->getSpeed() > m_kart->getCurrentMaxSpeed() / 2))
            m_controls->m_accel = 0.05f;
        else 
            m_controls->m_accel = 0.0f;
        return;
    }
    

    // FIXME: this needs to be rewritten, it doesn't make any sense:
    // wait for players triggers the opposite (if a player is ahead
    // of this AI, go full speed). Besides, it's going to use full
    // speed anyway.
    if( m_wait_for_players )
    {
        //Find if any player is ahead of this kart
        bool player_winning = false;
        for(unsigned int i = 0; i < race_manager->getNumPlayers(); ++i )
            if( m_kart->getPosition() > m_world->getPlayerKart(i)->getPosition() )
            {
                player_winning = true;
                break;
            }

        if( player_winning )
        {
            m_controls->m_accel = m_max_handicap_speed;
            return;
        }
    }

    m_controls->m_accel = stk_config->m_ai_acceleration;
}   // handleAcceleration

//-----------------------------------------------------------------------------
void FuzzyAIController::handleRaceStart()
{
    if( m_start_delay <  0.0f )
    {
        // Each kart starts at a different, random time, and the time is
        // smaller depending on the difficulty.
        m_start_delay = m_min_start_delay 
                      + (float) rand() / RAND_MAX * (m_max_start_delay-m_min_start_delay);

        // Now check for a false start. If so, add 1 second penalty time.
        if(rand() < RAND_MAX * m_false_start_probability)
        {
            m_start_delay+=stk_config->m_penalty_time;
            return;
        }
    }
}   // handleRaceStart

//-----------------------------------------------------------------------------
void FuzzyAIController::handleRescue(const float dt)
{
    // check if kart is stuck
    if(m_kart->getSpeed()<2.0f && !m_kart->playingEmergencyAnimation() && 
        !m_world->isStartPhase())
    {
        m_time_since_stuck += dt;
        if(m_time_since_stuck > 2.0f)
        {
            m_kart->forceRescue();
            m_time_since_stuck=0.0f;
        }   // m_time_since_stuck > 2.0f
    }
    else
    {
        m_time_since_stuck = 0.0f;
    }
}   // handleRescue

//-----------------------------------------------------------------------------
/** Decides wether to use nitro or not.
 */
void FuzzyAIController::handleNitroAndZipper()
{
    m_controls->m_nitro = false;
    // If we are already very fast, save nitro.
    if(m_kart->getSpeed() > 0.95f*m_kart->getCurrentMaxSpeed())
        return;
    // Don't use nitro when the AI has a plunger in the face!
    if(m_kart->hasViewBlockedByPlunger()) return;
    
    // Don't use nitro if the kart doesn't have any or is not on ground.
    if(!m_kart->isOnGround() || m_kart->hasFinishedRace()) return;
    
    // Don't compute nitro usage if we don't have nitro or are not supposed
    // to use it, and we don't have a zipper or are not supposed to use
    // it (calculated).
    if( (m_kart->getEnergy()==0 || m_nitro_level==NITRO_NONE)  &&
        (m_kart->getPowerup()->getType()!=PowerupManager::POWERUP_ZIPPER ||
          m_item_tactic==IT_TEN_SECONDS                                    ) )
        return;

    // If a parachute or anvil is attached, the nitro doesn't give much
    // benefit. Better wait till later.
    const bool has_slowdown_attachment = 
        m_kart->getAttachment()->getType()==Attachment::ATTACH_PARACHUTE ||
        m_kart->getAttachment()->getType()==Attachment::ATTACH_ANVIL;
    if(has_slowdown_attachment) return;

    // If the kart is very slow (e.g. after rescue), use nitro
    if(m_kart->getSpeed()<5)
    {
        m_controls->m_nitro = true;
        return;
    }

    // If this kart is the last kart, and we have enough 
    // (i.e. more than 2) nitro, use it.
    // -------------------------------------------------
    const unsigned int num_karts = m_world->getCurrentNumKarts();
    if(m_kart->getPosition()== (int)num_karts && m_kart->getEnergy()>2.0f)
    {
        m_controls->m_nitro = true;
        return;
    }

    // On the last track shortly before the finishing line, use nitro 
    // anyway. Since the kart is faster with nitro, estimate a 50% time
    // decrease (additionally some nitro will be saved when top speed
    // is reached).
    if(m_world->getLapForKart(m_kart->getWorldKartId())==race_manager->getNumLaps()-1 &&
        m_nitro_level == NITRO_ALL)
    {
        float finish = m_world->getEstimatedFinishTime(m_kart->getWorldKartId());
        if( 1.5f*m_kart->getEnergy() >= finish - m_world->getTime() )
        {
            m_controls->m_nitro = true;
            return;
        }
    }

    // A kart within this distance is considered to be overtaking (or to be
    // overtaken).
    const float overtake_distance = 10.0f;

    // Try to overtake a kart that is close ahead, except 
    // when we are already much faster than that kart
    // --------------------------------------------------
    if(m_kart_ahead                                       && 
        m_distance_ahead < overtake_distance              &&
        m_kart_ahead->getSpeed()+5.0f > m_kart->getSpeed()   )
    {
            m_controls->m_nitro = true;
            return;
    }

    if(m_kart_behind                                   &&
        m_distance_behind < overtake_distance          &&
        m_kart_behind->getSpeed() > m_kart->getSpeed()    )
    {
        // Only prevent overtaking on highest level
        m_controls->m_nitro = m_nitro_level==NITRO_ALL;
        return;
    }
    
}   // handleNitroAndZipper

//-----------------------------------------------------------------------------
void FuzzyAIController::checkCrashes(int steps, const Vec3& pos )
{
    //Right now there are 2 kind of 'crashes': with other karts and another
    //with the track. The sight line is used to find if the karts crash with
    //each other, but the first step is twice as big as other steps to avoid
    //having karts too close in any direction. The crash with the track can
    //tell when a kart is going to get out of the track so it steers.
    m_crashes.clear();

    // If slipstream should be handled actively, trigger overtaking the
    // kart which gives us slipstream if slipstream is ready
    const SlipStream *slip=m_kart->getSlipstream();
    if(m_make_use_of_slipstream && slip->isSlipstreamReady() &&
        slip->getSlipstreamTarget())
    {
        //printf("%s overtaking %s\n", m_kart->getIdent().c_str(),
        //    m_kart->getSlipstreamKart()->getIdent().c_str());
        // FIXME: we might define a minimum distance, and if the target kart
        // is too close break first - otherwise the AI hits the kart when
        // trying to overtake it, actually speeding the other kart up.
        m_crashes.m_kart = slip->getSlipstreamTarget()->getWorldKartId();
    }

    const size_t NUM_KARTS = m_world->getNumKarts();

    //Protection against having vel_normal with nan values
    const Vec3 &VEL = m_kart->getVelocity();
    Vec3 vel_normal(VEL.getX(), 0.0, VEL.getZ());
    float speed=vel_normal.length();
    // If the velocity is zero, no sense in checking for crashes in time
    if(speed==0) return;

    // Time it takes to drive for m_kart_length units.
    float dt = m_kart_length / speed; 
    vel_normal/=speed;

    int current_node = m_track_node;
    if(steps<1 || steps>1000)
    {
        printf("Warning, incorrect STEPS=%d. kart_length %f velocity %f\n",
            steps, m_kart_length, m_kart->getVelocityLC().getZ());
        steps=1000;
    }
    for(int i = 1; steps > i; ++i)
    {
        Vec3 step_coord = pos + vel_normal* m_kart_length * float(i);

        /* Find if we crash with any kart, as long as we haven't found one
         * yet
         */
        if( m_crashes.m_kart == -1 )
        {
            for( unsigned int j = 0; j < NUM_KARTS; ++j )
            {
                const Kart* kart = m_world->getKart(j);
                if(kart==m_kart||kart->isEliminated()) continue;   // ignore eliminated karts
                const Kart *other_kart = m_world->getKart(j);
                // Ignore karts ahead that are faster than this kart.
                if(m_kart->getVelocityLC().getZ() < other_kart->getVelocityLC().getZ())
                    continue;
                Vec3 other_kart_xyz = other_kart->getXYZ() + other_kart->getVelocity()*(i*dt);
                float kart_distance = (step_coord - other_kart_xyz).length_2d();

                if( kart_distance < m_kart_length)
                    m_crashes.m_kart = j;
            }
        }

        /*Find if we crash with the drivelines*/
        if(current_node!=QuadGraph::UNKNOWN_SECTOR &&
            m_next_node_index[current_node]!=-1)
            QuadGraph::get()->findRoadSector(step_coord, &current_node,
                        /* sectors to test*/ &m_all_look_aheads[current_node]);

        if( current_node == QuadGraph::UNKNOWN_SECTOR)
        {
            m_crashes.m_road = true;
            return;
        }
    }
}   // checkCrashes

//-----------------------------------------------------------------------------
/** Find the sector that at the longest distance from the kart, that can be
 *  driven to without crashing with the track, then find towards which of
 *  the two edges of the track is closest to the next curve after wards,
 *  and return the position of that edge.
 */
void FuzzyAIController::findNonCrashingPoint(Vec3 *result)
{    
    unsigned int sector = m_next_node_index[m_track_node];
    int target_sector;

    Vec3 direction;
    Vec3 step_track_coord;

    // The original while(1) loop is replaced with a for loop to avoid
    // infinite loops (which we had once or twice). Usually the number
    // of iterations in the while loop is less than 7.
    for(unsigned int i=0; i<100; i++)
    {
        //target_sector is the sector at the longest distance that we can drive
        //to without crashing with the track.
        target_sector = m_next_node_index[sector];

        //direction is a vector from our kart to the sectors we are testing
        direction = QuadGraph::get()->getQuadOfNode(target_sector).getCenter()
                  - m_kart->getXYZ();

        float len=direction.length_2d();
        unsigned int steps = (unsigned int)( len / m_kart_length );
        if( steps < 3 ) steps = 3;

        // That shouldn't happen, but since we had one instance of
        // STK hanging, add an upper limit here (usually it's at most
        // 20 steps)
        if( steps>1000) steps = 1000;

        //Protection against having vel_normal with nan values
        if(len>0.0f) {
            direction*= 1.0f/len;
        }

        Vec3 step_coord;
        //Test if we crash if we drive towards the target sector
        for(unsigned int i = 2; i < steps; ++i )
        {
            step_coord = m_kart->getXYZ()+direction*m_kart_length * float(i);

            QuadGraph::get()->spatialToTrack(&step_track_coord, step_coord,
                                             sector );
 
            float distance = fabsf(step_track_coord[0]);

            //If we are outside, the previous sector is what we are looking for
            if ( distance + m_kart_width * 0.5f 
                 > QuadGraph::get()->getNode(sector).getPathWidth() )
            {
                *result = QuadGraph::get()->getQuadOfNode(sector).getCenter();
                return;
            }
        }
        sector = target_sector;
    }   // for i<100
    *result = QuadGraph::get()->getQuadOfNode(sector).getCenter();
}   // findNonCrashingPoint

//-----------------------------------------------------------------------------
/** calc_steps() divides the velocity vector by the lenght of the kart,
 *  and gets the number of steps to use for the sight line of the kart.
 *  The calling sequence guarantees that m_future_sector is not UNKNOWN.
 */
int FuzzyAIController::calcSteps()
{
    int steps = int( m_kart->getVelocityLC().getZ() / m_kart_length );
    if( steps < m_min_steps ) steps = m_min_steps;

    //Increase the steps depending on the width, if we steering hard,
    //mostly for curves.
#if 0
    // FIXME: I don't understand this: if we are steering hard, we check
    //        for more steps if we hit another kart?? If we steer hard,
    //        the approximation used (pos + velocity*dt) will be even
    //        worse, since it doesn't take steering into account.
    if( fabsf(m_controls->m_steer) > 0.95 )
    {
        const int WIDTH_STEPS = 
            (int)( QuadGraph::get()->getNode(m_future_sector).getPathWidth()
                   /( m_kart_length * 2.0 ) );

        steps += WIDTH_STEPS;
    }
#endif
    // The AI is driving significantly better with more steps, so for now
    // add 5 additional steps.
    return steps+5;
}   // calcSteps

//-----------------------------------------------------------------------------
/**FindCurve() gathers info about the closest sectors ahead: the curve
 * angle, the direction of the next turn, and the optimal speed at which the
 * curve can be travelled at it's widest angle.
 *
 * The number of sectors that form the curve is dependant on the kart's speed.
 */
void FuzzyAIController::findCurve()
{
    float total_dist = 0.0f;
    int i;
    for(i = m_track_node; total_dist < m_kart->getVelocityLC().getZ(); 
        i = m_next_node_index[i])
    {
        total_dist += QuadGraph::get()->getDistanceToNext(i, 
                                                         m_successor_index[i]);
    }


    m_curve_angle = 
        normalizeAngle(QuadGraph::get()->getAngleToNext(i, 
                                                        m_successor_index[i])
                      -QuadGraph::get()->getAngleToNext(m_track_node, 
                                            m_successor_index[m_track_node]) );
    
    m_curve_target_speed = m_kart->getCurrentMaxSpeed();
}   // findCurve
