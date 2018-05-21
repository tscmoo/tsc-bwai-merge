
struct unit_controller {
	unit*u = nullptr;
	enum { action_idle, action_move, action_gather, action_build, action_attack, action_kite, action_scout, action_move_directly, action_use_ability, action_repair, action_building_movement, action_recharge, action_worker_micro, action_protect_marines_micro };
	int action = action_idle;
	xy go_to;
	unit*target = nullptr;
	unit*kite_attack_target = nullptr;
	unit*depot = nullptr;
	xy target_pos;
	unit_type*target_type = nullptr;
	upgrade_type*ability = nullptr;
	void*flag = nullptr;

	bool can_move = false;
	int last_move = 0;
	xy last_move_to_pos;
	int last_move_to = 0;
	int move_order = 0;
	int noorder_until = 0;
	int nospecial_until = 0;
	int wait_until = 0;
	int last_reached_go_to = 0;
	int fail_build_count = 0;
	xy move_away_from;
	int move_away_until = 0;
	int at_go_to_counter = 0;
	int last_siege = 0;
	int last_return_cargo = 0;
	bool lift = false;
	int timeout = 0;
	int attack_state = 0;
	int attack_timer = 0;
	int attack_state2 = 0;
	int weapon_ready_frames = 0;
	int last_process = 0;
	int not_moving_counter = 0;
	int last_command_frame = 0;
	int last_re_evaluate_target = 0;
	unit*re_evaluate_target = nullptr;
	xy re_evalute_move_to;
	xy defensive_concave_position;
	int return_minerals_frame = 0;
	unit*last_gather_target = nullptr;
	int last_gather_issued_frame = 0;
	int last_burrow = 0;
	int last_pickup = 0;
	int last_unload = 0;
	double incoming_damage;
	unit*pickup_to = nullptr;
	int no_unload_until = 0;
	int lay_mines_until = 0;
	int last_fast_kill_spider_mine = 0;
	int close_up_until = 0;
	int last_mineral_walk = 0;
	int carrier_no_threats_until = 0;
};

namespace unit_controls {
	a_deque<unit_controller> unit_controller_container;
}

unit_controller*get_unit_controller(unit*u) {
	if (u->controller) return u->controller;
	unit_controls::unit_controller_container.emplace_back();
	unit_controller*c = &unit_controls::unit_controller_container.back();
	c->move_order = unit_controls::unit_controller_container.size();
	c->u = u;
	c->go_to = u->pos;
	c->action = unit_controller::action_idle;
	u->controller = c;
	return u->controller;
}

namespace unit_controls {
;

a_map<xy, int> fail_build_positions;

static const int move_resolution = 4;

void move(unit_controller*c) {

	unit*u = c->u;

	if (diag_distance(u->pos - c->go_to) <= 32)  c->last_reached_go_to = current_frame;

	xy move_to;
	if (u->type->is_flyer) {

		move_to = c->go_to;

		if (u->irradiate_timer) {

			if (diag_distance(move_to - u->pos) > 32 * 10) {
				xy relpos = move_to - u->pos;
				double a = std::atan2(relpos.y, relpos.x);
				move_to.x = u->pos.x + (int)(std::cos(a) * 32 * 10);
				move_to.y = u->pos.y + (int)(std::sin(a) * 32 * 10);
			}

			a_vector<unit*> nearby_bio_allies;
			for (unit*au : my_units) {
				if (au->type->is_non_usable) continue;
				if (!au->type->is_biological) continue;
				double d = diag_distance(u->pos - au->pos);
				if (d >= 32 * 8) continue;
				nearby_bio_allies.push_back(au);
			}
			if (!nearby_bio_allies.empty()) {
				auto n_at = [&](xy pos) {
					int r = 0;
					for (unit*au : nearby_bio_allies) {
						if (diag_distance(au->pos - pos) <= 32 * 4) ++r;
					}
					return r;
				};
				xy safe_pos;
				a_deque<xy> path = flyer_pathing::find_path(u->pos, [&](xy pos, xy npos) {
					return diag_distance(npos - u->pos) < 32 * 12;
				}, [&](xy pos, xy npos) {
					double r = diag_distance(npos - move_to);
					int n = n_at(npos);
					if (n == 0 && safe_pos == xy()) safe_pos = npos;
					r += n * 32 * 5;
					return r;
				}, [&](xy pos) {
					return (pos - move_to).length() <= 32;
				});
				if (!path.empty()) {
					move_to = path.back();
					for (xy pos : path) {
						if (diag_distance(pos - u->pos) >= 32 * 4) {
							move_to = pos;
							break;
						}
					}
					if (n_at(move_to)) move_to = safe_pos;
				}
			}
		}

	} else {

		unit* nydus = square_pathing::get_nydus_canal_from_to(square_pathing::get_pathing_map(u->type), u->pos, c->go_to);
		if (nydus) {
			//log(log_level_test, "moving through nydus %p\n", nydus);
			if (units_distance(u, nydus) <= 64) {

				if (current_frame >= c->last_move_to + 15) {

					u->game_unit->rightClick(nydus->game_unit);

					c->noorder_until = current_frame + 30;
					c->last_move_to = current_frame;
					return;
				}

			} else {

				move_to = square_pathing::get_move_to(u, nydus->pos, c->last_reached_go_to, c->last_move_to_pos);

				xy pos = square_pathing::get_pos_in_square(move_to, u->type);
				if (pos != xy()) move_to = pos;
			}

		} else {

			move_to = square_pathing::get_move_to(u, c->go_to, c->last_reached_go_to, c->last_move_to_pos);

			xy pos = square_pathing::get_pos_in_square(move_to, u->type);
			//if (pos == xy()) log(" !! move: get_pos_in_square for %s failed\n", u->type->name);
			if (pos != xy()) move_to = pos;
		}
	}

	if (diag_distance(u->pos - move_to) <= 8) move_to = c->go_to;
	if (u->type == unit_types::dragoon) move_to = c->go_to;
	if (u->type == unit_types::lurker) move_to = c->go_to;
	
	//if (u->game_order != BWAPI::Orders::Move || u->game_unit->getOrderTargetPosition() != BWAPI::Position(move_to.x, move_to.y)) {
	if (c->last_move_to_pos != move_to || current_frame >= c->last_move_to + 30) {
		c->last_move_to = current_frame;
		c->last_move_to_pos = move_to;
		move_to += xy(-2, -2) + xy(rng(5), rng(5));
		//if (u->game_unit->isSieged()) u->game_unit->unsiege();
		//else u->game_unit->move(BWAPI::Position(move_to.x, move_to.y));

// 		if (u->type->is_flyer && false) {
// 			xy relpos = move_to - u->pos;
// 			double a = atan2(relpos.y, relpos.x);
// 			double r = relpos.length();
// 			relpos.x = (int)(cos(a)*(r + 15 * 8));
// 			relpos.y = (int)(sin(a)*(r + 15 * 8));
// 			if ((size_t)(u->pos.x + relpos.x) < (size_t)grid::map_width && (size_t)(u->pos.y + relpos.y) < (size_t)grid::map_height) {
// 				move_to = u->pos + relpos;
// 			}
// 		}

//		if (u->type == unit_types::siege_tank_siege_mode) {
//			if ((u->pos - move_to).length() <= 32 * 6) move_to = u->pos;
//		}
		
		//if ((u->pos - move_to).length() <= 16) {
		if ((u->pos - c->go_to).length() <= 8) {
			++c->at_go_to_counter;
			if (u->type == unit_types::siege_tank_tank_mode && c->at_go_to_counter>=20) {
				u->game_unit->siege();
				c->last_siege = current_frame;
			} else {
				if (u->game_order != BWAPI::Orders::HoldPosition) {
					u->game_unit->holdPosition();
				}
			}
		} else {
			if (u->loaded_into) {
				if (current_frame >= c->no_unload_until) {
					log("unload because unit_controls\n");
					u->loaded_into->game_unit->unload(u->game_unit);
				}
			} else if (u->game_unit->isSieged()) {
				if (current_frame - u->last_attacked >= 90 && current_frame - c->last_siege >= 15 * 8) u->game_unit->unsiege();
			} else if (u->game_unit->isBurrowed()) {
				if (current_frame - c->last_burrow >= 120) u->game_unit->unburrow();
			} else if (u->type == unit_types::medic) {
				if (!bwapi_is_healing_order(u->game_order)) {
					u->game_unit->useTech(upgrade_types::healing->game_tech_type, BWAPI::Position(move_to.x, move_to.y));
				}
			} else u->game_unit->move(BWAPI::Position(move_to.x, move_to.y));
		}

		c->noorder_until = current_frame + 4 + rng(4);
	}

}

void move_away(unit_controller*c) {
	auto path = square_pathing::find_square_path(square_pathing::get_pathing_map(c->u->type), c->u->pos, [&](xy pos, xy npos) {
		return true;
	}, [&](xy pos, xy npos) {
		return 0.0;
	}, [&](xy pos) {
		return diag_distance(c->move_away_from - pos) >= 32 * 10;
	});
	auto move_to = square_pathing::get_go_to_along_path(c->u, path);
	move_to += xy(-2, -2) + xy(rng(5), rng(5));
	c->u->game_unit->move(BWAPI::Position(move_to.x, move_to.y));
	c->noorder_until = current_frame + 4 + rng(4);
}

bool move_stuff_away_from_build_pos(xy build_pos, unit_type*ut, unit_controller*builder) { 
	bool something_in_the_way = false;
	for (unit*nu : visible_units) {
		if (nu->building) continue;
		if (nu->is_flying) continue;
		if (nu->owner->is_enemy && nu->cloaked && !nu->detected) continue;
		if (builder && nu == builder->u) continue;
		xy upper_left = nu->pos - xy(nu->type->dimension_left(), nu->type->dimension_up());
		xy bottom_right = nu->pos + xy(nu->type->dimension_right(), nu->type->dimension_down());
		if (nu->owner == players::my_player && nu->controller->can_move) {
			upper_left.x -= 32 * 2;
			upper_left.y -= 32 * 2;
			bottom_right.x += 32 * 2;
			bottom_right.y += 32 * 2;
		}
		int x1 = build_pos.x;
		int y1 = build_pos.y;
		int x2 = build_pos.x + ut->tile_width * 32;
		int y2 = build_pos.y + ut->tile_height * 32;
		if (bottom_right.x < x1) continue;
		if (bottom_right.y < y1) continue;
		if (upper_left.x > x2) continue;
		if (upper_left.y > y2) continue;
		something_in_the_way = true;
		if (builder) {
			if (nu->owner != players::my_player || (!nu->controller->can_move && nu->type != unit_types::siege_tank_siege_mode)) {
				if (current_used_total_supply - my_workers.size() == 0) {
					int&blocked_until = grid::get_build_square(build_pos + xy(ut->tile_width * 16, ut->tile_height * 16)).blocked_until;
					if (blocked_until < current_frame + 15 * 30) blocked_until = current_frame + 15 * 30;
				}
				if (builder->u->game_unit->attack(nu->game_unit)) {
					builder->noorder_until = current_frame + 15;
					break;
				}
			}
		}
		if (nu->owner == players::my_player) {
			nu->controller->move_away_from = xy((x1 + x2) / 2, (y1 + y2) / 2);
			nu->controller->move_away_until = current_frame + 15 * 2;
		}
	}
	return something_in_the_way;
}

double get_damage_type_modifier(int damage_type, int target_size) {
	if (damage_type == weapon_stats::damage_type_concussive) {
		if (target_size == unit_type::size_small) return 1.0;
		if (target_size == unit_type::size_medium) return 0.5;
		if (target_size == unit_type::size_large) return 0.25;
	}
	if (damage_type == weapon_stats::damage_type_normal) return 1.0;
	if (damage_type == weapon_stats::damage_type_explosive) {
		if (target_size == unit_type::size_small) return 0.5;
		if (target_size == unit_type::size_medium) return 0.75;
		if (target_size == unit_type::size_large) return 1.0;
	}
	return 1.0;
}

double get_weapon_damage_in_range(unit*u, unit*target) {
	weapon_stats*w = target->is_flying ? u->stats->air_weapon : u->stats->ground_weapon;
	if (!w) return 0.0;
	double d = units_distance(u, target);
	if (d > w->max_range || d < w->min_range) return 0.0;
	double damage = w->damage;
	if (target->shields <= 0) damage *= get_damage_type_modifier(w->damage_type, target->stats->type->size);
	damage -= target->stats->armor;
	if (damage <= 0) damage = 1.0;
	damage *= w == u->stats->ground_weapon ? u->stats->ground_weapon_hits : u->stats->air_weapon_hits;
	return damage;
}

void shuttle_micro(unit_controller*c) {

	a_vector<unit*> potential_pickups;
	for (unit*u : my_units) {
		if (u->is_flying || u->building) continue;
		if (u->type->total_minerals_cost + u->type->total_gas_cost < 125 && u != c->target) continue;
		if (u->is_loaded) continue;
		double d = diag_distance(c->u->pos - u->pos);
		if (d > 64) continue;
		if (u->type == unit_types::siege_tank_siege_mode || u->is_morphing) continue;

		if (current_frame - u->controller->last_pickup < 6 && u->controller->pickup_to != c->u) continue;

		potential_pickups.push_back(u);
		u->controller->incoming_damage = 0.0;
	}

	for (unit*e : enemy_units) {
		//log(log_level_info, "%p :: %s order is %s\n", e, e->type->name, e->game_order.c_str());
		if (e->weapon_cooldown > latency_frames) continue;
		if ((e->game_order == BWAPI::Orders::AttackUnit || e->game_order == BWAPI::Orders::ScarabAttack) && e->order_target && e->order_target->controller) {
			if (e->game_order == BWAPI::Orders::ScarabAttack) e->order_target->controller->incoming_damage += 100.0;
			else e->order_target->controller->incoming_damage += get_weapon_damage_in_range(e, e->order_target);
		} else if (current_frame - e->last_attacked <= 60 && e->last_attack_target && e->last_attack_target->controller) {
			e->last_attack_target->controller->incoming_damage += get_weapon_damage_in_range(e, e->last_attack_target);
		}
	}

	//bool is_unloading = false;
	for (unit*u : c->u->loaded_units) {
		u->controller->last_process = current_frame;
		if (current_frame >= u->controller->last_pickup + 15) {
			//is_unloading = true;
			if (current_frame - c->last_unload >= 4) {
				c->last_unload = current_frame;
				c->u->game_unit->unload(u->game_unit);
			}
		}
	}

	log(log_level_info, "potential_pickups.size() is %d\n", potential_pickups.size());
	for (unit*u : potential_pickups) {
		log(log_level_info, "%s incoming_damage is %g\n", u->type->name, u->controller->incoming_damage);
	}

	//if (c->u->loaded_units.empty() || is_unloading) {
	if (c->u->loaded_units.empty()) {
		unit*u = get_best_score(potential_pickups, [&](unit*u) {
			if (current_frame - u->controller->last_pickup < 6 && u->controller->pickup_to == c->u) return -1000.0;
			return -u->controller->incoming_damage;
		}, 0.0);
		if (u) {
			u->controller->last_process = current_frame;
			if (current_frame - u->controller->last_pickup >= 4) {
				log(log_level_info, " pickup!\n");
				u->controller->last_pickup = current_frame;
				u->controller->pickup_to = c->u;
				u->game_unit->rightClick(c->u->game_unit);
				return;
			}
		}
	}

	if (c->target) {
		c->last_process = current_frame;

		if (current_frame - c->last_move >= move_resolution) {
			c->go_to = c->target->pos;
			c->last_move = current_frame;
			move(c);
		}
	}

// 	auto has_issued_attack = [&](unit*u) {
// 		auto*c = u->controller;
// 		if (c->action != unit_controller::action_attack && c->action != unit_controller::action_kite) return false;
// 		return c->attack_state && current_frame >= c->attack_timer + 2 && current_frame < c->attack_timer + 15;
// 	};
// 	auto eval = [&](unit*u) {
// 		//if (u->weapon_cooldown < latency_frames * 2 && !has_issued_attack(u)) return false;
// 		log(log_level_info, " eval %p\n", u);
// 		if (!has_issued_attack(u)) return false;
// 		log(log_level_info, " pickup %p\n", u);
// 		//if (u->weapon_cooldown < latency_frames * 2) return false;
// 		if (current_frame - u->controller->last_pickup >= 8) {
// 			u->controller->last_pickup = current_frame;
// 			u->game_unit->rightClick(c->u->game_unit);
// 			log(log_level_info, " load!\n");
// 		}
// 		u->controller->attack_state = 0;
// 		u->controller->last_process = current_frame;
// 		c->last_process = current_frame;
// 		return true;
// 	};
// 
// 	for (unit*u : c->u->loaded_units) {
// 		u->controller->last_process = current_frame;
// 		//if (current_frame >= u->controller->last_pickup + latency_frames + 1 && u->weapon_cooldown < latency_frames) {
// 		//if (u->weapon_cooldown < latency_frames) {
// 		log(log_level_info, "u->controller->last_pickup is %d, u->weapon_cooldown is %d\n", u->controller->last_pickup, u->weapon_cooldown);
// 		if (current_frame >= u->controller->last_pickup + latency_frames + 1 && u->weapon_cooldown < latency_frames) {
// 			if (current_frame - c->last_unload >= 4) {
// 				c->last_unload = current_frame;
// 				c->u->game_unit->unload(u->game_unit);
// 			}
// 		}
// 	}
// 
// 	log(log_level_info, " shuttle, target is %p\n", c->target);
// 
// 	if (c->target && !c->target->is_loaded && eval(c->target)) return;
// 	for (unit*u : my_units) {
// 		if (u->is_flying || u->building) continue;
// 		if (u->type->total_minerals_cost + u->type->total_gas_cost < 125 && u != c->target) continue;
// 		if (u->is_loaded) continue;
// 		double d = diag_distance(c->u->pos - u->pos);
// 		if (d > 64) continue;
// 		if (u->type == unit_types::siege_tank_siege_mode || u->is_morphing) continue;
// 		
// 		if (eval(u)) return;
// 	}
// 
// 	if (c->target) {
// 		c->last_process = current_frame;
// 
// 		if (current_frame - c->last_move >= move_resolution) {
// 			c->go_to = c->target->pos;
// 			c->last_move = current_frame;
// 			move(c);
// 		}
// 	}

}


void issue_gather_order(unit_controller*c) {
	unit*u = c->u;
	auto o = u->game_order;
	if (o == BWAPI::Orders::MiningMinerals || o == BWAPI::Orders::HarvestGas) c->last_return_cargo = current_frame;
	if (u->game_unit->isCarryingMinerals() || u->game_unit->isCarryingGas()) {
		if ((o != BWAPI::Orders::ReturnMinerals && o != BWAPI::Orders::ReturnGas) || current_frame - c->last_return_cargo >= 15 * 8) {
			if (c->depot && !u->is_flying && !square_pathing::unit_can_reach(u, u->pos, c->depot->pos)) {
				c->go_to = c->depot->pos;
				move(c);
			} else {
				c->last_return_cargo = current_frame;
				u->game_unit->returnCargo();
				c->noorder_until = current_frame + 8;
			}
		}
	} else {
		if (c->target && o != BWAPI::Orders::MiningMinerals && o != BWAPI::Orders::HarvestGas) {
			if ((o != (c->target->type->is_gas ? BWAPI::Orders::MoveToGas : BWAPI::Orders::MoveToMinerals) && o != (c->target->type->is_gas ? BWAPI::Orders::WaitForGas : BWAPI::Orders::WaitForMinerals)) || u->game_unit->getOrderTarget() != c->target->game_unit) {
				bool issue_gather = true;
				if (issue_gather) {
					if (!square_pathing::unit_can_reach(u, u->pos, c->target->pos)) {
						c->go_to = c->target->pos;
						move(c);
					} else {
						c->last_gather_target = c->target;
						c->last_gather_issued_frame = current_frame;
						if (!u->game_unit->gather(c->target->game_unit)) {
							c->go_to = c->target->pos;
							move(c);
						}
						c->noorder_until = current_frame + 2;
						if (!c->target->is_being_gathered || current_frame + latency_frames >= c->target->is_being_gathered_begin_frame + 75 + 8) {
							c->noorder_until = current_frame + std::max(8, latency_frames + 1);
						}
					}
				}
			}
		}
	}
}

void worker_micro(const a_vector<unit_controller*>&controllers) {

	log(log_level_info, "worker_micro()\n");

	struct c_data_t {
		unit_controller*c = nullptr;
		bool assigned = false;
		a_unordered_map<unit*, a_deque<grid::build_square*>> paths;
		a_unordered_map<grid::build_square*, unit*> path_squares;
	};

	a_vector<c_data_t> c_data;

	a_unordered_map<grid::build_square*, double> path_score;

	struct closed_node {
		grid::build_square*bs;
		closed_node*prev;
	};

	a_unordered_set<grid::build_square*> visited;
	a_deque<std::pair<grid::build_square*, closed_node*>> open;
	a_list<closed_node> closed;

	for (size_t i = 0; i < controllers.size(); ++i) {
		auto*c = controllers[i];

		c_data.emplace_back();
		auto&cur_data = c_data.back();

		cur_data.c = c;

		closed.clear();

		visited.clear();
		open.emplace_back(&grid::get_build_square(c->u->pos), nullptr);
		visited.insert(&grid::get_build_square(c->u->pos));
		while (!open.empty()) {
			grid::build_square*bs = open.front().first;
			closed_node*prev = open.front().second;
			open.pop_front();

			closed.push_back({ bs, prev });
			closed_node*cn = &closed.back();

			for (int i = 0; i < 4; ++i) {
				auto*n = bs->get_neighbor(i);
				if (!n) continue;
				if (diag_distance(n->pos - c->u->pos) >= 32 * 14) continue;
				if (n->building && n->building->type->is_minerals && n->building->visible) {
					auto i = cur_data.paths.emplace(std::piecewise_construct, std::make_tuple(n->building), std::make_tuple());
					if (i.second) {
						auto&d = i.first->second;
						for (closed_node*nn = cn; nn; nn = nn->prev) {
							d.push_front(nn->bs);
							//if (nn->bs != bs) cur_data.path_squares.emplace(nn->bs, n->building);
							cur_data.path_squares.emplace(nn->bs, n->building);
							double d = diag_distance(nn->bs->pos - c->u->pos);
							path_score[nn->bs] = d*d;

							//game->drawBoxMap(nn->bs->pos.x + 1, nn->bs->pos.y + 1, nn->bs->pos.x + 31, nn->bs->pos.y + 31, BWAPI::Colors::Blue);
						}
					}
				}
				if (!n->entirely_walkable) continue;
				if (n->building) continue;
				if (!visited.insert(n).second) continue;
				open.emplace_back(n, cn);
			}

		}

	}

	a_deque<std::pair<grid::build_square*, double>> sorted_path_score;
	for (auto&v : path_score) sorted_path_score.emplace_back(v.first, v.second);
	std::sort(sorted_path_score.begin(), sorted_path_score.end(), [](auto&a, auto&b) {
		return a.second < b.second;
	});

	a_vector<unit*> nearby_enemies;

	int left = c_data.size();
	while (left && !sorted_path_score.empty()) {

		grid::build_square*bs = sorted_path_score.front().first;
		sorted_path_score.pop_front();

		//game->drawBoxMap(bs->pos.x + 2, bs->pos.y + 2, bs->pos.x + 30, bs->pos.y + 30, BWAPI::Colors::Green);

		//double nearest_enemy_distance = std::numeric_limits<double>::infinity();
		nearby_enemies.clear();
		for (unit*e : enemy_units) {
			double d = diag_distance(bs->pos - e->pos);
			if (d > 32 * 14) continue;
			if (!e->stats->ground_weapon) continue;
			//if (d < nearest_enemy_distance) nearest_enemy_distance = d;
			nearby_enemies.push_back(e);
		}

		a_vector<c_data_t*> allies;

		for (auto&cd : c_data) {
			if (cd.assigned) continue;
			if (cd.path_squares.find(bs) != cd.path_squares.end()) {
				allies.push_back(&cd);
				cd.c->last_process = current_frame;
				cd.assigned = true;
				--left;
			}
		}
		log(log_level_info, "allies.size() is %d, nearby_enemies.size() is %d\n", allies.size(), nearby_enemies.size());
		for (auto*cd : allies) {
			auto*c = cd->c;
			unit*u = c->u;

			//if (current_frame < c->attack_timer && c->attack_timer - current_frame < 60) continue;
			//if (current_frame < c->noorder_until) continue;

			double target_distance = std::numeric_limits<double>::infinity();
			double best_score = std::numeric_limits<double>::infinity();
			unit*target = nullptr;
			int turn_frames = 0;
			for (unit*e : nearby_enemies) {
				weapon_stats*w = e->is_flying ? u->stats->air_weapon : u->stats->ground_weapon;
				if (w) {
					xy upos = u->pos + xy((int)(u->hspeed*latency_frames), (int)(u->vspeed*latency_frames));
					xy epos = e->pos + xy((int)(e->hspeed*latency_frames), (int)(e->vspeed*latency_frames));
					double d = units_distance(upos, u, epos, e);

					double score = d <= w->max_range ? 0.0 : e->shields + e->hp;

					if (score < best_score) {
						best_score = score;
						target = e;
						target_distance = d;

						xy relpos = e->pos - u->pos;
						double rel_angle = std::atan2(relpos.y, relpos.x);

						double da = u->heading - rel_angle;
						if (da < -PI) da += PI * 2;
						if (da > PI) da -= PI * 2;
						turn_frames = (int)(std::abs(da) / c->u->stats->turn_rate);
					}
				}
			}

			auto move_to = [&](unit*move_target) {
				if (u->type->is_worker && move_target->type->is_minerals) {
					u->game_unit->gather(move_target->game_unit);
				} else {
					u->game_unit->follow(move_target->game_unit);
				}
				c->noorder_until = current_frame + 4;
			};
			auto move_to_attack_range = [&]() {
				if (current_frame < c->noorder_until) return;
				log(log_level_info, "move_to_attack_range()\n");
				grid::build_square*cur_bs = &grid::get_build_square(u->pos);
				double nearest_square_distance = std::numeric_limits<double>::infinity();
				unit*move_target = nullptr;
				xy move_dst;
				for (auto&v : cd->path_squares) {
					if (v.first == cur_bs) continue;
					double d = diag_distance(target->pos - (v.first->pos + xy(16, 16)));
					if (d < nearest_square_distance) {
						move_dst = v.first->pos + xy(16, 16);
						nearest_square_distance = d;
						move_target = v.second;
					}
				}
				if (move_target) {

					if (u->type->is_worker) {

						auto o = u->game_order;
						if (o == BWAPI::Orders::MiningMinerals || o == BWAPI::Orders::ReturnMinerals || o == BWAPI::Orders::WaitForMinerals) {
							if (nearest_square_distance < 48) move_to(move_target);
						} else move_to(move_target);
						
					} else {
						u->game_unit->move(BWAPI::Position(move_dst.x, move_dst.y));
						c->noorder_until = current_frame + 4;
						//move_to(move_target);
					}
				}
			};
			auto move_away = [&]() {
				if (current_frame < c->noorder_until) return;
				log(log_level_info, "move_away()\n");
				double best_score = std::numeric_limits<double>::infinity();
				unit*move_target = nullptr;
				xy move_dst;
				for (auto&v : cd->paths) {
					double score = 0.0;
					for (auto*bs : v.second) {
						xy pos = bs->pos + xy(16, 16);
						for (unit*e : nearby_enemies) {
							double d = diag_distance(e->pos - pos);
							if (d < 64) score += (64 - d)*(64 - d);
						}
					}
					if (v.second.size() < 4) score += 64 * 64 * (4 - v.second.size());
					if (score < best_score) {
						best_score = score;
						move_target = v.first;
						move_dst = v.second.empty() ? v.first->pos : v.second.back()->pos + xy(16, 16);
					}
				}
				if (move_target) {
					if (u->type->is_worker) {
						move_to(move_target);
					} else {
						u->game_unit->move(BWAPI::Position(move_dst.x, move_dst.y));
						c->noorder_until = current_frame + 4;
					}
				}
			};

			if (target) {

				weapon_stats*w = target->is_flying ? u->stats->air_weapon : u->stats->ground_weapon;

				log(log_level_info, "worker micro, attack_state %d, attack frame %d, order is %s, target_distance %g\n", c->attack_state, u->is_attacking_frame, u->game_order.getName(), target_distance);

				if (c->attack_state == 0) {

					double nd = target_distance - w->max_range;
					//bool next = turn_frames + std::max(frames_to_reach(u, nd), u->weapon_cooldown) <= latency_frames;
					bool next = nd <= 0 && (turn_frames <= 1 || u->speed == 0.0) && turn_frames + u->weapon_cooldown <= latency_frames;

					log(log_level_info, "nd %g turn_frames %d weapon_cooldown %d (latency %d)\n", nd, turn_frames, u->weapon_cooldown, latency_frames);

					if (next) {
						log(log_level_info, "attack rawr!\n");

						u->game_unit->attack(target->game_unit);
						//log(log_level_info, "attack() for %d\n", current_command_frame);
						c->attack_state = 1;
						c->attack_timer = current_command_frame + turn_frames + 2;

						c->last_command_frame = current_command_frame;

					} else {
						move_to_attack_range();
					}
				} else if (c->attack_state == 1) {
					if (u->is_attacking_frame) c->attack_state = 1;
					if (current_frame >= c->attack_timer + 15) c->attack_state = 0;
				} else if (c->attack_state == 2) {
					if (current_command_frame >= c->attack_timer) {
						int frames = turn_frames + frames_to_reach(u->stats, 0.0, target_distance - w->max_range);
						if (current_frame >= c->attack_timer + latency_frames && frames >= u->weapon_cooldown - latency_frames - 4) {
							move_to_attack_range();
							c->attack_state = 0;
						} else {
							move_away();
						}
					}
				} else c->attack_state = 0;

			}

// 			if (current_frame < c->attack_timer && c->attack_timer - current_frame < 60) {
// 
// 				if (u->type->is_worker) {
// 					if (current_frame >= c->noorder_until) {
// 
// 						unit*target = cd->path_squares[bs];
// 						if (target) {
// 							u->game_unit->gather(target->game_unit);
// 							c->noorder_until = current_command_frame + 4;
// 						}
// 					}
// 				}
// 
// 			}

		}

	}


}

void protect_marines_micro(const a_vector<unit_controller*>& controllers) {
	if (controllers.empty()) return;

	int worker_count = 0;
	xy worker_pos_sum;
	for (auto* c : controllers) {
		if (c->u->type->is_worker) {
			++worker_count;
			worker_pos_sum += c->u->pos;
		}
	}
	xy worker_pos_avg = worker_count ? worker_pos_sum / worker_count : controllers[0]->u->pos;

	for (auto* c : controllers) {
		
		auto gather = [&]() {
			if (current_frame < c->noorder_until) return;
			auto o = c->u->game_order;
			if (o == BWAPI::Orders::MiningMinerals || o == BWAPI::Orders::HarvestGas) return;
			if (o == BWAPI::Orders::MoveToGas || o == BWAPI::Orders::MoveToMinerals) return;
			if (o == BWAPI::Orders::WaitForGas || o == BWAPI::Orders::WaitForMinerals) return;
			c->target = get_best_score(resource_units, [&](unit* r) {
				if (!r->type->is_minerals) return std::numeric_limits<double>::infinity();
				return diag_distance(c->u->pos - r->pos);
			}, std::numeric_limits<double>::infinity());
			issue_gather_order(c);
		};

		unit* nearest_marine = get_best_score(my_completed_units_of_type[unit_types::marine], [&](unit* u) {
			double d = diag_distance(c->u->pos - u->pos);
			if (d >= 32 * 15) return std::numeric_limits<double>::infinity();
			return d;
		});
		unit* target = get_best_score(enemy_units, [&](unit* u) {
			weapon_stats* w = u->is_flying ? c->u->stats->air_weapon : c->u->stats->ground_weapon;
			if (!w) return std::numeric_limits<double>::infinity();
			double d = diag_distance(c->u->pos - u->pos);
			if (d >= 32 * 15) return std::numeric_limits<double>::infinity();
			return d;
		}, std::numeric_limits<double>::infinity());
		if (!target) {
			if (c->u->type->is_worker) gather();
			continue;
		}
		weapon_stats* w = target->is_flying ? c->u->stats->air_weapon : c->u->stats->ground_weapon;
		if (!w) {
			if (c->u->type->is_worker) gather();
			continue;
		}
		
		if (true) {
			
			double d = units_distance(c->u, target);
			if (d <= 20) {
				if (c->u->game_order != BWAPI::Orders::AttackUnit) {
					c->u->game_unit->attack(target->game_unit);
					c->noorder_until = current_frame + latency_frames + 5;
				}
				continue;
			}
			
			if (c->u->type == unit_types::scv && current_minerals >= 10) {
				bool repairing = false;
				for (unit* u : my_units) {
					if (!u->type->is_mechanical) continue;
					if (u == c->u) continue;
					if (units_distance(c->u, u) <= 5 && u->hp < u->stats->hp) {
						if (c->u->game_order != BWAPI::Orders::Repair || c->u->order_target != u) {
							c->u->game_unit->repair(u->game_unit);
							c->noorder_until = current_frame + 4;
						}
						c->last_process = current_frame;
						repairing = true;
						break;
					}
				}
				if (repairing) continue;
			}
			
			auto* nearest = get_best_score(controllers, [&](auto* c) {
				return diag_distance(c->u->pos - target->pos);
			});
			if (nearest && units_distance(nearest->u, target) <= 32) {
				if (c->u->game_order != BWAPI::Orders::AttackUnit) {
					c->u->game_unit->attack(target->game_unit);
					c->noorder_until = current_frame + latency_frames + 5;
				}
				continue;
			}
			
			continue;
		}
		if (!nearest_marine) {
			if (c->u->type->is_worker) gather();
			continue;
		}

		double marine_distance = units_distance(c->u, nearest_marine);
		double target_distance = units_distance(c->u, target);

		xy move_marine_to = worker_pos_avg;

		if (c->u->type == unit_types::marine) {
			unit* nearest_bunker = get_best_score(my_completed_units_of_type[unit_types::bunker], [&](unit* u) {
				return units_distance(c->u, u);
			});
			if (nearest_bunker && units_distance(c->u, nearest_bunker) <= 32 * 2 && units_distance(nearest_bunker, target) <= w->max_range + 32) {
				if (!c->u->loaded_into) {
					c->u->game_unit->rightClick(nearest_bunker->game_unit);
					c->noorder_until = current_frame + 8;
				}
				continue;
			}
// 			if (nearest_bunker && units_distance(c->u, nearest_bunker) <= 32 * 8) {
// 				move_marine_to = nearest_bunker->pos;
// 			}
		}
		if (c->u->loaded_into) {
			if (current_frame >= c->no_unload_until) {
				c->u->loaded_into->game_unit->unload(c->u->game_unit);
			}
		}

		auto move = [&]() {
			if (current_frame < c->noorder_until) return;
			if (c->u->type->is_worker) {
				if (!c->u->game_unit->follow(nearest_marine->game_unit)) {
					c->u->game_unit->move(BWAPI::Position(nearest_marine->pos.x, nearest_marine->pos.y));
					//printf("protect marine move!\n");
				}// else printf("protect marine follow!\n");
				c->noorder_until = current_frame + 4;
			} else {
				c->u->game_unit->move(BWAPI::Position(move_marine_to.x, move_marine_to.y));
				//printf("protect marine move to workers!\n");
			}
		};

		if (target_distance >= marine_distance + 32 * 3) {
			if (c->u->type->is_worker) {
				gather();
			} else move();
		} else {
			if (target_distance <= w->max_range) {
				//printf("protect marine attack!\n");

				if (c->u->game_order != BWAPI::Orders::AttackUnit) {
					c->u->game_unit->attack(target->game_unit);
					c->noorder_until = current_frame + latency_frames + 5;
				}
			} else move();
		}

// 		if (marine_distance < 32 * 2) {
// 
// 			//if (target_distance >= 32 * 3 || target_distance <= w->max_range) {
// 			if (target_distance >= 32 * 3 || target_distance <= w->max_range) {
// 
// 				printf("protect marine attack!\n");
// 				
// 				c->u->game_unit->attack(target->game_unit);
// 				c->noorder_until = current_frame + 15;
// 
// 			} else {
// 				move();
// 			}
// 
// 		} else {
// 			move();
// 		}

	}

}


bool worker_mineral_walk_fight(unit_controller* c) {
	
	unit* target = c->target;
	unit* ntarget = get_best_score(visible_enemy_units, [&](unit* e) {
		auto* w = e->is_flying ? c->u->stats->air_weapon : c->u->stats->ground_weapon;
		if (!w) return std::numeric_limits<double>::infinity();
		return units_distance(c->u, e) + e->hp / 100.0;
	}, std::numeric_limits<double>::infinity());
	if (ntarget) target = target;
	double wr = 0.0;
	auto* w = target ? target->is_flying ? c->u->stats->air_weapon : c->u->stats->ground_weapon : nullptr;
	if (w) wr = w->max_range;
	
	if (!target || !target->visible || target->dead) {
		return false;
	}
	
	bool is_lowest_hp = true;
	int n_nearby_workers = 0;
	for (unit* u : my_workers) {
		if (diag_distance(u->pos - c->u->pos) <= 32 * 4) {
			++n_nearby_workers;
			if (u->hp < c->u->hp) is_lowest_hp = false;
		}
	}
	
	double d = units_distance(c->u, target);
	render::draw_stacked_text(c->u->pos, format("d %g", d));
	//if (d <= 0 || c->u->weapon_cooldown <= latency_frames) {
	if (d > wr) {
		if (c->u->type == unit_types::scv) {
			unit* repair_target = get_best_score(my_workers, [&](unit* u) {
				if (u == c->u) return std::numeric_limits<double>::infinity();
				double d = std::max(units_distance(c->u, u) - 5, 0.0);
				if (d > 64) return std::numeric_limits<double>::infinity();
				if (u->hp >= u->stats->hp) return std::numeric_limits<double>::infinity();
				return d + u->hp / 100.0;
			}, std::numeric_limits<double>::infinity());
			if (repair_target) {
				game->drawLineMap(c->u->pos.x, c->u->pos.y, repair_target->pos.x, repair_target->pos.y, BWAPI::Colors::White);
				render::draw_stacked_text(c->u->pos, format("repair?"));
				if (current_frame >= c->noorder_until) {
					render::draw_stacked_text(c->u->pos, format(" !! REPAIR !!"));
					if (c->u->game_order != BWAPI::Orders::Repair || c->u->order_target != repair_target) {
						c->u->game_unit->repair(repair_target->game_unit);
						c->noorder_until = current_frame + std::max(latency_frames, 15);
					}
				}
				return true;
			}
		}
	}
	//if (c->u->weapon_cooldown <= latency_frames) {
	if (!is_lowest_hp || n_nearby_workers < 3) {
		if (d <= 0 && (c->u->game_order == BWAPI::Orders::MoveToMinerals || c->u->game_order == BWAPI::Orders::MoveToGas)) {
			render::draw_stacked_text(c->u->pos, format("stop?"));
			if (current_frame >= c->noorder_until) {
				render::draw_stacked_text(c->u->pos, format(" !! STOP !!"));
				c->u->game_unit->stop(target->game_unit);
				c->noorder_until = current_frame + 15;
			}
		} else {
			game->drawLineMap(c->u->pos.x, c->u->pos.y, target->pos.x, target->pos.y, BWAPI::Colors::White);
			render::draw_stacked_text(c->u->pos, format("attack?"));
			if (current_frame >= c->noorder_until) {
				render::draw_stacked_text(c->u->pos, format(" !! ATTACK !!"));
				if (c->u->game_order != BWAPI::Orders::AttackUnit || c->u->order_target != target) {
					c->u->game_unit->attack(target->game_unit);
					c->noorder_until = current_frame + latency_frames;
				}
			}
		}
		return true;
	}
	
	a_unordered_map<unit*, a_deque<grid::build_square*>> paths;
	a_unordered_map<grid::build_square*, unit*> path_squares;

	a_unordered_map<grid::build_square*, double> path_score;

	struct closed_node {
		grid::build_square*bs;
		closed_node*prev;
	};

	a_unordered_set<grid::build_square*> visited;
	a_deque<std::pair<grid::build_square*, closed_node*>> open;
	a_list<closed_node> closed;

	open.emplace_back(&grid::get_build_square(c->u->pos), nullptr);
	visited.insert(&grid::get_build_square(c->u->pos));
	while (!open.empty()) {
		grid::build_square*bs = open.front().first;
		closed_node*prev = open.front().second;
		open.pop_front();

		closed.push_back({ bs, prev });
		closed_node*cn = &closed.back();

		for (int i = 0; i < 4; ++i) {
			auto*n = bs->get_neighbor(i);
			if (!n) continue;
			if (diag_distance(n->pos - c->u->pos) >= 32 * 14) continue;
			if (n->building && (n->building->type->is_minerals || n->building->type->is_gas) && n->building->visible) {
				auto i = paths.emplace(std::piecewise_construct, std::make_tuple(n->building), std::make_tuple());
				if (i.second) {
					auto&d = i.first->second;
					for (closed_node*nn = cn; nn; nn = nn->prev) {
						d.push_front(nn->bs);
						//if (nn->bs != bs) path_squares.emplace(nn->bs, n->building);
						path_squares.emplace(nn->bs, n->building);
						double d = diag_distance(nn->bs->pos - c->u->pos);
						path_score[nn->bs] = d*d;

						game->drawBoxMap(nn->bs->pos.x + 1, nn->bs->pos.y + 1, nn->bs->pos.x + 31, nn->bs->pos.y + 31, BWAPI::Colors::Blue);
						
						if (diag_distance(nn->bs->pos + xy(16, 16) - target->pos) > 32 * 3) {
							game->drawLineMap(c->u->pos.x, c->u->pos.y, n->building->pos.x, n->building->pos.y, BWAPI::Colors::White);
							render::draw_stacked_text(c->u->pos, format("gather?"));
							c->last_mineral_walk = current_frame;
							if (current_frame >= c->noorder_until) {
								render::draw_stacked_text(c->u->pos, format(" !! GATHER !! "));
								c->u->game_unit->gather(n->building->game_unit);
								c->noorder_until = current_frame + 4;
							}
							return true;
						}
					}
				}
			}
			if (!n->entirely_walkable) continue;
			if (n->building) continue;
			if (!visited.insert(n).second) continue;
			open.emplace_back(n, cn);
		}
	}
	
	
	return false;
}

a_unordered_map<unit*, double> kill_spider_mine_damage;

void process(const a_vector<unit_controller*>&controllers) {

	for (auto*c : controllers) {
		bool can_move = c->u->game_unit->canIssueCommand(BWAPI::UnitCommand::move(c->u->game_unit,c->u->game_unit->getPosition()));
		if (c->u->type->game_unit_type==BWAPI::UnitTypes::Zerg_Larva) can_move = false;
		if (c->u->type == unit_types::spider_mine) can_move = false;
		c->can_move = can_move;
	}
	
	a_vector<unit*> enemy_spider_mines;
	for (unit* u : visible_enemy_units) {
		if (u->type == unit_types::spider_mine) enemy_spider_mines.push_back(u);
	}

	a_unordered_map<unit*, xy> re_eval_move_to;

	auto re_evaluate_target = [&](unit_controller*c, bool allow_actions) {
		//return false;
		if (c->action != unit_controller::action_attack || !c->target) return false;
		unit*u = c->u;
		if (u->is_flying) return false;
		weapon_stats*w = c->target->is_flying ? u->stats->air_weapon : u->stats->ground_weapon;
		if (!w) return false;

		auto&re_eval_move_to_bug_workaround = re_eval_move_to;

		if (current_frame - c->last_re_evaluate_target <= 4) {
			if (c->re_evaluate_target && !c->re_evaluate_target->dead) c->target = c->re_evaluate_target;
			if (allow_actions && c->re_evalute_move_to != xy()) {
				if (current_frame >= c->noorder_until) {
					c->last_move = current_frame;
					if (c->last_move_to_pos != c->re_evalute_move_to || current_frame >= c->last_move_to + 30) {
						c->last_move_to = current_frame;
						c->last_move_to_pos = c->re_evalute_move_to;
						c->u->game_unit->move(BWAPI::Position(c->re_evalute_move_to.x, c->re_evalute_move_to.y));
						c->noorder_until = current_frame + rng(8);
					}
				}
				return true;
			}
			return false;
		}

		c->last_re_evaluate_target = current_frame;
		c->re_evaluate_target = nullptr;
		c->re_evalute_move_to = xy();

		a_vector<std::tuple<unit*, xy>> path_units;
		for (unit*nu : visible_units) {
			if (nu->is_flying || nu->burrowed) continue;
			if (diag_distance(nu->pos - u->pos) >= 32 * 4) continue;
			if (nu == u) continue;
			xy pos = nu->pos;
			auto i = re_eval_move_to.find(nu);
			if (i != re_eval_move_to.end()) {
				pos = i->second;
			}
			path_units.emplace_back(nu, pos);
		}

		auto&dims = u->type->dimensions;

		auto can_move_to = [&](xy pos) {
			auto test = [&](xy pos) {
				int right = pos.x + dims[0];
				int bottom = pos.y + dims[1];
				int left = pos.x - dims[2];
				int top = pos.y - dims[3];
				for (auto&v : path_units) {
					unit*u;
					xy pos;
					std::tie(u, pos) = v;
					if (u->is_flying || u->burrowed) continue;
					int uright = pos.x + u->type->dimension_right();
					int ubottom = pos.y + u->type->dimension_down();
					int uleft = pos.x - u->type->dimension_left();
					int utop = pos.y - u->type->dimension_up();
					if (right >= uleft && bottom >= utop && uright >= left && ubottom >= top) return false;
				}
				return true;
			};
			pos.x &= -8;
			pos.y &= -8;
			if (test(pos)) return true;
			if (test(pos + xy(7, 0))) return true;
			if (test(pos + xy(0, 7))) return true;
			if (test(pos + xy(7, 7))) return true;
			for (int x = 1; x < 7; ++x) {
				if (test(pos + xy(x, 0))) return true;
				if (test(pos + xy(x, 7))) return true;
			}
			for (int y = 1; y < 7; ++y) {
				if (test(pos + xy(0, y))) return true;
				if (test(pos + xy(7, y))) return true;
			}
			return false;
		};

		struct node_data_t {
			double len = 0.0;
		};
		double path_len = 0.0;
		a_deque<xy> path = square_pathing::find_square_path<node_data_t>(square_pathing::get_pathing_map(u->type), u->pos, [&](xy pos, xy npos, node_data_t&n) {
			if (diag_distance(pos - u->pos) >= 32 * 3) return false;
			if (!can_move_to(npos)) return false;
			n.len += diag_distance(npos - pos);
			return true;
		}, [&](xy pos, xy npos, const node_data_t&n) {
			return 0.0;
		}, [&](xy pos, const node_data_t&n) {
			bool r = units_distance(pos, u->type, c->target->pos, c->target->type) <= w->max_range;
			if (r) path_len = n.len;
			return r;
		});
		log("%s -> %s target path: %d (len %g)\n", u->type->name, c->target->type->name, path.size(), path_len);
		//bool find_lowest_hp_target = c->u->type == unit_types::zergling && c->target->type == unit_types::zealot;
		bool find_lowest_hp_target = u->type == unit_types::zergling;
		bool find_most_expensive_target = true;
		
		if (c->target->type == unit_types::zergling || c->target->type == unit_types::marine || c->target->type->gas_cost >= 75.0) {
			find_lowest_hp_target = false;
			find_most_expensive_target = false;
		}
		//bool find_most_expensive_target = false;
		//bool find_lowest_hp_target = false;
		//if (path.empty() || path_len >= w->max_range || c->target->dead || !c->target->visible || find_lowest_hp_target) {
		bool any_sieged_tanks = false;
		a_vector<unit*> target_units;
		for (unit*nu : visible_enemy_units) {
			if (nu->stasis_timer || nu->type->is_non_usable) continue;
			if (nu->type != c->target->type && nu->building && !nu->stats->ground_weapon && !nu->stats->air_weapon && nu->type != unit_types::bunker) continue;
			//if (nu->type != c->target->type || nu->is_flying != c->target->is_flying) continue;
			if (diag_distance(nu->pos - u->pos) >= 32 * 4 + w->max_range) continue;
			auto* w = nu->is_flying ? u->stats->air_weapon : u->stats->ground_weapon;
			if (!w) continue;
			if (w->max_range > 32 && nu->game_unit->isUnderDarkSwarm()) continue;
			if (nu->type == unit_types::siege_tank_siege_mode) any_sieged_tanks = true;
			target_units.push_back(nu);
		}
		//if (c->target->dead || !c->target->visible || find_lowest_hp_target || any_sieged_tanks) {
		if (c->target->dead || !c->target->visible || find_lowest_hp_target) {
			//if (find_lowest_hp_target && target_units.size() > 2) find_lowest_hp_target = false;
			log("%d potential targets\n", target_units.size());
			if (!target_units.empty()) {
				struct node_data_t {
					double len = 0.0;
				};
				double path_len = 0.0;
				unit*target = nullptr;
				double lowest_hp = std::numeric_limits<double>::infinity();
				double highest_cost = 0.0;
				xy target_pos;
				a_deque<xy> path = square_pathing::find_square_path<node_data_t>(square_pathing::get_pathing_map(u->type), u->pos, [&](xy pos, xy npos, node_data_t&n) {
					if (diag_distance(pos - u->pos) >= 32 * 3) return false;
					if (!can_move_to(npos)) return false;
					n.len += diag_distance(npos - pos);
					return true;
				}, [&](xy pos, xy npos, const node_data_t&n) {
					return 0.0;
				}, [&](xy pos, const node_data_t&n) {
					for (unit*e : target_units) {
						bool r = units_distance(pos, u->type, e->pos, e->type) <= w->max_range;
						if (!r) return false;
						if (find_lowest_hp_target) {
							double hp = e->shields + e->hp;
							if (hp < lowest_hp) {
								lowest_hp = hp;
								target = e;
								path_len = n.len;
								target_pos = pos;
							}
						} else if (find_most_expensive_target) {
							double cost = e->type->total_minerals_cost + e->type->total_gas_cost;
							if (e->type->is_building) cost /= 3;
							if (e->type->is_worker) cost *= 4;
							if (e->type == unit_types::spider_mine) cost = 10000.0;
							if (cost > highest_cost) {
								highest_cost = cost;
								target = e;
								path_len = n.len;
								target_pos = pos;
							}
						} else {
							if (r) {
								target = e;
								path_len = n.len;
								target_pos = pos;
								return true;
							}
						}
					}
					return false;
				});
				if (target) {
					log("%s -> %s new target path: %d (len %g)\n", u->type->name, target->type->name, path.size(), path_len);
					c->target = target;
					c->re_evaluate_target = target;
					re_eval_move_to_bug_workaround[c->u] = target_pos;
				} else {
					log("no better target\n");
				}
			}

		}

		if (allow_actions) {
			if (!c->target->is_flying) {
				weapon_stats*w = c->target->is_flying ? u->stats->air_weapon : u->stats->ground_weapon;
				if (w && w->max_range <= 32 && c->target->speed > 2.0) {
					bool chase = true;
					if (units_distance(u, c->target) <= w->max_range + 16) {
						chase = false;
					}
					if (chase) {
						double target_heading = std::atan2(c->target->vspeed, c->target->hspeed);
						xy relpos = c->target->pos - u->pos;
						double rel_angle = std::atan2(relpos.y, relpos.x);
						double da = target_heading - rel_angle;
						if (da < -PI) da += PI * 2;
						if (da > PI) da -= PI * 2;
						if (std::abs(da) <= PI / 2) {

							double offset_heading;
							if (da >= 0) offset_heading = PI / 2;
							else offset_heading = PI / 2;

							xy target_pos = c->target->pos;
							double max_distance = 32 * 4;
							for (double distance = 0; distance < max_distance; distance += 32) {
								xy pos = c->target->pos;
								pos.x += (int)(std::cos(target_heading)*distance);
								pos.y += (int)(std::sin(target_heading)*distance);
								pos.x += (int)(std::cos(offset_heading) * 32 * 1);
								pos.y += (int)(std::sin(offset_heading) * 32 * 1);
								pos = square_pathing::get_pos_in_square(pos, c->target->type);
								if (pos == xy()) break;
								target_pos = pos;
							}
							log("chase, target_pos is %g away\n", (target_pos - c->target->pos).length());

							if (diag_distance(target_pos - c->target->pos) > 32) {

								c->re_evalute_move_to = target_pos;
								if (current_frame >= c->noorder_until) {
									c->last_move = current_frame;
									if (c->last_move_to_pos != c->re_evalute_move_to || current_frame >= c->last_move_to + 30) {
										c->last_move_to = current_frame;
										c->last_move_to_pos = c->re_evalute_move_to;
										c->u->game_unit->move(BWAPI::Position(c->re_evalute_move_to.x, c->re_evalute_move_to.y));
										c->noorder_until = current_frame + rng(8);
									}
								}

								return true;
							}
						}
					}
				}
			}
		}

		return false;
	};
	
	for (unit* u : visible_enemy_units) {
		if (current_frame - u->last_attacked < 30 && u->last_attack_target && u->last_attack_target->controller) {
			if (!u->is_flying && u->stats->ground_weapon && u->stats->ground_weapon->max_range <= 32) {
				unit_controller* c = u->last_attack_target->controller;
				if (c->action == unit_controller::action_gather) {
					c->action = unit_controller::action_attack;
					c->target = u;
				}
			}
		}
	}

	for (auto* c : controllers) {
		if (c->u->type != unit_types::shuttle) continue;
		if (c->action != unit_controller::action_attack) continue;
		shuttle_micro(c);
	}
	
	if (current_minerals >= 10) {
		for (auto* c : controllers) {
			if (c->u->type != unit_types::scv) continue;
			if (c->action == unit_controller::action_attack) {
				if (units_distance(c->u, c->target) <= 20) continue;
			} else if (c->action != unit_controller::action_gather) continue;
			bool is_gathering = c->action == unit_controller::action_gather;
			for (unit* u : my_units) {
				if (!u->type->is_mechanical) continue;
				if (is_gathering && u->controller && u->controller->action == unit_controller::action_gather) continue;
				if (u == c->u) continue;
				if (units_distance(c->u, u) <= 5 && u->hp < u->stats->hp) {
					if (c->u->game_order != BWAPI::Orders::Repair || c->u->order_target != u) {
						c->u->game_unit->repair(u->game_unit);
						c->noorder_until = current_frame + 4;
					}
					c->last_process = current_frame;
					break;
				}
			}
		}
	}

	a_vector<unit_controller*> worker_micro_units;
	for (auto* c : controllers) {
		if (c->last_process == current_frame) continue;
		if (c->action == unit_controller::action_worker_micro) worker_micro_units.push_back(c);
	}
	if (!worker_micro_units.empty()) worker_micro(worker_micro_units);

	a_vector<unit_controller*> protect_marines_micro_units;
	for (auto* c : controllers) {
		if (c->last_process == current_frame) continue;
		if (c->action == unit_controller::action_protect_marines_micro) protect_marines_micro_units.push_back(c);
	}
	if (!protect_marines_micro_units.empty()) protect_marines_micro(protect_marines_micro_units);
	
	kill_spider_mine_damage.clear();
	
	for (auto* c : controllers) {
		if (!c->u->is_flying && c->u->stats->ground_weapon) {
			double wr = c->u->stats->ground_weapon->max_range;
			if (c->action != unit_controller::action_attack || !c->target || c->target->is_flying || units_distance(c->u, c->target) > wr) {
				for (unit* u : enemy_spider_mines) {
					double hp = u->shields + u->hp - kill_spider_mine_damage[u];
					if (hp > 0 && units_distance(c->u, u) <= wr) {
						kill_spider_mine_damage[u] += get_weapon_damage_in_range(c->u, u);
						c->action = unit_controller::action_attack;
						c->target = u;
						break;
					}
				}
			}
		}
	}
	
	for (auto* c : controllers) {
		if ((c->action == unit_controller::action_attack || c->action == unit_controller::action_kite) && c->target) {
			if (diag_distance(c->u->pos - c->target->pos) <= 32) {
				if (!c->target->visible) c->target->gone = true;
				else if (!c->target->game_unit->exists()) c->target->visible = false;
			}
		}
	}

	for (auto* c : controllers) {
		if (c->action == unit_controller::action_idle) continue;
		if (c->last_process == current_frame) continue;
		//if (c->u->type == unit_types::lurker) continue;
		if (!c->u->burrowed) continue;
		c->last_process = current_frame;
		int t = 20 + 37;
		if (c->u->type == unit_types::lurker) {
			t = 15 * 15;
			if (current_frame - c->u->last_attacked <= 90) continue;
		}
		if (current_frame - c->last_burrow >= t) {
			c->last_burrow = current_frame;
			c->u->game_unit->unburrow();
		}
	}
	for (auto* c : controllers) {
		if (c->defensive_concave_position == xy()) continue;
		//log("%s: c->defensive_concave_position is %d %d (%g away)\n", c->u->type->name, c->defensive_concave_position.x, c->defensive_concave_position.y, (c->defensive_concave_position - c->u->pos).length());
		if (c->action != unit_controller::action_attack || !c->target) continue;
		if (c->last_process == current_frame) continue;
		if (c->last_command_frame == current_command_frame) continue;
		unit*u = c->u;
		weapon_stats*w = c->target->is_flying ? u->stats->air_weapon : u->stats->ground_weapon;
		if (!w) continue;
		if (units_distance(u, c->target) < w->max_range && (c->defensive_concave_position - u->pos).length() <= 48) {
			//log("in range, attacking\n");
			continue;
		}
		if (u->is_flying) continue;

		re_evaluate_target(c, false);
// 		if (c->target->shields + c->target->hp < c->target->stats->shields + c->target->stats->hp) {
// 			if (units_distance(c->defensive_concave_position, u->type, c->target->pos, c->target->type) <= w->max_range + 32) {
// 				log("hurt and close, attacking\n");
// 				continue;
// 			}
// 		}
		if (units_distance(c->defensive_concave_position, u->type, c->target->pos, c->target->type) <= w->max_range + 32) {
			//log("close, attacking\n");
			continue;
		}
		if (current_frame - c->target->last_attacked <= 15 * 2) {
			//log("attacking, attacking!\n");
			continue;
		}
		if (c->target->type->is_worker || current_used_total_supply >= 50) continue;
		c->last_process = current_frame;

		//log("wait\n");
		if (current_frame >= c->noorder_until) {
			if ((c->defensive_concave_position - u->pos).length() <= 8) {
				if (u->game_order != BWAPI::Orders::HoldPosition) {
					u->game_unit->holdPosition();
					c->noorder_until = current_frame + rng(8);
				}
			} else {
				c->last_move = current_frame;
				if (c->last_move_to_pos != c->defensive_concave_position || current_frame >= c->last_move_to + 30) {
					c->last_move_to = current_frame;
					c->last_move_to_pos = c->defensive_concave_position;
					c->u->game_unit->move(BWAPI::Position(c->defensive_concave_position.x, c->defensive_concave_position.y));
					c->noorder_until = current_frame + rng(8);
				}
			}
		}

	}

	for (size_t i = 0; i < controllers.size(); ++i) {
		auto*c = controllers[i];

// 		if (c->u->type == unit_types::dragoon && c->u->order_target) log(log_level_info, "distance %g\n", units_distance(c->u, c->u->order_target));
		if (c->u->weapon_cooldown == 0) ++c->weapon_ready_frames;
		else {
			if (c->u->weapon_cooldown > c->u->prev_weapon_cooldown) {
				log(log_level_info, "%s attacked %s at distance %g, weapon was ready for %d frames\n", c->u->type->name, c->u->order_target ? c->u->order_target->type->name : "null", c->u->order_target ? units_distance(c->u, c->u->order_target) : 0.0, c->weapon_ready_frames);
				c->weapon_ready_frames = 0;
			}
		}

		if (c->last_process == current_frame) continue;
		if (c->last_command_frame == current_command_frame) continue;
		c->last_process = current_frame;

		unit*u = c->u;
		if (u->building && c->action != unit_controller::action_building_movement) continue;

		if (u->type == unit_types::reaver) {
			if (u->remaining_train_time <= latency_frames) {
				u->game_unit->train(BWAPI::UnitTypes::Protoss_Scarab);
			}
		}
		if (u->type == unit_types::carrier) {
			if (u->remaining_train_time <= latency_frames) {
				u->game_unit->train(BWAPI::UnitTypes::Protoss_Interceptor);
			}
		}
		
//		if (c->action == unit_controller::action_attack && c->u->type->is_worker) {
//			if (!worker_mineral_walk_fight(c)) continue;
//		}

		if (u->game_order == BWAPI::Orders::Move && u->speed == 0.0) {
			++c->not_moving_counter;
			if (c->not_moving_counter == 60) {
				u->game_unit->stop();
				continue;
			} else if (c->not_moving_counter == 75) {
				u->game_unit->move(BWAPI::Position(u->pos.x - 32 + rng(64 + 1), u->pos.y - 32 + rng(64 + 1)));
				c->not_moving_counter = 0;
				continue;
			}
		} else if (c->not_moving_counter) c->not_moving_counter = 0;

		if ((c->action == unit_controller::action_attack || c->action == unit_controller::action_kite) && c->target) {
			if (!u->is_flying && !square_pathing::unit_can_reach(u, u->pos, c->target->pos, square_pathing::pathing_map_index::include_liftable_wall)) {
				weapon_stats*w = c->target->is_flying ? u->stats->air_weapon : u->stats->ground_weapon;
				if (w) {
					double best_dist;
					xy best_pos;
					auto path = square_pathing::find_square_path(square_pathing::get_pathing_map(u->type, square_pathing::pathing_map_index::include_liftable_wall), u->pos, [&](xy pos, xy npos) {
						double d = diag_distance(npos - c->target->pos);
						if (best_pos == xy() || d < best_dist) {
							best_dist = d;
							best_pos = pos;
						}
						return d <= 32 * 5;
					}, [&](xy pos, xy npos) {
						return diag_distance(npos - c->target->pos);
					}, [&](xy pos) {
						return units_distance(pos, u->type, c->target->pos, c->target->type) <= w->max_range;
					});
					if (best_pos != xy() && diag_distance(u->pos - best_pos) > 8) {
						c->go_to = best_pos;
						move(c);
						continue;
					}
				}
			}
		}

		if (c->action == unit_controller::action_attack || c->action == unit_controller::action_kite) {
			if ((u->type == unit_types::marine || u->type == unit_types::firebat) && u->owner->has_upgrade(upgrade_types::stim_packs)) {
				weapon_stats*w = c->target->is_flying ? u->stats->air_weapon : u->stats->ground_weapon;
				weapon_stats*ew = u->is_flying ? c->target->stats->air_weapon : c->target->stats->ground_weapon;
				if (w) {
					double d = units_distance(u, c->target);
					if (d <= w->max_range + 32 || (ew && d <= ew->max_range + 32)) {
						if (u->stim_timer <= latency_frames && current_frame >= c->nospecial_until) {
							u->game_unit->useTech(upgrade_types::stim_packs->game_tech_type);
							c->nospecial_until = current_frame + latency_frames + 4;
						}
					}
				}
			}
		}

		bool should_intercept = false;
		xy intercept_pos;
		if (c->action == unit_controller::action_attack && c->target && !c->target->dead && latency_frames < 10) {
			bool okay = true;
			okay &= c->target->type != unit_types::interceptor && u->type != unit_types::siege_tank_siege_mode;
			//if (c->target->stats->max_speed >= u->stats->max_speed * 0.875) okay = false;
			if (okay) {
				weapon_stats*w = c->target->is_flying ? u->stats->air_weapon : u->stats->ground_weapon;
				xy upos = u->pos + xy((int)(u->hspeed*latency_frames), (int)(u->vspeed*latency_frames));
				xy tpos = c->target->pos + xy((int)(c->target->hspeed*latency_frames), (int)(c->target->vspeed*latency_frames));
				double d = units_distance(upos, u, tpos, c->target);
				if (w && (d > w->max_range || (d > w->max_range*0.75 && c->u->weapon_cooldown > latency_frames)) && c->target->speed > 4 && d < 32 * 20 && d >= 64) {
					double target_heading = std::atan2(c->target->vspeed, c->target->hspeed);
					xy relpos = c->target->pos - u->pos;
					double rel_angle = std::atan2(relpos.y, relpos.x);
					double da = target_heading - rel_angle;
					if (da < -PI) da += PI * 2;
					if (da > PI) da -= PI * 2;
					if (std::abs(da) <= PI / 2) {
						log("target is running away, intercepting...\n");
						xy target_pos = c->target->pos;
						target_pos.x += (int)(c->target->hspeed*latency_frames);
						target_pos.y += (int)(c->target->vspeed*latency_frames);
						double max_distance = diag_distance(target_pos - u->pos);
						for (double distance = 0; distance < max_distance; distance += 32) {
							xy pos = target_pos;
							pos.x += (int)(std::cos(target_heading)*distance);
							pos.y += (int)(std::sin(target_heading)*distance);
							pos = square_pathing::get_pos_in_square(pos, c->target->type);
							if (pos == xy()) break;
							target_pos = pos;
						}
						should_intercept = true;
						intercept_pos = target_pos;
					}
				}
			}
		}

		if ((c->action == unit_controller::action_attack || c->action == unit_controller::action_kite) && c->target && !c->target->dead && c->target->visible && (!c->target->cloaked || c->target->detected) && c->u->type != unit_types::lurker) {

			bool use_patrol = false;

			weapon_stats*w = c->target->is_flying ? u->stats->air_weapon : u->stats->ground_weapon;
			weapon_stats*ew = u->is_flying ? c->target->stats->air_weapon : c->target->stats->ground_weapon;
			//if (w && ew) {
			if (w) {
				if (u->type == unit_types::vulture || u->type == unit_types::valkyrie) {
					if (ew && w->max_range >= ew->max_range) use_patrol = true;
					if (ew && w->max_range*1.5 >= ew->max_range && w->cooldown > ew->cooldown) use_patrol = true;
				}
				if (true) {
					unit*ne = get_best_score(enemy_units, [&](unit*e) {
						if (!e->visible) return std::numeric_limits<double>::infinity();
						return diag_distance(e->pos - u->pos);
					});
					if (ne && ne->type != c->target->type) {
						use_patrol = false;
					}
				}

				if (u->type == unit_types::vulture) {
					double nearest_sieged_tank_distance = get_best_score_value(my_completed_units_of_type[unit_types::siege_tank_siege_mode], [&](unit*u) {
						return diag_distance(u->pos - c->u->pos);
					});
					if (nearest_sieged_tank_distance == 0) nearest_sieged_tank_distance = std::numeric_limits<double>::infinity();
					double nearest_unsieged_tank_distance = get_best_score_value(my_completed_units_of_type[unit_types::siege_tank_tank_mode], [&](unit*u) {
						return diag_distance(u->pos - c->u->pos);
					});
					if (nearest_unsieged_tank_distance == 0) nearest_unsieged_tank_distance = std::numeric_limits<double>::infinity();
					if (nearest_sieged_tank_distance < 32 * 8 || nearest_unsieged_tank_distance < 32 * 8) use_patrol = false;
				}
				xy upos = u->pos + xy((int)(u->hspeed*latency_frames), (int)(u->vspeed*latency_frames));
				xy tpos = c->target->pos + xy((int)(c->target->hspeed*latency_frames), (int)(c->target->vspeed*latency_frames));
				double d = units_distance(upos, u, tpos, c->target);

				auto movedst = [&](xy origin, double a, double max_distance) {
					if (u->is_flying) {
						xy pos = origin;
						pos.x += (int)(std::cos(a)*max_distance);
						pos.y += (int)(std::sin(a)*max_distance);
						return pos;
					}
					xy r = origin;
					for (double distance = std::min(32.0, max_distance);; distance = std::min(distance + 32.0, max_distance)) {
						xy pos = origin;
						pos.x += (int)(std::cos(a)*distance);
						pos.y += (int)(std::sin(a)*distance);
						pos = square_pathing::get_pos_in_square(pos, u->type);
						if (pos == xy()) break;
						r = pos;
						if (distance >= max_distance) break;
					}
					return r;
				};

// 				if (use_patrol && d < 32 * 10 && c->action != unit_controller::action_kite) {
// 					xy rel = u->pos - c->target->pos;
// 					double a = std::atan2(rel.y, rel.x);
// 					xy dst = movedst(u->pos, a, 32 * 10);
// 					if (diag_distance(u->pos - dst) <= 32 * 3) use_patrol = false;
// 				}

				bool valkyrie_method = u->type == unit_types::valkyrie;

				auto move_to_attack_range = [&]() {
					if (units_distance(u, c->target) <= w->max_range) return;
					xy dst;
					if (d > w->max_range) dst = tpos;
					else {
						xy rel = u->pos - c->target->pos;
						double a = std::atan2(rel.y, rel.x);
						dst = movedst(u->pos, a, w->max_range - d);
					}

					xy movepos = (bwapi_pos)u->game_unit->getTargetPosition();
					if (u->game_unit->getOrder() != BWAPI::Orders::Move || diag_distance(dst - movepos) >= 32) {
						if (current_frame - c->last_move_to >= 6) {
							//log(log_level_info, "move in\n");
							u->game_unit->move(BWAPI::Position(dst.x, dst.y));
							c->last_move_to = current_frame;
						}
					}
				};
				auto move_away_or_intercept = [&]() {
					xy rel = u->pos - c->target->pos;
					double a = std::atan2(rel.y, rel.x);
					xy dst = movedst(u->pos, a, 32 * 10);
					if (should_intercept) dst = intercept_pos;
					if (c->action == unit_controller::action_kite) dst = c->target_pos;
					xy movepos = (bwapi_pos)u->game_unit->getTargetPosition();
					if (u->game_unit->getOrder() != BWAPI::Orders::Move || diag_distance(dst - movepos) >= 32) {
						if (current_frame - c->last_move_to >= 6) {
							//log(log_level_info, "move out\n");
							u->game_unit->move(BWAPI::Position(dst.x, dst.y));
							c->last_move_to = current_frame;
						}
					}
				};

				if (c->close_up_until >= current_frame) use_patrol = false;

				if (use_patrol && d < 32 * 10) {

					//log("c->attack_state is %d\n", c->attack_state);
					if (c->attack_state == 0) {
						bool next = d <= w->max_range && u->weapon_cooldown <= latency_frames;
						//if (valkyrie_method && u->speed < 4) next = false;
						if (next) c->attack_state = 1;
						else {
							move_to_attack_range();
						}
					} else if (c->attack_state == 1) {
						xy rel = c->target->pos - u->pos;
						double a = std::atan2(rel.y, rel.x);
						a += PI / 6;
						if (valkyrie_method) a += PI;
						xy dst = movedst(u->pos, a, rel.length());
						u->game_unit->patrol(BWAPI::Position(dst.x, dst.y));
						c->last_command_frame = current_frame;

						c->attack_timer = current_frame;
						c->attack_state = 2;
						if (valkyrie_method) {
							c->attack_state = 3;
						}
					} else if (c->attack_state == 2) {
						if (current_frame - c->attack_timer >= 4) {
							move_away_or_intercept();
							c->attack_timer = current_frame;
							c->attack_state = 3;
						}
					} else if (c->attack_state == 3) {
						int cooldown = 15;

						if (!c->target->type->is_worker) {
							double target_heading = std::atan2(c->target->vspeed, c->target->hspeed);
							xy relpos = c->target->pos - u->pos;
							double rel_angle = std::atan2(relpos.y, relpos.x);
							double da = target_heading - rel_angle;
							if (da < -PI) da += PI * 2;
							if (da > PI) da -= PI * 2;
							if (std::abs(da) > PI || c->target->speed < 4) {
								if (u->type == unit_types::vulture) cooldown = 25;
							}
						}

						if (u->type == unit_types::valkyrie) cooldown = 40;
						if (current_frame - c->attack_timer >= cooldown) c->attack_state = 0;
					} else c->attack_state = 0;

					continue;
				}

				int wait_frames = 0;
				if (u->type == unit_types::marine) wait_frames = 8;
				if (u->type == unit_types::ghost) wait_frames = 6;
				if (u->type == unit_types::firebat) wait_frames = 8;
				if (u->type == unit_types::vulture) wait_frames = 2;
				//if (u->type == unit_types::goliath) wait_frames = 2;
				if (u->type == unit_types::goliath) wait_frames = 4;
				//if (u->type == unit_types::goliath) wait_frames = 8;
				//if (u->type == unit_types::siege_tank_tank_mode) wait_frames = 2;
				//if (u->type == unit_types::siege_tank_tank_mode) wait_frames = 6;
				if (u->type == unit_types::wraith) wait_frames = 2;
				if (u->type == unit_types::battlecruiser) wait_frames = 2;

				if (u->type == unit_types::hydralisk) wait_frames = 6;
				if (u->type == unit_types::mutalisk) wait_frames = 2;
				if (u->type == unit_types::guardian) wait_frames = 2;
				if (u->type == unit_types::devourer) wait_frames = 18;

				//if (u->type == unit_types::dragoon) wait_frames = 15;
				//if (u->type == unit_types::dragoon) wait_frames = 7;
				if (u->type == unit_types::dragoon) wait_frames = 9;
				if (u->type == unit_types::scout) wait_frames = 2;
				if (u->type == unit_types::corsair) wait_frames = 60;

				unit* attack_target = c->action == unit_controller::action_kite ? c->kite_attack_target : c->target;

				bool do_state_machine = c->can_move && wait_frames;

				bool do_close_up = false;
				//if (u->type == unit_types::marine || u->type == unit_types::ghost || u->type == unit_types::firebat || u->type == unit_types::hydralisk || u->type == unit_types::goliath) {
				if (u->type == unit_types::marine || u->type == unit_types::ghost || u->type == unit_types::firebat || u->type == unit_types::hydralisk) {
					do_state_machine = false;
				}
				if (do_state_machine && attack_target->type == unit_types::siege_tank_siege_mode) {
					do_state_machine = false;
					do_close_up = true;
				}
				if (do_state_machine && attack_target->type == unit_types::interceptor) {
					do_state_machine = false;
				}
				if (u->type == unit_types::goliath) {
					if (c->action != unit_controller::action_kite) do_state_machine = false;
// 					if (attack_target->type->is_flyer || attack_target->stats->max_speed >= u->stats->max_speed) {
// 						c->action = unit_controller::action_attack;
// 						do_state_machine = false;
// 					}
					if (!ew || w->max_range <= ew->max_range - 32) do_state_machine = false;
				}
				if (c->action != unit_controller::action_kite) {
					if (u->type == unit_types::marine || u->type == unit_types::ghost || u->type == unit_types::firebat || u->type == unit_types::hydralisk || u->type == unit_types::goliath) {
						if (!ew || w->max_range <= ew->max_range || d >= ew->max_range + 64) do_state_machine = false;
					}
					if (u->type == unit_types::vulture) {
						if (!ew || ew->max_range >= w->max_range + 64 || (ew->max_range >= w->max_range && ew->cooldown >= w->cooldown)) do_state_machine = false;
					}

					if (!ew && do_state_machine) {
						bool found = false;
						double d = units_distance(u, c->target);
						for (unit* e : enemy_units) {
							if (e->gone) continue;
							weapon_stats* w = u->is_flying ? e->stats->air_weapon : e->stats->ground_weapon;
							if (!w) continue;
							if (units_distance(u, e) <= w->max_range + 64 + d) {
								found = true;
								break;
							}
						}
						if (!found) {
							do_close_up = true;
						}
					}
				}
				if (u->type == unit_types::marine || u->type == unit_types::firebat) {
					if (u->stim_timer) {
						do_state_machine = false;
						do_close_up = false;
					}
				}
				if (u->game_unit->isSieged() || u->game_unit->isBurrowed()) {
					do_state_machine = false;
					do_close_up = false;
				}
				
				//if (u->type == unit_types::siege_tank_tank_mode) * = false;

				bool do_wait_method = false;

				//if (u->type == unit_types::dragoon || u->type == unit_types::siege_tank_tank_mode) {
				if (u->type == unit_types::dragoon) {
					do_wait_method = true;
					do_state_machine = false;
					do_close_up = false;

					if (!ew || w->max_range <= ew->max_range) do_wait_method = false;
					if (c->target->type == unit_types::interceptor) do_wait_method = false;
					if (c->target->type->is_worker && units_distance(u, c->target) > 64) do_wait_method = false;
				}

				bool close_up_move_past = false;
				if (c->lay_mines_until && u->spider_mine_count) {
					close_up_move_past = true;
					do_close_up = true;
					bool target_is_moving_away = c->target->speed >= 2.0 && [&]() {
						double target_heading = std::atan2(c->target->vspeed, c->target->hspeed);
						xy relpos = c->target->pos - u->pos;
						double rel_angle = std::atan2(relpos.y, relpos.x);
						double da = target_heading - rel_angle;
						if (da < -PI) da += PI * 2;
						if (da > PI) da -= PI * 2;
						return std::abs(da) <= PI / 2;
					}();
					if (d <= 32 || (d <= 64 && target_is_moving_away)) {
						if (current_frame >= c->nospecial_until) {
							log(log_level_info, "lay mine!\n");
							c->u->game_unit->useTech(upgrade_types::spider_mines->game_tech_type, BWAPI::Position(upos.x, upos.y));
							c->nospecial_until = current_command_frame + 10;
							c->noorder_until = current_frame + 15;
						}
						continue;
					}
				}

				if (c->close_up_until >= current_frame && u->type != unit_types::dragoon && wait_frames) {
					if (c->target->type != unit_types::zealot && c->target->type != unit_types::marine && c->target->type != unit_types::zergling) {
						do_close_up = true;
						do_wait_method = false;
					}
				}
				
//				if (do_close_up) {
//					if (units_distance(u, c->target) <= 32) do_close_up = false;
//				}

				if (c->target->type == unit_types::spider_mine) do_close_up = false;

				//log(log_level_info, "%s: do_state_machine: %d do_close_up: %d\n", u->type->name, do_state_machine, do_close_up);
				if (!do_state_machine && !do_close_up && !u->is_flying) {
					if (re_evaluate_target(c, true)) continue;
				}

				xy relpos = c->target->pos - u->pos;
				double rel_angle = std::atan2(relpos.y, relpos.x);

				auto get_turn_frames = [&]() {
					double da = u->heading - rel_angle;
					if (da < -PI) da += PI * 2;
					if (da > PI) da -= PI * 2;
					return (int)(std::abs(da) / c->u->stats->turn_rate);
				};
				int turn_frames = get_turn_frames();

				if (do_wait_method) {

					//log(log_level_info, "u->is_attacking %d, u->is_attacking_frame %d, c->attack_state %d\n", u->is_attacking, u->is_attacking_frame, c->attack_state);
					
					auto is_close = [&]() {
						auto* ew = u->is_flying ? c->target->stats->air_weapon : c->target->stats->ground_weapon;
						if (!ew) return false;
						return units_distance(u, c->target) <= ew->max_range + 64.0;
					};

					if (c->attack_state == 0) {
// 						u->game_unit->attack(attack_target->game_unit);
// 						c->last_command_frame = current_command_frame;
// 						c->attack_state = 1;
// 						c->attack_timer = current_frame;

						double nd = units_distance(u->pos, u, tpos, c->target) - w->max_range;
						bool next = turn_frames + std::max(frames_to_reach(u, nd), u->weapon_cooldown) <= latency_frames;
						if (next) {
							
							bool issue_attack = true;
							if (nd <= w->max_range) {
								if (u->game_order == BWAPI::Orders::AttackUnit && u->order_target == c->target) {
									issue_attack = false;
								}
							}
							
							if (issue_attack) u->game_unit->attack(attack_target->game_unit);
							//log(log_level_info, "attack() for %d\n", current_command_frame);
							c->attack_state = 1;
							c->attack_timer = current_frame;

							c->last_command_frame = current_command_frame;

						} else {
							move_to_attack_range();
						}

					} else if (c->attack_state == 1) {
						if (u->is_attacking) {
							bool pull_back = true;
// 							if (c->target) {
// 								weapon_stats*ew = u->is_flying ? c->target->stats->air_weapon : c->target->stats->ground_weapon;
// 								if (ew && ew->max_range >= w->max_range) pull_back = false;
// 							}
							if (pull_back && current_frame - u->is_attacking_frame >= wait_frames - latency_frames) {
								if (is_close()) move_away_or_intercept();
								c->attack_state = 2;
							}
						} else {
							//if ((current_frame - c->attack_timer >= latency_frames + 15 && u->speed == 0.0) || current_frame - c->attack_timer >= 60) {
							if (current_frame - c->attack_timer >= 90) {
								u->game_unit->stop();
								c->last_command_frame = current_command_frame;
								c->attack_state = 0;
							}
						}
					} else if (c->attack_state == 2) {
						int frames = turn_frames + frames_to_reach(u->stats, 0.0, d - w->max_range);
						if (frames >= u->weapon_cooldown - latency_frames) {
							move_to_attack_range();
							c->attack_state = 0;
						} else {
							if (is_close()) move_away_or_intercept();
						}
					} else c->attack_state = 0;

					continue;

				} else if (do_close_up) {

// 					log("close up, c->attack_state is %d  c->attack_timer is %d\n", c->attack_state, c->attack_timer);
// 					log("turn_frames is %d\n", turn_frames);

					if (c->attack_state == 0) {
						bool next = d <= w->max_range && u->weapon_cooldown <= latency_frames + turn_frames;
						if (next) {
							//u->game_unit->stop();

							c->attack_state = 1;
							c->attack_timer = current_frame + turn_frames + wait_frames;
						} else {
							xy dst = c->target->pos;

							if (close_up_move_past) {
								xy movepos = dst;
//								xy movepos = attack_target->pos;
// 								double rx = attack_target->pos.x - u->pos.x;
// 								double ry = attack_target->pos.y - u->pos.y;
// 								double len = std::sqrt(rx*rx + ry*ry);
// 								xy past_pos((int)(attack_target->pos.x + (rx / len * 64)), (int)(attack_target->pos.y + (ry / len * 64)));
// 								if (grid::get_build_square(past_pos).entirely_walkable) movepos = past_pos;
								if (c->target->speed >= 2.0 && diag_distance(c->u->pos - c->target->pos) <= 32 * 4) {
									double target_heading = std::atan2(c->target->vspeed, c->target->hspeed);
									xy relpos = c->target->pos - u->pos;
									double rel_angle = std::atan2(relpos.y, relpos.x);
									double da = target_heading - rel_angle;
									if (da < -PI) da += PI * 2;
									if (da > PI) da -= PI * 2;
									if (std::abs(da) <= PI / 2) {

										double offset_heading;
										if (da >= 0) offset_heading = PI / 2;
										else offset_heading = PI / 2;

										xy target_pos = c->target->pos;
										double max_distance = 32 * 4;
										for (double distance = 0; distance < max_distance; distance += 32) {
											xy pos = c->target->pos;
											pos.x += (int)(std::cos(target_heading)*distance);
											pos.y += (int)(std::sin(target_heading)*distance);
											pos.x += (int)(std::cos(offset_heading) * 48);
											pos.y += (int)(std::sin(offset_heading) * 48);
											pos = square_pathing::get_pos_in_square(pos, c->target->type);
											if (pos == xy()) break;
											target_pos = pos;
										}
										//log(log_level_test, "chase, target_pos is %g away\n", (target_pos - c->target->pos).length());
										movepos = target_pos;
										//game->drawLineMap(c->u->pos.x, c->u->pos.y, movepos.x, movepos.y, BWAPI::Colors::Purple);
									}
								}
								if (u->game_unit->getOrder() != BWAPI::Orders::Move || diag_distance(dst - movepos) >= 32) {
									if (current_frame - c->last_move_to >= 6) {
										u->game_unit->move(BWAPI::Position(dst.x, dst.y));
										c->last_move_to = current_frame;
									}
								}
							} else {
								if (d >= w->max_range / 2) {
									xy movepos = (bwapi_pos)u->game_unit->getTargetPosition();
									if (u->game_unit->getOrder() != BWAPI::Orders::Move || diag_distance(dst - movepos) >= 32) {
										if (current_frame - c->last_move_to >= 6) {
											u->game_unit->move(BWAPI::Position(dst.x, dst.y));
											c->last_move_to = current_frame;
										}
									}
								}
							}
						}
					} else if (c->attack_state == 1) {
						u->game_unit->attack(attack_target->game_unit);
						c->last_command_frame = current_command_frame;
						c->attack_state = 2;
					} else if (c->attack_state == 2) {
						if (current_frame >= c->attack_timer) c->attack_state = 0;
					} else c->attack_state = 0;

					continue;

				} else if (do_state_machine && wait_frames) {

					//log(log_level_info, "c->attack_state is %d\n", c->attack_state);
					//log(log_level_info, "turn_frames is %d\n", turn_frames);

					auto get_target_is_facing_me = [&]() {
						double target_heading = std::atan2(c->target->vspeed, c->target->hspeed);
						xy relpos = c->target->pos - u->pos;
						double rel_angle = std::atan2(relpos.y, relpos.x);
						double da = target_heading - rel_angle;
						if (da < -PI) da += PI * 2;
						if (da > PI) da -= PI * 2;
						return std::abs(da) > PI / 2;
					};
					bool target_is_facing_me = get_target_is_facing_me();

					if (!u->is_flying) {

						if (c->attack_state == 0) {
							//bool next = d <= w->max_range && u->weapon_cooldown <= latency_frames + turn_frames + latency_frames + 1;
							//double cur_d = units_distance(u->pos, u, c->target->pos, c->target) - w->max_range;
							//bool next = std::max(frames_to_reach(u, cur_d), u->weapon_cooldown) <= latency_frames + turn_frames;
							double nd = units_distance(u->pos, u, tpos, c->target) - w->max_range;
							bool next = turn_frames + std::max(frames_to_reach(u, nd), u->weapon_cooldown) <= latency_frames;
							if (next) {
// 								u->game_unit->stop();
// 								log(log_level_info, "stop()\n");
// 
// 								c->attack_state = 1;
// 								c->attack_timer = latency_frames + 1 + current_command_frame + turn_frames + wait_frames;

								u->game_unit->attack(attack_target->game_unit);
								//log(log_level_info, "attack() for %d\n", current_command_frame);
								c->attack_state = 2;
								c->attack_timer = current_command_frame + turn_frames + wait_frames;

								c->last_command_frame = current_command_frame;

							} else {
								move_to_attack_range();
							}
						} else if (c->attack_state == 1) {
							u->game_unit->attack(attack_target->game_unit, true);
							//log(log_level_info, "attack()\n");
							c->last_command_frame = current_command_frame;
							c->attack_state = 2;
						} else if (c->attack_state == 2) {
							if (current_command_frame >= c->attack_timer) {
								int frames = turn_frames + frames_to_reach(u->stats, 0.0, d - w->max_range);
								if (current_frame >= c->attack_timer + latency_frames && frames >= u->weapon_cooldown - latency_frames) {
								//bool next = turn_frames + frames_to_reach(u->stats, 0.0, d - w->max_range) >= u->weapon_cooldown;
								//if (current_frame >= c->attack_timer + latency_frames && next) {
									move_to_attack_range();
									c->attack_state = 0;
								} else {
									move_away_or_intercept();
								}
							}
						} else c->attack_state = 0;

					} else {

//						log("c->attack_state is %d\n", c->attack_state);
//						log("turn_frames is %d\n", turn_frames);

						if (c->attack_state == 0) {
							bool next = d <= w->max_range && u->weapon_cooldown <= latency_frames + turn_frames;
							if (next) {
								if (turn_frames) {
									u->game_unit->move(BWAPI::Position(c->target->pos.x, c->target->pos.y));
									c->attack_state = 10;
									c->attack_timer = current_frame + turn_frames;
								} else {
									u->game_unit->attack(attack_target->game_unit);
									c->last_command_frame = current_command_frame;
									c->attack_state = 1;
									c->attack_timer = current_frame + turn_frames + wait_frames + 1;
								}
							} else {

								if (!c->target->visible || c->target->cloaked) {
									xy dst = u->pos;
									dst.x += (int)(std::cos(rel_angle) * 32 * 10);
									dst.y += (int)(std::sin(rel_angle) * 32 * 10);

									xy movepos = (bwapi_pos)u->game_unit->getTargetPosition();
									if (u->game_unit->getOrder() != BWAPI::Orders::Move || diag_distance(dst - movepos) >= 32) {
										if (current_frame - c->attack_timer >= 6) {
											u->game_unit->move(BWAPI::Position(dst.x, dst.y));
											c->attack_timer = current_frame;
										}
									}
								} else {
									if (u->game_unit->getOrder() != BWAPI::Orders::Follow || u->game_unit->getOrderTarget() != c->target->game_unit) {
										if (current_frame - c->attack_timer >= 6) {
											u->game_unit->follow(c->target->game_unit);
											c->attack_timer = current_frame;
										}
									}
								}

							}
						} else if (c->attack_state == 10) {
							//if (turn_frames <= latency_frames || current_frame >= c->attack_timer) {
							if (current_frame >= c->attack_timer) {
								u->game_unit->attack(attack_target->game_unit);
								c->last_command_frame = current_command_frame;
								c->attack_state = 1;
								c->attack_timer = current_frame + turn_frames + wait_frames + 1;
							}
						} else if (c->attack_state == 1) {
							if (current_frame >= c->attack_timer && current_command_frame != c->last_command_frame) {
								xy dst = u->pos;
								dst.x += (int)(std::cos(rel_angle) * 32 * -10);
								dst.y += (int)(std::sin(rel_angle) * 32 * -10);

								u->game_unit->move(BWAPI::Position(dst.x, dst.y));
								c->attack_state = 2;
								c->attack_timer = current_frame + w->cooldown;
								if (!target_is_facing_me) c->attack_timer -= 6;
							}
						} else if (c->attack_state == 2) {
							double speed = u->stats->max_speed;
							speed += target_is_facing_me ? c->target->speed : -c->target->speed;
							int frames = turn_frames + (int)(std::max(d - w->max_range, 0.0) / speed);
							//log("frames is %d\n", frames);
							if (current_frame >= c->attack_timer - frames) {
								c->attack_state = 0;
								c->attack_timer = 0;
							}

						} else c->attack_state = 0;

					}

					continue;
				}
			}
		}

		if (current_frame < c->noorder_until) continue;

		if (current_frame < c->move_away_until && !u->is_flying) {
			if (u->game_unit->isSieged()) u->game_unit->unsiege();
			move_away(c);
			continue;
		}
		
		if (u->game_unit->isSieged() && current_frame - c->last_siege >= 15 * 60 && current_frame - u->last_attacked >= 15 * 30)  {
			u->game_unit->unsiege();
		}

		if (should_intercept) {
			xy movepos = (bwapi_pos)u->game_unit->getTargetPosition();
			if (u->game_unit->getOrder() != BWAPI::Orders::Move || diag_distance(intercept_pos - movepos) > 32 * 2) {
				c->last_move = current_frame;
				c->u->game_unit->move(BWAPI::Position(intercept_pos.x, intercept_pos.y));
				c->noorder_until = current_frame + 8;
			}
			continue;
		}

		if (c->action==unit_controller::action_move && c->can_move && current_frame-c->last_move>=move_resolution) {
			c->last_move = current_frame;
			move(c);
		}
		if (c->action == unit_controller::action_move_directly && c->can_move && current_frame - c->last_move >= move_resolution) {
			c->last_move = current_frame;
			if (c->last_move_to_pos != c->go_to || current_frame >= c->last_move_to + 30) {
				c->last_move_to = current_frame;
				c->last_move_to_pos = c->go_to;
				c->u->game_unit->move(BWAPI::Position(c->go_to.x, c->go_to.y));
				c->noorder_until = current_frame + rng(8);
			}
		}

		if (c->action == unit_controller::action_use_ability) {
			c->noorder_until = current_frame + rng(8);
			bool used = false;
			if (c->ability == upgrade_types::spider_mines) {
				used = u->game_unit->useTech(c->ability->game_tech_type, BWAPI::Position(u->pos.x, u->pos.y));
				c->noorder_until = current_frame + 15;
			} else if (c->ability == upgrade_types::nuclear_strike) {
				used = u->game_unit->useTech(c->ability->game_tech_type, BWAPI::Position(c->target_pos.x, c->target_pos.y));
				c->noorder_until = current_frame + 15;
			} else if (c->ability == upgrade_types::personal_cloaking || c->ability == upgrade_types::cloaking_field) {
				used = u->game_unit->useTech(c->ability->game_tech_type);
				c->noorder_until = current_frame + 15;
			} else if (c->ability == upgrade_types::defensive_matrix) {
				used = u->game_unit->useTech(c->ability->game_tech_type, c->target->game_unit);
				c->noorder_until = current_frame + 15;
			} else xcept("unknown ability %s", c->ability->name);
			if (used) {
				c->action = unit_controller::action_idle;
			}
// 			if (c->ability->game_tech_type.targetsUnit()) {
// 				u->game_unit->useTech(c->ability->game_tech_type, c->target->game_unit);
// 			} else if (c->ability->game_tech_type.targetsPosition()) {
// 				u->game_unit->useTech(c->ability->game_tech_type, BWAPI::Position(u->pos.x, u->pos.y));
// 			} else {
// 				u->game_unit->useTech(c->ability->game_tech_type);
// 			}
		}

		if (c->action == unit_controller::action_repair) {
			if (units_pathing_distance(u, c->target) > 32 * 4 && square_pathing::unit_can_reach(u, u->pos, c->target->pos, square_pathing::pathing_map_index::include_liftable_wall)) {
				c->go_to = c->target->pos;
				move(c);
			} else {
				if (u->game_order != BWAPI::Orders::Repair || u->order_target != c->target) {
					u->game_unit->repair(c->target->game_unit);
					c->noorder_until = current_frame + 4;
				}
			}
		}

		if (c->action==unit_controller::action_gather) {

			unit* target_unit = u->game_unit->isCarryingMinerals() || u->game_unit->isCarryingGas() ? c->depot : c->target;
			unit* nydus = target_unit ? square_pathing::get_nydus_canal_from_to(square_pathing::get_pathing_map(u->type), u->pos, target_unit->pos) : nullptr;
			if (nydus) {
				//log(log_level_test, "gather: moving through nydus %p\n", nydus);
				if (current_frame >= c->last_move_to + 15) {

					u->game_unit->rightClick(nydus->game_unit);

					c->noorder_until = current_frame + 30;
					c->last_move_to = current_frame;
					return;
				}
			} else {

				issue_gather_order(c);
			}

		}

		auto wait_for_return_resources = [&]() {
			if (c->u->type->is_worker && c->u->is_carrying_minerals_or_gas) {
				if (c->return_minerals_frame == 0) c->return_minerals_frame = current_frame;
				if (current_frame - c->return_minerals_frame < 15 * 10) {
					auto o = u->game_order;
					if (u->game_unit->isCarryingMinerals() || u->game_unit->isCarryingGas()) {
						if (o != BWAPI::Orders::ReturnMinerals && o != BWAPI::Orders::ReturnGas) {
							u->game_unit->returnCargo();
							c->noorder_until = current_frame + 8;
						}
					}
					return true;
				}
			} else c->return_minerals_frame = 0;
			return false;
		};

		if (c->action != unit_controller::action_build) {
			if (c->fail_build_count) c->fail_build_count = 0;
		}
		if (c->action==unit_controller::action_build) {

			if (!wait_for_return_resources()) {
				if (c->target_type->is_building) {

					xy build_pos = c->target_pos;
					bool is_inside = units_distance(u->pos, u->type, build_pos + xy(c->target_type->tile_width * 16, c->target_type->tile_height * 16), c->target_type) <= 32;
					if (c->target) {
						c->fail_build_count = 0;
						if (u->build_unit && u->build_unit != c->target) {
							//log("building wrong thing, halt!\n");
							u->game_unit->haltConstruction();
							c->noorder_until = current_frame + 8;
						} else if (!u->build_unit) {
							if (units_pathing_distance(u, c->target) >= 32) {
								//log("move to resume building!\n");
								c->go_to = c->target->pos;
								move(c);
								//u->game_unit->move(BWAPI::Position(c->go_to.x,c->go_to.y));
								c->noorder_until = current_frame + 5;
							} else {
								//log("right click!\n");
								u->game_unit->rightClick(c->target->game_unit);
								c->noorder_until = current_frame + 4;
							}
						}// else log("building, lalala\n");
					} else if (is_inside && current_frame + latency_frames >= c->wait_until) {
						//log("build!\n");
						bool enough_minerals = c->target_type->minerals_cost == 0 || current_minerals >= c->target_type->minerals_cost;
						bool enough_gas = c->target_type->gas_cost == 0 || current_gas >= c->target_type->gas_cost;
						bool prereq_ok = true;
						for (auto*ut : c->target_type->required_units) {
							if (my_completed_units_of_type[ut].empty()) {
								if (ut == unit_types::spire) ut = unit_types::greater_spire;
								if (ut == unit_types::lair && !my_completed_units_of_type[unit_types::hive].empty()) ut = unit_types::hive;
								if (ut == unit_types::hatchery && !my_completed_units_of_type[unit_types::lair].empty()) ut = unit_types::lair;
								if (ut == unit_types::hatchery && !my_completed_units_of_type[unit_types::hive].empty()) ut = unit_types::hive;
							}
							if (my_completed_units_of_type[ut].empty()) {
								prereq_ok = false;
								break;
							}
						}
						if (enough_minerals && enough_gas && prereq_ok) {
							bwapi_call_build(u->game_unit, c->target_type->game_unit_type, BWAPI::TilePosition(build_pos.x / 32, build_pos.y / 32));
							bool something_in_the_way = move_stuff_away_from_build_pos(build_pos, c->target_type, c);
							if (!something_in_the_way) {
								++c->fail_build_count;
								++fail_build_positions[build_pos];
							}
							log("c->fail_build_count is now %d\n", c->fail_build_count);
						}
						c->noorder_until = current_frame + 7;
					} else {
						//log("move to %d %d!\n",build_pos.x,build_pos.y);
						c->go_to = build_pos + xy(c->target_type->tile_width * 16, c->target_type->tile_height * 16);
						move(c);
						if (c->last_move_to == current_frame && (current_frame > 15 * 60 * 8 && !combat::can_transfer_through(c->last_move_to_pos))) {
							if (diag_distance(c->go_to - u->pos) > 32*6) {
								a_deque<xy> path = square_pathing::find_square_path(square_pathing::get_pathing_map(u->type), u->pos, [&](xy pos, xy npos) {
									return diag_distance(npos - u->pos) <= 32 * 20;
								}, [&](xy pos, xy npos) {
									return diag_distance(c->last_move_to_pos - u->pos);
								}, [&](xy pos) {
									size_t index = grid::build_square_index(pos);
									return combat::can_transfer_through(pos);
								});
								if (!path.empty()) {
									u->game_unit->move(BWAPI::Position(path.back().x, path.back().y));
								}
							}
						}
						//u->game_unit->move(BWAPI::Position(c->go_to.x,c->go_to.y));
						c->noorder_until = current_frame + 5;
					}
				} else log("don't know how to build %s\n", c->target_type->name);
			}

		} else if (u->build_unit && !u->is_being_constructed) {
			if (u->type==unit_types::scv) {
				log("is building something, but shouldn't be!\n");
				u->game_unit->haltConstruction();
				c->noorder_until = current_frame + 8;
			}
		}
		
		if (c->u->type == unit_types::carrier && (c->action == unit_controller::action_attack || c->action == unit_controller::action_kite) && c->target) {
			//game->drawLineMap(c->u->pos.x, c->u->pos.y, c->target->pos.x, c->target->pos.y, BWAPI::Colors::Red);
			
			double max_d = 32 * 8;
			
			bool is_attacking = true;
			for (auto u : c->u->game_unit->getInterceptors()) {
				if (u->getOrder() != BWAPI::Orders::InterceptorAttack) {
					is_attacking = false;
					break;
				}
			}
			if (is_attacking) max_d = 32 * 9;
			
			if (current_frame < c->carrier_no_threats_until) max_d = 32 * 4;
			
			double d = units_distance(c->u, c->target);
			if (d <= max_d) {
				xy relpos = c->target->pos - c->u->pos;
				double x = relpos.x / relpos.length();
				double y = relpos.y / relpos.length();
				xy move_to = c->u->pos - xy(x * 32 * 4, y * 32 * 4);
				c->last_move = current_frame;
				if (c->last_move_to_pos != move_to || current_frame >= c->last_move_to + 30) {
					c->last_move_to = current_frame;
					c->last_move_to_pos = move_to;
					c->u->game_unit->attack(BWAPI::Position(move_to.x, move_to.y));
					c->noorder_until = current_frame + rng(8);
				}
				continue;
			}
			
		}

		if (c->u->type == unit_types::dragoon && (c->action == unit_controller::action_attack || c->action == unit_controller::action_kite) && c->target) {
			if (!u->is_attacking && u->weapon_cooldown > latency_frames + 4 && c->target->type == unit_types::siege_tank_siege_mode) {
				double x = 0.0;
				double y = 0.0;
				int n = 0;
				for (unit* n : my_units) {
					if (n != u && n->type == u->type && units_distance(u, n) < 20) {
						x += n->pos.x;
						y += n->pos.y;
						++n;
					}
				}
				if (n) {
					x /= n;
					y /= n;
					x -= u->pos.x;
					y -= u->pos.y;
					x = u->pos.x - x;
					y = u->pos.y - y;
					xy move_to((int)x, (int)y);
					c->last_move = current_frame;
					if (c->last_move_to_pos != move_to || current_frame >= c->last_move_to + 30) {
						c->last_move_to = current_frame;
						c->last_move_to_pos = move_to;
						c->u->game_unit->move(BWAPI::Position(move_to.x, move_to.y));
						c->noorder_until = current_frame + rng(4);
					}
					continue;
				}
			}
		}

		bool do_attack = false;
		if (c->action == unit_controller::action_kite && c->target) {

			weapon_stats*w = c->target->is_flying ? u->stats->air_weapon : u->stats->ground_weapon;
			weapon_stats*ew = u->is_flying ? c->target->stats->air_weapon : c->target->stats->ground_weapon;
			if (w && !ew) do_attack = true;
			else {
				xy upos = u->pos + xy((int)(u->hspeed*latency_frames), (int)(u->vspeed*latency_frames));
				xy tpos = c->target->pos + xy((int)(c->target->hspeed*latency_frames), (int)(c->target->vspeed*latency_frames));
				double d = units_distance(upos, u, tpos, c->target) - (w ? w->max_range : 64.0);
				if (c->u->type == unit_types::goliath) d += 32;
				if (w && frames_to_reach(u, d) >= u->weapon_cooldown - latency_frames) do_attack = true;
				else {
					xy move_to = c->target_pos;
					//if (c->u->type == unit_types::goliath && u->weapon_cooldown <= 2) move_to = c->target->pos;
					c->last_move = current_frame;
					if (c->last_move_to_pos != move_to || current_frame >= c->last_move_to + 30) {
						c->last_move_to = current_frame;
						c->last_move_to_pos = move_to;
						c->u->game_unit->move(BWAPI::Position(move_to.x, move_to.y));
						c->noorder_until = current_frame + rng(8);
						//if (c->u->type == unit_types::goliath) c->noorder_until -= 2;
					}
				}
			}
		}

		if ((c->action == unit_controller::action_attack && c->target) || do_attack) {

			if (c->target->dead) c->action = unit_controller::action_idle;
			else {
				if (u->order_target != c->target || u->game_order != BWAPI::Orders::AttackUnit) {
					bool issue_attack = true;
					if (u->type == unit_types::medic) {
						issue_attack = false;
						if (!bwapi_is_healing_order(u->game_order)) {
// 							if (!u->game_unit->useTech(upgrade_types::healing->game_tech_type, c->target->game_unit)) {
// 								c->go_to = c->target->pos;
// 								move(c);
// 							}
							u->game_unit->useTech(upgrade_types::healing->game_tech_type, BWAPI::Position(c->target->pos.x, c->target->pos.y));
						}
					}
					if (u->type == unit_types::lurker) {
						issue_attack = false;
						weapon_stats* w = c->target->is_flying ? u->stats->air_weapon : u->stats->ground_weapon;
						//weapon_stats* ew = u->is_flying ? c->target->stats->air_weapon : c->target->stats->ground_weapon;
						double d = units_distance(u, c->target);
						if (w && d <= w->max_range && d >= w->min_range) {
							//if (d <= 32 * 2 || (ew && d <= ew->max_range - 32)) {
							if (d <= 32 * 2) {
								if (current_frame - c->last_burrow >= 30) {
									if (!u->burrowed) {
										u->game_unit->burrow();
										c->last_burrow = current_frame;
									}
								}
							}
						} else {
							c->go_to = c->target->pos;
							move(c);
						}
					}
					if (issue_attack && u->weapon_cooldown > latency_frames + 4 && u->speed > 0.5) {
						weapon_stats* w = c->target->is_flying ? u->stats->air_weapon : u->stats->ground_weapon;
						if (w) {
							if (units_distance(u, c->target) <= w->max_range) {
								u->game_unit->holdPosition();
								issue_attack = false;
							}
						}
					}
					if (issue_attack && !u->game_unit->attack(c->target->game_unit)) {
						xy order_pos(((bwapi_pos)u->game_unit->getOrderTargetPosition()).x, ((bwapi_pos)u->game_unit->getOrderTargetPosition()).y);
						if (u->game_order != BWAPI::Orders::AttackMove || diag_distance(order_pos - c->target->pos) >= 32 * 4) {
							u->game_unit->attack(BWAPI::Position(c->target->pos.x, c->target->pos.y));
						}
					}
					c->last_command_frame = current_command_frame;
				}
			}

			c->noorder_until = current_frame + 4;

			if (u->type == unit_types::dragoon) c->noorder_until = current_command_frame + 9;
			if (u->type == unit_types::dragoon) {
				//if (c->target && !c->target->dead && c->target->visible && (!c->target->cloaked || c->target->detected)) xcept("attack with dragoon?");
			}
		}

		if (c->action == unit_controller::action_scout && c->can_move && current_frame - c->last_move >= move_resolution) {
			if (!wait_for_return_resources()) {
				c->last_move = current_frame;
				move(c);
			}
		}

		if (c->action == unit_controller::action_building_movement) {
			u->building->nobuild_until = current_frame + 15 * 4;
			if (current_frame >= c->timeout) {
				if (u->type == unit_types::cc) {
					c->lift = false;
					c->target_pos = get_nearest_available_cc_build_pos(u->pos);
				}
			}
			//if (c->lift || (!u->building->is_lifted && u->building->build_pos != c->target_pos)) {
			if (c->lift) {
				if (!u->building->is_lifted) {
					u->game_unit->lift();
					c->noorder_until = current_frame + 30;
				}
			} else {
				if (u->building->is_lifted) {
					if (diag_distance(c->target_pos - u->pos) >= 128) {
						u->game_unit->move(BWAPI::Position(c->target_pos.x, c->target_pos.y));
						c->noorder_until = current_frame + 15;
					} else {
						move_stuff_away_from_build_pos(c->target_pos, c->u->type, nullptr);
						u->game_unit->land(BWAPI::TilePosition(c->target_pos.x / 32, c->target_pos.y / 32));
						c->noorder_until = current_frame + 15;
					}
				} else c->action = unit_controller::action_idle;
			}
		}

		if (c->action == unit_controller::action_recharge) {
			unit*bat = get_best_score(my_completed_units_of_type[unit_types::shield_battery], [&](unit*bat) {
				double d = units_distance(bat, u);
				if (d <= 32 * 4) return 1.0 - bat->energy / bat->stats->energy;
				return d;
			});
			if (bat) {
				u->game_unit->rightClick(bat->game_unit);
				c->noorder_until = current_frame + 8;
			}
		}

	}

}

void fast_kill_spider_mine(unit* e) {
	double hp = e->shields + e->hp - kill_spider_mine_damage[e];
	if (hp <= 0.0) return;
	a_vector<std::pair<unit_controller*, double>> units_in_range;
	units_in_range.reserve(0x10);
	for (unit* u : my_units) {
		if (u->type->is_non_usable) continue;
		unit_controller* c = u->controller;
		if (c->last_command_frame == current_command_frame) continue;
		if (current_frame - c->last_fast_kill_spider_mine < 20) continue;
		double damage = get_weapon_damage_in_range(c->u, e);
		if (damage == 0.0) continue;
		units_in_range.emplace_back(c, damage);
	}
	std::sort(units_in_range.begin(), units_in_range.end(), [&](auto& a, auto& b) {
		if (a.first->u->weapon_cooldown != b.first->u->weapon_cooldown) return a.first->u->weapon_cooldown < b.first->u->weapon_cooldown;
		return a.second < b.second;
	});
	for (auto& v : units_in_range) {
		unit_controller* c = v.first;
		double damage = v.second;
		c->last_fast_kill_spider_mine = current_frame;
		c->last_process = current_frame;
		c->last_command_frame = current_command_frame;
		c->noorder_until = current_frame + c->u->weapon_cooldown + 8;
		c->u->game_unit->attack(e->game_unit);
		if (c->action == unit_controller::action_attack || c->action == unit_controller::action_kite) c->target = e;
		if (c->u->weapon_cooldown <= latency_frames) {
			kill_spider_mine_damage[e] += damage;
			hp -= damage;
			if (hp <= 0) break;
		}
		//printf("%s fast kill spider mine for %g damage!\n", c->u->type->name.c_str(), damage);
	}
}

void on_show(unit* u) {
	if (!u->owner->is_enemy) return;
	if (u->type == unit_types::spider_mine) fast_kill_spider_mine(u);
}


void control_units_task() {

	a_vector<unit_controller*> controllers;

	while (true) {

		controllers.clear();
		for (unit*u : my_units) {
			controllers.push_back(get_unit_controller(u));
		}

		process(controllers);


		multitasking::sleep(1);

	}

}

void render() {

	for (unit*u : my_units) {
		unit_controller*c = u->controller;
		if (!c || !c->can_move) continue;

		std::string s = "?";
		if (c->action == unit_controller::action_idle) s = "idle";
		if (c->action == unit_controller::action_move) s = "move";
		if (c->action == unit_controller::action_gather) s = "gather";
		if (c->action == unit_controller::action_build) s = "build";
		if (c->action == unit_controller::action_attack) s = "attack";
		if (c->action == unit_controller::action_kite) s = "kite";
		if (c->action == unit_controller::action_scout) s = "scout";
		if (c->action == unit_controller::action_move_directly) s = "move directly";
		if (c->action == unit_controller::action_use_ability) s = "use ability";
		if (c->action == unit_controller::action_repair) s = "repair";
		if (c->action == unit_controller::action_building_movement) s = "building movement";
		if (c->action == unit_controller::action_recharge) s = "recharge";
		if (c->action == unit_controller::action_worker_micro) s = "worker micro";

		game->drawTextMap(u->pos.x-8,u->pos.y+8,"%s",s.c_str());
		
	}

// 	for (unit*u : my_units) {
// 		if (current_frame - u->controller->last_move_to < 15) {
// 			game->drawLineMap(u->pos.x, u->pos.y, u->controller->last_move_to_pos.x, u->controller->last_move_to_pos.y, BWAPI::Colors::White);
// 		}
// 	}
// 
// 	for (unit*u : my_units) {
// 		if (current_frame - u->controller->last_move_to < 15 * 2) {
// 			game->drawLineMap(u->pos.x, u->pos.y, u->controller->go_to.x, u->controller->go_to.y, BWAPI::Colors::Purple);
// 		}
// 	}

// 	for (unit*u : my_units) {
// 		unit_controller*c = u->controller;
// 		if (!c || !c->can_move) continue;
// 
// 		game->drawLineMap(u->pos.x,u->pos.y,c->last_move_to_pos.x,c->last_move_to_pos.y,BWAPI::Colors::White);
// 	}

// 	for (int y=0;y<grid::walk_grid_height;++y) {
// 		for (int x=0;x<grid::walk_grid_width;++x) {
// 
// 			auto&v = force_field[x + y*grid::walk_grid_width];
// 			if (current_frame-std::get<1>(v)>60 || std::get<1>(v)==0) continue;
// 			xy force = std::get<0>(v);
// 			xy pos(x*8,y*8);
// 			xy fpos = pos + force;
// 			game->drawLineMap(pos.x+2,pos.y,fpos.x,fpos.y,BWAPI::Colors::Red);
// 			game->drawLineMap(pos.x,pos.y+2,fpos.x,fpos.y,BWAPI::Colors::Red);
// 			game->drawLineMap(pos.x-2,pos.y,fpos.x,fpos.y,BWAPI::Colors::Red);
// 			game->drawLineMap(pos.x,pos.y-2,fpos.x,fpos.y,BWAPI::Colors::Red);
// 
// 		}
// 	}

}



void init() {

	if (game->isReplay()) return;

	units::on_show_callbacks.push_back(on_show);

	multitasking::spawn(control_units_task, "control units");

	render::add(render);

}

}



