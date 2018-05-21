
double total_cannon_hp = 0;
int completed_cannon_count = 0;
int lost_probe_count = 0;
bool static_defence_pos_is_at_expo = false;

bool enemy_is_tanks = false;

bool fast_expand = false;
//if (players::opponent_player->race == race_terran) fast_expand = true;

bool logic1 = false;
bool logic2 = false;

bool inited = false;
bool being_zergling_rushed = false;

int total_dts_built = 0;

void strat_p_mod::mod_test_tick() {

//	if (army_supply == 0.0 && probe_count < 18) {
//		scouting::scout_supply = 12.0;
//	} else {
//		scouting::scout_supply = 200.0;
//		rm_all_scouts();
//	}
	
	if (!inited) {
		inited = true;
//		a_string opening = adapt::choose("ffe", "14 nexus", "default opening");
//		if (players::opponent_player->race == race_zerg || players::opponent_player->random) {
//			if (opening == "14 nexus") opening = "ffe";
//		}
//		forge_fast_expand = opening == "ffe";
//		fast_expand = opening == "14 nexus";
		a_string opening;
		if (players::opponent_player->race == race_zerg || players::opponent_player->random) {
			//opening = adapt::choose("ffe", "default opening");
			opening = "ffe";
		} else {
			opening = adapt::choose("14 nexus", "default opening");
		}
		forge_fast_expand = opening == "ffe";
		fast_expand = opening == "14 nexus";
		
//		a_string logic = adapt::choose("logic1", "logic2");
//		logic1 = logic == "logic1";
//		logic2 = logic == "logic2";
		logic1 = true;
		logic2 = false;
		
		if (forge_fast_expand || fast_expand) scouting::no_proxy_scout = true;
	}
	if ((fast_expand || forge_fast_expand) && !my_units_of_type[unit_types::pylon].empty()) {
		if (opening_state != -1) {
			bo_cancel_all();
			opening_state = -1;
		}
	}
	
	if (forge_fast_expand) {
		scouting::scout_supply = 9.0;
		//scouting::scout_supply = 20.0;
		//if (!my_units_of_type[unit_types::forge].empty()) scouting::scout_supply = 10;
	}

	//resource_gathering::max_gas = 1200.0;
	resource_gathering::max_gas = 0.0;
	if (probe_count < 25 && army_supply < 4.0) resource_gathering::max_gas = 150.0;
	if (my_completed_units_of_type[unit_types::cybernetics_core].empty()) {
		resource_gathering::max_gas = 50.0;
		if (players::opponent_player->race == race_terran && current_minerals >= 100) resource_gathering::max_gas = 150.0;
	}
	
	if (enemy_zergling_count >= 4 && current_frame <= 15*60*6) being_zergling_rushed = true;
	if (enemy_zergling_count && current_frame <= 15*60*5) being_zergling_rushed = true;
	if (being_zergling_rushed && army_supply < 6.0) {
		resource_gathering::max_gas = 0.0;
		rm_all_scouts();
	}
	
	//if (enemy_tank_count >= 12 && enemy_tank_count * 2 >= enemy_army_supply / 2) enemy_is_tanks = true;
	if (enemy_tank_count >= 10) enemy_is_tanks = true;
	else if (enemy_tank_count >= 4 && enemy_tank_count * 2 >= enemy_army_supply * 0.5) enemy_is_tanks = true;
	
	auto st = buildpred::get_my_current_state();

	combat::dont_attack_main_army = current_used_total_supply < 180 && army_supply < 70 && current_frame < 15 * 60 * 40;
	//combat::dont_attack_main_army = false;
	if (enemy_worker_count >= 44 || enemy_bases > st.bases.size()) ;//combat::dont_attack_main_army = false;
	else combat::no_aggressive_groups = true;
	combat::attack_bases = true;
	combat::aggressive_groups_done_only = false;
	combat::no_aggressive_groups = false;
	combat::no_aggressive_groups = army_supply < 60.0 && probe_count < 64 && st.bases.size() < 3;
	combat::aggressive_dark_templars = true;

	//get_next_base_check_reachable = probe_count < 45 || army_supply < probe_count;
//	get_next_base_check_reachable = true;
//	get_next_base_check_reachable = get_next_base();
	get_next_base_check_reachable = false;

	//combat::use_map_split_defence = true;
	combat::use_map_split_defence = false;
	if (players::opponent_player->race == race_terran) {
		//combat::dont_attack_main_army = enemy_is_tanks && current_used_total_supply < 180 && army_supply < enemy_army_supply + 60.0;
		combat::dont_attack_main_army = false;
		combat::use_map_split_defence = true;
		combat::no_aggressive_groups = false;
		//if (army_supply >= enemy_army_supply * 2) combat::no_aggressive_groups = false;
		//if (players::my_player->minerals_lost >= players::opponent_player->minerals_lost + 150.0 && army_supply < 80.0) combat::no_aggressive_groups = true;
		//combat::use_control_spots_non_terran = enemy_bases <= 2 && current_used_total_supply < 160;
	}
	if (players::opponent_player->race == race_protoss) {
		combat::dont_attack_main_army = false;
		//combat::no_aggressive_groups = army_supply < enemy_army_supply + 16.0;
		combat::no_aggressive_groups = army_supply < 6.0;
		
		if (army_supply < 16.0 && my_completed_units_of_type[unit_types::dragoon].empty()) combat::force_defence_is_scared_until = current_frame + 30;
		if (army_supply < 10.0) {
			combat::combat_mult_override = 0.5;
			combat::combat_mult_override_until = current_frame + 30;
		}
		if (enemy_proxy_building_count == 0 && enemy_cannon_count >= 3 && enemy_army_supply < army_supply / 2 && army_supply < 80.0) {
			combat::combat_mult_override = 4.0;
			combat::combat_mult_override_until = current_frame + 30;
		}
	}
	if (army_supply <= 4.0 && st.bases.size() == 1) combat::force_defence_is_scared_until = current_frame + 30;
	
//	combat::combat_mult_override = 0.5;
//	combat::combat_mult_override_until = current_frame + 30;
//	combat::no_aggressive_groups = true;
	
	if (players::opponent_player->race == race_zerg) {
//		combat::dont_attack_main_army = false;
//		combat::no_aggressive_groups = false;
		//combat::aggressive_zealots = true;
	}
	
//	combat::no_aggressive_groups = false;
//	combat::dont_attack_main_army = false;
//	combat::use_control_spots_non_terran = false;
	
	//forge_fast_expand = players::opponent_player->race == race_zerg;
	
	merge_high_templars();
	
	//if (forge_fast_expand && my_completed_units_of_type[unit_types::photon_cannon].size() < 3 && current_frame < 15*60*10 && natural_pos != xy()) {
	if (forge_fast_expand && current_frame < 15*60*10 && natural_pos != xy()) {
		//if (is_defending || enemy_army_supply >= 4 || enemy_zergling_count >= 5 || being_zergling_rushed) {
		if (is_defending) {
			int n_probes = 2 + (int)enemy_army_supply + enemy_zergling_count / 2 - (int)my_completed_units_of_type[unit_types::zealot].size();
			n_probes -= my_completed_units_of_type[unit_types::photon_cannon].size() * 2;
			if (n_probes > probe_count / 2) n_probes = probe_count / 2;
			if (being_zergling_rushed && my_completed_units_of_type[unit_types::photon_cannon].empty()) n_probes = probe_count - 4;
			//log(log_level_test, "n_probes is %d\n", n_probes);
			a_unordered_set<combat::combat_unit*> selected;
			int i = 0;
			while (n_probes > 0) {
				++i;
				--n_probes;
				combat::combat_unit* a = get_best_score(combat::live_combat_units, [&](combat::combat_unit* a) {
						if (selected.find(a) != selected.end()) return std::numeric_limits<double>::infinity();
						return unit_pathing_distance(a->u, natural_pos);
				}, std::numeric_limits<double>::infinity());
				if (!a) break;
				//log(log_level_test, "pull probe\n");
				selected.insert(a);
				if (my_completed_units_of_type[unit_types::nexus].size() == 1 && i <= 3) {
					a->strategy_busy_until = current_frame + 15 * 5;
					a->subaction = combat::combat_unit::subaction_move;
					a->target_pos = natural_pos;
					if (!natural_choke.build_squares.empty()) {
						size_t index = tsc::rng(natural_choke.build_squares.size());
						a->target_pos = natural_choke.build_squares[index]->pos + xy(16, 16);
					}
				}
				unit* target = get_best_score(enemy_units, [&](unit* e) {
					auto* w = e->is_flying ? a->u->stats->air_weapon : a->u->stats->ground_weapon;
					if (!w) return std::numeric_limits<double>::infinity();
					auto cannon_in_range = [&]() {
						for (unit* u : my_units_of_type[unit_types::photon_cannon]) {
							if (diag_distance(e->pos - u->pos) <= 32 * 5) return true;
						}
						return false;
					};
					if (!cannon_in_range()) return std::numeric_limits<double>::infinity();
					return diag_distance(e->pos - a->u->pos);
				}, std::numeric_limits<double>::infinity());
				if (target) {
					//log(log_level_test, "pull probe attack\n");
					a->strategy_busy_until = current_frame + 15 * 5;
					a->subaction = combat::combat_unit::subaction_fight;
					a->target = target;
				}
			}
		}
	}
	
	total_cannon_hp = 0;
	completed_cannon_count = 0;
	for (unit* u : my_completed_units_of_type[unit_types::photon_cannon]) {
		total_cannon_hp += u->hp;
		++completed_cannon_count;
	}
	
	lost_probe_count = game->self()->deadUnitCount(unit_types::probe->game_unit_type);
	
	//log(log_level_test, "my lost minerals %g op lost minerals %g\n", players::my_player->minerals_lost, players::opponent_player->minerals_lost);
	log(log_level_test, " can_expand %d force_expand %d\n", can_expand, force_expand);
	
	static_defence_pos_is_at_expo = false;
	if (static_defence_pos != xy()) {
		if (diag_distance(static_defence_pos - my_start_location) >= 32 * 10 && diag_distance(static_defence_pos - natural_pos) >= 32 * 10) {
			static_defence_pos_is_at_expo = true;
		}
	}
	
	if (true) {
		for (auto* a : combat::live_combat_units) {
			if (a->u->type == unit_types::high_templar) a->never_assign_to_aggressive_groups = true;
		}
	}
	
	if (army_supply >= enemy_army_supply * 2) {
		double min_defend_supply = 4.0 * st.bases.size();
	
		double defend_supply = 0.0;
		for (auto* a : combat::live_combat_units) {
			if (a->never_assign_to_aggressive_groups) defend_supply += a->u->type->required_supply;
		}
		log(log_level_info, "defend_supply is %g\n", defend_supply);
	
		if (defend_supply < min_defend_supply && !combat::live_combat_units.empty()) {
			for (int i = 0; i < 10 && defend_supply < min_defend_supply; ++i) {
				auto* a = combat::live_combat_units[tsc::rng(combat::live_combat_units.size())];
				if (a->never_assign_to_aggressive_groups) continue;
				if (a->u->type->is_worker) continue;
				//if (a->u->type != unit_types::vulture && a->u->stats->ground_weapon) {
				if (a->u->stats->ground_weapon) {
					a->never_assign_to_aggressive_groups = true;
					defend_supply += a->u->type->required_supply;
				}
			}
		}
	}

	if (current_used_total_supply >= 180 && current_minerals >= 1000 && current_gas >= 800) {
		for (auto* a : combat::live_combat_units) {
			if (a->never_assign_to_aggressive_groups) a->never_assign_to_aggressive_groups = false;
		}
	}
	
	total_dts_built = my_units_of_type[unit_types::dark_templar].size() + game->self()->deadUnitCount(unit_types::dark_templar->game_unit_type);
	
}

void strat_p_mod::mod_test_build(buildpred::state& st) {

	using namespace unit_types;
	using namespace upgrade_types;
	using namespace buildpred;
	
//	if (st.minerals >= 400) {
//		maxprod(zealot);
//		if ((dragoon_count < zealot_count / 2 || st.gas >= 200) && zealot_count >= enemy_tank_count * 2) build(dragoon);
//		if (enemy_anti_air_army_supply < enemy_army_supply * 0.5) maxprod(carrier);
//		if (probe_count < 80) build(probe);
//		return;
//	}
	
	if (st.used_supply[race_protoss] >= 190.0 && scout_count < 2) {
		build(scout);
		return;
	}

	bool enemy_is_bio = enemy_biological_army_supply >= 8.0 && enemy_biological_army_supply >= enemy_army_supply * 0.5;	

	bool build_army = false;
	if (probe_count >= 24 || is_defending) {
		if (enemy_is_bio || is_defending) {
			if (probe_count >= 34 && army_supply < probe_count && army_supply < std::max(enemy_army_supply * 2, 15.0)) build_army = true;
		}
		if (probe_count >= 34 && army_supply < probe_count / 2) build_army = true;
		if (army_supply < enemy_army_supply) build_army = true;
		if (army_supply < enemy_attacking_army_supply * 1.5) build_army = true;
		if (army_supply >= enemy_army_supply * 0.75 && count_production(st, probe) == 0) build_army = false;
		if (probe_count >= 45 && army_supply < probe_count + 32) build_army = true;
	}

	bool go_reavers = false;
	bool go_corsairs = false;
	
	if (fast_expand && can_expand && st.bases.size() == 1) {
		if (probe_count >= 14) build(nexus);
		else build(probe);
		return;
	}
	
	if (!forge_fast_expand && probe_count >= 12) build_n(assimilator, 1);
	
	if (enemy_is_bio || (enemy_barracks_count >= 2 && reaver_count == 0)) go_reavers = true;
	if (reaver_count >= 4) go_reavers = false;
	if (players::opponent_player->race == race_zerg) {
		//if (enemy_hydralisk_count && reaver_count < 4) go_reavers = true;
		
		go_corsairs = corsair_count < 2;
		if (enemy_spire_count && corsair_count < 4) go_corsairs = true;
		if (enemy_mutalisk_count && corsair_count < 6) go_corsairs = true;
		if (enemy_ground_army_supply > ground_army_supply) go_corsairs = false;
		
		if (army_supply == 0.0) build_army = true;
		
		st.dont_build_refineries = probe_count < (count_units_plus_production(st, assimilator) == 0 ? 19 : 27);
		
		if (count_units_plus_production(st, templar_archives)) {
			build_n(gateway, 6);
		}
	}
	
	if (players::my_player->minerals_lost >= 200 && army_supply < 16.0) {
		go_corsairs = false;
		go_reavers = false;
	}
	//go_reavers = false;
	
	if (probe_count >= 36 && count_production(st, probe)) build_army = true;
	
	if (enemy_is_tanks && enemy_attacking_army_supply < enemy_army_supply * 0.5 && army_supply > enemy_army_supply) {
		//if (army_supply >= probe_count && probe_count < 70) build_army = false;
	}
	
	if (players::opponent_player->race == race_protoss) {
		if (probe_count >= 20 && army_supply < probe_count) build_army = true;
		if (army_supply >= 4.0 && (army_supply > enemy_attacking_army_supply || reaver_count)) go_reavers = true;
		if (army_supply >= std::max(enemy_army_supply * 2, 20.0)) go_reavers = false;
	}

	bool build_probes = !build_army;
	
	if (st.minerals >= 200 && count_production(st, probe)) build_army = true;

	bool tech_up = false;
	if (army_supply >= enemy_army_supply * 1.25) tech_up = true;
	if (probe_count >= 50 && army_supply >= enemy_army_supply) tech_up = true;
	
	if (probe_count >= 66) {
		build_probes = false;
		build_army = true;
		tech_up = true;
	}
	
	//if (st.frame == current_frame) log(log_level_test, "go_reavers %d, go_corsairs %d, build_probes %d, build_army %d, tech_up %d\n", go_reavers, go_corsairs, build_probes, build_army, tech_up);

	if (probe_count < 55 && army_supply >= probe_count && count_production(st, probe) == 0) {
		build_probes = true;
		build_army = false;
	}
	
	bool go_dragoons = players::opponent_player->race != race_zerg;
	bool go_high_templars = players::opponent_player->race == race_zerg;
	bool go_carriers = false;
	bool go_arbiters = false;
	bool go_scouts = false;
	if (players::opponent_player->race == race_terran && logic1) {
		//if (enemy_attacking_army_supply < army_supply / 2 || army_supply >= 40.0) go_arbiters = true;
		//if (enemy_tank_count >= enemy_goliath_count && (enemy_anti_air_army_supply <= std::max(enemy_army_supply * 0.33, 8.0))) {
		//if (army_supply > enemy_army_supply + 8.0 && enemy_anti_air_army_supply <= std::max(enemy_army_supply * 0.33, 8.0)) {
		if ((army_supply > enemy_army_supply + 8.0 || probe_count >= 30) && enemy_anti_air_army_supply <= std::max(enemy_army_supply * 0.33, 8.0)) {
			if (scout_count < 12) go_scouts = true;
		} else {
			go_reavers = true;
		}
	}
//	if (players::opponent_player->race != race_zerg) {
//		if (army_supply >= enemy_army_supply + 32.0) go_carriers = true;
//		if (carrier_count >= 2) go_carriers = true;
//		if (enemy_anti_air_army_supply >= enemy_army_supply / 2) {
//			if (!has_upgrade(st, protoss_air_weapons_2) && army_supply < enemy_army_supply * 2) go_carriers = false;
//		}
//		if (enemy_is_tanks && army_supply >= enemy_attacking_army_supply + 16.0) go_carriers = true;
//	}
	if (enemy_is_tanks) go_dragoons = false;
	if (players::opponent_player->race == race_terran) {
		if (zealot_count < enemy_tank_count * 2) go_dragoons = false;
		else go_dragoons = true;
	}
	if (players::opponent_player->race == race_terran && zealot_count < dragoon_count - 10) go_dragoons = false;
	
	if (players::opponent_player->race == race_terran && probe_count >= 50) {
		//if (army_supply >= enemy_attacking_army_supply + 16.0) go_arbiters = true;
		if (!enemy_is_bio) go_arbiters = true;
	}
	if (players::opponent_player->race == race_terran && logic2 && (army_supply > enemy_army_supply || probe_count >= 28)) {
		go_arbiters = true;
		if (reaver_count < arbiter_count) go_reavers = true;
		else if ((zealot_count >= 20 || probe_count >= 48) && high_templar_count < 3) go_high_templars = true;
	}
	
	if (players::opponent_player->race == race_zerg) {
		go_high_templars = false;	
		go_reavers = true;
		if (corsair_count + scout_count < shuttle_count * 2) {
			go_corsairs = true;
//			if (corsair_count < enemy_air_army_supply + 2) go_corsairs = true;
//			else go_scouts = true;
		}
	}
	if (players::opponent_player->race == race_protoss) {
		go_reavers = army_supply >= 8.0;
		//go_high_templars = army_supply >= 30.0;
		
		if (ground_army_supply >= enemy_ground_army_supply * 2 + 8.0 && scout_count < enemy_stargate_count + enemy_carrier_count * 3 + enemy_cannon_count / 2) {
			//go_scouts = true;
			//go_high_templars = true;
		}
		if (enemy_air_army_supply >= enemy_army_supply / 2) go_high_templars = true;
	}
//	if (players::opponent_player->race == race_terran) {
//		go_high_templars = false;
//		go_reavers = true;
//		if (scout_count < shuttle_count * 2) go_scouts = true;
//	}
	
	if (enemy_cannon_count && army_supply >= enemy_army_supply * 2 + 8.0) {
		if (probe_count < 40) {
			maxprod(dragoon);
			upgrade(protoss_ground_armor_1) && upgrade(protoss_ground_armor_2);
			build(high_templar);
			upgrade(psionic_storm);
			build(reaver);
			build_n(shuttle, reaver_count);
			if (probe_count < (reaver_count >= 2 || st.minerals >= 350 ? 40 : 24)) build(probe);
			if (count_production(st, unit_types::nexus) == 0 && can_expand && probe_count >= max_mineral_workers) {
				build(nexus);
			}
			return;
		}
//		if (probe_count < 40) {
//			maxprod(dragoon);
//			upgrade(protoss_ground_armor_1) && upgrade(protoss_ground_armor_2);
//			build(probe);
//			if (count_production(st, unit_types::nexus) == 0 && can_expand && probe_count >= max_mineral_workers) {
//				build(nexus);
//			}
//			return;
//		}
	}
	
	if (army_supply >= 70.0) {
		//go_reavers = enemy_ground_army_supply > enemy_army_supply * 0.5;
		//go_high_templars = true;
	}
	
	if (probe_count >= 30) {
		build_n(gateway, 4);
		if (!go_dragoons) build(zealot);
		else build(dragoon);
	}
	if (enemy_army_supply < 2.0 && count_units_plus_production(st, assimilator)) build_n(cybernetics_core, 1);

	if (logic2 && probe_count >= 28 && probe_count < 36 && total_dts_built < 4) {
		if (probe_count < 32) build(probe);
		maxprod(dragoon);
		build_n(dark_templar, 4);
	} else if (build_probes) build(probe);
	
	if (logic2 && ((players::opponent_player->race == race_terran && army_supply > enemy_attacking_army_supply) || st.minerals >= 400)) {
		if (probe_count >= 20 && can_expand && count_production(st, nexus) == 0 && st.bases.size() == 1) build(nexus);
	}
	
	if ((players::opponent_player->race == race_terran && is_defending && total_dts_built < 4)) {
		build_n(dark_templar, 4);
	}

	if (probe_count >= 28 && (players::opponent_player->race == race_zerg || army_supply >= 40.0)) {
		upgrade(leg_enhancements);
		upgrade_wait(protoss_ground_weapons_1) && upgrade_wait(protoss_ground_weapons_2);
	}

	if (go_corsairs) build_n(stargate, 1);

	if (build_army || st.minerals >= 400) {
		if (st.minerals >= 400 || (st.bases.size() >= 2 && st.minerals >= 200)) maxprod(zealot);
		if (go_dragoons) build(dragoon);
		else build(zealot);
		//if (is_defending && enemy_hydralisk_count == 0 && count_units_plus_production(st, cybernetics_core)) build(dragoon);
		if (go_reavers && reaver_count < 15) build(reaver);
		if (go_corsairs && corsair_count < 15) build(corsair);
		
		if (go_high_templars && has_or_is_upgrading(st, psionic_storm)) {
			build_n(high_templar, (int)(army_supply / 10.0));
		}
		
//		if (army_supply >= 65.0) {
//			build_n(dark_templar, 3);
//		}

		if (players::opponent_player->race == race_zerg && probe_count >= 45) {
//			if (enemy_ground_army_supply >= enemy_air_army_supply * 2) {
//				build(reaver);
//				if (high_templar_count < reaver_count * 2 || st.gas >= 350) build(high_templar);
//				build_n(robotics_facility, 2);
//			}
			
//			if (army_supply >= enemy_army_supply * 2) {
//				build_n(corsair, 6);
//				//if (dark_templar_count < corsair_count) build(dark_templar);
//			}
		}
		
		if (players::opponent_player->race == race_zerg && probe_count >= 30 && !go_reavers) upgrade(leg_enhancements);
		if (dragoon_count && players::opponent_player->race == race_terran) upgrade(singularity_charge);
		else if (dragoon_count >= 4) upgrade(singularity_charge);
	}
	
//	if (count_units_plus_production(st, templar_archives)) {
//		if (players::opponent_player->race == race_zerg && army_supply < enemy_army_supply + 16) {
//			build_n(archon, 3);
//		}
//	}
	
	if (army_supply >= 30.0) {
		build_n(observer, (int)army_supply / 20.0);
	}

//	if (probe_count < 40) {
//		if (dragoon_count < enemy_vulture_count + enemy_goliath_count * 2) {
//			build(dragoon);
//		}
//	}

	if (go_carriers) {
		if (build_army) maxprod(carrier);
		upgrade(protoss_air_weapons_2);
		upgrade(carrier_capacity);
	}
	if (go_arbiters) {
		if (build_army && arbiter_count < army_supply / 10.0 && (!go_carriers || carrier_count >= arbiter_count * 2)) {
			if (army_supply >= 70.0) build_n(stargate, 2);
			build(arbiter);
		}
		upgrade(stasis_field) && upgrade(recall);
	}
	//maxprod(scout);
	if (go_scouts && (!go_arbiters || scout_count < arbiter_count * 2)) {
		build_n(stargate, 3);
		build(scout);
		if (army_supply >= 8.0) upgrade(gravitic_thrusters) && upgrade(apial_sensors);
		upgrade(protoss_air_weapons_1);
	}
	
	if (players::opponent_player->race == race_protoss) {
		if (logic1) {
			if (army_supply >= 30.0 && army_supply > enemy_army_supply && scout_count < 8) {
				//build(scout);
				//upgrade(gravitic_thrusters) && upgrade(apial_sensors);
			}
		} else {
			if (probe_count < 40) {
				build(dark_templar);
				if (reaver_count < dark_templar_count / 2) build(reaver);
				if (probe_count < 20 || count_production(st, probe) == 0) build(probe);
			}
		}
	}
	
//	if (corsair_count && probe_count < 55) {
//		build_n(dark_templar, 2);
//	}
	if (go_high_templars && has_or_is_upgrading(st, psionic_storm) && cannon_count >= 2) {
		build_n(high_templar, 3);
	}
	if (enemy_hydralisk_count && army_supply >= enemy_army_supply + 16) {
		build_n(reaver, 2);
	}
	
	if (enemy_dt_count + enemy_lurker_count || (players::opponent_player->race == race_protoss && army_supply >= enemy_army_supply + 8.0)) {
		build_n(observer, 1 + (enemy_dt_count * 2 + enemy_lurker_count) / 6);
	}
	if (players::opponent_player->race == race_protoss) {
		if (count_units_plus_production(st, cybernetics_core)) {
			build_n(gateway, 2);
			if (army_supply < enemy_army_supply + 4.0) build(dragoon);
		} else if (zealot_count < enemy_zealot_count * 2) maxprod(zealot);
	}
	
//	if (enemy_is_tanks) {
//		//if (zealot_count < enemy_tank_count * 2) maxprod(zealot);
//		maxprod(zealot);
//		maxprod(arbiter);
//	}

	if (corsair_count * 2 + archon_count * 2 + scout_count * 2 + dragoon_count * 2 < enemy_air_army_supply) {
		build_n(dragoon, ((int)enemy_air_army_supply + 1) / 2);
		if (go_scouts) build(scout);
	}

	if (tech_up) {
		if (probe_count >= 44) {
			//build_n(fleet_beacon, 1);
			//upgrade_wait(protoss_air_armor_1) && upgrade_wait(protoss_air_armor_2) && upgrade_wait(protoss_air_armor_3);
			//upgrade_wait(protoss_air_weapons_1) && upgrade_wait(protoss_air_weapons_2) && upgrade_wait(protoss_air_weapons_3);
			build_n(forge, 2);
			upgrade_wait(protoss_ground_armor_1) && upgrade_wait(protoss_ground_armor_2) && upgrade_wait(protoss_ground_armor_3);
			if (has_upgrade(st, protoss_ground_weapons_2)) upgrade(protoss_ground_weapons_3);
			else if (has_upgrade(st, protoss_ground_weapons_1)) upgrade(protoss_ground_weapons_2);
			else upgrade(protoss_ground_weapons_1);
		}

		if (go_high_templars && probe_count >= 28) {
			upgrade(psionic_storm);
		}

		if (probe_count >= 24) {
			if (go_high_templars) build_n(templar_archives, 1);
			if (go_corsairs) upgrade(protoss_air_weapons_1);
		}
	}
	
	if (go_corsairs && corsair_count < 2) build_n(corsair, 2);
	
	if (static_defence_pos != xy()) {
		if (st.bases.size() >= 4 || static_defence_pos_is_at_expo) build_n(photon_cannon, st.bases.size() * 3 + 3);
	}

	if (forge_fast_expand) {
		if (probe_count >= 24 && st.bases.size() == 2) {
			if (!opponent_has_expanded || enemy_army_supply > army_supply) {
				build_n(photon_cannon, 3);
			}
		}
	}

	if (go_reavers && (probe_count >= 34 || army_supply > enemy_army_supply)) {
		if (reaver_count) {
			if (count_units_plus_production(st, shuttle) < my_units_of_type[reaver].size()) build_n(shuttle, reaver_count);
			if (probe_count >= 34) {
				upgrade(gravitic_drive);
				build_n(robotics_facility, 2);
			}
		}
	}
	
	if (army_supply == 0.0 && gateway_count) {
		build_n(zealot, 1);
	}
	
	if (enemy_is_tanks && enemy_attacking_army_supply < army_supply * 0.33 && army_supply > enemy_army_supply) {
		if (army_supply >= enemy_attacking_army_supply + 16 && probe_count < 80 && army_supply >= probe_count) {
			build(probe);
			if (probe_count >= 30 && st.bases.size() < 3 && can_expand) build(nexus);
		}
	}
	
	if (count_production(st, unit_types::nexus) == 0 && force_expand) {
		build(nexus);
	}

	if (count_production(st, unit_types::nexus) == 0 && (!is_defending || army_supply > std::max(enemy_army_supply, 30.0) + 8.0)) {
		bool expand = false;
		//if (players::opponent_player->race != race_zerg) {
		if (players::opponent_player->race == race_terran) {
			//if (can_expand && probe_count >= max_mineral_workers * (enemy_is_tanks ? 0.5 : 0.75) && army_supply >= enemy_army_supply + 16.0) {
			if (can_expand && probe_count >= max_mineral_workers * (enemy_is_tanks ? 0.75 : 1.0) && army_supply >= enemy_army_supply + 16.0) {
				expand = true;
			}
		}
		if (can_expand && probe_count >= max_mineral_workers * 1.25) {
			expand = true;
		}
		
		if (fast_expand) {
			if (can_expand && st.bases.size() == 1 && probe_count >= 15) {
				expand = true;
			}
		}
		if (expand) {
			build(nexus);
		}
	}
	
//	if (probe_count >= 18 && (enemy_zergling_count > 6 || players::opponent_player->minerals_lost >= 200)) {
//		build_n(gateway, 2);
//		build_n(zealot, 4);
//	}
	
//	if (current_frame < 15*60*10 && fast_expand && st.bases.size() == 2 && army_supply < enemy_army_supply) {
//		if (probe_count >= 18 && players::opponent_player->race == race_terran) {
//			build(zealot);
//			maxprod(dragoon);
//		}
//	}
	
	if (forge_fast_expand && army_supply < 16.0) {
		build_n(cybernetics_core, 1);
		build_n(gateway, 1);
		build_n(assimilator, 1);
		build_n(probe, 18);
		if (probe_count >= 20 && players::opponent_player->minerals_lost >= 200) build_n(photon_cannon, 4);
		build_n(photon_cannon, 2);
		build_n(nexus, 2);
		if (my_units_of_type[forge].empty() || count_units_plus_production(st, nexus) >= 2) build_n(pylon, 2);
		if (enemy_army_supply || !opponent_has_expanded) build_n(photon_cannon, 2);
		else build_n(photon_cannon, 1);
		build_n(probe, 13);
		build_n(forge, 1);
		build_n(probe, 10);
		if (enemy_army_supply >= 6.0) build_n(photon_cannon, 4);
		else if (enemy_hydralisk_den_count) build_n(photon_cannon, 3);
		else if (enemy_zergling_count >= 5) build_n(photon_cannon, 3);
		if (total_cannon_hp < completed_cannon_count * 100) {
			build_n(photon_cannon, total_cannon_hp >= 200 ? 3 : 4);
			if (enemy_army_supply >= 6.0) build_n(photon_cannon, total_cannon_hp >= 300 ? 4 : 5);
		}
		if (total_cannon_hp < players::my_player->minerals_lost + players::opponent_player->minerals_lost) {
			build_n(photon_cannon, 5);
		}
		//build_n(photon_cannon, 5);
		//build_n(probe, 15);
	} else if (players::opponent_player->race == race_zerg || players::opponent_player->random) {
		if (zealot_count < 2) build(zealot);
	}
//	if (probe_count >= 20) {
//		build_n(photon_cannon, 15);
//		if (cannon_count >= 6) {
//			//upgrade(psionic_storm);
//			//build_n(high_templar, 10);
//			//build_n(reaver, 4);
//			//if (count_production(st, probe) == 0) build_n(probe, 30);
//			maxprod(scout);
//			upgrade(protoss_air_weapons_1) && upgrade(protoss_air_weapons_2);
//		}
//	} else if (cannon_count >= 4) build(probe);
	if (fast_expand && players::opponent_player->race == race_protoss && zealot_count < 6) {
		build(cybernetics_core);
		if (gateway_count < 3) build(gateway);
		build(zealot);
	}

	if ((enemy_attacking_worker_count || probe_count >= 14) && probe_count < 20) {
		build_n(zealot, enemy_attacking_worker_count / 4);
	}
	
	if (pylon_pos != xy() && count_production(st, pylon) == 0) build(pylon);

}
