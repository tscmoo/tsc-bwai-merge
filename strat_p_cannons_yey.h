

struct strat_p_cannons_yey : strat_p_base {


	xy enemy_main;
	xy enemy_natural;

	a_deque<xy> enemy_main_nat_path;
	a_deque<xy> my_nat_to_enemy_nat_path;
	a_deque<xy> cannon_path;

	struct proxy_t {
		unit_type* ut = nullptr;
		xy pos;
		xy pylon_pos;
		refcounted_ptr<build::build_task> build_task;
		refcounted_ptr<build::build_task> pylon_build_task;
		unit* builder = nullptr;
	};
	a_vector<proxy_t> proxy_list;
	int dead_count = 0;
	int cannon_path_state = 0;

	xy natural_pos;
	xy natural_cc_build_pos;
	combat::choke_t natural_choke;
	
	int build_prio_n = 0;

	virtual void init() override {

		sleep_time = 15;
		scouting::scout_supply = 20.0;
		
		build::never_add_pylons_or_creep = current_frame < 15 * 60 * 10 && my_units_of_type[unit_types::photon_cannon].empty() && dead_count == 0;

		multitasking::spawn(0.1, [&]() {

			while (true) {
				multitasking::sleep(1);

				for (size_t i = 0; i != cannon_path.size(); ++i) {
					if (i != 0) {
						game->drawLineMap(cannon_path[i - 1].x, cannon_path[i - 1].y, cannon_path[i].x, cannon_path[i].y, BWAPI::Colors::Red);
					}
				}

				for (auto& v : proxy_list) {
					game->drawBoxMap(v.pos.x, v.pos.y, v.pos.x + 32, v.pos.y + 32, BWAPI::Colors::Purple);
					game->drawTextMap(v.pos.x, v.pos.y, "%s", v.ut->name.c_str());
					if (v.pylon_pos != xy()) {
						game->drawLineMap(v.pos.x + 16, v.pos.y + 16, v.pylon_pos.x + 16, v.pylon_pos.y + 16, BWAPI::Colors::Purple);
						game->drawBoxMap(v.pylon_pos.x, v.pylon_pos.y, v.pylon_pos.x + 32, v.pylon_pos.y + 32, BWAPI::Colors::Cyan);
					}
				}
				
			}

		}, "cannons yey render");

	}

	virtual bool tick() override {
		if (opening_state == 0) {
			if (my_resource_depots.size() != 1 || my_workers.size() != 4) opening_state = -1;
			else {
				build::add_build_total(0.0, unit_types::probe, 9);
				++opening_state;
			}
		} else if (opening_state != -1) {
			if (bo_all_done()) {
				opening_state = -1;
			}
		}
		
		combat::attack_bases = true;

		if (current_frame < 15 * 60) {
			auto s = get_next_base();
			if (s) {
				natural_pos = s->pos;
				natural_cc_build_pos = s->cc_build_pos;

				natural_choke = combat::find_choke_from_to(unit_types::siege_tank_tank_mode, natural_pos, combat::op_closest_base, false, false, false, 32.0 * 10, 1, true);
			}
		}

		log(log_level_test, "enemy_main is %d %d\n", enemy_main.x, enemy_main.y);
		if (enemy_main == xy() && current_frame >= 15 * 30) {

			int scouts = start_locations.size() - 1;

			log(log_level_test, "scouting::all_scouts.size() is %d\n", scouting::all_scouts.size());

			if ((int)scouting::all_scouts.size() < scouts) {
				unit* scout_unit = get_best_score(my_workers, [&](unit*u) {
					if (u->controller->action != unit_controller::action_gather) return std::numeric_limits<double>::infinity();
					for (auto& v : scouting::all_scouts) {
						if (v.scout_unit == u) return std::numeric_limits<double>::infinity();
					}
					return 0.0;
				}, std::numeric_limits<double>::infinity());
				if (scout_unit) scouting::add_scout(scout_unit);
			}

			update_possible_start_locations();

			if (possible_start_locations.size() == 1) {
				enemy_main = possible_start_locations.front();
				enemy_natural = get_best_score_p(resource_spots::spots, [&](resource_spots::spot* s) {
					xy pos = s->cc_build_pos;
					if (diag_distance(pos - enemy_main) <= 32 * 8) return std::numeric_limits<double>::infinity();
					return unit_pathing_distance(unit_types::probe, enemy_main, pos);
				})->cc_build_pos;

				enemy_main += xy(4 * 16, 3 * 16);
				enemy_natural += xy(4 * 16, 3 * 16);
			}

		} else {
			rm_all_scouts();
		}
		if (enemy_main != xy()) {

			if (enemy_main_nat_path.empty()) {
				int iterations = 0;
				auto& pathing_map = square_pathing::get_pathing_map(unit_types::probe, square_pathing::pathing_map_index::no_enemy_buildings);
				a_deque<xy> path = square_pathing::find_square_path(pathing_map, enemy_natural, [&](xy pos, xy npos) {
					if (++iterations % 1024 == 0) multitasking::yield_point();
					return true;
				}, [&](xy pos, xy npos) {
					return 0.0;
				}, [&](xy pos) {
					return manhattan_distance(enemy_main - pos) <= 32;
				});

				enemy_main_nat_path = std::move(path);
				cannon_path = enemy_main_nat_path;
			}

			if (cannon_path_state == 0 && (dead_count >= 4 || players::my_player->minerals_lost >= 300)) {
				cannon_path_state = 1;

				int iterations = 0;
				auto& pathing_map = square_pathing::get_pathing_map(unit_types::probe, square_pathing::pathing_map_index::no_enemy_buildings);
				a_deque<xy> path = square_pathing::find_square_path(pathing_map, natural_pos, [&](xy pos, xy npos) {
					if (++iterations % 1024 == 0) multitasking::yield_point();
					return true;
				}, [&](xy pos, xy npos) {
					return 0.0;
				}, [&](xy pos) {
					return manhattan_distance(enemy_natural - pos) <= 32;
				});

				my_nat_to_enemy_nat_path = path;
				cannon_path = my_nat_to_enemy_nat_path;

			}

			a_vector<xy> cannons;
			for (unit* u : my_units_of_type[unit_types::photon_cannon]) {
				cannons.push_back(u->pos);
			}

			auto any_cannons_in_range = [&](xy pos) {
				for (xy cpos : cannons) {
					if (diag_distance(cpos - pos) <= 32 * 3) return true;
				}
				return false;
			};

			auto find_pos_in_range = [&](xy pos, xy npos, unit_type* ut) {
				auto pred = [&](grid::build_square& bs) {
					if (!ut->requires_creep && !build::build_has_not_creep(bs, ut)) return false;
					return build_spot_finder::is_buildable_at(ut, bs);
				};
				auto score = [&](xy pos) {
					double r = diag_distance(pos - npos);
					if (cannon_path_state == 0) {
						if (ut->requires_pylon && !build::build_has_pylon(grid::get_build_square(pos), ut)) r += 32 * 10;
						if (!any_cannons_in_range(pos)) r += 32 * 10;
					}
					return r;
				};

				std::array<xy, 1> starts;
				starts[0] = pos;
				return build_spot_finder::find_best(starts, 64, pred, score);
			};

			int n_cannons_planned = 0;
			int n_forges_planned = 0;
			int n_pylons_planned = 0;
			for (auto& v : proxy_list) {
				if (v.ut == unit_types::photon_cannon) ++n_cannons_planned;
				if (v.ut == unit_types::forge) ++n_forges_planned;
				if (v.pylon_pos != xy()) ++n_pylons_planned;
			}

			a_vector<unit*> available_builders;
			int gatherers = 0;
			for (unit* u : my_workers) {
				if (u->controller->action == unit_controller::action_gather) {
					++gatherers;
					if (gatherers >= 6) {
						available_builders.push_back(u);
					}
				}
			}

			bool has_damaged_pylon = false;
			for (unit* u : my_buildings) {
				if (u->type == unit_types::pylon && u->is_completed && u->shields + u->hp < u->stats->shields + u->stats->hp) has_damaged_pylon = true;
			}
			
			auto count_pylons_in_range = [&](xy pos) {
				int r = 0;
				for (unit* u : my_units_of_type[unit_types::pylon]) {
					if (pylons::is_in_pylon_range(u->pos - pos)) ++r;
				}
				return r;
			};

			auto add = [&](unit_type* ut, bool aggressive) {
				for (size_t i = 0; i != cannon_path.size(); ++i) {
					xy pos = cannon_path[i];
					xy npos = cannon_path[i + 12 >= cannon_path.size() ? cannon_path.size() - 1 : i + 12];
					if (!any_cannons_in_range(pos)) {
						multitasking::yield_point();
						xy build_pos = find_pos_in_range(pos, npos, ut);
						xy pylon_pos;
						bool want_more_pylons = has_damaged_pylon && my_units_of_type[unit_types::pylon].size() < 2 && n_pylons_planned == 0;
						if (my_units_of_type[unit_types::photon_cannon].size() + n_cannons_planned >= 4 && my_units_of_type[unit_types::pylon].size() < 3 && n_pylons_planned == 0) {
							if (count_pylons_in_range(build_pos) < 2) want_more_pylons = true;
						}
						if (build_pos != xy() && ut->requires_pylon && (!build::build_has_pylon(grid::get_build_square(pos), ut) || want_more_pylons)) {
							grid::reserve_build_squares(build_pos, ut);

							pylon_pos = find_pos_in_range(build_pos, build_pos, unit_types::pylon);
							if (pylon_pos == xy()) build_pos = xy();

							grid::unreserve_build_squares(build_pos, ut);
						}
						if (build_pos != xy()) {
							if (!available_builders.empty()) {
								unit* builder = available_builders.back();
								available_builders.pop_back();
								proxy_t v;
								v.ut = ut;
								v.pos = build_pos;
								v.pylon_pos = pylon_pos;
								double p = 0.0 + build_prio_n / 10.0;
								v.build_task = build::add_build_task(p, ut);
								++build_prio_n;
								v.build_task->skip_pylon_creep_check = true;
								build::set_build_pos(v.build_task, build_pos);
								if (pylon_pos != xy()) {
									v.pylon_build_task = build::add_build_task(p - 0.05, unit_types::pylon);
									v.pylon_build_task->send_builder_immediately = true;
									build::set_build_pos(v.pylon_build_task, build_pos);
									build::set_builder(v.pylon_build_task, builder, true);
								} else {
									v.build_task->send_builder_immediately = true;
									build::set_builder(v.build_task, builder, true);
								}
								proxy_list.push_back(v);
								return true;
							}
						}
					}
				}
				return false;
			};

			auto cur_st = buildpred::get_my_current_state();

			if (buildpred::count_units_plus_production(cur_st, unit_types::forge) + n_forges_planned == 0) {
				if (my_units_of_type[unit_types::pylon].size() < 2) {
					bool b = add(unit_types::forge, false);
					log(log_level_test, "add forge: %d\n", b);
				}
			}

			if (!my_units_of_type[unit_types::forge].empty() && (buildpred::count_units_plus_production(cur_st, unit_types::photon_cannon) < 5 || my_workers.size() >= 40)) {
			//if (buildpred::count_units_plus_production(cur_st, unit_types::pylon) >= 1 && (buildpred::count_units_plus_production(cur_st, unit_types::photon_cannon) < 5 || my_workers.size() >= 40)) {
				if (n_cannons_planned < 2 && (int)my_workers.size() >= 2 + current_frame / (15 * 60 * 2) + dead_count * 2) {
					if (cannon_path_state == 0 || (probe_count >= 16 && cannon_count < 2)) {
						bool b = add(unit_types::photon_cannon, true);
						log(log_level_test, "add cannon: %d\n", b);
					}
				}
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
				if (!built && i->build_task->type->unit == unit_types::forge && !my_units_of_type[unit_types::forge].empty()) {
					dead = true;
				}
				if (!built && i->build_task->type->unit == unit_types::forge && my_units_of_type[unit_types::pylon].size() >= 2) {
					dead = true;
				}
				if (i->pylon_pos != xy()) {
					if (i->build_task->builder) {
						build::set_builder(i->build_task, nullptr);
					}
					if (i->pylon_build_task->builder) {
						i->builder = i->pylon_build_task->builder;
					}
					dead |= i->pylon_build_task->dead;
					if (i->pylon_build_task->dead) log(log_level_test, "pylon build task is already dead\n");
					built &= i->pylon_build_task->built_unit != nullptr;
					if (i->pylon_build_task->built_unit != nullptr) {
						log(log_level_test, "pylon is built\n");
						i->build_task->skip_pylon_creep_check = false;
						i->pylon_pos = xy();
					} else if (!build::set_build_pos(i->pylon_build_task, i->pylon_pos)) {
						log(log_level_test, "failed to set pylon build pos\n");
						dead = true;
					}
				} else {
					if (i->build_task->builder) {
						i->builder = i->build_task->builder;
					}
					log(log_level_test, "there is no pylon\n");
					i->build_task->skip_pylon_creep_check = false;
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
					if (i->pylon_pos != xy()) i->pylon_build_task->dead = true;
					i = proxy_list.erase(i);
					continue;
				}

				++i;
			}

		}

		for (auto* a : combat::live_combat_units) {
			if (a->u->type == unit_types::dark_archon) {
				a->never_assign_to_aggressive_groups = a->u->energy < 150;
			}
		}

		attack_interval = 15 * 60 * 2;

		return is_island_map;
		//return current_used_total_supply >= 22.0 || my_units_of_type[unit_types::dragoon].size() >= 2;
	}
	virtual bool build(buildpred::state&st) override {
		using namespace buildpred;

		auto army = this->army;

		if (current_frame < 15 * 60 * 5 && count_units_plus_production(st, unit_types::pylon) == 0 && current_minerals < 300) return false;

		army = [army](state&st) {
			return maxprod(st, unit_types::zealot, army);
		};

		army = [army](state&st) {
			return maxprod(st, unit_types::dark_templar, army);
		};
		
		if (army_supply >= 8.0) {
			army = [army](state&st) {
				return nodelay(st, unit_types::high_templar, army);
			};
			army = [army](state&st) {
				return nodelay(st, unit_types::archon, army);
			};
		}
		
		army = [army](state&st) {
			return nodelay(st, upgrade_types::protoss_ground_weapons_1, army);
		};
		if (army_supply >= std::min(enemy_army_supply, 12.0)) {
			army = [army](state&st) {
				return nodelay(st, upgrade_types::leg_enhancements, army);
			};
		}

//		if (!has_or_is_upgrading(st, upgrade_types::argus_talisman)) {
//			army = [army](state&st) {
//				return nodelay(st, upgrade_types::argus_talisman, army);
//			};
//		} else if (!has_or_is_upgrading(st, upgrade_types::mind_control)) {
//			army = [army](state&st) {
//				return nodelay(st, upgrade_types::mind_control, army);
//			};
//		} else {
//			army = [army](state&st) {
//				return nodelay(st, unit_types::dark_archon, army);
//			};
//		}

// 		if (probe_count >= 20) {
// 			army = [army](state&st) {
// 				return maxprod(st, unit_types::arbiter, army);
// 			};
// // 			if (dark_archon_count < arbiter_count) {
// // 				army = [army](state&st) {
// // 					return maxprod(st, unit_types::dark_archon, army);
// // 				};
// // 			}
// 		}

		if (probe_count < 20 || (st.frame >= 15 * 60 * 12 && probe_count < 40)) {
			army = [army](state&st) {
				return nodelay(st, unit_types::probe, army);
			};
		}
		if (my_units_of_type[unit_types::pylon].size() >= 2) {
			if (count_units_plus_production(st, unit_types::forge) == 0) {
				army = [army](state&st) {
					return nodelay(st, unit_types::forge, army);
				};
			}
		}
// 		if (probe_count >= 18 && cannon_count >= 2 && army_supply >= enemy_army_supply) {
// 			if (count_units_plus_production(st, unit_types::nexus) < 4) {
// 				army = [army](state&st) {
// 					return nodelay(st, unit_types::nexus, army);
// 				};
// 			}
// 		}
		if (probe_count >= 15) {
//			if (count_units_plus_production(st, unit_types::nexus) < 2) {
//				army = [army](state&st) {
//					return nodelay(st, unit_types::nexus, army);
//				};
//			}
		}

		if (force_expand && count_production(st, unit_types::nexus) == 0) {
			army = [army](state&st) {
				return nodelay(st, unit_types::nexus, army);
			};
		}

		return army(st);
	}

};

