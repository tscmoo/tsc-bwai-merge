
struct strat_z_any_5pool_sunken_rush : strat_mod {
	
	int total_drones_made = 0;
	int total_zerglings_made = 0;
	
	struct proxy_t {
		unit_type* ut = nullptr;
		xy pos;
		refcounted_ptr<build::build_task> build_task;
		unit* builder = nullptr;
	};
	a_vector<proxy_t> proxy_list;
	xy enemy_main;
	int dead_count = 0;
	bool has_seen_creep = false;

	virtual void mod_init() override {
		
		//early_micro::start();

	}

	virtual void mod_tick() override {
		combat::no_aggressive_groups = false;
		
		scouting::scout_supply = 5;
		
		//if (current_frame < 15 * 60 * 6) early_micro::keepalive_until = current_frame + 60;
		
		total_drones_made = my_units_of_type[unit_types::drone].size() + game->self()->deadUnitCount(unit_types::drone->game_unit_type);
		total_zerglings_made = my_units_of_type[unit_types::zergling].size() + game->self()->deadUnitCount(unit_types::zergling->game_unit_type);
		
		
//		bool anything_to_attack = false;
//		for (unit*e : enemy_units) {
//			if (e->gone) continue;
//			if (e->is_flying) continue;
//			if (e->type->is_worker) continue;
//			anything_to_attack = true;
//			break;
//		}
//		if (anything_to_attack) {
//			rm_all_scouts();
//		} else {
//			if (!my_units_of_type[unit_types::zergling].empty()) {
////				for (unit*u : my_units_of_type[unit_types::zergling]) {
////					if (!scouting::is_scout(u)) scouting::add_scout(u);
////				}
//			} else {
//				int worker_scouts = 0;
//				for (auto& v : scouting::all_scouts) {
//					if (v.scout_unit && v.scout_unit->type->is_worker) ++worker_scouts;
//				}
//				if (worker_scouts == 0 && !my_units_of_type[unit_types::spawning_pool].empty()) {
//					if (true) {
//						unit*u = get_best_score(my_workers, [&](unit*u) {
//							if (u->controller->action != unit_controller::action_gather) return std::numeric_limits<double>::infinity();
//							return 0.0;
//						}, std::numeric_limits<double>::infinity());
//						if (u) scouting::add_scout(u);
//					}
//				}
//			}
//		}
		
		if (current_frame < 15 * 60 * 10) resource_gathering::max_gas = std::max(resource_gathering::max_gas, 200.0);
		
		//if (!enemy_buildings.empty()) rm_all_scouts();
		
		if (current_frame >= 15 * 60 * 10 && !has_seen_creep && current_frame < 15 * 60 * 6) {
			int creep_tiles = 0;
			for (int y = 0; y < grid::map_height; y += 32) {
				for (int x = 0; x < grid::map_height; x += 32) {
					xy pos(x, y);
					if (diag_distance(pos - my_start_location) > 32 * 15) {
						auto& bs = grid::get_build_square(pos);
						if (bs.has_creep) ++creep_tiles;
					}
				}
			}
			if (creep_tiles >= 16) has_seen_creep = true;
		}
		log(log_level_test, " has_seen_creep is %d\n", has_seen_creep);
		
		//if (enemy_main == xy() && current_frame >= 15 * 30) {
		if ((enemy_main == xy() || ((players::opponent_player->race == race_zerg || players::opponent_player->random) && !has_seen_creep)) && current_frame >= 15 * 20) {

			int scouts = 2;
			update_possible_start_locations();
			if (possible_start_locations.size() >= 4) scouts = 2;

			log(log_level_test, "scouting::all_scouts.size() is %d\n", scouting::all_scouts.size());

			int worker_scouts = 0;
			for (auto& v : scouting::all_scouts) {
				if (v.scout_unit && v.scout_unit->type->is_worker) ++worker_scouts;
			}
			if ((int)scouting::all_scouts.size() < scouts) {
				unit* scout_unit = get_best_score(my_workers, [&](unit*u) {
					if (u->controller->action != unit_controller::action_gather) return std::numeric_limits<double>::infinity();
					for (auto& v : scouting::all_scouts) {
						if (v.scout_unit == u) return std::numeric_limits<double>::infinity();
					}
					return 0.0;
				}, std::numeric_limits<double>::infinity());
				if (scout_unit) {
					scouting::add_scout(scout_unit);
					log(log_level_test, " added scout\n");
				}
			}

			if (possible_start_locations.size() == 1) {
				enemy_main = possible_start_locations.front();

				enemy_main += xy(4 * 16, 3 * 16);
			}

		} else {
			rm_all_scouts();
		}
		
		if (enemy_main != xy()) {
			
			if (!my_units_of_type[unit_types::creep_colony].empty()) {
				int sunkens = 0;
				for (auto& b : build::build_tasks) {
					if (b.priority == 0.0 && b.type->unit == unit_types::sunken_colony) {
						++sunkens;
					}
				}
				if (sunkens == 0) build::add_build_task(0.0, unit_types::sunken_colony);
			}

			auto find_build_pos = [&](xy pos, unit_type* ut) {
				auto pred = [&](grid::build_square& bs) {
					if (!ut->requires_creep && !build::build_has_not_creep(bs, ut)) return false;
					if (ut->requires_creep && !build::build_has_existing_creep(bs, ut)) return false;
					return build_spot_finder::is_buildable_at(ut, bs);
				};
				auto score = [&](xy pos) {
					double r = unit_pathing_distance(unit_types::drone, pos, my_start_location);
					double r2 = diag_distance(pos - enemy_main);
					return r*r + r2*r2*2;
				};

				std::array<xy, 1> starts;
				starts[0] = pos;
				return build_spot_finder::find_best(starts, 32, pred, score);
			};

			int n_sunkens_planned = 0;
			for (auto& v : proxy_list) {
				if (v.ut == unit_types::creep_colony || v.ut == unit_types::sunken_colony) ++n_sunkens_planned;
			}

			a_vector<unit*> available_builders;
			int gatherers = 0;
			for (unit* u : my_workers) {
				if (u->controller->action == unit_controller::action_gather) {
					++gatherers;
					if (gatherers >= 4) {
						available_builders.push_back(u);
					}
				}
			}

			auto add = [&](unit_type* ut) {
				
				xy build_pos = find_build_pos(enemy_main, unit_types::creep_colony);
				
				log(log_level_test, " build_pos is %d %d\n", build_pos.x, build_pos.y);
				
				if (build_pos != xy()) {
					if (!available_builders.empty()) {
						unit* builder = available_builders.back();
						available_builders.pop_back();
						proxy_t v;
						v.ut = ut;
						v.pos = build_pos;
						v.build_task = build::add_build_task(current_frame, ut);
						v.build_task->skip_pylon_creep_check = true;
						build::set_build_pos(v.build_task, build_pos);
						v.build_task->send_builder_immediately = true;
						build::set_builder(v.build_task, builder, true);
						proxy_list.push_back(v);
						return true;
					}
				}
				return false;
			};

			auto cur_st = buildpred::get_my_current_state();
			
			//if (my_workers.size() >= 4 && count_units_plus_production(cur_st, unit_types::creep_colony) == 0 && count_units_plus_production(cur_st, unit_types::sunken_colony) < 2) {
			if (my_workers.size() >= 4 && n_sunkens_planned < 2) {
				add(unit_types::creep_colony);
			}

			for (unit* u : my_buildings) {
				if (u->is_completed) continue;
				double incoming_damage = 0.0;
				for (unit* e : visible_enemy_units) {
					weapon_stats* w = u->is_flying ? e->stats->air_weapon : e->stats->ground_weapon;
					if (!w) continue;
					if (e->game_order != BWAPI::Orders::AttackUnit || e->order_target != u) continue;
					double d = units_distance(e, u);
					if (d > w->max_range || d < w->max_range) continue;
					incoming_damage += w->damage / w->cooldown * 45.0;
				}
				if (incoming_damage > 0 && u->shields + u->hp <= incoming_damage) {
					++dead_count;
					log(log_level_test, "construction cancelled\n");
					for (auto& b : build::build_tasks) {
						if (b.built_unit == u) b.dead = true;
					}
					u->game_unit->cancelConstruction();
				}
			}

			for (auto i = proxy_list.begin(); i != proxy_list.end();) {

				bool dead = i->build_task->dead;
				if (i->build_task->dead) log(log_level_test, "build task is already dead\n");
				if (i->builder && i->builder->dead) {
					++dead_count;
					log(log_level_test, "builder is dead\n");
					dead = true;
				}
				bool built = i->build_task->built_unit != nullptr;
				if (!built && !build::set_build_pos(i->build_task, i->pos)) {
					log(log_level_test, "failed to set build pos\n");
					dead = true;
				}
				if (i->build_task->builder) {
					i->builder = i->build_task->builder;
				}

				if (built) {
					log(log_level_test, "proxy is built\n");
					i = proxy_list.erase(i);
					continue;
				}

				if (dead) {
					log(log_level_test, "dead_count is now %d\n", dead_count);
					log(log_level_test, "proxy is dead\n");
					i->build_task->dead = true;
					i = proxy_list.erase(i);
					continue;
				}

				++i;
			}

		}

	}

	virtual void mod_build(buildpred::state& st) override {

		using namespace unit_types;
		using namespace upgrade_types;

		st.dont_build_refineries = count_units_plus_production(st, extractor) == 1 && st.frame < 15 * 60 * 8;
		st.auto_build_hatcheries = current_frame >= 15 * 60 * 9;

		if (total_drones_made < 5) build_n(drone, 5);
		else build_n(drone, 4);
		build(zergling);
		
		if (count_units_plus_production(st, creep_colony)) build(sunken_colony);
		
	}

};
