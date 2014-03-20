/*
 * kevin_ai
 * A toy AI of wesnoth, use for test and familiar
 * Strategy: find the best location to attack enemy nearby
 */

#include "ai.hpp"

#include "../actions.hpp"
#include "../game_info.hpp"
#include "../manager.hpp"
#include "../contexts.hpp"

#include "../../attack_prediction.hpp"
#include "../../actions/attack.hpp"

#include "../../game_events/pump.hpp"
#include "../../gamestatus.hpp"
#include "../../log.hpp"
#include "../../map_location.hpp"

#include "../../resources.hpp"

#include "../../unit_display.hpp"
#include "../../wml_exception.hpp"

#include <map.hpp>

#include <stdio.h>

#include <iterator>
#include <algorithm>

// log
static lg::log_domain log_ai("ai/general");
#define DBG_AI LOG_STREAM(debug, log_ai)
#define LOG_AI LOG_STREAM(info, log_ai)
#define WRN_AI LOG_STREAM(warn, log_ai)
#define ERR_AI LOG_STREAM(err, log_ai)

#ifdef _MSC_VER
#pragma warning(push)
//silence "inherits via dominance" warnings
#pragma warning(disable:4250)
#endif

namespace ai
{

typedef move_map::const_iterator Itor;  // itor of a map of possible moves
typedef std::multimap<map_location,int>::iterator locItor;  // itor of locations

//-----------Implement----------------

kevin_ai::kevin_ai(default_ai_context &context, const config& cfg):ai_composite(context, cfg), units_(resources::units) {}

std::string kevin_ai::describe_self() const
{
    return "[kevin_ai]";
}


void kevin_ai::new_turn()
{
    LOG_AI << "kevin_ai: new turn" << std::endl;
}

void kevin_ai::on_create()
{
    LOG_AI << "kevin_ai: create" << std::endl;
}

config kevin_ai::to_config() const
{
    config cfg;
    cfg["ai_algorithm"]= "kevin_ai";
    return cfg;
}

void kevin_ai::switch_side(side_number side)
{
    LOG_AI << "kevin_ai new side: " << side << std::endl;
}

void kevin_ai::play_turn()
{
    game_events::fire("ai turn");
    do_attacks();
}

void kevin_ai::do_attacks()
{
    std::map<map_location, pathfind::paths> possible_moves; // record path-dst
    move_map srcdst, dstsrc;    // record unit-all possible dst
                                // record dst-all possible unit
    calculate_possible_moves(possible_moves, srcdst, dstsrc, false);   // calculate the moves units may possibly make, on AI's side
    const unit_map & units_ = *resources::units;    // units-locations
    gamemap &map_ = *resources::game_map;

    for(unit_map::const_iterator i = units_.begin(); i != units_.end(); ++i)
    {
        if(current_team().is_enemy(i->side()))
        {
            // get enemy's adjacent hexes
            map_location adjacent_tiles[6];
            get_adjacent_tiles(i->get_location(), adjacent_tiles);
            
            // find the best location to attack
            int best_defense = -1;  // [1, 100]
            std::pair<map_location, map_location> best_movement;    // record dst-src of the best location

            for(size_t n = 0; n != 6; ++n)
            {
                typedef move_map::const_iterator Itor;
                std::pair<Itor, Itor> range = dstsrc.equal_range(adjacent_tiles[n]);  // record all moves that make units to enemy's adjacent hexes

                // alias for convenient
                while(range.first != range.second)
                {
                    const map_location& dst = range.first->first;
                    const map_location& src = range.first->second;
                    const unit_map::const_iterator un = units_.find(src);   // record all units can make that hit


                    // TODO: update the wikipage http://wiki.wesnoth.org/WritingYourOwnAI
                    // figure out the best defense location
                    const t_translation::t_terrain terrain = map_.get_terrain(dst);
                    const int chance_to_hit = un->movement_type().defense_modifier(terrain);

                    // maintain the best choose
                    if(best_defense == -1 || chance_to_hit < best_defense)
                    {
                        best_defense = chance_to_hit;
                        best_movement = *range.first;
                    }

                    ++range.first;
                }
            }

            // if the best move is found, do that move and attack
            if(best_defense != -1)
            {
                bool gamestate_changed = false;
                bool do_attack = false;
                if (best_movement.first != best_movement.second)
                {
                    move_result_ptr move_res = execute_move_action(best_movement.second, best_movement.first, true);
                    gamestate_changed |= move_res->is_gamestate_changed();
                    if (move_res->is_ok())
                    {
                        do_attack = true;
                    }
                    else
                    {
                        LOG_AI << "move_failed" << std::endl;
                    }
                }
                else
                {
                    do_attack = true;
                }
                if (do_attack)
                {
                    attack_result_ptr attack_res = execute_attack_action(best_movement.first,i->get_location(),-1);
                    gamestate_changed |= attack_res->is_gamestate_changed();
                    if (!attack_res->is_ok())
                    {
                        LOG_AI << "attack failed" << std::endl;
                    }
                }

                if (gamestate_changed)
                {
                    do_attacks();
                }
                return;
            }
        }
    }
}

#ifdef _MSC_VER
#pragma warning(pop)
#endif


} //end of namespace ai

