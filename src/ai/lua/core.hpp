/*
   Copyright (C) 2010 - 2014 by Yurii Chernyi <terraninfo@terraninfo.net>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

#ifndef AI_LUA_CORE_HPP
#define AI_LUA_CORE_HPP

#include <boost/shared_ptr.hpp>
#include "../../unit.hpp"
#include "../../map.hpp"
#include "../../team.hpp"

struct lua_State;
class LuaKernel;
class config;



namespace ai {

class engine_lua;
class lua_object_base;
typedef boost::shared_ptr<lua_object_base> lua_object_ptr;

class decision;
class stage_state;

/**
 * Proxy table for the AI context
 */
class lua_ai_context
{
private:
	lua_State *L;
	int num_;
	int side_;
	lua_ai_context(lua_State *l, int num, int side) : L(l), num_(num), side_(side)
	{
	}
	static lua_ai_context* create(lua_State *L, char const *code, engine_lua *engine);
public:
	~lua_ai_context();
	lua_ai_context()
		: L(NULL)
		, num_(0)
		, side_(0)
	{
	}
	void load();
	void load_and_inject_ai_table(engine_lua* engine);
	void get_persistent_data(config &) const;
	void set_persistent_data(const config &);
	static void init(lua_State *L);
	friend class ::LuaKernel;
};


/**
 * Proxy class for calling AI action handlers defined in Lua.
 */
class lua_ai_action_handler
{
private:
	lua_State *L;
	lua_ai_context &context_;
	int num_;
	lua_ai_action_handler(lua_State *l, lua_ai_context &context, int num) : L(l), context_(context),num_(num)
	{
	}
	static lua_ai_action_handler* create(lua_State *L, char const *code, lua_ai_context &context);
public:
	~lua_ai_action_handler();
	void handle(config &, bool configOut, lua_object_ptr);
	friend class ::LuaKernel;
};


// Main changes of prototype_new are available in commit message

/**
 * Represent decision.
 */
class decision
{
    friend std::ostream& operator<<(std::ostream&, const decision&);

private:
    typedef std::vector<int> ca_container;
    int decision_no_; // Decision variable.
    ca_container recommend_ca_;

public:
    static const int total_decisions = 2; // We have two kinds of decisions now.

    decision(int decision_no_) : decision_no_(decision_no_)
    {
    }

    ~decision();

    int get_decision_no() const { return decision_no_; }
    decision& set_decision_no(int decision_no_) { this->decision_no_ = decision_no_; return *this; }
    const ca_container& get_recommend_ca() const { return recommend_ca_; }
    decision& set_recommend_ca(const ca_container &recommend_ca_) { this->recommend_ca_ = recommend_ca_; return *this; }
    bool is_valid() const { return (decision_no_ >= 0 && decision_no_ < total_decisions); }
    void add_recommend_ca(int ca_no_) { recommend_ca_.push_back(ca_no_); }
};


/**
 * Record the state of stage.
 */
class stage_state
{
private:
    const int own_side_;
    const int turn_no_;
    const int stage_no_;   // Stage variable.
    double state_value_; // State variable.

    const unit_map units_;
    const gamemap map_; // Maybe don't need if we have team
    const std::vector<team> teams_;

    decision decision_;

public:
    stage_state(const int own_side_, const int turn_no_, const int stage_no_, const unit_map &units_, const gamemap &map_, const std::vector<team> &teams_);
    ~stage_state();

    int get_turn_no() const { return turn_no_; }
    int get_stage_no() const { return stage_no_; }
    double get_state_value() const { return state_value_; }

    const unit_map& get_units() const { return units_; }
    const gamemap& get_map() const { return map_; }
    const std::vector<team>& get_teams() const { return teams_; }

    const decision& get_decision() const { return decision_; }
    void set_decision(int decision_no_, const std::vector<int> &recommend_ca_) { decision_.set_decision_no(decision_no_).set_recommend_ca(recommend_ca_); }
};

}//of namespace ai

#endif
