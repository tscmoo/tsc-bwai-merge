
namespace optc {
	int e_vulture_count = 0;
	int e_goliath_count = 0;
	int e_tank_count = 0;
	double e_army_supply = 0.0;
	double e_biological_army_supply = 0.0;
	int e_worker_count = 0;
}

int total_zerglings_made = 0;
int total_drones_made = 0;
int total_scourge_made = 0;
int total_mutalisks_made = 0;
int total_lurkers_made = 0;
int total_hydralisks_made = 0;

int lose_count = 0;
int total_count = 0;

bool t3_inited = false;
bool ling_rush = false;
bool build_lurkers = false;
bool mass_mutas_vs_bio = false;
bool fast_hive_tech = false;

bool ling_attack = false;

bool is_losing_an_overlord = false;

int stop_attacking_counter = 0;
int next_attack = 0;

bool switch_to_hydras = false;

bool pool_first = false;

void strat_z_mod::mod_test3_tick() {
	
	buildpred::enable_verbose_build_logging = current_minerals >= 1000;

	if (!t3_inited) {
		t3_inited = true;
//		ling_rush = adapt::choose_bool("ling rush");
//		build_lurkers = adapt::choose_bool("build lurkers");
//		mass_mutas_vs_bio = adapt::choose_bool("mass mutas vs bio");
//		fast_hive_tech = adapt::choose_bool("fast hive tech");
		
		//if (players::opponent_player->race == race_zerg) pool_first = true;
		//else pool_first = adapt::choose_bool("pool first");
	}

	if (opening_state != -1) bo_cancel_all();
	if (players::opponent_player->race == race_terran) {
		scouting::scout_supply = 12.0;
		if (opponent_has_expanded || enemy_proxy_building_count) {
			if (current_used_total_supply < 40) rm_all_scouts();
		}
	} else if (players::opponent_player->race == race_protoss) {
		scouting::scout_supply = 12.0;
		if (opponent_has_expanded || enemy_proxy_building_count) {
			if (current_used_total_supply < 40) rm_all_scouts();
		}
	} else {
		if (current_used_total_supply < 40) rm_all_scouts();
	}
	
	combat::dont_attack_main_army = current_used_total_supply < 180 && army_supply < 70 && current_frame < 15 * 60 * 40;
	//combat::dont_attack_main_army = false;
	combat::aggressive_groups_done_only = false;
	combat::no_aggressive_groups = false;
	//combat::no_aggressive_groups = !is_attacking;
	
	combat::use_map_split_defence = army_supply >= 30.0 || drone_count >= 40;
	
	if (drone_count < 20 && total_zerglings_made < 6 && !opponent_has_expanded) combat::no_aggressive_groups = true;
	
	if (players::opponent_player->race == race_zerg) {
		combat::aggressive_zerglings = false;
		//combat::no_aggressive_groups = army_supply < enemy_army_supply + 10.0 && !is_defending && total_drones_made < 20;
		combat::no_aggressive_groups = total_zerglings_made + total_drones_made * 2 < 30 || army_supply < enemy_army_supply + 10.0;
		combat::aggressive_groups_done_only = enemy_army_supply < army_supply / 2 && total_zerglings_made < 20;
		combat::dont_attack_main_army = false;
	}
	
	if (players::opponent_player->race == race_terran) {
		//combat::no_aggressive_groups = army_supply < 70.0 && enemy_attacking_army_supply < enemy_army_supply / 2 && current_frame < 15 * 60 * 12;
	}
	
//	if (current_frame < 15 * 60 * 15) {
//		if (drone_count < 48 && enemy_attacking_army_supply < 8.0) combat::no_aggressive_groups = true;
//	}
	
	if (is_attacking) {
		if (lose_count >= total_count - total_count / 4) {
			++stop_attacking_counter;
			if (stop_attacking_counter >= 1) {
				is_attacking = false;
				next_attack = current_frame + 15 * 60 * 2;
			}
		}
	} else {
		if (current_frame >= next_attack) {
			stop_attacking_counter = 0;
			is_attacking = true;
			
//			for (auto& g : combat::groups) {
//				for (auto* a : g.allies) {
//					a->force_attack = true;
//				}
//			}
		}
		
//		if (!is_attacking) {
//			for (auto& g : combat::groups) {
//				for (auto* a : g.allies) {
//					if (current_frame - a->last_fight <= 15 * 10) {
//						if (a->last_run >= a->last_fight) a->force_attack = false;
//					}
//				}
//			}
//		}
	}

	if (hatch_count >= 3 && army_supply > enemy_army_supply) min_bases = 4;
	//min_bases = 5;

	//get_next_base_check_reachable = drone_count < 45 || army_supply < drone_count;
	get_next_base_check_reachable = drone_count < 20;
	
	if (players::opponent_player->race == race_terran) {
		if (!my_units_of_type[unit_types::spire].empty() && total_mutalisks_made < 9 && drone_count < 40) {
			//combat::no_aggressive_groups = true;
		}
	}

	combat::use_map_split_defence = players::opponent_player->race != race_zerg;
//	if (mutalisk_count == 0 && army_supply < enemy_army_supply * 1.5 && drone_count < 30) {
//		combat::use_map_split_defence = false;
//	}
	
//	if (ling_rush && total_zerglings_made < 30) {
//		combat::aggressive_zerglings = false;
//		combat::no_aggressive_groups = true;
//	}

	if (total_zerglings_made >= 8) {
		//combat::dont_attack_main_army = false;
		if (current_frame < 15*60*12) {
			//combat::combat_mult_override_until = current_frame + 30;
			//combat::combat_mult_override = 8.0;
			if (total_zerglings_made >= 30 && lose_count >= total_count / 2) ling_attack = true;
			//if (ling_attack) combat::combat_mult_override = 0.5;
			
			//if (is_defending) combat::combat_mult_override = 0.5;
		}
	}
	
	if (players::opponent_player->race == race_zerg) {
		if (total_zerglings_made < 26) {
			//combat::combat_mult_override_until = current_frame + 30;
			//combat::combat_mult_override = 0.0;
		} else {
			//combat::combat_mult_override_until = 0;
			combat::no_aggressive_groups = false;
		}
	}
	
	if (drone_count >= 20) {
		size_t n = buildpred::get_my_current_state().bases.size();
		if (n) {
			//combat::combat_mult_override_until = current_frame + 60;
			//combat::combat_mult_override = (double)(buildpred::get_op_current_state().bases.size() + 1) / n;
		}
	}

//	if (players::opponent_player->race == race_zerg) {
//		if (total_mutalisks_made == 0) {
//			combat::use_map_split_defence = false;
//			combat::no_aggressive_groups = current_frame < 15 * 60 * 6;
//			combat::combat_mult_override_until = 0;
//			combat::combat_mult_override = 0.0;
//		}
//	}
	
	total_zerglings_made = my_units_of_type[unit_types::zergling].size() + game->self()->deadUnitCount(unit_types::zergling->game_unit_type);
	total_drones_made = my_units_of_type[unit_types::drone].size() + game->self()->deadUnitCount(unit_types::drone->game_unit_type);
	total_scourge_made = my_units_of_type[unit_types::scourge].size() + game->self()->deadUnitCount(unit_types::scourge->game_unit_type);
	total_mutalisks_made = my_units_of_type[unit_types::mutalisk].size() + game->self()->deadUnitCount(unit_types::mutalisk->game_unit_type);
	total_lurkers_made = my_units_of_type[unit_types::lurker].size() + game->self()->deadUnitCount(unit_types::lurker->game_unit_type);
	total_hydralisks_made = my_units_of_type[unit_types::hydralisk].size() + game->self()->deadUnitCount(unit_types::hydralisk->game_unit_type);
	
	lose_count = 0;
	total_count = 0;
	for (auto* a : combat::live_combat_units) {
		if (a->u->type->is_worker) continue;
		if (!a->u->stats->ground_weapon && !a->u->stats->air_weapon) continue;
		++total_count;
		if (current_frame - a->last_fight <= 15 * 10) {
			if (a->last_run >= a->last_fight) ++lose_count;
		}
	}
	
	//min_bases = 3;
	if (hatch_count >= 7) min_bases = 8;
	//resource_gathering::max_gas = 3000.0;
	
	log(log_level_test, "drones in production: %d\n", buildpred::count_production(buildpred::get_my_current_state(), unit_types::drone));
	
//	if (my_workers.size() >= 60) {
//		if (lose_count >= total_count * 0.75) {
//			combat::combat_mult_override_until = current_frame + 300;
//			combat::combat_mult_override = 0.25;
//		}
//	}
	
	is_losing_an_overlord = false;
	
	for (unit* u : my_units_of_type[unit_types::overlord]) {
		if (u->hp <= u->stats->hp / 2) is_losing_an_overlord = true;
	}
	
	if (current_used_total_supply - my_workers.size() >= 20 || enemy_attacking_army_supply >= 8.0) {
		for (auto*a : combat::live_combat_units) {
			if (!a->u->type->is_flyer && !a->u->type->is_worker) a->use_for_drops_until = current_frame + 15 * 10;
		}
	}
	
	log(log_level_test, "%g / 2 + %g + %d * 4 + %d * 4 + %d\n", enemy_army_supply, enemy_attacking_army_supply, enemy_proxy_building_count, enemy_gateway_count, enemy_attacking_worker_count);
	
	log("%g < %g ?\n", army_supply, enemy_army_supply / 2 + enemy_attacking_army_supply + enemy_proxy_building_count * 4 + enemy_gateway_count * 4 + enemy_attacking_worker_count);
	if (drone_count < 20 && hydralisk_count == 0 && army_supply < enemy_army_supply / 2 + enemy_attacking_army_supply + enemy_proxy_building_count * 4 + enemy_gateway_count * 4 + enemy_attacking_worker_count) log(log_level_test, " build lings!\n");
	
	
	if (enemy_vulture_count >= 1 && drone_count < 20) switch_to_hydras = true;
	if (enemy_vulture_count >= 3 && army_supply < 30.0) switch_to_hydras = true;
	if (enemy_vulture_count >= 4 && enemy_vulture_count * 2 >= enemy_army_supply / 3) switch_to_hydras = true;
	
	if (players::opponent_player->race == race_terran) {
		//if (drone_count >= 40 && army_supply > enemy_army_supply) enable_auto_upgrades = true;
		if (drone_count >= 60 && army_supply > enemy_army_supply) enable_auto_upgrades = true;
	}
}

void strat_z_mod::mod_test3_build(buildpred::state& st) {

	using namespace unit_types;
	using namespace upgrade_types;
	
//	st.auto_build_hatcheries = false;

//	if (drone_count >= 12 && hatch_count == 1) build(hatchery) ;
//	else build(drone);
	
	bool build_army = false;
	
	st.dont_build_refineries = drone_count < 20 && count_units_plus_production(st, extractor) != 0;
	
	st.auto_build_hatcheries = drone_count >= 12;
	
	if (st.minerals >= 400) build_n(drone, 30);
	
	if (pool_first && total_zerglings_made < 6) {
		build_n(zergling, 12);
		build_n(drone, 12);
		build_n(zergling, 6);
		build_n(drone, 11);
		build_n(overlord, 2);
		build_n(spawning_pool, 1);
		build_n(drone, 9);
		return;
	}
	
	if (players::opponent_player->race == race_protoss) {
		
		st.dont_build_refineries = count_units_plus_production(st, extractor) != 0 && drone_count < 36 && hydralisk_count == 0;
		
		if (drone_count < 14) {
			if (total_drones_made > 12 || larva_count >= std::min(completed_hatch_count * 3, 6) || players::opponent_player->race == race_terran) build_n(drone, 14);
			if (count_units_plus_production(st, creep_colony)) build(sunken_colony);
			if (!is_defending && enemy_army_supply > 0 && my_completed_units_of_type[unit_types::hatchery].size() >= 2) build_n(sunken_colony, 1);
			if (players::opponent_player->race != race_protoss) build_n(extractor, 1);
			if (enemy_zealot_count >= 5 || players::opponent_player->race == race_zerg) build_n(zergling, 18);
			if (is_defending && enemy_zealot_count) build_n(zergling, enemy_zealot_count * 5);
			if (is_defending && army_supply < enemy_army_supply) build(zergling);
			//if (count_units_plus_production(st, extractor)) upgrade(metabolic_boost); 
			build_n(zergling, 4);
			build_n(hatchery, 3);
			build_n(spawning_pool, 1);
			build_n(hatchery, 2);
			if (hatch_count < 2 || drone_count < 10) build_n(drone, 12);
			return;
		}
		
		if (switch_to_hydras) build(hydralisk);
		else build(zergling);
		
		if (drone_count >= 40) upgrade(adrenal_glands);
		
		//if (army_supply > enemy_army_supply) upgrade(pneumatized_carapace) && upgrade(ventral_sacs);
		if (army_supply > enemy_army_supply) upgrade(pneumatized_carapace);
		
		if ((count_production(st, drone) == 0 || (army_supply >= 40.0 && count_production(st, drone) < 2)) && drone_count < 54) build(drone);
		build_n(drone, 36);
		
		build_n(drone, 20);
		
		if (drone_count >= 40) {
			build_n(spire, 1);
			if (st.used_supply[race_zerg] >= 190) build_n(mutalisk, 9);
			else if (st.used_supply[race_zerg] >= 140 && enemy_army_supply < 20) build_n(mutalisk, 6);
			else if (st.used_supply[race_zerg] >= 90 && enemy_army_supply == enemy_air_army_supply) build_n(mutalisk, 6);
			build_n(scourge, (int)enemy_air_army_supply);
		}
		//if (drone_count >= 16 && (drone_count >= 20 || army_supply > enemy_army_supply)) {
		if (drone_count >= 20 && (army_supply > enemy_army_supply || drone_count >= 26)) {
			if (switch_to_hydras) {
				upgrade(zerg_carapace_1) && upgrade(zerg_missile_attacks_1) && upgrade(zerg_carapace_2) && upgrade(zerg_missile_attacks_2) && upgrade(zerg_carapace_3) && upgrade(zerg_missile_attacks_3);
			//if (enemy_vulture_count != 0) {
			} else if (false) {
				upgrade(zerg_melee_attacks_1) && upgrade(zerg_carapace_1) && upgrade(zerg_melee_attacks_2) && upgrade(zerg_carapace_2) && upgrade(zerg_melee_attacks_3) && upgrade(zerg_carapace_3);
			} else {
				upgrade(zerg_carapace_1) && upgrade(zerg_melee_attacks_1) && upgrade(zerg_carapace_2) && upgrade(zerg_melee_attacks_2) && upgrade(zerg_carapace_3) && upgrade(zerg_melee_attacks_3);
			}
		}
		
		if ((zergling_count >= 50 || drone_count >= 40) && army_supply > enemy_army_supply) build_n(evolution_chamber, 2);
		
		//build_n(hatchery, 5);
		//if (players::opponent_player->race != race_protoss || count_units_plus_production(st, extractor)) upgrade(metabolic_boost);
		if (drone_count >= 20 && !switch_to_hydras) upgrade(metabolic_boost);
		build_n(drone, 14);
		
		if (drone_count < 34 && army_supply < (drone_count >= 34 ? enemy_vulture_count * 2 : 0) + enemy_army_supply * 0.75 + enemy_attacking_army_supply * 0.5) {
			if (switch_to_hydras) build(hydralisk);
			else build(zergling);
		}
		
		if ((is_defending && enemy_vulture_count >= 2) || (drone_count < 24 && enemy_vulture_count >= 3) || (drone_count < 40 && enemy_vulture_count >= 6) || enemy_vulture_count > zergling_count / 3) {
			build_n(hydralisk, enemy_vulture_count + 4);
		}
		
		if (players::opponent_player->race == race_terran && drone_count < 20) {
			build_n(hydralisk, enemy_vulture_count);
			if (drone_count >= 16) build_n(hydralisk_den, 1);
		}
		
		if (hydralisk_count >= 6 || switch_to_hydras) {
			upgrade(muscular_augments) && upgrade(grooved_spines);
		}
		
		if (force_expand && count_production(st, hatchery) == 0) {
			build(hatchery);
		}
		int n_safe_overlords = enemy_corsair_count + enemy_wraith_count ? 2 : 1;
		if (is_losing_an_overlord && count_production(st, overlord) < n_safe_overlords && st.used_supply[race_zerg] >= st.max_supply[race_zerg] - 8 * n_safe_overlords) build(overlord);
		
		return;
	}
	
	if (players::opponent_player->race == race_terran && false) {
		bool go_mutas = enemy_army_supply < 8 || (enemy_army_supply - enemy_biological_army_supply >= enemy_army_supply * 0.5 && enemy_anti_air_army_supply < enemy_army_supply * 0.66);
		if (total_drones_made < 13) {
			if (total_zerglings_made >= 4) build_n(drone, 13);
			//build_n(lair, 1);
			if (army_supply < enemy_army_supply / 2 + enemy_attacking_army_supply + enemy_proxy_building_count * 4 + enemy_gateway_count * 4 + enemy_attacking_worker_count) build(zergling);
			if (enemy_attacking_army_supply || enemy_proxy_building_count) build_n(zergling, 10);
			build_n(zergling, 4);
			build_n(hatchery, 3);
			build_n(spawning_pool, 1);
			build_n(hatchery, 2);
			if (hatch_count < 2 || drone_count < 10) build_n(drone, 12);
			return;
		} else build_n(drone, 60);
		if (st.used_supply[race_zerg] < 180) {
			//if (drone_count >= 40 || enemy_vulture_count + enemy_firebat_count < 2) build(zergling);
			//if (defiler_count == 0 || zergling_count >= enemy_tank_count * 10) build(hydralisk);
			build(hydralisk);
			if (drone_count >= 16) {
				if (go_mutas) {
					build_n(mutalisk, 9);
				} else {
					if (total_lurkers_made < 4 || lurker_count * 2 < enemy_ground_small_army_supply) {
						upgrade(lurker_aspect) && build_n(lurker, 14);
					}
				}
			}
			if (total_hydralisks_made >= 12) {
				if (enemy_anti_air_army_supply < enemy_army_supply * 0.4) {
					build_n(mutalisk, 13);
					if (army_supply < enemy_army_supply + enemy_air_army_supply) build_n(mutalisk, 13) && build_n(greater_spire, 1);
					else build_n(guardian, 8);
				} else {
					build_n(mutalisk, enemy_tank_count);
				}
			}
			if (drone_count >= 34) {
				if (enemy_weak_against_ultra_supply >= enemy_army_supply * 0.33 || ultralisk_count < army_supply / 12.0) {
					build(ultralisk);
					upgrade(chitinous_plating) && upgrade(anabolic_synthesis);
				}
			}
			if (total_hydralisks_made >= 12) {
				if (enemy_air_army_supply >= enemy_army_supply / 2) {
					build_n(devourer, (int)(army_supply / 12.0));
				}
			}
			if (total_scourge_made < 14) {
				if (drone_count >= 30) build_n(scourge, (int)enemy_air_army_supply);
			}
			if (drone_count >= 16 || total_zerglings_made >= 16) upgrade(metabolic_boost);
			if (army_supply >= std::min(enemy_army_supply, 70.0 - drone_count)) {
				if (total_drones_made >= 34) {
					if (upgrade(plague)) {
						build_n(defiler, (int)(army_supply / 12.0));
						upgrade(consume);
					}
					//build_n(defiler, (int)(army_supply / 12.0));
					//upgrade(consume) && upgrade(plague);
					if (zergling_count < defiler_count * 4) build(zergling);
				}
				if (drone_count >= 32 && army_supply >= enemy_army_supply * 1.5) {
					build_n(queen, (int)(army_supply / 12.0));
					if (enemy_biological_army_supply >= enemy_army_supply * 0.5) upgrade(ensnare);
					else if (queen_count >= 2 && enemy_tank_count >= 2) upgrade(spawn_broodling);
				}
			}
		} else build(mutalisk);
		if (drone_count < 20 || army_supply >= enemy_army_supply) {
			build_n(hive, 1);
			//upgrade(pneumatized_carapace) && upgrade(ventral_sacs);
			upgrade(pneumatized_carapace);
			build_n(drone, 26 + enemy_static_defence_count * 3 - enemy_proxy_building_count * 2 + std::max(enemy_tank_count * 3 - (int)enemy_attacking_army_supply, 0));
			
			if (army_supply >= enemy_attacking_army_supply * 1.5) build_n(drone, 36);
			
			if (drone_count >= 52) {
				build_n(spire, 1);
			}
			if (drone_count >= 44 && army_supply >= 40) {
				build_n(evolution_chamber, 2);
			}
		}
		int drones_inprod = 1;
		if (drone_count < max_mineral_workers - max_mineral_workers / 4) drones_inprod = 2;
		if (drone_count >= 30) {
			if (army_supply < std::min((double)drone_count, enemy_army_supply / 2 + enemy_attacking_army_supply)) drones_inprod = 0;
		} else {
			if (army_supply < enemy_attacking_army_supply) drones_inprod = 0;
		}
		if (count_production(st, drone) < drones_inprod) {
			if (drone_count < max_mineral_workers * 0.75) {
				build_n(drone, 74);
			} else if (can_expand && count_production(st, hatchery) < 2) {
				build(hatchery);
			}
		}
		if (go_mutas && drone_count >= 26) build_n(spire, 1);
		if (hatch_count < 5) build_n(hatchery, 5);
		if (total_hydralisks_made < 4) build_n(hydralisk, 4);
		build_n(drone, 20);
		if (players::opponent_player->race == race_terran) {
			if (my_completed_units_of_type[hatchery].size() >= 2 && drone_count >= 14) build_n(sunken_colony, 1);
		}
		if (st.bases.size() >= 3 && static_defence_pos != xy() && drone_count >= 28) {
			//if (drone_count >= 38) build_n(sunken_colony, st.bases.size() * 3);
			//else build_n(sunken_colony, st.bases.size());
			//build_n(sunken_colony, st.bases.size());
		}
		//if (drone_count < 20 && hydralisk_count == 0 && (army_supply < enemy_army_supply / 2 + enemy_attacking_army_supply || zergling_count < enemy_zergling_count * 2 + enemy_zealot_count * 5)) build(zergling);
		//if (drone_count < 20 && hydralisk_count == 0 && army_supply < enemy_army_supply / 2 + enemy_attacking_army_supply + enemy_proxy_building_count * 4 + enemy_gateway_count * 4 + enemy_attacking_worker_count) build(zergling);
		if (drone_count < 12 && (enemy_army_supply || enemy_proxy_building_count)) build_n(zergling, 10);
		build_n(hydralisk, (int)enemy_air_army_supply + std::max(enemy_vulture_count - mutalisk_count, 0));
		if (hydralisk_count >= 8 && drone_count >= 26) upgrade(muscular_augments) && upgrade(grooved_spines);
		if (drone_count >= 18) build_n(hydralisk_den, 1);
//		if (total_drones_made < 18) {
//			if (opponent_has_expanded && total_zerglings_made < 8) {
//				build(zergling);
//				upgrade(metabolic_boost);
//			}
//		}
		//build_n(zergling, 6);
		//build_n(drone, 12) && build_n(hatchery, 3) && build_n(spawning_pool, 1);
		build_n(drone, 9) && build_n(spawning_pool, 1);
		
		if (force_expand && count_production(st, hatchery) == 0) {
			build(hatchery);
		}
		if (is_losing_an_overlord && count_production(st, overlord) == 0 && st.used_supply[race_zerg] >= st.max_supply[race_zerg] - 8) build(overlord);
		return;
	}
	
	if (players::opponent_player->race == race_terran && false) {
		if (total_drones_made < 12) {
			build_n(drone, 12);
			build_n(hatchery, 3);
			if (army_supply < enemy_army_supply / 2 + enemy_attacking_army_supply + enemy_proxy_building_count * 4 + enemy_gateway_count * 4 + enemy_attacking_worker_count) build(zergling);
			if (enemy_attacking_army_supply || enemy_proxy_building_count) build_n(zergling, 10);
			build_n(zergling, 4);
			build_n(hatchery, 3);
			if (players::opponent_player->race == race_zerg) build_n(zergling, 18);
			build_n(spawning_pool, 1);
			build_n(hatchery, 2);
			if (hatch_count < 2 || drone_count < 10) build_n(drone, 12);
			return;
		}
		if (st.used_supply[race_zerg] < 180) {
			build(zergling);
			build(mutalisk);
			if (mutalisk_count * 2 < enemy_air_army_supply && devourer_count < mutalisk_count / 3) build(devourer);
			else if (guardian_count < mutalisk_count * 2) build(guardian);
//			if (count_production(st, queen) == 0 && queen_count < 2 + enemy_tank_count * 2) {
//				build(queen);
//				upgrade(spawn_broodling);
//			}
		} else build(mutalisk);
		if (drone_count < 20 || army_supply >= enemy_army_supply) {
			//upgrade(pneumatized_carapace) && upgrade(ventral_sacs);
			build_n(drone, 36);
			
			if (drone_count >= 52) {
				build_n(spire, 1);
			}
			if (drone_count >= 44 && army_supply >= 40) {
				build_n(evolution_chamber, 2);
			}
		}
		//if (count_production(st, drone) == 0 && army_supply >= drone_count) build_n(drone, 74);
		int drones_inprod = 1;
		if (drone_count < max_mineral_workers - max_mineral_workers / 4) drones_inprod = 2;
		if (drone_count >= 30) {
			if (army_supply < std::min((double)drone_count, enemy_army_supply / 2 + enemy_attacking_army_supply)) drones_inprod = 0;
		} else {
			if (army_supply < enemy_attacking_army_supply) drones_inprod = 0;
		}
		if (count_production(st, drone) < drones_inprod) {
			if (drone_count < max_mineral_workers * 0.75) {
				build_n(drone, 74);
			} else if (can_expand && count_production(st, hatchery) < 2) {
				build(hatchery);
			}
		}
		if (drone_count >= 22) {
			//upgrade(zerg_carapace_1) && upgrade(zerg_missile_attacks_1) && upgrade(zerg_carapace_2) && upgrade(zerg_missile_attacks_2) && upgrade(zerg_carapace_3) && upgrade(zerg_missile_attacks_3);
			if (!my_units_of_type[unit_types::greater_spire].empty()) {
				upgrade(zerg_flyer_attacks_1) && upgrade(zerg_flyer_carapace_1) && upgrade(zerg_flyer_attacks_2) && upgrade(zerg_flyer_carapace_2) && upgrade(zerg_flyer_attacks_3) && upgrade(zerg_flyer_carapace_3);
			} else build_n(greater_spire, 1);
		}
		if (hatch_count < 5) build_n(hatchery, 5);
		build_n(drone, 20);
		if (players::opponent_player->race == race_terran) {
			if (my_completed_units_of_type[hatchery].size() >= 2 && drone_count >= 14) build_n(sunken_colony, 1);
		}
		//if (drone_count < 20 && hydralisk_count == 0 && (army_supply < enemy_army_supply / 2 + enemy_attacking_army_supply || zergling_count < enemy_zergling_count * 2 + enemy_zealot_count * 5)) build(zergling);
		if (drone_count < 20 && hydralisk_count == 0 && army_supply < enemy_attacking_army_supply + enemy_proxy_building_count * 4 + enemy_gateway_count * 4 + enemy_attacking_worker_count) build(zergling);
		if (drone_count < 12 && (enemy_army_supply || enemy_proxy_building_count)) build_n(zergling, 10);
		//build_n(hydralisk, (int)enemy_air_army_supply);
		if (hydralisk_count >= 1 && drone_count >= 26) upgrade(muscular_augments) && upgrade(grooved_spines);
		//if (drone_count >= 18) build_n(hydralisk_den, 1);
		if (drone_count >= 15) {
			build_n(lair, 1) && build_n(spire, 1) && build_n(mutalisk, 4);
		}
		//build_n(zergling, 6);
		//build_n(drone, 12) && build_n(hatchery, 3) && build_n(spawning_pool, 1);
		build_n(drone, 9) && build_n(spawning_pool, 1);
		
		if (players::opponent_player->race == race_zerg && total_drones_made >= 9) {
			if (total_drones_made >= 7 + (current_frame / 15 / 60 - 5) / 2) {
				build(zergling);
				if (zergling_count >= 20 || hydralisk_count < enemy_air_army_supply) build(hydralisk);
			}
		}
		
		if (force_expand && count_production(st, hatchery) == 0) {
			build(hatchery);
		}
		if (is_losing_an_overlord && count_production(st, overlord) == 0 && st.used_supply[race_zerg] >= st.max_supply[race_zerg] - 8) build(overlord);
		return;
	}
	
	if (players::opponent_player->race == race_terran) {
		if (total_drones_made < 15) {
			build_n(drone, 16);
			build_n(hatchery, 3);
			build_n(drone, 12);
			return;
		}
		
		if (drone_count >= 20 || st.frame >= 15 * 60 * 10) {
			//build(zergling);
			//if (total_mutalisks_made < 9 || mutalisk_count < zergling_count / 8 || mutalisk_count * 2 < enemy_vulture_count + enemy_tank_count * 3) build(mutalisk);
			//build(hydralisk);
			if (total_hydralisks_made < 8 || (army_supply < enemy_attacking_army_supply + 4.0 && enemy_attacking_army_supply < 8.0)) {
				build(zergling);
				build(hydralisk);
				if (drone_count >= 26) build(lurker);
			} else {
				build_n(drone, 60);
			}
			if (total_hydralisks_made >= 8 || army_supply > enemy_attacking_army_supply + 4.0 || army_supply >= 40.0 || drone_count >= 60) {
				if (drone_count >= 40) {
					build_n(queen, 8);
					upgrade(spawn_broodling);
					if (enemy_biological_army_supply >= enemy_army_supply / 2) {
						upgrade(ensnare);
					}
				}
			}
			if (drone_count >= 60) {
				build_n(evolution_chamber, 2);
			}
			if (drone_count >= 44 || !st.units[hive].empty()) {
				//build(ultralisk);
				//upgrade(anabolic_synthesis) && upgrade(chitinous_plating);
				build(lurker);
				if (zergling_count < lurker_count * 4) build(zergling);
				if (hydralisk_count < 6) build(hydralisk);
				if (upgrade(plague) && upgrade(consume)) {
					build_n(defiler, (int)(army_supply / 12.0));
					//build(defiler);
					if (zergling_count < defiler_count * 6) build(zergling);
				}
				if (queen_count < lurker_count / 2) {
					if (count_production(st, queen) < (queen_count < lurker_count / 3 ? 2 : 1)) build(queen);
					if (enemy_tank_count) upgrade(spawn_broodling);
				}
			}
			//if (drone_count >= 60 || !st.units[hive].empty()) upgrade(adrenal_glands);
			if (zergling_count >= 10) {
				upgrade(metabolic_boost);
				if (zergling_count >= 30 && !st.units[hive].empty()) upgrade(adrenal_glands);
			}
		}
		
		if (drone_count >= 32 && army_supply >= std::min(20.0, enemy_army_supply)) {
			build_n(hive, 1);
		}
		
		if (drone_count < 20 || army_supply >= enemy_army_supply) {
			//upgrade(pneumatized_carapace) && upgrade(ventral_sacs);
			build_n(drone, 36);
			
			if (drone_count >= 52) {
				build_n(spire, 1);
			}
			if (drone_count >= 44 && army_supply >= 40) {
				build_n(evolution_chamber, 2);
			}
		}
		//if (count_production(st, drone) == 0 && army_supply >= drone_count) build_n(drone, 74);
		int drones_inprod = 1;
		if (drone_count < max_mineral_workers) drones_inprod = 2;
		if (drone_count >= 30) {
			if (army_supply < std::min((double)drone_count, enemy_army_supply / 2 + enemy_attacking_army_supply)) drones_inprod = 0;
		} else {
			if (army_supply < enemy_attacking_army_supply) drones_inprod = 0;
		}
		if (count_production(st, drone) < drones_inprod) {
			if (drone_count < max_mineral_workers * 0.75) {
				build_n(drone, 74);
			} else if (can_expand && count_production(st, hatchery) < 2) {
				build(hatchery);
			}
		}
		if (drone_count >= 22) {
			//upgrade(zerg_carapace_1) && upgrade(zerg_melee_attacks_1) && upgrade(zerg_carapace_2) && upgrade(zerg_melee_attacks_2) && upgrade(zerg_carapace_3) && upgrade(zerg_melee_attacks_3);
			//upgrade(zerg_melee_attacks_1) && upgrade(zerg_melee_attacks_2) && upgrade(zerg_melee_attacks_3) && upgrade(zerg_carapace_1) && upgrade(zerg_carapace_2) && upgrade(zerg_carapace_3);
			//upgrade(zerg_missile_attacks_1) && upgrade(zerg_missile_attacks_2) && upgrade(zerg_missile_attacks_3) && upgrade(zerg_carapace_1) && upgrade(zerg_carapace_2) && upgrade(zerg_carapace_3);
			upgrade(zerg_melee_attacks_1) && upgrade(zerg_melee_attacks_2) && upgrade(zerg_melee_attacks_3) && upgrade(zerg_carapace_1) && upgrade(zerg_carapace_2) && upgrade(zerg_carapace_3);
			build_n(lair, 1);
		}
		if (drone_count >= 33) {
			//build_n(sunken_colony, st.bases.size());
		}
		if (hatch_count < 4) build_n(hatchery, 4);
		build_n(drone, 26);
		if (total_hydralisks_made < 4) build_n(hydralisk, 4);
		build_n(drone, 20);
		if (players::opponent_player->race == race_terran) {
			if (my_completed_units_of_type[hatchery].size() >= 2 && drone_count >= 14) build_n(sunken_colony, 1);
		}
		//if (drone_count < 20 && hydralisk_count == 0 && (army_supply < enemy_army_supply / 2 + enemy_attacking_army_supply || zergling_count < enemy_zergling_count * 2 + enemy_zealot_count * 5)) build(zergling);
		if (drone_count < 20 && hydralisk_count == 0 && army_supply < enemy_attacking_army_supply + enemy_proxy_building_count * 4 + enemy_gateway_count * 4 + enemy_attacking_worker_count) build(zergling);
		if (drone_count < 12 && (enemy_army_supply || enemy_proxy_building_count)) build_n(zergling, 10);
		build_n(hydralisk, (int)enemy_air_army_supply);
		if (hydralisk_count >= 8 && drone_count >= 26) upgrade(muscular_augments) && upgrade(grooved_spines);
		if (drone_count >= 16) build_n(hydralisk_den, 1);
		//build_n(zergling, 6);
		//build_n(drone, 12) && build_n(hatchery, 3) && build_n(spawning_pool, 1);
		build_n(drone, 9) && build_n(spawning_pool, 1);
		
		if (players::opponent_player->race == race_zerg && total_drones_made >= 9) {
			if (total_drones_made >= 7 + (current_frame / 15 / 60 - 5) / 2) {
				build(zergling);
				if (zergling_count >= 20 || hydralisk_count < enemy_air_army_supply) build(hydralisk);
			}
		}
		
		if (build_nydus_pos != xy() && !has_nydus_without_exit) {
			build_n(nydus_canal, my_units_of_type[unit_types::nydus_canal].size() + 1);
		}
		
		if (force_expand && count_production(st, hatchery) == 0) {
			build(hatchery);
		}
		if (is_losing_an_overlord && count_production(st, overlord) == 0 && st.used_supply[race_zerg] >= st.max_supply[race_zerg] - 8) build(overlord);
		return;
	}
	
	if (players::opponent_player->race == race_terran) {
		if (total_drones_made < 15) {
			build_n(drone, 16);
			build_n(hatchery, 3);
			build_n(drone, 12);
			return;
		}
		if (total_drones_made < 13) {
			build_n(drone, 13);
			//build_n(hatchery, 3);
			if (enemy_army_supply == 0.0 && enemy_proxy_building_count == 0) {
				if (my_units_of_type[unit_types::hatchery].size() < 2) {
					build_n(hatchery, 2);
					build_n(drone, 12);
					return;
				}
			}
			if (army_supply < enemy_army_supply / 2 + enemy_attacking_army_supply + enemy_proxy_building_count * 4 + enemy_gateway_count * 4 + enemy_attacking_worker_count) build(zergling);
			if (enemy_attacking_army_supply || enemy_proxy_building_count) build_n(zergling, 10);
			build_n(zergling, 4);
			//build_n(hatchery, 3);
			if (players::opponent_player->race == race_zerg) build_n(zergling, 18);
			build_n(spawning_pool, 1);
			build_n(hatchery, 2);
			if (hatch_count < 2 || drone_count < 10) build_n(drone, 12);
			return;
		}
		if (st.used_supply[race_zerg] < 180) {
			//build(hydralisk);
			build(zergling);
			if (count_units_plus_production(st, hydralisk) >= 40) {
				if (lurker_count * 2 < enemy_ground_small_army_supply) {
					if (st.minerals >= 400) {
						if (lurker_count < hydralisk_count / 6) {
							build(zergling);
							build(lurker);
						}
					} else build(lurker);
				}
				upgrade(lurker_aspect);
			}
			if (mutalisk_count < hydralisk_count / 2) build(mutalisk);
//			if (count_production(st, queen) == 0 && queen_count < 2 + enemy_tank_count * 2) {
//				build(queen);
//				upgrade(spawn_broodling);
//			}
		} else build(mutalisk);
		if (drone_count < 20 || army_supply >= enemy_army_supply) {
			//upgrade(pneumatized_carapace) && upgrade(ventral_sacs);
			build_n(drone, 36);
			
			if (drone_count >= 52) {
				build_n(spire, 1);
			}
			if (drone_count >= 44 && army_supply >= 40) {
				build_n(evolution_chamber, 2);
			}
		}
		//if (count_production(st, drone) == 0 && army_supply >= drone_count) build_n(drone, 74);
		int drones_inprod = 1;
		if (drone_count < max_mineral_workers - max_mineral_workers / 4) drones_inprod = 2;
		if (drone_count >= 30) {
			//if (army_supply < std::min((double)drone_count, enemy_army_supply / 2 + enemy_attacking_army_supply)) drones_inprod = 0;
		} else {
			//if (army_supply < enemy_attacking_army_supply) drones_inprod = 0;
		}
		if (count_production(st, drone) < drones_inprod) {
			if (drone_count < max_mineral_workers * 0.75) {
				build_n(drone, 74);
			} else if (can_expand && count_production(st, hatchery) < 2) {
				build(hatchery);
			}
		}
		if (drone_count >= 22) {
			upgrade(zerg_carapace_1) && upgrade(zerg_missile_attacks_1) && upgrade(zerg_carapace_2) && upgrade(zerg_missile_attacks_2) && upgrade(zerg_carapace_3) && upgrade(zerg_missile_attacks_3);
		}
		if (drone_count >= 33) {
			build_n(sunken_colony, st.bases.size());
		}
		if (hatch_count < 4) build_n(hatchery, 4);
		build_n(drone, 26);
		upgrade(muscular_augments);
		build_n(hydralisk, 4);
		build_n(drone, 20);
		if (players::opponent_player->race == race_terran) {
			if (my_completed_units_of_type[hatchery].size() >= 2 && drone_count >= 14) build_n(sunken_colony, 1);
		}
		//if (drone_count < 20 && hydralisk_count == 0 && (army_supply < enemy_army_supply / 2 + enemy_attacking_army_supply || zergling_count < enemy_zergling_count * 2 + enemy_zealot_count * 5)) build(zergling);
		if (drone_count < 20 && hydralisk_count == 0 && army_supply < enemy_attacking_army_supply + enemy_proxy_building_count * 4 + enemy_gateway_count * 4 + enemy_attacking_worker_count) build(zergling);
		if (drone_count < 12 && (enemy_army_supply || enemy_proxy_building_count)) build_n(zergling, 10);
		build_n(hydralisk, (int)enemy_air_army_supply);
		if (hydralisk_count >= 1 && drone_count >= 26) upgrade(muscular_augments) && upgrade(grooved_spines);
		if (drone_count >= 16) build_n(hydralisk_den, 1);
		//build_n(zergling, 6);
		//build_n(drone, 12) && build_n(hatchery, 3) && build_n(spawning_pool, 1);
		build_n(drone, 9) && build_n(spawning_pool, 1);
		
		if (players::opponent_player->race == race_zerg && total_drones_made >= 9) {
			if (total_drones_made >= 7 + (current_frame / 15 / 60 - 5) / 2) {
				build(zergling);
				if (zergling_count >= 20 || hydralisk_count < enemy_air_army_supply) build(hydralisk);
			}
		}
		
		if (force_expand && count_production(st, hatchery) == 0) {
			build(hatchery);
		}
		if (is_losing_an_overlord && count_production(st, overlord) == 0 && st.used_supply[race_zerg] >= st.max_supply[race_zerg] - 8) build(overlord);
		return;
	}
	
	if (true) {
		if (players::opponent_player->race == race_zerg) {
			st.auto_build_hatcheries = false;
//			if (total_zerglings_made >= 6) {
//				build_n(hatchery, 4);
//				build(zergling);
//				//build(hydralisk);
//				//upgrade(muscular_augments) && upgrade(grooved_spines);
//				//build_n(hydralisk, 4);
//				build_n(drone, 12);
//				//upgrade(metabolic_boost);
//				if (completed_hatch_count >= 2) {
//					//build_n(sunken_colony, 1);
//					//build_n(hydralisk_den, 1);
//				}
//				build_n(zergling, 26);
//				build_n(hatchery, 2);
//			}
//			if (zergling_count >= 10 && army_supply > enemy_army_supply) {
//				//if (hatch_count >= 2) upgrade(zerg_melee_attacks_1) && upgrade(zerg_carapace_1);
//				if (hatch_count >= 2) build(mutalisk);
//				upgrade(metabolic_boost);
//			}
			build(mutalisk);
			build(zergling);
			if (!my_completed_units_of_type[unit_types::spire].empty()) build(mutalisk);
			build_n(scourge, std::min(enemy_mutalisk_count, 8) + enemy_spire_count);
			upgrade(metabolic_boost);
			build_n(zergling, 18);
			if (hatch_count < 2) build(hatchery);
			build_n(zergling, 9);
			if (!my_completed_units_of_type[unit_types::hydralisk_den].empty()) build_n(hydralisk, 20);
			if (count_units_plus_production(st, creep_colony)) build_n(sunken_colony, 1);
			build_n(overlord, 2);
			build_n(spawning_pool, 1);
//			build_n(hatchery, 2);
//			build_n(zergling, 400);
//			upgrade(metabolic_boost);
			build_n(zergling, 6);
			build_n(overlord, 2);
			build_n(spawning_pool, 1);
			build_n(drone, 9 + std::max(total_zerglings_made - 18, 0) / 12 + total_hydralisks_made / 4);
			return;
			
		}
		if (total_drones_made < 12) {
			build_n(drone, 12);
			build_n(hatchery, 3);
			if (army_supply < enemy_army_supply / 2 + enemy_attacking_army_supply + enemy_proxy_building_count * 4 + enemy_gateway_count * 4 + enemy_attacking_worker_count) build(zergling);
			if (enemy_attacking_army_supply || enemy_proxy_building_count) build_n(zergling, 10);
//			build_n(zergling, 6);
//			build_n(drone, 11);
//			build_n(overlord, 2);
//			build_n(drone, 9);
			build_n(zergling, 4);
			build_n(hatchery, 3);
			if (players::opponent_player->race == race_zerg) build_n(zergling, 18);
			build_n(spawning_pool, 1);
			build_n(hatchery, 2);
			if (hatch_count < 2 || drone_count < 10) build_n(drone, 12);
			return;
		}
		if (st.used_supply[race_zerg] < 180) {
			build(hydralisk);
			if (count_units_plus_production(st, hydralisk) >= 40) {
				if (lurker_count * 2 < enemy_ground_small_army_supply) {
					if (st.minerals >= 400) {
						if (lurker_count < hydralisk_count / 6) {
							build(zergling);
							build(lurker);
						}
					} else build(lurker);
				}
				upgrade(lurker_aspect);
			}
			if (enemy_anti_air_army_supply <= enemy_army_supply - 20 && hydralisk_count >= 40 - enemy_reaver_count * 10) build_n(mutalisk, 9);
		} else build(mutalisk);
		if (drone_count < 20 || army_supply >= enemy_army_supply) {
			//upgrade(pneumatized_carapace) && upgrade(ventral_sacs);
			build_n(drone, 36);
			
			if (drone_count >= 52) {
				build_n(spire, 1);
			}
			if (drone_count >= 44 && army_supply >= 40) {
				build_n(evolution_chamber, 2);
			}
		}
		//if (count_production(st, drone) == 0 && army_supply >= drone_count) build_n(drone, 74);
		int drones_inprod = 1;
		if (drone_count < max_mineral_workers - max_mineral_workers / 4) drones_inprod = 2;
		if (drone_count >= 30) {
			if (army_supply < std::min((double)drone_count, enemy_army_supply / 2 + enemy_attacking_army_supply)) drones_inprod = 0;
		} else {
			if (army_supply < enemy_attacking_army_supply) drones_inprod = 0;
		}
		if (count_production(st, drone) < drones_inprod) {
			if (drone_count < max_mineral_workers * 0.75) {
				build_n(drone, 74);
			} else if (can_expand && count_production(st, hatchery) < 2) {
				build(hatchery);
			}
		}
		if (drone_count >= 22) {
			upgrade(zerg_carapace_1) && upgrade(zerg_missile_attacks_1) && upgrade(zerg_carapace_2) && upgrade(zerg_missile_attacks_2) && upgrade(zerg_carapace_3) && upgrade(zerg_missile_attacks_3);
		}
		if (hatch_count < 5) build_n(hatchery, 5);
		build_n(drone, 20);
		if (players::opponent_player->race == race_terran) {
			if (my_completed_units_of_type[hatchery].size() >= 2 && drone_count >= 14) build_n(sunken_colony, 1);
		}
		//if (drone_count < 20 && hydralisk_count == 0 && (army_supply < enemy_army_supply / 2 + enemy_attacking_army_supply || zergling_count < enemy_zergling_count * 2 + enemy_zealot_count * 5)) build(zergling);
		if (drone_count < 20 && hydralisk_count == 0 && army_supply < enemy_attacking_army_supply + enemy_proxy_building_count * 4 + enemy_gateway_count * 4 + enemy_attacking_worker_count) build(zergling);
		if (drone_count < 12 && (enemy_army_supply || enemy_proxy_building_count)) build_n(zergling, 10);
		build_n(hydralisk, (int)enemy_air_army_supply + enemy_vulture_count);
		if (hydralisk_count >= 1 && drone_count >= 26) upgrade(muscular_augments) && upgrade(grooved_spines);
		if (drone_count >= 18) build_n(hydralisk_den, 1);
		//build_n(zergling, 6);
		//build_n(drone, 12) && build_n(hatchery, 3) && build_n(spawning_pool, 1);
		build_n(drone, 9) && build_n(spawning_pool, 1);
		
		if (players::opponent_player->race == race_zerg && total_drones_made >= 9) {
			if (total_drones_made >= 7 + (current_frame / 15 / 60 - 5) / 2) {
				build(zergling);
				if (zergling_count >= 20 || hydralisk_count < enemy_air_army_supply) build(hydralisk);
			}
		}
		
		if (force_expand && count_production(st, hatchery) == 0) {
			build(hatchery);
		}
		if (is_losing_an_overlord && count_production(st, overlord) == 0 && st.used_supply[race_zerg] >= st.max_supply[race_zerg] - 8) build(overlord);
		return;
	}
	
	if (true) {
		//if (hatch_count < 4 && total_drones_made < 19) {
		if (total_drones_made < 14) {
			build_n(drone, 14);
			build_n(hatchery, 4);
			build_n(zergling, 6);
			build_n(hatchery, 2);
			build_n(drone, 11);
			build_n(spawning_pool, 1);
			build_n(overlord, 2);
			build_n(drone, 9);
			//if (army_supply < enemy_army_supply / 2 + enemy_attacking_army_supply + enemy_proxy_building_count * 4 + enemy_gateway_count * 4 + enemy_attacking_worker_count) build(zergling);
			if (army_supply < enemy_army_supply / 2 + (is_defending ? enemy_attacking_army_supply : 0) + enemy_proxy_building_count * 4 + std::max(enemy_attacking_worker_count - 2, 0)) build(zergling);
			if (enemy_attacking_army_supply || enemy_proxy_building_count) build_n(zergling, 10);
			if (zergling_count >= 14 && count_production(st, drone) == 0) build(drone);
			return;
		}
		if (st.used_supply[race_zerg] < 180) {
			build(hydralisk);
			if (count_units_plus_production(st, hydralisk) >= 40) {
				if (lurker_count * 2 < enemy_ground_small_army_supply) {
					if (st.minerals >= 400) {
						if (lurker_count < hydralisk_count / 6) {
							build(zergling);
							build(lurker);
						}
					} else build(lurker);
				}
				upgrade(lurker_aspect);
			}
			if (enemy_anti_air_army_supply <= enemy_army_supply - 20 && hydralisk_count >= 40 - enemy_reaver_count * 10) build_n(mutalisk, 9);
		} else build(mutalisk);
		if (drone_count < 20 || army_supply >= enemy_army_supply) {
			upgrade(pneumatized_carapace) && upgrade(ventral_sacs);
			build_n(drone, 36);
			
			if (drone_count >= 52) {
				build_n(spire, 1);
			}
			if (drone_count >= 44 && army_supply >= 40) {
				build_n(evolution_chamber, 2);
			}
		}
		if ((drone_count >= 32 || (drone_count < 26 && sunken_count >= 2)) && count_production(st, drone) == 0 && (army_supply >= enemy_army_supply || drone_count < 44)) build_n(drone, 74);
		if (drone_count >= 22) {
			upgrade(zerg_carapace_1) && upgrade(zerg_missile_attacks_1) && upgrade(zerg_carapace_2) && upgrade(zerg_missile_attacks_2) && upgrade(zerg_carapace_3) && upgrade(zerg_missile_attacks_3);
		}
		if (hatch_count < 5) build_n(hatchery, 5);
		build_n(drone, 20);
		if (players::opponent_player->race == race_terran) {
			if (my_completed_units_of_type[hatchery].size() >= 2 && drone_count >= 14) build_n(sunken_colony, 1);
		}
		//if (drone_count < 20 && hydralisk_count == 0 && (army_supply < enemy_army_supply / 2 + enemy_attacking_army_supply || zergling_count < enemy_zergling_count * 2 + enemy_zealot_count * 5)) build(zergling);
		//if (drone_count < 20 && hydralisk_count == 0 && army_supply < enemy_army_supply / 2 + enemy_attacking_army_supply + enemy_proxy_building_count * 4 + enemy_gateway_count * 4 + enemy_attacking_worker_count) build(zergling);
		if (drone_count < 20 && hydralisk_count == 0) {
			//if (drone_count >= 14 && enemy_army_supply > army_supply) build_n(sunken_colony, 1);
			if (drone_count >= 18) build_n(zergling, std::min((int)enemy_army_supply + 6, 16));
			//if (army_supply < enemy_army_supply / 2 + (is_defending ? enemy_attacking_army_supply : 0) + enemy_proxy_building_count * 4 + std::max(enemy_attacking_worker_count - 2, 0)) build(zergling);
			if (army_supply < (is_defending ? enemy_attacking_army_supply : 0) + enemy_proxy_building_count * 4 + std::max(enemy_attacking_worker_count - 2, 0)) build(zergling);
			
			if (zergling_count >= 14 && drone_count < 18 && count_production(st, drone) == 0) build(drone);
		}
		if (zergling_count >= 18) build_n(hydralisk, 4);
		
		if (zergling_count >= 10) upgrade(metabolic_boost);
		
		if (my_completed_units_of_type[unit_types::hatchery].size() >= 2) {
			if (enemy_army_supply >= 6 && (enemy_army_supply >= 12 || (enemy_static_defence_count == 0 && !opponent_has_expanded))) {
				if (!is_defending) build_n(drone, 20);
				//build_n(sunken_colony, std::min((int)(enemy_army_supply - army_supply) / 4, 4));
			}
		}
		
		if (enemy_cloaked_unit_count) upgrade(pneumatized_carapace);
		
		if (drone_count < 12 && (enemy_army_supply || enemy_proxy_building_count)) build_n(zergling, 10);
		build_n(hydralisk, (int)enemy_air_army_supply + enemy_vulture_count);
		if (hydralisk_count >= 1 && drone_count >= 26) upgrade(muscular_augments) && upgrade(grooved_spines);
		if (drone_count >= 18) build_n(hydralisk_den, 1);
		build_n(zergling, 6);
		//build_n(drone, 12) && build_n(hatchery, 3) && build_n(spawning_pool, 1);
		build_n(drone, 9) && build_n(spawning_pool, 1);
		
		if (players::opponent_player->race == race_zerg && total_drones_made >= 9) {
			if (total_drones_made >= 7 + (current_frame / 15 / 60 - 5) / 2) {
				build(zergling);
				if (zergling_count >= 20 || hydralisk_count < enemy_air_army_supply) build(hydralisk);
			}
		}
		
		if (force_expand && count_production(st, hatchery) == 0) {
			build(hatchery);
		}
		if (is_losing_an_overlord && count_production(st, overlord) == 0 && st.used_supply[race_zerg] >= st.max_supply[race_zerg] - 8) build(overlord);
		return;
	}
	
//	if (true) {
//		if (st.gas >= 3000) {
//			if (drone_count >= 30 && current_minerals < 3000 && my_completed_units_of_type[spire].empty()) {
//				if (st.used_supply[race_zerg] + 60 > st.max_supply[race_zerg] + count_production(st, overlord) * 8) build(overlord);
//				if (hatch_count * 3 < 30) build(hatchery);
//				build_n(spire, 1);
//				return;
//			}
//		}
//		if (st.used_supply[race_zerg] < 180) build(zergling);
//		if (st.gas >= 2000) {
//			build_n(spire, 1);
//		}
//		if (st.gas >= 3000 || mutalisk_count) {
//			build(mutalisk);
//		}
//		build_n(drone, 40);
//		if (count_production(st, drone) == 0) build_n(drone, 74);
//		if (drone_count >= 20) {
//			upgrade(zerg_carapace_1) && upgrade(zerg_melee_attacks_1);
//			upgrade(metabolic_boost);
//		}
//		if (hatch_count < 5) build_n(hatchery, 5);
//		build_n(drone, 20);
//		build_n(hydralisk, (int)enemy_air_army_supply + enemy_vulture_count);
//		if (hydralisk_count >= 6) upgrade(muscular_augments) && upgrade(grooved_spines);
//		if (drone_count >= 26) build_n(hydralisk_den, 1);
//		build_n(zergling, 6);
//		build_n(drone, 12) && build_n(hatchery, 3) && build_n(spawning_pool, 1);
		
//		if (force_expand && count_production(st, hatchery) == 0) {
//			build(hatchery);
//		}
//		if (is_losing_an_overlord && count_production(st, overlord) == 0 && st.used_supply[race_zerg] >= st.max_supply[race_zerg] - 8) build(overlord);
//		return;
//	}
	
	if (true) {
		build(mutalisk);
		if (st.used_supply[race_zerg] < 180 || (mutalisk_count >= 8 && hydralisk_count + mutalisk_count * 2 > enemy_air_army_supply)) {
			build(zergling);
			//upgrade(chitinous_plating) && upgrade(anabolic_synthesis) && build_n(ultralisk, 30);
		}
//		if (army_supply >= 40.0) {
//			build_n(defiler, std::min((int)(army_supply / 12.0), 3));
//			upgrade(consume) && upgrade(plague);
//		}
		upgrade(zerg_carapace_2) && upgrade(zerg_melee_attacks_2) && upgrade(zerg_carapace_3) && upgrade(zerg_melee_attacks_3);
		build_n(evolution_chamber, 2);
		if (drone_count >= 50 || count_units_plus_production(st, hive)) upgrade(adrenal_glands);
		build_n(drone, 40);
		if (count_production(st, drone) == 0) build_n(drone, 74);
		if (drone_count >= 20) {
			upgrade(zerg_carapace_1) && upgrade(zerg_melee_attacks_1);
			upgrade(metabolic_boost);
		}
		if (hatch_count < 5) build_n(hatchery, 5);
		build_n(drone, 20);
		build_n(hydralisk, (int)enemy_air_army_supply);
		if (hydralisk_count >= 6) upgrade(muscular_augments) && upgrade(grooved_spines);
		if (drone_count >= 26) build_n(hydralisk_den, 1);
		build_n(zergling, 6);
		build_n(drone, 12) && build_n(hatchery, 3) && build_n(spawning_pool, 1);
		
		if (force_expand && count_production(st, hatchery) == 0) {
			build(hatchery);
		}
		if (is_losing_an_overlord && count_production(st, overlord) == 0 && st.used_supply[race_zerg] >= st.max_supply[race_zerg] - 8) build(overlord);
		return;
	}
	
	if (drone_count >= 24 && players::opponent_player->race == race_protoss) {
		build_n(drone, 32);
		if (st.frame >= 15 * 60 * 15) build_n(drone, 56);
		build_n(hydralisk, 12);
		upgrade(muscular_augments);
		
		if (drone_count >= 16 && zergling_count < enemy_zealot_count * 5) build(zergling);
		
		if (army_supply < enemy_army_supply || army_supply < drone_count) build(hydralisk);
	} else {
		if (drone_count < 14) {
			if (players::opponent_player->race == race_protoss) {
				build_n(drone, 12) && build_n(hatchery, 2) && build_n(spawning_pool, 1) && build_n(hatchery, 3) && build_n(zergling, 4) && build_n(drone, 14);
			} else if (players::opponent_player->race == race_terran) {
				build_n(drone, 12) && build_n(hatchery, 3) && build_n(spawning_pool, 1) && build_n(drone, 14);
			} else {
				build_n(drone, 9) && build_n(spawning_pool, 1) && build_n(zergling, 8);
			}
			if (army_supply < enemy_army_supply) build(zergling);
		} else {
			build_n(drone, 16);
			if (drone_count >= 12) upgrade(metabolic_boost);
			if (ling_rush) {
				if (drone_count >= 16 && (players::opponent_player->race != race_protoss || zergling_count < enemy_zealot_count * 5)) build(zergling);
				else build_n(drone, 24);
			} else build_n(drone, 30);
			//if (drone_count >= 18) upgrade(zerg_carapace_1) && upgrade(zerg_carapace_2) && upgrade(zerg_melee_attacks_1);
			build_n(zergling, 4);
			if (enemy_static_defence_count && enemy_proxy_building_count == 0) build_n(drone, 18 + 6 * enemy_static_defence_count);
//			if (total_zerglings_made >= 30 && army_supply > enemy_attacking_army_supply * (is_defending ? 1.25 : 0.75)) {
//				if (lose_count >= (int)(total_count * 0.75)) build_n(drone, 50);
//			}
			if (total_zerglings_made >= 30 || !ling_rush) {
				if (army_supply > enemy_attacking_army_supply * 2) {
					build_n(drone, 50);
				}
				if (lose_count >= (int)(total_count * 0.75) || lose_count >= 20) build_n(drone, 50);
			}
			
			if (total_drones_made >= 36) {
				build(zergling);
			}
		}
	}
	
	if (drone_count >= 24) {
		
		if (drone_count >= 20 && army_supply > enemy_army_supply) build_n(spire, 1);
	}
	
	if (players::opponent_player->race == race_protoss || drone_count >= 16) {
		if (drone_count >= 20) build_n(sunken_colony, 1);
	} else {
		if (army_supply < enemy_army_supply && total_drones_made >= 12 && drone_count < 14) {
			build(zergling);
			if (my_completed_units_of_type[unit_types::hatchery].size() >= 2) build_n(sunken_colony, 1);
		}
	}
	
	int n_hydras = enemy_dragoon_count * 2 + enemy_corsair_count * 2 + enemy_archon_count * 3;
	if (enemy_vulture_count * 2 >= enemy_army_supply * 0.66) n_hydras += enemy_vulture_count;
	if (mutalisk_count == 0 || mutalisk_count >= 9) build_n(hydralisk, n_hydras);
	if (has_or_is_upgrading(st, consume)) build(zergling);
	if (drone_count >= 26) {
		if (total_mutalisks_made >= 9) {
			upgrade(zerg_melee_attacks_1) && upgrade(zerg_melee_attacks_2) && upgrade(zerg_melee_attacks_3);
			upgrade(zerg_carapace_1) && upgrade(zerg_carapace_2) && upgrade(zerg_carapace_3);
			build_n(evolution_chamber, 2);
		}
		//upgrade(zerg_flyer_attacks_1) && upgrade(zerg_flyer_carapace_1) && upgrade(zerg_flyer_carapace_2);
		if (players::opponent_player->race == race_terran && army_supply < enemy_attacking_army_supply * 1.5) {
			build(zergling);
			//build_n(hydralisk, (enemy_vulture_count + enemy_goliath_count - enemy_tank_count * 2) * 2);
			if (enemy_anti_air_army_supply < enemy_army_supply / 2) build_n(mutalisk, 18);
		}
		if (total_drones_made >= 42) build(zergling);
		if (hydralisk_count < n_hydras) {
			if (total_mutalisks_made < 9) build_n(mutalisk, 9 + enemy_reaver_count * 6);
		} else {
			if (mass_mutas_vs_bio) {
				if (players::opponent_player->race != race_terran || (enemy_biological_army_supply >= enemy_army_supply * 0.66)) {
					build(mutalisk);
				}
			}
		}
		if (total_scourge_made < 14) {
			//build_n(scourge, enemy_shuttle_count * 5 + enemy_corsair_count * 3);
			build_n(scourge, enemy_shuttle_count * 4);
			if (drone_count >= 40) build_n(scourge, (int)enemy_air_army_supply);
		}
	}
	
	if (build_lurkers && (total_mutalisks_made >= 9 || drone_count >= 40)) {
		//if ((drone_count >= 40 || players::opponent_player->race == race_terran) && lurker_count * 2 < enemy_ground_small_army_supply) build(lurker);
		if (lurker_count * 2 < enemy_ground_small_army_supply) build(lurker);
		if (enemy_biological_army_supply >= enemy_army_supply * 0.66 && lurker_count < 4 + enemy_biological_army_supply / 6.0) {
			build(hydralisk);
			build(lurker);
		}
		
		if (total_lurkers_made < 4) build_n(lurker, 4);
		upgrade(lurker_aspect);
	}

	//if (drone_count >= 45 && mutalisk_count + hydralisk_count > enemy_air_army_supply) {
	if (drone_count >= 45 || !st.units[hive].empty()) {
		build(ultralisk);
		upgrade(chitinous_plating) && upgrade(anabolic_synthesis);
	}
	if (mutalisk_count + hydralisk_count < enemy_air_army_supply + enemy_vulture_count + enemy_goliath_count) build(hydralisk);
	
	if (hydralisk_count >= 9 && drone_count >= 30) {
		upgrade(grooved_spines) && upgrade(muscular_augments);
	}
	
	if ((zergling_count >= 25 || drone_count >= 20) && count_production(st, drone) == 0) build_n(drone, 66);
	if (total_zerglings_made >= 40 && total_drones_made < 26) build_n(drone, 26);
	if (total_zerglings_made >= 80 && total_drones_made < 36) build_n(drone, 36);
	
	if (drone_count >= 30 && players::opponent_player->race == race_terran && total_mutalisks_made < 11) {
		build_n(mutalisk, 9);
	}
	
	if ((drone_count >= 38 && (army_supply > enemy_army_supply || drone_count >= 52)) || (fast_hive_tech && !st.units[hive].empty())) {
		upgrade(consume) && upgrade(plague);
	}
	if (has_or_is_upgrading(st, consume)) {
		build_n(defiler, std::min((int)(army_supply / 12.0), 3));
		build_n(defiler, zergling_count / 12);
	}
	
	if (!st.units[hive].empty()) {
		upgrade(adrenal_glands);
		
		build_n(ultralisk_cavern, 1);
	}
	
//	if (has_or_is_upgrading(st, zerg_carapace_2) && army_supply > enemy_army_supply) {
//		build_n(drone, 45);
//		build_n(hive, 1);
//	}
	
	if (fast_hive_tech && drone_count >= 18 && (army_supply > enemy_attacking_army_supply || drone_count >= 26)) {
		if (!ling_rush || total_zerglings_made >= 30) {
			build_n(hive, 1);
		}
	}
	
	if (st.minerals >= 200 && st.used_supply[race_zerg] > std::max(st.max_supply[race_zerg] - 12, 200.0)) {
		build(overlord);
	}
	
	if (drone_count >= 20 || zergling_count >= 16) {
		upgrade(metabolic_boost);
	}
	
	if (drone_count >= 32 && (!fast_hive_tech || !my_completed_units_of_type[unit_types::hive].empty())) {
		upgrade(pneumatized_carapace);
		if (drone_count >= 50) {
			upgrade(antennae);
		}
	}
	
	if (drone_count >= 50 || count_units_plus_production(st, evolution_chamber) >= 2) {
		upgrade(zerg_melee_attacks_1) && upgrade(zerg_melee_attacks_2) && upgrade(zerg_melee_attacks_3);
		upgrade(zerg_carapace_1) && upgrade(zerg_carapace_2) && upgrade(zerg_carapace_3);
		build_n(evolution_chamber, 2);
	}
	
	if ((players::opponent_player->race == race_protoss && hydralisk_count >= 4) || hydralisk_count >= 8) {
		upgrade(metabolic_boost) && upgrade(grooved_spines);
	}
	
	if (!ling_rush && drone_count >= 20) {
		if (!fast_hive_tech) build_n(evolution_chamber, 2);
		if (total_zerglings_made < 8) build_n(zergling, 3);
	}
	
	if (force_expand && count_production(st, hatchery) == 0) {
		build(hatchery);
	}
	
	if (!st.units[evolution_chamber].empty()) {
		upgrade(zerg_carapace_1) && upgrade(zerg_melee_attacks_1) && upgrade(zerg_carapace_2) && upgrade(zerg_melee_attacks_2) && upgrade(zerg_carapace_3) && upgrade(zerg_melee_attacks_3);
	}
	
	if (drone_count >= 13) build_n(zergling, 2);
	
	if (players::opponent_player->race == race_zerg && total_drones_made >= 9) {
		build_n(drone, 18);
		if (count_units_plus_production(st, hive) || build_n(lair, 1)) build_n(spire, 1);
		upgrade(metabolic_boost);
		build_n(hatchery, 2);
		if (total_drones_made >= 7 + (current_frame / 15 / 60 - 5) / 2) build(zergling);
		if (total_zerglings_made >= 24 && (zergling_count > enemy_zergling_count || !my_completed_units_of_type[unit_types::spire].empty())) build(mutalisk);
	}
	
	return;
	
//	if (drone_count < 69) build(drone);
//	else build_army = true;
	
//	if (drone_count >= 16) build_n(hydralisk_den, 1);
	
//	if (army_supply < 6.0) {
//		if (optc::e_army_supply) build_n(sunken_colony, 2);
//		if (is_defending) build_n(zergling, 4);
//	}
	
//	if (drone_count >= 30 && army_supply < optc::e_army_supply) build(zergling);
	
//	build_n(hydralisk, optc::e_vulture_count + optc::e_goliath_count * 2);
//	build_n(zergling, optc::e_tank_count * 8);
//	if (has_or_is_upgrading(st, lurker_aspect)) {
//		build_n(hydralisk, 4);
//		build_n(lurker, optc::e_biological_army_supply / 4.0);
//	}
//	if (hydralisk_count < 8) build_n(hydralisk, optc::e_vulture_count + optc::e_goliath_count * 2);
	
//	//if (army_supply < 8.0 && lurker_count == 0 && optc::e_biological_army_supply >= 6.0) build_n(sunken_colony, std::min(2 + (int)(optc::e_biological_army_supply / 4.0), 5));
//	if (army_supply < 8.0 && lurker_count == 0 && optc::e_biological_army_supply >= 6.0) build_n(sunken_colony, 2);
	
//	if (build_army) {
//		build(hydralisk);
//	}
//	build_n(hydralisk, lurker_count / 2 + (int)enemy_air_army_supply);
	
//	if (has_or_is_upgrading(st, consume)) build_n(defiler, (int)(army_supply / 10.0));
	
//	build_n(drone, (int)(optc::e_worker_count * 0.75));
	
//	if (army_supply > optc::e_army_supply || drone_count > optc::e_worker_count || (drone_count >= 34 && army_supply >= enemy_army_supply * 0.75)) {
//		if (drone_count >= 24) {
//			//build(ultralisk);
//			//upgrade(anabolic_synthesis) && upgrade(chitinous_plating);
			
//			upgrade(consume);
			
//			upgrade(metabolic_boost);
//			upgrade(zerg_carapace_1) && upgrade(zerg_carapace_2) && upgrade(zerg_carapace_3);
//		}
		
//		if (hydralisk_count && optc::e_biological_army_supply < optc::e_army_supply * 0.5) {
//			upgrade(grooved_spines) && upgrade(muscular_augments);
//		}
		
//		if (st.minerals >= 400) build(zergling);
//	}
	
//	if (drone_count >= 18) {
//		build_n(lair, 1);
//		if (optc::e_army_supply < 10.0 || optc::e_biological_army_supply >= optc::e_army_supply * 0.5) upgrade(lurker_aspect);
//	}
	
//	if (force_expand && count_production(st, unit_types::hatchery) == 0) {
//		build(hatchery);
//	}
	
//	if (army_supply < buildpred2::enemy_army_supply) {
//		build(zergling);
//		if (has_or_is_upgrading(st, lurker_aspect)) build(lurker);
//	} else if (drone_count >= 15) upgrade(lurker_aspect);

}
