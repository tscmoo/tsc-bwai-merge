

struct strat_z_mod : strat_z_base {

	virtual void init() override {

		sleep_time = 15;

		call_place_static_defence = false;
		default_upgrades = false;

	}

	xy natural_pos;
	xy natural_cc_build_pos;
	combat::choke_t natural_choke;
	int max_mineral_workers = 0;

	int gas_state = 0;

	xy static_defence_pos;
	xy build_nydus_pos;
	bool has_nydus_without_exit = false;

	void mod_test_tick();
	void mod_test_build(buildpred::state& st);

	void mod_test2_tick();
	void mod_test2_build(buildpred::state& st);
	
	void mod_test3_tick();
	void mod_test3_build(buildpred::state& st);
	
	bool enable_auto_upgrades = false;

	virtual bool tick() override {
		if (opening_state == 0) {
			if (my_resource_depots.size() != 1 || my_workers.size() != 4) opening_state = -1;
			else {
				build::add_build_total(0.0, unit_types::drone, 9);
				build::add_build_total(1.0, unit_types::overlord, 2);
				build::add_build_total(2.0, unit_types::drone, 12);
				build::add_build_total(3.0, unit_types::hatchery, 2);
				//build::add_build_total(2.0, unit_types::drone, 14);
				build::add_build_total(4.0, unit_types::spawning_pool, 1);
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
				resource_gathering::max_gas = 650.0;
				if (current_gas >= 600) gas_state = 1;
			} else {
				resource_gathering::max_gas = 300.0;
				if (current_gas <= 300) gas_state = 0;
			}
		}
		if (!my_units_of_type[unit_types::spire].empty() && my_completed_units_of_type[unit_types::spire].empty()) {
			resource_gathering::max_gas = 900.0;
		}
		
//		if (drone_count < 30) {
//			double max_gas = 1.0;
//			for (auto& b : build::build_order) {
//				if (b.build_frame == 0) continue;
//				if (current_frame - b.build_frame > 15 * 60 * 2) continue;
//				if (b.type->gas_cost > max_gas) max_gas = b.type->gas_cost;
//			}
//			resource_gathering::max_gas = max_gas + 20.0;
//		}
		if (drone_count < 30) {
			double total_gas = 1.0;
			for (auto& b : build::build_order) {
				if (b.build_frame == 0) continue;
				if (current_frame - b.build_frame > 15 * 60 * 2) continue;
				total_gas += b.type->gas_cost;
			}
			resource_gathering::max_gas = std::min(total_gas, 600.0);
		}
		
		if (players::opponent_player->race == race_zerg) {
			if (my_units_of_type[unit_types::lair].empty()) resource_gathering::max_gas = 100.0;
			else resource_gathering::max_gas = 200.0;
			if (!my_units_of_type[unit_types::spire].empty() || current_frame >= 15 * 60 * 12) resource_gathering::max_gas = 0.0;
			min_bases = 1;
			max_bases = 1;
			if (current_frame >= 15 * 60 * 12) max_bases = 0;
		}

		threats::eval(*this);

// 		if (hydralisk_count) min_bases = 3;
// 		max_bases = 3;

		tactics::enable_anti_push_until = current_frame + 15 * 10;

		if (enable_auto_upgrades) {
			get_upgrades::set_no_auto_upgrades(false);
		} else {
			if (current_used_total_supply < 180) {
				get_upgrades::set_no_auto_upgrades(true);
			}
		}

		if (players::opponent_player->race != race_zerg) {
			if (!is_defending && current_frame < 15 * 60 * 8) {
				combat::aggressive_zerglings = true;
				combat::combat_mult_override_until = current_frame + 15 * 5;
				combat::combat_mult_override = 128.0;
			} else {
				if (current_frame < 15 * 60 * 10) tactics::force_attack_n(unit_types::zergling, 4);
				combat::no_scout_around = true;
			}
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
			for (auto& v : cur_st.bases) {
				auto& bs = grid::get_build_square(v.s->cc_build_pos);
				if (bs.building && bs.building->owner == players::my_player) {
					bool found = false;
					for (unit* u : my_buildings) {
						if (u->type == unit_types::creep_colony || u->stats->ground_weapon) {
							if (diag_distance(u->pos - v.s->pos) <= 32 * 12) {
								found = true;
								break;
							}
						}
					}
					if (!found) {
						xy pos = get_static_defence_position_next_to(bs.building, unit_types::creep_colony);
						if (pos != xy()) {
							static_defence_pos = pos;
							break;
						}
					}
				}
			}
			//for (unit* u : my_resource_depots) {
			if (static_defence_pos == xy()) {
				for (auto& v : cur_st.bases) {
					auto& bs = grid::get_build_square(v.s->cc_build_pos);
					if (bs.building && bs.building->owner == players::my_player) {
						xy pos = get_static_defence_position_next_to(bs.building, unit_types::creep_colony);
						if (pos != xy()) {
							static_defence_pos = pos;
							break;
						}
					}
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

		//mod_test_tick();
		//mod_test2_tick();
		mod_test3_tick();

		return false;
	}

	buildpred::state* build_st;
	std::function<bool(buildpred::state&)> build_army;
	void build(unit_type* ut) {
		using namespace buildpred;
		auto& army = this->build_army;
		army = [army = std::move(army), ut](state& st) {
			return nodelay(st, ut, army);
		};
	}
	bool upgrade(upgrade_type* upg) {
		using namespace buildpred;
		if (!has_or_is_upgrading(*build_st, upg)) {
			auto& army = this->build_army;
			army = [army = std::move(army), upg](state& st) {
				return nodelay(st, upg, army);
			};
			return false;
		}
		return true;
	}
	bool build_n(unit_type* ut, int n) {
		using namespace buildpred;
		if (count_units_plus_production(*build_st, ut) < n) {
			auto& army = this->build_army;
			army = [army = std::move(army), ut](state& st) {
				return nodelay(st, ut, army);
			};
			return false;
		}
		return true;
	}

	bool upgrade_wait(upgrade_type* upg) {
		using namespace buildpred;
		if (!has_or_is_upgrading(*build_st, upg)) {
			auto& army = this->build_army;
			army = [army = std::move(army), upg](state & st) {
				return nodelay(st, upg, army);
			};
			return false;
		}
		return has_upgrade(*build_st, upg);
	}

	virtual bool build(buildpred::state&st) override {
		using namespace buildpred;
		if (opening_state != -1) return false;

		st.auto_build_hatcheries = true;

		//st.dont_build_refineries = count_units_plus_production(st, unit_types::extractor) && drone_count < 18;
		st.dont_build_refineries = drone_count < 18;

		build_army = army;
		build_st = &st;

		//mod_test_build(st);
		//mod_test2_build(st);
		mod_test3_build(st);

		return build_army(st);
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

#include "strat_z_mod_test.h"
#include "strat_z_mod_test2.h"
#include "strat_z_mod_test3.h"
