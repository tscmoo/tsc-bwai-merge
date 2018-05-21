

struct strat_z_10hatch_ling : strat_z_base {


	virtual void init() override {

		sleep_time = 15;

		scouting::scout_supply = 10;

		default_worker_defence = false;

	}
	bool fight_ok = true;
	bool defence_fight_ok = true;
	virtual bool tick() override {
		if (opening_state == 0) {
			if (my_resource_depots.size() != 1 || my_workers.size() > 5) opening_state = -1;
			else {
				build::add_build_total(0.0, unit_types::drone, 9);
				++opening_state;
			}
		} else if (opening_state == 1) {
			bo_gas_trick();
		} else if (opening_state == 2) {
			build::add_build_task(0.0, unit_types::hatchery);
			build::add_build_task(1.0, unit_types::spawning_pool);
			build::add_build_task(2.0, unit_types::drone);
			build::add_build_total(3.0, unit_types::overlord, 2);
			build::add_build_task(4.0, unit_types::zergling);
			build::add_build_task(4.0, unit_types::zergling);
			build::add_build_task(4.0, unit_types::zergling);
			build::add_build_task(4.0, unit_types::zergling);
			++opening_state;
		} else if (opening_state != -1) {
			if (bo_all_done()) {
				opening_state = -1;
			}
		}
		
		if (current_frame < 15 * 60 * 10) {
			bool is_near_workers = false;
			
			for (unit* u : visible_enemy_units) {
				for (unit* w : my_workers) {
					if (diag_distance(u->pos - w->pos) <= 32 * 4)  {
						is_near_workers = true;
						break;
					}
				}
				if (is_near_workers) break;
			}
			
			if (is_defending && being_rushed && is_near_workers) {
				combat::combat_mult_override_until = current_frame + 30;
				combat::combat_mult_override = 0.25;
			}
		}
		
		fight_ok = eval_combat(false, 0);
		defence_fight_ok = eval_combat(true, 1);

		bool anything_to_attack = false;
		for (unit*e : enemy_units) {
			if (e->gone) continue;
			if (e->is_flying) continue;
			if (e->type->is_worker) continue;
			anything_to_attack = true;
			break;
		}
		if (anything_to_attack) rm_all_scouts();
		else {
			if (!my_units_of_type[unit_types::zergling].empty()) {
				for (unit*u : my_units_of_type[unit_types::zergling]) {
					if (!scouting::is_scout(u)) scouting::add_scout(u);
				}
			} else {
				if (scouting::all_scouts.empty() && !my_units_of_type[unit_types::spawning_pool].empty()) {
					unit*u = get_best_score(my_workers, [&](unit*u) {
						if (u->controller->action != unit_controller::action_gather) return std::numeric_limits<double>::infinity();
						return 0.0;
					}, std::numeric_limits<double>::infinity());
					if (u) scouting::add_scout(u);
				}
			}
		}

		if (enemy_proxy_building_count && army_supply == 0.0) max_bases = 1;

		resource_gathering::max_gas = 150.0;
		
		if (drone_count < 11 && zergling_count < 10) {
			resource_gathering::max_gas = 100.0;
			auto st = buildpred::get_my_current_state();
			if (buildpred::has_or_is_upgrading(st, upgrade_types::metabolic_boost)) resource_gathering::max_gas = 1.0;
		}
// 		combat::no_aggressive_groups = false;
// 		if (enemy_zergling_count || enemy_zealot_count) {
// 			combat::no_aggressive_groups = true;
// 			if ((int)my_units_of_type[unit_types::zergling].size() > enemy_army_supply * 2 + 4) combat::no_aggressive_groups = false;
// 		}
		combat::no_aggressive_groups = army_supply < enemy_army_supply + 2.0;
		combat::no_scout_around = true;

		if (!my_completed_units_of_type[unit_types::lair].empty() && !my_completed_units_of_type[unit_types::hydralisk_den].empty()) {
			get_upgrades::set_upgrade_value(upgrade_types::lurker_aspect, -1.0);
			resource_gathering::max_gas = 200.0;
		}

		return current_used_total_supply >= 40;
	}
	virtual bool build(buildpred::state&st) override {
		using namespace buildpred;
		if (opening_state != -1) return false;

		auto army = this->army;

		st.auto_build_hatcheries = st.minerals >= 300;
		st.dont_build_refineries = true;

		army = [army](state&st) {
			return nodelay(st, unit_types::zergling, army);
		};

		bool make_hydras = (zergling_count >= 12 || hydralisk_count >= 6) && (zergling_count >= 24 - hydralisk_count * 2 || hydralisk_count < enemy_ground_large_army_supply + enemy_air_army_supply + enemy_vulture_count);
		if (players::opponent_player->race == race_zerg && enemy_spire_count == 0 && army_supply < enemy_army_supply + 16.0) make_hydras = false;
		//if (enemy_dragoon_count * 2.0 >= enemy_army_supply * 0.5 || (players::opponent_player->race == race_protoss && army_supply > enemy_army_supply + 4.0)) make_hydras = true;
		//if (players::opponent_player->race == race_protoss && zergling_count >= 18) make_hydras = true;
		if (enemy_zealot_count * 2.0 + enemy_zergling_count * 0.5 >= enemy_army_supply * 0.75 && drone_count >= 12) {
			//if (zergling_count < -18 + hydralisk_count * 8) make_hydras = false;

			if ((army_supply > enemy_army_supply && army_supply >= 12.0) || army_supply >= 18.0) {
				if (count_units_plus_production(st, unit_types::lair) == 0) {
					army = [army](state&st) {
						return nodelay(st, unit_types::lair, army);
					};
				}	
			}
			if (players::my_player->has_upgrade(upgrade_types::lurker_aspect)) {
				make_hydras = false;
				if (hydralisk_count < 2) {
					army = [army](state&st) {
						return nodelay(st, unit_types::hydralisk, army);
					};
				}
				army = [army](state&st) {
					return nodelay(st, unit_types::lurker, army);
				};
			} else {
				if ((army_supply > enemy_army_supply && army_supply >= 12.0) || army_supply >= 18.0) {
					if (count_units_plus_production(st, unit_types::hydralisk_den) == 0) {
						army = [army](state&st) {
							return nodelay(st, unit_types::hydralisk_den, army);
						};
					}
				}
			}
		}
		if (make_hydras) {
			army = [army](state&st) {
				return nodelay(st, unit_types::hydralisk, army);
			};
		}

		//if (army_supply < enemy_army_supply || enemy_static_defence_count >= army_supply / 8.0) {
		if (enemy_static_defence_count > army_supply / 8.0 && drone_count < 9 + enemy_static_defence_count * 6) {
			if (army_supply > enemy_attacking_army_supply + 8.0) {
				army = [army](state&st) {
					return nodelay(st, unit_types::drone, army);
				};
			}
		}
		
		if (enemy_zealot_count >= 6 && !fight_ok) {
			if (!defence_fight_ok) {
				if (sunken_count < 4) {
					army = [army](state&st) {
						return nodelay(st, unit_types::sunken_colony, army);
					};
				}
			} else {
				army = [army](state&st) {
					return nodelay(st, unit_types::drone, army);
				};
			}
		}
		if (enemy_marine_count + enemy_attacking_worker_count / 2 >= 8 && !fight_ok) {
			if (sunken_count == 0) {
				army = [army](state&st) {
					return nodelay(st, unit_types::sunken_colony, army);
				};
			}
		}
// 		if (enemy_zealot_count >= 8 && zergling_count >= 24) {
// 			army = [army](state&st) {
// 				return nodelay(st, unit_types::drone, army);
// 			};
// 			if (drone_count >= 18) {
// 				if (!defence_fight_ok) {
// 					if (sunken_count < 5) {
// 						army = [army](state&st) {
// 							return nodelay(st, unit_types::sunken_colony, army);
// 						};
// 					}
// 					if (is_defending) {
// 						army = [army](state&st) {
// 							return nodelay(st, unit_types::zergling, army);
// 						};
// 						if (!st.units[unit_types::hydralisk_den].empty() && make_hydras) {
// 							army = [army](state&st) {
// 								return nodelay(st, unit_types::hydralisk, army);
// 							};
// 						}
// 					}
// 				}
// 			}
// 		}

		//if (players::opponent_player->race != race_zerg || drone_count < 7 + army_supply / 5.0) {
		if (drone_count < 7 + army_supply / 5.0 && (st.frame >= 15 * 60 * 7 || !being_rushed)) {
			if (drone_count < 9 + army_supply / 5.0 || (army_supply >= 12.0 && count_production(st, unit_types::drone) == 0) && drone_count < 14) {
				army = [army](state&st) {
					return nodelay(st, unit_types::drone, army);
				};
			}
		}
		if (drone_count >= 10) {
			if (count_units_plus_production(st, unit_types::extractor) == 0) {
				army = [army](state&st) {
					return nodelay(st, unit_types::extractor, army);
				};
			}
		}
		if (hydralisk_count < enemy_air_army_supply) {
			army = [army](state&st) {
				return nodelay(st, unit_types::hydralisk, army);
			};
		}

		return army(st);
	}

};

