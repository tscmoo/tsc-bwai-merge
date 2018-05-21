
struct strat_any_zvt : strat_mod {
	
	
	virtual void mod_init() override {
		
//		if (current_used_total_supply < 150.0) {
//			combat::no_aggressive_groups = true;
//			combat::aggressive_mutalisks = true;
//			combat::use_control_spots_non_terran = true;
//		} else {
//			combat::use_control_spots_non_terran = false;
//		}
		
		combat::dont_attack_main_army = current_used_total_supply < 150.0;

	}
	
	virtual void mod_tick() override {
		
		total_zerglings_made = my_units_of_type[unit_types::zergling].size() + game->self()->deadUnitCount(unit_types::zergling->game_unit_type);
		total_drones_made = my_units_of_type[unit_types::drone].size() + game->self()->deadUnitCount(unit_types::drone->game_unit_type);
		total_scourge_made = my_units_of_type[unit_types::scourge].size() + game->self()->deadUnitCount(unit_types::scourge->game_unit_type);
		total_mutalisks_made = my_units_of_type[unit_types::mutalisk].size() + game->self()->deadUnitCount(unit_types::mutalisk->game_unit_type);
		total_lurkers_made = my_units_of_type[unit_types::lurker].size() + game->self()->deadUnitCount(unit_types::lurker->game_unit_type);
		total_hydralisks_made = my_units_of_type[unit_types::hydralisk].size() + game->self()->deadUnitCount(unit_types::hydralisk->game_unit_type);
		
		if (my_hatch_count < 3) {
			max_bases = 2;
		} else if (my_hatch_count < 6) {
			resource_gathering::max_gas = 900.0;
			max_bases = 3;
		} else max_bases = 0;
		
	}
	
	virtual void mod_build(buildpred::state& st) override {
	
		using namespace unit_types;
		using namespace upgrade_types;
		
		//st.dont_build_refineries = count_units_plus_production(st, extractor) && drone_count < 40;
		st.dont_build_refineries = count_units_plus_production(st, extractor) && drone_count < 20;
		st.auto_build_hatcheries = drone_count >= 12;
		
		//if (st.frame >= 15 * 60 * 12) {
		if (total_drones_made >= 34) {
			build(zergling);
			build(hydralisk);
			if (st.frame >= 15 * 60 * 20) {
				build(ultralisk);
			}
			upgrade(muscular_augments) && upgrade(grooved_spines);
				
			if (hydralisk_count >= mutalisk_count * 2) build_n(mutalisk, enemy_vulture_count + enemy_tank_count);
		}
		if (st.frame >= 15 * 60 * 14) {
			if (army_supply > enemy_army_supply * 1.25 || count_production(st, drone) < 2) {
				build_n(drone, 60);
			}
		}
		
		//if (st.frame >= 15 * 60 * 16) {
			if (count_production(st, drone) == 0) build_n(drone, 48);
		//}
		
		if ((total_drones_made < 44 && army_supply > enemy_attacking_army_supply * 2) || total_drones_made < 30) {
			build_n(drone, 34);
		}
		
		if (st.frame < 15 * 60 * 12) {
			if (drone_count >= 12) {
				build_n(hatchery, 3);
			}
			if (drone_count >= 15) {
				build_n(lair, 1);
				build_n(zergling, 2);
				build_n(extractor, 1);
			}
		} else {
			if (st.frame >= 15 * 60 * 18) {
				build_n(ultralisk_cavern, 1);
				upgrade(anabolic_synthesis) && upgrade(chitinous_plating);
				upgrade(adrenal_glands);
			}
		}
		//if (count_units_plus_production(st, queens_nest)) {
		if (mutalisk_count >= 6 || st.frame >= 15 * 60 * 15) {
			build_n(evolution_chamber, 2);
			//upgrade_wait(zerg_melee_attacks_1) && upgrade_wait(zerg_melee_attacks_2) && upgrade(zerg_melee_attacks_3);
			upgrade_wait(zerg_missile_attacks_1) && upgrade_wait(zerg_missile_attacks_2) && upgrade(zerg_missile_attacks_3);
			upgrade_wait(zerg_carapace_1) && upgrade_wait(zerg_carapace_2) && upgrade(zerg_carapace_3);
		}
		
		if (st.frame < 15 * 60 * 16 && count_units(st, lair)) {
			build_n(mutalisk, 9) && build_n(hydralisk_den, 1);
		}
		
		if (st.frame >= 15 * 60 * 8) {
			upgrade(metabolic_boost);
			if (static_defence_pos_is_undefended) {
				if (count_production(st, creep_colony) + count_production(st, sunken_colony) == 0) {
					build(sunken_colony);
				}
			}
		} else {
			if (my_completed_units_of_type[unit_types::hatchery].size() >= 2) {
				build_n(sunken_colony, 1);
			}
		}
		
		if (count_units(st, creep_colony)) build(sunken_colony);
		
		if (force_expand && count_production(st, hatchery) == 0) {
			build(hatchery);
		}
		
		
	}
	
};
