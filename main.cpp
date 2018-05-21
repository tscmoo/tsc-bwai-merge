#ifdef _MSC_VER
#define _CRT_SECURE_NO_WARNINGS
#define _SCL_SECURE_NO_WARNINGS

#pragma warning(disable: 4456) // warning C4456: declaration of 'x' hides previous local declaration
#pragma warning(disable: 4458) // warning C4458: declaration of 'x' hides class member
#pragma warning(disable: 4459) // warning C4459: declaration of 'x' hides global declaration
#pragma warning(disable: 4457) // warning C4457: declaration of 'x' hides function parameter
#endif

// #ifdef _WIN32
// #include <winsock2.h>
// #include <ws2tcpip.h>
// #include <Windows.h>
// #pragma comment(lib,"ws2_32.lib")
// #pragma comment(lib,"winmm.lib")
// #else
// #include <unistd.h>
// #include <sys/types.h>
// #include <sys/socket.h>
// #include <netinet/in.h>
// #include <netinet/tcp.h>
// #include <netdb.h>
// #endif

#include <cstdlib>
#include <cstdio>
#include <cstring>

#include <BWApi.h>
#include <BWAPI/Client.h>

#ifdef _MSC_VER
//#ifndef _DEBUG
#pragma comment(lib,"BWAPILIB.lib")
#pragma comment(lib,"BWAPIClient.lib")
//#else
//#pragma comment(lib,"BWAPId.lib")
//#pragma comment(lib,"BWAPIClientd.lib")
//#endif
//#pragma comment(lib,"BWAPILIBStatic.lib")
//#pragma comment(lib,"BWAPIClientStatic.lib")
#endif

#include <cmath>

#include <array>
#include <vector>
#include <set>
#include <map>
#include <unordered_set>
#include <unordered_map>
#include <queue>
#include <functional>
#include <numeric>
#include <memory>
#include <random>
#include <chrono>
#include <thread>
#include <initializer_list>
#include <mutex>
#include <algorithm>
#include <utility>

#ifdef _WIN32
#include <windows.h>
#endif

#undef min
#undef max
#undef near
#undef far

#include "tsc/intrusive_list.h"
//#include "tsc/alloc.h"
template<typename T>
using alloc = std::allocator<T>;
#include "tsc/alloc_containers.h"

#include "tsc/strf.h"

constexpr bool test_mode = false;

constexpr bool output_enabled = false;
constexpr bool enable_logfile = false;

//#define ENABLE_PRINT_CALLSTACK

int current_frame;

struct simple_logger {
	std::mutex mut;	
	tsc::a_string str, str2;
	bool newline = true;
	FILE*f = nullptr;
	simple_logger() {
		if (enable_logfile) f = fopen("log.txt", "w");
	}
	template<typename...T>
	void operator()(const char*fmt,T&&...args) {
		std::lock_guard<std::mutex> lock(mut);
		try {
			tsc::strf::format(str, fmt, std::forward<T>(args)...);
		} catch (const std::exception&) {
			str = fmt;
		}
		if (newline) tsc::strf::format(str2,"%5d: %s",current_frame,str);
		const char*out_str= newline ? str2.c_str() : str.c_str();
		newline = strchr(out_str,'\n') ? true : false;
		if (f) {
			fputs(out_str,f);
			fflush(f);
		}
		if (output_enabled) fputs(out_str, stdout);
	}
};
simple_logger logger;


enum {
	log_level_all,
	log_level_debug,
	log_level_info,
	log_level_test,
	log_level_connect,
	log_level_top
};

int current_log_level = test_mode ? log_level_all : log_level_info;
//int current_log_level = log_level_info;
//int current_log_level = log_level_test;

template<typename...T>
void log(int level, const char*fmt, T&&...args) {
	if (current_log_level <= level) logger(fmt, std::forward<T>(args)...);
}

template<typename...T>
void log(const char*fmt, T&&...args) {
	log(log_level_debug, fmt, std::forward<T>(args)...);
}

#ifndef _WIN32
void DebugBreak() {
	std::terminate();
}
#endif

struct xcept_t {
	tsc::a_string str1, str2;
	int n;
	xcept_t() {
		str1.reserve(0x100);
		str2.reserve(0x100);
		n = 0;
	}
	template<typename...T>
	void operator()(const char*fmt,T&&...args) {
		try {
			auto&str = ++n%2 ? str1 : str2;
			tsc::strf::format(str,fmt,std::forward<T>(args)...);
			log(log_level_top, "about to throw exception %s\n", str);
//#ifdef _DEBUG
			DebugBreak();
//#endif
			throw (const char*)str.c_str();
		} catch (const std::bad_alloc&) {
			throw (const char*)fmt;
		}
	}
};
xcept_t xcept;

tsc::a_string format_string;
template<typename...T>
const char*format(const char*fmt,T&&...args) {
	return tsc::strf::format(format_string,fmt,std::forward<T>(args)...);
}

#include "tsc/userthreads.h"

#include "tsc/high_resolution_timer.h"
#include "tsc/rng.h"
#include "tsc/bitset.h"

using tsc::rng;
using tsc::a_string;
using tsc::a_vector;
using tsc::a_deque;
using tsc::a_list;
using tsc::a_set;
using tsc::a_multiset;
using tsc::a_map;
using tsc::a_multimap;
using tsc::a_unordered_set;
using tsc::a_unordered_multiset;
using tsc::a_unordered_map;
using tsc::a_unordered_multimap;

#include "tsc/json.h"

struct default_link_f {
	template<typename T>
	auto* operator()(T*ptr) {
		return (std::pair<T*, T*>*)&ptr->link;
	}
};

const double PI = 3.1415926535897932384626433;

BWAPI::Game* game;

constexpr bool is_bwapi_4 = std::is_pointer<BWAPI::Player>::value;

using BWAPI_Player = std::conditional<is_bwapi_4, BWAPI::Player, BWAPI::Player*>::type;
using BWAPI_Unit = std::conditional<is_bwapi_4, BWAPI::Unit, BWAPI::Unit*>::type;
using BWAPI_Bullet = std::conditional<is_bwapi_4, BWAPI::Bullet, BWAPI::Bullet*>::type;

template<typename utype>
struct xy_typed;
using xy = xy_typed<int>;

struct bwapi_pos {
	int x, y;
	bwapi_pos(BWAPI::Position pos) : x(pos.x), y(pos.y) {}
	bwapi_pos(BWAPI::TilePosition pos) : x(pos.x), y(pos.y) {}
	template<typename T=xy>
	operator T() const {
		return T(x, y);
	}
};

template<typename T = BWAPI_Unit, bool b = is_bwapi_4, typename std::enable_if<b>::type* = nullptr>
bool bwapi_call_build(T unit, BWAPI::UnitType type, BWAPI::TilePosition pos) {
	return unit->build(type, pos);
}
template<typename T = BWAPI_Unit, bool b = is_bwapi_4, typename std::enable_if<!b>::type* = nullptr>
bool bwapi_call_build(T unit, BWAPI::UnitType type, BWAPI::TilePosition pos) {
	return unit->build(pos, type);
}

template<typename T = BWAPI::TechType, bool b = is_bwapi_4, typename std::enable_if<b>::type* = nullptr>
int bwapi_tech_type_energy_cost(T type) {
	return type.energyCost();
}
template<typename T = BWAPI::TechType, bool b = is_bwapi_4, typename std::enable_if<!b>::type* = nullptr>
int bwapi_tech_type_energy_cost(T type) {
	return type.energyUsed();
}

template<typename T = BWAPI_Unit, bool b = is_bwapi_4, typename std::enable_if<b>::type* = nullptr>
bool bwapi_is_powered(T unit) {
	return unit->isPowered();
}
template<typename T = BWAPI_Unit, bool b = is_bwapi_4, typename std::enable_if<!b>::type* = nullptr>
bool bwapi_is_powered(T unit) {
	return !unit->isUnpowered();	
}

//template<typename T = BWAPI::Order, bool b = is_bwapi_4, typename std::enable_if<b>::type* = 0>
//bool bwapi_is_healing_order(BWAPI::Order order) {
//	return order == BWAPI::Orders::MedicHeal || order == BWAPI::Orders::MedicHealToIdle;
//}
//template<bool b = is_bwapi_4, typename std::enable_if<!b>::type* = 0>
//bool bwapi_is_healing_order(BWAPI::Order order) {
//	return order == BWAPI::Orders::MedicHeal1 || order == BWAPI::Orders::MedicHeal2;
//}
bool bwapi_is_healing_order(BWAPI::Order order) {
	return order.getID() == 176 || order.getID() == 179;
}

int latency_frames;
int current_command_frame;

double current_minerals, current_gas;
std::array<double,3> current_used_supply, current_max_supply;
double current_used_total_supply;

double current_minerals_per_frame, current_gas_per_frame;
double predicted_minerals_per_frame, predicted_gas_per_frame;

enum {race_unknown = -1, race_terran = 0, race_protoss = 1, race_zerg = 2};

struct player_t;
struct upgrade_type;
struct unit;
struct unit_stats;
struct unit_type;


#include "ranges.h"
#include "common.h"

a_deque<a_string> send_text_queue;
void send_text(a_string str) {
	if (current_frame < 30 || !send_text_queue.empty()) send_text_queue.push_back(str);
	else game->sendText("%s", str.c_str());
}

namespace square_pathing {
	void invalidate_area(xy from, xy to);
}
namespace wall_in {
	void lift_please(unit*);
	int active_wall_count();
}
a_vector<xy> start_locations;
xy my_start_location;
namespace combat {
	bool can_transfer_through(xy pos);
	bool is_in_defence_choke_outside_squares(xy pos);
	bool is_in_detected_area(xy pos);
	void add_detected_area(xy pos);
}

a_vector<xy> possible_enemy_start_locations;
int last_update_possible_enemy_start_locations = 0;

// namespace patrec {
// 	extern bool execute_as_strat;
// };
// namespace test_stratm {
// 	extern bool execute_as_strat;
// };

//#define USE_COMBAT_EVAL2

// xy test_one;
// xy test_two;
// xy test_three;

#include "multitasking.h"
#include "multitasking_sync.h"
#include "players.h"
#include "grid.h"
#include "stats.h"
#include "render.h"
#include "units.h"
#include "upgrades.h"
#include "square_pathing.h"
#include "flyer_pathing.h"
#include "unit_controls.h"
#include "resource_gathering.h"
#include "resource_spots.h"
#include "creep.h"
#include "pylons.h"
#include "build_spot_finder.h"
#include "build.h"
#include "combat_eval.h"
#ifdef USE_COMBAT_EVAL2
#include "combat_eval2.h"
#endif
#include "buildpred.h"
#include "wall_in.h"
#include "combat.h"
#include "scouting.h"
#include "get_upgrades.h"
#include "threats.h"
#include "tactics.h"
#include "harassment.h"
#include "early_micro.h"
//#include "marine_micro.h"
#include "base_harass_defence.h"
#include "strategy.h"
//#include "patrec.h"
//#include "test_stratm.h"
#include "buildopt.h"
#include "buildopt2.h"
#include "unit_prediction.h"
//#include "openbw_combat_sim.h"

void init() {

	multitasking::init();
	players::init();

	for (auto&v : game->getStartLocations()) {
		start_locations.emplace_back(((bwapi_pos)v).x * 32, ((bwapi_pos)v).y * 32);
	}
	my_start_location = xy(((bwapi_pos)players::self->getStartLocation()).x * 32, ((bwapi_pos)players::self->getStartLocation()).y * 32);

	grid::init();
	stats::init();
	units::init();
	upgrades::init();
	square_pathing::init();
	unit_controls::init();
	resource_gathering::init();
	resource_spots::init();
	creep::init();
	pylons::init();
	build_spot_finder::init();
	build::init();
	buildpred::init();
	combat::init();
	scouting::init();
	get_upgrades::init();
	wall_in::init();
	threats::init();
	tactics::init();
	harassment::init();
	//marine_micro::init();
	base_harass_defence::init();
	strategy::init();
	//patrec::init();
	//test_stratm::init();
	if (players::my_player->race == race_protoss && false) {
		buildopt::init();
	}
	//buildopt2::init();
	unit_prediction::init();
	//openbw_combat_sim::init();
}


double units_distance(unit* a, unit* b) {
	return (double)sc_distance(units_difference(a->pos, a->type->dimensions, b->pos, b->type->dimensions));
	//return units_difference(a->pos,a->type->dimensions,b->pos,b->type->dimensions).length();
}

double units_distance(xy a_pos,unit* a,xy b_pos,unit* b) {
	return (double)sc_distance(units_difference(a_pos, a->type->dimensions, b_pos, b->type->dimensions));
	//return units_difference(a_pos,a->type->dimensions,b_pos,b->type->dimensions).length();
}
double units_distance(xy a_pos,unit_type* at,xy b_pos,unit_type* bt) {
	return (double)sc_distance(units_difference(a_pos, at->dimensions, b_pos, bt->dimensions));
	//return units_difference(a_pos,at->dimensions,b_pos,bt->dimensions).length();
}

double units_pathing_distance(unit* a, unit* b, bool include_enemy_buildings) {
	if (a->type->is_flyer) return units_difference(a->pos, a->type->dimensions, b->pos, b->type->dimensions).length();
	double ud = units_difference(a->pos, a->type->dimensions, b->pos, b->type->dimensions).length();
	if (ud <= 32) return ud;
	auto&map = square_pathing::get_pathing_map(a->type, include_enemy_buildings ? square_pathing::pathing_map_index::no_enemy_buildings : square_pathing::pathing_map_index::default_);
	double d = square_pathing::get_distance(map, a->pos, b->pos);
	if (d > 32 * 4) return d;
	return ud;
}

double units_pathing_distance(unit_type*ut,unit*a,unit*b) {
	if (ut->is_flyer) return units_difference(a->pos, a->type->dimensions, b->pos, b->type->dimensions).length();
	double ud = units_difference(a->pos, a->type->dimensions, b->pos, b->type->dimensions).length();
	if (ud <= 32) return ud;
	auto&map = square_pathing::get_pathing_map(ut);
	double d = square_pathing::get_distance(map, a->pos, b->pos);
	if (d > 32 * 4) return d;
	return ud;
}

double unit_pathing_distance(unit_type*ut,xy from,xy to) {
	if (ut->is_flyer) return (to - from).length();
	auto&map = square_pathing::get_pathing_map(ut, square_pathing::pathing_map_index::no_enemy_buildings);
	return square_pathing::get_distance(map, from, to);
	//return (from-to).length();
}
double unit_pathing_distance(unit*u,xy to) {
	return unit_pathing_distance(u->type,u->pos,to);
}

a_string short_type_name(unit_type*type) {
	if (type->game_unit_type==BWAPI::UnitTypes::Terran_Command_Center) return "CC";
	if (type->game_unit_type==BWAPI::UnitTypes::Terran_Siege_Tank_Tank_Mode) return "Tank";
	a_string&s = type->name;
	if (s.find("Terran ")==0) return s.substr(7);
	if (s.find("Protoss ")==0) return s.substr(8);
	if (s.find("Zerg ")==0) return s.substr(5);
	if (s.find("Terran_") == 0) return s.substr(7);
	if (s.find("Protoss_") == 0) return s.substr(8);
	if (s.find("Zerg_") == 0) return s.substr(5);

	return s;
}

xy get_nearest_available_cc_build_pos(xy pos) {
	resource_spots::spot*s = get_best_score_p(resource_spots::spots, [&](resource_spots::spot*s) {
		if (grid::get_build_square(s->cc_build_pos).building) return std::numeric_limits<double>::infinity();
		double d = unit_pathing_distance(unit_types::scv, pos, s->cc_build_pos);
		if (d == std::numeric_limits<double>::infinity()) d = diag_distance(pos - s->cc_build_pos) + 100000;
		return d;
	}, std::numeric_limits<double>::infinity());
	if (s) return s->cc_build_pos;
	return pos;
}

namespace players {
	void update_upgrades(player_t&p) {

		for (auto&v : upgrades::upgrade_type_container) {
			int level = p.game_player->getUpgradeLevel(v.game_upgrade_type);
			if (level >= v.level) p.upgrades.insert(&v);
			if (p.game_player->hasResearched(v.game_tech_type)) p.upgrades.insert(&v);
		}

	}
}

namespace grid {
	void update_mineral_reserved_task() {
		multitasking::sleep(1);
		while (true) {

			for (auto&v : build_grid) {
				v.mineral_reserved = false;
				v.no_resource_depot = false;
				v.blocked_by_larva_or_egg = false;
				v.blocked_until = 0;
				v.reserved_for_comsat = false;
			}

			for (unit* u : resource_units) {
				if (!u->building) continue;
				if (u->resources<8*(int)my_workers.size()/4) continue;
				for (int y=-3;y<3+u->type->tile_height;++y) {
					for (int x=-3;x<3+u->type->tile_width;++x) {
						get_build_square(u->building->build_pos + xy(x*32,y*32)).no_resource_depot = true;
					}
				}

			}

			for (unit*u : resource_units) {
				if (!u->building) continue;
				if (u->resources==0) continue;

				auto invalidate = [&](unit*u) {
					for (int y = -1; y < u->type->tile_height + 1; ++y) {
						for (int x = -1; x < u->type->tile_width + 1; ++x) {
							get_build_square(u->building->build_pos + xy(x * 32, y * 32)).mineral_reserved = true;
						}
					}
				};
				invalidate(u);
			}

			for (unit*u : my_resource_depots) {
				for (unit*u2 : resource_units) {
					if (units_distance(u, u2) >= 32 * 8) continue;
					xy relpos = u2->pos - u->pos;
					for (int i = 0; i < 8; ++i) {
						get_build_square(u->pos + relpos / 8 * (i + 1)).mineral_reserved = true;
						get_build_square(u->pos + xy(-u->type->dimension_left(), -u->type->dimension_up()) + relpos / 8 * (i + 1)).mineral_reserved = true;
						get_build_square(u->pos + xy(u->type->dimension_right(), -u->type->dimension_up()) + relpos / 8 * (i + 1)).mineral_reserved = true;
						get_build_square(u->pos + xy(u->type->dimension_right(), u->type->dimension_down()) + relpos / 8 * (i + 1)).mineral_reserved = true;
						get_build_square(u->pos + xy(-u->type->dimension_left(), u->type->dimension_down()) + relpos / 8 * (i + 1)).mineral_reserved = true;
					}
				}
			}

			// quick hack to allow space for comsats
			for (unit*u : my_resource_depots) {
				if (u->type != unit_types::cc) continue;
				for (int y = 1; y < 3+1; ++y) {
					for (int x = 4; x < 6+1; ++x) {
						get_build_square(u->building->build_pos + xy(x * 32, y * 32)).reserved_for_comsat = true;
					}
				}
			}

			for (unit*u : visible_units) {
				if (u->type != unit_types::larva && u->type != unit_types::egg && u->type != unit_types::lurker_egg) continue;
				get_build_square(u->pos).blocked_by_larva_or_egg = true;
				get_build_square(u->pos + xy(-u->type->dimension_left(), -u->type->dimension_up())).blocked_by_larva_or_egg = true;
				get_build_square(u->pos + xy(u->type->dimension_right(), -u->type->dimension_up())).blocked_by_larva_or_egg = true;
				get_build_square(u->pos + xy(u->type->dimension_right(), u->type->dimension_down())).blocked_by_larva_or_egg = true;
				get_build_square(u->pos + xy(-u->type->dimension_left(), u->type->dimension_down())).blocked_by_larva_or_egg = true;
			}

			multitasking::sleep(15*10);
		}
	}
}

template<bool is_bwapi_4>
struct x_key;
template<>
struct x_key<true> {
	static const auto value = BWAPI::Key::K_X;
};
template<>
struct x_key<false> {
	static const auto value = 'X';
};

struct module : BWAPI::AIModule {

	virtual void onStart() override {

		log(log_level_info, "game start\n");

		current_frame = game->getFrameCount();

		game->enableFlag(BWAPI::Flag::UserInput);
		game->setCommandOptimizationLevel(2);
		game->setLocalSpeed(0);
		game->setLatCom(false);

		//if (!game->self()) return;

		send_text("tsc-bwai v1.0.19");

		init();

	}

	virtual void onEnd(bool is_winner) override {
		strategy::on_end(is_winner);
#ifdef _MSC_VER
		multitasking::stop();
#endif
	}
  
  a_deque<int> avg_res_hist[3];

	virtual void onFrame() override {

		if (test_mode) {
			static bool holding_x;
			if (game->getKeyState(x_key<is_bwapi_4>::value)) holding_x = true;
			else holding_x = false;
			static bool last_holding_x;
			static bool fast = true;
			if (last_holding_x && !holding_x) {
				fast = !fast;
				if (fast) {
					game->setLocalSpeed(0);
				} else {
					game->setLocalSpeed(30);
				}
			}
			last_holding_x = holding_x;
		}

		current_frame = game->getFrameCount();
		latency_frames = game->getRemainingLatencyFrames();
		//latency_frames = 6;
		current_command_frame = current_frame + latency_frames;
		//if (!game->self()) return;
		
		//if (current_frame >= 15 * 10) *(int*)0x100 = 0x42;

		tsc::high_resolution_timer ht;

		static a_map<multitasking::task_id, double> last_cpu_time;
		for (auto id : multitasking::detail::running_tasks) {
			last_cpu_time[id] = multitasking::get_cpu_time(id);
		}

		if (current_frame - last_update_possible_enemy_start_locations >= 60) {
			last_update_possible_enemy_start_locations = current_frame;
			strategy::update_possible_start_locations();
			possible_enemy_start_locations = strategy::possible_start_locations;
		}

		multitasking::run();
		render::render();

		double elapsed = ht.elapsed();
		if (elapsed >= 1.0 / 20) {
			log(log_level_test, " WARNING: frame took %fs!\n", elapsed);
 			log(log_level_test, " current_task: %s!\n", multitasking::detail::current_task ? multitasking::detail::current_task->name : "none");
// 			for (auto id : multitasking::detail::running_tasks) {
// 				auto&t = multitasking::detail::tasks[id];
// 				log(log_level_test, " - %s took %f\n", t.name, multitasking::get_cpu_time(id) - last_cpu_time[id]);
// 			}
		}

		if (current_frame >= 30 && !send_text_queue.empty()) {
			a_string str = send_text_queue.front();
			send_text_queue.pop_front();
			game->sendText("%s", str.c_str());
		}

// 		game->drawCircleMap(test_one.x, test_one.y, 24, BWAPI::Colors::Red);
// 		game->drawLineMap(test_one.x, test_one.y, test_two.x, test_two.y, BWAPI::Colors::Red);
// 		game->drawCircleMap(test_two.x, test_two.y, 24, BWAPI::Colors::Green);
// 		game->drawLineMap(test_two.x, test_two.y, test_three.x, test_three.y, BWAPI::Colors::Green);
// 		game->drawCircleMap(test_three.x, test_three.y, 24, BWAPI::Colors::Blue);

		if (game->self()) {
			a_vector<std::tuple<int, int, unit_type*>> unit_counts;
			for (auto& t : unit_types::all) {
				auto i = my_units_of_type.find(t);
				int alive = i == my_units_of_type.end() ? 0 : i->second.size();
				int dead = game->self()->deadUnitCount(t->game_unit_type);
				if (alive == 0 && dead == 0) continue;
				unit_counts.emplace_back(-alive, -dead, t);
			}
// 			for (auto& v : my_units_of_type) {
// 				if (v.second.empty() && game->self()->deadUnitCount(v.first->game_unit_type) == 0) continue;
// 				unit_counts.emplace_back(-(int)v.second.size(), v.first);
// 			}
			std::sort(unit_counts.begin(), unit_counts.end());
			int i = 0;
			for (auto& v : unit_counts) {
				int neg_alive;
				int neg_dead;
				unit_type* t;
				std::tie(neg_alive, neg_dead, t) = v;
				game->drawTextScreen(80, 30 + i * 10, "%d - %d %s\n", -neg_alive, -neg_dead, t->name.c_str());
				++i;
			}
		}

//		if (output_enabled) {
//			for (auto* a : combat::live_combat_units) {
//				if (a->copy) {
//					game->drawLineMap(a->u->pos.x, a->u->pos.y, a->copy->u->pos.x, a->copy->u->pos.y, BWAPI::Colors::Green);
//				}
//				if (a->subaction == combat::combat_unit::subaction_fight) {
//					game->drawLineMap(a->u->pos.x, a->u->pos.y, a->target->pos.x, a->target->pos.y, BWAPI::Colors::Red);
//				}
//				if (a->subaction == combat::combat_unit::subaction_kite) {
//					game->drawLineMap(a->u->pos.x, a->u->pos.y, a->target->pos.x, a->target->pos.y, BWAPI::Color(192, 32, 32));
//				}
//			}
//		}

	}

	
	virtual void onUnitShow(BWAPI_Unit gu) override {
		units::on_unit_show(gu);
	}

	virtual void onUnitHide(BWAPI_Unit gu) override {
		units::on_unit_hide(gu);
	}

	virtual void onUnitCreate(BWAPI_Unit gu) override {
		units::on_unit_create(gu);
	}

	virtual void onUnitMorph(BWAPI_Unit gu) override {
		units::on_unit_morph(gu);
	}

	virtual void onUnitDestroy(BWAPI_Unit gu) override {
		units::on_unit_destroy(gu);
	}

	virtual void onUnitRenegade(BWAPI_Unit gu) override {
		units::on_unit_renegade(gu);
	}

	virtual void onUnitComplete(BWAPI_Unit gu) override {
		units::on_unit_complete(gu);
	}

};


bool use_custom_connect = false;

#ifdef _WIN32
struct handle_closer {
	HANDLE h = nullptr;
	handle_closer() = default;
	handle_closer(HANDLE h) : h(h) {}
	~handle_closer() {
		if (h) CloseHandle(h);
	}
};
struct view_unmapper {
	void* ptr = nullptr;
	view_unmapper() = default;
	view_unmapper(void* ptr) : ptr(ptr) {}
	~view_unmapper() {
		if (ptr) UnmapViewOfFile(ptr);
	}
};

// handle_closer custom_connect_shm_game_table_handle;
// handle_closer custom_connect_shm_game_data_handle;
view_unmapper custom_connect_data_handle;
handle_closer custom_connect_pipe_handle;
std::unique_ptr<BWAPI::GameImpl> custom_connect_game;

#endif

bool custom_connect() {
//
//	auto h = OpenFileMappingA(FILE_MAP_READ | FILE_MAP_WRITE, FALSE, "Local\\bwapi_shared_memory_game_list");
//	if (!h) {
//		auto error = GetLastError();
//		if (error == ERROR_FILE_NOT_FOUND) return false;
//		if (error == ERROR_ACCESS_DENIED) {
//			log(log_level_connect, "Access to bwapi shared memory denied. Please run me as administrator.\n");
//			return false;
//		}
//		log(log_level_connect, "OpenFileMapping failed with error %d.\n", error);
//		return false;
//	}
//	handle_closer game_table_handle(h);
//
//	struct GameInstance {
//		uint32_t serverProcessID;
//		uint8_t isConnected;
//		uint32_t lastKeepAliveTime;
//	};
//	struct GameTable {
//		std::array<GameInstance, 8> gameInstances;
//	};
//
//	GameTable* table = (GameTable*)MapViewOfFile(h, FILE_MAP_READ | FILE_MAP_WRITE, 0, 0, sizeof(GameTable));
//	if (!table) {
//		auto error = GetLastError();
//		log(log_level_connect, "MapViewOfFile failed with error %d.\n", error);
//		return false;
//	}
//	view_unmapper table_unmapper(table);
//
//	auto try_connect = [&](DWORD proc_id) {
//
//		HANDLE shm_h = OpenFileMappingA(FILE_MAP_READ | FILE_MAP_WRITE, FALSE, format("Local\\bwapi_shared_memory_%d", proc_id));
//		if (!h) log(log_level_connect, "shm err %d\n", GetLastError());
//		if (!h) return false;
//		handle_closer shm_hc(h);
//
//		HANDLE pipe_h = CreateFileA(format("\\\\.\\pipe\\bwapi_pipe_%d", proc_id), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr);
//		if (pipe_h == INVALID_HANDLE_VALUE) log(log_level_connect, "pipe err %d\n", GetLastError());
//		if (pipe_h == INVALID_HANDLE_VALUE) return false;
//		handle_closer pipe_hc(pipe_h);
//
//		// 17,656,848
//		// 33,017,040
//		void* ptr = MapViewOfFile(shm_h, FILE_MAP_READ | FILE_MAP_WRITE, 0, 0, sizeof(BWAPI::GameData) - 4);
//		if (!ptr) {
//			log(log_level_connect, "MapViewOfFile (game data) failed with error %d.\n", GetLastError());
//			return false;
//		}
//		view_unmapper ptr_unmapper(ptr);
//
//		MEMORY_BASIC_INFORMATION meminfo;
//		VirtualQuery(ptr, &meminfo, sizeof(meminfo));
//
//		custom_connect_game = std::make_unique<BWAPI::GameImpl>((BWAPI::GameData*)ptr);
//		game = custom_connect_game.get();
//		std::swap(custom_connect_data_handle, ptr_unmapper);
//		std::swap(custom_connect_pipe_handle, pipe_hc);
//
//		return true;
//
//	};
//
//	for (auto& v : table->gameInstances) {
//		if (v.isConnected) {
//			log(log_level_connect, "%d is busy\n", v.serverProcessID);
//			continue;
//		}
//		if (try_connect(v.serverProcessID)) {
//			log(log_level_connect, "Successfully connected to %d.\n", v.serverProcessID);
//			//custom_connect_shm_game_table_handle = std::move(game_table_handle);
//			return true;
//		}
//		log(log_level_connect, "Failed to connect to %d.\n", v.serverProcessID);
//	}

	return false;
}

template<bool b = is_bwapi_4, typename std::enable_if<b>::type* = nullptr>
void bwapi_connect() {
	while (!(use_custom_connect ? custom_connect() : BWAPI::BWAPIClient.connect())) std::this_thread::sleep_for(std::chrono::milliseconds(250));
	game = BWAPI::BroodwarPtr;
	//xcept("fixme, i don't think there's a portable way to do this (maybe some trickery with using namespace?)");
}
template<bool b = is_bwapi_4, typename std::enable_if<!b>::type* = nullptr>
void bwapi_connect() {
	//BWAPI::BWAPI_init();
	xcept("is bwapi init necessary?");
	while (!(use_custom_connect ? custom_connect() : BWAPI::BWAPIClient.connect())) std::this_thread::sleep_for(std::chrono::milliseconds(250));
	game = BWAPI::Broodwar;
}

bool is_connected() {
	return BWAPI::BWAPIClient.isConnected();
}
void update() {
	BWAPI::BWAPIClient.update();
}

#define IS_DLL

#ifndef IS_DLL

int main() {

	//strategy::strat_t_new_nn2::static_train();

	log(log_level_info, "connecting\n");
	bwapi_connect();

	log(log_level_info, "waiting for game start\n");
	while (is_connected() && !game->isInGame()) {
		update();
	}
	module m;
	while (is_connected() && game->isInGame()) {
		for (auto&e : game->getEvents()) {
			switch (e.getType()) {
			case BWAPI::EventType::MatchStart:
				m.onStart();
				break;
			case BWAPI::EventType::MatchEnd:
				m.onEnd(e.isWinner());
				break;
			case BWAPI::EventType::MatchFrame:
				m.onFrame();
				break;
			case BWAPI::EventType::UnitShow:
				m.onUnitShow(e.getUnit());
				break;
			case BWAPI::EventType::UnitHide:
				m.onUnitHide(e.getUnit());
				break;
			case BWAPI::EventType::UnitCreate:
				m.onUnitCreate(e.getUnit());
				break;
			case BWAPI::EventType::UnitMorph:
				m.onUnitMorph(e.getUnit());
				break;
			case BWAPI::EventType::UnitDestroy:
				m.onUnitDestroy(e.getUnit());
				break;
			case BWAPI::EventType::UnitRenegade:
				m.onUnitRenegade(e.getUnit());
				break;
			case BWAPI::EventType::UnitComplete:
				m.onUnitComplete(e.getUnit());
				break;
			case BWAPI::EventType::SendText:
				game->sendText("%s", e.getText().c_str());
				break;
			}
		}
		update();
		if (!is_connected()) {
			log(log_level_info, "disconnected\n");
			break;
		}
	}
	log(log_level_info, "game over\n");

	return 0;
}

#else

#ifdef _WIN32
#define DLLEXPORT __declspec(dllexport)
#else
#define DLLEXPORT
#endif

template<bool b = is_bwapi_4, typename std::enable_if<b>::type* = nullptr>
void gameInit_impl(BWAPI::Game* game) {
	BWAPI::BroodwarPtr = game;
	::game = game;
}
template<bool b = is_bwapi_4, typename std::enable_if<!b>::type* = nullptr>
void gameInit_impl(BWAPI::Game* game) {
	BWAPI::Broodwar = game;
	::game = game;
}
template<bool b = is_bwapi_4, typename std::enable_if<b>::type* = nullptr>
BWAPI::AIModule*newAIModule_impl(BWAPI::Game*game) {
	return new module();
}
template<bool b = is_bwapi_4, typename std::enable_if<!b>::type* = nullptr>
BWAPI::AIModule*newAIModule_impl(BWAPI::Game*game) {
	BWAPI::Broodwar = game;
	::game = game;
	return new module();
}

extern "C" DLLEXPORT void gameInit(BWAPI::Game* game) {
	gameInit_impl(game);
	AllocConsole();
	freopen("CONOUT$", "w", stdout);
	freopen("CONOUT$", "w", stderr);
}
//extern "C" __declspec(dllexport) BWAPI::AIModule* newAIModule(BWAPI::Game*game) {
extern "C" DLLEXPORT BWAPI::AIModule* newAIModule() {
	return newAIModule_impl(nullptr);
}

#endif
