
struct strat_z_uni : strat_z_base {

	virtual void init() override {

		sleep_time = 15;

		scouting::scout_supply = 12;

		call_place_static_defence = false;
		default_upgrades = false;

	}

	xy natural_pos;
	xy natural_cc_build_pos;
	combat::choke_t natural_choke;
	int max_mineral_workers = 0;

	int gas_state = 0;
	int three_hatch_muta_state = 0;
	int test_state = 0;
	int muta_hydra_state = 0;
	int muta_guardian_state = 0;
	int lurker_state = 3;

	xy static_defence_pos;
	xy build_nydus_pos;
	bool has_nydus_without_exit = false;

	virtual bool tick() override {
		if (opening_state == 0) {
			if (my_resource_depots.size() != 1 || my_workers.size() != 4) opening_state = -1;
			else {
				build::add_build_total(0.0, unit_types::drone, 9);
				build::add_build_total(1.0, unit_types::overlord, 2);
				build::add_build_total(2.0, unit_types::drone, 12);
				build::add_build_total(3.0, unit_types::hatchery, 2);
				build::add_build_total(2.0, unit_types::drone, 14);
				build::add_build_total(4.0, unit_types::spawning_pool, 1);
				//build::add_build_total(5.0, unit_types::drone, 12);
				//build::add_build_total(6.0, unit_types::drone, 11);
				//build::add_build_total(6.5, unit_types::hatchery, 3);
				//build::add_build_total(7.0, unit_types::zergling, 1);
// 				build::add_build_task(1.0, unit_types::spawning_pool);
// 				build::add_build_task(2.0, unit_types::overlord);
				++opening_state;
			}
		} else if (opening_state != -1) {
			if (bo_all_done()) {
				opening_state = -1;
			}
		}

		max_mineral_workers = get_max_mineral_workers();

		auto cur_st = buildpred::get_my_current_state();
		size_t bases = cur_st.bases.size();

		if (current_frame < 15 * 60) {
			auto s = get_next_base();
			for (auto& v : resource_spots::spots) {
				auto& bs = grid::get_build_square(v.cc_build_pos);
				if ((bs.building && bs.building->owner == players::my_player) || bs.reserved.first) {
					if (diag_distance(bs.pos - my_start_location) >= 32 * 10) {
						s = &v;
						break;
					}
				}
			}
			if (s) {
				natural_pos = s->pos;
				natural_cc_build_pos = s->cc_build_pos;
			}
			if (natural_pos != xy()) {
				natural_choke = combat::find_choke_from_to(unit_types::siege_tank_tank_mode, natural_pos, combat::op_closest_base, false, false, false, 32.0 * 10, 1, true);
			}
		}

		//combat::no_aggressive_groups = true;
		combat::aggressive_groups_done_only = false;
		combat::aggressive_zerglings = false;

// 		if (drone_count >= 40) {
// 			combat::no_aggressive_groups = false;
// 		}

		resource_gathering::max_gas = 100.0;
		if (!my_units_of_type[unit_types::lair].empty() || current_frame >= 15 * 60 * 8) {
			resource_gathering::max_gas = 0.0;
		}
		if (current_frame >= 15 * 60 * 15) {
			resource_gathering::max_gas = 0.0;
		}
		//execute_build_auto_max_gas = true;

		if (resource_gathering::max_gas == 0.0) {
			if (gas_state == 0) {
				resource_gathering::max_gas = 450.0;
				if (current_gas >= 400) gas_state = 1;
			} else {
				resource_gathering::max_gas = 250.0;
				if (current_gas <= 250) gas_state = 0;
			}
		}

// 		if (drone_count < 24) {
// 			if (!my_units_of_type[unit_types::hive].empty()) {
// 				auto pred = [](unit* u) {
// 					return !u->is_morphing;
// 				};
// 				if (test_pred(my_units_of_type[unit_types::hive], pred)) {
// 					resource_gathering::max_gas = 1.0;
// 				}
// 			}
// 		}

		threats::eval(*this);

// 		if (hydralisk_count) min_bases = 3;
// 		max_bases = 3;

		tactics::enable_anti_push_until = current_frame + 15 * 10;

		if (!is_defending && current_frame < 15 * 60 * 8) {
			combat::aggressive_zerglings = true;
			combat::combat_mult_override_until = current_frame + 15 * 5;
			combat::combat_mult_override = 128.0;
		} else {
			if (current_frame < 15 * 60 * 10) tactics::force_attack_n(unit_types::zergling, 4);
			combat::no_scout_around = true;
		}

		if (army_supply < 4.0 || (army_supply < 8.0 && army_supply < enemy_army_supply)) {
			if (threats::being_marine_rushed && enemy_attacking_army_supply && (sunken_count == 0 || is_defending)) {
				rm_all_scouts();
				resource_gathering::max_gas = 1.0;
				//if (my_completed_units_of_type[unit_types::zergling].size() < 6) {
				if (army_supply + tactics::pulled_workers.size() < enemy_army_supply) {
					tactics::pull_workers_to_buy_time();
				} else tactics::pull_workers_to_fight();
			} else tactics::reset_workers();
		} else tactics::reset_workers();

		log(log_level_test, "army supply %g, enemy army supply %g\n", army_supply, enemy_army_supply);

		if (lurker_state) {
			if (drone_count >= 30) {
				min_bases = 3;
				if (drone_count >= max_mineral_workers) min_bases = 8;
			}
			combat::no_aggressive_groups = army_supply < 60.0;
			//combat::no_aggressive_groups = army_supply < 14.0;
			//if (bases >= 4 && army_supply < enemy_army_supply + 24.0) combat::no_aggressive_groups = true;
			//if (army_supply > enemy_army_supply && lurker_count < 4) resource_gathering::max_gas = 600.0;

			if (lurker_state == 3) {
				if (current_frame >= 15 * 60 * 15) lurker_state = 1;
			}
		}

		if (test_state) {
			if (drone_count >= 30) {
				min_bases = 4;
				if (drone_count >= max_mineral_workers) min_bases = 8;
			}
			if (is_defending || current_frame >= 15 * 60 * 15) {
				combat::no_aggressive_groups = army_supply >= 60.0;
				if (drone_count >= max_mineral_workers) combat::no_aggressive_groups = false;
			}
			if (bases >= 4 && army_supply < enemy_army_supply + 24.0) combat::no_aggressive_groups = true;
			tactics::force_attack_n(unit_types::queen, 10);

			if (zergling_count >= 30) tactics::force_attack_n(unit_types::zergling, 10);
		}

		if (three_hatch_muta_state == 1) {
			//rm_all_scouts();
			resource_gathering::max_gas = 900.0;
			//if (my_units_of_type[unit_types::spire].empty()) resource_gathering::max_gas = 150.0;
			if (!my_units_of_type[unit_types::spire].empty()) min_bases = 3;
			if (mutalisk_count >= 9) {
				//attack_interval = 15 * 30;
				combat::aggressive_mutalisks = true;
			}
			if (mutalisk_count >= 11) {
				if (drone_count >= 32) three_hatch_muta_state = 2;
				//else resource_gathering::max_gas = 1.0;
			}
		} else if (three_hatch_muta_state == 2) {
			//attack_interval = 15 * 30;
			combat::aggressive_mutalisks = true;
			//if (army_supply >= 40.0 || drone_count >= 36) three_hatch_muta_state = 0;
			//if (army_supply >= 80.0 || drone_count >= 44) three_hatch_muta_state = 0;
			//if (lurker_count >= 6) three_hatch_muta_state = 0;
			if (army_supply >= 80.0 || drone_count >= 60) three_hatch_muta_state = 0;
		}

		if (muta_hydra_state == 1) {
			if (drone_count < 30) resource_gathering::max_gas = 900.0;
			if (!my_completed_units_of_type[unit_types::spire].empty()) min_bases = 3;
			combat::aggressive_mutalisks = true;
		}

		if (muta_guardian_state == 1) {
			if (drone_count < 30) resource_gathering::max_gas = 900.0;
			if (!my_completed_units_of_type[unit_types::spire].empty()) min_bases = 3;
			//combat::aggressive_mutalisks = true;
// 			if (mutalisk_count >= 9) {
// 				muta_guardian_state = 0;
// 				test_state = 1;
// 			}
		}

		auto get_static_defence_position_next_to = [&](unit* cc, unit_type* ut) {
			a_vector<xy> resources;
			for (unit* r : resource_units) {
				if (diag_distance(r->pos - cc->pos) > 32 * 12) continue;
				resources.push_back(r->pos);
			}
			if (resources.empty()) return xy();
			int n_near = 0;
			for (auto* u : my_units_of_type[ut]) {
				if (diag_distance(u->pos - cc->pos) <= 32 * 6) ++n_near;
			}
			if (n_near >= 3) return xy();
			a_vector<xy> potential_edges;
			for (auto i = resources.begin(); i != resources.end();) {
				xy r = *i;
				bool remove = false;
				int n_in_range = 0;
				for (auto* u : my_units_of_type[ut]) {
					if (diag_distance(u->pos - r) <= 32 * 7) {
						++n_in_range;
						if (n_in_range >= 2) {
							remove = true;
							break;
						}
					}
				}
				if (remove) i = resources.erase(i);
				else {
					if (n_in_range == 0) {
						potential_edges.push_back(r);
					}
					++i;
				}
			}
			if (resources.empty()) return xy();
			xy edge_a;
			xy edge_b;
			double max_d = 0.0;
			for (xy r : potential_edges) {
				for (xy r2 : resources) {
					if (r == r2) continue;
					double d = diag_distance(r - r2);
					if (d > max_d) {
						max_d = d;
						edge_a = r;
						edge_b = r2;
					}
				}
			}

			int ut_w = ut->tile_width;
			int ut_h = ut->tile_width;
			int cc_w = cc->type->tile_width;
			int cc_h = cc->type->tile_height;
			xy cc_build_pos = cc->building->build_pos;

			xy best;
			double best_score = std::numeric_limits<double>::infinity();

			auto eval = [&](xy pos) {
				auto& bs = grid::get_build_square(pos);
				if (!build::build_is_valid(bs, ut)) {
					game->drawBoxMap(pos.x, pos.y, pos.x + ut_w * 32, pos.y + ut_h * 32, BWAPI::Colors::Red);
					return;
				}
				xy cpos = pos + xy(ut_w * 16, ut_h * 16);
				double s = 0.0;
				if (edge_a != xy()) s += diag_distance(edge_a - cpos) * 1000.0;
				if (edge_b != xy()) s += diag_distance(edge_b - cpos) * 1000.0;
				for (xy r : resources) {
					double d = diag_distance(r - cpos);
					s += d*d;
				}
				if (s < best_score) {
					best_score = s;
					best = pos;
				}
			};

			for (int y = cc_build_pos.y - ut_h * 32; y <= cc_build_pos.y + cc_h * 32; y += 32) {
				if (y == cc_build_pos.y + 32 || y == cc_build_pos.y + 64) continue;
				eval(xy(cc_build_pos.x - ut_w * 32, y));
				eval(xy(cc_build_pos.x + cc_w * 32, y));
			}
			for (int x = cc_build_pos.x - ut_w * 32; x <= cc_build_pos.x + cc_w * 32; x += 32) {
				if (x == cc_build_pos.x + 32 || x == cc_build_pos.x + 64) continue;
				eval(xy(x, cc_build_pos.y - ut_h * 32));
				eval(xy(x, cc_build_pos.y + cc_h * 32));
			}

			game->drawBoxMap(best.x, best.y, best.x + ut_w * 32, best.y + ut_h * 32, BWAPI::Colors::Green);

			return best;
		};

		static_defence_pos = xy();
		if (bases >= 3) {
			for (auto& b : build::build_tasks) {
				if (b.built_unit || b.dead) continue;
				if (b.type->unit == unit_types::creep_colony) {
					build::unset_build_pos(&b);
				}
			};
			for (unit* u : my_resource_depots) {
				xy pos = get_static_defence_position_next_to(u, unit_types::creep_colony);
				if (pos != xy()) {
					static_defence_pos = pos;
				}
			}
		}

		build_nydus_pos = xy();
		if (natural_pos != xy() && bases >= 3 && !my_completed_units_of_type[unit_types::hive].empty()) {
			for (auto& b : build::build_tasks) {
				if (b.built_unit || b.dead) continue;
				if (b.type->unit == unit_types::nydus_canal) {
					build::unset_build_pos(&b);
				}
			};
			for (auto& b : cur_st.bases) {
				if (!my_units_of_type[unit_types::nydus_canal].empty()) {
					double nearest_nydus_distance = get_best_score_value(my_units_of_type[unit_types::nydus_canal], [&](unit* u) {
						if (!u->game_unit->getNydusExit()) return diag_distance(u->pos - b.s->pos);
						unit* e = units::get_unit(u->game_unit->getNydusExit());
						if (!e) return diag_distance(u->pos - b.s->pos);
						return std::min(diag_distance(u->pos - b.s->pos), diag_distance(e->pos - b.s->pos));
					});
					if (nearest_nydus_distance <= 32 * 15) continue;
				}
				if (unit_pathing_distance(unit_types::drone, b.s->pos, natural_pos) >= 32 * 30) {
					auto pred = [&](grid::build_square&bs) {
						return build::build_full_check(bs, unit_types::nydus_canal);
					};
					auto score = [&](xy pos) {
						return 0.0;
					};

					std::array<xy, 1> starts;
					starts[0] = b.s->pos;
					xy pos = build_spot_finder::find_best(starts, 4, pred, score);

					if (pos != xy() && diag_distance(pos - b.s->pos) <= 32 * 15) {
						build_nydus_pos = pos;
						break;
					}
				}
			}
		}
		has_nydus_without_exit = false;
		for (unit* u : my_units_of_type[unit_types::nydus_canal]) {
			if (u->game_unit->getNydusExit()) {
				unit* e = units::get_unit(u->game_unit->getNydusExit());
				if (e) continue;
			}
			has_nydus_without_exit = true;
			auto pred = [&](grid::build_square&bs) {
				return build::build_full_check(bs, unit_types::nydus_canal);
			};
			auto score = [&](xy pos) {
				return 0.0;
			};

			std::array<xy, 1> starts;
			starts[0] = natural_pos;
			xy pos = build_spot_finder::find_best(starts, 4, pred, score);
			if (pos != xy()) {
				bwapi_call_build(u->game_unit, unit_types::nydus_canal->game_unit_type, BWAPI::TilePosition(pos.x / 32, pos.y / 32));
			}
		}

		return false;
	}
	virtual bool build(buildpred::state&st) override {
		using namespace buildpred;
		if (opening_state != -1) return false;

		auto army = this->army;

		st.auto_build_hatcheries = true;

		//st.dont_build_refineries = count_units_plus_production(st, unit_types::extractor) && drone_count < 18;
		st.dont_build_refineries = drone_count < 18;

		if (true) {

			auto build = [&](unit_type* ut) {
				army = [army, ut](state&st) {
					return nodelay(st, ut, army);
				};
			};
			auto upgrade = [&](upgrade_type* upg) {
				if (!has_or_is_upgrading(st, upg)) {
					army = [army, upg](state&st) {
						return nodelay(st, upg, army);
					};
					return false;
				}
				return true;
			};
			auto build_n = [&](unit_type* ut, int n) {
				if (count_units_plus_production(st, ut) < n) {
					army = [army, ut](state&st) {
						return nodelay(st, ut, army);
					};
					return false;
				}
				return true;
			};

			int desired_drone_count = 28;

			auto carapace_missile_upgrades = [&]() {
				upgrade(upgrade_types::muscular_augments) &&
					upgrade(upgrade_types::grooved_spines);

				upgrade(upgrade_types::zerg_carapace_2) &&
					upgrade(upgrade_types::zerg_missile_attacks_1) &&
					upgrade(upgrade_types::zerg_missile_attacks_2) &&
					upgrade(upgrade_types::zerg_carapace_3) &&
					upgrade(upgrade_types::zerg_missile_attacks_3);
			};

			auto overlord_upgrades = [&]() {
				upgrade(upgrade_types::pneumatized_carapace) &&
					upgrade(upgrade_types::antennae);
			};

			using namespace unit_types;
			using namespace upgrade_types;

			if (lurker_state == 3) {
				st.dont_build_refineries = drone_count < 26;
				desired_drone_count = 40;
				if (drone_count < desired_drone_count && drone_count < 66 && (current_frame < 15 * 60 * 10 || army_supply > enemy_army_supply || army_supply >= 20.0)) {
					build(drone);
				} else {
					build(zergling);
					build(hydralisk);
					build(lurker);
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
				if (drone_count >= 16) {
					if (((count_units_plus_production(st, unit_types::hive)) || build_n(lair, 1)) && drone_count >= 21) {
						upgrade(lurker_aspect);
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

				if (build_nydus_pos != xy() && !has_nydus_without_exit) {
					build_n(nydus_canal, my_units_of_type[unit_types::nydus_canal].size() + 1);
				}

				if (force_expand && count_production(st, unit_types::hatchery) == 0) {
					army = [army](state&st) {
						return nodelay(st, unit_types::hatchery, army);
					};
				}

				return army(st);
			}

			if (lurker_state == 2) {
				st.dont_build_refineries = drone_count < 26;
				desired_drone_count = 36;
				if (count_production(st, unit_types::drone) <= (drone_count >= 50 ? 1 : 0)) {
					desired_drone_count = drone_count + 1;
				}
				if (drone_count < desired_drone_count && drone_count < 66 && (current_frame < 15 * 60 * 10 || army_supply > enemy_army_supply || army_supply >= 20.0)) {
					build(drone);
				} else {
					build(zergling);
					build(hydralisk);
					build(lurker);
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
				if (drone_count >= 16) {
					if (((count_units_plus_production(st, unit_types::hive)) || build_n(lair, 1)) && drone_count >= 21) {
						upgrade(lurker_aspect) &&
							build_n(hive, 1) &&
							upgrade(metabolic_boost) &&
							build_n(extractor, 2) &&
							upgrade(consume);
					}
					if (drone_count >= 30 && army_supply >= 24 && st.frame >= 15 * 60 * 20) {
						if (build_n(ultralisk_cavern, 1) && upgrade(chitinous_plating)) {
							upgrade(anabolic_synthesis) &&
								build_n(ultralisk, 10);
						}
						upgrade(zerg_carapace_1) &&
							upgrade(zerg_carapace_2);
					}
// 					if (((count_units_plus_production(st, unit_types::hive)) || build_n(lair, 1)) && drone_count >= 21) {
// 						upgrade(metabolic_boost) &&
// 							upgrade(lurker_aspect) &&
// 							build_n(extractor, 2) &&
// 							upgrade(zerg_carapace_1);
// 						if (drone_count >= 55 || army_supply >= 55) {
// 							build_n(hive, 1) &&
// 								build_n(evolution_chamber, 2) &&
// 								upgrade(zerg_missile_attacks_1) &&
// 								upgrade(consume) &&
// 								upgrade(zerg_carapace_2) &&
// 								upgrade(zerg_missile_attacks_2) &&
// 								upgrade(zerg_carapace_3) &&
// 								upgrade(zerg_missile_attacks_3);
// 						}
// 					}
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

				if (st.frame < 15 * 60 * 20 && drone_count >= 16) {
					if (my_completed_units_of_type[unit_types::hatchery].size() + my_units_of_type[unit_types::lair].size() >= 2) {
						if (threats::nearest_attack_time <= unit_types::creep_colony->build_time + unit_types::sunken_colony->build_time + 15 * 15) {
							int n_sunkens = (int)((enemy_army_supply - army_supply) / 5) + 1;
							if (n_sunkens >= 4) n_sunkens = 4;
							build_n(sunken_colony, n_sunkens);
						}
					}
				}

				if (build_nydus_pos != xy() && !has_nydus_without_exit) {
					build_n(nydus_canal, my_units_of_type[unit_types::nydus_canal].size() + 1);
				}

				if (force_expand && count_production(st, unit_types::hatchery) == 0) {
					army = [army](state&st) {
						return nodelay(st, unit_types::hatchery, army);
					};
				}

				return army(st);
			}

			if (lurker_state == 1) {
				st.dont_build_refineries = drone_count < 26;
				desired_drone_count = 36;
				if (count_production(st, unit_types::drone) <= (drone_count >= 50 ? 1 : 0)) {
					desired_drone_count = drone_count + 1;
				}
				if (drone_count < desired_drone_count && drone_count < 66) {
					build(drone);
				} else {
					build(zergling);
					build(hydralisk);
					build(lurker);
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
				if (drone_count >= 16) {
					if (((count_units_plus_production(st, unit_types::hive)) || build_n(lair, 1)) && drone_count >= 21) {
						upgrade(metabolic_boost) &&
							upgrade(lurker_aspect) &&
							build_n(extractor, 2) &&
							upgrade(zerg_carapace_1);
						if (drone_count >= 55 || army_supply >= 55) {
							build_n(hive, 1) &&
								build_n(evolution_chamber, 2) &&
								upgrade(zerg_missile_attacks_1) &&
								upgrade(consume) &&
								upgrade(zerg_carapace_2) &&
								upgrade(zerg_missile_attacks_2) &&
								upgrade(zerg_carapace_3) &&
								upgrade(zerg_missile_attacks_3);
						}
					}
				}
				if (drone_count >= 14) {
					if (my_completed_units_of_type[unit_types::hatchery].size() + my_units_of_type[unit_types::lair].size() >= 2) {
						build_n(sunken_colony, 1);
					}
				}
				if (drone_count >= 18) {
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

				if (st.frame < 15 * 60 * 20 && drone_count >= 16) {
					if (my_completed_units_of_type[unit_types::hatchery].size() + my_units_of_type[unit_types::lair].size() >= 2) {
						if (threats::nearest_attack_time <= unit_types::creep_colony->build_time + unit_types::sunken_colony->build_time + 15 * 15) {
							int n_sunkens = (int)((enemy_army_supply - army_supply) / 5) + 1;
							if (n_sunkens >= 4) n_sunkens = 4;
							build_n(sunken_colony, n_sunkens);
						}
					}
				}

				if (build_nydus_pos != xy() && !has_nydus_without_exit) {
					build_n(nydus_canal, my_units_of_type[unit_types::nydus_canal].size() + 1);
				}

				if (force_expand && count_production(st, unit_types::hatchery) == 0) {
					army = [army](state&st) {
						return nodelay(st, unit_types::hatchery, army);
					};
				}

				return army(st);
			}

			if (test_state == 1) {
				st.dont_build_refineries = drone_count < 26;
				desired_drone_count = 36;
// 				if (current_frame >= 15 * 60 * 15) {
// 					desired_drone_count = 40;
// 				}
// 				if (count_production(st, unit_types::drone) <= (army_supply >= 60.0 ? 1 : 0)) {
// 					if (my_completed_units_of_type[unit_types::drone].size() == my_units_of_type[unit_types::drone].size()) {
// 						double desired_army_supply = drone_count*drone_count * 0.015 + drone_count * 0.8 - 16;
// 						//if (army_supply >= desired_army_supply) {
// // 						if (current_frame >= 15 * 60 * 15 && drone_count < 44) {
// // 							desired_drone_count = drone_count + 1;
// // 						}
// // 						if (current_frame >= 15 * 60 * 20 && drone_count < 55) {
// // 							desired_drone_count = drone_count + 1;
// // 						}
// 						desired_drone_count = drone_count + 1;
// 					}
// 				}
				if (count_production(st, unit_types::drone) <= (drone_count >= 50 ? 1 : 0)) {
					desired_drone_count = drone_count + 1;
				}
				if (drone_count < desired_drone_count && drone_count < 66) {
					build(drone);
				} else {
					build(zergling);
					if (defiler_count < zergling_count / 10) build(defiler);
					upgrade(consume) && upgrade(plague);

// 					if (drone_count >= 45 || st.gas >= 500) {
// 						build_n(lurker, 10);
// 						if (army_supply >= enemy_army_supply) {
// 							build_n(mutalisk, 20);
// 						}
// 						build_n(mutalisk, 20);
//  						upgrade(ensnare) && build_n(queen, 1);
// 					}

// 					if (drone_count >= 45) {
// 						upgrade(muscular_augments) &&
// 							upgrade(grooved_spines) &&
// 							build_n(hydralisk, 20);
// 						upgrade(zerg_missile_attacks_1) &&
// 							upgrade(zerg_missile_attacks_2) &&
// 							upgrade(zerg_missile_attacks_3);
// 					}
// 					if (drone_count >= 35) upgrade(zerg_missile_attacks_1);
					if (drone_count >= 40 && st.frame >= 15 * 60 * 20) {
						if (build_n(ultralisk_cavern, 1) && upgrade(chitinous_plating) && upgrade(anabolic_synthesis)) {
							build_n(ultralisk, 10);
							build_n(defiler, ultralisk_count / 3);
						}
					}
					build_n(defiler, 4);
					build_n(zergling, defiler_count * 4);

					if (hydralisk_count < enemy_bc_count * 2 + enemy_wraith_count) {
						build(hydralisk);
					}
					if (enemy_bc_count >= 4) {
						if (hydralisk_count < enemy_bc_count * 4) {
							build(hydralisk);
						}
					}

// 					if (scourge_count / 2 < enemy_air_army_supply) {
// 						build(scourge);
// 					}
					if (army_supply >= 80.0) {
						build_n(mutalisk, 4);
						build_n(queen, 4);
					}

// 					build_n(hydralisk, 4) &&
// 						(hatch_count >= 6 || build_n(hatchery, 6));
// 
// // 					upgrade(consume) &&
// // 						build_n(defiler, 4) &&
// // 						build_n(zergling, 60) &&
// // 						build_n(defiler, 6) &&
// // 						build_n(zergling, 200);
// 					build_n(zergling, 200);
// 					build_n(defiler, 6);
// 					build_n(zergling, 60);
// 					if (has_upgrade(st, grooved_spines)) build_n(hydralisk, 50);
// 					build_n(defiler, 4);
// 					if (st.gas >= 400) {
// 						build_n(queen, 10);
// 						build_n(defiler, 12);
// 					}
// 					upgrade(consume) && (st.gas >= 200 && upgrade(plague));
// 
// 					if (drone_count >= 40) {
// 						if (build_n(ultralisk_cavern, 1) && upgrade(chitinous_plating) && upgrade(anabolic_synthesis)) {
// 							build_n(ultralisk, 10);
// 							build_n(defiler, ultralisk_count / 3);
// 						}
// 						build_n(queen, 4);
// 						upgrade(plague);
// 					}
// 
// // 					if (hydralisk_count < enemy_air_army_supply) {
// // 						build(hydralisk);
// // 					}
// 					build_n(queen, 1) &&
// 						upgrade(ensnare);
				}
				if (drone_count >= 16) {
					if (((count_units_plus_production(st, unit_types::hive)) || build_n(lair, 1)) && drone_count >= 21) {
						upgrade(metabolic_boost) &&
							upgrade(zerg_carapace_1) &&
							build_n(extractor, 2) &&
							build_n(hive, 1) &&
							build_n(evolution_chamber, 2) &&
							upgrade(zerg_melee_attacks_1) &&
							upgrade(adrenal_glands) &&
							upgrade(zerg_carapace_2) &&
							upgrade(zerg_melee_attacks_2) &&
							upgrade(zerg_carapace_3) &&
							upgrade(zerg_melee_attacks_3);
// 						upgrade(metabolic_boost) &&
// 							upgrade(zerg_carapace_1) &&
// 							build_n(extractor, 2) &&
// 							build_n(evolution_chamber, 2) &&
// 							upgrade(zerg_missile_attacks_1) &&
// 							build_n(hive, 1) &&
// 							upgrade(adrenal_glands) &&
// 							upgrade(zerg_carapace_2) &&
// 							upgrade(zerg_missile_attacks_2) &&
// 							upgrade(zerg_carapace_3) &&
// 							upgrade(zerg_missile_attacks_3) &&
// 							upgrade(muscular_augments) &&
// 							upgrade(grooved_spines);
						if (enemy_vulture_count >= 4) {
							build_n(mutalisk, enemy_vulture_count / 2 + enemy_tank_count);
							if (defiler_count == 0) build_n(hydralisk, enemy_goliath_count);
						} else {
							build_n(hydralisk, enemy_vulture_count);
						}
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

				if (st.frame < 15 * 60 * 20 && drone_count >= 16) {
					if (my_completed_units_of_type[unit_types::hatchery].size() + my_units_of_type[unit_types::lair].size() >= 2) {
						if (threats::nearest_attack_time <= unit_types::creep_colony->build_time + unit_types::sunken_colony->build_time + 15 * 15) {
							int n_sunkens = (int)((enemy_army_supply - army_supply) / 5) + 1;
							if (n_sunkens >= 2) n_sunkens = 2;
							build_n(sunken_colony, n_sunkens);
						}
					}
				}
// 				if (static_defence_pos != xy() && drone_count >= 50) {
// 					if (sunken_count < (int)st.bases.size() * 2) {
// 						army = [army](state&st) {
// 							return nodelay(st, unit_types::sunken_colony, army);
// 						};
// 					}
// 				}

				if (build_nydus_pos != xy() && !has_nydus_without_exit) {
					build_n(nydus_canal, my_units_of_type[unit_types::nydus_canal].size() + 1);
				}

				if (force_expand && count_production(st, unit_types::hatchery) == 0) {
					army = [army](state&st) {
						return nodelay(st, unit_types::hatchery, army);
					};
				}

				return army(st);
			}

			if (muta_guardian_state) {
				st.dont_build_refineries = drone_count < 26;
				desired_drone_count = 26;
				if (mutalisk_count >= 9) desired_drone_count = 33;
				if (drone_count >= 30) {
					if (count_production(st, unit_types::drone) <= (drone_count >= 50 ? 1 : 0)) {
						desired_drone_count = drone_count + 1;
					}
				}
				if (drone_count < desired_drone_count && drone_count < 66) {
					build(drone);
				} else {
					if (drone_count < 30) {
						build_n(zergling, 10);

						build_n(mutalisk, 9) &&
							build_n(greater_spire, 1) &&
							build_n(mutalisk, 11) &&
							build_n(spire, 1) &&
							build_n(drone, 40);

						build_n(queens_nest, 1) &&
							build_n(hive, 1);
					} else {
						build(zergling);
						build_n(mutalisk, 20);
						//build_n(guardian, 20);
						upgrade(ensnare) && build_n(queen, 2);
						if (enemy_air_army_supply >= 6) {
							build_n(devourer, (int)(enemy_air_army_supply / 12 + 1));
							//build_n(mutalisk, (int)(enemy_air_army_supply / 4));
						}
					}
				}
				if (mutalisk_count >= 9) {
					build_n(hive, 1);
// 					build_n(hive, 1) &&
// 						build_n(greater_spire, 1) &&
// 						build_n(spire, 1) &&
// 						upgrade(zerg_flyer_carapace_1) &&
// 						upgrade(zerg_flyer_attacks_1) &&
// 						upgrade(zerg_flyer_carapace_2) &&
// 						upgrade(zerg_flyer_attacks_2) &&
// 						upgrade(zerg_flyer_carapace_3) &&
// 						upgrade(zerg_flyer_attacks_3);
				}
				if (drone_count >= 16) {
					if (((count_units_plus_production(st, unit_types::hive)) || build_n(lair, 1)) && drone_count >= 21) {
						upgrade(metabolic_boost) &&
							build_n(extractor, 2) &&
							build_n(spire, 1);
					}
				}
				if (hatch_count < 3) {
					if (drone_count >= 15) {
						build(hatchery);
					}
				} else {
					build_n(extractor, 1) &&
						build_n(zergling, 2);
				}

				if (build_nydus_pos != xy() && !has_nydus_without_exit) {
					build_n(nydus_canal, my_units_of_type[unit_types::nydus_canal].size() + 1);
				}

				if (st.frame < 15 * 60 * 20 && drone_count >= 16) {
					if (my_completed_units_of_type[unit_types::hatchery].size() + my_units_of_type[unit_types::lair].size() >= 2) {
						if (threats::nearest_attack_time <= unit_types::creep_colony->build_time + unit_types::sunken_colony->build_time + 15 * 15) {
							int n_sunkens = (int)((enemy_army_supply - army_supply) / 5) + 1;
							if (n_sunkens >= 2) n_sunkens = 2;
							build_n(sunken_colony, n_sunkens);
						}
					}
				}

				if (force_expand && count_production(st, unit_types::hatchery) == 0) {
					army = [army](state&st) {
						return nodelay(st, unit_types::hatchery, army);
					};
				}

				return army(st);
			}

			if (muta_hydra_state) {
				st.dont_build_refineries = true;
				desired_drone_count = 26;
				if (drone_count >= 30) {
					if (count_production(st, unit_types::drone) <= (drone_count >= 50 ? 1 : 0)) {
						desired_drone_count = drone_count + 1;
					}
				}
				if (drone_count < desired_drone_count && drone_count < 66) {
					build(drone);
				} else {
					if (drone_count < 30) {
						build_n(zergling, 10);

						build_n(mutalisk, 9) &&
							build_n(hydralisk_den, 1) &&
							build_n(mutalisk, 11) &&
							build_n(drone, 40);
					} else {
						//build_n(sunken_colony, st.bases.size() * 3);
						build(zergling);
						build(mutalisk);
						build_n(hydralisk, enemy_goliath_count * 2);

						upgrade(muscular_augments) &&
							upgrade(grooved_spines);
					}
				}
				if (drone_count >= 16) {
					if (build_n(lair, 1) && drone_count >= 21) {
						upgrade(metabolic_boost) &&
							build_n(extractor, 2) &&
							build_n(spire, 1);
					}
				}
				if (hatch_count < 3) {
					if (drone_count >= 15) {
						build(hatchery);
					}
				} else {
					build_n(extractor, 1) &&
						build_n(zergling, 2);
				}

				if (build_nydus_pos != xy() && !has_nydus_without_exit) {
					build_n(nydus_canal, my_units_of_type[unit_types::nydus_canal].size() + 1);
				}

				if (st.frame < 15 * 60 * 20 && drone_count >= 16) {
					if (my_completed_units_of_type[unit_types::hatchery].size() + my_units_of_type[unit_types::lair].size() >= 2) {
						if (threats::nearest_attack_time <= unit_types::creep_colony->build_time + unit_types::sunken_colony->build_time + 15 * 15) {
							int n_sunkens = (int)((enemy_army_supply - army_supply) / 5) + 1;
							if (n_sunkens >= 2) n_sunkens = 2;
							build_n(sunken_colony, n_sunkens);
						}
					}
				}

				if (force_expand && count_production(st, unit_types::hatchery) == 0) {
					army = [army](state&st) {
						return nodelay(st, unit_types::hatchery, army);
					};
				}

				return army(st);
			}

			if (three_hatch_muta_state) {
				if (three_hatch_muta_state == 1) {
					st.dont_build_refineries = true;
					desired_drone_count = 26;
					if (drone_count < desired_drone_count) {
						build(drone);
					} else {
						build_n(zergling, 10);

						build_n(mutalisk, 9) &&
							build_n(hive, 1) &&
							build_n(hydralisk_den, 1) &&
							build_n(mutalisk, 11) &&
// 							(build_n(drone, 40),
// 								upgrade(zerg_flyer_carapace_1));
							(build_n(drone, 40),
								upgrade(lurker_aspect));
					}
					if (drone_count >= 16) {
						if (build_n(lair, 1) && drone_count >= 21) {
							upgrade(metabolic_boost) &&
								build_n(extractor, 2) &&
								build_n(spire, 1);
						}
					}
					if (hatch_count < 3) {
						if (drone_count >= 15) {
							build(hatchery);
						}
					} else {
						build_n(extractor, 1) &&
							build_n(zergling, 2);
					}

					if (st.frame < 15 * 60 * 20 && drone_count >= 16) {
						if (my_completed_units_of_type[unit_types::hatchery].size() + my_units_of_type[unit_types::lair].size() >= 2) {
							if (threats::nearest_attack_time <= unit_types::creep_colony->build_time + unit_types::sunken_colony->build_time + 15 * 15) {
								int n_sunkens = (int)((enemy_army_supply - army_supply) / 5) + 1;
								if (n_sunkens >= 2) n_sunkens = 2;
								build_n(sunken_colony, n_sunkens);
							}
						}
					}

					if (force_expand && count_production(st, unit_types::hatchery) == 0) {
						army = [army](state&st) {
							return nodelay(st, unit_types::hatchery, army);
						};
					}

					return army(st);
				} else if (three_hatch_muta_state == 2) {

					desired_drone_count = 30;
					if (defiler_count >= 3) {
						desired_drone_count = 36;
					}
					if (drone_count >= desired_drone_count && (army_supply >= 60.0 || defiler_count >= 3)) {
						//if (army_supply > enemy_army_supply && count_production(st, unit_types::drone) <= (army_supply >= 30.0 ? 1 : 0)) {
						if (count_production(st, unit_types::drone) <= (army_supply >= 60.0 ? 1 : 0)) {
							desired_drone_count = drone_count + 1;
						}
					}

					if (drone_count < desired_drone_count) {
						if (drone_count >= 45 && hatch_count < 8) {
							build(hatchery);
						} else build(drone);
					} else {
						build(zergling);
						build(hydralisk);
// 						if (mutalisk_count < 18) build(mutalisk);
// 						else build(lurker);
						build(lurker);
						if (count_units(st, defiler_mound)) build_n(defiler, lurker_count / 3 + 1);

						if (army_supply >= 40.0 && drone_count >= 40) {
							build_n(evolution_chamber, 2);
							carapace_missile_upgrades();
						}
					}

					upgrade(lurker_aspect) &&
						upgrade(zerg_carapace_1);
						//upgrade(zerg_flyer_carapace_1);
						//build_n(evolution_chamber, 2) &&
						//upgrade(zerg_missile_attacks_1) &&

					build_n(defiler_mound, 1) &&
						upgrade(consume);

					//build_n(hive, 1);

					if (defiler_count >= 3 && lurker_count >= 6) {
						overlord_upgrades();
					}

					if (force_expand && count_production(st, unit_types::hatchery) == 0) {
						army = [army](state&st) {
							return nodelay(st, unit_types::hatchery, army);
						};
					}

					return army(st);
				}
			}

			desired_drone_count = 28;
			if (drone_count >= desired_drone_count && army_supply >= 22.0) {
				//if (army_supply > enemy_army_supply && count_production(st, unit_types::drone) <= (army_supply >= 30.0 ? 1 : 0)) {
				if (count_production(st, unit_types::drone) <= (army_supply >= 30.0 ? 1 : 0)) {
					desired_drone_count = drone_count + 1;
				}
			}
			if (desired_drone_count > 70) desired_drone_count = 70;

			double desired_army_supply = drone_count*drone_count * 0.015 + drone_count * 0.8 - 16;

			if (drone_count < desired_drone_count && army_supply >= desired_army_supply) {
				build(drone);
			} else {

				build(zergling);
				build(hydralisk);

				build(hydralisk);
				build(lurker);
				if (count_units(st, defiler_mound)) build_n(defiler, lurker_count / 3 + 1);
// 				if (lurker_count < hydralisk_count / 2 || lurker_count * 4 < enemy_ground_small_army_supply) {
// 					build(lurker);
// 				}
// 				if (defiler_count < army_supply / 20.0) {
// 					build(defiler);
// 				}
// 				if (lurker_count >= 6) {
// 					build_n(unit_types::mutalisk, 12);
// 				}
				if (army_supply >= 40.0 && queen_count < enemy_tank_count && count_production(st, unit_types::queen) < 2) {
					build(queen);
				}

				carapace_missile_upgrades();
				upgrade(zerg_flyer_attacks_1);
			}

			if (drone_count >= 50) {
				build_n(evolution_chamber, 2);
				overlord_upgrades();
				upgrade(spawn_broodling);
			}

			upgrade(adrenal_glands);

			return army(st);
		}
		/*
		int desired_drone_count = 25;

		if (true) {

			desired_drone_count = 28;
// 			if (army_supply >= 20.0 && army_supply > enemy_army_supply + 8.0) {
// 				desired_drone_count = 36;
// 			}
			if (drone_count >= desired_drone_count && army_supply >= 22.0) {
				if (army_supply > enemy_army_supply && count_production(st, unit_types::drone) <= (army_supply >= 30.0 ? 1 : 0)) {
					desired_drone_count = drone_count + 1;
				}
			}

			if (drone_count < desired_drone_count) {
				army = [army](state&st) {
					return nodelay(st, unit_types::drone, army);
				};
			}

			bool go_ultras = false;
			if (has_or_is_upgrading(st, upgrade_types::chitinous_plating) && has_or_is_upgrading(st, upgrade_types::anabolic_synthesis)) {
				go_ultras = true;
			}

			if (drone_count >= desired_drone_count) {
// 				army = [army](state&st) {
// 					return nodelay(st, unit_types::zergling, army);
// 				};
// 				if (defiler_count < army_supply / 14.0) {
// 					army = [army](state&st) {
// 						return nodelay(st, unit_types::defiler, army);
// 					};
// 				}
				army = [army](state&st) {
					return nodelay(st, unit_types::zergling, army);
				};
				if (go_ultras) {
					army = [army](state&st) {
						return nodelay(st, unit_types::ultralisk, army);
					};
				} else {
					army = [army](state&st) {
						return nodelay(st, unit_types::hydralisk, army);
					};
					if (enemy_ground_large_army_supply <= enemy_army_supply / 2) {
						army = [army](state&st) {
							return nodelay(st, unit_types::lurker, army);
						};
					}
				}
				if (defiler_count < lurker_count / 3 + zergling_count / 6 + ultralisk_count / 2) {
					army = [army](state&st) {
						return nodelay(st, unit_types::defiler, army);
					};
				}
			}

			if (drone_count >= 36 && (army_supply >= enemy_army_supply || !is_defending)) {
				if (!has_or_is_upgrading(st, upgrade_types::chitinous_plating)) {
					army = [army](state&st) {
						return nodelay(st, upgrade_types::chitinous_plating, army);
					};
				} else if (!has_or_is_upgrading(st, upgrade_types::anabolic_synthesis)) {
					army = [army](state&st) {
						return nodelay(st, upgrade_types::anabolic_synthesis, army);
					};
				}
			}

			if (count_units(st, unit_types::lair) + count_units(st, unit_types::hive)) {
				if (!has_or_is_upgrading(st, upgrade_types::adrenal_glands)) {
					army = [army](state&st) {
						return nodelay(st, upgrade_types::adrenal_glands, army);
					};
				}
				if (!has_or_is_upgrading(st, upgrade_types::zerg_carapace_1)) {
					army = [army](state&st) {
						return nodelay(st, upgrade_types::zerg_carapace_1, army);
					};
				} else if (!has_or_is_upgrading(st, upgrade_types::zerg_carapace_2)) {
					army = [army](state&st) {
						return nodelay(st, upgrade_types::zerg_carapace_2, army);
					};
				}
				if (!has_or_is_upgrading(st, upgrade_types::metabolic_boost)) {
					army = [army](state&st) {
						return nodelay(st, upgrade_types::metabolic_boost, army);
					};
				}
				if (!has_or_is_upgrading(st, upgrade_types::lurker_aspect)) {
					army = [army](state&st) {
						return nodelay(st, upgrade_types::lurker_aspect, army);
					};
				}
			}

			if (count_units(st, unit_types::defiler_mound)) {
				if (!has_or_is_upgrading(st, upgrade_types::consume)) {
					army = [army](state&st) {
						return nodelay(st, upgrade_types::consume, army);
					};
				}
			}

// 			if (hatch_count >= 2 && drone_count >= 11 && count_units_plus_production(st, unit_types::extractor) < 1) {
// 				army = [army](state&st) {
// 					return nodelay(st, unit_types::extractor, army);
// 				};
// 			}

			if (hatch_count >= 2 && drone_count >= 16 && count_units_plus_production(st, unit_types::extractor) < 2) {
				army = [army](state&st) {
					return nodelay(st, unit_types::extractor, army);
				};
			}

			if (drone_count >= 14) {
				if (count_units(st, unit_types::lair) == 0) {
					army = [army](state&st) {
						return nodelay(st, unit_types::lair, army);
					};
				}
			}

			if (drone_count >= 16) {
				if (count_units_plus_production(st, unit_types::defiler_mound) == 0) {
					army = [army](state&st) {
						return nodelay(st, unit_types::defiler_mound, army);
					};
				}
			}
// 			if (drone_count >= desired_drone_count && defiler_count < 3) {
// 				army = [army](state&st) {
// 					return nodelay(st, unit_types::defiler, army);
// 				};
// 			}

			if (drone_count >= 18) {
				if (my_completed_units_of_type[unit_types::hatchery].size() + my_units_of_type[unit_types::lair].size() >= 2 && sunken_count == 0) {
					army = [army](state&st) {
						return nodelay(st, unit_types::sunken_colony, army);
					};
				}
			}
			if (zergling_count < 2) {
				army = [army](state&st) {
					return nodelay(st, unit_types::zergling, army);
				};
			}

// 			if (threats::being_marine_rushed && (army_supply < 4.0 || army_supply < enemy_army_supply + enemy_attacking_worker_count)) {
// 				army = [army](state&st) {
// 					return nodelay(st, unit_types::zergling, army);
// 				};
// 
// 				if (completed_hatch_count >= 2 && drone_count >= 11) {
// 					if (enemy_army_supply >= 8.0 && sunken_count < enemy_army_supply / 4) {
// 						army = [army](state&st) {
// 							return nodelay(st, unit_types::sunken_colony, army);
// 						};
// 					}
// 				}
// 			}
// 
// 			if (st.frame < 15 * 60 * 20 && completed_hatch_count >= 2 && drone_count >= 16) {
// 				if (threats::nearest_attack_time <= unit_types::creep_colony->build_time + unit_types::sunken_colony->build_time + 15 * 15) {
// 					int n_sunkens = (int)((enemy_army_supply - army_supply) / 5) + 1;
// 					if (n_sunkens >= 2) n_sunkens = 2;
// 					if (sunken_count < n_sunkens) {
// 						army = [army](state&st) {
// 							return nodelay(st, unit_types::sunken_colony, army);
// 						};
// 					}
// 				}
// 			}

			return army(st);

		}

// 		if (true) {
// 
// 			desired_drone_count = 28;
// 
// 			if (drone_count < desired_drone_count) {
// 				army = [army](state&st) {
// 					return nodelay(st, unit_types::drone, army);
// 				};
// 			}
// 
// 			if (drone_count >= desired_drone_count) {
// 				army = [army](state&st) {
// 					return nodelay(st, unit_types::zergling, army);
// 				};
// 				if (defiler_count < army_supply / 14.0) {
// 					army = [army](state&st) {
// 						return nodelay(st, unit_types::defiler, army);
// 					};
// 				}
// 			}
// 
// 			if (count_units(st, unit_types::lair) + count_units(st, unit_types::hive)) {
// 				if (!has_or_is_upgrading(st, upgrade_types::metabolic_boost)) {
// 					army = [army](state&st) {
// 						return nodelay(st, upgrade_types::metabolic_boost, army);
// 					};
// 				}
// 
// 				if (!has_or_is_upgrading(st, upgrade_types::zerg_melee_attacks_1)) {
// 					army = [army](state&st) {
// 						return nodelay(st, upgrade_types::zerg_melee_attacks_1, army);
// 					};
// 				}
// 			}
// 
// 			if (count_units(st, unit_types::hive)) {
// 				if (!has_or_is_upgrading(st, upgrade_types::adrenal_glands)) {
// 					army = [army](state&st) {
// 						return nodelay(st, upgrade_types::adrenal_glands, army);
// 					};
// 				}
// 			}
// 			if (count_units(st, unit_types::defiler_mound)) {
// 				if (!has_or_is_upgrading(st, upgrade_types::consume)) {
// 					army = [army](state&st) {
// 						return nodelay(st, upgrade_types::consume, army);
// 					};
// 				}
// 			}
// 
// 			if (defiler_count == 0) {
// 				army = [army](state&st) {
// 					return nodelay(st, unit_types::defiler, army);
// 				};
// 			}
// 
// 			return army(st);
// 
// 		}

		if (drone_count < desired_drone_count) {
			army = [army](state&st) {
				return nodelay(st, unit_types::drone, army);
			};
		}

		if (drone_count >= 11) {
			if (hatch_count == 1) {
				army = [army](state&st) {
					return nodelay(st, unit_types::hatchery, army);
				};
			} else {
				if (count_units_plus_production(st, unit_types::extractor) == 0) {
					army = [army](state&st) {
						return nodelay(st, unit_types::extractor, army);
					};
				}
			}
		}

		if (drone_count >= 14) {
			if (!has_or_is_upgrading(st, upgrade_types::metabolic_boost)) {
				army = [army](state&st) {
					return nodelay(st, upgrade_types::metabolic_boost, army);
				};
			}
			if (count_units_plus_production(st, unit_types::lair) + count_units_plus_production(st, unit_types::hive) == 0) {
				army = [army](state&st) {
					return nodelay(st, unit_types::lair, army);
				};
			}
		}

		if (count_units(st, unit_types::lair) + count_units(st, unit_types::hive)) {
			if (!has_or_is_upgrading(st, upgrade_types::lurker_aspect)) {
				army = [army](state&st) {
					return nodelay(st, upgrade_types::lurker_aspect, army);
				};
			}
// 			else if (!has_or_is_upgrading(st, upgrade_types::muscular_augments)) {
// 				army = [army](state&st) {
// 					return nodelay(st, upgrade_types::muscular_augments, army);
// 				};
// 			} else if (!has_or_is_upgrading(st, upgrade_types::grooved_spines)) {
// 				army = [army](state&st) {
// 					return nodelay(st, upgrade_types::grooved_spines, army);
// 				};
// 			}
		}

		double desired_army_supply = drone_count*drone_count * 0.015 + drone_count * 0.8 - 16;

// 		if (army_supply < desired_army_supply) {
// 			unit_type* main_unit = unit_types::hydralisk;
// 
// 			if (enemy_ground_large_army_supply <= enemy_army_supply / 2) {
// 				main_unit = unit_types::lurker;
// 			}
// 
// 			army = [army, main_unit](state&st) {
// 				return nodelay(st, main_unit, army);
// 			};
// 		}

		//if (drone_count >= max_mineral_workers + 4 && army_supply < desired_army_supply) {
		//if (drone_count >= desired_drone_count && army_supply < desired_army_supply) {
		//if (drone_count >= desired_drone_count) {
		if (drone_count >= desired_drone_count) {
			army = [army](state&st) {
				return nodelay(st, unit_types::zergling, army);
			};

			unit_type* main_unit = unit_types::hydralisk;

// 			if (enemy_ground_large_army_supply <= enemy_army_supply / 2) {
// 				main_unit = unit_types::lurker;
// 			}
			if (main_unit == unit_types::lurker) {
				army = [army](state&st) {
					return nodelay(st, unit_types::hydralisk, army);
				};
			}
			army = [army, main_unit](state&st) {
				return nodelay(st, main_unit, army);
			};

			if (hydralisk_count < enemy_vulture_count || hydralisk_count < enemy_ground_large_army_supply) {
				army = [army](state&st) {
					return nodelay(st, unit_types::hydralisk, army);
				};
			}
			if (has_or_is_upgrading(st, upgrade_types::lurker_aspect)) {
				if (lurker_count * 2 < enemy_ground_small_army_supply) {
					army = [army](state&st) {
						return nodelay(st, unit_types::lurker, army);
					};
				}
			}
			if (count_units(st, unit_types::defiler_mound)) {
				if (defiler_count < army_supply / 20.0) {
					army = [army](state&st) {
						return nodelay(st, unit_types::defiler, army);
					};
				}
			}
		}

		if (count_units(st, unit_types::hydralisk_den)) {
			if (drone_count >= 24 && hydralisk_count < 6) {
				army = [army](state&st) {
					return nodelay(st, unit_types::hydralisk, army);
				};
			}
			if (has_or_is_upgrading(st, upgrade_types::lurker_aspect)) {
				if (lurker_count < 6) {
					army = [army](state&st) {
						return nodelay(st, unit_types::lurker, army);
					};
				}
			}
		}

		if (count_units(st, unit_types::hive)) {
			if (count_units_plus_production(st, unit_types::defiler_mound) == 0) {
				army = [army](state&st) {
					return nodelay(st, unit_types::defiler_mound, army);
				};
			}
			if (count_units(st, unit_types::defiler_mound) && !has_or_is_upgrading(st, upgrade_types::consume)) {
				army = [army](state&st) {
					return nodelay(st, upgrade_types::consume, army);
				};
			}
		}

		if (drone_count >= 20) {
			if (count_units_plus_production(st, unit_types::hydralisk_den) == 0) {
				army = [army](state&st) {
					return nodelay(st, unit_types::hydralisk_den, army);
				};
			}
			if (count_units_plus_production(st, unit_types::hive) == 0) {
				army = [army](state&st) {
					return nodelay(st, unit_types::hive, army);
				};
			}
		}

		if (threats::being_marine_rushed && (army_supply < 4.0 || army_supply < enemy_army_supply + enemy_attacking_worker_count)) {
			army = [army](state&st) {
				return nodelay(st, unit_types::zergling, army);
			};

			if (completed_hatch_count >= 2 && drone_count >= 11) {
				if (enemy_army_supply >= 8.0 && sunken_count < enemy_army_supply / 4) {
					army = [army](state&st) {
						return nodelay(st, unit_types::sunken_colony, army);
					};
				}
			}
		}

		if (st.frame < 15 * 60 * 20 && completed_hatch_count >= 2 && drone_count >= 16) {
			if (threats::nearest_attack_time <= unit_types::creep_colony->build_time + unit_types::sunken_colony->build_time + 15 * 15) {
				int n_sunkens = (int)((enemy_army_supply - army_supply) / 5) + 1;
				if (n_sunkens >= 2) n_sunkens = 2;
				if (sunken_count < n_sunkens) {
					army = [army](state&st) {
						return nodelay(st, unit_types::sunken_colony, army);
					};
				}
// 				if (zergling_count < 6 || (enemy_army_supply >= 16.0 && army_supply < enemy_army_supply)) {
// 					army = [army](state&st) {
// 						return nodelay(st, unit_types::zergling, army);
// 					};
// 				}
			}
		}

		return army(st);*/
	}

	virtual void post_build() override {

		if (output_enabled) {
			for (auto* bs : natural_choke.build_squares) {
				game->drawBoxMap(bs->pos.x, bs->pos.y, bs->pos.x + 32, bs->pos.y + 32, BWAPI::Colors::Orange, true);
			}
		}

		if (natural_pos != xy()) {
			combat::control_spot* cs = get_best_score(combat::active_control_spots, [&](combat::control_spot* cs) {
				return diag_distance(cs->pos - natural_pos);
			});
			if (cs) {
				size_t iterations = 0;
				for (auto&b : build::build_tasks) {
					if (b.built_unit || b.dead) continue;
					if (b.type->unit == unit_types::nydus_canal && build_nydus_pos != xy()) {
						build::set_build_pos(&b, build_nydus_pos);
					}
					if (b.type->unit == unit_types::creep_colony && static_defence_pos != xy()) {
						build::unset_build_pos(&b);
						build::set_build_pos(&b, static_defence_pos);
						continue;
					}
					if (b.type->unit == unit_types::creep_colony) {
						xy pos = b.build_pos;
						build::unset_build_pos(&b);

						a_vector<std::tuple<xy, double>> scores;

						auto pred = [&](grid::build_square&bs) {
							if (++iterations % 1000 == 0) multitasking::yield_point();
	
							for (unit* u : enemy_units) {
								if (u->gone) continue;
								if (u->stats->ground_weapon && diag_distance(u->pos - bs.pos) <= u->stats->ground_weapon->max_range + 64) {
									return false;
								}
							}
							if (b.type->unit->requires_creep && !build::build_has_existing_creep(bs, b.type->unit)) return false;
							return build::build_full_check(bs, b.type->unit);
						};
						auto score = [&](xy pos) {
							double r = 0.0;
							auto& bs = grid::get_build_square(pos);
							bool is_inside = test_pred(natural_choke.inside_squares, [&](auto* nbs) {
								return nbs == &bs;
							});
							if (!natural_choke.inside_squares.empty() && !is_inside) r += (32 * 12) * (32 * 12);
							xy center_pos = pos + xy(b.type->unit->tile_width * 16, b.type->unit->tile_height * 16);
							for (auto* nbs : natural_choke.build_squares) {
								double d = diag_distance(center_pos - nbs->pos);
								r += d*d;
							}
							return r;
						};

						std::array<xy, 1> starts;
						//starts[0] = threats::nearest_attack_pos != xy() ? threats::nearest_attack_pos : natural_pos;
						starts[0] = natural_pos;
						pos = build_spot_finder::find_best(starts, 128, pred, score);

						if (pos != xy()) build::set_build_pos(&b, pos);
						else {
							b.dead = true;
						}
					}
				}
			}
		}

	}

};

