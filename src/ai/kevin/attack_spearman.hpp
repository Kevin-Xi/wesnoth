/*
   Copyright (C) 2009 - 2014 by Yurii Chernyi <terraninfo@terraninfo.net>
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

#ifndef AI_KEVIN_ATTACK_SPEARMAN_HPP_INCLUDED
#define AI_KEVIN_ATTACK_SPEARMAN_HPP_INCLUDED

#include "../composite/rca.hpp"
#include "../../team.hpp"

#ifdef _MSC_VER
#pragma warning(push)
//silence "inherits via dominance" warnings
#pragma warning(disable:4250)
#endif

namespace ai {


namespace kevin_ai_pieces {


class attack_spearman : public candidate_action {
public:

	attack_spearman( rca_context &context, const config &cfg );

	virtual ~attack_spearman();

	virtual double evaluate();

	virtual void execute();

//private:
//	attack_analysis best_analysis_;
//	double choice_rating_;


};


} // of namespace kevin_ai_pieces


} // of namespace ai

#ifdef _MSC_VER
#pragma warning(pop)
#endif

#endif

