

namespace buildopt {
;

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

buildpred::state get_initial_state(resource_spots::spot*s, int race) {
	using namespace buildpred;

	state initial_state;
	initial_state.frame = 0;
	initial_state.minerals = 50;
	initial_state.gas = 0;
	initial_state.used_supply = { 0, 0, 0 };
	initial_state.max_supply = { 0, 0, 0 };
	add_base(initial_state, *s).verified = true;
	if (race == race_terran) {
		add_unit_and_supply(initial_state, unit_types::cc);
		add_unit_and_supply(initial_state, unit_types::scv);
		add_unit_and_supply(initial_state, unit_types::scv);
		add_unit_and_supply(initial_state, unit_types::scv);
		add_unit_and_supply(initial_state, unit_types::scv);
	} else if (race == race_protoss) {
		add_unit_and_supply(initial_state, unit_types::nexus);
		add_unit_and_supply(initial_state, unit_types::probe);
		add_unit_and_supply(initial_state, unit_types::probe);
		add_unit_and_supply(initial_state, unit_types::probe);
		add_unit_and_supply(initial_state, unit_types::probe);
	} else {
		add_unit_and_supply(initial_state, unit_types::hatchery).larva_count = 3;
		add_unit_and_supply(initial_state, unit_types::drone);
		add_unit_and_supply(initial_state, unit_types::drone);
		add_unit_and_supply(initial_state, unit_types::drone);
		add_unit_and_supply(initial_state, unit_types::drone);
		add_unit_and_supply(initial_state, unit_types::overlord);
	}
	return initial_state;

}

buildpred::state get_initial_opponent_state() {
	using namespace buildpred;

	for (xy start_pos : start_locations) {
		if (start_pos == my_start_location) continue;
		resource_spots::spot*s = get_best_score_p(resource_spots::spots, [&](auto*s) {
			return diag_distance(s->cc_build_pos - start_pos);
		});
		if (!s) continue;
		return get_initial_state(s, players::opponent_player->race);
	}

	return buildpred::state();
}

buildpred::state get_initial_my_state() {
	using namespace buildpred;

	resource_spots::spot*s = get_best_score_p(resource_spots::spots, [&](auto*s) {
		return diag_distance(s->cc_build_pos - my_start_location);
	});
	if (!s) return buildpred::state();
	return get_initial_state(s, players::my_player->race);
}

a_vector<buildpred::state> fit_unit_counts(buildpred::state st, const a_map<int, a_unordered_map<unit_type*, int>>&unit_counts, const a_multimap<int, unit_type*>&destroyed_at, int end_frame, bool counter = false, bool is_defending = false, bool future_is_unknown = false) {
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

	int passes = counter ? 6 : 4;

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
			log("pass %d entry -- \n", pass);
			log("frame %d, minerals %g gas %g supply %g/%g\n", st.frame, st.minerals, st.gas, st.used_supply[race], st.max_supply[race]);
			log(" partial build order- ");
			for (auto&v : st.produced) {
				build_type bt = std::get<0>(v.second);
				log(" %s", bt->unit ? short_type_name(bt->unit) : bt->name);
			}
			log("\n");
		}

		satisfied = false;
		while (!all_states.empty()) {
			st = all_states.back();

			if (verbose_logging) {
				log("pass %d -- \n", pass);
				log("frame %d, minerals %g gas %g supply %g/%g\n", st.frame, st.minerals, st.gas, st.used_supply[race], st.max_supply[race]);
				log(" partial build order- ");
				for (auto&v : st.produced) {
					build_type bt = std::get<0>(v.second);
					log(" %s", bt->unit ? short_type_name(bt->unit) : bt->name);
				}
				log("\n");
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
					if (st.minerals > 600) mp(unit_types::zealot);
					if (worker_count >= 12) mp(unit_types::dragoon);
					if (opponent_race == race_terran) {
						if (worker_count > (int)st.bases.size() * 16 && can_expand) nd(cc_type);
					}
// 					if (opponent_race == race_protoss) {
// 						if (count_units_plus_production(st, unit_types::observer) == 0) nd(unit_types::observer);
// 						else nd(unit_types::reaver);
// 					}
					if (worker_count < (int)st.bases.size() * 24) nd(worker_type);
					//if (worker_count > (int)st.bases.size() * 24) nd(cc_type);

					if (opponent_race == race_protoss && worker_count >= 24) {
						if (count_units_plus_production(st, unit_types::observer) == 0) nd(unit_types::observer);
					}
				}

				auto get_power = [&](state&st, auto&counts) {

					auto count = [&](unit_type*ut) {
						auto i = counts.find(ut);
						if (i == counts.end()) return 0;
						return i->second;
					};

					int dragoons = count_units_plus_production(st, unit_types::dragoon);
					int zealots = count_units_plus_production(st, unit_types::zealot);
					int dts = count_units_plus_production(st, unit_types::dark_templar);
					int archons = count_units_plus_production(st, unit_types::archon);
					int arbiters = count_units_plus_production(st, unit_types::arbiter);
					int cannons = count_units_plus_production(st, unit_types::photon_cannon);
					int reavers = count_units_plus_production(st, unit_types::reaver);
					int corsairs = count_units_plus_production(st, unit_types::corsair);
					int high_templars = count_units_plus_production(st, unit_types::high_templar);
					int scouts = count_units_plus_production(st, unit_types::scout);
					int carriers = count_units_plus_production(st, unit_types::carrier);

					double ground_power = opponent_race != race_protoss ? 2 : 0;
					double air_power = 0;
					ground_power += dragoons * 2.5;
					air_power += dragoons * 2.5;
					ground_power += zealots * 1.75;
					ground_power += dts * 3;
					ground_power += archons * 6;
					air_power += archons * 6;

					if (!is_defending) ground_power += cannons * 3.5;
					ground_power += reavers * 5;
					if (zealots + dragoons >= 6) ground_power += high_templars * 4.5;

					if (arbiters) ground_power += 8.0;
					if (arbiters > 1) ground_power += count(unit_types::siege_tank_tank_mode) * 1.0;

					air_power += corsairs * 4.0;
					air_power += scouts * 4.0;
					ground_power += carriers * 8.0;
					air_power += carriers * 8.0;

					bool they_have_obs = false;
					if (count(unit_types::science_vessel)) they_have_obs = true;
					if (count(unit_types::observer)) they_have_obs = true;
					if (count(unit_types::overlord)) they_have_obs = true;

					//if (!they_have_obs) ground_power += dts * 3;

					return std::make_pair(ground_power, air_power);
				};

				bool i_have_obs = false;
				if (count_units_plus_production(st, unit_types::science_vessel)) i_have_obs = true;
				if (count_units_plus_production(st, unit_types::observer)) i_have_obs = true;
				if (count_units_plus_production(st, unit_types::overlord)) i_have_obs = true;

				auto get_required_power = [&](state&st, auto&counts) {
					double required_ground_power = 0.0;
					double required_air_power = 0.0;

					for (auto&v : counts) {
						unit_type*ut = v.first;
						int count = v.second;

						if (ut == unit_types::marine) required_ground_power += 0.75 * count;
						if (ut == unit_types::medic) required_ground_power += 1.5 * count;
						if (ut == unit_types::firebat) required_ground_power += 1.5 * count;
						if (ut == unit_types::siege_tank_tank_mode) required_ground_power += 5.0 * count;
						if (ut == unit_types::vulture) required_ground_power += 1.25 * count;
						if (ut == unit_types::ghost) required_ground_power += 1.5 * count;
						if (ut == unit_types::goliath) required_ground_power += 2.25 * count;
						if (ut == unit_types::wraith) required_air_power += 2.0 * count;
						if (ut == unit_types::dropship) required_air_power += 1.0 * count;
						if (ut == unit_types::science_vessel) required_air_power += 2.0 * count;
						if (ut == unit_types::battlecruiser) required_air_power += 7.0 * count;
						if (ut == unit_types::valkyrie) required_air_power += 4.0 * count;

						if (ut == unit_types::dragoon) required_ground_power += 2.5 * count;
						if (ut == unit_types::zealot) required_ground_power += 1.75 * count;
						if (ut == unit_types::dark_templar) required_ground_power += 4.0 * count;
						if (ut == unit_types::dark_templar && !i_have_obs) required_ground_power += 9 * count;
						if (ut == unit_types::archon) required_ground_power += 6 * count;
						if (ut == unit_types::reaver) required_ground_power += 6 * count;
						if (ut == unit_types::high_templar) required_ground_power += 4.5 * count;
						if (ut == unit_types::dark_archon) required_ground_power += 3.0 * count;
						if (ut == unit_types::shuttle) required_air_power += 1.0 * count;
						if (ut == unit_types::scout) required_air_power += 4.0 * count;
						if (ut == unit_types::carrier) required_air_power += 8.0 * count;
						if (ut == unit_types::arbiter) required_air_power += 1.0 * count;

						if (ut == unit_types::zergling) required_ground_power += 0.5 * count;
						if (ut == unit_types::hydralisk) required_ground_power += 1.5 * count;
						if (ut == unit_types::mutalisk) required_air_power += 3.5 * count;
						if (ut == unit_types::lurker) required_ground_power += 3.5 * count;
						if (ut == unit_types::ultralisk) required_ground_power += 4.5 * count;
						if (ut == unit_types::defiler) required_ground_power += 2.0 * count;
						if (ut == unit_types::queen) required_air_power += 3.0 * count;
						if (ut == unit_types::guardian) required_air_power += 4.0 * count;
						if (ut == unit_types::devourer) required_air_power += 5.0 * count;

					}

					return std::make_pair(required_ground_power, required_air_power);
				};

				bool force_pass = false;
				failed = false;
				auto next_i = unit_counts.upper_bound(st.frame);
				if (next_i != unit_counts.begin()) {
					auto prev = next_i;
					--prev;
					if (counter) {
						
						double ground_power, air_power;
						std::tie(ground_power, air_power) = get_power(st, prev->second);

						double required_ground_power, required_air_power;
						std::tie(required_ground_power, required_air_power) = get_required_power(st, prev->second);

						if (ground_power < required_ground_power || air_power < required_air_power) {
							if (verbose_logging) {
								if (ground_power < required_ground_power) log("ground_power failed (%g / %g)\n", ground_power, required_ground_power);
								if (air_power < required_air_power) log("air_power failed (%g / %g)\n", air_power, required_air_power);
								log("frame %d, minerals %g gas %g supply %g/%g\n", st.frame, st.minerals, st.gas, st.used_supply[race], st.max_supply[race]);
								log(" partial build order- ");
								for (auto&v : st.produced) {
									build_type bt = std::get<0>(v.second);
									log(" %s", bt->unit ? short_type_name(bt->unit) : bt->name);
								}
								log("\n");
							}
							failed = true;

							double power_required = std::max(required_ground_power - ground_power, 0.0) + std::max(required_air_power + air_power, 0.0);
							if (last_pass_frame > best_fail_frame || (last_pass_frame == best_fail_frame && power_required > best_fail_power_required)) {
								if (pass != 0) {
									best_fail_frame = last_pass_frame;
									best_fail_power_required = power_required;
									best_fail_pass = pass;
									//best_fail_add_states = add_states;
									best_fail_all_states_size = all_states.size();
									if (verbose_logging) log("new best fail, yey\n");
								}
							}
						}

					} else {
						for (auto&v : prev->second) {
							unit_type*ut = v.first;
							if (ut->is_addon) continue;
							//if (ut == unit_types::siege_tank_siege_mode) ut = unit_types::siege_tank_tank_mode;
							if (ut == unit_types::siege_tank_siege_mode) xcept("sieged tank");
							if (count_units_plus_production_plus_morphed(st, ut) < v.second) {
								if (verbose_logging) {
									log("%s failed - %d / %d\n", v.first->name, count_units_plus_production(st, ut), v.second);
									log("frame %d, minerals %g gas %g supply %g/%g\n", st.frame, st.minerals, st.gas, st.used_supply[race], st.max_supply[race]);
									log(" partial build order- ");
									for (auto&v : st.produced) {
										build_type bt = std::get<0>(v.second);
										log(" %s", bt->unit ? short_type_name(bt->unit) : bt->name);
									}
									log("\n");
								}
								failed = true;
								break;
							}
						}
					}
					if (failed && (st.used_supply[race_terran] >= 190 || st.used_supply[race_protoss] >= 190 || st.used_supply[race_zerg] >= 190)) {
						if (verbose_logging) log("maxed out, breaking\n");
						failed = false;
						break;
					}
					if (failed) {
						if (!last_fail_frame) {
							if (verbose_logging) log("last_fail_frame set to %d\n", prev->first);
							last_fail_frame = prev->first;
							break;
						} else if (prev->first > last_fail_frame) {
							if (pass != passes - 1) {
								if (verbose_logging) log("passed previous fail at %d, but failed again\n", last_fail_frame);
								if (verbose_logging) log("last_fail_frame set to %d\n", prev->first);
								last_fail_frame = prev->first;
								break;
							}
						}
						if (pass == best_fail_pass_active) {
							log("best fail pass, not breaking\n");
							failed = false;
							force_pass = true;
						} else if (pass != passes - 1) {
							break;
						} else {
							if (all_states.size() == 1) {
								if (best_fail_frame) {
									failed = false;
									force_pass = true;
									log("last pass, restoring best fail\n");

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
									log("last pass, not breaking\n");
								}
							}
// 							if (last_fail_frame && (st.frame <= last_fail_frame - 15 * 60 * 4 || all_states.size() == 1)) {
// 								failed = false;
// 								force_pass = true;
// 								log("last pass, not breaking\n");
// 							} else break;
						}
					} else last_pass_frame = prev->first;
					//if (last_fail_frame && st.frame > last_fail_frame) {
					if (!force_pass && !failed && last_fail_frame && (prev->first >= last_fail_frame || st.frame >= last_fail_frame)) {

						if (verbose_logging) {
							log("frame %d passed previous fail at %d, setting pass to 0\n", st.frame, last_fail_frame);
							if (counter) {
								double ground_power, air_power;
								std::tie(ground_power, air_power) = get_power(st, prev->second);

								double required_ground_power, required_air_power;
								std::tie(required_ground_power, required_air_power) = get_required_power(st, prev->second);

								log("passed ground_power (%g / %g)\n", ground_power, required_ground_power);
							}
							log("frame %d, minerals %g gas %g supply %g/%g\n", st.frame, st.minerals, st.gas, st.used_supply[race], st.max_supply[race]);
							log("at %d - ", st.frame);
							for (auto&v : st.units) {
								if (!v.first || !v.second.size()) continue;
								log(" %dx%s", v.second.size(), short_type_name(v.first));
							}
							log("\n");
							log("  inprod - ");
							for (auto&v : st.production) {
								log(" %s", v.second->unit ? short_type_name(v.second->unit) : v.second->name);
							}
							log("\n");
						}
						last_fail_frame = 0;
						pass = 0;
					}
				}
				if (last_fail_frame && (count_units_plus_production(st, unit_types::pylon) || worker_count >= 8)) {
					queue.clear();
					if (counter) {
						queued_bt.clear();

						auto enqueue = [&](build_type_autocast bt, int mode) {
							int&q = queued_bt[bt.t];
							if (q) return;
							++q;
							queue.emplace_back(bt, mode);
						};

						//bool force_build_last = future_is_unknown && st.used_supply[race] >= 20 && worker_count >= st.used_supply[race] / 2;

						auto last = unit_counts.end();
						if (last != unit_counts.begin()) --last;
						for (auto i = next_i; i != unit_counts.end(); ++i) {
// 							bool force_build = i == last && force_build_last;
// 							if (!force_build) {
// 								if (i != next_i && i->first > last_fail_frame) break;
// 							}
							if (i != next_i && i->first > last_fail_frame) break;

							double ground_power, air_power;
							std::tie(ground_power, air_power) = get_power(st, i->second);

							double required_ground_power, required_air_power;
							std::tie(required_ground_power, required_air_power) = get_required_power(st, i->second);

							//if (!force_build && ground_power >= required_ground_power && air_power >= required_air_power) continue;
							if (ground_power >= required_ground_power && air_power >= required_air_power) continue;

							auto count = [&](unit_type*ut) {
								auto it = i->second.find(ut);
								if (it == i->second.end()) return 0;
								return it->second;
							};

							int my_arbiters = count_units_plus_production(st, unit_types::arbiter);
							int my_zealots = count_units_plus_production(st, unit_types::zealot);
							int my_dragoons = count_units_plus_production(st, unit_types::dragoon);
							int my_archons = count_units_plus_production(st, unit_types::archon);
							int my_probes = count_units_plus_production(st, unit_types::probe);
							int my_cannons = count_units_plus_production(st, unit_types::photon_cannon);
							int my_reavers = count_units_plus_production(st, unit_types::reaver);
							int my_corsairs = count_units_plus_production(st, unit_types::corsair);
							int my_high_templars = count_units_plus_production(st, unit_types::high_templar);
							int my_scouts = count_units_plus_production(st, unit_types::scout);
							int my_observers = count_units_plus_production(st, unit_types::observer);

							int op_tanks = count(unit_types::siege_tank_tank_mode);
							int op_marines = count(unit_types::marine);
							int op_medics = count(unit_types::medic);
							int op_firebats = count(unit_types::firebat);
							int op_vultures = count(unit_types::vulture);
							int op_battlecruisers = count(unit_types::battlecruiser);
							int op_wraiths = count(unit_types::wraith);
							int op_ghosts = count(unit_types::ghost);

							int op_zerglings = count(unit_types::zergling);
							int op_hydralisks = count(unit_types::hydralisk);
							int op_mutalisks = count(unit_types::mutalisk);
							int op_devourers = count(unit_types::devourer);
							int op_lurkers = count(unit_types::lurker);
							int op_ultralisks = count(unit_types::ultralisk);

							int op_zealots = count(unit_types::zealot);
							int op_dragoons = count(unit_types::dragoon);
							int op_carriers = count(unit_types::carrier);
							int op_dark_templars = count(unit_types::dark_templar);
							int op_arbiters = count(unit_types::arbiter);

							int op_resource_depots = count(unit_types::cc) + count(unit_types::nexus) + count(unit_types::hatchery) + count(unit_types::lair) + count(unit_types::hive);

							bool they_have_obs = false;
							if (count(unit_types::science_vessel)) they_have_obs = true;
							if (count(unit_types::observer)) they_have_obs = true;
							if (count(unit_types::overlord)) they_have_obs = true;

							int use_pass = pass;

							//if (force_pass && pass == 5 && st.bases.size() >= 3 && my_probes >= 40) use_pass = 2;
							//if (force_pass && st.bases.size( >= 3 && my_probes >= 60) use_pass = 2;
							if (my_probes >= 60) use_pass = 2;

							int zealot_mp = my_probes > 16 || count_units_plus_production(st, unit_types::gateway) < 2 ? 1 : 0;
							//int zealot_mp = my_probes >= 16 ? 1 : 0;

							if (op_dark_templars + op_lurkers + op_wraiths + op_ghosts + op_arbiters && !i_have_obs) enqueue(unit_types::observer, 0);
							//if ((my_probes >= 30 || my_cannons >= 3) && op_dark_templars + op_lurkers + op_wraiths + op_ghosts + op_arbiters && !i_have_obs) enqueue(unit_types::observer, 0);

							if (my_probes >= 45 && op_tanks && my_arbiters < (int)((op_tanks + 5) / 6)) enqueue(unit_types::arbiter, 0);

							if (use_pass == 2) {

// 								if (!is_defending && opponent_race != race_terran) {
// 									if (my_probes >= 12) {
// // 										if (my_cannons < 2) enqueue(unit_types::photon_cannon, 0);
// // 										else if (st.bases.size() == 1) enqueue(unit_types::nexus, 0);
// // 										else if (my_cannons < 6) enqueue(unit_types::photon_cannon, 0);
// 										if (my_cannons < 6) enqueue(unit_types::photon_cannon, 0);
// 									}
// 								}

								if (my_observers < op_dark_templars / 3 + op_lurkers / 3 + op_wraiths / 4 + op_ghosts / 4 + op_arbiters) {
									if (my_observers < 8) enqueue(unit_types::observer, 0);
								}

// 								if (opponent_race == race_protoss && count_units_plus_production(st, unit_types::gateway) >= 2) {
// 									if (ground_power < required_ground_power && my_reavers <= my_zealots / 4 + my_dragoons / 3) {
// 										enqueue(unit_types::reaver, 0);
// 									}
// 								}

								if (my_probes >= 20) {

									if (!has_or_is_upgrading(st, upgrade_types::singularity_charge)) {
										enqueue(upgrade_types::singularity_charge, 0);
									}

// 									if (my_probes >= 20) {
// 										if (!has_or_is_upgrading(st, upgrade_types::leg_enhancements)) {
// 											enqueue(upgrade_types::leg_enhancements, 0);
// 										}
// 									}
								}

								if (my_probes >= 55) {
									if (my_archons < op_zerglings / 12 + op_ultralisks + op_zealots / 4) {
										enqueue(unit_types::archon, 0);
										if (my_high_templars < 4) enqueue(unit_types::high_templar, 1);
									}
								}

								if (ground_power < required_ground_power) {
									if (opponent_race == race_terran && my_zealots + my_dragoons >= 4 && !they_have_obs) {
										enqueue(unit_types::dark_templar, 0);
									}
// 									if (my_zealots + my_dragoons >= 6 && !they_have_obs) {
// 										if (count_units_plus_production(st, unit_types::templar_archives) || op_zealots + op_dragoons < my_dragoons) {
// 											enqueue(unit_types::dark_templar, 0);
// 										} else {
// 											enqueue(unit_types::reaver, 0);
// 										}
// 									}

									if (my_zealots < op_tanks * 2) enqueue(unit_types::zealot, zealot_mp);
								}


								if (my_dragoons < my_reavers * 2) enqueue(unit_types::dragoon, 1);
								if (my_reavers < op_marines / 6 + op_medics / 4 + op_firebats / 4 + op_vultures / 4) enqueue(unit_types::reaver, 0);

								if (!has_or_is_upgrading(st, upgrade_types::singularity_charge)) {
									enqueue(upgrade_types::singularity_charge, 0);
								}

								if (my_probes >= 30) {
									if (my_reavers < op_zealots / 4 + op_dragoons / 6) {
										if (!has_or_is_upgrading(st, upgrade_types::reaver_capacity)) enqueue(upgrade_types::reaver_capacity, 0);
										enqueue(unit_types::reaver, 0);
									}
								}

								if (my_probes >= 40) {
									if (air_power < required_air_power) {
										if (my_scouts < op_battlecruisers * 2 + op_carriers * 2 + op_devourers) enqueue(unit_types::dragoon, 1);
										else enqueue(unit_types::corsair, 1);
									}
								}

								//if (my_zealots + my_dragoons >= 6 && !they_have_obs) enqueue(unit_types::dark_templar, 0);

								if (my_zealots < op_zerglings / 3) enqueue(unit_types::zealot, zealot_mp);
								if (my_zealots < op_tanks * 2) enqueue(unit_types::zealot, zealot_mp);

								if (my_corsairs < op_mutalisks / 2 || (my_dragoons >= 16 && air_power < required_air_power)) {
									enqueue(unit_types::corsair, 1);
								}

								if (my_zealots < op_hydralisks) {
									if (!has_or_is_upgrading(st, upgrade_types::leg_enhancements)) enqueue(upgrade_types::leg_enhancements, 0);
									if (my_archons < my_zealots / 3 || my_high_templars >= 8) enqueue(unit_types::archon, 1);
									enqueue(unit_types::zealot, zealot_mp);
									if (my_zealots >= 8 && my_archons + my_high_templars < my_zealots / 6) enqueue(unit_types::high_templar, 0);
								}

								enqueue(unit_types::dragoon, 1);
								if (ground_power < required_ground_power) enqueue(unit_types::zealot, zealot_mp);

							} else if (use_pass == 3) {

								//if (op_resource_depots > 1 && st.bases.size() < 2 && can_expand && my_probes >= 14) enqueue(unit_types::nexus, 0);
								if (my_probes >= 16 && count_units_plus_production(st, unit_types::assimilator) == 0) enqueue(unit_types::assimilator, 0);

								if (air_power < required_air_power) enqueue(unit_types::dragoon, 0);

								if (my_probes < 30 && (op_marines || op_dragoons)) {
									if (my_zealots == 0) enqueue(unit_types::zealot, 0);
									//if (my_probes >= 12 && count_units_plus_production(st, unit_types::assimilator) == 0) enqueue(unit_types::assimilator, 0);
									if (my_dragoons) {
										if (!has_or_is_upgrading(st, upgrade_types::singularity_charge)) {
											enqueue(upgrade_types::singularity_charge, 0);
										}
									}
									enqueue(unit_types::dragoon, 0);
								} else {
// 									if (my_zealots + my_dragoons >= 4 && my_reavers < (my_zealots + my_dragoons) / 8) {
// 										if (!they_have_obs) enqueue(unit_types::dark_templar, 0);
// 										else {
// 											enqueue(unit_types::reaver, 0);
// 											//enqueue(unit_types::archon, 0);
// 											//enqueue(unit_types::high_templar, 0);
// 										}
// 									}
								}
								if (my_dragoons < op_dragoons * 2) enqueue(unit_types::dragoon, 1);
								enqueue(unit_types::zealot, zealot_mp);

							} else if (use_pass == 4) {

								//if (st.bases.size() < 2 && can_expand && my_probes >= 14) enqueue(unit_types::nexus, 0);
								if (my_probes >= 16 && count_units_plus_production(st, unit_types::assimilator) == 0) enqueue(unit_types::assimilator, 0);

								if (my_probes >= 24) {
									if (op_resource_depots == 1 && !they_have_obs) enqueue(unit_types::dark_templar, 0);
								}

								if (!has_or_is_upgrading(st, upgrade_types::singularity_charge)) {
									enqueue(upgrade_types::singularity_charge, 0);
								}

								if (my_zealots >= op_hydralisks + op_zerglings / 4 + op_tanks * 3 || my_dragoons < op_dragoons * 2) {
									enqueue(unit_types::dragoon, 1);
								} else enqueue(unit_types::zealot, zealot_mp);

// 								if (my_zealots > my_dragoons * 2) enqueue(unit_types::zealot, 1);
// 								else enqueue(unit_types::dragoon, 1);

							} else if (use_pass == 5) {

								if (my_probes >= 16 && count_units_plus_production(st, unit_types::assimilator) == 0) enqueue(unit_types::assimilator, 0);

								if (my_probes >= 20 && ground_power < required_ground_power && opponent_race == race_terran) {
									if (my_zealots + my_dragoons >= 4 && op_tanks && !they_have_obs) enqueue(unit_types::dark_templar, 0);
								}

								if (air_power >= required_air_power) {
									if (op_resource_depots <= 1 && !they_have_obs && count_units_plus_production(st, unit_types::templar_archives)) enqueue(unit_types::dark_templar, 0);
								}

								if (air_power < required_air_power || (my_zealots >= 6 && my_dragoons < my_zealots && my_zealots > op_hydralisks + op_tanks)) {
									enqueue(unit_types::dragoon, 1);
								}
								if (my_dragoons < op_dragoons * 2 + op_marines / 4 + op_vultures / 4) enqueue(unit_types::dragoon, 1);
								enqueue(unit_types::zealot, zealot_mp);

// 							} else if (use_pass == 6) {
// 
// 								enqueue(unit_types::zealot, 1);

							}

						}

					} else {
						queued_ut.clear();
						for (auto i = next_i; i != unit_counts.end(); ++i) {
							//if (i->first >= st.frame + 15 * 60 * 8) break;
							if (i != next_i && i->first > last_fail_frame) break;
							for (auto&v : i->second) {
								unit_type*ut = v.first;
								if (ut->is_addon) continue;
								log("i frame %d, %s is %d / %d\n", i->first, ut->name, count_units_plus_production_plus_morphed(st, ut), v.second);
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
					}
					for (auto i = queue.rbegin(); i != queue.rend(); ++i) {
						if (verbose_logging) log("queue %s\n", i->first->name);
						if (i->second == 1) mp(i->first);
						else nd(i->first);
					}
				}

				if (pass == 0) {
					int op_tanks = 0;
					int op_vultures = 0;
					int op_goliaths = 0;
					if (next_i != unit_counts.end()) {
						auto count = [&](unit_type*ut) {
							auto it = next_i->second.find(ut);
							if (it == next_i->second.end()) return 0;
							return it->second;
						};
						op_tanks = count(unit_types::siege_tank_tank_mode);
						op_vultures = count(unit_types::vulture);
						op_goliaths = count(unit_types::goliath);
					}
					if (worker_count < 70) nd(worker_type);
					if (worker_count > (int)st.bases.size() * 24) nd(cc_type);
					if (counter) {
						int worker_count = count_units_plus_production(st, worker_type);
						if (worker_type == unit_types::probe) {
							if (worker_count >= 20 && future_is_unknown) {
								if (count_production(st, worker_type)) mp(unit_types::dragoon);
							}
							if (worker_count >= 12) {
// 								if (count_units_plus_production(st, unit_types::zealot) < 2) nd(unit_types::zealot);
// 								if (is_defending && worker_count < 30) {
// 									if (count_units_plus_production(st, unit_types::zealot) < 5) nd(unit_types::zealot);
// 								}

// 								if (opponent_race != race_terran) {
// 									if (count_units_plus_production(st, unit_types::photon_cannon) < 1) {
// 										nd(unit_types::photon_cannon);
// 									}
// 								}
							}
							if (worker_count >= 14) {
								//if (count_units_plus_production(st, unit_types::assimilator) == 0) nd(unit_types::assimilator);
							}
							if (worker_count >= 18) {
								if (count_units_plus_production(st, unit_types::zealot) + count_units_plus_production(st, unit_types::dragoon) < 2) nd(unit_types::zealot);
								if (count_units_plus_production(st, unit_types::cybernetics_core) == 0) nd(unit_types::cybernetics_core);
							}
							if (worker_count >= 24) {
								if (count_units_plus_production(st, unit_types::gateway) < 3) nd(unit_types::gateway);
								if (!has_or_is_upgrading(st, upgrade_types::protoss_ground_weapons_1)) nd(upgrade_types::protoss_ground_weapons_1);

								//if (!has_or_is_upgrading(st, upgrade_types::psionic_storm)) nd(upgrade_types::psionic_storm);
								if (count_units_plus_production(st, unit_types::zealot) + count_units_plus_production(st, unit_types::dragoon) < 4) nd(unit_types::dragoon);
								if (!has_or_is_upgrading(st, upgrade_types::singularity_charge)) nd(upgrade_types::singularity_charge);

								//if (count_units_plus_production(st, unit_types::photon_cannon) == 0) nd(unit_types::photon_cannon);
							}

							if (worker_count >= 36) {
								if (!has_or_is_upgrading(st, upgrade_types::protoss_ground_weapons_1)) nd(upgrade_types::protoss_ground_weapons_1);
							}

							if (worker_count >= 70) {
								if (st.minerals >= 1800 && st.used_supply[race_protoss] < 175) mp(unit_types::zealot);
								if (count_units_plus_production(st, unit_types::dragoon) < 24) mp(unit_types::dragoon);
								mp(unit_types::carrier);
								if (!has_or_is_upgrading(st, upgrade_types::protoss_air_armor_3)) nd(upgrade_types::protoss_air_armor_3);
								if (!has_or_is_upgrading(st, upgrade_types::protoss_air_weapons_3)) nd(upgrade_types::protoss_air_weapons_3);
								if (!has_or_is_upgrading(st, upgrade_types::carrier_capacity)) nd(upgrade_types::carrier_capacity);
							}
// 							if (opponent_race == race_protoss && worker_count >= 22) {
// 								if (count_units_plus_production(st, unit_types::observer) == 0) nd(unit_types::observer);
// 								else if (count_units_plus_production(st, unit_types::reaver) == 0) nd(unit_types::reaver);
// 							}
							if (worker_count >= 38) {
								if (count_units_plus_production(st, unit_types::high_templar)) {
									if (!has_or_is_upgrading(st, upgrade_types::khaydarin_amulet)) nd(upgrade_types::khaydarin_amulet);
									if (!has_or_is_upgrading(st, upgrade_types::psionic_storm)) nd(upgrade_types::psionic_storm);
								}
								if (count_units_plus_production(st, unit_types::forge) < 2) nd(unit_types::forge);
								if (!has_or_is_upgrading(st, upgrade_types::protoss_ground_armor_1)) nd(upgrade_types::protoss_ground_armor_1);

								if (count_units_plus_production(st, unit_types::reaver)) {
									if (!has_or_is_upgrading(st, upgrade_types::reaver_capacity)) nd(upgrade_types::reaver_capacity);
								}
								if (!has_or_is_upgrading(st, upgrade_types::singularity_charge)) nd(upgrade_types::singularity_charge);
							}
							if (worker_count >= 40) {

								if (op_tanks + op_vultures + op_goliaths >= 4) {
									if (count_units_plus_production(st, unit_types::arbiter_tribunal) == 0) nd(unit_types::arbiter_tribunal);
								}

								if (!has_or_is_upgrading(st, upgrade_types::leg_enhancements)) nd(upgrade_types::leg_enhancements);
								if (!has_or_is_upgrading(st, upgrade_types::protoss_ground_weapons_2)) nd(upgrade_types::protoss_ground_weapons_2);
								if (!has_or_is_upgrading(st, upgrade_types::protoss_ground_armor_2)) nd(upgrade_types::protoss_ground_armor_2);

								if (count_units_plus_production(st, unit_types::observer) < 2) nd(unit_types::observer);
// 								if (count_units_plus_production(st, unit_types::dragoon) < 4) nd(unit_types::dragoon);
// 								if (count_units_plus_production(st, unit_types::high_templar) < 4) nd(unit_types::high_templar);
// 								if (count_units_plus_production(st, unit_types::dark_templar) < 4) nd(unit_types::dark_templar);
// 								if (count_units_plus_production(st, unit_types::dragoon) < 8) nd(unit_types::dragoon);
							}

							if (worker_count >= 50) {
								if (count_units_plus_production(st, unit_types::scout) && !has_or_is_upgrading(st, upgrade_types::gravitic_thrusters)) nd(upgrade_types::gravitic_thrusters);
								if (count_units_plus_production(st, unit_types::reaver)) {
									if (!has_or_is_upgrading(st, upgrade_types::scarab_damage)) nd(upgrade_types::scarab_damage);
								}
								if (!has_or_is_upgrading(st, upgrade_types::sensor_array)) nd(upgrade_types::sensor_array);
								if (!has_or_is_upgrading(st, upgrade_types::gravitic_boosters)) nd(upgrade_types::gravitic_boosters);

								if (op_tanks >= 4) {
									if (count_units_plus_production(st, unit_types::arbiter) < op_tanks / 4) nd(unit_types::arbiter);
									if (count_production(st, unit_types::arbiter)) {
										if (!has_or_is_upgrading(st, upgrade_types::stasis_field)) nd(upgrade_types::stasis_field);
									}
								} else {
									if (!has_or_is_upgrading(st, upgrade_types::khaydarin_amulet)) nd(upgrade_types::khaydarin_amulet);
									else if (!has_or_is_upgrading(st, upgrade_types::psionic_storm)) nd(upgrade_types::psionic_storm);
									else if (count_units_plus_production(st, unit_types::high_templar) < 4) nd(unit_types::high_templar);
								}
							}
						}
					}
// 					if (worker_type == unit_types::probe) {
// 						if (worker_count >= 12 && count_units_plus_production(st, unit_types::assimilator) < (int)st.bases.size()) nd(unit_types::assimilator);
// 						//if (worker_count >= 16 && count_units_plus_production(st, unit_types::gateway) < 1) nd(unit_types::gateway);
// 					}
				}

				int cc_count = count_units_plus_production(st, cc_type);
				prev_frame = st.frame;
				if (!q(st)) break;

				for (; destroyed_i != destroyed_at.end() && destroyed_i->first < st.frame; ++destroyed_i) {
					unit_type*ut = destroyed_i->second;
					if (!st.units[ut].empty()) rm_unit_and_supply(st, ut);
				}

				if (count_units_plus_production(st, cc_type) > cc_count) {
					auto s = get_next_base();
					if (s) add_base(st, *s);
					can_expand = get_next_base();
				}
			}

			if (!failed) {
				log("satisfied (at frame %d)!\n", st.frame);
				log("frame %d, minerals %g gas %g supply %g/%g\n", st.frame, st.minerals, st.gas, st.used_supply[race], st.max_supply[race]);
				log(" partial build order- ");
				for (auto&v : st.produced) {
					build_type bt = std::get<0>(v.second);
					log(" %s", bt->unit ? short_type_name(bt->unit) : bt->name);
				}
				log("\n");
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
		log("exhausted all states, failed to satisfy\n");
		log("frame %d, minerals %g gas %g supply %g/%g\n", st.frame, st.minerals, st.gas, st.used_supply[race], st.max_supply[race]);
// 		st = std::move(prev_st);
// 		all_states = std::move(prev_all_states);
	}

	log("final all states -- \n");
	for (auto&st : all_states) {
		log("at %d - ", st.frame);
		for (auto&v : st.units) {
			if (!v.first || !v.second.size()) continue;
			log(" %dx%s", v.second.size(), short_type_name(v.first));
		}
		log("\n");
		log("  inprod - ");
		for (auto&v : st.production) {
			log(" %s", v.second->unit ? short_type_name(v.second->unit) : v.second->name);
		}
		log("\n");
	}

	log(" frame %d -- satisfied is %d\n", st.frame, satisfied);
	for (auto&v : st.units) {
		if (!v.first || !v.second.size()) continue;
		log(" %dx%s", v.second.size(), short_type_name(v.first));
	}
	log("\n");
	log(" build order - ");
	for (auto&v : st.produced) {
		build_type bt = std::get<0>(v.second);
		log(" %s", bt->unit ? short_type_name(bt->unit) : bt->name);
	}
	log("\n");

// 	if (!satisfied) {
// 		xcept("failed :(");
// 	}

	return all_states;
}

unit_type* get_unit_type(const a_string&name) {
	for (auto&v : units::unit_type_container) {
		if (v.name == name) {
			return &v;
		}
	}
	return nullptr;
}

int long_distance_miners() {
	int count = 0;
	for (auto&g : resource_gathering::live_gatherers) {
		if (!g.resource) continue;
		unit*ru = g.resource->u;
		resource_spots::spot*rs = nullptr;
		for (auto&s : resource_spots::spots) {
			if (grid::get_build_square(s.cc_build_pos).building) continue;
			for (auto&r : s.resources) {
				if (r.u == ru) {
					rs = &s;
					break;
				}
			}
			if (rs) break;
		}
		if (rs) ++count;
	}
	return count;
};

auto free_mineral_patches = [&]() {
	int free_mineral_patches = 0;
	for (auto&b : buildpred::get_my_current_state().bases) {
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

auto get_next_base = [&]() {
	auto st = buildpred::get_my_current_state();
	unit_type*worker_type = unit_types::probe;
	unit_type*cc_type = unit_types::nexus;
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
		double ned = get_best_score_value(enemy_buildings, [&](unit*e) {
			if (e->type->is_worker) return std::numeric_limits<double>::infinity();
			return diag_distance(s->pos - e->pos);
		});
		if (ned <= 32 * 30) score += 10000;
		bool has_gas = false;
		for (auto&r : s->resources) {
			res += r.u->resources;
			if (r.u->type->is_gas) has_gas = true;
		}
		score -= (has_gas ? res : res / 4) / 10 + ned;
		return score;
	}, std::numeric_limits<double>::infinity());
};

auto place_expos = [&]() {

	auto st = buildpred::get_my_current_state();

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
			else b.dead = true;
			if (pos != xy()) build::set_build_pos(&b, pos);
			else build::unset_build_pos(&b);
		}
	}

};


xy natural_pos;
a_unordered_set<build::build_task*> has_placed;

auto ffe = [&]() {

	//bool build_base_cannons = current_used_total_supply >= 30;
	bool build_base_cannons = false;

	auto post_build = [&]() {

		auto&dragoon_pathing_map = square_pathing::get_pathing_map(unit_types::dragoon);
		while (dragoon_pathing_map.path_nodes.empty()) multitasking::sleep(1);
		double max_center_distance = std::max(diag_distance(combat::defence_choke.inside - combat::defence_choke.center), diag_distance(combat::defence_choke.outside - combat::defence_choke.center));

		auto get_cannon_pos = [&](unit_type*ut) {

			a_vector<double> costgrid(grid::build_grid_width * grid::build_grid_height);
			using node_t = std::tuple<xy, double>;
			struct node_t_cmp {
				bool operator()(const node_t&a, const node_t&b) {
					return std::get<1>(a) > std::get<1>(b);
				}
			};
			std::priority_queue<node_t, a_vector<node_t>, node_t_cmp> open;

			a_vector<unit*> buildings;
			for (unit*u : my_units_of_type[unit_types::photon_cannon]) buildings.push_back(u);
			for (unit*u : my_units_of_type[unit_types::pylon]) buildings.push_back(u);
			for (unit*u : my_units_of_type[unit_types::forge]) buildings.push_back(u);
			for (unit*u : my_units_of_type[unit_types::gateway]) buildings.push_back(u);

			int iterations = 0;
			auto generate_costgrid = [&](xy pos) {
				xy center_pos = pos + xy(ut->tile_width * 16, ut->tile_height * 16);
				auto get_cost = [&](xy npos) {
					if (npos.x >= pos.x && npos.x < pos.x + ut->tile_width * 32 && npos.y >= pos.y && npos.y < pos.y + ut->tile_height * 32) return 0.0;
					double r = 1.0;
					for (unit*u : buildings) {
						if (u->building && npos.x >= u->building->build_pos.x && npos.x < u->building->build_pos.x + u->type->tile_width * 32 && npos.y >= u->building->build_pos.y && npos.y < u->building->build_pos.y + u->type->tile_height * 32) return 0.0;
						double d = diag_distance(center_pos - u->pos);
						r += 256.0 / (d*d);
						if (u->type == unit_types::photon_cannon) {
							if (d <= 32 * 5) r += 1.0;
						}
					}
					xy n_center_pos = npos + xy(16, 16);
					double d = diag_distance(center_pos - n_center_pos);
					r += 256.0 / (d*d);
					if (d <= 32 * 5) r += 1.0;

					return r;
				};

				std::fill(costgrid.begin(), costgrid.end(), 0);
				for (xy pos : start_locations) {
					if (pos != my_start_location) {
						open.emplace(pos, 1.0);
						costgrid[grid::build_square_index(pos)] = 1.0;
					}
				}
				while (!open.empty()) {
					++iterations;
					if (iterations % 0x200 == 0) multitasking::yield_point();
					xy cpos;
					double cost;
					std::tie(cpos, cost) = open.top();
					open.pop();

					auto add = [&](xy npos) {
						if ((size_t)npos.x >= (size_t)grid::map_width || (size_t)npos.y >= (size_t)grid::map_height) return;
						size_t index = grid::build_square_index(npos);
						if (!grid::build_grid[index].entirely_walkable) return;
						if (costgrid[index]) return;
						double ncost = get_cost(npos);
						if (ncost == 0.0) return;
						if (ncost < cost) ncost = cost;
						costgrid[index] = ncost;
						open.emplace(npos, ncost);
					};
					add(cpos + xy(32, 0));
					add(cpos + xy(0, 32));
					add(cpos + xy(-32, 0));
					add(cpos + xy(0, -32));
				}
			};

			xy screen_pos = bwapi_pos(game->getScreenPosition());
			generate_costgrid(screen_pos + xy(200, 320));

			size_t path_iterations = 0;
			auto pred = [&](grid::build_square&bs) {
				if (!build_spot_finder::is_buildable_at(ut, bs)) return false;
				if (ut->requires_pylon && !build::build_has_pylon(bs, ut)) {
					for (unit*u : my_units_of_type[unit_types::pylon]) {
						if (pylons::is_in_pylon_range(u->pos - (bs.pos + xy(ut->tile_width * 16, ut->tile_height * 16)))) return true;
					}
					return false;
				}
				return true;
			};
			auto score = [&](xy pos) {

				generate_costgrid(pos);

				double cost_to_main = costgrid[grid::build_square_index(my_start_location)];
				double cost_to_natural = costgrid[grid::build_square_index(natural_pos)];

				return std::max(1.0 / cost_to_main, 1.0 / cost_to_natural);
			};
			std::vector<xy> starts;
			starts.push_back(natural_pos);
			xy pos = build_spot_finder::find_best(starts, 256, pred, score);
			return pos;
		};

		a_vector<unit*> nexuses_without_a_pylon;
		for (auto&u : my_units_of_type[unit_types::nexus]) {
			double d = get_best_score_value(my_units_of_type[unit_types::photon_cannon], [&](unit*u2) {
				return diag_distance(u->pos - u2->pos);
			});
			if (my_units_of_type[unit_types::photon_cannon].empty() || d >= 32 * 12) nexuses_without_a_pylon.push_back(u);
		}
		unit*least_protected_nexus = get_best_score(my_resource_depots, [&](unit*u) {
			int n = 0;
			for (unit*u2 : my_units_of_type[unit_types::photon_cannon]) {
				if (diag_distance(u->pos - u2->pos) < 32 * 12) ++n;
			}
			return n;
		});

		bool has_pylon_at_natural = false;
		for (unit*u : my_units_of_type[unit_types::pylon]) {
			if (diag_distance(u->pos - natural_pos) <= 32 * 12) has_pylon_at_natural = true;
		}

		auto at_wall = [&](unit_type*ut) {
			if (!ut || !ut->is_building) return false;
			if (my_completed_units_of_type[unit_types::pylon].empty()) return true;
			if (ut == unit_types::photon_cannon && !build_base_cannons) return true;
			if (ut == unit_types::pylon && (my_units_of_type[unit_types::pylon].size() == 0 || my_units_of_type[unit_types::pylon].size() == 2)) return true;
			if (ut == unit_types::pylon && !has_pylon_at_natural) return true;
			if (my_completed_units_of_type[unit_types::pylon].size() > 2) return false;
			if (ut == unit_types::forge && my_units_of_type[unit_types::forge].empty()) return true;
			if (ut == unit_types::gateway && my_units_of_type[unit_types::gateway].size() < 2) return true;
			return false;
		};

		for (auto&b : build::build_order) {
			if (b.built_unit || b.dead) continue;
			if (at_wall(b.type->unit)) {
				if ((b.type->unit == unit_types::pylon || !my_units_of_type[unit_types::pylon].empty()) && has_placed.insert(&b).second) {
					b.build_near = natural_pos;
					xy prev_pos = b.build_pos;
					build::unset_build_pos(&b);
					b.dont_find_build_pos = true;
					xy pos = get_cannon_pos(b.type->unit);
					b.dont_find_build_pos = false;
					if (pos != xy()) {
						build::set_build_pos(&b, pos);
					} else build::set_build_pos(&b, prev_pos);
				}
			} else if (b.type->unit == unit_types::pylon) {
				if (!nexuses_without_a_pylon.empty()) b.build_near = nexuses_without_a_pylon.front()->pos;
			} else if (b.type->unit == unit_types::photon_cannon) {
				if (least_protected_nexus) b.build_near = least_protected_nexus->pos;
			}
		}

	};

	post_build();
};

std::array<a_vector<a_map<int, a_unordered_map<unit_type*, int>>>, 3> unit_counts_per_race;

void test4() {

	using namespace buildpred;

	multitasking::sleep(2);

	auto jv = strategy::read_json(format("bwapi-data/read/buildopt-unit_counts-%s.txt", strategy::escape_filename(strategy::opponent_name)));
	if (jv.type == json_value::t_null) jv = strategy::read_json(format("bwapi-data/write/buildopt-unit_counts-%s.txt", strategy::escape_filename(strategy::opponent_name)));

	//a_map<int, a_unordered_map<unit_type*, int>> read_unit_counts_at;
	for (int i = 0; i < 3; ++i) {
		unit_counts_per_race[i].clear();
		for (auto&jv_list_v : jv[i].vector) {
			unit_counts_per_race[i].emplace_back();
			auto&uc = unit_counts_per_race[i].back();
			for (auto&jv_v : jv_list_v.map) {
				int f = strtol(jv_v.first.c_str(), nullptr, 0);
				auto&v = uc[f];
				for (auto&jv_v2 : jv_v.second.map) {
					unit_type*ut = get_unit_type(jv_v2.first);
					//if (ut == unit_types::siege_tank_siege_mode) ut = unit_types::siege_tank_tank_mode;
					if (ut == unit_types::siege_tank_siege_mode) xcept("sieged tank");
					if (ut) v[ut] = jv_v2.second;
				}
			}
		}
	}

// 	log(" read counts is - \n");
// 	for (auto&v : read_unit_counts_at) {
// 		log("at %d\n", v.first);
// 		for (auto&v2 : v.second) {
// 			log("  %dx%s\n", v2.second, v2.first->name);
// 		}
// 	}

// 	auto initial_op_st = get_initial_opponent_state();
// 
// 	if (!initial_op_st.bases.empty()) op_spawn_loc = initial_op_st.bases.front().s->pos;

	int time_frame = 15 * 60 * 10;

	//auto op_st = initial_op_st;

	double initial_build_random_seed = rng(1.0);

	while (true) {

		if (current_used_total_supply < 160) get_upgrades::set_no_auto_upgrades(true);
		else get_upgrades::set_no_auto_upgrades(false);
		//combat::no_aggressive_groups = true;

		double army_supply = 0.0;
		int my_detector_count = 0;
		for (unit*u : my_units) {
			if (!u->type->is_worker) army_supply += u->type->required_supply;
			if (u->type->is_detector && !u->type->is_building) ++my_detector_count;
		}
		double enemy_army_supply = 0.0;
		bool they_have_any_detectors = false;
		int enemy_dt_count = 0;
		int enemy_lurker_count = 0;
		for (unit*u : enemy_units) {
			if (!u->type->is_worker) enemy_army_supply += u->type->required_supply;
			if (u->type->is_detector) they_have_any_detectors = true;
			if (u->type == unit_types::dark_templar) ++enemy_dt_count;
			if (u->type == unit_types::lurker) ++enemy_lurker_count;
		}

		combat::no_aggressive_groups = current_used_total_supply < 190 && army_supply < enemy_army_supply + std::min(enemy_army_supply, 20.0);

		if (players::opponent_player->race == race_zerg && my_units_of_type[unit_types::zealot].size() < 8 && current_used_total_supply < 70) combat::no_aggressive_groups = true;

		if (my_lost_worker_count > (int)my_workers.size() / 3) combat::no_aggressive_groups = false;

		if (army_supply >= 60.0 && army_supply >= enemy_army_supply * 0.75) combat::no_aggressive_groups = false;
		else {
			if ((enemy_dt_count || enemy_lurker_count) && my_detector_count < 3) combat::no_aggressive_groups = true;
		}

		if (players::opponent_player->race != race_terran && army_supply < 12.0) {
			combat::force_defence_is_scared_until = current_frame + 15 * 30;
			combat::no_aggressive_groups = true;
		}

		if (!my_completed_units_of_type[unit_types::dark_templar].empty() && !they_have_any_detectors) combat::no_aggressive_groups = false;

		a_map<int, a_unordered_map<unit_type*, int>> op_unit_counts_at;
		double best_op_unit_counts_at_score = std::numeric_limits<double>::infinity();

		const auto&possible_unit_counts = unit_counts_per_race[players::opponent_player->race];

		bool use_random_guess = true;
		if (!built_unit_counts_at.empty()) {
			int workers = 0;
			int hatcheries = 0;
			for (auto&v : (--built_unit_counts_at.end())->second) {
				unit_type*ut = v.first;
				if (ut->is_worker) workers += v.second;
				else if (ut == unit_types::hatchery) hatcheries += v.second;
				else if (ut != unit_types::supply_depot && ut != unit_types::pylon && ut != unit_types::overlord) {
					use_random_guess = false;
					break;
				}
			}
			//if (hatcheries >= 2 || workers >= 8) use_random_guess = false;
			if (hatcheries >= 2) use_random_guess = false;
		}

		if (use_random_guess) {
			if (!possible_unit_counts.empty()) {
				size_t n = possible_unit_counts.size();
				if (n > 4) n = 4;
				n = (int)(initial_build_random_seed * n);
				if ((size_t)n >= possible_unit_counts.size()) n = 0;
				op_unit_counts_at = possible_unit_counts[possible_unit_counts.size() - 1 - n];
				log("random guess %d\n", n);
			}
		} else {
			if (!built_unit_counts_at.empty()) {
				auto&uc = (--built_unit_counts_at.end())->second;
				int age = possible_unit_counts.size();
				for (auto&n : possible_unit_counts) {
					--age;
					auto it = n.lower_bound(current_frame);
					if (it == n.end()) continue;
					auto&n_uc = it->second;
					double score = 0.0;
					auto diff = [&](unit_type*ut, int count) {
						double s = (ut->total_minerals_cost + ut->total_gas_cost) * count;
						score += s*s;
					};
					for (auto&v : uc) {
						if (v.first->is_worker) continue;
						if (v.first == unit_types::supply_depot) continue;
						if (v.first == unit_types::pylon) continue;
						if (v.first == unit_types::overlord) continue;
						auto i = n_uc.find(v.first);
						if (i == n_uc.end()) diff(v.first, v.second);
						else if (i->second != v.second) diff(v.first, std::abs(i->second - v.second));
					}
					for (auto&v : n_uc) {
						if (v.first->is_worker) continue;
						if (v.first == unit_types::supply_depot) continue;
						if (v.first == unit_types::pylon) continue;
						if (v.first == unit_types::overlord) continue;
						auto i = uc.find(v.first);
						if (i == uc.end()) diff(v.first, v.second);
					}
					score += score * (age / 32.0);
					log("uc score %g\n", score);
					if (score < best_op_unit_counts_at_score) {
						best_op_unit_counts_at_score = score;
						op_unit_counts_at = n;
					}
				}
			}
		}

		log("best_op_unit_counts_at_score is %g\n", best_op_unit_counts_at_score);

		//auto op_unit_counts_at = read_unit_counts_at;

// 		for (auto&v : built_unit_counts_at) {
// 			auto&d = op_unit_counts_at[v.first];
// 			if (v.first < current_frame - time_frame) d.clear();
// 			for (auto&v2 : v.second) {
// 				int&c = d[v2.first];
// 				if (v2.second > c) c = v2.second;
// 			}
// 		}

// 		log(" op_unit_counts_at is - \n");
// 		for (auto&v : op_unit_counts_at) {
// 			log("at %d\n", v.first);
// 			for (auto&v2 : v.second) {
// 				log("  %dx%s\n", v2.second, v2.first->name);
// 			}
// 		}

// 		auto op_states = fit_unit_counts(op_st, op_unit_counts_at, unit_destroyed_at, current_frame + time_frame);
// 		if (op_st.frame < current_frame - time_frame) {
// 			for (auto&v : op_states) {
// 				if (v.frame >= current_frame - time_frame) {
// 					op_st = v;
// 					break;
// 				}
// 			}
// 		}
// 		auto last_op_state = op_states.empty() ? op_st : op_states.back();

		auto initial_my_st = get_my_current_state();

		log("initial_my_st.bases.size() is %d\n", initial_my_st.bases.size());

		int begin_frame = current_frame;
		int end_frame = current_frame + time_frame;
		a_map<int, a_unordered_map<unit_type*, int>> units_to_counter_at;
// 		for (auto&v : read_unit_counts_at) {
// 			//int delay = 15 * 30;
// 			int delay = 0;
// 			int frame = v.first + delay;
// 			if (frame <= current_frame) continue;
// 			if (frame > end_frame) end_frame = frame;
// 			auto&c = units_to_counter_at[frame];
// 			c = v.second;
// 		}

// 		for (auto&st : op_states) {
// 			for (auto&v : st.units) {
// 				units_to_counter_at[st.frame][v.first] = v.second.size() - dead_unit_counts_at[st.frame][v.first];
// 			}
// 		}

// 		for (auto&st : op_states) {
// 			for (auto&v : st.units) {
// 				units_to_counter_at[st.frame][v.first] = v.second.size();
// 			}
// 		}

		units_to_counter_at = op_unit_counts_at;

// 		for (auto&v : units_to_counter_at) {
// 			auto i = dead_unit_counts_at.lower_bound(v.first);
// 			if (i != dead_unit_counts_at.end()) {
// 				for (auto&v2 : v.second) {
// 					v2.second -= i->second[v2.first];
// 				}
// 			}
// 		}

		for (auto i = units_to_counter_at.begin(); i != units_to_counter_at.end();) {
			for (auto i2 = i->second.begin(); i2 != i->second.end();) {
				i2->second -= destroyed_unit_counts[i2->first];
				if (i2->second <= 0) i2 = i->second.erase(i2);
				else ++i2;
			}
			if (i->second.empty()) i = units_to_counter_at.erase(i);
			else ++i;
		}

		units_to_counter_at[current_frame] = live_unit_counts;

		if ((--units_to_counter_at.end())->first < current_frame + time_frame) {
			auto val = (--units_to_counter_at.end())->second;
			units_to_counter_at[current_frame + time_frame] = std::move(val);
		}
		bool future_is_unknown = false;
		if ((--units_to_counter_at.end())->first < current_frame) {
			if (enemy_army_supply >= army_supply / 2 || army_supply < 10.0) {
				future_is_unknown = true;
			}
		}

		for (auto&v : units_to_counter_at) {
			if (v.first > current_frame) {
				for (auto&vx : live_unit_counts) {
					int&c = v.second[vx.first];
					if (vx.second > c) c = vx.second;
				}
			}
		}

		log(" units_to_counter_at is - \n");
		for (auto&v : units_to_counter_at) {
			log("at %d\n", v.first);
			for (auto&v2 : v.second) {
				log("  %dx%s\n", v2.second, v2.first->name);
			}
		}

		bool is_defending = false;
		for (auto&g : combat::groups) {
			if (!g.is_aggressive_group && !g.is_defensive_group) {
				if (g.ground_dpf >= 2.0) {
					is_defending = true;
					break;
				}
			}
		}

		bool is_being_pool_rushed = false;

		if (true) {
			auto i = units_to_counter_at.lower_bound(15 * 60 * 6);
			if (i != units_to_counter_at.end()) {
				auto i2 = i->second.find(unit_types::zergling);
				if (i2 != i->second.end() && i2->second >= 6) {
					is_being_pool_rushed = true;
				}
			}
		}

		if (is_being_pool_rushed && my_workers.size() < 16) {

			for (auto&v : scouting::all_scouts) {
				scouting::rm_scout(v.scout_unit);
			}
			strategy::execute_build([](state&st) {
				using namespace buildpred;
				if (count_units_plus_production(st, unit_types::probe) >= 16) {
					if (count_units_plus_production(st, unit_types::zealot) >= 3) return false;
					return depbuild(st, state(st), unit_types::zealot);
				}
				unit_type*first = unit_types::probe;
				unit_type*second = unit_types::zealot;
				if (count_units_plus_production(st, first) >= 12) std::swap(first, second);
				return nodelay(st, first, [&](state&st) {
					return depbuild(st, state(st), second);
				});
			});

		} else {
			auto my_states = fit_unit_counts(initial_my_st, units_to_counter_at, {}, current_frame + time_frame, true, is_defending, future_is_unknown);

			if (!my_states.empty()) {
				add_builds(my_states.back());
			}
		}

		bool can_expand = get_next_base();
		if (current_frame >= 15 * 60 * 15) {
			bool force_expand = can_expand && long_distance_miners() >= 1 && free_mineral_patches() < 6 && my_units_of_type[unit_types::nexus].size() == my_completed_units_of_type[unit_types::nexus].size();

			if (force_expand) {
				bool is_expanding = false;
				for (auto&b : build::build_tasks) {
					if (b.built_unit || b.dead) continue;
					if (b.type->unit && b.type->unit->is_resource_depot && b.type->unit->builder_type->is_worker) {
						is_expanding = true;
						break;
					}
				}
				if (!is_expanding) {
					if (!my_units_of_type[unit_types::cc].empty()) build::add_build_task(0.0, unit_types::cc);
					else if (!my_units_of_type[unit_types::nexus].empty()) build::add_build_task(0.0, unit_types::nexus);
					else build::add_build_task(0.0, unit_types::hatchery);
				}
			}
		}

		if (!can_expand) {
			for (auto&b : build::build_tasks) {
				if (b.built_unit || b.dead) continue;
				if (b.type->unit == unit_types::cc || b.type->unit == unit_types::nexus) {
					b.dead = true;
				}
			}
		}

		place_expos();

// 		if (players::opponent_player->race != race_terran) {
// 			bool is_ffeing = false;
// 			for (auto&b : build::build_order) {
// 				if (b.type->unit == unit_types::forge || b.type->unit == unit_types::photon_cannon) is_ffeing = true;
// 			}
// 
// 			if (natural_pos == xy() && current_frame >= 15 * 30) {
// 				auto s = get_next_base();
// 				if (s) natural_pos = s->pos;
// 			}
// 
// 			if ((!my_units_of_type[unit_types::forge].empty() || is_ffeing) && natural_pos != xy() && current_used_total_supply < 80) {
// 
// 				bool has_pylon_at_natural = false;
// 				for (unit*u : my_units_of_type[unit_types::pylon]) {
// 					if (diag_distance(u->pos - natural_pos) <= 32 * 12) has_pylon_at_natural = true;
// 				}
// 				bool pylon_planned_before_cannons = false;
// 				for (auto&b : build::build_order) {
// 					if (b.type->unit == unit_types::pylon) {
// 						pylon_planned_before_cannons = true;
// 						break;
// 					}
// 					if (b.type->unit == unit_types::photon_cannon) break;
// 				}
// 				//if (!has_pylon_at_natural && !pylon_planned_before_cannons) build::add_build_task(0.0, unit_types::pylon);
// 
// 				ffe();
// 
// 				combat::my_closest_base_override = natural_pos;
// 				combat::my_closest_base_override_until = current_frame + 15 * 10;
// 			}
// 		}

		multitasking::sleep(15 * 10);
	}

}

void buildopt_task() {

	multitasking::sleep(2);

	test4();

	while (true) {

		//test2();

		multitasking::sleep(1);
	}

}


void init() {

	units::on_new_unit_callbacks.push_back(on_new_unit);
	units::on_type_change_callbacks.push_back(on_type_change);
	units::on_destroy_callbacks.push_back(on_destroy);

	strategy::on_end_funcs.push_back([](bool won) {

// 		json_value jv;
// 
// 		for (auto&v : built_unit_counts_at) {
// 			auto&jv_v = jv[format("%d", v.first)];
// 			for (auto&v2 : v.second) {
// 				jv_v[v2.first->name] = v2.second;
// 			}
// 		}
// 
// 		strategy::write_json("bwapi-data/write/buildopt_tmp.txt", jv);

		json_value jv;

		auto&unit_counts_vec = unit_counts_per_race[players::opponent_player->race];
		unit_counts_vec.push_back(built_unit_counts_at);
		if (unit_counts_vec.size() > 8) unit_counts_vec.erase(unit_counts_vec.begin());

		for (int i = 0; i < 3; ++i) {
			auto&vec = unit_counts_per_race[i];
			for (size_t i2 = 0; i2 < vec.size(); ++i2) {
				for (auto&v : vec[i2]) {
					auto&jv_v = jv[i][i2][format("%d", v.first)];
					for (auto&v2 : v.second) {
						jv_v[v2.first->name] = v2.second;
					}
				}
			}
		}

		strategy::write_json(format("bwapi-data/write/buildopt-unit_counts-%s.txt", strategy::escape_filename(strategy::opponent_name)), jv);


	});

	multitasking::spawn(buildopt_task, "buildopt");


}

}

