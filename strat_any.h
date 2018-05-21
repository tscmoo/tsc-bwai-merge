
struct strat_any : strat_mod {
	
	bool very_fast_expand = false;
	bool early_tanks = false;
	
	virtual void mod_init() override {
		//combat::defensive_spider_mine_count = 20;
		
		//get_upgrades::set_upgrade_value(upgrade_types::terran_infantry_weapons_1, 10000.0);
		//get_upgrades::set_upgrade_value(upgrade_types::terran_infantry_armor_1, 10000.0);
	}
	
	virtual void mod_tick() override {
		
		//attack_interval = 15 * 60 * 4;
		
		very_fast_expand = players::opponent_player->race == race_protoss;
		
		combat::use_map_split_defence = false;
		combat::use_control_spots = false;
		combat::aggressive_vultures = vulture_count > enemy_vulture_count + 3;
		combat::aggressive_groups_done_only = false;
		combat::attack_bases = true;
		//if (players::opponent_player->race == race_terran) combat::no_aggressive_groups = enemy_attacking_army_supply < 8.0 && army_supply < 60.0;
		if (scv_count < 40 && enemy_army_supply >= army_supply - 8.0) {
			//if (players::opponent_player->race == race_terran) combat::no_aggressive_groups = true;
			//combat::aggressive_vultures = false;
		}
		if (scv_count < 60 && army_supply < std::min(enemy_army_supply + 30.0, 60.0)) {
			if (enemy_vulture_count + enemy_wraith_count >= 3 || army_supply < 30.0) {
				if (players::opponent_player->race == race_terran) {
					combat::no_aggressive_groups = true;
					combat::aggressive_vultures = false;
				}
			}
		}
		//combat::no_aggressive_groups = current_frame < 15 * 60 * 13;
		
		//combat::no_aggressive_groups = !players::my_player->has_upgrade(upgrade_types::spider_mines) && current_frame < 15 * 60 * 10;
		//combat::no_aggressive_groups = current_frame < 15 * 60 * 8;
		
		//if (players::opponent_player->race != race_terran) attack_interval = 15 * 30;
		
//		if (players::opponent_player->race != race_terran) {
//			if (army_supply < 90.0) combat::no_aggressive_groups = true;
//		}
		
		//if (current_frame < 15 * 60 * 7) combat::no_aggressive_groups = true;
		if (current_frame < 15 * 60 * 15) {
			combat::no_aggressive_groups = true;
			attack_interval = 0;
		} else attack_interval = 15 * 30;
		
		log(log_level_test, "combat::no_aggressive_groups is %d\n", combat::no_aggressive_groups);
		
		auto cur_st = buildpred::get_my_current_state();
		
//		if (my_completed_units_of_type[unit_types::siege_tank_tank_mode].size() + my_completed_units_of_type[unit_types::siege_tank_siege_mode].size() == 0) {
//			if (army_supply < 16.0 && cur_st.bases.size() == 1) {
//				combat::force_defence_is_scared_until = current_frame + 30;
//			}
//		}
		
		enable_auto_upgrades = current_frame >= 15 * 60 * 25;
		
		if (enemy_spire_count + enemy_mutalisk_count || enemy_air_army_supply >= 8.0) {
			if (my_resource_depots.size() >= 2 && scv_count >= 26) {
				combat::build_missile_turret_count = 8;
			}
		}
		if (players::opponent_player->race != race_terran && cur_st.bases.size() >= 2 && tank_count && army_supply >= enemy_army_supply + 6.0) {
			combat::build_missile_turret_count = 2;
		}
		
		combat::build_bunker_count = 0;
		if (very_fast_expand) {
			if (current_frame < 15 * 60 * 10 && cur_st.bases.size() == 2 && my_units_of_type[unit_types::marine].size() >= 2) {
				//combat::build_bunker_count = 1;
			}
//			if (current_frame < 15 * 60 * 10 && cur_st.bases.size() == 2 && !opponent_has_expanded && army_supply < enemy_army_supply + 8.0) {
//				combat::build_bunker_count = 1;
//			} else combat::build_bunker_count = 0;
		}
		
		//if (early_tanks && !my_units_of_type[unit_types::factory].empty()) resource_gathering::max_gas = 0.0;
		
//		if (early_tanks && tank_count >= 1 && natural_pos != xy() && current_frame < 15 * 60 * 10) {
//			//combat::build_bunker_count = 1;
//			combat::force_defence_is_scared_until = 0;
//			combat::my_closest_base_override_until = current_frame + 30;
//			combat::my_closest_base_override = natural_pos;
//		}
		
		//combat::no_aggressive_groups = current_frame < 15 * 60 * 7;
		
		if (players::opponent_player->race != race_terran) scouting::comsat_supply = 30.0;
		
		//resource_gathering::max_gas = 1200.0;
	}
	
	virtual void mod_build(buildpred::state& st) override {
	
		using namespace unit_types;
		using namespace upgrade_types;
		
		st.dont_build_refineries = count_units_plus_production(st, refinery) && my_completed_units_of_type[unit_types::factory].empty();
		
		auto busy_factories = [&]() {
			int r = 0;
			for (auto& v : st.units_range(unit_types::factory)) {
				if (v.busy_until >= st.frame) ++r;
			}
			return r;
		};
		
		if (st.used_supply[race_zerg] >= 194 && wraith_count < 2) {
			build(wraith);
			return;
		}
		
		if (players::opponent_player->race == race_protoss) {
			if (st.frame < 15 * 60 * 8 && st.bases.size() < 2 && st.minerals < 500) {
				build_n(cc, 2);
				build_n(bunker, 1);
				if (!opponent_has_expanded && enemy_gas_count == 0 && enemy_cybernetics_core_count == 0) {
					build_n(marine, 8);
					build_n(scv, 18);
				}
				build_n(scv, 15);
				build_n(marine, 4);
				if (enemy_attacking_army_supply + enemy_proxy_building_count) build_n(marine, 3 + (int)enemy_army_supply + enemy_proxy_building_count * 2);
				build_n(scv, 11);
				return;
			}
			
			auto bio = [&](bool use_maxprod) {
				if (use_maxprod) maxprod(marine);
				else build(marine);
				build_n(firebat, marine_count / 4);
				if (firebat_count < enemy_zealot_count - enemy_dragoon_count) build(firebat);
				build_n(medic, (marine_count + firebat_count) / 4);
				
				upgrade(u_238_shells) && upgrade(stim_packs);
				
				if (count_units_plus_production(st, barracks) >= 3) {
					upgrade_wait(terran_infantry_weapons_1) &&
						upgrade_wait(terran_infantry_armor_1) &&
						upgrade_wait(terran_infantry_weapons_2) &&
						upgrade_wait(terran_infantry_armor_2);
				}
			};
			
			auto mech = [&]() {
				maxprod(vulture);
				build(siege_tank_tank_mode);
				upgrade(spider_mines) && upgrade(ion_thrusters);
				upgrade(siege_mode);
				if (machine_shop_count < 2) build(machine_shop);
			};
			
			if (count_units_plus_production(st, barracks) < 8) {
				bio(true);
			} else if (st.frame < 15 * 60 * 15 || st.minerals >= 400) {
				mech();
				bio(false);
			} else if (st.frame < 15 * 60 * 25) {
				if (busy_factories() >= 1) {
					mech();
					bio(false);
				} else {
					bio(false);
					mech();
				}
			} else {
				mech();
				if (busy_factories() >= 4) bio(false);
			}
			
			build_n(scv, 40);
			if (st.frame >= 15 * 60 * 10) {
				if (count_production(st, scv) < (st.frame >= 15 * 60 * 15 ? 2 : 1)) build_n(scv, 72);
			}
			
			if (st.frame >= 15 * 60 * 12) {
				if (force_expand && count_production(st, cc) == 0) {
					build(cc);
				}
			}
			
			if (enemy_zealot_count + enemy_zergling_count / 2 > marine_count) {
				build_n(bunker, 2);
				if (enemy_army_supply >= army_supply + 6.0) build_n(bunker, 3);
			}
			
			if (st.frame < 15 * 60 * 7) {
				if (!opponent_has_expanded && enemy_gas_count == 0 && enemy_cybernetics_core_count == 0 && enemy_dragoon_count == 0) {
					build_n(marine, 8);
				}
			}
			
			if (st.bases.size() >= 2 && !opponent_has_expanded) {
				if (enemy_zealot_count >= 4) {
					build_n(marine, enemy_zealot_count * 2);
					if (count_units_plus_production(st, academy)) {
						build_n(firebat, 4);
					}
				}
				if (marine_count < enemy_dragoon_count * 3) {
					build(marine);
				}
			}
			
			return;
		}
		
		//if (players::opponent_player->race == race_protoss && scv_count >= 40) {
		//if (players::opponent_player->race != race_terran && st.bases.size() >= 2) {
		if (players::opponent_player->race == race_zerg || (players::opponent_player->race == race_protoss && st.bases.size() >= 2)) {
			if (my_completed_units_of_type[unit_types::barracks].empty() && st.minerals < 200) {
				build_n(scv, 12);
				build_n(marine, 2);
				build_n(barracks, 1);
				build_n(scv, 8);
				return;
			}
			//build(vulture);
			//build(siege_tank_tank_mode);
			maxprod(marine);
			build_n(medic, marine_count / 4);
			build_n(firebat, marine_count / 4);
			
			//build_n(science_vessel, (int)(army_supply / 20.0));
			if (st.frame >= 15 * 60 * 12) {
				build(science_vessel);
				if (army_supply >= 30.0 || science_vessel_count >= 2) upgrade(irradiate);
			}
			
//			build_n(starport, 2);
//			if (control_tower_count < count_units(st, starport)) build(control_tower);
			
//			build(battlecruiser);
//			upgrade(yamato_gun);
			upgrade(u_238_shells) && upgrade(stim_packs);
			
			if (players::opponent_player->race == race_protoss) build_n(scv, 40);
			else build_n(scv, 20);
			
			if (scv_count >= 13) build_n(marine, 2 + (int)(enemy_army_supply / 1.5));
			
//			if (force_expand && count_production(st, cc) == 0) {
//				if (max_mineral_workers < 30) {
//					build(cc);
//				}
//			}
			
			return;
		}
		
		if (players::opponent_player->race == race_terran) {
			
//			if (st.bases.size() < 2 && enemy_army_supply == 0.0 && enemy_proxy_building_count == 0 && enemy_attacking_worker_count < 4) {
//				build_n(cc, 2);
//				build_n(scv, 14);
//				return;
//			}
			
//			auto bio = [&](bool use_maxprod) {
//				if (use_maxprod) maxprod(marine);
//				else build(marine);
//				build_n(medic, marine_count / 4);
				
//				upgrade(u_238_shells) && upgrade(stim_packs);
//			};
			
			if (st.frame < 15 * 60 * 10) {
				if (current_used_total_supply <= 16) {
					build_n(scv, 17);
					build_n(supply_depot, 2);
					build_n(scv, 16);
					build_n(refinery, 1);
					build_n(barracks, 1);
					build_n(scv, 15);
				} else {
					build_n(armory, 1);
					build_n(factory, 2);
					//upgrade(spider_mines) && upgrade(siege_mode);
					//upgrade(siege_mode);
					
					//bio(false);
					build(vulture);
					
					if (vulture_count >= 3) build(siege_tank_tank_mode);
					upgrade(siege_mode);
					
					if (count_production(st, scv) == 0 || army_supply > enemy_army_supply) build_n(scv, 30);
					
					//build_n(bunker, 1);
					build_n(scv, 21);
					build_n(factory, 1);
					build_n(marine, 1);
					build_n(scv, 20);
				}
				
				if (enemy_army_supply > army_supply || enemy_attacking_worker_count >= marine_count + vulture_count * 3 + 2) {
					if (my_completed_units_of_type[unit_types::factory].empty()) {
						build(marine);
					} else {
						build(vulture);
					}
				}
				return;
			}
			
			if (st.used_supply[race_terran] < 160) build(vulture);
			if (tank_count + battlecruiser_count * 2 < enemy_goliath_count) {
				build(siege_tank_tank_mode);
				upgrade(siege_mode);
			} else {
				build(siege_tank_tank_mode);
				upgrade(siege_mode);
				
				if (tank_count >= 3 && vulture_count < std::min(tank_count, 6)) build(vulture);
				
				if ((scv_count >= 50 && army_supply > enemy_army_supply) || wraith_count >= 20 || battlecruiser_count >= 2) {
					if (control_tower_count < count_units(st, starport)) build(control_tower);
					maxprod(battlecruiser);
					upgrade(yamato_gun);
				}
			}
			
			if ((scv_count >= 30 || early_tanks) && machine_shop_count < count_units(st, factory)) build(machine_shop);
			
			if (st.minerals >= 500) {
				if (st.used_supply[race_terran] < 160) maxprod(vulture);
			}
			
			int cc_count = count_units_plus_production(st, unit_types::cc);
			
//			bool build_defensive_turrets = false;
//			if (players::opponent_player->race == race_zerg && (tank_count >= 2 || enemy_mutalisk_count + enemy_spire_count)) build_defensive_turrets = true;
//			if (cc_count >= 2 && army_supply >= enemy_army_supply + 20.0) build_defensive_turrets = true;
//			if (cc_count >= 2 && enemy_air_army_supply > marine_count + goliath_count * 2 + wraith_count * 2) build_defensive_turrets = true;
//			if (enemy_shuttle_count + enemy_dropship_count) build_defensive_turrets = true;
//			if (build_defensive_turrets) {	
//				int n_turrets = 2;
//				if (static_defence_pos != xy()) n_turrets += cc_count * 2 + 1;
//				if (count_units_plus_production(st, unit_types::missile_turret) < n_turrets) {
//					if (count_production(st, unit_types::missile_turret) == 0) {
//						build(missile_turret);
//					}
//				}
//			}
			
			if (army_supply >= 30.0) {
				if (science_vessel_count) {
					if (army_supply >= enemy_army_supply + 12.0) build_n(wraith, 2);
					build_n(wraith, enemy_shuttle_count);
				}
				build_n(science_vessel, (int)(army_supply / 20.0));
			}
			
			if (vulture_count >= 6) upgrade(spider_mines) && upgrade(ion_thrusters);
			
			if (busy_factories() >= 2 || marine_count < enemy_air_army_supply) {
				//bio(count_units_plus_production(st, barracks) < 4);
			}
			
			build_n(scv, 30);
			
			int scvs_inprod = army_supply >= enemy_army_supply ? 2 : 1;
			if (count_production(st, scv) < scvs_inprod) {
				if (scv_count < max_mineral_workers * 1.0) {
					build_n(scv, 74);
				} else if (can_expand && count_production(st, cc) < 1) {
					//if ((scv_count > 40 || (army_supply >= 28.0 && army_supply > enemy_army_supply)) && army_supply >= scv_count) {
					if (scv_count > 50 || (army_supply >= 28.0 && army_supply > enemy_army_supply)) {
						build_n(scv, 74);
						build(cc);
					} else build_n(scv, 74);
				} else build_n(scv, 74);
			}
			//if (count_production(st, scv) < (army_supply >= enemy_army_supply ? 2 : 1)) build_n(scv, 72);
			
			if (army_supply < 30.0 && tank_count < 2) {
				if (enemy_air_army_supply) build_n(missile_turret, st.bases.size() * 3);
				build_n(marine, 4);
			} else {
				if (tank_count > enemy_tank_count || tank_count >= goliath_count) {
					if (goliath_count < enemy_air_army_supply / 2.0) build(goliath);
				}
			}
			
			if (goliath_count >= 2) upgrade(charon_boosters);
			
			if (st.frame >= 15 * 60 * 12) {
				if (force_expand && count_production(st, cc) == 0) {
					build(cc);
				}
			}
			return;
		}
		
//		if (true) {
			
////			maxprod(vulture);
////			build_n(siege_tank_tank_mode, vulture_count / 3);
////			upgrade(spider_mines) && upgrade(ion_thrusters);
////			//if (tank_count < 2) build_n(siege_tank_tank_mode, enemy_dragoon_count);
////			if (vulture_count > enemy_zealot_count) {
////				if (machine_shop_count < 2) build(machine_shop);
////				build(siege_tank_tank_mode);
////				if (st.minerals >= 200) maxprod(vulture);
////			}
//			maxprod(marine);
//			build(siege_tank_tank_mode);
//			build_n(scv, 20);
//			//build_n(scv, 14) && build_n(factory, 2);
//			//if (tank_count) build_n(marine, 8);
//			//else build_n(marine, 3);
//			build_n(marine, 3);
//			build_n(scv, 11);
			
////			if (st.frame >= 15 * 60 * 14) {
////				build_n(goliath, (int)(enemy_air_army_supply / 2.0));
////				build_n(scv, 40);
////			}
			
//			if (force_expand && count_production(st, cc) == 0) {
//				if (st.bases.size() < 3 || army_supply >= 80.0 || max_mineral_workers < 30) {
//					build(cc);
//				}
//			}
			
//			return;
//		}
		
//		if (players::opponent_player->race == race_protoss && scv_count >= 40) {
//			build_n(missile_turret, st.bases.size() * 4);
//			maxprod(siege_tank_tank_mode);
//			if (tank_count >= 6 && goliath_count < tank_count / 5) build(goliath);
//			//if (st.minerals >= 500 || tank_count >= std::min(4, enemy_dragoon_count + 1 + vulture_count / 5 + enemy_reaver_count * 2)) {
//			if (st.minerals >= 500 || tank_count >= std::min(4, enemy_dragoon_count + 1)) {
//				maxprod(vulture);
//			}
//			if (count_production(st, scv) < 1) build_n(scv, 64);
			
//			upgrade(siege_mode) && upgrade(spider_mines);
//			if (vulture_count >= 2 || tank_count >= 6) {
//				upgrade(terran_vehicle_weapons_1) && upgrade(terran_vehicle_plating_1) && upgrade(terran_vehicle_weapons_2);
//				upgrade(spider_mines) && upgrade(ion_thrusters);
//			}
			
//			if (scv_count >= 55 || army_supply >= scv_count) {
//				if ((force_expand || scv_count >= max_mineral_workers + 12) && can_expand && count_production(st, cc) == 0) {
//					if (st.bases.size() < 3 || army_supply >= 80.0 || max_mineral_workers < 30) {
//						build(cc);
//					}
//				}
//			}
			
//			if (scv_count >= 45) {
//				if ((tank_count > enemy_dragoon_count && tank_count + vulture_count >= enemy_zealot_count) || army_supply >= 50.0) {
//					int n = enemy_shuttle_count + enemy_arbiter_count + enemy_reaver_count;
//					if (wraith_count < n) {
//						if (count_units_plus_production(st, starport) < 2) maxprod(wraith);
//						else build(wraith);
//					}
//				}
//				build_n(science_vessel, 1 + enemy_arbiter_count + enemy_dt_count / 3);
//			}
			
//			return;
//		}
		
		if (current_frame < 15*60*10 && my_completed_units_of_type[unit_types::factory].empty()) {
			if (players::opponent_player->race == race_zerg ? build_n(scv, 8) && build_n(barracks, 1) && build_n(scv, 11) : build_n(scv, 11)) {
				if (!my_units_of_type[unit_types::factory].empty()) {
					build_n(marine, 4);
					if (early_tanks) {
						build_n(scv, 18);
						build_n(siege_tank_tank_mode, 1);
					} else {
						build_n(scv, 24);
						build_n(factory, 2);
					}
				}
				build_n(marine, 2);
				build_n(factory, 1);
				build_n(scv, 15);
				build_n(marine, 1);
				build_n(refinery, 1);
				build_n(barracks, 1);
				if (enemy_zergling_count >= 4 || enemy_zealot_count) build_n(marine, 6);
				
				if (very_fast_expand) {
					if (!my_units_of_type[unit_types::factory].empty() || current_gas < 80) build_n(scv, 40);
					if (players::opponent_player->race == race_terran) {
						build_n(bunker, 1);
						build_n(cc, 2);
						build_n(scv, 14);
					} else {
						build_n(cc, 2);
						build_n(bunker, 1);
						build_n(scv, 15);
						build_n(marine, 4);
					}
				}
			}
			if (enemy_attacking_army_supply + enemy_proxy_building_count) build_n(marine, 3 + (int)enemy_army_supply + enemy_proxy_building_count * 2);
			if (!early_tanks && players::opponent_player->race != race_zerg) {
				if (st.bases.size() == 1 && can_expand && scv_count >= 14 && (opponent_has_expanded || (enemy_static_defence_count && enemy_proxy_building_count == 0))) build(cc);
			}
			return;
		}
		//if (early_tanks && current_frame < 15 * 60 * 10 && my_units_of_type[unit_types::siege_tank_tank_mode].size() + my_units_of_type[unit_types::siege_tank_siege_mode].size() < 2) {
		if (early_tanks && current_frame < 15 * 60 * 10 && st.bases.size() < 2) {
			//if (!my_units_of_type[unit_types::machine_shop].empty() && current_minerals < 400 && current_gas < 200) {
			if (!my_units_of_type[unit_types::machine_shop].empty()) {
				build_n(engineering_bay, 1);
				build_n(marine, 4 + enemy_zealot_count - enemy_dragoon_count * 2);
				build(scv);
				build_n(siege_tank_tank_mode, 1) && upgrade(siege_mode) && build_n(siege_tank_tank_mode, 10);
				if (tank_count >= 2) build_n(cc, 2);
				return;
			}
		}
		//build_n(science_vessel, 1);
		build_n(factory, 5);
		//if (army_supply > enemy_army_supply + 2.0) upgrade(terran_vehicle_weapons_1);
		if (st.used_supply[race_terran] < 160) build(vulture);
//		if (vulture_count >= 4 || scv_count >= 30) {
//			build(siege_tank_tank_mode);
//			upgrade(siege_mode);
//		}
		if (tank_count + battlecruiser_count * 2 < enemy_goliath_count) {
			build(siege_tank_tank_mode);
			upgrade(siege_mode);
		} else {
			//if (vulture_count >= wraith_count * 2) maxprod(wraith);
//			if (vulture_count >= goliath_count * 2) {
//				maxprod(goliath);
//				if (tank_count < goliath_count) {
//					build(siege_tank_tank_mode);
//					upgrade(siege_mode);
//				}
//			}
			build(siege_tank_tank_mode);
			upgrade(siege_mode);
			
			//if (goliath_count < tank_count / 3) build(goliath);
			if (tank_count >= 3 && vulture_count < std::min(tank_count, 6)) build(vulture);
			
			if ((scv_count >= 50 && army_supply > enemy_army_supply) || wraith_count >= 20 || battlecruiser_count >= 2) {
				if (control_tower_count < count_units(st, starport)) build(control_tower);
				maxprod(battlecruiser);
				upgrade(yamato_gun);
			}
		}
		
		if (army_supply < 16.0) {
			if (scv_count < 33 && count_units_plus_production(st, bunker)) build_n(marine, 3);
			
			if (current_frame < 15 * 60 * 10 && (enemy_vulture_count > std::max(vulture_count - 2, 2) || enemy_wraith_count)) {
				if (enemy_air_army_supply == 0.0) build_n(vulture, 4);
			}
			
			if (early_tanks) {
				//build_n(factory, 2);
				build_n(marine, 6);
				build(siege_tank_tank_mode);
			}
			
			if (enemy_wraith_count) build_n(goliath, 2 + enemy_wraith_count);
		}
		
		if (st.frame < 15 * 60 * 10 && early_tanks) {
			build(marine);
			build_n(siege_tank_tank_mode, 3);
			upgrade(siege_mode);
			build_n(cc, 2);
			build_n(marine, 6);
			build_n(scv, 18);
			build_n(siege_tank_tank_mode, 2);
		}
		
		if ((scv_count >= 30 || early_tanks) && machine_shop_count < count_units(st, factory)) build(machine_shop);
		
		if (st.minerals >= 500) {
			if (st.used_supply[race_terran] < 160) maxprod(vulture);
			//maxprod(siege_tank_tank_mode);
		} else if (vulture_count >= tank_count * 4) {
			//maxprod(siege_tank_tank_mode);
		}
		
		int cc_count = count_units_plus_production(st, unit_types::cc);
		
		bool build_defensive_turrets = false;
		if (players::opponent_player->race == race_zerg && (tank_count >= 2 || enemy_mutalisk_count + enemy_spire_count)) build_defensive_turrets = true;
		if (cc_count >= 2 && army_supply >= enemy_army_supply + 20.0) build_defensive_turrets = true;
		if (cc_count >= 2 && enemy_air_army_supply > marine_count + goliath_count * 2 + wraith_count * 2) build_defensive_turrets = true;
		if (enemy_shuttle_count + enemy_dropship_count) build_defensive_turrets = true;
		if (build_defensive_turrets) {
			int n_turrets = 2;
			if (static_defence_pos != xy()) n_turrets += cc_count * 2 + 1;
			if (count_units_plus_production(st, unit_types::missile_turret) < n_turrets) {
				if (count_production(st, unit_types::missile_turret) == 0) {
					build(missile_turret);
				}
			}
		}
		
		if (army_supply >= 30.0) {
			if (science_vessel_count) {
				if (army_supply >= enemy_army_supply + 12.0) build_n(wraith, 2);
				build_n(wraith, enemy_shuttle_count);
			}
			build_n(science_vessel, (int)(army_supply / 20.0));
		}
		
		if (scv_count >= 50) {
//			if (enemy_tank_count >= 6 && enemy_tank_count > enemy_goliath_count) {
//				maxprod(wraith);
//				if (wraith_count * 2 > enemy_air_army_supply || battlecruiser_count < wraith_count / 2) {
//					if (upgrade(yamato_gun)) build(battlecruiser);
//				}
//			}
		}
		
		if (scv_count >= 32) {
			//build_n(factory, 5);
		}
		
		if ((scv_count >= 30 && army_supply > enemy_army_supply) || scv_count >= 42) {
			upgrade(terran_vehicle_weapons_1) && upgrade(terran_vehicle_plating_1) && upgrade(terran_vehicle_weapons_2);
		}
		
		int scvs_inprod = 1;
		if (scv_count < max_mineral_workers - max_mineral_workers / 4) scvs_inprod = 2;
		if (!is_defending && army_supply > enemy_army_supply) scvs_inprod = 2;
		if (st.bases.size() == 2 && scv_count >= std::max(max_mineral_workers - 2, 24) && army_supply < 18.0) scvs_inprod = 0;
		if (scv_count >= 40) {
			if (army_supply < std::min((double)scv_count, enemy_army_supply / 2 + enemy_attacking_army_supply)) scvs_inprod = 0;
		} else {
			if (army_supply < enemy_attacking_army_supply && army_supply < scv_count) scvs_inprod = 0;
		}
		//scvs_inprod = 2;
		if (count_production(st, scv) < scvs_inprod) {
			if (scv_count < max_mineral_workers * 1.0) {
				build_n(scv, 74);
			} else if (can_expand && count_production(st, cc) < 1) {
				//if ((scv_count > 40 || (army_supply >= 28.0 && army_supply > enemy_army_supply)) && army_supply >= scv_count) {
				if (scv_count > 50 || (army_supply >= 28.0 && army_supply > enemy_army_supply)) {
					build_n(scv, 74);
					build(cc);
				} else build_n(scv, 74);
			} else build_n(scv, 74);
		}
		
		if (army_supply > enemy_army_supply || vulture_count >= 6) {
			if (!early_tanks || vulture_count + tank_count >= 6) upgrade(spider_mines) && upgrade(ion_thrusters);
			else if (!my_units_of_type[unit_types::siege_tank_tank_mode].empty()) upgrade(siege_mode) && upgrade(spider_mines);
		}
		if (goliath_count >= 2) upgrade(charon_boosters);
		
		if (army_supply >= 32.0 || enemy_air_army_supply) build_n(goliath, (int)(army_supply / 12.0 + enemy_air_army_supply / 1.5 - enemy_wraith_count));
		//build_n(goliath, (int)(enemy_air_army_supply / 2.0));
		
		if (st.bases.size() == 1 && can_expand && scv_count >= 14 && (opponent_has_expanded || (enemy_static_defence_count && enemy_proxy_building_count == 0))) build(cc);
		
		if (enemy_cloaked_unit_count + enemy_lurker_count && army_supply > enemy_army_supply) build_n(comsat_station, 1);
		
		if (force_expand && count_production(st, cc) == 0) {
			//if (st.bases.size() >= 3 || scv_count >= max_mineral_workers + 8) {
			if (scv_count >= max_mineral_workers + 8 && (st.bases.size() <= 1 ||  army_supply >= scv_count)) {
				build(cc);
			}
		}
		
		if (st.frame < 15 * 60 * 10 && early_tanks) {
			build_n(machine_shop, 1) && build_n(siege_tank_tank_mode, 2);
		}
		
	}
	
};
