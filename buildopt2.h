
namespace buildopt2 {

using namespace buildpred;

a_multimap<int, unit_type*> seen_history;
a_map<int, a_unordered_map<unit_type*, int>> seen_unit_counts_at;
a_map<int, a_unordered_map<unit_type*, int>> built_unit_counts_at;
a_map<int, a_unordered_map<unit_type*, int>> dead_unit_counts_at;

a_unordered_map<unit_type*, int> live_unit_counts;
a_unordered_map<unit_type*, int> seen_unit_counts;
a_unordered_map<unit_type*, int> dead_unit_counts;
a_unordered_map<unit_type*, int> destroyed_unit_counts;

a_multimap<int, unit_type*> unit_destroyed_at;

auto add_unit_count = [&](unit_type*ut, int frame, auto&unit_counts) {
	if (unit_counts.empty()) {
		a_unordered_map<unit_type*, int> new_entry;
		new_entry[ut] = 1;
		unit_counts[frame] = std::move(new_entry);
	} else {
		auto i = --unit_counts.end();
		while (i->first > frame) {
			++i->second[ut];
			if (i == unit_counts.begin()) return;
			--i;
		}
		if (frame - i->first < 15) {
			++i->second[ut];
		} else {
			a_unordered_map<unit_type*, int> new_entry = i->second;
			++new_entry[ut];
			unit_counts[frame] = std::move(new_entry);
		}
	}
};

xy op_spawn_loc;

a_unordered_map<unit*, unit_type*> last_seen_unit_type;

int my_lost_worker_count = 0;

void on_new_unit(unit*u) {
	if (!u->owner->is_enemy) return;
	unit_type*ut = u->type;
	if (ut == unit_types::siege_tank_siege_mode) ut = unit_types::siege_tank_tank_mode;
	if (ut->is_non_usable) return;
	seen_history.emplace(current_frame, ut);
	add_unit_count(ut, current_frame, seen_unit_counts_at);
	//double distance_from_home = unit_pathing_distance(u, op_spawn_loc);
	double distance_from_home = get_best_score_value(enemy_buildings, [&](unit*e) {
		return units_pathing_distance(ut, u, e);
	});
	if (distance_from_home >= 32 * 64) {
		distance_from_home = unit_pathing_distance(u, op_spawn_loc);
		if (distance_from_home >= 32 * 64) distance_from_home = 32 * 32;
	}
	int age = (int)(distance_from_home / std::max(u->stats->max_speed, 1.0));
	if (u->is_completed && !u->is_morphing) age += ut->build_time;
	add_unit_count(ut, current_frame - age, built_unit_counts_at);
	++seen_unit_counts[ut];
	++live_unit_counts[ut];
	last_seen_unit_type[u] = ut;
}
void on_type_change(unit*u, unit_type*old_type) {
	if (!u->owner->is_enemy) return;
	unit_type*ut = u->type;
	if (ut == unit_types::siege_tank_siege_mode) ut = unit_types::siege_tank_tank_mode;
	if (ut->is_non_usable) return;
	unit_type*&last_seen_type = last_seen_unit_type[u];
	if (last_seen_type == ut) return;
	if (last_seen_type) {
		add_unit_count(last_seen_type, current_frame, dead_unit_counts_at);
		++dead_unit_counts[last_seen_type];
		--live_unit_counts[last_seen_type];
	}
	int age = 0;
	if (u->is_completed && !u->is_morphing) age += ut->build_time;
	add_unit_count(ut, current_frame - age, built_unit_counts_at);
	++seen_unit_counts[ut];
	++live_unit_counts[ut];
	last_seen_type = ut;
}
void on_destroy(unit*u) {
	if (u->owner == players::my_player && u->type->is_worker) ++my_lost_worker_count;
	if (!u->owner->is_enemy) return;
	unit_type*ut = u->type;
	if (ut == unit_types::siege_tank_siege_mode) ut = unit_types::siege_tank_tank_mode;
	if (ut->is_non_usable) return;
	++dead_unit_counts[ut];
	++destroyed_unit_counts[ut];
	--live_unit_counts[ut];
	add_unit_count(ut, current_frame, dead_unit_counts_at);

	unit_destroyed_at.emplace(current_frame, ut);
}

void add_nearest_base(state& st, xy pos) {
	resource_spots::spot* s = nullptr;
	double sd;
	for (auto& v : resource_spots::spots) {
		double d = diag_distance(v.pos - pos);
		if (!s || d < sd) {
			sd = d;
			s = &v;
		}
	}
	if (s) add_base(st, *s);
}

state new_state(xy start_pos, int race) {
	state initial_state;
	initial_state.frame = 0;
	initial_state.minerals = 50;
	initial_state.gas = 0;
	initial_state.used_supply = { 0, 0, 0 };
	initial_state.max_supply = { 0, 0, 0 };
	add_nearest_base(initial_state, start_pos);
	unit_type* worker_type;
	unit_type* cc_type;
	if (race == race_terran) {
		worker_type = unit_types::scv;
		cc_type = unit_types::cc;
		add_unit_and_supply(initial_state, unit_types::cc);
		add_unit_and_supply(initial_state, unit_types::scv);
		add_unit_and_supply(initial_state, unit_types::scv);
		add_unit_and_supply(initial_state, unit_types::scv);
		add_unit_and_supply(initial_state, unit_types::scv);
	} else if (race == race_protoss) {
		worker_type = unit_types::probe;
		cc_type = unit_types::nexus;
		add_unit_and_supply(initial_state, unit_types::nexus);
		add_unit_and_supply(initial_state, unit_types::probe);
		add_unit_and_supply(initial_state, unit_types::probe);
		add_unit_and_supply(initial_state, unit_types::probe);
		add_unit_and_supply(initial_state, unit_types::probe);
	} else {
		worker_type = unit_types::drone;
		cc_type = unit_types::hatchery;
		add_unit_and_supply(initial_state, unit_types::hatchery);
		add_unit_and_supply(initial_state, unit_types::drone);
		add_unit_and_supply(initial_state, unit_types::drone);
		add_unit_and_supply(initial_state, unit_types::drone);
		add_unit_and_supply(initial_state, unit_types::drone);
		add_unit_and_supply(initial_state, unit_types::overlord);
	}
	return initial_state;
}

auto get_next_base(state& st) {
	unit_type* worker_type = unit_types::drone;
	unit_type* cc_type = unit_types::hatchery;
	if (count_units(st, unit_types::scv)) {
		worker_type = unit_types::scv;
		cc_type = unit_types::cc;
	} else if (count_units(st, unit_types::probe)) {
		worker_type = unit_types::probe;
		cc_type = unit_types::nexus;
	}
	a_vector<refcounted_ptr<resource_spots::spot>> available_bases;
	for (auto& s : resource_spots::spots) {
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
		double score = 0;
		score += unit_pathing_distance(worker_type, s->cc_build_pos, st.bases.front().s->cc_build_pos);
		double res = 0;
		bool has_gas = false;
		for (auto&r : s->resources) {
			res += r.u->resources;
			if (r.u->type->is_gas) has_gas = true;
		}
		score -= (has_gas ? res : res / 4) / 10;
		return score;
	}, std::numeric_limits<double>::infinity());
}

a_vector<buildpred::state> fit_unit_counts(buildpred::state st, const a_map<int, a_unordered_map<unit_type*, int>>& unit_counts, const a_multimap<int, unit_type*>& destroyed_at, int end_frame) {
	using namespace buildpred;

	unit_type*worker_type;
	if (!st.units[unit_types::scv].empty()) worker_type = unit_types::scv;
	else if (!st.units[unit_types::probe].empty()) worker_type = unit_types::probe;
	else worker_type = unit_types::drone;

	unit_type*cc_type;
	if (!st.units[unit_types::scv].empty()) cc_type = unit_types::cc;
	else if (!st.units[unit_types::probe].empty()) cc_type = unit_types::nexus;
	else cc_type = unit_types::hatchery;

	int race;
	if (!st.units[unit_types::scv].empty()) race = race_terran;
	else if (!st.units[unit_types::probe].empty()) race = race_protoss;
	else race = race_zerg;

	int opponent_race = race_terran;
	if (!unit_counts.empty()) {
		auto i = --unit_counts.end();
		if (i->second.find(unit_types::scv) != i->second.end()) opponent_race = race_terran;
		if (i->second.find(unit_types::probe) != i->second.end()) opponent_race = race_protoss;
		if (i->second.find(unit_types::drone) != i->second.end()) opponent_race = race_zerg;
	}

	auto get_next_base = [&]() {
		a_vector<refcounted_ptr<resource_spots::spot>> available_bases;
		for (auto&s : resource_spots::spots) {
			//if (grid::get_build_square(s.cc_build_pos).building) continue;
			bool okay = true;
			for (auto&v : st.bases) {
				if ((resource_spots::spot*)v.s == &s) okay = false;
			}
			if (!okay) continue;
			//if (!build::build_is_valid(grid::get_build_square(s.cc_build_pos), cc_type)) continue;
			available_bases.push_back(&s);
		}
		return get_best_score(available_bases, [&](resource_spots::spot*s) {
			double score = unit_pathing_distance(worker_type, s->cc_build_pos, st.bases.front().s->cc_build_pos);
			double res = 0;
			bool has_gas = false;
			for (auto&r : s->resources) {
				res += r.u->resources;
				if (r.u->type->is_gas) has_gas = true;
			}
			score -= (has_gas ? res : res / 4) / 10;
			return score;
		}, std::numeric_limits<double>::infinity());
	};

	auto count_units_plus_production_plus_morphed = [](state&st, unit_type*ut) {
		int r = count_units_plus_production(st, ut);
		auto add = [&](unit_type*ut) {
			r += count_units_plus_production(st, ut);
		};
		if (ut == unit_types::hive) {
			add(unit_types::lair);
			add(unit_types::hatchery);
		}
		if (ut == unit_types::lair) add(unit_types::hatchery);
		if (ut == unit_types::creep_colony) {
			add(unit_types::sunken_colony);
			add(unit_types::spore_colony);
		}
		if (ut == unit_types::spire) add(unit_types::greater_spire);
		if (ut == unit_types::mutalisk) {
			add(unit_types::guardian);
			add(unit_types::devourer);
		}
		if (ut == unit_types::hydralisk) add(unit_types::lurker);
		return r;
	};

	a_vector<state> all_states;

	bool satisfied = false;
	
// 	while (st.frame < v.first) {
// 		all_states.push_back(st);
// 		//if (!depbuild(st, state(st), worker_type)) break;
// 		if (!nodelay(st, worker_type, [&](state&st) {
// 			if (can_expand) return depbuild(st, state(st), cc_type);
// 			else return false;
// 		})) break;
// 	}

	st.auto_build_hatcheries = true;
	all_states.push_back(st);

	a_vector<std::pair<build_type, int>> queue;
	a_unordered_map<unit_type*, int> queued_ut;
	a_unordered_map<build::build_type*, int> queued_bt;

	auto base_all_states = all_states;
	int last_fail_frame = 0;
	
	int passes = 4;

	bool verbose_logging = false;

	//a_vector<state> best_fail_add_states;
	int best_fail_pass = 0;
	int best_fail_frame = 0;
	double best_fail_power_required = 0.0;
	size_t best_fail_all_states_size = 0;
	int best_fail_pass_active = -1;

	for (int pass = 0; pass < passes; ++pass) {
		if (pass == 1) pass = 2;

		all_states = base_all_states;

		if (verbose_logging) {
			st = all_states.back();
			log(log_level_test, "pass %d entry -- \n", pass);
			log(log_level_test, "frame %d, minerals %g gas %g supply %g/%g\n", st.frame, st.minerals, st.gas, st.used_supply[race], st.max_supply[race]);
			log(log_level_test, " partial build order- ");
			for (auto&v : st.produced) {
				build_type bt = std::get<0>(v.second);
				log(log_level_test, " %s", bt->unit ? short_type_name(bt->unit) : bt->name);
			}
			log(log_level_test, "\n");
		}

		satisfied = false;
		while (!all_states.empty()) {
			st = all_states.back();

			if (verbose_logging) {
				log(log_level_test, "pass %d -- \n", pass);
				log(log_level_test, "frame %d, minerals %g gas %g supply %g/%g\n", st.frame, st.minerals, st.gas, st.used_supply[race], st.max_supply[race]);
				log(log_level_test, " partial build order- ");
				for (auto&v : st.produced) {
					build_type bt = std::get<0>(v.second);
					log(log_level_test, " %s", bt->unit ? short_type_name(bt->unit) : bt->name);
				}
				log(log_level_test, "\n");
			}

			bool failed = true;
			bool can_expand = get_next_base();
			a_vector<state> add_states;
			int prev_frame = st.frame;
			auto destroyed_i = destroyed_at.lower_bound(st.frame);
			int last_pass_frame = 0;
			bool reset = false;
			while (st.frame < end_frame) {

				if (reset) {
					reset = false;
					st = all_states.back();
					can_expand = get_next_base();
					add_states.clear();
					prev_frame = st.frame;
					destroyed_i = destroyed_at.lower_bound(st.frame);
				}

				multitasking::yield_point();
				add_states.push_back(st);

				std::function<bool(state&st)> q = [](state&st) {
					return false;
				};
				auto nd = [&](build_type_autocast bt) {
					q = [q, bt](state&st) {
						return nodelay(st, bt, q);
					};
				};
				auto mp = [&](build_type_autocast bt) {
					q = [q, bt](state&st) {
						return maxprod(st, bt, q);
					};
				};

				int worker_count = count_units_plus_production(st, worker_type);
				if (worker_count < 60) {
					//if (worker_count < (int)st.bases.size() * 24) nd(worker_type);
					nd(cc_type);
					nd(worker_type);
				} else {
					if (worker_type == unit_types::scv) nd(unit_types::marine);
					else if (worker_type == unit_types::probe) nd(unit_types::zealot);
					else if (worker_type == unit_types::drone) nd(unit_types::zergling);
				}

				bool force_pass = false;
				failed = false;
				auto next_i = unit_counts.upper_bound(st.frame);
				if (next_i != unit_counts.begin()) {
					auto prev = next_i;
					--prev;

					for (auto&v : prev->second) {
						unit_type*ut = v.first;
						if (ut->is_addon) continue;
						//if (ut == unit_types::siege_tank_siege_mode) ut = unit_types::siege_tank_tank_mode;
						if (ut == unit_types::siege_tank_siege_mode) xcept("sieged tank");
						if (count_units_plus_production_plus_morphed(st, ut) + st.units_lost[ut] < v.second) {
							if (verbose_logging) {
								log(log_level_test, "%s failed - %d / %d\n", v.first->name, count_units_plus_production(st, ut), v.second);
								log(log_level_test, "frame %d, minerals %g gas %g supply %g/%g\n", st.frame, st.minerals, st.gas, st.used_supply[race], st.max_supply[race]);
								log(log_level_test, " partial build order- ");
								for (auto&v : st.produced) {
									build_type bt = std::get<0>(v.second);
									log(log_level_test, " %s", bt->unit ? short_type_name(bt->unit) : bt->name);
								}
								log(log_level_test, "\n");
							}
							failed = true;
							break;
						}
					}
					
					if (failed && (st.used_supply[race_terran] >= 190 || st.used_supply[race_protoss] >= 190 || st.used_supply[race_zerg] >= 190)) {
						if (verbose_logging) log(log_level_test, "maxed out, breaking\n");
						failed = false;
						break;
					}
					if (failed) {
						if (!last_fail_frame) {
							if (verbose_logging) log(log_level_test, "last_fail_frame set to %d\n", prev->first);
							last_fail_frame = prev->first;
							break;
						} else if (prev->first > last_fail_frame) {
							if (pass != passes - 1) {
								if (verbose_logging) log(log_level_test, "passed previous fail at %d, but failed again\n", last_fail_frame);
								if (verbose_logging) log(log_level_test, "last_fail_frame set to %d\n", prev->first);
								last_fail_frame = prev->first;
								break;
							}
						}
						if (pass == best_fail_pass_active) {
							log(log_level_test, "best fail pass, not breaking\n");
							failed = false;
							force_pass = true;
						} else if (pass != passes - 1) {
							break;
						} else {
							if (all_states.size() == 1) {
								if (best_fail_frame) {
									failed = false;
									force_pass = true;
									log(log_level_test, "last pass, restoring best fail\n");

									all_states = base_all_states;
									all_states.resize(best_fail_all_states_size);
									//add_states = std::move(best_fail_add_states);
									pass = best_fail_pass;
									best_fail_pass_active = pass;

									best_fail_frame = 0;
									reset = true;
									continue;
								} else {
									failed = false;
									force_pass = true;
									log(log_level_test, "last pass, not breaking\n");
								}
							}
// 							if (last_fail_frame && (st.frame <= last_fail_frame - 15 * 60 * 4 || all_states.size() == 1)) {
// 								failed = false;
// 								force_pass = true;
// 								log(log_level_test, "last pass, not breaking\n");
// 							} else break;
						}
					} else last_pass_frame = prev->first;
					//if (last_fail_frame && st.frame > last_fail_frame) {
					if (!force_pass && !failed && last_fail_frame && (prev->first >= last_fail_frame || st.frame >= last_fail_frame)) {

						if (verbose_logging) {
							log(log_level_test, "frame %d passed previous fail at %d, setting pass to 0\n", st.frame, last_fail_frame);
							log(log_level_test, "frame %d, minerals %g gas %g supply %g/%g\n", st.frame, st.minerals, st.gas, st.used_supply[race], st.max_supply[race]);
							log(log_level_test, "at %d - ", st.frame);
							for (auto&v : st.units) {
								if (!v.first || !v.second.size()) continue;
								log(log_level_test, " %dx%s", v.second.size(), short_type_name(v.first));
							}
							log(log_level_test, "\n");
							log(log_level_test, "  inprod - ");
							for (auto&v : st.production) {
								log(log_level_test, " %s", v.second->unit ? short_type_name(v.second->unit) : v.second->name);
							}
							log(log_level_test, "\n");
						}
						last_fail_frame = 0;
						pass = 0;
					}
				}
				if (last_fail_frame && (count_units_plus_production(st, unit_types::pylon) || worker_count >= 8)) {
					queue.clear();
					
					queued_ut.clear();
					for (auto i = next_i; i != unit_counts.end(); ++i) {
						//if (i->first >= st.frame + 15 * 60 * 8) break;
						if (i != next_i && i->first > last_fail_frame) break;
						for (auto&v : i->second) {
							unit_type*ut = v.first;
							if (ut->is_addon) continue;
							//log(log_level_test, "i frame %d, %s is %d / %d\n", i->first, ut->name, count_units_plus_production_plus_morphed(st, ut), v.second);
							//if (ut == unit_types::siege_tank_siege_mode) ut = unit_types::siege_tank_tank_mode;
							if (ut == unit_types::siege_tank_siege_mode) xcept("sieged tank");
// 								if (ut == unit_types::dragoon) {
// 									if (!has_or_is_upgrading(st, upgrade_types::singularity_charge)) {
// 										queue.emplace_back(build_type(upgrade_types::singularity_charge), 0);
// 									}
// 								}
							if (ut == unit_types::larva) DebugBreak();
							int needed = v.second - count_units_plus_production_plus_morphed(st, ut) - queued_ut[ut];
							if (needed > 0) {
								if (!ut->is_building && ut->builder_type != unit_types::larva && !ut->is_worker && needed > 1) {
									int frames = i->first - st.frame - 15 * 4;
									if (frames > ut->build_time) {
										int built = 0;
										bool requires_addon = false;
										if (ut == unit_types::siege_tank_tank_mode) requires_addon = true;
										if (ut == unit_types::dropship || ut == unit_types::science_vessel || ut == unit_types::battlecruiser || ut == unit_types::valkyrie) requires_addon = true;
										for (auto&v : st.units[ut->builder_type]) {
											int f = frames;
											if (v.busy_until > st.frame) f -= v.busy_until - st.frame;
											if (f >= ut->build_time) built += f / ut->build_time;
										}
										if (built < needed) {
											for (auto&v : st.production) {
												if (v.second->unit == ut->builder_type) {
													int f = frames;
													f -= v.first - st.frame;
													if (f >= ut->build_time) built += f / ut->build_time;
												}
											}
										}
										if (built >= needed) queue.emplace_back(build_type(ut), 0);
										else queue.emplace_back(build_type(ut), 1);
									} else {
										queue.emplace_back(build_type(ut), 1);
									}
								} else queue.emplace_back(build_type(ut), 0);

								++queued_ut[ut];

// 									if (!v.first->is_building && v.first->builder_type != unit_types::larva) mp(ut);
// 									else nd(ut);

// 									if (pass == 2 && !v.first->is_building) queue.emplace_back(build_type(ut), 1);
// 									else queue.emplace_back(build_type(ut), 0);
								// 							if (pass == 2 && !v.first->is_building) mp(ut);
								// 							else nd(ut);
							}
						}
					}
					for (auto i = queue.rbegin(); i != queue.rend(); ++i) {
						if (verbose_logging) log(log_level_test, "queue %s\n", i->first->name);
						if (i->second == 1) mp(i->first);
						else nd(i->first);
					}
				}

				//int cc_count = count_units_plus_production(st, cc_type);
				int cc_count = count_units(st, cc_type);
				prev_frame = st.frame;
				if (!q(st)) break;

				for (; destroyed_i != destroyed_at.end() && destroyed_i->first < st.frame; ++destroyed_i) {
					unit_type* ut = destroyed_i->second;
					if (!st.units[ut].empty()) {
						++st.units_lost[ut];
						rm_unit_and_supply(st, ut);
					}
				}

				//if (count_units_plus_production(st, cc_type) > cc_count) {
				if (count_units(st, cc_type) > cc_count) {
					auto s = get_next_base();
					if (s) add_base(st, *s);
					can_expand = get_next_base();
				}
			}

			if (!failed) {
				log(log_level_test, "satisfied (at frame %d)!\n", st.frame);
				log(log_level_test, "frame %d, minerals %g gas %g supply %g/%g\n", st.frame, st.minerals, st.gas, st.used_supply[race], st.max_supply[race]);
				log(log_level_test, " partial build order- ");
				for (auto&v : st.produced) {
					build_type bt = std::get<0>(v.second);
					log(log_level_test, " %s", bt->unit ? short_type_name(bt->unit) : bt->name);
				}
				log(log_level_test, "\n");
				satisfied = true;
				for (auto&v : add_states) {
					all_states.push_back(std::move(v));
				}
				break;
			} else {
				if (pass == 0) {
					for (auto&v : add_states) {
						all_states.push_back(std::move(v));
					}
					base_all_states = std::move(all_states);
					break;
				} else all_states.pop_back();
			}
		}
		if (satisfied) break;
	}

	if (!satisfied) {
		log(log_level_test, "exhausted all states, failed to satisfy\n");
		log(log_level_test, "frame %d, minerals %g gas %g supply %g/%g\n", st.frame, st.minerals, st.gas, st.used_supply[race], st.max_supply[race]);
// 		st = std::move(prev_st);
// 		all_states = std::move(prev_all_states);
	}

	log(log_level_test, "final all states -- \n");
	for (auto&st : all_states) {
		log(log_level_test, "at %d - ", st.frame);
		for (auto&v : st.units) {
			if (!v.first || !v.second.size()) continue;
			log(log_level_test, " %dx%s", v.second.size(), short_type_name(v.first));
		}
		log(log_level_test, "\n");
		log(log_level_test, "  inprod - ");
		for (auto&v : st.production) {
			log(log_level_test, " %s", v.second->unit ? short_type_name(v.second->unit) : v.second->name);
		}
		log(log_level_test, "\n");
	}

	log(log_level_test, " frame %d -- satisfied is %d\n", st.frame, satisfied);
	for (auto&v : st.units) {
		if (!v.first || !v.second.size()) continue;
		log(log_level_test, " %dx%s", v.second.size(), short_type_name(v.first));
	}
	log(log_level_test, "\n");
	log(log_level_test, " build order - ");
	for (auto&v : st.produced) {
		build_type bt = std::get<0>(v.second);
		log(log_level_test, " %s", bt->unit ? short_type_name(bt->unit) : bt->name);
	}
	log(log_level_test, "\n");

// 	if (!satisfied) {
// 		xcept("failed :(");
// 	}

	return all_states;
}

struct op_state {
	std::function<bool(state&st)> func;
	state begin_state;
	state end_state;
};

a_vector<op_state> opponent_states;

void test_start(xy start_pos, int race) {
	
	auto initial_state = new_state(start_pos, race);
	
	auto& st = initial_state;
	
	unit_type* worker_type = unit_types::drone;
	unit_type* cc_type = unit_types::hatchery;
	
	auto func = [&](state& st) {
		//return false;
		if (count_units_plus_production(st, worker_type) >= 12) {
			if (count_units_plus_production(st, cc_type) == 1) return depbuild(st, state(st), cc_type);
		}
		return depbuild(st, state(st), worker_type);
	};
	
	int end_frame = 15 * 60 * 10;
	
	while (st.frame < end_frame) {
		int cc_count = count_units_plus_production(st, cc_type);
		if (!func(st)) break;
		if (count_units_plus_production(st, cc_type) > cc_count) {
			auto s = get_next_base(st);
			if (s) {
				add_base(st, *s);
				log(log_level_test, "expanded\n");
			}
		}
	}
	while (st.frame < end_frame) {
		advance(st, nullptr, end_frame, false, false);
	}
	
	log(log_level_test, "frame %d, minerals %g gas %g supply %g/%g\n", st.frame, st.minerals, st.gas, st.used_supply[race], st.max_supply[race]);
	
	log(log_level_test, "at %d - ", st.frame);
	for (auto&v : st.units) {
		if (!v.first || !v.second.size()) continue;
		log(log_level_test, " %dx%s", v.second.size(), short_type_name(v.first));
	}
	log(log_level_test, "\n");
	log(log_level_test, "  inprod - ");
	for (auto&v : st.production) {
		log(log_level_test, " %s", v.second->unit ? short_type_name(v.second->unit) : v.second->name);
	}
	log(log_level_test, "\n");
	
}

double enemy_army_supply = 0.0;

void update() {
	
	auto& op_unit_counts_at = built_unit_counts_at;
	
	log(log_level_test, " op_unit_counts_at is - \n");
	for (auto&v : op_unit_counts_at) {
		log(log_level_test, "at %d\n", v.first);
		for (auto&v2 : v.second) {
			log(log_level_test, "  %dx%s\n", v2.second, v2.first->name);
		}
	}
	
	xy op_start_loc = get_best_score(start_locations, [&](xy pos) {
		return diag_distance(pos - my_start_location);
	});
	
	auto op_st = new_state(op_start_loc, players::opponent_player->race);

	auto op_states = fit_unit_counts(op_st, op_unit_counts_at, unit_destroyed_at, current_frame);

	//test_start(my_start_location, players::my_player->race);
	return;
	
	for (auto& v : opponent_states) {
		while (v.begin_state.frame < current_frame - 15 * 60 * 10) {
			if (!v.func(v.begin_state)) break;
		}
		v.end_state = v.begin_state;
		while (v.end_state.frame < current_frame) {
			if (!v.func(v.end_state)) break;
		}
		
		auto& st = v.end_state;
		int race = players::opponent_player->race;
		log(log_level_test, "frame %d, minerals %g gas %g supply %g/%g\n", st.frame, st.minerals, st.gas, st.used_supply[race], st.max_supply[race]);
		
		log(log_level_test, "at %d - ", st.frame);
		for (auto&v : st.units) {
			if (!v.first || !v.second.size()) continue;
			log(log_level_test, " %dx%s", v.second.size(), short_type_name(v.first));
		}
		log(log_level_test, "\n");
		log(log_level_test, "  inprod - ");
		for (auto&v : st.production) {
			log(log_level_test, " %s", v.second->unit ? short_type_name(v.second->unit) : v.second->name);
		}
		log(log_level_test, "\n");
		
		enemy_army_supply = 0.0;
		for (auto& v : st.units) {
			if (!v.first->is_worker) {
				enemy_army_supply += v.first->required_supply * v.second.size();
			}
		}
	}
	
}

unit_type* get_unit_type(const a_string& name) {
	for (auto& v : units::unit_type_container) {
		if (v.name == name) {
			return &v;
		}
	}
	return nullptr;
}

std::array<a_vector<a_map<int, a_unordered_map<unit_type*, int>>>, 3> unit_counts_per_race;

void test() {
	auto jv = strategy::read_json(format("bwapi-data/read/buildopt-unit_counts-%s.txt", strategy::escape_filename(strategy::opponent_name)));
	if (jv.type == json_value::t_null) jv = strategy::read_json(format("bwapi-data/write/buildopt-unit_counts-%s.txt", strategy::escape_filename(strategy::opponent_name)));

	for (int i = 0; i < 3; ++i) {
		unit_counts_per_race[i].clear();
		for (auto&jv_list_v : jv[i].vector) {
			unit_counts_per_race[i].emplace_back();
			auto&uc = unit_counts_per_race[i].back();
			for (auto& jv_v : jv_list_v.map) {
				int f = strtol(jv_v.first.c_str(), nullptr, 0);
				auto&v = uc[f];
				for (auto& jv_v2 : jv_v.second.map) {
					unit_type* ut = get_unit_type(jv_v2.first);
					if (ut) v[ut] = jv_v2.second;
				}
			}
		}
	}
	
	const auto& possible_unit_counts = unit_counts_per_race[players::opponent_player->race];
	
	while (true) {
		
		a_map<int, a_unordered_map<unit_type*, int>> op_unit_counts_at;
		
		if (!possible_unit_counts.empty()) {
			op_unit_counts_at = possible_unit_counts[0];
		}
		
		log(log_level_test, " op_unit_counts_at is - \n");
		for (auto&v : op_unit_counts_at) {
			log(log_level_test, "at %d\n", v.first);
			for (auto&v2 : v.second) {
				log(log_level_test, "  %dx%s\n", v2.second, v2.first->name);
			}
		}
		
		xy op_start_loc = get_best_score(start_locations, [&](xy pos) {
			return diag_distance(pos - my_start_location);
		});
		
		auto op_st = new_state(op_start_loc, players::opponent_player->race);
	
		auto op_states = fit_unit_counts(op_st, op_unit_counts_at, unit_destroyed_at, current_frame + 15 * 30);
		
		op_st = op_states.back();
		
		strategy::optc::e_vulture_count = count_units_plus_production(op_st, unit_types::vulture);
		strategy::optc::e_goliath_count = count_units_plus_production(op_st, unit_types::goliath);
		strategy::optc::e_tank_count = count_units_plus_production(op_st, unit_types::siege_tank_tank_mode) + count_units_plus_production(op_st, unit_types::siege_tank_siege_mode);
		
		strategy::optc::e_army_supply = 0.0;
		strategy::optc::e_biological_army_supply = 0.0;
		strategy::optc::e_worker_count = 0;
		for (auto& v : op_st.units) {
			if (!v.first->is_worker) {
				strategy::optc::e_army_supply += v.first->required_supply * v.second.size();
				if (v.first->is_biological) strategy::optc::e_biological_army_supply += v.first->required_supply * v.second.size();
			} else strategy::optc::e_worker_count += v.second.size();
		}
		
		multitasking::sleep(30);
		
	}
	
}

void buildopt2_task() {
	multitasking::sleep(15 * 30);
	
	//test();
	
//	xy op_start_loc = get_best_score(start_locations, [&](xy pos) {
//		return diag_distance(pos - my_start_location);
//	});
	
//	unit_type* worker_type;
//	if (players::opponent_player->race == race_zerg) worker_type = unit_types::drone;
//	else if (players::opponent_player->race == race_protoss) worker_type = unit_types::probe;
//	else worker_type = unit_types::scv;
	
//	op_state ops;
//	ops.begin_state = new_state(op_start_loc, players::opponent_player->race);
//	ops.func = [&](state& st) {
//		return nodelay(st, worker_type, [&](state& st) {
//			return maxprod1(st, unit_types::marine);
//		});
//	};
//	opponent_states.push_back(ops);

//	while (true) {
		
//		update();
		
//		multitasking::sleep(30);
		
//	}
	
}


void init() {
	
	units::on_new_unit_callbacks.push_back(on_new_unit);
	units::on_type_change_callbacks.push_back(on_type_change);
	units::on_destroy_callbacks.push_back(on_destroy);
	
	strategy::on_end_funcs.push_back([](bool won) {

		json_value jv;

		auto& unit_counts_vec = unit_counts_per_race[players::opponent_player->race];
		unit_counts_vec.push_back(built_unit_counts_at);
		//if (unit_counts_vec.size() > 8) unit_counts_vec.erase(unit_counts_vec.begin());
		while (unit_counts_vec.size() > 1) unit_counts_vec.erase(unit_counts_vec.begin());

		for (int i = 0; i < 3; ++i) {
			auto&vec = unit_counts_per_race[i];
			for (size_t i2 = 0; i2 < vec.size(); ++i2) {
				for (auto& v : vec[i2]) {
					auto& jv_v = jv[i][i2][format("%d", v.first)];
					for (auto& v2 : v.second) {
						jv_v[v2.first->name] = v2.second;
					}
				}
			}
		}

		strategy::write_json(format("bwapi-data/write/buildopt-unit_counts-%s.txt", strategy::escape_filename(strategy::opponent_name)), jv);


	});
	
	multitasking::spawn(buildopt2_task, "buildopt2");
	
}


}
