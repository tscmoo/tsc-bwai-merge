bool fast_hive = false;
bool faster_hive_tech = true;
bool early_4_bases = true;

void strat_z_mod::mod_test_tick() {

	//scouting::scout_supply = 30.0;
	scouting::scout_supply = 200.0;
	rm_all_scouts();

	if (drone_count >= 30) {
		min_bases = 3;
		if (early_4_bases) min_bases = 4;
		if (drone_count >= max_mineral_workers) min_bases = 8;
	}
	combat::no_aggressive_groups = army_supply < 60.0 && drone_count < 48;
	//combat::no_aggressive_groups = current_frame < 15*60*12 || enemy_army_supply + drone_count / 2 > army_supply;

	if (enemy_biological_army_supply <= enemy_army_supply * 0.25) {
		if (hydralisk_count / 2 > enemy_tank_count) {
			combat::no_aggressive_groups = false;
		}
	}
	//log(log_level_test, "combat::no_aggressive_groups is %d\n", combat::no_aggressive_groups);

	if (current_frame < 15 * 60 * 15) {
		if (fast_hive) resource_gathering::max_gas = 600.0;
	}
	if (current_frame >= 15 * 60 * 10) {
		resource_gathering::max_gas = 1200.0;
	}

	//combat::dont_attack_main_army = current_used_total_supply < 180 && army_supply < enemy_army_supply * 2 && current_frame < 15*60*40;
	combat::dont_attack_main_army = current_used_total_supply < 180 && army_supply < 70 && current_frame < 15 * 60 * 40;
	combat::aggressive_groups_done_only = combat::dont_attack_main_army;
	combat::no_aggressive_groups = false;

	//log(log_level_test, "enemy_biological_army_supply %g, enemy_army_supply %g\n", enemy_biological_army_supply, enemy_army_supply);

	//log(log_level_test, "is_defending %d\n", is_defending);

	//log(log_level_test, "threats::being_marine_rushed is %d\n", threats::being_marine_rushed);
	//log(log_level_test, "zergling_count %d enemy_marine_count %d current_minerals %g\n", zergling_count, enemy_marine_count, current_minerals);

//	if (threats::being_marine_rushed && zergling_count / 3 < enemy_marine_count && current_minerals < 100) {
//		if (my_completed_units_of_type[unit_types::hatchery].size() == 2) {
//			for (auto& b : build::build_tasks) {
//				if (b.type->unit == unit_types::hatchery) {
//					b.dead = true;
//					if (b.built_unit) b.built_unit->game_unit->cancelConstruction();
//				}
//			}
//		}
//	}
}

void strat_z_mod::mod_test_build(buildpred::state& st) {

	using namespace unit_types;
	using namespace upgrade_types;

	int desired_drone_count = 0;

	bool go_hydras = hydralisk_count < enemy_vulture_count + enemy_goliath_count * 2 + enemy_tank_count * 3;
	//bool go_hydras = true;
	if (enemy_biological_army_supply < 8) go_hydras = true;
	else {
		if (enemy_biological_army_supply > enemy_army_supply * 0.75) go_hydras = false;
		else if (enemy_biological_army_supply <= enemy_army_supply * 0.125) go_hydras = true;
	}
	if (drone_count < 24 && enemy_biological_army_supply < 6.0) go_hydras = true;
	if (hydralisk_count >= 20 && lurker_count < hydralisk_count / 8) go_hydras = false;

	if (has_or_is_upgrading(st, consume)) go_hydras = false;

	if (st.frame <= 15*60*10) {
		st.dont_build_refineries = drone_count < 26;
		desired_drone_count = fast_hive ? 30 : 40;
		if (is_defending && drone_count > army_supply && army_supply < enemy_attacking_army_supply) desired_drone_count = drone_count;
		if (drone_count < desired_drone_count && drone_count < 66 && (current_frame < 15 * 60 * 10 || army_supply > enemy_army_supply || army_supply >= 20.0)) {
			build(drone);
		} else if (drone_count < 16 && enemy_vulture_count == 0) {
			build(zergling);
		} else {
			if (hydralisk_count >= enemy_vulture_count) build(zergling);
			build(hydralisk);
			if (hydralisk_count > enemy_vulture_count + enemy_tank_count + enemy_goliath_count && !go_hydras) build(lurker);
			if (has_or_is_upgrading(st, consume)) {
				build_n(defiler, lurker_count / 4 + zergling_count / 14);
			}

			if (army_supply >= 40.0) {
				build_n(hydralisk, (int)enemy_air_army_supply);
			} else {
				build_n(hydralisk, enemy_wraith_count);
			}
			if (st.used_supply[race_zerg] >= 150) {
				build_n(mutalisk, 4);
			}
		}
		if (go_hydras && drone_count >= 18 && ((enemy_bunker_count == 0 && !opponent_has_expanded) || enemy_vulture_count)) {
			build_n(hydralisk, 4);
		}
		if (fast_hive && drone_count >= 28) {
			build(hive);
		}
		if (drone_count >= 16) {
			if (((count_units_plus_production(st, unit_types::hive)) || build_n(lair, 1)) && drone_count >= 21) {
				upgrade(go_hydras ? muscular_augments : lurker_aspect) &&
					upgrade(zerg_carapace_1) &&
					upgrade(zerg_missile_attacks_1);
			}
		}
		if (drone_count >= 14) {
			if (my_completed_units_of_type[unit_types::hatchery].size() + my_units_of_type[unit_types::lair].size() >= 2) {
				build_n(sunken_colony, 1);
			}
		}
		if (hatch_count < 3) {
			if (drone_count >= 15) {
				build(hatchery);
				build_n(extractor, 1);
			}
		} else {
			build_n(extractor, 1) &&
				build_n(zergling, 2);
		}
		if (zergling_count < 6 && enemy_worker_count < 14 && enemy_gas_count == 0 && sunken_count == 0) {
			build_n(zergling, 6);
		}

		if (threats::being_marine_rushed && (zergling_count - 4) / 3 <= enemy_marine_count) {
			if (is_defending || zergling_count < 6) build_n(zergling, drone_count);
			if (my_completed_units_of_type[unit_types::hatchery].size() + my_units_of_type[unit_types::lair].size() >= 2) {
				build_n(sunken_colony, 1);
				if (enemy_marine_count >= 6) build_n(sunken_colony, 2);
			}
		}

		if (build_nydus_pos != xy() && !has_nydus_without_exit) {
			build_n(nydus_canal, my_units_of_type[unit_types::nydus_canal].size() + 1);
		}

		if (force_expand && count_production(st, unit_types::hatchery) == 0) {
			build(hatchery);
		}
	} else {
		st.dont_build_refineries = drone_count < 26;
		desired_drone_count = 36;
		//if (count_production(st, unit_types::drone) <= (drone_count >= 50 ? 1 : 0)) {
		if (count_production(st, unit_types::drone) <= (int)((army_supply + 1) / drone_count)) {
			desired_drone_count = drone_count + 1;
		}
		if (st.frame < 15*60*20 && desired_drone_count > 48) desired_drone_count = 48;
		else desired_drone_count = 48 + (st.frame - 15*60*20) / (15*30);
		if (enemy_attacking_army_supply < army_supply / 2) {
			if (army_supply >= 18.0 && enemy_bunker_count && drone_count < 40) desired_drone_count = 40;
			if (army_supply >= 30.0 && army_supply >= enemy_army_supply * 1.5 && drone_count < 60) desired_drone_count = 60;
		}
		if (is_defending && drone_count > army_supply && army_supply < enemy_army_supply) desired_drone_count = drone_count;
		if (fast_hive) desired_drone_count -= 6;
		if (drone_count < desired_drone_count && drone_count < 66) {
			build(drone);
		} else {
			if (army_supply >= enemy_army_supply && drone_count < 46 && enemy_attacking_army_supply < 8.0) build(drone);
			//else build(zergling);
			build(hydralisk);
			//if (hydralisk_count > enemy_vulture_count + enemy_tank_count + enemy_goliath_count) build(lurker);
			if (!go_hydras) build(lurker);
			if (drone_count >= 50 || has_or_is_upgrading(st, anabolic_synthesis)) build(ultralisk);
			//if (enemy_tank_count >= 8 && mutalisk_count < enemy_tank_count * 2 - enemy_anti_air_army_supply / 2) build(mutalisk);
			if (has_or_is_upgrading(st, consume)) {
				build_n(defiler, (lurker_count + 3) / 4 + zergling_count / 14);
			}

			if (army_supply >= 40.0) {
				build_n(hydralisk, (int)enemy_air_army_supply);
			} else {
				//build_n(hydralisk, enemy_wraith_count);
				build_n(hydralisk, (int)enemy_air_army_supply - enemy_science_vessel_count * 2);
			}
			if (st.used_supply[race_zerg] >= 150) {
				build_n(mutalisk, 4);
			}
		}
		if (drone_count >= 16) {
			if (((count_units_plus_production(st, unit_types::hive)) || build_n(lair, 1)) && drone_count >= 21) {
				upgrade(metabolic_boost) &&
					upgrade(go_hydras ? muscular_augments : lurker_aspect) &&
					build_n(extractor, 2) &&
					upgrade(zerg_carapace_1) &&
					upgrade(zerg_missile_attacks_1);
				if (drone_count >= 40 && (has_upgrade(st, zerg_missile_attacks_1) || count_units_plus_production(st, evolution_chamber) >= 2)) {
					upgrade(zerg_carapace_2) && upgrade(zerg_carapace_3);
					upgrade(zerg_missile_attacks_2) && upgrade(zerg_missile_attacks_3);
					upgrade(anabolic_synthesis);
				}
				if (drone_count >= 40 && army_supply >= 20.0) {
					upgrade(pneumatized_carapace);
				}
				if (drone_count >= 32 && (enemy_tank_count >= 4 || drone_count >= 42)) {
					//build_n(mutalisk, 1);
				}
				int hive_at = 50;
				if (faster_hive_tech) hive_at = 40;
				if (drone_count >= hive_at || army_supply >= hive_at) {
					build_n(hive, 1) &&
						build_n(evolution_chamber, 2) &&
						upgrade(zerg_missile_attacks_1) &&
						upgrade(consume) &&
						upgrade(zerg_carapace_2) &&
						upgrade(zerg_missile_attacks_2) &&
						upgrade(zerg_carapace_3) &&
						upgrade(zerg_missile_attacks_3) &&
				        upgrade(zerg_melee_attacks_1) &&
						upgrade(zerg_melee_attacks_2) &&
						upgrade(zerg_melee_attacks_3);
					if (drone_count >= 55) upgrade(adrenal_glands);
				}
				if (fast_hive) {
					upgrade(consume);
				}
				//if (hydralisk_count >= 8 || enemy_vulture_count + enemy_goliath_count >= 5) {
				if (hydralisk_count >= 8) {
					upgrade(grooved_spines) &&
						upgrade(muscular_augments);
				}
			}
		}
		if (drone_count >= 36 && enemy_biological_army_supply < enemy_army_supply * 0.25) {
			build_n(evolution_chamber, 3);
			upgrade(zerg_melee_attacks_1) && upgrade(zerg_melee_attacks_2) && upgrade(zerg_melee_attacks_3);
		}
		if (drone_count >= 32) {
			if (hatch_count < (drone_count >= 46 ? 8 : 6)) {
				build(hatchery);
			}
		}
		if (drone_count >= 14) {
			if (my_completed_units_of_type[unit_types::hatchery].size() + my_units_of_type[unit_types::lair].size() >= 2) {
				build_n(sunken_colony, 1);
			}
		}
		if (drone_count >= 38) {
			build_n(hydralisk, 4);
		}
		if (hatch_count < 3) {
			if (drone_count >= 15) {
				build(hatchery);
				build_n(extractor, 1);
			}
		} else {
			build_n(extractor, 1) &&
				build_n(zergling, 2);
		}

//		if (st.frame < 15 * 60 * 20 && drone_count >= 16) {
//			if (my_completed_units_of_type[unit_types::hatchery].size() + my_units_of_type[unit_types::lair].size() >= 2) {
//				if (threats::nearest_attack_time <= unit_types::creep_colony->build_time + unit_types::sunken_colony->build_time + 15 * 15) {
//					int n_sunkens = (int)((enemy_army_supply - army_supply) / 5) + 1;
//					if (n_sunkens >= 4) n_sunkens = 4;
//					build_n(sunken_colony, n_sunkens);
//				}
//			}
//		}

//		if (drone_count >= 48 && static_defence_pos != xy()) {
//			build_n(sunken_colony, st.bases.size() * 2 - 2);
//		}

		if (build_nydus_pos != xy() && !has_nydus_without_exit) {
			build_n(nydus_canal, my_units_of_type[unit_types::nydus_canal].size() + 1);
		}

		if (faster_hive_tech && drone_count >= 40) {
			upgrade(anabolic_synthesis);
			upgrade(consume);
			build_n(hive, 1);
		}

		if (force_expand && count_production(st, unit_types::hatchery) == 0) {
			build(hatchery);
		}
	}

}
