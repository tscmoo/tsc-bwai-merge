
struct strat_base {

	int my_zergling_count;
	int my_queen_count;
	int my_defiler_count;
	int my_mutalisk_count;

	int my_hatch_count;

	int enemy_terran_unit_count = 0;
	int enemy_protoss_unit_count = 0;
	int enemy_zerg_unit_count = 0;
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
	int enemy_reaver_count = 0;
	int enemy_shuttle_count = 0;
	int enemy_robotics_facility_count = 0;
	double enemy_army_supply = 0.0;
	double enemy_air_army_supply = 0.0;
	double enemy_ground_army_supply = 0.0;
	double enemy_ground_large_army_supply = 0.0;
	double enemy_ground_small_army_supply = 0.0;
	double enemy_weak_against_ultra_supply = 0.0;
	double enemy_anti_air_army_supply = 0.0;
	double enemy_biological_army_supply = 0.0;
	int enemy_static_defence_count = 0;
	int enemy_proxy_building_count = 0;
	double enemy_attacking_army_supply = 0.0;
	int enemy_attacking_worker_count = 0;
	int enemy_dt_count = 0;
	int enemy_lurker_count = 0;
	int enemy_arbiter_count = 0;
	int enemy_cannon_count = 0;
	int enemy_bunker_count = 0;
	int enemy_units_that_shoot_up_count = 0;
	int enemy_gas_count = 0;
	int enemy_barracks_count = 0;
	int enemy_gateway_count = 0;
	int enemy_zergling_count = 0;
	int enemy_spire_count = 0;
	int enemy_cloaked_unit_count = 0;
	int enemy_zealot_count = 0;
	int enemy_dragoon_count = 0;
	int enemy_worker_count = 0;
	int enemy_spawning_pool_count = 0;
	int enemy_evolution_chamber_count = 0;
	int enemy_corsair_count = 0;
	int enemy_lair_count = 0;
	int enemy_mutalisk_count = 0;
	int enemy_scourge_count = 0;
	int enemy_queen_count = 0;
	int enemy_archon_count = 0;
	int enemy_stargate_count = 0;
	int enemy_fleet_beacon_count = 0;
	int enemy_missile_turret_count = 0;
	int enemy_observer_count = 0;
	int enemy_hydralisk_count = 0;
	int enemy_hydralisk_den_count = 0;
	int enemy_factory_count = 0;
	int enemy_citadel_of_adun_count = 0;
	int enemy_templar_archives_count = 0;
	int enemy_cybernetics_core_count = 0;

	bool opponent_has_expanded = false;
	bool being_rushed = false;
	bool is_defending = false;

	bool default_upgrades = true;
	bool default_worker_defence = true;
	bool pull_workers_for_ling_zealot_defence = false;

	int min_bases = 2;
	int max_bases = 0;

	int opening_state = 0;
	int gas_trick_state = 0;
	int sleep_time = 15 * 5;

	int attack_interval = 0;

	bool call_build = true;
	bool call_place_static_defence = true;
	bool place_static_defence_only_at_main = false;
	bool call_place_expos = true;
	bool maxed_out_aggression = false;

	void bo_gas_trick(unit_type*ut = unit_types::drone) {
		if (!bo_all_done()) return;
		if (gas_trick_state == 0) {
			if (current_minerals >= 80) {
				build::add_build_task(100.0, unit_types::extractor);
				build::add_build_task(101.0, ut);
				++gas_trick_state;
			}
		} else {
			bool done = false;
			int gases = 0;
			for (auto&b : build::build_tasks) {
				if (b.type->unit && b.type->unit->is_gas) ++gases;
			}
			if (gases == 0) done = true;
			for (unit*u : my_units_of_type[unit_types::extractor]) {
				if (!u->is_completed) {
					u->game_unit->cancelConstruction();
					for (auto&b : build::build_tasks) {
						if (b.built_unit == u) {
							b.dead = true;
							break;
						}
					}
					done = true;
					break;
				}
			}
			if (done) {
				++opening_state;
				gas_trick_state = 0;
			}
		}
	}

	bool bo_all_done() {
		for (auto&b : build::build_order) {
			if (b.type->unit && !b.built_unit) return false;
		}
		return true;
	}
	void bo_cancel_all() {
		for (auto&b : build::build_order) {
			if (b.type->unit && !b.built_unit) {
				b.dead = true;
			}
		}
	}

	bool can_expand = false;
	bool force_expand = false;

	bool is_attacking = false;

	bool static_defence_pos_is_valid = false;

	virtual void init() = 0;
	virtual bool tick() = 0;

	int drone_count;
	int hatch_count;
	int completed_hatch_count;
	int larva_count;
	int damaged_overlord_count;

	int zergling_count;
	int mutalisk_count;
	int scourge_count;
	int hydralisk_count;
	int lurker_count;
	int ultralisk_count;
	int queen_count;
	int defiler_count;
	int devourer_count;
	int guardian_count;
	int sunken_count;
	int spore_count;
	
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
	
	int probe_count;
	int zealot_count;
	int dragoon_count;
	int observer_count;
	int shuttle_count;
	int reaver_count;
	int high_templar_count;
	int dark_templar_count;
	int arbiter_count;
	int archon_count;
	int dark_archon_count;
	int corsair_count;
	int cannon_count;
	int carrier_count;
	int scout_count;
	int gateway_count;

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
	}

	bool get_next_base_check_reachable = true;
	std::function<refcounted_ptr<resource_spots::spot>()> get_next_base = [this]() {
		auto st = buildpred::get_my_current_state();
		unit_type*worker_type = unit_types::drone;
		unit_type*cc_type = unit_types::cc;
		a_vector<refcounted_ptr<resource_spots::spot>> available_bases;
		for (auto& s : resource_spots::spots) {
			if (grid::get_build_square(s.cc_build_pos).building) continue;
			bool okay = true;
			for (auto&v : st.bases) {
				if ((resource_spots::spot*)v.s == &s) okay = false;
			}
			if (!okay) continue;
			if (!build::build_is_valid(grid::get_build_square(s.cc_build_pos), cc_type)) continue;
//			if (get_next_base_check_reachable) {
//				bool reachable = st.bases.empty();
//				for (auto& v : st.bases) {
//					bool reachable_from_base = true;
//					for (xy pos : combat::find_bigstep_path(unit_types::siege_tank_tank_mode, v.s->pos, s.cc_build_pos, square_pathing::pathing_map_index::default_)) {
//						if (!combat::can_transfer_through(pos)) {
//							reachable_from_base = false;
//							break;
//						}
//					}
//					if (reachable_from_base) {
//						reachable = true;
//						break;
//					}
//				}
//				if (!reachable) continue;
//			}
			available_bases.push_back(&s);
		}
		return get_best_score(available_bases, [&](resource_spots::spot*s) {
			//double score = unit_pathing_distance(worker_type, s->cc_build_pos, st.bases.front().s->cc_build_pos);
			double score = 0;
			//if (st.bases.size() == 1 || players::my_player->race != race_zerg) score += unit_pathing_distance(worker_type, s->cc_build_pos, st.bases.front().s->cc_build_pos);
			//else score += unit_pathing_distance(worker_type, s->cc_build_pos, st.bases.front().s->cc_build_pos) / 1024.0;
			score += unit_pathing_distance(worker_type, s->cc_build_pos, st.bases.front().s->cc_build_pos);
			double res = 0;
// 			double ned = get_best_score_value(enemy_buildings, [&](unit*e) {
// 				if (e->type->is_worker) return std::numeric_limits<double>::infinity();
// 				return diag_distance(s->pos - e->pos);
// 			});
			int workers_nearby = 0;
			double ned = 0.0;
			if (st.bases.size() > 1) {
				unit* ne = get_best_score(enemy_buildings, [&](unit*e) {
					if (e->type->is_worker) return std::numeric_limits<double>::infinity();
					if (e->is_flying) return std::numeric_limits<double>::infinity();
					return diag_distance(s->pos - e->pos);
				}, std::numeric_limits<double>::infinity());
//				if (ne && st.bases.size() >= 2 && players::my_player->race == race_zerg) {
//					score -= unit_pathing_distance(worker_type, s->cc_build_pos, ne->pos);
//				}
				ned = ne ? diag_distance(s->pos - ne->pos) : 0.0;
				if (st.bases.size() == 1 && (!ne || (!ne->stats->ground_weapon && ne->type != unit_types::bunker))) ned = 0.0;
				if (ned && ned <= 32 * 30) score += 10000;
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
				score -= diag_distance(s->pos - xy(grid::map_width / 2, grid::map_height / 2)) * 2;
			}
			bool has_gas = false;
			for (auto&r : s->resources) {
				res += r.u->resources;
				if (r.u->type->is_gas) has_gas = true;
			}
			score -= (has_gas ? res : res / 4) / 40 + ned + workers_nearby * 1024.0;
			return score;
		}, std::numeric_limits<double>::infinity());
	};
//	auto get_next_base() {
//		auto st = buildpred::get_my_current_state();
//		unit_type*worker_type = unit_types::scv;
//		unit_type*cc_type = unit_types::cc;
//		a_vector<refcounted_ptr<resource_spots::spot>> available_bases;
//		for (auto&s : resource_spots::spots) {
//			if (grid::get_build_square(s.cc_build_pos).building) continue;
//			bool okay = true;
//			for (auto&v : st.bases) {
//				if ((resource_spots::spot*)v.s == &s) okay = false;
//			}
//			if (!okay) continue;
//			if (!build::build_is_valid(grid::get_build_square(s.cc_build_pos), cc_type)) continue;
//			available_bases.push_back(&s);
//		}
//		return get_best_score(available_bases, [&](resource_spots::spot*s) {
//			double score = unit_pathing_distance(worker_type, s->cc_build_pos, st.bases.front().s->cc_build_pos);
//			double res = 0;
//			double ned = get_best_score_value(enemy_buildings, [&](unit* e) {
//				if (e->type->is_worker) return std::numeric_limits<double>::infinity();
//				if (e->is_flying) return std::numeric_limits<double>::infinity();
//				return diag_distance(s->pos - e->pos);
//			});
//			if (st.bases.size() == 1) ned = 0;
//			else if (ned <= 32 * 30) score += 10000;
//			int workers_nearby = 0;
//// 			for (unit* u : my_workers) {
//// 				if (diag_distance(s->pos - u->pos) <= 32 * 10) ++workers_nearby;
//// 			}
//			for (auto& v : resource_gathering::live_gatherers) {
//				if (v.resource) {
//					for (auto& r : s->resources) {
//						if (v.resource->u == r.u) {
//							++workers_nearby;
//							break;
//						}
//					}
//				}
//			}
//			bool has_gas = false;
//			for (auto&r : s->resources) {
//				res += r.u->resources;
//				if (r.u->type->is_gas) has_gas = true;
//			}
//			score -= (has_gas ? res : res / 4) / 10 + ned + workers_nearby * 256.0;
//			return score;
//		}, std::numeric_limits<double>::infinity());
//	}

	void rm_all_scouts() {
		log(log_level_test, "rm_all_scouts()\n");
		for (auto&v : scouting::all_scouts) {
			scouting::rm_scout(v.scout_unit);
		}
	}

	bool eval_combat(bool include_static_defence, int min_sunkens) {
		combat_eval::eval eval;
		eval.max_frames = 15 * 120;
		for (unit*u : my_units) {
			if (!u->is_completed) continue;
			if (!include_static_defence && u->building) continue;
			if (!u->stats->ground_weapon) continue;
			if (u->type->is_worker || u->type->is_non_usable) continue;
			auto&c = eval.add_unit(u, 0);
			if (u->building) c.move = 0.0;
		}
		for (unit*u : enemy_units) {
			if (u->building || u->type->is_worker || u->type->is_non_usable) continue;
			if (u->is_flying) continue;
			eval.add_unit(u, 1);
		}
		eval.run();
		a_map<unit_type*, int> my_count, op_count;
		for (auto&v : eval.teams[0].units) ++my_count[v.st->type];
		for (auto&v : eval.teams[1].units) ++op_count[v.st->type];
		log("eval_combat::\n");
		log("my units -");
		for (auto&v : my_count) log(" %dx%s", v.second, short_type_name(v.first));
		log("\n");
		log("op units -");
		for (auto&v : op_count) log(" %dx%s", v.second, short_type_name(v.first));
		log("\n");

		log("result: score %g %g  supply %g %g  damage %g %g  in %d frames\n", eval.teams[0].score, eval.teams[1].score, eval.teams[0].end_supply, eval.teams[1].end_supply, eval.teams[0].damage_dealt, eval.teams[1].damage_dealt, eval.total_frames);
		if (min_sunkens) {
			int low_hp_or_dead_sunkens = 0;
			int total_sunkens = 0;
			for (auto&c : eval.teams[0].units) {
				if (c.st->type == unit_types::sunken_colony) {
					if (c.st->hp <= c.st->hp / 2) ++low_hp_or_dead_sunkens;
					++total_sunkens;
				}
			}
			if (total_sunkens - low_hp_or_dead_sunkens < min_sunkens && total_sunkens >= min_sunkens) return false;
		}
		return eval.teams[1].end_supply == 0;
	}

	void set_build_vars(const buildpred::state& st) {
		using namespace buildpred;
		drone_count = count_units_plus_production(st, unit_types::drone);
		hatch_count = count_units_plus_production(st, unit_types::hatchery) + count_units_plus_production(st, unit_types::lair) + count_units_plus_production(st, unit_types::hive);
		completed_hatch_count = st.units_range(unit_types::hatchery).size() + st.units_range(unit_types::lair).size() + st.units_range(unit_types::hive).size();
		larva_count = 0;
		for (int i = 0; i < 3; ++i) {
			for (const st_unit& u : st.units_range(i == 0 ? unit_types::hive : i == 1 ? unit_types::lair : unit_types::hatchery)) {
				larva_count += u.larva_count;
			}
		}
		damaged_overlord_count = 0;
		for (unit*u : my_completed_units_of_type[unit_types::overlord]) {
			if (u->hp < u->stats->hp) ++damaged_overlord_count;
		}

		zergling_count = count_units_plus_production(st, unit_types::zergling);
		mutalisk_count = count_units_plus_production(st, unit_types::mutalisk);
		scourge_count = count_units_plus_production(st, unit_types::scourge);
		hydralisk_count = count_units_plus_production(st, unit_types::hydralisk);
		lurker_count = count_units_plus_production(st, unit_types::lurker);
		ultralisk_count = count_units_plus_production(st, unit_types::ultralisk);
		queen_count = count_units_plus_production(st, unit_types::queen);
		defiler_count = count_units_plus_production(st, unit_types::defiler);
		devourer_count = count_units_plus_production(st, unit_types::devourer);
		guardian_count = count_units_plus_production(st, unit_types::guardian);
		sunken_count = count_units_plus_production(st, unit_types::sunken_colony);
		spore_count = count_units_plus_production(st, unit_types::spore_colony);
		
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
		
		probe_count = count_units_plus_production(st, unit_types::probe);
		zealot_count = count_units_plus_production(st, unit_types::zealot);
		dragoon_count = count_units_plus_production(st, unit_types::dragoon);
		observer_count = count_units_plus_production(st, unit_types::observer);
		shuttle_count = count_units_plus_production(st, unit_types::shuttle);
		reaver_count = count_units_plus_production(st, unit_types::reaver);
		high_templar_count = count_units_plus_production(st, unit_types::high_templar);
		dark_templar_count = count_units_plus_production(st, unit_types::dark_templar);
		arbiter_count = count_units_plus_production(st, unit_types::arbiter);
		archon_count = count_units_plus_production(st, unit_types::archon);
		dark_archon_count = count_units_plus_production(st, unit_types::dark_archon);
		corsair_count = count_units_plus_production(st, unit_types::corsair);
		cannon_count = count_units_plus_production(st, unit_types::photon_cannon);
		carrier_count = count_units_plus_production(st, unit_types::carrier);
		scout_count = count_units_plus_production(st, unit_types::scout);
		gateway_count = count_units_plus_production(st, unit_types::gateway);

		army_supply = 0.0;
		air_army_supply = 0.0;
		ground_army_supply = 0.0;
		for (auto&v : st.units) {
			if (v.second.empty()) continue;
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
				int n = 1;
				if (ut->is_two_units_in_one_egg) n = 2;
				army_supply += ut->required_supply * n;
				if (ut->is_flyer) air_army_supply += ut->required_supply * n;
				else ground_army_supply += ut->required_supply * n;
			}
		}
	}

	int n_mineral_patches = 0;

	virtual void post_build() {
	}

	void run() {

		using namespace buildpred;

		get_upgrades::set_upgrade_value(upgrade_types::burrow, 100000.0);
		get_upgrades::set_upgrade_value(upgrade_types::ventral_sacs, 100000.0);
		get_upgrades::set_upgrade_value(upgrade_types::antennae, 100000.0);
		
		get_upgrades::set_upgrade_value(upgrade_types::restoration, 10000.0);
		get_upgrades::set_upgrade_value(upgrade_types::optical_flare, 10000.0);
		get_upgrades::set_upgrade_value(upgrade_types::caduceus_reactor, 10000.0);
		get_upgrades::set_upgrade_value(upgrade_types::emp_shockwave, 10000.0);
		
		get_upgrades::set_upgrade_value(upgrade_types::protoss_plasma_shields_1, 5000.0);
		get_upgrades::set_upgrade_value(upgrade_types::hallucination, 100000.0);

		//get_upgrades::set_no_auto_upgrades(true);

		scouting::scout_supply = 12;

		resource_gathering::max_gas = 0.0;
		
		auto get_static_defence_pos = [&](unit_type*ut) {
			a_vector<std::tuple<xy, xy>> my_bases;
			for (auto&v : buildpred::get_my_current_state().bases) {
				my_bases.emplace_back(v.s->pos, square_pathing::get_nearest_node_pos(unit_types::siege_tank_tank_mode, v.s->cc_build_pos));
			}
			xy op_base = combat::op_closest_base;
			a_vector<a_deque<xy>> paths;
			for (auto&v : my_bases) {
				xy pos = std::get<1>(v);
				auto path = combat::find_bigstep_path(unit_types::siege_tank_tank_mode, pos, op_base, square_pathing::pathing_map_index::no_enemy_buildings);
				if (path.empty()) continue;
				path.push_front(pos);
				path.push_front(std::get<0>(v));
				paths.push_back(std::move(path));
			}
			log("static defence (%s) %d paths:\n", ut->name, paths.size());
			for (auto&p : paths) {
				log(" - %d nodes\n", p.size());
			}
			if (!paths.empty()) {

				auto pred = [&](grid::build_square&bs) {
					if (!build::build_has_existing_creep(bs, ut)) {
						bool is_next_to_hatchery = false;
						for (unit*u : my_units_of_type[unit_types::hatchery]) {
							if (!u->building) continue;
							if (u->is_completed) continue;
							if (u->remaining_build_time > 15 * 20) continue;
							xy pos = u->building->build_pos;
							xy a0 = bs.pos;
							xy a1 = bs.pos + xy(ut->tile_width * 32, ut->tile_height * 32);
							xy b0 = u->pos;
							xy b1 = u->pos + xy(u->type->tile_width * 32, u->type->tile_height * 32);
							if (a0.x > b1.x) continue;
							else if (b0.x > a1.x) continue;
							if (a0.y > b1.y) continue;
							else if (b0.y > a1.y) continue;
							is_next_to_hatchery = true;
							break;
						}
						if (!is_next_to_hatchery) return false;
					}
					if (!build_spot_finder::can_build_at(ut, bs)) return false;
					return true;
				};
				auto score = [&](xy pos) {
					double r = 0.0;
					for (auto&path : paths) {
						bool in_range = false;
						double v = 1.0 / 64;
						double dist = 0.0;
						for (xy p : path) {
							if (place_static_defence_only_at_main && diag_distance(p - my_start_location) >= 32 * 12) continue;
							double d = diag_distance(pos - p);
							if (d <= 32 * 7) {
								in_range = true;
								d = std::max(d, 32.0);
								dist += d*d;
							}
							v /= 2;
						}
						dist = std::sqrt(dist);
						if (dist == 0.0) dist = 32 * 15;
						if (dist < 64.0) dist = 64.0;
						r -= 1.0 / dist;
						if (in_range) r -= 1.0;
					}
					return r;
				};
				std::vector<xy> starts;
				xy pos;
				if (place_static_defence_only_at_main) {
					for (unit*u : my_resource_depots) {
						if (diag_distance(u->pos - my_start_location) <= 32 * 6) starts.push_back(u->pos);
					}
					pos = build_spot_finder::find_best(starts, 256, pred, score);
				} else {
					for (unit*u : my_resource_depots) starts.push_back(u->pos);
					pos = build_spot_finder::find_best(starts, 256, pred, score);
					log("score(pos) is %g\n", score(pos));
					if (score(pos) > -2.0) {
						log("not good enough\n");
						return xy();
					}
				}
				log("static defence (%s) build pos -> %d %d (%g away from my_closest_base)\n", ut->name, pos.x, pos.y, (pos - combat::my_closest_base).length());
				return pos;
			}
			return xy();
		};

		auto place_static_defence = [&]() {
			for (auto&b : build::build_tasks) {
				if (b.built_unit || b.dead) continue;
				if (b.type->unit == unit_types::creep_colony) {
					//if (b.build_near != combat::defence_choke.center) {
					if (true) {
						b.build_near = combat::defence_choke.center;
						b.require_existing_creep = true;
						xy prev_pos = b.build_pos;
						build::unset_build_pos(&b);
						xy pos = get_static_defence_pos(b.type->unit);
						if (pos != xy()) {
							build::set_build_pos(&b, pos);
						} else build::set_build_pos(&b, prev_pos);
					}
				}
			}
		};

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

			if (players::my_player->race == race_zerg) {
				int bases_with_minerals = 0;
				for (auto&b : get_my_current_state().bases) {
					int min_count = 0;
					for (auto&s : b.s->resources) {
						if (s.u->type->is_minerals) ++min_count;
					}
					if (min_count >= 4) ++bases_with_minerals;
				}
	
				if (bases_with_minerals && (current_used_total_supply < 60.0 || free_mineral_patches())) {
					if (max_bases && bases_with_minerals >= max_bases) return;
					if ((int)st.bases.size() >= min_bases) {
						int my_hatch_count = my_units_of_type[unit_types::hatchery].size() + my_units_of_type[unit_types::lair].size() + my_units_of_type[unit_types::hive].size();
						if (my_hatch_count > 5 || my_hatch_count >= (int)st.bases.size() * 2) {
							int n = 8;
							auto s = get_next_base();
							//if (s) n = (int)s->resources.size();
							if (s) {
								n = 0;
								for (auto&v : s->resources) ++n;
							}
							if (free_mineral_patches() >= n && long_distance_miners() == 0) return;
						} else {
							if (free_mineral_patches() >= 2 && long_distance_miners() == 0) return;
						}
					}
				}
			}

			for (auto&b : build::build_tasks) {
				if (b.built_unit || b.dead) continue;
				if (b.type->unit == unit_types::hatchery || b.type->unit == unit_types::cc || b.type->unit == unit_types::nexus) {
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

		auto weapon_damage_against_size = [&](weapon_stats*w, int size) {
			if (!w) return 0.0;
			double damage = w->damage;
			damage *= combat_eval::get_damage_type_modifier(w->damage_type, size);
			if (damage <= 0) damage = 1.0;
			return damage;
		};

		double attack_my_lost;
		double attack_op_lost;
		int last_attack_begin = 0;
		int last_attack_end = 0;
		int attack_count = 0;

		auto begin_attack = [&]() {
			is_attacking = true;
			attack_my_lost = players::my_player->minerals_lost + players::my_player->gas_lost;
			attack_op_lost = players::opponent_player->minerals_lost + players::opponent_player->gas_lost;
			last_attack_begin = current_frame;
			++attack_count;
			log("begin attack\n");
		};
		auto end_attack = [&]() {
			is_attacking = false;
			last_attack_end = current_frame;
			log("end attack\n");
		};

		while (true) {

			my_hatch_count = my_units_of_type[unit_types::hatchery].size() + my_units_of_type[unit_types::lair].size() + my_units_of_type[unit_types::hive].size();

			my_zergling_count = my_units_of_type[unit_types::zergling].size();
			my_queen_count = my_units_of_type[unit_types::queen].size();
			my_defiler_count = my_units_of_type[unit_types::defiler].size();
			my_mutalisk_count = my_units_of_type[unit_types::mutalisk].size();

			set_build_vars(get_my_current_state());

			enemy_terran_unit_count = 0;
			enemy_protoss_unit_count = 0;
			enemy_zerg_unit_count = 0;
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
			enemy_reaver_count = 0;
			enemy_shuttle_count = 0;
			enemy_robotics_facility_count = 0;
			enemy_army_supply = 0.0;
			enemy_air_army_supply = 0.0;
			enemy_ground_army_supply = 0.0;
			enemy_ground_large_army_supply = 0.0;
			enemy_ground_small_army_supply = 0.0;
			enemy_weak_against_ultra_supply = 0.0;
			enemy_anti_air_army_supply = 0.0;
			enemy_biological_army_supply = 0.0;
			enemy_static_defence_count = 0;
			enemy_proxy_building_count = 0;
			enemy_attacking_army_supply = 0.0;
			enemy_attacking_worker_count = 0;
			enemy_dt_count = 0;
			enemy_lurker_count = 0;
			enemy_arbiter_count = 0;
			enemy_cannon_count = 0;
			enemy_bunker_count = 0;
			enemy_units_that_shoot_up_count = 0;
			enemy_gas_count = 0;
			enemy_barracks_count = 0;
			enemy_gateway_count = 0;
			enemy_zergling_count = 0;
			enemy_spire_count = 0;
			enemy_cloaked_unit_count = 0;
			enemy_zealot_count = 0;
			enemy_dragoon_count = 0;
			enemy_worker_count = 0;
			enemy_spawning_pool_count = 0;
			enemy_evolution_chamber_count = 0;
			enemy_corsair_count = 0;
			enemy_lair_count = 0;
			enemy_mutalisk_count = 0;
			enemy_queen_count = 0;
			enemy_scourge_count = 0;
			enemy_archon_count = 0;
			enemy_stargate_count = 0;
			enemy_fleet_beacon_count = 0;
			enemy_missile_turret_count = 0;
			enemy_observer_count = 0;
			enemy_hydralisk_count = 0;
			enemy_hydralisk_den_count = 0;
			enemy_factory_count = 0;
			enemy_citadel_of_adun_count = 0;
			enemy_templar_archives_count = 0;
			enemy_cybernetics_core_count = 0;

			update_possible_start_locations();
			for (unit* e : enemy_units) {
				if (current_frame - e->last_seen >= 15 * 60 * 10) continue;

				if (e->type->race == race_terran) ++enemy_terran_unit_count;
				if (e->type->race == race_protoss) ++enemy_protoss_unit_count;
				if (e->type->race == race_zerg) ++enemy_zerg_unit_count;

				if (!e->type->is_worker && !e->building) {
					if (e->is_flying) enemy_air_army_supply += e->type->required_supply;
					else {
						enemy_ground_army_supply += e->type->required_supply;
						if (e->type->size == unit_type::size_large) enemy_ground_large_army_supply += e->type->required_supply;
						if (e->type->size == unit_type::size_small) enemy_ground_small_army_supply += e->type->required_supply;
						if (weapon_damage_against_size(e->stats->ground_weapon, unit_type::size_large) <= 12.0) enemy_weak_against_ultra_supply += e->type->required_supply;
					}
					enemy_army_supply += e->type->required_supply;
					if (e->stats->air_weapon) enemy_anti_air_army_supply += e->type->required_supply;
					if (e->type->is_biological) enemy_biological_army_supply += e->type->required_supply;
				}

				if (!e->type->is_refinery) {
					double e_d = get_best_score_value(possible_start_locations, [&](xy pos) {
						return unit_pathing_distance(unit_types::scv, e->pos, pos);
					});
					if (unit_pathing_distance(unit_types::scv, e->pos, combat::my_closest_base) < e_d) {
						log("%s is proxied\n", e->type->name);
						if (e->building) ++enemy_proxy_building_count;
						if (!e->type->is_worker) enemy_attacking_army_supply += e->type->required_supply;
						else ++enemy_attacking_worker_count;
					}
				}
				if (e->stats->air_weapon) ++enemy_units_that_shoot_up_count;
				if (e->type->is_gas) ++enemy_gas_count;
				if (e->type->is_worker) ++enemy_worker_count;
				if (e->cloaked && e->type != unit_types::spider_mine) ++enemy_cloaked_unit_count;
				
				if (e->type == unit_types::marine) ++enemy_marine_count;
				else if (e->type == unit_types::firebat) ++enemy_firebat_count;
				else if (e->type == unit_types::ghost) ++enemy_ghost_count;
				else if (e->type == unit_types::vulture) ++enemy_vulture_count;
				else if (e->type == unit_types::siege_tank_tank_mode) ++enemy_tank_count;
				else if (e->type == unit_types::siege_tank_siege_mode) ++enemy_tank_count;
				else if (e->type == unit_types::goliath) ++enemy_goliath_count;
				else if (e->type == unit_types::wraith) ++enemy_wraith_count;
				else if (e->type == unit_types::valkyrie) ++enemy_valkyrie_count;
				else if (e->type == unit_types::battlecruiser) ++enemy_bc_count;
				else if (e->type == unit_types::science_vessel) ++enemy_science_vessel_count;
				else if (e->type == unit_types::dropship) ++enemy_dropship_count;
				else if (e->type == unit_types::reaver) ++enemy_reaver_count;
				else if (e->type == unit_types::shuttle) ++enemy_shuttle_count;
				else if (e->type == unit_types::robotics_facility) ++enemy_robotics_facility_count;
				else if (e->type == unit_types::missile_turret) ++enemy_static_defence_count;
				else if (e->type == unit_types::photon_cannon) ++enemy_static_defence_count;
				else if (e->type == unit_types::sunken_colony) ++enemy_static_defence_count;
				else if (e->type == unit_types::spore_colony) ++enemy_static_defence_count;
				else if (e->type == unit_types::dark_templar) ++enemy_dt_count;
				else if (e->type == unit_types::lurker) ++enemy_lurker_count;
				else if (e->type == unit_types::arbiter) ++enemy_arbiter_count;
				else if (e->type == unit_types::photon_cannon) ++enemy_cannon_count;
				else if (e->type == unit_types::bunker) ++enemy_bunker_count;
				else if (e->type == unit_types::barracks) ++enemy_barracks_count;
				else if (e->type == unit_types::gateway) ++enemy_gateway_count;
				else if (e->type == unit_types::zergling) ++enemy_zergling_count;
				else if (e->type == unit_types::spire || e->type == unit_types::greater_spire) ++enemy_spire_count;
				else if (e->type == unit_types::zealot) ++enemy_zealot_count;
				else if (e->type == unit_types::dragoon) ++enemy_dragoon_count;
				else if (e->type == unit_types::spawning_pool) ++enemy_spawning_pool_count;
				else if (e->type == unit_types::evolution_chamber) ++enemy_evolution_chamber_count;
				else if (e->type == unit_types::corsair) ++enemy_corsair_count;
				else if (e->type == unit_types::lair || e->type == unit_types::hive) ++enemy_lair_count;
				else if (e->type == unit_types::mutalisk) ++enemy_mutalisk_count;
				else if (e->type == unit_types::scourge) ++enemy_scourge_count;
				else if (e->type == unit_types::queen) ++enemy_queen_count;
				else if (e->type == unit_types::archon) ++enemy_archon_count;
				else if (e->type == unit_types::stargate) ++enemy_stargate_count;
				else if (e->type == unit_types::fleet_beacon) ++enemy_fleet_beacon_count;
				else if (e->type == unit_types::missile_turret) ++enemy_missile_turret_count;
				else if (e->type == unit_types::observer) ++enemy_observer_count;
				else if (e->type == unit_types::hydralisk) ++enemy_hydralisk_count;
				else if (e->type == unit_types::hydralisk_den) ++enemy_hydralisk_den_count;
				else if (e->type == unit_types::factory) ++enemy_factory_count;
				else if (e->type == unit_types::citadel_of_adun) ++enemy_citadel_of_adun_count;
				else if (e->type == unit_types::templar_archives) ++enemy_templar_archives_count;
				else if (e->type == unit_types::cybernetics_core) ++enemy_cybernetics_core_count;
			}
			if (enemy_hydralisk_count && enemy_hydralisk_den_count == 0) enemy_hydralisk_den_count = 1;
			if (enemy_mutalisk_count && enemy_spire_count == 0) enemy_spire_count = 1;
			
			if (enemy_factory_count == 0) {
				if (enemy_vulture_count + enemy_goliath_count + enemy_tank_count) enemy_factory_count = 1;
			}

			if (enemy_terran_unit_count + enemy_protoss_unit_count) overlord_scout(enemy_gas_count + enemy_units_that_shoot_up_count + enemy_barracks_count == 0 && players::opponent_player->race != race_terran);
			else overlord_scout(enemy_units_that_shoot_up_count + enemy_spire_count == 0 && players::opponent_player->race != race_terran);

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
			for (auto&g : combat::groups) {
				if (!g.is_aggressive_group && !g.is_defensive_group) {
					if (g.ground_dpf >= 0.5) {
						is_defending = true;
						break;
					}
				}
			}
			
			n_mineral_patches = 0;
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
						if (!res->gatherers.empty()) ++n_mineral_patches;
					}
				}
			}

			static_defence_pos_is_valid = get_static_defence_pos(unit_types::creep_colony) != xy();
			
			if (!attack_interval) {
// 				combat::no_aggressive_groups = true;
// 				combat::aggressive_groups_done_only = false;
			} else {
				if (!is_attacking) {

					bool attack = false;
					if (attack_count == 0) attack = true;
					if (current_frame - last_attack_end >= attack_interval) attack = true;

					if (attack) begin_attack();

				} else {

					int lose_count = 0;
					int total_count = 0;
					for (auto*a : combat::live_combat_units) {
						if (a->u->type->is_worker) continue;
						if (!a->u->stats->ground_weapon && !a->u->stats->air_weapon) continue;
						++total_count;
						if (current_frame - a->last_fight <= 15 * 10) {
							if (a->last_run >= a->last_fight) ++lose_count;
						}
					}
					if (lose_count >= total_count / 2 + total_count / 4 || (lose_count >= total_count / 4 && !eval_combat(false, 0))) {
						end_attack();
					}

				}

				combat::no_aggressive_groups = !is_attacking;
				//combat::aggressive_zerglings = is_attacking;
			}

			if (current_used_total_supply >= 190.0) maxed_out_aggression = true;
			if (maxed_out_aggression) {
				combat::no_aggressive_groups = false;
				combat::aggressive_groups_done_only = false;
				if (current_used_total_supply < 150.0) maxed_out_aggression = false;
			}

			can_expand = get_next_base();
			force_expand = can_expand && long_distance_miners() >= 2 && my_workers.size() >= get_max_mineral_workers() && my_units_of_type[unit_types::hatchery].size() == my_completed_units_of_type[unit_types::hatchery].size();
			//if (can_expand && free_mineral_patches() == 0 && my_workers.size() >= 24) force_expand = true;

			if (opening_state != -1 && current_used_supply[race_zerg] + 0.5 >= current_max_supply[race_zerg]) {
				for (auto&b : build::build_tasks) {
					if (b.dead) continue;
					if (b.type->unit && b.type->unit->is_gas) {
						auto st = get_my_current_state();
						int gas_count = 0;
						for (auto&base : st.bases) {
							for (auto&r : base.s->resources) {
								if (r.u->type == unit_types::vespene_geyser) ++gas_count;
								if (r.u->type == unit_types::extractor && r.u->owner == players::my_player) ++gas_count;
							}
						}
						if (gas_count == 0) {
							b.dead = true;
						}
					}
				}

				auto st = get_my_current_state();
				int overlords = 0;
				int buildings = 0;
				double supply_needed = 0.0;
				for (auto&b : build::build_tasks) {
					if (b.type->unit == unit_types::overlord) ++overlords;
					if (b.dead || b.built_unit) continue;
					if (b.type->unit && b.type->unit->is_building) ++buildings;
					if (b.type->unit) supply_needed += b.type->unit->required_supply;
				}
				if (overlords == 0 && st.used_supply[race_zerg] + supply_needed - buildings > st.max_supply[race_zerg]) {
					bo_cancel_all();
				}
			}

			if (tick() || should_transition()) {
				bo_cancel_all();
				break;
			}

			if (my_workers.size() <= 4) {
				for (auto&v : scouting::all_scouts) {
					if (v.scout_unit && v.scout_unit->type->is_worker) scouting::rm_scout(v.scout_unit);
				}
			}

			auto build = [&](state&st) {

				set_build_vars(st);

				army = [&](state&st) {
					return false;
				};

				return this->build(st);

			};

			if (call_build) execute_build(false, build);

			if (call_place_expos) place_expos();
			if (call_place_static_defence) place_static_defence();

			post_build();

			multitasking::sleep(sleep_time);
			
			get_next_base_check_reachable = my_workers.size() < 30 && current_frame < 15 * 60 * 15;
		}


	}

	void render() {

	}

};

