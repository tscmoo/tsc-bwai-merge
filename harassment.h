namespace harassment {

	bool pick_up_dragoons = false;
	bool pick_up_anything = false;
	
	bool use_dropships = true;

	struct target {
		bool valid = false;
		xy pos;
		bool scouted = false;
	};

	struct targets_t {
		a_list<target> targets;
	};

	struct group {
		a_vector<combat::combat_unit*> units;
	};

	target& get_target(targets_t& targets, xy pos) {
		for (auto& v : targets.targets) {
			if (v.pos == pos) return v;
		}
		targets.targets.emplace_back();
		targets.targets.back().scouted = tsc::rng(1.0) < 0.5;
		targets.targets.back().pos = pos;
		return targets.targets.back();
	}

	void update_targets(targets_t& targets) {

		for (auto& v : targets.targets) {
			v.valid = false;
		}

		for (auto& s : resource_spots::spots) {
			bool reachable = true;
			for (xy pos : combat::find_bigstep_path(unit_types::siege_tank_tank_mode, my_start_location, s.cc_build_pos, square_pathing::pathing_map_index::default_)) {
				if (!combat::can_transfer_through(pos)) {
					reachable = false;
					break;
				}
			}
			bool valid = !reachable;
			if (!valid) {
				for (unit*e : enemy_units) {
					double d = diag_distance(e->pos - s.cc_build_pos);
					if (d <= 32 * 10) {
						if (e->building || e->type->is_worker) valid = true;
					}
				}
			}
			auto& t = get_target(targets, s.cc_build_pos);
			t.valid = false;
		}

	}

	struct scouter {
		combat::combat_unit* a = nullptr;
		a_vector<int> costgrid;
		
		int last_update_costgrid = 0;
		targets_t targets;
		int last_update_targets = 0;
	};

	void run_scouter(scouter& s) {
		if (!s.a) return;
		if (s.a->u->controller->action == unit_controller::action_scout || s.a->u->dead || !combat::can_transfer_through(s.a->u->pos)) {
			s.a->strategy_busy_until = 0;
			s.a = nullptr;
			return;
		}
		if (current_frame - s.last_update_targets >= 90) {
			s.last_update_targets = current_frame;
			update_targets(s.targets);
		}
		bool all_scouted = true;
		for (auto& t : s.targets.targets) {
			if (!t.scouted && current_frame - grid::get_build_square(t.pos).last_seen <= 90) t.scouted = true;
			if (!t.scouted) all_scouted = false;
		}
		if (all_scouted) {
			//log(log_level_test, "all targets scouted\n");
			for (auto& t : s.targets.targets) {
				t.scouted = false;
			}
		}
		if (current_frame - s.last_update_costgrid >= 15) {
			tsc::high_resolution_timer ht;
			s.costgrid.clear();
			s.costgrid.resize(grid::build_grid_width * grid::build_grid_height);
			a_deque<std::pair<xy, int>> open;
			for (auto& t : s.targets.targets) {
				if (t.scouted) continue;
				open.emplace_back(t.pos, 1);
			}
			int iterations = 0;
			while (!open.empty()) {
				++iterations;
				if (iterations % 1024 == 0) multitasking::yield_point();
				auto cur = open.front();
				open.pop_front();
				auto& bs = grid::get_build_square(cur.first);
				for (int i = 0; i != 4; ++i) {
					auto* n = bs.get_neighbor(i);
					if (n && n->entirely_walkable && !n->building) {
						auto& cost = s.costgrid[grid::build_square_index(*n)];
						if (cost == 0 && combat::can_transfer_through(n->pos)) {
							cost = cur.second + 1;
							open.emplace_back(n->pos, cur.second + 1);
						}
					}
				}
			}
			//log(log_level_test, "costgrid updated in %gms\n", ht.elapsed() * 1000);
		}
		s.costgrid.resize(grid::build_grid_width * grid::build_grid_height);

		auto* sq = &grid::get_build_square(s.a->u->pos);
		for (int i = 0; i < 8; ++i) {
			grid::build_square* best_n = sq;
			int best_cost = std::numeric_limits<int>::max();
			for (int i2 = 0; i2 != 4; ++i2) {
				auto* n = sq->get_neighbor(i2);
				if (n && n->entirely_walkable && !n->building) {
					int cost = s.costgrid[grid::build_square_index(*n)];
					if (cost < best_cost) {
						best_cost = cost;
						best_n = n;
					}
				}
			}
			sq = best_n;
		}
		if (sq == &grid::get_build_square(s.a->u->pos)) {
			//log(log_level_test, "no scout target found, resetting\n");
			for (auto& t : s.targets.targets) {
				if (tsc::rng(1.0) < 0.5) t.scouted = false;
			}
		}
		s.a->strategy_busy_until = current_frame + 15;
		s.a->subaction = combat::combat_unit::subaction_move;
		s.a->target_pos = sq->pos + xy(16,16);
		game->drawLineMap(s.a->u->pos.x, s.a->u->pos.y, s.a->target_pos.x, s.a->target_pos.y, BWAPI::Colors::White);
		//log(log_level_test, " scout moving distance %g\n", (s.a->u->pos - s.a->target_pos).length());
	}

	a_list<scouter> scouters;

	int last_scout = 0;
	void update_groups() {

		bool is_defending = false;
		for (auto&g : combat::groups) {
			if (!g.is_aggressive_group && !g.is_defensive_group) {
				if (g.ground_dpf >= 2.0) {
					is_defending = true;
					break;
				}
			}
		}

		for (auto i = scouters.begin(); i != scouters.end();) {
			if (!i->a || is_defending) i = scouters.erase(i);
			else ++i;
		}

		if (!is_defending && (my_workers.size() >= 16 || current_used_total_supply >= 30)) {
			int n = std::min((int)my_units_of_type[unit_types::zergling].size() / 10, 3);
			if (scouters.size() < n && scouters.size() < my_units_of_type[unit_types::zergling].size() / 2 && current_frame - last_scout >= 15 * 10) {
				if (players::opponent_player->race != race_zerg || (my_units_of_type[unit_types::zergling].size() >= 12 && scouters.empty())) {
					for (auto* a : combat::live_combat_units) {
						if (current_frame < a->strategy_busy_until) continue;
						if (a->u->type == unit_types::zergling) {
							if (combat::my_base.test(grid::build_square_index(a->u->pos))) {
								last_scout = current_frame;
								scouters.emplace_back();
								auto& n = scouters.back();
								a->strategy_busy_until = current_frame + 15;
								n.a = a;
								//log(log_level_test, "new scouter found\n");
								break;
							}
						}
					}
				}
			}
		}

		for (auto& v : scouters) {
			run_scouter(v);
		}

	}

	int n_attacking_science_vessels = 0;
	int n_attacking_tanks = 0;
	xy largest_enemy_army_pos;

	a_unordered_set<unit*> science_vessel_targets;
	a_unordered_set<unit*> scout_targets;

	struct queen_scout {
		refcounter<combat::combat_unit> h;
		combat::combat_unit* a = nullptr;
		a_vector<int> costgrid;
		int last_update_costgrid = 0;
		a_vector<unit*> nearby_enemies;
		a_vector<unit*> too_close_enemies;
		a_vector<unit*> almost_too_close_enemies;
		bool has_used_parasite = false;
		bool attack = false;
		a_vector<refcounter<combat::combat_unit>> loaded_units;
		bool processed = false;
		int last_find_units = 0;
		unit* shield_battery_target = nullptr;
		unit* repair_target = nullptr;
		int last_look_for_shield_battery = 0;
		unit* follow_shuttle = nullptr;
		int last_look_for_follow_shuttle = 0;
		
		a_vector<refcounter<combat::combat_unit>> repairers;
	};

	a_list<queen_scout> queen_scouts;

	int last_recall = 0;
	int is_recalling_frame = 0;
	combat::combat_unit* is_recalling_arbiter = nullptr;
	xy is_recalling_pos;
	xy is_recalling_arbiter_pos;
	
	a_unordered_map<queen_scout*, xy> need_detection_at;
	int last_check_need_detection_at = 0;

	auto restrict_to_map_bounds = [&](xy pos) {
		if (pos.x < 0) pos.x = 0;
		if (pos.y < 0) pos.y = 0;
		if (pos.x >= grid::map_width) pos.x = grid::map_width - 1;
		if (pos.y >= grid::map_height) pos.y = grid::map_height - 1;
		return pos;
	};

	void update_queen_scout(queen_scout& s) {
		s.repairers.clear();
		if (!s.a) return;
		if (s.a->u->controller->action == unit_controller::action_scout || s.a->u->dead) {
			s.a = nullptr;
			return;
		}
		s.costgrid.resize(grid::build_grid_width * grid::build_grid_height);

		auto update_costgrid = [&]() {
			//a_deque<std::pair<xy, int>> open;
			using open_t = std::pair<xy, int>;
			struct cmp_t {
				bool operator()(const open_t& a, const open_t& b) const {
					return a.second > b.second;
				}
			};
			std::priority_queue<open_t, a_vector<open_t>, cmp_t> open;

			tsc::dynamic_bitset is_in_range(grid::build_grid_width * grid::build_grid_height);

			auto add_open = [&](xy pos, int value) {
				size_t index = grid::build_square_index(pos);
				//s.costgrid[index] = value;
				open.emplace(pos, value);
			};

			a_deque<grid::build_square*> range_open;
			tsc::dynamic_bitset range_visited(grid::build_grid_width * grid::build_grid_height);
			bool do_add_open = true;
			if (s.a->u->type == unit_types::shuttle && s.loaded_units.empty()) {
				if (!my_completed_units_of_type[unit_types::robotics_facility].empty()) {
					do_add_open = false;
					for (unit* u : my_completed_units_of_type[unit_types::robotics_facility]) {
						add_open(u->pos, 0);
					}
				}
			} else if (s.shield_battery_target && units_distance(s.a->u, s.shield_battery_target) > 8) {
				add_open(s.shield_battery_target->pos, 0);
				do_add_open = false;
			} else if (s.repair_target) {
				add_open(s.repair_target->pos, 0);
				do_add_open = false;
			} else if (s.follow_shuttle) {
				add_open(s.follow_shuttle->pos, 0);
				do_add_open = false;
			} else if ((s.a->u->type == unit_types::overlord || s.a->u->type == unit_types::observer) && !unit_controls::fail_build_positions.empty() && need_detection_at.find(&s) == need_detection_at.end()) {
				for (auto i = unit_controls::fail_build_positions.begin(); i != unit_controls::fail_build_positions.end();) {
					if (diag_distance(s.a->u->pos - i->first) <= 32 * 3) i = unit_controls::fail_build_positions.erase(i);
					else {
						add_open(i->first, 0);
						++i;
					}
				}
				do_add_open = false;
			} else if (s.a->u->type == unit_types::arbiter) {
				xy largest_group_pos;
				double largest_group_supply = 0.0;
				for (auto& g : combat::groups) {
					if (g.allies.empty()) continue;
					double supply = 0.0;
					double x = 0.0;
					double y = 0.0;
					int n = 0;
					for (auto* c : g.allies) {
						supply += c->u->type->required_supply;
//						if (diag_distance(c->u->pos - s.a->u->pos) <= 32 * 16) {
//							x += c->u->pos.x;
//							y += c->u->pos.y;
//							++n;
//						}
					}
					if (n == 0) {
						n = 1;
						x = g.allies.front()->u->pos.x;
						y = g.allies.front()->u->pos.y;
					}
					if (supply > largest_group_supply && n) {
						largest_group_supply = supply;
						x /= n;
						y /= n;
						largest_group_pos = g.allies.front()->u->pos;
						//largest_group_pos = xy((int)x, (int)y);
//						largest_group_pos = get_best_score(g.allies, [&](auto* a) {
//							return diag_distance(a->u->pos - s.a->u->pos);
//						})->u->pos;
					}
				}
				if (largest_group_pos != xy() && diag_distance(s.a->u->pos - largest_group_pos) > 32 * 8) {
					do_add_open = false;
					add_open(largest_group_pos, 0);
				} else {
					int n = 0;
					for (auto& g : combat::groups) {
						for (auto* u : g.enemies) {
							if (u->type == unit_types::siege_tank_tank_mode || u->type == unit_types::siege_tank_siege_mode) {
								add_open(u->pos, 0);
								++n;
							}
						}
					}
					if (n) {
						do_add_open = false;
					}
				}
			} else if (s.a->u->type == unit_types::science_vessel || s.a->u->type == unit_types::observer) {
				auto i = need_detection_at.find(&s);
				if (i != need_detection_at.end()) {
					do_add_open = false;
					add_open(i->second, 0);
				} else {
					xy nearest_group_pos;
					double nearest_group_distance = std::numeric_limits<double>::infinity();
					for (auto& g : combat::groups) {
						if (g.is_aggressive_group) continue;
						int okay_n = 0;
						for (auto* a : g.allies) {
							if (current_frame - a->last_fight < 90 && a->last_win_ratio > 4.0) ++okay_n;
						}
						if (okay_n <= g.allies.size() / 2) {
							xy target_pos;
							for (auto* u : g.enemies) {
								if (u->type->total_gas_cost >= 100.0) {
									target_pos = u->pos;
									break;
								}
							}
							if (target_pos != xy()) {
								double d = diag_distance(s.a->u->pos - target_pos);
								if (d < nearest_group_distance) {
									nearest_group_distance = d;
									nearest_group_pos = target_pos;
								}
							}
						}
					}
					if (nearest_group_pos != xy() && diag_distance(s.a->u->pos - nearest_group_pos) >= 32 * 8) {
						do_add_open = false;
						add_open(nearest_group_pos, 0);
					} else {
						xy largest_group_pos;
						double largest_group_supply = 0.0;
						for (auto& g : combat::groups) {
							double supply = 0.0;
							for (auto* c : g.allies) {
								supply += c->u->type->required_supply;
							}
							if (supply > largest_group_supply) {
								largest_group_supply = supply;
								largest_group_pos = g.allies.front()->u->pos;
							}
						}
						if (largest_group_pos != xy() && diag_distance(s.a->u->pos - largest_group_pos) >= 32 * 14) {
							do_add_open = false;
							add_open(largest_group_pos, 0);
						}
					}
				}
			}
			bool prefer_air_units = false;
			if (s.a->u->type == unit_types::corsair || s.a->u->type == unit_types::scout || s.a->u->type == unit_types::wraith) {
				prefer_air_units = true;
			}
			for (unit* u : enemy_units) {
				bool add = do_add_open;
				if (s.a->u->type == unit_types::arbiter && n_attacking_tanks) {
					if (u->type != unit_types::siege_tank_tank_mode && u->type != unit_types::siege_tank_siege_mode) add = false;
				} else if (largest_enemy_army_pos != xy()) {
				//if (s.a->u->type == unit_types::shuttle && largest_enemy_army_pos != xy()) {
					if (diag_distance(u->pos - largest_enemy_army_pos) > 32 * 20) add = false;
				}
//				if (s.a->u->type == unit_types::scout && n_attacking_science_vessels) {
//					if (u->type != unit_types::science_vessel) add = false;
//				}
				xy pos = u->pos;
				auto* bs = &grid::get_build_square(pos);
				//size_t index = grid::build_square_index(*bs);
				int age = current_frame - bs->last_seen;
				constexpr int max_age = 15 * 60;
				if (s.a->u->type == unit_types::arbiter && n_attacking_tanks) {
					age = std::max(max_age - (current_frame - u->last_attacked), 0);
				}
				if (u->type == unit_types::science_vessel) {
					if (science_vessel_targets.count(u)) age = max_age;
				}
				if (u->type == unit_types::scout) {
					if (scout_targets.count(u)) age = max_age;
				}
				if (prefer_air_units && !u->is_flying) {
					age = max_age / 2 + age / 2;
				}
				if (age > max_age) age = max_age;
				int value = (max_age - age) * 32 * 512 / max_age;

				if (add) add_open(pos, value);
				if (u->stats->air_weapon || u->type == unit_types::bunker) {
					range_visited.reset();
					double max_range = u->type == unit_types::bunker ? 32 * 5 : u->stats->air_weapon->max_range;
					max_range += 64;
					if (s.a->u->hp <= s.a->u->stats->hp / 2) max_range += 64;
					auto* start_bs = &grid::get_build_square(pos);
					range_open.push_back(start_bs);
					range_visited.set(grid::build_square_index(*start_bs));
					while (!range_open.empty()) {
						auto* bs = range_open.front();
						range_open.pop_front();
						for (int i = 0; i != 4; ++i) {
							auto* n = bs->get_neighbor(i);
							if (n) {
								size_t index = grid::build_square_index(*n);
								if (range_visited.test(index)) continue;
								range_visited.set(index);
								is_in_range.set(index);
								double d = diag_distance(pos - n->pos);
								if (d >= max_range) {
									if (add) add_open(n->pos, value);
								} else range_open.push_back(n);
							}
						}
					}
				}
			}
			int iterations = 0;
			while (!open.empty()) {
				++iterations;
				if (iterations % 1024 == 0) multitasking::yield_point();
				auto cur = open.top();
				open.pop();
				auto& bs = grid::get_build_square(cur.first);
				auto add = [&](xy npos) {
					if ((size_t)npos.x - 64 >= grid::map_width - 128 || (size_t)npos.y - 64 >= grid::map_height - 128) return;
					auto* n = &grid::get_build_square(npos);
					size_t index = grid::build_square_index(*n);
					auto& cost = s.costgrid[index];
					if (cost == 0 && !is_in_range.test(index)) {
						cost = cur.second + 1;
						open.emplace(n->pos, cur.second + 1);
					}
				};
				add(bs.pos + xy(32, 0));
				add(bs.pos + xy(32, 32));
				add(bs.pos + xy(0, 32));
				add(bs.pos + xy(-32, 32));
				add(bs.pos + xy(-32, 0));
				add(bs.pos + xy(-32, -32));
				add(bs.pos + xy(0, -32));
				add(bs.pos + xy(32, -32));
			}
		};

		if (current_frame - s.last_update_costgrid >= (s.too_close_enemies.empty() ? 40 : 15)) {
			s.last_update_costgrid = current_frame;
			tsc::high_resolution_timer ht;
			s.costgrid.clear();
			s.costgrid.resize(grid::build_grid_width * grid::build_grid_height);
			update_costgrid();
			//log(log_level_test, "costgrid updated in %gms\n", ht.elapsed() * 1000);
		};

		s.nearby_enemies.clear();
		s.too_close_enemies.clear();
		s.almost_too_close_enemies.clear();
		xy mypos = s.a->u->pos + xy((int)(s.a->u->hspeed * latency_frames), (int)(s.a->u->vspeed * latency_frames));
		mypos = restrict_to_map_bounds(mypos);
		//for (unit* u : visible_enemy_units) {
		for (unit* u : enemy_units) {
			if (u->gone) continue;
			bool too_close = false;
			bool almost_too_close = false;
			xy upos = u->pos + xy((int)(u->hspeed * latency_frames), (int)(u->vspeed * latency_frames));
			if (!s.loaded_units.empty()) {
//				for (auto& h : s.loaded_units) {
//					auto* w = h->u->type->is_flyer ? u->stats->air_weapon : u->stats->ground_weapon;
//					if (w || u->type == unit_types::bunker) break;
//					double w_range = u->type == unit_types::bunker ? 32 * 5 : w->max_range;
//					double d = units_distance(upos, u, h->u->pos, h->u);
//					if (d <= w_range + 32) too_close = true;
//					if (d <= w_range + 32 + 64) almost_too_close = true;
//				}
			}
			double d = units_distance(upos, u, mypos, s.a->u);
			if (d <= s.a->u->stats->sight_range) {
				auto* w = s.a->u->is_flying ? u->stats->air_weapon : u->stats->ground_weapon;
				if (w || u->type == unit_types::bunker) {
					int f = latency_frames + 6;
					d -= u->stats->max_speed * f;
					double w_range = u->type == unit_types::bunker ? 32 * 5 : w->max_range;
					if (s.a->u->hp + s.a->u->shields <= (s.a->u->stats->hp + s.a->u->stats->shields) / 2) w_range += 32;
					//if (u->type == unit_types::goliath) w_range = 32 * 12;
					if (d <= w_range + 32) {
						too_close = true;
					}
					if (d <= w_range + 32 + 64) {
						almost_too_close = true;
					}
				}
			}
			if (too_close) {
				s.too_close_enemies.push_back(u);
			}
			if (almost_too_close) {
				s.almost_too_close_enemies.push_back(u);
			}
			double dr = 32 * 14;
			if (s.a->u->type == unit_types::queen) dr = 32 * 10;
			//if (s.a->u->type == unit_types::arbiter) dr = 32 * 10;
			if (s.a->u->type == unit_types::arbiter) dr = 32 * 20;
			if (s.a->u->type == unit_types::science_vessel) dr = 32 * 11;
			if (d <= dr) {
				s.nearby_enemies.push_back(u);
			}
		}

		if (s.processed) return;
		
		if (s.shield_battery_target) {
			auto shields_all_full = [&]() {
				for (auto& v : s.loaded_units) {
					if (v->u->shields < v->u->stats->shields) return false;
				}
				return s.a->u->shields == s.a->u->stats->shields;
			};

			if (s.shield_battery_target->energy < 2.0 || shields_all_full() || s.shield_battery_target->dead) s.shield_battery_target = nullptr;
			else if (units_distance(s.a->u, s.shield_battery_target) <= 32 * 2) {
				
				for (auto& v : s.loaded_units) {
					if (v->u->loaded_into == s.a->u) {
						if (current_frame - s.a->last_used_special >= 15) {
							s.a->u->game_unit->unload(v->u->game_unit);
							s.a->last_used_special = current_frame;
							s.a->u->controller->noorder_until = current_frame + 4;
							
							v->subaction = combat::combat_unit::subaction_recharge;
						}
					}
				}
				
				if (s.a->u->shields < s.a->u->stats->shields) {
					s.a->subaction = combat::combat_unit::subaction_recharge;
					return;
				}
			}
		} else if (s.a->u->stats->shields && current_frame - s.last_look_for_shield_battery >= 15) {
			s.last_look_for_shield_battery = current_frame;
			auto needs_recharge = [&](unit* u) {
				return u->shields <= 2.0 + u->stats->shields * 0.5 * (1.1 - u->hp / u->stats->hp);
			};
			auto go_recharge = [&]() {
				size_t n = 0;
				for (auto& v : s.loaded_units) {
					if (needs_recharge(v->u)) ++n;
				}
				return needs_recharge(s.a->u) || n == s.loaded_units.size() || n >= 2;
			};

			if (go_recharge()) {
				unit* shield_battery = get_best_score(my_completed_units_of_type[unit_types::shield_battery], [&](unit* u) {
					if (u->energy < s.a->u->shields / 2 / 2) return std::numeric_limits<double>::infinity();
					return diag_distance(s.a->u->pos - u->pos);
				}, std::numeric_limits<double>::infinity());
				if (shield_battery) s.shield_battery_target = shield_battery;
			}
		} else if (s.repair_target) {
			if (s.a->u->hp >= s.a->u->stats->hp || s.repair_target->dead) s.repair_target = nullptr;
			else if (units_distance(s.a->u, s.repair_target) <= 32 * 2) {
				
				for (auto& v : s.loaded_units) {
					if (v->u->loaded_into == s.a->u) {
						if (current_frame - s.a->last_used_special >= 15) {
							s.a->u->game_unit->unload(v->u->game_unit);
							s.a->last_used_special = current_frame;
							s.a->u->controller->noorder_until = current_frame + 4;
							
							v->subaction = combat::combat_unit::subaction_idle;
						}
					}
				}
				
				if (current_frame >= s.a->u->controller->noorder_until) {
					s.a->u->game_unit->follow(s.repair_target->game_unit);
					s.a->u->controller->noorder_until = current_frame + 4;
				}
				
				for (auto* a : combat::live_combat_units) {
					if (a->u->type != unit_types::scv) continue;
					if (diag_distance(s.a->u->pos - a->u->pos) > 64) continue;
					auto h = combat::request_control(a);
					if (h) {
						s.repairers.push_back(h);
						
						h->subaction = combat::combat_unit::subaction_repair;
						h->target = s.a->u;
					}
				}
				
				return;
			}
		} else if (s.a->u->type->is_mechanical && s.a->u->hp <= s.a->u->stats->hp / 2 && current_frame - s.last_look_for_shield_battery >= 15) {
			
			double nearest_distance = std::numeric_limits<double>::infinity();
			unit* target = nullptr;
			for (auto* a : combat::live_combat_units) {
				if (a->u->type != unit_types::scv) continue;
				double d = diag_distance(s.a->u->pos - a->u->pos);
				if (d < nearest_distance) {
					nearest_distance = d;
					target = a->u;
					break;
				}
			}
			if (target) s.repair_target = target;
		}
		
		if (s.follow_shuttle) {
			if (s.follow_shuttle->dead) s.follow_shuttle = nullptr;
		} else if (s.a->u->type == unit_types::observer && current_frame - s.last_look_for_follow_shuttle >= 90) {
			s.last_look_for_follow_shuttle = current_frame;
			unit* shuttle = get_best_score(my_completed_units_of_type[unit_types::shuttle], [&](unit* u) {
				for (auto& v : queen_scouts) {
					if (v.follow_shuttle == u) return std::numeric_limits<double>::infinity();
				}
				return diag_distance(s.a->u->pos - u->pos);
			}, std::numeric_limits<double>::infinity());
			if (shuttle) s.follow_shuttle = shuttle;
		}

		auto is_storm_at = [&](xy pos) {
			for (auto& v : combat::active_storms) {
				if (max_distance(pos - v.second) <= 32 * 2) return true;
			}
			return false;
		};

		if (s.a->u->type == unit_types::shuttle || s.a->u->type == unit_types::dropship) {
			int unloaded_units = 0;
			for (auto i = s.loaded_units.begin(); i != s.loaded_units.end();) {
				if ((*i)->u->dead || (*i)->u->stasis_timer || (*i)->u->maelstrom_timer || (*i)->u->lockdown_timer) i = s.loaded_units.erase(i);
				else {
					if (!(*i)->u->loaded_into) ++unloaded_units;
					++i;
				}
			}
			int space_left = s.a->u->type->space_provided;
			int ht_count = 0;
			int zealot_count = 0;
			int nearby_zealot_count = 0;
			int da_count = 0;
			int marine_count = 0;
			int firebat_count = 0;
			int medic_count = 0;
			for (auto& v : s.loaded_units) {
				space_left -= v->u->type->space_required;
				if (v->u->type == unit_types::high_templar) ++ht_count;
				else if (v->u->type == unit_types::zealot) {
					++zealot_count;
					if (diag_distance(s.a->u->pos - v->u->pos) <= 32 * 6) ++nearby_zealot_count;
				} else if (v->u->type == unit_types::dark_archon) ++da_count;
				else if (v->u->type == unit_types::marine) ++marine_count;
				else if (v->u->type == unit_types::firebat) ++firebat_count;
				else if (v->u->type == unit_types::medic) ++medic_count;
			}
			if (space_left < 0) {
				s.loaded_units.clear();
				return;
			}
			if (current_frame - s.last_find_units >= 40 && unloaded_units == 0) {
				s.last_find_units = current_frame;
				if (space_left) {
					for (auto* a : combat::live_combat_units) {
						if (diag_distance(a->u->pos - s.a->u->pos) > 32 * 12 && combat::entire_threat_area.test(grid::build_square_index(a->u->pos))) continue;
						if (a->u->type == unit_types::high_templar && ht_count < 2) {
							if (a->u->type->space_required > space_left) continue;
							auto h = combat::request_control(a);
							if (h) {
								log(log_level_test, " found high templar\n");
								s.loaded_units.push_back(std::move(h));
								break;
							}
						}
//						if (a->u->type == unit_types::zealot) {
//							if (a->u->type->space_required > space_left) continue;
//							if (zealot_count < ht_count && units_distance(s.a->u, a->u) <= 32 * 12) {
//								auto h = combat::request_control(a);
//								if (h) {
//									log(log_level_test, " found zealot\n");
//									s.loaded_units.push_back(std::move(h));
//									break;
//								}
//							}
//						}
						if (a->u->type == unit_types::reaver && s.loaded_units.empty()) {
							if (a->u->type->space_required > space_left) continue;
							auto h = combat::request_control(a);
							if (h) {
								log(log_level_test, " found reaver\n");
								s.loaded_units.push_back(std::move(h));
								break;
							}
						}
						if (a->u->type == unit_types::dark_archon) {
							if (a->u->type->space_required > space_left) continue;
							auto h = combat::request_control(a);
							if (h) {
								log(log_level_test, " found dark archon\n");
								s.loaded_units.push_back(std::move(h));
								break;
							}
						}
//						if (a->u->type == unit_types::dark_templar) {
//							if (a->u->type->space_required > space_left) continue;
//							auto h = combat::request_control(a);
//							if (h) {
//								log(log_level_test, " found dark templar\n");
//								s.loaded_units.push_back(std::move(h));
//								break;
//							}
//						}
						
						if (pick_up_dragoons) {
							if (a->u->type == unit_types::dragoon) {
								if (a->u->type->space_required > space_left) continue;
								auto h = combat::request_control(a);
								if (h) {
									log(log_level_test, " found dragoon\n");
									s.loaded_units.push_back(std::move(h));
									break;
								}
							}
						}
						
						if (a->u->type == unit_types::medic && medic_count < (marine_count + firebat_count) / 2) {
							if (a->u->type->space_required > space_left) continue;
							auto h = combat::request_control(a);
							if (h) {
								log(log_level_test, " found medic\n");
								s.loaded_units.push_back(std::move(h));
								break;
							}
						}
						
						if (a->u->type == unit_types::marine) {
							if (a->u->type->space_required > space_left) continue;
							auto h = combat::request_control(a);
							if (h) {
								log(log_level_test, " found marine\n");
								s.loaded_units.push_back(std::move(h));
								break;
							}
						}
						
						if (a->u->type == unit_types::firebat) {
							if (a->u->type->space_required > space_left) continue;
							auto h = combat::request_control(a);
							if (h) {
								log(log_level_test, " found firebat\n");
								s.loaded_units.push_back(std::move(h));
								break;
							}
						}
						
						if (pick_up_anything) {
							if (!a->u->is_flying && !a->u->type->is_worker) {
								if (a->u->type->space_required > space_left) continue;
								auto h = combat::request_control(a);
								if (h) {
									log(log_level_test, " found %s\n", a->u->type->is_worker);
									s.loaded_units.push_back(std::move(h));
									break;
								}
							}
						}
						
					}
				}
			}
			if (s.shield_battery_target && unloaded_units == 0) {
				
//			} else if (s.loaded_units.empty()) {
//				for (auto* a : combat::live_combat_units) {
//					if (a->u->type == unit_types::reaver) {
//						auto h = combat::request_control(a);
//						if (h) {
//							log(log_level_test, " found reaver\n");
//							s.loaded_units.push_back(std::move(h));
//							break;
//						}
//					}
//				}
			} else {
				
				if (!s.loaded_units.empty()) {
					// .
				}
				
				xy move_to;
				unit* unload = nullptr;
				for (auto& h : s.loaded_units) {
					if (current_frame - h->last_used_special <= 6) continue;
					//if (h->u->stats->ground_weapon) log(log_level_test, " range is %g\n", h->u->stats->ground_weapon->max_range);
					unit* target = nullptr;
					bool is_ht = h->u->type == unit_types::high_templar;
					bool is_da = h->u->type == unit_types::dark_archon;
					bool is_medic = h->u->type == unit_types::medic;
					bool can_attack = h->u->stats->ground_weapon != nullptr;
					if (is_ht) {
						can_attack = players::my_player->has_upgrade(upgrade_types::psionic_storm) && h->u->energy >= 75.0;
						//if (current_frame - h->last_used_special <= latency_frames && h->u->energy < 150.0) can_attack = false;
					}
					if (is_da) {
						can_attack = players::my_player->has_upgrade(upgrade_types::mind_control) && h->u->energy >= 150.0;
					}
					if (is_medic) {
						can_attack = true;
					}
					if (can_attack) {
						bool can_take_damage = true;
						if (h->u->type == unit_types::dark_templar) {
							can_take_damage = false;
							for (unit* u : enemy_detector_units) {
								if (units_distance(u, h->u) <= u->stats->sight_range + 64) {
									can_take_damage = true;
								}
							}
							if (!can_take_damage && combat::is_in_detected_area(h->u->pos)) can_take_damage = true;
						}
						target = get_best_score(s.nearby_enemies, [&](unit* e) {
							if (e->building && !e->stats->air_weapon && !e->stats->ground_weapon) return std::numeric_limits<double>::infinity();
							if (e->type->is_non_usable) return std::numeric_limits<double>::infinity();
							if (!e->visible || (!is_ht && !e->detected) || e->stasis_timer || e->invincible) return std::numeric_limits<double>::infinity();
							auto* w = e->is_flying ? h->u->stats->air_weapon : h->u->stats->ground_weapon;
							double wr = w ? w->max_range : 0.0;
							if (is_ht) wr = 32.0 * 9;
							else if (is_da) wr = 32.0 * 10;
							else if (is_medic) wr = 32.0 * 6;
							if (wr == 0.0) return std::numeric_limits<double>::infinity();
							double ed = units_distance(h->u, e);
							double d = std::max(ed - wr, 0.0);
							//if (d > 96) return std::numeric_limits<double>::infinity();

							if (h->u->type == unit_types::reaver && !square_pathing::unit_can_reach(h->u, h->u->pos, e->pos)) return std::numeric_limits<double>::infinity();

							//auto* ew = h->u->type->is_flyer ? e->stats->air_weapon : e->stats->ground_weapon;
							//double ew_range = e->type == unit_types::bunker ? 32 * 5 : ew ? ew->max_range : 0.0;

							//if (ew_range > wr) return std::numeric_limits<double>::infinity();
							//if (ew_range && (!ew || ed >= ew->min_range)) {
							//if (h->u->loaded_into) {
							if (can_take_damage) {
								double x = e->pos.x - h->u->pos.x;
								double y = e->pos.y - h->u->pos.y;
								double l = (e->pos - h->u->pos).length();
								x /= l;
								y /= l;
								xy pos = h->u->pos + xy((int)(x * d), (int)(y * d));
								double hp = h->u->shields + h->u->hp;
								double hp_threshold = (h->u->stats->shields + h->u->stats->hp) * 0.33;
								hp += 160.0 * nearby_zealot_count;
								for (unit* e : s.nearby_enemies) {
									if (e->type == unit_types::reaver && current_frame - h->u->last_attacked >= 30) continue;
									if (e->weapon_cooldown > 10) continue;
									if (e->type->is_worker) continue;
									if (e->stasis_timer || e->lockdown_timer || e->maelstrom_timer) continue;
									if (e->game_order == BWAPI::Orders::Sieging) continue;
									auto* ew = h->u->type->is_flyer ? e->stats->air_weapon : e->stats->ground_weapon;
									double ew_range = e->type == unit_types::bunker ? 32 * 5 : ew ? ew->max_range : 0.0;
									if (ew_range == 0.0) continue;
									if (e->stats->max_speed >= 2.0) ew_range += 32;
									if (e->type == unit_types::reaver && ew_range < 32 * 5) ew_range = 32 * 5;
									double d = units_distance(pos, h->u, e->pos, e);
									if (ew && d < ew->min_range) continue;
									if (d <= ew_range) {
										double damage = 8.0;
										if (ew) {
											damage = combat::get_weapon_damage(e, ew, h->u);
											if (ew->cooldown < 30) damage *= 2;
										}
										hp -= damage;
										if (hp <= hp_threshold) return std::numeric_limits<double>::infinity();
									}
								}
							}

							if (is_ht) {
								double value = 0.0;
								int n = 0;
								for (unit* e2 : s.nearby_enemies) {
									if (e2->building || e2->invincible) continue;
									if (unit_distance(e2, e->pos) > 48) continue;
									if (is_storm_at(e2->pos)) continue;
									++n;
									value += e2->type->total_minerals_cost + e2->type->total_gas_cost;
								}
								if (value < 200.0 && n < 3) return std::numeric_limits<double>::infinity();
								if (d > 0.0) value += 200.0;
								return -value;
							}
							if (is_da) {
								if (combat::disable_target_taken.find(e) != combat::disable_target_taken.end()) return std::numeric_limits<double>::infinity();
								double value = e->type->total_minerals_cost + e->type->total_gas_cost;
								if (value < 200.0) return std::numeric_limits<double>::infinity();
								return -value;
							}

							if (h->u->type == unit_types::reaver && d == 0.0 && w) {
								double value = 0.0;
								double r = w->outer_splash_radius;
								for (unit* e2 : s.nearby_enemies) {
									if (unit_distance(e2, e->pos) <= r) value += e2->type->is_worker ? 200.0 : e2->type->total_minerals_cost + e2->type->total_gas_cost;
								}
								return -value;
							}

							if (e->type->is_worker) return d + e->hp / 500.0;
							return d + e->hp / 1000.0;
						}, std::numeric_limits<double>::infinity());
					}
					if (h->u->type == unit_types::zealot) {
						if (target && target->type != unit_types::siege_tank_siege_mode) target = nullptr;
					}
					h->subaction = combat::combat_unit::subaction_move;
					h->target_pos = s.a->u->pos;
					if (target) {
						log(log_level_test, "target %s\n", target->type->name);
						h->subaction = combat::combat_unit::subaction_fight;
						h->target = target;
						
						game->drawLineMap(h->u->pos.x, h->u->pos.y, target->pos.x, target->pos.y, BWAPI::Colors::Red);

						if (is_ht || is_da) {
							//int f = current_command_frame;
							//while (current_command_frame == f) multitasking::sleep(1);
							if (!h->u->is_loaded) {
								//a_vector<combat::combat_unit*> allies;
								//allies.push_back(*h);
								//combat::do_high_templars(allies, s.nearby_enemies);

								h->subaction = combat::combat_unit::subaction_idle;
								h->u->controller->action = unit_controller::action_idle;
								
								xy target_pos = target->pos + xy((int)(target->hspeed*(latency_frames + 12)), (int)(target->vspeed*(latency_frames + 12)));
								auto cast = [&]() {
									if (is_ht) {
										return h->u->game_unit->useTech(upgrade_types::psionic_storm->game_tech_type, BWAPI::Position(target_pos.x, target_pos.y));
									}
									if (is_da) {
										return h->u->game_unit->useTech(upgrade_types::mind_control->game_tech_type, target->game_unit);
									}
									return false;
								};

								if (h->u->game_unit->getSpellCooldown() > latency_frames + 1) {
									if (current_frame >= h->u->controller->noorder_until) {
										cast();
										//if (unit_distance(h->u, target_pos) > 32 * 9) {
										//	h->u->game_unit->useTech(upgrade_types::psionic_storm->game_tech_type, BWAPI::Position(target_pos.x, target_pos.y));
										//}// else h->u->game_unit->holdPosition();
										h->u->controller->noorder_until = current_frame + 10;
									}
								} else {
									if (current_frame - h->last_used_special >= 6) {
										if (cast()) {
											h->last_used_special = current_frame;
											h->u->controller->noorder_until = current_frame + 15;

											if (is_ht && unit_distance(h->u, target_pos) <= 32 * 9) {
												combat::active_storms.emplace_back(current_frame + latency_frames + 4, target_pos);
											} else if (is_da && units_distance(h->u, target) <= 32 * 10) {
												combat::disable_target_taken[target] = std::make_tuple(s.a, current_frame + latency_frames + 10);
											}
										}
									}
								}

								if (is_ht) log(log_level_test, " storm!\n");
								else log(log_level_test, " mind control!\n");
							}
						}
					}
					if (!h->u->is_loaded) {
						//if (units_distance(s.a->u, h->u) > 32) {
						if (units_distance(s.a->u, h->u) > 0) {
							if (s.too_close_enemies.empty() || (h->u->type != unit_types::zealot && h->u->type->minerals_cost + h->u->type->total_gas_cost >= 200 && units_distance(s.a->u, h->u) < 96)) move_to = h->u->pos;
						}
						//if (!s.too_close_enemies.empty() || !target) {
						if (!target) {
							bool pick_up = true;
							if (h->u->type == unit_types::zealot) {
								if (!s.too_close_enemies.empty()) pick_up = false;
								for (auto& h : s.loaded_units) {
									if (!h->u->loaded_into && h->u->type != unit_types::zealot) {
										pick_up = false;
										break;
									}
								}
							}
							if (pick_up) {
								h->subaction = combat::combat_unit::subaction_idle;
								if (current_frame >= h->u->controller->noorder_until && current_frame >= h->last_used_special + 6) {
									log(log_level_test, " right click\n");
									h->u->game_unit->rightClick(s.a->u->game_unit);
									h->u->controller->noorder_until = current_frame + 15;
								} else if (current_frame >= s.a->u->controller->noorder_until && units_distance(s.a->u, h->u) <= 8) {
									log(log_level_test, " load\n");
									s.a->u->game_unit->load(h->u->game_unit);
									s.a->u->controller->noorder_until = current_frame + 8;
								}
							}
						}
					} else {
						//if (s.too_close_enemies.empty() && target) unload = h->u;
						if (target) unload = h->u;

						if (target && zealot_count && h->u->type != unit_types::zealot) {
							for (auto& h : s.loaded_units) {
								if (h->u->loaded_into && h->u->type == unit_types::zealot) {
									unload = h->u;
									break;
								}
							}
						}

						double r = 32 * 9;
						if (h->u->type == unit_types::zealot) r = 32;
						if (is_da) r = 32 * 10;
						if (target && units_distance(s.a->u->pos, h->u, target->pos, target) > r) {
							unload = nullptr;
							//move_to = target->pos;
						}
					}
				}
				if (unload) {
					if (current_frame - s.a->last_used_special >= 15) {
						log(log_level_test, " unload!\n");
						s.a->u->game_unit->unload(unload->game_unit);
						s.a->last_used_special = current_frame;
						s.a->u->controller->noorder_until = current_frame + 4;
						return;
					}
				} else {
					for (unit* u : s.a->u->loaded_units) {
						bool found = false;
						for (auto& v : s.loaded_units) {
							if (v->u == u) {
								found = true;
								break;
							}
						}
						if (!found) {
							if (current_frame - s.a->last_used_special >= 15) {
								s.a->u->game_unit->unload(unload->game_unit);
								s.a->last_used_special = current_frame;
								s.a->u->controller->noorder_until = current_frame + 4;
								return;
							}
						}
					}
				}
				if (move_to != xy()) {
					auto rel_pos = xy_typed<double>(move_to.x, move_to.y) - xy_typed<double>(mypos.x, mypos.y);
					auto move = rel_pos / rel_pos.length() * 32 * 8;
					move_to.x = mypos.x + (int)move.x;
					move_to.y = mypos.y + (int)move.y;
					move_to = restrict_to_map_bounds(move_to);
					auto* c = s.a->u->controller;
					if (current_frame - c->last_move_to >= 15 || diag_distance(c->last_move_to_pos - move_to) >= 32) {
						c->noorder_until = current_frame + 4;
						c->last_move_to = current_frame;
						c->last_move_to_pos = move_to;
						s.a->u->game_unit->move(BWAPI::Position(move_to.x, move_to.y));
					}
					return;
				}
			}
		}

		if (s.a->u->type == unit_types::arbiter) {
			if (current_frame - s.a->last_used_special < 45) return;

			if (is_recalling_frame && is_recalling_arbiter == s.a) {
				auto* c = s.a->u->controller;
				if (current_frame >= c->noorder_until) {
					if (current_frame - c->last_move_to >= 4) {
						c->noorder_until = current_frame + 4;
						c->last_move_to = current_frame;
						c->last_move_to_pos = is_recalling_arbiter_pos;
						s.a->u->game_unit->move(BWAPI::Position(is_recalling_arbiter_pos.x, is_recalling_arbiter_pos.y));
					}
					if (current_frame >= is_recalling_frame) {
						is_recalling_arbiter = nullptr;
						log(log_level_test, " recall!\n");
						c->noorder_until = current_frame + 30;
						s.a->u->game_unit->useTech(upgrade_types::recall->game_tech_type, BWAPI::Position(is_recalling_pos.x, is_recalling_pos.y));
						for (auto& g : combat::groups) {
							for (auto* a : g.allies) {
								if (unit_max_distance(a->u, is_recalling_pos) > 32 * 2.5) continue;
								a->last_attack_enemy_army_supply = 24.0;
								a->last_attack_win_ratio = 0.5;
							}
						}
					}
				}
				return;
			}

			if (current_frame - s.a->last_used_special >= 45) {
				double nearby_army_supply = 0.0;
				for (unit* u : my_units) {
					if (diag_distance(u->pos - s.a->u->pos) > 32 * 20) continue;
					if (u->type == unit_types::arbiter) continue;
					if (!u->stats->ground_weapon) continue;
					nearby_army_supply += u->type->required_supply;
				}
				if (players::my_player->has_upgrade(upgrade_types::stasis_field) && s.a->u->energy >= 100 && (nearby_army_supply >= 8.0 || s.a->u->energy >= 200 || s.a->u->hp != s.a->u->stats->hp)) {

					double best_cost = 0.0;
					xy best_pos;
					for (unit* u : s.nearby_enemies) {
						//if (u->type->total_gas_cost < 100 && !u->type->is_worker) continue;
						if (u->building) continue;
						if (u->stasis_timer) continue;
						//if (u->type->is_worker) continue;
						if (u->lockdown_timer) continue;
						if (u->maelstrom_timer) continue;

						if (combat::disable_target_taken.find(u) != combat::disable_target_taken.end()) continue;

						for (int i = 0; i != 7; ++i) {
							xy pos = u->pos;
							if (i == 1) pos.x += 56;
							else if (i == 2) pos.y += 56;
							else if (i == 3) pos.x += 63;
							else if (i == 4) pos.y += 63;
							else if (i == 5) pos += xy(56, 56);
							else if (i == 6) pos += xy(63, 63);
							double cost = 0.0;
							for (unit* u2 : s.nearby_enemies) {
								if (u2->type == unit_types::science_vessel) continue;
								if (u2->speed >= 4.0) continue;
								//if (u2->type->total_gas_cost < 50) continue;
								if (u2->building) continue;
								if (u2->stasis_timer) continue;
								//if (u2->type->is_worker) continue;
								if (u2->lockdown_timer) continue;
								if (u2->maelstrom_timer) continue;
								if (unit_max_distance(u2, pos) > 48) continue;
								if (combat::disable_target_taken.find(u2) != combat::disable_target_taken.end()) continue;
								cost += u2->type->total_minerals_cost / 2 + u2->type->total_gas_cost;
								if (u2->type->is_worker) cost += 25;
							}
							if (cost > best_cost) {
								best_cost = cost;
								best_pos = pos;
							}
						}
					}
					//if (best_cost) log(log_level_test, "best_cost %g\n", best_cost);
					double min_cost = 300.0;
					//double min_cost = 200.0;
					min_cost *= s.a->u->hp / s.a->u->stats->hp;
					if (best_cost >= min_cost) {
						game->drawLineMap(s.a->u->pos.x, s.a->u->pos.y, best_pos.x, best_pos.y, BWAPI::Colors::Blue);
						if (sc_distance(s.a->u->pos - best_pos) <= 32 * 9) {
							for (unit* u2 : s.nearby_enemies) {
								if (u2->type == unit_types::science_vessel) continue;
								//if (u2->type->total_gas_cost < 50) continue;
								if (u2->building) continue;
								if (u2->stasis_timer) continue;
								if (u2->type->is_worker) continue;
								if (u2->lockdown_timer) continue;
								if (u2->maelstrom_timer) continue;
								if (unit_max_distance(u2, best_pos) > 48) continue;
								combat::disable_target_taken[u2] = std::make_tuple(s.a, current_frame + 60);
							}
							s.a->u->game_unit->useTech(upgrade_types::stasis_field->game_tech_type, BWAPI::Position(best_pos.x, best_pos.y));
							s.a->last_used_special = current_frame;
							s.a->u->controller->noorder_until = current_frame + 30;
							return;
						} else {
							if (current_frame - s.a->u->controller->noorder_until >= 6) {
								s.a->u->game_unit->move(BWAPI::Position(best_pos.x, best_pos.y));
								s.a->u->controller->noorder_until = current_frame + 6;
							}
							return;
						}
					}
				}
			}
			if (current_frame - s.a->last_used_special >= 45 && is_recalling_frame == 0) {
				if (players::my_player->has_upgrade(upgrade_types::recall) && s.a->u->energy >= 150) {

					int n_tanks = 0;
					int n_buildings = 0;
					int n_resource_depots = 0;
					int n_workers = 0;
					double nearest_unit_distance = std::numeric_limits<double>::infinity();
					double nearby_enemy_supply = 0.0;
					for (unit* u : s.nearby_enemies) {
						//if (u->type->total_gas_cost < 100) continue;
						if (u->stasis_timer) continue;
						if (u->lockdown_timer) continue;
						if (u->maelstrom_timer) continue;
						if (u->is_flying) continue;
						if (u->building) ++n_buildings;
						if (u->type->is_resource_depot) ++n_resource_depots;
						if (u->type->is_worker) ++n_workers;
						if (u->type == unit_types::siege_tank_tank_mode || u->type == unit_types::siege_tank_siege_mode) {
							++n_tanks;
						}
						double d = units_distance(s.a->u, u);
						if (d < nearest_unit_distance) nearest_unit_distance = d;
						nearby_enemy_supply += u->type->required_supply;
					}
					bool okay = true;
					if (nearest_unit_distance > 64 && n_tanks >= 5) okay = false;
					//if (!square_pathing::can_fit_at(s.a->u->pos, unit_types::siege_tank_tank_mode->dimensions)) okay = false;
					if (!grid::get_build_square(s.a->u->pos).entirely_walkable) okay = false;
					if (n_tanks < 3 && n_buildings < 10 && n_resource_depots == 0 && n_workers < 8) okay = false;
					if (okay) {

						log(log_level_test, " arbiter wants to recall!\n");

						xy best_supply_in_range_pos;
						double best_supply_in_range = 0.0;
						xy best_supply_ready_pos;
						double best_supply_ready = 0.0;
						for (auto& g : combat::groups) {
							if (g.allies.empty()) continue;
							for (int i = 0; i != 2; ++i) {
								xy pos;
								if (i == 0) {
									for (auto* a : g.allies) {
										pos += a->u->pos;
									}
									pos /= g.allies.size();
								} else pos = g.allies.front()->u->pos;
								if (diag_distance(pos - s.a->u->pos) <= 32 * 15) continue;
								if (!grid::get_build_square(pos).entirely_walkable) continue;
								double supply_in_range = 0.0;
								double supply_ready = 0.0;
								for (auto* a : g.allies) {
									if (combat::entire_threat_area.test(grid::build_square_index(a->u->pos))) continue;
									if (diag_distance(a->u->pos - pos) > 32 * 30) continue;
									supply_in_range += a->u->type->required_supply;
									if (unit_max_distance(a->u, pos) <= 32 * 2.5) supply_ready += a->u->type->required_supply;
								}
								if (supply_in_range > best_supply_in_range) {
									best_supply_in_range = supply_in_range;
									best_supply_in_range_pos = pos;
								}
								if (supply_ready > best_supply_ready) {
									best_supply_ready = supply_ready;
									best_supply_ready_pos = pos;
								}
							}
						}
						log(log_level_test, "nearby_enemy_supply %g, best_supply_in_range %g, best_supply_ready %g\n", nearby_enemy_supply, best_supply_in_range, best_supply_ready);
						if (best_supply_ready > nearby_enemy_supply && best_supply_ready >= 15.0) {
							log(log_level_test, " recall soon!\n");
							is_recalling_frame = current_frame + 15 * 5;
							if (s.a->u->hp != s.a->u->stats->hp) is_recalling_frame = current_frame + 15;
							if (best_supply_in_range - best_supply_ready <= 4) is_recalling_frame = current_frame;
							is_recalling_arbiter = s.a;
							is_recalling_pos = best_supply_ready_pos;
							is_recalling_arbiter_pos = s.a->u->pos;
						} else if (best_supply_in_range > nearby_enemy_supply && best_supply_in_range >= 15.0) {
							log(log_level_test, " group for recall!\n");
							for (auto& g : combat::groups) {
								for (auto* a : g.allies) {
									if (combat::entire_threat_area.test(grid::build_square_index(a->u->pos))) continue;
									if (diag_distance(a->u->pos - best_supply_in_range_pos) > 32 * 30) continue;
									a->strategy_busy_until = current_frame + 15;
									a->subaction = combat::combat_unit::subaction_move;
									a->target_pos = best_supply_in_range_pos;
								}
							}
							if (s.too_close_enemies.empty() || (s.a->u->shields && s.a->u->hp > s.a->u->stats->hp / 2)) {
								auto* c = s.a->u->controller;
								if (current_frame - c->last_move_to >= 4) {
									c->noorder_until = current_frame + 4;
									c->last_move_to = current_frame;
									c->last_move_to_pos = s.a->u->pos;
									s.a->u->game_unit->move(BWAPI::Position(s.a->u->pos.x, s.a->u->pos.y));
								}
								return;
							}
						}
					}
				}
			}
		}

		if (s.a->u->type == unit_types::queen) {
			if (current_frame - s.a->last_used_special < 45) return;
			if (current_frame - s.a->last_used_special >= 45) {
				if (!s.has_used_parasite && s.a->u->energy >= 75 && false) {
					//			bool any_medics_nearby = false;
					//			for (unit* u : enemy_units) {
					//				if (u->gone) continue;
					//				if (diag_distance(u->pos - s.a->u->pos) >= 32*20) continue;
					//				if (u->type == unit_types::medic) {
					//					any_medics_nearby = true;
					//					break;
					//				}
					//			}
					//			if (!any_medics_nearby) {
					for (unit* u : s.nearby_enemies) {
						if (u->type->total_gas_cost < 100) continue;
						if (u->building) continue;
						if (u->has_parasite) continue;
						//if (current_frame - u->last_shown > 60) continue;
						auto it = combat::parasite_target_taken.find(u);
						if (it != combat::parasite_target_taken.end() && std::get<0>(it->second) != s.a) continue;

						if (combat::disable_target_taken.find(u) != combat::disable_target_taken.end()) continue;

						combat::parasite_target_taken[u] = std::make_tuple(s.a, current_frame + 60);

						s.a->u->game_unit->useTech(upgrade_types::parasite->game_tech_type, u->game_unit);
						s.a->last_used_special = current_frame;
						s.a->u->controller->noorder_until = current_frame + 30;

						s.has_used_parasite = true;
						break;
					}
				}
			}
			if (players::my_player->has_upgrade(upgrade_types::spawn_broodling) && s.a->u->energy >= 150) {
				for (unit* u : s.nearby_enemies) {
					if (u->type->total_gas_cost < 100) continue;
					if (u->building) continue;
					if (u->type->is_robotic) continue;
					if (u->stasis_timer) continue;
					if (u->building) continue;
					if (u->type->is_worker) continue;
					if (u->lockdown_timer) continue;
					if (u->maelstrom_timer) continue;
					if (u->is_flying) continue;
					if (u->hp < u->stats->hp / 2) continue;

					if (combat::disable_target_taken.find(u) != combat::disable_target_taken.end()) continue;

					combat::disable_target_taken[u] = std::make_tuple(s.a, current_frame + 60);
					s.a->u->game_unit->useTech(upgrade_types::spawn_broodling->game_tech_type, u->game_unit);
					s.a->last_used_special = current_frame;
					s.a->u->controller->noorder_until = current_frame + 30;
					break;
				}
			}
		} else if (s.a->u->type == unit_types::corsair || s.a->u->type == unit_types::scout || s.a->u->type == unit_types::mutalisk || s.a->u->type == unit_types::wraith) {
			//if (s.too_close_enemies.empty() || s.attack) {
			if (!s.shield_battery_target) {
				bool is_anti_air = s.a->u->type == unit_types::corsair || s.a->u->type == unit_types::mutalisk || s.a->u->type == unit_types::wraith;
				unit* target = get_best_score(s.nearby_enemies, [&](unit* e) {
					auto* w = e->is_flying ? s.a->u->stats->air_weapon : s.a->u->stats->ground_weapon;
					if (!w) return std::numeric_limits<double>::infinity();
					if (!e->visible || !e->detected || e->stasis_timer || e->invincible) return std::numeric_limits<double>::infinity();
					if (e->type == unit_types::interceptor) return std::numeric_limits<double>::infinity();
					auto* ew = s.a->u->is_flying ? e->stats->air_weapon : e->stats->ground_weapon;
					double ew_range = e->type == unit_types::bunker ? 32 * 6 : ew ? ew->max_range : 0.0;
					if (s.attack) {
						if (!ew_range) return std::max(diag_distance(e->pos - mypos) - w->max_range, 0.0) + e->hp / 1000.0 + 256.0;
					} else {
						if (ew_range >= w->max_range) return std::numeric_limits<double>::infinity();
						double x = e->pos.x - mypos.x;
						double y = e->pos.y - mypos.y;
						double l = (e->pos - mypos).length();
						x /= l;
						y /= l;
						double d = std::max(units_distance(s.a->u, e) - w->max_range, 0.0);
						xy pos = mypos + xy((int)(x * d), (int)(y * d));
						for (unit* e : s.nearby_enemies) {
							if (e->stasis_timer || e->lockdown_timer || e->maelstrom_timer) continue;
							auto* ew = s.a->u->is_flying ? e->stats->air_weapon : e->stats->ground_weapon;
							double ew_range = e->type == unit_types::bunker ? 32 * 6 : ew ? ew->max_range : 0.0;
							if (ew_range && ew_range <= 64) ew_range += std::min(e->stats->max_speed * 30, 32.0 * 6);
							if (ew_range) {
								if (units_distance(pos, s.a->u, e->pos, e) <= ew_range + 32) return std::numeric_limits<double>::infinity();
								if (units_distance(s.a->u, e) <= ew_range) return std::numeric_limits<double>::infinity();
							}
						}
					}
					double d = units_distance(e->pos, e, mypos, s.a->u);
					if (is_anti_air && !e->is_flying && (s.a->u->weapon_cooldown > latency_frames || d > w->max_range)) {
					    return 32.0 * 20 + std::max(d - w->max_range, 0.0) + e->hp / 1000.0;
					}
					return std::max(d - w->max_range, 0.0) + e->hp / 1000.0;
				}, std::numeric_limits<double>::infinity());
				if (target) {
					s.a->strategy_busy_until = current_frame + 15;
					s.a->subaction = combat::combat_unit::subaction_fight;
					s.a->target = target;
					return;
				}
			}
		} else if (s.a->u->type == unit_types::science_vessel) {
			if (current_frame - s.a->last_used_special < 15) return;
			if (players::my_player->has_upgrade(upgrade_types::irradiate) && s.a->u->energy >= 75) {
				
				int n_winners_nearby = 0;
				int n_losers_nearby = 0;
				double best_target_score = 0.0;
				for (auto& g : combat::groups) {
					for (auto* a : g.allies) {
						if (sc_distance(a->u->pos - s.a->u->pos) <= 32 * 20 && current_frame - a->last_fight <= 90) {
							if (a->last_win_ratio >= 4.0) ++n_winners_nearby;
							else ++n_losers_nearby;
						}
					}
					for (unit* e : g.enemies) {
						double s = e->type->total_gas_cost + e->hp / 10.0;
						if (s > best_target_score) {
							best_target_score = s;
						}
					}
				}
				
				//if (n_winners_nearby + n_losers_nearby == 0 || n_losers_nearby >= n_winners_nearby / 2 || s.a->u->hp <= s.a->u->stats->hp / 2) {
				if (true) {
					for (unit* u : s.nearby_enemies) {
						if (!u->visible | !u->detected) continue;
						if (u->building) continue;
						if (u->type->is_non_usable) continue; 
						if (u->stasis_timer) continue;
						if (!u->type->is_biological) continue;
						if (u->irradiate_timer) continue;
						if (u->lockdown_timer) continue;
						if (u->maelstrom_timer) continue;
						
						if (s.a->u->energy < 190 && s.a->u->hp > s.a->u->stats->hp / 2) {
							if (s.a->u->energy < 140 || u->type->total_gas_cost < 100.0) {
								if (u->hp < u->stats->hp / 2) continue;
								if (u->hp < 100) continue;
							}
							double s = u->type->total_gas_cost + u->hp / 10.0;
							if (s < best_target_score * 0.75) continue;
						}
	
						if (combat::disable_target_taken.find(u) != combat::disable_target_taken.end()) continue;
	
						combat::disable_target_taken[u] = std::make_tuple(s.a, current_frame + 15);
						s.a->u->game_unit->useTech(upgrade_types::irradiate->game_tech_type, u->game_unit);
						s.a->last_used_special = current_frame;
						s.a->u->controller->noorder_until = current_frame + 15;
						break;
					}
				}
			}
		}

		xy move_to = mypos;
		if (s.a->u->irradiate_timer) {
			for (unit* u : my_units) {
				if (u->building) continue;
				if (!u->type->is_biological) continue;
				if (diag_distance(u->pos - s.a->u->pos) > 96) continue;
				if (u == s.a->u) continue;
				auto rel_pos = xy_typed<double>(s.a->u->pos.x, s.a->u->pos.y) - xy_typed<double>(u->pos.x, u->pos.y);
				auto move = rel_pos / rel_pos.length() * 32 * 8;
				move_to.x = s.a->u->pos.x + (int)move.x;
				move_to.y = s.a->u->pos.y + (int)move.y;
				move_to = restrict_to_map_bounds(move_to);
				break;
			}
		}
		if (s.costgrid[grid::build_square_index(move_to)] == 0) {
			xy pos = mypos;
			tsc::dynamic_bitset visited(grid::build_grid_width * grid::build_grid_height);
			std::deque<xy> open;
			visited.set(grid::build_square_index(pos));
			open.push_back(pos);
			while (!open.empty()) {
				xy cur_pos = open.front();
				open.pop_front();
				if (s.costgrid[grid::build_square_index(cur_pos)]) {
					auto rel_pos = xy_typed<double>(cur_pos.x, cur_pos.y) - xy_typed<double>(mypos.x, mypos.y);
					auto move = rel_pos / rel_pos.length() * 32 * 8;
					move_to.x = mypos.x + (int)move.x;
					move_to.y = mypos.y + (int)move.y;
					move_to = restrict_to_map_bounds(move_to);
					break;
				}
				auto add = [&](xy npos) {
					if ((size_t)npos.x >= grid::map_width || (size_t)npos.y >= grid::map_height) return;
					if (diag_distance(npos - pos) >= 32 * 12) return;
					auto* n = &grid::get_build_square(npos);
					size_t index = grid::build_square_index(*n);
					if (visited.test(index)) return;
					visited.set(index);
					open.push_back(n->pos);
				};
				add(cur_pos + xy(32, 0));
				add(cur_pos + xy(32, 32));
				add(cur_pos + xy(0, 32));
				add(cur_pos + xy(-32, 32));
				add(cur_pos + xy(-32, 0));
				add(cur_pos + xy(-32, -32));
				add(cur_pos + xy(0, -32));
				add(cur_pos + xy(32, -32));
			}
		}
		if (move_to == mypos) {
			if (!s.too_close_enemies.empty()) {
				//log(log_level_test, " %d too close enemies\n", s.too_close_enemies.size());
				xy_typed<double> sum_pos;
				for (unit* u : s.too_close_enemies) {
					sum_pos.x += u->pos.x;
					sum_pos.y += u->pos.y;
				}
				auto avg_pos = sum_pos / s.too_close_enemies.size();
				auto rel_pos = xy_typed<double>(mypos.x, mypos.y) - avg_pos;
				auto move = rel_pos / rel_pos.length() * 32 * 8;
				move_to.x = mypos.x + (int)move.x;
				move_to.y = mypos.y + (int)move.y;
				move_to = restrict_to_map_bounds(move_to);
			} else {
				auto* sq = &grid::get_build_square(mypos);
				for (int i = 0; i < 2; ++i) {
					grid::build_square* best_n = sq;
					int best_cost = std::numeric_limits<int>::max();
					for (int i2 = 0; i2 != 4; ++i2) {
						auto* n = sq->get_neighbor(i2);
						if (n) {
							int cost = s.costgrid[grid::build_square_index(*n)];
							if (cost && cost < best_cost) {
								best_cost = cost;
								best_n = n;
							}
						}
					}
					sq = best_n;
				}
				move_to = sq->pos + xy(16, 16);
				auto rel_pos = xy_typed<double>(move_to.x, move_to.y) - xy_typed<double>(mypos.x, mypos.y);
				auto move = rel_pos / rel_pos.length() * 32 * 8;
				move_to.x = mypos.x + (int)move.x;
				move_to.y = mypos.y + (int)move.y;
				move_to = restrict_to_map_bounds(move_to);
			}
		}

		auto* c = s.a->u->controller;
		if (current_frame >= c->noorder_until) {
			if (current_frame - c->last_move_to >= 15 || diag_distance(c->last_move_to_pos - move_to) >= 32) {
				c->noorder_until = current_frame + 4;
				c->last_move_to = current_frame;
				c->last_move_to_pos = move_to;
				s.a->u->game_unit->move(BWAPI::Position(move_to.x, move_to.y));
			}
		}

		s.a->strategy_busy_until = current_frame + 15;
		s.a->subaction = combat::combat_unit::subaction_idle;
		//game->drawLineMap(mypos.x, mypos.y, move_to.x, move_to.y, BWAPI::Colors::White);

	}

	struct snipe_enemy_info {
		unit* u;
		xy pos;
		double range;
		double damage_per_hit;
		int cooldown;
		double dpf;
		int tmp_in_range_after_frames;
	};
	a_vector<snipe_enemy_info> enemies;

	auto snipe = [&](auto& my_units, auto& enemy_units, bool target_spellcasters_only) {

		if (my_units.empty() || enemy_units.empty()) return false;

		//unit* front_unit = enemy_units.front();
		unit* front_unit = *enemy_units.begin();
		unit* target = front_unit;
		enemies.clear();
		double highest_cost = 0.0;
		unit* any_my_unit = my_units.front()->a->u;
		for (unit* u : enemy_units) {
			if (!u->visible || !u->detected || u->stasis_timer || u->invincible || u->type->is_non_usable) continue;
			if (u->type == unit_types::interceptor) continue;
			if (target_spellcasters_only && u->stats->energy == 0.0) continue;
			weapon_stats* mw = u->is_flying ? any_my_unit->stats->air_weapon : any_my_unit->stats->ground_weapon;
			//if (mw && diag_distance(u->pos - front_unit->pos) <= 32 * 3) {
			if (mw) {
				double cost = 0.0;
				for (auto* s : my_units) {
					double d = diag_distance(s->a->u->pos - u->pos);
					cost += d*d;
				}
				cost = 1.0 / cost;
				if (cost > highest_cost) {
					highest_cost = cost;
					target = u;
				}
//				double cost = u->type->total_minerals_cost + u->type->total_gas_cost;
//				cost += diag_distance(u->pos - any_my_unit->pos);
//				if (u->type->is_worker) cost += 100.0;
//				else if (!u->stats->air_weapon) cost -= 50.0;
//				else if (u->building) cost -= 100.0;
//				if (cost > highest_cost) {
//					highest_cost = cost;
//					target = u;
//				}
			}
			if (u->type == unit_types::bunker) {
				enemies.push_back({ u, u->pos, 32 * 6, 6 * 4, 15, (6 * 4) / 15.0 });
			} else {
				weapon_stats* w = any_my_unit->is_flying ? u->stats->air_weapon : u->stats->ground_weapon;
				if (!w) continue;
				double damage = combat::get_weapon_damage(u, w, any_my_unit);
				enemies.push_back({ u, u->pos, w->max_range + 64.0, damage, w->cooldown, damage / w->cooldown });
			}
		}
		if (enemies.empty()) return false;
		weapon_stats* w = target->is_flying ? any_my_unit->stats->air_weapon : any_my_unit->stats->ground_weapon;
		if (!w) return false;
//		if (!any_my_unit->is_flying && my_units.size() > 4) {
//			std::sort(my_units.begin(), my_units.end(), [&](auto* a, auto* b) {
//				double ad = units_distance(a->u, target);
//				double bd = units_distance(b->u, target);
//				return ad < bd;
//			});
//			my_units.resize(4);
//		}
		double damage = combat::get_weapon_damage(any_my_unit, w, target);
		double dpf = damage / w->cooldown;
		double target_hp = target->shields + target->hp;
		for (auto*& s : my_units) {
			double hp = s->a->u->hp;
			double shields = s->a->u->shields;
			double d = units_distance(s->a->u, target) - w->max_range;
			if (d < 0.0) d = 0.0;
			int move_frames = frames_to_reach(s->a->u, d);
			xy pos = s->a->u->pos;
			xy target_relpos = target->pos - pos;
			double rescale_mult = 1.0 / target_relpos.length() * d;
			xy attack_pos = xy((int)(target_relpos.x * rescale_mult), (int)(target_relpos.y * rescale_mult));
			xy attack_relpos = attack_pos - pos;
			for (auto& e : enemies) {
				xy e_relpos = e.pos - pos;
				double dot_product = e_relpos.x*attack_relpos.x + e_relpos.y*attack_relpos.y;
				double t = dot_product / d;
				if (t < 0.0) t = 0.0;
				else if (t > 1.0) t = 1.0;
				xy nearest_point = pos + xy((int)(attack_relpos.x * t), (int)(attack_relpos.y * t));
				double e_d = units_distance(pos, s->a->u->type, nearest_point, e.u->type);
				if (e_d <= e.range) e.tmp_in_range_after_frames = (int)(move_frames * t);
				else e.tmp_in_range_after_frames = 15 * 10;

				if (e.tmp_in_range_after_frames <= move_frames) {
					if (e_d < e.range) {
						double escape_d = e.range - e_d;
						int escape_f = frames_to_reach(s->a->u->stats, 0.0, escape_d);
						hp -= e.dpf * escape_f;
					}
				}
			}
			for (int f = 0; f < 15 * 20; f += 8) {
				double max_incoming_damage = 0.0;
				if (s->a->u->stats->shields) {
					for (auto& e : enemies) {
						if (f >= e.tmp_in_range_after_frames) {
							shields -= e.dpf * 8;
							if (e.damage_per_hit >= max_incoming_damage) max_incoming_damage = e.damage_per_hit;
						}
					}
					if (shields <= s->a->u->stats->shields / 8 - max_incoming_damage) {
						if (f <= move_frames + 8) s = nullptr;
						break;
					}
				} else {
					for (auto& e : enemies) {
						if (f >= e.tmp_in_range_after_frames) {
							hp -= e.dpf * 8;
							if (e.damage_per_hit >= max_incoming_damage) max_incoming_damage = e.damage_per_hit;
						}
					}
					if (hp <= s->a->u->stats->hp / 2 - max_incoming_damage) {
						if (f <= move_frames + 8) s = nullptr;
						break;
					}
				}
				if (f >= move_frames) {
					target_hp -= dpf * 8;
				}
			}
		}
		if (target_hp <= 0.0) {

			for (auto* s : my_units) {
				if (!s) continue;
				s->a->subaction = combat::combat_unit::subaction_fight;
				s->a->target = target;
				s->processed = true;
			}

			int pull_back_n = 0;
			for (auto* s : my_units) {
				if (!s) ++pull_back_n;
			}

			log(log_level_test, " sniping target %s with %d %s (pull back with %d)\n", target->type->name, my_units.size(), any_my_unit->type->name, pull_back_n);

			for (auto* s : my_units) {
				if (!s) continue;
				//game->drawLineMap(s->a->u->pos.x, s->a->u->pos.y, target->pos.x, target->pos.y, BWAPI::Colors::Green);
			}
			return true;
		}
		return false;
	};

	void update_queens() {

		if (is_recalling_frame) {
			if (current_frame - is_recalling_frame >= 30) is_recalling_frame = 0;
			if (is_recalling_pos != xy()) {
				for (auto& g : combat::groups) {
					for (auto* a : g.allies) {
						if (combat::entire_threat_area.test(grid::build_square_index(a->u->pos))) continue;
						if (diag_distance(a->u->pos - is_recalling_pos) > 32 * 30) continue;
						a->strategy_busy_until = current_frame + 15;
						a->subaction = combat::combat_unit::subaction_move;
						a->target_pos = is_recalling_pos;
					}
				}
			}
		}

		int n_observers = 0;
		int n_overlords = 0;
		for (auto i = queen_scouts.begin(); i != queen_scouts.end();) {
			if (!i->a) i = queen_scouts.erase(i);
			else {
				if (i->a->u->type == unit_types::observer) ++n_observers;
				else if (i->a->u->type == unit_types::overlord) ++n_overlords;
				++i;
			}
		}

		for (auto* a : combat::live_combat_units) {
			if (current_frame < a->strategy_busy_until) continue;
			//if (a->u->type == unit_types::queen || a->u->type == unit_types::corsair || a->u->type == unit_types::scout || (a->u->type == unit_types::observer && n_observers > my_units_of_type[unit_types::observer].size() / 2) || a->u->type == unit_types::arbiter || a->u->type == unit_types::shuttle || a->u->type == unit_types::mutalisk) {
			//if (a->u->type == unit_types::queen || a->u->type == unit_types::corsair || a->u->type == unit_types::scout || (a->u->type == unit_types::observer && n_observers > my_units_of_type[unit_types::observer].size() / 2) || a->u->type == unit_types::arbiter || a->u->type == unit_types::shuttle) {
			bool add = a->u->type == unit_types::corsair || a->u->type == unit_types::scout || (a->u->type == unit_types::observer && n_observers < my_units_of_type[unit_types::observer].size() / 2) || a->u->type == unit_types::arbiter || a->u->type == unit_types::shuttle;
			if (a->u->type == unit_types::overlord && n_overlords < std::min(my_units_of_type[unit_types::overlord].size() / 2, (size_t)3) && current_frame >= 15 * 60 * 6) add = true;
			if (a->u->type == unit_types::dropship && use_dropships) add = true;
			if (a->u->type == unit_types::science_vessel && players::opponent_player->race == race_zerg) add = true;
			//if (a->u->type == unit_types::wraith) add = true;
			if (add) {
				auto h = combat::request_control(a);
				if (h) {
					queen_scouts.emplace_back();
					auto& n = queen_scouts.back();
					n.h = std::move(h);
					n.a = a;
					//log(log_level_test, "new scouter found\n");
				}
				break;
			}
		}

		a_unordered_set<queen_scout*> has_grouped;

		for (auto& v : queen_scouts) {
			v.attack = false;
			v.processed = false;
		}

		a_vector<queen_scout*> allies;
		a_unordered_set<unit*> enemies;
		for (auto& v : queen_scouts) {
			if (!has_grouped.insert(&v).second) continue;
			if (v.almost_too_close_enemies.empty()) continue;
			if (v.a->u->type == unit_types::arbiter) continue;
			if (v.a->u->type == unit_types::queen) continue;
			if (!v.a->u->stats->air_weapon && !v.a->u->stats->ground_weapon) continue;
			allies.clear();
			enemies.clear();
			allies.push_back(&v);
			bool any_spellcasters = false;
			for (auto* e : v.almost_too_close_enemies) {
				if (e->dead || e->gone) continue;
				enemies.insert(e);
				if (e->stats->energy) any_spellcasters = true;
			}
			for (auto& v2 : queen_scouts) {
				if (v2.a->u->type == unit_types::arbiter) continue;
				if (v2.a->u->type == unit_types::queen) continue;
				if (!v2.a->u->stats->air_weapon && !v2.a->u->stats->ground_weapon) continue;
				if (has_grouped.count(&v2)) continue;
				bool add = false;
				for (auto* e : v2.almost_too_close_enemies) {
					if (enemies.count(e)) {
						add = true;
						break;
					}
				}
				if (!add) continue;
				has_grouped.insert(&v2);
				allies.push_back(&v2);
				for (auto* e : v2.almost_too_close_enemies) {
					if (e->dead || e->gone) continue;
					enemies.insert(e);
					if (e->stats->energy) any_spellcasters = true;
				}
			}
			if (any_spellcasters) {
				auto tmp = allies;
				if (!snipe(allies, enemies, true)) snipe(tmp, enemies, false);
			} else snipe(allies, enemies, false);
//			combat_eval::eval eval;
//			eval.max_frames = 15 * 10;
//			for (auto* v : allies) {
//				auto& c = eval.add_unit(v->a->u->stats, 0);
//				eval.set_unit_stuff(c, v->a->u);
//			}
//			for (unit* e : enemies) {
//				if (e->type == unit_types::bunker && e->is_completed) {
//					for (int i = 0; i < e->marines_loaded; ++i) {
//						eval.add_unit(units::get_unit_stats(unit_types::marine, e->owner), 1);
//					}
//				}
//				auto& c = eval.add_unit(e->stats, 1);
//				eval.set_unit_stuff(c, e);
//			}
//			eval.run();
//			if (eval.teams[0].score >= eval.teams[1].score * 1.5) {
//				log("attack with %d vs %d!\n", allies.size(), enemies.size());
//				for (auto* v : allies) v->attack = true;
//			}
		}
		
		if (current_frame - last_check_need_detection_at >= 30) {
			need_detection_at.clear();
			a_vector<queen_scout*> available_detectors;
			for (auto& v : queen_scouts) {
				if (v.a->u->type == unit_types::science_vessel) available_detectors.push_back(&v);
				else if (v.a->u->type == unit_types::observer) available_detectors.push_back(&v);
			}
			if (!available_detectors.empty()) {
				for (auto& g : combat::groups) {
					if (g.allies.empty()) continue;
					for (unit* u : g.enemies) {
						if (u->gone) continue;
						if (u->cloaked || u->type == unit_types::lurker) {
							xy pos = u->pos;
							bool taken = false;
							for (auto& v : need_detection_at) {
								if (diag_distance(pos - v.second) <= 32 * 10) {
									taken = true;
									break;
								}
							}
							if (!taken) {
								queen_scout* s = get_best_score(available_detectors, [&](queen_scout* s) {
									return diag_distance(s->a->u->pos - pos);
								});
								if (!s) break;
								find_and_erase(available_detectors, s);
								need_detection_at[s] = pos;
								if (available_detectors.empty()) break;
							}
						}
					}
					if (available_detectors.empty()) break;
				}
			}
		}
		for (auto& v : need_detection_at) {
			game->drawLineMap(v.first->a->u->pos.x, v.first->a->u->pos.y, v.second.x, v.second.y, BWAPI::Colors::Yellow);
		}

		for (auto& v : queen_scouts) {
			update_queen_scout(v);
		}

	}

	void harassment_task() {

		while (true) {

			double highest_dpf = 0.0;
			largest_enemy_army_pos = xy();
			n_attacking_tanks = 0;
			n_attacking_science_vessels = 0;
			science_vessel_targets.clear();
			scout_targets.clear();
			for (auto& g : combat::groups) {
				if (g.is_aggressive_group || g.is_defensive_group) continue;
				for (unit* e : g.enemies) {
					if (e->type == unit_types::siege_tank_tank_mode || e->type == unit_types::siege_tank_siege_mode) {
						++n_attacking_tanks;
					}
					if (e->type == unit_types::science_vessel) {
						++n_attacking_science_vessels;
					}
				}
				if (g.allies.size() >= 2) {
					for (unit* e : g.enemies) {
						bool is_target = false;
						if (e->type == unit_types::siege_tank_tank_mode || e->type == unit_types::siege_tank_siege_mode) is_target = true;
						if (e->cloaked) is_target = true;
						if (e->type == unit_types::arbiter) is_target = true;
						if (e->type == unit_types::lurker) is_target = true;
						if (!is_target) continue;
						science_vessel_targets.insert(e);
					}
				}
				if (g.ground_dpf + g.air_dpf >= highest_dpf) {
					largest_enemy_army_pos = g.enemies.front()->pos;
					highest_dpf = g.ground_dpf + g.air_dpf;
				}
				for (auto* a : g.allies) {
					if (a->u->type == unit_types::shuttle || a->u->type == unit_types::reaver) {
						unit* best_target = nullptr;
						double best_d = std::numeric_limits<double>::infinity();
						for (unit* e : g.enemies) {
							if (e->stats->air_weapon) {
								double d = sc_distance(a->u->pos - e->pos);
								if (d < best_d) {
									best_d = d;
									best_target = e;
								}
							}
						}
						if (best_target) {
							scout_targets.insert(best_target);
						}
					}
				}
			}
			log("n_attacking_tanks is %d\n", n_attacking_tanks);

			update_groups();

			multitasking::sleep(4);
		}

	}

	void queen_task() {

		while (true) {

			update_queens();

			multitasking::sleep(1);
		}
	}

	void queen_drawing_task() {

		while (true) {

			for (auto& s : queen_scouts) {

				int max_value = 1;
				for (auto& v : s.costgrid) {
					if (v > max_value) max_value = v;
				}

				xy screen_pos = bwapi_pos(game->getScreenPosition());

				for (int y = screen_pos.y & -32; y < screen_pos.y + 400; y += 32) {
					for (int x = screen_pos.x & -32; x < screen_pos.x + 640; x += 32) {
						if ((size_t)x >= (size_t)grid::map_width || (size_t)y >= (size_t)grid::map_height) break;

						size_t index = grid::build_square_index(xy(x, y));
						int v = s.costgrid[index];
						int c = v * 255 / max_value;
						game->drawBoxMap(x + 1, y + 1, x + 32 - 1, y + 32 - 1, BWAPI::Color(c, 0, 0));
					}
				}
			}


			multitasking::sleep(1);
		}

	}

	void init() {

		multitasking::spawn(harassment_task, "harassment");

		multitasking::spawn(queen_task, "queen");

		//multitasking::spawn(0.1, queen_drawing_task, "queen drawing");

	}


}
