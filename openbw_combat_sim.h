#include "/c++/bwgame/tsc-bw/bwgame.h"
#include "/c++/bwgame/tsc-bw/actions.h"

namespace openbw_combat_sim {

void openbw_combat_sim_task() {
	
	a_string map_filename = game->mapPathName();
	
	log(log_level_test, "map filename is '%s'\n", map_filename);
	
	bwgame::game_player player(".");
	bwgame::game_load_functions game_load_funcs(player.st());
	game_load_funcs.load_map_file(map_filename, [&]() {
		game_load_funcs.setup_info.create_no_units = true;
		auto& st = player.st();
		for (auto& v : game->getPlayers()) {
			int id = v->getID();
			st.players.at(id).controller = bwgame::player_t::controller_occupied;
			st.players.at(id).race = (bwgame::race_t)v->getRace().getID();
			st.players.at(id).force = v->getForce()->getID();
		}
	});
	
	while (true) {
		
		tsc::high_resolution_timer ht;
		
		auto base_st = bwgame::copy_state(player.st());
		
		bwgame::state_functions base_funcs(base_st);
		
		a_unordered_map<unit*, bwgame::unit_id> unitmap;
		
		int succeeded = 0;
		int failed = 0;
		for (unit* u : live_units) {
			if (u->gone) continue;
			auto* unit_type = base_funcs.get_unit_type((bwgame::UnitTypes)u->type->id);
			auto* nu = base_funcs.create_unit(unit_type, {u->pos.x, u->pos.y}, u->owner->game_player->getID());
			if (nu) {
				if (base_funcs.unit_type_spreads_creep(unit_type, true) || base_funcs.ut_requires_creep(unit_type)) {
					base_funcs.spread_creep_completely(nu, nu->sprite->position);
				}
				base_funcs.finish_building_unit(nu);
				base_funcs.complete_unit(nu);
				
				for (int i = 0; i != 100; ++i) {
					if (!base_funcs.iscript_execute_sprite(nu->sprite)) xcept("iscript_execute_sprite returned false");
				}
			}
			if (nu) ++succeeded;
			else ++failed;
			unitmap[u] = base_funcs.get_unit_id(nu);
		}
		log(log_level_test, "successfully created %d units (%d failed)\n", succeeded, failed);
		
		for (auto& upg : BWAPI::UpgradeTypes::allUpgradeTypes()) {
			if ((size_t)upg.getID() >= (size_t)bwgame::UpgradeTypes::None) continue;
			for (auto& p : game->getPlayers()) {
				base_st.upgrade_levels.at(p->getID()).at((bwgame::UpgradeTypes)upg.getID()) = p->getUpgradeLevel(upg);
			}
		}
		for (auto& tech : BWAPI::TechTypes::allTechTypes()) {
			if ((size_t)tech.getID() >= (size_t)bwgame::TechTypes::None) continue;
			for (auto& p : game->getPlayers()) {
				base_st.tech_researched.at(p->getID()).at((bwgame::TechTypes)tech.getID()) = p->hasResearched(tech);
			}
		}
		
		struct grp {
			a_vector<combat::combat_unit*> allies;
			a_vector<unit*> enemies;
			
			a_vector<bwgame::unit_id> my_units;
			a_vector<bwgame::unit_id> op_units;
			
			double my_start_hp = 0.0;
			double op_start_hp = 0.0;
			
			double my_fight_stop_hp = 0.0;
			double op_fight_stop_hp = 0.0;
		};
		
		a_vector<grp> groups;
		
		for (auto& g : combat::groups) {
			groups.emplace_back();
			auto& v = groups.back();
			v.allies = g.allies;
			v.enemies = g.enemies;
		}
		
		log(log_level_test, "setup took %gms\n", ht.elapsed() * 1000);
		
//		auto run = [&](auto& g) {
			
			
			

			
//			log(log_level_test, "my hp %g -> %g, op hp %g -> %g\n", my_start_hp, my_stop_hp, op_start_hp, op_stop_hp);
			
//			return (op_start_hp - op_stop_hp) > (my_start_hp - my_stop_hp);
			
//			bool fight = (op_start_hp - op_stop_hp) > (my_start_hp - my_stop_hp) || op_stop_hp == 0.0;
			
//			log("openbw fight %d\n", fight);
			
//			for (auto& v : g.first) {
//				v->openbw_fight = fight;
//			}
			
//			log(log_level_test, "sim ran in %gms\n", ht.elapsed() * 1000);
//		};
		
		ht.reset();
		auto n_st = copy_state(base_st);
		
		bwgame::state_functions funcs(n_st);
		bwgame::action_state action_st;
		bwgame::action_functions actfuncs(n_st, action_st);
		
		for (auto& g : groups) {
			
			g.my_units.reserve(g.allies.size());
			g.op_units.reserve(g.enemies.size());
			
			double my_start_hp = 0.0;
			
			auto set_unit_stats = [&](bwgame::unit_t* nu, unit* u) {
				funcs.set_unit_hp(nu, bwgame::fp8::from_raw((int)(u->hp * 256)));
				funcs.set_unit_shield_points(nu, bwgame::fp8::from_raw((int)(u->shields * 256)));
			};
			
			for (auto& v : g.allies) {
				auto uid = unitmap[v->u];
				if (uid.index() == 0) continue;
				g.my_units.push_back(uid);
				set_unit_stats(funcs.get_unit(uid), v->u);
				my_start_hp += v->u->hp;
			}
			
			double op_start_hp = 0.0;
			
			for (auto& v : g.enemies) {
				auto uid = unitmap[v];
				if (uid.index() == 0) continue;
				g.op_units.push_back(uid);
				set_unit_stats(funcs.get_unit(uid), v);
				op_start_hp += v->hp;
			}
			
			g.my_start_hp = my_start_hp;
			g.op_start_hp = op_start_hp;
		}
		
		int t = 600 / 4;
		
		for (int i = 0; i < t; ++i) {
			
			for (auto& v : groups) {
				for (auto& uid : v.my_units) {
					auto* u = funcs.get_unit(uid);
					if (!u) continue;
					if (u->order_type->id != u->unit_type->return_to_idle && u->order_type->id != u->unit_type->human_ai_idle) continue;
					for (auto& uid2 : v.op_units) {
						auto* u2 = funcs.get_unit(uid2);
						if (!u2) continue;
						if (!funcs.unit_can_attack_target(u, u2)) continue;
						
						actfuncs.action_select(u->owner, u);
						actfuncs.action_order(u->owner, funcs.get_order_type(bwgame::Orders::AttackMove), u2->sprite->position, nullptr, nullptr, false);
						
						log(log_level_test, "attackmove unit of type %d to unit of type %d\n", (int)u->unit_type->id, (int)u2->unit_type->id);
						break;
					}
				}
			}
			funcs.next_frame();
			funcs.next_frame();
			funcs.next_frame();
			funcs.next_frame();
			
			multitasking::yield_point();
		}
		
		auto get_units_hp = [&](auto& units) {
			double r = 0.0;
			for (auto& uid : units) {
				auto* u = funcs.get_unit(uid);
				if (!u) continue;
				r += u->hp.raw_value / 256.0;
			}
			return r;
		};
		
		for (auto& v : groups) {
			v.my_fight_stop_hp = get_units_hp(v.my_units);
			v.op_fight_stop_hp = get_units_hp(v.op_units);
		}
		
		n_st = copy_state(base_st);
		
		for (int i = 0; i < t; ++i) {
			
			for (auto& v : groups) {
				//for (auto& uid : v.my_units) {
				for (auto* a : v.allies) {
					auto uid = unitmap[a->u];
					auto* u = funcs.get_unit(uid);
					if (!u) continue;
					if (u->order_type->id == u->unit_type->return_to_idle || u->order_type->id == u->unit_type->human_ai_idle) continue;
					if (!funcs.u_can_move(u)) continue;
					if (a->last_run_pos == xy() || combat::entire_threat_area.test(grid::build_square_index(a->last_run_pos))) {
						if (!combat::entire_threat_area.test(grid::build_square_index(a->u->pos))) a->last_run_pos = a->u->pos;
					}
					if (diag_distance(xy(u->sprite->position.x, u->sprite->position.y) - a->last_run_pos) <= 32) continue;
					if (u->order_type->id == bwgame::Orders::Move || u->order_type->id == bwgame::Orders::ReaverCarrierMove) continue;
					
					actfuncs.action_select(u->owner, u);
					actfuncs.action_order(u->owner, funcs.get_order_type(bwgame::Orders::Move), {a->last_run_pos.x, a->last_run_pos.y}, nullptr, nullptr, false);
				}
			}
			funcs.next_frame();
			funcs.next_frame();
			funcs.next_frame();
			funcs.next_frame();
			
			multitasking::yield_point();
		}
		
		for (auto& v : groups) {
			double my_run_stop_hp = get_units_hp(v.my_units);
			double op_run_stop_hp = get_units_hp(v.op_units);
			
			log(log_level_test, "fight - my hp %g -> %g, op hp %g -> %g\n", v.my_start_hp, v.my_fight_stop_hp, v.op_start_hp, v.op_fight_stop_hp);
			log(log_level_test, "run   - my hp %g -> %g, op hp %g -> %g\n", v.my_start_hp, my_run_stop_hp, v.op_start_hp, op_run_stop_hp);
			
			double fight_score = (v.op_start_hp - v.op_fight_stop_hp) - (v.my_start_hp - v.my_fight_stop_hp);
			double run_score = (v.op_start_hp - op_run_stop_hp) - (v.my_start_hp - my_run_stop_hp);
			
			log(log_level_test, " scores - fight %g run %g\n", fight_score, run_score);
			
			bool fight = fight_score > run_score || v.op_fight_stop_hp == 0.0;
			
			for (auto* a : v.allies) {
				a->openbw_fight = fight;
			}
		}
		
		log(log_level_test, "sim ran in %gms\n", ht.elapsed() * 1000);
		
		multitasking::sleep(9);
		
	}
	
}

void init() {
	
	multitasking::spawn(openbw_combat_sim_task, "openbw combat sim");
	
}

}
