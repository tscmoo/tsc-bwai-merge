
struct strat_t_any_slow_mech : strat_mod {
	
	int total_scvs_made = 0;

	virtual void mod_init() override {
		combat::defensive_spider_mine_count = 40.0;
	}

	virtual void mod_tick() override {

		auto st = buildpred::get_my_current_state();
		//combat::no_aggressive_groups = st.frame < 15 * 60 * 9 || (st.frame > 15 * 60 * 11 && st.frame < 15 * 60 * 22);
		combat::no_aggressive_groups = st.frame < 15 * 60 * 22;
		if (players::opponent_player->race == race_zerg) {
			combat::no_aggressive_groups = st.frame < 15 * 60 * 16;
		}
		combat::aggressive_vultures = players::opponent_player->race == race_zerg;
		if (players::opponent_player->race == race_zerg) {
			if (vulture_count > enemy_dragoon_count && tank_count < 6) combat::aggressive_vultures = true;
		}
		combat::use_control_spots = false;
		combat::use_map_split_defence = st.frame >= 15 * 60 * 14 && players::opponent_player->race == race_terran;
		//combat::use_map_split_defence = players::opponent_player->race == race_terran;
		combat::attack_bases = true;
		
		//combat::defend_in_control_spots = st.frame < 15 * 60 * 15 && army_supply < 40.0;
		
//		combat::combat_mult_override = 0.0;
//		combat::combat_mult_override_until = current_frame + 15 * 10;
		
		total_scvs_made = my_units_of_type[unit_types::scv].size() + game->self()->deadUnitCount(unit_types::scv->game_unit_type);
		
		//if (st.bases.size() >= 4) enable_auto_upgrades = true;
		
		scouting::comsat_supply = 60.0;
		if (players::opponent_player->race == race_zerg) scouting::comsat_supply = 48.0;
		if (players::opponent_player->race == race_terran) scouting::comsat_supply = 100.0;
		if (st.frame >= 15 * 60 * 18) scouting::comsat_supply = 30.0;
		
		//combat::attack_bases = !combat::no_aggressive_groups && (st.frame < 15 * 60 * 20 || army_supply < enemy_army_supply * 2 + 64.0);
		combat::attack_bases = !combat::no_aggressive_groups && (st.frame < 15 * 60 * 20 || enemy_army_supply >= 8.0);
		
//		if (players::opponent_player->race == race_zerg && st.frame < 15 * 60 * 15) {
//			if (st.bases.size() >= 2 && tank_count >= 2) {
//				combat::build_missile_turret_count = 2;
//			}
//		}
		
		base_harass_defence::defend_enabled = st.bases.size() >= 3;
		//base_harass_defence::defend_enabled = st.bases.size() >= 3 && players::opponent_player->race == race_terran;

	}

	virtual void mod_build(buildpred::state& st) override {

		using namespace unit_types;
		using namespace upgrade_types;

		st.dont_build_refineries = count_units_plus_production(st, assimilator) == 1 && st.frame < 15 * 60 * 8;

		if (st.frame < 15 * 60 * 8 && my_units_of_type[unit_types::factory].empty()) {
			build_n(factory, 1);
			build_n(scv, 15);
			if (players::opponent_player->race != race_terran) build_n(marine, 2);
			build_n(barracks, 1) && build_n(scv, 12) && build_n(refinery, 1);
			build_n(supply_depot, 1);
			build_n(scv, players::opponent_player->race == race_zerg || players::opponent_player->random ? 10 : 11);
			return;
		}
		
		if (st.frame < 15 * 60 * 9) {
			
//			if (players::opponent_player->race == race_terran) {
//				if (enemy_wraith_count) build(valkyrie);
//				build_n(starport, 1);
//			}
			
			if (players::opponent_player->race != race_terran) {
				if (my_completed_units_of_type[unit_types::siege_tank_tank_mode].size() + my_completed_units_of_type[unit_types::siege_tank_siege_mode].size() >= 2) {
					build_n(engineering_bay, 1);
					build_n(cc, 2);
					//build_n(bunker, 1);
				}
			}
			if (players::opponent_player->race != race_terran) build(marine);
			if (players::opponent_player->race == race_terran) {
				//build_n(valkyrie, 1);
				build_n(armory, 1);
			}
			//if (players::opponent_player->race == race_terran) build_n(starport, 1);
			build(vulture);
			if (players::opponent_player->race == race_zerg) {
				build_n(academy, 1);
				build_n(factory, 1);
			} else build_n(factory, 2);
			build(scv);
			if (players::opponent_player->race != race_terran) {
				if (vulture_count == 0) build_n(marine, 6);
			}
			if (players::opponent_player->race == race_protoss || vulture_count >= 6) build(siege_tank_tank_mode);
			
			if (st.frame >= 15 * 60 * 9) build_n(scv, 24);
			
			if (enemy_attacking_army_supply - enemy_vulture_count * 3 >= army_supply) build(marine);
			
			//if (players::opponent_player->race == race_terran && tank_count) build(wraith);
			
			build_n(scv, 16);
			
			if (vulture_count < enemy_zealot_count && tank_count >= enemy_dragoon_count / 2) build(vulture);
			
			if (tank_count >= (players::opponent_player->race == race_protoss ? 2 : 1)) {
				if (players::opponent_player->race == race_terran) build_n(goliath, 2);
				upgrade(siege_mode);
			}
			
			if (opponent_has_expanded && (players::opponent_player->race != race_zerg || (army_supply > enemy_army_supply && vulture_count + tank_count >= 2))) {
				build_n(bunker, 1);
				build_n(scv, 22);
				build_n(cc, 2);
				build_n(scv, 14);
			}
			
			if (army_supply > enemy_army_supply + 4.0) build_n(scv, 40);
			
			if (players::opponent_player->race == race_protoss) {
				if (army_supply >= enemy_army_supply + 14.0 - (4 * (enemy_citadel_of_adun_count + enemy_templar_archives_count)) || enemy_cloaked_unit_count) {
					build_n(missile_turret, 1 + enemy_cloaked_unit_count / 2);
				}
			}
			
			return;
		}
		
		if (st.frame >= 15 * 60 * 10) {
			if (vulture_count < 32) {
				maxprod(vulture);
			} else {
				if (machine_shop_count < count_units(st, factory)) build(machine_shop);
				build_n(factory, 10);
			}
			build(siege_tank_tank_mode);
			
			if (tank_count >= 4 && players::opponent_player->race == race_zerg) {
//				if (army_supply >= 30.0) {
//					build_n(starport, 2);
//					if (control_tower_count < count_units_plus_production(st, starport)) build(control_tower);
//				}
				
				build_n(science_vessel, std::max(8, tank_count));
				if (science_vessel_count) upgrade(irradiate);
			}
			
			if (st.minerals >= 400 && vulture_count < 16) maxprod(vulture);
			
			if (players::opponent_player->race == race_zerg) {
				//if (st.frame < 15 * 60 * 14 || st.minerals >= 400 || marine_count < enemy_air_army_supply) {
				if (true) {
					if (marine_count < tank_count * 5 || marine_count < enemy_air_army_supply) {
						//if (count_units_plus_production(st, barracks) < 4 || st.bases.size() >= 3) maxprod(marine);
						if (count_units_plus_production(st, barracks) < 6) maxprod(marine);
						else build(marine);
					}
					build_n(medic, marine_count / 3);
					upgrade_wait(stim_packs) && upgrade_wait(u_238_shells);
				}
				if (valkyrie_count < enemy_mutalisk_count / 3 + tank_count / 7) build(valkyrie);
			}
			
			if (players::opponent_player->race == race_terran) {
				if (battlecruiser_count >= 4 && enemy_anti_air_army_supply < battlecruiser_count * 4) maxprod(battlecruiser);
				else if (tank_count >= 14) build(battlecruiser);
				if (battlecruiser_count) upgrade(yamato_gun);
				
				if (tank_count >= 10) build_n(science_vessel, tank_count / 4);
			}
		}
		
		if (machine_shop_count < std::min(count_units(st, factory), 1 + (int)st.bases.size())) build(machine_shop);
		
		if (st.frame >= 15 * 60 * 13) {
			if (players::opponent_player->race != race_zerg) {
				//upgrade_wait(terran_vehicle_weapons_1) && upgrade_wait(terran_vehicle_weapons_2) && upgrade_wait(terran_vehicle_weapons_3);
				if (st.bases.size() >= 3 && st.frame >= 15 * 60 * 25) {
					upgrade_wait(terran_vehicle_weapons_1) && upgrade_wait(terran_vehicle_plating_2) && upgrade_wait(terran_vehicle_plating_3);
					upgrade_wait(terran_vehicle_plating_1) && upgrade_wait(terran_vehicle_weapons_2) && upgrade_wait(terran_vehicle_weapons_3);
					build_n(armory, 2);
				} else {
					upgrade_wait(terran_vehicle_weapons_1) && upgrade_wait(terran_vehicle_plating_2) && upgrade_wait(terran_vehicle_plating_3);
				}
			} else {
				if (science_vessel_count && has_or_is_upgrading(st, irradiate)) {
					build_n(engineering_bay, 2);
					upgrade_wait(terran_infantry_armor_1) && upgrade_wait(terran_infantry_armor_2) && upgrade_wait(terran_infantry_armor_3);
					upgrade_wait(terran_infantry_weapons_1) && upgrade_wait(terran_infantry_weapons_2) && upgrade_wait(terran_infantry_weapons_3);
				}
			}
		}
		
		if (vulture_count >= 4) {
			upgrade_wait(spider_mines) && upgrade_wait(ion_thrusters);
		}
		if (tank_count) {
			upgrade(siege_mode);
		}
		
//		if (tank_count >= 8) {
//			build_n(science_vessel, (int)(army_supply / 20.0));
//		}
		if (st.frame >= 15 * 60 * 20 && tank_count >= 9) {
			build_n(science_vessel, (int)(army_supply / 30.0));
		}
		
		//if (marine_count >= 16) upgrade(terran_infantry_weapons_1) && upgrade(terran_infantry_weapons_2) && upgrade(terran_infantry_weapons_3);
		
		if (players::opponent_player->race != race_terran && st.frame < 15 * 60 * 15) {
			if (st.bases.size() >= 2) {
				if (tank_count >= 2) {
					build_n(missile_turret, 4);
				}
				build_n(bunker, 1);
			}
		} else {
			if (players::opponent_player->race != race_zerg) {
				if (goliath_count + wraith_count + valkyrie_count * 2 < tank_count / 3 + ((enemy_air_army_supply - enemy_wraith_count - enemy_queen_count * 1.5) + 1) / 2 + enemy_stargate_count * 3 + enemy_fleet_beacon_count * 3) {
					if (goliath_count < tank_count + vulture_count / 2 || goliath_count + wraith_count + enemy_observer_count < enemy_air_army_supply / 3) {
						if (goliath_count + marine_count / 2 < enemy_air_army_supply / 2 && count_units_plus_production(st, armory) == 0) build(marine);
						build(goliath);
						if (players::opponent_player->race == race_terran) {
							//build(wraith);
							//if (st.frame >= 15 * 60 * 15 && enemy_wraith_count >= 3 && valkyrie_count < std::max(enemy_wraith_count / 2, 3)) build(valkyrie);
						}
					}
				}
			}
		}
		
		if (enemy_wraith_count >= 1) {
			build_n(valkyrie, (enemy_wraith_count + 1) / 2);
		}
		if (players::opponent_player->race == race_terran) {
			if (tank_count >= 10) {
				build_n(valkyrie, std::max(4, enemy_wraith_count));
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
		
		if (st.frame < 15 * 60 * 12 && vulture_count < enemy_zealot_count && enemy_dragoon_count == 0) {
			build_n(marine, 4);
			build(vulture);
		}
		
		if (count_production(st, scv) < 2) {
			if (army_supply > enemy_army_supply || army_supply >= 6.0) build_n(scv, 20);
			if (army_supply > enemy_army_supply * 1.5) build_n(scv, st.frame < 15 * 60 * 17 ? 52 : 72);
		}
		
		if (goliath_count) upgrade(charon_boosters);
	}

};
