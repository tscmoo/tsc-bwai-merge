
struct strat_t_any_vz3 : strat_mod {

	int wall_calculated = 0;
	bool has_wall = false;
	wall_in::wall_builder wall;
	
	int total_scvs_made = 0;

	virtual void mod_init() override {
		//combat::defensive_spider_mine_count = 40;
		//early_defence_is_scared = false;
	}

	virtual void mod_tick() override {

		auto st = buildpred::get_my_current_state();
		//combat::no_aggressive_groups = st.frame < 15 * 60 * 6 && my_units_of_type[unit_types::vulture].empty();
		if (st.frame < 15 * 60 * 15) {
			if (enemy_lurker_count && army_supply < enemy_army_supply + 12.0 && false) {
				combat::no_aggressive_groups = true;
			} else {
				attack_interval = 15 * 30;
			}
		//if (st.frame < 15 * 60 * 11) {
		//	combat::no_aggressive_groups = true;
		} else {
			attack_interval = 0;
			combat::no_aggressive_groups = false;
		}

		
		total_scvs_made = my_units_of_type[unit_types::scv].size() + game->self()->deadUnitCount(unit_types::scv->game_unit_type);
		
		combat::use_decisive_engagements = false;
		combat::attack_bases = true;
		
		//combat::defensive_spider_mine_count = 40;
		
		//if (scv_count >= 12) resource_gathering::max_gas = 1000.0;
		
		scouting::comsat_supply = 80.0;
		scouting::scan_disabled = true;
		
		base_harass_defence::defend_enabled = false;
		
		place_static_defence_only_at_main = !my_units_of_type[unit_types::bunker].empty() && my_units_of_type[unit_types::missile_turret].size() < 2;
		
		if (army_supply >= 60.0) combat::build_missile_turret_count = 5;
		if (army_supply >= 90.0) combat::build_missile_turret_count = 15;
		
		//if (enemy_air_army_supply > valkyrie_count * 3 + 4.0) combat::build_missile_turret_count = 5;
		
//		bool keep_valkyries_back = valkyrie_count < enemy_air_army_supply / 2.5;
		
//		for (auto* a : combat::live_combat_units) {
//			if (a->u->type == unit_types::marine) {
//				a->never_assign_to_aggressive_groups = true;
//			}
//			if (a->u->type == unit_types::valkyrie) {
//				a->never_assign_to_aggressive_groups = keep_valkyries_back;
//			}
//		}

//		if (combat::defence_choke.center != xy()) {
//			if (wall_calculated < 2) {
//				++wall_calculated;

//				wall = wall_in::wall_builder();

//				wall.spot = wall_in::find_wall_spot_from_to(unit_types::zealot, combat::my_closest_base, combat::defence_choke.outside, false);
//				wall.spot.outside = combat::defence_choke.outside;

//				wall.against(wall_calculated == 1 ? unit_types::zergling : unit_types::hydralisk);
//				wall.add_building(unit_types::academy);
//				wall.add_building(unit_types::barracks);
//				has_wall = true;
//				if (!wall.find()) {
//					wall.add_building(unit_types::supply_depot);
//					if (!wall.find()) {
//						log(log_level_test, "failed to find wall in :(\n");
//						has_wall = false;
//					}
//				}
//				if (has_wall) wall_calculated = 2;
//			}
//		}
//		if (has_wall) {
//			if (!being_zergling_rushed) wall.build();
//			if (current_used_total_supply >= 45) {
//				wall = wall_in::wall_builder();
//				has_wall = false;
//			}
//		}

		if (army_supply > enemy_army_supply || enemy_cloaked_unit_count) {
			if (current_used_total_supply >= 28.0) scouting::comsat_supply = 28.0;
		}

	}

	virtual void mod_build(buildpred::state& st) override {

		using namespace unit_types;
		using namespace upgrade_types;

		st.dont_build_refineries = count_units_plus_production(st, refinery) == 1 &&st.frame < 15 * 60 * 11;
		
		//if (st.frame < 15 * 60 * 8 && my_units_of_type[unit_types::academy].empty()) {
		if (st.frame < 15 * 60 * 8 && my_units_of_type[unit_types::barracks].size() < 2) {
			build_n(barracks, 2);
			build_n(marine, 3);
			//build_n(academy, 1) && build_n(refinery, 1);
			build_n(scv, players::opponent_player->race == race_terran ? 16 : 14);
			build_n(marine, 1);
			//build_n(supply_depot, 2);
			build_n(barracks, 1);
			build_n(supply_depot, 1);
			build_n(scv, 11);
			if (players::opponent_player->race == race_zerg || players::opponent_player->random) {
				build_n(barracks, 1) && build_n(marine, 3 + enemy_attacking_army_supply / 2.0);
				build_n(scv, 8);
			}
			return;
		}

		auto anti_lurker_bunkers = [&]() {
			if (enemy_lurker_count) {
				build_n(missile_turret, 2);
				build_n(bunker, 3);
			}
		};

		maxprod(vulture);
		upgrade(spider_mines) && upgrade(ion_thrusters);
		//build_n(barracks, 4);
		build(marine);

		if (st.frame < 15 * 60 * 12) {
			build_n(medic, marine_count / 2);
			if (army_supply > enemy_army_supply + 8.0) {
				//build_n(starport, 1);
				//build_n(engineering_bay, 1) && upgrade(terran_infantry_weapons_1);
			}
			build_n(cc, 2);
			build_n(marine, 6);
			build_n(scv, 20);

			//upgrade(u_238_shells) && upgrade(stim_packs);

			//build_n(medic, 2);
			//build_n(firebat, 2);
			build_n(marine, 2 + (int)enemy_army_supply);
			build_n(barracks, 2);

			if (army_supply > enemy_army_supply + 2.0) {
				build_n(scv, 30);
			}
			//anti_lurker_bunkers();
			return;
		}


		if (marine_count > enemy_air_army_supply) {
			//build_n(firebat, enemy_ground_army_supply - enemy_lurker_count * 4);
			//build_n(ghost, firebat_count + enemy_lurker_count * 2);
			if (tank_count || army_supply >= 18.0) {
				build(siege_tank_tank_mode);
				if (tank_count) upgrade(siege_mode);
			}
		}
		build_n(medic, marine_count / 4);

		upgrade(u_238_shells) && upgrade(stim_packs);

		if (army_supply > enemy_army_supply + 16.0) {
			build_n(science_vessel, (int)(army_supply / 16.0));
			if (science_vessel_count) upgrade(irradiate);

			build_n(engineering_bay, 2);
			upgrade_wait(terran_infantry_armor_1) && upgrade_wait(terran_infantry_armor_2) && upgrade_wait(terran_infantry_armor_3);
			upgrade_wait(terran_infantry_weapons_1) && upgrade_wait(terran_infantry_weapons_2) && upgrade_wait(terran_infantry_weapons_3);
		}

		if (army_supply >= 14.0) {
			if (ground_army_supply > enemy_anti_air_army_supply + 6.0) {
				//build_n(wraith, marine_count / 8);
			}
		}

		if (st.frame >= 15 * 60 * 5) {
			if (can_expand && scv_count >= max_mineral_workers + 8 && count_production(st, cc) == 0) {
				if (players::opponent_player->race != race_terran || opponent_has_expanded || st.frame >= 15 * 60 * 10) {
					if (st.frame >= 15 * 60 * 17 || st.bases.size() < 2) {
						if (n_mineral_patches < 8 * 3 || army_supply > enemy_army_supply) build(cc);
					}
				}
			} else if (count_production(st, scv) == 0 && total_scvs_made < 6 + st.frame / 15 * 30) {
				build_n(scv, 48);
			} else if (((players::opponent_player->race == race_terran && tank_count >= 8) || st.frame >= 15 * 60 * 20) && total_scvs_made < 6 + st.frame / 15 * 10) {
				build_n(scv, 62);
			}

			if (enemy_cloaked_unit_count) build_n(science_vessel, 1);
		}

		if (count_production(st, scv) < 2) {
			if (army_supply > enemy_army_supply || army_supply >= 6.0) build_n(scv, 20);
			if (army_supply > enemy_army_supply * 1.5) build_n(scv, st.frame < 15 * 60 * 17 ? 52 : 72);
		}

		if (st.frame < 15 * 60 * 16) {
			//anti_lurker_bunkers();
		}


	}

};
