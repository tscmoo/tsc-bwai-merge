
void strat_z_mod::mod_test2_tick() {

	//scouting::scout_supply = 30.0;
	if (zergling_count == 0 && drone_count < 18) {
		scouting::scout_supply = 12.0;
	} else {
		scouting::scout_supply = 200.0;
		rm_all_scouts();
	}

	resource_gathering::max_gas = 1200.0;

	//combat::dont_attack_main_army = current_used_total_supply < 180 && army_supply < enemy_army_supply * 2 && current_frame < 15*60*40;
	combat::dont_attack_main_army = current_used_total_supply < 180 && army_supply < 70 && current_frame < 15 * 60 * 40;
	//combat::aggressive_groups_done_only = combat::dont_attack_main_army;
	combat::aggressive_groups_done_only = false;
	combat::no_aggressive_groups = false;

	if (hatch_count >= 3) min_bases = 5;

	get_next_base_check_reachable = drone_count < 45 || army_supply < drone_count;

	combat::use_map_split_defence = true;
	if (mutalisk_count == 0 && army_supply < enemy_army_supply * 1.5 && drone_count < 30) {
		combat::use_map_split_defence = false;
	}

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

void strat_z_mod::mod_test2_build(buildpred::state& st) {

	using namespace unit_types;
	using namespace upgrade_types;

	bool lurker_defence = false;
	if (st.frame < 15*60*15 && enemy_army_supply - enemy_biological_army_supply < 16.0 && drone_count > enemy_worker_count) {
		lurker_defence = true;
	}

	bool enemy_is_bio = enemy_biological_army_supply >= 8.0 && enemy_biological_army_supply >= enemy_army_supply * 0.5;

	bool build_army = false;
	if (drone_count >= 24 + 2 * sunken_count || is_defending) {
		if (enemy_is_bio || is_defending) {
			if (drone_count >= 34 && army_supply < drone_count && army_supply < std::max(enemy_army_supply * 2, 15.0)) build_army = true;
		}
		if (drone_count >= 34 && army_supply < drone_count / 2) build_army = true;
		if (army_supply < enemy_army_supply) build_army = true;
		if (army_supply < enemy_attacking_army_supply * 1.5) build_army = true;
		if (army_supply >= enemy_army_supply * 0.75 && count_production(st, drone) == 0) build_army = false;
	}

	bool go_lurkers = false;
	if (enemy_is_bio) go_lurkers = true;

	if (lurker_defence) {
		if (!has_or_is_upgrading(st, lurker_aspect)) go_lurkers = true;
	}
	//if (lurker_defence && drone_count >= 30 && army_supply < drone_count) build_army = true;
	bool build_drones = !build_army;

	bool tech_up = false;
	// army_supply > enemy_army_supply || (drone_count >= 48 && army_supply >= enemy_army_supply * 0.75)
	if (army_supply >= enemy_army_supply * 1.25) tech_up = true;
	if (drone_count >= 50 && army_supply >= enemy_army_supply) tech_up = true;

	if (lurker_defence && army_supply < 20.0) tech_up = false;

	if (lurker_defence && drone_count >= 34) {
		if (army_supply < drone_count && current_frame < 15 * 60 * 15) {
			if (army_supply < enemy_army_supply) {
				build_army = true;
				build_drones = false;
			} else {
				build_army = false;
				build_drones = false;
				tech_up = false;
				build_n(spire, 1);
				if (enemy_tank_count * 4 + enemy_vulture_count - enemy_anti_air_army_supply >= 12) {
					build(mutalisk);
				}
			}
		}
	}
	if (drone_count >= 66) {
		build_drones = false;
		build_army = true;
		tech_up = true;
	}

	if (drone_count < 55 && army_supply >= drone_count && count_production(st, drone) == 0) build_army = false;

	if (build_drones) build(drone);

	if (drone_count >= 28 && hatch_count < 5) {
		build(hatchery);
	}

	if (drone_count >= 12 && hatch_count < 3) {
		build(hatchery);
	}

	if (drone_count >= 28) {
		upgrade_wait(zerg_carapace_1) && upgrade_wait(zerg_carapace_2);
	}

	if (drone_count >= 18) {
		//build_n(queen, 1);
		upgrade(metabolic_boost);
		if (go_lurkers) upgrade(lurker_aspect);
		build_n(hydralisk_den, 1);
		build_n(lair, 1);
	}

	if (build_army) {
		if (st.minerals >= 400) build(zergling);
		build(hydralisk);
		if (go_lurkers) build(lurker);
		if (has_or_is_upgrading(st, anabolic_synthesis) && has_or_is_upgrading(st, zerg_melee_attacks_2)) {
			build(ultralisk);
		}
		if (has_or_is_upgrading(st, consume)) {
			build_n(defiler, (int)(army_supply / 20.0));
		}
		if (army_supply >= 20 && queen_count < enemy_tank_count && army_supply >= enemy_army_supply) {
			if (enemy_wraith_count + enemy_science_vessel_count + enemy_corsair_count + enemy_corsair_count + enemy_mutalisk_count + enemy_scourge_count < 4) {
				build_n(queen, 4);
				upgrade(spawn_broodling);
			}
		}

		if ((drone_count >= 40 || count_units_plus_production(st, spire))) {
			if (enemy_anti_air_army_supply < enemy_tank_count * 2 || hydralisk_count > enemy_anti_air_army_supply) {
				build_n(mutalisk, 32);
			}
		}
	}

	if (go_lurkers && hydralisk_count > enemy_vulture_count + enemy_goliath_count * 2 + (int)enemy_air_army_supply) {
		build(lurker);
	}

	if (drone_count < 40) {
		if (hydralisk_count < enemy_vulture_count + enemy_goliath_count * 2) {
			build(hydralisk);
		}
	}

	build_n(hydralisk, (int)enemy_air_army_supply);

	if (drone_count >= 24) {
		if (army_supply >= enemy_army_supply && enemy_army_supply >= 8.0 && enemy_anti_air_army_supply < enemy_army_supply * 0.5) {
			build_n(spire, 1);
		}
	}

	if (tech_up) {
		if (drone_count >= 44) {
			upgrade_wait(zerg_flyer_attacks_1) && upgrade_wait(zerg_flyer_attacks_2) && upgrade_wait(zerg_flyer_attacks_3);
			upgrade_wait(zerg_flyer_carapace_1) && upgrade_wait(zerg_flyer_carapace_2) && upgrade_wait(zerg_flyer_carapace_3);
			if (has_upgrade(st, zerg_flyer_carapace_1)) upgrade_wait(zerg_flyer_attacks_1);
			if (has_or_is_upgrading(st, zerg_melee_attacks_1)) {
				if (!go_lurkers || has_upgrade(st, consume)) {
					upgrade(chitinous_plating);
					upgrade(anabolic_synthesis);
				}
			}
			if (has_upgrade(st, zerg_melee_attacks_2)) upgrade(zerg_melee_attacks_3);
			else if (has_upgrade(st, zerg_melee_attacks_1)) upgrade(zerg_melee_attacks_3);
			else upgrade(zerg_melee_attacks_1);
			if (!has_upgrade(st, zerg_carapace_3) || !has_upgrade(st, zerg_missile_attacks_3)) {
				if (!go_lurkers || has_upgrade(st, consume)) build_n(evolution_chamber, 3);
			}
		}

		if (drone_count >= 28) {
			if (!go_lurkers || has_upgrade(st, consume)) build_n(spire, 1);
			if (count_units(st, hive) && army_supply >= 28) {
				upgrade(adrenal_glands);
			}
			if (drone_count >= 60) upgrade(plague);
			if (ultralisk_count || drone_count >= 60 || go_lurkers) upgrade(consume);
			if (go_lurkers || drone_count >= 42) build_n(hive, 1);
		}

		if (drone_count >= 24) {
			if (hydralisk_count >= 16 || enemy_biological_army_supply >= enemy_army_supply * 0.66) {
				if (has_upgrade(st, zerg_missile_attacks_2)) upgrade(zerg_missile_attacks_3);
				else if (has_upgrade(st, zerg_missile_attacks_1)) upgrade(zerg_missile_attacks_2);
				else upgrade(zerg_missile_attacks_1);
			} else {
				upgrade(zerg_missile_attacks_1);
			}
			if (army_supply >= 20) {
				if (!go_lurkers || has_upgrade(st, consume)) build_n(evolution_chamber, 2);
				if (has_upgrade(st, zerg_carapace_2)) upgrade(zerg_carapace_3);
				else if (has_upgrade(st, zerg_carapace_1)) upgrade(zerg_carapace_2);
				else upgrade(zerg_carapace_1);
			}
			if (enemy_army_supply - enemy_biological_army_supply >= 16.0 || drone_count >= 36) {
				upgrade(grooved_spines);
				upgrade(muscular_augments);
			}
			if (enemy_biological_army_supply >= 8.0 || drone_count >= 40) {
				upgrade(lurker_aspect);
			}
		}
	}

	if (drone_count >= 24 && st.bases.size() == 2) {
		if (!opponent_has_expanded || enemy_army_supply > army_supply) {
			build_n(sunken_colony, 3);
		}
	}

	if (go_lurkers && drone_count >= 34) {
		upgrade(consume);
	}

	if (force_expand && count_production(st, unit_types::hatchery) == 0) {
		build(hatchery);
	}

	if ((enemy_attacking_worker_count || drone_count >= 14) && drone_count < 20) {
		build_n(zergling, 1 + enemy_attacking_worker_count);
	}
	if (drone_count >= 30 && army_supply > enemy_army_supply) {
		build_n(zergling, 8);
	}

	if (st.frame < 15 * 60 * 15) {
		if (drone_count >= 14) {
			if (my_completed_units_of_type[unit_types::hatchery].size() + my_units_of_type[unit_types::lair].size() >= 2) {
				build_n(sunken_colony, 1);
			}
		}

		if (enemy_army_supply < 8.0 || army_supply > enemy_army_supply) {
			if (enemy_attacking_army_supply < 4.0) {
				build_n(queen, 1);
			}
		}
	}

}
