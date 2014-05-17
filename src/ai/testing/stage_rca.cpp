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
 * Candidate actions evaluator
 * @file
 */

#include "stage_rca.hpp"

#include "../manager.hpp"
#include "../composite/ai.hpp"
#include "../composite/engine.hpp"
#include "../composite/property_handler.hpp"
#include "../gamestate_observer.hpp"
#include "../../log.hpp"
#include "../../resources.hpp"
#include "../../tod_manager.hpp"
#include "../../unit.hpp"
#include "../../team.hpp"

#include <boost/bind.hpp>
#include <boost/foreach.hpp>

namespace ai {

namespace testing_ai_default {

static lg::log_domain log_ai_testing_rca_default("ai/stage/rca");
#define DBG_AI_TESTING_RCA_DEFAULT LOG_STREAM(debug, log_ai_testing_rca_default)
#define LOG_AI_TESTING_RCA_DEFAULT LOG_STREAM(info, log_ai_testing_rca_default)
#define ERR_AI_TESTING_RCA_DEFAULT LOG_STREAM(err, log_ai_testing_rca_default)

/*void make_decision(int own_side_);
void simulate_states_ahead(int own_side_, std::queue<turn_state> &states_);
void set_optimal_policy(std::queue<turn_state> &states_);
const turn_state simulate_state(const int own_side_, const int decision_no_, const turn_state &state_);*/

candidate_action_evaluation_loop::candidate_action_evaluation_loop( ai_context &context, const config &cfg)
	: stage(context,cfg)
	, candidate_actions_()
	, cfg_(cfg)
{
}

void candidate_action_evaluation_loop::on_create()
{
	//init the candidate actions
	BOOST_FOREACH(const config &cfg_element, cfg_.child_range("candidate_action")){
		engine::parse_candidate_action_from_config(*this,cfg_element,back_inserter(candidate_actions_));
	}

	boost::function2<void, std::vector<candidate_action_ptr>&, const config&> factory_candidate_actions =
		boost::bind(&testing_ai_default::candidate_action_evaluation_loop::create_candidate_action,*this,_1,_2);

	register_vector_property(property_handlers(),"candidate_action",candidate_actions_, factory_candidate_actions);

}

void candidate_action_evaluation_loop::create_candidate_action(std::vector<candidate_action_ptr> &candidate_actions, const config &cfg)
{
	engine::parse_candidate_action_from_config(*this,cfg,std::back_inserter(candidate_actions));
}


config candidate_action_evaluation_loop::to_config() const
{
	config cfg = stage::to_config();
	BOOST_FOREACH(candidate_action_ptr ca, candidate_actions_){
		cfg.add_child("candidate_action",ca->to_config());
	}
	return cfg;
}


class desc_sorter_of_candidate_actions {
public:
	bool operator()(const candidate_action_ptr &a, const candidate_action_ptr &b)
	{
		return a->get_max_score() > b->get_max_score();
	}
};

bool candidate_action_evaluation_loop::do_play_stage()
{
	LOG_AI_TESTING_RCA_DEFAULT << "Starting candidate action evaluation loop for side "<< get_side() << std::endl;

	BOOST_FOREACH(candidate_action_ptr ca, candidate_actions_){
		ca->enable();
	}

	//sort candidate actions by max_score DESC
	std::sort(candidate_actions_.begin(),candidate_actions_.end(),desc_sorter_of_candidate_actions());

	//Make general 'attack' or 'defence' decision for current turn
	//utilizing dynamic programming.
	int ticks = SDL_GetTicks();

	make_decision();

	int time_taken = SDL_GetTicks() - ticks;
	LOG_AI_TESTING_RCA_DEFAULT <<"Took " << time_taken <<"ticks on decision making." << std::endl;

	bool executed = false;
	bool gamestate_changed = false;
	do {
		executed = false;
		double best_score = candidate_action::BAD_SCORE;
		candidate_action_ptr best_ptr;

		//Evaluation
		BOOST_FOREACH(candidate_action_ptr ca_ptr, candidate_actions_){
			if (!ca_ptr->is_enabled()){
				DBG_AI_TESTING_RCA_DEFAULT << "Skipping disabled candidate action: "<< *ca_ptr << std::endl;
				continue;
			}

			if (ca_ptr->get_max_score()<=best_score) {
				DBG_AI_TESTING_RCA_DEFAULT << "Ending candidate action evaluation loop because current score "<<best_score<<" is greater than the upper bound of score for remaining candidate actions "<< ca_ptr->get_max_score()<< std::endl;
				break;
			}

			DBG_AI_TESTING_RCA_DEFAULT << "Evaluating candidate action: "<< *ca_ptr << std::endl;
			double score = ca_ptr->evaluate();
			DBG_AI_TESTING_RCA_DEFAULT << "Evaluated candidate action to score "<< score << " : " << *ca_ptr << std::endl;

			if (score>best_score) {
				best_score = score;
				best_ptr = ca_ptr;
			}
		}

		//Execution
		if (best_score>candidate_action::BAD_SCORE) {
			DBG_AI_TESTING_RCA_DEFAULT << "Executing best candidate action: "<< *best_ptr << std::endl;
			gamestate_observer gs_o;
			best_ptr->execute();
			executed = true;
			if (!gs_o.is_gamestate_changed()) {
				//this means that this CA has lied to us in evaluate()
				//we punish it by disabling it
				DBG_AI_TESTING_RCA_DEFAULT << "Disabling candidate action because it failed to change the game state: "<< *best_ptr << std::endl;
				best_ptr->disable();
				//since we don't re-enable at this play_stage, if we disable this CA, other may get the chance to go.
			} else {
				gamestate_changed = true;
			}
		} else {
			LOG_AI_TESTING_RCA_DEFAULT << "Ending candidate action evaluation loop due to best score "<< best_score<<"<="<< candidate_action::BAD_SCORE<<std::endl;
		}
	} while (executed);
	LOG_AI_TESTING_RCA_DEFAULT << "Ended candidate action evaluation loop for side "<< get_side() << std::endl;
	remove_completed_cas();
	return gamestate_changed;
}

void candidate_action_evaluation_loop::remove_completed_cas()
{
	std::vector<size_t> tbr; // indexes of elements to be removed

	for (size_t i = 0; i != candidate_actions_.size(); ++i)
	{
		if (candidate_actions_[i]->to_be_removed())
		{
			tbr.push_back(i); // so we fill the array with the indexes
		}
	}

	for (size_t i = 0; i != tbr.size(); ++i)
	{
		// we should go downwards, so that index shifts don't affect us
		size_t index = tbr.size() - i - 1; // downcounting for is not possible using unsigned counters, so we hack around
		std::string path = "stage[" + this->get_id() + "].candidate_action[" + candidate_actions_[tbr[index]]->get_name() + "]";

		config cfg = config();
		cfg["path"] = path;
		cfg["action"] = "delete";

		ai::manager::modify_active_ai_for_side(this->get_side(), cfg); // we remove the CA
	}


// @note: this code might be more convenient, but is obviously faulty and incomplete, because of iterator invalidation rules
//	  If you see a way to complete it, please contact me(Nephro).
// 	for (std::vector<candidate_action_ptr>::iterator it = candidate_actions_.begin(); it != candidate_actions_.end(); )
// 	{
// 		if ((*it)->to_be_removed())
// 		{
// 			// code to remove a CA
// 			std::string path = "stage[" + this->get_id() + "].candidate_action[" + (*it)->get_name() + "]";
//
// 			config cfg = config();
// 			cfg["path"] = path;
// 			cfg["action"] = "delete";
//
// 			ai::manager::modify_active_ai_for_side(this->get_side(), cfg);
// 		}
// 		else
// 		{
// 			++it; // @note: should I modify this to a while loop?
// 		}
// 	}
}

rca_context& candidate_action_evaluation_loop::get_rca_context()
{
	return *this;
}

candidate_action_evaluation_loop::~candidate_action_evaluation_loop()
{
}

void candidate_action_evaluation_loop::make_decision()
{
	// Utilizing dynamic programming to find optimal policy

	LOG_AI_TESTING_RCA_DEFAULT << "inside make_decision()"<< std::endl;

	// First calculate state of current turn.
	boost::shared_ptr<turn_state> stage_1(new turn_state(this->get_side(), resources::tod_manager->turn(), *resources::units, *resources::teams));
	states_.push(*stage_1);

	// Second calculate decisions based on current states.
	simulate_states_ahead();

	// Finally figure out the optimal policy for the total
	// three turns and set the flag for CA to use.
	set_optimal_policy();

	LOG_AI_TESTING_RCA_DEFAULT << "------Analyze completed------" << std::endl;
}

void candidate_action_evaluation_loop::simulate_states_ahead()
{
	// Assume the final turn of dynamic programming is two
	// turns ahead, that is, consider three turns totally.
	const int total_turns = 3;
	int current_turn = 1;

	// Pop one state from queue and calculate resulted states
	// of each decision, push them back to the queue.
	while (current_turn < total_turns){
		turn_state current_state = states_.front();
		states_.pop();

		for(int i = 0; i != decision::total_decisions; ++i){
			states_.push(simulate_state(i, current_state));
		}

		current_turn = states_.front().get_turn_no();
	}
}


void candidate_action_evaluation_loop::set_optimal_policy()
{
	// Find the final stage with maximum score.

	turn_state optimal_final_state = states_.front();
	states_.pop();
	optimal_final_state.scoring_state();
	double optimal_score = optimal_final_state.get_state_score();
	decision optimal_decision = optimal_final_state.get_decision();

	while(!states_.empty()){
		turn_state current_final_state = states_.front();
		states_.pop();
		current_final_state.scoring_state();
		double current_score = current_final_state.get_state_score();
		if(current_score > optimal_score){
			optimal_decision = current_final_state.get_decision();
			optimal_score = current_score;
		}
	}

	LOG_AI_TESTING_RCA_DEFAULT << optimal_decision << std::endl;

	// TODO: Set flag.
}

const turn_state candidate_action_evaluation_loop::simulate_state(int decision_no_, turn_state &state)
{
	// Simulate CA based on state and get resulted turn_state.

	const unit_map &units_ = state.get_units();
	const std::vector<team> &teams_ = state.get_teams();

	// TODO: Simulate.

	// Construct new stage after execute CA.
	boost::shared_ptr<turn_state> state_next(new turn_state(this->get_side(), state.get_turn_no()+1, units_, teams_));
	const decision &decision_ = state.get_decision();
	if(decision_.is_valid()){
		state_next->set_decision(decision_.get_decision_no());
	} else {
		state_next->set_decision(decision_no_);
	}

	return *state_next;
}

// Implement class turn_state.
turn_state::turn_state(const int own_side_, const int turn_no_, const unit_map &units_, const std::vector<team> &teams_) :
	own_side_(own_side_),
	turn_no_(turn_no_),
	state_score_(0.0),
	units_(units_),
	teams_(teams_),
	decision_(-1)
{
}

turn_state::~turn_state()
{
}

void turn_state::scoring_state(){
	// A very simple scoring function.

	const int total_team = teams_.size();
	std::vector<double> state(total_team, 0.0);
	std::vector<int> total_level(total_team, 0);

	// Sum up the units' score.
	for(unit_map::const_unit_iterator ui = units_.begin(); ui != units_.end(); ++ui) {
		int current_side = ui->side() - 1;

		double unit_score = (double)ui->hitpoints()/(double)ui->max_hitpoints()*(double)ui->cost();
		LOG_AI_TESTING_RCA_DEFAULT << "\tside " << current_side+1 << " unit " << ui->type_name() << "'s score is " << unit_score << std::endl;
		state[current_side] += unit_score;
		total_level[current_side] += ui->level();
	}


	const int total_turns = 3;
	const int turn_left = total_turns - turn_no_;
	std::vector<int> upkeep_per_turn(total_team, 0);
	std::vector<int> income_per_turn(total_team, 0);
	std::vector<double> gold(total_team, 0.0);

	// Add the gold that current have and that intend to
	// get as income in future totally(subtract the upkeep),
	// for each team.
	for(std::vector<team>::const_iterator ti = teams_.begin(); ti != teams_.end(); ++ti) {
		int current_side = ti->side() - 1;
		upkeep_per_turn[current_side] = total_level[current_side]>ti->support() ? total_level[current_side]-ti->support() : 0;
		income_per_turn[current_side] = ti->total_income() - upkeep_per_turn[current_side];

		// The further, the rougher. So drop 0.1 weight for each turn.
		gold[current_side] = ti->gold() + income_per_turn[current_side] * (1+(1-0.1*(double)(turn_left-1)))*turn_left/2;
		state[current_side] += gold[current_side];

		LOG_AI_TESTING_RCA_DEFAULT << "\tside " << current_side+1 << " will totally get " << gold[current_side] << " gold." << std::endl;
	}


	for(std::vector<team>::const_iterator ti = teams_.begin(); ti != teams_.end(); ++ti) {
		int current_side = ti->side() - 1;
		LOG_AI_TESTING_RCA_DEFAULT << "\tside " << current_side+1 << "'s state: " << state[current_side] << std::endl;
		if(ti->is_enemy(own_side_)){ // 'is_enemy()' based on 1.
			state_score_ -= state[current_side];
		} else {
			state_score_ += state[current_side];
		}
	}

	LOG_AI_TESTING_RCA_DEFAULT << "State constructed with score " << state_score_ << std::endl;
}

// Implement class decision and overload operator<<.
decision::~decision()
{
}

std::ostream& operator<<(std::ostream &output, const decision &decision_)
{
	std::string policy = "";

	switch(decision_.decision_no_){
		case 0:
			policy = "offensively";
			break;
		case 1:
			policy = "defensively";
			break;
		default:
			policy = "unknown";
			break;
	}

	output << "The global optimal policy of this turn is to play "
		<< policy << std::endl;

	return output;
}

} // end of namespace testing_ai_default

} // end of namespace ai
