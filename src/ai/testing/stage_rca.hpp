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
 * candidate action evaluator
 */

#ifndef AI_TESTING_STAGE_RCA_HPP_INCLUDED
#define AI_TESTING_STAGE_RCA_HPP_INCLUDED

#include "../composite/rca.hpp"
#include "../composite/stage.hpp"

#include <queue>

#ifdef _MSC_VER
#pragma warning(push)
//silence "inherits via dominance" warnings
#pragma warning(disable:4250)
#endif

namespace ai {

namespace testing_ai_default {

class decision;
class turn_state;

class candidate_action_evaluation_loop: public virtual stage, public virtual rca_context {
public:
	candidate_action_evaluation_loop( ai_context &context, const config &cfg );

	~candidate_action_evaluation_loop();

	bool do_play_stage();

	void on_create();

	config to_config() const;

	rca_context& get_rca_context();

	void create_candidate_action(std::vector<candidate_action_ptr> &candidate_actions, const config &cfg);

	void remove_completed_cas();

	// Functions for dynamic programming.
	void make_decision();

	void simulate_states_ahead();

	void set_optimal_policy();

	const turn_state simulate_state(const int decision_no_, turn_state &state);

private:
	std::vector<candidate_action_ptr> candidate_actions_;

	const config &cfg_;

	std::queue<turn_state> states_;
};

class decision
{
	friend std::ostream& operator<<(std::ostream&, const decision&);

private:
	int decision_no_;

public:
	static const int total_decisions = 2; // We have two kinds of decisions now.

	decision(int decision_no_) : decision_no_(decision_no_)
	{
	}

	~decision();

	int get_decision_no() const { return decision_no_; }

	decision& set_decision_no(int decision_no_) { this->decision_no_ = decision_no_; return *this; }

	bool is_valid() const { return (decision_no_ >= 0 && decision_no_ < total_decisions); }
};

class turn_state
{
private:
	const int own_side_;

	const int turn_no_;

	double state_score_;

	const unit_map units_;

	const std::vector<team> teams_;

	decision decision_;

public:
	turn_state(const int own_side_, const int turn_no_, const unit_map &units_, const std::vector<team> &teams_);

	~turn_state();

	void scoring_state();

	int get_turn_no() const { return turn_no_; }

	double get_state_score() const { return state_score_; } //Should I check this?

	const unit_map& get_units() const { return units_; }

	const std::vector<team>& get_teams() const { return teams_; }

	const decision& get_decision() const { return decision_; }

	void set_decision(int decision_no_) { decision_.set_decision_no(decision_no_); }
};

} // of namespace testing_ai_default

} // of namespace ai

#ifdef _MSC_VER
#pragma warning(pop)
#endif

#endif
