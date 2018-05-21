
struct strat_t_any_5rax_wraiths : strat_mod {
	
	int total_scvs_made = 0;

	bool has_built_wraiths = false;

	virtual void mod_init() override {
		combat::defensive_spider_mine_count = 40.0;
		use_opening_state = true;
	}

	xy find_proxy_pos(double desired_range = 32 * 15) {
			while (square_pathing::get_pathing_map(unit_types::scv).path_nodes.empty()) multitasking::sleep(1);
			update_possible_start_locations();

			a_vector<double> cost_map(grid::build_grid_width*grid::build_grid_height);
			auto spread = [&]() {

	// 			double desired_range = units::get_unit_stats(unit_types::scv, players::my_player)->sight_range * 2;
	// 			desired_range = 32 * 15;

				auto spread_from_to = [&](xy from, xy to) {
					a_vector<double> this_cost_map(grid::build_grid_width*grid::build_grid_height);

					tsc::dynamic_bitset visited(grid::build_grid_width*grid::build_grid_height);
					struct node_t {
						grid::build_square*sq;
						double distance;
						double start_distance;
					};
					a_deque<node_t> open;
					auto add_open = [&](xy pos, double start_dist) {
						open.push_back({ &grid::get_build_square(pos), 0.0, start_dist });
						visited.set(grid::build_square_index(pos));
					};
					auto&map = square_pathing::get_pathing_map(unit_types::scv);
					while (map.path_nodes.empty()) multitasking::sleep(1);
					auto path = square_pathing::find_path(map, from, to);
					double path_dist = 0.0;
					xy lp;
					double total_path_dist = square_pathing::get_distance(map, from, to);
					double best_path_dist = total_path_dist*0.5;
					double max_path_dist_diff = std::max(total_path_dist - best_path_dist, best_path_dist);
					add_open(from, 0.0);
					for (auto*n : path) {
						if (lp != xy()) path_dist += diag_distance(n->pos - lp);
						lp = n->pos;
						double d = path_dist;
						add_open(n->pos, d);
					}
					while (!open.empty()) {
						node_t curnode = open.front();
						open.pop_front();
						double offset = desired_range - curnode.distance;
						double variation = (32 * 4)*(32 * 4);
						if (offset < 0) variation = (32 * 6)*(32 * 6);
	// 					double variation = desired_range / 3.75 * (desired_range / 3.75);
	// 					if (offset < 0) variation = desired_range / 2.5 * (desired_range / 2.5);
						double v = exp(-offset*offset / variation);
						if (offset > 0) v -= 1;
						//else v -= 0.25;
						//if (v > 0) v *= 1.0 - (std::abs(curnode.start_distance - best_path_dist) / max_path_dist_diff);
						double&ov = this_cost_map[grid::build_square_index(*curnode.sq)];
						ov = v;
						for (int i = 0; i < 4; ++i) {
							auto*n_sq = curnode.sq->get_neighbor(i);
							if (!n_sq) continue;
							if (!n_sq->entirely_walkable) continue;
							size_t index = grid::build_square_index(*n_sq);
							if (visited.test(index)) continue;
							visited.set(index);
							open.push_back({ n_sq, curnode.distance + 32.0 });
						}
					}
					for (size_t i = 0; i < cost_map.size(); ++i) {
						cost_map[i] += this_cost_map[i];
						//if (cost_map[i] == 0) cost_map[i] = this_cost_map[i];
						//else cost_map[i] = std::min(cost_map[i], this_cost_map[i]);
					}
				};
				for (auto&p : possible_start_locations) {
					for (auto&p2 : start_locations) {
						if (p == p2) continue;
						spread_from_to(p, p2);
						multitasking::yield_point();
					}
					spread_from_to(my_start_location, p);
				}
				// 		for (auto&p : start_locations) spread_from(p, 0.0);
				// 		add_open(my_start_location, 0.0);
			};
			spread();

			double best_score;
			xy best_pos;
			for (size_t i = 0; i < cost_map.size(); ++i) {
				xy pos(i % (size_t)grid::build_grid_width * 32, i / (size_t)grid::build_grid_width * 32);
				if (pos.x == 0) multitasking::yield_point();
				double v = cost_map[i];
				if (v == 0) continue;
				double d = get_best_score_value(possible_start_locations, [&](xy p) {
					return diag_distance(pos - p);
				});
				if (d < 32 * 30) continue;
				unit_type*ut = unit_types::barracks;
				//if (!build::build_is_valid(grid::get_build_square(pos), ut)) continue;
				if (!build::build_full_check(grid::get_build_square(pos), ut, true)) continue;
				v = 0.0;
				for (int y = 0; y < ut->tile_height; ++y) {
					for (int x = 0; x < ut->tile_width; ++x) {
						v += cost_map[grid::build_square_index(pos + xy(x * 32, y * 32))];
					}
				}
				double distance_sum = 0.0;
				for (auto&p : possible_start_locations) {
					double d = unit_pathing_distance(unit_types::scv, pos, p);
					distance_sum += d*d;
				}
				distance_sum = std::sqrt(distance_sum);
				//distance_sum -= unit_pathing_distance(unit_types::scv, pos, my_start_location) * 2;
				//double distance_from_home = unit_pathing_distance(unit_types::scv, pos, my_start_location);
				distance_sum = distance_sum / (32 * 2);
				//double score = std::sqrt(v*v - distance_sum*distance_sum);
				double score = v - distance_sum;
				//log("%d %d :: v %g d %g sum %g score %g\n", pos.x, pos.y, v, d, distance_sum, score);
				if (best_pos == xy() || score > best_score) {
					best_score = score;
					best_pos = pos;
				}
			}
			//log("best score is %g\n", best_score);
			return best_pos;
		}

	refcounted_ptr<build::build_task> bt_rax[1];
	int last_find_proxy_pos = 0;
	std::array<unit*, 1> proxy_builder{};
	std::array<unit*, 1> proxy_rax{};

	bool stopped_scouting = false;

	virtual void mod_tick() override {
		if (opening_state == 0) {
			if (my_resource_depots.size() != 1 || my_workers.size() != 4) opening_state = -1;
			else {
				build::add_build_total(0.0, unit_types::scv, 5);
				bt_rax[0] = build::add_build_task(1.0, unit_types::barracks);
				build::add_build_total(2.0, unit_types::scv, 7);
				build::add_build_total(3.0, unit_types::supply_depot, 1);
				++opening_state;
			}
		} else if (opening_state != -1) {
			if (bo_all_done()) {
				opening_state = -1;
			}
		}


		bool failed = false;
		bool refind_proxy = false;
		if (last_find_proxy_pos == 0 || current_frame - last_find_proxy_pos >= 15 * 10) {
			last_find_proxy_pos = current_frame;
			refind_proxy = true;
		}
		for (size_t i = 0; i != 1; ++i) {
			if (!proxy_rax[i] || !proxy_rax[i]->is_completed) {
				if (proxy_builder[i] && proxy_builder[i]->dead) failed = true;
				if (proxy_rax[i] && proxy_rax[i]->dead) failed = true;
			}
			if (bt_rax[i]) {
				if (!bt_rax[i]->builder || bt_rax[i]->builder->controller->action != unit_controller::action_build) {
					bt_rax[i]->send_builder_immediately = true;
				}
				if (refind_proxy) {
					build::unset_build_pos(bt_rax[i]);
					build::set_build_pos(bt_rax[i], find_proxy_pos(32 * 20));
				}
				if (bt_rax[i]->builder) proxy_builder[i] = bt_rax[i]->builder;
				if (bt_rax[i]->built_unit) {
					proxy_rax[i] = bt_rax[i]->built_unit;
					bt_rax[i] = nullptr;
				}
			}
			if (proxy_rax[i] && proxy_rax[i]->is_completed) {
				if (proxy_builder[i]) {
					unit* u = proxy_builder[i];
					if (!scouting::is_scout(u)) scouting::add_scout(u);
				}
			}
		}

		auto st = buildpred::get_my_current_state();
		//combat::no_aggressive_groups = st.frame < 15 * 60 * 9 || (st.frame > 15 * 60 * 11 && st.frame < 15 * 60 * 22);
		combat::no_aggressive_groups = st.frame < 15 * 60 * 22;
		if (players::opponent_player->race == race_zerg) {
			combat::no_aggressive_groups = st.frame < 15 * 60 * 16;
		}
		if (wraith_count >= 2) {
			has_built_wraiths = true;
		}
		//if (has_built_wraiths) {
		if (true) {
			combat::no_aggressive_groups = false;
		}
		combat::aggressive_vultures = players::opponent_player->race == race_zerg;
		if (players::opponent_player->race == race_zerg) {
			if (vulture_count > enemy_dragoon_count && tank_count < 6) combat::aggressive_vultures = true;
		}
		combat::use_control_spots = false;
		combat::use_map_split_defence = st.frame >= 15 * 60 * 14 && players::opponent_player->race == race_terran;
		//combat::use_map_split_defence = players::opponent_player->race == race_terran;
		combat::attack_bases = true;

		combat::use_decisive_engagements = players::opponent_player->race == race_terran;
		
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

		if (my_workers.size() >= 15) resource_gathering::max_gas = 1000.0;

		combat::no_aggressive_groups = false;
		combat::attack_bases = true;

		combat::use_decisive_engagements = st.frame >= 15 * 60 * 15;

		scouting::no_proxy_scout = true;

		scouting::scout_supply = 20.0;

		if (current_frame >= 15 * 60 * 10) {
			if (!stopped_scouting) {
				stopped_scouting = true;
				rm_all_scouts();
			}
		} else {
			bool anything_to_attack = false;
			for (unit*e : enemy_units) {
				if (e->gone) continue;
				if (e->is_flying) continue;
				if (e->type->is_worker) continue;
				if (e->type->is_non_usable) continue;
				anything_to_attack = true;
				break;
			}
			if (anything_to_attack) {
				rm_all_scouts();
			} else {
				for (unit* u : my_units_of_type[unit_types::marine]) {
					if (!scouting::is_scout(u)) scouting::add_scout(u);
				}
			}
		}

	}

	virtual void mod_build(buildpred::state& st) override {

		using namespace unit_types;
		using namespace upgrade_types;

		st.dont_build_refineries = count_units_plus_production(st, assimilator) == 1 && st.frame < 15 * 60 * 8;

		if (st.frame < 15 * 60 * 10 && my_units_of_type[unit_types::factory].empty()) {
			build_n(factory, 1);
			build_n(scv, 15);
			build_n(refinery, 1);
			build_n(scv, 12);
			build(marine);
			return;
		}

		if (st.frame < 15 * 60 * 15) {

			if (players::opponent_player->race == race_zerg) {
				build(marine);
			}

			build_n(cc, 2);
			build(scv);

			build(vulture);

			build_n(scv, 30);

			build_n(starport, wraith_count ? 2 : 1);
			build(wraith);

			if (wraith_count >= 4) {
				if (players::opponent_player->race == race_zerg && wraith_count < enemy_air_army_supply) {
					build(marine);
				} else {
					build(siege_tank_tank_mode);
					upgrade(siege_mode);
				}
			}

			upgrade(cloaking_field);
			if (players::opponent_player->race == race_zerg && wraith_count + goliath_count < enemy_air_army_supply / 2) build_n(marine, 6);
			else build_n(siege_tank_tank_mode, 1);

			if (vulture_count >= 4) {
				upgrade(spider_mines) && upgrade(ion_thrusters);
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
			
//			if (players::opponent_player->race == race_zerg) {
//				//if (st.frame < 15 * 60 * 14 || st.minerals >= 400 || marine_count < enemy_air_army_supply) {
//				if (true) {
//					if (marine_count < tank_count * 5 || marine_count < enemy_air_army_supply) {
//						//if (count_units_plus_production(st, barracks) < 4 || st.bases.size() >= 3) maxprod(marine);
//						if (count_units_plus_production(st, barracks) < 6) maxprod(marine);
//						else build(marine);
//					}
//					build_n(medic, marine_count / 3);
//					upgrade_wait(stim_packs) && upgrade_wait(u_238_shells);
//				}
//				if (valkyrie_count < enemy_mutalisk_count / 3 + tank_count / 7) build(valkyrie);
//			}
			
			if (players::opponent_player->race == race_terran) {
				if (battlecruiser_count >= 4 && enemy_anti_air_army_supply < battlecruiser_count * 4) maxprod(battlecruiser);
				else if (tank_count >= 14) {
					if (army_supply > enemy_army_supply) {
						build_n(starport, 3);
						if (control_tower_count < count_units_plus_production(st, starport)) build(control_tower);
					}
					build(battlecruiser);
				}
				if (battlecruiser_count) upgrade(yamato_gun);
				
				if (tank_count >= 10) build_n(science_vessel, tank_count / 4);
			}
		}
		
		if (machine_shop_count < std::min(count_units(st, factory), 1 + (int)st.bases.size())) build(machine_shop);
		
		if (st.frame >= 15 * 60 * 13) {
			//if (players::opponent_player->race != race_zerg) {
			if (true) {
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
			if (players::opponent_player->race == race_zerg && scv_count >= 55) {
				if (control_tower_count < count_units_plus_production(st, starport)) build(control_tower);
				build_n(starport, 2);
			}
			if (goliath_count + wraith_count + valkyrie_count * 2 < tank_count / 3 + ((enemy_air_army_supply - enemy_wraith_count - enemy_queen_count * 1.5) + 1) / 2 + enemy_stargate_count * 3 + enemy_fleet_beacon_count * 3) {
				if (goliath_count < tank_count + vulture_count / 2 || goliath_count + wraith_count + enemy_observer_count < enemy_air_army_supply / 3) {
					if (goliath_count + marine_count / 2 < enemy_air_army_supply / 2 && count_units_plus_production(st, armory) == 0) build(marine);
					build(goliath);
					if (players::opponent_player->race == race_terran) {
						//build(wraith);						//if (st.frame >= 15 * 60 * 15 && enemy_wraith_count >= 3 && valkyrie_count < std::max(enemy_wraith_count / 2, 3)) build(valkyrie);
					}
					if (players::opponent_player->race == race_zerg) {
						if (control_tower_count < count_units_plus_production(st, starport)) build(control_tower);
						if (scv_count >= 50) build_n(valkyrie, 8);
						else build_n(valkyrie, 4);
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
			bool allow_scvs = true;
			if (players::opponent_player->race == race_zerg) {
				if (scv_count >= 48 && army_supply < enemy_army_supply + 20.0) allow_scvs = false;
			}
			if (can_expand && scv_count >= max_mineral_workers + 8 && count_production(st, cc) == 0) {
				if (players::opponent_player->race != race_terran || opponent_has_expanded || st.frame >= 15 * 60 * 10) {
					if (st.frame >= 15 * 60 * 17 || st.bases.size() < 2) {
						if (n_mineral_patches < 8 * 3 || army_supply > enemy_army_supply) build(cc);
					}
				}
			} else if (count_production(st, scv) == 0 && total_scvs_made < 6 + st.frame / (15 * 20)) {
				if (allow_scvs) build_n(scv, 48);
			} else if (((players::opponent_player->race == race_terran && tank_count >= 8) || st.frame >= 15 * 60 * 20) && total_scvs_made < 6 + st.frame / (15 * 10)) {
				if (allow_scvs) build_n(scv, 62);
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
