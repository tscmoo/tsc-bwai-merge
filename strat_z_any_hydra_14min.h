
struct strat_z_any_hydra_14min : strat_mod {
	
	int total_drones_made = 0;

	virtual void mod_init() override {

	}

	virtual void mod_tick() override {

		auto st = buildpred::get_my_current_state();
		if (st.frame < 15 * 60 * 14) combat::no_aggressive_groups = true;
		else if (st.frame < 15 * 60 * 18) combat::no_aggressive_groups = false;
		else attack_interval = 15 * 30;
		combat::aggressive_vultures = false;
		combat::aggressive_zerglings = (army_supply >= enemy_army_supply * 1.5 || players::opponent_player->race == race_terran) && enemy_vulture_count == 0;
		
		total_drones_made = my_units_of_type[unit_types::drone].size() + game->self()->deadUnitCount(unit_types::drone->game_unit_type);
		
		if (st.bases.size() >= 4) enable_auto_upgrades = true;
		
		//if (current_frame >= 15 * 60 * 9) min_bases = 4;
		if (current_frame < 15 * 60 * 14) {
			if (my_workers.size() >= 15) resource_gathering::max_gas = 400.0;
			max_bases = 2;
		} else {
			get_next_base_check_reachable = false;
			min_bases = 3;
			max_bases = 0;
		}

	}

	virtual void mod_build(buildpred::state& st) override {

		using namespace unit_types;
		using namespace upgrade_types;

		st.dont_build_refineries = count_units_plus_production(st, extractor) == 1 && st.frame < 15 * 60 * 8;
		st.auto_build_hatcheries = current_frame >= 15 * 60 * 9;
		
		if (st.frame < 15 * 60 * 11) {
			bool make_lings = zergling_count < enemy_zealot_count * 3;
			if (players::opponent_player->race == race_protoss || enemy_zealot_count) {
				if (drone_count >= 14 && zergling_count < 8) make_lings = true;
			}
			if (make_lings) {
				build_n(hatchery, 3);
				upgrade(metabolic_boost);
				if (my_completed_units_of_type[unit_types::hatchery].size() >= 2) {
					if (enemy_army_supply > 8.0) build_n(sunken_colony, 3);
					else build_n(sunken_colony, 2);
				}
				build(zergling);
			}
			
			if (players::opponent_player->race == race_protoss || enemy_zealot_count) {
				if (!opponent_has_expanded && enemy_cannon_count == 0) {
					if (my_completed_units_of_type[unit_types::hatchery].size() >= 2) build_n(sunken_colony, 1);
				}
			}
			
			if (make_lings) return;
		}

		if (st.frame < 15 * 60 * 8 && my_units_of_type[unit_types::hydralisk_den].empty()) {
			build_n(hydralisk_den, 1);
			build_n(drone, 20);
			if (my_completed_units_of_type[unit_types::hatchery].size() >= 2) build_n(sunken_colony, 1);
			build_n(drone, 14);
			build_n(zergling, 3);
			build_n(hatchery, 2);
			build_n(drone, 12);
			return;
		}
		
		build_n(hatchery, 5);
		
		if (st.frame < 15 * 60 * 10) {
			
			build(hydralisk);
			build_n(hatchery, 3);
			
//			if (hydralisk_count >= 4) {
//				if (army_supply > enemy_army_supply * 0.75 + enemy_attacking_army_supply) {
//					build_n(drone, 28);
//				}
//			}
			
			if (my_completed_units_of_type[unit_types::hatchery].size() >= 2) build_n(sunken_colony, 1);
			
			if (hydralisk_count >= 4) {
				upgrade(muscular_augments);
				upgrade(grooved_spines);
			}
			
			//build_n(extractor, 2);
			build_n(drone, 16);
			
			return;
		}
		
		if (st.frame >= 15 * 60 * 10) {
			build(zergling);
			build(hydralisk);
			
			//upgrade(metabolic_boost) && upgrade(muscular_augments);
			
			if (drone_count >= 30) {
				upgrade(zerg_missile_attacks_1) && upgrade(zerg_missile_attacks_2) && upgrade(zerg_missile_attacks_3);
			}
			
			//build_n(drone, 28);
			build_n(drone, 20);
		}
		
		if (st.frame >= 15 * 60 * 14 && st.bases.size() < 3 && hatch_count < 6 && can_expand) {
			if (army_supply >= std::min(enemy_army_supply * 0.75, 24.0)) {
				build(hatchery);
			}
		}

		if (hydralisk_count + scourge_count < enemy_air_army_supply) {
			build(hydralisk);
		}
		
		if (hydralisk_count >= 4) {
			upgrade(muscular_augments);
			upgrade(grooved_spines);
		}
		
		if (std::max(enemy_army_supply - 6.0, enemy_attacking_army_supply) >= 4.0 && army_supply < 8.0) {
			if (total_units_made_of_type[hydralisk] < 4) {
				if (my_completed_units_of_type[unit_types::hatchery].size() >= 2) build_n(sunken_colony, 3);
			}
		}
		
		if (st.frame >= 15 * 60 * 15) {
			if (can_expand && drone_count >= max_mineral_workers + 8 && count_production(st, hatchery) == 0) {
				build(hatchery);
			} else if (count_production(st, drone) == 0 && total_drones_made < 6 + st.frame / 15 * 30) {
				build_n(drone, 48);
			}
		}
		
		if (army_supply > enemy_army_supply || army_supply >= 6.0) build_n(drone, 20);
	}

};
