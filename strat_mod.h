

struct strat_mod : strat_base {
	
	virtual void mod_init() = 0;

	virtual void init() override {

		sleep_time = 15;

		call_place_static_defence = false;
		
		mod_init();

	}

	xy natural_pos;
	xy natural_cc_build_pos;
	combat::choke_t natural_choke;
	int max_mineral_workers = 0;

	xy static_defence_pos;
	xy build_nydus_pos;
	bool has_nydus_without_exit = false;
	bool static_defence_pos_is_undefended = false;
	
	bool enable_auto_upgrades = false;
	
	bool place_ffe = false;
	bool place_ffe_all_pylons = false;
	bool place_ffe_everything = false;
	bool being_pool_rushed = false;
	bool being_zealot_rushed = false;
	bool being_marine_rushed = false;
	
	bool being_early_rushed = false;
	bool use_default_early_rush_defence = true;
	bool early_defence_is_scared = true;

	bool use_opening_state = false;
	
	xy pylon_pos;
	
	bool strat_is_done = false;
	
	virtual void mod_tick() = 0;
	
	void merge_high_templars() {
		a_vector<combat::combat_unit*> hts;
		for (auto* a : combat::live_combat_units) {
			if (a->u->type != unit_types::high_templar) continue;
			//if (current_frame - a->last_fight >= 15*30) continue;
			hts.push_back(a);
		}
		for (auto* a : hts) {
			if (current_frame <= a->u->controller->nospecial_until) continue;
			//if (current_frame - a->last_fight >= 15*30 && a->u->energy >= 60) continue;
			if (a->u->energy >= 50) continue;
			combat::combat_unit* n = nullptr;
			for (auto* a2 : hts) {
				if (current_frame <= a2->u->controller->nospecial_until) continue;
				if (a2 == a) continue;
				if (a2->u->energy >= 60) continue;
				if (diag_distance(a->u->pos - a2->u->pos) >= 32 * 8) continue;
				n = a2;
				break;
			}
			if (n) {
				if (current_frame >= a->u->controller->nospecial_until) {
					auto tech = BWAPI::TechTypes::Archon_Warp;
					a->u->game_unit->useTech(tech, n->u->game_unit);
					a->u->controller->noorder_until = current_frame + 30;
					a->u->controller->nospecial_until = current_frame + 30;
					n->u->game_unit->useTech(tech, a->u->game_unit);
					n->u->controller->noorder_until = current_frame + 30;
					n->u->controller->nospecial_until = current_frame + 30;
				}
			}
		}
	}

	virtual bool tick() override {
		if (opening_state != -1 && !use_opening_state) opening_state = -1;

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
		
		//if (drone_count < 30) {
		if (my_workers.size() < 30 || (my_workers.size() < 50 && current_gas >= 250)) {
			double total_gas = 1.0;
			double max_gas = 1.0;
			for (auto& b : build::build_order) {
				if (b.build_frame == 0) continue;
				if (current_frame - b.build_frame > 15 * 60 * 2) continue;
				total_gas += b.type->gas_cost;
				max_gas = std::max(max_gas, b.type->gas_cost);
			}
			resource_gathering::max_gas = my_workers.size() < 20 ? max_gas : total_gas;
		} else resource_gathering::max_gas = 0.0;

		threats::eval(*this);

// 		if (hydralisk_count) min_bases = 3;
// 		max_bases = 3;

		tactics::enable_anti_push_until = current_frame + 15 * 10;

		if (enable_auto_upgrades) {
			get_upgrades::set_no_auto_upgrades(false);
		} else {
			if (current_used_total_supply < 180) {
				get_upgrades::set_no_auto_upgrades(true);
			} else get_upgrades::set_no_auto_upgrades(false);
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
		
		unit_type* static_defence_type = unit_types::creep_colony;
		if (players::my_player->race == race_terran) static_defence_type = unit_types::missile_turret;
		else if (players::my_player->race == race_protoss) static_defence_type = unit_types::photon_cannon;

		static_defence_pos = xy();
		static_defence_pos_is_undefended = false;
		if (bases >= 3 || enemy_air_army_supply) {
			for (auto& b : build::build_tasks) {
				if (b.built_unit || b.dead) continue;
				if (b.type->unit == static_defence_type) {
					build::unset_build_pos(&b);
				}
			};
			for (auto& v : cur_st.bases) {
				auto& bs = grid::get_build_square(v.s->cc_build_pos);
				if (diag_distance(bs.pos - my_start_location) <= 32 * 10) continue;
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
						xy pos = get_static_defence_position_next_to(bs.building, static_defence_type);
						if (pos != xy()) {
							static_defence_pos = pos;
							static_defence_pos_is_undefended = true;
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
						xy pos = get_static_defence_position_next_to(bs.building, static_defence_type);
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
		
		pylon_pos = xy();
		if (probe_count) {
			for (auto& v : cur_st.bases) {
				auto& bs = grid::get_build_square(v.s->cc_build_pos);
				if (bs.building && bs.building->owner == players::my_player) {
					double d = get_best_score_value(my_units_of_type[unit_types::pylon], [&](unit* u) {
						return diag_distance(u->pos - v.s->pos);
					});
					if (d <= 32 * 6) continue;
					double d2 = get_best_score_value(build::build_tasks, [&](auto& b) {
						if (b.type->unit != unit_types::pylon) return std::numeric_limits<double>::infinity();
						return diag_distance(b.build_pos);
					});
					if (d2 <= 32 * 6) continue;
					xy pos = get_static_defence_position_next_to(bs.building, unit_types::pylon);
					if (pos != xy()) {
						pylon_pos = pos;
					}
				}
			}
		}
		
		if (early_defence_is_scared) {
			if (current_frame < 15 * 60 * 6 && army_supply < 7.0) {
				if (players::my_player->race != race_zerg && dragoon_count + vulture_count + tank_count == 0) {
					combat::force_defence_is_scared_until = current_frame + 15 * 10;
				} else combat::defence_is_scared = false;
			} else combat::defence_is_scared = false;
		}
		
		merge_high_templars();
		
		if (current_frame <= 15 * 60 * 6) {
			if (!being_early_rushed) {
				update_possible_start_locations();
				for (unit* u : enemy_units) {
					//if (u->type == unit_types::zergling || u->type == unit_types::zealot || u->type == unit_types::marine) {
					if (u->type == unit_types::zergling) {
						double neg_travel_distance = get_best_score_value(possible_start_locations, [&](xy pos) {
							return -unit_pathing_distance(u, pos);
						});
						if (neg_travel_distance) {
							int travel_time = frames_to_reach(u, -neg_travel_distance);
							int spawn_time = u->creation_frame - travel_time;
							int t = 15 * 60 * 3 + 15 * 30;
							if (u->type == unit_types::zealot || u->type == unit_types::marine) t = 15 * 60 * 4 + 16;
							if (spawn_time <= t) {
								if (u->type == unit_types::zealot) {
									log(log_level_test, " being zealot rushed\n");
									being_zealot_rushed = true;
								} else if (u->type == unit_types::marine) {
									log(log_level_test, " being marine rushed\n");
									being_marine_rushed = true;
								} else {
									log(log_level_test, " being pool rushed\n");
									being_pool_rushed = true;
								}
								break;
							}
						}
					}
					if (u->type == unit_types::spawning_pool) {
						double progress = u->hp / u->stats->hp;
						int build_time = current_frame - (int)(u->type->build_time * progress);
						if (build_time <= 15 * 60 * 2) {
							log(log_level_test, " being pool rushed\n");
							being_pool_rushed = true;
							break;
						}
					}
//					if (u->type == unit_types::gateway) {
//						double progress = u->hp / u->stats->hp;
//						int build_time = current_frame - (int)(u->type->build_time * progress);
//						if (build_time <= 15 * 60 * 3 - 15 * 5) {
//							log(log_level_test, " being zealot rushed\n");
//							being_zealot_rushed = true;
//							break;
//						}
//					}
//					if (u->type == unit_types::barracks) {
//						double progress = u->hp / u->stats->hp;
//						int build_time = current_frame - (int)(u->type->build_time * progress);
//						if (build_time <= 15 * 60 * 3 - 15 * 5) {
//							log(log_level_test, " being marine rushed\n");
//							being_marine_rushed = true;
//							break;
//						}
//					}
				}
				
				if (army_supply < enemy_attacking_army_supply) {
					if (enemy_zealot_count >= 2) being_zealot_rushed = true;
					if (enemy_marine_count >= 2 && current_frame < 15 * 60 * 6) being_marine_rushed = true;
				}
				if (threats::being_marine_rushed && (enemy_marine_count >= current_frame / (15 * 60) || enemy_barracks_count >= 2)) being_marine_rushed = true;
			}
		}
		if (current_frame < 15 * 60 * 10 && current_used_total_supply < 26 && my_units_of_type[unit_types::photon_cannon].size() < 2) {
			being_early_rushed = being_pool_rushed || being_zealot_rushed || being_marine_rushed || enemy_attacking_worker_count >= my_workers.size() / 2 || enemy_proxy_building_count;
		} else {
			if (being_early_rushed) being_early_rushed = false;
		}
		if (being_early_rushed) {
			combat::combat_mult_override = 0.8;
			combat::combat_mult_override_until = current_frame + 15 * 10;
			if (current_used_total_supply < 20 && dragoon_count + vulture_count + tank_count == 0) {
				rm_all_scouts();
				combat::my_closest_base_override = my_start_location;
				combat::force_defence_is_scared_until = current_frame + 15 * 10;
			} else combat::defence_is_scared = false;
			if (army_supply < 4.0 && current_minerals < 100 && current_frame < 15 * 60 * 5 && current_used_total_supply < 20 && my_completed_units_of_type[unit_types::photon_cannon].size() < 2) {
//				if (my_units_of_type[unit_types::forge].empty()) {
//					resource_gathering::max_gas = 1.0;
//					a_unordered_set<build::build_task*> keep;
//					auto keep_one = [&](unit_type* ut) {
//						if (my_completed_units_of_type[ut].empty()) {
//							auto* t = get_best_score_p(build::build_tasks, [&](build::build_task* t) {
//								if (t->type->unit != ut) return std::numeric_limits<double>::infinity();
//								if (t->built_unit) return (double)t->built_unit->remaining_build_time;
//								return (double)ut->build_time;
//							}, std::numeric_limits<double>::infinity());
//							if (t) keep.insert(t);
//						}-
//					};
//					keep_one(unit_types::supply_depot);
//					keep_one(unit_types::barracks);
//					keep_one(unit_types::bunker);
//					keep_one(unit_types::pylon);
//					keep_one(unit_types::gateway);
//					keep_one(unit_types::forge);
//					keep_one(unit_types::spawning_pool);
//					keep_one(unit_types::factory);
//					for (auto& b : build::build_tasks) {
//						if ((!b.type->unit || b.type->unit->is_building) && keep.count(&b) == 0 && b.type->unit != unit_types::photon_cannon && b.type->unit != unit_types::creep_colony && b.type->unit != unit_types::sunken_colony && b.type->unit != unit_types::hatchery && b.type->unit != unit_types::nexus && b.type->unit != unit_types::cc) {
//							b.dead = true;
//							if (b.built_unit) b.built_unit->game_unit->cancelConstruction();
//						}
//					}
//				}
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
					log(log_level_test, "construction cancelled\n");
					for (auto& b : build::build_tasks) {
						if (b.built_unit == u) b.dead = true;
					}
					u->game_unit->cancelConstruction();
				}
			}
		}
		
		if (current_frame	< 15 * 60 * 10) {
			
			if (army_supply < 8.0 && enemy_proxy_building_count && enemy_army_supply == 0.0) {

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

				if (proxy_pos != xy() && my_workers.size() < 18 && current_used_total_supply < 20) {
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
			
		}

		mod_tick();

		return strat_is_done;
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
	void maxprod(unit_type* ut) {
		using namespace buildpred;
		auto& army = this->build_army;
		army = [army = std::move(army), ut](state& st) {
			return buildpred::maxprod(st, ut, army);
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
		int cn = count_units_plus_production(*build_st, ut);
		if (ut == unit_types::siege_tank_tank_mode) cn += count_units_plus_production(*build_st, unit_types::siege_tank_siege_mode);
		if (cn < n) {
			auto& army = this->build_army;
			army = [army = std::move(army), ut](state& st) {
				return nodelay(st, ut, army);
			};
			return false;
		}
		return true;
	}
	bool build_total(unit_type* ut, int n) {
		using namespace buildpred;
		if (total_units_made_of_type[ut] + count_production(*build_st, ut) < n) {
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
	
	virtual void mod_build(buildpred::state&st) = 0;

	virtual bool build(buildpred::state&st) override {
		using namespace buildpred;
		if (opening_state != -1) return false;

		st.auto_build_hatcheries = true;

		//st.dont_build_refineries = count_units_plus_production(st, unit_types::extractor) && drone_count < 18;
		st.dont_build_refineries = drone_count + scv_count < 18;

		build_army = army;
		build_st = &st;
		
		//if (being_early_rushed && army_supply < 6.0 && my_workers.size() < 20 && st.frame < 15 * 60 * 10 && st.minerals < 400) {
		if (being_early_rushed && use_default_early_rush_defence && army_supply < 6.0 && my_workers.size() < 20 && st.frame < 15 * 60 * 10) {
			using namespace unit_types;
			//if (count_units_plus_production(st, drone)) {
			if (false) {
				if (st.used_supply[race_zerg] >= st.max_supply[race_zerg] - 2 && current_used_total_supply < st.used_supply[race_zerg]) return false;
				if (my_completed_units_of_type[unit_types::hatchery].size() >= 2 || st.minerals >= 150) {
					if (enemy_army_supply) build_n(sunken_colony, 2);
				}
				build(zergling);
				if (total_units_made_of_type[drone] + count_production(st, drone) < 9) build(drone);
				if (st.frame >= 15 * 60 * 5 && count_production(st, drone) == 0 && army_supply >= 6.0) build(drone);
				if (!my_completed_units_of_type[unit_types::creep_colony].empty()) build(sunken_colony);
				return build_army(st);
			} else if (count_units_plus_production(st, probe)) {
				if (players::opponent_player->race == race_terran) {
					if (probe_count >= 16) build(probe);
					build_n(cybernetics_core, 1);
					build_n(assimilator, 1);
					if (probe_count < 16) build(probe);
				}
				//if (st.minerals >= 250) build_n(shield_battery, 1);
				if (st.minerals >= 250) build_n(probe, 24);
				if (st.used_supply[race_protoss] >= st.max_supply[race_protoss] - 2 && current_used_total_supply < st.used_supply[race_protoss]) return false;
				if (st.frame >= 15 * 60 * 5) build_n(gateway, 2);
				build_n(probe, 11);
				if (count_units_plus_production(st, forge) && cannon_count < 3) build(photon_cannon);
				else build(zealot);
				if (!st.units[cybernetics_core].empty()) build(dragoon);
				if (count_units_plus_production(st, zealot) >= 2) build_n(probe, 10);
				if (st.frame >= 15 * 60 * 5 && count_production(st, probe) == 0 && army_supply >= 4.0) build(probe);
				return build_army(st);
			} else if (count_units_plus_production(st, scv)) {
				if (st.used_supply[race_terran] >= st.max_supply[race_terran] - 2 && current_used_total_supply < st.used_supply[race_terran]) return false;
				if (scv_count >= 16) {
					build(vulture);
				}
				build_n(scv, 30);
				build(marine);
				if (!my_completed_units_of_type[factory].empty()) build_n(vulture, 2);
				if (st.frame >= 15 * 60 * 5 && count_production(st, scv) == 0 && army_supply >= 4.0) build(scv);
				//if (scv_count >= 11 && st.units[marine].empty()) build_n(bunker, 1);
				return build_army(st);
			}
		}

		mod_build(st);

		return build_army(st);
	}

	virtual void post_build() override {

		if (output_enabled) {
			for (auto* bs : natural_choke.build_squares) {
				game->drawBoxMap(bs->pos.x, bs->pos.y, bs->pos.x + 32, bs->pos.y + 32, BWAPI::Colors::Orange, true);
			}
		}

		if (natural_pos != xy() && !being_early_rushed) {
			combat::control_spot* cs = get_best_score(combat::active_control_spots, [&](combat::control_spot* cs) {
				return diag_distance(cs->pos - natural_pos);
			});
			if (cs) {
				unit_type* static_defence_type = unit_types::creep_colony;
				if (players::my_player->race == race_terran) static_defence_type = unit_types::missile_turret;
				else if (players::my_player->race == race_protoss) static_defence_type = unit_types::photon_cannon;
				size_t iterations = 0;
				for (auto& b : build::build_tasks) {
					if (b.built_unit || b.dead) continue;
					if (b.type->unit == unit_types::nydus_canal && build_nydus_pos != xy()) {
						build::set_build_pos(&b, build_nydus_pos);
					}
					if (b.type->unit == static_defence_type && static_defence_pos != xy()) {
						build::unset_build_pos(&b);
						build::set_build_pos(&b, static_defence_pos);
						continue;
					}
					if ((b.type->unit == static_defence_type && (static_defence_type != unit_types::missile_turret || my_units_of_type[unit_types::missile_turret].empty())) || b.type->unit == unit_types::bunker) {
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
		
		if (pylon_pos != xy()) {
			for (auto& b : build::build_order) {
				if (b.built_unit || b.dead) continue;
				if (b.type->unit == unit_types::pylon) {
					build::set_build_pos(&b, pylon_pos);
					break;
				}
			}
		}
		
		if (place_ffe) {
			
			if (natural_pos != xy()) {
				combat::control_spot* cs = get_best_score(combat::active_control_spots, [&](combat::control_spot* cs) {
					return diag_distance(cs->pos - natural_pos);
				});
				if (cs) {
					bool place_pylon = false;
					
					size_t n_pylons = my_units_of_type[unit_types::pylon].size();
					if (place_ffe_all_pylons || (n_pylons == 0 || n_pylons == 2)) place_pylon = true;
					
					size_t iterations = 0;
					for (auto& b : build::build_tasks) {
						if (b.built_unit || b.dead) continue;
						if (b.type->unit == unit_types::photon_cannon && static_defence_pos != xy()) {
							//build::unset_build_pos(&b);
							//build::set_build_pos(&b, static_defence_pos);
							if (!build::set_build_pos(&b, static_defence_pos)) b.dead = true;
							continue;
						}
						if (b.type->unit == unit_types::photon_cannon || (place_pylon && b.type->unit == unit_types::pylon) || b.type->unit == unit_types::forge || (place_ffe_everything && b.type->unit && b.type->unit->is_building && b.type->unit != unit_types::pylon)) {
							xy pos = b.build_pos;
							build::unset_build_pos(&b);
	
							a_vector<std::tuple<xy, double>> scores;
	
							auto pred = [&](grid::build_square&bs) {
								if (++iterations % 1000 == 0) multitasking::yield_point();
								
								xy pos = bs.pos + xy(b.type->unit->tile_width * 16, b.type->unit->tile_height * 16);
		
								for (unit* u : enemy_units) {
									if (u->gone) continue;
									if (u->stats->ground_weapon && units_distance(u->pos, u->type, pos, b.type->unit) <= u->stats->ground_weapon->max_range) {
										bool okay = false;
										for (unit* u2 : my_units) {
											auto* w = u->is_flying ? u2->stats->air_weapon : u2->stats->ground_weapon;
											if (w && units_distance(u2, u) <= w->max_range) {
												okay = true;
												break;
											}
										}
										if (!okay) return false;
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

	}

};
