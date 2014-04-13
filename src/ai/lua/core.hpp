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
#include "../../unit.hpp"  // (Kevin)
#include "../../map.hpp"  // (Kevin)
#include "../../team.hpp"  // (Kevin)

struct lua_State;
class LuaKernel;
class config;



namespace ai {

class engine_lua;
class lua_object_base;
typedef boost::shared_ptr<lua_object_base> lua_object_ptr;

class decision;

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


/**
 * Record the state of stage.
 */
class stage_state
{
private:
    int own_side_;
    const unit_map units_;
    const gamemap map_; // Maybe don't need if we have team
    const std::vector<team> teams_;
    int stage_no_;   // Stage variable.
    double state_value_; // State variable.
    decision& decision_;

public:
    stage_state(int own_side_, const unit_map &units_, const gamemap &map_, const std::vector<team> &teams_, int stage_no_);
    int get_stage_no() const;
    double get_state_value() const;
    const decision get_decision();
    ~stage_state();
};


/**
 * Represent decision.
 */
class decision
{
private:
    int decision_no_; // Decision variable.
    std::vector<int> recommend_ca_;
    double gain_;  // Decision gain.

public:
    static const int total_decision = 2;

    decision();
    stage_state calc_decision(int decision_no_, stage_state &state_);
    std::string describe();
    std::string recommend_ca();
    double get_gain();
    ~decision();
};

}//of namespace ai

#endif
