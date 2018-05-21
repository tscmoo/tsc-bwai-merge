
struct strat_z_any_13pool_muta : strat_mod {
	
	int total_drones_made = 0;

	virtual void mod_init() override {

	}

	virtual void mod_tick() override {

		auto st = buildpred::get_my_current_state();
		combat::no_aggressive_groups = st.frame < 15 * 60 * 8;
		combat::aggressive_vultures = false;
		combat::aggressive_zerglings = (army_supply >= enemy_army_supply * 1.5 || players::opponent_player->race == race_terran) && enemy_vulture_count == 0;
		
		total_drones_made = my_units_of_type[unit_types::drone].size() + game->self()->deadUnitCount(unit_types::drone->game_unit_type);
		
		if (st.bases.size() >= 4) enable_auto_upgrades = true;
		
		resource_gathering::max_gas = 100.0;
		if (!my_units_of_type[unit_types::lair].empty()) {
			resource_gathering::max_gas = 600.0;
		}
		army_supply = current_used_total_supply - my_workers.size();
		if (being_rushed && army_supply < enemy_army_supply + 4.0 && army_supply < 12.0) {
			if (larva_count && my_units_of_type[unit_types::sunken_colony].size() < 2) resource_gathering::max_gas = 1.0;
			max_bases = 1;
			rm_all_scouts();
			if (my_units_of_type[unit_types::sunken_colony].size() >= 2) combat::no_aggressive_groups = true;
		} else {
			max_bases = 0;
		}
		if (!my_units_of_type[unit_types::mutalisk].empty() && my_hatch_count >= 3) {
			min_bases = 3;
			max_bases = 0;
		}
		
		if (st.frame >= 15 * 60 * 10) resource_gathering::max_gas = 0.0;
		
		get_next_base_check_reachable = mutalisk_count == 0;

	}

	virtual void mod_build(buildpred::state& st) override {

		using namespace unit_types;
		using namespace upgrade_types;

		st.dont_build_refineries = count_units_plus_production(st, extractor) == 1 && st.frame < 15 * 60 * 8;
		st.auto_build_hatcheries = current_frame >= 15 * 60 * 9;

		if (st.frame < 15 * 60 * 12 && my_units_of_type[unit_types::spire].empty()) {
			if (my_units_of_type[unit_types::overlord].size() >= 2) {
				build_n(drone, 18);
				build_n(spire, 1);
				if (my_completed_units_of_type[unit_types::hatchery].size() + my_units_of_type[unit_types::lair].size() >= 2) build_n(sunken_colony, 1);
			}
			if (is_defending) build_n(zergling, (int)(enemy_attacking_army_supply * 0.875));
			build_n(overlord, 2);
			build_n(drone, 14);
			build_n(zergling, 4);
			build_n(lair, 1);
			if (hatch_count < 2) build(hatchery);
			build_n(extractor, 1);
			build_n(spawning_pool, 1);
			if (total_drones_made < 13) build_n(drone, 13);
			
			//if (drone_count >= 18 && enemy_army_supply >= 8) build_n(sunken_colony, std::max((int)(enemy_army_supply / 2.5), 5));
			return;
		}
		
		if (st.frame >= 15 * 60 * 6 && (drone_count >= 24 || (is_defending && army_supply < enemy_attacking_army_supply - sunken_count * 3) || !st.units[spire].empty())) {
			build(zergling);
			build(mutalisk);
			
			if (st.frame >= 15 * 60 * 9) {
				if (mutalisk_count >= 9 || zergling_count >= 12) upgrade(metabolic_boost);
				
				if (zergling_count < enemy_missile_turret_count * 3 - enemy_vulture_count * 4 + enemy_goliath_count * 2) {
					build_n(zergling, mutalisk_count * 3);
				}
				
				if (mutalisk_count >= 24) {
					build_n(zergling, mutalisk_count);
				}
			}
			
			if (scourge_count < enemy_air_army_supply) build(scourge);
		} else build(drone);
		
		if (st.frame >= 15 * 60 * 7 + 15 * 30 && !opponent_has_expanded && enemy_factory_count == 0 && mutalisk_count == 0) {
			build_n(sunken_colony, 3);
			if (enemy_attacking_army_supply - enemy_vulture_count * 2 >= 4.0) build_n(sunken_colony, 4);
		}
		if (drone_count < 22 && st.units[spire].empty()) {
			if (drone_count >= 16 && enemy_marine_count >= 8) build_n(sunken_colony, std::max((int)(enemy_army_supply / 2.5), 5));
		}
		
		if (mutalisk_count >= 10) {
			upgrade(zerg_flyer_carapace_1) && upgrade(zerg_flyer_attacks_1) && upgrade(zerg_flyer_carapace_2) && upgrade(zerg_flyer_attacks_2);
			if (mutalisk_count >= 20) {
				upgrade(zerg_melee_attacks_1) && upgrade(zerg_carapace_1);
			}
		}
		
		if (my_completed_units_of_type[unit_types::hatchery].size() + my_units_of_type[unit_types::lair].size() >= 2) {
			if (!opponent_has_expanded) {
				if (enemy_static_defence_count) build_n(sunken_colony, 1);
				else build_n(sunken_colony, 2);
			}
		}
		
		if (is_defending && my_completed_units_of_type[unit_types::spire].empty()) build_n(zergling, (int)(enemy_attacking_army_supply * 0.875));
		
//		if (hydralisk_count >= 4) {
//			upgrade(grooved_spines);
//			upgrade(muscular_augments);
//		}
		
		if (std::max(enemy_army_supply - 6.0, enemy_attacking_army_supply) >= 4.0 && army_supply < 8.0) {
			if (my_completed_units_of_type[unit_types::hatchery].size() + my_units_of_type[unit_types::lair].size() >= 2) build_n(sunken_colony, 2);
		}
		
		if (st.frame >= 15 * 60 * 15) {
			if (can_expand && drone_count >= max_mineral_workers + 8 && count_production(st, hatchery) == 0) {
				build(hatchery);
			} else if (count_production(st, drone) == 0 && total_drones_made < 6 + st.frame / 15 * 30) {
				build_n(drone, 48);
			}
			
			if (drone_count >= 29) {
				if (upgrade_wait(pneumatized_carapace)) {
					if (st.bases.size() >= 3) {
						build_n(hive, 1) && upgrade(adrenal_glands);
					}
				}
			}
		}
		
		if (army_supply > enemy_army_supply || army_supply >= 6.0) build_n(drone, 20);
		if (mutalisk_count >= 11 && mutalisk_count * 2 >= std::max(7, (int)(enemy_army_supply / 2.0))) build_n(drone, 32);
		
		if (count_units_plus_production(st, creep_colony)) build(sunken_colony);
		
		if (st.frame < 15 * 60 * 11) {
			bool make_lings = zergling_count < enemy_zealot_count * 3;
			if (players::opponent_player->race == race_protoss || enemy_zealot_count) {
				if (drone_count >= 14 && zergling_count < 8) make_lings = true;
			}
			if (make_lings) {
				//build_n(hatchery, 3);
				upgrade(metabolic_boost);
				if (my_completed_units_of_type[unit_types::hatchery].size() >= 2) {
					if (enemy_army_supply >= 6.0) build_n(sunken_colony, 3);
					else build_n(sunken_colony, 2);
				}
				build(zergling);
			}
		}
	}

};
