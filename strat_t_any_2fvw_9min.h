
struct strat_t_any_2fvw_9min : strat_mod {
	
	int total_scvs_made = 0;
	
	bool has_set_defensive_mines = false;

	virtual void mod_init() override {

	}

	virtual void mod_tick() override {

		auto st = buildpred::get_my_current_state();
//		if (st.frame < 15 * 60 * 9) combat::no_aggressive_groups = true;
//		else if (st.frame < 15 * 60 * 14) combat::no_aggressive_groups = true;
//		else if (st.frame >= 15 * 60 * 20) combat::no_aggressive_groups = false;
//		else combat::no_aggressive_groups = false;
		//else attack_interval = 15 * 60;
		bool prev_no_aggressive_groups = combat::no_aggressive_groups;
		combat::no_aggressive_groups = st.frame < 15 * 60 * 9 && !opponent_has_expanded;
		//if (st.frame >= 15 * 60 * 12 && st.bases.size() >= 2 && enemy_army_supply >= army_supply && st.frame < 15 * 60 * 20) combat::no_aggressive_groups = true;
		if (st.bases.size() >= 2 && enemy_army_supply >= army_supply * 0.75) {
			if (players::opponent_player->race == race_terran) {
				if (st.frame < 15 * 60 * 35 && !maxed_out_aggression) combat::no_aggressive_groups = true;
				if (tank_count >= 6 || army_supply >= 40.0) combat::no_aggressive_groups = false;
			} else {
				if (st.frame < 15 * 60 * 16) combat::no_aggressive_groups = true;
			}
		}
		//combat::aggressive_vultures = st.frame >= 15 * 60 * 9 && vulture_count > enemy_vulture_count;
		combat::aggressive_vultures = true;
		if (players::opponent_player->race == race_terran) {
			combat::aggressive_vultures = st.frame >= 15 * 60 * 9 && vulture_count > enemy_vulture_count;
			if (enemy_wraith_count && vulture_count < enemy_army_supply) combat::aggressive_vultures = false;
		}
		if (players::opponent_player->race == race_protoss) {
//			if (eval_combat(false, 0)) {
//				combat::no_aggressive_groups = prev_no_aggressive_groups;
//				attack_interval = 15 * 60 * 2;
//			} else {
//				combat::no_aggressive_groups = true;
//			}
			combat::no_aggressive_groups = !maxed_out_aggression;
			combat::aggressive_vultures = enemy_zealot_count >= enemy_dragoon_count || army_supply >= enemy_army_supply + 16.0;
		}
		//combat::aggressive_vultures = false;
		//if (players::opponent_player->race == race_protoss) combat::aggressive_vultures = st.frame < 15 * 60 * 10 && enemy_dragoon_count == 0;
		if (enemy_army_supply == enemy_zealot_count * 2.0) combat::aggressive_vultures = true;
		combat::use_control_spots = false;
		combat::use_map_split_defence = players::opponent_player->race == race_terran && (army_supply >= enemy_army_supply || tank_count >= 4);
		combat::attack_bases = true;
		
		if (enemy_ground_large_army_supply) {
			if (!has_set_defensive_mines) {
				combat::defensive_spider_mine_count = 20.0;
			}
			if (st.frame < 15 * 60 * 16) combat::no_aggressive_groups = true;
		}
		
//		if (enemy_tank_count && st.frame < 15 * 60 * 14) {
//			combat::no_aggressive_groups = true;
//			combat::aggressive_vultures = vulture_count > enemy_vulture_count + enemy_tank_count * 4;
//		}
		
		if (!my_units_of_type[unit_types::factory].empty() && my_workers.size() >= 16 && my_units_of_type[unit_types::factory].size() < 16) {
			resource_gathering::max_gas = 200.0;
		}
		
		total_scvs_made = my_units_of_type[unit_types::scv].size() + game->self()->deadUnitCount(unit_types::scv->game_unit_type);
		
		if (st.bases.size() >= 4) enable_auto_upgrades = true;
		
		base_harass_defence::defend_enabled = st.bases.size() >= 3;
		
		scouting::comsat_supply = 72.0;

	}

	virtual void mod_build(buildpred::state& st) override {

		using namespace unit_types;
		using namespace upgrade_types;

		st.dont_build_refineries = count_units_plus_production(st, assimilator) == 1 && st.frame < 15 * 60 * 8;

		if (st.frame < 15 * 60 * 8 && my_units_of_type[unit_types::factory].empty()) {
			build_n(marine, 3);
			build_n(factory, 1);
			build_n(scv, players::opponent_player->race == race_terran ? 16 : 14);
			build_n(marine, 1);
			build_n(barracks, 1) && build_n(refinery, 1);
			build_n(supply_depot, 1);
			build_n(scv, 11);
			if (players::opponent_player->race == race_zerg || players::opponent_player->random) {
				build_n(barracks, 1) && build_n(marine, 3 + enemy_attacking_army_supply / 2.0);
				build_n(scv, 8);
			}
			return;
		}
//		if (players::opponent_player->race != race_terran && my_units_of_type[vulture].empty()) {
//			build_n(marine, 3);
//			build_n(scv, 20);
//			build_n(vulture, 1);
//			return;
//		}
		
		if (st.frame < 15 * 60 * 10) {
			
			if (players::opponent_player->race == race_zerg) build_n(armory, 1);
			build_n(factory, 3);
			
			//build(wraith);
			build_n(factory, 2);
			build(vulture);
			
			if (players::opponent_player->race == race_protoss && machine_shop_count && vulture_count > enemy_zealot_count) {
				build(marine);
				build(siege_tank_tank_mode);
			}
			
			if (players::opponent_player->race == race_protoss) {
				build_n(siege_tank_tank_mode, vulture_count / 2 + enemy_dragoon_count);
			}
			
			if (enemy_tank_count + enemy_goliath_count) {
				build_n(siege_tank_tank_mode, enemy_tank_count + enemy_goliath_count);
			}
			
			//if (players::opponent_player->race == race_protoss || enemy_tank_count) {
			if (enemy_tank_count) {
				if (vulture_count >= enemy_zealot_count || enemy_tank_count + enemy_dragoon_count) {
					upgrade(spider_mines) && upgrade(ion_thrusters);
				}
			}
			
			if (players::opponent_player->race != race_zerg) {
				if (vulture_count >= 2 && army_supply >= enemy_army_supply + 2.0 && machine_shop_count == 0)  {
					build(machine_shop);
				}
				if (vulture_count >= 4) upgrade(spider_mines);
			} else {
				if (vulture_count >= 6) upgrade_wait(spider_mines) && upgrade_wait(ion_thrusters);
			}
			
			if (players::opponent_player->race != race_zerg) {
				if (st.bases.size() == 1 && (opponent_has_expanded || enemy_static_defence_count)) {
					build_n(scv, 34);
					build_n(cc, 2);
				}
			}
			
			if (enemy_army_supply == enemy_zealot_count * 2.0) {
				if (vulture_count < 8) build(vulture);
				else upgrade(spider_mines);
			}
			
			build_n(scv, 20);
			
			return;
		}
		
//		if (st.frame >= 15 * 60 * 10) {
//			maxprod(vulture);
////			if (goliath_count < tank_count / 3 + enemy_air_army_supply / 2) {
////				build(goliath);
////			}
//			build(siege_tank_tank_mode);
//		}
//		if (st.frame < 15 * 60 * 15) {
//			//build_n(barracks, 2);
//			//build_n(factory, 3);
//			maxprod(vulture);
//			if (enemy_air_army_supply) build_n(armory, 1);
//			if (army_supply > enemy_army_supply + enemy_attacking_army_supply) build_n(scv, 34);
//			else build_n(scv, 22);
//			build_n(vulture, 3);
//			//build_n(bunker, 1);
//			build_n(cc, 2);
//			build_n(vulture, 1);
//			build_n(factory, 1);
//			build_n(barracks, 1);
//			//build_n(marine, 3);
//			return;
//		}
		
//		if (enemy_anti_air_army_supply < wraith_count) {
//			if (count_units_plus_production(st, starport) && enemy_anti_air_army_supply < std::max(wraith_count * 2, std::min((int)army_supply, 6))) {
//				build(wraith);
//			}
//		} else {
//			if (tank_count < enemy_tank_count * 2 + enemy_dragoon_count + enemy_lurker_count) {
//				build(siege_tank_tank_mode);
//			}
//		}
		
		if (vulture_count < 28) {
			build(vulture);
		} else build(goliath);
		
		if (tank_count < enemy_tank_count * 2 + enemy_dragoon_count + enemy_lurker_count || army_supply >= 20.0) {
			//if (machine_shop_count < std::max(count_units_plus_production(st, factory), 6)) build(machine_shop);
//			if (machine_shop_count < std::max(count_units_plus_production(st, factory), 6)) {
//				if (count_production(st, siege_tank_tank_mode) >= machine_shop_count) build(machine_shop);
//			}
			if (machine_shop_count < count_units(st, factory)) build(machine_shop);
			build(siege_tank_tank_mode);
			if (tank_count > enemy_dragoon_count && goliath_count < tank_count / 5) build(goliath);
			if (players::opponent_player->race != race_terran) {
				if (vulture_count < tank_count * 2) build(vulture);
			}
		}
		
		if (st.minerals >= 400) {
			if (vulture_count < 18) {
				maxprod(vulture);
			} else maxprod(goliath);
			build_n(armory, 1);
		}
		
		if (goliath_count + valkyrie_count < enemy_air_army_supply / 2 - enemy_wraith_count) {
			build(goliath);
		}
		
		if (vulture_count >= 4) {
			if (enemy_tank_count + enemy_dragoon_count) {
				upgrade(spider_mines) && upgrade(ion_thrusters);
			} else {
				upgrade(ion_thrusters) && upgrade(spider_mines);
			}
		}
		if (tank_count >= 1) {
			upgrade(siege_mode);
		}
		
		if (st.bases.size() >= 3 && count_production(st, missile_turret) < 2) build_n(missile_turret, st.bases.size() * 2);
		if (enemy_air_army_supply > marine_count + goliath_count * 2 || (players::opponent_player->race == race_zerg && army_supply >= enemy_ground_army_supply + 16.0)) {
			build(goliath);
		}
		
		if (st.bases.size() == 1 && army_supply >= enemy_army_supply + 4.0 && (opponent_has_expanded || enemy_static_defence_count)) {
			build_n(scv, 34);
			build_n(cc, 2);
		}
		
		if (st.bases.size() >= 2 && players::opponent_player->race == race_terran) {
			if (st.bases.size() >= 3 && st.frame >= 15 * 60 * 25) {
				upgrade_wait(terran_vehicle_weapons_1) && upgrade_wait(terran_vehicle_plating_2) && upgrade_wait(terran_vehicle_plating_3);
				upgrade_wait(terran_vehicle_plating_1) && upgrade_wait(terran_vehicle_weapons_2) && upgrade_wait(terran_vehicle_weapons_3);
				build_n(armory, 2);
			} else {
				upgrade_wait(terran_vehicle_weapons_1) && upgrade_wait(terran_vehicle_plating_2) && upgrade_wait(terran_vehicle_plating_3);
			}
		}
		
		if (enemy_wraith_count >= 4) {
			build_n(valkyrie, enemy_wraith_count / 2);
		}
		if (players::opponent_player->race == race_terran) {
			if (tank_count >= 10) {
				build_n(valkyrie, std::max(4, enemy_wraith_count));
			}
		}
		
		if (st.frame >= 15 * 60 * 20 && tank_count >= 9) {
			build_n(science_vessel, (int)(army_supply / 30.0));
		}
		
		if (st.frame >= 15 * 60 * 5) {
			if (can_expand && scv_count >= max_mineral_workers + 8 && count_production(st, cc) == 0) {
				if (st.bases.size() > 1 || opponent_has_expanded || army_supply >= std::min(enemy_army_supply + 4.0, 20.0) || st.frame >= 15 * 60 * 14) {
					if (st.bases.size() != 2 || scv_count >= 45 || army_supply >= enemy_army_supply + 20.0) {
						build(cc);
					}
				}
			} else if (count_production(st, scv) == 0 && total_scvs_made < 6 + st.frame / 15 * 30) {
				build_n(scv, 72);
			}
			if (st.bases.size() >= 2 && army_supply >= enemy_army_supply + 4.0) {
				build_n(scv, 45);
			}
			
			if (enemy_cloaked_unit_count) build_n(science_vessel, 1);
		}
		
		if (army_supply > enemy_army_supply || army_supply >= 6.0) build_n(scv, 20);
		
		if (goliath_count >= 2) upgrade(charon_boosters);
		
		if (st.used_supply[race_terran] >= 186 && wraith_count < 8) build(wraith);
	}

};
