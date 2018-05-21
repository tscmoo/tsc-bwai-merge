
struct strat_t_base {

	int enemy_marine_count = 0;
	int enemy_firebat_count = 0;
	int enemy_ghost_count = 0;
	int enemy_vulture_count = 0;
	int enemy_tank_count = 0;
	int enemy_goliath_count = 0;
	int enemy_wraith_count = 0;
	int enemy_valkyrie_count = 0;
	int enemy_bc_count = 0;
	int enemy_science_vessel_count = 0;
	int enemy_dropship_count = 0;
	double enemy_army_supply = 0.0;
	double enemy_air_army_supply = 0.0;
	double enemy_ground_army_supply = 0.0;
	double enemy_ground_large_army_supply = 0.0;
	double enemy_ground_small_army_supply = 0.0;
	int enemy_static_defence_count = 0;
	int enemy_proxy_building_count = 0;
	int enemy_non_gas_proxy_building_count = 0;
	double enemy_attacking_army_supply = 0.0;
	int enemy_attacking_worker_count = 0;
	int enemy_dt_count = 0;
	int enemy_lurker_count = 0;
	int enemy_arbiter_count = 0;
	int enemy_lair_count = 0;
	int enemy_spire_count = 0;
	int enemy_factory_count = 0;
	int enemy_zealot_count = 0;
	int enemy_zergling_count = 0;
	int enemy_dragoon_count = 0;
	int enemy_mutalisk_count = 0;
	int enemy_shuttle_count = 0;
	int enemy_robotics_facility_count = 0;
	int enemy_robotics_support_bay_count = 0;
	int enemy_citadel_of_adun_count = 0;
	int enemy_templar_archives_count = 0;
	int enemy_hydralisk_count = 0;
	int enemy_queen_count = 0;

	bool opponent_has_expanded = false;
	bool being_rushed = false;
	bool is_defending = false;
	bool is_defending_with_workers = false;

	bool default_bio_upgrades = true;
	bool default_mech_upgrades = true;
	bool default_upgrades = true;
	bool default_worker_defence = true;

	int opening_state = 0;
	int sleep_time = 15 * 5;

	bool call_place_expos = true;

	bool bo_all_done() {
		for (auto&b : build::build_order) {
			if (b.dead) continue;
			if (b.type->unit && !b.built_unit) return false;
		}
		return true;
	}
	void bo_cancel_all() {
		for (auto&b : build::build_order) {
			if (b.type->unit && !b.built_unit) {
				b.dead = true;
			}
			if (!b.type->unit) b.dead = true;
		}
	}

	bool can_expand = false;
	bool force_expand = false;

	virtual void init() = 0;
	virtual bool tick() = 0;

	int scv_count;
	int marine_count;
	int medic_count;
	int firebat_count;
	int wraith_count;
	int science_vessel_count;
	int ghost_count;
	int tank_count;
	int vulture_count;
	int goliath_count;
	int battlecruiser_count;
	int valkyrie_count;
	int dropship_count;

	int machine_shop_count;
	int control_tower_count;

	double army_supply = 0.0;
	double air_army_supply = 0.0;
	double ground_army_supply = 0.0;

	std::function<bool(buildpred::state&)> army;

	virtual bool build(buildpred::state&st) = 0;

	int get_max_mineral_workers() {
		using namespace buildpred;
		int count = 0;
		for (auto&b : get_my_current_state().bases) {
			for (auto&s : b.s->resources) {
				if (!s.u->type->is_minerals) continue;
				count += (int)s.full_income_workers[players::my_player->race];
			}
		}
		return count;
	};

	auto get_next_base() {
		auto st = buildpred::get_my_current_state();
		unit_type*worker_type = unit_types::scv;
		unit_type*cc_type = unit_types::cc;
		a_vector<refcounted_ptr<resource_spots::spot>> available_bases;
		for (auto&s : resource_spots::spots) {
			if (grid::get_build_square(s.cc_build_pos).building) continue;
			bool okay = true;
			for (auto&v : st.bases) {
				if ((resource_spots::spot*)v.s == &s) okay = false;
			}
			if (!okay) continue;
			if (!build::build_is_valid(grid::get_build_square(s.cc_build_pos), cc_type)) continue;
			available_bases.push_back(&s);
		}
		return get_best_score(available_bases, [&](resource_spots::spot*s) {
			double score = unit_pathing_distance(worker_type, s->cc_build_pos, st.bases.front().s->cc_build_pos);
			double res = 0;
			double ned = get_best_score_value(enemy_buildings, [&](unit* e) {
				if (e->type->is_worker) return std::numeric_limits<double>::infinity();
				if (e->is_flying) return std::numeric_limits<double>::infinity();
				return diag_distance(s->pos - e->pos);
			});
			if (st.bases.size() == 1) ned = 0;
			else if (ned <= 32 * 30) score += 10000;
			int workers_nearby = 0;
// 			for (unit* u : my_workers) {
// 				if (diag_distance(s->pos - u->pos) <= 32 * 10) ++workers_nearby;
// 			}
			for (auto& v : resource_gathering::live_gatherers) {
				if (v.resource) {
					for (auto& r : s->resources) {
						if (v.resource->u == r.u) {
							++workers_nearby;
							break;
						}
					}
				}
			}
			bool has_gas = false;
			for (auto&r : s->resources) {
				res += r.u->resources;
				if (r.u->type->is_gas) has_gas = true;
			}
			score -= (has_gas ? res : res / 4) / 10 + ned + workers_nearby * 256.0;
			return score;
		}, std::numeric_limits<double>::infinity());
	};

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

	void rm_all_scouts() {
		for (auto&v : scouting::all_scouts) {
			scouting::rm_scout(v.scout_unit);
		}
	}

	void set_build_vars(const buildpred::state& st) {
		using namespace buildpred;
		scv_count = count_units_plus_production(st, unit_types::scv);
		marine_count = count_units_plus_production(st, unit_types::marine);
		medic_count = count_units_plus_production(st, unit_types::medic);
		firebat_count = count_units_plus_production(st, unit_types::firebat);
		wraith_count = count_units_plus_production(st, unit_types::wraith);
		science_vessel_count = count_units_plus_production(st, unit_types::science_vessel);
		ghost_count = count_units_plus_production(st, unit_types::ghost);
		tank_count = count_units_plus_production(st, unit_types::siege_tank_tank_mode) + count_units_plus_production(st, unit_types::siege_tank_siege_mode);
		vulture_count = count_units_plus_production(st, unit_types::vulture);
		goliath_count = count_units_plus_production(st, unit_types::goliath);
		battlecruiser_count = count_units_plus_production(st, unit_types::battlecruiser);
		valkyrie_count = count_units_plus_production(st, unit_types::valkyrie);
		dropship_count = count_units_plus_production(st, unit_types::dropship);

		machine_shop_count = count_production(st, unit_types::machine_shop);
		for (auto&v : st.units_range(unit_types::factory)) {
			if (v.has_addon) ++machine_shop_count;
		}
		control_tower_count = count_production(st, unit_types::control_tower);
		for (auto&v : st.units_range(unit_types::starport)) {
			if (v.has_addon) ++control_tower_count;
		}

		army_supply = 0.0;
		air_army_supply = 0.0;
		ground_army_supply = 0.0;
		for (auto&v : st.units) {
			unit_type*ut = v.first;
			if (!ut->is_worker) {
				army_supply += ut->required_supply * v.second.size();
				if (ut->is_flyer) air_army_supply += ut->required_supply * v.second.size();
				else ground_army_supply += ut->required_supply * v.second.size();
			}
		}
		for (auto&v : st.production) {
			unit_type*ut = v.second->unit;
			if (!ut) continue;
			if (!ut->is_worker) {
				army_supply += ut->required_supply;
				if (ut->is_flyer) air_army_supply += ut->required_supply;
				else ground_army_supply += ut->required_supply;
			}
		}
	}

	virtual void post_build() {
	}

	void run() {

		using namespace buildpred;

		combat::no_aggressive_groups = true;
		combat::no_scout_around = false;
		combat::hide_wraiths = false;

		get_upgrades::set_upgrade_value(upgrade_types::restoration, 10000.0);
		get_upgrades::set_upgrade_value(upgrade_types::optical_flare, 10000.0);
		get_upgrades::set_upgrade_value(upgrade_types::caduceus_reactor, 10000.0);
		get_upgrades::set_upgrade_value(upgrade_types::emp_shockwave, 10000.0);

		get_upgrades::set_no_auto_upgrades(true);

		scouting::scout_supply = 12;

		resource_gathering::max_gas = 0.0;

		auto free_mineral_patches = [&]() {
			int free_mineral_patches = 0;
			for (auto&b : get_my_current_state().bases) {
				for (auto&s : b.s->resources) {
					resource_gathering::resource_t*res = nullptr;
					for (auto&s2 : resource_gathering::live_resources) {
						if (s2.u == s.u) {
							res = &s2;
							break;
						}
					}
					if (res) {
						if (res->gatherers.empty()) ++free_mineral_patches;
					}
				}
			}
			return free_mineral_patches;
		};

		auto place_expos = [&]() {

			auto st = get_my_current_state();

			for (auto&b : build::build_tasks) {
				if (b.built_unit || b.dead) continue;
				if (b.type->unit && b.type->unit->is_resource_depot && b.type->unit->builder_type->is_worker) {
					xy pos = b.build_pos;
					build::unset_build_pos(&b);
				}
			}

			for (auto&b : build::build_tasks) {
				if (b.built_unit || b.dead) continue;
				if (b.type->unit && b.type->unit->is_resource_depot && b.type->unit->builder_type->is_worker) {
					xy pos = b.build_pos;
					build::unset_build_pos(&b);
					auto s = get_next_base();
					if (s) pos = s->cc_build_pos;
					if (pos != xy()) build::set_build_pos(&b, pos);
					else build::unset_build_pos(&b);
				}
			}

		};

		init();

		bool maxed_out_aggression = false;
		int opening_state = 0;
		while (true) {

			set_build_vars(get_my_current_state());

			enemy_marine_count = 0;
			enemy_firebat_count = 0;
			enemy_ghost_count = 0;
			enemy_vulture_count = 0;
			enemy_tank_count = 0;
			enemy_goliath_count = 0;
			enemy_wraith_count = 0;
			enemy_valkyrie_count = 0;
			enemy_bc_count = 0;
			enemy_science_vessel_count = 0;
			enemy_dropship_count = 0;
			enemy_army_supply = 0.0;
			enemy_air_army_supply = 0.0;
			enemy_ground_army_supply = 0.0;
			enemy_ground_large_army_supply = 0.0;
			enemy_ground_small_army_supply = 0.0;
			enemy_static_defence_count = 0;
			enemy_proxy_building_count = 0;
			enemy_non_gas_proxy_building_count = 0;
			enemy_attacking_army_supply = 0.0;
			enemy_attacking_worker_count = 0;
			enemy_dt_count = 0;
			enemy_lurker_count = 0;
			enemy_arbiter_count = 0;
			enemy_lair_count = 0;
			enemy_spire_count = 0;
			enemy_factory_count = 0;
			enemy_zealot_count = 0;
			enemy_zergling_count = 0;
			enemy_dragoon_count = 0;
			enemy_mutalisk_count = 0;
			enemy_shuttle_count = 0;
			enemy_robotics_facility_count = 0;
			enemy_robotics_support_bay_count = 0;
			enemy_citadel_of_adun_count = 0;
			enemy_templar_archives_count = 0;
			enemy_hydralisk_count = 0;
			enemy_queen_count = 0;

			update_possible_start_locations();
			for (unit*e : enemy_units) {
				if (e->type == unit_types::marine) ++enemy_marine_count;
				if (e->type == unit_types::firebat) ++enemy_firebat_count;
				if (e->type == unit_types::ghost) ++enemy_ghost_count;
				if (e->type == unit_types::vulture) ++enemy_vulture_count;
				if (e->type == unit_types::siege_tank_tank_mode) ++enemy_tank_count;
				if (e->type == unit_types::siege_tank_siege_mode) ++enemy_tank_count;
				if (e->type == unit_types::goliath) ++enemy_goliath_count;
				if (e->type == unit_types::wraith) ++enemy_wraith_count;
				if (e->type == unit_types::valkyrie) ++enemy_valkyrie_count;
				if (e->type == unit_types::battlecruiser) ++enemy_bc_count;
				if (e->type == unit_types::science_vessel) ++enemy_science_vessel_count;
				if (e->type == unit_types::dropship) ++enemy_dropship_count;
				if (!e->type->is_worker && !e->building) {
					if (e->is_flying) enemy_air_army_supply += e->type->required_supply;
					else {
						enemy_ground_army_supply += e->type->required_supply;
						if (e->type->size == unit_type::size_large) enemy_ground_large_army_supply += e->type->required_supply;
						if (e->type->size == unit_type::size_small) enemy_ground_small_army_supply += e->type->required_supply;
					}
					enemy_army_supply += e->type->required_supply;
				}
				if (e->type == unit_types::missile_turret) ++enemy_static_defence_count;
				if (e->type == unit_types::photon_cannon) ++enemy_static_defence_count;
				if (e->type == unit_types::sunken_colony) ++enemy_static_defence_count;
				if (e->type == unit_types::spore_colony) ++enemy_static_defence_count;
				if (true) {
					double e_d = get_best_score_value(possible_start_locations, [&](xy pos) {
						return unit_pathing_distance(unit_types::scv, e->pos, pos);
					});
					if (unit_pathing_distance(unit_types::scv, e->pos, combat::my_closest_base) < e_d) {
						log("%s is proxied\n", e->type->name);
						if (e->building) ++enemy_proxy_building_count;
						if (e->building && !e->type->is_gas) ++enemy_non_gas_proxy_building_count;
						if (!e->type->is_worker) enemy_attacking_army_supply += e->type->required_supply;
						else ++enemy_attacking_worker_count;
					}
				}
				if (e->type == unit_types::dark_templar) ++enemy_dt_count;
				if (e->type == unit_types::lurker || e->type == unit_types::lurker_egg) ++enemy_lurker_count;
				if (e->type == unit_types::arbiter) ++enemy_arbiter_count;
				if (e->type == unit_types::lair) ++enemy_lair_count;
				if (e->type == unit_types::spire) ++enemy_spire_count;
				if (e->type == unit_types::factory) ++enemy_factory_count;
				if (e->type == unit_types::zealot) ++enemy_zealot_count;
				if (e->type == unit_types::zergling) ++enemy_zergling_count;
				if (e->type == unit_types::dragoon) ++enemy_dragoon_count;
				if (e->type == unit_types::mutalisk) ++enemy_mutalisk_count;
				if (e->type == unit_types::shuttle) ++enemy_mutalisk_count;
				if (e->type == unit_types::robotics_facility) ++enemy_mutalisk_count;
				if (e->type == unit_types::robotics_support_bay) ++enemy_robotics_support_bay_count;
				if (e->type == unit_types::citadel_of_adun) ++enemy_citadel_of_adun_count;
				if (e->type == unit_types::templar_archives) ++enemy_templar_archives_count;
				if (e->type == unit_types::hydralisk) ++enemy_hydralisk_count;
				if (e->type == unit_types::queen) ++enemy_queen_count;
			}

			opponent_has_expanded = false;
			for (auto&s : resource_spots::spots) {
				bool is_start_location = false;
				for (xy pos : start_locations) {
					if (diag_distance(pos - s.cc_build_pos) <= 32 * 10) {
						is_start_location = true;
						break;
					}
				}
				if (is_start_location) continue;
				auto&bs = grid::get_build_square(s.cc_build_pos);
				if (bs.building && bs.building->owner == players::opponent_player) opponent_has_expanded = true;
			}

			if (enemy_attacking_army_supply > 2.0 || enemy_attacking_worker_count >= 4 || enemy_proxy_building_count) {
				if (current_frame < 15 * 60 * 8 && !opponent_has_expanded) {
					being_rushed = true;
				}
			} else {
				if (current_used_total_supply - my_workers.size() >= enemy_army_supply + 8.0) {
					being_rushed = false;
				}
			}

			is_defending = false;
			is_defending_with_workers = false;
			for (auto&g : combat::groups) {
				if (!g.is_aggressive_group && !g.is_defensive_group) {
					if (g.ground_dpf >= 2.0) {
						is_defending = true;
						int worker_count = 0;
						for (auto* a : g.allies) {
							if (a->u->type->is_worker) {
								if (current_frame - a->last_fight <= 15 * 10) ++worker_count;
							}
						}
						if (worker_count >= 4) is_defending_with_workers = true;
						break;
					}
				}
			}

			if (default_worker_defence && current_used_total_supply - my_workers.size() < 8.0 && enemy_proxy_building_count && enemy_army_supply == 0.0) {

				bool dont_pull_workers = false;
				xy proxy_pos;
				for (auto&g : combat::groups) {
					if (!g.is_aggressive_group && !g.is_defensive_group) {
						for (unit*e : g.enemies) {
							if (!e->building) continue;
							if (e->type->is_gas) continue;
							if (e->is_flying) continue;
							if (e->type != unit_types::pylon && e->type != unit_types::photon_cannon && e->type != unit_types::bunker) {
								dont_pull_workers = true;
								break;
							}
						}
						for (unit*e : g.enemies) {
							if (!e->building) continue;
							if (e->type->is_gas) continue;
							if (e->is_flying) continue;
							if (e->type != unit_types::pylon && e->type != unit_types::photon_cannon && e->type != unit_types::bunker) {
							}
							double e_d = get_best_score_value(possible_start_locations, [&](xy pos) {
								return unit_pathing_distance(unit_types::scv, e->pos, pos);
							});
							if (unit_pathing_distance(unit_types::scv, e->pos, combat::my_closest_base) < e_d) {
								proxy_pos = e->pos;
								break;
							}
						}
					}
				}
				if (dont_pull_workers) proxy_pos = xy();

				//if (is_defending && !ground_defence_fight_ok && my_workers.size() < 18) {
				//if (is_defending && my_workers.size() < 18 && current_used_total_supply < 20) {
				if (proxy_pos != xy() && my_workers.size() < 18 && current_used_total_supply < 20) {
					//int n = my_workers.size() / 2;
					int n = my_workers.size() - 3;
					if (enemy_proxy_building_count <= 1) n = 3;
					if (n > enemy_proxy_building_count * 4) n = enemy_proxy_building_count * 4;
					for (unit*u : my_workers) {
						if (u->force_combat_unit) {
							if (u->hp < u->stats->hp / 2) u->force_combat_unit = false;
							else {
								--n;
								if (n < 0) u->force_combat_unit = false;
							}
						}
					}
					while (n > 0) {
						unit*u = get_best_score(my_workers, [&](unit*u) {
							if (u->force_combat_unit) return 0.0;
							return -u->hp;
						});
						if (!u) break;
						u->force_combat_unit = true;
						combat::combat_unit*cu = nullptr;
						for (auto*a : combat::live_combat_units) {
							if (a->u == u) cu = a;
						}
						if (cu) {
							if (diag_distance(cu->u->pos - proxy_pos) > 32 * 6) {
								cu->strategy_busy_until = current_frame + 15 * 7;
								cu->action = combat::combat_unit::action_offence;
								cu->subaction = combat::combat_unit::subaction_move;
								cu->target_pos = proxy_pos;
							} else {
								cu->strategy_attack_until = current_frame + 15 * 7;
								cu->strategy_busy_until = 0;
							}
						}
						--n;
					}
				} else {
					for (unit*u : my_workers) {
						if (u->force_combat_unit) u->force_combat_unit = false;
					}
				}
			} else {
				for (unit*u : my_workers) {
					if (u->force_combat_unit) u->force_combat_unit = false;
				}
			}

			if (default_upgrades) {
				if (default_bio_upgrades) {
					if (my_units_of_type[unit_types::marine].size() >= 8) {
						get_upgrades::set_upgrade_value(upgrade_types::stim_packs, -1.0);
						get_upgrades::set_upgrade_value(upgrade_types::u_238_shells, -1.0);
					}
					if (!my_units_of_type[unit_types::ghost].empty()) {
						get_upgrades::set_upgrade_value(upgrade_types::lockdown, -1.0);
					}
				}
				if (default_mech_upgrades) {
					if (!my_units_of_type[unit_types::vulture].empty()) {
						get_upgrades::set_upgrade_value(upgrade_types::spider_mines, -1.0);
					}
					if (!my_units_of_type[unit_types::siege_tank_tank_mode].empty()) {
						get_upgrades::set_upgrade_value(upgrade_types::siege_mode, -1.0);
					}
				}
			}

			if (current_used_total_supply >= 100 && get_upgrades::no_auto_upgrades) {
				get_upgrades::set_no_auto_upgrades(false);
			}

			combat::no_aggressive_groups = true;
			combat::aggressive_groups_done_only = false;

			auto next_base = get_next_base();
			can_expand = next_base;
			if (next_base && get_my_current_state().bases.size() == 1 && enemy_proxy_building_count) {
				if (scv_count < 30 && my_completed_units_of_type[unit_types::factory].empty()) {
					can_expand = false;
				}
// 				if (!combat::can_transfer_through(next_base->pos)) {
// 					if (scv_count < 30) can_expand = false;
// 				}
			}
			force_expand = can_expand && long_distance_miners() >= 1 && (get_my_current_state().bases.size() >= 3 || free_mineral_patches() < 3) && my_units_of_type[unit_types::cc].size() == my_completed_units_of_type[unit_types::cc].size();
			if (can_expand && free_mineral_patches() == 0 && my_workers.size() >= 45) force_expand = true;

			if (tick()) {
				bo_cancel_all();
				break;
			}

			if (current_used_total_supply >= 190.0) maxed_out_aggression = true;
			if (maxed_out_aggression) {
				combat::no_aggressive_groups = false;
				combat::aggressive_groups_done_only = false;
				if (current_used_total_supply < 150.0) maxed_out_aggression = false;
			}

			auto build = [&](state&st) {

				set_build_vars(st);

				army = [&](state&st) {
					return false;
				};

				if (scv_count <= 2) {
					return depbuild(st, state(st), unit_types::scv);
				}

				return this->build(st);

			};

			execute_build(false, build);

			if (call_place_expos) place_expos();

			post_build();

			multitasking::sleep(sleep_time);
		}


	}

	void render() {

		game->drawTextScreen(300, 8, "scvs: %d", my_workers.size());

	}

};

