
namespace buildpred {
;

struct build_type {
	build::build_type*t;
	explicit build_type(unit_type*unit) {
		t = build::get_build_type(unit);
	}
	explicit build_type(upgrade_type*upg) {
		t = build::get_build_type(upg);
	}
	build_type(build::build_type*t) : t(t) {}
	build_type(std::nullptr_t) : t(nullptr) {}
	build::build_type*operator->() const {
		return t;
	}
	explicit operator bool() const {
		return !!t;
	}
	bool operator==(const build_type&n) const {
		return t == n.t;
	}
	bool operator!=(const build_type&n) const {
		return t != n.t;
	}
};

struct build_type_autocast : build_type {
	build_type_autocast(const build_type&n) : build_type(n.t) {}
	build_type_autocast(unit_type*unit) : build_type(unit) {}
	build_type_autocast(upgrade_type*upg) : build_type(upg) {}
	build_type_autocast(build::build_type*t) : build_type(t) {}
	build_type_autocast(std::nullptr_t) : build_type(nullptr) {}
};

struct st_base {
	refcounted_ptr<resource_spots::spot> s;
	bool verified;
};
struct unit_match_info;
struct st_unit {
	unit_type*type;
	int busy_until = 0;
	bool has_addon = false;
	int larva_count = 0;
	st_unit(unit_type*type) : type(type) {}
};

const a_vector<st_unit> empty_units_range;

struct state {
	int race = race_terran;
	int frame = 0;
	double minerals = 0, gas = 0;
	double total_minerals_gathered = 0;
	double total_gas_gathered = 0;
	std::array<double, 3> used_supply, max_supply;
	a_unordered_map<unit_type*, a_vector<st_unit>> units;
	a_unordered_set<upgrade_type*> upgrades;
	a_multimap<int, build_type> production;
	a_multimap<int, std::tuple<build_type, resource_spots::spot*>> produced;
	struct resource_info_t {
		double gathered = 0.0;
		int workers = 0;
	};
	a_unordered_map<resource_spots::resource_t*, resource_info_t> resource_info;
	a_vector<st_base> bases;
	int idle_workers = 0;
	int army_size = 0;
	bool dont_build_refineries = false;
	bool auto_build_hatcheries = false;
	
	a_unordered_map<unit_type*, int> units_lost;

	auto units_range(unit_type* ut) const {
		auto i = units.find(ut);
		if (i != units.end()) return make_iterators_range(i->second.begin(), i->second.end());
		return make_iterators_range(empty_units_range.begin(), empty_units_range.end());
	};
};

a_unordered_map<unit*, double> last_seen_resources;

bool free_worker(state&st, bool minerals) {
	auto*worst = get_best_score_p(st.resource_info, [&](const std::pair<resource_spots::resource_t*const, state::resource_info_t>*ptr) {
		auto&v = *ptr;
		if (v.second.workers == 0) return 0.0;
		if (v.first->u->type->is_minerals != minerals) return 0.0;
		int n = (int)v.first->full_income_workers[st.race] - (v.second.workers - 1);
		double score = 0.0;
		if (n > 0) score = v.first->income_rate[st.race];
		else if (n > -1) score = v.first->income_rate[st.race] * v.first->last_worker_mult[st.race];
		return -score - 1.0;
	}, 0.0);
	if (!worst) return false;
	--worst->second.workers;
	++st.idle_workers;
	return true;
}

st_base&add_base(state&st, resource_spots::spot&s) {
	st.bases.push_back({ &s, false });
	for (auto&r : s.resources) {
		if (r.u->type->is_gas) continue;
		//st.resource_info.emplace(&r,&r);
		st.resource_info[&r];
	}
	return st.bases.back();
}
void rm_base(state&st, resource_spots::spot&s) {
	double resources_lost[2] = { 0, 0 };
	for (auto&r : s.resources) {
		auto i = st.resource_info.find(&r);
		if (i != st.resource_info.end()) {
			st.idle_workers += i->second.workers;
			resources_lost[i->first->u->type->is_gas] += i->second.gathered;
			st.resource_info.erase(i);
		}
	}
	log("rm_base: resources lost: %g %g\n", resources_lost[0], resources_lost[1]);
	st.minerals -= resources_lost[0];
	st.gas -= resources_lost[1];
	st.total_minerals_gathered -= resources_lost[0];
	st.total_gas_gathered -= resources_lost[1];
	for (auto i = st.bases.begin(); i != st.bases.end(); ++i) {
		if (&*i->s == &s) {
			st.bases.erase(i);
			return;
		}
	}
	xcept("rm_base: no such base");
}

st_unit&add_unit(state&st, unit_type*ut) {
	auto&vec = st.units[ut];
	vec.emplace_back(ut);
	//if (ut->is_addon) xcept("attempt to add addon");
	if (ut->is_worker) ++st.idle_workers;
	if (!ut->is_building && !ut->is_worker) ++st.army_size;
	return vec.back();
}
st_unit&add_unit_and_supply(state&st, unit_type*ut) {
	auto&u = add_unit(st, ut);
	if (ut->required_supply) st.used_supply[ut->race] += ut->required_supply;
	if (ut->provided_supply) st.max_supply[ut->race] += ut->provided_supply;
	return u;
}

void rm_unit(state&st, unit_type*ut) {
	auto&vec = st.units[ut];
	vec.pop_back();
	if (ut->is_worker) {
		if (!st.idle_workers) {
			if (!free_worker(st, true) && !free_worker(st, false)) xcept("buildpred rm_unit: failed to find worker to remove");
		}
		--st.idle_workers;
	}
	if (!ut->is_building && !ut->is_worker) --st.army_size;
}
void rm_unit_and_supply(state&st, unit_type*ut) {
	rm_unit(st, ut);
	if (ut->required_supply) st.used_supply[ut->race] -= ut->required_supply;
	if (ut->provided_supply) st.max_supply[ut->race] -= ut->provided_supply;
}

struct build_rule {
	unit_type*ut;
	double army_size_percentage;
	double weight;
};

struct ruleset {
	int end_frame = 0;
	int bases = 0;
	//a_vector<build_rule> build;
	std::function<bool(state&)> func;
};

int transfer_workers(state&st, bool minerals) {
	int count = 0;
	while (st.idle_workers != 0) {
		//log("%d resources\n", st.resource_info.size());
		auto*best = get_best_score_p(st.resource_info, [&](const std::pair<resource_spots::resource_t*const, state::resource_info_t>*ptr) {
			auto&v = *ptr;
			if (v.first->u->type->is_minerals != minerals) return 0.0;
			int n = (int)v.first->full_income_workers[st.race] - v.second.workers;
			double score = 0.0;
			if (n > 0) score = v.first->income_rate[st.race];
			else if (n > -1) score = v.first->income_rate[st.race] * v.first->last_worker_mult[st.race];
			return score;
		}, 0.0);
		if (best) {
			--st.idle_workers;
			++best->second.workers;
			++count;
			//log("transfer worker yey\n");
		} else break;
	}
	return count;
};

int count_units_plus_production(const state& st, unit_type* ut);

bool enable_verbose_build_logging = false;
int buildst_log_depth = 0;

build_type failed = build_type(((build::build_type*)nullptr) + 1);
build_type timeout = build_type(((build::build_type*)nullptr) + 2);

build_type advance(state&st, build_type_autocast build, int end_frame, bool nodep, bool no_busywait) {

	if (st.frame >= end_frame) {
		if (enable_verbose_build_logging) {
			for (int i = 0; i < buildst_log_depth; ++i) log(log_level_test, "  ");
			log(log_level_test, "advance '%s' -> instant timeout\n", build->name);
		}
		return timeout;
	}
	if (build && build->unit && !players::my_player->game_player->isUnitAvailable(build->unit->game_unit_type)) {
		if (enable_verbose_build_logging) {
			for (int i = 0; i < buildst_log_depth; ++i) log(log_level_test, "  ");
			log(log_level_test, "advance '%s' -> not available\n", build->name);
		}
		return failed;
	}

	//if (st.gas < st.minerals) transfer_workers(false);
	transfer_workers(st, false);
	transfer_workers(st, true);
	//log("%d idle workers\n", st.idle_workers);

	unit_type*addon_required = nullptr;
	bool prereq_in_prod = false;
	bool builder_in_prod = false;
	if (build) {
		for (build_type prereq : build->prerequisites) {
			if (prereq->unit) {
				if (prereq->unit == unit_types::larva) continue;
				if (prereq->unit->is_addon && prereq->builder == build->builder && !build->builder->is_addon) {
					addon_required = prereq->unit;
					continue;
				}
				if (st.units[prereq->unit].empty()) {
					if (prereq->unit == unit_types::spire && count_units_plus_production(st, unit_types::greater_spire)) continue;
					if (prereq->unit == unit_types::hatchery && count_units_plus_production(st, unit_types::hive)) continue;
					if (prereq->unit == unit_types::hatchery && count_units_plus_production(st, unit_types::lair)) continue;
					if (prereq->unit == unit_types::lair && count_units_plus_production(st, unit_types::hive)) continue;
				}
				if (st.units[prereq->unit].empty()) {
					bool found = false;
					if (prereq->unit->is_addon && !st.units[prereq->builder].empty()) {
						for (auto&v : st.units[prereq->builder]) {
							if (v.has_addon) {
								found = true;
								break;
							}
						}
					}
					for (auto&v : st.production) {
						if (v.second == prereq) {
							found = true;
							prereq_in_prod = true;
							break;
						}
					}
					if (!found) {
						if (enable_verbose_build_logging) {
							for (int i = 0; i < buildst_log_depth; ++i) log(log_level_test, "  ");
							log(log_level_test, "advance '%s' -> prereq: '%s'\n", build->name, prereq->name);
						}
						return prereq;
					}
				}
			}
		}
// 		if (build->builder != unit_types::larva) {
// 			bool builder_exists = false;
// 			if (build->builder->is_addon) {
// 				for (st_unit& u : st.units[build->builder->builder_type]) {
// 					if (u.has_addon) {
// 						builder_exists = true;
// 					}
// 				}
// 			} else {
// 				for (st_unit& u : st.units[build->builder]) {
// 					if (build->unit && build->unit->is_addon && u.has_addon) continue;
// 					if (!addon_required || u.has_addon) builder_exists = true;
// 				}
// 			}
// 			if (!builder_exists) {
// 				for (auto&v : st.production) {
// 					if (v.second->unit == build->builder) {
// 						builder_in_prod = true;
// 						break;
// 					}
// 				}
// 			}
// 		}
	}

	bool dont_free_workers = false;
	while (true) {
		//min_income *= 0.5;
		//gas_income *= 0.5;
		//log("frame %d: min_income %g gas_income %g\n", st.frame, min_income, gas_income);

		if (!st.production.empty()) {
			auto&v = *st.production.begin();
			if (v.first <= st.frame) {
				if (v.second->unit) {
					if (v.second->unit->is_addon) {
						auto*ptr = get_best_score_p(st.units[v.second->builder], [&](st_unit*st_u) {
							if (st_u->busy_until <= current_frame && !st_u->has_addon) return 0;
							return 1;
						});
						if (ptr) {
							ptr->has_addon = true;
						}
					} else {
						add_unit(st, v.second->unit);
						st.max_supply[v.second->unit->race] += v.second->unit->provided_supply;
					}
					if (v.second->unit->is_worker) {
						transfer_workers(st, false);
						transfer_workers(st, true);
					}
				}
				if (v.second->upgrade) {
					st.upgrades.insert(v.second->upgrade);
				}
				if (prereq_in_prod) {
					prereq_in_prod = false;
					for (build_type prereq : build->prerequisites) {
						if (prereq->unit) {
							if (prereq->unit == unit_types::larva) continue;
							if (prereq->unit == unit_types::spire && count_units_plus_production(st, unit_types::greater_spire)) continue;
							if (prereq->unit == unit_types::hatchery && count_units_plus_production(st, unit_types::hive)) continue;
							if (prereq->unit == unit_types::hatchery && count_units_plus_production(st, unit_types::lair)) continue;
							if (prereq->unit == unit_types::lair && count_units_plus_production(st, unit_types::hive)) continue;
							if (prereq->unit->is_addon && prereq->builder == build->builder && !build->builder->is_addon) {
								addon_required = prereq->unit;
								continue;
							}
							if (st.units[prereq->unit].empty()) {
								prereq_in_prod = true;
								break;
							}
						}
					}
				}

				st.production.erase(st.production.begin());
			}
		}
		if (builder_in_prod) {
			if (build->builder->is_addon) {
				for (st_unit& u : st.units[build->builder->builder_type]) {
					if (u.has_addon) {
						builder_in_prod = false;
					}
				}
			} else {
				for (st_unit& u : st.units[build->builder]) {
					if (build->unit && build->unit->is_addon && u.has_addon) continue;
					if (!addon_required || u.has_addon) builder_in_prod = false;
				}
			}
		}

		if (build) {
			auto add_built = [&](unit_type*t, bool subtract_build_time) {
				st.produced.emplace(st.frame - (subtract_build_time ? t->build_time : 0), std::make_tuple(build_type(t), nullptr));
				add_unit(st, t);
				st.minerals -= t->minerals_cost;
				st.gas -= t->gas_cost;
				st.used_supply[t->race] += t->required_supply;
				st.max_supply[t->race] += t->provided_supply;
			};
			//log("frame %d: min %g gas %g\n", st.frame, st.minerals, st.gas);
			bool has_enough_minerals = build->minerals_cost == 0 || st.minerals >= build->minerals_cost;
			bool has_enough_gas = build->gas_cost == 0 || st.gas >= build->gas_cost;
			if (has_enough_minerals && !has_enough_gas && !dont_free_workers) {
				dont_free_workers = true;
				if (free_worker(st, true)) {
					free_worker(st, true);
					transfer_workers(st, false);
					transfer_workers(st, true);
					//log("transfer -> gas\n");
					continue;
				}
			}
			if (has_enough_gas && !has_enough_minerals && !dont_free_workers) {
				dont_free_workers = true;
				if (free_worker(st, false)) {
					free_worker(st, false);
					transfer_workers(st, true);
					transfer_workers(st, false);
					//log("transfer -> min\n");
					continue;
				}
			}
			if (!has_enough_gas) {
				unit_type*refinery = unit_types::refinery;
				if (st.race == race_terran) refinery = unit_types::refinery;
				else if (st.race == race_protoss) refinery = unit_types::assimilator;
				else refinery = unit_types::extractor;
				if (st.minerals >= refinery->minerals_cost && !st.dont_build_refineries) {
					bool found = false;
					for (auto&v : st.bases) {
						for (auto&r : v.s->resources) {
							if (!r.u->type->is_gas) continue;
							//if (!st.resource_info.emplace(std::piecewise_construct, std::make_tuple(&r), std::make_tuple(&r)).second) continue;
							if (!st.resource_info.emplace(std::piecewise_construct, std::make_tuple(&r), std::make_tuple()).second) continue;
							found = true;
							break;
						}
						if (found) break;
					}
					//if (found && !nodep) {
					if (found) {
						//return unit_types::refinery;
						add_built(refinery, false);
						dont_free_workers = false;
						//log("refinery built\n");
						for (int i = 0; i < 3; ++i) free_worker(st, true);
						transfer_workers(st, false);
						transfer_workers(st, true);
						// 					if (no_refinery_depots) return failed;
						if (enable_verbose_build_logging) {
							for (int i = 0; i < buildst_log_depth; ++i) log(log_level_test, "  ");
							log(log_level_test, "advance '%s' -> add refinery, return nullptr?\n", build->name);
						}
						return nullptr;
						continue;
					}
				}
			}
			// TODO: properly calculate the required supply for morphing units, instead of just checking for mutalisk etc.
			if (build->unit) {
				if (build->unit->required_supply && build->unit->builder_type != unit_types::mutalisk && build->unit != unit_types::archon && build->unit != unit_types::dark_archon) {
					if (st.used_supply[build->unit->race] + build->unit->required_supply > 200) {
						if (enable_verbose_build_logging) {
							for (int i = 0; i < buildst_log_depth; ++i) log(log_level_test, "  ");
							log(log_level_test, "advance '%s' -> failed: maxed out\n", build->name);
						}
						return failed;
					}
					unit_type*supply_type;
					if (build->unit->race == race_terran) supply_type = unit_types::supply_depot;
					else if (build->unit->race == race_protoss) supply_type = unit_types::pylon;
					else supply_type = unit_types::overlord;
					if (st.max_supply[build->unit->race] > 10 || st.production.empty()) {
						if (st.used_supply[build->unit->race] + build->unit->required_supply > st.max_supply[build->unit->race]) {
							if (st.max_supply[build->unit->race] <= 10) {
								if (enable_verbose_build_logging) {
									for (int i = 0; i < buildst_log_depth; ++i) log(log_level_test, "  ");
									log(log_level_test, "advance '%s' -> req: supply_type (%s)\n", build->name, supply_type->name);
								}
								return build_type(supply_type);
							} else {
								add_built(supply_type, true);
							}
							continue;
						}
					}
				}
			}
			if (has_enough_minerals && has_enough_gas && !prereq_in_prod && !builder_in_prod) {
				st_unit*builder = nullptr;
				st_unit*builder2 = nullptr;
				st_unit*builder_without_addon = nullptr;
				bool builder_exists = false;
				if (build->builder == unit_types::larva) {
					for (int i = 0; i < 3 && !builder; ++i) {
						for (st_unit&u : st.units[i == 0 ? unit_types::hive : i == 1 ? unit_types::lair : unit_types::hatchery]) {
							if (u.larva_count) {
								builder = &u;
								break;
							}
						}
					}
					if (!builder && st.auto_build_hatcheries && st.minerals >= 300) {
						//add_built(unit_types::hatchery, true);
						add_built(unit_types::hatchery, false);
					}
				} else if (build->builder->is_addon) {
					for (st_unit&u : st.units[build->builder->builder_type]) {
						if (u.has_addon) {
							builder_exists = true;
							if (u.busy_until <= st.frame) {
								builder = &u;
								break;
							}
						}
					}
				} else {
					for (st_unit&u : st.units[build->builder]) {
						if (build->unit && build->unit->is_addon && u.has_addon) continue;
						if (!addon_required || u.has_addon) builder_exists = true;
						if (u.busy_until <= st.frame || u.type == unit_types::hatchery || u.type == unit_types::lair || u.type == unit_types::hive) {
							if (addon_required && !u.has_addon) builder_without_addon = &u;
							else {
								builder = &u;
								break;
							}
						}
					}
					if (builder && (build->unit == unit_types::archon || build->unit == unit_types::dark_archon)) {
						for (st_unit&u : st.units[build->builder]) {
							if (&u == builder) continue;
							builder2 = &u;
							break;
						}
						if (!builder2) {
							builder = nullptr;
							builder_exists = false;
						}
					}
				}
				if (build->builder == unit_types::larva) {
					if (!builder) {
						if (st.units[unit_types::hatchery].empty() && count_units_plus_production(st, unit_types::lair) == 0 && count_units_plus_production(st, unit_types::hive) == 0) {
							if (enable_verbose_build_logging) {
								for (int i = 0; i < buildst_log_depth; ++i) log(log_level_test, "  ");
								log(log_level_test, "advance '%s' -> req (larva): hatchery\n", build->name);
							}
							return build_type(unit_types::hatchery);
						};
					}
				} else if (!builder) {
					bool found = false;
					if (builder_without_addon) {
						// 						add_built(addon_required, false);
						// 						builder_without_addon->has_addon = true;
						// 						found = true;

						// 						for (auto&v : st.production) {
						// 							if (v.second == addon_required) {
						// 								found = true;
						// 								break;
						// 							}
						// 						}
					}
					if (!no_busywait) {
						for (auto&v : st.production) {
							if (v.second->unit == build->builder) {
								found = true;
								break;
							}
						}
					}
					if (!found) {
						if (no_busywait) {
							if (enable_verbose_build_logging) {
								for (int i = 0; i < buildst_log_depth; ++i) log(log_level_test, "  ");
								log(log_level_test, "advance '%s' -> failed: no builder, no_busywait\n", build->name);
							}
							return failed;
						}
						if (build->builder->is_worker) {
							if (enable_verbose_build_logging) {
								for (int i = 0; i < buildst_log_depth; ++i) log(log_level_test, "  ");
								log(log_level_test, "advance '%s' -> failed: no workers\n", build->name);
							}
							return failed;
						}
						if (!nodep) {
							// 							if (addon_required) return addon_required;
							// 							//if (addon_required) add_built(addon_required);
							// 							add_built(build->builder_type);
							// 							return nullptr;
							if (!builder_exists) {
								if (addon_required) {
									if (enable_verbose_build_logging) {
										for (int i = 0; i < buildst_log_depth; ++i) log(log_level_test, "  ");
										log(log_level_test, "advance '%s' -> req (builder): addon (%s)\n", build->name, addon_required->name);
									}
									return build_type(addon_required);
								}
								if (enable_verbose_build_logging) {
									for (int i = 0; i < buildst_log_depth; ++i) log(log_level_test, "  ");
									log(log_level_test, "advance '%s' -> req (builder): '%s'\n", build->name, build->builder->name);
								}
								return build_type(build->builder);
							}
						} else {
							if (!builder_exists) {
								if (enable_verbose_build_logging) {
									for (int i = 0; i < buildst_log_depth; ++i) log(log_level_test, "  ");
									log(log_level_test, "advance '%s' -> failed: nodep, no builder\n", build->name);
								}
								return failed;
							}
						}
					}
				}
				if (builder) {
					if (build->builder == unit_types::larva) {
						if (builder->larva_count == 3) builder->busy_until = st.frame + 333;
						--builder->larva_count;
					} else builder->busy_until = st.frame + build->build_time;
					if (build->unit == unit_types::nuclear_missile) builder->busy_until = st.frame + 15 * 60 * 30;
					st.production.emplace(st.frame + build->build_time, build);
					st.produced.emplace(st.frame, std::make_tuple(build, nullptr));
					if (build->unit && build->unit->is_two_units_in_one_egg) {
						st.production.emplace(st.frame + build->build_time, build);
						st.produced.emplace(st.frame, std::make_tuple(build, nullptr));
					}
					st.minerals -= build->minerals_cost;
					st.gas -= build->gas_cost;
					if (build->unit) {
						st.used_supply[build->unit->race] += build->unit->required_supply;
						if (build->unit->is_addon) builder->has_addon = true;
					}
					if (builder->type->race == race_zerg && builder->type != unit_types::hatchery && builder->type != unit_types::lair && builder->type != unit_types::hive) {
						if (build->unit) rm_unit_and_supply(st, builder->type);
					}
					if (build->unit == unit_types::archon || build->unit == unit_types::dark_archon) {
						rm_unit_and_supply(st, builder->type);
						rm_unit_and_supply(st, builder2->type);
					}
					if (enable_verbose_build_logging) {
						for (int i = 0; i < buildst_log_depth; ++i) log(log_level_test, "  ");
						log(log_level_test, "advance '%s' -> success\n", build->name);
					}
					//log("%s successfully built\n", build->name);
					return nullptr;
				}
			}
		}
		int f = std::min(15, end_frame - st.frame);

		double min_income = 0;
		double gas_income = 0;
		for (auto&v : st.resource_info) {
			auto&r = *v.first;
			double rate = r.income_rate[st.race];
			double w = std::min((double)v.second.workers, r.full_income_workers[st.race]);
			double inc = rate * w;
			if (v.second.workers > w) inc += rate * r.last_worker_mult[st.race];
			inc *= f;
			//if (v.second.last_seen_resources < inc) inc = v.second.last_seen_resources;
			double max_res = last_seen_resources[v.first->u];
			if (inc > max_res) inc = max_res;
			v.second.gathered += inc;
			if (r.u->type->is_gas) gas_income += inc;
			else min_income += inc;
		}
		st.minerals += min_income;
		st.gas += gas_income;
		st.total_minerals_gathered += min_income;
		st.total_gas_gathered += gas_income;
		st.frame += f;

		for (int i = 0; i < 3; ++i) {
			for (st_unit&u : st.units[i == 0 ? unit_types::hive : i == 1 ? unit_types::lair : unit_types::hatchery]) {
				if (st.frame >= u.busy_until && u.larva_count < 3) {
					++u.larva_count;
					u.busy_until = st.frame + 333;
				}
			}
		}

		if (st.frame >= end_frame) {
			if (enable_verbose_build_logging) {
				for (int i = 0; i < buildst_log_depth; ++i) log(log_level_test, "  ");
				log(log_level_test, "advance '%s' -> timeout\n", build ? build->name : "null");
			}
			return timeout;
		}

	}

};

void run(a_vector<state>&all_states, ruleset rules, bool is_for_me) {

	//int race = race_terran;

	unit_type*cc_type = unit_types::cc;
	unit_type*worker_type = unit_types::scv;

	if (is_for_me) {
		if (players::my_player->race == race_zerg) {
			cc_type = unit_types::hatchery;
			worker_type = unit_types::drone;
		}
	}

	auto initial_state = all_states.back();
	if (initial_state.bases.empty()) return;

	a_vector<refcounted_ptr<resource_spots::spot>> available_bases;
	for (auto&s : resource_spots::spots) {
		if (grid::get_build_square(s.cc_build_pos).building) continue;
		bool okay = true;
		for (auto&v : initial_state.bases) {
			if ((resource_spots::spot*)v.s == &s) okay = false;
		}
		if (!okay) continue;
		available_bases.push_back(&s);
	}

	auto get_next_base = [&]() {
		return get_best_score(available_bases, [&](resource_spots::spot*s) {
			double score = unit_pathing_distance(worker_type, s->cc_build_pos, initial_state.bases.front().s->cc_build_pos);
			double res = 0;
			if (is_for_me && initial_state.bases.size() > 1) {
// 				double ned = get_best_score_value(is_for_me ? enemy_units : my_units, [&](unit*e) {
// 					if (e->type->is_worker) return std::numeric_limits<double>::infinity();
// 					return diag_distance(s->pos - e->pos);
// 				});
// 				score -= ned;
				double ned = get_best_score_value(is_for_me ? enemy_buildings : my_buildings, [&](unit*e) {
					if (e->type->is_worker) return std::numeric_limits<double>::infinity();
					return diag_distance(s->pos - e->pos);
				});
				if (ned <= 32 * 30) score += 10000;
			}
			bool has_gas = false;
			for (auto&r : s->resources) {
				res += r.u->resources;
				if (r.u->type->is_gas) has_gas = true;
			}
			score -= (has_gas ? res : res / 4) / 10;
			//if (d == std::numeric_limits<double>::infinity()) d = 10000.0 + diag_distance(s->pos - st.bases.front().s->cc_build_pos);
			return score;
		}, std::numeric_limits<double>::infinity());
	};
	auto next_base = get_next_base();

	auto st = initial_state;
	unit_type*next_t = nullptr;
	a_vector<std::tuple<double, build_rule*, int>> sorted_build;
	//build_rule worker_rule{ unit_types::scv, 1.0, 1.0 };
// 	for (size_t i = 0; i < rules.build.size(); ++i) {
// 		sorted_build.emplace_back(0.0, &rules.build[i], 0);
// 	}
	while (st.frame < rules.end_frame) {
		multitasking::yield_point();

		int expand_frame = -1;
		if ((int)st.bases.size() < rules.bases && next_base) {
			unit_type*t = cc_type;
			if (advance(st, build_type(t), rules.end_frame, true, true)) st = all_states.back();
			else {
				auto s = next_base;
				add_base(st, *s);
				std::get<1>((--st.produced.end())->second) = s;
				find_and_erase(available_bases, s);

				next_base = get_next_base();

				all_states.push_back(st);
				continue;
			}
		}
		if (enable_verbose_build_logging) {
			for (int i = 0; i < buildst_log_depth; ++i) log(log_level_test, "  ");
			log(log_level_test, "run frame %d --\n", st.frame);
		}
		if (!rules.func(st)) {
			if (enable_verbose_build_logging) {
				log(log_level_test, "failed :(\n", st.frame);
			}
			break;
		}
		
		if (enable_verbose_build_logging) {
			log(log_level_test, "frame %d, minerals %g gas %g supply %g/%g\n", st.frame, st.minerals, st.gas, st.used_supply[st.race], st.max_supply[st.race]);
			log(log_level_test, " partial build order- ");
			for (auto&v : st.produced) {
				build_type bt = std::get<0>(v.second);
				log(log_level_test, " %s", bt->unit ? short_type_name(bt->unit) : bt->name);
			}
			log(log_level_test, "\n");
		}

		all_states.push_back(st);
	}
	if (all_states.back().frame < rules.end_frame) {
		auto st = all_states.back();
		while (st.frame < rules.end_frame) {
			advance(st, nullptr, rules.end_frame, false, false);
		}
		all_states.push_back(std::move(st));
	}
	//if (all_states.back().frame != rules.end_frame) xcept("all_states.back().frame is %d, expected %d", all_states.back().frame, rules.end_frame);

}

void add_builds(const state&st, int end_frame = current_frame + 15 * 90) {
	static void*flag = &flag;
	void*new_flag = &new_flag;

	std::unordered_map<unit_type*, int> two_units_in_one_egg_counter;
	for (auto&v : st.produced) {
		int frame = v.first;
		if (frame > end_frame) continue;
		build_type bt = std::get<0>(v.second);
		if (bt->unit && bt->unit->is_two_units_in_one_egg) {
			if (two_units_in_one_egg_counter[bt->unit]++ & 1) continue;
		}
		if ((bt->unit == unit_types::lair || bt->unit == unit_types::hive) && !my_units_of_type[unit_types::hive].empty()) continue;
		resource_spots::spot*s = std::get<1>(v.second);
		if (s) {
			if (bt->unit != unit_types::cc && bt->unit != unit_types::nexus && bt->unit != unit_types::hatchery) xcept("add_builds: resource_spot is set for %s", bt->name);
		}
		bool found = false;
		xy build_pos;
		if (s) build_pos = s->cc_build_pos;
		for (auto&t : build::build_tasks) {
			if (build_type(t.type) == bt && t.flag == flag) {
				t.flag = new_flag;
				if (t.priority != frame) build::change_priority(&t, frame);
				if (bt->unit && build_pos != xy()) {
					bool okay = true;
					for (int y = 0; y < bt->unit->tile_height && okay; ++y) {
						for (int x = 0; x < bt->unit->tile_width && okay; ++x) {
							auto&bs = grid::get_build_square(build_pos + xy(x * 32, y * 32));
							if (bs.reserved.first) okay = false;
						}
					}
					if (okay) {
						if (t.build_pos != xy()) grid::unreserve_build_squares(t.build_pos, bt->unit);
						t.build_pos = build_pos;
						grid::reserve_build_squares(build_pos, bt->unit);
					}
				}
				found = true;
				break;
			}
		}
		if (!found) {
			bool okay = true;
			if (bt->unit && build_pos != xy()) {
				for (int y = 0; y < bt->unit->tile_height && okay; ++y) {
					for (int x = 0; x < bt->unit->tile_width && okay; ++x) {
						auto&bs = grid::get_build_square(build_pos + xy(x * 32, y * 32));
						if (bs.reserved.first) okay = false;
					}
				}
			}
			if (okay) {
				auto&t = *(bt->unit ? build::add_build_task(frame, bt->unit) : build::add_build_task(frame, bt->upgrade));
				t.flag = new_flag;
				if (bt->unit && build_pos != xy()) {
					t.build_pos = build_pos;
					grid::reserve_build_squares(build_pos, bt->unit);
				}
			}
		}
	}
	a_vector<build::build_task*> dead_list;
	for (auto&t : build::build_tasks) {
		if (t.built_unit) continue;
		if (t.flag == flag) dead_list.push_back(&t);
		if (t.flag == new_flag) t.flag = flag;
	}
	for (auto*v : dead_list) build::cancel_build_task(v);
}

int count_units(const state& st, unit_type* ut) {
	auto i = st.units.find(ut);
	if (i == st.units.end()) return 0;
	return i->second.size();
};
int count_production(const state& st, unit_type* ut) {
	int r = 0;
	for (auto& v : st.production) {
		if (v.second->unit == ut) ++r;
	}
	if (ut->is_two_units_in_one_egg) r *= 2;
	return r;
}
int count_units_plus_production(const state& st, unit_type* ut) {
	return count_units(st, ut) + count_production(st, ut);
};

bool is_upgrading(const state& st, upgrade_type* upg) {
	for (auto& v : st.production) {
		if (v.second->upgrade == upg) return true;
	}
	return false;
}

bool has_upgrade(const state& st, upgrade_type* upg) {
	return st.upgrades.find(upg) != st.upgrades.end();
}

bool has_or_is_upgrading(const state&st, upgrade_type*upg) {
	return st.upgrades.find(upg) != st.upgrades.end() || is_upgrading(st, upg);
}

auto depbuild_until(state&st, const state&prev_st, build_type_autocast bt, int end_frame) {
	if (&st == &prev_st) xcept("&st == &prev_st");
	build_type t = bt;
	if (enable_verbose_build_logging) {
		for (int i = 0; i < buildst_log_depth; ++i) log(log_level_test, "  ");
		log(log_level_test, "depbuild '%s'\n", t->name);
	}
	while (true) {
		if (enable_verbose_build_logging) ++buildst_log_depth;
		auto built_t = t;
		t = advance(st, t, end_frame, false, false);
		if (enable_verbose_build_logging) {
			--buildst_log_depth;
			log("advance returned %p (%s)\n", t, t == nullptr ? "null" : t == failed ? "failed" : t == timeout ? "timeout" : t->name);
		}
		if (t) {
			st = prev_st;
			if (t == failed) {
				if (enable_verbose_build_logging) {
					for (int i = 0; i < buildst_log_depth; ++i) log(log_level_test, "  ");
					log(log_level_test, "depbuild '%s': failed\n", bt->name);
				}
				return false;
			}
			if (t == timeout) {
				if (enable_verbose_build_logging) {
					for (int i = 0; i < buildst_log_depth; ++i) log(log_level_test, "  ");
					log(log_level_test, "depbuild '%s': timeout\n", bt->name);
				}
				return false;
			}
			if (t->unit && t->unit->is_worker) {
				if (enable_verbose_build_logging) {
					for (int i = 0; i < buildst_log_depth; ++i) log(log_level_test, "  ");
					log(log_level_test, "depbuild '%s': failing because of worker req\n", bt->name);
				}
				return false;
			}
			if (enable_verbose_build_logging) {
				for (int i = 0; i < buildst_log_depth; ++i) log(log_level_test, "  ");
				log(log_level_test, "depbuild '%s': req '%s'\n", bt->name, t->name);
			}
			continue;
		}
		if (enable_verbose_build_logging) {
			for (int i = 0; i < buildst_log_depth; ++i) log(log_level_test, "  ");
			log(log_level_test, "depbuild '%s': successfully built '%s'\n", bt->name, built_t->name);
		}
		return true;
	}
};
static const int advance_frame_amount = 15 * 60 * 10;
auto depbuild(state&st, const state&prev_st, build_type_autocast bt) {
	return depbuild_until(st, prev_st, bt, st.frame + advance_frame_amount);
};
auto depbuild_now(state&st, const state&prev_st, build_type_autocast bt) {
	return depbuild_until(st, prev_st, bt, st.frame + 4);
};
auto build_now(state&st, build_type_autocast bt) {
	return advance(st, bt, st.frame + 4, true, false) == nullptr;
};
auto build_nodep(state&st, build_type_autocast bt) {
	return advance(st, bt, st.frame + advance_frame_amount, true, false) == nullptr;
};
auto could_build_instead = [](state&st, build_type_autocast bt) {
	build_type lt = (--st.production.end())->second;
	return (bt->minerals_cost == 0 || st.minerals + lt->minerals_cost >= bt->minerals_cost) && (bt->gas_cost == 0 || st.gas + lt->gas_cost >= bt->gas_cost);
};
auto nodelay_stage2 = [](state&st, state imm_st, build_type_autocast bt, int n, const std::function<bool(state&)>&func) {
	if (enable_verbose_build_logging) {
		for (int i = 0; i < buildst_log_depth; ++i) log(log_level_test, "  ");
		log(log_level_test, "nodelay_stage2 '%s'\n", bt->name);
		++buildst_log_depth;
	}
	if (func(st)) {
		if (enable_verbose_build_logging) --buildst_log_depth;
		if (st.frame >= imm_st.frame) {
			if (enable_verbose_build_logging) {
				for (int i = 0; i < buildst_log_depth; ++i) log(log_level_test, "  ");
				log(log_level_test, "nodelay_stage2 '%s': choose depbuild; imm equal or better\n", bt->name);
			}
			st = std::move(imm_st);
			return true;
		}
		auto del_st = st;
		if (enable_verbose_build_logging) ++buildst_log_depth;
		if (depbuild(st, del_st, bt)) {
			if (enable_verbose_build_logging) --buildst_log_depth;
			if (st.frame <= imm_st.frame + n) {
				if (enable_verbose_build_logging) {
					for (int i = 0; i < buildst_log_depth; ++i) log(log_level_test, "  ");
					log(log_level_test, "nodelay_stage2 '%s': choose func\n", bt->name);
				}
				st = std::move(del_st);
				return true;
			} else {
				if (enable_verbose_build_logging) {
					for (int i = 0; i < buildst_log_depth; ++i) log(log_level_test, "  ");
					log(log_level_test, "nodelay_stage2 '%s': choose depbuild\n", bt->name);
				}
				st = std::move(imm_st);
				return true;
			}
		} else {
			if (enable_verbose_build_logging) {
				--buildst_log_depth;
				for (int i = 0; i < buildst_log_depth; ++i) log(log_level_test, "  ");
				log(log_level_test, "nodelay_stage2 '%s': depbuild failed\n", bt->name);
			}
			st = std::move(imm_st);
			return true;
		}
	} else {
		if (enable_verbose_build_logging) {
			--buildst_log_depth;
			for (int i = 0; i < buildst_log_depth; ++i) log(log_level_test, "  ");
			log(log_level_test, "nodelay_stage2 '%s': func failed\n", bt->name);
		}
		st = std::move(imm_st);
		return true;
	}
};
auto nodelay_n = [](state&st, build_type_autocast bt, int n, const std::function<bool(state&)>&func) {
	if (enable_verbose_build_logging) {
		for (int i = 0; i < buildst_log_depth; ++i) log(log_level_test, "  ");
		log(log_level_test, "nodelay '%s'\n", bt->name);
	}
	auto prev_st = st;
	if (enable_verbose_build_logging) ++buildst_log_depth;
	if (depbuild(st, prev_st, bt)) {
		//if (st.frame <= prev_st.frame + 15) return true;
		if (enable_verbose_build_logging) --buildst_log_depth;
		auto imm_st = st;
		st = std::move(prev_st);
		if (enable_verbose_build_logging) ++buildst_log_depth;
		auto r = nodelay_stage2(st, std::move(imm_st), bt, n, func);
		if (enable_verbose_build_logging) --buildst_log_depth;
		return r;
	} else {
		if (enable_verbose_build_logging) {
			--buildst_log_depth;
			for (int i = 0; i < buildst_log_depth; ++i) log(log_level_test, "  ");
			log(log_level_test, "nodelay '%s': depbuild failed\n", bt->name);
		}
		st = std::move(prev_st);
		if (bt->unit && bt->unit->required_supply && st.used_supply[bt->unit->race] + bt->unit->required_supply > 200) return false;
		if (enable_verbose_build_logging) ++buildst_log_depth;
		auto r = func(st);
		if (enable_verbose_build_logging) --buildst_log_depth;
		return r;
	}
};
auto nodelay(state&st, build_type_autocast bt, const std::function<bool(state&)>&func) {
// 	if (bt->unit && bt->unit->is_worker && bt->unit != unit_types::drone && st.units[bt->unit].size() >= 70) return func(st);
// 	if (bt->unit && bt->unit->is_worker && bt->unit != unit_types::drone) {
// 		if (count_production(st, bt->unit) >= 3) return func(st);
// 	}
	return nodelay_n(st, bt, 0, func);
};

auto maxprod(state&st, build_type_autocast bt, const std::function<bool(state&)>&func) {
	unit_type*prod_type = bt->builder;
	unit_type*addon = nullptr;
	if (bt->unit == unit_types::siege_tank_tank_mode) addon = unit_types::machine_shop;
	if (bt->unit == unit_types::dropship || bt->unit == unit_types::science_vessel || bt->unit == unit_types::battlecruiser || bt->unit == unit_types::valkyrie) addon = unit_types::control_tower;
	if (addon) {
		prod_type = addon;
	}
	auto prev_st = st;
	build_type t = advance(st, bt, st.frame + advance_frame_amount, true, true);
	if (!t) {
		auto imm_st = st;
		st = std::move(prev_st);
		return nodelay_stage2(st, std::move(imm_st), bt, 0, func);
	} else {
		st = std::move(prev_st);
		if (t != failed) return nodelay(st, bt, func);
		if (bt->unit && bt->unit->required_supply && st.used_supply[bt->unit->race] + bt->unit->required_supply > 200) return false;
		for (auto&v : st.units[bt->builder]) {
			if (st.frame >= v.busy_until && (!addon || v.has_addon)) return nodelay(st, bt, func);
		}
		if (count_production(st, prod_type) >= 2 || (st.minerals < bt->minerals_cost && st.gas < bt->gas_cost)) return nodelay(st, bt, func);
		return nodelay(st, prod_type, func);
	}
};
auto maxprod1(state&st, build_type_autocast bt) {
	unit_type*prod_type = bt->builder;
	unit_type*addon = nullptr;
	if (bt->unit == unit_types::siege_tank_tank_mode) addon = unit_types::machine_shop;
	if (bt->unit == unit_types::dropship || bt->unit == unit_types::science_vessel || bt->unit == unit_types::battlecruiser || bt->unit == unit_types::valkyrie) addon = unit_types::control_tower;
	if (addon) {
		bt = addon;
	}
	auto prev_st = st;
	build_type t = advance(st, bt, st.frame + advance_frame_amount, true, true);
	if (!t) return true;
	else {
		st = std::move(prev_st);
		if (t != failed) return depbuild(st, state(st), bt);
		if (bt->unit && bt->unit->required_supply && st.used_supply[bt->unit->race] + bt->unit->required_supply > 200) return false;
		for (auto&v : st.units[bt->builder]) {
			if (st.frame >= v.busy_until) return depbuild(st, state(st), bt);
		}
		if (count_production(st, prod_type) >= 2 || (st.minerals < bt->minerals_cost && st.gas < bt->gas_cost)) return depbuild(st, state(st), bt);
		return depbuild(st, state(st), prod_type);
	}
};


struct variant {
	bool expand;
	//a_vector<build_rule> build;
	std::function<bool(state&)> func;
	double score = 0.0;
};
a_vector<variant> variants;

void init_variants() {
	variant v;
	v.expand = false;

	v.func = [](state&st) {
		unit_type*ut = unit_types::scv;
		if (!st.units[unit_types::nexus].empty()) ut = unit_types::probe;
		else if (!st.units[unit_types::hatchery].empty()) ut = unit_types::drone;
		if (st.units[ut].size() < 70) return depbuild(st, state(st), ut);
		return advance(st, nullptr, st.frame + 15 * 5, false, false) == timeout;
	};
	variants.push_back(v);
}

a_vector<std::tuple<variant, state, xy>> opponent_states;
multitasking::mutex opponent_states_mut;

struct unit_match_info {
	unit_type*type = nullptr;
	double minerals_value = 0.0;
	double gas_value = 0.0;
	bool dead = false;
};
a_unordered_map<unit*, unit_match_info> umi_map;
a_unordered_map<unit_type*, size_t> umi_live_count;

struct umi_update_t {
	unit*u;
	unit_type*type;
	double minerals_value;
	double gas_value;
	bool dead;
};

void free_up_resources(state&st, double minerals, double gas) {
	if ((minerals == 0 || st.minerals >= minerals) && (gas == 0 || st.gas >= gas)) return;
	log(" -- st %p -- free up resources - need %g %g, have %g %g\n", &st, minerals, gas, st.minerals, st.gas);
	for (auto i = st.produced.rbegin(); i != st.produced.rend() && (st.minerals < minerals || st.gas < gas);) {
		build_type bt = std::get<0>(i->second);
		bool rm = false;
		if (bt->unit) {
			auto&vec = st.units[bt->unit];
			if (vec.size() > umi_live_count[bt->unit]) {
				if (minerals && minerals > st.minerals && bt->unit->total_minerals_cost) rm = true;
				if (gas && gas > st.gas && bt->unit->total_gas_cost) rm = true;
			}
		}
		if (rm) {
			st.minerals += bt->unit->total_minerals_cost;
			st.gas += bt->unit->total_gas_cost;
			//if (ut->required_supply) st.used_supply[ut->race] -= ut->required_supply;
			//if (ut->provided_supply) st.max_supply[ut->race] -= ut->provided_supply;
			rm_unit_and_supply(st, bt->unit);
			log(" -- st %p -- %s removed to free up resources\n", &st, bt->name);
			auto ri = i;
			st.produced.erase((++ri).base());
			i = st.produced.rbegin();
			//vec.pop_back();
		} else ++i;
	}
	log(" -- st %p -- freed up - have %g %g\n", &st, st.minerals, st.gas);
};

void umi_reset() {
	umi_map.clear();
	umi_live_count.clear();
}

void umi_update(umi_update_t upd) {
	auto umi_add_unit = [](unit_type*ut, double minerals, double gas, bool always_add) {
		size_t&live_count = umi_live_count[ut];
		for (auto&v : opponent_states) {
			auto&st = std::get<1>(v);
			auto&vec = st.units[ut];
			if (always_add) {
				free_up_resources(st, minerals, gas);
				add_unit_and_supply(st, ut);
				st.minerals -= minerals;
				st.gas -= gas;
				log(" -- st %p -- %s force added\n", &st, ut->name);
			} else {
				if (vec.size() > live_count) {
					log(" -- st %p -- %s matched\n", &st, ut->name);
// 					st.minerals += ut->total_minerals_cost;
// 					st.gas += ut->total_gas_cost;
				} else {
					free_up_resources(st, minerals, gas);
					add_unit_and_supply(st, ut);
					st.minerals -= minerals;
					st.gas -= gas;
					log(" -- st %p -- %s added (no match)\n", &st, ut->name);
				}
			}
		}
		++live_count;
	};
	auto umi_rm_unit = [](unit_type*ut, double minerals, double gas) {
		for (auto&v : opponent_states) {
			auto&st = std::get<1>(v);
			log(" -- st %p -- %s removed\n", &st, ut->name);
			rm_unit_and_supply(st, ut);
			st.minerals += minerals;
			st.gas += gas;
		}
		--umi_live_count[ut];
	};
	std::function<void(unit_type*)> add_prereq = [&](unit_type*ut) {
		if (ut->is_worker) return;
		for (auto&v : opponent_states) {
			auto&st = std::get<1>(v);
			for (unit_type*prereq_type : ut->required_units) {
				if (st.units[prereq_type].empty()) {
					free_up_resources(st, prereq_type->total_minerals_cost, prereq_type->total_gas_cost);
					add_unit_and_supply(st, ut);
					st.minerals -= prereq_type->total_minerals_cost;
					st.gas -= prereq_type->total_gas_cost;
					log(" -- st %p -- %s added as prerequisite\n", &st, ut->name);
				}
			}
		}
	};
	auto&umi = umi_map[upd.u];
	if (umi.dead) {
		log(" !! umi_update for dead unit (upd.dead: %d)", upd.dead);
		return;
	}
	//if (umi.dead) xcept("umi_update for dead unit (u->dead: %d)", u->dead);
	if (upd.type != umi.type) {
		bool is_new = true;
		bool is_fast_expo = false;
		if (umi.type) {
			umi_rm_unit(umi.type, umi.minerals_value, umi.gas_value);
			is_new = false;
		} else {
			if (upd.type->is_resource_depot && upd.u->building) {
				auto is_start_location = [&](xy pos) {
					return test_pred(start_locations, [&](xy start_pos) {
						return start_pos == pos;
					});
				};
				if (umi_live_count[upd.type] == 0 && !is_start_location(upd.u->building->build_pos)) is_fast_expo = true;
			}
		}
		umi.type = upd.type;
		umi.minerals_value = upd.minerals_value;
		umi.gas_value = upd.gas_value;
		umi_add_unit(umi.type, umi.minerals_value, umi.gas_value, !is_new);
		add_prereq(umi.type);
		if (umi.type->is_refinery) {
			
		}
	}
	if (upd.dead) {
		umi_rm_unit(umi.type, 0, 0);
		umi.dead = true;
	}

	for (auto&v : opponent_states) {
		auto&st = std::get<1>(v);
		log(" -- st %p -- min %g gas %g supply %g/%g\n", &st, st.minerals, st.gas, st.used_supply[st.race], st.max_supply[st.race]);

		for (auto&v : st.units) {
			unit_type*ut = std::get<0>(v);
			auto&vec = std::get<1>(v);
			if (vec.size() < umi_live_count[ut]) xcept("umi_live_count mismatch in state %p for %s - %d vs %d\n", &st, ut->name, vec.size(), umi_live_count[ut]);
		}
	}
	log("umi_update: %s updated -- \n", umi.type->name);
}

a_vector<umi_update_t> umi_update_history;
a_vector<umi_update_t> umi_update_queue;

umi_update_t get_umi_update(unit*u) {
	umi_update_t r;
	r.u = u;
	r.type = u->type;
	r.minerals_value = u->minerals_value;
	r.gas_value = u->gas_value;
	r.dead = u->dead;
	return r;
}

void on_new_unit(unit*u) {
	log("new unit: %s\n", u->type->name);
	if (!u->owner->is_enemy) return;
	if (u->type->is_non_usable) return;
	umi_update_queue.push_back(get_umi_update(u));
	umi_update_history.push_back(get_umi_update(u));
}
void on_type_change(unit*u, unit_type*old_type) {
	log("unit type %s changed to %s\n", old_type->name, u->type->name);
	if (!u->owner->is_enemy) return;
	if (u->type->is_non_usable) return;
	umi_update_queue.push_back(get_umi_update(u));
	umi_update_history.push_back(get_umi_update(u));
}
void on_destroy(unit*u) {
	log("unit %s was destroyed\n", u->type->name);
	if (!u->owner->is_enemy) return;
	if (u->type->is_non_usable) return;
	umi_update_queue.push_back(get_umi_update(u));
	umi_update_history.push_back(get_umi_update(u));
}

void update_opponent_builds() {
	std::lock_guard<multitasking::mutex> l(opponent_states_mut);
	if (opponent_states.empty()) return;
	if (!umi_update_queue.empty()) {
		static a_vector<umi_update_t> queue;
		queue.clear();
		std::swap(queue, umi_update_queue);
		for (auto&upd : queue) {
			umi_update(upd);
		}
	}
	for (auto&s : resource_spots::spots) {
		bool has_this_base = false;
		for (unit*e : enemy_buildings) {
			if (!e->building || !e->type->is_resource_depot) continue;
			if (diag_distance(e->building->build_pos - s.cc_build_pos) > 32 * 8) continue;
			has_this_base = true;
			break;
		}
		if (!has_this_base && !grid::is_visible(s.cc_build_pos, 4, 3)) continue;
		if (!has_this_base && game->isReplay()) continue;
		for (auto&v : opponent_states) {
			auto&st = std::get<1>(v);
			bool st_has_this_base = false;
			for (auto&v : st.bases) {
				if (&*v.s == &s) {
					st_has_this_base = true;
					if (has_this_base && !v.verified) v.verified = true;
					break;
				}
			}
			if (has_this_base != st_has_this_base) {
				if (has_this_base) {
					log(" -- st %p - add base %p\n", &st, &s);
					add_base(st, s).verified = true;
					for (auto&v : st.bases) {
						if (!v.verified) {
							log(" -- st %p - remove unverified base %p\n", &st, &*v.s);
							rm_base(st, *v.s);
							break;
						}
					}
				} else {
					log(" -- st %p - remove base %p\n", &st, &s);
					rm_base(st, s);
				}
			}
		}
	}
	for (auto i = opponent_states.begin(); i != opponent_states.end();) {
		auto&st = std::get<1>(*i);
		if (st.bases.empty() && opponent_states.size() > 1) i = opponent_states.erase(i);
		else ++i;
	}
// 	for (auto&v : opponent_states) {
// 		auto&st = std::get<1>(v);
// 		if (!st.bases.empty()) continue;
// 		resource_spots::spot*s = nullptr;
// 		for (int i = 0; i < 10 && (!s || grid::is_visible(s->cc_build_pos)); ++i) s = starting_spots[rng(starting_spots.size())];
// 		if (s) {
// 			add_base(st, *s);
// 			if (st.minerals < 0) {
// 				for (auto&v : st.resource_info) {
// 					if (v.first->u->type->is_gas) continue;
// 					v.second.gathered = -st.minerals;
// 					st.total_minerals_gathered += -st.minerals;
// 					st.minerals = 0;
// 					break;
// 				}
// 			}
// 		}
// 	}

	for (auto&v : opponent_states) {
		auto&st = std::get<1>(v);
		for (auto&b : st.bases) {
			for (auto&r : b.s->resources) {
				if (r.u->owner == players::opponent_player) {
					//st.resource_info.emplace(&r, &r);
					st.resource_info[&r];
				}
			}
		}
	}

	for (auto&v : opponent_states) {
		auto&st = std::get<1>(v);
		a_map<resource_spots::spot*, int> count[2];
		a_map<resource_spots::spot*, double> sum[2];
		for (auto&v : st.resource_info) {
			++count[v.first->u->type->is_gas][v.first->s];
			sum[v.first->u->type->is_gas][v.first->s] += v.second.gathered;
		}
		for (auto&v : st.resource_info) {
			v.second.gathered = sum[v.first->u->type->is_gas][v.first->s] / count[v.first->u->type->is_gas][v.first->s];

			//log(" resource-- %p gathered is %g\n", v.first->u, v.second.gathered);
		}
	}

	// TODO: Give all resources not mined by me to the opponent.
	//       Not just from bases he owns.
	a_unordered_set<unit*> update_res;
// 	for (auto&r : resource_spots::all_resources) {
// 		update_res.insert(r.u);
// 	}
	for (auto&v : opponent_states) {
		auto&st = std::get<1>(v);
		//double total_adjust[2] = { 0, 0 };
		for (auto&v : st.resource_info) {
			update_res.insert(v.first->u);
		}
	}

	for (auto&v : opponent_states) {
		auto&st = std::get<1>(v);
		double total_adjust[2] = { 0, 0 };
		for (auto&v : st.resource_info) {
			if (!update_res.count(v.first->u)) continue;
			double res = v.first->u->resources;
			if (v.first->u->dead || v.first->u->gone) res = 0.0;
			double last_seen = last_seen_resources[v.first->u];
			if (res == last_seen) continue;
			double actual_gathered = last_seen - res;
			double adjust = actual_gathered - v.second.gathered;
			log(" resource-- %p - last_seen_resources is %g, res is %g, gathered is %g, actual_gathered is %g\n", v.first->u, last_seen, res, v.second.gathered, actual_gathered);
			v.second.gathered = 0.0;
			total_adjust[v.first->u->type->is_gas] += adjust;
		}
		for (auto i = st.resource_info.begin(); i != st.resource_info.end();) {
			if (i->first->u->dead || i->first->u->gone) {
				st.idle_workers += i->second.workers;
				i = st.resource_info.erase(i);
			} else ++i;
		}
		if (total_adjust[0] || total_adjust[1]) {
			log(" -- adjust resources -- %+g minerals %+g gas\n", total_adjust[0], total_adjust[1]);
			st.minerals += total_adjust[0];
			st.gas += total_adjust[1];
			st.total_minerals_gathered += total_adjust[0];
			st.total_gas_gathered += total_adjust[1];

			if (st.minerals < 0 || st.gas < 0) {
				free_up_resources(st, 1, 1);
			}

			if (total_adjust[1] >= 200) {
				for (int i = 0; i < 3; ++i) free_worker(st, true);
				transfer_workers(st, false);
				transfer_workers(st, true);
			} else if (total_adjust[1] < -200) {
				for (int i = 0; i < 3; ++i) free_worker(st, false);
				transfer_workers(st, true);
				transfer_workers(st, false);
			}
		}
	}

	//for (unit*u : update_res) {
	for (auto&r : resource_spots::all_resources) {
		unit*u = r.u;
		last_seen_resources[u] = u->dead || u->gone ? 0 : u->resources;
	}
	
}

auto rules_from_variant = [&](const state&st, variant var, int end_frame) {
	size_t verified_bases = 0;
	for (auto&v : st.bases) {
		if (v.verified) ++verified_bases;
	}
	ruleset rules;
	rules.end_frame = end_frame;
	rules.bases = verified_bases + (var.expand ? 1 : 0);
	//rules.build = var.build;
	rules.func = var.func;
	return rules;
};

bool found_op_base = false;

a_vector<state> run_opponent_builds(int end_frame) {
	a_vector<state> r;
	for (auto&v : opponent_states) {
		variant&var = std::get<0>(v);
		auto&st = std::get<1>(v);
		a_vector<state> all_states;
		all_states.push_back(st);
		run(all_states, rules_from_variant(all_states.back(), var, end_frame), false);
		r.push_back(std::move(all_states.back()));
	}
	return r;
}

void progress_opponent_builds() {
	auto reset = []() {
		log("opponent states reset\n");
		opponent_states.clear();
		last_seen_resources.clear();
		for (auto&r : resource_spots::all_resources) {
			last_seen_resources[r.u] = r.u->resources;
		}
		umi_reset();
		umi_update_queue = umi_update_history;
	};
	if (current_frame < 15 * 30) reset();

// 	starting_spots.clear();
// 	for (auto&v : game->getStartLocations()) {
// 		bwapi_pos p = v;
// 		xy start_pos(p.x * 32, p.y * 32);
// 		auto&bs = grid::get_build_square(start_pos);
// 		auto*s = get_best_score_p(resource_spots::spots, [&](resource_spots::spot*s) {
// 			return diag_distance(start_pos - s->cc_build_pos);
// 		});
// 		if (s) {
// 			if (!bs.building) starting_spots.push_back(s);
// 			if (bs.building && bs.building->owner->is_enemy) {
// 				starting_spots.clear();
// 				starting_spots.push_back(s);
// 				break;
// 			}
// 		}
// 	}

// 	refcounted_ptr<resource_spots::spot> override_spot;
// 	if (!opponent_states.empty()) {
// 		auto&op_st = std::get<1>(opponent_states.front());
// 		bool has_verified_base = test_pred(op_st.bases, [&](st_base&v) {
// 			return v.verified;
// 		});
// 		if (!has_verified_base) found_op_base = false;
// 		else if (!found_op_base) {
// 			found_op_base = true;
// 			override_spot = op_st.bases.front().s;
// 			reset();
// 		}
// 	}

	xy opponent_start_location;

	if (game->isReplay()) opponent_start_location = xy(((bwapi_pos)players::opponent_player->game_player->getStartLocation()).x * 32, ((bwapi_pos)players::opponent_player->game_player->getStartLocation()).y * 32);
	else {
		for (xy pos : start_locations) {
			auto&bs = grid::get_build_square(pos);
			if (bs.building && bs.building->owner->is_enemy) {
				opponent_start_location = pos;
				break;
			}
		}
	}

	if (opponent_start_location != xy()) {
		for (auto i = opponent_states.begin(); i != opponent_states.end();) {
			xy start_loc = std::get<2>(*i);
			if (start_loc != opponent_start_location && opponent_states.size() > 1) i = opponent_states.erase(i);
			else ++i;
		}
	}

	{
		std::lock_guard<multitasking::mutex> l(opponent_states_mut);
		if (opponent_states.empty()) {
// 			resource_spots::spot*s = nullptr;
// 			if (override_spot) s = override_spot;
// 			else for (int i = 0; i < 10 && (!s || grid::is_visible(s->cc_build_pos)); ++i) s = starting_spots[rng(starting_spots.size())];
			for (xy start_pos : start_locations) {
				if (start_pos == my_start_location) continue;
				resource_spots::spot*s = get_best_score_p(resource_spots::spots, [&](auto*s) {
					return diag_distance(s->cc_build_pos - start_pos);
				});
				if (!s) continue;
				for (auto&var : variants) {
					opponent_states.emplace_back();
					std::get<0>(opponent_states.back()) = var;
					state initial_state;
					initial_state.frame = 0;
					initial_state.minerals = 50;
					initial_state.gas = 0;
					initial_state.used_supply = { 0, 0, 0 };
					initial_state.max_supply = { 0, 0, 0 };
					add_base(initial_state, *s).verified = true;
					if (players::opponent_player->race == race_terran) {
						add_unit_and_supply(initial_state, unit_types::cc);
						add_unit_and_supply(initial_state, unit_types::scv);
						add_unit_and_supply(initial_state, unit_types::scv);
						add_unit_and_supply(initial_state, unit_types::scv);
						add_unit_and_supply(initial_state, unit_types::scv);
					} else if (players::opponent_player->race == race_protoss) {
						add_unit_and_supply(initial_state, unit_types::nexus);
						add_unit_and_supply(initial_state, unit_types::probe);
						add_unit_and_supply(initial_state, unit_types::probe);
						add_unit_and_supply(initial_state, unit_types::probe);
						add_unit_and_supply(initial_state, unit_types::probe);
					} else {
						add_unit_and_supply(initial_state, unit_types::hatchery);
						add_unit_and_supply(initial_state, unit_types::drone);
						add_unit_and_supply(initial_state, unit_types::drone);
						add_unit_and_supply(initial_state, unit_types::drone);
						add_unit_and_supply(initial_state, unit_types::drone);
						add_unit_and_supply(initial_state, unit_types::overlord);
					}
					std::get<1>(opponent_states.back()) = std::move(initial_state);
				}
			}

		}

		for (auto&v : opponent_states) {
			variant&var = std::get<0>(v);
			auto&st = std::get<1>(v);
			a_vector<state> all_states;
			all_states.push_back(st);
			run(all_states, rules_from_variant(all_states.back(), var, current_frame), false);
			state*closest = &all_states.front();
			for (auto&v : all_states) {
				if (v.frame <= current_frame) closest = &v;
				else break;
			}
			st = std::move(*closest);
			//if (st.frame != current_frame) advance(st, nullptr, current_frame, false, false);
			{
				log("min %g gas %g supply %g/%g  bases %d\n", st.minerals, st.gas, st.used_supply[st.race], st.max_supply[st.race], st.bases.size());
				log("opponent state (frame %d)--- \n", st.frame);
				for (auto&v : st.units) {
					if (!v.first || !v.second.size()) continue;
					log(" %dx%s", v.second.size(), short_type_name(v.first));
				}
				log("\n");
			}
		}
	}
}


state get_my_current_state() {
	state initial_state;
	initial_state.race = players::my_player->race;
	initial_state.frame = current_frame;
	initial_state.minerals = current_minerals;
	initial_state.gas = current_gas;
	initial_state.used_supply = current_used_supply;
	initial_state.max_supply = current_max_supply;

	for (auto&s : resource_spots::spots) {
		//for (unit*u : my_resource_depots) {
		for (unit*u : my_buildings) {
			if (!u->type->is_resource_depot) continue;
			if (u->building->is_lifted) continue;
			if (diag_distance(u->building->build_pos - s.cc_build_pos) <= 32 * 4) {
				add_base(initial_state, s).verified = true;
				break;
			}
		}
	}
	for (unit*u : my_units) {
		if (u->game_unit->isUpgrading()) {
			auto game_upg = u->game_unit->getUpgrade();
			auto upg = upgrades::get_upgrade_type(game_upg, players::my_player->game_player->getUpgradeLevel(game_upg) + 1);
			initial_state.production.emplace(initial_state.frame + u->game_unit->getRemainingUpgradeTime(), build_type(upg));
		}
		if (u->game_unit->isResearching()) {
			auto upg = upgrades::get_upgrade_type(u->game_unit->getTech());
			initial_state.production.emplace(initial_state.frame + u->game_unit->getRemainingResearchTime(), build_type(upg));
		}

		//if (!u->is_completed) continue;
		if (u->type->is_addon) continue;
		if (u->type == unit_types::larva) continue;
		unit_type*ut = u->type;
		if (ut->game_unit_type == BWAPI::UnitTypes::Zerg_Egg) {
			ut = units::get_unit_type(u->game_unit->getBuildType());
			if (!ut) continue;
			if (ut->is_two_units_in_one_egg && u->is_completed && !u->is_morphing) add_unit(initial_state, ut);
		}
		if ((!u->is_completed || u->is_morphing) && (!ut->provided_supply || ut->is_resource_depot)) {
			initial_state.production.emplace(initial_state.frame + u->remaining_build_time, build_type(ut));
			continue;
		}
		auto&st_u = add_unit(initial_state, ut);
		if (u->addon) st_u.has_addon = true;
		if (u->has_nuke) st_u.busy_until = current_frame + 15 * 60 * 30;
		if ((!u->is_completed || u->is_morphing) && ut->provided_supply) {
			initial_state.max_supply[ut->race] += ut->provided_supply;
		}
		if (u->type == unit_types::hatchery || u->type == unit_types::lair || u->type == unit_types::hive) {
			st_u.busy_until = initial_state.frame + std::max(u->remaining_build_time, std::max(u->remaining_research_time, u->remaining_upgrade_time));
		} else if (u->remaining_whatever_time) st_u.busy_until = initial_state.frame + u->remaining_whatever_time;
		st_u.larva_count = u->game_unit->getLarva().size();
		if (ut->is_gas) {
			for (auto&r : resource_spots::live_resources) {
				if (r.u == u) {
					//initial_state.resource_info.emplace(&r, &r);
					initial_state.resource_info[&r];
					break;
				}
			}
		}
	}
	for (auto&v : players::my_player->upgrades) {
		initial_state.upgrades.insert(v);
	}

	if (initial_state.bases.empty()) {
		auto*s = get_best_score_p(resource_spots::spots, [&](resource_spots::spot*s) {
			return get_best_score(make_transform_filter(my_resource_depots, [&](unit*u) {
				return unit_pathing_distance(unit_types::scv, u->pos, s->cc_build_pos);
			}), identity<double>());
		});
		if (s) add_base(initial_state, *s).verified = true;
	}
	return initial_state;
}
state get_op_current_state() {
	state initial_state;
	initial_state.frame = current_frame;
	initial_state.minerals = 0;
	initial_state.gas = 0;
	initial_state.used_supply = { 0, 0, 0 };
	initial_state.max_supply = { 0, 0, 0 };

	for (auto&s : resource_spots::spots) {
		for (unit*u : enemy_units) {
			if (!u->type->is_resource_depot) continue;
			if (diag_distance(u->building->build_pos - s.cc_build_pos) <= 32 * 4) {
				add_base(initial_state, s).verified = true;
				break;
			}
		}
	}
	for (unit*u : enemy_units) {
		if (u->type->provided_supply) initial_state.max_supply[u->type->race] += u->type->provided_supply;
		if (u->type->required_supply) initial_state.used_supply[u->type->race] += u->type->required_supply;
		//if (!u->is_completed) continue;
		if (u->type->is_addon) continue;
		auto&st_u = add_unit(initial_state, u->type);
		if (u->addon) st_u.has_addon = true;
		if (!u->is_completed && u->type->provided_supply) {
			initial_state.max_supply[u->type->race] += u->type->provided_supply;
		}
		if (u->type->is_gas) {
			for (auto&r : resource_spots::live_resources) {
				if (r.u == u) {
					//initial_state.resource_info.emplace(&r, &r);
					initial_state.resource_info[&r];
					break;
				}
			}
		}
	}
	if (initial_state.bases.empty()) {
		auto*s = get_best_score_p(resource_spots::spots, [&](resource_spots::spot*s) {
			double start_dist = get_best_score_value(start_locations, [&](xy pos) {
				return diag_distance(pos - s->cc_build_pos);
			});
			if (start_dist >= 32 * 10) return 10000 + start_dist;
			return get_best_score(make_transform_filter(my_resource_depots, [&](unit*u) {
				return -unit_pathing_distance(unit_types::scv, u->pos, s->cc_build_pos);
			}), identity<double>());
		});
		if (s) add_base(initial_state, *s).verified = true;
	}
	return initial_state;
}

double op_total_minerals_gathered, op_total_gas_gathered;
double op_visible_minerals_value, op_visible_gas_value;
double op_unaccounted_minerals, op_unaccounted_gas;
double op_unverified_minerals_gathered, op_unverified_gas_gathered;

void buildpred_task() {

	while (true) {

		progress_opponent_builds();

		state initial_state = get_my_current_state();
		log("%d bases, %d units\n", initial_state.bases.size(), initial_state.units.size());
		if (!opponent_states.empty()) {
			auto&op_st = std::get<1>(opponent_states.front());
 			op_total_minerals_gathered = op_st.total_minerals_gathered;
 			op_total_gas_gathered = op_st.total_gas_gathered;
			double min_sum = 0;
			double gas_sum = 0;
			for (unit*u : enemy_units) {
				min_sum += u->minerals_value;
				gas_sum += u->gas_value;
			}
			op_visible_minerals_value = min_sum;
			op_visible_gas_value = gas_sum;
			op_unaccounted_minerals = op_total_minerals_gathered - op_visible_minerals_value - players::opponent_player->minerals_lost;
			op_unaccounted_gas = op_total_gas_gathered - op_visible_gas_value - players::opponent_player->gas_lost;

			if (op_unaccounted_gas < 0) {
				for (int i = 0; i < 3; ++i) free_worker(op_st, true);
				transfer_workers(op_st, false);
				transfer_workers(op_st, true);
			} else if (op_unaccounted_minerals < 0) {
				for (int i = 0; i < 3; ++i) free_worker(op_st, false);
				transfer_workers(op_st, true);
				transfer_workers(op_st, false);
			}
			double unv_min = 0;
			double unv_gas = 0;
			for (auto&v : op_st.resource_info) {
				if (v.first->u->type->is_gas) unv_gas += v.second.gathered;
				else unv_min += v.second.gathered;
			}
			op_unverified_minerals_gathered = unv_min;
			op_unverified_gas_gathered = unv_gas;
		}

		multitasking::sleep(15);

	}

}

void update_opponent_builds_task() {
	while (true) {
		multitasking::sleep(15);

		update_opponent_builds();

		for (auto&v : opponent_states) {
			auto&st = std::get<1>(v);
			{
				log("min %g gas %g supply %g/%g  bases %d\n", st.minerals, st.gas, st.used_supply[st.race], st.max_supply[st.race], st.bases.size());
				log("opponent state (frame %d)--- \n", st.frame);
				for (auto&v : st.units) {
					if (!v.first || !v.second.size()) continue;
					log(" %dx%s", v.second.size(), short_type_name(v.first));
				}
				log("\n");
			}
		}

	}
}

void render() {

	for (auto&s : resource_spots::spots) {
		int count = 0;
	
		for (auto&v : opponent_states) {
			state&st = std::get<1>(v);
			auto find = [&]() {
				for (auto&v : st.bases) if (&*v.s == &s) return true;
				return false;
			};
			if (find()) {
				game->drawCircleMap(s.pos.x, s.pos.y, 32 * 8 + 8 * count, BWAPI::Colors::Red);
				++count;
			}

		}
	}

	double my_total_minerals_gathered = players::my_player->game_player->gatheredMinerals();
	double my_total_gas_gathered = players::my_player->game_player->gatheredGas();
	double min_sum = 0;
	double gas_sum = 0;
	for (unit*u : my_units) {
		min_sum += u->minerals_value;
		gas_sum += u->gas_value;
	}
	double my_visible_minerals_value = min_sum;
	double my_visible_gas_value = gas_sum;
	double my_unaccounted_minerals = my_total_minerals_gathered - my_visible_minerals_value - players::my_player->minerals_lost;
	double my_unaccounted_gas = my_total_gas_gathered - my_visible_gas_value - players::my_player->gas_lost;;

// 	render::draw_screen_stacked_text(260, 20, format("my gathered minerals: %g  gas: %g", std::round(my_total_minerals_gathered), std::round(my_total_gas_gathered)));
// 	render::draw_screen_stacked_text(260, 20, format("my visible minerals: %g  gas: %g", std::round(my_visible_minerals_value), std::round(my_visible_gas_value)));
// 	render::draw_screen_stacked_text(260, 20, format("my unaccounted minerals: %g  gas: %g", std::round(my_unaccounted_minerals), std::round(my_unaccounted_gas)));
// 	render::draw_screen_stacked_text(260, 20, format("op gathered minerals: %g  gas: %g", std::round(op_total_minerals_gathered), std::round(op_total_gas_gathered)));
// 	render::draw_screen_stacked_text(260, 20, format("op visible minerals: %g  gas: %g", std::round(op_visible_minerals_value), std::round(op_visible_gas_value)));
// 	render::draw_screen_stacked_text(260, 20, format("op unaccounted minerals: %g  gas: %g", std::round(op_unaccounted_minerals), std::round(op_unaccounted_gas)));
// 	render::draw_screen_stacked_text(260, 20, format("op unverified minerals: %g  gas: %g", std::round(op_unverified_minerals_gathered), std::round(op_unverified_gas_gathered)));

}

void init() {

	init_variants();

	multitasking::spawn(buildpred_task, "buildpred");
	multitasking::spawn(update_opponent_builds_task, "update opponent builds");

	units::on_new_unit_callbacks.push_back(on_new_unit);
	units::on_type_change_callbacks.push_back(on_type_change);
	units::on_destroy_callbacks.push_back(on_destroy);

	render::add(render);

}

}

