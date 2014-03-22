/*
   Copyright (C) 2009 - 2014 by Aline Riss <aline.riss@gmail.com>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

/**
* @file
* Attack spearman
*/

#include "attack_spearman.hpp"
#include "../actions.hpp"
#include "../contexts.hpp"
#include "../manager.hpp"
#include "../composite/engine.hpp"
#include "../composite/rca.hpp"
#include "../composite/stage.hpp"
#include "../../gamestatus.hpp"
#include "../../log.hpp"
#include "../../map.hpp"
#include "../../resources.hpp"
#include "../../team.hpp"
#include "../../pathfind/pathfind.hpp"
#include "../../pathfind/teleport.hpp"

#include <boost/foreach.hpp>

#include <numeric>

static lg::log_domain log_ai_kevin("ai/kevin");
#define DBG_AI_KEVIN LOG_STREAM(debug, log_ai_kevin)
#define LOG_AI_KEVIN LOG_STREAM(info, log_ai_kevin)
#define WRN_AI_KEVIN LOG_STREAM(warn, log_ai_kevin)
#define ERR_AI_KEVIN LOG_STREAM(err, log_ai_kevin)

#ifdef _MSC_VER
#pragma warning(push)
//silence "inherits via dominance" warnings
#pragma warning(disable:4250)
#endif

namespace ai
{

namespace kevin_ai_pieces
{

attack_spearman::attack_spearman( rca_context &context, const config &cfg ) : candidate_action(context,cfg)
{
}

attack_spearman::~attack_spearman()
{
}

double attack_spearman::evaluate()
{
    return HIGH_SCORE;  // This CA will always be considered first
}

void attack_spearman::execute()
{
    const move_map possible_moves = get_dstsrc();
    const move_map enemy_possible_moves = get_enemy_dstsrc();
    const unit_map & units_ = *resources::units;

    const unit_map::const_iterator leader = units_.find_leader(get_side());
    if(leader == units_.end()){
        return;
    }

    for(unit_map::const_iterator i = units_.begin(); i != units_.end(); ++i)
    {
        if(current_team().is_enemy(i->side()) && i->type_name().str() == "Spearman" )    //t_string should overload the ==, why directly compare to a string fail?
        {
            map_location adjacent_tiles[6];
            get_adjacent_tiles(i->get_location(), adjacent_tiles);

            double min_power_projection = 1000.0;   //magic number here, find if there is a #define for this
            std::pair<map_location, map_location> best_movement;
            LOG_AI_KEVIN<<possible_moves.size()<<std::endl;
            LOG_AI_KEVIN<<enemy_possible_moves.size()<<std::endl;

            for(size_t n = 0; n != 6; ++n)
            {
                typedef move_map::const_iterator Itor;
                std::pair<Itor, Itor> range = possible_moves.equal_range(adjacent_tiles[n]);
                LOG_AI_KEVIN<<adjacent_tiles[n]<<std::endl;

                while(range.first != range.second)
                {
                    const map_location& dst = range.first->first;

                    // find out the minimum power projection location
                    double power_here = power_projection(dst, enemy_possible_moves);
                    LOG_AI_KEVIN<<"power here:"<<dst<<":"<<power_here<<std::endl;

                    // maintain the best choose
                    if(power_here < min_power_projection)
                    {
                        min_power_projection = power_here;
                        best_movement = *range.first;
                    }

                    ++range.first;
                }
            }

            map_location from   = best_movement.second;
            map_location to     = best_movement.first;
            map_location target_loc = i->get_location();

            LOG_AI_KEVIN<<from<<"\t"<<to<<"\t"<<target_loc<<std::endl;

            if (from!=to)
            {
                move_result_ptr move_res = execute_move_action(from,to,false);
                if (!move_res->is_ok())
                {
                    LOG_AI_KEVIN << get_name() << "::execute not ok, move failed" << std::endl;
                    return;
                }
            }

            attack_result_ptr attack_res = check_attack_action(to, target_loc, -1);
            if (!attack_res->is_ok())
            {
                LOG_AI_KEVIN << get_name() << "::execute not ok, attack cancelled" << std::endl;
            }
            else
            {
                attack_res->execute();
                if (!attack_res->is_ok())
                {
                    LOG_AI_KEVIN << get_name() << "::execute not ok, attack failed" << std::endl;
                }
            }
        }
    }
}

} //end of kevin_ai_pieces

} // end of namespace ai
