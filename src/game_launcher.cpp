/*
   Copyright (C) 2003 - 2014 by David White <dave@whitevine.net>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

#include "game_launcher.hpp"
#include "global.hpp"                   // for false_, bool_

#include "about.hpp" //for show_about
#include "commandline_options.hpp"      // for commandline_options
#include "config.hpp"                   // for config, etc
#include "config_assign.hpp"
#include "construct_dialog.hpp"         // for dialog
#include "cursor.hpp"                   // for set, CURSOR_TYPE::NORMAL
#include "exceptions.hpp"               // for error
#include "filesystem.hpp"               // for get_user_config_dir, etc
#include "game_classification.hpp"      // for game_classification, etc
#include "game_config.hpp"              // for path, no_delay, revision, etc
#include "game_config_manager.hpp"      // for game_config_manager
#include "game_end_exceptions.hpp"      // for LEVEL_RESULT, etc
#include "gettext.hpp"                  // for _
#include "gui/dialogs/language_selection.hpp"  // for tlanguage_selection
#include "gui/dialogs/message.hpp" //for show error message
#include "gui/dialogs/mp_host_game_prompt.hpp" //for host game prompt
#include "gui/dialogs/mp_method_selection.hpp"
#include "gui/dialogs/transient_message.hpp"  // for show_transient_message
#include "gui/dialogs/title_screen.hpp"  // for show_debug_clock_button
#include "gui/widgets/settings.hpp"     // for new_widgets
#include "gui/widgets/window.hpp"       // for twindow, etc
#include "intro.hpp"
#include "language.hpp"                 // for language_def, etc
#include "loadscreen.hpp"               // for loadscreen, etc
#include "log.hpp"                      // for LOG_STREAM, logger, general, etc
#include "map_exception.hpp"
#include "multiplayer.hpp"              // for start_client, etc
#include "create_engine.hpp"
#include "network.hpp"
#include "playcampaign.hpp"             // for play_game, etc
#include "preferences.hpp"              // for disable_preferences_save, etc
#include "preferences_display.hpp"      // for detect_video_settings, etc
#include "replay.hpp"                   // for replay, recorder
#include "resources.hpp"                // for config_manager
#include "savegame.hpp"                 // for clean_saves, etc
#include "sdl/utils.hpp"                // for surface
#include "serialization/compression.hpp"  // for format::NONE
#include "serialization/string_utils.hpp"  // for split
#include "singleplayer.hpp"             // for sp_create_mode
#include "statistics.hpp"
#include "tstring.hpp"                  // for operator==, operator!=
#include "util.hpp"                     // for lexical_cast_default
#include "wml_exception.hpp"            // for twml_exception

#include <algorithm>                    // for copy, max, min, stable_sort
#include <boost/foreach.hpp>            // for auto_any_base, etc
#include <boost/optional.hpp>           // for optional
#include <boost/tuple/tuple.hpp>        // for tuple
#include <cstdlib>                     // for NULL, system
#include <iostream>                     // for operator<<, basic_ostream, etc
#include <utility>                      // for pair
#include "SDL.h"                        // for SDL_INIT_JOYSTICK, etc
#include "SDL_events.h"                 // for SDL_ENABLE
#include "SDL_joystick.h"               // for SDL_JoystickEventState, etc
#include "SDL_timer.h"                  // for SDL_Delay
#include "SDL_version.h"                // for SDL_VERSION_ATLEAST
#include "SDL_video.h"                  // for SDL_WM_SetCaption, etc

#ifdef DEBUG_WINDOW_LAYOUT_GRAPHS
#include "gui/widgets/debug.hpp"
#endif

struct incorrect_map_format_error;

static lg::log_domain log_config("config");
#define ERR_CONFIG LOG_STREAM(err, log_config)
#define WRN_CONFIG LOG_STREAM(warn, log_config)
#define LOG_CONFIG LOG_STREAM(info, log_config)

#define LOG_GENERAL LOG_STREAM(info, lg::general)
#define WRN_GENERAL LOG_STREAM(warn, lg::general)
#define DBG_GENERAL LOG_STREAM(debug, lg::general)

static lg::log_domain log_mp_create("mp/create");
#define DBG_MP LOG_STREAM(debug, log_mp_create)

static lg::log_domain log_network("network");
#define ERR_NET LOG_STREAM(err, log_network)

static lg::log_domain log_enginerefac("enginerefac");
#define LOG_RG LOG_STREAM(info, log_enginerefac)

game_launcher::game_launcher(const commandline_options& cmdline_opts, const char *appname) :
	cmdline_opts_(cmdline_opts),
	disp_(NULL),
	video_(),
	thread_manager(),
	font_manager_(),
	prefs_manager_(),
	image_manager_(),
	main_event_context_(),
	hotkey_manager_(),
	music_thinker_(),
	resize_monitor_(),
	test_scenario_("test"),
	screenshot_map_(),
	screenshot_filename_(),
	state_(),
	multiplayer_server_(),
	jump_to_multiplayer_(false),
	jump_to_campaign_(false, -1, "", ""),
	jump_to_editor_(false)
{
	bool no_music = false;
	bool no_sound = false;

	// The path can be hardcoded and it might be a relative path.
	if(!game_config::path.empty() &&
#ifdef _WIN32
		// use c_str to ensure that index 1 points to valid element since c_str() returns null-terminated string
		game_config::path.c_str()[1] != ':'
#else
		game_config::path[0] != '/'
#endif
	)
	{
		game_config::path = filesystem::get_cwd() + '/' + game_config::path;
		font_manager_.update_font_path();
	}

	const std::string app_basename = filesystem::base_name(appname);
	jump_to_editor_ = app_basename.find("editor") != std::string::npos;

	if (cmdline_opts_.campaign)	{
		jump_to_campaign_.jump_ = true;
		jump_to_campaign_.campaign_id_ = *cmdline_opts_.campaign;
		std::cerr << "selected campaign id: [" << jump_to_campaign_.campaign_id_ << "]\n";

		if (cmdline_opts_.campaign_difficulty) {
			jump_to_campaign_.difficulty_ = *cmdline_opts_.campaign_difficulty;
			std::cerr << "selected difficulty: [" << jump_to_campaign_.difficulty_ << "]\n";
		}
		else
			jump_to_campaign_.difficulty_ = -1; // let the user choose the difficulty

		if (cmdline_opts_.campaign_scenario) {
			jump_to_campaign_.scenario_id_ = *cmdline_opts_.campaign_scenario;
			std::cerr << "selected scenario id: [" << jump_to_campaign_.scenario_id_ << "]\n";
		}
	}
	if (cmdline_opts_.clock)
		gui2::show_debug_clock_button = true;
	if (cmdline_opts_.debug) {
		game_config::debug = true;
		game_config::mp_debug = true;
	}
#ifdef DEBUG_WINDOW_LAYOUT_GRAPHS
	if (cmdline_opts_.debug_dot_domain)
		gui2::tdebug_layout_graph::set_domain (*cmdline_opts_.debug_dot_domain);
	if (cmdline_opts_.debug_dot_level)
		gui2::tdebug_layout_graph::set_level (*cmdline_opts_.debug_dot_level);
#endif
	if (cmdline_opts_.editor)
	{
		jump_to_editor_ = true;
		if(!cmdline_opts_.editor->empty())
			game::load_game_exception::game = *cmdline_opts_.editor;
	}
	if (cmdline_opts_.fps)
		preferences::set_show_fps(true);
	if (cmdline_opts_.fullscreen)
		preferences::set_fullscreen(true);
	if (cmdline_opts_.load)
		game::load_game_exception::game = *cmdline_opts_.load;
	if (cmdline_opts_.max_fps) {
		int fps;
		//FIXME: remove the next line once the weird util.cpp specialized template lexical_cast_default() linking issue is solved
		fps = lexical_cast_default<int>("", 50);
		fps = *cmdline_opts_.max_fps;
		fps = std::min<int>(fps, 1000);
		fps = std::max<int>(fps, 1);
		fps = 1000 / fps;
		// increase the delay to avoid going above the maximum
		if(1000 % fps != 0) {
			++fps;
		}
		preferences::set_draw_delay(fps);
	}
	if (cmdline_opts_.nogui || cmdline_opts_.headless_unit_test) {
		no_sound = true;
		preferences::disable_preferences_save();
	}
	if (cmdline_opts_.new_widgets)
		gui2::new_widgets = true;
	if (cmdline_opts_.nodelay)
		game_config::no_delay = true;
	if (cmdline_opts_.nomusic)
		no_music = true;
	if (cmdline_opts_.nosound)
		no_sound = true;
	//These commented lines should be used to implement support of connection
	//through a proxy via command line options.
	//The ANA network module should implement these methods (while the SDL_net won't.)
	if (cmdline_opts_.proxy)
		network::enable_connection_through_proxy();
	if (cmdline_opts_.proxy_address)
	{
		network::enable_connection_through_proxy();
		network::set_proxy_address(*cmdline_opts_.proxy_address);
	}
	if (cmdline_opts_.proxy_password)
	{
		network::enable_connection_through_proxy();
		network::set_proxy_password(*cmdline_opts_.proxy_password);
	}
	if (cmdline_opts_.proxy_port)
	{
		network::enable_connection_through_proxy();
		network::set_proxy_port(*cmdline_opts_.proxy_port);
	}
	if (cmdline_opts_.proxy_user)
	{
		network::enable_connection_through_proxy();
		network::set_proxy_user(*cmdline_opts_.proxy_user);
	}
	if (cmdline_opts_.resolution) {
		const int xres = cmdline_opts_.resolution->get<0>();
		const int yres = cmdline_opts_.resolution->get<1>();
		if(xres > 0 && yres > 0) {
			const std::pair<int,int> resolution(xres,yres);
			preferences::set_resolution(resolution);
		}
	}
	if (cmdline_opts_.screenshot) {
		//TODO it could be simplified to use cmdline_opts_ directly if there is no other way to enter screenshot mode
		screenshot_map_ = *cmdline_opts_.screenshot_map_file;
		screenshot_filename_ = *cmdline_opts_.screenshot_output_file;
		no_sound = true;
		preferences::disable_preferences_save();
	}
	if (cmdline_opts_.server){
		jump_to_multiplayer_ = true;
		//Do we have any server specified ?
		if (!cmdline_opts_.server->empty())
			multiplayer_server_ = *cmdline_opts_.server;
		else //Pick the first server in config
		{
			if (game_config::server_list.size() > 0)
				multiplayer_server_ = preferences::network_host();
			else
				multiplayer_server_ = "";
		}
	}
	if (cmdline_opts_.username) {
		preferences::disable_preferences_save();
		preferences::set_login(*cmdline_opts_.username);
	}
	if (cmdline_opts_.password) {
		preferences::disable_preferences_save();
		preferences::set_password(*cmdline_opts_.password);
	}
	if (cmdline_opts_.test)
	{
		if (!cmdline_opts_.test->empty())
			test_scenario_ = *cmdline_opts_.test;
	}
	if (cmdline_opts_.unit_test)
	{
		if (!cmdline_opts_.unit_test->empty()) {
			test_scenario_ = *cmdline_opts_.unit_test;
		}

	}
	if (cmdline_opts_.windowed)
		preferences::set_fullscreen(false);
	if (cmdline_opts_.with_replay)
		game::load_game_exception::show_replay = true;

	std::cerr << '\n';
	std::cerr << "Data directory: " << game_config::path
		<< "\nUser configuration directory: " << filesystem::get_user_config_dir()
		<< "\nUser data directory: " << filesystem::get_user_data_dir()
		<< "\nCache directory: " << filesystem::get_cache_dir()
		<< '\n';

	// disable sound in nosound mode, or when sound engine failed to initialize
	if (no_sound || ((preferences::sound_on() || preferences::music_on() ||
	                  preferences::turn_bell() || preferences::UI_sound_on()) &&
	                 !sound::init_sound())) {
		preferences::set_sound(false);
		preferences::set_music(false);
		preferences::set_turn_bell(false);
		preferences::set_UI_sound(false);
	}
	else if (no_music) { // else disable the music in nomusic mode
		preferences::set_music(false);
	}
}

game_display& game_launcher::disp()
{
	if(disp_.get() == NULL) {
		if(get_video_surface() == NULL) {
			throw CVideo::error();
		}
		disp_.assign(game_display::create_dummy_display(video_));
	}
	return *disp_.get();
}

bool game_launcher::init_joystick()
{
	if (!preferences::joystick_support_enabled())
		return false;

	if(SDL_WasInit(SDL_INIT_JOYSTICK) == 0)
		if(SDL_InitSubSystem(SDL_INIT_JOYSTICK) == -1)
			return false;

	int joysticks = SDL_NumJoysticks();
	if (joysticks == 0) return false;

	SDL_JoystickEventState(SDL_ENABLE);

	bool joystick_found = false;
	for (int i = 0; i<joysticks; i++)  {

		if (SDL_JoystickOpen(i))
			joystick_found = true;
	}
	return joystick_found;
}

bool game_launcher::init_language()
{
	if(!::load_language_list())
		return false;

	language_def locale;
	if(cmdline_opts_.language) {
		std::vector<language_def> langs = get_languages();
		BOOST_FOREACH(const language_def & def, langs) {
			if(def.localename == *cmdline_opts_.language) {
				locale = def;
				break;
			}
		}
		if(locale.localename.empty()) {
			std::cerr << "Language symbol '" << *cmdline_opts_.language << "' not found.\n";
			return false;
		}
	} else {
		locale = get_locale();
	}
	::set_language(locale);

	return true;
}

bool game_launcher::init_video()
{
	if(cmdline_opts_.nogui || cmdline_opts_.headless_unit_test) {
		if( !(cmdline_opts_.multiplayer || cmdline_opts_.screenshot || cmdline_opts_.headless_unit_test) ) {
			std::cerr << "--nogui flag is only valid with --multiplayer flag or --screenshot flag\n";
			return false;
		}
		video_.make_fake();
		game_config::no_delay = true;
		return true;
	}

	std::string wm_title_string = _("The Battle for Wesnoth");
	wm_title_string += " - " + game_config::revision;
#if !SDL_VERSION_ATLEAST(2, 0, 0)
	SDL_WM_SetCaption(wm_title_string.c_str(), NULL);
#endif

#if !(defined(__APPLE__))
	surface icon(image::get_image("game-icon.png", image::UNSCALED));
	if(icon != NULL) {
		///must be called after SDL_Init() and before setting video mode
#if !SDL_VERSION_ATLEAST(2, 0, 0)
		SDL_WM_SetIcon(icon,NULL);
#endif
	}
#endif

	std::pair<int,int> resolution;
	int bpp = 0;
	int video_flags = 0;

	bool found_matching = preferences::detect_video_settings(video_, resolution, bpp, video_flags);

	if (cmdline_opts_.bpp) {
		bpp = *cmdline_opts_.bpp;
	} else if (cmdline_opts_.screenshot) {
		bpp = 32;
	}

	if(!found_matching && (video_flags & FULL_SCREEN)) {
		video_flags ^= FULL_SCREEN;
		found_matching = preferences::detect_video_settings(video_, resolution, bpp, video_flags);
		if (found_matching) {
			std::cerr << "Failed to set " << resolution.first << 'x' << resolution.second << 'x' << bpp << " in fullscreen mode. Using windowed instead.\n";
		}
	}

	if(!found_matching) {
		std::cerr << "Video mode " << resolution.first << 'x'
			<< resolution.second << 'x' << bpp
			<< " is not supported.\n";

		return false;
	}

	std::cerr << "setting mode to " << resolution.first << "x" << resolution.second << "x" << bpp << "\n";
	const int res = video_.setMode(resolution.first,resolution.second,bpp,video_flags);
	video_.setBpp(bpp);
	if(res == 0) {
		std::cerr << "required video mode, " << resolution.first << "x"
		          << resolution.second << "x" << bpp << " is not supported\n";
		return false;
	}
#if SDL_VERSION_ATLEAST(2, 0, 0)
	CVideo::set_window_title(wm_title_string);
#if !(defined(__APPLE__))
	if(icon != NULL) {
		CVideo::set_window_icon(icon);
	}
#endif
#endif
	return true;
}

bool game_launcher::play_test()
{
	static bool first_time = true;

	if(!cmdline_opts_.test) {
		return true;
	}
	if(!first_time)
		return false;

	first_time = false;

	state_.classification().campaign_type = game_classification::TEST;
	state_.classification().campaign_define = "TEST";

	state_.set_carryover_sides_start(
		config_of("next_scenario", test_scenario_)
	);


	
	resources::config_manager->
		load_game_config_for_game(state_.classification());

	try {
		play_game(disp(),state_,resources::config_manager->game_config());
	} catch (game::load_game_exception &) {
		return true;
	}

	return false;
}

// Same as play_test except that we return the results of play_game.
int game_launcher::unit_test()
{
	static bool first_time_unit = true;

	if(!cmdline_opts_.unit_test) {
		return 0;
	}
	if(!first_time_unit)
		return 0;

	first_time_unit = false;

	state_.classification().campaign_type = game_classification::TEST;
	state_.classification().campaign_define = "TEST";
	state_.set_carryover_sides_start(
		config_of("next_scenario", test_scenario_)
	);


	resources::config_manager->
		load_game_config_for_game(state_.classification());

	try {
		LEVEL_RESULT res = play_game(disp(),state_,resources::config_manager->game_config(), IO_SERVER, false, false, false, true);
		if (!(res == VICTORY || res == NONE) || lg::broke_strict()) {
			return 1;
		}
	} catch (game::load_game_exception &) {
		std::cerr << "Load_game_exception encountered while loading the unit test!" << std::endl;
		return 1; //failed to load the unit test scenario
	} catch(twml_exception& e) {
		std::cerr << "Caught WML Exception:" << e.dev_message << std::endl; //e.show(disp());
		return 1;
	}

	savegame::clean_saves(state_.classification().label);

	if (cmdline_opts_.noreplaycheck)
		return 0; //we passed, huzzah!

	savegame::replay_savegame save(state_, compression::NONE);
	save.save_game_automatic(disp().video(), false, "unit_test_replay"); //false means don't check for overwrite

	clear_loaded_game();

	//game::load_game_exception::game = *cmdline_opts_.load
	game::load_game_exception::game = "unit_test_replay";
	//	game::load_game_exception::game = "Unit_test_" + test_scenario_ + "_replay";

	game::load_game_exception::show_replay = true;
	game::load_game_exception::cancel_orders = true;

	if (!load_game()) {
		std::cerr << "Failed to load the replay!" << std::endl;
		return 3; //failed to load replay
	}

	try {
		//LEVEL_RESULT res = play_game(disp(), state_, resources::config_manager->game_config(), IO_SERVER, false,false,false,true);
		LEVEL_RESULT res = ::play_replay(disp(), state_, resources::config_manager->game_config(), video_, true);
		if (!(res == VICTORY || res == NONE)) {
			std::cerr << "Observed failure on replay" << std::endl;
			return 4;
		}
		/*::play_replay(disp(),state_,resources::config_manager->game_config(),
		    video_);*/
		clear_loaded_game();
	} catch (game::load_game_exception &) {
		std::cerr << "Load_game_exception encountered during play_replay!" << std::endl;
		return 3; //failed to load replay
	} catch(twml_exception& e) {
		std::cerr << "WML Exception while playing replay: " << e.dev_message << std::endl; //e.show(disp());
		return 4; //failed with an error during the replay
	}

	return 0; //we passed, huzzah!
}

bool game_launcher::play_screenshot_mode()
{
	if(!cmdline_opts_.screenshot) {
		return true;
	}

	resources::config_manager->load_game_config_for_editor();

	::init_textdomains(resources::config_manager->game_config());

	editor::start(resources::config_manager->game_config(), video_,
	    screenshot_map_, true, screenshot_filename_);
	return false;
}

bool game_launcher::play_render_image_mode()
{
	if(!cmdline_opts_.render_image) {
		return true;
	}

	state_.classification().campaign_type = game_classification::MULTIPLAYER;
	DBG_GENERAL << "Current campaign type: " << state_.classification().campaign_type << std::endl;

	try {
		resources::config_manager->
			load_game_config_for_game(state_.classification());
	} catch(config::error& e) {
		std::cerr << "Error loading game config: " << e.what() << std::endl;
		return false;
	}

	// A default output filename
	std::string outfile = "wesnoth_image.bmp";

	// If a output path was given as an argument, use that instead
	if (cmdline_opts_.render_image_dst) {
		outfile = *cmdline_opts_.render_image_dst;
	}

	image::save_image(*cmdline_opts_.render_image, outfile);

	return false;
}

bool game_launcher::is_loading() const
{
	return !game::load_game_exception::game.empty();
}

bool game_launcher::load_game()
{
	assert(resources::config_manager);

	DBG_GENERAL << "Current campaign type: " << state_.classification().campaign_type << std::endl;

	savegame::loadgame load(disp(), resources::config_manager->game_config(),
	    state_);

	try {
		load.load_game(game::load_game_exception::game, game::load_game_exception::show_replay, game::load_game_exception::cancel_orders, game::load_game_exception::select_difficulty, game::load_game_exception::difficulty);


		try {
			resources::config_manager->
				load_game_config_for_game(state_.classification());
		} catch(config::error&) {
			return false;
		}

		load.set_gamestate();

	} catch(load_game_cancelled_exception&) {
		clear_loaded_game();
		return false;
	} catch(config::error& e) {
		if(e.message.empty()) {
			gui2::show_error_message(disp().video(), _("The file you have tried to load is corrupt"));
		}
		else {
			gui2::show_error_message(disp().video(), _("The file you have tried to load is corrupt: '") + e.message + '\'');
		}
		return false;
	} catch(twml_exception& e) {
		e.show(disp());
		return false;
	} catch(filesystem::io_exception& e) {
		if(e.message.empty()) {
			gui2::show_error_message(disp().video(), _("File I/O Error while reading the game"));
		} else {
			gui2::show_error_message(disp().video(), _("File I/O Error while reading the game: '") + e.message + '\'');
		}
		return false;
	} catch(game::error& e) {
		if(e.message.empty()) {
			gui2::show_error_message(disp().video(), _("The file you have tried to load is corrupt"));
		}
		else {
			gui2::show_error_message(disp().video(), _("The file you have tried to load is corrupt: '") + e.message + '\'');
		}
		return false;
	}
	recorder = replay(state_.replay_data);
	recorder.start_replay();
	recorder.set_skip(false);

	LOG_CONFIG << "has is middle game savefile: " << (state_.is_mid_game_save() ? "yes" : "no") << "\n";

	if (!state_.is_mid_game_save()) {
		//this is a start-of-scenario
		if (load.show_replay()) {
			// There won't be any turns to replay, but the
			// user gets to watch the intro sequence again ...
			LOG_CONFIG << "replaying (start of scenario)\n";
		} else {
			LOG_CONFIG << "skipping...\n";
			recorder.set_skip(false);
		}
	} else {
		// We have a snapshot. But does the user want to see a replay?
		if(load.show_replay()) {
			statistics::clear_current_scenario();
			LOG_CONFIG << "replaying (snapshot)\n";
		} else {
			LOG_CONFIG << "setting replay to end...\n";
			recorder.set_to_end();
			if(!recorder.at_end()) {
				WRN_CONFIG << "recorder is not at the end!!!" << std::endl;
			}
		}
	}

	if(state_.classification().campaign_type == game_classification::MULTIPLAYER) {
		state_.unify_controllers();
		gui2::show_message(disp().video(), _("Warning") , _("This is a multiplayer scenario. Some parts of it may not work properly in single-player. It is recommended to load this scenario through the <b>Multiplayer</b> → <b>Load Game</b> dialog instead."), "", true, true);
	}

	if (load.cancel_orders()) {
		state_.cancel_orders();
	}

	return true;
}

void game_launcher::set_tutorial()
{
	state_ = saved_game();
	state_.classification().campaign_type = game_classification::TUTORIAL;
	state_.classification().campaign_define = "TUTORIAL";
	state_.set_carryover_sides_start(
		config_of("next_scenario", "tutorial")
	);

}

void game_launcher::mark_completed_campaigns(std::vector<config> &campaigns)
{
	BOOST_FOREACH(config &campaign, campaigns) {
		campaign["completed"] = preferences::is_campaign_completed(campaign["id"]);
	}
}

bool game_launcher::new_campaign()
{
	state_ = saved_game();
	state_.classification().campaign_type = game_classification::SCENARIO;

	state_.mp_settings().show_configure = false;
	state_.mp_settings().show_connect = false;

	return sp::enter_create_mode(disp(), resources::config_manager->game_config(),
		state_, jump_to_campaign_, true);
}

std::string game_launcher::jump_to_campaign_id() const
{
	return jump_to_campaign_.campaign_id_;
}

bool game_launcher::goto_campaign()
{
	if(jump_to_campaign_.jump_){
		if(new_campaign()) {
			jump_to_campaign_.jump_ = false;
			launch_game(NO_RELOAD_DATA);
		}else{
			jump_to_campaign_.jump_ = false;
			return false;
		}
	}
	return true;
}

bool game_launcher::goto_multiplayer()
{
	if(jump_to_multiplayer_){
		jump_to_multiplayer_ = false;
		if(play_multiplayer()){
			;
		}else{
			return false;
		}
	}
	return true;
}

bool game_launcher::goto_editor()
{
	if(jump_to_editor_){
		jump_to_editor_ = false;
		if (start_editor(filesystem::normalize_path(game::load_game_exception::game)) ==
		    editor::EXIT_QUIT_TO_DESKTOP)
		{
			return false;
		}
		clear_loaded_game();
	}
	return true;
}

void game_launcher::start_wesnothd()
{
	const std::string wesnothd_program =
		preferences::get_mp_server_program_name().empty() ?
		filesystem::get_program_invocation("wesnothd") : preferences::get_mp_server_program_name();

	std::string config = filesystem::get_user_config_dir() + "/lan_server.cfg";
	if (!filesystem::file_exists(config)) {
		// copy file if it isn't created yet
		filesystem::write_file(config, filesystem::read_file(filesystem::get_wml_location("lan_server.cfg")));
	}

#ifndef _WIN32
	std::string command = "\"" + wesnothd_program +"\" -c \"" + config + "\" -d -t 2 -T 5";
#else
	// start wesnoth as background job
	std::string command = "cmd /C start \"wesnoth server\" /B \"" + wesnothd_program + "\" -c \"" + config + "\" -t 2 -T 5";
#endif
	LOG_GENERAL << "Starting wesnothd: "<< command << "\n";
	if (std::system(command.c_str()) == 0) {
		// Give server a moment to start up
		SDL_Delay(50);
		return;
	}
	preferences::set_mp_server_program_name("");

	// Couldn't start server so throw error
	WRN_GENERAL << "Failed to run server start script" << std::endl;
	throw game::mp_server_error("Starting MP server failed!");
}

bool game_launcher::play_multiplayer()
{
	int res;

	state_ = saved_game();
	state_.classification().campaign_type = game_classification::MULTIPLAYER;

	//Print Gui only if the user hasn't specified any server
	if( multiplayer_server_.empty() ){

		int start_server;
		do {
			start_server = 0;

			gui2::tmp_method_selection dlg;

			dlg.show(disp().video());

			if(dlg.get_retval() == gui2::twindow::OK) {
				res = dlg.get_choice();
			} else {
				return false;

			}

			if (res == 2 && preferences::mp_server_warning_disabled() < 2) {
				start_server = !gui2::tmp_host_game_prompt::execute(disp().video());
			}
		} while (start_server);
		if (res < 0) {
			return false;
		}

	}else{
		res = 4;
	}

	try {
		if (res == 2)
		{
			try {
				start_wesnothd();
			} catch(game::mp_server_error&)
			{
				std::string path = preferences::show_wesnothd_server_search(disp());

				if (!path.empty())
				{
					preferences::set_mp_server_program_name(path);
					start_wesnothd();
				}
				else
				{
					return false;
				}
			}


		}

		resources::config_manager->
			load_game_config_for_game(state_.classification());

		events::discard_input(); // prevent the "keylogger" effect
		cursor::set(cursor::NORMAL);

		if(res == 3) {
			mp::start_local_game(disp(),
			    resources::config_manager->game_config(), state_);
		} else if((res >= 0 && res <= 2) || res == 4) {
			std::string host;
			if(res == 0) {
				host = preferences::server_list().front().address;
			}else if(res == 2) {
				host = "localhost";
			}else if(res == 4){
				host = multiplayer_server_;
				multiplayer_server_ = "";
			}
			mp::start_client(disp(), resources::config_manager->game_config(),
				state_, host);
		}

	} catch(game::mp_server_error& e) {
		gui2::show_error_message(disp().video(), _("Error while starting server: ") + e.message);
	} catch(game::load_game_failed& e) {
		gui2::show_error_message(disp().video(), _("The game could not be loaded: ") + e.message);
	} catch(game::game_error& e) {
		gui2::show_error_message(disp().video(), _("Error while playing the game: ") + e.message);
	} catch(network::error& e) {
		if(e.message != "") {
			ERR_NET << "caught network::error: " << e.message << std::endl;
			gui2::show_transient_message(disp().video()
					, ""
					, translation::gettext(e.message.c_str()));
		} else {
			ERR_NET << "caught network::error" << std::endl;
		}
	} catch(config::error& e) {
		if(e.message != "") {
			ERR_CONFIG << "caught config::error: " << e.message << std::endl;
			gui2::show_transient_message(disp().video(), "", e.message);
		} else {
			ERR_CONFIG << "caught config::error" << std::endl;
		}
	} catch(incorrect_map_format_error& e) {
		gui2::show_error_message(disp().video(), std::string(_("The game map could not be loaded: ")) + e.message);
	} catch (game::load_game_exception &) {
		//this will make it so next time through the title screen loop, this game is loaded
	} catch(twml_exception& e) {
		e.show(disp());
	}

	return false;
}

bool game_launcher::play_multiplayer_commandline()
{
	if(!cmdline_opts_.multiplayer) {
		return true;
	}

	DBG_MP << "starting multiplayer game from the commandline" << std::endl;

	// These are all the relevant lines taken literally from play_multiplayer() above
	state_ = saved_game();
	state_.classification().campaign_type = game_classification::MULTIPLAYER;

	resources::config_manager->
		load_game_config_for_game(state_.classification());

	events::discard_input(); // prevent the "keylogger" effect
	cursor::set(cursor::NORMAL);

	mp::start_local_game_commandline(disp(),
	    resources::config_manager->game_config(), state_, cmdline_opts_);

	return false;
}

bool game_launcher::change_language()
{
	gui2::tlanguage_selection dlg;
	dlg.show(disp().video());
	if (dlg.get_retval() != gui2::twindow::OK) return false;

	if (!(cmdline_opts_.nogui || cmdline_opts_.headless_unit_test)) {
		std::string wm_title_string = _("The Battle for Wesnoth");
		wm_title_string += " - " + game_config::revision;
#if SDL_VERSION_ATLEAST(2, 0, 0)
		CVideo::set_window_title(wm_title_string);
#else
		SDL_WM_SetCaption(wm_title_string.c_str(), NULL);
#endif
	}

	return true;
}

void game_launcher::show_preferences()
{
	const preferences::display_manager disp_manager(&disp());
	preferences::show_preferences_dialog(disp(),
	    resources::config_manager->game_config());

	disp().redraw_everything();
}

void game_launcher::launch_game(RELOAD_GAME_DATA reload)
{
	loadscreen::global_loadscreen_manager loadscreen_manager(disp().video());
	loadscreen::start_stage("load data");
	if(reload == RELOAD_DATA) {
		try {
			resources::config_manager->
				load_game_config_for_game(state_.classification());
		} catch(config::error&) {
			return;
		}
	}

	try {
		const LEVEL_RESULT result = play_game(disp(),state_,
		    resources::config_manager->game_config());
		// don't show The End for multiplayer scenario
		// change this if MP campaigns are implemented
		if(result == VICTORY && state_.classification().campaign_type != game_classification::MULTIPLAYER) {
			preferences::add_completed_campaign(state_.classification().campaign);
			the_end(disp(), state_.classification().end_text, state_.classification().end_text_duration);
			if(state_.classification().end_credits) {
				about::show_about(disp(),state_.classification().campaign);
			}
		}

		clear_loaded_game();
	} catch (game::load_game_exception &) {
		//this will make it so next time through the title screen loop, this game is loaded
	} catch(twml_exception& e) {
		e.show(disp());
	}
}

void game_launcher::play_replay()
{
	try {
		::play_replay(disp(),state_,resources::config_manager->game_config(),
		    video_);

		clear_loaded_game();
	} catch (game::load_game_exception &) {
		//this will make it so next time through the title screen loop, this game is loaded
	} catch(twml_exception& e) {
		e.show(disp());
	}
}

editor::EXIT_STATUS game_launcher::start_editor(const std::string& filename)
{
	while(true){
		resources::config_manager->load_game_config_for_editor();

		::init_textdomains(resources::config_manager->game_config());

		editor::EXIT_STATUS res = editor::start(
		    resources::config_manager->game_config(), video_, filename);

		if(res != editor::EXIT_RELOAD_DATA)
			return res;

		resources::config_manager->reload_changed_game_config();
		image::flush_cache();
	}
	return editor::EXIT_ERROR; // not supposed to happen
}

game_launcher::~game_launcher()
{
	try {
		gui::dialog::delete_empty_menu();
		sound::close_sound();
	} catch (...) {}
}
