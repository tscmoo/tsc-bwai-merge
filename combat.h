
namespace combat {
;

struct choke_t {
	a_deque<xy> path;
	a_vector<grid::build_square*> build_squares;
	double width;
	xy inside;
	xy center;
	xy outside;
	a_vector<grid::build_square*> inside_squares;
	a_vector<grid::build_square*> outside_squares;
};
choke_t defence_choke;
tsc::dynamic_bitset defence_choke_outside_bitset;
a_deque<xy> dont_siege_here_path;
tsc::dynamic_bitset dont_siege_here;
bool defence_is_scared;
int force_defence_is_scared_until;

double combat_mult_override;
int combat_mult_override_until;

xy my_closest_base;
xy op_closest_base;
xy my_closest_base_override;
int my_closest_base_override_until = 0;
a_vector<a_deque<xy>> inter_base_paths;

bool has_found_opponent_start_location = false;
xy opponent_start_location;

int defence_choke_use_nearest_until = 0;

tsc::dynamic_bitset my_base;
tsc::dynamic_bitset op_base;

tsc::dynamic_bitset my_base_defence;

struct control_spot {
	xy pos;
	tsc::dynamic_bitset area;
	double control_value = 0.0;
	xy incoming_pos;
	double weight = 0.0;
	bool reachable = false;
	bool visited = false;

	a_vector<grid::build_square*> squares;

	bool is_map_split_center = false;
	bool is_best_defence = false;
};

struct xy_hash {
	size_t operator()(xy pos) const {
		return std::hash<int>()(pos.x) ^ std::hash<int>()(pos.y);
	}
};

a_unordered_map<xy, control_spot, xy_hash> control_spot_map;

a_vector<control_spot*> active_control_spots;

a_vector<int> enemy_siege_tank_area;

tsc::dynamic_bitset map_split_my_area;
tsc::dynamic_bitset map_split_op_area;

tsc::dynamic_bitset map_split_border_area;
a_vector<xy> map_split_border_locations;

xy map_split_center_pos;

void update_base(tsc::dynamic_bitset&base, const a_vector<xy>& positions, double radius = 32 * 12, a_vector<grid::build_square*>* squares = nullptr) {
	base.reset();
	if (squares) squares->clear();

	tsc::dynamic_bitset visited(grid::build_grid_width*grid::build_grid_height);
	a_deque<std::tuple<grid::build_square*, xy>> open;
	for (xy pos : positions) {
		open.emplace_back(&grid::get_build_square(pos), pos);
		size_t index = grid::build_square_index(pos);
		visited.set(index);
		base.set(index);
		if (squares) squares->push_back(&grid::get_build_square(pos));
	}
	while (!open.empty()) {
		grid::build_square*bs;
		xy origin;
		std::tie(bs, origin) = open.front();
		open.pop_front();

		auto add = [&](int n) {
			grid::build_square*nbs = bs->get_neighbor(n);
			if (!nbs) return;
			if (!nbs->entirely_walkable) return;
			if (diag_distance(nbs->pos - origin) >= radius) return;
			size_t index = grid::build_square_index(*nbs);
			if (visited.test(index)) return;
			visited.set(index);
			base.set(index);

			if (squares) squares->push_back(nbs);

			open.emplace_back(nbs, origin);
		};
		add(0);
		add(1);
		add(2);
		add(3);
	}
}

void update_base(tsc::dynamic_bitset&base, const a_vector<unit*>&buildings, double radius = 32 * 12) {
	base.reset();

	a_vector<xy> positions;
	for (unit*u : buildings) {
		for (auto*v : u->building->build_squares_occupied) {
			positions.push_back(v->pos);
		}
	}
	update_base(base, positions, radius);
}

void update_my_base() {
	update_base(my_base, my_buildings);

	my_base_defence.reset();

	a_vector<xy> positions;
	for (unit* u : my_buildings) {
		for (auto*v : u->building->build_squares_occupied) {
			positions.push_back(v->pos);
		}
	}
	for (auto& b : build::build_tasks) {
		if (b.build_pos != xy()) positions.push_back(b.build_pos);
	}
	update_base(my_base_defence, positions, 32 * 2);
}
void update_op_base() {
	update_base(op_base, enemy_buildings);
}

template<typename pred_T>
void find_nearby_entirely_walkable_tiles(xy pos, pred_T&&pred, double max_distance = 32 * 12, bool allow_crossing_nonwalkable = false) {
	tsc::dynamic_bitset visited(grid::build_grid_width*grid::build_grid_height);
	a_deque<std::tuple<grid::build_square*, xy>> open;
	open.emplace_back(&grid::get_build_square(pos), pos);
	size_t index = grid::build_square_index(pos);
	visited.set(index);
	while (!open.empty()) {
		grid::build_square*bs;
		xy origin;
		std::tie(bs, origin) = open.front();
		open.pop_front();
		if (!pred(bs->pos)) continue;

		auto add = [&](int n) {
			grid::build_square*nbs = bs->get_neighbor(n);
			if (!nbs) return;
			if (!allow_crossing_nonwalkable && !nbs->entirely_walkable) return;
			if (diag_distance(nbs->pos - origin) >= max_distance) return;
			size_t index = grid::build_square_index(*nbs);
			if (visited.test(index)) return;
			visited.set(index);

			open.emplace_back(nbs, origin);
		};
		add(0);
		add(1);
		add(2);
		add(3);
	}
}

struct combat_unit: refcounted {
	unit*u = nullptr;
	enum { action_idle, action_offence, action_tactic };
	int action = action_idle;
	enum { subaction_idle, subaction_move, subaction_fight, subaction_kite, subaction_move_directly, subaction_use_ability, subaction_repair, subaction_recharge, subaction_worker_micro, subaction_protect_marines_micro };
	int subaction = subaction_idle;
	int prev_subaction = subaction_idle;
	xy defend_spot;
	xy goal_pos;
	int last_fight = 0;
	int last_run = 0;
	int last_processed_fight = 0;
	unit*target = nullptr;
	unit*kite_attack_target= nullptr;
	xy target_pos;
	int last_wanted_a_lift = 0;
	int last_given_a_lift = 0;
	upgrade_type*ability = nullptr;
	int last_used_special = 0;
	int last_nuke = 0;
	xy siege_pos;
	int last_find_siege_pos = 0;
	bool siege_up_close = false;
	a_deque<xy> path;
	int last_find_path = 0;
	int strategy_busy_until = 0;
	int strategy_attack_until = 0;
	int strategy_run_until = 0;
	bool maybe_stuck = false;
	unit*stuck_target = nullptr;
	bool is_repairing = false;
	int registered_focus_fire_until = 0;
	int last_lurker_dodge = 0;
	int lurker_dodge_until = 0;
	int lurker_dodge_dir = 1;
	xy lurker_dodge_to;
	int last_lurker_dodge_cooldown = 0;
	double last_win_ratio = 0.0;
	int last_op_detectors = 0;
	int run_until = 0;
	bool is_nearest = false;
	bool is_nearest_weak = false;
	combat_unit* nearest_ally = nullptr;
	int retreat_until = 0;
	int try_retreat_until = 0;
	int use_for_drops_until = 0;
	xy drop_target;
	xy drop_move_target;
	bool force_attack = false;
	bool is_recharging = false;

	bool never_assign_to_aggressive_groups = false;
	control_spot* assigned_control_spot = nullptr;
	control_spot* assigned_defence_control_spot = nullptr;
	int last_default_move = 0;

	size_t num_in_enemy_tank_range = 0;

	int last_find_border_scout = 0;
	xy border_scout_pos;

	combat_unit* copy = nullptr;
	a_vector<combat_unit*> copying_units;

	unit* last_target = nullptr;
	int last_target_frame = 0;
	int last_target_in_range_frame = 0;
	double last_attack_enemy_army_supply = 0.0;
	double last_attack_win_ratio = 0.0;
	int last_spread_frame = 0;
	int last_attack_frame = 0;
	int attack_counter = 0;
	
	bool any_attacking = false;
	int stop_wait_frame = 0;
	int last_stop = 0;
	
	bool openbw_fight = false;
	xy last_run_pos;
	int last_hold_position = 0;
	
	int last_going_for_repair = 0;
	unit* going_for_repair_scv = nullptr;
};
a_unordered_map<unit*, combat_unit> combat_unit_map;

a_vector<combat_unit*> live_combat_units;
a_vector<combat_unit*> idle_combat_units;

refcounter<combat_unit> request_control(combat_unit* a) {
	if (a->reference_count) return nullptr;
	return refcounter<combat_unit>(*a);
}

void update_combat_units() {
	live_combat_units.clear();
	//size_t worker_count = 0;
	for (unit*u : my_units) {
		if (u->building) continue;
		if (!u->is_completed) continue;
		if (u->controller->action == unit_controller::action_build) continue;
		if (u->controller->action == unit_controller::action_scout) continue;
		if (u->type->is_non_usable) continue;
		combat_unit&c = combat_unit_map[u];
		if (!c.u) c.u = u;
		live_combat_units.push_back(&c);
	}
	idle_combat_units.clear();
	for (auto*cu : live_combat_units) {
		if (cu->action == combat_unit::action_idle) idle_combat_units.push_back(cu);
	}
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

double get_weapon_damage(unit* attacker, weapon_stats* w, unit* target) {
	double damage = w->damage;
	if (target->shields <= 0) damage *= get_damage_type_modifier(w->damage_type, target->stats->type->size);
	damage -= target->stats->armor;
	if (damage <= 0) damage = 1.0;
	damage *= w == attacker->stats->ground_weapon ? attacker->stats->ground_weapon_hits : attacker->stats->air_weapon_hits;
	return damage;
}

a_unordered_map<combat_unit*, int> pickup_taken;

bool give_lifts(combat_unit*dropship, a_vector<combat_unit*>&allies, int process_uid) {

	a_vector<combat_unit*> wants_a_lift;
	for (auto*cu : allies) {
		double path_time = unit_pathing_distance(cu->u, cu->goal_pos) / cu->u->stats->max_speed;
		double pickup_time = diag_distance(dropship->u->pos - cu->u->pos) / dropship->u->stats->max_speed;
		double air_time = diag_distance(cu->goal_pos - cu->u->pos) / dropship->u->stats->max_speed;
		if (cu->u->is_loaded) {
			if (cu->u->loaded_into == dropship->u && air_time < path_time) {
				cu->last_given_a_lift = current_frame;
			}
			//if (current_frame - cu->last_wanted_a_lift <= 15 * 5) {
			if (air_time <= path_time) {
				cu->last_wanted_a_lift = current_frame;
				wants_a_lift.push_back(cu);
				continue;
			} else {
				dropship->u->game_unit->unload(cu->u->game_unit);
			}
			if (path_time + 15 * 5 < pickup_time + air_time) continue;
		} else if (path_time <= (pickup_time + air_time) + 15 * 10) continue;
		log("%s : path_time %g pickup_time %g air_time %g\n", cu->u->type->name, path_time, pickup_time, air_time);
		wants_a_lift.push_back(cu);
		cu->last_wanted_a_lift = current_frame;
	}
	int space = dropship->u->type->space_provided;
	for (unit*lu : dropship->u->loaded_units) {
		space -= lu->type->space_required;
	}
	a_unordered_set<combat_unit*> units_to_pick_up;
	while (space > 0) {
		combat_unit*nu = get_best_score(wants_a_lift, [&](combat_unit*nu) {
			if (nu->u->type->is_flyer) return std::numeric_limits<double>::infinity();
			if (nu->u->is_loaded || units_to_pick_up.find(nu) != units_to_pick_up.end()) return std::numeric_limits<double>::infinity();
			if (nu->u->type->space_required > space) return std::numeric_limits<double>::infinity();
			if (pickup_taken[nu] == process_uid) return std::numeric_limits<double>::infinity();
			return diag_distance(nu->u->pos - dropship->u->pos);
		}, std::numeric_limits<double>::infinity());
		if (!nu) break;
		units_to_pick_up.insert(nu);
		space -= nu->u->type->space_required;
	}

	for (unit*u : dropship->u->loaded_units) {
		bool found = false;
		for (auto*cu : wants_a_lift) {
			if (cu->u == u) {
				found = true;
				break;
			}
		}
		if (!found) {
// 			log("unload because it doesn't want a lift\n");
// 			if (dropship->u->game_unit->unload(u->game_unit)) {
// 				dropship->u->controller->noorder_until = current_frame + 4;
// 			}
		}
	}

	//if (units_to_pick_up.empty()) return false;

	for (auto*cu : units_to_pick_up) {
		pickup_taken[cu] = process_uid;
		// todo: find the best spot to meet the dropship
		cu->subaction = combat_unit::subaction_move;
		cu->target_pos = dropship->u->pos;
	}

	combat_unit*pickup = get_best_score(units_to_pick_up, [&](combat_unit*cu) {
		return diag_distance(cu->u->pos - dropship->u->pos);
	});
	if (pickup) {
		pickup->last_given_a_lift = current_frame;
		pickup->u->controller->no_unload_until = current_frame + 15 * 5;
		dropship->subaction = combat_unit::subaction_move;
		dropship->target_pos = pickup->u->pos;
		if (diag_distance(pickup->u->pos - dropship->u->pos) <= 32 * 4) {
			dropship->subaction = combat_unit::subaction_idle;
			dropship->u->controller->action = unit_controller::action_idle;
			if (current_frame >= dropship->u->controller->noorder_until) {
				pickup->u->game_unit->rightClick(dropship->u->game_unit);
				pickup->u->controller->noorder_until = current_frame + 15;
				dropship->u->controller->noorder_until = current_frame + 4;
			}
		}
	} else {
		xy goal_pos;
		for (auto*cu : allies) {
			if (cu->u->loaded_into == dropship->u) {
				cu->last_given_a_lift = current_frame;
				cu->u->controller->no_unload_until = current_frame + 15 * 5;
				goal_pos = cu->goal_pos;
				if (diag_distance(goal_pos - dropship->u->pos) <= 32) {
					//dropship->u->game_unit->unload(cu->u->game_unit);
					dropship->u->game_unit->unloadAll();
				}
			}
		}
		if (goal_pos != xy()) {
			dropship->subaction = combat_unit::subaction_move;
			dropship->target_pos = goal_pos;
		} else return false;
	}

	return true;
}

struct group_t {
	size_t idx;
	double value;
	a_vector<unit*> enemies;
	a_vector<combat_unit*> allies;
	tsc::dynamic_bitset threat_area;
	bool is_defensive_group = false;
	bool is_aggressive_group = false;
	bool is_main_army = false;
	bool is_near_desired_expansion = false;
	double ground_dpf = 0.0;
	double air_dpf = 0.0;
#ifdef USE_COMBAT_EVAL2
	bool fight = false;
	double fight_my_resources_lost = 0.0;
	double fight_op_resources_lost = 0.0;
#endif
	bool is_done = false;
	double my_total_hp = 0.0;
	
	int n_air_enemies = 0;
	int n_ground_enemies = 0;
};
a_vector<group_t> groups;
multitasking::mutex groups_mut;
tsc::dynamic_bitset entire_threat_area;
tsc::dynamic_bitset entire_threat_area_edge;

tsc::dynamic_bitset transfer_unsafe_area;
tsc::dynamic_bitset my_units_area;

void update_group_area(group_t&g) {

	double my_army_supply = current_used_total_supply - my_workers.size();
	double enemy_army_supply = 0.0;
	for (unit*u : enemy_units) {
		if (u->gone || u->type->is_worker) continue;
		enemy_army_supply += u->type->required_supply;
	}

	g.threat_area.reset();
	g.n_air_enemies = 0;
	g.n_ground_enemies = 0;
	for (unit*e : g.enemies) {
		if (e->is_flying) ++g.n_air_enemies;
		else ++g.n_ground_enemies;
		if (e->type == unit_types::overlord) continue;
		//if (my_army_supply >= 40.0 && enemy_army_supply >= my_army_supply * 0.5) {
		if (true) {
			if (e->building && !e->stats->air_weapon && !e->stats->ground_weapon && e->type != unit_types::bunker) continue;
		}
		tsc::dynamic_bitset visited(grid::build_grid_width*grid::build_grid_height);
		a_deque<xy> walk_open;
		a_deque<std::tuple<xy, xy>> threat_open;
		double threat_radius = 32;
		if (e->stats->ground_weapon && e->stats->ground_weapon->max_range > threat_radius) threat_radius = e->stats->ground_weapon->max_range;
		if (e->stats->air_weapon && e->stats->air_weapon->max_range > threat_radius) threat_radius = e->stats->air_weapon->max_range;
		if (e->type == unit_types::bunker) threat_radius = 32 * 6;
		threat_radius += e->type->width;
		threat_radius += 64;
		if (e->type == unit_types::siege_tank_siege_mode) threat_radius += 128;
		if (e->type == unit_types::dragoon) threat_radius += 32;
		//if (e->type == unit_types::interceptor) threat_radius = 32;
		//if (e->type == unit_types::interceptor) continue;
		if (e->type == unit_types::carrier) threat_radius = 32 * 10;
		if (e->type == unit_types::mutalisk) threat_radius += 64;
		if (e->type == unit_types::zergling) threat_radius += 64;
		if (e->type == unit_types::corsair) threat_radius += 64;
		if (e->type == unit_types::reaver) threat_radius += 96;
		//double walk_radius = e->stats->max_speed * 15 * 2;
		double walk_radius = e->stats->max_speed * 20;
		//double walk_radius = 32;
		walk_open.emplace_back(e->pos);
		visited.set(grid::build_square_index(e->pos));
		while (!walk_open.empty() || !threat_open.empty()) {
			xy pos;
			xy origin;
			bool is_walk = !walk_open.empty();
			if (is_walk) {
				pos = walk_open.front();
				walk_open.pop_front();
			} else {
				std::tie(pos, origin) = threat_open.front();
				threat_open.pop_front();
			}
			g.threat_area.set(grid::build_square_index(pos));
			for (int i = 0; i < 4; ++i) {
				xy npos = pos;
				if (i == 0) npos.x += 32;
				if (i == 1) npos.y += 32;
				if (i == 2) npos.x -= 32;
				if (i == 3) npos.y -= 32;
				if ((size_t)npos.x >= (size_t)grid::map_width || (size_t)npos.y >= (size_t)grid::map_height) continue;
				size_t index = grid::build_square_index(npos);
				if (visited.test(index)) continue;
				visited.set(index);
				double walk_d = (npos - e->pos).length();
				if (is_walk) {
					bool out_of_range = !e->type->is_flyer && !grid::get_build_square(npos).entirely_walkable;
					if (!out_of_range) out_of_range = walk_d >= walk_radius;
					if (out_of_range) threat_open.emplace_back(npos, npos);
					else walk_open.emplace_back(npos);
				} else {
					double d = (npos - origin).length();
					if (d < threat_radius) {
						threat_open.emplace_back(npos, origin);
					}
				}
			}
		}
	}
}

a_deque<xy> find_bigstep_path(unit_type*ut, xy from, xy to, square_pathing::pathing_map_index index) {
	if (ut->is_flyer) {
		return flyer_pathing::find_bigstep_path(from, to);
	} else {
		auto path = square_pathing::find_path(square_pathing::get_pathing_map(ut, index), from, to);
		a_deque<xy> r;
		for (auto*n : path) r.push_back(n->pos);
		return r;
	}
};

// struct air_node {
// 	xy pos;
// 	a_vector<air_node*> neighbors;
// 	double cost;
// 
// 	bool temp_visited = false;
// 	air_node* temp_prev = nullptr;
// 	double temp_distance = 0.0;
// 	double temp_est_distance = 0.0;
// };
// 
// a_list<air_node> all_air_nodes;
// 
// void create_air_nodes() {
// 
// 	int spacing = 32 * 8;
// 	double max_neighbor_distance = std::sqrt(spacing * spacing + spacing * spacing) + spacing * 0.25;
// 
// 	a_list<air_node> new_air_nodes;
// 	for (int y = 0; y < grid::map_height; y += spacing) {
// 		for (int x = 0; x < grid::map_width; x += spacing) {
// 			a_vector<air_node*> neighbors;
// 			xy pos(x, y);
// 			for (auto& v : new_air_nodes) {
// 				if ((pos - v.pos).length() < max_neighbor_distance) neighbors.push_back(&v);
// 			}
// 			air_node n;
// 			n.pos = pos;
// 			n.neighbors = std::move(neighbors);
// 			n.cost = 0.0;
// 			new_air_nodes.push_back(std::move(n));
// 		}
// 	}
// 
// 	all_air_nodes = std::move(new_air_nodes);
// }
// 
// air_node* find_nearest_air_node(xy pos) {
// 	return get_best_score_p(all_air_nodes, [&](air_node* n) {
// 		return diag_distance(n->pos - pos);
// 	});
// }
// 
// template<typename pred_F, typename est_dist_F, typename goal_F>
// a_deque<xy> find_air_node_path(air_node* src, pred_F&& pred, est_dist_F&& est_dist, goal_F&& goal) {
// 	a_deque<xy> r;
// 
// 	struct open_cmp {
// 		bool operator()(air_node* a, air_node* b) const {
// 			return a->temp_est_distance < b->temp_est_distance;
// 		}
// 	};
// 
// 	std::priority_queue<air_node*, a_vector<air_node*>, open_cmp> open;
// 	a_vector<air_node*> visited;
// 
// 	src->temp_visited = true;
// 	src->temp_prev = nullptr;
// 	src->temp_distance = 0.0;
// 	src->temp_est_distance = 0.0;
// 	open.push(src);
// 	visited.push_back(src);
// 
// 	while (!open.empty()) {
// 
// 		air_node* cn = open.top();
// 		if (goal(cn)) {
// 			for (air_node* n = cn; n; n = n->temp_prev) {
// 				r.push_front(n->pos);
// 			}
// 			break;
// 		}
// 		open.pop();
// 
// 		for (air_node* n : cn->neighbors) {
// 			if (n->temp_visited) continue;
// 			if (!pred(n)) {
// 				n->temp_visited = true;
// 				visited.push_back(n);
// 				continue;
// 			}
// 			double d = diag_distance(n->pos - cn->pos);
// 
// 			double distance = cn->temp_distance + d;
// 			double est_distance = distance + est_dist(n);
// 			n->temp_visited = true;
// 			n->temp_prev = cn;
// 			n->temp_distance = distance;
// 			n->temp_est_distance = est_distance;
// 			open.push(n);
// 			visited.push_back(n);
// 		}
// 
// 	}
// 
// 	for (air_node* n : visited) {
// 		n->temp_visited = false;
// 	}
// 
// 	return r;
// }

bool can_transfer_through(xy pos) {
	size_t index = grid::build_square_index(pos);
	if (!entire_threat_area.test(index)) return true;
	for (auto& g : groups) {
 		if (g.ground_dpf < 0.5) continue;
 		if (g.threat_area.test(index) && transfer_unsafe_area.test(index)) return false;
 	}
	//if (transfer_unsafe_area.test(index)) return false;
	return true;
}

bool is_in_defence_choke_outside_squares(xy pos) {
	size_t index = grid::build_square_index(pos);
	return defence_choke_outside_bitset.test(index);
}

bool no_aggressive_groups = false;
bool aggressive_groups_done_only = false;
bool aggressive_wraiths = false;
bool aggressive_vultures = true;
bool aggressive_valkyries = false;
bool aggressive_zerglings = false;
bool aggressive_mutalisks = false;
bool aggressive_corsairs = false;
bool aggressive_dark_templars = false;
bool aggressive_zealots = false;

bool attack_bases = false;

bool dont_attack_main_army = false;

bool use_map_split_defence = true;

bool hide_wraiths = false;
bool no_scout_around = false;

int defensive_spider_mine_count = 0;

bool use_control_spots = false;
bool use_control_spots_non_terran = false;

bool use_decisive_engagements = true;
bool defend_in_control_spots = false;

bool search_and_destroy = false;
int scans_available = 0;

void update_groups() {

	a_vector<group_t> new_groups;
	
	bool we_have_any_anti_air = false;
	for (unit* u : my_units) {
		if (!u->building && u->stats->air_weapon) {
			we_have_any_anti_air = true;
			break;
		}
	}

	a_vector<std::tuple<double, unit*>> sorted_enemy_units;
	for (unit*e : enemy_units) {
		if (e->gone) continue;
		if (e->stasis_timer) continue;
		if (e->is_flying && !we_have_any_anti_air) continue;
		double d = get_best_score_value(my_units, [&](unit*u) {
			if (u->type->is_non_usable) return std::numeric_limits<double>::infinity();
			return diag_distance(e->pos - u->pos);
		}, std::numeric_limits<double>::infinity());
		sorted_enemy_units.emplace_back(d, e);
	}
	if (true) {
		log("rescuable_units.size() is %d\n", rescuable_units.size());
		for (unit*e : rescuable_units) {
			if (e->gone) continue;
			double d = get_best_score_value(my_units, [&](unit*u) {
				if (u->type->is_non_usable) return std::numeric_limits<double>::infinity();
				return diag_distance(e->pos - u->pos);
			}, std::numeric_limits<double>::infinity());
			sorted_enemy_units.emplace_back(d, e);
		}
	}
	std::sort(sorted_enemy_units.begin(), sorted_enemy_units.end());

	search_and_destroy = false;
	if (current_used_total_supply >= 150) {
		int enemy_non_building_units = 0;
		int enemy_building_units = 0;
		for (unit*e : enemy_units) {
			if (e->gone) continue;
			if (current_frame - e->last_seen >= 15 * 60 * 10) continue;
			if (e->building) ++enemy_building_units;
			else ++enemy_non_building_units;
		}
		if (enemy_non_building_units == 0) no_aggressive_groups = false;
		if (enemy_non_building_units == 0 && enemy_building_units == 0) search_and_destroy = true;
	}
	if (current_frame >= 15 * 60 * 60) {
		no_aggressive_groups = false;
		aggressive_groups_done_only = false;
	}

	log(log_level_test, "no_aggressive_groups is %d\n", no_aggressive_groups);
	log("aggressive_groups_done_only is %d\n", aggressive_groups_done_only);
	unit*first_enemy_building = nullptr;
	for (auto&v : sorted_enemy_units) {
		unit*e = std::get<1>(v);
		//if (e->type->is_worker) continue;
		if (e->gone) continue;
		if (e->invincible) continue;
		if (e->type->is_non_usable) continue;
		if (my_detector_units.size() < 4) {
			if (e->type == unit_types::observer && !my_base.test(grid::build_square_index(e->pos)) && !e->detected) continue;
			if (e->type == unit_types::observer && !e->detected && scans_available <= 2) continue;
		}
		if (e->type == unit_types::larva || e->type == unit_types::egg) continue;
		if (e->building) first_enemy_building = e;
		bool is_aggressive = true;
		bool is_near_desired_expansion = false;
		if (players::my_player->race == race_terran) {
			if (use_map_split_defence) {
				if (!e->type->is_worker && (current_used_total_supply >= 80 || players::opponent_player->race == race_terran) && map_split_my_area.test(grid::build_square_index(e->pos))) is_aggressive = false;
			}
		} else if (players::my_player->race == race_zerg) {
			if (use_map_split_defence) {
				if (map_split_my_area.test(grid::build_square_index(e->pos))) is_aggressive = false;
			} else {
				for (unit* u : my_buildings) {
					weapon_stats* w = u->is_flying ? e->stats->air_weapon : e->stats->ground_weapon;
					if (w && units_distance(e, u) <= w->max_range) {
						is_aggressive = false;
						break;
					}
				}
				if (is_aggressive) {
					for (unit* u : my_workers) {
						if (u->controller->action == unit_controller::action_scout) continue;
						weapon_stats* w = u->is_flying ? e->stats->air_weapon : e->stats->ground_weapon;
						if (w && units_distance(e, u) <= w->max_range) {
							is_aggressive = false;
							break;
						}
					}
				}
				if (is_aggressive) {
					for (auto& b : build::build_tasks) {
						if (b.built_unit) continue;
						if (b.build_pos != xy() && b.type->unit && b.type->unit->is_resource_depot) {
							if (diag_distance(b.build_pos - e->pos) <= 32 * 20) {
								is_near_desired_expansion = true;
								is_aggressive = false;
							}
						}
					}
				}
			}
		}
		//if (no_aggressive_groups) {
// 		if (true) {
// 			if (e->visible) {
// 				double nearest_building_distance = get_best_score_value(my_units, [&](unit*u) {
// 					if (!u->building) return std::numeric_limits<double>::infinity();
// 					return units_pathing_distance(unit_types::scv, u, e);
// 				});
// 				bool any_units_in_attack_range = false;
// 				double nearest_unit_distance = get_best_score_value(my_units, [&](unit*u) {
// 					if (u->type->is_non_usable) return std::numeric_limits<double>::infinity();
// 					if (u->controller->action == unit_controller::action_scout) return std::numeric_limits<double>::infinity();
// 					if (u->building) return std::numeric_limits<double>::infinity();
// 					weapon_stats*w = u->is_flying ? e->stats->air_weapon : e->stats->ground_weapon;
// 					double d = units_pathing_distance(u, e);;
// 					if (w && d <= w->max_range) any_units_in_attack_range = true;
// 					return d;
// 				});
// 				if (nearest_building_distance < 32 * 25) {
// 					if (nearest_building_distance <= nearest_unit_distance || any_units_in_attack_range) {
// 						is_aggressive = false;
// 					}
// 				}
// 				if (is_aggressive) {
// 					any_units_in_attack_range = false;
// 					double nearest_worker_distance = get_best_score_value(my_workers, [&](unit*u) {
// 						if (u->controller->action == unit_controller::action_scout) return std::numeric_limits<double>::infinity();
// 						weapon_stats*w = u->is_flying ? e->stats->air_weapon : e->stats->ground_weapon;
// 						double d = units_pathing_distance(u, e);;
// 						if (w && d <= w->max_range + 128) any_units_in_attack_range = true;
// 						return d;
// 					});
// 					if (any_units_in_attack_range) is_aggressive = false;
// 				}
// 				if (diag_distance(defence_choke.center - e->pos) <= 32 * 15) is_aggressive = false;
// 				if (is_aggressive) {
// 					for (auto&v : inter_base_paths) {
// 						for (xy pos : v) {
// 							if (diag_distance(pos - e->pos) <= 32 * 15) {
// 								is_aggressive = false;
// 								break;
// 							}
// 							if (!is_aggressive) break;
// 						}
// 					}
// 				}
// 			}
// 		}
		//if (!buildpred::attack_now && op_base.test(grid::build_square_index(e->pos))) continue;
		group_t*g = nullptr;
		auto get_wd = [&](unit* u) {
			if (u->type == unit_types::bunker) return 32.0 * 5;
			if (u->type == unit_types::carrier) return 32.0 * 12;
			if (u->type == unit_types::reaver) return 32.0 * 11;
			double r = 0.0;
			if (u->stats->air_weapon) r = u->stats->air_weapon->max_range;
			if (u->stats->ground_weapon && u->stats->ground_weapon->max_range > r) r = u->stats->ground_weapon->max_range;
			r += u->stats->max_speed * 20;
			return r + 64;
		};
		double wd = get_wd(e); 
		for (auto&v : new_groups) {
			unit* ne = v.enemies.front();
			double d = units_distance(e, ne);
			//double max_d = 32 * 15;
			double max_d = 32 * 25;
			//double max_d = std::max(wd, get_wd(ne));
			//if (e->type == unit_types::bunker) max_d = 32 * 5;
			if (d >= max_d) continue;
// 			unit_type*ut = e->type;
// 			if (ut->is_building) ut = unit_types::scv;
// 			if (!square_pathing::unit_can_reach(ut, e->pos, ne->pos)) continue;
			g = &v;
			break;
		}
		if (!g) {
			new_groups.emplace_back();
			g = &new_groups.back();
			g->threat_area.resize(grid::build_grid_width*grid::build_grid_height);
			g->is_defensive_group = false;
			g->is_aggressive_group = true;
		}
		g->enemies.push_back(e);
		if (!is_aggressive) g->is_aggressive_group = false;
		if (is_near_desired_expansion) g->is_near_desired_expansion = true;
	}
	//if (!live_units.empty() && (new_groups.empty() || (no_aggressive_groups || aggressive_groups_done_only))) {
	//	if (defence_choke.center != xy()) {
	
	for (auto&g : new_groups) {
		int buildings = 0;
		bool just_workers = true;
		for (unit*e : g.enemies) {
			//if (e->building && !e->stats->air_weapon && !e->stats->ground_weapon && e->type != unit_types::bunker && e->type != unit_types::pylon && current_frame >= e->strategy_high_priority_until) ++buildings;
			if ((e->building || e->type == unit_types::overlord) && !e->stats->air_weapon && !e->stats->ground_weapon && e->type != unit_types::bunker && current_frame >= e->strategy_high_priority_until) ++buildings;
			else if (!e->type->is_worker) just_workers = false;
		}
		if ((size_t)buildings != g.enemies.size() && !just_workers) {
			for (auto i = g.enemies.begin(); i != g.enemies.end();) {
				unit*e = *i;
				//if (e->building && !e->stats->air_weapon && !e->stats->ground_weapon && e->type != unit_types::bunker && e->type != unit_types::pylon && current_frame >= e->strategy_high_priority_until) i = g.enemies.erase(i);
				if ((e->building || e->type == unit_types::overlord) && !e->stats->air_weapon && !e->stats->ground_weapon && e->type != unit_types::bunker && current_frame >= e->strategy_high_priority_until) i = g.enemies.erase(i);
				else ++i;
			}
		}
	}

	entire_threat_area.reset();
	entire_threat_area_edge.reset();
	for (auto&g : new_groups) {
		update_group_area(g);
		if ((g.threat_area & my_base_defence).any()) {
			g.is_aggressive_group = false;
		}
	}

	if (!live_units.empty()) {
		bool create_defensive_group = true;
		for (auto& g : new_groups) {
			if (g.is_aggressive_group) continue;
			double enemy_supply = 0.0;
			for (unit* e : g.enemies) {
				enemy_supply += e->type->required_supply;
			}
			if (enemy_supply >= 8.0) {
				create_defensive_group = false;
				break;
			}
		}
		if (create_defensive_group) {
			new_groups.emplace_back();
			group_t*g = &new_groups.back();
			g->threat_area.resize(grid::build_grid_width*grid::build_grid_height);
			if (first_enemy_building) g->enemies.push_back(first_enemy_building);
			else {
				g->enemies.push_back(get_best_score(live_units, [&](unit*u) {
					if (u->is_flying) return 2;
					if (u->owner != players::my_player) return 0;
					return 1;
				}));
			}
			g->is_defensive_group = true;
		}
	}

	group_t* largest_army_group = nullptr;
	double largest_army_supply = 0.0;
	for (auto&g : new_groups) {
		double ground_dpf = 0.0;
		double air_dpf = 0.0;
		int worker_count = 0;
		for (unit*e : g.enemies) {
			if (e->type->is_worker) ++worker_count;
		}
		double army_supply = 0.0;
		for (unit*e : g.enemies) {
			if (e->type->is_worker && worker_count < 4) continue;
			if (e->stats->ground_weapon) {
				ground_dpf += e->stats->ground_weapon->damage*e->stats->ground_weapon_hits / e->stats->ground_weapon->cooldown;
			}
			if (e->stats->air_weapon) {
				air_dpf += e->stats->air_weapon->damage*e->stats->air_weapon_hits / e->stats->air_weapon->cooldown;
			}
			if (!e->type->is_worker) army_supply += e->type->required_supply;
			if (e->type == unit_types::bunker) {
				army_supply += 8.0;
				ground_dpf += 1.5;
				air_dpf += 1.5;
			}
		}
		g.ground_dpf = ground_dpf;
		g.air_dpf = air_dpf;
		if (army_supply > largest_army_supply) {
			largest_army_supply = army_supply;
			largest_army_group = &g;
		}
		double my_total_hp = 0.0;
		for (auto* a : g.allies) {
			my_total_hp += a->u->hp + a->u->shields;
		}
		g.my_total_hp = my_total_hp;
	}
	if (largest_army_group) largest_army_group->is_main_army = true;
	
	for (auto&g : new_groups) {
		entire_threat_area |= g.threat_area;
	}

	transfer_unsafe_area = entire_threat_area;
	my_units_area.reset();
	for (unit* u : my_units) {
		if (!u->stats->ground_weapon && !u->stats->air_weapon) continue;
		if (u->type->is_worker) continue;
		if (u->type->is_non_usable) continue;
		int r = 32 * 6;
		int ux = u->pos.x;
		int uy = u->pos.y;
		for (int iy = -r; iy <= r; iy += 32) {
			for (int ix = -r; ix <= r; ix += 32) {
				int x = ux + ix;
				int y = uy + iy;
				if ((size_t)x < (size_t)grid::map_width && (size_t)y < (size_t)grid::map_height) {
					size_t index = grid::build_square_index(xy(x, y));
					my_units_area.set(index);
					transfer_unsafe_area.reset(index);
				}
			}
		}
	}
// 	for (auto i = new_groups.begin(); i != new_groups.end();) {
// 		auto&g = *i;
// 		bool merged = false;
// 		for (auto&g2 : new_groups) {
// 			if (&g2 == &g) break;
// 			if ((g.threat_area&g2.threat_area).none()) continue;
// 			for (unit*u : g.enemies) {
// 				g2.enemies.push_back(u);
// 			}
// 			update_group_area(g2);
// 			merged = true;
// 			break;
// 		}
// 		if (merged) i = new_groups.erase(i);
// 		else ++i;
// 	}
//	for (auto&g : new_groups) {
//		entire_threat_area |= g.threat_area;
//	}
	for (size_t idx : entire_threat_area) {
		size_t xidx = idx % (size_t)grid::build_grid_width;
		size_t yidx = idx / (size_t)grid::build_grid_width;
//		auto is_something_there = [&](size_t x, size_t y) {
//			auto& bs = grid::get_build_square(xy(x * 32, y * 32));
//			if (!bs.entirely_walkable) return true;
//			if (bs.building) return true;
//			return false;
//		};
		auto test = [&](size_t index) {
			if (entire_threat_area.test(index)) return;
// 			size_t test_x = index % (size_t)grid::build_grid_width;
// 			size_t test_y = index / (size_t)grid::build_grid_width;
// 			if (is_something_there(test_x, test_y)) return;
// 			if (test_x != 0 && is_something_there(test_x - 1, test_y)) return;
// 			if (test_x != grid::build_grid_last_x && is_something_there(test_x + 1, test_y)) return;
// 			if (test_y != 0 && is_something_there(test_x, test_y - 1)) return;
// 			if (test_y != grid::build_grid_last_y && is_something_there(test_x, test_y + 1)) return;
			entire_threat_area_edge.set(index);
		};
		if (xidx != grid::build_grid_last_x) test(idx + 1);
		if (yidx != grid::build_grid_last_y) test(idx + grid::build_grid_width);
		if (xidx != 0) test(idx - 1);
		if (yidx != 0) test(idx - grid::build_grid_width);
	}

	for (auto&g : new_groups) {
		double value = 0.0;
		//if (no_aggressive_groups && !g.is_aggressive_group) value += 100000.0;
		if (!g.is_aggressive_group) value += 100000.0;
		if (g.is_near_desired_expansion) value += 100000.0;
		if (!no_aggressive_groups && g.enemies.size() == 1 && g.enemies.front()->type->is_worker) {
			value -= 100000000.0;
		}
		bool has_resource_depot = false;
		for (unit*e : g.enemies) {
			//if (e->type->is_worker) continue;
			bool is_attacking_my_workers = e->order_target && e->order_target->owner == players::my_player && e->order_target->type->is_worker;
			if (is_attacking_my_workers) {
				if (!my_base.test(grid::build_square_index(e->order_target->pos))) is_attacking_my_workers = false;
			}
			bool is_attacking_my_base = e->order_target && my_base.test(grid::build_square_index(e->order_target->pos));
			if (is_attacking_my_base || is_attacking_my_workers) e->high_priority_until = current_frame + 15 * 15;

// 			if (e->high_priority_until >= current_frame || e->owner->rescuable) {
// 				value += e->minerals_value;
// 				value += e->gas_value * 2;
// 				value += 1000;
// 			} else {
// 				if (e->type->is_worker) value += 200;
// 				value -= e->minerals_value;
// 				value -= e->gas_value * 2;
// 			}
			if (e->type == unit_types::photon_cannon || e->type == unit_types::sunken_colony || e->type == unit_types::spore_colony) value -= 200.0;
			if (!e->type->is_worker) {
				value -= e->minerals_value;
				value -= e->gas_value * 2;
			}
			if (g.is_aggressive_group && attack_bases) {
				//if (e->type->is_worker) value += 500.0;
				//if (e->type->is_resource_depot) value += 4000.0;
			}
			if (e->type->is_resource_depot) has_resource_depot = true;

			//if (e->type->is_building) value /= 4;
			//if (my_base.test(grid::build_square_index(e->pos))) value *= 4;
			//if (my_base.test(grid::build_square_index(e->pos))) value *= 4;
			//if (op_base.test(grid::build_square_index(e->pos))) value /= 4;
			if (my_base.test(grid::build_square_index(e->pos))) value += 1000.0;

			if (my_base.test(grid::build_square_index(e->pos))) value += 1000.0 * 1000.0;
			
			if (e->type == unit_types::shuttle || e->type == unit_types::arbiter) {
				if (g.enemies.size() < 10) value += 1000.0 * 1000.0;
			}
		}
		if (attack_bases && has_resource_depot) value += 4000.0;
// 		unit*nb = get_best_score(my_buildings, [&](unit*u) {
// 			return unit_pathing_distance(unit_types::scv, u->pos, g.enemies.front()->pos);
// 		});
// 		if (nb) {
// 			for (auto*n : square_pathing::find_path(square_pathing::get_pathing_map(unit_types::scv), nb->pos, g.enemies.front()->pos)) {
// 				for (unit*e : enemy_units) {
// 					if (e->gone) continue;
// 					if (e->type->is_non_usable) continue;
// 					weapon_stats*w = e->stats->ground_weapon;
// 					if (!w) continue;
// 					if (diag_distance(e->pos - n->pos) <= w->max_range) {
// 						value -= e->minerals_value;
// 						value -= e->gas_value;
// 					}
// 				}
// 			}
// 		}
		g.value = value;
	}
	std::sort(new_groups.begin(), new_groups.end(), [&](const group_t&a, const group_t&b) {
		return a.value > b.value;
	});
	for (size_t i = 0; i < new_groups.size(); ++i) {
		new_groups[i].idx = i;
	}

//	for (auto&g : new_groups) {
//		if (true) {
//			a_map<unit_type*, int> counts;
//			for (unit*e : g.enemies) ++counts[e->type];
//			log(log_level_test, "group value %g - enemies -", g.value);
//			for (auto&v : counts) {
//				log(log_level_test, "%dx%s ", std::get<1>(v), std::get<0>(v)->name);
//			}
//			if (g.is_aggressive_group) log(log_level_test, " (aggressive)");
//			if (g.is_defensive_group) log(log_level_test, " (defensive)");
//			if (g.is_main_army) log(log_level_test, " (main army)");
//			log(log_level_test, "\n");
//		}
//	}

	a_unordered_set<combat_unit*> available_units;
	for (auto*c : live_combat_units) {
		if (c->action == combat_unit::action_tactic) continue;
		if (current_frame < c->strategy_busy_until) continue;
		if (c->reference_count) continue;
		if (c->u->type == unit_types::overlord) continue;
		available_units.insert(c);
	}

// 	if (false) {
// 		a_deque<std::tuple<double, combat_unit*>> workers;
// 		for (auto*c : available_units) {
// 			if (!c->u->type->is_worker || c->u->force_combat_unit) continue;
// 			double d = get_best_score_value(enemy_units, [&](unit*e) {
// 				if (e->gone) return std::numeric_limits<double>::infinity();
// 				if (e->type->is_non_usable) return std::numeric_limits<double>::infinity();
// 				weapon_stats*w = c->u->is_flying ? e->stats->air_weapon : e->stats->ground_weapon;
// 				if (!w) return std::numeric_limits<double>::infinity();
// 				return diag_distance(c->u->pos - e->pos);
// 			});
// 			workers.emplace_back(d, c);
// 		}
// 		std::sort(workers.begin(), workers.end());
// 		for (int i = 0; i < 8 && !workers.empty(); ++i) workers.pop_front();
// 		for (auto&v : workers) {
// 			auto*c = std::get<1>(v);
// 			if (c->u->controller->action != unit_controller::action_gather && c->u->controller->action != unit_controller::action_build && c->u->controller->action != unit_controller::action_scout) {
// 				c->u->controller->action = unit_controller::action_idle;
// 			}
// 			available_units.erase(c);
// 		}
// 	}

	
	a_unordered_map<combat_unit*, a_unordered_set<group_t*>> can_reach_group;
	a_map<size_t, int> can_reach_count;
	for (auto*c : available_units) {
		auto&c_can_reach = can_reach_group[c];
		int reachable_non_defensive_groups = 0;
		for (auto&g : new_groups) {
			bool okay = true;
			size_t count = 0;
			if (!c->u->is_flying) {
			//if (true) {
				auto path = find_bigstep_path(c->u->type, c->u->pos, g.enemies.front()->pos, square_pathing::pathing_map_index::no_enemy_buildings);
				if (path.empty()) okay = false;
				if (!path.empty()) path.pop_back();
				for (xy pos : path) {
					size_t index = grid::build_square_index(pos);
					if (entire_threat_area.test(index)) {
						bool found = false;
						for (auto&g2 : new_groups) {
							if (&g2 == &g) continue;
							if (g2.is_defensive_group) continue;
							for (unit*e : g2.enemies) {
								double wr = 0.0;
								if (e->type == unit_types::bunker) {
									wr = 32 * 5;
								} else {
									weapon_stats*w = c->u->is_flying ? e->stats->air_weapon : e->stats->ground_weapon;
									if (!w) continue;
									wr = w->max_range;
								}
								double d = diag_distance(pos - e->pos);
								//log("%s - %d - d to %s is %g\n", c->u->type->name, count, e->type->name, d);
								if (d <= wr || d <= 32 * 10) {
									//log("%s - %d - -> %d bumped into %d (%s %g)\n", c->u->type->name, count, g.idx, g2.idx, e->type->name, d);
									found = true;
									okay = false;
									break;
								}
							}
							if (found) break;
						}
						if (found) break;
					}
					++count;
				}
			}
			multitasking::yield_point();
			if (g.is_defensive_group) {
				double d = get_best_score_value(enemy_units, [&](unit*e) {
					if (!e->visible) return std::numeric_limits<double>::infinity();
					if (e->type->is_non_usable) return std::numeric_limits<double>::infinity();
					return diag_distance(c->u->pos - e->pos);
				});
				if (d > 32 * 20) {
					okay = true;
				}
			} else if (okay) ++reachable_non_defensive_groups;
			//if (okay) log("%s can reach %d\n", c->u->type->name, g.idx);
			//else log("%s can not reach %d\n", c->u->type->name, g.idx);
			if (okay) c_can_reach.insert(&g);
			multitasking::yield_point();
		}
		if (reachable_non_defensive_groups == 0) {
			c->maybe_stuck = true;
			//log("no reachable non defensive groups\n");
			for (auto&g : new_groups) {
				c_can_reach.insert(&g);
			}
		} else {
			if (c->maybe_stuck) c->maybe_stuck = false;
		}
		for (auto*g : c_can_reach) {
			++can_reach_count[g->idx];
		}
	}
	for (auto&v : can_reach_count) {
		log("group %d: reachable by %d\n", v.first, v.second);
	}

	for (auto i = available_units.begin(); i != available_units.end();) {
		auto*cu = *i;
		size_t index = grid::build_square_index(cu->u->pos);
		group_t* inside_group = nullptr;
		for (auto&g : new_groups) {
			//if (cu->u->stats->air_weapon || cu->u->stats->ground_weapon) {
			if (!no_aggressive_groups && g.enemies.size() == 1 && g.enemies.front()->type->is_worker) {
				if (units_distance(g.enemies.front(), cu->u) > 64) continue;
			}
			if (true) {
				bool can_attack_any = false;
				bool any_can_attack_me = false;
				bool any_can_attack = false;
				bool any_buildings = false;
				//bool any_facing_me = false;
				for (unit* e : g.enemies) {
					if (e->building && !any_buildings) any_buildings = true;
					if (cu->u->type->is_worker) {
						if (!square_pathing::unit_can_reach(cu->u, cu->u->pos, e->pos, square_pathing::pathing_map_index::include_liftable_wall)) continue;
					}
					//if (e->speed > 2) {
// 					if (true) {
// 						weapon_stats*ew = cu->u->is_flying ? e->stats->air_weapon : e->stats->ground_weapon;
// 						if (ew && units_distance(cu->u, e) > ew->max_range + 32) {
// 							xy relpos = cu->u->pos - e->pos;
// 							double ang = e->heading - std::atan2(relpos.y, relpos.x);
// 							if (ang < -PI) ang += PI * 2;
// 							if (ang > PI) ang -= PI * 2;
// 							if (std::abs(ang) > PI / 2) continue;
// 						}
// 					}
					weapon_stats*w = e->is_flying ? cu->u->stats->air_weapon : cu->u->stats->ground_weapon;
					if (w) can_attack_any = true;
					weapon_stats*ew = cu->u->is_flying ? e->stats->air_weapon : e->stats->ground_weapon;
					if (ew) any_can_attack_me = true;
					if (e->stats->air_weapon || e->stats->ground_weapon) any_can_attack = true;
//					if (e->speed < 3.0) {
//						if (ew && units_distance(cu->u, e) <= ew->max_range) any_facing_me = true;
//					} else if ((e->pos + xy(e->hspeed, e->vspeed) - cu->u->pos).length() < (e->pos - cu->u->pos).length() - 3.0) {
//						any_facing_me = true;
//					}
					//if (can_attack_any && any_can_attack_me && any_facing_me) break;
					if (can_attack_any && any_can_attack_me) break;
				}
				//if (!cu->u->type->is_worker && !any_facing_me) continue;
				if (!can_attack_any && !any_can_attack_me) continue;
				if (!any_can_attack && !any_buildings) continue;
//				if (cu->u->stats->ground_weapon || cu->u->stats->air_weapon) {
//					if (cu->u->is_flying && !can_attack_any) continue;
//					if (!cu->u->is_flying && !can_attack_any) continue;
//				}
				if (cu->u->type->is_worker && !can_attack_any) continue;
				//if (!can_attack_any && (cu->u->stats->ground_weapon || cu->u->stats->air_weapon) && g.allies.size() >= 12) continue;
// 				if (current_used_total_supply >= 40) {
// 					if (cu->u->type->is_worker && !any_can_attack_me) continue;
// 				}

//				if (!can_attack_any) {
//					if ((cu->u->is_flying ? g.air_dpf : g.ground_dpf) < (cu->u->hp + cu->u->shields) / 90) continue;
//					if ((cu->u->is_flying ? g.air_dpf : g.ground_dpf) < g.my_total_hp / (15 * 30)) continue;
//				}
			}
			if (g.threat_area.test(index)) {
				inside_group = &g;
// 				if (!inside_group || diag_distance(cu->u->pos - g.enemies.front()->pos) < diag_distance(cu->u->pos - inside_group->enemies.front()->pos)) {
// 					double d = units_distance(cu->u, g.enemies.front());
// 					weapon_stats* ew = cu->u->is_flying ? g.enemies.front()->stats->air_weapon : g.enemies.front()->stats->ground_weapon;
// 					double wr = ew ? ew->max_range + 64 : 32 * 6;
// 					weapon_stats* w = g.enemies.front()->is_flying ? cu->u->stats->air_weapon : cu->u->stats->ground_weapon;
// 					if (w && w->max_range > wr) wr = w->max_range;
// 					if (d <= wr) {
// 						inside_group = &g;
// 					}
// 				}
			}
		}
		if (cu->u->type->is_worker) {
			if (inside_group && my_base.test(grid::build_square_index(cu->u->pos))) {
				bool is_near_bunker = false;
				if (!my_units_of_type[unit_types::bunker].empty()) {
					double nearest_bunker_distance = get_best_score_value(my_units_of_type[unit_types::bunker], [&](unit*u) {
						return diag_distance(cu->u->pos - u->pos);
					});
					if (nearest_bunker_distance <= 32 * 15) is_near_bunker = true;
				}
				if (is_near_bunker) {
					inside_group = nullptr;
				}
			}
// 			if (inside_group && current_used_total_supply < 40) {
// 				bool any_buildings = false;
// 				for (unit*e : inside_group->enemies) {
// 					if (e->building) {
// 						any_buildings = true;
// 						break;
// 					}
// 				}
// 				if (!any_buildings) inside_group = nullptr;
// 			}
			if (inside_group && inside_group->enemies.size() == 1 && inside_group->enemies.front()->type->is_worker) inside_group = nullptr;
		}
		if (inside_group && current_frame < cu->strategy_busy_until) inside_group = nullptr;
		if (inside_group && cu->reference_count) inside_group = nullptr;
		if (inside_group) {
			log("%s is inside group %p\n", cu->u->type->name, inside_group);
			inside_group->allies.push_back(cu);
			i = available_units.erase(i);
			
			//game->drawTextMap(cu->u->pos.x, cu->u->pos.y, "inside group");
		} else ++i;
	}

	a_vector<combat_unit*> overlords;
	for (auto*c : live_combat_units) {
		if (current_frame < c->strategy_busy_until) continue;
		if (c->reference_count) continue;
		if (c->u->type == unit_types::overlord) overlords.push_back(c);
	}
	for (auto*c : overlords) {
		c->action = combat_unit::action_idle;
		c->subaction = combat_unit::subaction_idle;
		c->u->controller->action = unit_controller::action_idle;
		unit*n = get_best_score(my_resource_depots, [&](unit*u) {
			return diag_distance(u->pos - c->u->pos);
		});
		if (n && diag_distance(n->pos - c->u->pos) > 32 * 10) {
			c->action = combat_unit::action_offence;
			c->subaction = combat_unit::subaction_move;
			c->target_pos = n->pos;
		}
	}
	
	log(log_level_test, "no_aggressive_groups %d, dont_attack_main_army %d, aggressive_groups_done_only %d\n", no_aggressive_groups, dont_attack_main_army, aggressive_groups_done_only);

	for (auto&g : new_groups) {
		bool is_attacking = false;
		bool is_base_defence = false;
		bool is_just_one_worker = g.enemies.size() == 1 && g.enemies.front()->type->is_worker;
		bool enemy_has_bunker = false;
		int enemy_workers = 0;
		int enemy_buildings = 0;
		int enemy_vultures = 0;
		for (unit*e : g.enemies) {
			if (current_frame - e->last_attacked <= 15 * 30) is_attacking = true;
			if (!is_base_defence) is_base_defence = my_base.test(grid::build_square_index(e->pos));
			if (e->type == unit_types::bunker && e->is_completed) enemy_has_bunker = true;
			if (e->type->is_worker) ++enemy_workers;
			if (e->building) ++enemy_buildings;
			if (e->type == unit_types::vulture) ++enemy_vultures;
		}
		bool i_have_bunker = false;
		for (unit*u : my_completed_units_of_type[unit_types::bunker]) {
			if (u->loaded_units.empty()) continue;
			if (diag_distance(u->pos - g.enemies.front()->pos) > 32 * 20) continue;
			i_have_bunker = true;
			break;
		}
		if (is_just_one_worker) {
			xy pos = g.enemies.front()->pos;
			for (unit*u : enemy_units) {
				if (u->gone || u->type->is_non_usable) continue;
				if (u->type == unit_types::overlord) continue;
				if (diag_distance(u->pos - pos) <= 32 * 20) {
					is_just_one_worker = false;
					break;
				}
			}
		}
		bool is_strategy_priority = false;
		for (unit*u : enemy_units) {
			if (current_frame < u->strategy_high_priority_until) {
				is_strategy_priority = true;
				break;
			}
		}
		int non_worker_allies = 0;
		int worker_allies = 0;
		bool done = false;
		a_vector<combat_unit*> added;
		while (true) {
			multitasking::yield_point();

			a_unordered_set<combat_unit*> blacklist;
			auto get_nearest_unit = [&](bool allow_worker) {
				return get_best_score(available_units, [&](combat_unit*c) {
					if (g.is_aggressive_group && dont_attack_main_army && g.is_main_army) return std::numeric_limits<double>::infinity();
					if (g.is_aggressive_group && c->never_assign_to_aggressive_groups) return std::numeric_limits<double>::infinity();
					if (c->u->type == unit_types::overlord) return std::numeric_limits<double>::infinity();
					if (c->u->type->is_worker && !c->u->force_combat_unit && !allow_worker) return std::numeric_limits<double>::infinity();
					if (no_aggressive_groups && !c->force_attack && (c->u->type != unit_types::vulture || !aggressive_vultures) && (c->u->type != unit_types::wraith || !aggressive_wraiths) && (c->u->type != unit_types::valkyrie || !aggressive_valkyries) && (c->u->type != unit_types::zergling || !aggressive_zerglings) && (c->u->type != unit_types::mutalisk || !aggressive_mutalisks) && (c->u->type != unit_types::corsair || !aggressive_corsairs) && (c->u->type != unit_types::dark_templar || !aggressive_dark_templars) && (c->u->type != unit_types::zealot || !aggressive_zealots)) {
						if (g.is_aggressive_group) return std::numeric_limits<double>::infinity();
					} else if (!aggressive_groups_done_only && !dont_attack_main_army && g.is_defensive_group && !c->never_assign_to_aggressive_groups) return std::numeric_limits<double>::infinity();
					if (c->u->type->is_worker && !c->u->force_combat_unit && g.is_defensive_group) return std::numeric_limits<double>::infinity();
					if (!c->u->stats->ground_weapon && !c->u->stats->air_weapon) {
						if (c->u->type != unit_types::medic && c->u->type != unit_types::carrier && c->u->type != unit_types::science_vessel) {
							return std::numeric_limits<double>::infinity();
						}
					}
					//if (is_just_one_worker && !is_attacking && c->u->type->is_worker) return std::numeric_limits<double>::infinity();
					if (!is_attacking && c->u->type->is_worker && !c->u->force_combat_unit) return std::numeric_limits<double>::infinity();
					if (c->u->type->is_worker && !c->u->force_combat_unit && !is_base_defence) return std::numeric_limits<double>::infinity();
					if (blacklist.count(c)) return std::numeric_limits<double>::infinity();
					if (!can_reach_group[c].count(&g)) return std::numeric_limits<double>::infinity();;
					double d;
					if (c->u->is_loaded) d = diag_distance(g.enemies.front()->pos - c->u->pos);
					else d = units_pathing_distance(c->u, g.enemies.front());
					//if (c->u->type->is_worker && !c->u->force_combat_unit && d >= 32 * 20 && current_used_total_supply > 30) return std::numeric_limits<double>::infinity();
					if (c->u->type->is_worker && !c->u->force_combat_unit && d >= 32 * 10 && current_used_total_supply > 70) return std::numeric_limits<double>::infinity();
					//if (is_just_one_worker && !is_attacking && d >= 32 * 15) return std::numeric_limits<double>::infinity();
					if (c->u->type->is_worker && !c->u->force_combat_unit && is_just_one_worker && !g.allies.empty()) return std::numeric_limits<double>::infinity();
					if (c->u->type->is_worker && !c->u->force_combat_unit && worker_allies >= (int)my_workers.size() - 3) return std::numeric_limits<double>::infinity();
					//if (c->u->type == unit_types::drone && !is_just_one_worker) return std::numeric_limits<double>::infinity();
					if (c->u->type == unit_types::wraith && hide_wraiths && !allow_worker) return std::numeric_limits<double>::infinity();
					if (c->u->type->is_worker && !c->u->force_combat_unit && (enemy_has_bunker || i_have_bunker) && g.enemies.size() < 10) return std::numeric_limits<double>::infinity();
					if (c->u->type == unit_types::zealot && enemy_vultures >= g.enemies.size() - g.enemies.size() / 4) return d / 0.001;
					double hp_percent = c->u->hp / std::min(c->u->stats->hp, 1.0);
					return d / hp_percent;
				}, std::numeric_limits<double>::infinity());
			};
			auto any_non_buildings = [&]() {
				for (unit* e : g.enemies) {
					if (!e->building) return true;
				}
				return false;
			};
			auto any_ground_units = [&]() {
				for (unit* e : g.enemies) {
					if (!e->is_flying) return true;
				}
				return false;
			};
			auto any_tanks = [&]() {
				for (unit* e : g.enemies) {
					if (e->type == unit_types::siege_tank_tank_mode || e->type == unit_types::siege_tank_siege_mode) return true;
				}
				return false;
			};
			auto is_useful = [&](combat_unit*c) {
				//if (!can_reach_group[c].count(&g)) return false;
				//if (!c->u->stats->ground_weapon && !c->u->stats->air_weapon) return true;
				if (c->u->type == unit_types::defiler) return any_non_buildings();
				if (c->u->type == unit_types::queen) return any_ground_units();
				if (c->u->type == unit_types::science_vessel) return true;
				if (c->u->type == unit_types::carrier) return true;
				if (c->u->type == unit_types::medic) return true;
				if (!c->u->stats->ground_weapon && !c->u->stats->air_weapon) return false;
				if (c->u->type == unit_types::vulture && any_tanks()) return false;
				for (unit*e : g.enemies) {
					weapon_stats*w = e->is_flying ? c->u->stats->air_weapon : c->u->stats->ground_weapon;
					if (w) return true;
				}
				return false;
			};
			combat_unit*nearest_unit = get_nearest_unit(false);
			//if (!nearest_unit && non_worker_allies == 0) non_worker_allies = g.allies.size();
			if (!nearest_unit && (g.allies.size() < 6 || non_worker_allies < 6) && current_used_total_supply < 60 && !is_strategy_priority && is_attacking) nearest_unit = get_nearest_unit(true);
			while (nearest_unit && !is_useful(nearest_unit)) {
				//log("%p (%s) -> %p is not useful\n", nearest_unit, nearest_unit->u->type->name, &g);
				if (blacklist.count(nearest_unit)) {
					nearest_unit = nullptr;
					break;
				}
				blacklist.insert(nearest_unit);
				nearest_unit = get_nearest_unit(false);
				//if (!nearest_unit && non_worker_allies == 0) non_worker_allies = g.allies.size();
				if (!nearest_unit && (g.allies.size() < 6 || non_worker_allies < 6) && current_used_total_supply < 60 && !is_strategy_priority && is_attacking) nearest_unit = get_nearest_unit(true);
			}
			if (!nearest_unit) {
				log("failed to find unit for group %d\n", g.idx);
				if (aggressive_groups_done_only && g.is_aggressive_group) {
					for (auto*c : added) {
						find_and_erase(g.allies, c);
						available_units.insert(c);
					}
				}
				break;
			}
			log("assign %s to group %d\n", nearest_unit->u->type->name, g.idx);

			if (nearest_unit->u->type->is_worker) ++worker_allies;

			if (g.allies.size() % 2 == 0 && (!g.is_aggressive_group || !attack_bases)) {
				combat_eval::eval eval;
				eval.max_frames = 15 * 40;
				auto addu = [&](unit*u, int team) {
					//auto&c = eval.add_unit(u, team);
					eval.add_unit(u, team);
					if (team == 0 && u->type == unit_types::carrier) {
						for (int i = 0; i != 4; ++i) {
							eval.add_unit(units::get_unit_stats(unit_types::interceptor, players::my_player), 0);
						}
					}
				};
				for (auto*a : g.allies) addu(a->u, 0);
				for (unit*e : g.enemies) addu(e, 1);
				addu(nearest_unit->u, 0);
				eval.run();
				done = eval.teams[0].score > eval.teams[1].score*1.5;
				//if (nearest_unit->u->type->is_worker) {
				if (worker_allies) {
					//done |= eval.teams[0].score > eval.teams[1].score;
					//done |= eval.teams[0].end_supply >= eval.teams[1].end_supply;
				} else {
					if (enemy_workers || enemy_buildings) {
						//done = false;
						//if (g.allies.size() < 14) done = false;
						if (eval.teams[0].start_supply < 8.0) done = false;
					}
					//if (eval.teams[1].end_supply >= eval.teams[1].start_supply / 2) done = false;
				}
				if (is_strategy_priority) done = false;
			}
			
			if (done) {
				for (unit* e : g.enemies) {
					if (e->type == unit_types::shuttle || e->type == unit_types::dropship) {
						if (g.allies.size() < 6) done = false;
						break;
					}
				}
			}

			g.allies.push_back(nearest_unit);
			available_units.erase(nearest_unit);
			added.push_back(nearest_unit);
			if (done) {
				g.is_done = true;
				break;
			}
			if (is_just_one_worker) break;
		}

		if (done || g.allies.size() >= 4) {
			int cloaked_units = 0;
			for (unit*e : g.enemies) {
				if (e->cloaked) ++cloaked_units;
			}
			if (cloaked_units) log("group has %d cloaked units\n", cloaked_units);
			int send_n = std::min(cloaked_units, 3);
			double supply = 0.0;
			for (auto* a : g.allies) {
				supply += a->u->type->required_supply;
			}
			send_n += supply / 12.0;
			if (g.is_defensive_group && send_n == 0) send_n = 1;
			if (!g.is_aggressive_group && send_n == 0) send_n = 1;
			for (int detectors = 0; detectors < send_n;) {
				auto pred = [&](combat_unit*c) {
					if (!c->u->type->is_detector) return std::numeric_limits<double>::infinity();
					double d;
					if (c->u->is_loaded) d = diag_distance(g.enemies.front()->pos - c->u->pos);
					else d = units_pathing_distance(c->u, g.enemies.front());
					if (g.is_defensive_group) d = 0.0;
					return d;
				};
				combat_unit*c = get_best_score(available_units, pred, std::numeric_limits<double>::infinity());
				if (c) {
					g.allies.push_back(c);
					available_units.erase(c);
					++detectors;
					log("assigned %s\n", c->u->type->name);
				} else {
					c = get_best_score(overlords, pred, std::numeric_limits<double>::infinity());
					if (c) {
						g.allies.push_back(c);
						find_and_erase(overlords, c);
						++detectors;
						log("assigned %s\n", c->u->type->name);
					} else {
						break;
					}
				}
			}
		}

//		log(log_level_test, "group %d: %d allies %d enemies\n", g.idx, g.allies.size(), g.enemies.size());
//		for (auto*cu : g.allies) {
//			log(log_level_test, " ally %s\n", cu->u->type->name);
//		}
	}

	for (auto ci = available_units.begin(); ci != available_units.end();) {
		auto*c = *ci;
		if (c->u->type == unit_types::shuttle) {
			auto*g = get_best_score_p(new_groups, [&](const group_t*g) {
				if (g->allies.empty()) return std::numeric_limits<double>::infinity();
				return diag_distance(c->u->pos - g->allies.front()->u->pos);
			}, std::numeric_limits<double>::infinity());
			if (g) {
				g->allies.push_back(c);
				ci = available_units.erase(ci);
				continue;
			}
		}
		++ci;
	}

// 	for (auto*c : available_units) {
// 		if (c->u->type->is_worker && !c->u->force_combat_unit) {
// 			if (c->action != combat_unit::action_idle || c->subaction != combat_unit::subaction_idle && c->subaction != combat_unit::subaction_repair) {
// 				c->action = combat_unit::action_idle;
// 				c->subaction = combat_unit::subaction_idle;
// 				c->u->controller->action = unit_controller::action_idle;
// 			}
// 		} else {
// 			if (c->action == combat_unit::action_offence) {
// 				c->action = combat_unit::action_idle;
// 				c->subaction = combat_unit::subaction_idle;
// 				c->u->controller->action = unit_controller::action_idle;
// 			}
// 		}
// 	}

	if (!new_groups.empty()) {
		if (dont_attack_main_army) {
			auto* g = get_best_score_p(new_groups, [&](const group_t* g) {
				if (g->is_defensive_group) return 0;
				return 1;
			});
			for (auto ci = available_units.begin(); ci != available_units.end();) {
				auto* c = *ci;
				bool skip = false;
				if (c->u->type->is_worker && !c->u->force_combat_unit) skip = true;
				if (c->u->type->is_worker && !c->u->force_combat_unit && g->is_defensive_group) skip = true;
				if (c->u->type == unit_types::wraith && hide_wraiths) skip = true;
				if (c->u->type == unit_types::overlord) skip = true;
				if (skip) ++ci;
				else {
					g->allies.push_back(c);
					ci = available_units.erase(ci);
				}
			}
		} else {
			auto* largest_air_group = get_best_score_p(new_groups, [&](const group_t* g) {
				if (g->is_defensive_group) return 2;
				if ((no_aggressive_groups || aggressive_groups_done_only) && g->is_aggressive_group) return 2;
				if (dont_attack_main_army && g->is_aggressive_group && g->is_main_army) return 3;
				if (no_aggressive_groups && g->is_done) return 4;
				int flyers = 0;
				for (auto* e : g->enemies) {
					if (e->is_flying) ++flyers;
				}
				if (flyers == 0) return 1;
				return -(int) g->enemies.size();
			});
			auto* largest_ground_group = get_best_score_p(new_groups, [&](const group_t* g) {
				if (g->is_defensive_group) return 2;
				if ((no_aggressive_groups || aggressive_groups_done_only) && g->is_aggressive_group) return 2;
				if (dont_attack_main_army && g->is_aggressive_group && g->is_main_army) return 3;
				if (no_aggressive_groups && g->is_done) return 4;
				int non_flyers = 0;
				for (auto* e : g->enemies) {
					if (!e->is_flying) ++non_flyers;
				}
				if (non_flyers == 0) return 1;
				if (g->enemies.size() == 1 && g->enemies.front()->type->is_worker) return 100;
				return -(int) g->enemies.size();
			});
			auto* largest_total_group = get_best_score_p(new_groups, [&](const group_t* g) {
				if (g->is_defensive_group) return 2;
				if ((no_aggressive_groups || aggressive_groups_done_only) && g->is_aggressive_group) return 2;
				if (dont_attack_main_army && g->is_aggressive_group && g->is_main_army) return 3;
				if (g->allies.empty() && !g->is_defensive_group && (no_aggressive_groups || aggressive_groups_done_only)) return 1;
				if (no_aggressive_groups && g->is_done) return 4;
				if (g->enemies.size() == 1 && g->enemies.front()->type->is_worker) return 100;
				return -(int) g->enemies.size();
			});
			for (int i = 0; i < 3; ++i) {
				auto* largest_group = i == 0 ? largest_air_group : i == 1 ? largest_ground_group : largest_total_group;
				bool is_just_one_worker = largest_group && largest_group->enemies.size() == 1 && largest_group->enemies.front()->type->is_worker;
				if (largest_group && (!largest_group->allies.empty() || i == 2) && !is_just_one_worker) {
					for (auto ci = available_units.begin(); ci != available_units.end();) {
						auto* c = *ci;
						bool skip = false;
						if (i == 0 && !c->u->stats->air_weapon) skip = true;
						if (i == 1 && !c->u->stats->ground_weapon) skip = true;
						if (c->u->type->is_worker && !c->u->force_combat_unit) skip = true;
						if (c->u->type->is_worker && !c->u->force_combat_unit && largest_group->is_defensive_group) skip = true;
						if (c->u->type == unit_types::wraith && hide_wraiths) skip = true;
						if (c->u->type == unit_types::overlord) skip = true;
						if (skip) ++ci;
						else {
							largest_group->allies.push_back(c);
							ci = available_units.erase(ci);
						}
					}
				}
			}
		}
	} else {
// 		for (auto*c : available_units) {
// 			c->action = combat_unit::action_idle;
// 			if (!c->u->type->is_worker || c->u->force_combat_unit) {
// 				c->subaction = combat_unit::subaction_move;
// 				c->target_pos = defence_choke.inside;
// 			}
// 		}
	}
	available_units.clear();
	
//	for (auto& g : new_groups) {
//		log(log_level_test, "group %d: %d allies %d enemies\n", g.idx, g.allies.size(), g.enemies.size());
//		for (auto* cu : g.allies) {
//			log(log_level_test, " ally %s\n", cu->u->type->name);
			
//			for (auto& g2 : new_groups) {
//				if (&g == &g2) continue;
//				for (auto* cu2 : g2.allies) {
//					if (cu == cu2) xcept("waa unit is in multiple groups");
//				}
//			}
//		}
//	}

	bool has_ventral_sacs = players::my_player->has_upgrade(upgrade_types::ventral_sacs);
	a_unordered_set<combat_unit*> given_lifts;
	for (auto&g : new_groups) {
		xy pos = g.enemies.front()->pos;
		a_vector<combat_unit*> dropships;
		a_vector<combat_unit*> moving_units;
		for (auto*cu : live_combat_units) {
			if (has_ventral_sacs && cu->u->type == unit_types::overlord) dropships.push_back(cu);
		}

 		for (auto*cu : g.allies) {
// 			cu->goal_pos = pos;
// 			cu->action = combat_unit::action_offence;
// 			if (current_frame - cu->last_fight <= 40) continue;
// 			//log("move %s\n", cu->u->type->name);
// 			//if (cu->subaction != combat_unit::subaction_move) {
// 			if (!g.is_defensive_group || current_frame - cu->last_default_move >= 15*20) {
// 				cu->subaction = combat_unit::subaction_move;
// 				cu->target_pos = pos;
// 				cu->last_default_move = current_frame;
// 			}
			if (cu->u->type == unit_types::dropship) dropships.push_back(cu);
			moving_units.push_back(cu);

// 			if (cu->assigned_control_spot) {
// 				cu->subaction = combat_unit::subaction_move;
// 				cu->target_pos = cu->assigned_control_spot->pos;
// 			}
// 			if (cu->assigned_defence_control_spot) {
// 				cu->subaction = combat_unit::subaction_move;
// 				cu->target_pos = cu->assigned_defence_control_spot->pos;
// 			}
 		}

// 		int process_uid = current_frame;
// 		for (auto*cu : dropships) {
// 			if (given_lifts.count(cu)) continue;
// 			if (give_lifts(cu, moving_units, process_uid)) given_lifts.insert(cu);
// 		}
	}
	if (true) {
		a_vector<combat_unit*> dropships;
		a_vector<combat_unit*> pickup_workers;
		for (auto*cu : live_combat_units) {
			if (has_ventral_sacs && cu->u->type == unit_types::overlord) dropships.push_back(cu);
			if (cu->u->type->is_worker) {
				if (cu->u->controller->action == unit_controller::action_build) {
					cu->goal_pos = cu->u->controller->target_pos;
					pickup_workers.push_back(cu);
				}
			}
		}
// 		for (auto*cu : dropships) {
// 			if (given_lifts.count(cu)) continue;
// 			if (give_lifts(cu, pickup_workers, current_frame)) given_lifts.insert(cu);
// 		}
	}

	if (true) {
		auto do_copy = [&](unit_type* ut, int count) {
			combat_unit* copy_target = nullptr;
			int copy_n = 0;
			for (auto* a : live_combat_units) {
				if (a->copy && a->copy->u->dead) {
					find_and_erase(a->copy->copying_units, a);
					a->copy = nullptr;
				}
				if (a->u->type != ut) continue;
				if (!copy_target) copy_target = a;
				else if (!a->copy) {
					a->copy = copy_target;
					copy_target->copying_units.push_back(a);
					if (++copy_n >= count) copy_target = nullptr;
				}
			}
		};
		//do_copy(unit_types::wraith, 8);
		//do_copy(unit_types::valkyrie, 4);
	}

#ifdef USE_COMBAT_EVAL2
	for (auto&g : new_groups) {

		combat_eval2::eval eval;

		for (auto*a : g.allies) {
			eval.add(a->u, 0);
		}
		for (unit*e : g.enemies) {
			auto&c = eval.add(e, 1);
			if (e->type == unit_types::bunker) {
				c.marines_loaded = e->marines_loaded;
			}
		}

		eval.run();

		g.fight = eval.teams[0].resources_lost <= eval.teams[1].resources_lost;
		g.fight |= eval.teams[0].resources_value >= eval.teams[1].resources_value;

		if (!g.fight) {

			combat_eval2::eval run_eval;
			run_eval.max_frames = 15 * 20;

			for (auto*a : g.allies) {
				run_eval.add(a->u, 0);
			}
			for (unit*e : g.enemies) {
				auto&c = run_eval.add(e, 1);
				if (e->type == unit_types::bunker) {
					c.marines_loaded = e->marines_loaded;
				}
			}

			run_eval.teams[0].run_away = true;
			run_eval.run();

			if (run_eval.teams[0].resources_lost >= eval.teams[0].resources_lost / 2) {
				log("can't run, fight!\n");
				g.fight = true;
			}

		}

		g.fight_my_resources_lost = eval.teams[0].resources_lost;
		g.fight_op_resources_lost = eval.teams[1].resources_lost;

	}

#endif
	
	std::lock_guard<multitasking::mutex> l(groups_mut);
	groups = std::move(new_groups);
}

template<typename node_data_t = no_value_t, typename pred_T, typename est_dist_T, typename goal_T>
a_deque<xy> find_path(unit_type*ut, xy from, pred_T&&pred, est_dist_T&&est_dist, goal_T&&goal) {
	if (ut->is_flyer) {
		return flyer_pathing::find_path<node_data_t>(from, pred, est_dist, goal);
	} else {
		return square_pathing::find_square_path<node_data_t>(square_pathing::get_pathing_map(ut), from, pred, est_dist, goal);
	}
};

a_unordered_map<combat_unit*, std::tuple<unit*, double, int>> registered_focus_fire;

a_unordered_map<unit*, int> dropship_spread;
a_unordered_map<unit*, double> focus_fire;
a_unordered_map<unit*, double> prepared_damage;
a_unordered_map<unit*, bool> is_being_healed;
a_vector<xy> spread_positions;

//a_vector<combat_unit*> spider_mine_layers;
a_vector<unit*> active_spider_mines;
a_unordered_set<unit*> active_spider_mine_targets;
a_unordered_set<unit*> my_spider_mines_in_range_of;
a_unordered_map<unit*, int> n_spider_mines_in_range_of;

//a_unordered_map<unit*, double> nuke_damage;
tsc::dynamic_bitset build_square_taken;
a_unordered_map<unit*, int> space_left;
a_unordered_map<unit*, int> bunker_repair_count;
a_unordered_set<unit*> attack_pylons;

a_unordered_set<unit*> no_splash_target;
a_unordered_map<unit*, std::tuple<combat_unit*, int>> disable_target_taken;
a_unordered_map<unit*, std::tuple<combat_unit*, int>> optical_flare_target_taken;
a_unordered_map<unit*, std::tuple<combat_unit*, int>> parasite_target_taken;
a_map<xy, std::tuple<combat_unit*, int>> dark_swarm_target_taken;
a_map<xy, std::tuple<combat_unit*, int>> plague_target_taken;
tsc::dynamic_bitset existing_dark_swarm;

a_map<xy, std::tuple<combat_unit*, int>> storm_target_taken;
a_map<xy, std::tuple<combat_unit*, int>> aoe_disable_target_taken;

a_unordered_map<unit*, std::tuple<combat_unit*, int>> feedback_target_taken;

a_unordered_set<unit*> shuttle_target_taken;

a_vector<std::tuple<combat_unit*, unit*>> need_detector;

tsc::dynamic_bitset run_spot_taken;

a_vector<unit*> enemy_overlords;

a_unordered_map<unit*, int> enemy_tank_move_count;

a_deque<std::tuple<xy, int>> detected_areas;

a_vector<int> mine_threat_area;

void prepare_attack() {
	for (auto i = registered_focus_fire.begin(); i != registered_focus_fire.end();) {
		unit*target;
		double damage;
		int end_frame;
		std::tie(target, damage, end_frame) = i->second;
		if (current_frame >= end_frame) {
			if (std::abs(focus_fire[target] -= damage) < 1) {
				focus_fire.erase(target);
			}
			i = registered_focus_fire.erase(i);
		} else ++i;
	}
	static int last_cleanup_focus_fire;
	if (current_frame - last_cleanup_focus_fire >= 15 * 5) {
		last_cleanup_focus_fire = current_frame;
		for (auto i = focus_fire.begin(); i != focus_fire.end();) {
			if (i->second < 1) i = focus_fire.erase(i);
			else ++i;
		}
	}
	//focus_fire.clear();
	prepared_damage.clear();
	dropship_spread.clear();
	is_being_healed.clear();
	spread_positions.clear();
	//spider_mine_layers.clear();

	active_spider_mines.clear();
	active_spider_mine_targets.clear();
	my_spider_mines_in_range_of.clear();
	n_spider_mines_in_range_of.clear();
	for (unit*u : my_units_of_type[unit_types::spider_mine]) {
		if (u->order_target) {
			active_spider_mines.push_back(u);
			active_spider_mine_targets.insert(u->order_target);
		}
		for (unit*e : enemy_units) {
			if (!e->visible) continue;
			if (e->is_flying || e->type->is_hovering) continue;
			if (units_distance(u, e) <= 32 * 3) {
				my_spider_mines_in_range_of.insert(e);
				++n_spider_mines_in_range_of[e];
			}
		}
	}
	for (unit*u : enemy_units) {
		if (!u->visible) continue;
		if (u->type != unit_types::spider_mine) continue;
		if (u->order_target) active_spider_mine_targets.insert(u->order_target);
	}

	//nuke_damage.clear();
	build_square_taken.reset();
	space_left.clear();
	for (unit*u : my_units_of_type[unit_types::bunker]) {
		int space = u->type->space_provided;
		for (unit*lu : u->loaded_units) {
			space -= lu->type->space_required;
		}
		space_left[u] = space;
	}
	bunker_repair_count.clear();

	a_vector<unit*> enemy_pylons;
	for (unit*u : enemy_buildings) {
		if (u->type == unit_types::pylon) enemy_pylons.push_back(u);
	}
	for (unit*u : enemy_buildings) {
		if (u->type->requires_pylon) {
			int count = 0;
			unit*pylon = nullptr;
			for (unit*p : enemy_pylons) {
				if (pylons::is_in_pylon_range(u->pos - p->pos)) {
					pylon = p;
					++count;
					if (count >= 2) break;
				}
			}
			if (count == 1 && pylon) {
				attack_pylons.insert(pylon);
			}
		}
	}

	no_splash_target.clear();
	for (unit*e : visible_enemy_units) {
		if (!e->visible || e->is_flying || e->building) continue;
		int n = 0;
		for (auto*a : live_combat_units) {
			if (a->u->hp < 20 || a->u->is_flying) continue;
			if (diag_distance(e->pos - a->u->pos) > 32 * 3) continue;
			if (units_distance(e, a->u) > 20) continue;
			if (a->u->type == unit_types::siege_tank_tank_mode || a->u->type == unit_types::siege_tank_siege_mode) n += 4;
			else ++n;
			if (n >= 2) break;
		}
		if (n >= 2) {
			no_splash_target.insert(e);
		}
	}

	double scan_energy_cost = bwapi_tech_type_energy_cost(BWAPI::TechTypes::Scanner_Sweep);
	scans_available = 0;
	int comsats = 0;
	for (unit*u : my_units_of_type[unit_types::comsat_station]) {
		++comsats;
		if (u->energy < scan_energy_cost) continue;
		scans_available += (int)(u->energy / scan_energy_cost);
	}

	for (auto i = disable_target_taken.begin(); i != disable_target_taken.end();) {
		if (current_frame >= std::get<1>(i->second)) i = disable_target_taken.erase(i);
		else ++i;
	}

	for (auto i = optical_flare_target_taken.begin(); i != optical_flare_target_taken.end();) {
		if (current_frame >= std::get<1>(i->second)) i = optical_flare_target_taken.erase(i);
		else ++i;
	}

	for (auto i = parasite_target_taken.begin(); i != parasite_target_taken.end();) {
		if (current_frame >= std::get<1>(i->second)) i = parasite_target_taken.erase(i);
		else ++i;
	}

	for (auto i = dark_swarm_target_taken.begin(); i != dark_swarm_target_taken.end();) {
		if (current_frame >= std::get<1>(i->second)) i = dark_swarm_target_taken.erase(i);
		else ++i;
	}

	for (auto i = plague_target_taken.begin(); i != plague_target_taken.end();) {
		if (current_frame >= std::get<1>(i->second)) i = plague_target_taken.erase(i);
		else ++i;
	}

	for (auto i = storm_target_taken.begin(); i != storm_target_taken.end();) {
		if (current_frame >= std::get<1>(i->second)) i = storm_target_taken.erase(i);
		else ++i;
	}

	for (auto i = aoe_disable_target_taken.begin(); i != aoe_disable_target_taken.end();) {
		if (current_frame >= std::get<1>(i->second)) i = aoe_disable_target_taken.erase(i);
		else ++i;
	}

	for (auto i = feedback_target_taken.begin(); i != feedback_target_taken.end();) {
		if (current_frame >= std::get<1>(i->second)) i = feedback_target_taken.erase(i);
		else ++i;
	}

	need_detector.clear();

	shuttle_target_taken.clear();

	enemy_overlords.clear();
	for (unit* e : enemy_units) {
		if (!e->gone && e->type == unit_types::overlord) enemy_overlords.push_back(e);
	}

	enemy_tank_move_count.clear();
	
	while (!detected_areas.empty()) {
		if (std::get<1>(detected_areas.front()) < current_frame) break;
		detected_areas.pop_front();
	}
}

a_vector<std::tuple<xy, double>> nuke_test;

auto lay_mine = [&](combat_unit*c, bool forced = false) {
	int my_inrange = 0;
	int my_non_vultures_nearby = 0;
	for (unit*u : my_units) {
		if (u->type->is_building) continue;
		if (u->is_flying) continue;
		if (u->type->is_non_usable) continue;
		if (units_distance(c->u, u) <= 32 * 3) ++my_inrange;
		if (u->type != unit_types::vulture && units_distance(c->u, u) <= 32 * 6) ++my_non_vultures_nearby;
	}
	if (!forced && my_non_vultures_nearby > 1) {
		int inrange = 0;
		for (unit*u : my_units_of_type[unit_types::spider_mine]) {
			if (diag_distance(u->pos - c->u->pos) <= 32 * 3) ++inrange;
		}
		if (inrange >= 3) return;
		int op_inrange = 0;
		for (unit*u : enemy_units) {
			if (!u->visible) continue;
			if (u->type->is_building) continue;
			if (u->is_flying) continue;
			if (u->type->is_hovering) continue;
			if (u->type->is_non_usable) continue;
			if (units_distance(c->u, u) <= 32 * 3) ++op_inrange;
		}
		if (op_inrange + 2 <= my_inrange) return;
		if (my_non_vultures_nearby >= 5) return;
	}
	if (current_frame - c->last_used_special >= 15) {
		c->u->game_unit->useTech(upgrade_types::spider_mines->game_tech_type, BWAPI::Position(c->u->pos.x, c->u->pos.y));
		c->last_used_special = current_frame;
		c->u->controller->noorder_until = current_frame + 15;
	}
};

void do_control_spot_positioning(combat_unit* a, control_spot* cs) {
	bool has_bunkers = my_completed_units_of_type[unit_types::bunker].size() >= 2;

	auto siege_score = [&](xy pos) {
		double outside_d = unit_pathing_distance(unit_types::zealot, pos, cs->incoming_pos);
		double inside_d = unit_pathing_distance(unit_types::zealot, pos, cs->pos);
		if (inside_d >= 32 * 15) inside_d *= 10;
		int count = 0;
		if (diag_distance(pos - cs->incoming_pos + xy(16, 16)) <= 32 * 12 + 32) count += 8;
		//if (count < std::min(4, (int)defence_choke.build_squares.size())) return 0.0;
		if (!my_units_of_type[unit_types::spider_mine].empty()) {
			double d = get_best_score_value(my_units_of_type[unit_types::spider_mine], [&](unit* u) {
				return diag_distance(pos - u->pos);
			});
			if (d <= 32 * 5) return 0.0;
		}
		if (count == 0) return 0.0;
		double height = grid::get_build_square(pos).height - grid::get_build_square(cs->incoming_pos).height;
		inside_d -= 32 * 8;
		if (inside_d < 0) inside_d = 0;
		return -outside_d + inside_d - count * 12;
		//return -outside_d + inside_d - count * 12 - height * 64;
	};

	if (a->siege_pos != xy()) {
		double s = siege_score(a->siege_pos);
		bool reset = s >= 0;
		if (a->u->type == unit_types::siege_tank_tank_mode) {
			size_t index = grid::build_square_index(a->siege_pos);
			if (build_square_taken.test(index)) reset = true;
			if (dont_siege_here.test(index)) reset = true;
		}
		if (a->u->type == unit_types::siege_tank_siege_mode && current_frame - a->u->controller->last_siege >= 15 * 40) reset = true;
		if (reset) a->siege_pos = xy();
	}
	if (a->siege_pos != xy()) {
		auto set = [&](xy pos) {
			if ((size_t)pos.x >= (size_t)grid::map_width || (size_t)pos.y >= (size_t)grid::map_height) return;
			build_square_taken.set(grid::build_square_index(pos));
		};
		set(a->siege_pos);
		set(a->siege_pos + xy(+32, 0)); set(a->siege_pos + xy(+32, +32));
		set(a->siege_pos + xy(0, +32)); set(a->siege_pos + xy(-32, +32));
		set(a->siege_pos + xy(-32, 0)); set(a->siege_pos + xy(-32, -32));
		set(a->siege_pos + xy(0, -32)); set(a->siege_pos + xy(+32, -32));
	}

	a_vector<std::tuple<xy, int>> defence_position_count;

// 	bool is_defensive_concave = false;
// 	a_vector<xy> defensive_positions;
// 	//get_defensive_positions(allies, defensive_positions, is_defensive_concave);
// 	for (xy pos : defensive_positions) {
// 		defence_position_count.emplace_back(pos + xy(16, 16), 0);
// 	}


	// 	a_vector<std::tuple<xy, int>> defence_position_count;
	// 	for (auto*bs : defence_choke.build_squares) {
	// 		defence_position_count.emplace_back(bs->pos + xy(16, 16), 0);
	// 	}

	int path_iterations = 0;
	bool default_move = true;
	//if (a->u->type == unit_types::siege_tank_tank_mode || a->u->type == unit_types::siege_tank_siege_mode) {
	if (true) {
		if (a->u->type != unit_types::siege_tank_siege_mode && current_frame - a->last_find_siege_pos >= 15 * 2) {
			a->last_find_siege_pos = current_frame;
			tsc::dynamic_bitset visited(grid::build_grid_width*grid::build_grid_height);
			double best_score = 0.0;
			xy best_pos;
			find_path(a->u->type, a->u->pos, [&](xy pos, xy npos) {
				if (++path_iterations % 1024 == 0) multitasking::yield_point();
				double d = diag_distance(npos - a->u->pos);
				if (d >= 32 * 30) return false;
				size_t index = grid::build_square_index(npos);
				if (build_square_taken.test(index)) return false;
				if (d >= 32 * 3 && !visited.test(index) && !dont_siege_here.test(index)) {
					visited.set(index);
					double s = siege_score(npos);
					if (s < best_score) {
						best_score = s;
						best_pos = npos;
					}
				}
				return true;
			}, [&](xy pos, xy npos) {
				return 0.0;
			}, [&](xy pos) {
				return false;
			});
			if (a->siege_pos != xy() && siege_score(a->siege_pos) < best_score) best_pos = a->siege_pos;
			if (best_pos != xy()) {
				a->siege_pos = best_pos;
			}
		}
		if (a->siege_pos != xy()) {
			default_move = false;
			if (a->u->type == unit_types::siege_tank_siege_mode) {
				a->siege_pos = a->u->pos;
				a->subaction = combat_unit::subaction_idle;
				if (diag_distance(a->u->pos - a->siege_pos) >= 16) {
					if (a->u->game_unit->unsiege()) {
						a->u->controller->noorder_until = current_frame + 30;
					}
				}
			} else {
				a->subaction = combat_unit::subaction_idle;
				if (diag_distance(a->u->pos - a->siege_pos) > 8) {
					a->subaction = combat_unit::subaction_move;
					a->target_pos = a->siege_pos;
				} else if (a->u->type == unit_types::siege_tank_tank_mode) {
					if (players::my_player->has_upgrade(upgrade_types::siege_mode)) {
						if (current_frame >= a->u->controller->move_away_until && a->u->game_unit->siege()) {
							a->u->controller->noorder_until = current_frame + 30;
							a->u->controller->last_siege = current_frame;
						}
					}
				}
			}
		}
	}
	if (default_move) {
		a->subaction = combat_unit::subaction_move;
// 		if (!defence_position_count.empty()) {
// 			auto*v = get_best_score_p(defence_position_count, [&](std::tuple<xy, int>*v) {
// 				return std::make_tuple(std::get<1>(*v), diag_distance(std::get<0>(*v) - a->u->pos));
// 			});
// 			++std::get<1>(*v);
// 			a->target_pos = std::get<0>(*v);
// 		} else {
// 			if (a->target_pos == defence_choke.center || a->target_pos == defence_choke.inside) {
// 				if (diag_distance(a->u->pos - a->target_pos) <= 32 * 3) {
// 					if (a->target_pos == defence_choke.center) a->target_pos = defence_choke.inside;
// 					else a->target_pos = defence_choke.center;
// 				}
// 			} else a->target_pos = defence_choke.center;
// 		}
		a->target_pos = cs->pos;

		//a->target_pos = defence_choke.inside;
		if (wall_in::active_wall_count()) a->target_pos = defence_choke.inside;
	}

}


a_unordered_map<combat_unit*, xy> use_dark_swarm_please;
a_unordered_map<combat_unit*, xy> use_plague_please;

a_unordered_map<combat_unit*, xy> use_storm_please;
a_unordered_map<combat_unit*, xy> use_stasis_please;
a_unordered_map<combat_unit*, xy> use_maelstrom_please;
a_unordered_map<combat_unit*, xy> use_ensnare_please;

a_vector<std::pair<combat_unit*, combat_unit*>> pickup_list;

a_map<xy, std::pair<int, int>> active_nukes;

void finish_attack() {
// 	a_vector<combat_unit*> spider_mine_layers;
// 	a_vector<combat_unit*> close_enough;
// 	bool close_enough_cloaked = false;
// 
// 	for (combat_unit*c : live_combat_units) {
// 		if (current_frame < c->strategy_busy_until) continue;
//      if (c->reference_count) continue;
// 		if (players::my_player->has_upgrade(upgrade_types::spider_mines) && c->u->spider_mine_count) {
// 			unit*target = nullptr;
// 			unit*nearby_target = nullptr;
// 			double nearest_sieged_tank_distance = get_best_score_value(my_units_of_type[unit_types::siege_tank_siege_mode], [&](unit*u) {
// 				return diag_distance(u->pos - c->u->pos);
// 			});
// 			if (my_units_of_type[unit_types::siege_tank_siege_mode].empty()) nearest_sieged_tank_distance = std::numeric_limits<double>::infinity();
// 			for (unit*e : enemy_units) {
// 				if (!e->visible) continue;
// 				if (e->type->is_building) continue;
// 				if (e->is_flying) continue;
// 				if (e->type->is_hovering) continue;
// 				if (e->type->is_non_usable) continue;
// 				if (e->invincible) continue;
// 				if (e->type->size == unit_type::size_small && (nearest_sieged_tank_distance < 32 * 5 || nearest_sieged_tank_distance > 32 * 12)) continue;
// 				if (e->type == unit_types::lurker && e->burrowed) continue;
// 				double d = diag_distance(e->pos - c->u->pos);
// 				if (d <= 32 * 5) {
// 					nearby_target = e;
// 				}
// 				if (d <= 32 * 3) {
// 					target = e;
// 					break;
// 				}
// 			}
// 			if (nearby_target) spider_mine_layers.push_back(c);
// 			if (target) close_enough.push_back(c);
// 			if (target && target->cloaked && !target->detected) close_enough_cloaked = true;
// 
// 			if (nearby_target && !target && current_used_total_supply < 80) {
// 				if (current_frame - c->last_fight <= 15 * 5 && c->last_win_ratio < 2.0) {
// 					lay_mine(c, false);
// 				}
// 			}
// 		}
// 	}
// 
// 	bool lay = false;
// 	if (close_enough.size() >= 4 || close_enough.size() >= spider_mine_layers.size()) {
// 		for (auto*c : close_enough) {
// 			lay = true;
// 			break;
// 		}
// 	} else {
// 		for (auto*c : close_enough) {
// 			bool is_solo = true;
// 			for (auto*c2 : spider_mine_layers) {
// 				if (units_distance(c->u, c2->u) <= 32) {
// 					is_solo = false;
// 					break;
// 				}
// 			}
// 			if (is_solo || c->u->hp <= 60) {
// 				lay = true;
// 				break;
// 			}
// 		}
// 	}
// 	if (lay) {
// 		for (auto*c : close_enough) {
// 			lay_mine(c, close_enough_cloaked);
// 		}
// 	}

	a_vector<combat_unit*> ghosts;
	for (auto*c : live_combat_units) {
		if (current_frame < c->strategy_busy_until) continue;
		if (c->reference_count) continue;
		if (c->u->type == unit_types::ghost) ghosts.push_back(c);
	}
	if (players::my_player->has_upgrade(upgrade_types::personal_cloaking)) {
		for (auto*c : ghosts) {
			if (current_frame - c->last_used_special < 8) continue;
			if (c->u->cloaked) {
				if (current_frame - c->last_fight >= 15 * 5) {
					c->u->game_unit->decloak();
					c->last_used_special = current_frame;
				}
				continue;
			}
			if (current_frame - c->last_fight >= 15 * 5) continue;
			if (c->u->hp >= c->u->stats->hp) {
				if (c->last_win_ratio >= 4.0 && c->u->energy < 240) continue;
				if (c->last_win_ratio >= 0.25 && c->u->energy < 140) continue;
			}
			if (c->last_op_detectors == 0) {
				c->subaction = combat_unit::subaction_use_ability;
				c->ability = upgrade_types::personal_cloaking;
				c->last_used_special = current_frame;
			}
		}
	}
	auto test_in_range = [&](combat_unit*c, unit*e, xy stand_pos, bool&in_attack_range, bool&is_revealed) {
		if (!in_attack_range) {
			weapon_stats*w = c->u->is_flying ? e->stats->air_weapon : e->stats->ground_weapon;
			if (w) {
				double d = diag_distance(e->pos - stand_pos);
				if (d <= w->max_range + 32) {
					in_attack_range = true;
				}
			}
		}
		if (!is_revealed) {
			if (e->type->is_detector) {
				double d = diag_distance(e->pos - stand_pos);
				if (d <= e->stats->sight_range + 32) is_revealed = true;
			}
		}
	};
	if (players::my_player->has_upgrade(upgrade_types::lockdown)) {
		a_unordered_set<unit*> target_taken;
		int total_available_lockdowns = 0;
		for (auto*c : ghosts) {
			if (c->u->energy >= 100) ++total_available_lockdowns;
		}
		for (auto*c : ghosts) {
			if (c->u->energy < 100) continue;
			if (c->last_win_ratio >= 2.0 && c->u->hp >= c->u->stats->hp && c->u->energy < 200) continue;
			unit*target = get_best_score(enemy_units, [&](unit*u) {
				if (!u->visible || !u->detected) return std::numeric_limits<double>::infinity();
				if (!u->type->is_mechanical) return std::numeric_limits<double>::infinity();
				if (u->stasis_timer) return std::numeric_limits<double>::infinity();
				if (u->building) return std::numeric_limits<double>::infinity();
				if (u->type->is_worker) return std::numeric_limits<double>::infinity();
				if (u->lockdown_timer) return std::numeric_limits<double>::infinity();
				if (u->maelstrom_timer) return std::numeric_limits<double>::infinity();

				double r = units_distance(u, c->u) - 32 * 8;
				if (r >= 32 * 6) return std::numeric_limits<double>::infinity();

				if (target_taken.count(u)) return std::numeric_limits<double>::infinity();
				double value = u->minerals_value + u->gas_value;
				//if (value < 200 && (c->u->energy < 200 && c->u->hp >= c->u->stats->hp && total_available_lockdowns < 4)) return std::numeric_limits<double>::infinity();
				if (value < 125) return std::numeric_limits<double>::infinity();
				//double r = diag_distance(u->pos - c->u->pos) - 32 * 8;
				if (r < 0) r = 0;
				if (total_available_lockdowns >= 4 && r > 0) return std::numeric_limits<double>::infinity();

				auto it = disable_target_taken.find(u);
				if (it != disable_target_taken.end() && std::get<0>(it->second) != c) return std::numeric_limits<double>::infinity();

				return r / value / ((u->shields + u->hp) / (u->stats->shields + u->stats->hp));
			}, std::numeric_limits<double>::infinity());
			if (target) {
				target_taken.insert(target);
				double r = units_distance(c->u, target);
				if (r <= 32 * 8) {
					disable_target_taken[target] = std::make_tuple(c, current_frame + 60);
					if (current_frame - c->last_used_special >= 15) {
						c->u->game_unit->useTech(upgrade_types::lockdown->game_tech_type, target->game_unit);
						c->last_used_special = current_frame;
						c->u->controller->noorder_until = current_frame + 15;
					}
				} else {
					xy stand_pos;
					xy relpos = c->u->pos - target->pos;
					double ang = std::atan2(relpos.y, relpos.x);
					stand_pos.x = target->pos.x + (int)(std::cos(ang) * 32 * 8);
					stand_pos.y = target->pos.y + (int)(std::sin(ang) * 32 * 8);
					bool can_cloak = players::my_player->has_upgrade(upgrade_types::personal_cloaking) && c->u->energy >= 100 + 25 + 10;
					bool in_attack_range = false;
					bool is_revealed = !can_cloak;
					for (unit*e : enemy_units) {
						if (e->gone) continue;
						test_in_range(c, e, stand_pos, in_attack_range, is_revealed);
						if (in_attack_range && is_revealed) break;
					}
					if (!in_attack_range || !is_revealed) {
						if (in_attack_range && !c->u->cloaked && can_cloak) {
							c->subaction = combat_unit::subaction_use_ability;
							c->ability = upgrade_types::personal_cloaking;
						} else {
							c->subaction = combat_unit::subaction_move;
							c->target_pos = stand_pos;
						}
					}
				}
			}
		}
	}
	if (my_completed_units_of_type[unit_types::nuclear_missile].size() > 0) {
		double nuke_range = players::my_player->has_upgrade(upgrade_types::ocular_implants) ? 32 * 10 : 32 * 8;
		nuke_test.clear();

		a_vector<unit*> nearby_enemies;
		a_vector<unit*> nearby_my_buildings;
		tsc::dynamic_bitset stand_visited((grid::build_grid_width / 2)*(grid::build_grid_height / 2));
		tsc::dynamic_bitset target_visited(grid::build_grid_width*grid::build_grid_height);
		for (auto*c : ghosts) {
			xy best_stand_pos;
			xy best_target_pos;
			double best_score = 0;

			nearby_enemies.clear();
			nearby_my_buildings.clear();
			for (unit*u : enemy_units) {
				if (u->gone) continue;
				if (diag_distance(u->pos - c->u->pos) <= 32 * 25) nearby_enemies.push_back(u);
			}
			for (unit*u : my_buildings) {
				if (diag_distance(u->pos - c->u->pos) <= 32 * 25) nearby_my_buildings.push_back(u);
			}
			
			find_nearby_entirely_walkable_tiles(c->u->pos, [&](xy tile_pos) {
				size_t stand_index = tile_pos.x / 64 + tile_pos.y / 64 * (grid::build_grid_width / 2);
				if (stand_visited.test(stand_index)) return true;
				stand_visited.set(stand_index);
				double d = diag_distance(tile_pos - c->u->pos);
				if (d >= 32 * 8) return false;
				xy stand_pos = tile_pos + xy(16, 16);
				bool in_attack_range = false;
				bool is_revealed = !c->u->cloaked;
				for (unit*e : nearby_enemies) {
					test_in_range(c, e, stand_pos, in_attack_range, is_revealed);
					if (in_attack_range && is_revealed) return false;
				}
				for (double ang = 0.0; ang < PI * 2; ang += PI / 4) {
					xy pos = stand_pos;
					pos.x += (int)(cos(ang)*nuke_range);
					pos.y += (int)(sin(ang)*nuke_range);
					if ((size_t)pos.x >= (size_t)grid::map_width || (size_t)pos.y >= (size_t)grid::map_height) continue;
					size_t target_index = grid::build_square_index(pos);
					if (target_visited.test(target_index)) continue;
					target_visited.set(target_index);
					double score = 0.0;
					for (unit*e : nearby_enemies) {
						// todo: use the actual blast radius of a nuke
						if (diag_distance(e->pos - pos) <= 32 * 7) {
							double damage = std::max(500.0, (e->shields + e->hp) * 2 / 3);
							double mult = 1.0;
							if (!e->type->is_building && e->stats->max_speed>1) mult = 1.0 / e->stats->max_speed;
							if (e->lockdown_timer || e->maelstrom_timer) mult = 1.0;
							if (e->type->is_worker) mult *= 2;
							if (e->shields + e->hp - damage > 0) mult = damage / (e->stats->shields + e->stats->hp);
							score += (e->minerals_value + e->gas_value)*mult;
						}
					}
					for (unit*e : nearby_my_buildings) {
						// todo: use the actual blast radius of a nuke
						if (diag_distance(e->pos - pos) <= 32 * 7) {
							double damage = std::max(500.0, (e->shields + e->hp) * 2 / 3);
							double mult = 1.0;
							if (e->shields + e->hp - damage > 0) mult = damage / (e->stats->shields + e->stats->hp);
							score -= (e->minerals_value + e->gas_value)*mult;
						}
					}
					nuke_test.emplace_back(pos, score);
					if (score > best_score) {
						best_score = score;
						best_stand_pos = stand_pos;
						best_target_pos = pos;
					}
				}
				return true;
			});
			//log("nuke: best pos is %d %d -> %d %d with score %g\n", best_stand_pos.x, best_stand_pos.y, best_target_pos.x, best_target_pos.y, best_score);
			if (best_score >= 400.0) {
				log("nuking %d %d -> %d %d with score %g\n", best_stand_pos.x, best_stand_pos.y, best_target_pos.x, best_target_pos.y, best_score);
				bool okay = true;
				if (players::my_player->has_upgrade(upgrade_types::personal_cloaking)) {
					if (!c->u->cloaked) {
						c->subaction = combat_unit::subaction_use_ability;
						c->ability = upgrade_types::personal_cloaking;
						okay = false;
					}
				}
				if (okay) {
					if (diag_distance(c->u->pos - best_stand_pos) > 32) {
						c->subaction = combat_unit::subaction_move;
						c->target_pos = best_stand_pos;
					} else {
						c->subaction = combat_unit::subaction_use_ability;
						c->ability = upgrade_types::nuclear_strike;
						c->target_pos = best_target_pos;
						c->last_nuke = current_frame;
					}
				}
			}
		}
	}

	a_vector<combat_unit*> science_vessels;
	for (auto*c : live_combat_units) {
		if (current_frame < c->strategy_busy_until) continue;
		if (c->reference_count) continue;
		if (c->u->type == unit_types::science_vessel) science_vessels.push_back(c);
	}

	if (players::my_player->has_upgrade(upgrade_types::irradiate)) {
		a_unordered_set<unit*> target_taken;
		int num_with_100_energy = 0;
		int irradiated_count = 0;
		for (auto*c : science_vessels) {
			if (c->u->energy >= 100) ++num_with_100_energy;
			if (c->u->irradiate_timer) ++irradiated_count;
		}
		for (auto*c : science_vessels) {
			if (c->u->energy < 75) continue;
			if (c->last_win_ratio >= 4.0 && c->u->energy < c->u->stats->energy) continue;
			double air_dpf = 0.0;
			double non_burrowed_hp = 0.0;
			unit*target = get_best_score(enemy_units, [&](unit*e) {
				if (!e->visible || !e->detected) return std::numeric_limits<double>::infinity();
				if (e->building) return std::numeric_limits<double>::infinity();
				if (e->type == unit_types::egg || e->type == unit_types::lurker_egg) return std::numeric_limits<double>::infinity();
				if (e->stasis_timer) return std::numeric_limits<double>::infinity();
				if (e->irradiate_timer) return std::numeric_limits<double>::infinity();
				if (!e->type->is_biological) return std::numeric_limits<double>::infinity();
				if (e->type == unit_types::lurker && c->last_win_ratio >= 2.0 && c->u->energy < c->u->stats->energy) return std::numeric_limits<double>::infinity();
				if (target_taken.count(e)) return std::numeric_limits<double>::infinity();
				double value = e->minerals_value + e->gas_value * 2;
				if (value < 200 && c->u->hp > c->u->stats->hp*0.5 && c->u->energy < c->u->stats->energy - 75) return std::numeric_limits<double>::infinity();
				double d = diag_distance(e->pos - c->u->pos);
				if (d > 32 * 20) return std::numeric_limits<double>::infinity();
				if (d < 32 * 9) d = 32 * 9;

				double ehp = e->shields + e->hp;
				ehp -= focus_fire[e];
				if (ehp < 0) ehp = 0;
				
				if (!e->burrowed) non_burrowed_hp += ehp;

				if (c->u->hp >= c->u->stats->hp*0.5 && c->u->energy < 150) {
					if (d > 32 * 12) return std::numeric_limits<double>::infinity();
					if (ehp < 100) return std::numeric_limits<double>::infinity();
				}

				if (e->stats->air_weapon) {
					d /= 2;
					weapon_stats*w = e->stats->air_weapon;
					double damage = w->damage;
					if (c->u->shields <= 0) damage *= combat_eval::get_damage_type_modifier(w->damage_type, c->u->stats->type->size);
					damage -= c->u->stats->armor;
					if (damage <= 0) damage = 1.0;
					damage *= w == e->stats->ground_weapon ? e->stats->ground_weapon_hits : e->stats->air_weapon_hits;
					air_dpf += damage / w->cooldown;
				}

				return d / value / (ehp / (e->stats->shields + e->stats->hp));
			}, std::numeric_limits<double>::infinity());
			//if ((air_dpf < 0.5 || air_dpf < num_with_100_energy * 2) && non_burrowed_hp >= 200) {
			//if (air_dpf < 0.5 && non_burrowed_hp - 400 * irradiated_count >= 200) {
// 			if (air_dpf < 0.125 && non_burrowed_hp - 400 * irradiated_count >= 200) {
// 				combat_unit*nearest_science_vessel = get_best_score(science_vessels, [&](combat_unit*c2) {
// 					if (c2 == c) return std::numeric_limits<double>::infinity();
// 					if (target_taken.count(c2->u)) return std::numeric_limits<double>::infinity();
// 					return diag_distance(c->u->pos - c2->u->pos);
// 				}, std::numeric_limits<double>::infinity());
// 				if (nearest_science_vessel && units_distance(c->u, nearest_science_vessel->u) <= 32 * 10) {
// 					target = nearest_science_vessel->u;
// 					++irradiated_count;
// 				}
// 			}
			if (target) {
				if (units_distance(c->u, target) <= 32 * 15) {
					target_taken.insert(target);
					c->subaction = combat_unit::subaction_idle;
					if (current_frame - c->last_used_special >= 8) {
						c->u->game_unit->useTech(upgrade_types::irradiate->game_tech_type, target->game_unit);
						c->last_used_special = current_frame;
						c->u->controller->noorder_until = current_frame + 8;
					}
				}
			}
		}
	}


	if (!science_vessels.empty() && (players::opponent_player->race != race_zerg || !players::my_player->has_upgrade(upgrade_types::irradiate))) {
		a_vector<combat_unit*> wants_defensive_matrix;
		for (auto*c : live_combat_units) {
			if (current_frame - c->last_nuke <= 15 * 14 || (c->u->irradiate_timer && c->u->is_flying && c->u->hp < c->u->stats->hp)) {
				if (c->u->cloaked) {
					bool in_attack_range = false;
					bool is_revealed = false;
					for (unit*e : enemy_units) {
						if (e->gone) continue;
						test_in_range(c, e, c->u->pos, in_attack_range, is_revealed);
						if (in_attack_range && is_revealed) break;
					}
					if (!is_revealed) continue;
				}
				wants_defensive_matrix.push_back(c);
			}
		}
		a_unordered_set<combat_unit*> target_taken;
		for (auto*c : science_vessels) {
			if (c->u->energy < 100) continue;
			combat_unit*target = get_best_score(wants_defensive_matrix, [&](combat_unit*target) {
				if (target->u->defensive_matrix_timer) return std::numeric_limits<double>::infinity();
				if (target->u->stasis_timer) return std::numeric_limits<double>::infinity();
				if (target_taken.count(target)) return std::numeric_limits<double>::infinity();
				return diag_distance(c->u->pos - target->u->pos);
			}, std::numeric_limits<double>::infinity());
			//if (!target && (c->u->energy >= 180 || c->u->hp < c->u->stats->hp / 2 || c->last_win_ratio < 2.0)) {
			if (!target) {
				target = get_best_score(live_combat_units, [&](combat_unit*target) {
					if (target->u->defensive_matrix_timer && target->u->defensive_matrix_hp > 40) return std::numeric_limits<double>::infinity();
					if (target->u->stasis_timer) return std::numeric_limits<double>::infinity();
					if (target->last_win_ratio >= 2.0) return std::numeric_limits<double>::infinity();
					if (units_distance(target->u, c->u) > 32 * 11) return std::numeric_limits<double>::infinity();
					int n = 0;
					for (unit*e : enemy_units) {
						if (!e->visible) continue;
						weapon_stats*w = target->u->is_flying ? e->stats->air_weapon : e->stats->ground_weapon;
						if (!w) continue;
						if (units_distance(target->u, e) <= w->max_range) ++n;
					}
					if (n == 0) return std::numeric_limits<double>::infinity();
					return (double)-n + (target->u->minerals_value + target->u->gas_value) / 100;
				}, std::numeric_limits<double>::infinity());
			}
			if (target) {
				target_taken.insert(target);
				if (current_frame - c->last_used_special >= 8) {
					c->u->game_unit->useTech(upgrade_types::defensive_matrix->game_tech_type, target->u->game_unit);
					c->last_used_special = current_frame;
					c->u->controller->noorder_until = current_frame + 8;
				}
			}
		}
	}

	a_vector<combat_unit*> overlords;
	for (auto*c : live_combat_units) {
		if (current_frame < c->strategy_busy_until) continue;
		if (c->reference_count) continue;
		if (c->u->type == unit_types::overlord) overlords.push_back(c);
	}
	if (!overlords.empty()) {
		if (!need_detector.empty()) {
			log("need_detector.size() is %d\n", need_detector.size());
			a_unordered_set<combat_unit*> ov_sent;
			a_vector<xy> pos_sent;
			for (auto&v : need_detector) {
				xy pos = std::get<0>(v)->u->pos;
				bool covered = false;
				for (xy pos2 : pos_sent) {
					if (diag_distance(pos - pos2) <= 32 * 4) {
						covered = true;
						break;
					}
				}
				if (covered) break;
				auto*c = get_best_score(overlords, [&](combat_unit*c) {
					if (ov_sent.count(c)) return std::numeric_limits<double>::infinity();
					return diag_distance(pos - c->u->pos);
				}, std::numeric_limits<double>::infinity());
				if (c) {
					c->action = combat_unit::action_offence;
					c->subaction = combat_unit::subaction_move;
					c->target_pos = pos;
					ov_sent.insert(c);
					pos_sent.push_back(pos);
					log("sent overlord to %d %d\n", pos.x, pos.y);
				}
			}
		}
	}

	if (!my_completed_units_of_type[unit_types::scv].empty() && current_frame < 15 * 60 * 12) {
	//if (!my_completed_units_of_type[unit_types::scv].empty()) {
		a_vector<combat_unit*> scvs;
		int non_scv_count = 0;
		for (auto*a : live_combat_units) {
			if (current_frame < a->strategy_busy_until) continue;
			if (a->reference_count) continue;
			if (a->u->type == unit_types::scv) {
				if (current_used_total_supply < 80 || !entire_threat_area.test(grid::build_square_index(a->u->pos))) {
					scvs.push_back(a);
				}
			} else ++non_scv_count;
		}
		if (!scvs.empty()) scvs.pop_back();
		for (auto*c : live_combat_units) {
			if (current_frame < c->strategy_busy_until) continue;
			if (c->reference_count) continue;
			if (c->subaction == combat_unit::subaction_repair) {
				c->u->controller->action = unit_controller::action_idle;
				c->subaction = combat_unit::action_idle;
			}
		}
		unit*prio_unit = nullptr;
		int prio_repair_n = std::min((int)current_used_total_supply / 10, 4);
		auto prio_score_func = [&](unit*u) {
			int inrange = 0;
			double ned = get_best_score_value(enemy_units, [&](unit*e) {
				if (e->gone) return std::numeric_limits<double>::infinity();
				weapon_stats*ew = u->is_flying ? e->stats->air_weapon : e->stats->ground_weapon;
				if (!ew) return std::numeric_limits<double>::infinity();
				if (e->type->is_worker || e->type == unit_types::marine) return std::numeric_limits<double>::infinity();
				if (current_frame - e->last_seen >= 15 * 20) return std::numeric_limits<double>::infinity();
				if (e->is_flying) {
					if (u->type != unit_types::goliath && u->type != unit_types::missile_turret) return std::numeric_limits<double>::infinity();
				}
				if (!square_pathing::unit_can_reach(u, u->pos, e->pos, square_pathing::pathing_map_index::include_liftable_wall)) {
					if (units_distance(u, e) > ew->max_range) return std::numeric_limits<double>::infinity();
				}
				double d = diag_distance(e->pos - u->pos);
				if (d < 32 * 30) ++inrange;
				return d;
			});
			if (ned >= 32 * 30 || inrange < 3) return std::numeric_limits<double>::infinity();
			if (u->type == unit_types::bunker) {
				int marines_nearby = 0;
				double nm = get_best_score_value(my_units_of_type[unit_types::marine], [&](unit*m) {
					double d = diag_distance(m->pos - u->pos);
					if (d <= 32 * 12) ++marines_nearby;
					return d;
				}, std::numeric_limits<double>::infinity());
				if (nm >= 32 * 25) return std::numeric_limits<double>::infinity();
				if (marines_nearby <= 1) return std::numeric_limits<double>::infinity();
			}
			return ned;
		};
		if (current_used_total_supply - my_workers.size() < 30 && no_aggressive_groups) {
			prio_unit = get_best_score(my_completed_units_of_type[unit_types::bunker], prio_score_func, std::numeric_limits<double>::infinity());
			if (!prio_unit) prio_unit = get_best_score(my_completed_units_of_type[unit_types::siege_tank_tank_mode], prio_score_func, std::numeric_limits<double>::infinity());
			if (!prio_unit) prio_unit = get_best_score(my_completed_units_of_type[unit_types::siege_tank_siege_mode], prio_score_func, std::numeric_limits<double>::infinity());

			if (!prio_unit && my_completed_units_of_type[unit_types::bunker].size() == 1) {
				prio_unit = my_completed_units_of_type[unit_types::bunker].front();
			}

			if (!prio_unit) {
				a_vector<unit*> wall_buildings;
				for (auto*w : wall_in::active_walls) {
					if (w->is_walled_off) {
						for (xy pos : w->buildings_pos) {
							auto&bs = grid::get_build_square(pos);
							if (bs.building) wall_buildings.push_back(bs.building);
						}
					}
				}
				prio_unit = get_best_score(wall_buildings, [&](unit*u) {
					return u->hp / u->stats->hp;
				}, 1.0);
			}

			//if (prio_unit && !prio_unit->building && prio_unit->hp == prio_unit->stats->hp) prio_unit = nullptr;
			if (prio_unit && prio_unit->hp == prio_unit->stats->hp) {
				if (!prio_unit->building && current_frame - prio_unit->last_attacked > 120) prio_unit = nullptr;
				else {
					//if (my_resource_depots.size() >= 2) prio_unit = nullptr;
				}
			}
		}

		if (prio_unit) {
			double repair_rate = 0.75; // not the actual repair rate
			double dpf = 0.0;
			for (unit* e : enemy_units) {
				if (e->gone) continue;
				if (!e->stats->ground_weapon) continue;
				if (e->is_flying) continue;
				if (current_frame - e->last_seen >= 15 * 20) continue;
				if (diag_distance(e->pos - prio_unit->pos) >= 32 * 30) continue;
				weapon_stats*w = e->stats->ground_weapon;
				double damage = w->damage;
				if (prio_unit->shields <= 0) damage *= combat_eval::get_damage_type_modifier(w->damage_type, prio_unit->stats->type->size);
				damage -= prio_unit->stats->armor;
				if (damage <= 0) damage = 1.0;
				damage *= w == e->stats->ground_weapon ? e->stats->ground_weapon_hits : e->stats->air_weapon_hits;
				dpf += damage / w->cooldown;
			}
			////prio_repair_n = (int)(dpf / repair_rate) + 2;
			prio_repair_n = (int)std::ceil(dpf / repair_rate);
			if (dpf >= 2.0 || (scvs.size() >= 16 && my_completed_units_of_type[unit_types::siege_tank_tank_mode].size() + my_completed_units_of_type[unit_types::siege_tank_siege_mode].size() < 2)) {
				int n_near = 0;
				for (combat_unit* a: scvs) {
					if (diag_distance(a->u->pos - prio_unit->pos) <= 32 * 8) ++n_near;
				}
				if (n_near < 4) {
					prio_repair_n += 2;
				}
			}
			log("dpf %g, repair_rate %g, prio_repair_n %d\n", dpf, repair_rate, prio_repair_n);
			if (prio_repair_n > 6) prio_repair_n = 6;
		}
		if (!prio_unit) {
			if ((int)scvs.size() > non_scv_count + 1) scvs.resize(non_scv_count + 1);
			if (non_scv_count <= 2) scvs.clear();
		}
//		if (prio_unit) log("prio_unit is %s\n", prio_unit->type->name);
//		else log("prio_unit is null\n");
		//if (scvs.size() / 4 + 2 < scvs.size()) scvs.resize(scvs.size() / 4 + 2);
		for (unit*u : my_workers) {
			if (u->controller->action == unit_controller::action_repair) u->controller->action = unit_controller::action_idle;
		}
		a_vector<unit*> wants_repair;
		int worker_repair_count = 0;
		for (unit* u : my_units) {
			if (!u->is_completed) continue;
			if (!u->type->is_building && !u->type->is_mechanical) continue;
			if (u->controller->action == unit_controller::action_scout) continue;
			if (u->type->minerals_cost && current_minerals < 1) continue;
			if (u->type->gas_cost && current_gas < 5) continue;
			if (u->type->is_worker && ++worker_repair_count>1) continue;
			if (u->irradiate_timer) continue;
			if (u->type != unit_types::siege_tank_tank_mode && u->type != unit_types::siege_tank_siege_mode && !u->is_flying) {
				if (!my_base.test(grid::build_square_index(u->pos))) continue;
			}
			if (u != prio_unit && u->type == unit_types::bunker && u->loaded_units.empty() && entire_threat_area.test(grid::build_square_index(u->pos))) continue;
			if (u->type == unit_types::bunker) {
				double d = get_best_score_value(my_resource_depots, [&](unit*depot) {
					return diag_distance(depot->pos - u->pos);
				});
				if (d > 32 * 20 && diag_distance(my_closest_base - u->pos) >= 32 * 20) continue;
			}
			if (u->hp < u->stats->hp || u == prio_unit) wants_repair.push_back(u);
		}
		std::sort(wants_repair.begin(), wants_repair.end(), [&](unit* a, unit* b) {
			if (a->type == unit_types::bunker && b->type != unit_types::bunker) return true;
			if (b->type == unit_types::bunker && a->type != unit_types::bunker) return false;
			if (a->type == unit_types::missile_turret && b->type != unit_types::missile_turret) return true;
			if (b->type == unit_types::missile_turret && a->type != unit_types::missile_turret) return false;
			if (!a->building && b->building && b->hp > b->stats->hp / 2) return true;
			if (!b->building && a->building && a->hp > a->stats->hp / 2) return false;
			return a->minerals_value + a->gas_value > b->minerals_value + b->gas_value;
		});
		int scvs_assigned = 0;
		int max_scvs_assigned = (int)scvs.size() / 10 + 2;
		for (auto* u : wants_repair) {
			if (scvs.empty()) break;
			if (scvs_assigned >= max_scvs_assigned) break;
			auto*cu = &combat_unit_map[u];
			int max_n = 2;
			bool repair_flyer = u->is_flying && (u->hp <= u->stats->hp / 2 || cu->is_repairing);
			if (repair_flyer) max_n = 4;
			if (u->type == unit_types::siege_tank_tank_mode || u->type == unit_types::siege_tank_siege_mode) max_n = 5;
			if (u->building) max_n = 4;
			if (u == prio_unit) max_n = prio_repair_n;
			else if (u->type != unit_types::bunker && u->type != unit_types::missile_turret && !cu->is_repairing) {
				if (u->hp > u->stats->hp / 2) max_n = 1;
			}

			bool lurkers_attacking = false;
			for (unit*e : enemy_units) {
				if (!e->visible) continue;
				if (e->type == unit_types::lurker) {
					if (units_distance(u, e) <= 32 * 6) lurkers_attacking = true;
				}
				if (lurkers_attacking) break;
			}

			//auto*ug = u->building ? nullptr : unit_group[u];
			unit* moved_to = nullptr;
			for (int i = 0; i < max_n; ++i) {
				combat_unit*c = get_best_score(scvs, [&](combat_unit*c) {
					if (c->u == u) return std::numeric_limits<double>::infinity();
					//if (ug && unit_group[c->u] != ug) return std::numeric_limits<double>::infinity();
					
					if (!repair_flyer && !square_pathing::unit_can_reach(c->u, c->u->pos, u->pos)) return std::numeric_limits<double>::infinity();
					//if (u->type->is_worker && c->u->hp < c->u->stats->hp) return std::numeric_limits<double>::infinity();
					if (current_frame - c->last_run <= 15 * 2 && u != prio_unit) return std::numeric_limits<double>::infinity();

					if (lurkers_attacking && !c->u->defensive_matrix_timer) return std::numeric_limits<double>::infinity();

					double nearest_ally_distance = get_best_score_value(my_units, [&](unit*u) {
						if (u->building) return std::numeric_limits<double>::infinity();
						if (!u->stats->ground_weapon) return std::numeric_limits<double>::infinity();
						return diag_distance(u->pos - c->u->pos);
					}, std::numeric_limits<double>::infinity());
					double d = diag_distance(u->pos - c->u->pos);
					if (nearest_ally_distance > d + 32 * 5) return std::numeric_limits<double>::infinity();

					return d;
				}, std::numeric_limits<double>::infinity());
				if (!c) break;
				c->subaction = combat_unit::subaction_repair;
				c->target = u;
				find_and_erase(scvs, c);
				++scvs_assigned;

				bool move = false;
				if (repair_flyer || (u->is_flying && units_distance(c->u, u) <= 32 * 4) || cu->is_repairing) {
					double d = units_distance(c->u, u);
					if (d > 32 || cu->is_repairing) {
// 						if (d <= 32 && (cu->subaction == combat_unit::subaction_fight || cu->subaction == combat_unit::subaction_kite)) {
// 							weapon_stats*w = cu->target->is_flying ? u->stats->air_weapon : u->stats->ground_weapon;
// 							if (!w || units_distance(u, cu->target) > w->max_range) {
// 								move = true;
// 							}
// 						} else {
// 							move = true;
// 						}
						move = true;
						cu->is_repairing = u->hp < u->stats->hp*(1.0 - 1.0 / 64);
					}
				}
				if (c->u > moved_to && (u->type != unit_types::battlecruiser || current_frame - cu->last_fight >= 15 * 5)) {
					moved_to = c->u;
					if (move && current_frame > cu->strategy_busy_until && !cu->reference_count) {
						cu->subaction = combat_unit::subaction_move_directly;
						cu->target_pos = c->u->pos;
					}
				}
			}
		}
	} else {
		for (auto*c : live_combat_units) {
			if (current_frame < c->strategy_busy_until) continue;
			if (c->reference_count) continue;
			if (c->subaction == combat_unit::subaction_repair) {
				c->u->controller->action = unit_controller::action_idle;
				c->subaction = combat_unit::action_idle;
			}
		}
	}
	
	if (!my_completed_units_of_type[unit_types::scv].empty()) {
		auto& scvs = my_completed_units_of_type[unit_types::scv];
		for (auto* a : live_combat_units) {
			if (current_frame < a->strategy_busy_until) continue;
			if (a->reference_count) continue;
			if (a->u->type->is_building || !a->u->type->is_mechanical) continue;
			if (a->u->hp == a->u->stats->hp) continue;
			if (a->u->type->is_worker) continue;
			if (a->u->type->minerals_cost && current_minerals < 1) continue;
			if (a->u->type->gas_cost && current_gas < 5) continue;
			if (a->u->irradiate_timer) continue;
			if (a->u->hp >= a->u->stats->hp * 0.5 && current_frame - a->last_going_for_repair > 90) continue;
			if (entire_threat_area.test(grid::build_square_index(a->u->pos))) continue;
			
			unit* scv = current_frame - a->last_going_for_repair < 90 ? a->going_for_repair_scv : nullptr;
			if (scv && (scv->dead || scv->controller->action != unit_controller::action_gather || !square_pathing::unit_can_reach(a->u, a->u->pos, scv->pos))) scv = nullptr;
			if (!scv || current_frame - a->last_going_for_repair >= 15) {
				scv = get_best_score(scvs, [&](unit* u) {
					if (u->controller->action != unit_controller::action_gather) return std::numeric_limits<double>::infinity();	
					if (!a->u->is_flying && !square_pathing::unit_can_reach(a->u, a->u->pos, u->pos)) return std::numeric_limits<double>::infinity();
					return unit_pathing_distance(a->u, u->pos);
				}, std::numeric_limits<double>::infinity());
				if (!scv) continue;
				a->last_going_for_repair = current_frame;
			}
			
			a->going_for_repair_scv = scv;
			
			a->subaction = combat_unit::subaction_move;
			a->target_pos = scv->pos;
			
			unit* in_range_scv = get_best_score(scvs, [&](unit* u) {
				if (u->controller->action != unit_controller::action_gather && u->controller->action != unit_controller::action_repair) return std::numeric_limits<double>::infinity();
				if (units_distance(a->u, u) > 5) return std::numeric_limits<double>::infinity();
				return 0.0;
			}, std::numeric_limits<double>::infinity());
			if (in_range_scv) a->target_pos = a->u->pos;
			
			game->drawLineMap(a->u->pos.x, a->u->pos.y, scv->pos.x, scv->pos.y, BWAPI::Colors::Purple);
		}
	}

	auto scout_around = [&](combat_unit*a) {
		bool find_new_path = false;
		if (current_frame - a->last_find_path >= 15 * 60) find_new_path = true;
		if (a->path.empty() && current_frame - a->last_find_path >= 15 * 5) find_new_path = true;
		for (xy p : a->path) {
			if (entire_threat_area.test(grid::build_square_index(p))) {
				find_new_path = true;
				break;
			}
		}
		if (find_new_path) {
			a->last_find_path = current_frame;
			struct node_data_t {
				double len = 0;
			};
			int iterations = 0;
			a_deque<xy> path = find_path<node_data_t>(a->u->type, a->u->pos, [&](xy pos, xy npos, node_data_t&n) {
				if (++iterations % 1024 == 0) multitasking::yield_point();
				if (entire_threat_area.test(grid::build_square_index(npos))) return false;
				n.len += diag_distance(npos - pos);
				return true;
			}, [&](xy pos, xy npos, const node_data_t&n) {
				return 0.0;
			}, [&](xy pos, const node_data_t&n) {
				if (n.len >= 32 * 60) return true;
				return current_frame - grid::get_build_square(pos).last_seen >= 15 * 60;
			});
			log("found new path - %d nodes\n", path.size());
			a->path = std::move(path);
		}
		while (!a->path.empty() && diag_distance(a->u->pos - a->path.front()) <= 32 * 3) a->path.pop_front();
		if (!a->path.empty()) {
			xy move_to = square_pathing::get_go_to_along_path(a->u, a->path);
			a->subaction = combat_unit::subaction_move;
			a->target_pos = move_to;
		}
	};

	if (no_aggressive_groups && current_used_total_supply < 120 && !no_scout_around) {
		for (auto&g : groups) {
			if (!g.is_aggressive_group) continue;
			for (auto*a : g.allies) {
				if (current_frame - a->last_fight < 15 * 3 && current_frame - a->last_run >= 15 * 3) continue;
				if (current_frame - a->last_run >= 15 * 20) continue;
				if (a->subaction != combat_unit::subaction_move && a->subaction != combat_unit::subaction_idle) continue;
				if (current_frame < a->strategy_busy_until) continue;
				if (a->reference_count) continue;
				scout_around(a);
			}
		}
	}

	for (auto&g : groups) {
		if (!g.is_aggressive_group) continue;
		for (auto* a : g.allies) {
			if (!a->assigned_control_spot || !a->assigned_control_spot->is_map_split_center) continue;
			if (a->u->type != unit_types::vulture) continue;
			if (current_frame - a->last_fight < 15 * 3) continue;
			if (current_frame - a->last_run < 15 * 3) continue;
			if (a->subaction != combat_unit::subaction_move && a->subaction != combat_unit::subaction_idle) continue;
			if (a->subaction == combat_unit::subaction_move && a->target_pos != a->assigned_control_spot->pos) continue;
			if (current_frame < a->strategy_busy_until) continue;
			if (a->reference_count) continue;

			if (current_frame - a->last_find_border_scout >= 15 * 30 || (current_frame - a->last_find_border_scout >= 15 * 5 && diag_distance(a->u->pos - a->border_scout_pos) <= 32 * 4)) {
				a->last_find_border_scout = current_frame;
				xy scout_pos;
				if (!a->u->is_flying) {
					xy best_pos;
					double best_score = std::numeric_limits<double>::infinity();
					find_nearby_entirely_walkable_tiles(a->u->pos, [&](xy npos) {
						size_t index = grid::build_square_index(npos);
						if (map_split_op_area.test(index)) return false;
						if (!map_split_border_area.test(index)) return true;
						auto& bs = grid::get_build_square(npos);
						int age = current_frame - bs.last_seen;
						if (age < 15 * 10) return true;
						double s = diag_distance(a->u->pos - npos) / age;
						if (s < best_score) {
							best_score = s;
							best_pos = npos;
						}
						return true;
					}, 32 * 128);
					multitasking::yield_point();
					scout_pos = best_pos;
				} else {
					if (!map_split_border_locations.empty()) {
						scout_pos = map_split_border_locations[tsc::rng(map_split_border_locations.size())];
					}
				}
				a->border_scout_pos = scout_pos;
			}
			if (a->border_scout_pos != xy()) {
				a->subaction = combat_unit::subaction_move;
				a->target_pos = a->border_scout_pos;
			}
		}
	}

// 	if (no_aggressive_groups && aggressive_zerglings && false) {
// 		for (auto&g : groups) {
// 			if (!g.is_aggressive_group) continue;
// 			for (auto*a : g.allies) {
// 				if (a->u->type != unit_types::zergling) continue;
// 				if (current_frame - a->last_fight < 15 * 3 && current_frame - a->last_run >= 15 * 3) continue;
// 				if (current_frame - a->last_run >= 15 * 20) continue;
// 				if (a->subaction != combat_unit::subaction_move && a->subaction != combat_unit::subaction_idle) continue;
// 				scout_around(a);
// 			}
// 		}
// 	}

	if (search_and_destroy) {
		for (auto*a : live_combat_units) {
			if (current_frame < a->strategy_busy_until) continue;
			if (a->reference_count) continue;
			scout_around(a);
		}
	}

	for (auto*a : live_combat_units) {
		if (current_frame < a->strategy_busy_until) continue;
		if (a->reference_count) continue;
		if (a->u->loaded_into && current_frame - a->last_fight >= 15 * 5 && current_frame - a->last_given_a_lift >= 15 * 5) {
			log("unload because idle\n");
			if (a->u->loaded_into->game_unit->unload(a->u->game_unit)) {
				a->u->controller->noorder_until = current_frame + 4;
			}
		}
	}

	for (auto*a : live_combat_units) {
		if (current_frame < a->strategy_busy_until) continue;
		if (a->reference_count) continue;
		if (a->maybe_stuck && (a->subaction != combat_unit::subaction_fight || !a->target || units_distance(a->target, a->u) >= 32 * 15)) {
			unit*t = a->stuck_target;
			if (!t || t->dead || t->gone || t->invincible || (t->cloaked && !t->detected) || current_frame - a->last_used_special >= 15 * 10) {
				a->last_used_special = current_frame;
				t = get_best_score(live_units, [&](unit*u) {
					if (u->gone || u->invincible || (u->cloaked && !u->detected)) return std::numeric_limits<double>::infinity();
					if (u->owner == players::my_player) return std::numeric_limits<double>::infinity();
					double d = diag_distance(a->u->pos - u->pos);
					if (d >= 32 * 10) return std::numeric_limits<double>::infinity();
					return d;
				}, std::numeric_limits<double>::infinity());
				a->stuck_target = t;
			}
			if (t) {
				a->subaction = combat_unit::subaction_fight;
				a->target = t;
			}
		}
	}

// 	if (!my_completed_units_of_type[unit_types::overlord].empty()) {
// 		for (auto*a : live_combat_units) {
// 			if (current_frame < a->strategy_busy_until) continue;
//          if (a->reference_count) continue;
// 			if (a->u->type != unit_types::overlord) continue;
// 			a->action = combat_unit::action_offence;
// 			a->subaction = combat_unit::subaction_idle;
// 			a->u->controller->action = unit_controller::action_idle;
// 		}
// 	}

	if (!my_completed_units_of_type[unit_types::wraith].empty()) {

		a_vector<combat_unit*> wraiths;
		for (auto*c : live_combat_units) {
			if (current_frame < c->strategy_busy_until) continue;
			if (c->reference_count) continue;
			if (c->u->type == unit_types::wraith) wraiths.push_back(c);
		}

		if (hide_wraiths) {
			unit*hide_at_building = get_best_score(my_buildings, [&](unit*u) {
				return get_best_score_value(enemy_units, [&](unit*e) {
					if (e->gone) return std::numeric_limits<double>::infinity();
					return -diag_distance(e->pos - u->pos);
				});
			});
			if (hide_at_building) {
				for (auto*a : wraiths) {
					a->action = combat::combat_unit::action_offence;
					a->subaction = combat::combat_unit::subaction_move;
					a->target_pos = hide_at_building->pos;
				}
			}
		}

		if (players::my_player->has_upgrade(upgrade_types::cloaking_field)) {
			for (auto*c : wraiths) {
				if (c->u->hp < 100 || current_frame - c->last_run <= 90) {
					if (!c->u->cloaked && c->u->energy > 50) {
						c->subaction = combat_unit::subaction_use_ability;
						c->ability = upgrade_types::cloaking_field;
					}
				}
			}
		}

	}

	if (defensive_spider_mine_count > 0) {
		int n_left = defensive_spider_mine_count;
		a_unordered_set<grid::build_square*> bs_taken;
		for (combat_unit*c : live_combat_units) {
			if (n_left <= 0) break;
			if (current_frame < c->strategy_busy_until) continue;
			if (c->reference_count) continue;
			if (c->subaction != combat_unit::subaction_move) continue;
			if (current_frame - c->last_run <= 15 * 5) continue;
			if (players::my_player->has_upgrade(upgrade_types::spider_mines) && c->u->spider_mine_count) {

				auto*bs = get_best_score(defence_choke.outside_squares, [&](grid::build_square*bs) {
					if (bs->building) return std::numeric_limits<double>::infinity();
					if (bs_taken.count(bs)) return std::numeric_limits<double>::infinity();
					xy pos = bs->pos + xy(16, 16);
					double r = 0.0;
					for (unit*u : my_units_of_type[unit_types::spider_mine]) {
						if (units_distance(u->pos, u->type, pos, unit_types::spider_mine) <= 32 * 3) {
							r += 128.0;
						}
					}
					for (auto*bst : bs_taken) {
						if (units_distance(bst->pos + xy(16, 16), unit_types::spider_mine, pos, unit_types::spider_mine) <= 32 * 3) {
							r += 128.0;
						}
					}
					r += diag_distance(pos - defence_choke.center);
					return r;
				});
				if (bs) {
					--n_left;
					bs_taken.insert(bs);
					xy pos = bs->pos + xy(16, 16);
					if (diag_distance(c->u->pos - pos) > 16) {
						c->subaction = combat_unit::subaction_move;
						c->target_pos = pos;
					} else {
						if (current_frame - c->last_used_special >= 15) {
							c->last_used_special = current_frame;
							c->subaction = combat_unit::subaction_use_ability;
							c->ability = upgrade_types::spider_mines;
							--defensive_spider_mine_count;
						}
					}
				}
			}
		}
	}

	if (true) {
		
		for (combat_unit* c : live_combat_units) {
			if (!c->assigned_control_spot) continue;
			if (current_frame < c->strategy_busy_until) continue;
			if (c->reference_count) continue;
			if (c->subaction != combat_unit::subaction_move) continue;
			if (current_frame - c->last_run <= 15 * 5) continue;

			auto* cs = c->assigned_control_spot;
			if (!cs->area.test(grid::build_square_index(c->u->pos))) continue;
			if (cs->is_best_defence) continue;
			

			c->subaction = combat_unit::subaction_move;
			c->target_pos = cs->pos;

			if (c->u->spider_mine_count && players::my_player->has_upgrade(upgrade_types::spider_mines)) {

				int n_mines = 0;
				for (unit* u : my_units_of_type[unit_types::spider_mine]) {
					if (cs->area.test(grid::build_square_index(u->pos))) {
						++n_mines;
					}
				}

				auto* bs = get_best_score(cs->squares, [&](grid::build_square* bs) {
					if (bs->building) return std::numeric_limits<double>::infinity();
					xy pos = bs->pos + xy(16, 16);
					double r = 0.0;
					for (unit* u : my_units_of_type[unit_types::spider_mine]) {
						if (diag_distance(pos - u->pos) <= 32 * 3) {
							r += 128.0;
						}
					}
					if (n_mines % 3 == 1) r += diag_distance(pos - cs->incoming_pos);
					else r += diag_distance(pos - cs->pos);
					return r;
				});

				if (bs && n_mines < 8) {
					xy pos = bs->pos + xy(16, 16);
					if (diag_distance(c->u->pos - pos) > 16) {
						c->subaction = combat_unit::subaction_move;
						c->target_pos = pos;
					} else {
						if (current_frame - c->last_used_special >= 15) {
							c->last_used_special = current_frame;
							c->subaction = combat_unit::subaction_use_ability;
							c->ability = upgrade_types::spider_mines;
						}
					}
				}

			}
		}

	}

	if (players::my_player->has_upgrade(upgrade_types::yamato_gun)) {
		a_vector<combat_unit*> battlecruisers;
		for (auto*c : live_combat_units) {
			if (current_frame < c->strategy_busy_until) continue;
			if (c->reference_count) continue;
			if (c->u->type == unit_types::battlecruiser) battlecruisers.push_back(c);
		}
		for (auto*c : battlecruisers) {
			if (c->u->energy < 150) continue;
			auto yamato_damage = [&](unit*target) {
				double damage = 260;
				if (target->shields <= 0) damage *= combat_eval::get_damage_type_modifier(weapon_stats::damage_type_explosive, target->stats->type->size);
				damage -= target->stats->armor;
				if (damage <= 0) damage = 1.0;
				return damage;
			};
			if (current_frame >= c->registered_focus_fire_until) {
				auto i = registered_focus_fire.find(c);
				if (i != registered_focus_fire.end()) {
					if (std::abs(focus_fire[std::get<0>(i->second)] -= std::get<1>(i->second)) < 1) {
						focus_fire.erase(std::get<0>(i->second));
					}
					registered_focus_fire.erase(i);
				}
			}
			//bool kill_turrets = current_frame - c->last_fight < 90 && c->last_win_ratio < 2.0;
			bool kill_turrets = true;
			unit* target = get_best_score(enemy_units, [&](unit*e) {
				if (!e->visible || !e->detected) return std::numeric_limits<double>::infinity();
				if (e->building && !e->stats->air_weapon && !e->stats->ground_weapon && (e->type != unit_types::bunker || e->marines_loaded == 0)) return std::numeric_limits<double>::infinity();
				if (e->lockdown_timer > 15 * 10) return std::numeric_limits<double>::infinity();
				if (e->maelstrom_timer > 15 * 10) return std::numeric_limits<double>::infinity();
				if (e->stasis_timer) return std::numeric_limits<double>::infinity();
				double value = e->minerals_value + e->gas_value * 2;
				if (kill_turrets && e->type == unit_types::missile_turret && e->hp > e->stats->hp / 2) value = 200.0;
				if (value < 200 && c->u->hp > c->u->stats->hp*0.25) return std::numeric_limits<double>::infinity();
				double d = diag_distance(e->pos - c->u->pos);
				if (d > 32 * 15) return std::numeric_limits<double>::infinity();
				if (d < 32 * 10) d = 32 * 10;

				double ehp = e->shields + e->hp;
				ehp -= focus_fire[e];
				if (ehp < 0) ehp = 0;
				ehp /= combat_eval::get_damage_type_modifier(weapon_stats::damage_type_explosive, e->stats->type->size);

				if (c->u->hp >= c->u->stats->hp*0.5) {
					if (d > 32 * 10) return std::numeric_limits<double>::infinity();
					if (ehp < 100) return std::numeric_limits<double>::infinity();
				}

				return d / value / (ehp / (e->stats->shields + e->stats->hp));
			}, std::numeric_limits<double>::infinity());
			if (target) {
				if (units_distance(c->u, target) <= 32 * 10) {
					c->subaction = combat_unit::subaction_idle;
					if (current_frame - c->last_used_special >= 15 * 5) {
						c->u->game_unit->useTech(upgrade_types::yamato_gun->game_tech_type, target->game_unit);
						c->last_used_special = current_frame;
						c->u->controller->noorder_until = current_frame + 15 * 10;

						double damage = yamato_damage(target);
						focus_fire[target] += damage;
						auto i = registered_focus_fire.find(c);
						if (i != registered_focus_fire.end()) {
							if (std::abs(focus_fire[std::get<0>(i->second)] -= std::get<1>(i->second)) < 1) {
								focus_fire.erase(std::get<0>(i->second));
							}
						}
						registered_focus_fire[c] = std::make_tuple(target, damage, current_frame + 15 * 10);
						c->registered_focus_fire_until = current_frame + 15 * 10;
					}
				}
			}
		}
	}


	if (players::my_player->has_upgrade(upgrade_types::optical_flare)) {
		a_vector<combat_unit*> medics;
		for (auto*c : live_combat_units) {
			if (current_frame < c->strategy_busy_until) continue;
			if (c->reference_count) continue;
			if (c->u->type == unit_types::medic) medics.push_back(c);
		}
		for (auto*c : medics) {
			if (c->u->energy < 75 + 40) continue;
			if (c->last_win_ratio >= 8.0) continue;
			unit*target = get_best_score(enemy_units, [&](unit*e) {
				if (!e->visible || !e->detected) return std::numeric_limits<double>::infinity();
				if (e->building) return std::numeric_limits<double>::infinity();
				if (e->stasis_timer) return std::numeric_limits<double>::infinity();
				if (e->is_blind) return std::numeric_limits<double>::infinity();
				double d = diag_distance(e->pos - c->u->pos);
				if (d > 32 * 9) return std::numeric_limits<double>::infinity();

				weapon_stats*gw = e->stats->ground_weapon;
				if (gw && gw->max_range <= 32 * 5) return std::numeric_limits<double>::infinity();

				auto it = optical_flare_target_taken.find(e);
				if (it != optical_flare_target_taken.end() && std::get<0>(it->second) != c) return std::numeric_limits<double>::infinity();

				return d / e->stats->sight_range / (e->hp / e->stats->hp);
			}, std::numeric_limits<double>::infinity());
			if (target) {
				if (units_distance(c->u, target) <= 32 * 9) {
					optical_flare_target_taken[target] = std::make_tuple(c, current_frame + 30);
					c->subaction = combat_unit::subaction_idle;
					if (current_frame - c->last_used_special >= 8) {
						c->u->game_unit->useTech(upgrade_types::optical_flare->game_tech_type, target->game_unit);
						c->last_used_special = current_frame;
						c->u->controller->noorder_until = current_frame + 8;
					}
				}
			}
		}
	}

	if (!my_units_of_type[unit_types::queen].empty()) {
		bool has_parasite = players::my_player->has_upgrade(upgrade_types::parasite);
		bool has_spawn_broodling = players::my_player->has_upgrade(upgrade_types::spawn_broodling);
		bool has_ensnare = players::my_player->has_upgrade(upgrade_types::ensnare);
		if (has_parasite || has_spawn_broodling || has_ensnare) {
			a_vector<combat_unit*> queens;
			for (auto*c : live_combat_units) {
				if (current_frame < c->strategy_busy_until) continue;
				if (c->reference_count) continue;
				if (c->u->type == unit_types::queen) queens.push_back(c);
			}
			int total_available_spawn_broodlings = 0;
			for (auto*c : queens) {
				if (c->u->energy >= 150) ++total_available_spawn_broodlings;
			}
			if (has_parasite && false) {
				for (auto*c : queens) {
					if (c->u->energy < 75) continue;
					if (has_spawn_broodling && c->u->energy < 200) continue;
					unit*target = get_best_score(enemy_units, [&](unit*u) {
						if (!u->visible || !u->detected) return std::numeric_limits<double>::infinity();
						if (u->building) return std::numeric_limits<double>::infinity();
						if (u->has_parasite) return std::numeric_limits<double>::infinity();

						double r = units_distance(u, c->u) - 32 * 12;
						if (r >= 32 * 3) return std::numeric_limits<double>::infinity();
						//if (c->u->hp < c->u->stats->hp && r > 0) return std::numeric_limits<double>::infinity();

						double value = u->minerals_value + u->gas_value;
						if (value < 200 && has_spawn_broodling) return std::numeric_limits<double>::infinity();
						if (c->u->energy < 125 && value < 125) return std::numeric_limits<double>::infinity();
						if (u->type == unit_types::siege_tank_tank_mode || u->type == unit_types::siege_tank_siege_mode) value += 200;
						if (u->type == unit_types::science_vessel) value += 1000;
						if (u->is_flying) value *= 2;
						if (r < 1) r = 1;

						auto it = parasite_target_taken.find(u);
						if (it != parasite_target_taken.end() && std::get<0>(it->second) != c) return std::numeric_limits<double>::infinity();

						if (disable_target_taken.find(u) != disable_target_taken.end()) return std::numeric_limits<double>::infinity();

						return r / value / ((u->shields + u->hp) / (u->stats->shields + u->stats->hp));
					}, std::numeric_limits<double>::infinity());
					if (target) {
						double r = units_distance(c->u, target);
						if (r <= 32 * 12) {
							parasite_target_taken[target] = std::make_tuple(c, current_frame + 60);
							c->subaction = combat_unit::subaction_idle;
							if (current_frame - c->last_used_special >= 45) {
								c->u->game_unit->useTech(upgrade_types::parasite->game_tech_type, target->game_unit);
								c->last_used_special = current_frame;
								c->u->controller->noorder_until = current_frame + 45;
							}
						} else {
							xy stand_pos;
							xy relpos = c->u->pos - target->pos;
							double ang = std::atan2(relpos.y, relpos.x);
							stand_pos.x = target->pos.x + (int)(std::cos(ang) * 32 * 12);
							stand_pos.y = target->pos.y + (int)(std::sin(ang) * 32 * 12);
							bool in_attack_range = false;
							bool is_revealed = true;
							for (unit*e : enemy_units) {
								if (e->gone) continue;
								test_in_range(c, e, stand_pos, in_attack_range, is_revealed);
								if (in_attack_range && is_revealed) break;
							}
							if (!in_attack_range) {
								c->subaction = combat_unit::subaction_move;
								c->target_pos = stand_pos;
							}
						}
					}
				}
			}
			if (has_spawn_broodling) {
				int n_enemy_tanks = 0;
				for (unit* u : enemy_units) {
					if (u->type == unit_types::siege_tank_tank_mode || u->type == unit_types::siege_tank_siege_mode) ++n_enemy_tanks;
				}
				for (auto*c : queens) {
					if (c->u->energy < 150) continue;
					//if (c->last_win_ratio >= 2.0 && c->u->hp >= c->u->stats->hp) continue;
					unit*target = get_best_score(enemy_units, [&](unit*u) {
						//if (!u->visible || !u->detected) return std::numeric_limits<double>::infinity();
						if (u->gone || !u->detected) return std::numeric_limits<double>::infinity();
						if (u->type->is_robotic) return std::numeric_limits<double>::infinity();
						if (u->stasis_timer) return std::numeric_limits<double>::infinity();
						if (u->building) return std::numeric_limits<double>::infinity();
						if (u->type->is_worker) return std::numeric_limits<double>::infinity();
						if (u->lockdown_timer) return std::numeric_limits<double>::infinity();
						if (u->maelstrom_timer) return std::numeric_limits<double>::infinity();
						if (u->is_flying) return std::numeric_limits<double>::infinity();

						double r = units_distance(u, c->u) - 32 * 9;
						if (u->type == unit_types::siege_tank_tank_mode || u->type == unit_types::siege_tank_siege_mode) r -= 32 * 3;
						if (r >= 32 * 30) return std::numeric_limits<double>::infinity();
						//if (c->u->hp < c->u->stats->hp && r > 0) return std::numeric_limits<double>::infinity();

						double value = u->minerals_value + u->gas_value;
						if (value < 150) return std::numeric_limits<double>::infinity();
						if (total_available_spawn_broodlings < n_enemy_tanks + 1 && total_available_spawn_broodlings < 6 && u->gas_value < 100) return std::numeric_limits<double>::infinity();
						if (u->type == unit_types::siege_tank_tank_mode || u->type == unit_types::siege_tank_siege_mode) value += 200;
						if (total_available_spawn_broodlings < n_enemy_tanks && total_available_spawn_broodlings < 6 && value < 300) return std::numeric_limits<double>::infinity();
						// 				if (value < 125 && total_available_spawn_broodlings < 8) return std::numeric_limits<double>::infinity();
						// 				if (r < 0) r = 0;
						// 				if (total_available_spawn_broodlings >= 4 && r > 0) return std::numeric_limits<double>::infinity();
						if (value < 125 && c->u->energy < c->u->stats->energy) {
							if (current_frame - u->last_attacked <= 15 * 5 && u->last_attack_target) {
								if (u->last_attack_target->owner == players::my_player) {
									weapon_stats*w = u->is_flying ? u->last_attack_target->stats->air_weapon : u->last_attack_target->stats->ground_weapon;
									if (w && u->last_attack_target->minerals_value + u->last_attack_target->gas_value < 200) return std::numeric_limits<double>::infinity();
								}
							}
						}
						if (r < 0) r = 0;

						auto it = disable_target_taken.find(u);
						if (it != disable_target_taken.end() && std::get<0>(it->second) != c) return std::numeric_limits<double>::infinity();

						return r / value / ((u->shields + u->hp) / (u->stats->shields + u->stats->hp));
					}, std::numeric_limits<double>::infinity());
					if (target) {
						double r = units_distance(c->u, target);
						if (r <= 32 * 9 && target->visible) {
							disable_target_taken[target] = std::make_tuple(c, current_frame + 60);
							c->subaction = combat_unit::subaction_idle;
							if (current_frame - c->last_used_special >= 45) {
								c->u->game_unit->useTech(upgrade_types::spawn_broodling->game_tech_type, target->game_unit);
								c->last_used_special = current_frame;
								c->u->controller->noorder_until = current_frame + 45;
							}
						} else {
							xy stand_pos;
							xy relpos = c->u->pos - target->pos;
							double ang = std::atan2(relpos.y, relpos.x);
							stand_pos.x = target->pos.x + (int)(std::cos(ang) * 32 * 9);
							stand_pos.y = target->pos.y + (int)(std::sin(ang) * 32 * 9);
							bool in_attack_range = false;
							bool is_revealed = true;
							for (unit*e : enemy_units) {
								if (e->gone) continue;
								test_in_range(c, e, stand_pos, in_attack_range, is_revealed);
								if (in_attack_range && is_revealed) break;
							}
							if (!in_attack_range) {
								c->subaction = combat_unit::subaction_move;
								c->target_pos = stand_pos;
							}
						}
					}
				}
			}
			if (has_ensnare) {
				for (auto*c : queens) {
					if (c->u->energy < 75) continue;
					auto i = use_ensnare_please.find(c);
					if (i == use_ensnare_please.end()) continue;
					xy target_pos = i->second;
					if (current_frame - c->last_used_special >= 45) {
						if ((c->u->pos - target_pos).length() <= 32 * 9) aoe_disable_target_taken[target_pos] = std::make_tuple(c, current_frame + 60);
						c->u->game_unit->useTech(upgrade_types::ensnare->game_tech_type, BWAPI::Position(target_pos.x, target_pos.y));
						c->last_used_special = current_frame;
						c->u->controller->noorder_until = current_frame + 45;
						c->subaction = combat_unit::subaction_idle;
					}
				}
			}
		}
	}

	if (!my_completed_units_of_type[unit_types::defiler].empty()) {
		a_vector<combat_unit*> defilers;
		for (auto*c : live_combat_units) {
			if (current_frame < c->strategy_busy_until) continue;
			if (c->reference_count) continue;
			if (c->u->type == unit_types::defiler) defilers.push_back(c);
		}
		if (players::my_player->has_upgrade(upgrade_types::dark_swarm)) {
			auto add_dark_swarm = [&](xy pos) {
				xy upper_left = pos - xy(3 * 16, 3 * 16);
				for (int y = 0; y < 32 * 6; y += 32) {
					for (int x = 0; x < 32 * 6; x += 32) {
						xy npos = upper_left + xy(x, y);
						if ((size_t)npos.x >= (size_t)grid::map_width || (size_t)npos.y >= (size_t)grid::map_height) continue;
						existing_dark_swarm.set(grid::build_square_index(npos));
					}
				}
			};
			for (auto*c : defilers) {
				//continue;
				if (c->u->energy < 100) continue;
				if (current_frame - c->u->last_incoming_bullet <= 15) {
					if (!existing_dark_swarm.test(grid::build_square_index(c->u->pos))) {
						if (current_frame - c->last_used_special >= latency_frames + 15) {
							add_dark_swarm(c->u->pos);
							c->u->game_unit->useTech(upgrade_types::dark_swarm->game_tech_type, BWAPI::Position(c->u->pos.x, c->u->pos.y));
							c->last_used_special = current_frame;
							c->u->controller->noorder_until = current_frame + 15;
							c->subaction = combat_unit::subaction_idle;
						}
					}
				}
				auto i = use_dark_swarm_please.find(c);
				if (i == use_dark_swarm_please.end()) continue;
				xy target_pos = i->second;
				//if (!existing_dark_swarm.test(grid::build_square_index(target_pos))) {
				if (true) {
					if (current_frame - c->last_used_special >= latency_frames + 15) {
						c->last_used_special = current_frame;
						if ((c->u->pos - target_pos).length() <= 32 * 10) {
							dark_swarm_target_taken[target_pos] = std::make_tuple(c, current_frame + 60);
							add_dark_swarm(target_pos);
							c->last_used_special = current_frame + 15;
							c->u->game_unit->useTech(upgrade_types::dark_swarm->game_tech_type, BWAPI::Position(target_pos.x, target_pos.y));
							c->u->controller->noorder_until = current_frame + 15;
							c->subaction = combat_unit::subaction_idle;
							c->u->controller->action = unit_controller::action_idle;
						} else {
							c->subaction = combat_unit::subaction_move;
							c->target_pos = target_pos;
						}
						
					}
				}
			}
		}
		if (players::my_player->has_upgrade(upgrade_types::plague)) {
			for (auto*c : defilers) {
				if (c->u->energy < 150) continue;
				auto i = use_plague_please.find(c);
				if (i == use_plague_please.end()) continue;
				xy target_pos = i->second;
				if (current_frame - c->last_used_special >= latency_frames + 15) {
					if (diag_distance(c->u->pos - target_pos) <= 32 * 9) plague_target_taken[target_pos] = std::make_tuple(c, current_frame + 60);
					c->u->game_unit->useTech(upgrade_types::plague->game_tech_type, BWAPI::Position(target_pos.x, target_pos.y));
					c->last_used_special = current_frame;
					c->u->controller->noorder_until = current_frame + 15;
					c->subaction = combat_unit::subaction_idle;
					
					c->u->controller->action = unit_controller::action_idle;
				}
			}
		}
		if (players::my_player->has_upgrade(upgrade_types::consume)) {
			for (auto*c : defilers) {
				if (c->u->energy >= c->u->stats->energy - 50) continue;
				//if (c->last_win_ratio >= 2.0) continue;
				bool need_energy_right_now = current_frame - c->last_fight <= 60 && c->last_win_ratio < 1.0;
				//if (current_frame - c->last_fight <= 60 && c->last_win_ratio >= 1.0) continue;
				combat_unit*target = get_best_score(live_combat_units, [&](combat_unit*a) {
					unit*u = a->u;
					if (u->stasis_timer) return std::numeric_limits<double>::infinity();
					if (u->building) return std::numeric_limits<double>::infinity();
					if (u == c->u) return std::numeric_limits<double>::infinity();

					if (u->type != unit_types::zergling) return std::numeric_limits<double>::infinity();

					double r = units_distance(u, c->u);
					if (r >= 32 * 12) return std::numeric_limits<double>::infinity();

					double value = u->minerals_value + u->gas_value;
					if (value > 50 && !need_energy_right_now) return std::numeric_limits<double>::infinity();
					if (value > 100) return std::numeric_limits<double>::infinity();

					auto it = disable_target_taken.find(u);
					//if (it != disable_target_taken.end() && std::get<0>(it->second) != c) return std::numeric_limits<double>::infinity();
					if (it != disable_target_taken.end()) return std::numeric_limits<double>::infinity();

					//if (existing_dark_swarm.test(grid::build_square_index(a->u->pos))) return std::numeric_limits<double>::infinity();

					return r * value * (u->shields + u->hp);
				}, std::numeric_limits<double>::infinity());
				if (target) {
					double r = units_distance(c->u, target->u);
					//if (r <= 32 * 4) {
					if (r <= 32) {
						if (current_frame - c->last_used_special >= latency_frames + 8) {
							disable_target_taken[target->u] = std::make_tuple(c, current_frame + 60);
							c->u->game_unit->useTech(upgrade_types::consume->game_tech_type, target->u->game_unit);
							c->last_used_special = current_frame;
							c->u->controller->noorder_until = current_frame + 8;
							c->subaction = combat_unit::subaction_idle;

							target->subaction = combat_unit::subaction_move;
							target->target_pos = c->u->pos;
						}
					} else {
						c->subaction = combat_unit::subaction_move;
						c->target_pos = target->u->pos;

						target->subaction = combat_unit::subaction_move;
						target->target_pos = c->u->pos;
					}
				}
			}
		}
	}

	if (!my_completed_units_of_type[unit_types::high_templar].empty()) {
		a_vector<combat_unit*> high_templars;
		for (auto*c : live_combat_units) {
			if (current_frame < c->strategy_busy_until) continue;
			if (c->reference_count) continue;
			if (c->u->type == unit_types::high_templar) high_templars.push_back(c);
		}
		if (players::my_player->has_upgrade(upgrade_types::psionic_storm)) {
			for (auto*c : high_templars) {
				if (c->u->energy < 75) continue;
				auto i = use_storm_please.find(c);
				if (i == use_storm_please.end()) continue;
				xy target_pos = i->second;
				if (current_frame - c->last_used_special >= 15) {
					storm_target_taken[target_pos] = std::make_tuple(c, current_frame + 15);
					c->u->game_unit->useTech(upgrade_types::psionic_storm->game_tech_type, BWAPI::Position(target_pos.x, target_pos.y));
					c->last_used_special = current_frame;
					//c->u->controller->noorder_until = current_frame + 8;
					c->u->controller->noorder_until = current_frame + 15;
					c->subaction = combat_unit::subaction_idle;
				}
			}
		}
	}

	if (!my_completed_units_of_type[unit_types::arbiter].empty()) {
		a_vector<combat_unit*> arbiters;
		for (auto*c : live_combat_units) {
			if (current_frame < c->strategy_busy_until) continue;
			if (c->reference_count) continue;
			if (c->u->type == unit_types::arbiter) arbiters.push_back(c);
		}
		if (players::my_player->has_upgrade(upgrade_types::stasis_field)) {
			for (auto*c : arbiters) {
				if (c->u->energy < 100) continue;
				auto i = use_stasis_please.find(c);
				if (i == use_stasis_please.end()) continue;
				xy target_pos = i->second;
				if (current_frame - c->last_used_special >= 15) {
					aoe_disable_target_taken[target_pos] = std::make_tuple(c, current_frame + 15);
					c->u->game_unit->useTech(upgrade_types::stasis_field->game_tech_type, BWAPI::Position(target_pos.x, target_pos.y));
					c->last_used_special = current_frame;
					//c->u->controller->noorder_until = current_frame + 8;
					c->u->controller->noorder_until = current_frame + 15;
					c->subaction = combat_unit::subaction_idle;
				}
			}
		}
	}

	if (!my_completed_units_of_type[unit_types::dark_archon].empty()) {
		a_vector<combat_unit*> dark_archons;
		for (auto*c : live_combat_units) {
			if (current_frame < c->strategy_busy_until) continue;
			if (c->reference_count) continue;
			if (c->u->type == unit_types::dark_archon) dark_archons.push_back(c);
		}

		if (players::my_player->has_upgrade(upgrade_types::feedback)) {

			for (auto*c : dark_archons) {
				if (c->u->energy < 50) continue;
				unit*target = get_best_score(enemy_units, [&](unit*e) {
					if (!e->visible || !e->detected) return std::numeric_limits<double>::infinity();
					if (e->building) return std::numeric_limits<double>::infinity();
					if (e->lockdown_timer) return std::numeric_limits<double>::infinity();
					if (e->maelstrom_timer) return std::numeric_limits<double>::infinity();
					if (e->invincible) return std::numeric_limits<double>::infinity();
// 					bool do_it = e->energy >= e->shields + e->hp;
// 					if (e->type == unit_types::high_templar && e->energy >= 75 - 5) do_it = true;
// 					if (e->type == unit_types::dark_archon && e->energy >= 50 - 5) do_it = true;
// 					if (e->type == unit_types::arbiter && e->energy >= 100 - 5) do_it = true;
// 					if (e->type == unit_types::science_vessel && e->energy >= 100 - 5) do_it = true;
// 					if (do_it && c->u->hp > c->u->stats->hp*0.25) return std::numeric_limits<double>::infinity();
					double d = diag_distance(e->pos - c->u->pos);
					if (d > 32 * 15) return std::numeric_limits<double>::infinity();
					if (d < 32 * 10) d = 32 * 10;

					auto it = disable_target_taken.find(e);
					if (it != disable_target_taken.end() && std::get<0>(it->second) != c) return std::numeric_limits<double>::infinity();

					auto it2 = feedback_target_taken.find(e);
					if (it2 != feedback_target_taken.end()) return std::numeric_limits<double>::infinity();

					return d / (double)e->energy;
				}, std::numeric_limits<double>::infinity());
				if (target) {
					c->subaction = combat_unit::subaction_idle;
					if (current_frame - c->last_used_special >= 8) {
						if (c->u->game_order != BWAPI::Orders::CastFeedback) {
							c->u->game_unit->useTech(upgrade_types::feedback->game_tech_type, target->game_unit);
						}
						c->last_used_special = current_frame;
						c->u->controller->noorder_until = current_frame + 8;

						disable_target_taken[target] = std::make_tuple(c, current_frame + 15);

						if (units_distance(c->u, target) <= 32 * 10) {
							disable_target_taken[target] = std::make_tuple(c, (int)(current_frame + (75 / (8.0 / 256))));
						}
					}
				}
			}
		}

		if (players::my_player->has_upgrade(upgrade_types::mind_control)) {

			for (auto*c : dark_archons) {
				if (c->u->energy < 150) continue;
				unit*target = get_best_score(enemy_units, [&](unit*e) {
					if (!e->visible || !e->detected) return std::numeric_limits<double>::infinity();
					if (e->building) return std::numeric_limits<double>::infinity();
					if (e->lockdown_timer) return std::numeric_limits<double>::infinity();
					if (e->maelstrom_timer) return std::numeric_limits<double>::infinity();
					if (e->invincible) return std::numeric_limits<double>::infinity();
					double value = e->minerals_value + e->gas_value;
					if (e->type->is_worker && my_units_of_type[e->type].empty()) value += 300.0;
					double min_value = 300;
					if (dark_archons.size() >= 6) min_value = 200;
					if (dark_archons.size() >= 10) min_value = 100;
					if (value < min_value && c->u->hp > c->u->stats->hp*0.25) return std::numeric_limits<double>::infinity();
					double d = diag_distance(e->pos - c->u->pos);
					if (d > 32 * 15) return std::numeric_limits<double>::infinity();
					if (d < 32 * 10) d = 32 * 10;

					double ehp = e->shields + e->hp;
					value *= ehp / (e->stats->shields + e->stats->hp);

					auto it = disable_target_taken.find(e);
					if (it != disable_target_taken.end() && std::get<0>(it->second) != c) return std::numeric_limits<double>::infinity();

					return d / value / (ehp / (e->stats->shields + e->stats->hp));
				}, std::numeric_limits<double>::infinity());
				if (target) {
					c->subaction = combat_unit::subaction_idle;
					if (current_frame - c->last_used_special >= 15) {
						c->u->game_unit->useTech(upgrade_types::mind_control->game_tech_type, target->game_unit);
						c->last_used_special = current_frame;
						c->u->controller->noorder_until = current_frame + 60;

						disable_target_taken[target] = std::make_tuple(c, current_frame + 60);
					}
				}
			}
		}

		if (players::my_player->has_upgrade(upgrade_types::maelstrom)) {
			for (auto*c : dark_archons) {
				if (c->u->energy < 100) continue;
				auto i = use_maelstrom_please.find(c);
				if (i == use_maelstrom_please.end()) continue;
				xy target_pos = i->second;
				if (current_frame - c->last_used_special >= 15) {
					aoe_disable_target_taken[target_pos] = std::make_tuple(c, current_frame + 60);
					c->u->game_unit->useTech(upgrade_types::maelstrom->game_tech_type, BWAPI::Position(target_pos.x, target_pos.y));
					c->last_used_special = current_frame;
					c->u->controller->noorder_until = current_frame + 8;
					c->subaction = combat_unit::subaction_idle;
				}
			}
		}

	}

	for (auto* a : live_combat_units) {
		if (a->assigned_control_spot && a->subaction == combat_unit::subaction_move && a->target_pos == a->assigned_control_spot->pos) do_control_spot_positioning(a, a->assigned_control_spot);
		if (a->assigned_defence_control_spot && a->subaction == combat_unit::subaction_move && a->target_pos == a->assigned_defence_control_spot->pos) do_control_spot_positioning(a, a->assigned_defence_control_spot);
	}

	if (!my_completed_units_of_type[unit_types::shield_battery].empty()) {

		for (auto*bat : my_completed_units_of_type[unit_types::shield_battery]) {

			if (bat->energy < 10.0) continue;

			for (auto*c : live_combat_units) {
				if (c->u->stats->shields == 0.0) continue;
				double d = diag_distance(c->u->pos - bat->pos);
				if (d > 32 * 14) continue;
				bool recharge = false;
				if (c->u->shields <= c->u->stats->shields * 0.125) recharge = true;
				//if (d <= 32 * 4 && c->u->weapon_cooldown >= latency_frames && c->u->shields <= c->u->stats->shields * 0.5) recharge = true;
				if (c->is_recharging && c->u->shields >= c->u->stats->shields * 0.875) c->is_recharging = false;
				if (recharge || c->is_recharging) {
					c->is_recharging = true;
					if (units_distance(bat, c->u) <= 32 * 4) {
						c->subaction = combat_unit::subaction_recharge;
						//c->u->game_unit->rightClick(bat->game_unit);
						//c->u->controller->noorder_until = current_frame + 20;
					} else {

						a_vector<unit*> nearby_enemies;
						for (unit*e : enemy_units) {
							if (diag_distance(e->pos - c->u->pos) <= 32 * 15) nearby_enemies.push_back(e);
						}

						a_deque<xy> path = find_path(c->u->type, c->u->pos, [&](xy pos, xy npos) {
							return diag_distance(npos - c->u->pos) <= 32 * 20;
						}, [&](xy pos, xy npos) {
							double cost = 0.0;
							for (unit*e : nearby_enemies) {
								weapon_stats*w = c->u->is_flying ? e->stats->air_weapon : e->stats->ground_weapon;
								if (e->type == unit_types::bunker) w = units::get_unit_stats(unit_types::marine, e->owner)->ground_weapon;
								if (!w) continue;
								double d = diag_distance(pos - e->pos);
								if (d <= w->max_range*1.5) {
									cost += w->max_range*1.5 - d;
								}
							}
							return cost;
						}, [&](xy pos) {
							size_t index = grid::build_square_index(pos);
							return units_distance(bat->pos, bat->type, pos, c->u->type) <= 32 * 3 && (c->u->is_flying || !run_spot_taken.test(index));
						});

						if (!path.empty()) {

							xy goal_pos = path.back();

							run_spot_taken.set(grid::build_square_index(goal_pos));

							c->subaction = combat_unit::subaction_move;
							c->target_pos = goal_pos;
// 							c->u->game_unit->move(BWAPI::Position(goal_pos.x, goal_pos.y));
// 							c->u->controller->noorder_until = current_frame + 20;
						} else c->subaction = combat_unit::subaction_recharge;
					}
				}
			}

		}

	}

	for (auto&v : pickup_list) {
		combat_unit*dropship = v.first;
		combat_unit*pickup = v.second;

		pickup->last_given_a_lift = current_frame;
		pickup->u->controller->no_unload_until = current_frame + 15 * 5;
		dropship->subaction = combat_unit::subaction_move;
		dropship->target_pos = pickup->u->pos;
		if (diag_distance(pickup->u->pos - dropship->u->pos) <= 32 * 4) {
			dropship->subaction = combat_unit::subaction_idle;
			dropship->u->controller->action = unit_controller::action_idle;
			if (current_frame >= dropship->u->controller->noorder_until) {
				pickup->u->game_unit->rightClick(dropship->u->game_unit);
				pickup->u->controller->noorder_until = current_frame + 15;
				dropship->u->controller->noorder_until = current_frame + 4;
			}
		}
	}

	for (auto*c : live_combat_units) {
		if (current_frame < c->strategy_busy_until) continue;
		if (c->reference_count) continue;
		if (c->drop_target == xy()) continue;

		bool drop_now = false;

		double distance_left = unit_pathing_distance(unit_types::scv, c->u->pos, c->drop_target);

		size_t index = grid::build_square_index(c->u->pos);
		if (entire_threat_area.test(index)) {
			for (auto&g : groups) {
				if (g.air_dpf < 1.0 && g.ground_dpf < 1.0) continue;
				if (g.threat_area.test(index)) {
					if (g.air_dpf >= 5.0 || g.ground_dpf >= 5.0) drop_now = true;
					else if (distance_left <= 32 * 12) drop_now = true;
				}
			}
		}

		if (distance_left <= 32 * 6) {
			drop_now = true;
		}

		if (drop_now) {
			//log(log_level_info, "drop now!\n");
			if (current_frame - c->u->controller->last_unload >= 8) {
				c->u->controller->last_unload = current_frame;
				c->u->controller->noorder_until = current_frame + 8;
				//c->u->game_unit->unloadAll();
				//c->u->game_unit->unload(c->u->game_unit);
				if (!c->u->loaded_units.empty()) c->u->game_unit->unload(c->u->loaded_units.front()->game_unit);
			}
		} else {
			c->subaction = combat_unit::subaction_move;
			c->target_pos = c->drop_move_target;
		}

		if (c->u->loaded_units.empty()) {
			c->drop_target = xy();
			c->drop_move_target = xy();
		}

	}

	a_vector<xy> all_landing_nukes;
	for (auto i = active_nukes.begin(); i != active_nukes.end();) {
		if (current_frame - i->second.second >= 15 * 8) i = active_nukes.erase(i);
		else ++i;
	}
	for (auto&v : game->getNukeDots()) {
		xy pos = bwapi_pos(v);
		auto&active_nuke = active_nukes[pos];
		if (active_nuke.first == 0) active_nuke.first = current_frame;
		active_nuke.second = current_frame;
	}
	for (auto&v : active_nukes) {
		if (current_frame - v.second.first >= 15 * 18) all_landing_nukes.push_back(v.first);
	}

	if (!all_landing_nukes.empty()) {
		int iterations = 0;
		for (auto*a : live_combat_units) {
			xy nearest_nuke = get_best_score(all_landing_nukes, [&](xy pos) {
				double d = diag_distance(pos - a->u->pos);
				if (d > 32 * 10) return std::numeric_limits<double>::infinity();
				return d;
			}, std::numeric_limits<double>::infinity());
			if (nearest_nuke == xy()) continue;

			//log(log_level_info, " waa dodge nuke!\n");

			a_deque<xy> path = find_path(a->u->type, a->u->pos, [&](xy pos, xy npos) {
				if (++iterations % 1024 == 0) multitasking::yield_point();
				return diag_distance(npos - a->u->pos) <= 32 * 20;
			}, [&](xy pos, xy npos) {
				return -diag_distance(nearest_nuke - npos);
			}, [&](xy pos) {
				return diag_distance(nearest_nuke - pos) >= 32 * 12;
			});

			a->subaction = combat_unit::subaction_move;
			a->target_pos = path.empty() ? a->goal_pos : path.back();

		}
	}

	a_vector<combat_unit*> mutalisks;
	a_vector<combat_unit*> zerglings;
	a_vector<combat_unit*> ultras;
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
	for (auto& g : groups) {

		auto snipe = [&](auto& my_units) {

			unit* front_unit = g.enemies.front();
			unit* target = front_unit;
			enemies.clear();
			double highest_cost = 0.0;
			unit* any_my_unit = my_units.front()->u;
			for (unit* u : g.enemies) {
				weapon_stats* mw = u->is_flying ? any_my_unit->stats->air_weapon : any_my_unit->stats->ground_weapon;
				if (mw && diag_distance(u->pos - front_unit->pos) <= 32 * 3) {
					double cost = u->type->total_minerals_cost + u->type->total_gas_cost;
					if (u->type->is_worker) cost += 100.0;
					else if (!u->stats->air_weapon) cost -= 50.0;
					else if (u->building) cost -= 100.0;
					if (cost > highest_cost) {
						highest_cost = cost;
						target = u;
					}
				}
				weapon_stats* w = any_my_unit->is_flying ? u->stats->air_weapon : u->stats->ground_weapon;
				if (!w) continue;
				double damage = get_weapon_damage(u, w, any_my_unit);
				enemies.push_back({ u, u->pos, w->max_range + 64.0, damage, w->cooldown, damage / w->cooldown });
			}
			weapon_stats* w = target->is_flying ? any_my_unit->stats->air_weapon : any_my_unit->stats->ground_weapon;
			if (!w) return;
			if (!any_my_unit->is_flying && my_units.size() > 4) {
				std::sort(my_units.begin(), my_units.end(), [&](auto* a, auto* b) {
					double ad = units_distance(a->u, target);
					double bd = units_distance(b->u, target);
					return ad < bd;
				});
				my_units.resize(4);
			}
			double damage = get_weapon_damage(any_my_unit, w, target);
			double dpf = damage / w->cooldown;
			double target_hp = target->shields + target->hp;
			for (auto*& a : my_units) {
				double hp = a->u->shields + a->u->hp;
				double d = units_distance(a->u, target) - w->max_range;
				if (d < 0.0) d = 0.0;
				int move_frames = frames_to_reach(a->u, d);
				xy pos = a->u->pos;
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
					double e_d = units_distance(pos, a->u->type, nearest_point, e.u->type);
					if (e_d <= e.range) e.tmp_in_range_after_frames = (int)(move_frames * t);
					else e.tmp_in_range_after_frames = 15 * 10;

					if (e.tmp_in_range_after_frames <= move_frames) {
						if (e_d < e.range) {
							double escape_d = e.range - e_d;
							int escape_f = frames_to_reach(a->u->stats, 0.0, escape_d);
							hp -= e.dpf * escape_f;
						}
					}
				}
				for (int f = 0; f < 15 * 20; f += 8) {
					double max_incoming_damage = 0.0;
					for (auto& e : enemies) {
						if (f >= e.tmp_in_range_after_frames) {
							hp -= e.dpf * 8;
							if (e.damage_per_hit >= max_incoming_damage) max_incoming_damage = e.damage_per_hit;
						}
					}
					if (hp <= a->u->stats->hp / 4 - max_incoming_damage) {
						if (f <= move_frames + 8) a = nullptr;
						break;
					}
					if (f >= move_frames) {
						target_hp -= dpf * 8;
					}
				}
			}
			if (target_hp <= 0.0) {

				for (auto* a : my_units) {
					if (!a) continue;
					a->subaction = combat_unit::subaction_fight;
					a->target = target;
				}

				int pull_back_n = 0;
				for (auto* a : my_units) {
					if (!a) ++pull_back_n;
				}

				log(log_level_test, " sniping target %s with %d %s (pull back with %d)\n", target->type->name, my_units.size(), any_my_unit->type->name, pull_back_n);

				for (auto* a : my_units) {
					if (!a) continue;
					game->drawLineMap(a->u->pos.x, a->u->pos.y, target->pos.x, target->pos.y, BWAPI::Colors::Green);
				}

			}
		};

		zerglings.clear();
		ultras.clear();
		mutalisks.clear();
		for (auto* a : g.allies) {
			if (a->u->irradiate_timer) continue;
			if (a->u->stasis_timer) continue;
			if (a->u->lockdown_timer) continue;
			if (a->u->maelstrom_timer) continue;
			if (a->u->type == unit_types::mutalisk) mutalisks.push_back(a);
			if (a->u->type == unit_types::zergling) zerglings.push_back(a);
			if (a->u->type == unit_types::ultralisk) ultras.push_back(a);
		}
//		if (!mutalisks.empty()) snipe(mutalisks);
//		//if (!zerglings.empty()) snipe(zerglings);
//		if (!ultras.empty()) snipe(ultras);

	}

// 	for (auto* a : live_combat_units) {
// 		if (!a->u->is_flying) continue;
// 		if (a->subaction != combat_unit::subaction_move) continue;
// 		if (entire_threat_area.test(grid::build_square_index(a->u->pos))) continue;
// 		auto* src = find_nearest_air_node(a->u->pos);
// 		auto* dst = find_nearest_air_node(a->target_pos);
// 		if (!src || !dst) continue;
// 		if (src == dst) continue;
// 		auto path = find_air_node_path(src, [&](air_node* n) {
// 			return true;
// 		}, [&](air_node* n) {
// 			return n->cost + diag_distance(n->pos - dst->pos);
// 		}, [&](air_node* n) {
// 			return n == dst;
// 		});
// 		xy lpos = a->u->pos;
// 		for (xy pos : path) {
// 			if (lpos != xy()) game->drawLineMap(lpos.x, lpos.y, pos.x, pos.y, BWAPI::Colors::Grey);
// 			lpos = pos;
// 		}
// 		if (path.size() > 1) {
// 			a->target_pos = path[1];
// 		}
// 	}

}

void do_run(combat_unit*a, const a_vector<unit*>&enemies);

auto move_close_if_unreachable = [&](combat_unit*a,unit*target) {
	if (square_pathing::unit_can_reach(a->u, a->u->pos, target->pos, square_pathing::pathing_map_index::include_liftable_wall)) return false;
	weapon_stats*e_weapon = a->u->is_flying ? target->stats->air_weapon : target->stats->ground_weapon;
	if (e_weapon && units_distance(a->u, target) <= e_weapon->max_range) return false;
	weapon_stats*my_weapon = target->is_flying ? a->u->stats->air_weapon : a->u->stats->ground_weapon;
	double wr = my_weapon ? my_weapon->max_range : 64;
	xy closest;
	double closest_dist;
	a_deque<xy> path = find_path(a->u->type, a->u->pos, [&](xy pos, xy npos) {
		return diag_distance(npos - a->u->pos) <= 32 * 5;
	}, [&](xy pos, xy npos) {
		double d = diag_distance(npos - target->pos);
		if (closest == xy() || d < closest_dist) {
			closest = npos;
			closest_dist = d;
		}
		return d;
	}, [&](xy pos) {
		return units_distance(pos, a->u->type, target->pos, target->type) <= wr;
	});
	if (!path.empty()) closest = path.back();
	log("move_close_if_unreachable: closest is %d %d\n", closest.x, closest.y);
	if (closest != xy()) {
		a->subaction = combat_unit::subaction_move;
		a->target_pos = closest;
	}
	return true;
};

int last_attack_scan = 0;

void do_attack(combat_unit* a, const a_vector<unit*>& allies, const a_vector<unit*>& enemies) {
	a->defend_spot = xy();
	a->subaction = combat_unit::subaction_fight;
	if (active_spider_mine_targets.count(a->u)) {
		unit*ne = get_best_score(enemies, [&](unit*e) {
			if (e->is_flying) return std::numeric_limits<double>::infinity();
			return diag_distance(e->pos - a->u->pos);
		});
		a->subaction = combat_unit::subaction_move_directly;
		a->target_pos = ne->pos;
		return;
	}
	if (!a->u->is_flying && a->u->controller->can_move) {
		for (unit*t : active_spider_mine_targets) {
			if (t != a->u && units_distance(t, a->u) <= 32 * 3) {
				do_run(a, enemies);
				return;
			}
		}
		for (unit*t : active_spider_mines) {
			if (units_distance(t, a->u) <= 32 * 3) {
				if (t->order_target) {
					int my_inrange = 0;
					for (unit*u : allies) {
						if (u->type->is_building) continue;
						if (u->is_flying) continue;
						if (u->type->is_hovering) continue;
						if (u->type->is_non_usable) continue;
						if (units_distance(t, u) <= 32 * 3) ++my_inrange;
					}
					int op_inrange = 0;
					for (unit*u : enemies) {
						if (u->type->is_building) continue;
						if (u->is_flying) continue;
						if (u->type->is_hovering) continue;
						if (u->type->is_non_usable) continue;
						if (units_distance(t, u) <= 32 * 3) ++op_inrange;
					}
					if (my_inrange > op_inrange + 2 && my_inrange >= 3) {
						if (a->u->weapon_cooldown <= latency_frames) {
							a->target = t;
							log("attack spider mine waa (%d vs %d)!\n", my_inrange, op_inrange);
							return;
						}
					}
				}
				if (a->u != t->order_target) {
					do_run(a, enemies);
					return;
				}
			}
// 			if (t->owner->is_enemy) {
// 				weapon_stats*w = t->is_flying ? a->u->stats->air_weapon : a->u->stats->ground_weapon;
// 				if (w && units_distance(t, a->u) <= w->max_range) {
// 					if (t->shields + t->hp - focus_fire[t] > 0.0) {
// 						if (a->u->weapon_cooldown <= latency_frames && current_frame >= a->registered_focus_fire_until) {
// 							double damage = w->damage;
// 							if (t->shields <= 0) damage *= combat_eval::get_damage_type_modifier(w->damage_type, t->stats->type->size);
// 							damage -= t->stats->armor;
// 							if (damage <= 0) damage = 1.0;
// 							damage *= w == a->u->stats->ground_weapon ? a->u->stats->ground_weapon_hits : a->u->stats->air_weapon_hits;
// 							focus_fire[t] += damage;
// 							auto i = registered_focus_fire.find(a);
// 							if (i != registered_focus_fire.end()) {
// 								if (std::abs(focus_fire[std::get<0>(i->second)] -= std::get<1>(i->second)) < 1) {
// 									focus_fire.erase(std::get<0>(i->second));
// 								}
// 							}
// 							registered_focus_fire[a] = std::make_tuple(t, damage, current_frame + 8);
// 						}
// 						a->target = t;
// 						return;
// 					}
// 				}
// 			}
		}
	}

	bool has_sieged_tanks = false;
	for (unit*u : allies) {
		if (u->type == unit_types::siege_tank_siege_mode) {
			has_sieged_tanks = true;
			break;
		}
	}

	if (a->u->weapon_cooldown <= latency_frames && current_frame >= a->registered_focus_fire_until) {
		auto i = registered_focus_fire.find(a);
		if (i != registered_focus_fire.end()) {
			if (std::abs(focus_fire[std::get<0>(i->second)] -= std::get<1>(i->second)) < 1) {
				focus_fire.erase(std::get<0>(i->second));
			}
			registered_focus_fire.erase(i);
		}
	}

	int my_goliaths = 0;
	int my_tanks = 0;
	for (auto*a : allies) {
		if (a->type == unit_types::goliath) ++my_goliaths;
		if (a->type == unit_types::siege_tank_tank_mode || a->type == unit_types::siege_tank_siege_mode) {
			++my_tanks;
		}
	}
	int enemy_interceptors = 0;
	int enemy_units_that_hit_up = 0;
	int enemy_tanks = 0;
	for (auto* e : enemies) {
		if (e->type == unit_types::interceptor) ++enemy_interceptors;
		if (e->stats->air_weapon) ++enemy_units_that_hit_up;
		if (e->type == unit_types::siege_tank_tank_mode || e->type == unit_types::siege_tank_siege_mode) ++enemy_tanks;
	}

	bool wants_to_lay_spider_mines = a->u->spider_mine_count && players::my_player->has_upgrade(upgrade_types::spider_mines);
	weapon_stats*spider_mine_weapon = nullptr;
	if (wants_to_lay_spider_mines) {
		spider_mine_weapon = units::get_weapon_stats(BWAPI::WeaponTypes::Spider_Mines, players::my_player);
	}

	bool has_siege_mode = players::my_player->has_upgrade(upgrade_types::siege_mode);

	unit*u = a->u;
	std::function<std::tuple<double, double, double>(unit*)> score;

	bool has_slow_projectile = false;
	//if (u->type == unit_types::carrier) has_slow_projectile = true;
	if (u->type == unit_types::dragoon) has_slow_projectile = true;
	if (u->type == unit_types::reaver) has_slow_projectile = true;
	if (u->type == unit_types::goliath) has_slow_projectile = true;
	if (u->type == unit_types::vulture) has_slow_projectile = true;
	if (u->type == unit_types::wraith) has_slow_projectile = true;
	if (u->type == unit_types::siege_tank_siege_mode) has_slow_projectile = true;
	
	int interceptor_count = u->type == unit_types::carrier ? u->game_unit->getInterceptorCount() : 0;

	//if (players::my_player->race != race_protoss) {
	if (a->u->type == unit_types::carrier) {
		score = [&](unit* e) {
			if (!e->visible) return std::make_tuple(std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity());
			if (e->invincible || e->stasis_timer) return std::make_tuple(std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity());
			if (e->type == unit_types::egg || e->type == unit_types::lurker_egg) return std::make_tuple(std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity());
			if (e->dead) return std::make_tuple(std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity());
			
			double d = units_distance(a->u, e);
			if (u->target == e) d -= 64;
			if ((e->stats->air_weapon || e->type == unit_types::bunker || e->type->is_worker) && (a->last_win_ratio < 128.0 || d <= 32 * 9)) {
				d -= 32 * 8;
				if (d < 0) d = 0;
				if (u->target == e && d == 0) d = -1.0;
				return std::make_tuple(d - 32 * 8, d, e->energy + e->hp);
			} else {
				d -= 32 * 8;
				if (d < 0) d = 0;
				if (u->target == e && d == 0) d = -1.0;
				return std::make_tuple(d, e->energy + e->hp, std::numeric_limits<double>::infinity());
			}
		};
	} else if (a->u->is_flying) {
		score = [&](unit*e) {
			//double ehp = e->shields + e->hp - focus_fire[e];
			//if (ehp <= 0) return std::make_tuple(std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity());
			//if (!e->visible || !e->detected) return std::make_tuple(std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity());
			//if (!e->visible || (!e->detected && scans_available == 0)) return std::make_tuple(std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity());
			if (e->shields + e->hp - prepared_damage[e] <= 0.0) return std::make_tuple(std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity());
			if (!e->visible) return std::make_tuple(std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity());
			if (e->invincible || e->stasis_timer) return std::make_tuple(std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity());
			if (e->type == unit_types::egg || e->type == unit_types::lurker_egg) return std::make_tuple(std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity());
			if (e->dead) return std::make_tuple(std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity());
			if (e->type == unit_types::spider_mine && !e->visible) return std::make_tuple(std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity());
			//if (u->is_flying && e->type == unit_types::interceptor) w = nullptr;
			
			double d = units_distance(u, e);
			bool is_weaponless_target = false;
			is_weaponless_target |= e->type == unit_types::bunker;
			is_weaponless_target |= e->type == unit_types::carrier;
			is_weaponless_target |= e->type == unit_types::overlord;
			//is_weaponless_target |= e->type == unit_types::pylon;
			is_weaponless_target |= e->type == unit_types::reaver;
			is_weaponless_target |= e->type == unit_types::high_templar;
			is_weaponless_target |= e->type == unit_types::defiler;
			is_weaponless_target |= e->type == unit_types::observer;
			is_weaponless_target |= e->type == unit_types::queen;
			is_weaponless_target |= e->type == unit_types::dark_archon;
			is_weaponless_target |= e->type == unit_types::medic;
			is_weaponless_target |= e->type == unit_types::carrier;
			bool is_attack_pylon = false;
			if (e->type == unit_types::pylon && attack_pylons.count(e)) {
				is_attack_pylon = true;
				is_weaponless_target = true;
			}
			if (!e->stats->ground_weapon && !e->stats->air_weapon && !is_weaponless_target) return std::make_tuple(std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity(), d);
			weapon_stats* ew = a->u->is_flying ? e->stats->air_weapon : e->stats->ground_weapon;
			weapon_stats* w = e->is_flying ? u->stats->air_weapon : u->stats->ground_weapon;
			if (u->type == unit_types::scourge && (e->type == unit_types::overlord || e->type == unit_types::interceptor || e->building)) w = nullptr;
			double w_min_range = std::numeric_limits<double>::infinity();
			double w_max_range = 0.0;
			double damage = 0.0;
			if (w) {
				w_min_range = w->min_range;
				w_max_range = w->max_range;
				damage = (w->damage*combat_eval::get_damage_type_modifier(w->damage_type, e->type->size));
			} else if (u->type == unit_types::carrier) {
				w_min_range = 0;
				w_max_range = 32 * 10;
				damage = 6 * interceptor_count;
			}
			if (d < w_min_range) return std::make_tuple(std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity());
			double ehp = e->shields + e->hp;
			double hits = ehp / damage;
			if (!ew && !e->building) hits += 10;
			if (e->lockdown_timer) hits += 10;
			if (e->maelstrom_timer) hits += 10;
			if (e->type->requires_pylon && !e->is_powered) hits += 10;
			if (d > w_max_range + 32) hits += (d - w_max_range) / a->u->stats->max_speed;
			if (d > w_max_range) hits += 100;
			if (d <= w_max_range) {
				if (e->type == unit_types::spider_mine) hits /= 1000;
				if (e->type == unit_types::observer) hits /= 2;
				if (e->type == unit_types::siege_tank_siege_mode) hits /= 2;
				if (e->type == unit_types::carrier) hits /= 10;
				if (e->is_flying && e->type != unit_types::overlord && e->type != unit_types::corsair) hits /= 10;
				if (e->build_unit && e->build_unit->type->builder_type == e->type) hits /= 20;
				if (e->cloaked && scans_available) hits /= 4;
				if (current_frame - e->last_seen >= 15 * 30) hits += 20;
				if (e->type == unit_types::reaver) hits /= 10;
				if (e->type == unit_types::high_templar) hits /= 10;
				if (e->type == unit_types::arbiter) hits /= 20;
				if (is_attack_pylon) hits /= 20;
				if (w_max_range <= 32 * 2) hits /= 10;
				if (has_sieged_tanks && a->u->type != unit_types::siege_tank_siege_mode && e->type == unit_types::zealot) hits /= 2;
				//if (a->u->type == unit_types::wraith && e->type == unit_types::overlord) hits /= 20;
				if (a->u->type == unit_types::siege_tank_siege_mode && e->type == unit_types::lurker) hits /= 5;
				if (a->u->type == unit_types::siege_tank_siege_mode && e->type == unit_types::siege_tank_siege_mode) hits /= 5;
				if (a->u->type == unit_types::siege_tank_siege_mode && ehp < damage) hits += 5;

				if (u->type->race == race_zerg && e->type == unit_types::siege_tank_siege_mode) hits /= 12;

				if (no_splash_target.count(e)) {
					if (a->u->type == unit_types::siege_tank_siege_mode || a->u->type == unit_types::siege_tank_tank_mode) hits += 10;
					else --hits;
				}
			} else if (!u->is_flying && d >= w_max_range + 64) hits += 20;
			if (u->type == unit_types::wraith || u->type == unit_types::valkyrie) {
				if (d <= w_max_range + 64) {
					if (e->type == unit_types::overlord) hits /= 4;
					if (e->type == unit_types::scourge) hits /= 100;
				}
			}
			if (e->type == unit_types::interceptor) hits += 10;
			if (e->type == unit_types::dark_templar) hits /= 4;
			if (e->irradiate_timer) hits += 2;
			
			if (a->u->is_flying && enemy_units_that_hit_up == 0 && e->type->is_worker) hits /= 50;
			if (u->is_flying && u->cloaked && e->type->is_worker) hits /= 20;
			if (!e->cloaked && !e->detected && scans_available == 0) hits += 20;
			return std::make_tuple(hits, 0.0, 0.0);
		};
	} else if (true) {
		score = [&](unit*e) {
			//double ehp = e->shields + e->hp - focus_fire[e];
			//if (ehp <= 0) return std::make_tuple(std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity());
			//if (!e->visible || !e->detected) return std::make_tuple(std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity());
			//if (!e->visible || (!e->detected && scans_available == 0)) return std::make_tuple(std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity());
			if (e->shields + e->hp - prepared_damage[e] <= 0.0) return std::make_tuple(std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity());
			if (!e->visible) return std::make_tuple(std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity());
			if (e->invincible || e->stasis_timer) return std::make_tuple(std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity());
			if (e->type == unit_types::egg || e->type == unit_types::lurker_egg) return std::make_tuple(std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity());
			if (e->dead) return std::make_tuple(std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity());
			if (e->type == unit_types::spider_mine && !e->visible) return std::make_tuple(std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity());
			weapon_stats*w = e->is_flying ? u->stats->air_weapon : u->stats->ground_weapon;
			if (u->type == unit_types::scourge && (e->type == unit_types::overlord || e->type == unit_types::interceptor || e->building)) w = nullptr;
			//if (u->is_flying && e->type == unit_types::interceptor) w = nullptr;
			if (!w) return std::make_tuple(std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity());
			double d = units_distance(u, e);
			bool is_weaponless_target = false;
			is_weaponless_target |= e->type == unit_types::bunker;
			is_weaponless_target |= e->type == unit_types::carrier;
			is_weaponless_target |= e->type == unit_types::overlord;
			//is_weaponless_target |= e->type == unit_types::pylon;
			is_weaponless_target |= e->type == unit_types::reaver;
			is_weaponless_target |= e->type == unit_types::high_templar;
			is_weaponless_target |= e->type == unit_types::defiler;
			is_weaponless_target |= e->type == unit_types::observer;
			is_weaponless_target |= e->type == unit_types::queen;
			is_weaponless_target |= e->type == unit_types::dark_archon;
			is_weaponless_target |= e->type == unit_types::medic;
			is_weaponless_target |= e->type == unit_types::carrier;
			bool is_attack_pylon = false;
			if (e->type == unit_types::pylon && attack_pylons.count(e)) {
				is_attack_pylon = true;
				is_weaponless_target = true;
			}
			if (!e->stats->ground_weapon && !e->stats->air_weapon && !is_weaponless_target) return std::make_tuple(std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity(), d);
			//if (!e->stats->ground_weapon && !e->stats->air_weapon && e->type != unit_types::bunker) return std::make_tuple(std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity(), d);
			//if (!e->stats->ground_weapon && !e->stats->air_weapon) return std::make_tuple(std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity(), d);
			weapon_stats*ew = a->u->is_flying ? e->stats->air_weapon : e->stats->ground_weapon;
			double w_min_range = w->min_range;
			double w_max_range = w->max_range;
			if (u->type == unit_types::siege_tank_tank_mode && has_siege_mode) {
				w_min_range = 32 * 2;
				w_max_range = 32 * 12;
			}
			if (u->is_flying && u->cloaked) {
				w_max_range = 32 * 15;
			}
			if (d < w_min_range) return std::make_tuple(std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity());
			double ehp = e->shields + e->hp;
			//if (e != a->u->order_target) ehp -= focus_fire[e];
			double damage = (w->damage*combat_eval::get_damage_type_modifier(w->damage_type, e->type->size));
			//if (ehp <= 0) ehp = e->stats->shields + e->stats->hp - ehp;
//			if (wants_to_lay_spider_mines && !e->type->is_flyer && !e->type->is_hovering && !e->type->is_building) {
//				//if (e->type->is_hovering) return std::make_tuple(1000 + d / a->u->stats->max_speed, 0.0, 0.0);
//				//return std::make_tuple(prepared_damage[e] + d / a->u->stats->max_speed, 0.0, 0.0);
//				ehp += prepared_damage[e] * 4;
//				//if (e->type->size == unit_type::size_large) {
//				if (e->type == unit_types::siege_tank_siege_mode) {
//					w = spider_mine_weapon;
//					damage = (w->damage*combat_eval::get_damage_type_modifier(w->damage_type, e->type->size));
//				}
//			}
			//if (d > w->max_range) return std::make_tuple(std::numeric_limits<double>::infinity(), d, 0.0);
			double hits = ehp / damage;
			//if (d > w->max_range) return std::make_tuple(std::numeric_limits<double>::infinity(), std::ceil(hits), d);
			if (!ew && !e->building) hits += 10;
			if (e->lockdown_timer) hits += 10;
			if (e->maelstrom_timer) hits += 10;
			if (e->type->requires_pylon && !e->is_powered) hits += 10;
			//if (d > w->max_range) return std::make_tuple(std::numeric_limits<double>::infinity(), hits + (d - w->max_range) / a->u->stats->max_speed / 90, 0.0);
			//if (d > w_max_range) hits += (d - w_max_range) / a->u->stats->max_speed / 4;
			//if (d > w_max_range) hits += (d - w_max_range);
			//if (d > w_max_range + 32 && a->u->type != unit_types::mutalisk) hits += (d - w_max_range) / a->u->stats->max_speed;
			//if (d > w_max_range + 32) hits += (d - w_max_range) / (a->u->stats->max_speed * 8);
			if (d > w_max_range + 32) hits += (d - w_max_range) / a->u->stats->max_speed;
			if (d > w_max_range && a->u->type == unit_types::lurker && a->u->burrowed) hits += 100;
			//if (d > w_max_range && allies.size() >= 10) hits += 100;
			if (d > w_max_range) hits += 100;
			//if (d < w_max_range || a->u->is_flying) {
			if (d <= w_max_range) {
				if (e->type == unit_types::spider_mine) hits /= 1000;
				if (e->type == unit_types::observer) hits /= 2;
				if (e->type == unit_types::siege_tank_siege_mode) hits /= 2;
				if (e->type == unit_types::carrier) hits /= 10;
				if (e->is_flying && e->type != unit_types::overlord && e->type != unit_types::corsair) hits /= 10;
				if (e->build_unit && e->build_unit->type->builder_type == e->type) hits /= 20;
				if (e->cloaked && scans_available) hits /= 4;
				if (current_frame - e->last_seen >= 15 * 30) hits += 20;
				if (e->type == unit_types::reaver) hits /= 10;
				if (e->type == unit_types::high_templar) hits /= 10;
				if (e->type == unit_types::arbiter) hits /= 20;
				if (is_attack_pylon) hits /= 20;
				if (w->max_range <= 32 * 2) hits /= 10;
				if (has_sieged_tanks && a->u->type != unit_types::siege_tank_siege_mode && e->type == unit_types::zealot) hits /= 2;
				//if (a->u->type == unit_types::wraith && e->type == unit_types::overlord) hits /= 20;
				if (a->u->type == unit_types::siege_tank_siege_mode && e->type == unit_types::lurker) hits /= 5;
				if (a->u->type == unit_types::siege_tank_siege_mode && e->type == unit_types::siege_tank_siege_mode) hits /= 5;
				if (a->u->type == unit_types::siege_tank_siege_mode && ehp < damage) hits += 5;

				if (u->type->race == race_zerg && e->type == unit_types::siege_tank_siege_mode) hits /= 12;

				if (no_splash_target.count(e)) {
					if (a->u->type == unit_types::siege_tank_siege_mode || a->u->type == unit_types::siege_tank_tank_mode) hits += 10;
					else --hits;
				}
			} else if (!u->is_flying && d >= w_max_range + 64) hits += 20;
			if (u->type == unit_types::wraith || u->type == unit_types::valkyrie) {
				if (d <= w_max_range + 64) {
					if (e->type == unit_types::overlord) hits /= 4;
					if (e->type == unit_types::scourge) hits /= 100;
				}
			}
			if (e->type == unit_types::vulture && e->type->is_building && allies.size() < 12) hits += 100;
			if (e->type == unit_types::interceptor && enemy_interceptors < my_goliaths) hits += 10;
			if (e->type == unit_types::dark_templar) hits /= 4;
			if (e->irradiate_timer) hits += 2;
			//if (w->max_range > 32 && existing_dark_swarm.test(grid::build_square_index(e->pos))) hits += 10;
			if (w->max_range > 32 && existing_dark_swarm.test(grid::build_square_index(e->pos))) {
				if (u->type != unit_types::siege_tank_siege_mode && u->type != unit_types::lurker) {
					return std::make_tuple(std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity());
				}
			}
			//if (e->type == unit_types::overlord && a->u->type != unit_types::wraith) hits += 20;
			//		if (a->u->is_flying && a->last_win_ratio >= 4.0 && e->type->is_worker) hits /= 50;
			if (a->u->is_flying && enemy_units_that_hit_up == 0 && e->type->is_worker) hits /= 50;
			//if (a->last_win_ratio >= 2.0 && e->type->is_worker) hits /= 50;
			if (u->is_flying && u->cloaked && e->type->is_worker) hits /= 20;
			if (!e->cloaked && !e->detected && scans_available == 0) hits += 20;
			//if (u->type == unit_types::zergling && e->type == unit_types::siege_tank_siege_mode) hits /= 1000;
			//if (u->type == unit_types::lurker && e->type == unit_types::siege_tank_siege_mode) hits /= 1000;
			if (u->type == unit_types::zergling && e->type == unit_types::siege_tank_siege_mode) hits /= 2;
			else if (u->type == unit_types::lurker && e->type == unit_types::siege_tank_siege_mode) hits /= 4;
			else if (e->type == unit_types::siege_tank_siege_mode) hits /= 4;
			//if (u->type == unit_types::dragoon && e->type == unit_types::bunker && d <= w_max_range + 32) hits /= 100;
			if (d <= 64) hits /= 10;
			//else if (u->type == unit_types::dragoon && e->type == unit_types::vulture) hits += 1000;
			else if (w_max_range < 32 * 7 && enemy_siege_tank_area[grid::build_square_index(u->pos)] && e->type != unit_types::siege_tank_siege_mode) {
				if (w->damage_type != weapon_stats::damage_type_concussive) {
					hits += 1000;
				}
			}
			return std::make_tuple(hits, 0.0, 0.0);
		};
	} else {
		score = [&](unit*e) {

			if (e->shields + e->hp - prepared_damage[e] <= 0.0) return std::make_tuple(std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity());
			if (e->type == unit_types::interceptor && u->is_flying) return std::make_tuple(std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity());

			int w_damage_type = weapon_stats::damage_type_normal;
			double w_min_range = 0.0;
			double w_max_range = 128.0;
			weapon_stats*w = e->type->is_flyer ? u->stats->air_weapon : u->stats->ground_weapon;
			if (w) {
				w_damage_type = w->damage_type;
				w_min_range = w->min_range;
				w_max_range = w->max_range;
			} else {
				if (u->type == unit_types::carrier) {
					w_max_range = 32 * 8;
				} else return std::make_tuple(std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity());
			}
			double d = units_distance(u->pos, u->type, e->pos, e->type);
			if (d < w_min_range) return std::make_tuple(std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity());
			if (d > w_max_range) return std::make_tuple(std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity(), d);

			double hp = e->hp / combat_eval::get_damage_type_modifier(w_damage_type, e->type->size);
			double v = e->shields + hp;

			if (u->type == unit_types::dragoon && e->type == unit_types::interceptor) hp += 450.0;

			if (!(u->is_flying ? e->stats->air_weapon : e->stats->ground_weapon)) {
				bool is_priority_target = false;
				is_priority_target |= e->type == unit_types::carrier;
				is_priority_target |= e->type == unit_types::reaver;
				is_priority_target |= e->type == unit_types::high_templar;
				is_priority_target |= e->type == unit_types::defiler;
				is_priority_target |= e->type == unit_types::observer;
				is_priority_target |= e->type == unit_types::queen;
				is_priority_target |= e->type == unit_types::dark_archon;
				is_priority_target |= e->type == unit_types::science_vessel;
				if (!is_priority_target) return std::make_tuple(std::numeric_limits<double>::infinity(), v, 0.0);
			}

			return std::make_tuple(v, 0.0, 0.0);
		};
	}
	unit* target = nullptr;

	if (a->u->type == unit_types::siege_tank_tank_mode || a->u->type == unit_types::siege_tank_siege_mode) {
		if (has_siege_mode) {
			target = get_best_score(enemies, score);
			if (target && current_frame - target->last_seen < 15 * 8 && !target->is_flying) {
				double d = units_distance(a->u, target);
				int siege_tank_count = 0;
				int sieged_tank_count = 0;
				int unsieged_tank_count = 0;
				bool any_tanks_in_range_in_range = false;
				for (unit*u : allies) {
					if (u->type == unit_types::siege_tank_tank_mode || u->type == unit_types::siege_tank_siege_mode) {
						++siege_tank_count;
						if (!any_tanks_in_range_in_range && units_distance(u, target) <= 32 * 12) {
							if (units_distance(a->u, u) <= 32 * 8) any_tanks_in_range_in_range = true;
						}
					}
					if (u->type == unit_types::siege_tank_siege_mode) ++sieged_tank_count;
					if (u->type == unit_types::siege_tank_tank_mode) ++unsieged_tank_count;
				}
				bool unreachable = !square_pathing::unit_can_reach(a->u, a->u->pos, target->pos, square_pathing::pathing_map_index::include_liftable_wall);
				if (unreachable) a->siege_up_close = true;
				if (siege_tank_count >= 4 && target->visible && current_frame - target->last_shown <= 15 * 15) a->siege_up_close = false;
				if (!unreachable && enemy_tanks == 0 && any_tanks_in_range_in_range && !target->building && target->stats->max_speed >= 2.0 && !target->burrowed && (current_used_total_supply < 150 || a->last_win_ratio < 64.0) && d > 32 * 12) {
					if (a->u->type == unit_types::siege_tank_tank_mode) {
						if (current_frame >= a->u->controller->move_away_until && a->u->game_unit->siege()) {
							a->u->controller->noorder_until = current_frame + 30;
							a->u->controller->last_siege = current_frame;
							a->siege_up_close = false;
						}
					}
				} else {
					if (a->u->type == unit_types::siege_tank_tank_mode) {
						double add = target->stats->max_speed * 15;
						if (target->burrowed || target->type->is_non_usable) add = 0;
						if (current_frame - a->u->last_attacked <= 15 * 4) a->siege_up_close = false;
						if (a->siege_up_close) add = 0;
						if (d <= 32 * 12 + add && target->visible) {
							bool should_siege = true;
							if (siege_tank_count >= (int)allies.size() / 2 && siege_tank_count < 6) {
								int zealot_count = 0;
								for (unit*e : enemies) {
									if (e->type == unit_types::zealot) ++zealot_count;
									if (e->type == unit_types::dark_templar) ++zealot_count;
								}
								if (zealot_count >= (int)allies.size() - siege_tank_count) should_siege = false;
							}
							// 						if (target->type == unit_types::zealot && allies.size() < 8) {
							// 							if (units_distance(u, target) < 32 * 10) should_siege = false;
							// 						}

							if (units_distance(u, target) <= 32 * 6 && siege_tank_count < 4 && allies.size() < 8) {
								if (target->type == unit_types::vulture) should_siege = false;
								if (target->type == unit_types::spider_mine) should_siege = false;
								if (target->type == unit_types::lurker) {
									should_siege = false;
								}
							}
							if (allies.size() >= std::max(enemies.size() * 2, enemies.size() + 4)) {
								if (a->last_win_ratio >= 2.0) {
									should_siege = false;
								}
							}
							if (a->last_win_ratio >= 128.0) should_siege = false;
							if (!target->stats->ground_weapon) {
								bool any_ground_weapons = false;
								for (unit* e : enemies) {
									if (e->stats->ground_weapon) {
										any_ground_weapons = true;
										break;
									}
								}
								if (!any_ground_weapons) should_siege = false;
							}
							if (no_splash_target.count(target)) should_siege = false;
							//if (should_siege || (target && target->building)) {
							if (should_siege) {
								if (current_frame >= a->u->controller->move_away_until && a->u->game_unit->siege()) {
									a->u->controller->noorder_until = current_frame + 30;
									a->u->controller->last_siege = current_frame;
									a->siege_up_close = false;
								}
							}
						}
					} else {
						double add = target->stats->max_speed * 15 * 6;
						if (target->burrowed || target->type->is_non_usable) add = 0;
						if (a->siege_up_close && sieged_tank_count >= siege_tank_count / 2) add = 0;
						if (no_splash_target.count(target)) {
							if (a->u->game_unit->unsiege()) {
								a->u->controller->noorder_until = current_frame + 30;
							}
						} else if (d > 32 * 12 + add || (!target->visible && current_frame - a->u->controller->last_siege >= 15 * 8)) {
							if (current_frame - a->u->last_attacked >= 15 * 4 && current_frame - a->u->controller->last_siege >= 15 * 10) {
								if (!target->visible) a->siege_up_close = true;
								if (a->u->game_unit->unsiege()) {
									a->u->controller->noorder_until = current_frame + 30;
								}
							}
						} else {
							if (current_frame - a->u->controller->last_siege >= 15 * 20) {
								a->u->controller->last_siege = current_frame;
							}
							if (current_frame - a->u->controller->last_siege >= 15 * 10 && current_frame - a->u->last_attacked >= 15 * 4) {
								a->siege_up_close = true;
							}
						}
						if (current_frame - a->u->controller->last_siege >= 15 * 15 && current_frame - a->u->last_attacked >= 15 * 15) {
							a->siege_up_close = true;
						}
					}
				}
			} else {
				if (a->u->type == unit_types::siege_tank_siege_mode) {
					if (current_frame - a->u->last_attacked >= 15 * 4) {
						if (a->u->game_unit->unsiege()) {
							a->u->controller->noorder_until = current_frame + 30;
						}
					}
				}
			}
		}
	}

	if (a->u->type == unit_types::lurker) {
		target = get_best_score(enemies, score);
		if (target && current_frame - target->last_seen < 15 * 8 && !target->is_flying) {
			weapon_stats* w = target->is_flying ? a->u->stats->air_weapon : a->u->stats->ground_weapon;
			weapon_stats* ew = a->u->is_flying ? target->stats->ground_weapon : target->stats->air_weapon;
			bool up_close = (!w || !ew || ew->max_range > w->max_range) || a->last_win_ratio >= 8.0;
			xy upos = a->u->pos + xy((int)(a->u->hspeed*latency_frames), (int)(a->u->vspeed*latency_frames));
			xy tpos = target->pos + xy((int)(target->hspeed*latency_frames), (int)(target->vspeed*latency_frames));
			double d = units_distance(upos, a->u, tpos, target);
			double w_range = w ? w->max_range : 32 * 4;
			double ew_range = ew ? ew->max_range : 32;
			bool burrow = false;
			if (up_close) {
				if (d <= ew_range + 64 && d <= w_range) {
					burrow = true;
				}
				if (a->u->burrowed && d <= w_range) burrow = true;
			} else {
				if (d <= w_range) {
					burrow = true;
				}
			}
			int n_nearby_burrowed = 0;
			for (auto* u : allies) {
				if (u->type != unit_types::lurker) continue;
				if (!u->burrowed) continue;
				if (diag_distance(a->u->pos - u->pos) <= 32 * 6) ++n_nearby_burrowed;
			}
// 			burrow = d <= 32 * 2;
			if (!a->u->burrowed && a->last_win_ratio >= 4.0 && d > 64) {
				//if (current_frame - a->u->last_incoming_bullet > 15) burrow = false;
				burrow = false;
			}
			if (a->u->burrowed && d <= w_range) burrow = true;
			if (d <= w_range && n_nearby_burrowed == 0) burrow = true;
			//if (target->type == unit_types::siege_tank_siege_mode) burrow = d <= w_range;

			if (!a->u->burrowed && target->type == unit_types::vulture) {
				if (n_nearby_burrowed && d > 32) burrow = false;
			}
			if (a->u->burrowed) {
				if (current_frame - a->last_target_in_range_frame <= 60) burrow = true;
			}
			if (a->u->hp <= a->u->stats->hp / 2 && d <= w_range - 64) burrow = true;

			if (burrow) {
				a->subaction = combat_unit::subaction_idle;
				u->controller->action = unit_controller::action_idle;
			}

			if (current_frame - a->u->controller->last_burrow >= 20 + 37) {
				if (a->u->burrowed && !burrow) {
					if (current_frame >= a->u->controller->noorder_until) {
						a->u->game_unit->unburrow();
						a->u->controller->noorder_until = current_frame + 20;
						a->u->controller->last_burrow = current_frame;
					}
				}
				if (!a->u->burrowed && burrow) {
					a->u->game_unit->burrow();
					a->u->controller->last_burrow = current_frame;
				}
			}

		} else {
			if (current_frame - a->u->controller->last_burrow >= 20 + 37) {
				if (a->u->burrowed) {
					if (current_frame >= a->u->controller->noorder_until) {
						a->u->game_unit->unburrow();
						a->u->controller->noorder_until = current_frame + 20;
						a->u->controller->last_burrow = current_frame;
					}
				}
			}
		}
	}

	if (a->u->type == unit_types::dropship) {
		score = [&](unit*e) {
			if (e->is_flying) return std::make_tuple(std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity());
			double b = e->type == unit_types::siege_tank_siege_mode ? 0.0 : 1.0;
			double d = units_distance(u, e);
			if (dropship_spread[e] == 1) return std::make_tuple(std::numeric_limits<double>::infinity(), b, d);
			return std::make_tuple(0.0, b, d);
		};
	}
	bool targets_allies = false;
	if (a->u->type == unit_types::medic) {
		targets_allies = true;
		score = [&](unit* a) {
			if (!a->type->is_biological) return std::make_tuple(std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity());
			if (a->is_loaded) return std::make_tuple(std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity());
			if (is_being_healed[a]) return std::make_tuple(std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity());
			if (a == u) return std::make_tuple(std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity());
			//if (a->type == unit_types::medic && a->hp == a->stats->hp) return std::make_tuple(std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity());
			//double d = units_pathing_distance(u, a);
			double d = units_distance(u, a);
			return std::make_tuple(a->hp < a->stats->hp ? 0.0 : 1.0, d, 0.0);
		};
	}

	if (a->u->type == unit_types::queen) {
		score = [&](unit*e) {
			if (!e->visible || !e->detected) return std::make_tuple(std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity());
			if (e->type->is_robotic) return std::make_tuple(std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity());
			if (e->stasis_timer) return std::make_tuple(std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity());
			if (e->building) return std::make_tuple(std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity());
			if (e->type->is_worker) return std::make_tuple(std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity());
			if (e->lockdown_timer) return std::make_tuple(std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity());
			if (e->maelstrom_timer) return std::make_tuple(std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity());
			if (e->is_flying) return std::make_tuple(std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity());

			double r = units_distance(u, a->u) - 32 * 9;

			double value = u->minerals_value + u->gas_value;

			auto it = disable_target_taken.find(u);
			if (it != disable_target_taken.end() && std::get<0>(it->second) != a) return std::make_tuple(std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity());

			return std::make_tuple(r / value / ((u->shields + u->hp) / (u->stats->shields + u->stats->hp)), 0.0, 0.0);
		};
	}
	if (a->u->type == unit_types::defiler) {
		score = [&](unit*e) {
			if (!e->visible || !e->detected) return std::make_tuple(std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity());
			if (e->stasis_timer) return std::make_tuple(std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity());
			if (e->building) return std::make_tuple(std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity());
			if (e->type->is_worker) return std::make_tuple(std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity());
			if (e->lockdown_timer) return std::make_tuple(std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity());
			if (e->maelstrom_timer) return std::make_tuple(std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity());

			if (!e->stats->ground_weapon) return std::make_tuple(std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity());

			double r = units_distance(u, a->u);

			return std::make_tuple(r, 0.0, 0.0);
		};
	}

	if (a->u->is_loaded && a->u->loaded_into->type == unit_types::bunker) {
		target = get_best_score(enemies, [&](unit*e) {
			//if (e->cloaked && !e->detected) return std::numeric_limits<double>::infinity();
			return units_distance(a->u->loaded_into, e);
		}, std::numeric_limits<double>::infinity());
		a->target = target;
		bool unload = false;
		if (target) {
			double d = units_distance(a->u->loaded_into, target);
			if (!target->stats->ground_weapon) unload = true;
			else {
				if (d > 32 * 3 && target->stats->ground_weapon->max_range < 32 * 2) unload = true;
			}
			if (d >= 32 * 10 && !no_aggressive_groups) unload = true;
			if (d >= 32 * 20) unload = true;
			//if (d > 32 * 6) unload = true;
			if (!unload && d > 32 * 6) {
				unit*bunker = get_best_score(my_completed_units_of_type[unit_types::bunker], [&](unit*u) {
					if (space_left[u] < a->u->type->space_required) return std::numeric_limits<double>::infinity();
					double d = diag_distance(u->pos - a->u->pos);
					if (d >= 32 * 40) return std::numeric_limits<double>::infinity();
					return diag_distance(target->pos - u->pos);
				}, std::numeric_limits<double>::infinity());
				if (bunker != a->u->loaded_into) unload = true;
			}
			if (d < 32 * 7) unload = false;
			if (d > 32 * 6 && my_base.test(grid::build_square_index(target->pos))) unload = true;
			double wr = 0.0;
			if (a->u->stats->ground_weapon) wr = a->u->stats->ground_weapon->max_range + 32.0;
			if (d <= wr) {
				if ((target->cloaked || target->burrowing) && !target->detected && scans_available) {
					if (current_frame - last_attack_scan > 15) {
						last_attack_scan = current_frame;
						unit*u = get_best_score(my_units_of_type[unit_types::comsat_station], [&](unit*u) {
							return -u->energy;
						});
						if (u) {
							xy scan_pos = target->pos;
							if (target->speed > 4) {
								double target_heading = std::atan2(target->vspeed, target->hspeed);
								double max_distance = std::min(target->speed * 15 * 2, 32.0 * 4);
								for (double distance = 0; distance < max_distance; distance += 32) {
									xy pos = target->pos;
									pos.x += (int)(std::cos(target_heading)*distance);
									pos.y += (int)(std::sin(target_heading)*distance);
									pos = square_pathing::get_pos_in_square(pos, target->type);
									if (pos == xy()) break;
									scan_pos = pos;
								}
							}
							log("attack scan at %d %d\n", scan_pos.x, scan_pos.y);
							u->game_unit->useTech(BWAPI::TechTypes::Scanner_Sweep, BWAPI::Position(scan_pos.x, scan_pos.y));
						}
					}
				}
			}
		} else unload = true;
		//if (target && target->cloaked) unload = true;
		if (unload) {
			int marine_count = 0;
			for (unit* u : allies) {
				if (u->type == unit_types::marine) ++marine_count;
			}
			if (marine_count >= 4) {
				unit* nearest_building = get_best_score(my_buildings, [&](unit*u) {
					return diag_distance(u->pos - target->pos);
				});
				if (!nearest_building || nearest_building->type == unit_types::bunker) unload = false;
			}
			if (unload) {
				if (a->u->loaded_into->game_unit->unload(a->u->game_unit)) {
					a->u->controller->noorder_until = current_frame + 4;
				}
			}
		}
		return;
	}

	if (u->type == unit_types::wraith || u->type == unit_types::valkyrie) {
		std::tuple<double, double, double> best_score;
		for (unit* e : enemies) {
			auto s = score(e);
			if (!target || s < best_score) {
				best_score = s;
				target = e;
			}
		}
		for (unit* e : enemy_overlords) {
			if (e->gone) continue;
			if (diag_distance(u->pos - e->pos) >= 32 * 20) continue;
			auto s = score(e);
			if (!target || s < best_score) {
				best_score = s;
				target = e;
			}
		}
	}

	auto&targets = targets_allies ? allies : enemies;
	if (!target) {
//		if (a->last_target && current_frame - a->last_target_frame < 30 && !a->last_target->dead && !a->last_target->gone && a->last_target->owner->is_enemy && !a->last_target->building) {
//			weapon_stats* w = a->last_target->is_flying ? a->u->stats->air_weapon : a->u->stats->ground_weapon;
//			double d = units_distance(a->u, a->last_target);
//			if (w && d >= w->min_range && d <= w->max_range) {
//				target = a->last_target;
//			}
//		}
	}
	if (!target) target = get_best_score(targets, score);
	a->target = target;

	if (target) {
		a->last_target = target;
		a->last_target_frame = current_frame;
		weapon_stats* w = target->is_flying ? a->u->stats->air_weapon : a->u->stats->ground_weapon;
		double d = units_distance(a->u, target);
		if (w && d >= w->min_range && d <= w->max_range) a->last_target_in_range_frame = current_frame;
	}

	if (target && target->owner->rescuable) {
		a->subaction = combat_unit::subaction_move;
		a->target_pos = target->pos;
		return;
	}
	
	if (target && target->type == unit_types::interceptor) {
		unit* carrier = get_best_score(targets, [&](unit* e) {
			if (!e->visible || !e->detected || e->stasis_timer || e->invincible) return std::numeric_limits<double>::infinity();
			auto* w = e->is_flying ? a->u->stats->air_weapon : a->u->stats->ground_weapon;
			if (!w) return std::numeric_limits<double>::infinity();
			double d = std::max(units_distance(a->u, e) - w->max_range, 0.0);
			return d + (e->hp + e->shields) / 4.0;
		}, std::numeric_limits<double>::infinity());
		if (carrier) {
			if (a->u->is_flying) target = carrier;
			else {
				auto* w = carrier->is_flying ? a->u->stats->air_weapon : a->u->stats->ground_weapon;
				double x = carrier->pos.x - a->u->pos.x;
				double y = carrier->pos.y - a->u->pos.y;
				double l = (carrier->pos - a->u->pos).length();
				x /= l;
				y /= l;
				double d = std::max(units_distance(a->u, carrier) - w->max_range, 0.0);
				xy pos = a->u->pos + xy((int)(x * d), (int)(y * d));
				if (grid::get_build_square(pos).entirely_walkable) target = carrier;
			}
		}
	}

	if (target && ((target->cloaked && !target->detected) || target->burrowing || (!target->visible && units_distance(a->u, target) <= 32 * 12)) && (target->is_flying || !wants_to_lay_spider_mines || target->type == unit_types::lurker) && !a->u->type->is_detector && !a->u->is_loaded) {
		bool ignore = false;
		auto is_detected = [&](unit* n) {
			for (unit* u : my_detector_units) {
				if (!u->is_completed) continue;
				if (units_distance(u, n) <= u->stats->sight_range) return true;
			}
			return false;
		};
		if (!target->cloaked && !target->burrowing) {
			ignore = grid::get_build_square(target->pos).visible && current_frame - target->last_seen < 15 * 30;
			int min_scans_available = 3;
			if (a->u->type == unit_types::siege_tank_tank_mode || a->u->type == unit_types::siege_tank_siege_mode) min_scans_available = 1;
			if (scans_available < min_scans_available) ignore = true;
		}
		if (target->detected && target->burrowing) {
			if (is_detected(target)) ignore = true;
		}
		if (!target->game_unit->exists()) ignore = true;
		if (!target->game_unit->isVisible() || (target->game_unit->isDetected() && !target->burrowing)) ignore = true;
		need_detector.emplace_back(a, target);
		if (!ignore) {
			weapon_stats*ew = a->u->is_flying ? target->stats->air_weapon : target->stats->ground_weapon;
			bool scan = false;
			if (current_frame - last_attack_scan >= 15 * 5) {
				if (ew && scans_available) scan = true;
				if (!ew && scans_available > 2) scan = true;
			}
			if (current_frame - last_attack_scan > 15) {
				if (!scan && ew) {
					do_run(a, enemies);
					return;
				}
				weapon_stats*w = target->is_flying ? a->u->stats->air_weapon : a->u->stats->ground_weapon;
				double d = units_distance(a->u, target);
				if (w && d <= w->max_range) {
					if (scan && current_frame - last_attack_scan >= 15 * 5) {
						last_attack_scan = current_frame;
						unit*u = get_best_score(my_units_of_type[unit_types::comsat_station], [&](unit*u) {
							return -u->energy;
						});
						if (u) {
							xy scan_pos = target->pos;
							if (target->speed > 4) {
								double target_heading = std::atan2(target->vspeed, target->hspeed);
								double max_distance = std::min(target->speed * 15 * 2, 32.0 * 4);
								for (double distance = 0; distance < max_distance; distance += 32) {
									xy pos = target->pos;
									pos.x += (int)(std::cos(target_heading)*distance);
									pos.y += (int)(std::sin(target_heading)*distance);
									pos = square_pathing::get_pos_in_square(pos, target->type);
									if (pos == xy()) break;
									scan_pos = pos;
								}
							}
							log("attack scan at %d %d\n", scan_pos.x, scan_pos.y);
							u->game_unit->useTech(BWAPI::TechTypes::Scanner_Sweep, BWAPI::Position(scan_pos.x, scan_pos.y));
						}
					}
				}
			}
		}
	}

// 	if (target && a->u->type != unit_types::lurker && !target->is_flying && !a->u->is_flying && a->last_win_ratio < 32.0) {
// 
// 		unit*net = get_best_score(enemies, [&](unit*e) {
// 			if (e->is_flying) return std::numeric_limits<double>::infinity();
// 			if (e->type == unit_types::bunker) {
// 				return units_distance(e, a->u) - 32 * 6;
// 			}
// 			weapon_stats*ew = a->u->is_flying ? e->stats->air_weapon : e->stats->ground_weapon;
// 			if (!ew) return std::numeric_limits<double>::infinity();
// 			return units_distance(e, a->u) - ew->max_range;
// 		}, std::numeric_limits<double>::infinity());
// 
// 		if (a->u->stats->ground_weapon && net && net->visible && !(net->type->requires_pylon && !net->is_powered)) {
// 			bool okay = true;
// 			int lurker_count = 0;
// 			for (unit*u : allies) {
// 				if (u->type == unit_types::lurker) ++lurker_count;
// 			}
// 			int enemy_sieged_tanks_ready_to_fire = 0;
// 			for (unit*e : enemies) {
// 				if (e->type == unit_types::siege_tank_siege_mode && e->weapon_cooldown < 30) ++enemy_sieged_tanks_ready_to_fire;
// 			}
// 			if (enemies.size() <= 5 && allies.size() >= 10) {
// 				okay = false;
// 			}
// 			if (lurker_count < (int)allies.size() / 6) okay = false;
// 			if (enemy_sieged_tanks_ready_to_fire) okay = false;
// 			if (okay) {
// 				unit*nearest_lurker = get_best_score(allies, [&](unit*u) {
// 					if (u->type != unit_types::lurker) return std::numeric_limits<double>::infinity();
// 					return diag_distance(u->pos - a->u->pos);
// 				}, std::numeric_limits<double>::infinity());
// 				double nearest_lurker_distance = nearest_lurker ? units_distance(a->u, nearest_lurker) : 0;
// 				if (nearest_lurker && nearest_lurker_distance <= 32 * 20 && nearest_lurker_distance >= 32 * 1) {
// 
// 					weapon_stats*netw = a->u->is_flying ? net->stats->air_weapon : net->stats->ground_weapon;
// 					double wr = net->type == unit_types::bunker ? 32 * 6 : netw->max_range;
// 					double r = units_distance(net, nearest_lurker);
// 					if (r > wr + 32 * 1) {
// 						if (r > 32 * 6 - wr) {
// 							if (a->u->type->is_worker) {
// 								unit*nr = nullptr;
// 								double nd = 0.0;
// 								for (auto&s : resource_spots::spots) {
// 									auto*b = grid::get_build_square(s.cc_build_pos).building;
// 									if (!b || b->owner != players::my_player) continue;
// 									if (s.resources.empty()) continue;
// 									unit*r = s.resources.front().u;
// 									double d = diag_distance(a->u->pos - r->pos);
// 									if (d < nd) {
// 										nr = r;
// 										nd = d;
// 									}
// 								}
// 								if (nr) {
// 									a->subaction = combat_unit::subaction_idle;
// 									a->u->controller->action = unit_controller::action_gather;
// 									a->u->controller->target = nr;
// 									return;
// 								}
// 							}
// 
// 							bool just_attack = false;
// 							weapon_stats*w = target->is_flying ? a->u->stats->air_weapon : a->u->stats->ground_weapon;
// 							if (w && units_distance(a->u, target) <= w->max_range) {
// 								if (current_frame - nearest_lurker->last_attacked <= 15 * 5) {
// 									just_attack = true;
// 								}
// 							}
// 							if (!just_attack) {
// 								double max_d = diag_distance(a->u->pos - nearest_lurker->pos) + 32 * 1;
// 								a_deque<xy> path = find_path(a->u->type, a->u->pos, [&](xy pos, xy npos) {
// 									double d = diag_distance(npos - nearest_lurker->pos);
// 									if (d >= max_d) return false;
// 									if (build_square_taken.test(grid::build_square_index(npos))) return false;
// 									return true;
// 								}, [&](xy pos, xy npos) {
// 									double d = diag_distance(npos - nearest_lurker->pos);
// 									return d;
// 								}, [&](xy pos) {
// 									double d = (pos - nearest_lurker->pos).length();
// 									return d <= 32 * 6 - wr - 32 * 1;
// 								});
// 								//if (!path.empty() && diag_distance(path.back() - a->u->pos) > 32) {
// 								if (!path.empty()) {
// 									a->subaction = combat_unit::subaction_move;
// 									build_square_taken.set(grid::build_square_index(path.back()));
// 									a->target_pos = path.back();
// 								}
// 							}
// 						}
// 					}
// 				}
// 			}
// 		}
// 	}

	if (target && a->u->type != unit_types::lurker && !a->u->is_flying && !target->is_flying && a->last_win_ratio < 32.0) {
		if (target->type != unit_types::siege_tank_siege_mode) {
			int enemy_sieged_tanks = 0;
			for (unit*e : enemies) {
				if (e->type == unit_types::siege_tank_siege_mode) ++enemy_sieged_tanks;
			}
			if (enemy_sieged_tanks == 0) {
				int lurker_count = 0;
				bool any_lurkers_in_range = false;
				double r = units_distance(a->u, target);
				for (unit*u : allies) {
					if (u->type == unit_types::lurker) {
						++lurker_count;
						double d = units_distance(u, target);
						if (!any_lurkers_in_range && (d <= 32 * 5 || d < r)) any_lurkers_in_range = true;
					}
				}
				if (lurker_count >= 3 && !any_lurkers_in_range) {
					do_run(a, enemies);
					return;
				}
			}
		}
	}

	//if (target && a->u->type != unit_types::siege_tank_tank_mode && a->u->type != unit_types::siege_tank_siege_mode && a->u->type != unit_types::goliath && !a->u->is_flying) {
	if (target && a->u->type != unit_types::siege_tank_tank_mode && a->u->type != unit_types::siege_tank_siege_mode && a->u->type != unit_types::defiler && a->u->type != unit_types::reaver && !target->is_flying && !a->u->is_flying && a->last_win_ratio < 128.0) {

		unit*net = get_best_score(enemies, [&](unit*e) {
			if (e->is_flying) return std::numeric_limits<double>::infinity();
			if (e->type == unit_types::bunker) {
				return units_distance(e, a->u) - 32 * 6;
			}
			weapon_stats*ew = a->u->is_flying ? e->stats->air_weapon : e->stats->ground_weapon;
			if (!ew) return std::numeric_limits<double>::infinity();
			return units_distance(e, a->u) - ew->max_range;
		}, std::numeric_limits<double>::infinity());

		//if (a->u->stats->ground_weapon && (target->stats->ground_weapon || target->type == unit_types::bunker) && !target->is_flying && target->visible && !(target->type->requires_pylon && !target->is_powered)) {
		if (a->u->stats->ground_weapon && net && net->visible && !(net->type->requires_pylon && !net->is_powered)) {
			bool okay = true;
// 			int enemy_sieged_tanks = 0;
// 			for (unit*e : enemies) {
// 				if (e->type == unit_types::siege_tank_siege_mode) ++enemy_sieged_tanks;
// 			}
// 			if (enemy_sieged_tanks >= 3) okay = false;
			int siege_tank_count = 0;
			int sieged_tank_count = 0;
			int unsieged_tank_count = 0;
			int defiler_count = 0;
			int lurker_count = 0;
			for (unit*u : allies) {
				if (u->type == unit_types::siege_tank_tank_mode || u->type == unit_types::siege_tank_siege_mode) ++siege_tank_count;
				if (u->type == unit_types::siege_tank_siege_mode) ++sieged_tank_count;
				if (u->type == unit_types::siege_tank_tank_mode) ++unsieged_tank_count;
				if (u->type == unit_types::defiler) ++defiler_count;
				if (u->type == unit_types::lurker) ++lurker_count;
			}
			int enemy_sieged_tanks_ready_to_fire = 0;
			for (unit*e : enemies) {
				if (e->type == unit_types::siege_tank_siege_mode && e->weapon_cooldown < 30) ++enemy_sieged_tanks_ready_to_fire;
			}
// 			if (enemy_sieged_tanks_ready_to_fire > unsieged_tank_count && target->type == unit_types::siege_tank_siege_mode) {
// 				do_run(a, enemies);
// 				return;
// 			}
//			if (enemies.size() <= 5 && allies.size() >= 10) {
//				okay = false;
//			}
			if (enemies.size() <= 3) {
				okay = false;
			}
			if (siege_tank_count) {
				if (siege_tank_count < (int)allies.size() / 12) okay = false;
			}
			if (okay) {
				unit* nearest_siege_tank = get_best_score(allies, [&](unit*u) {
					if (u->type != unit_types::siege_tank_tank_mode && u->type != unit_types::siege_tank_siege_mode && u->type != unit_types::defiler && u->type != unit_types::lurker && u->type != unit_types::reaver) return std::numeric_limits<double>::infinity();
					return diag_distance(u->pos - a->u->pos);
				}, std::numeric_limits<double>::infinity());
				double nearest_siege_tank_distance = nearest_siege_tank ? units_distance(a->u, nearest_siege_tank) : 0;
				if (nearest_siege_tank && nearest_siege_tank_distance <= 32 * 20 && nearest_siege_tank_distance >= 32 * 2) {

					if (enemy_sieged_tanks_ready_to_fire >= 6) {
						if (!target->visible) {
							if (scans_available > 2 && current_frame - last_attack_scan >= 15 * 2) {
								last_attack_scan = current_frame;
								unit*u = get_best_score(my_units_of_type[unit_types::comsat_station], [&](unit*u) {
									return -u->energy;
								});
								if (u) {
									log("lots of siege tanks scan at %d %d\n", u->pos.x, u->pos.y);
									u->game_unit->useTech(BWAPI::TechTypes::Scanner_Sweep, BWAPI::Position(u->pos.x, u->pos.y));
								}
							}
						}
// 						if (units_distance(nearest_siege_tank, target) > 32 * 13) {
// 							do_run(a, enemies);
// 							return;
// 						}
					}

					int max_range = nearest_siege_tank->type == unit_types::defiler ? 32 * 9 : 32 * 12;
					if (nearest_siege_tank->type == unit_types::lurker) max_range = 32 * 6;
					if (nearest_siege_tank->type == unit_types::reaver) max_range = 32 * 8;

					weapon_stats*netw = a->u->is_flying ? net->stats->air_weapon : net->stats->ground_weapon;
					double wr = net->type == unit_types::bunker ? 32 * 6 : netw->max_range;
					double r = units_distance(net, nearest_siege_tank);
					//if (r > wr + 32 * 2) {
					if (r > wr) {
						if (r > max_range - wr) {
							if (a->u->type->is_worker) {
								unit*nr = nullptr;
								double nd = 0.0;
								for (auto&s : resource_spots::spots) {
									auto*b = grid::get_build_square(s.cc_build_pos).building;
									if (!b || b->owner != players::my_player) continue;
									if (s.resources.empty()) continue;
									unit*r = s.resources.front().u;
									double d = diag_distance(a->u->pos - r->pos);
									if (d < nd) {
										nr = r;
										nd = d;
									}
								}
								if (nr) {
									a->subaction = combat_unit::subaction_idle;
									a->u->controller->action = unit_controller::action_gather;
									a->u->controller->target = nr;
									return;
								}
							}

							bool just_attack = false;
							weapon_stats*w = target->is_flying ? a->u->stats->air_weapon : a->u->stats->ground_weapon;
							if (w && units_distance(a->u, target) <= w->max_range) {
								if (current_frame - nearest_siege_tank->last_attacked <= 15 * 5) {
									just_attack = true;
								}
							}
							if (!just_attack) {
								double max_d = diag_distance(a->u->pos - nearest_siege_tank->pos) + 32 * 2;
								a_deque<xy> path = find_path(a->u->type, a->u->pos, [&](xy pos, xy npos) {
									double d = diag_distance(npos - nearest_siege_tank->pos);
									if (d >= max_d) return false;
									if (build_square_taken.test(grid::build_square_index(npos))) return false;
									return true;
								}, [&](xy pos, xy npos) {
									double d = diag_distance(npos - nearest_siege_tank->pos);
									return d;
								}, [&](xy pos) {
									double d = (pos - nearest_siege_tank->pos).length();
									return d <= max_range - wr - 32 * 3;
								});
								//if (!path.empty() && diag_distance(path.back() - a->u->pos) > 32) {
								if (!path.empty()) {
									a->subaction = combat_unit::subaction_move;
									build_square_taken.set(grid::build_square_index(path.back()));
									a->target_pos = path.back();
								}
							}
						}
					}
				}
			}
		}
	}

	if (a->u->type == unit_types::marine && allies.size() < 20 && enemies.size() < 10) {
		int my_workers = 0;
		int my_marines = 0;
		for (unit*u : allies) {
			if (u->type->is_worker) ++my_workers;
			if (u->type == unit_types::marine) ++my_marines;
		}
		if (my_workers + my_marines == allies.size()) {
			int op_marines = 0;
			for (unit*e : enemies) {
				if (e->type == unit_types::marine) ++op_marines;
			}
			if (my_marines < op_marines / 2) {
				if (diag_distance(defence_choke.inside - a->u->pos) >= 32 * 4) {
					a->subaction = combat_unit::subaction_move;
					a->target_pos = defence_choke.inside;
				}
			}
		}
		bool enemy_has_bunker = false;
		for (unit*e : enemies) {
			if (e->type == unit_types::bunker) enemy_has_bunker = true;
		}
		if (target && enemy_has_bunker && ((target->type == unit_types::bunker && target->marines_loaded) || target->type == unit_types::marine)) {
			do_run(a, enemies);
			return;
		}
	}
	if (current_frame < 15 * 60 * 9 && allies.size() < 9 && (a->u->type == unit_types::zealot || a->u->type == unit_types::zergling) && a->last_win_ratio < 2.0) {
		bool enemy_has_bunker = false;
		for (unit* e : enemies) {
			if (e->type == unit_types::bunker && units_distance(a->u, e) <= 32 * 6) enemy_has_bunker = true;
		}
		if (target && enemy_has_bunker && ((target->type == unit_types::bunker && target->marines_loaded) || target->type == unit_types::marine)) {
			do_run(a, enemies);
			return;
		}
	}

	if (a->u->type->is_worker && a->last_win_ratio < 16.0) {
		bool enemy_has_bunker = false;
		for (unit*e : enemies) {
			if (e->type == unit_types::bunker) enemy_has_bunker = true;
		}
		if (target && enemy_has_bunker && ((target->type == unit_types::bunker && target->marines_loaded) || target->type == unit_types::marine)) {
			if (allies.size() < 20 && enemies.size() < 10) {
				a->subaction = combat_unit::subaction_idle;
				if (a->u->controller->action != unit_controller::action_gather && a->u->controller->action != unit_controller::action_build) {
					a->u->controller->action = unit_controller::action_idle;
				}
			}
		}
	}

	if ((a->u->type == unit_types::marine || a->u->type == unit_types::firebat) && target && !a->u->is_loaded && a->last_win_ratio < 8.0) {
		double distance_to_target = units_distance(a->u, target);
		unit* bunker = get_best_score(my_completed_units_of_type[unit_types::bunker], [&](unit*u) {
			if (space_left[u] < a->u->type->space_required) return std::numeric_limits<double>::infinity();
			double d = diag_distance(u->pos - a->u->pos);
			if (d >= 32 * 40) return std::numeric_limits<double>::infinity();
			if (units_distance(u, a->u) > distance_to_target) return std::numeric_limits<double>::infinity();
			return diag_distance(target->pos - u->pos);
		}, std::numeric_limits<double>::infinity());
		if (a->u->type == unit_types::firebat) {
			if (target->stats->ground_weapon && target->stats->ground_weapon->max_range > 32 * 3) bunker = nullptr;
		}
		if (bunker) {
			double d = units_distance(a->u, bunker);
			bool run_to_bunker = a->last_win_ratio < 8.0;
			if (!target->stats->ground_weapon) run_to_bunker = false;
			if (units_distance(bunker, target) > 32 * 7) {
				unit*nearest_building = get_best_score(my_buildings, [&](unit*u) {
					return diag_distance(u->pos - target->pos);
				});
				if (nearest_building != bunker) run_to_bunker = false;
			}
			if (units_distance(bunker, target) < 32 * 10 && d < 32 * 40 && run_to_bunker) {
				if (units_distance(a->u, bunker) <= 32 * 3) space_left[bunker] -= a->u->type->space_required;
				a->subaction = combat_unit::subaction_idle;
				a->u->controller->action = unit_controller::action_idle;
				if (current_frame >= a->u->controller->noorder_until || a->u->order_target != bunker) {
					if ((a->u->type == unit_types::marine || a->u->type == unit_types::firebat) && a->u->owner->has_upgrade(upgrade_types::stim_packs) && a->last_win_ratio < 8.0) {
						if (a->u->stim_timer <= latency_frames && current_frame >= a->u->controller->nospecial_until) {
							a->u->game_unit->useTech(upgrade_types::stim_packs->game_tech_type);
							a->u->controller->noorder_until = current_frame + 2;
							a->u->controller->nospecial_until = current_frame + latency_frames + 4;
						}
					}
					if (a->u->game_unit->rightClick(bunker->game_unit)) {
						a->u->controller->noorder_until = current_frame + 30;
					}
				}
				return;
			}
		}
	}

	if (a->u->type == unit_types::carrier && target) {
//		bool move_back = false;
//		for (unit* e : enemies) {
//			auto* w = a->u->is_flying ? e->stats->air_weapon : e->stats->ground_weapon;
//			if (!w) continue;
//			double d = units_distance(a->u, e);
//			if (d >= 32 * 10) continue;
//			if (d <= w->max_range + 64) move_back = true;
//		}
//		if (move_back && units_distance(a->u, target) <= 32 * 10 - 16) {
//			bool is_attacking = true;
//			for (auto u : a->u->game_unit->getInterceptors()) {
//				if (u->getOrder() != BWAPI::Orders::InterceptorAttack) {
//					is_attacking = false;
//					break;
//				}
//			}
//			if (is_attacking) {
//				do_run(a, enemies);
//				return;
//			}
//		}
		
		bool any_threats = false;
		for (unit* e : enemies) {
			auto* w = a->u->is_flying ? e->stats->air_weapon : e->stats->ground_weapon;
			double wr = w ? w->max_range : 0.0;
			if (e->type == unit_types::bunker) wr = 32 * 6;
			if (!w) continue;
			double d = units_distance(a->u, e);
			if (d <= wr + 32 * 6) {
				any_threats = true;
				break;
			}
		}
		if (!any_threats) a->u->controller->carrier_no_threats_until = current_frame + 15;
	}
	if (a->u->type == unit_types::reaver && target) {
		bool move_back = false;
		for (unit* e : enemies) {
			auto* w = a->u->is_flying ? e->stats->air_weapon : e->stats->ground_weapon;
			if (!w) continue;
			double d = units_distance(a->u, e);
			if (d > 32 * 8) continue;
			if (d <= w->max_range + 64) move_back = true;
		}
		if (move_back && a->u->weapon_cooldown > latency_frames) {
			do_run(a, enemies);
			return;
		}
	}
	
	if (a->u->type == unit_types::medic && target) {
		unit* enemy_target = get_best_score(enemies, [&](unit* e) {
			return diag_distance(a->u->pos - e->pos);
		});
		if (enemy_target) {
			if (target->hp == target->stats->hp && units_distance(a->u, target) <= 32) {
				a->target = enemy_target;
			}
		}
	}

	if (target) {
		double r = units_distance(a->u, target);
		if (a->u->type == unit_types::dropship) {
			dropship_spread[target] = 1;
			a->target = nullptr;
			a->subaction = combat_unit::subaction_move;
			a->target_pos = target->pos;
			a->target_pos.x += (int)(cos(current_frame / 23.8) * 48);
			a->target_pos.y += (int)(sin(current_frame / 23.8) * 48);
		}

		if (a->u->type == unit_types::medic && r < 30) {
			is_being_healed[target] = true;
		}
	}
	//if (target && (a->u->type == unit_types::marine || a->u->type == unit_types::ghost || a->u->type == unit_types::hydralisk)) {
	if (target && (a->u->type == unit_types::marine || a->u->type == unit_types::ghost)) {
		weapon_stats*splash_weapon = nullptr;
		unit*targeted_by_lurker = nullptr;
		int lurker_count = 0;
		int reaver_count = 0;
		for (unit*e : enemies) {
			if (e->type == unit_types::reaver && units_distance(a->u, e) <= 32 * 8) ++reaver_count;
			weapon_stats*e_weapon = a->u->is_flying ? e->stats->air_weapon : e->stats->ground_weapon;
			if (!e_weapon) continue;
			if (e_weapon->explosion_type != weapon_stats::explosion_type_radial_splash && e_weapon->explosion_type != weapon_stats::explosion_type_enemy_splash) continue;
			double max_range = e_weapon->max_range;
			//if (e->type == unit_types::lurker && e->burrowed) max_range = 32 * 8;
			if (units_distance(a->u, e) > max_range) continue;
			if (e->type == unit_types::lurker && (!e->burrowed || e->game_order == BWAPI::Orders::Burrowing)) continue;
			//if (e->type == unit_types::lurker) continue;
			if (e->type == unit_types::lurker && e->burrowed) {
				targeted_by_lurker = e;
				++lurker_count;
			}
			splash_weapon = e_weapon;
		}
		if (lurker_count > 2) targeted_by_lurker = nullptr;
		//if (splash_weapon && allies.size() < 20) {
		if ((splash_weapon && a->u->type != unit_types::hydralisk) || reaver_count) {
			bool do_lurker_dodge = targeted_by_lurker && current_frame - a->last_lurker_dodge >= 37;
			if (do_lurker_dodge) {
				//if (targeted_by_lurker->weapon_cooldown > latency_frames && targeted_by_lurker->weapon_cooldown <= a->last_lurker_dodge_cooldown) do_lurker_dodge = false;
			}
			if (targeted_by_lurker && targeted_by_lurker->weapon_cooldown > a->last_lurker_dodge_cooldown) a->last_lurker_dodge = current_frame - 4 - latency_frames;
			if (targeted_by_lurker) a->last_lurker_dodge_cooldown = targeted_by_lurker->weapon_cooldown;
// 			if (targeted_by_lurker) {
// 				weapon_stats*w = targeted_by_lurker->is_flying ? a->u->stats->air_weapon : a->u->stats->ground_weapon;
// 				if (w && units_distance(a->u, targeted_by_lurker) < w->max_range - 32) {
// 					xy relpos = a->u->pos - targeted_by_lurker->pos;
// 					double ang = std::atan2(relpos.y, relpos.x);
// 
// 					xy move_to = a->u->pos;
// 					move_to.x += (int)(std::cos(ang + PI / 8 * a->lurker_dodge_dir) * 52.0);
// 					move_to.y += (int)(std::sin(ang + PI / 8 * a->lurker_dodge_dir) * 52.0);
// 
// 					move_to = square_pathing::get_pos_in_square(move_to, a->u->type);
// 					if (move_to != xy()) {
// 						a->subaction = combat_unit::subaction_move_directly;
// 						a->target_pos = move_to;
// 						do_lurker_dodge = false;
// 					}
// 				}
// 			}
			if (a->u->defensive_matrix_timer) {
				if (units_distance(target, a->u) > 32) {
					xy move_to = square_pathing::get_pos_in_square(target->pos, a->u->type);
					if (move_to != xy()) {
						a->subaction = combat_unit::subaction_move_directly;
						a->target_pos = target->pos;
					}
				}
			} else if (do_lurker_dodge || current_frame < a->lurker_dodge_until) {

				if (do_lurker_dodge) {
					xy relpos = targeted_by_lurker->pos - a->u->pos;
					double ang = std::atan2(relpos.y, relpos.x);

					xy move_to = a->u->pos;
					move_to.x += (int)(std::cos(ang + PI / 2 * a->lurker_dodge_dir) * 56.0);
					move_to.y += (int)(std::sin(ang + PI / 2 * a->lurker_dodge_dir) * 56.0);
					move_to = square_pathing::get_pos_in_square(move_to, a->u->type);
					if (move_to != xy()) {
						a->last_lurker_dodge = current_frame;
						a->lurker_dodge_dir = a->lurker_dodge_dir > 0 ? -1 : 1;
						//a->lurker_dodge_until = current_frame + (int)(46.0 / (a->u->stim_timer ? a->u->stats->max_speed * 1.5 : a->u->stats->max_speed)) + 2;
						a->lurker_dodge_until = current_frame + 15;
					}
					a->lurker_dodge_to = move_to;
				}

				if (a->lurker_dodge_to != xy()) {
					a->subaction = combat_unit::subaction_move_directly;
					a->target_pos = a->lurker_dodge_to;

					xy pos = a->u->pos + xy((int)(a->u->hspeed*latency_frames), (int)(a->u->vspeed*latency_frames));
					if ((a->target_pos - pos).length() <= 4) a->lurker_dodge_until = 0;
				}

			} else {
				a_vector<unit*> too_close;
				//double radius = splash_weapon->outer_splash_radius;
				double radius = 56;
				for (unit*u : allies) {
					double d = (u->pos - a->u->pos).length();
					if (d <= radius) too_close.push_back(u);
				}
				if (too_close.size() > 1) {
					unit*nearest = get_best_score(too_close, [&](unit*u) {
						return diag_distance(target->pos - u->pos);
					});
					if (nearest != a->u) {
						double avg_dang = 0.0;
						size_t avg_count = 0;
						xy relpos = target->pos - a->u->pos;
						double ang = std::atan2(relpos.y, relpos.x);
						for (unit*u : allies) {
							double d = (u->pos - a->u->pos).length();
							if (d <= 64) {
								xy relpos = target->pos - u->pos;
								double dang = ang - std::atan2(relpos.y, relpos.x);
								if (dang < -PI) dang += PI * 2;
								if (dang > PI) dang -= PI * 2;
								avg_dang += dang;
								++avg_count;
							}
						}
						avg_dang /= avg_count;
						if (avg_dang < 0) ang += PI / 2;
						else ang -= PI / 2;
						xy npos;
						for (double add = 0.0; add <= PI / 2; add += PI / 8) {
							npos = a->u->pos;
							npos.x += (int)(std::cos(ang + (avg_dang < 0 ? -add : add)) * 128.0);
							npos.y += (int)(std::sin(ang + (avg_dang < 0 ? -add : add)) * 128.0);
							bool okay = !test_pred(spread_positions, [&](xy p) {
								return (p - npos).length() <= radius;
							});
							if (okay) break;
							break;
						}
						spread_positions.push_back(npos);
						a->subaction = combat_unit::subaction_move_directly;
						a->target_pos = npos;
					} else {
						double d = units_distance(a->u, target);
						if (splash_weapon && splash_weapon->min_range && d > splash_weapon->min_range) {
							a->subaction = combat_unit::subaction_move_directly;
							a->target_pos = target->pos;
						}
					}
				}
			}
		}
	}

	int do_retreat_until = 0;
	if (current_frame < a->try_retreat_until) do_retreat_until = a->try_retreat_until;

	//if (target && (a->u->type->is_worker || (a->u->type == unit_types::zergling && a->u->hp < a->u->stats->hp / 2 && allies.size() < 16 && a->last_win_ratio >= 1.5 && allies.size() > enemies.size()))) {
	if (target && a->u->type->is_worker) {
		weapon_stats*w = a->u->is_flying ? target->stats->air_weapon : target->stats->ground_weapon;
		if (w) {
			double total_damage = 0.0;
			double total_dps = 0.0;
			for (unit*e : enemies) {
				if (!a->u->type->is_worker && e->last_attack_target != a->u) continue;
				weapon_stats*w = a->u->is_flying ? e->stats->air_weapon : e->stats->ground_weapon;
				if (!w) continue;
				if (w->max_range >= 32 * 2) continue;
				if (units_distance(a->u, e) > w->max_range + 16) continue;

				double damage = w->damage;
				if (a->u->shields <= 0) damage *= combat_eval::get_damage_type_modifier(w->damage_type, a->u->stats->type->size);
				damage -= a->u->stats->armor;
				if (damage <= 0) damage = 1.0;
				damage *= w == target->stats->ground_weapon ? target->stats->ground_weapon_hits : target->stats->air_weapon_hits;
				double dps = damage * (15.0 / w->cooldown);

				total_damage += damage;
				if (e->last_attack_target == a->u) {
					total_dps += dps;
				}
			}
			log("%p: %g vs %g (or %g)\n", a->u, a->u->hp, total_damage, total_dps * 2);
			if (a->u->hp < a->u->stats->hp && (a->u->hp <= total_damage || a->u->hp <= total_dps * 2) && units_distance(a->u, target) <= w->max_range + 64) {
				do_retreat_until = current_frame + 15 * 4;
			}
		}
	}
	if (do_retreat_until) {
		a->retreat_until = do_retreat_until;
		if (a->u->type->is_worker) {
			unit*nr = get_best_score(resource_units, [&](unit*r) {
				if (!r->visible) return std::numeric_limits<double>::infinity();
				double d = diag_distance(a->u->pos - r->pos);
				if (d <= 32 * 3) return std::numeric_limits<double>::infinity();
				return d;
			}, std::numeric_limits<double>::infinity());
			if (nr) {
				log("send worker back to mining\n");
				if (a->u->game_unit->getOrder() == BWAPI::Orders::AttackUnit) a->u->game_unit->gather(nr->game_unit);
				a->subaction = combat_unit::subaction_idle;
				a->u->controller->action = unit_controller::action_gather;
				a->u->controller->target = nr;
			}
		} else {
			if (!a->u->is_flying) {

				auto&dims = a->u->type->dimensions;

				auto can_move_to = [&](xy pos) {
					auto test = [&](xy pos) {
						int right = pos.x + dims[0];
						int bottom = pos.y + dims[1];
						int left = pos.x - dims[2];
						int top = pos.y - dims[3];
						for (unit*u : allies) {
							if (u == a->u) continue;
							if (u->is_flying || u->burrowed) continue;
							int uright = u->pos.x + u->type->dimension_right();
							int ubottom = u->pos.y + u->type->dimension_down();
							int uleft = u->pos.x - u->type->dimension_left();
							int utop = u->pos.y - u->type->dimension_up();
							if (right >= uleft && bottom >= utop && uright >= left && ubottom >= top) return false;
						}
						for (unit*u : enemies) {
							if (u->is_flying || u->burrowed) continue;
							int uright = u->pos.x + u->type->dimension_right();
							int ubottom = u->pos.y + u->type->dimension_down();
							int uleft = u->pos.x - u->type->dimension_left();
							int utop = u->pos.y - u->type->dimension_up();
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

				a_deque<xy> path = find_path(a->u->type, a->u->pos, [&](xy pos, xy npos) {
					if (diag_distance(npos - target->pos) <= diag_distance(pos - target->pos)) return false;
					return can_move_to(npos);
				}, [&](xy pos, xy npos) {
					return 0.0;
				}, [&](xy pos) {
					return diag_distance(pos - a->u->pos) >= 32 * 2;
				});
				log("zling path.size() is %d\n", path.size());

				if (!path.empty()) {
					a->subaction = combat_unit::subaction_move_directly;
					a->target_pos = path.back();
					return;
				}

			} else {
				do_run(a, enemies);
				return;
			}
		}
	}

	if (target && (a->u->type == unit_types::science_vessel || a->u->type == unit_types::overlord || a->u->type == unit_types::observer)) {
	//if (target && a->u->type == unit_types::science_vessel) {
		bool moved = false;
		if (a->u->irradiate_timer) {
			unit*target = get_best_score(enemies, [&](unit*e) {
				if (e->building) return std::numeric_limits<double>::infinity();
				if (e->type == unit_types::egg || e->type == unit_types::lurker_egg) return std::numeric_limits<double>::infinity();
				if (e->burrowed) return std::numeric_limits<double>::infinity();
				if (e->stasis_timer) return std::numeric_limits<double>::infinity();
				if (e->irradiate_timer) return std::numeric_limits<double>::infinity();
				if (!e->type->is_biological) return std::numeric_limits<double>::infinity();
				return units_distance(e, a->u);
			}, std::numeric_limits<double>::infinity());
			if (target) {
				a->subaction = combat_unit::subaction_move;
				xy tpos = target->pos + xy((int)(target->hspeed*(latency_frames + 4)), (int)(target->vspeed*(latency_frames + 4)));
				a->target_pos = tpos;
				moved = true;
			}
		}
		if (!moved && entire_threat_area.test(grid::build_square_index(a->u->pos))) {

			unit*cloaked = get_best_score(enemies, [&](unit*e) {
				if (!e->cloaked && e->type != unit_types::lurker) return std::numeric_limits<double>::infinity();
				return units_distance(e, a->u);
			}, std::numeric_limits<double>::infinity());

			unit*net = get_best_score(enemies, [&](unit*e) {
				if (e->type == unit_types::bunker) {
					return units_distance(e, a->u) - 32 * 6;
				}
				weapon_stats*ew = a->u->is_flying ? e->stats->air_weapon : e->stats->ground_weapon;
				if (!ew) return std::numeric_limits<double>::infinity();
				return units_distance(e, a->u) - ew->max_range;
			}, std::numeric_limits<double>::infinity());
			if (net) {

				if (!cloaked) {
					do_run(a, enemies);
					return;
				}

				weapon_stats*netw = a->u->is_flying ? net->stats->air_weapon : net->stats->ground_weapon;
				double wr = net->type == unit_types::bunker ? 32 * 6 : netw->max_range;
				double max_d = 32 * 8;
				a_deque<xy> path = find_path(a->u->type, a->u->pos, [&](xy pos, xy npos) {
					double d = diag_distance(npos - a->u->pos);
					if (d >= max_d) return false;
					return true;
				}, [&](xy pos, xy npos) {
					return diag_distance(npos - cloaked->pos);
				}, [&](xy pos) {
					if (units_distance(pos, a->u->type, cloaked->pos, cloaked->type) <= a->u->stats->sight_range) {
						if (units_distance(pos, a->u->type, net->pos, net->type) >= wr + 32 * 2) {
							return true;
						}
					}
					return false;
				});
				if (!path.empty()) {
					a->subaction = combat_unit::subaction_move;
					a->target_pos = path.back();
				}

			} else {
				if (cloaked) {
					a->subaction = combat_unit::subaction_move;
					a->target_pos = cloaked->pos;
				}
			}


		}
	}

	if (a->u->irradiate_timer) {
		bool any_biological_allies_nearby = false;
		for (unit* u : allies) {
			if (a->u->type->is_non_usable) continue;
			if (u->type->is_biological) {
				if (diag_distance(a->u->pos - u->pos) <= 32 * 4) {
					any_biological_allies_nearby = true;
					break;
				}
			}
		}
		if (any_biological_allies_nearby) {
			unit*target = get_best_score(enemies, [&](unit*e) {
				if (e->building) return std::numeric_limits<double>::infinity();
				if (e->type == unit_types::egg || e->type == unit_types::lurker_egg) return std::numeric_limits<double>::infinity();
				if (e->burrowed) return std::numeric_limits<double>::infinity();
				if (e->stasis_timer) return std::numeric_limits<double>::infinity();
				if (e->irradiate_timer) return std::numeric_limits<double>::infinity();
				if (!e->type->is_biological) return std::numeric_limits<double>::infinity();
				return units_distance(e, a->u);
			}, std::numeric_limits<double>::infinity());
			if (target) {
				a->subaction = combat_unit::subaction_move;
				xy tpos = target->pos + xy((int)(target->hspeed*(latency_frames + 4)), (int)(target->vspeed*(latency_frames + 4)));
				a->target_pos = tpos;
				return;
			} else {
				a->subaction = combat_unit::subaction_move;
				a->target_pos = a->goal_pos;
				return;
			}
		}
	}

	if (a->u->type == unit_types::shuttle) {
		if (u->shields + u->hp < (u->stats->shields + u->stats->hp) / 2) {
			do_run(a, enemies);
			return;
		}
		unit*target = get_best_score(allies, [&](unit*u) {
			if (u->is_flying) return std::numeric_limits<double>::infinity();
			if (u->is_loaded) return std::numeric_limits<double>::infinity();
			if (shuttle_target_taken.count(u)) return std::numeric_limits<double>::infinity();
			return -(u->type->total_gas_cost + u->type->total_minerals_cost - (u->shields + u->hp) / 100.0);
		}, std::numeric_limits<double>::infinity());
		if (target) {
			a->subaction = combat_unit::subaction_fight;
			a->target = target;
			shuttle_target_taken.insert(a->target);
			return;
		}
		if (!a->u->loaded_units.empty()) {
			a->subaction = combat_unit::subaction_fight;
			a->target = a->u->loaded_units.front();
			shuttle_target_taken.insert(a->target);
			return;
		}
	}

	if (target && a->u->type == unit_types::carrier) {
		auto interceptors = u->game_unit->getInterceptors();
		int n_attacking = 0;
		int n_damaged = 0;
		for (auto&game_unit : interceptors) {
			if (game_unit->getOrder() == BWAPI::Orders::InterceptorAttack) ++n_attacking;
		}
		if (n_attacking > (int)interceptors.size() / 2) {
			double d = units_distance(u, target);
			if (d < 32 * 10) {
				prepared_damage[target] += 6 * u->game_unit->getInterceptorCount();
// 				do_run(a, enemies);
// 				a->subaction = combat_unit::subaction_idle;
// 				if (current_frame > u->controller->noorder_until) {
// 					u->game_unit->attack(BWAPI::Position(a->target_pos.x, a->target_pos.y));
// 					u->controller->noorder_until = current_frame + 10;
// 				}
				return;
			}
		}
	}

// 	if (target && u->type == unit_types::dragoon && target->type == unit_types::interceptor) {
// 		unit*carrier = get_best_score(enemies, [&](unit*e) {
// 			if (e->type != unit_types::carrier) return std::numeric_limits<double>::infinity();
// 			return diag_distance(u->pos - e->pos);
// 		}, std::numeric_limits<double>::infinity());
// 		if (carrier) {
// 			a->subaction = combat_unit::subaction_kite;
// 			a->target = carrier;
// 			a->target_pos = carrier->pos;
// 			a->kite_attack_target = target;
// 		}
// 	}

	if (entire_threat_area.test(grid::build_square_index(a->u->pos))) {
		bool dodge_enemies = false;
		if (a->u->type == unit_types::defiler) dodge_enemies = true;
		if (a->u->type == unit_types::queen) dodge_enemies = true;
		if (a->u->type == unit_types::high_templar && (a->u->energy < 75 || a->u->shields < a->u->stats->shields)) dodge_enemies = true;
		if (a->u->type == unit_types::arbiter) dodge_enemies = true;
		if (a->u->type == unit_types::dark_archon) dodge_enemies = true;
		if (a->u->type == unit_types::corsair && target && !target->is_flying) dodge_enemies = true;
		if (target && dodge_enemies) {
			unit* net = get_best_score(enemies, [&](unit* e) {
				if (e->type == unit_types::bunker) {
					return units_distance(e, a->u) - 32 * 6;
				}
				weapon_stats* ew = a->u->is_flying ? e->stats->air_weapon : e->stats->ground_weapon;
				if (!ew) return std::numeric_limits<double>::infinity();
				return units_distance(e, a->u) - ew->max_range;
			}, std::numeric_limits<double>::infinity());
			if (net) {

				weapon_stats* netw = a->u->is_flying ? net->stats->air_weapon : net->stats->ground_weapon;
				double wr = net->type == unit_types::bunker ? 32 * 6 : netw->max_range;
				if (a->u->type == unit_types::defiler && wr >= 32 * 9) wr = 32 * 9;
				double max_d = 32 * 8;
				a_deque<xy> path = find_path(a->u->type, a->u->pos, [&](xy pos, xy npos) {
					double d = diag_distance(npos - a->u->pos);
					if (d >= max_d) return false;
					return true;
				}, [&](xy pos, xy npos) {
					return diag_distance(npos - target->pos);
				}, [&](xy pos) {
					double d = units_distance(pos, a->u->type, net->pos, net->type);
					if (d >= wr + 32 && d < wr + 32 * 3) {
						return true;
					}
					// 				if (units_distance(pos, a->u->type, target->pos, target->type) <= a->u->stats->sight_range) {
					// 					if (units_distance(pos, a->u->type, net->pos, net->type) >= wr + 32 * 2) {
					// 						return true;
					// 					}
					// 				}
					return false;
				});
				if (!path.empty()) {
					a->subaction = combat_unit::subaction_move;
					a->target_pos = path.back();
				} else {
					do_run(a, enemies);
					return;
				}
			}
		}
		if (!target && (a->u->type == unit_types::defiler || a->u->type == unit_types::queen)) {
			do_run(a, enemies);
			return;
		}
	}

// 	if (target && (a->u->type == unit_types::defiler || a->u->type == unit_types::queen)) {
// 		unit*net = get_best_score(enemies, [&](unit*e) {
// 			if (e->is_flying) return std::numeric_limits<double>::infinity();
// 			if (e->type == unit_types::bunker) {
// 				return units_distance(e, a->u) - 32 * 6;
// 			}
// 			weapon_stats*ew = a->u->is_flying ? e->stats->air_weapon : e->stats->ground_weapon;
// 			if (!ew) return std::numeric_limits<double>::infinity();
// 			return units_distance(e, a->u) - ew->max_range;
// 		}, std::numeric_limits<double>::infinity());
// 		if (net) {
// 			weapon_stats*ew = a->u->is_flying ? net->stats->air_weapon : net->stats->ground_weapon;
// 			if (ew && units_distance(a->u,net) < ew->max_range + 64) {
// 				do_run(a, enemies);
// 				return;
// 			}
// 		}
// 	}


// 	if (target && a->subaction == combat_unit::subaction_fight) {
// 		if (a->u->stats->ground_weapon && target->stats->ground_weapon && a->u->stats->ground_weapon->max_range >= target->stats->ground_weapon->max_range + 64) {
// 			if (a->u->stats->max_speed >= target->stats->max_speed) {
// 				if (enemies.size() < 15 && allies.size() < 15) {
// 					unit*nearest = get_best_score(allies, [&](unit*u) {
// 						return units_distance(u, target);
// 					});
// 					if (nearest == a->u) {
// 						if (a->u->stats->ground_weapon->cooldown > latency_frames && units_distance(a->u, target) < target->stats->ground_weapon->max_range + 64) {
// 							do_run(a, enemies);
// 							return;
// 						}
// 					}
// 				}
// 			}
// 		}
// 	}


// 	if (target && a->subaction == combat_unit::subaction_fight && allies.size() < 15) {
// 		if (target->stats->ground_weapon && !target->type->is_worker && my_base.test(grid::build_square_index(a->u->pos))) {
// 			unit*nearest_building = get_best_score(my_buildings, [&](unit*u) {
// 				return diag_distance(u->pos - target->pos);
// 			});
// 			if (nearest_building && nearest_building->type == unit_types::bunker) {
// 				if (units_distance(target, nearest_building) >= 32 * 7) {
// 					do_run(a, enemies);
// 					return;
// 				}
// 			}
// 		}
// 	}

// 	if (target && a->subaction == combat_unit::subaction_fight && allies.size() < 15 && no_aggressive_groups && current_used_total_supply < 60) {
// 		if (target->stats->ground_weapon && !target->type->is_worker) {
// 			if (diag_distance(target->pos - defence_choke.center) <= 32 * 25) {
// 				if (diag_distance(target->pos - defence_choke.outside) < diag_distance(target->pos - defence_choke.inside) && diag_distance(target->pos - defence_choke.center) > 32 * 3) {
// 					do_run(a, enemies);
// 					return;
// 				}
// 			}
// 		}
// 	}

	if (target && a->subaction == combat_unit::subaction_fight) {
		if (a->u->type == unit_types::scourge) {
			if (!target->is_flying) {
				do_run(a, enemies);
				return;
			}
			double ehp = target->shields + target->hp;
			ehp -= focus_fire[target];
			if (ehp <= 0) {
				a->subaction = combat_unit::subaction_move;
				a->target_pos = target->pos;
			}
		}
	}

	if (a->is_nearest || a->is_nearest_weak) {
		if (target && a->subaction == combat_unit::subaction_fight && a->u->type != unit_types::lurker) {
			double d = get_best_score_value(enemies, [&](unit*e) {
				return units_distance(a->u, e);
			});
			weapon_stats*my_weapon = target->is_flying ? a->u->stats->air_weapon : a->u->stats->ground_weapon;
			weapon_stats*e_weapon = a->u->is_flying ? target->stats->air_weapon : target->stats->ground_weapon;
			if (my_weapon && d < my_weapon->max_range - 64) {
				if (!e_weapon || my_weapon->max_range - 64 > e_weapon->max_range) {
					do_run(a, enemies);
					return;
				}
			}
		}
	}

	if (u->type == unit_types::zealot && target && target->type == unit_types::vulture && a->subaction == combat_unit::subaction_fight) {
		if (units_distance(u, target) > 32 * 4) {
			double nearest_building = get_best_score_value(my_buildings, [&](unit* n) {
				return sc_distance(u->pos - n->pos);
			});
			if (nearest_building > 32 * 8) {
				int n_vultures = 0;
				for (unit* e : enemies) {
					if (e->type == unit_types::vulture) ++n_vultures;
				}
				if (n_vultures >= enemies.size() / 2 + enemies.size() / 4) {
					do_run(a, enemies);
					return;
				}
			}
		}
	}
	
//	if (a->last_win_ratio < 128.0) {
//		if (!a->any_attacking && target && !target->is_flying && target->stats->ground_weapon && u->stats->ground_weapon && target->stats->ground_weapon->max_range < 32 && u->stats->ground_weapon->max_range < 32) {
//			if (units_distance(u, target) > u->stats->ground_weapon->max_range + 32 && (current_frame - a->stop_wait_frame < 30 || current_frame - a->stop_wait_frame >= 60)) {
//				if (a->is_nearest) {
//					if (u->game_order != BWAPI::Orders::HoldPosition && current_frame - a->last_stop >= 4) {
//						a->last_stop = current_frame;
//						u->game_unit->holdPosition();
//						u->controller->noorder_until = current_frame + 4;
//					}
//				} else if (a->nearest_ally && units_distance(u, target) <= units_distance(a->nearest_ally->u, target) + 16) {
//					double d = (target->pos - a->nearest_ally->u->pos).length();
//					a->subaction = combat_unit::subaction_move_directly;
//					xy relpos = target->pos - u->pos;
//					a->target_pos = u->pos + relpos / relpos.length() * d;
//				}
//			}
//		}
//	}

	if (target && a->subaction == combat_unit::subaction_fight && !u->burrowed && u->stats->max_speed > 1.0) {
		if (a->last_win_ratio >= 64.0) {
			u->controller->close_up_until = current_frame + 15;
		} else u->controller->close_up_until = 0;
	}

	if (target && a->u->type == unit_types::hydralisk && false) {
		if (enemy_siege_tank_area[grid::build_square_index(a->u->pos)]) {
			if (current_frame - a->last_spread_frame < 5 || current_frame - a->last_spread_frame > 15) {
				//u->controller->close_up_until = current_frame + 15;
				size_t too_close_n = 0;
				xy_typed<double> nearby_pos_sum;
				size_t nearby_pos_n = 0;
				for (unit* u : allies) {
					if (u == a->u) continue;
					if (u->is_flying) continue;
					double d = units_distance(a->u, u);
					if (d <= 32) ++too_close_n;
					if (d <= 64) {
						nearby_pos_sum += xy_typed<double>(u->pos.x, u->pos.y);
						++nearby_pos_n;
					}
				}
				if (too_close_n >= 1) {
					xy_typed<double> nearby_pos_avg = nearby_pos_sum / nearby_pos_n;
					auto rel_pos = xy_typed<double>(a->u->pos.x, a->u->pos.y) - nearby_pos_avg;
					auto move = rel_pos / rel_pos.length() * 64;
					xy move_to(a->u->pos.x + (int)move.x, a->u->pos.y + (int)move.y);
					if ((size_t)move_to.x < grid::map_width && (size_t)move_to.y < grid::map_height) {
						if (grid::get_build_square(move_to).entirely_walkable) {
							a->subaction = combat_unit::subaction_move_directly;
							a->target_pos = move_to;
							if (current_frame - a->last_spread_frame > 15) {
								a->last_spread_frame = current_frame;
							}
						}
					}
				}
			}
		}
	}
	//if (target && (a->u->type == unit_types::zealot || a->u->type == unit_types::dragoon || a->u->type == unit_types::zergling || a->u->type == unit_types::hydralisk)) {
	if (target && !a->u->is_flying) {
		if (a->subaction == combat_unit::subaction_fight) {
			if (enemy_siege_tank_area[grid::build_square_index(a->u->pos)] || enemy_siege_tank_area[grid::build_square_index(target->pos)]) {
				auto* w = target->is_flying ? a->u->stats->air_weapon : a->u->stats->ground_weapon;
				if (w && units_distance(a->u, target) > w->max_range + 32) {
					unit* move_target = get_best_score(enemies, [&](unit* e) {
						if (e->type != unit_types::siege_tank_siege_mode) return std::numeric_limits<double>::infinity();
						if (e->invincible || e->stasis_timer) return std::numeric_limits<double>::infinity();
						return diag_distance(e->pos - a->u->pos) + enemy_tank_move_count[e] * 128.0;
					}, std::numeric_limits<double>::infinity());
					if (move_target) {
						++enemy_tank_move_count[move_target];
						a->subaction = combat_unit::subaction_move;
						a->target_pos = move_target->pos;
					}
				}
			}
		}
		//if ((a->u->type == unit_types::zealot || a->u->type == unit_types::zergling) && !my_base.test(grid::build_square_index(a->u->pos))) {
		if (a->u->type == unit_types::zealot && !my_base.test(grid::build_square_index(a->u->pos))) {
			bool dragoons_engaged = false;
			int n_dragoons = 0;
			int n_zealots = 0;
			int n_zerglings = 0;
			for (auto* u : allies) {
				if (u->type == unit_types::zealot) ++n_zealots;
				if (u->type == unit_types::zergling) ++n_zerglings;
				if (u->type == unit_types::dragoon || u->type == unit_types::hydralisk) {
					++n_dragoons;
					if (diag_distance(a->u->pos - u->pos) <= 48 || enemy_siege_tank_area[grid::build_square_index(u->pos)]) {
						dragoons_engaged = true;
					}
				}
			}
			/*if (n_dragoons >= 1 + n_zealots / 2 + n_zerglings / 5) {
				if (!dragoons_engaged) {
					do_run(a, enemies);
					return;
				}
			}
			if (target->type == unit_types::vulture) {
				if (enemies.size() < 8) {
					size_t n_vultures = 0;
					for (unit* e : enemies) {
						if (e->type == unit_types::vulture) ++n_vultures;
					}
					if (n_vultures >= enemies.size() - enemies.size() / 4) {
						do_run(a, enemies);
						return;
					}
				}
			}*/
		}
	}

	if (target && a->subaction == combat_unit::subaction_fight) {
		if (u->spider_mine_count && players::my_player->has_upgrade(upgrade_types::spider_mines)) {

			if (target->type != unit_types::vulture) {
				double op_worth = 0.0;
				double my_worth = 0.0;

				bool any_targetable = false;
				xy pos = target->pos;
				for (unit* u : enemies) {
					if (u->type->is_building) continue;
					if (u->is_flying) continue;
					if (u->type->is_non_usable) continue;
					if (!any_targetable && !u->type->is_hovering) any_targetable = true;
					double r = diag_distance(pos - u->pos);
					if (r <= 32 * 4) {
						int n = 0;
						auto i = n_spider_mines_in_range_of.find(u);
						if (i != n_spider_mines_in_range_of.end()) n = i->second;
						if (n < 2) {
							op_worth += u->type->total_minerals_cost;
							op_worth += u->type->total_gas_cost;
							if (u->cloaked && u->stats->ground_weapon) op_worth += 1000.0;
						}
					}
				}

				if (any_targetable) {
					for (unit* u : allies) {
						if (u->type->is_building) continue;
						if (u->is_flying) continue;
						if (u->type->is_non_usable) continue;
						double r = diag_distance(pos - u->pos);
						if (r <= 32 * 4) {
							my_worth += u->type->total_minerals_cost;
							my_worth += u->type->total_gas_cost;
						}
					}

					log(log_level_info, "lay mine: %g <= %g * 2 = %d\n", my_worth, op_worth, my_worth <= op_worth * 2);

					if (my_worth <= op_worth * 2) {
						u->controller->lay_mines_until = current_frame + 15 * 5;
					} else u->controller->lay_mines_until = 0;
				}
			}

		}
	}

// 	if (target && a->subaction == combat_unit::subaction_fight) {
// 		if (a->u->game_unit->isUnderStorm()) {
// 			do_run(a, enemies);
// 			return;
// 		}
// 	}

	//log(log_level_info, "a->last_win_ratio is %g\n", a->last_win_ratio);
	//if (target && a->subaction == combat_unit::subaction_fight && !a->u->is_flying && a->last_win_ratio < 8.0 && u->type != unit_types::siege_tank_siege_mode) {
	if (target && a->subaction == combat_unit::subaction_fight && !a->u->is_flying && a->last_win_ratio < 8.0) {
		int n_enemy_tanks = enemy_siege_tank_area[grid::build_square_index(target->pos)];
		if (n_enemy_tanks >= 2 && my_tanks >= n_enemy_tanks / 2 && a->num_in_enemy_tank_range < allies.size() / 2) {
			weapon_stats* my_weapon = target->is_flying ? a->u->stats->air_weapon : a->u->stats->ground_weapon;
			if (!my_weapon || (units_distance(u, target) > my_weapon->max_range + 32 || u->type == unit_types::siege_tank_tank_mode)) {

				xy best_pos;
				double best_distance = std::numeric_limits<double>::infinity();
				a_deque<xy> path = find_path(u->type, u->pos, [&](xy pos, xy npos) {
					size_t index = grid::build_square_index(npos);
					//if (run_spot_taken.test(index)) return false;
					int num = enemy_siege_tank_area[index];
					if (num >= 1) return false;
					if (num == 0 && !run_spot_taken.test(index)) {
						double d = diag_distance(npos - target->pos);
						if (d < best_distance) {
							best_distance = d;
							best_pos = npos;
						}
					}
					return diag_distance(npos - u->pos) <= 32 * 8;
				}, [&](xy pos, xy npos) {
					double cost = enemy_siege_tank_area[grid::build_square_index(npos)];
					return cost;
				}, [&](xy pos) {
					return my_weapon ? units_distance(u, target) <= my_weapon->max_range : false;
				});

				log(log_level_info, " waa tanks, path.size() %d\n", path.size());
				
				if (!path.empty()) {
					a->subaction = combat_unit::subaction_move;
					a->target_pos = path.empty() ? a->goal_pos : path.back();
				}
				
				if (best_pos != xy()) {
					a->subaction = combat_unit::subaction_move;
					a->target_pos = best_pos;
					if (diag_distance(a->u->pos - best_pos) <= 32) {
						a->target_pos = a->u->pos;
						if (u->type == unit_types::siege_tank_tank_mode && has_siege_mode) {
							if (current_frame >= a->u->controller->last_siege + 12) {
								if (current_frame >= a->u->controller->move_away_until && a->u->game_unit->siege()) {
									a->u->controller->noorder_until = current_frame + 30;
									a->u->controller->last_siege = current_frame;
									a->siege_up_close = false;
								}
							}
						}
					}
				}
				
				if (u->type == unit_types::siege_tank_siege_mode && enemy_siege_tank_area[grid::build_square_index(a->u->pos)]) {
					a->subaction = combat_unit::subaction_fight;
				}

//				if (path.size() <= 1) {
//					if (u->type == unit_types::siege_tank_tank_mode && best_pos != xy() && has_siege_mode) {
//					//if (best_pos != xy() && enemy_siege_tank_area[grid::build_square_index(u->pos)] == 0) {
//						run_spot_taken.set(grid::build_square_index(best_pos));
//						a->target_pos = best_pos;

//						if (units_distance(u, target) <= 32 * 12 && current_frame - target->last_seen < 15 * 8) {
//							if (current_frame >= a->u->controller->move_away_until && a->u->game_unit->siege()) {
//								a->u->controller->noorder_until = current_frame + 30;
//								a->u->controller->last_siege = current_frame;
//								a->siege_up_close = false;
//							}
//						}

//					} else {
//						do_run(a, enemies);
//						return;
//					}
//				}
			}

		}
	}

	if (target && a->subaction == combat_unit::subaction_fight) {
		weapon_stats*my_weapon = target->is_flying ? a->u->stats->air_weapon : a->u->stats->ground_weapon;
		weapon_stats*e_weapon = a->u->is_flying ? target->stats->air_weapon : target->stats->ground_weapon;
		double max_range = 1000.0;
		double min_range = 0.0;
		if (e_weapon && my_weapon) {
			//if (my_weapon->max_range < e_weapon->max_range) max_range = my_weapon->max_range / 2;
			if (e_weapon->min_range) max_range = e_weapon->min_range;
			//if (my_weapon->max_range >= e_weapon->max_range + 32) min_range = e_weapon->max_range + 32 * 2;
		}
		if (a->u->spider_mine_count && !target->is_flying && !target->type->is_hovering && !target->type->is_building && players::my_player->has_upgrade(upgrade_types::spider_mines)) {
			if (target->hp >= 40) {
				//spider_mine_layers.push_back(a);
				max_range = 0;
			}
		}
		double damage = 0.0;
		if (u->type == unit_types::carrier) damage = 6 * u->game_unit->getInterceptorCount();
		if (my_weapon) {
			damage = my_weapon->damage;
			if (target->shields <= 0) damage *= combat_eval::get_damage_type_modifier(my_weapon->damage_type, target->stats->type->size);
			damage -= target->stats->armor;
			if (damage <= 0) damage = 1.0;
			damage *= my_weapon == a->u->stats->ground_weapon ? a->u->stats->ground_weapon_hits : a->u->stats->air_weapon_hits;
		} else if (e_weapon && a->last_win_ratio < 32.0 && (u->stats->ground_weapon || u->stats->air_weapon)) {
			do_run(a, enemies);
			return;
		}
		double d = units_distance(target, a->u);
		if (d > max_range && move_close_if_unreachable(a, target)) {
			//if (!move_close_if_unreachable(a, target) && a->u->weapon_cooldown > latency_frames) {
			//	a->subaction = combat_unit::subaction_move_directly;
			//	a->target_pos = target->pos;
			//}
		} else {
			if (d < min_range) {
				double wr = my_weapon ? my_weapon->max_range : 0.0;
				if (a->u->weapon_cooldown <= frames_to_reach(a->u, d - wr) + latency_frames) {
					do_run(a, enemies);
				}
			} else {
				if (has_slow_projectile && a->u->weapon_cooldown <= latency_frames + 8) prepared_damage[target] += damage;
				if (a->u->type != unit_types::lurker) {
					double range = my_weapon ? my_weapon->max_range : 0.0;
					if (a->u->type == unit_types::scourge) range = 64;
					if (my_weapon && d <= range && a->u->weapon_cooldown <= latency_frames && current_frame >= a->registered_focus_fire_until) {
						focus_fire[target] += damage;
						auto i = registered_focus_fire.find(a);
						if (i != registered_focus_fire.end()) {
							if (std::abs(focus_fire[std::get<0>(i->second)] -= std::get<1>(i->second)) < 1) {
								focus_fire.erase(std::get<0>(i->second));
							}
						}
						registered_focus_fire[a] = std::make_tuple(target, damage, current_frame + 8);
					}
				}
			}
		}
	}

	//if (target && a->subaction == combat_unit::subaction_fight && allies.size() < 20 && a->last_win_ratio < 2.0) {
// 	if (target && a->subaction == combat_unit::subaction_fight && allies.size() < 20) {
// 		unit*bunker = get_best_score(my_completed_units_of_type[unit_types::bunker], [&](unit*u) {
// 			if (u->loaded_units.empty()) return std::numeric_limits<double>::infinity();
// 			double d = diag_distance(u->pos - target->pos);
// 			if (d >= 32 * 10) return std::numeric_limits<double>::infinity();
// 			return d;
// 		}, std::numeric_limits<double>::infinity());
// 		if (bunker) {
// 			double bunker_distance = diag_distance(bunker->pos - target->pos);
// 			double my_distance = diag_distance(a->u->pos - target->pos);
// 			if (bunker_distance < my_distance*1.5) {
// 				if (my_distance < bunker_distance + 64) do_run(a, enemies);
// 			}
// 		}
// 	}

	if (target && a->subaction == combat_unit::subaction_fight && a->last_win_ratio < 16.0 && current_used_total_supply < 80 && allies.size() < 15) {
		unit* bunker = get_best_score(my_completed_units_of_type[unit_types::bunker], [&](unit* u) {
			if (u->loaded_units.empty()) return std::numeric_limits<double>::infinity();
			double d = diag_distance(u->pos - a->u->pos);
			if (d >= 32 * 14) return std::numeric_limits<double>::infinity();
			return d;
		}, std::numeric_limits<double>::infinity());
		if (bunker) {
			target = get_best_score(enemies, score);
			if (target) {
				double bunker_distance = units_distance(bunker, target) - 8.0;
				//printf("bunker_distance %g, target->stats->ground_weapon->max_range %g\n", bunker_distance, target->stats->ground_weapon->max_range);
// 				if (u->type == unit_types::siege_tank_tank_mode && units_distance(u, target) <= 32 * 11) {
// 					for (unit* u : my_completed_units_of_type[unit_types::machine_shop]) {
// 						if (u->game_unit->isResearching()) {
// 							auto upg = upgrades::get_upgrade_type(u->game_unit->getTech());
// 							if (upg == upgrade_types::siege_mode) {
// 								//if (u->remaining_research_time <= 15 * 30) {
// 								if (true) {
// 									if (!entire_threat_area.test(grid::build_square_index(u->pos))) {
// 										a->subaction = combat_unit::subaction_move;
// 										a->target_pos = u->pos;
// 									} else {
// 										//do_run(a, enemies);
// 										a->subaction = combat_unit::subaction_move;
// 										a_deque<xy> path = find_path(u->type, u->pos, [&](xy pos, xy npos) {
// 											return diag_distance(npos - u->pos) <= 32 * 20;
// 										}, [&](xy pos, xy npos) {
// 											double cost = 0.0;
// 											for (unit*e : enemies) {
// 												weapon_stats*w = a->u->is_flying ? e->stats->air_weapon : e->stats->ground_weapon;
// 												if (e->type == unit_types::bunker) w = units::get_unit_stats(unit_types::marine, e->owner)->ground_weapon;
// 												if (!w) continue;
// 												double d = diag_distance(pos - e->pos);
// 												if (d <= w->max_range*1.5) {
// 													cost += w->max_range*1.5 - d;
// 												}
// 											}
// 											return cost + diag_distance(a->goal_pos - a->u->pos);
// 										}, [&](xy pos) {
// 											size_t index = grid::build_square_index(pos);
// 											return entire_threat_area_edge.test(index) && (u->is_flying || !run_spot_taken.test(index));
// 										});
// 
// 										a->target_pos = path.empty() ? a->goal_pos : path.back();
// 									}
// 									return;
// 								}
// 							}
// 						}
// 					}
// 				}
				if (target->stats->ground_weapon && bunker_distance > target->stats->ground_weapon->max_range) {
					weapon_stats* w = target->is_flying ? u->stats->air_weapon : u->stats->ground_weapon;
					double w_range = w ? w->max_range : 0.0;
					if (u->type == unit_types::siege_tank_tank_mode) {
						if (has_siege_mode) {
							w_range = 32 * 12;
						}
					}
					if (!w || bunker_distance > w_range) {
						if (!entire_threat_area.test(grid::build_square_index(u->pos))) {
							a->subaction = combat_unit::subaction_move;
							a->target_pos = u->pos;
						} else {
							do_run(a, enemies);
						}
						return;
					}
// 					double target_distance = units_distance(u, target);
// 					if (w && target_distance < w_range - 32 * 2 && bunker_distance < target_distance && bunker_distance > 32 * 2) {
// 						if (u->type != unit_types::siege_tank_siege_mode) {
// 							if (entire_threat_area.test(grid::build_square_index(u->pos))) {
// 								do_run(a, enemies);
// 								return;
// 							}
// 						}
// 					}
					if (u->type == unit_types::siege_tank_tank_mode) {
						a->siege_up_close = true;
					}
				}
			}
		}
	}

	if (u->type == unit_types::vulture && target && a->subaction == combat_unit::subaction_fight && !target->building) {

		a_deque<xy> path = find_path(u->type, u->pos, [&](xy pos, xy npos) {
			return diag_distance(npos - u->pos) <= 32 * 20;
		}, [&](xy pos, xy npos) {
			double cost = 0.0;
			for (unit*e : enemies) {
				weapon_stats*w = a->u->is_flying ? e->stats->air_weapon : e->stats->ground_weapon;
				if (e->type == unit_types::bunker) w = units::get_unit_stats(unit_types::marine, e->owner)->ground_weapon;
				if (!w) continue;
				double d = diag_distance(pos - e->pos);
				if (d <= w->max_range*1.5) {
					cost += w->max_range*1.5 - d;
				}
			}
			return cost + diag_distance(a->goal_pos - a->u->pos);
		}, [&](xy pos) {
			size_t index = grid::build_square_index(pos);
			return entire_threat_area_edge.test(index) && (u->is_flying || !run_spot_taken.test(index));
		});

		if (!path.empty()) {
			a->subaction = combat_unit::subaction_kite;
			a->kite_attack_target = a->target;
			a->target_pos = path.back();
		}
	}

	if (target && (a->subaction == combat_unit::subaction_fight || a->subaction == combat_unit::subaction_kite) && target->type == unit_types::lurker && target->burrowed) {
		if (u->type == unit_types::siege_tank_tank_mode || u->type == unit_types::siege_tank_siege_mode) {
			if (units_distance(u, target) <= 32 * 7) {
				do_run(a, enemies);
				return;
			}
		}
		if (u->type != unit_types::siege_tank_siege_mode || current_frame - last_attack_scan > latency_frames) {
			if (!target->detected) {
				do_run(a, enemies);
				return;
			}
		}
	}
	
	if (target && (a->subaction == combat_unit::subaction_fight || a->subaction == combat_unit::subaction_kite) && current_frame < 15 * 60 * 18) {
		if (a->u->type == unit_types::goliath || a->u->type == unit_types::siege_tank_tank_mode || a->u->type == unit_types::hydralisk || a->u->type == unit_types::dragoon) {
			if (mine_threat_area[grid::build_square_index(a->u->pos)] >= 128) {
				auto* w = target->is_flying ? a->u->stats->air_weapon : a->u->stats->ground_weapon;
				if (!w || units_distance(a->u, target) > w->max_range) {
					bool any_enemies_in_range = false;
					for (unit* e : enemies) {
						auto* ew = a->u->is_flying ? e->stats->air_weapon : e->stats->ground_weapon;
						if (ew && units_distance(e, a->u) <= ew->max_range) {
							any_enemies_in_range = true;
							break;
						}
					}
					if (!any_enemies_in_range) {
						int n_nearby_allies = 0;
						unit* nearest_vulture = nullptr;
						double nearest_vulture_distance = std::numeric_limits<double>::infinity();
						for (unit* u : allies) {
							double d = units_distance(a->u, u);
							if (d <= 32 * 3) ++n_nearby_allies;
							if (u->type == unit_types::vulture || u->type == unit_types::hydralisk || u->type == unit_types::dragoon) {
								if (u != a->u && d < nearest_vulture_distance) {
									nearest_vulture_distance = d;
									nearest_vulture = u;
								}
							}
						}
						if (n_nearby_allies < 4 && nearest_vulture_distance > 32 * 2) {
							if (nearest_vulture_distance <= 32 * 20) {
								a->subaction = combat_unit::subaction_move;
								a->target_pos = nearest_vulture->pos;
							}
						}
					}
				}
			}
		}
		
	}

// 	if (target && a->subaction == combat_unit::subaction_fight && a->u->type != unit_types::siege_tank_tank_mode && a->u->type != unit_types::siege_tank_siege_mode) {
// 		int static_defence_count = 0;
// 		for (unit*e : enemies) {
// 			if (units_distance(a->u, e) > 32 * 10) continue;
// 			if (a->u->is_flying && e->type == unit_types::spore_colony) ++static_defence_count;
// 			if (!a->u->is_flying && (e->type == unit_types::photon_cannon || e->type == unit_types::sunken_colony)) ++static_defence_count;
// 		}
// 		if (static_defence_count >= 4) {
// 			double wr = 32 * 2;
// 			weapon_stats*w = target->is_flying ? a->u->stats->air_weapon : a->u->stats->ground_weapon;
// 			if (w) wr = w->max_range;
// 			if (units_distance(a->u, target) > wr) {
// 				do_run(a, enemies);
// 				return;
// 			}
// 		}
// 	}
	
	
//	if (target && (a->subaction == combat_unit::subaction_fight || a->subaction == combat_unit::subaction_kite)) {
//		if (no_aggressive_groups && current_frame < 15 * 60 * 15 && !a->u->is_flying && a->last_win_ratio < 32.0) {
			
//			auto* w = target->is_flying ? a->u->stats->air_weapon : a->u->stats->ground_weapon;
//			if (!w || units_distance(a->u, target) > w->max_range) {
				
//				auto any_attacking = [&]() {
//					for (unit* e : enemies) {
//						if (current_frame - e->last_attacked <= 90 && e->last_attack_target && e->last_attack_target->owner == players::my_player) {
//							return true;
//						}
//					}
//					return false;
//				};
				
//				auto is_near_base = [&](xy pos, double range, bool allow_crossing_nonwalkable) {
//					bool rv = false;
//					find_nearby_entirely_walkable_tiles(pos, [&](xy pos) {
//						if (my_base.test(grid::build_square_index(pos))) rv = true;
//						return !rv;
//					}, range, allow_crossing_nonwalkable);
//					return rv;
//				};
				
//				if (my_base.test(grid::build_square_index(a->u->pos)) || is_near_base(a->u->pos, 32 * 8, false)) {
					
//					double r = 32 * 4;
//					if (target->stats->ground_weapon) r = target->stats->ground_weapon->max_range;
					
//					//if (!is_near_base(target->pos, r, true) && !any_attacking()) {
//					if (!is_near_base(target->pos, r, true)) {
						
//						xy move_to;
//						bool found = false;
//						find_nearby_entirely_walkable_tiles(a->u->pos, [&](xy pos) {
//							if (my_base.test(grid::build_square_index(pos))) {
//								found = true;
//								move_to = pos;
//							}
//							return !found;
//						}, 32 * 8);
						
//						if (move_to != xy()) {
							
//							a->subaction = combat_unit::subaction_move;
//							a->target_pos = move_to;
							
//							game->drawLineMap(a->u->pos.x, a->u->pos.y, move_to.x, move_to.y, BWAPI::Colors::Blue);
//						}
						
//					}
					
//				}
				
//			}
			
//		}
//	}
	
	
	if (target && (a->subaction == combat_unit::subaction_fight || a->subaction == combat_unit::subaction_kite)) {
		
		bool move_to_leader = false;
		
		if (!my_base.test(grid::build_square_index(a->u->pos))) {
			//if (u->type == unit_types::marine && a->last_win_ratio < 256.0) move_to_leader = true;
			
//			if (!a->u->is_flying && target->type != unit_types::spider_mine && mine_threat_area[grid::build_square_index(a->u->pos)] >= 64) {
//				double damage = 0.0;
//				for (unit* u : allies) {
//					if (diag_distance(a->u->pos - u->pos) > 64) continue;
//					auto* w = u->stats->ground_weapon;
//					if (w) damage += w->damage * get_damage_type_modifier(w->damage_type, unit_type::size_small) * w->cooldown / 30;
//				}
//				if (damage < 50.0) {
//					move_to_leader = true;
//				}
//			}
		}
		
		if (move_to_leader) {
			
			unit* leader = get_best_score(allies, [&](unit* u) {
				return (32.0 * 8 + diag_distance(a->u->pos - target->pos)) / (u->type->total_minerals_cost + u->type->total_gas_cost);
			});
			double leader_d = units_distance(a->u, leader);
			if (leader && leader_d <= 32 * 18 && leader_d > 48) {
				double wr = 32.0 * 6;
				auto* w = target->is_flying ? leader->stats->air_weapon : leader->stats->ground_weapon;
				if (w) wr = std::max(wr, w->max_range);
				auto* ew = leader->is_flying ? target->stats->air_weapon : target->stats->ground_weapon;
				if (ew) wr = std::max(wr, ew->max_range);
				if (wr < 32 * 12) wr += 64;
				if (units_distance(leader, target) > wr) {
					
					xy best_pos;
					double best_pos_distance = std::numeric_limits<double>::infinity();
					double initial_leader_distance = diag_distance(u->pos - leader->pos);
					a_deque<xy> path = find_path(u->type, u->pos, [&](xy pos, xy npos) {
						if (diag_distance(npos - leader->pos) > initial_leader_distance) return false;
						size_t index = grid::build_square_index(npos);
						return diag_distance(npos - u->pos) <= 32 * 12 && !run_spot_taken.test(index);
					}, [&](xy pos, xy npos) {
						double cost = 0.0;
						for (unit*e : enemies) {
							weapon_stats*w = a->u->is_flying ? e->stats->air_weapon : e->stats->ground_weapon;
							if (e->type == unit_types::bunker) w = units::get_unit_stats(unit_types::marine, e->owner)->ground_weapon;
							if (!w) continue;
							double d = diag_distance(pos - e->pos);
							if (d <= w->max_range*1.5) {
								cost += w->max_range*1.5 - d;
							}
						}
						return cost + diag_distance(a->goal_pos - a->u->pos);
					}, [&](xy pos) {
						size_t index = grid::build_square_index(pos);
						if (!run_spot_taken.test(index)) {
							double d = diag_distance(pos - leader->pos);
							if (d	< best_pos_distance) {
								best_pos_distance = d;
								best_pos = pos;
							}
						}
						return false;
					});
					
					if (best_pos != xy()) {
						auto* tw = u->is_flying ? target->stats->air_weapon : target->stats->ground_weapon;
						if (!tw || units_distance(best_pos, u->type, target->pos, target->type) > tw->max_range + 32) {
							a->subaction = combat_unit::subaction_move;
							a->target_pos = best_pos;
							
							run_spot_taken.set(grid::build_square_index(best_pos));
							
							game->drawLineMap(u->pos.x, u->pos.y, best_pos.x, best_pos.y, BWAPI::Colors::Blue);
						}
					}
					
				}
				
			}
		}
	}
	
	if (a->u->type == unit_types::vulture && (a->subaction == combat_unit::subaction_fight || a->subaction == combat_unit::subaction_kite) && target) {
		//if (target->type == unit_types::siege_tank_tank_mode || target->type == unit_types::siege_tank_siege_mode) {
		if (!target->stats->ground_weapon || target->stats->ground_weapon->max_range >= 32 * 5) {
			a->u->controller->close_up_until = current_frame + 15;
		}
	}
	
//	if ((a->subaction == combat_unit::subaction_fight || a->subaction == combat_unit::subaction_kite) && target) {
//		if (!u->burrowed && u->type != unit_types::siege_tank_siege_mode && units_distance(a->u, target) > 16) {
//			xy relpos = a->u->pos - target->pos;
//			double d = sc_distance(relpos);
//			xy npos = a->u->pos + xy(relpos.x / d * 32, relpos.y / d * 32);
//			for (auto* u : allies) {
//				if (diag_distance(u->pos - npos) <= 32 && sc_distance(u->pos - target->pos) >= d + 16) {
//					a->u->controller->close_up_until = current_frame + 15;
//					break;
//				}
//			}
//		}
//	}
	
//	if (target && (a->subaction == combat_unit::subaction_fight || a->subaction == combat_unit::subaction_kite) && !square_pathing::unit_can_reach(u, u->pos, target->pos)) {
//		log(log_level_test, "waa target is unreachable\n");
		
//		weapon_stats* w = target->is_flying ? u->stats->air_weapon : u->stats->ground_weapon;
		
//		if (w) {
//			if (units_distance(u, target) <= w->max_range && target->visible) {
//				log(log_level_test, "waa nvm in range\n");
//			} else {
//				double best_distance = std::numeric_limits<double>::infinity();
//				xy best_pos;
//				double best_building_distance = std::numeric_limits<double>::infinity();
//				unit* best_building = nullptr;
//				int target_height = grid::get_build_square(target->pos).height;
//				find_nearby_entirely_walkable_tiles(target->pos, [&](xy pos) {
//					auto& bs = grid::get_build_square(pos);
//					if (bs.building) {
//						if (bs.building->owner != players::my_player) {
//							double d = diag_distance(pos - u->pos);
//							if (d < best_building_distance) {
//								best_building_distance = d;
//								best_building = bs.building;
//							}
//						}
//						return true;
//					}
//					if (bs.entirely_walkable && bs.height >= target_height) {
//						xy npos = square_pathing::get_pos_in_square(pos, u->type);
//						if (npos != xy() && square_pathing::unit_can_reach(u, u->pos, npos)) {
//							double d = diag_distance(npos - u->pos);
//							if (d < best_distance) {
//								best_distance = d;
//								best_pos = npos;
//							}
//						}
//					}
//					return true;
//				}, w->max_range, true);
//				if (best_pos == xy()) {
//					log(log_level_test, "waa target is REALLY unreachable\n");
//					if (a->last_win_ratio >= 64.0) {
//						log(log_level_test, "waa but we can just kill the building(s) :D\n");
//						a->target = best_building;
//					} else {
//						log(log_level_test, "waa so run\n");
//						do_run(a, enemies);
//					}
//				} else {
//					xy pos = square_pathing::get_pos_in_square(best_pos + xy(16, 16), u->type);
//					if (pos == xy()) pos = square_pathing::get_pos_in_square(best_pos, u->type);
//					if (pos == xy()) pos = best_pos;
//					log(log_level_test, "waa target is kinda maybe reachable?\n");
//					a->subaction = combat_unit::subaction_move;
//					a->target_pos = best_pos;
					
//					game->drawLineMap(u->pos.x, u->pos.y, best_pos.x, best_pos.y, BWAPI::Colors::White);
//				}
//			}
//		}
		
//	}

}

bool worker_is_safe(combat_unit*a, const a_vector<unit*>&enemies) {
	unit*res = nullptr;
	if (a->u->controller->action == unit_controller::action_gather) res = a->u->controller->target;
	else {
		res = get_best_score(resource_units, [&](unit*u) {
			return units_pathing_distance(a->u, u);
		});
	}
	if (res) {
		double gd = units_pathing_distance(a->u, res);
		unit*ne = get_best_score(enemies, [&](unit*e) {
			weapon_stats*w = a->u->is_flying ? e->stats->air_weapon : e->stats->ground_weapon;
			if (!w) return std::numeric_limits<double>::infinity();
			return diag_distance(e->pos - a->u->pos);
		}, std::numeric_limits<double>::infinity());
		if (ne) {
			weapon_stats*w = a->u->is_flying ? ne->stats->air_weapon : ne->stats->ground_weapon;
			double ned = units_pathing_distance(a->u, ne);
			if (ned <= 32 * 15 && ned < gd) return false;
			if (ned <= w->max_range + 64) return false;
		}
	}
	return true;
}

void prepare_run() {
	run_spot_taken.resize(grid::build_grid_width*grid::build_grid_height);
	run_spot_taken.reset();
}

std::vector<int> run_score;

void do_run(combat_unit*a, const a_vector<unit*>&enemies) {
	a->defend_spot = xy();
	a->subaction = combat_unit::subaction_move;
	a->target_pos = xy(grid::map_width / 2, grid::map_height / 2);

	if (a->u->is_loaded) return;

	if (a->u->type == unit_types::marine || a->u->type == unit_types::firebat) {
		unit*target = get_best_score(enemies, [&](unit*e) {
			return diag_distance(a->u->pos - e->pos);
		});
		unit*bunker = get_best_score(my_completed_units_of_type[unit_types::bunker], [&](unit*u) {
			if (space_left[u] < a->u->type->space_required) return std::numeric_limits<double>::infinity();
			double d = diag_distance(u->pos - a->u->pos);
			if (d >= 32 * 40) return std::numeric_limits<double>::infinity();
			return diag_distance(target->pos - u->pos);
		}, std::numeric_limits<double>::infinity());
		log("%s: bunker is %p\n", a->u->type->name, bunker);
		if (a->u->type == unit_types::firebat) {
			if (target->stats->ground_weapon && target->stats->ground_weapon->max_range > 32 * 3) bunker = nullptr;
		}
		if (bunker && units_distance(bunker, target) > 32 * 7) {
			unit*nearest_building = get_best_score(my_buildings, [&](unit*u) {
				return diag_distance(u->pos - target->pos);
			});
			if (nearest_building != bunker) bunker = nullptr;
		}
		if (bunker) {
			log("%s: run to bunker %p!\n", a->u->type->name, bunker);
			if (units_distance(a->u, bunker) <= 32 * 4) space_left[bunker] -= a->u->type->space_required;
			if (current_frame >= a->u->controller->noorder_until) {
				if ((a->u->type == unit_types::marine || a->u->type == unit_types::firebat) && a->u->owner->has_upgrade(upgrade_types::stim_packs)) {
					if (a->u->stim_timer <= latency_frames && current_frame >= a->u->controller->nospecial_until) {
						a->u->game_unit->useTech(upgrade_types::stim_packs->game_tech_type);
						a->u->controller->noorder_until = current_frame + 2;
						a->u->controller->nospecial_until = current_frame + latency_frames + 4;
					}
				}
				if (a->u->game_unit->rightClick(bunker->game_unit)) {
					a->u->controller->noorder_until = current_frame + 30;
				}
			}
			return;
		}
	}
	
	if (a->u->type == unit_types::scourge) {
		unit* target = get_best_score(enemies, [&](unit* e) {
			if (e->is_flying) return std::numeric_limits<double>::infinity();
			auto* w = e->stats->air_weapon;
			if (!w) return std::numeric_limits<double>::infinity();
			double d = units_distance(a->u, e);
			if (d > w->max_range) return std::numeric_limits<double>::infinity();
			return d;
		}, std::numeric_limits<double>::infinity());
		if (target) {
			a->subaction = combat_unit::subaction_fight;
			a->target = target;
			return;
		}
	}

	//log("%s : run!\n", a->u->type->name);
	
	if (a->u->type != unit_types::lurker && a->u->type != unit_types::corsair && entire_threat_area.test(grid::build_square_index(a->u->pos))) {
		unit*ne = get_best_score(enemies, [&](unit*e) {
			weapon_stats*w = e->is_flying ? a->u->stats->air_weapon : a->u->stats->ground_weapon;
			if (!w) return std::numeric_limits<double>::infinity();
			if (e->cloaked && !e->detected) return std::numeric_limits<double>::infinity();
			return units_distance(e, a->u);
		}, std::numeric_limits<double>::infinity());
		unit*net = get_best_score(enemies, [&](unit*e) {
			weapon_stats*ew = a->u->is_flying ? e->stats->air_weapon : e->stats->ground_weapon;
			if (e->type == unit_types::bunker) return units_distance(e, a->u) - 32 * 6;
			if (!ew) return std::numeric_limits<double>::infinity();
			if (e->type == unit_types::lurker && !e->burrowed) return std::numeric_limits<double>::infinity();
			return units_distance(e, a->u) - ew->max_range;
		}, std::numeric_limits<double>::infinity());
		if (ne && ne->type != unit_types::bunker && (!ne->cloaked || ne->detected) && ne->type != unit_types::interceptor) {
			weapon_stats*w = ne->is_flying ? a->u->stats->air_weapon : a->u->stats->ground_weapon;
			weapon_stats*ew = net ? a->u->is_flying ? net->stats->air_weapon : net->stats->ground_weapon : nullptr;
			double d = units_distance(ne, a->u);
			double wr = w->max_range;
			double net_d = net ? units_distance(net, a->u) : 1000.0;
			double net_wr = ew ? ew->max_range : 0.0;
			if (net && net->type == unit_types::bunker) net_wr = 32 * 6;
			double margin = 32 * 2;
			if (a->u->type == unit_types::vulture) margin = 0;
			if (a->u->type == unit_types::siege_tank_tank_mode) margin = 0;
			if (a->u->type == unit_types::firebat) margin = 0;
			if (a->u->type == unit_types::ghost) margin = 0;
			if (net && !square_pathing::unit_can_reach(a->u, a->u->pos, net->pos, square_pathing::pathing_map_index::include_liftable_wall)) margin = 0.0;
			//bool too_close = net_d <= wr - 64.0;
			bool too_close = net_d <= net_wr + 32;
			//if (a->u->type == unit_types::goliath) too_close = false;
			//if (current_used_total_supply >= 80) too_close = false;
			//if (net && net->stats->max_speed > a->u->stats->max_speed) too_close = false;
			if (d - wr < net_d - net_wr - margin && !too_close) {
			//if (d - wr < net_d - net_wr - margin && (!too_close || enemies.size() >= 8)) {
				if (move_close_if_unreachable(a, ne)) return;
// 				if (a->u->weapon_cooldown <= frames_to_reach(a->u, d - wr) + latency_frames || (!net || net->building)) {
// 					a->subaction = combat_unit::subaction_fight;
// 					a->target = ne;
// 				}
				if (!net || net->building) {
					a->subaction = combat_unit::subaction_fight;
					a->target = ne;
				} else {
					bool kite = true;
					if (a->u->type == unit_types::goliath) kite = false;
					if (a->u->type == unit_types::hydralisk) kite = a->last_win_ratio > 0.5;
					//if (net->type == unit_types::interceptor) kite = false;
					if (net->type == unit_types::scourge && net_d <= 32 * 3) kite = false;
					if (kite) {
						a->subaction = combat_unit::subaction_kite;
						a->target = ne;
						unit*attack_target = get_best_score(enemies, [&](unit*e) {
							weapon_stats*w = e->is_flying ? a->u->stats->air_weapon : a->u->stats->ground_weapon;
							if (!w) return std::numeric_limits<double>::infinity();
							if (e->cloaked && !e->detected) return std::numeric_limits<double>::infinity();
							if (units_distance(e, a->u) > w->max_range - 16) return std::numeric_limits<double>::infinity();
							return e->shields + e->hp;
						}, std::numeric_limits<double>::infinity());
						if (!attack_target) attack_target = ne;
						a->kite_attack_target = attack_target;
					}
				}
			}
		}
	}

	if (a->u->type->is_worker) {
		if (worker_is_safe(a, enemies)) {
			a->subaction = combat_unit::subaction_idle;
			if (a->u->controller->action != unit_controller::action_gather && a->u->controller->action != unit_controller::action_build) {
				a->u->controller->action = unit_controller::action_idle;
			}
			return;
		}
// 		unit*res = nullptr;
// 		double best_distance;
// 		for (auto&s : resource_spots::spots) {
// 			auto&bs = grid::get_build_square(s.cc_build_pos);
// 			if (!bs.building || bs.building->owner != players::my_player) continue;
// 			for (auto&r : s.resources) {
// 				if (entire_threat_area.test(grid::build_square_index(r.u->pos))) continue;
// 				double d = unit_pathing_distance(a->u, r.u->pos);
// 				if (!res || d < best_distance) {
// 					res = r.u;
// 					best_distance = d;
// 				}
// 			}
// 		}
// 		if (res) {
// 			a->subaction = combat_unit::subaction_idle;
// 			a->u->controller->action = unit_controller::action_gather;
// 			a->u->controller->target = res;
// 			return;
// 		}
	}

	unit*u = a->u;

	auto can_burrow = [&]() {
		return u->type==unit_types::drone || u->type==unit_types::zergling || u->type==unit_types::hydralisk || u->type==unit_types::defiler;
	};

// 	if (a->subaction == combat_unit::subaction_move && players::my_player->has_upgrade(upgrade_types::burrow) && can_burrow()) {
// 		bool detected = false;
// 		for (unit*e : enemy_detector_units) {
// 			if (units_distance(e, u) <= e->stats->sight_range) {
// 				detected = true;
// 				break;
// 			}
// 		}
// 		if (!detected) {
// 			a->subaction = combat_unit::subaction_idle;
// 			u->controller->action = unit_controller::action_idle;
// 			if (current_frame - u->controller->last_burrow >= 15) {
// 				u->controller->last_burrow = current_frame;
// 				u->game_unit->burrow();
// 			}
// 		}
// 	}

//	if (a->subaction == combat_unit::subaction_move && u->type == unit_types::lurker) {
//		bool detected = false;
//		for (unit*e : enemy_detector_units) {
//			if (units_distance(e, u) <= e->stats->sight_range) {
//				detected = true;
//				break;
//			}
//		}
//		if (!detected || existing_dark_swarm.test(grid::build_square_index(u->pos))) {
//			a->subaction = combat_unit::subaction_idle;
//			u->controller->action = unit_controller::action_idle;
//			if (!u->burrowed && current_frame - u->controller->last_burrow >= 15) {
//				u->controller->last_burrow = current_frame;
//				u->game_unit->burrow();
//			}
//		}
//	}

	unit* ne = get_best_score(enemies, [&](unit*e) {
		return units_distance(u, e);
	});
	size_t u_index = grid::build_square_index(u->pos);
// 	if (!entire_threat_area.test(u_index)) {
// 
// 		double best_distance = 0.0;
// 		xy best_pos = my_closest_base;
// 		find_nearby_entirely_walkable_tiles(u->pos, [&](xy pos) {
// 			if (entire_threat_area.test(grid::build_square_index(pos))) return false;
// 			double d = diag_distance(ne->pos - pos);
// 			if (d > best_distance) {
// 				best_distance = d;
// 				best_pos = pos;
// 			}
// 			return true;
// 		}, 32 * 12);
// 
// 		a->target_pos = best_pos;
	if (false) {
	} else {
		//if (current_used_total_supply >= 40 || entire_threat_area.test(u_index) || my_base.test(u_index) || diag_distance(u->pos - my_closest_base) <= 32 * 12) {
		if (true) {
			xy best_pos;
			int best_pos_score = std::numeric_limits<int>::max();
			int n = 0;
			find_path(u->type, u->pos, [&](xy pos, xy npos) {
				return diag_distance(npos - u->pos) <= 32 * 20;
			}, [&](xy pos, xy npos) {
				double cost = 0.0;
				for (unit*e : enemies) {
					weapon_stats*w = a->u->is_flying ? e->stats->air_weapon : e->stats->ground_weapon;
					if (e->type == unit_types::bunker) w = units::get_unit_stats(unit_types::marine, e->owner)->ground_weapon;
					if (!w) continue;
					double d = diag_distance(pos - e->pos);
					if (d <= w->max_range*1.5) {
						cost += w->max_range*1.5 - d;
					}
				}
				return cost + diag_distance(a->goal_pos - a->u->pos);
			}, [&](xy pos) {
				size_t index = grid::build_square_index(pos);
				if (entire_threat_area_edge.test(index) && (u->is_flying || !run_spot_taken.test(index))) {
					int s = run_score[index];
					if (s < best_pos_score) {
						best_pos_score = s;
						best_pos = pos;
					}
					++n;
					if (n >= 16) return true;
				}
				return false;
				//return !entire_threat_area.test(index) && (u->is_flying || !run_spot_taken.test(index));
			});

			if (best_pos == xy()) {
				if (entire_threat_area.test(grid::build_square_index(u->pos))) {
					unit* target = get_best_score(enemies, [&](unit* e) {
						weapon_stats* w = e->is_flying ? u->stats->ground_weapon : u->stats->air_weapon;
						if (!w) return std::numeric_limits<double>::infinity();
						double d = units_distance(u, e);
						if (d <= w->max_range && d >= w->min_range) return -(1.0 / (e->shields + e->hp));
						return d;
					}, std::numeric_limits<double>::infinity());
					if (target) {
						a->subaction = combat_unit::subaction_fight;
						a->target = target;
						return;
					}
				}
			}

			a->target_pos = best_pos == xy() ? a->goal_pos : best_pos;
			if (no_aggressive_groups && best_pos == xy()) {
				xy relpos = a->target_pos - u->pos;
				double ang = relpos.angle();
				double r = relpos.length();
				xy npos = u->pos + xy((int)(std::cos(ang) * (r + 64)), (int)(std::sin(ang) * (r + 64)));
				if ((size_t)npos.x < (size_t)grid::map_width && (size_t)npos.y < (size_t)grid::map_height) {
					auto&bs = grid::get_build_square(npos);
					if (bs.entirely_walkable || u->is_flying) {
						a->target_pos = npos;
					}
				}
			}
		} else {
			a->target_pos = my_closest_base;
		}
		
		if (a->subaction == combat_unit::subaction_move) {
			a->last_run_pos = a->target_pos;
		}
	}

	if (diag_distance(a->target_pos - u->pos) <= 32 * 4 && entire_threat_area.test(grid::build_square_index(u->pos))) {
		xy dstrel = a->target_pos - u->pos;
		double len = dstrel.length();
		double norm_x = dstrel.x / len;
		double norm_y = dstrel.y / len;
		for (double i = len; i <= 32 * 6; i += 32.0) {
			xy npos = u->pos + xy((int)(norm_x * i), (int)(norm_y * i));
			if ((size_t)npos.x >= (size_t)grid::map_width || (size_t)npos.y >= (size_t)grid::map_height) continue;
			auto& bs = grid::get_build_square(npos);
			if (!bs.entirely_walkable || bs.building) break;
			a->target_pos = npos;
		}
	}

// 	a->target_pos.x = a->target_pos.x&-32 + 16;
// 	a->target_pos.y = a->target_pos.y&-32 + 16;

	//log("%s: running to spot %g away (%d %d)\n", a->u->type->name, (a->target_pos - a->u->pos).length(), a->target_pos.x, a->target_pos.y);

//	int keep_mines = 2;
//	if (a->u->hp <= 53) keep_mines = 0;
//	if (a->u->spider_mine_count > keep_mines && players::my_player->has_upgrade(upgrade_types::spider_mines) && ((a->target_pos - a->u->pos).length() < 32 || a->u->spider_mine_count == 3)) {
//		lay_mine(a);
//	}
	if (a->u->spider_mine_count && players::my_player->has_upgrade(upgrade_types::spider_mines)) {
		if (ne && !ne->is_flying) {
			auto* w = a->u->is_flying ? ne->stats->air_weapon : ne->stats->ground_weapon;
			double d = units_distance(a->u, ne);
			if (!w || d > w->max_range || d < w->min_range) {
				int n_nearby = 0;
				for (unit* u : my_units_of_type[unit_types::spider_mine]) {
					if (diag_distance(u->pos - a->u->pos) <= 32 * 3) ++n_nearby;
				}
				if (n_nearby < 3) lay_mine(a);
			}
		}
	}

	run_spot_taken.set(grid::build_square_index(a->target_pos));

	if (a->target && a->target->visible) {
		unit*target = a->target;
		if (a->u->spider_mine_count && !target->is_flying && !target->type->is_hovering && !target->type->is_building && players::my_player->has_upgrade(upgrade_types::spider_mines)) {
			if (target->hp >= 40) {
				//spider_mine_layers.push_back(a);
			}
		}
	}

}


choke_t find_choke_from_to(unit_type*path_ut, xy from, xy to, bool under_attack, bool is_from_expo, bool use_nearest, double arg_max_len = 0.0, size_t arg_n_space = 0, bool must_be_buildable = false) {
	while (square_pathing::get_pathing_map(path_ut).path_nodes.empty()) multitasking::sleep(1);
	//from = square_pathing::get_nearest_node_pos(path_ut, from);
	//to = square_pathing::get_nearest_node_pos(path_ut, to);
	auto&pathing_map = square_pathing::get_pathing_map(path_ut, square_pathing::pathing_map_index::no_enemy_buildings);
	auto nodes = square_pathing::get_nearest_path_nodes(pathing_map, from, to);
	if (nodes.first) from = nodes.first->pos;
	if (nodes.second) to = nodes.second->pos;
	size_t iterations = 0;
	a_deque<xy> path = square_pathing::find_square_path(pathing_map, from, [&](xy pos, xy npos) {
		if (++iterations % 1024 == 0) multitasking::yield_point();
		return true;
	}, [&](xy pos, xy npos) {
		return 0.0;
	}, [&](xy pos) {
		return manhattan_distance(to - pos) <= 32;
	});
	log("choke path size is %d\n", path.size());
	size_t n_space = use_nearest ? 8 : 32;
	if (arg_n_space) n_space = arg_n_space;
	double len = 0.0;
	double max_len = 32 * 45;
	if (is_from_expo) max_len = 32 * 10;
	if (under_attack) max_len = 32 * 8;
	if (arg_max_len) max_len = arg_max_len;
	size_t far_index = 0;
	for (size_t i = 1; i < path.size(); ++i) {
		len += diag_distance(path[i] - path[i - 1]);
		if (len >= 32 * 25 && far_index == 0) far_index = i;
		if (len >= max_len) {
			path.resize(std::min(i + n_space * 2, path.size()));
			break;
		}
	}

	double best_score = std::numeric_limits<double>::infinity();
	choke_t best;
	bool has_found_short = false;
	for (size_t i = 0; i + n_space * 2 < path.size(); ++i) {
		if (has_found_short && far_index && i >= far_index + n_space * 2) break;
		for (int n = 0; n < 3; ++n) {
			choke_t ch;
			ch.center = path[i + n_space];

			ch.inside = get_best_score(make_iterators_range(path.begin() + i, path.begin() + i + n_space), [&](xy pos) {
				return -diag_distance(pos - ch.center);
			});
			ch.outside = get_best_score(make_iterators_range(path.begin() + i + n_space, path.begin() + i + n_space + n_space), [&](xy pos) {
				return -diag_distance(pos - ch.center);
			});

			xy rel = path[i + n_space] - path[i + n_space - 1];
			double a = atan2(rel.y, rel.x);
			bool is_buildable = true;
			auto go = [&](double ang) {
				xy add;
				add.x = (int)(cos(ang) * 32);
				add.y = (int)(sin(ang) * 32);
				xy pos = ch.center;
				while (true) {
					pos += add;
					if ((size_t)pos.x >= (size_t)grid::map_width || (size_t)pos.y >= (size_t)grid::map_height) return pos - add;
					auto&bs = grid::get_build_square(pos);
					if (!bs.entirely_walkable) return pos - add;
					if (!bs.buildable) is_buildable = false;
					ch.build_squares.push_back(&bs);
				}
			};
			ch.build_squares.push_back(&grid::get_build_square(ch.center));
			double add = -PI / 8 + PI / 8 * n;
			xy left = go(a + PI / 2 + add);
			xy right = go(a - PI / 2 + add);
			ch.width = (right - left).length();

			double s = ch.width;
			if (must_be_buildable && !is_buildable) s += 32 * 10;
			s -= (grid::get_build_square(ch.center).height - grid::get_build_square(ch.outside).height) * 32 * 3;
			//log("found choke with width %g score %g (%d build squares)\n", ch.width, s, ch.build_squares.size());
			if (s < best_score) {
				best_score = s;
				best = std::move(ch);

				if (!has_found_short && ch.build_squares.size() <= 5) has_found_short = true;
			}
		}
		if (use_nearest) break;
	}
	best.path = path;

	std::sort(best.build_squares.begin(), best.build_squares.end(), [&](grid::build_square*a, grid::build_square*b) {
		return diag_distance(a->pos - best.center) < diag_distance(b->pos - best.center);
	});
	if (best.build_squares.size() > 15) best.build_squares.resize(15);

	a_unordered_set<grid::build_square*> uncrossable;
	for (auto*bs : best.build_squares) {
		uncrossable.insert(bs);
	}

	tsc::dynamic_bitset visited(grid::build_grid_width*grid::build_grid_height);
	a_deque<std::tuple<grid::build_square*, bool>> open;
	open.emplace_back(&grid::get_build_square(best.inside), true);
	visited.set(grid::build_square_index(best.inside));
	open.emplace_back(&grid::get_build_square(best.outside), false);
	visited.set(grid::build_square_index(best.outside));
	while (!open.empty()) {
		grid::build_square*bs;
		bool is_inside;
		std::tie(bs, is_inside) = open.front();
		open.pop_front();

		if (is_inside) best.inside_squares.push_back(bs);
		else best.outside_squares.push_back(bs);

		for (int d = 0; d < 4; ++d) {
			auto*n = bs->get_neighbor(d);
			if (!n) continue;
			if (!n->entirely_walkable) continue;
			if ((n->pos - best.center).length() > 32 * 12) continue;
			if (uncrossable.count(n)) continue;
			size_t index = grid::build_square_index(*n);
			if (visited.test(index)) continue;
			visited.set(index);
			open.emplace_back(n, is_inside);
		}
	}
	log("%d inside squares\n", best.inside_squares.size());
	log("%d outside squares\n", best.outside_squares.size());

	return best;
}


int last_calc_defence_paths;

void update_defence_choke() {
	a_vector<xy> my_bases;
	a_vector<xy> op_bases;
	while (square_pathing::get_pathing_map(unit_types::siege_tank_tank_mode).path_nodes.empty()) multitasking::sleep(1);
	for (auto&v : buildpred::get_my_current_state().bases) {
		my_bases.push_back(square_pathing::get_nearest_node_pos(unit_types::siege_tank_tank_mode, v.s->cc_build_pos));
	}
	for (auto&v : buildpred::get_op_current_state().bases) {
		op_bases.push_back(square_pathing::get_nearest_node_pos(unit_types::siege_tank_tank_mode, v.s->cc_build_pos));
	}
	group_t*largest_enemy_group = get_best_score_p(groups, [&](group_t*g) {
		return -(int)g->enemies.size();
	});
	if (largest_enemy_group && largest_enemy_group->enemies.size() >= 5) {
		op_bases.push_back(largest_enemy_group->enemies.front()->pos);
	}

	if (!has_found_opponent_start_location) {
		int n = 0;
		for (auto pos : start_locations) {
			auto& bs = grid::get_build_square(pos);
			if (bs.building && bs.building->owner->is_enemy && bs.building->type->is_resource_depot) {
				has_found_opponent_start_location = true;
				opponent_start_location = pos;
				break;
			}
		}
		if (opponent_start_location == xy()) {
			int n_seen = 0;
			int n_unseen = 0;
			opponent_start_location = get_best_score(start_locations, [&](xy pos) {
				auto& bs = grid::get_build_square(pos);
				if (bs.last_seen < 90) ++n_unseen;
				else ++n_seen;
				return bs.last_seen;
			});
			if (n_unseen == 1) has_found_opponent_start_location = true;
		}
	}
	
	for (auto&b : build::build_tasks) {
		if (b.built_unit) continue;
		if (b.build_pos != xy() && b.type->unit && b.type->unit->is_resource_depot) my_bases.push_back(b.build_pos);
	}
	log("my_bases.size() is %d op_bases.size() is %d\n", my_bases.size(), op_bases.size());

	inter_base_paths.clear();
	xy main_base = get_best_score(my_bases, [&](xy pos) {
		double r = 0.0;
		for (unit*u : my_buildings) {
			double d = diag_distance(pos - u->pos);
			if (d <= 32 * 30) r += d;
			else r += 32 * 128 + d;
		}
		return r;
	});
	for (xy pos : my_bases) {
		if (pos == main_base) continue;
		multitasking::yield_point();
		auto path = find_bigstep_path(unit_types::siege_tank_tank_mode, main_base, pos, square_pathing::pathing_map_index::default_);
		if (path.empty()) continue;
		inter_base_paths.push_back(std::move(path));
	}

	if (!my_bases.empty() && !op_bases.empty()) {
// 		xy my_closest = get_best_score(my_bases, [&](xy pos) {
// 			return get_best_score_value(op_bases, [&](xy pos2) {
// 				return unit_pathing_distance(unit_types::zealot, pos, pos2);
// 			});
// 		});
		a_vector<xy> all_inter_base_positions;
		for (xy pos : my_bases) {
			all_inter_base_positions.push_back(pos);
		}
		for (auto&v : inter_base_paths) {
			for (xy pos : v) all_inter_base_positions.push_back(pos);
		}
		xy my_closest = get_best_score(all_inter_base_positions, [&](xy pos) {
			multitasking::yield_point();
			return get_best_score_value(op_bases, [&](xy pos2) {
				return unit_pathing_distance(unit_types::siege_tank_tank_mode, pos, pos2);
			});
		});
		xy op_closest = get_best_score(op_bases, [&](xy pos) {
			multitasking::yield_point();
			return get_best_score_value(my_bases, [&](xy pos2) {
				return unit_pathing_distance(unit_types::siege_tank_tank_mode, pos, pos2);
			});
		});
		if (current_frame <= my_closest_base_override_until) {
			log("my closest base override\n");
			my_closest = my_closest_base_override;
		}
		//if (buildpred::get_my_current_state().bases.size() > 2 && no_aggressive_groups && my_closest_base != xy()) my_closest = my_closest_base;
		log("my_closest is %d %d, op_closest is %d %d\n", my_closest.x, my_closest.y, op_closest.x, op_closest.y);
		bool under_attack = false;
		double enemy_supply_nearby = 0;
		for (unit*e : enemy_units) {
			if (e->gone || e->type->is_non_usable) continue;
			if (diag_distance(e->pos - my_closest) <= 32 * 25) enemy_supply_nearby += e->type->required_supply;
		}
		if (enemy_supply_nearby > current_used_total_supply - my_workers.size() && enemy_supply_nearby > 1) under_attack = true;

		if (current_frame < force_defence_is_scared_until) defence_is_scared = true;
		else {
			//if (under_attack || (enemy_supply_nearby > 1 && my_workers.size() <= 11)) defence_is_scared = true;
			if (under_attack || (enemy_supply_nearby > 1 && my_workers.size() <= 11)) ;
			else if (defence_is_scared && current_used_total_supply - my_workers.size() >= 12) {
				defence_is_scared = false;
				log("defence no longer scared, since %g\n", current_used_total_supply - my_workers.size());
			}
			if (!my_units_of_type[unit_types::siege_tank_tank_mode].empty()) defence_is_scared = false;
			if (!my_units_of_type[unit_types::siege_tank_siege_mode].empty()) defence_is_scared = false;
			if (defence_is_scared) {
				for (auto&b : build::build_tasks) {
					if (b.built_unit || b.dead) continue;
					if (b.type->unit == unit_types::bunker || b.type->unit == unit_types::missile_turret) {
						defence_is_scared = false;
						break;
					}
				}
			}
		}

		bool redo = current_frame - last_calc_defence_paths >= 15 * 60;
		if (defence_choke.center == xy() && current_frame - last_calc_defence_paths >= 15 * 4) redo = true;
		if ((under_attack || defence_is_scared) && current_frame - last_calc_defence_paths >= 15) redo = true;
		if (my_closest != my_closest_base || op_closest != op_closest_base || redo) {
			last_calc_defence_paths = current_frame;
			my_closest_base = my_closest;
			op_closest_base = op_closest;
			bool is_from_expo = diag_distance(my_closest_base - my_start_location) > 32 * 10;
			bool use_nearest = defence_choke_use_nearest_until >= current_frame;
			defence_choke = find_choke_from_to(unit_types::zealot, my_closest_base, op_closest_base, under_attack, is_from_expo, use_nearest);

			defence_choke_outside_bitset.reset();
			for (auto*bs : defence_choke.outside_squares) {
				defence_choke_outside_bitset.set(grid::build_square_index(*bs));
			}

			log("my_start_location is %d %d\n", my_start_location.x, my_start_location.y);
			log("my_closest_base is %d %d\n", my_closest_base.x, my_closest_base.y);

			int iterations = 0;
			dont_siege_here_path = find_path(unit_types::siege_tank_tank_mode, square_pathing::get_nearest_node_pos(unit_types::siege_tank_tank_mode, my_start_location), [&](xy pos, xy npos) {
				if (++iterations % 1024 == 0) multitasking::yield_point();
				return true;
			}, [&](xy pos, xy npos) {
				return 0.0;
			}, [&](xy pos) {
				return manhattan_distance(pos - op_closest_base) <= 64;
			});
			log("dont_siege_here_path.size() is %d\n", dont_siege_here_path.size());
			dont_siege_here_path.resize(dont_siege_here_path.size() / 2);

			dont_siege_here.reset();
			for (xy pos : dont_siege_here_path) {
				auto set = [&](xy pos) {
					if ((size_t)pos.x >= (size_t)grid::map_width || (size_t)pos.y >= (size_t)grid::map_height) return;
					dont_siege_here.set(grid::build_square_index(pos));
				};
				set(pos);
				set(pos + xy(-32, -32));
				set(pos + xy(+32, -32));
				set(pos + xy(+32, +32));
				set(pos + xy(-32, +32));
			}

			if (defence_is_scared) {
				xy scvs_pos;
				int count = 0;
				for (unit*u : my_workers) {
					if (diag_distance(u->pos - my_closest) >= 32 * 20) continue;
					scvs_pos += u->pos;
					++count;
				}
				if (count) {
					scvs_pos /= count;
					defence_choke.center = scvs_pos;
					defence_choke.inside = scvs_pos;
					defence_choke.build_squares.clear();
					defence_choke.build_squares.push_back(&grid::get_build_square(scvs_pos));
				}
			}
		}
	}
}

void get_defensive_positions(const a_vector<combat_unit*>&allies, a_vector<xy>&defensive_positions, bool&is_defensive_concave) {
	defensive_positions.clear();
	is_defensive_concave = false;
	double total_width = 0.0;
	for (auto*a : allies) {
		total_width += a->u->type->width;
	}
	tsc::dynamic_bitset inside_bitset(grid::build_grid_width*grid::build_grid_height);
	for (auto*bs : defence_choke.inside_squares) {
		inside_bitset.set(grid::build_square_index(*bs));
	}
	xy avg_center;
	for (auto*bs : defence_choke.build_squares) {
		avg_center += bs->pos + xy(16, 16);
	}
	if (defence_choke.build_squares.empty()) avg_center = defence_choke.center;
	else avg_center /= defence_choke.build_squares.size();
	tsc::dynamic_bitset added(grid::build_grid_width*grid::build_grid_height);
	for (double r = 32.0; r <= 32.0 * 8; r += 32.0) {
		defensive_positions.clear();
		added.reset();
		double step = (32.0 / (r * PI * 2)) * PI * 2;
		for (double a = 0.0; a < PI * 2; a += step) {
			xy pos = avg_center;
			pos.x += (int)(std::cos(a) * r);
			pos.y += (int)(std::sin(a) * r);
			if ((size_t)pos.x >= (size_t)grid::map_width || (size_t)pos.y >= (size_t)grid::map_height) continue;
			size_t index = grid::build_square_index(pos);
			if (!inside_bitset.test(index)) continue;
			if (added.test(index)) continue;
			added.set(index);
			defensive_positions.push_back(pos);
		}
		if (defensive_positions.size() >= (size_t)(total_width / 32)) break;
		if (defensive_positions.empty() && r >= 32 * 4) break;
		is_defensive_concave = true;
	}
	is_defensive_concave = true;
}


void do_defence_choke(const a_vector<combat_unit*>& allies) {

	bool has_bunkers = my_completed_units_of_type[unit_types::bunker].size() >= 2;

	auto siege_score = [&](xy pos) {
		double outside_d = unit_pathing_distance(unit_types::zealot, pos, defence_choke.outside);
		double inside_d = unit_pathing_distance(unit_types::zealot, pos, defence_choke.inside);
		if (inside_d >= 32 * 15) inside_d *= 10;
		int count = 0;
		if (has_bunkers && false) {
			if (diag_distance(pos - defence_choke.outside) <= 32 * 12 + 32) count += 4;
		} else {
			for (auto*bs : defence_choke.build_squares) {
				if (diag_distance(pos - bs->pos + xy(16, 16)) <= 32 * 12 + 32) ++count;
			}
		}
		if (count < std::min(4, (int)defence_choke.build_squares.size())) return 0.0;
		double height = grid::get_build_square(pos).height - grid::get_build_square(defence_choke.outside).height;
		return -outside_d + inside_d - count * 12;
		//return -outside_d + inside_d - count * 12 - height * 64;
	};

	for (auto*a : allies) {
		if (a->siege_pos != xy()) {
			double s = siege_score(a->siege_pos);
			bool reset = s >= 0;
			if (a->u->type == unit_types::siege_tank_tank_mode) {
				size_t index = grid::build_square_index(a->siege_pos);
				if (build_square_taken.test(index)) reset = true;
				if (dont_siege_here.test(index)) reset = true;
			}
			if (a->u->type == unit_types::siege_tank_siege_mode && current_frame - a->u->controller->last_siege >= 15 * 40) reset = true;
			if (reset) a->siege_pos = xy();
		}
		if (a->siege_pos != xy()) {
			auto set = [&](xy pos) {
				if ((size_t)pos.x >= (size_t)grid::map_width || (size_t)pos.y >= (size_t)grid::map_height) return;
				build_square_taken.set(grid::build_square_index(pos));
			};
			set(a->siege_pos);
			set(a->siege_pos + xy(+32, 0)); set(a->siege_pos + xy(+32, +32));
			set(a->siege_pos + xy(0, +32)); set(a->siege_pos + xy(-32, +32));
			set(a->siege_pos + xy(-32, 0)); set(a->siege_pos + xy(-32, -32));
			set(a->siege_pos + xy(0, -32)); set(a->siege_pos + xy(+32, -32));
		}
	}

	a_vector<std::tuple<xy, int>> defence_position_count;

	bool is_defensive_concave = false;
	a_vector<xy> defensive_positions;
	get_defensive_positions(allies, defensive_positions, is_defensive_concave);
	for (xy pos : defensive_positions) {
		defence_position_count.emplace_back(pos + xy(16, 16), 0);
	}


	// 	a_vector<std::tuple<xy, int>> defence_position_count;
	// 	for (auto*bs : defence_choke.build_squares) {
	// 		defence_position_count.emplace_back(bs->pos + xy(16, 16), 0);
	// 	}

	int path_iterations = 0;
	for (auto*a : allies) {
		bool default_move = true;
		if ((a->u->type == unit_types::siege_tank_tank_mode || a->u->type == unit_types::siege_tank_siege_mode) && players::my_player->has_upgrade(upgrade_types::siege_mode)) {
			if (a->u->type == unit_types::siege_tank_tank_mode && current_frame - a->last_find_siege_pos >= 15 * 2) {
				a->last_find_siege_pos = current_frame;
				tsc::dynamic_bitset visited(grid::build_grid_width*grid::build_grid_height);
				double best_score = 0.0;
				xy best_pos;
				find_path(a->u->type, a->u->pos, [&](xy pos, xy npos) {
					if (++path_iterations % 1024 == 0) multitasking::yield_point();
					double d = diag_distance(npos - a->u->pos);
					if (d >= 32 * 30) return false;
					size_t index = grid::build_square_index(npos);
					if (build_square_taken.test(index)) return false;
					if (d >= 32 * 3 && !visited.test(index) && !dont_siege_here.test(index)) {
						visited.set(index);
						double s = siege_score(npos);
						if (s < best_score) {
							best_score = s;
							best_pos = npos;
						}
					}
					return true;
				}, [&](xy pos, xy npos) {
					return 0.0;
				}, [&](xy pos) {
					return false;
				});
				if (a->siege_pos != xy() && siege_score(a->siege_pos) < best_score) best_pos = a->siege_pos;
				if (best_pos != xy()) {
					a->siege_pos = best_pos;
				}
			}
			if (a->siege_pos != xy()) {
				default_move = false;
				if (a->u->type == unit_types::siege_tank_siege_mode) {
					a->siege_pos = a->u->pos;
					a->subaction = combat_unit::subaction_idle;
					if (diag_distance(a->u->pos - a->siege_pos) >= 16) {
						if (a->u->game_unit->unsiege()) {
							a->u->controller->noorder_until = current_frame + 30;
						}
					}
				} else {
					a->subaction = combat_unit::subaction_idle;
					if (diag_distance(a->u->pos - a->siege_pos) > 8) {
						a->subaction = combat_unit::subaction_move;
						a->target_pos = a->siege_pos;
					} else {
						if (players::my_player->has_upgrade(upgrade_types::siege_mode)) {
							if (current_frame >= a->u->controller->move_away_until && a->u->game_unit->siege()) {
								a->u->controller->noorder_until = current_frame + 30;
								a->u->controller->last_siege = current_frame;
							}
						}
					}
				}
			}
		}
		if (default_move) {
			a->subaction = combat_unit::subaction_move;
			if (!defence_position_count.empty()) {
				auto*v = get_best_score_p(defence_position_count, [&](std::tuple<xy, int>*v) {
					return std::make_tuple(std::get<1>(*v), diag_distance(std::get<0>(*v) - a->u->pos));
				});
				++std::get<1>(*v);
				a->target_pos = std::get<0>(*v);
			} else {
				if (a->target_pos == defence_choke.center || a->target_pos == defence_choke.inside) {
					if (diag_distance(a->u->pos - a->target_pos) <= 32 * 3) {
						if (a->target_pos == defence_choke.center) a->target_pos = defence_choke.inside;
						else a->target_pos = defence_choke.center;
					}
				} else a->target_pos = defence_choke.center;
			}
			//a->target_pos = defence_choke.inside;
			if (wall_in::active_wall_count()) a->target_pos = defence_choke.inside;
		}
	}

}

struct defence_spot {
	xy pos;
	double weight;
};

a_vector<defence_spot> defence_spots;

void do_defence_spots(const a_vector<combat_unit*>& allies) {

	a_vector<combat_unit*> available = allies;

	auto get_nearest = [&](xy pos) {

		auto* r = get_best_score(available, [&](combat_unit* a) {
			return unit_pathing_distance(a->u, pos);
		});
		available.erase(std::find(available.begin(), available.end(), r));
		return r;

// 		auto* r = available.back();
// 		available.pop_back();
// 		return r;

// 		auto** a_ptr = get_best_score_p(available, [&](combat_unit** a) {
// 			return unit_pathing_distance((*a)->u, pos);
// 		});
// 
// 		std::swap(*a_ptr, available.back());
// 		available.pop_back();
// 
// 		return *a_ptr;
	};

// 	for (auto* cs : active_control_spots) {
// 		if (cs->is_best_defence) {
// 			for (auto i = available.begin(); i != available.end();) {
// 				auto* a = *i;
// 				if (a->u->type == unit_types::siege_tank_tank_mode || a->u->type == unit_types::siege_tank_siege_mode) {
// 					a->assigned_defence_control_spot = cs;
// 
// 					a->subaction = combat_unit::subaction_move;
// 					a->target_pos = cs->pos;
// 
// 					i = available.erase(i);
// 				} else ++i;
// 			}
// 			break;
// 		}
// 	}

	size_t n_left = available.size();

	double carry = 0.0;
	for (auto& v : defence_spots) {
		double to_assign_val = allies.size() * v.weight;
		size_t to_assign = (size_t)to_assign_val;
		carry += to_assign_val - to_assign;
		if (carry >= 1.0 || &v == &defence_spots.back()) {
			++to_assign;
			carry -= 1.0;
		}

		//log(log_level_info, "defence spot at %d %d - assigning %d / %d\n", v.pos.x, v.pos.y, to_assign, allies.size());

		control_spot* cs = get_best_score(active_control_spots, [&](control_spot* cs) {
			return diag_distance(cs->pos - v.pos);
		});
		if (cs) {

			for (; to_assign; --to_assign) {
				if (!n_left) break;
				auto* a = get_nearest(v.pos);
				--n_left;

				a->assigned_defence_control_spot = cs;

				a->subaction = combat_unit::subaction_move;
				//a->target_pos = v.pos;
				a->target_pos = cs->pos;

				//log(log_level_info, "defence spot - send %s to %d %d\n", a->u->type->name, v.pos.x, v.pos.y);

			}
		}

	}


}

void do_defence(const a_vector<combat_unit*>&allies) {

	//if (current_used_total_supply < 30 || defence_spots.empty()) {
	if (buildpred::get_my_current_state().bases.size() == 1 || defence_spots.empty()) {
		do_defence_choke(allies);
	} else do_defence_spots(allies);
}

int last_tank_line_scan = 0;

void do_tank_line(const a_vector<combat_unit*>& allies, const a_vector<unit*>& enemies, bool defend, group_t* g) {
	
	log(log_level_test, " do_tank_line %d allies, %d enemies (defend %d)\n", allies.size(), enemies.size(), defend);
	
	a_vector<combat_unit*> my_tanks;
	int my_bc_count = 0;
	for (auto* a : allies) {
		if (a->u->type == unit_types::siege_tank_tank_mode || a->u->type == unit_types::siege_tank_siege_mode) my_tanks.push_back(a);
		if (a->u->type == unit_types::battlecruiser) ++my_bc_count;
	}
	
	a_vector<unit*> enemy_tanks;
	int enemy_anti_air_count = 0;
	for (unit* u : enemies) {
		if (u->type == unit_types::siege_tank_tank_mode || u->type == unit_types::siege_tank_siege_mode) enemy_tanks.push_back(u);
		if (u->stats->air_weapon) ++enemy_anti_air_count;
	}
	
	a_vector<unit*> allies_units;
	allies_units.reserve(allies.size());
	for (auto* a : allies) allies_units.push_back(a->u);
	
	for (auto* a : allies) {
		
		if (a->u->type->is_worker) continue;
		
		unit* u = a->u;
		unit* target = get_best_score(enemies, [&](unit*e) {
			if (e->shields + e->hp - prepared_damage[e] <= 0.0) return std::make_tuple(std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity());
			if (e->type == unit_types::interceptor && u->is_flying) return std::make_tuple(std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity(), 32.0 * 12);

			int w_damage_type = weapon_stats::damage_type_normal;
			double w_min_range = 0.0;
			double w_max_range = 128.0;
			weapon_stats* w = e->type->is_flyer ? u->stats->air_weapon : u->stats->ground_weapon;
			if (w) {
				w_damage_type = w->damage_type;
				w_min_range = w->min_range;
				w_max_range = w->max_range;
				if (u->type == unit_types::siege_tank_tank_mode) w_max_range = 32 * 12;
			} else {
				if (u->type == unit_types::carrier) {
					w_max_range = 32 * 8;
				} else return std::make_tuple(std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity());
			}
			double d = units_distance(u->pos, u->type, e->pos, e->type);
			if (d < w_min_range) return std::make_tuple(std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity());
			if (d > w_max_range) return std::make_tuple(std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity(), d);

			double hp = e->hp / combat_eval::get_damage_type_modifier(w_damage_type, e->type->size);
			double v = e->shields + hp;

			if (u->type == unit_types::dragoon && e->type == unit_types::interceptor) hp += 450.0;

			if (!(u->is_flying ? e->stats->air_weapon : e->stats->ground_weapon)) {
				bool is_priority_target = false;
				is_priority_target |= e->type == unit_types::carrier;
				is_priority_target |= e->type == unit_types::reaver;
				is_priority_target |= e->type == unit_types::high_templar;
				is_priority_target |= e->type == unit_types::defiler;
				is_priority_target |= e->type == unit_types::observer;
				is_priority_target |= e->type == unit_types::queen;
				is_priority_target |= e->type == unit_types::dark_archon;
				is_priority_target |= e->type == unit_types::science_vessel;
				if (!is_priority_target) return std::make_tuple(std::numeric_limits<double>::infinity(), v, 0.0);
				v /= 10;
			}
			
			if (e->type == unit_types::siege_tank_siege_mode) v /= 10;
			if (u->type == unit_types::siege_tank_siege_mode) {
				if (no_splash_target.count(e)) v += 1000.0;
			}
			if (!e->detected && scans_available < 2) v += 1000.0;

			return std::make_tuple(v, 0.0, 0.0);
		});
		
		if (target) {
			a->subaction = combat_unit::subaction_fight;
			a->target = target;
			
			if (target->type == unit_types::shuttle || target->type == unit_types::reaver) {
				if (my_base.test(grid::build_square_index(target->pos)) || my_base.test(grid::build_square_index(u->pos))) {
					if (a->u->type == unit_types::siege_tank_siege_mode) {
						if (current_frame - a->u->controller->last_siege >= 12 && a->u->game_unit->unsiege()) {
							a->u->controller->noorder_until = current_frame + 30;
							a->u->controller->last_siege = current_frame;
							log(log_level_test, "tank line unsiege (reaver/shuttle)\n");
						}
					}
					continue;
				}
			}
			
		} else {
			a->subaction = combat_unit::subaction_move;
			a->target_pos = a->goal_pos;
		}
		
		xy upos = a->u->pos + xy((int)(a->u->hspeed*latency_frames), (int)(a->u->vspeed*latency_frames));
		xy tpos;
		if (target) tpos = target->pos + xy((int)(target->hspeed*latency_frames), (int)(target->vspeed*latency_frames));
		
		if (a->u->type != unit_types::siege_tank_tank_mode && a->u->type != unit_types::siege_tank_siege_mode) {
			bool attack = false;
			combat_unit* nearest_tank = get_best_score(my_tanks, [&](combat_unit* c) {
				return diag_distance(target->pos - c->u->pos);
			});
			if (target) {
				if (nearest_tank) {
					if (u->type == unit_types::battlecruiser && players::my_player->has_upgrade(upgrade_types::yamato_gun) && u->energy >= 150.0) {
						if (units_distance(u, nearest_tank->u) <= 32 * 10) {
							unit* yamato_target = get_best_score(enemies, [&](unit* e) {
								if (e->type->total_gas_cost < 100.0 && e->type != unit_types::missile_turret && (enemy_anti_air_count > my_bc_count || e->type != unit_types::goliath)) return std::numeric_limits<double>::infinity();
								double d = units_distance(u, e);
								if (d > 32 * 14) return std::numeric_limits<double>::infinity();
								double ehp = e->shields + e->hp;
								ehp -= focus_fire[e];
								if (ehp < 0) return std::numeric_limits<double>::infinity();
								return d;
							}, std::numeric_limits<double>::infinity());
							if (yamato_target) {
								double nearest_threat_d = get_best_score_value(enemies, [&](unit* e) {
									auto* ew = u->is_flying ? e->stats->air_weapon : e->stats->ground_weapon;
									double r = ew ? ew->max_range : 0.0;
									if (e->type == unit_types::bunker) r = 32 * 6;
									return units_distance(upos, u, e->pos, e) - r;
								});
								if (nearest_threat_d > 32 * 10 - units_distance(u, yamato_target)) {
									if (units_distance(upos, u, yamato_target->pos, yamato_target) <= 32 * 10) {
										if (current_frame - a->last_used_special >= 15 * 2) {
											a->u->game_unit->useTech(upgrade_types::yamato_gun->game_tech_type, target->game_unit);
											a->last_used_special = current_frame;
											a->u->controller->noorder_until = current_frame + 15 * 5;
					
											auto yamato_damage = [&](unit*target) {
												double damage = 260;
												if (target->shields <= 0) damage *= combat_eval::get_damage_type_modifier(weapon_stats::damage_type_explosive, target->stats->type->size);
												damage -= target->stats->armor;
												if (damage <= 0) damage = 1.0;
												return damage;
											};
											double damage = yamato_damage(target);
											focus_fire[target] += damage;
											auto i = registered_focus_fire.find(a);
											if (i != registered_focus_fire.end()) {
												if (std::abs(focus_fire[std::get<0>(i->second)] -= std::get<1>(i->second)) < 1) {
													focus_fire.erase(std::get<0>(i->second));
												}
											}
											registered_focus_fire[a] = std::make_tuple(target, damage, current_frame + 15 * 10);
											a->registered_focus_fire_until = current_frame + 15 * 10;
										}
									} else {
										a->subaction = combat_unit::subaction_move;
										a->target_pos = yamato_target->pos;
										continue;
									}
								}
							}
						}
					}
					double tank_target_d = units_distance(nearest_tank->u->pos, nearest_tank->u, tpos, target);
					if (tank_target_d <= 32 * 12) {
						unit* nearest_enemy_tank = get_best_score(enemy_tanks, [&](unit* e) {
							return diag_distance(a->u->pos - e->pos);
						});
						if (!nearest_enemy_tank) {
							unit* nearest_tank_target = get_best_score(enemies, [&](unit* e) {
								if (e->is_flying) return std::numeric_limits<double>::infinity();
								return diag_distance(nearest_tank->u->pos - e->pos);
							});
							if (!nearest_tank_target) attack = true;
							else {
								if (tank_target_d <= 32 * 7) attack = true;
								else {
									double r = units_distance(upos, a->u, tpos, target);
									if (tank_target_d + 16 < r) attack = true;
								}
							}
						} else {
							if (!nearest_enemy_tank || units_distance(upos, u, nearest_enemy_tank->pos, nearest_enemy_tank) > 32 * 12 + 96) attack = true;
						}
						if (tank_target_d <= 32 * 7 && !target->is_flying) attack = true;
					}
					auto* w = target->is_flying ? u->stats->air_weapon : u->stats->ground_weapon;
					auto* ew = nearest_tank->u->is_flying ? target->stats->air_weapon : target->stats->ground_weapon;
					if (w && ew && w->max_range >= ew->max_range + 32) {
						//attack = true;
						double r = units_distance(upos, a->u, tpos, target);
						if (r > w->max_range) {
							a->subaction = combat_unit::subaction_move;
							a->target_pos = nearest_tank->u->pos;
							if (units_distance(upos, u, nearest_tank->u->pos, nearest_tank->u) <= 4) {
								a->subaction = combat_unit::subaction_move;
								a->target_pos = upos;
								if (current_frame - a->last_hold_position >= 15) {
									u->controller->noorder_until = current_frame + 30;
									if (u->game_order != BWAPI::Orders::HoldPosition) {
										a->last_hold_position = current_frame;
										u->game_unit->holdPosition();
									}
								}
							}
						}
					}
				}
			}
			if (!attack) {
				if (entire_threat_area.test(grid::build_square_index(u->pos))) {
					if (nearest_tank && u->is_flying && u->stats->max_speed >= 4.0) {
						unit* nearest_tank_target = get_best_score(enemies, [&](unit* e) {
							if (e->is_flying) return std::numeric_limits<double>::infinity();
							return diag_distance(nearest_tank->u->pos - e->pos);
						});
						if (nearest_tank_target) {
							double nearest_threat_d = get_best_score_value(enemies, [&](unit* e) {
								auto* ew = u->is_flying ? e->stats->air_weapon : e->stats->ground_weapon;
								double r = ew ? ew->max_range : 0.0;
								if (e->type == unit_types::bunker) r = 32 * 6;
								return units_distance(upos, u, e->pos, e) - r;
							});
							if (nearest_threat_d > 96 && units_distance(upos, u, nearest_tank_target->pos, nearest_tank_target) > u->stats->sight_range - 64) {
								a->subaction = combat_unit::subaction_move;
								a->target_pos = nearest_tank_target->pos;
							} else do_run(a, enemies);
						} else do_run(a, enemies);
					} else do_run(a, enemies);
				} else {
					combat_unit* nearest_tank = get_best_score(my_tanks, [&](combat_unit* c) {
						return diag_distance(u->pos - c->u->pos);
					});
					if (nearest_tank && units_distance(u, nearest_tank->u) > 32 * 3) {
						a->subaction = combat_unit::subaction_move;
						a->target_pos = nearest_tank->u->pos;
					} else do_run(a, enemies);
				}
			}
		}
		
		if (a->u->type == unit_types::siege_tank_tank_mode || a->u->type == unit_types::siege_tank_siege_mode) {
			if (target) {
				double r = units_distance(upos, a->u, tpos, target);
				bool has_siege_mode = players::my_player->has_upgrade(upgrade_types::siege_mode);
				if (r <= 32 * 7 && no_splash_target.count(target)) {
					has_siege_mode = false;
					if (a->u->type == unit_types::siege_tank_siege_mode) {
						if (current_frame - a->u->controller->last_siege >= 12 && a->u->game_unit->unsiege()) {
							a->u->controller->noorder_until = current_frame + 30;
							a->u->controller->last_siege = current_frame;
							log(log_level_test, "tank line unsiege (no splash)\n");
						}
					}
				}
				if (has_siege_mode) {
					if (scans_available > 4 || (r <= 32 * 14 && scans_available > 2) || (r <= 32 * 12 && scans_available > 1)) {
						if (current_frame - last_tank_line_scan >= 15 * 20) {
							for (unit* e : enemies) {
								if (diag_distance(e->pos - target->pos) >= 32 * 5) continue;
								if (!e->visible && !grid::get_build_square(e->pos).visible) {
									unit* u = get_best_score(my_units_of_type[unit_types::comsat_station], [&](unit*u) {
										return -u->energy;
									});
									if (u && u->energy >= 50) {
										u->game_unit->useTech(BWAPI::TechTypes::Scanner_Sweep, BWAPI::Position(e->pos.x, e->pos.y));
										last_tank_line_scan = current_frame;
										log(log_level_test, "used tank line scan\n");
									}
									break;
								}
							}
						}
					}
				}
				if (r <= 32 * 12 && has_siege_mode && !target->is_flying) {
					if (a->u->type == unit_types::siege_tank_tank_mode) {
						if (current_frame >= a->u->controller->last_siege + 12 && current_frame >= a->u->controller->move_away_until && a->u->game_unit->siege()) {
							a->u->controller->noorder_until = current_frame + 30;
							a->u->controller->last_siege = current_frame;
							log(log_level_test, "tank line siege (target in range)\n");
						}
					}
				} else if (current_frame - a->u->last_attacked >= 90) {
					double d = get_best_score_value(my_tanks, [&](combat_unit* c) {
						return units_distance(c->u, target);
					});
					if (defend) {
						int od = d;
						for (unit* u : my_buildings) {
							double nd = units_distance(u, target);
							if (nd < d) d = nd;
						}
						for (unit* u : my_workers) {
							double nd = units_distance(u, target);
							if (nd < d) d = nd;
						}
						if (d < od) {
							if (od - d <= 32 * 5) {
								d = od + 8;
							} else {
								has_siege_mode = false;
								if (r > 32 * 12 && a->u->type == unit_types::siege_tank_siege_mode) {
									if (current_frame - a->u->controller->last_siege >= 12 && a->u->game_unit->unsiege()) {
										a->u->controller->noorder_until = current_frame + 30;
										a->u->controller->last_siege = current_frame;
										log(log_level_test, "tank line unsiege (no splash)\n");
									}
								}
							}
						}
					} else {
						if (d > 32 * 14) d = 32 * 14;
						if (my_tanks.size() >= enemy_tanks.size() * 2 && d > 32 * 12) d = 32 * 12;
					}
					log(log_level_test, " tank line, d is %g\n", d);
					if (current_frame >= a->u->controller->last_siege + 12) {
						if (r <= d + 8) {
							if (a->u->type == unit_types::siege_tank_tank_mode) {
								if (has_siege_mode) {
									if (!dont_siege_here.test(grid::build_square_index(u->pos))) {
										if (current_frame >= a->u->controller->move_away_until && a->u->game_unit->siege()) {
											a->u->controller->noorder_until = current_frame + 30;
											a->u->controller->last_siege = current_frame;
											log(log_level_test, "tank line siege\n");
										}
									}
								} else {
									a->subaction = combat_unit::subaction_move;
									a->target_pos = upos;
								}
							}
						} else if (r >= d + 24) {
							if (a->u->type == unit_types::siege_tank_siege_mode) {
								if (a->u->game_unit->unsiege()) {
									a->u->controller->noorder_until = current_frame + 30;
									a->u->controller->last_siege = current_frame;
									log(log_level_test, "tank line unsiege\n");
								}
							}
						}
					}
				}
			} else do_run(a, enemies);
		}
		
		if (a->subaction == combat_unit::subaction_fight) {
			if (a->target->cloaked) {
				do_attack(a, allies_units, enemies);
			}
		}
		
		if (a->u->type == unit_types::vulture && g) {
			if (a->subaction == combat_unit::subaction_move) {
				auto* start_node = square_pathing::get_nearest_path_node(square_pathing::get_pathing_map(a->u->type), a->u->pos);
				a_unordered_map<square_pathing::path_node*, square_pathing::path_node*> visited;
				a_deque<square_pathing::path_node*> open;
				open.push_back(start_node);
				visited[start_node] = nullptr;
				square_pathing::path_node* goal_node = nullptr;
				while (!open.empty() && !goal_node) {
					auto* c = open.front();
					open.pop_front();
					for (auto* n : c->neighbors) {
						if (!visited.emplace(n, c).second) continue;
						size_t index = grid::build_square_index(n->pos);
						if (g->threat_area.test(index)) continue;
						for (auto& ng : groups) {
							if (&ng == g) continue;
							if (ng.n_ground_enemies) {
								if (ng.threat_area.test(index)) {
									goal_node = n;
									break;
								}
							}
						}
						if (goal_node) break;
						open.push_back(n);
					}
				}
				if (goal_node) {
					a_deque<square_pathing::path_node*> path;
					for (auto* i = goal_node; i; i = visited[i]) {
						path.push_front(i);
					}
					if (path.size() <= 2) a->target_pos = path.back()->pos;
					else a->target_pos = path[2]->pos;
				}
			}
		}
		
		multitasking::yield_point();
	}

}

a_vector<std::pair<int, xy>> active_storms;

void do_high_templars(const a_vector<combat_unit*>& allies, const a_vector<unit*>& enemies) {
	
	for (auto i = active_storms.begin(); i != active_storms.end();) {
		if (current_frame >= i->first + 15 * 3) i = active_storms.erase(i);
		else ++i;
	}
	
	for (auto* a : allies) {
		if (a->u->type != unit_types::high_templar && a->u->type != unit_types::dark_archon) continue;
		
		a->subaction = combat_unit::subaction_move;
		//a->target_pos = a->u->pos;
		
		unit* nearest_threat = get_best_score(enemies, [&](unit* e) {
			auto* w = a->u->is_flying ? e->stats->air_weapon : e->stats->ground_weapon;
			if (!w) return std::numeric_limits<double>::infinity();
			return units_distance(a->u, e) - w->max_range;
		}, std::numeric_limits<double>::infinity());
		
		auto* nearest_help = get_best_score(allies, [&](auto* c) {
			if (!c->u->stats->air_weapon && !c->u->stats->ground_weapon) return std::numeric_limits<double>::infinity();
			return diag_distance(a->u->pos - c->u->pos);
		}, std::numeric_limits<double>::infinity());
		
		if (nearest_threat) {
			if (nearest_help && units_distance(a->u, nearest_help->u) >= 32 * 3) {
				a->target_pos = nearest_help->u->pos;
			} else do_run(a, enemies);
		}
		
		if (a->u->type == unit_types::high_templar && players::my_player->has_upgrade(upgrade_types::psionic_storm) && a->u->energy >= 75) {
			
			auto is_storm_at = [&](xy pos) {
				for (auto& v : active_storms) {
					if (max_distance(pos - v.second) <= 32 * 2) return true;
				}
				return false;
			};
			
			for (int i = 0; i != 2; ++i) {
				if (i == 1) {
					if (nearest_threat) {
						auto* nearest_threat_target = get_best_score(allies, [&](combat_unit* c) {
							auto* w = c->u->is_flying ? nearest_threat->stats->air_weapon : nearest_threat->stats->ground_weapon;
							if (!w) return std::numeric_limits<double>::infinity();
							return units_distance(nearest_threat, c->u) - w->max_range;
						}, std::numeric_limits<double>::infinity());
						if (nearest_threat_target && units_distance(nearest_threat, nearest_threat_target->u) >= units_distance(nearest_threat, a->u) - 32) continue;
					}
				}
				xy best_pos;
				int best_targets = 0;
				for (unit* e : enemies) {
					if (e->building || e->stasis_timer) continue;
					double d = sc_distance(a->u->pos - e->pos);
					if (i == 0 && d > 32 * 9) continue;
					//xy pos = e->pos + xy((int)(e->hspeed*(latency_frames + 12)), (int)(e->vspeed*(latency_frames + 12)));
					xy pos = e->pos + xy((int)(e->hspeed*(latency_frames + 1)), (int)(e->vspeed*(latency_frames + 1)));
					
					if (is_storm_at(pos)) continue;
					
					int n_targets = 0;
					for (unit* e2 : enemies) {
						if (e2->building || e2->stasis_timer) continue;
						//xy e2_pos = e2->pos + xy((int)(e2->hspeed*(latency_frames + 12)), (int)(e2->vspeed*(latency_frames + 12)));
						xy e2_pos = e2->pos + xy((int)(e2->hspeed*(latency_frames + 1)), (int)(e2->vspeed*(latency_frames + 1)));
						if (is_storm_at(e2_pos)) continue;
						if (unit_distance(e2_pos, e2, pos) <= 48) ++n_targets;
					}
					if (e->detected) {
						if (n_targets <= 1 && (e->type->total_gas_cost < 100 || e->speed >= 4.0)) continue;
					}
					
					if (n_targets > best_targets) {
						best_targets = n_targets;
						best_pos = pos;
					}
				}
				
				if (best_pos != xy()) {
					if (current_frame - a->last_used_special >= 6 && a->u->game_unit->getSpellCooldown() <= latency_frames - 1) {
						if (a->u->game_unit->useTech(upgrade_types::psionic_storm->game_tech_type, BWAPI::Position(best_pos.x, best_pos.y))) {
							a->last_used_special = current_frame;
							a->u->controller->noorder_until = current_frame + 15;
							
							a->subaction = combat_unit::subaction_idle;
							a->u->controller->action = unit_controller::action_idle;
							
							if (unit_distance(a->u, best_pos) <= 32 * 9) {
								active_storms.emplace_back(current_frame + latency_frames + 4, best_pos);
							}
						}
					}
				}
			}
			
		} else if (a->u->type == unit_types::dark_archon && players::my_player->has_upgrade(upgrade_types::mind_control) && a->u->energy >= 150) {
			
			auto is_storm_at = [&](xy pos) {
				for (auto& v : active_storms) {
					if (max_distance(pos - v.second) <= 32 * 2) return true;
				}
				return false;
			};
			
			for (int i = 0; i != 2; ++i) {
				if (i == 1) {
					if (nearest_threat) {
						auto* nearest_threat_target = get_best_score(allies, [&](combat_unit* c) {
							auto* w = c->u->is_flying ? nearest_threat->stats->air_weapon : nearest_threat->stats->ground_weapon;
							if (!w) return std::numeric_limits<double>::infinity();
							return units_distance(nearest_threat, c->u) - w->max_range;
						}, std::numeric_limits<double>::infinity());
						if (nearest_threat_target && units_distance(nearest_threat, nearest_threat_target->u) >= units_distance(nearest_threat, a->u) - 32) continue;
					}
				}
				unit* target = nullptr;
				double target_value = 0.0;
				for (unit* e : enemies) {
					if (e->building || e->stasis_timer) continue;
					if (!e->detected) continue;
					double d = sc_distance(a->u->pos - e->pos);
					if (i == 0 && d > 32 * 10) continue;
					//xy pos = e->pos + xy((int)(e->hspeed*(latency_frames + 12)), (int)(e->vspeed*(latency_frames + 12)));
					xy pos = e->pos + xy((int)(e->hspeed*(latency_frames + 1)), (int)(e->vspeed*(latency_frames + 1)));
					
					if (disable_target_taken.find(e) != disable_target_taken.end()) continue;
					
					double value = e->type->total_minerals_cost + e->type->total_gas_cost;
					if (value < 200.0) continue;
					
					if (value > target_value) {
						target_value = value;
						target = e;
					}
				}
				
				if (target) {
					if (current_frame - a->last_used_special >= 6 && a->u->game_unit->getSpellCooldown() <= latency_frames - 1) {
						if (a->u->game_unit->useTech(upgrade_types::mind_control->game_tech_type, target->game_unit)) {
							a->last_used_special = current_frame;
							a->u->controller->noorder_until = current_frame + 15;
							
							a->subaction = combat_unit::subaction_idle;
							a->u->controller->action = unit_controller::action_idle;
							
							if (units_distance(a->u, target) <= 32 * 10) {
								combat::disable_target_taken[target] = std::make_tuple(a, current_frame + latency_frames + 10);
							}
						}
					}
				}
			}
			
		}
		
	}
	
}

bool careless_aggression = false;
void fight() {

	for (auto*c : live_combat_units) {
		if (c->action == combat_unit::action_tactic) continue;
		if (current_frame < c->strategy_busy_until) continue;
		if (c->reference_count) continue;
		if (c->u->type == unit_types::overlord) continue;
		if (c->u->type->is_worker && !c->u->force_combat_unit) {
			if (c->action != combat_unit::action_idle || c->subaction != combat_unit::subaction_idle && c->subaction != combat_unit::subaction_repair) {
				c->action = combat_unit::action_idle;
				c->subaction = combat_unit::subaction_idle;
				//c->u->controller->action = unit_controller::action_idle;
			}
		} else {
			if (c->action == combat_unit::action_offence) {
				c->action = combat_unit::action_idle;
				c->subaction = combat_unit::subaction_idle;
				//c->u->controller->action = unit_controller::action_idle;
			}
		}
	}

	for (auto&g : groups) {
		xy pos = g.enemies.front()->pos;
// 		combat_unit* leader = get_best_score(g.allies, [&](combat_unit* cu) {
// 			int lf = current_frame - cu->last_fight;
// 			if (lf <= 40) lf = 0;
// 			else lf = 40;
// 			return std::make_tuple(lf, -cu->u->type->gas_cost * 2 - cu->u->type->minerals_cost, cu->u->creation_frame);
// 		});;
		for (auto* cu : g.allies) {
			cu->goal_pos = pos;
			cu->action = combat_unit::action_offence;
			if (current_frame - cu->last_fight <= 40) continue;
			//log("move %s\n", cu->u->type->name);
			//if (cu->subaction != combat_unit::subaction_move) {
			if (!g.is_defensive_group || current_frame - cu->last_default_move >= 15 * 20) {

// 				if (diag_distance(cu->u->pos - leader->u->pos) >= g.allies.size() * 24) {
// 					pos = leader->u->pos;
// 				}

				cu->subaction = combat_unit::subaction_move;
				cu->target_pos = pos;
				cu->last_default_move = current_frame;
			}

			if (cu->assigned_control_spot) {
				cu->subaction = combat_unit::subaction_move;
				cu->target_pos = cu->assigned_control_spot->pos;
			}
			if (cu->assigned_defence_control_spot) {
				cu->subaction = combat_unit::subaction_move;
				cu->target_pos = cu->assigned_defence_control_spot->pos;
			}
		}
	}

	prepare_attack();
	prepare_run();

	if (current_minerals >= 4000 && current_used_total_supply >= (current_gas >= 1000 ? 80 : 180)) careless_aggression = true;
	if (current_minerals <= 2000) careless_aggression = false;

	for (auto* c : live_combat_units) {
		c->assigned_defence_control_spot = nullptr;
	}

	a_vector<unit*> nearby_enemies, nearby_allies;
	a_vector<combat_unit*> nearby_combat_units;
	std::lock_guard<multitasking::mutex> l(groups_mut);
	for (auto&g : groups) {

		if (g.is_defensive_group) {
			do_defence(g.allies);
			continue;
		}

		nearby_enemies.clear();
		nearby_combat_units.clear();
		nearby_allies.clear();

		nearby_enemies = g.enemies;
		for (auto i = nearby_enemies.begin(); i != nearby_enemies.end();) {
			if ((*i)->owner->rescuable) i = nearby_enemies.erase(i);
			else ++i;
		}
		for (auto*cu : g.allies) {
			double dist = get_best_score_value(nearby_enemies, [&](unit*e) {
				return diag_distance(e->pos - cu->u->pos);
			});
			if ((!cu->assigned_control_spot && dist <= 32 * 25) || dist <= 32 * 14) {
			//if (dist <= 32 * 12 || entire_threat_area.test(grid::build_square_index(cu->u->pos))) {
			//if (entire_threat_area.test(grid::build_square_index(cu->u->pos))) {
				nearby_combat_units.push_back(cu);
				if (current_frame >= cu->retreat_until) nearby_allies.push_back(cu->u);
			}
		}
//		for (unit* e : enemy_units) {
//			if (e->gone) continue;
//			if (e->stasis_timer || e->lockdown_timer || e->maelstrom_timer) continue;
//			if (!e->stats->air_weapon && !e->stats->ground_weapon) continue;
//			if (diag_distance(e->pos - nearby_enemies.front()->pos) <= 32 * 25) {
//				auto i = std::find(nearby_enemies.begin(), nearby_enemies.end(), e);
//				if (i == nearby_enemies.end()) {
//					double dist = get_best_score_value(nearby_combat_units, [&](auto* a) {
//						return diag_distance(e->pos - a->u->pos);
//					});
//					if (dist <= 32 * 20) {
//						nearby_enemies.push_back(e);
//					}
//				}
//			}
//		}
		double my_shield_battery_energy = 0.0;
		bool have_static_defence = false;
		bool static_defence_in_range = false;
		for (unit*u : my_buildings) {
			if (u->stats->ground_weapon || u->stats->air_weapon) {
				double dist = get_best_score_value(nearby_enemies, [&](unit*e) {
					return diag_distance(e->pos - u->pos);
				});
				if (dist <= 32 * 15) {
					bool can_hit_anything = false;
					for (unit*e : nearby_enemies) {
						weapon_stats*w = e->is_flying ? u->stats->air_weapon : u->stats->ground_weapon;
						if (w) {
							can_hit_anything = true;
							if (units_distance(u, e) <= w->max_range) {
								static_defence_in_range = true;
							}
							if (static_defence_in_range) break;
						}
					}
					have_static_defence = true;
					//nearby_allies.push_back(u);
				}
			}
		}
		if (static_defence_in_range) {
			for (unit*u : my_buildings) {
				if (u->stats->ground_weapon || u->stats->air_weapon) {
					double dist = get_best_score_value(nearby_enemies, [&](unit*e) {
						return diag_distance(e->pos - u->pos);
					});
					if (dist <= 32 * 15) {
						nearby_allies.push_back(u);
					}
				}
			}
		}
		for (unit*u : my_completed_units_of_type[unit_types::shield_battery]) {
			double dist = get_best_score_value(nearby_allies, [&](unit*n) {
				return diag_distance(n->pos - u->pos);
			});
			if (dist <= 32 * 9) {
				my_shield_battery_energy += u->energy;
			}
		}
		if (g.allies.size() >= 10) {
			for (unit* u : live_units) {
				if (u->gone) continue;
				if (diag_distance(u->pos - g.enemies.front()->pos) > 32 * 25) continue;
				if (u->building && !u->stats->air_weapon && !u->stats->ground_weapon) continue;
				if (u->type->is_non_usable) continue;
				if (u->owner == players::my_player) {
					if (std::find(nearby_allies.begin(), nearby_allies.end(), u) == nearby_allies.end()) {
						nearby_allies.push_back(u);
					}
				} else if (u->owner->is_enemy) {
					if (std::find(nearby_enemies.begin(), nearby_enemies.end(), u) == nearby_enemies.end()) {
						nearby_enemies.push_back(u);
					}
				}
			}
		}
		//log(log_level_info, "my_shield_battery_energy is %g\n", my_shield_battery_energy);
		if (!nearby_enemies.empty() && !nearby_allies.empty()) {

			bool is_base_defence = false;
			int n_enemy_lings = 0;
			bool is_only_melee = true;
			bool is_attacking_my_workers = false;
			for (unit*e : nearby_enemies) {
				e->temporary_combat_flag = true;
				if (e->type == unit_types::zergling) ++n_enemy_lings;
				if (!is_base_defence) is_base_defence = my_base.test(grid::build_square_index(e->pos));
				if (is_only_melee && ((e->is_flying && e->stats->ground_weapon) || (e->stats->ground_weapon && e->stats->ground_weapon->max_range > 32))) is_only_melee = false;
				if (!is_attacking_my_workers) {
					if (e->order_target && e->order_target->owner == players::my_player && e->order_target->type->is_worker) is_attacking_my_workers = true;
				}
			}

			bool is_mostly_lings = n_enemy_lings >= (int)nearby_enemies.size() - (int)nearby_enemies.size() / 3;

			bool has_siege_mode = players::my_player->upgrades.count(upgrade_types::siege_mode) != 0;
#ifndef USE_COMBAT_EVAL2
			auto add = [&](combat_eval::eval&eval, unit*u, int team) {
				//log("add %s to team %d\n", u->type->name, team);
				if (u->stasis_timer || u->maelstrom_timer || u->lockdown_timer) return;
				if (u->type == unit_types::bunker && u->is_completed) {
					for (int i = 0; i < u->marines_loaded; ++i) {
						eval.add_unit(units::get_unit_stats(unit_types::marine, u->owner), team);
					}
				}
				auto*st = u->stats;
				int cooldown_override = 0;
				if (!u->visible) cooldown_override = 0;
				if (u->type->requires_pylon && !u->is_powered) return;
				if (team == 0 && u->type == unit_types::siege_tank_tank_mode) {
					st = units::get_unit_stats(unit_types::siege_tank_siege_mode, u->owner);
				}
				if (team == 1 && u->type == unit_types::lurker && !u->burrowed) cooldown_override = 15 * 4;
				//if (!u->is_completed) cooldown_override = u->remaining_whatever_time;
				auto&c = eval.add_unit(st, team);
// 				c.move = get_best_score_value(team ? nearby_allies : nearby_enemies, [&](unit*e) {
// 					if (e->type->is_flyer && !c.st->air_weapon) return std::numeric_limits<double>::infinity();
// 					double d = units_distance(e, u);
// 					if (u->is_flying) return d;
// 					weapon_stats*w = e->is_flying ? u->stats->air_weapon : u->stats->ground_weapon;
// 					if (w && d <= w->max_range) return d;
// 					return units_pathing_distance(e, u);
// 				});
//				multitasking::yield_point();
				if (c.move == std::numeric_limits<double>::infinity()) c.move = 32 * 8;
				eval.set_unit_stuff(c, u);
				if (cooldown_override > c.cooldown) c.cooldown = cooldown_override;
				//log(log_level_info, "added %s to team %d -- move %g, shields %g, hp %g, cooldown %d\n", st->type->name, team, c.move, c.shields, c.hp, c.cooldown);
			};
			int eval_frames = 15 * 20;
			//int eval_frames = 15 * 40;
			//int eval_frames = 15 * 90;
			combat_eval::eval eval;
			eval.teams[0].shield_battery_energy = my_shield_battery_energy;
			eval.teams[1].wait_until_attacked = true;
			eval.max_frames = eval_frames;
			int worker_count = 0;
			for (unit*a : nearby_allies) {
				if (a->type->is_worker && !a->force_combat_unit) {
					++worker_count;
					continue;
				}
				add(eval, a, 0);
			}
			for (unit*e : nearby_enemies) {
				if (e->type->is_worker && current_frame - e->last_attacked > 15 * 4) continue;
				//if (e->building && e->remaining_build_time >= 15 * 1) continue;
				if (e->building && !e->is_completed) continue;
				add(eval, e, 1);
			}
			eval.run();

			if (nearby_allies.size() > 1 && eval.teams[0].end_hp > 0.0 && eval.teams[1].score >= 25.0 && !careless_aggression) {
				if (nearby_allies.size() < 12 && false) {
					combat_eval::eval skip_lowest_eval;
					combat_unit*lowest_hp = get_best_score(nearby_combat_units, [&](combat_unit*a) {
						if (current_frame < a->retreat_until) return std::numeric_limits<double>::infinity();
						if (!a->u->stats->ground_weapon && !a->u->stats->air_weapon) return std::numeric_limits<double>::infinity();
						if (a->u->type == unit_types::zergling && is_base_defence && is_mostly_lings) return std::numeric_limits<double>::infinity();
						for (unit*e : nearby_enemies) {
							weapon_stats*ew = a->u->is_flying ? e->stats->air_weapon : e->stats->ground_weapon;
							if (ew && units_distance(e, a->u) <= ew->max_range - 32) return std::numeric_limits<double>::infinity();
						}
						return a->u->shields + a->u->hp;
					}, std::numeric_limits<double>::infinity());
					if (lowest_hp) {
						skip_lowest_eval.teams[0].shield_battery_energy = my_shield_battery_energy;
						skip_lowest_eval.teams[1].wait_until_attacked = true;
						skip_lowest_eval.max_frames = eval_frames;
						for (unit*a : nearby_allies) {
							if (a == lowest_hp->u) continue;
							if (a->type->is_worker && !a->force_combat_unit) continue;
							add(skip_lowest_eval, a, 0);
						}
						for (unit*e : nearby_enemies) {
							if (e->type->is_worker && current_frame - e->last_attacked > 15 * 4) continue;
							//if (e->building && e->remaining_build_time >= 15 * 1) continue;
							if (e->building && !e->is_completed) continue;
							add(skip_lowest_eval, e, 1);
						}
						skip_lowest_eval.run();
						//if (skip_lowest_eval.teams[0].score > eval.teams[0].score || skip_lowest_eval.teams[1].score < eval.teams[1].score) {
						//	if (skip_lowest_eval.teams[0].score >= eval.teams[0].score*0.75) {
						//log(log_level_info, "skip lowest? team 1 score %g -> %g\n", eval.teams[1].score, skip_lowest_eval.teams[1].score);
						//if (skip_lowest_eval.teams[0].end_hp > 0.0 && skip_lowest_eval.teams[1].score <= eval.teams[1].score) {
						//if (skip_lowest_eval.teams[0].score >= eval.teams[0].score * 0.75 && skip_lowest_eval.teams[1].score < eval.teams[1].score) {
						if (skip_lowest_eval.teams[0].score >= eval.teams[0].score && skip_lowest_eval.teams[1].score < eval.teams[1].score) {
							if (true) {
								//log(log_level_info, "skip lowest! team 1 score %g -> %g\n", eval.teams[1].score, skip_lowest_eval.teams[1].score);
								eval = skip_lowest_eval;
								lowest_hp->try_retreat_until = current_frame + 15 * 4;
							}
						}
					}
				}
			}

			bool run_with_workers = true;
			if ((eval.teams[0].score < eval.teams[1].score || eval.teams[0].units.empty()) && worker_count) {
				combat_eval::eval use_workers_eval;
				eval.teams[0].shield_battery_energy = my_shield_battery_energy;
				eval.teams[1].wait_until_attacked = true;
				use_workers_eval.max_frames = eval_frames;
				for (unit*a : nearby_allies) {
					add(use_workers_eval, a, 0);
				}
// 				int zealots = 0;
				for (unit*e : nearby_enemies) {
					//if (e->type == unit_types::zealot) ++zealots;
					if (e->type->is_worker && current_frame - e->last_attacked > 15 * 4) continue;
					//if (e->building && e->remaining_build_time >= 15 * 1) continue;
					if (e->building && !e->is_completed) continue;
					add(use_workers_eval, e, 1);
				}
				use_workers_eval.run();
				//if (use_workers_eval.teams[0].score > eval.teams[0].score && zealots < 2) {
				if (use_workers_eval.teams[0].score > eval.teams[0].score && (players::my_player->race != race_protoss || eval.teams[0].start_supply < 4.0)) {
					log("use workers! (%g vs %g > %g vs %g)\n", use_workers_eval.teams[0].score, use_workers_eval.teams[1].score, eval.teams[0].score, eval.teams[1].score);
					eval = use_workers_eval;
					run_with_workers = false;
				} else log("using workers is not beneficial (%g vs %g <= %g vs %g)\n", use_workers_eval.teams[0].score, use_workers_eval.teams[1].score, eval.teams[0].score, eval.teams[1].score);
			} else if (worker_count) log("don't have to use workers\n");

			double mult = 2.0;
			if (is_base_defence || !g.is_aggressive_group) {
				if (current_frame < 15 * 60 * 10) mult = 0.5;
				else mult = 1.0;
			}
			if (current_frame < combat_mult_override_until) mult = combat_mult_override;
			log("mult is %g\n", mult);

			for (unit* u : nearby_allies) {
				if (u->type == unit_types::defiler) {
					if (u->energy >= 100) {
						mult *= 0.75;
					}
				}
			}

// 			combat_eval::eval ground_eval;
// 			ground_eval.max_frames = eval_frames;
// 			for (unit*a : nearby_allies) {
// 				if (a->type->is_worker) continue;
// 				if (!a->is_flying) add(ground_eval, a, 0);
// 			}
// 			for (unit*e : nearby_enemies) add(ground_eval, e, 1);
// 			ground_eval.run();
// 
// 			bool ground_fight = ground_eval.teams[0].score > ground_eval.teams[1].score * mult;
// 			//bool ground_fight = ground_eval.teams[0].end_supply > ground_eval.teams[1].end_supply;

// 			combat_eval::eval air_eval;
// 			air_eval.max_frames = eval_frames;
// 			for (unit*a : nearby_allies) {
// 				if (a->type->is_worker) continue;
// 				if (a->is_flying) add(air_eval, a, 0);
// 			}
// 			for (unit*e : nearby_enemies) add(air_eval, e, 1);
// 			air_eval.run();
// 
// 			bool air_fight = air_eval.teams[0].score > air_eval.teams[1].score * mult;
// 			//bool air_fight = air_eval.teams[0].end_supply > air_eval.teams[1].end_supply;

			bool ground_fight = false;
			bool air_fight = false;

			if (false) {
				a_map<unit_type*, int> my_count, op_count;
				for (unit*a : nearby_allies) ++my_count[a->type];
				for (unit*e : nearby_enemies) ++op_count[e->type];
				log(log_level_test, "combat::\n");
				log(log_level_test, "my units -");
				for (auto&v : my_count) log(log_level_test, " %dx%s", v.second, short_type_name(v.first));
				log(log_level_test, "\n");
				log(log_level_test, "op units -");
				for (auto&v : op_count) log(log_level_test, " %dx%s", v.second, short_type_name(v.first));
				log(log_level_test, "\n");

				log(log_level_test, "result: score %g %g  supply %g %g  damage %g %g  in %d frames\n", eval.teams[0].score, eval.teams[1].score, eval.teams[0].end_supply, eval.teams[1].end_supply, eval.teams[0].damage_dealt, eval.teams[1].damage_dealt, eval.total_frames);
				//log(" --- %g / %g = %g\n", eval.teams[0].end_hp, eval.teams[0].start_hp, eval.teams[0].end_hp / eval.teams[0].start_hp);
			}

// 			double fact = 1.0;

//			bool already_fighting = test_pred(nearby_combat_units, [&](combat_unit*cu) {
//				return current_frame - cu->last_fight <= 60 && cu->last_fight - cu->last_run > 0;
//			});
//			if (already_fighting) mult *= 0.5;

//			size_t n_in_threat_area = 0;
//			for (auto* a : nearby_combat_units) {
//				if (entire_threat_area.test(grid::build_square_index(a->u->pos))) {
//					++n_in_threat_area;
//				}
//			}
//			if (n_in_threat_area >= nearby_combat_units.size() / 2) {
//				if (mult > 1.0) mult = 1.0;
//			}
//			if (n_in_threat_area >= nearby_combat_units.size() - nearby_combat_units.size() / 4) {
//				if (mult > 0.75) mult = 0.75;
//			}

// 			if (already_fighting) fact = 0.5;
// 			else if (current_used_total_supply >= 20) fact = 1.5;
 
// 			if (players::my_player->has_upgrade(upgrade_types::dark_swarm) && players::my_player->has_upgrade(upgrade_types::consume)) {
// 				bool has_defilers = false;
// 				for (auto*a : nearby_combat_units) {
// 					if (a->u->type == unit_types::defiler) {
// 						has_defilers = true;
// 						break;
// 					}
// 				}
// 				if (has_defilers) fact *= 0.75;
// 			}

			ground_fight = false;
			air_fight = false;
			
			size_t n_fight = 0;
			for (auto* a : nearby_combat_units) {
				if (a->openbw_fight) ++n_fight;
			}

			//log(log_level_test, "n_fight is %d/%d\n", n_fight, nearby_combat_units.size());	
			
			//bool fight = n_fight > nearby_combat_units.size() / 2;

			bool fight = eval.teams[0].score >= eval.teams[1].score * mult;
			if (fight) log(" fight because score %g >= %g * %g\n", eval.teams[0].score, eval.teams[1].score, mult);
			//if (is_base_defence && worker_count) fight |= eval.teams[0].end_supply >= eval.teams[1].end_supply;
			if (is_base_defence) {
				fight |= eval.teams[0].end_supply >= eval.teams[1].end_supply;
				if (fight) log(" fight because end supply %g >= %g\n", eval.teams[0].end_supply, eval.teams[1].end_supply);
			}
			if (eval.teams[1].end_supply == 0) {
				log(" fight because win! :D\n");
				fight = true;
			}
			
			
//			if (eval.teams[0].start_supply >= 16.0 && eval.teams[1].end_supply <= std::max(eval.teams[1].start_supply - 16.0, 0.0)) {
//				fight = true;
//				log("fight because we killed some supply. yey\n");
//			}

			//log(log_level_test, " initial fight is %d\n", fight);

			if (!g.is_aggressive_group && current_used_total_supply - my_workers.size() < 10.0 && players::my_player->race == race_terran) {
				if (is_base_defence) {
					//run_with_workers = false;
					//fight = true;
				}
				//log(log_level_test, " fight because 0\n");
			}

			if (air_fight || ground_fight) fight = false;
			else {
				if (fight) log("fight!\n");
				else log("run!\n");
			}
			if (ground_fight) log("ground fight!\n");
			if (air_fight) log("air fight!\n");

// 			if (already_fighting && !fight && !ground_fight && !air_fight) {
// 				//if (eval.teams[0].score >= eval.teams[1].score || eval.teams[0].end_supply >= eval.teams[1].end_supply) {
// 				if (eval.teams[0].damage_dealt >= eval.teams[1].damage_dealt) {
// 					log("already fighting, fight!\n");
// 					fight = true;
// 				}
// 			}


// 			if (!fight && !ground_fight && !air_fight) {
// 				combat_eval::eval run_eval;
// 				run_eval.max_frames = eval_frames;
// 				for (unit*a : nearby_allies) {
// // 					if (op_detectors == 0 && players::my_player->has_upgrade(upgrade_types::burrow)) {
// // 						if (a->type == unit_types::drone || a->type == unit_types::zergling || a->type == unit_types::hydralisk || a->type == unit_types::defiler) {
// // 							continue;
// // 						}
// // 					}
// 					add(run_eval, a, 0);
// 				}
// 				for (unit*e : nearby_enemies) add(run_eval, e, 1);
// 				run_eval.teams[0].run = true;
// 				run_eval.run();
// 
// 				log("run_eval: supply %g %g  damage %g %g  in %d frames\n", run_eval.teams[0].end_supply, run_eval.teams[1].end_supply, run_eval.teams[0].damage_dealt, run_eval.teams[1].damage_dealt, run_eval.total_frames);
// 				//if (run_eval.teams[0].damage_taken >= eval.teams[0].damage_taken*0.75) {
// // 				double fight_diff = eval.teams[1].score - eval.teams[0].score;
// // 				double run_diff = run_eval.teams[1].score - run_eval.teams[0].score;
// // 				if (run_diff > fight_diff) {
// // 					log("would take too much damage from running, fight instead!\n");
// // 					fight = true;
// // 				}
// 				if (run_eval.teams[0].end_supply <= eval.teams[0].end_supply) {
// 					log("would take too much damage from running, fight instead!\n");
// 					fight = true;
// 				}
// 			}

// 			unit*defensive_matrix_target = nullptr;
// 			if (!fight && !ground_fight && !air_fight) {
// 				bool has_defensive_matrix = false;
// 				for (unit*a : nearby_allies) {
// 					if (a->type == unit_types::science_vessel && a->energy >= 100) {
// 						has_defensive_matrix = true;
// 						break;
// 					}
// 				}
// 				if (has_defensive_matrix) {
// 					combat_eval::eval sp_eval;
// 					sp_eval.max_frames = eval_frames;
// 					double lowest_move = std::numeric_limits<double>::infinity();
// 					unit*target = nullptr;
// 					size_t target_idx = 0;
// 					size_t idx = 0;
// 					for (unit*a : nearby_allies) {
// 						auto&c = add(sp_eval, a, 0);
// 						if (c.move < lowest_move) {
// 							lowest_move = c.move;
// 							target = a;
// 							target_idx = idx;
// 						}
// 						++idx;
// 					}
// 					for (unit*e : nearby_enemies) add(sp_eval, e, 1);
// 					if (target) {
// 						sp_eval.teams[0].units[target_idx].hp += 250;
// 						sp_eval.run();
// 
// 						log("defensive matrix result: supply %g %g  damage %g %g  in %d frames\n", sp_eval.teams[0].end_supply, sp_eval.teams[1].end_supply, sp_eval.teams[0].damage_dealt, sp_eval.teams[1].damage_dealt, sp_eval.total_frames);
// 
// 						bool fight = sp_eval.teams[0].score >= sp_eval.teams[1].score;
// 						if (fight) {
// 							defensive_matrix_target = target;
// 						}
// 					}
// 				}
// 			}
// 
// 			unit*defensive_matrix_vessel = nullptr;
// 			if (defensive_matrix_target) {
// 				defensive_matrix_vessel = get_best_score(nearby_allies, [&](unit*u) {
// 					if (u->type != unit_types::science_vessel || u->energy < 100) return std::numeric_limits<double>::infinity();
// 					return diag_distance(defensive_matrix_target->pos - u->pos);
// 				}, std::numeric_limits<double>::infinity());
// 			}

#else

			bool fight = g.fight;
			bool ground_fight = false;
			bool air_fight = false;
			bool run_with_workers = false;

#endif

			a_unordered_set<combat_unit*> processed;

			bool use_worker_micro = false;
			bool use_worker_micro_skip_workers = false;
			bool use_protect_marines_micro = false;
			//log(log_level_test, "use_worker_micro?\n");
			if (!g.is_aggressive_group && (my_resource_depots.size() < 2 || my_workers.size() >= current_used_total_supply - 4) && my_units_of_type[unit_types::bunker].empty()) {
			//if (!g.is_aggressive_group && my_workers.size() >= current_used_total_supply - 5 && nearby_enemies.size() > 1 && players::my_player->race == race_terran) {
				//if (g.fight_my_resources_lost >= g.fight_op_resources_lost / 2) {
				if (players::my_player->race != race_zerg) {
					fight = true;
					//log(log_level_test, " fight because 1\n");
					//log(log_level_test, "is_only_melee is %d\n", is_only_melee);
					if (is_only_melee) {
						int worker_count = 0;
						int non_worker_count = 0;
						int marine_count = 0;
						int non_marine_count = 0;
						for (unit* a : nearby_allies) {
							if (a->type->is_worker && !a->force_combat_unit) ++worker_count;
							else {
								++non_worker_count;
								if (a->type == unit_types::marine) ++marine_count;
								else ++non_marine_count;
							}
						}
						int enemy_melee_units = 0;
						int enemy_non_melee_units = 0;
						for (unit* a : nearby_enemies) {
							if (a->stats->ground_weapon && a->stats->ground_weapon->max_range <= 32) ++enemy_melee_units;
							else if (a->stats->ground_weapon) ++enemy_non_melee_units;
						}
						//log(log_level_test, "worker_count %d, non_worker_count %d\n", worker_count, non_worker_count);
						if (marine_count && non_marine_count == 0 && non_worker_count < 4 && enemy_melee_units && enemy_non_melee_units == 0) {
							//printf("protect marines!\n");
							use_protect_marines_micro = true;
							run_with_workers = false;
						} else {
							bool any_damaged_buildings = false;
							for (unit*u : my_buildings) {
								if (!u->is_completed) continue;
								if (u->shields + u->hp < (u->stats->shields + u->stats->hp) * 0.75) {
									any_damaged_buildings = true;
									break;
								}
							}
							//if (!any_damaged_buildings && non_worker_count < 3) {
							if (!any_damaged_buildings && non_worker_count < 8) {
								//log(log_level_test, "use worker micro!\n");
								use_worker_micro = true;
							}
							bool is_attacking = false;
							for (unit*e : g.enemies) {
								if (current_frame - e->last_attacked <= 15 * 5 && (!e->last_attack_target || !e->last_attack_target->building)) {
									is_attacking = true;
									break;
								}
							}
							//log(log_level_test, "is_attacking is %d\n", is_attacking);
							if (!is_attacking) use_worker_micro_skip_workers = true;
							if (non_worker_count < 6 && enemy_melee_units && enemy_non_melee_units == 0 && marine_count && non_marine_count == 0) {
								use_worker_micro = false;
								use_worker_micro_skip_workers = false;
								fight = true;
								//log(log_level_test, " fight because 2\n");
								for (unit* e : nearby_enemies) {
									if (e->is_flying) continue;
									int n_workers = 2;
									if (e->type == unit_types::zealot) ++n_workers;
									for (int i = 0; i < n_workers; ++i) {
										combat_unit* a = get_best_score(nearby_combat_units, [&](combat_unit* a) {
											if (!a->u->type->is_worker) return std::numeric_limits<double>::infinity();
											if (a->u->hp < a->u->stats->hp) return std::numeric_limits<double>::infinity();
											double d = diag_distance(a->u->pos - e->pos);
											if (d >= 32 * 10) return std::numeric_limits<double>::infinity();
											if (processed.count(a)) return std::numeric_limits<double>::infinity();
											return d;
										}, std::numeric_limits<double>::infinity());
										if (a) {
											processed.insert(a);
											a->subaction = combat_unit::subaction_fight;
											a->target = e;
										}
									}
								}
							}
						}
					}
				}
			}
			//use_worker_micro = is_only_melee && is_attacking_my_workers;
			
//			if (current_frame <= 16 * 60 * 7) {
//				run_with_workers = false;
//				fight = true;
//			}
			
			if (current_frame <= 15 * 60 * 15 && is_only_melee) {
				a_unordered_set<combat_unit*> keep_worker;
				for (unit* e : nearby_enemies) {
					if (e->is_flying) continue;
					int n_workers = 2;
					if (e->type == unit_types::zealot) ++n_workers;
					for (int i = 0; i < n_workers; ++i) {
						combat_unit* a = get_best_score(nearby_combat_units, [&](combat_unit* a) {
							if (!a->u->type->is_worker) return std::numeric_limits<double>::infinity();
							double d = diag_distance(a->u->pos - e->pos);
							if (d >= 32 * 10) return std::numeric_limits<double>::infinity();
							if (keep_worker.count(a)) return std::numeric_limits<double>::infinity();
							return d;
						}, std::numeric_limits<double>::infinity());
						if (a) {
							keep_worker.insert(a);
						}
					}
				}
				for (auto* a : nearby_combat_units) {
					if (a->u->type->is_worker && !keep_worker.count(a)) {
						processed.insert(a);
						a->subaction = combat_unit::subaction_idle;
					}
				}
			}

			for (unit*e : enemy_units) {
				if (!e->visible) continue;
				if (e->type != unit_types::spider_mine) continue;
				if (e->temporary_combat_flag) continue;
				if (diag_distance(e->pos - g.enemies.front()->pos) <= 32 * 15) {
					e->temporary_combat_flag = true;
					nearby_enemies.push_back(e);
				}
			}

			bool op_is_all_lings = true;
			int op_detectors = 0;
			double op_supply = 0.0;
			for (unit*u : nearby_enemies) {
				if (u->type->is_detector) ++op_detectors;
				if (op_is_all_lings && u->type != unit_types::zergling) op_is_all_lings = false;
				op_supply += u->type->required_supply;
			}

			double win_ratio = eval.teams[0].score / eval.teams[1].score;
			//double win_ratio = 1.0;

			if (win_ratio >= 256.0) {
				for (unit* e : enemy_buildings) {
					if (e->gone) continue;
					if (e->temporary_combat_flag) continue;
					if (diag_distance(e->pos - g.enemies.front()->pos) <= 32 * 15) {
						e->temporary_combat_flag = true;
						nearby_enemies.push_back(e);
					}
				}
			}
			
			use_worker_micro = false;
			//if (is_base_defence && (win_ratio >= 16.0 || (g.enemies.size() == 1 && g.enemies.front()->type->is_worker))) {
			if (is_base_defence && g.enemies.size() == 1 && g.enemies.front()->type->is_worker) {
				use_worker_micro = false;
				use_protect_marines_micro = false;
			}

			auto is_outranged_or_not_focused = [&]() {
				for (unit*e : nearby_enemies) {
					if (!e->stats->ground_weapon) continue;
					if (e->is_flying) return true;
					if (current_frame - e->last_attacked <= 15 * 2) {
						unit*t = e->last_attack_target;
						if (t && t->owner == players::my_player) {
							bool found = false;
							for (unit*u : nearby_allies) {
								if (u == t) {
									found = true;
									break;
								}
							}
							if (!found) return true;
							weapon_stats*tw = e->is_flying ? t->stats->air_weapon : t->stats->ground_weapon;
							weapon_stats*ew = t->is_flying ? e->stats->air_weapon : e->stats->ground_weapon;
							if (ew) {
								if (!tw) return true;
								if (ew->max_range > tw->max_range) return true;
								if (e->type == unit_types::zealot && units_distance(e, t) <= ew->max_range + 64) return true;
							}
						}
					}
				}
				return false;
			};
			if (current_used_total_supply < 50 && !g.is_aggressive_group && no_aggressive_groups && !is_outranged_or_not_focused()) {

				bool is_defensive_concave = false;
				a_vector<xy> defensive_positions;
				get_defensive_positions(nearby_combat_units, defensive_positions, is_defensive_concave);
				bool do_it = false;
				log("defensive_positions.size() is %d\n", defensive_positions.size());
				if (is_defensive_concave && !defensive_positions.empty()) {
					int n_near = 0;
					bool is_near_formation = false;
					for (unit*u : nearby_allies) {
						double d = get_best_score_value(defensive_positions, [&](xy pos) {
							return diag_distance(pos - u->pos);
						});
						//log("%s: d is %g\n", u->type->name, d);
						if (d <= 32 * 3) {
							++n_near;
							if (n_near >= (int)nearby_allies.size() / 3) {
								is_near_formation = true;
								break;
							}
						}
					}
					log("n_near is %d (nearby_allies.size() is %d)\n", n_near, nearby_allies.size());
					if (is_near_formation) do_it = true;
				}
				if (do_it) {
					tsc::dynamic_bitset inside(grid::build_grid_width*grid::build_grid_height);
					for (xy pos : defensive_positions) inside.set(grid::build_square_index(pos));
					bool any_inside = false;
					for (auto*e : nearby_enemies) {
						if (!e->stats->ground_weapon) continue;
						if (inside.test(grid::build_square_index(e->pos))) {
							any_inside = true;
							break;
						}
					}
					if (any_inside) do_it = false;
					log("any_inside is %d\n", any_inside);
				}

				log("do_it is %d\n", do_it);
				if (do_it) {
					a_vector<std::tuple<xy, int>> defence_position_count;
					for (xy pos : defensive_positions) {
						defence_position_count.emplace_back(pos + xy(16, 16), 0);
					}
					for (auto*a : nearby_combat_units) {
						auto*v = get_best_score_p(defence_position_count, [&](std::tuple<xy, int>*v) {
							return std::make_tuple(std::get<1>(*v), diag_distance(std::get<0>(*v) - a->u->pos));
						});
						++std::get<1>(*v);
						a->u->controller->defensive_concave_position = square_pathing::get_pos_in_square(std::get<0>(*v), a->u->type);
					}
					fight = true;
					//log(log_level_test, " fight because 3\n");
				} else {
					for (auto*a : nearby_combat_units) {
						a->u->controller->defensive_concave_position = xy();
					}
				}

			} else {
				for (auto*a : nearby_combat_units) {
					a->u->controller->defensive_concave_position = xy();
				}
			}
			
			//if (players::my_player->race == race_zerg && current_used_total_supply - my_workers.size() < 4.0) {
//			if (players::my_player->race == race_zerg && current_frame < 15 * 60 * 10) {
//				if (!is_attacking_my_workers) run_with_workers = true;
//			}

// 			int ling_group_count = 0;
// 			combat_unit*prev_ling = nullptr;

			int run_until = 0;
			for (auto*a : nearby_combat_units) {
				if (current_frame < a->run_until) {
					if (a->run_until > run_until) run_until = a->run_until;
					fight = false;
					if (a->u->is_flying) air_fight = false;
					else ground_fight = false;
				}
			}

			combat_unit* nearest_combat_unit = get_best_score(nearby_combat_units, [&](combat_unit*a) {
				return diag_distance(a->u->pos - g.enemies.front()->pos);
			});
			combat_unit* nearest_weak_combat_unit = get_best_score(nearby_combat_units, [&](combat_unit*a) {
				if (a->u->hp >= a->u->stats->hp / 2) return std::numeric_limits<double>::infinity();
				return diag_distance(a->u->pos - g.enemies.front()->pos);
			}, std::numeric_limits<double>::infinity());

			int num_in_enemy_tank_range = 0;
			for (auto* a : nearby_combat_units) {
				if (enemy_siege_tank_area[grid::build_square_index(a->u->pos)] >= 2) {
					++num_in_enemy_tank_range;
				}
			}

			if (!g.is_aggressive_group && current_used_total_supply < 60 && false) {
				if (!is_base_defence && !is_attacking_my_workers) {
					if (win_ratio < 4.0) {
						size_t my_inside_count = 0;
						size_t op_inside_count = 0;
						size_t op_outside_count = 0;
						for (auto* a : nearby_combat_units) {
							bool is_inside = false;
							for (auto* bs : defence_choke.inside_squares) {
								if (diag_distance(a->u->pos - bs->pos + xy(16, 16)) <= 32 * 2) {
									is_inside = true;
									break;
								}
							}
							if (is_inside) ++my_inside_count;
						}
						for (auto* e : nearby_enemies) {
							bool is_inside = false;
							bool is_outside = false;
							for (auto* bs : defence_choke.inside_squares) {
								if (diag_distance(e->pos - bs->pos + xy(16, 16)) <= 32 * 2) {
									is_inside = true;
									break;
								}
							}
							for (auto* bs : defence_choke.outside_squares) {
								if (diag_distance(e->pos - bs->pos + xy(16, 16)) <= 32 * 2) {
									is_outside = true;
									break;
								}
							}
							if (is_inside) ++op_inside_count;
							if (is_outside) ++op_outside_count;
						}
						if (my_inside_count >= nearby_combat_units.size() / 2 && op_outside_count >= nearby_enemies.size() / 2) {
							if (op_inside_count == 0) {
								fight = false;
							}
						}
					}
				}
			}

			int my_tanks = 0;
			for (unit* u : nearby_allies) {
				if (u->type == unit_types::siege_tank_tank_mode || u->type == unit_types::siege_tank_siege_mode) ++my_tanks;
			}
			int op_tanks = 0;
			for (unit* u : nearby_enemies) {
				if (u->type == unit_types::siege_tank_tank_mode || u->type == unit_types::siege_tank_siege_mode) ++op_tanks;
			}
			if (!g.is_aggressive_group) {
				if (my_tanks >= 5) {
					fight = true;
					//log(log_level_test, " fight because 4\n");
				}
			}
			
//			int dragoons_plus_zealots = 0;
//			for (unit* u : nearby_allies) {
//				if (u->type == unit_types::zealot || u->type == unit_types::dragoon) {
//					++dragoons_plus_zealots;
//				}
//			}
//			if (dragoons_plus_zealots >= 20) fight = true;

			if (use_protect_marines_micro) {
				xy pos = nearby_combat_units[0]->u->pos;
				int adds = 0;
				for (auto* a : live_combat_units) {
					if (nearby_combat_units.size() >= 8) break;
					if (!a->u->type->is_worker) continue;
					if (diag_distance(a->u->pos - pos) <= 32 * 10) {
						bool found = false;
						for (auto* a2 : nearby_combat_units) {
							if (a == a2) {
								found = true;
								break;
							}
						}
						if (!found) {
							nearby_combat_units.push_back(a);
							++adds;
						}
					}
				}
				//printf("added %d workers\n", adds);
			}

// 			printf("use_protect_marines_micro: %d\n", use_protect_marines_micro);
// 			for (auto* a : nearby_combat_units) {
// 				printf(" - %s\n", a->u->type->name.c_str());
// 			}

			log(log_level_info, " final fight is %d\n", fight);
			
			//log(log_level_test, "use_worker_micro is %d\n", use_worker_micro);
			//log(log_level_test, "run_with_workers is %d\n", run_with_workers);
			//log(log_level_test, "use_worker_micro_skip_workers is %d\n", use_worker_micro_skip_workers);
			
			bool any_attacking = false;
			
			bool actually_attack = current_frame <= 15 * 60 * 10;
			for (auto* a : nearby_combat_units) {
				if (current_frame - a->last_attack_frame >= 120) a->attack_counter = 0;
				if (a->attack_counter >= 5) actually_attack = true;
			}
			
			if (!any_attacking) {
				for (auto* a : nearby_combat_units) {
					if (current_frame - a->u->last_attacked <= 120) any_attacking = true;
				}
				if (!any_attacking) {
					for (unit* u : nearby_enemies) {
						if (current_frame - u->last_attacked <= 120) any_attacking = true;
					}
				}
			}
			
			bool tank_line = false;
			if (my_tanks && op_tanks && players::my_player->has_upgrade(upgrade_types::siege_mode)) {
				if (win_ratio < 8.0) {
					tank_line = true;
//					if (!fight || op_tanks >= 4) {
//						if (my_tanks >= nearby_allies.size() / 3) {
//							tank_line = true;
//						}
//					}
				}
			}
			if (my_tanks && !g.is_aggressive_group && no_aggressive_groups) {
				//if (win_ratio < 8.0) tank_line = true;
			}
			
			bool last_attack_fight = false;
			if (use_decisive_engagements) {
				for (auto* a : nearby_combat_units) {
					if (a->u->is_flying) continue;
					if (current_frame - a->last_fight <= 60 && current_frame - a->last_run > 60) {
						if (current_frame - a->u->last_attacked <= 60 || current_frame - a->u->last_lost_hp <= 60 || enemy_siege_tank_area[grid::build_square_index(a->u->pos)]) {
							if (op_supply <= a->last_attack_enemy_army_supply * 1.25) last_attack_fight = true;
							if (win_ratio >= a->last_attack_win_ratio * 0.5 || eval.teams[0].end_supply) last_attack_fight = true;
						}
					}
				}
			}
			
//			if (fight && !g.is_aggressive_group && current_frame < 15 * 60 * 15 && win_ratio < (no_aggressive_groups ? 32.0 : 4.0)) {
//				if (!have_static_defence || !static_defence_in_range) {
					
//					bool any_in_range = false;
//					for (unit* e : nearby_enemies) {
//						if (e->type == unit_types::overlord) continue;
//						if (my_base.test(grid::build_square_index(e->pos))) {
//							any_in_range = true;
//							break;
//						}
//						if (!have_static_defence && !is_in_defence_choke_outside_squares(e->pos)) any_in_range = true;
//						weapon_stats* w = e->stats->ground_weapon;
//						if (!w) continue;
//						for (unit* u : my_buildings) {
//							if (u->is_flying) continue;
//							if (units_distance(e, u) <= w->max_range) {
//								any_in_range = true;
//								break;
//							}
//						}
//						if (!any_in_range) {
//							for (unit* u : my_workers) {
//								if (u->is_flying) continue;
//								if (u->controller->action == unit_controller::action_scout) continue;
//								if (units_distance(e, u) <= w->max_range) {
//									any_in_range = true;
//									break;
//								}
//							}
//						}
//					}
					
//					if (!any_in_range) fight = false;
//				}
				
//			}
			
//			if (!fight) {
//				double my_supply = 0.0;
//				double op_supply = 0.0;
//				for (unit* u : nearby_allies) {
//					my_supply += u->type->required_supply;
//				}
//				for (unit* u : nearby_enemies) {
//					op_supply += u->type->required_supply;
//				}
//				if (my_supply >= 30.0 && my_supply >= op_supply) fight = true;
//			}
			
			if (fight && defend_in_control_spots) {
				int n_allies = 0;
				a_unordered_set<control_spot*> nearby_control_spots;
				for (unit* u : nearby_allies) {
					size_t index = grid::build_square_index(u->pos);
					for (auto* cs : active_control_spots) {
						if (cs->area.test(index)) {
							nearby_control_spots.emplace(cs);
							++n_allies;
							break;
						}
					}
				}
				if (n_allies >= nearby_allies.size() / 2) {
					a_vector<unit*> inside_enemies;
					for (unit* e : nearby_enemies) {
						size_t index = grid::build_square_index(e->pos);
						bool inside = false;
						for (auto* cs : nearby_control_spots) {
							if (cs->area.test(index)) {
								inside = true;
								break;
							}
						}
						if (!inside) {
							for (unit* u : my_buildings) {
								auto* w = u->is_flying ? e->stats->air_weapon : e->stats->ground_weapon;
								if (w && units_distance(e, u) <= w->max_range + 32 * 4) {
									inside = true;
									break;
								}
							}
							if (!inside) {
								for (unit* u : my_workers) {
									auto* w = u->is_flying ? e->stats->air_weapon : e->stats->ground_weapon;
									if (w && units_distance(e, u) <= w->max_range + 32 * 4) {
										inside = true;
										break;
									}
								}
							}
						}
						if (inside) {
							inside_enemies.push_back(e);
						}
					}
					if (inside_enemies.empty()) {
						fight = false;
						for (auto* a : nearby_combat_units) {
							auto* cs = get_best_score(nearby_control_spots, [&](auto* cs) {
								return diag_distance(a->u->pos - cs->pos);
							});
							if (cs) {
								processed.insert(a);
								a->subaction = combat_unit::subaction_move;
								a->target_pos = cs->pos;
							}
						}
					} else nearby_enemies = inside_enemies;
				}
			}

			int workers_attacking_count = 0;
			for (auto* a : nearby_combat_units) {
				if (processed.count(a)) continue;
				bool is_fighting = false;
				if (current_frame - a->last_fight <= 30 && current_frame - a->last_run > 30) is_fighting = true;
				a->is_nearest = nearest_combat_unit == a;
				a->is_nearest_weak = nearest_weak_combat_unit == a;
				a->nearest_ally = nearest_combat_unit;
				a->last_win_ratio = win_ratio;
				a->last_op_detectors = op_detectors;
				a->last_fight = current_frame;
				a->last_processed_fight = current_frame;
				a->num_in_enemy_tank_range = num_in_enemy_tank_range;
				a->any_attacking = any_attacking;
// 				if (a->u->type == unit_types::zergling) {
// 					if (prev_ling && ++ling_group_count >= 4) {
// 						ling_group_count = 0;
// 						a->subaction = prev_ling->subaction;
// 						a->target = prev_ling->target;
// 						a->target_pos = prev_ling->target_pos;
// 						a->ability = prev_ling->ability;
// 						prev_ling = nullptr;
// 						continue;
// 					}
// 					prev_ling = a;
// 				}
				if (a->subaction == combat_unit::subaction_repair) continue;
				if (a->copy) continue;
				bool attack = fight;
				if (a->u->is_flying && g.air_dpf < 0.1) attack = true;
				if (!a->u->is_flying && g.ground_dpf < 0.1) attack = true;
				attack |= a->u->is_flying && air_fight;
				attack |= !a->u->is_flying && ground_fight;
				//if (is_fighting && op_supply <= a->last_attack_enemy_army_supply * 1.25) attack = true;
				if (attack) {
					a->last_attack_enemy_army_supply = op_supply;
					a->last_attack_win_ratio = win_ratio;
					
					a->last_attack_frame = current_frame;
					++a->attack_counter;
				}
				if (last_attack_fight && !a->u->is_flying) attack = true;
				if (g.is_defensive_group) attack = true;
				//if (!actually_attack) attack = false;
//				if (!a->u->type->is_worker) {
//					if (is_fighting && !a->u->is_flying && a->last_attack_enemy_army_supply && (!a->last_target || !a->last_target->building)) {
//						if (current_frame - a->u->last_attacked <= 60 || current_frame - a->u->last_lost_hp <= 60 || enemy_siege_tank_area[grid::build_square_index(a->u->pos)]) {
//						//if (true) {
//							//if (op_supply <= a->last_attack_enemy_army_supply * 1.5) attack = true;
//							if (op_supply <= a->last_attack_enemy_army_supply * 1.25) attack = true;
//							if (win_ratio >= a->last_attack_win_ratio * 0.5 || eval.teams[0].end_supply) attack = true;
//							if (win_ratio >= 1.0) attack = true;
//						}
//					}
//				}
				
				if (current_frame < a->retreat_until) attack = false;
//				if (!attack && !enemy_siege_tank_area[grid::build_square_index(a->u->pos)]) {
//					a->last_attack_enemy_army_supply = 0.0;
//					a->last_attack_win_ratio = 0.0;
//				}
				//attack = true;
				bool skip = false;
				if (a->u->type->is_worker && !a->u->force_combat_unit && !is_attacking_my_workers && !use_worker_micro) {
					if (!op_is_all_lings && workers_attacking_count >= (int)my_workers.size() - 3) {
						skip = true;
					}
				}
				if (a->u->type->is_worker && !a->u->force_combat_unit && !run_with_workers && current_frame >= 15 * 60 * 10) {
					unit* target = get_best_score(nearby_enemies, [&](unit* u) {
						double d = units_distance(a->u, u);
						if (d > 32) return std::numeric_limits<double>::infinity();
						return d;
					}, std::numeric_limits<double>::infinity());
					if (!target) {
						attack = false;
						if (fight && a->u->controller->action == unit_controller::action_gather && !use_worker_micro) skip = true;
					}
				}
				if (a->u->type->is_worker && !a->u->force_combat_unit && run_with_workers) {
					attack = false;
					if (fight && a->u->controller->action == unit_controller::action_gather && !use_worker_micro) skip = true;
					//if (eval.teams[0].score > eval.teams[1].score && a->u->controller->action == unit_controller::action_gather) skip = true;
				}
				if (a->u->type->is_worker && !a->u->force_combat_unit) {
					if (have_static_defence && current_frame >= 15 * 6) skip = true;
					if (any_attacking && worker_count == nearby_allies.size()) skip = false;
				}
				if (skip) {
					a->subaction = combat_unit::subaction_idle;
					continue;
				}
				if (a->u->type->is_worker && !a->u->force_combat_unit) {
					++workers_attacking_count;
				}
				if (use_worker_micro && !a->u->is_flying && a->u->type != unit_types::vulture && a->u->type != unit_types::dragoon && (a->u->type != unit_types::zealot || my_workers.size() < 12 || current_frame <= 15*60*6)) {
					bool ok = true;
					if (a->u->type->is_worker) {
						unit* minerals = get_best_score(resource_units, [&](unit* u) {
							if (!u->type->is_minerals) return std::numeric_limits<double>::infinity();
							double d = diag_distance(a->u->pos - u->pos);
							if (d > 32 * 12) return std::numeric_limits<double>::infinity();
							return d;
						}, std::numeric_limits<double>::infinity());
						ok = minerals != nullptr;
					}
					if (ok) {
						if (!a->u->type->is_worker || current_frame < 15*60*8) {
							if (use_worker_micro_skip_workers && a->u->type->is_worker) a->subaction = combat_unit::subaction_idle;
							else a->subaction = combat_unit::subaction_worker_micro;
							continue;
						}
					}
				}
				//if (use_protect_marines_micro && a->u->type->is_worker) {
				if (use_protect_marines_micro) {
					//printf("set subaction_protect_marines_micro for %s\n", a->u->type->name.c_str());
					a->subaction = combat_unit::subaction_protect_marines_micro;
					continue;
				}
				if (a->u->type->is_worker && !a->u->force_combat_unit) {
					if (!square_pathing::unit_can_reach(a->u, a->u->pos, g.enemies.front()->pos, square_pathing::pathing_map_index::include_liftable_wall)) continue;
				}
				//if (a->u->type == unit_types::shuttle && ground_fight) attack = true;
				if (a->u->type == unit_types::shuttle) attack = true;
				if (a->u->type == unit_types::science_vessel) attack = true;
				if (a->u->type == unit_types::overlord) attack = true;
				if (my_workers.size() == current_used_total_supply) attack = true;
				//if (a->u->cloaked && !a->u->burrowed && !op_detectors && (players::opponent_player->race != race_terran || a->u->type == unit_types::dark_templar)) attack = true;
				if (a->u->cloaked && !a->u->burrowed && !op_detectors) {
					if (!is_in_detected_area(a->u->pos)) {
						attack = true;
					}
				}
				//if (a->u->type == unit_types::lurker && !op_detectors) attack = true;
				if (a->u->type == unit_types::lurker && a->u->burrowed && !op_detectors) attack = true;
				//if (a->u->irradiate_timer && eval.teams[0].end_supply) attack = true;
				if (current_frame < a->strategy_attack_until) attack = true;
				if (current_frame < a->strategy_run_until) attack = false;
				if (a->u->type == unit_types::zergling || a->u->type == unit_types::lurker || a->u->type == unit_types::ultralisk) {
					unit* target = get_best_score(nearby_enemies, [&](unit*e) {
						weapon_stats* w = e->is_flying ? a->u->stats->air_weapon : a->u->stats->ground_weapon;
						if (!w) return std::numeric_limits<double>::infinity();
						return units_distance(a->u, e);
					}, std::numeric_limits<double>::infinity());
					if (target && existing_dark_swarm.test(grid::build_square_index(target->pos))) {
						a->subaction = combat_unit::subaction_fight;
						a->target = target;
						continue;
					}
				} else if (!a->u->is_flying && existing_dark_swarm.test(grid::build_square_index(a->u->pos))) {
					//attack = true;
					unit*target = get_best_score(nearby_enemies, [&](unit*e) {
						weapon_stats*w = e->is_flying ? a->u->stats->air_weapon : a->u->stats->ground_weapon;
						if (!w) return std::numeric_limits<double>::infinity();
						return units_distance(a->u, e);
					}, std::numeric_limits<double>::infinity());
					if (target) {
						weapon_stats*w = target->is_flying ? a->u->stats->air_weapon : a->u->stats->ground_weapon;
						xy relpos = target->pos - a->u->pos;
						double len = relpos.length();
						double r = units_distance(target, a->u) - w->max_range;
						xy pos = a->u->pos + xy((int)(relpos.x / len * r), (int)(relpos.y / len*r));
						if ((size_t)pos.x < (size_t)grid::map_width && (size_t)pos.y < (size_t)grid::map_height) {
							if (existing_dark_swarm.test(grid::build_square_index(pos))) attack = true;
						}
					}
				}

				if (a->u->type == unit_types::siege_tank_tank_mode || a->u->type == unit_types::siege_tank_siege_mode) {
					if (my_tanks == 1 && a->last_win_ratio < 16.0 && current_used_total_supply < 80 && nearby_allies.size() < 15) {
						unit* bunker = get_best_score(my_completed_units_of_type[unit_types::bunker], [&](unit* u) {
							if (u->loaded_units.empty()) return std::numeric_limits<double>::infinity();
							double d = diag_distance(u->pos - a->u->pos);
							if (d >= 32 * 14) return std::numeric_limits<double>::infinity();
							return d;
						}, std::numeric_limits<double>::infinity());
						if (bunker) {
							fight = true;
							//log(log_level_test, " fight because 5\n");
							//if (!entire_threat_area.test(grid::build_square_index(a->u->pos))) {
							if (diag_distance(a->u->pos - my_start_location) >= 32 * 10) {
								double enemy_bunker_distance = get_best_score_value(nearby_enemies, [&](unit* e) {
									return units_distance(e, bunker);
								});
								double my_bunker_distance = units_distance(a->u, bunker);
								if (players::my_player->game_player->isResearching(BWAPI::TechTypes::Tank_Siege_Mode)) {
									if (enemy_bunker_distance > 32 * 3) {
										if (my_bunker_distance < 32 * 6) {
											//printf("pull tank back\n");
											a->subaction = combat_unit::subaction_move;
											a->target_pos = my_start_location;
											multitasking::yield_point();
											continue;
										}
									}
								} else if (players::my_player->has_upgrade(upgrade_types::siege_mode)) {
									if (my_bunker_distance <= 115 && enemy_bunker_distance <= 32 * 15) {
										//printf("force siege\n");
										a->subaction = combat_unit::subaction_idle;
										if (a->u->type == unit_types::siege_tank_tank_mode) {
											if (current_frame >= a->u->controller->move_away_until && a->u->game_unit->siege()) {
												a->u->controller->noorder_until = current_frame + 30;
												a->u->controller->last_siege = current_frame;
												multitasking::yield_point();
												continue;
											}
										}
									}
								}
							}
						}
					}
				}

				if (tank_line && !a->u->type->is_worker) continue;
				//if (attack && have_static_defence && win_ratio < 32.0 && !static_defence_in_range && current_used_total_supply < 40) attack = false;
				if (careless_aggression) attack = true;
				if (a->u->type == unit_types::high_templar) continue;
				if (a->u->type == unit_types::dark_archon) continue;
				if (attack) {
					do_attack(a, nearby_allies, nearby_enemies);
					if (a->u->type == unit_types::marine && a->subaction == combat_unit::subaction_fight && a->target && a->target->type == unit_types::zealot) {
						if (nearby_enemies.size() <= 6 && units_distance(a->u, a->target) <= 32 * 2) {
							do_run(a, nearby_enemies);
						}
					}
				} else {
					// fix interaction with retreat_until before uncommenting
					//if (!run_until) a->run_until = current_frame + 15 * 4;
					a->last_run = current_frame;
					bool run = true;
// 					if (a->u == defensive_matrix_vessel) {
// 						a->subaction = combat_unit::subaction_use_ability;
// 						a->ability = upgrade_types::defensive_matrix;
// 						a->target = defensive_matrix_target;
// 						run = false;
// 					}

// 					if (run) {
// 						if (a->u->type == unit_types::siege_tank_tank_mode || a->u->type == unit_types::siege_tank_siege_mode) {
// 							do_attack(a, nearby_allies, nearby_enemies);
// 							run = false;
// 						}
// 					}
					if (run) do_run(a, nearby_enemies);
				}
				multitasking::yield_point();
			}
			
			do_high_templars(nearby_combat_units, nearby_enemies);
			
			if (tank_line) {
				do_tank_line(nearby_combat_units, nearby_enemies, op_tanks == 0, &g);
			}

		}
		for (unit* e : nearby_enemies) {
			e->temporary_combat_flag = false;
		}
	}

	finish_attack();

}

bool is_in_detected_area(xy pos) {
	for (auto& v : detected_areas) {
		if (sc_distance(pos - std::get<0>(v)) <= 32 * 4) return true;
	}
	return false;
}

void add_detected_area(xy pos) {
	detected_areas.emplace_back(pos, current_frame + 15 * 60 * 10);
}

void execute() {

	for (auto*cu : live_combat_units) {

		if (cu->u->type->game_unit_type == BWAPI::UnitTypes::Hero_Fenix_Dragoon) continue;

		if (cu->copy) {
			cu->subaction = cu->copy->subaction;
			cu->target = cu->copy->target;
			cu->kite_attack_target = cu->copy->kite_attack_target;
			cu->target_pos = cu->copy->target_pos;
			cu->ability = cu->copy->ability;
		}

		if ((cu->u->type->is_worker && !cu->u->force_combat_unit) && (current_frame - cu->last_processed_fight >= 15 * 4) && cu->subaction != combat_unit::subaction_repair && current_frame >= cu->strategy_busy_until && !cu->reference_count) {
			cu->subaction = combat_unit::subaction_idle;
		}

		if (cu->subaction == combat_unit::subaction_fight) {
			if (cu->target && cu->target->dead) cu->subaction = combat_unit::subaction_idle;
			cu->u->controller->action = unit_controller::action_attack;
			cu->u->controller->target = cu->target;
		}
		if (cu->subaction == combat_unit::subaction_kite) {
			if (cu->target && cu->target->dead) cu->subaction = combat_unit::subaction_idle;
			cu->u->controller->action = unit_controller::action_kite;
			cu->u->controller->target = cu->target;
			cu->u->controller->kite_attack_target = cu->kite_attack_target;
			cu->u->controller->target_pos = cu->target_pos;
		}
		if (cu->subaction == combat_unit::subaction_move) {
			cu->u->controller->action = unit_controller::action_move;
			cu->u->controller->go_to = cu->target_pos;
			//game->drawLineMap(cu->u->pos.x, cu->u->pos.y, cu->target_pos.x, cu->target_pos.y, BWAPI::Colors::White);
		}
		if (cu->subaction == combat_unit::subaction_move_directly) {
			cu->u->controller->action = unit_controller::action_move_directly;
			cu->u->controller->go_to = cu->target_pos;
			game->drawLineMap(cu->u->pos.x, cu->u->pos.y, cu->target_pos.x, cu->target_pos.y, BWAPI::Colors::Blue);
		}
		if (cu->subaction == combat_unit::subaction_use_ability) {
			cu->u->controller->action = unit_controller::action_use_ability;
			cu->u->controller->ability = cu->ability;
			cu->u->controller->target = cu->target;
			cu->u->controller->target_pos = cu->target_pos;
		}
		if (cu->subaction == combat_unit::subaction_repair) {
			cu->u->controller->action = unit_controller::action_repair;
			cu->u->controller->target = cu->target;
		}
		if (cu->subaction == combat_unit::subaction_recharge) {
			cu->u->controller->action = unit_controller::action_recharge;
		}
		if (cu->subaction == combat_unit::subaction_worker_micro) {
			cu->u->controller->action = unit_controller::action_worker_micro;
		}
		if (cu->subaction == combat_unit::subaction_protect_marines_micro) {
			cu->u->controller->action = unit_controller::action_protect_marines_micro;
		}

		if (cu->subaction == combat_unit::subaction_idle) {
			if (cu->prev_subaction != combat_unit::subaction_idle) cu->u->controller->action = unit_controller::action_idle;
		}

		cu->prev_subaction = cu->subaction;

	}

}

void combat_task() {

	int last_update_bases = 0;

	while (true) {

		if (current_frame - last_update_bases >= 15 * 10) {
			last_update_bases = current_frame;

			update_my_base();
			update_op_base();
		}

		update_combat_units();

		fight();

		execute();

		multitasking::sleep(1);
	}

}

void update_combat_groups_task() {
	while (true) {
		
		for (auto i = active_storms.begin(); i != active_storms.end();) {
			if (current_frame >= i->first + 15 * 3) i = active_storms.erase(i);
			else ++i;
		}

		multitasking::sleep(10);

		update_groups();

	}
}

int build_bunker_count = 0;
int build_missile_turret_count = 0;

void update_defence_choke_task() {
	while (true) {

		multitasking::sleep(15 * 2);

		update_defence_choke();

		multitasking::yield_point();

		// This does not belong here!
		// Just a quick hack to build bunkers in a reasonable spot
		static int turret_flag;
		static int bunker_flag;
		//if (current_used_total_supply < 75) {
		if (true) {
			int desired_bunkers = build_bunker_count;
			if (desired_bunkers > (int)my_units_of_type[unit_types::marine].size() / 2 + 1) desired_bunkers = my_units_of_type[unit_types::marine].size() / 2 + 1;
			int bunkers_in_production = 0;
			for (build::build_task&b : build::build_tasks) {
				if (b.type->unit == unit_types::bunker) ++bunkers_in_production;
			}
			int bunker_count = my_completed_units_of_type[unit_types::bunker].size();
			if (bunker_count + bunkers_in_production < desired_bunkers) build::add_build_task(0, unit_types::bunker)->flag = &bunker_flag;
			for (auto&b : build::build_tasks) {
				if (b.built_unit || b.dead) continue;
				//if ((b.type->unit == unit_types::bunker && b.flag == &flag) || b.type->unit == unit_types::missile_turret) {
				//if (b.flag == &bunker_flag || (b.type->unit == unit_types::missile_turret && b.flag != &turret_flag && b.flag == nullptr)) {
				if (b.flag == &bunker_flag) {
					//if (b.build_near != defence_choke.center) {
					if (true) {
						b.build_near = defence_choke.center;
						build::unset_build_pos(&b);

						std::array<xy, 1> starts;
						starts[0] = combat::defence_choke.center;

						//bool in_danger = false;
						auto pred = [&](grid::build_square&bs) {
							if (!build_spot_finder::can_build_at(unit_types::bunker, bs)) return false;
							xy pos = bs.pos + xy(unit_types::bunker->tile_width * 16, unit_types::bunker->tile_height * 16);
							// 							if (!in_danger) {
							// 								for (unit*u : enemy_units) {
							// 									if (u->type->is_non_usable || !u->stats->ground_weapon) continue;
							// 									if (u->type->is_worker) continue;
							// 									if (diag_distance(u->pos - pos) <= u->stats->ground_weapon->max_range + 32 * 6) {
							// 										in_danger = true;
							// 										break;
							// 									}
							// 								}
							// 							}
							// 							if (in_danger) return true;
							for (auto*bs : defence_choke.outside_squares) {
								double d = (bs->pos + xy(16, 16) - pos).length();
								if (d <= 32 * 2) return false;
							}
							for (auto*bs : defence_choke.inside_squares) {
								double d = (bs->pos + xy(16, 16) - pos).length();
								if (d <= 32 * 2) return true;
							}
							return false;
						};
						auto score = [&](xy pos) {
							double r = 0;
							for (unit*u : enemy_units) {
								if (u->type->is_non_usable || !u->stats->ground_weapon) continue;
								//if (diag_distance(u->pos - pos) <= u->stats->ground_weapon->max_range + 32 * 6) r += 10000;
								if (diag_distance(u->pos - pos) <= u->stats->ground_weapon->max_range + 32 * 3) r += 10000;
							}
							for (auto*bs : defence_choke.build_squares) {
								double d = (bs->pos + xy(16, 16) - pos).length();
								r += d;
							}
							return r;
						};
						xy pos = build_spot_finder::find_best(starts, 128, pred, score);

						build::set_build_pos(&b, pos);

						if (b.type->unit == unit_types::bunker && b.flag == &bunker_flag) {
							++bunker_count;
							if (bunker_count > desired_bunkers) {
								cancel_build_task(&b);
								break;
							}
						}
					}
				}
			}
		}
		if (!my_completed_units_of_type[unit_types::scv].empty()) {
			if (build_missile_turret_count || current_used_total_supply >= 75) {
				if (current_minerals >= 500 || build_missile_turret_count) {
					double air_threat = 0;
					for (unit*e : enemy_units) {
						if (e->is_flying) air_threat += e->type->required_supply;
						if (e->type == unit_types::stargate) air_threat += 8;
						if (e->type == unit_types::fleet_beacon) air_threat += 12;
					}
					if (air_threat >= 12 || build_missile_turret_count) {
						int desired_missile_turrets = (int)current_minerals / 100;
						desired_missile_turrets += (int)air_threat / 4;
						if (desired_missile_turrets > 20) desired_missile_turrets = 20;
						if (build_missile_turret_count > desired_missile_turrets) desired_missile_turrets = build_missile_turret_count;
						int misile_turrets_in_production = 0;
						for (build::build_task&b : build::build_tasks) {
							if (b.type->unit == unit_types::missile_turret) ++misile_turrets_in_production;
						}
						int missile_turret_count = my_completed_units_of_type[unit_types::missile_turret].size();
						int max_inprod = 4;
						if (my_completed_units_of_type[unit_types::engineering_bay].empty()) max_inprod = 1;
						if (missile_turret_count + misile_turrets_in_production < desired_missile_turrets && misile_turrets_in_production <= max_inprod) build::add_build_task(0, unit_types::missile_turret)->flag = &turret_flag;
						int near_base = 0;
						for (unit*u : my_units_of_type[unit_types::missile_turret]) {
							bool near_building = test_pred(my_buildings, [&](unit*b) {
								if (u == b || b->type == unit_types::missile_turret) return false;
								return diag_distance(u->pos - b->pos) <= 32 * 8;
							});
							if (near_building) ++near_base;
						}
						if (near_base >= (int)my_units_of_type[unit_types::missile_turret].size() / 2) {
							unit*tank = nullptr;
							int oldest_tank_age = 0;
							for (int i = 0; i < 2; ++i) {
								for (unit*u : my_completed_units_of_type[i == 0 ? unit_types::siege_tank_tank_mode : unit_types::siege_tank_siege_mode]) {
									if (!tank || u->creation_frame < oldest_tank_age) {
										tank = u;
										oldest_tank_age = u->creation_frame;
									}
								}
							}
							if (tank) {
								for (auto&b : build::build_tasks) {
									if (b.built_unit || b.dead) continue;
									if (b.flag == &turret_flag) {
										if (b.build_near != tank->pos) {
											b.build_near = tank->pos;
											build::unset_build_pos(&b);
										}
									}
								}
							}
						}
					}
				}
			}
		}

	}
}

tsc::dynamic_bitset dark_swarm_grid;

void update_aoe_spellcasting() {
	int last_dark_swarm_clear = 0;
	while (true) {
		
		dark_swarm_grid.reset();
		use_dark_swarm_please.clear();
		use_plague_please.clear();

		use_storm_please.clear();
		use_stasis_please.clear();
		use_maelstrom_please.clear();
		use_ensnare_please.clear();

		existing_dark_swarm.reset();

		auto add_dark_swarm = [&](xy pos) {
			xy upper_left = pos - xy(3 * 16, 3 * 16);
			for (int y = 0; y < 32 * 6; y += 32) {
				for (int x = 0; x < 32 * 6; x += 32) {
					xy npos = upper_left + xy(x, y);
					if ((size_t)npos.x >= (size_t)grid::map_width || (size_t)npos.y >= (size_t)grid::map_height) continue;
					existing_dark_swarm.set(grid::build_square_index(npos));
				}
			}
		};

		a_vector<xy> existing_dark_swarms;
		for (unit*u : visible_units) {
			if (u->type != unit_types::spell_dark_swarm) continue;
			add_dark_swarm(u->pos);
			existing_dark_swarms.push_back(u->pos);
		}
		for (auto&v : dark_swarm_target_taken) {
			add_dark_swarm(v.first);
			existing_dark_swarms.push_back(v.first);
		}

// 		if (!players::my_player->has_upgrade(upgrade_types::dark_swarm) || my_completed_units_of_type[unit_types::defiler].empty()) {
// 			multitasking::sleep(60);
// 			continue;
// 		}

		auto do_dark_swarm = [&]() {
			a_vector<xy> swarm_positions;
			a_vector<combat_unit*> defilers;

			for (auto&g : groups) {
				if (g.enemies.size() < 4) continue;

				defilers.clear();
				for (auto*a : g.allies) {
					if (a->u->type == unit_types::defiler && a->u->energy >= 100) defilers.push_back(a);
				}
				if (defilers.empty()) continue;

				if (true) {
					for (auto*a : g.allies) {
						if (a->u->is_flying) continue;
						//if (a->last_win_ratio >= 8.0) continue;
						//if (a->subaction != combat_unit::subaction_fight && a->subaction != combat_unit::subaction_kite) continue;
						unit* target = a->target;
						bool in_range = false;
						double incoming_dpf = 0.0;
						for (auto* e : g.enemies) {
							weapon_stats* w = e->is_flying ? a->u->stats->air_weapon : a->u->stats->ground_weapon;
							if (w) {
								double d = units_distance(a->u, e);
								if (d >= w->min_range && d <= w->max_range + a->u->stats->max_speed*latency_frames) {
									in_range = true;
									target = e;
									break;
								}
							}
							double damage = get_weapon_damage_in_range(a->u, e);
							if (damage > 0) {
								weapon_stats* ew = e->is_flying ? a->u->stats->air_weapon : a->u->stats->ground_weapon;
								damage /= ew->cooldown;
								incoming_dpf += damage;
							}
						}
						if (!target) continue;
						if (incoming_dpf < 1.0) continue;
						auto add = [&](xy pos) {
							if (existing_dark_swarm.test(grid::build_square_index(pos))) return;
							bool found = false;
							for (xy npos : existing_dark_swarms) {
								if (max_distance(pos - npos) <= 32 * 3) {
									found = true;
									break;
								}
							}
							if (found) return;

							size_t index = grid::build_square_index(pos);
							if (!dark_swarm_grid.test(index)) swarm_positions.push_back(pos);
							dark_swarm_grid.set(index);
						};
						if (a->u->type == unit_types::defiler) {
							add(a->u->pos);
						} else if (a->u->type == unit_types::zergling || a->u->type == unit_types::ultralisk) {
							add(target->pos);
						} else {
							if (!in_range) continue;
							weapon_stats* w = target->is_flying ? a->u->stats->air_weapon : a->u->stats->ground_weapon;
							if (!w) continue;

							xy relpos = target->pos - a->u->pos;
							double ang = std::atan2(relpos.y, relpos.x);
							double d = units_distance(a->u, target) - w->max_range;
							if (d < 0) d = 0;
							for (double v = d + 64.0; v >= d; v -= 32.0) {
								xy pos = a->u->pos;
								pos.x += (int)(std::cos(ang)*v);
								pos.y += (int)(std::sin(ang)*v);
								if ((size_t)pos.x >= (size_t)grid::map_width || (size_t)pos.y >= (size_t)grid::map_height) break;
								add(pos);
							}
						}
					}
				}
				if (swarm_positions.empty()) {
					for (auto*a : g.allies) {
						if (a->u->is_flying) continue;
						if (a->last_win_ratio >= 8.0) continue;
						//if (a->subaction != combat_unit::subaction_fight && a->subaction != combat_unit::subaction_kite) continue;
						unit*target = a->target;
						bool in_range = false;
						for (auto*e : g.enemies) {
							weapon_stats*w = a->u->is_flying ? e->stats->air_weapon : e->stats->ground_weapon;
							if (!w) continue;
							if (w->max_range <= 32) continue;
							double d = units_distance(a->u, e);
							if (d >= w->min_range && d <= w->max_range + a->u->stats->max_speed*latency_frames) {
								in_range = true;
								target = e;
								break;
							}
						}
						if (!in_range) continue;
						if (!target) continue;
						weapon_stats*w = target->is_flying ? a->u->stats->air_weapon : a->u->stats->ground_weapon;
						if (!w) continue;

						xy relpos = target->pos - a->u->pos;
						double ang = std::atan2(relpos.y, relpos.x);
						double d = units_distance(a->u, target) - w->max_range;
						if (d < 0) d = 0;
						// 					if (d >= 160) d = 160;
						// 					for (double v = 0.0; v < d + 32; v += 32) {
											//for (double v = d + 64.0; v >= std::max(0.0, d - 160.0); v -= 32.0) {
						for (double v = d + 64.0; v >= d; v -= 32.0) {
							xy pos = a->u->pos;
							pos.x += (int)(std::cos(ang)*v);
							pos.y += (int)(std::sin(ang)*v);
							if ((size_t)pos.x >= (size_t)grid::map_width || (size_t)pos.y >= (size_t)grid::map_height) break;
							bool found = false;
							for (xy npos : existing_dark_swarms) {
								if (max_distance(pos - npos) <= 32 * 3) {
									found = true;
									break;
								}
							}
							if (found) continue;

							size_t index = grid::build_square_index(pos);
							if (!dark_swarm_grid.test(index)) swarm_positions.push_back(pos);
							dark_swarm_grid.set(index);
						}
					}
				}
				if (swarm_positions.empty()) {
					for (auto*a : g.allies) {
						if (a->u->is_flying) continue;
						if (a->last_win_ratio >= 8.0) continue;
						if (a->u->type != unit_types::lurker) continue;
						if (existing_dark_swarm.test(grid::build_square_index(a->u->pos))) continue;
						bool in_range = false;
						for (auto* e : g.enemies) {
							double damage = get_weapon_damage_in_range(a->u, e);
							if (damage) {
								in_range = true;
								break;
							}
						}
						if (!in_range) continue;
						size_t index = grid::build_square_index(a->u->pos);
						if (!dark_swarm_grid.test(index)) swarm_positions.push_back(a->u->pos);
						dark_swarm_grid.set(index);
					}
				}
// 				if (swarm_positions.empty()) {
// 					for (auto*a : g.allies) {
// 						if (a->u->is_flying) continue;
// 						if (a->last_win_ratio >= 8.0) continue;
// 						if (a->u->type != unit_types::zergling && a->u->type != unit_types::ultralisk) continue;
// 						unit* target = a->target;
// 						if (!target) continue;
// 						xy pos;
// 						for (auto* e : g.enemies) {
// 							double damage = get_weapon_damage_in_range(e, a->u);
// 							if (damage) {
// 								pos = e->pos;
// 								break;
// 							}
// 						}
// 						if (pos == xy()) continue;
// 						size_t index = grid::build_square_index(pos);
// 						if (!dark_swarm_grid.test(index)) swarm_positions.push_back(pos);
// 						dark_swarm_grid.set(index);
// 					}
// 				}

// 				auto get_defiler_in_range = [&](xy pos) {
// 					for (auto*a : defilers) {
// 						if (diag_distance(pos - a->u->pos) <= 32 * 9) return a;
// 					}
// 					return (combat_unit*)nullptr;
// 				};
				auto get_nearest_defiler = [&](xy pos) {
					return get_best_score(defilers, [&](combat_unit* a) {
						double d = diag_distance(pos - a->u->pos);
						if (d >= 32 * 16) return std::numeric_limits<double>::infinity();
						return d;
					}, std::numeric_limits<double>::infinity());
				};

				auto score_func = [&](xy pos) {
					if ((size_t)pos.x >= (size_t)grid::map_width || (size_t)pos.y >= (size_t)grid::map_height) return std::numeric_limits<double>::infinity();
					if (existing_dark_swarm.test(grid::build_square_index(pos))) return std::numeric_limits<double>::infinity();
					double score = 0.0;
					xy upper_left = pos - xy(3 * 16, 3 * 16);
					for (int y = 0; y < 32 * 6; y += 32) {
						for (int x = 0; x < 32 * 6; x += 32) {
							xy npos = upper_left + xy(x, y);
							if ((size_t)npos.x >= (size_t)grid::map_width || (size_t)npos.y >= (size_t)grid::map_height) continue;
							if (dark_swarm_grid.test(grid::build_square_index(npos))) score -= 1.0;
							auto& bs = grid::get_build_square(npos);
							if (!bs.entirely_walkable) score += 0.5;
						}
					}
					//if (score > -12) return std::numeric_limits<double>::infinity();
					//if (score > -4) return std::numeric_limits<double>::infinity();
					return score;
				};
				xy best_pos = get_best_score(swarm_positions, [&](xy pos) {
					//if (!get_defiler_in_range(pos)) return std::numeric_limits<double>::infinity();
					if (!get_nearest_defiler(pos)) return std::numeric_limits<double>::infinity();

					return score_func(pos);
				}, std::numeric_limits<double>::infinity());

				if (best_pos != xy()) {
					auto* a = get_nearest_defiler(best_pos);
					if (a) {
						xy pos = best_pos;
						double best_score = std::numeric_limits<double>::infinity();
						xy upper_left = pos - xy(3 * 16, 3 * 16);
						for (int y = 0; y < 32 * 6; y += 32) {
							for (int x = 0; x < 32 * 6; x += 32) {
								xy npos = upper_left + xy(x, y);
								double score = score_func(npos);
								if (score < best_score) {
									best_score = score;
									best_pos = pos;
								}
							}
						}
						use_dark_swarm_please[a] = best_pos;
					}
				}

			}
		};
		a_unordered_map<combat_unit*, double> spellcaster_used;
		auto do_aoe = [&](unit_type* spellcaster_type, auto& target_taken, double energy, double radius, double cast_range, double acquire_range, double min_value, auto& use_please, bool is_maelstrom, bool is_ensnare) {
			a_vector<combat_unit*> spellcasters;

//			a_vector<xy> existing_aoe;
//			for (auto&v : target_taken) {
//				existing_aoe.push_back(v.first);
//			}
//			auto existing_aoe_at = [&](xy pos) {
//				for (xy v : existing_aoe) {
//					if (max_distance(v - pos) <= radius) return true;
//				}
//				return false;
//			};
			auto existing_aoe_at = [&](auto* caster, xy pos) {
				for (auto& v : target_taken) {
					if (std::get<0>(v.second) == caster) continue;
					if (max_distance(v.first - pos) <= radius) return true;
				}
				return false;
			};

			for (auto&g : groups) {

				spellcasters.clear();
				for (auto* a : g.allies) {
					if (a->u->type == spellcaster_type && a->u->energy >= energy) spellcasters.push_back(a);
				}
				if (spellcasters.empty()) continue;

				auto get_spellcaster_in_range = [&](xy pos) {
					return get_best_score(spellcasters, [&](combat_unit*a) {
						double r = diag_distance(pos - a->u->pos);
						auto i = spellcaster_used.find(a);
						if (i != spellcaster_used.end()) {
							if (r > i->second) return std::numeric_limits<double>::infinity();
						}
						if (r <= cast_range) return 0.0;
						if (r <= acquire_range) return r;
						return std::numeric_limits<double>::infinity();
					}, std::numeric_limits<double>::infinity());
				};

				xy best_pos;
				double best_value = 0.0;
				for (auto* e : g.enemies) {
					//if (!e->visible) continue;
					if (e->gone) continue;
					if (e->stasis_timer) continue;
					if (e->plague_timer) continue;
					if (e->maelstrom_timer) continue;
					if (e->lockdown_timer) continue;

					xy epos = e->pos + xy((int)(e->hspeed * (latency_frames + 4)), (int)(e->vspeed * (latency_frames + 4)));

					auto* spellcaster = get_spellcaster_in_range(epos);
					if (!spellcaster) continue;

					double total_value = 1.0 - diag_distance(epos - spellcaster->u->pos);
					for (auto* e2 : g.enemies) {
						if (!e2->visible) continue;
						if (e2->stasis_timer) continue;
						if (e2->plague_timer) continue;
						if (e2->maelstrom_timer) continue;
						if (e2->lockdown_timer) continue;
						if (e2->building) continue;
						if (is_maelstrom && (!e->type->is_biological || e->burrowed)) continue;
						if (max_distance(epos - e2->pos) > radius) continue;
						if (existing_aoe_at(spellcaster, e2->pos)) continue;
						double value = e->type->total_minerals_cost + e->type->total_gas_cost;
						value *= e->hp / e->stats->hp;
						if (is_ensnare) {
							if (e2->game_unit->isEnsnared()) continue;
							//if (!e->type->is_biological) continue;
							if (e2->type == unit_types::marine) value *= 1.5;
							if (e2->type == unit_types::siege_tank_tank_mode) value *= 0.1;
							if (e2->type == unit_types::siege_tank_siege_mode) value *= 0.1;
							if (e2->type == unit_types::goliath) value *= 0.1;
							if (e2->type == unit_types::vulture) value *= 0.25;
							if (e2->type == unit_types::science_vessel) value *= 0.25;
						}
						total_value += value;
					}

					for (auto* a : g.allies) {
						if (a->u->stasis_timer) continue;
						if (a->u->plague_timer) continue;
						if (existing_aoe_at(spellcaster, a->u->pos)) continue;
						if (max_distance(epos - a->u->pos) > radius) continue;
						double value = e->type->total_minerals_cost + e->type->total_gas_cost;
						value *= e->hp / e->stats->hp;
						total_value -= value / 2;
					}
					double minval = min_value;
					minval *= (spellcaster->u->hp + spellcaster->u->shields) / (spellcaster->u->stats->hp + spellcaster->u->stats->shields);
					if (total_value < minval) continue;
					if (total_value > best_value) {
						best_value = total_value;
						best_pos = epos;
					}
				}

				if (best_pos != xy()) {
					auto* a = get_spellcaster_in_range(best_pos);
					if (a) {
						use_please[a] = best_pos;
						
						spellcaster_used[a] = diag_distance(best_pos - a->u->pos);
						
						game->drawLineMap(a->u->pos.x, a->u->pos.y, best_pos.x, best_pos.y, BWAPI::Colors::Red);
					}
				}

			}
		};

		if (!my_completed_units_of_type[unit_types::defiler].empty()) {
			if (players::my_player->has_upgrade(upgrade_types::dark_swarm)) {
				do_dark_swarm();
				multitasking::yield_point();
			}
			if (players::my_player->has_upgrade(upgrade_types::plague)) {
				do_aoe(unit_types::defiler, plague_target_taken, 150.0, 32 * 1.5, 32 * 9, 32 * 9, 100.0, use_plague_please, false, false);
				do_aoe(unit_types::defiler, plague_target_taken, 150.0, 32 * 1.5, 32 * 9, 32 * 9 + 32 * 4, 200.0, use_plague_please, false, false);
				multitasking::yield_point();
			}
		}
		if (!my_completed_units_of_type[unit_types::high_templar].empty()) {
			if (players::my_player->has_upgrade(upgrade_types::psionic_storm)) {
				do_aoe(unit_types::high_templar, storm_target_taken, 75.0, 32 * 2.25, 32 * 9, 32 * 9 + 32 * 9, 200.0, use_storm_please, false, false);
				//do_aoe(unit_types::high_templar, storm_target_taken, 75.0, 32 * 2.25, 32 * 9, 32 * 9 + 32 * 6, 200.0, use_storm_please, false, false);
				//do_aoe(unit_types::high_templar, storm_target_taken, 75.0, 32 * 1.5, 32 * 9, 32 * 9 + 32 * 0, 200.0, use_storm_please, false, false);
				multitasking::yield_point();
			}
		}
		if (!my_completed_units_of_type[unit_types::arbiter].empty()) {
			if (players::my_player->has_upgrade(upgrade_types::stasis_field)) {
				do_aoe(unit_types::arbiter, aoe_disable_target_taken, 100.0, 32 * 1.5, 32 * 9, 32 * 9 + 32 * 6, 250.0, use_stasis_please, false, false);
				multitasking::yield_point();
			}
		}
		if (!my_completed_units_of_type[unit_types::dark_archon].empty()) {
			if (players::my_player->has_upgrade(upgrade_types::maelstrom)) {
				do_aoe(unit_types::dark_archon, aoe_disable_target_taken, 100.0, 32 * 1.5, 32 * 10, 32 * 10 + 32 * 4, 250.0, use_maelstrom_please, true, false);
				multitasking::yield_point();
			}
		}
		if (!my_completed_units_of_type[unit_types::queen].empty()) {
			if (players::my_player->has_upgrade(upgrade_types::ensnare)) {
				do_aoe(unit_types::queen, aoe_disable_target_taken, 75.0, 32 * 2, 32 * 9, 32 * 10, 200.0, use_ensnare_please, false, true);
				do_aoe(unit_types::queen, aoe_disable_target_taken, 75.0, 32 * 2, 32 * 9, 32 * 9 + 32 * 12, 200.0, use_ensnare_please, false, true);
				multitasking::yield_point();
			}
		}


		multitasking::sleep(8);
	}
}

void drops_task() {
	a_vector<combat_unit*> dropships;
	while (true) {
		dropships.clear();
		for (auto*a : combat::live_combat_units) {
			if (a->u->type == unit_types::dropship || a->u->type == unit_types::shuttle || (a->u->type == unit_types::overlord && players::my_player->has_upgrade(upgrade_types::ventral_sacs))) {
				dropships.push_back(a);
			}
		}

		a_unordered_set<combat_unit*> available_units;
		for (auto*a : combat::live_combat_units) {
			if (a->force_attack) continue;
			if (a->use_for_drops_until > current_frame && !a->u->is_flying && !a->u->is_loaded) {
				available_units.insert(a);
			}
		}

		//log(log_level_info, "available_units.size() is %d\n", available_units.size());

		pickup_list.clear();

		for (auto*a : dropships) {
			if (a->u->loaded_units.empty() && a->u->hp < a->u->stats->hp) continue;
			if (a->drop_target != xy()) continue;

			int space = a->u->type->space_provided;
			int ghost_count = 0;
			int marine_count = 0;
			int medic_count = 0;
			for (unit*lu : a->u->loaded_units) {
				space -= lu->type->space_required;
				if (lu->type == unit_types::ghost) ++ghost_count;
				if (lu->type == unit_types::marine) ++marine_count;
				if (lu->type == unit_types::medic) ++medic_count;

				combat_unit_map[lu].last_given_a_lift = current_frame;
				lu->controller->no_unload_until = current_frame + 15 * 5;
			}
			a_vector<combat_unit*> units_to_pick_up;
			while (space > 0) {
				combat_unit*nu = get_best_score(available_units, [&](combat_unit*nu) {
					if (nu->u->type->is_flyer) return std::numeric_limits<double>::infinity();
					if (nu->u->is_loaded) return std::numeric_limits<double>::infinity();
					if (nu->u->type->space_required > space) return std::numeric_limits<double>::infinity();
					double r = 0.0;
					if (nu->u->type == unit_types::medic) {
						if (ghost_count + marine_count == 0 || medic_count >= 4) return std::numeric_limits<double>::infinity();
						r += medic_count * 32;
					}
					if (nu->u->type == unit_types::ghost) {
						r += -32 + ghost_count * 16;
					}
					if (nu->u->type == unit_types::reaver) r -= 1000.0;
					double d = diag_distance(nu->u->pos - a->u->pos);
					if (d < 32 * 6) d /= 10.0;
					return r + d;
				}, std::numeric_limits<double>::infinity());
				if (!nu) break;
				available_units.erase(nu);
				units_to_pick_up.push_back(nu);
				space -= nu->u->type->space_required;
			}

			//log(log_level_info, "units_to_pick_up.size() is %d\n", units_to_pick_up.size());

			for (auto*nu : units_to_pick_up) {
				pickup_list.emplace_back(a, nu);
			}

			if (units_to_pick_up.empty() && !a->u->loaded_units.empty()) {

				group_t*goal = get_best_score_p(groups, [&](group_t*g) {
					double r = 0.0;
					for (unit*e : g->enemies) {
						if (e->type->is_worker) r += 1.0;
						if (e->type->is_building) r += 1.0;
						if (!e->type->is_worker && (e->stats->air_weapon || e->stats->ground_weapon)) r -= 1.0;
					}
					return -r * rng(1.0);
				});

				if (goal) {
					a->drop_target = goal->enemies.front()->pos;
					//log("%p - drop_target set to %d %d\n", a, a->drop_target.x, a->drop_target.y);

					for (auto*na : live_combat_units) {
						if (na->u->loaded_into == a->u) na->force_attack = true;
					}
				}
			}

		}

		for (auto*a : dropships) {
			if (a->drop_target == xy()) continue;

			for (unit*lu : a->u->loaded_units) {
				combat_unit_map[lu].last_given_a_lift = current_frame;
				lu->controller->no_unload_until = current_frame + 15 * 5;
			}

			int iterations = 0;
			struct node_data_t {
				double total_cost = 0.0;
			};
			a_deque<xy> path = combat::find_path<node_data_t>(a->u->type, a->u->pos, [&](xy pos, xy npos, node_data_t&n) {
				if (++iterations % 1024 == 0) multitasking::yield_point();
				if (combat::entire_threat_area.test(grid::build_square_index(npos))) n.total_cost += 32 * 2;
				return true;
			}, [&](xy pos, xy npos, const node_data_t&n) {
				return diag_distance(npos - a->drop_target) + n.total_cost;
			}, [&](xy pos, const node_data_t&n) {
				return diag_distance(pos - a->drop_target) <= 32 * 2;
			});
			xy go_to = a->drop_target;
			if (!path.empty()) {
				double move_distance = a->u->stats->max_speed * 15;
				for (xy pos : path) {
					if (diag_distance(pos - a->u->pos) >= move_distance) {
						go_to = pos;
						break;
					}
				}
			}
			xy relpos = go_to - a->u->pos;
			double ang = std::atan2(relpos.y, relpos.x);
			double x = std::cos(ang) * 32 * 8;
			double y = std::sin(ang) * 32 * 8;
			go_to = a->u->pos + xy((int)x, (int)y);
			if (go_to.x < 0) go_to.x = 0;
			if (go_to.x >= grid::map_width) go_to.x = grid::map_width;
			if (go_to.y < 0) go_to.y = 0;
			if (go_to.y >= grid::map_height) go_to.y = grid::map_height;
			a->drop_move_target = go_to;

		}


		multitasking::sleep(6);
	}
}


void generate_defence_spots(xy src) {

	a_vector<xy> spots;

	tsc::dynamic_bitset visited(grid::build_grid_width * grid::build_grid_height);
	tsc::dynamic_bitset taken(grid::build_grid_width * grid::build_grid_height);

// 	tsc::dynamic_bitset area_to_defend(grid::build_grid_width * grid::build_grid_height);
// 
// 	for (auto& s : resource_spots::spots) {
// 		auto& bs = grid::get_build_square(s.cc_build_pos);
// 		if (bs.building && bs.building->owner == players::my_player) {
// 
// 			xy pos = s.pos;
// 			for (int x = -12; x <= 12; ++x) {
// 				for (int y = -12; y <= 12; ++y) {
// 					xy npos = pos + xy(x * 32, y * 32);
// 					if ((size_t)npos.x >= (size_t)grid::build_grid_width) continue;
// 					if ((size_t)npos.y >= (size_t)grid::build_grid_height) continue;
// 					area_to_defend.set(grid::build_square_index(npos));
// 				}
// 			}
// 
// 		}
// 	}

	tsc::dynamic_bitset area_to_defend(grid::build_grid_width * grid::build_grid_height);
	a_vector<xy> positions;
	for (unit* u : my_resource_depots) {
		for (auto*v : u->building->build_squares_occupied) {
			positions.push_back(v->pos);
		}
	}
	for (auto&b : build::build_tasks) {
		if (b.built_unit) continue;
		if (b.build_pos != xy() && b.type->unit && b.type->unit->is_resource_depot) positions.push_back(b.build_pos);
	}
	if (false) {
		for (auto* cs : active_control_spots) {
			if (cs->is_best_defence) {
				positions.push_back(cs->pos);
			}
		}
	}
	update_base(area_to_defend, positions, 32 * 8);

	a_deque<xy> open;

	open.push_back(src);
	visited.set(grid::build_square_index(src));

	while (!open.empty()) {

		xy pos = open.front();
		open.pop_front();

		auto add = [&](xy npos) {
			if ((size_t)npos.x >= (size_t)grid::map_width) return;
			if ((size_t)npos.y >= (size_t)grid::map_height) return;

			grid::build_square& bs = grid::get_build_square(npos);

			if (!bs.entirely_walkable) return;
			if (bs.building && bs.building->owner == players::neutral_player) return;

			size_t index = grid::build_square_index(npos);
			if (visited.test(index)) return;
			if (taken.test(index)) return;
			visited.set(index);

			if (area_to_defend.test(index)) {
				for (int x = -12; x <= 12; ++x) {
					for (int y = -12; y <= 12; ++y) {
						xy tpos = npos + xy(x * 32, y * 32);
						if ((size_t)tpos.x >= (size_t)grid::map_width) continue;
						if ((size_t)tpos.y >= (size_t)grid::map_height) continue;
						taken.set(grid::build_square_index(tpos));
					}
				}

				spots.push_back(npos);
				return;
			}

			open.push_back(npos);

		};

		add(pos + xy(32, 0));
		add(pos + xy(0, 32));
		add(pos + xy(-32, 0));
		add(pos + xy(0, -32));


	}

	log(log_level_info, "generated %d defence spots\n", spots.size());

	double total_weight = 0.0;
	defence_spots.clear();
	for (xy& pos : spots) {
		defence_spot ds;
		ds.pos = pos;

		double distance_sum = 1.0;
		for (xy& pos2 : spots) {
			if (&pos == &pos2) continue;
			double d = unit_pathing_distance(unit_types::scv, pos, pos2);
			if (d != std::numeric_limits<double>::infinity()) distance_sum += d * d;
		}

		ds.weight = 1.0 / distance_sum;
		total_weight += ds.weight;

		defence_spots.push_back(ds);

	}

	for (auto& v : defence_spots) {
		v.weight /= total_weight;
		log(log_level_info, "v.weight is %g (total_weight %g)\n", v.weight, total_weight);
	}

}

xy best_defence_pos;

int last_generate_control_spots = 0;

void generate_control_spots() {

	tsc::high_resolution_timer ht;

	int frames_since_last = current_frame - last_generate_control_spots;
	last_generate_control_spots = current_frame;

	tsc::dynamic_bitset visited(grid::build_grid_width * grid::build_grid_height);

	tsc::dynamic_bitset entire_control_area(grid::build_grid_width * grid::build_grid_height);
	tsc::dynamic_bitset op_control_area(grid::build_grid_width * grid::build_grid_height);

	active_control_spots.clear();

	auto get_control_spot = [&](xy pos) {
		auto& cs = control_spot_map[pos];
		cs.pos = pos;
		cs.incoming_pos = cs.pos;
		cs.reachable = false;
		cs.visited = false;
		cs.area.resize(grid::build_grid_width * grid::build_grid_height);
		update_base(cs.area, { cs.pos }, 32 * 12, &cs.squares);

		entire_control_area |= cs.area;

		active_control_spots.push_back(&cs);

		for (int i = 0; i < frames_since_last; ++i) {
			int neutral_time = 15 * 60 * 1;
			int degen_time = 15 * 60 * 2;
			size_t index = grid::build_square_index(cs.pos);
			cs.control_value = (cs.control_value - neutral_time) * (1.0 - (1.0 / degen_time)) + neutral_time;
			if ((!my_units_area.test(index) && !my_base.test(index)) || !can_transfer_through(cs.pos)) {
				if (entire_threat_area.test(index) || diag_distance(cs.pos - opponent_start_location) <= 32 * 20) {
					cs.control_value -= 1.0;
				}
			}
			//if (my_units_area.test(index) || my_base.test(index)) {
			//	cs.control_value = (cs.control_value - neutral_time) * (1.0 - (1.0 / degen_time)) + neutral_time;
			//}
			//if (entire_threat_area.test(grid::build_square_index(cs.pos))) {
			//if (!can_transfer_through(cs.pos)) {
			//	cs.control_value -= 1;
			//}
		}

		if (cs.control_value < 0.0) op_control_area |= cs.area;
		return &cs;
	};

	for (auto& s : resource_spots::spots) {
		//get_control_spot(s.cc_build_pos + xy(4 * 16, 3 * 16))->is_map_split_center = true;
		get_control_spot(s.cc_build_pos + xy(4 * 16, 3 * 16));
	}

	if (map_split_center_pos != xy()) {
		//auto* cs = get_control_spot(map_split_center_pos);
		//cs->is_map_split_center = true;
	}
	if (best_defence_pos != xy()) {
// 		bool any_nearby = false;
// 		for (auto* cs : active_control_spots) {
// 			if (diag_distance(best_defence_pos - cs->pos) <= 32 * 14) {
// 				any_nearby = true;
// 				break;
// 			}
// 		}
// 		if (!any_nearby) {
// 			auto* cs = get_control_spot(best_defence_pos);
// 			cs->is_best_defence = true;
// 		}

// 		if (my_resource_depots.size() >= 3) {
// 			auto* cs = get_control_spot(best_defence_pos);
// 			cs->is_best_defence = true;
// 		}
	}

	a_deque<xy> open;

	open.push_back(op_closest_base);
	visited.set(grid::build_square_index(op_closest_base));

	while (!open.empty()) {

		xy pos = open.front();
		open.pop_front();

		auto add = [&](xy npos) {
			if ((size_t)npos.x >= (size_t)grid::map_width) return;
			if ((size_t)npos.y >= (size_t)grid::map_height) return;

			grid::build_square& bs = grid::get_build_square(npos);

			if (!bs.entirely_walkable) return;

			size_t index = grid::build_square_index(npos);
			if (visited.test(index)) return;
			visited.set(index);

			if (entire_control_area.test(index)) {
// 				for (int y = -12; y <= 12; ++y) {
// 					for (int x = -12; x <= 12; ++x) {
// 						xy tpos = npos + xy(x * 32, y * 32);
// 						if ((size_t)tpos.x >= (size_t)grid::map_width) continue;
// 						if ((size_t)tpos.y >= (size_t)grid::map_height) continue;
// 						visited.set(grid::build_square_index(tpos));
// 					}
// 				}

				for (auto* cs : active_control_spots) {
					if (!cs->visited && cs->area.test(index)) {
						cs->incoming_pos = npos;
						cs->visited = true;
						//return;
					}
				}
				//xcept("first iter: area mismatch");
				//return;
			}

			open.push_back(npos);

		};

		add(pos + xy(32, 0));
		add(pos + xy(0, 32));
		add(pos + xy(-32, 0));
		add(pos + xy(0, -32));
	}

	visited.reset();
	open.push_back(my_closest_base);
	visited.set(grid::build_square_index(my_closest_base));

	while (!open.empty()) {

		xy pos = open.front();
		open.pop_front();

		auto add = [&](xy npos) {
			if ((size_t)npos.x >= (size_t)grid::map_width) return;
			if ((size_t)npos.y >= (size_t)grid::map_height) return;

			grid::build_square& bs = grid::get_build_square(npos);

			if (!bs.entirely_walkable) return;

			size_t index = grid::build_square_index(npos);
			if (visited.test(index)) return;
			visited.set(index);

			if (entire_control_area.test(index)) {
				for (auto* cs : active_control_spots) {
					if (!cs->reachable && cs->area.test(index)) {
						cs->reachable = true;
					}
				}
				if (op_control_area.test(index)) return;
			}

			open.push_back(npos);

		};

		add(pos + xy(32, 0));
		add(pos + xy(0, 32));
		add(pos + xy(-32, 0));
		add(pos + xy(0, -32));
	}

	double total_weight = 0.0;
	for (auto* cs : active_control_spots) {

		double distance = unit_pathing_distance(unit_types::scv, op_closest_base, cs->pos);

		cs->weight = 1.0 / distance;
		total_weight += cs->weight;

	}

// 	for (auto& v : defence_spots) {
// 		v.weight /= total_weight;
// 	}


	int n_op_controlled = 0;
	a_vector<control_spot*> top_control_spots;
	for (auto* cs : active_control_spots) {
		if (!cs->reachable) continue;
		if (cs->control_value < 0.0) ++n_op_controlled;
		if (cs->control_value < 0.0) continue;
		auto& bs = grid::get_build_square(cs->pos);
		if (bs.building && bs.building->owner == players::my_player) continue;
		top_control_spots.push_back(cs);
	}
	std::sort(top_control_spots.begin(), top_control_spots.end(), [&](control_spot* a, control_spot* b) {
		if (a->is_map_split_center != b->is_map_split_center) return a->is_map_split_center;
		if (std::floor(a->control_value / 20.0) != std::floor(b->control_value / 20.0)) return a->control_value < b->control_value;
		return a->weight > b->weight;
	});
	if (top_control_spots.size() > 3) top_control_spots.resize(3);

	for (auto* c : live_combat_units) {
		c->assigned_control_spot = nullptr;
	}

	if (current_used_total_supply >= 160) return;
	if (n_op_controlled >= 4) return;

	double total_supply = 0.0;
	a_vector<combat_unit*> available;
	for (auto& g : groups) {
		if (!g.is_aggressive_group) continue;
		for (auto* c : g.allies) {
			if (current_frame < c->strategy_busy_until) continue;
			if (c->reference_count) continue;
			if (c->subaction != combat_unit::subaction_move) continue;
			if (current_frame - c->last_run <= 15 * 5) continue;
			if (c->u->is_flying) continue;
			total_supply += c->u->type->required_supply;

			available.push_back(c);
		}
	}

	std::sort(top_control_spots.begin(), top_control_spots.end(), [&](control_spot* a, control_spot* b) {
		return a->control_value < b->control_value;
	});

	log(log_level_info, "top_control_spots.size() is %d\n", top_control_spots.size());

	double my_army_supply = current_used_total_supply - my_workers.size();
	double enemy_army_supply = 0;
	for (unit*u : enemy_units) {
		if (u->gone || u->type->is_worker) continue;
		enemy_army_supply += u->type->required_supply;
	}

	//if (my_army_supply < enemy_army_supply + std::min(std::max(enemy_army_supply, 40.0), 5.0)) {
	//if (true) {
	//if (false) {
	if ((players::my_player->race == race_terran || use_control_spots_non_terran) && use_control_spots) {

		if (!top_control_spots.empty()) {
			for (auto* c : available) c->assigned_control_spot = top_control_spots.front();
		}

		auto get_nearest = [&](xy pos) {

			auto* r = get_best_score(available, [&](combat_unit* a) {
				return unit_pathing_distance(a->u, pos);
			});
			available.erase(std::find(available.begin(), available.end(), r));
			return r;
		};

		double assign_supply = total_supply;
		double assign_supply_left = assign_supply;
		size_t i = 0;
		for (auto* cs : top_control_spots) {
			double assigned = 0.0;
			double to_assign = assign_supply_left / (top_control_spots.size() - i);
			if (cs->is_map_split_center) to_assign = assign_supply_left * 0.75;
			while (assigned < to_assign) {
				if (available.empty()) break;
				auto* a = get_nearest(cs->pos);
				assigned += a->u->type->required_supply;

				a->assigned_control_spot = cs;

				log(log_level_info, "assigned control spot to %s\n", a->u->type->name);
			}
			++i;
		}

		if (!available.empty() && !top_control_spots.empty()) {
			for (auto* c : available) c->assigned_control_spot = top_control_spots.front();
		}
	}

	log(log_level_info, "generated %d control spots in %g\n", active_control_spots.size(), ht.elapsed());

}

void generate_defence_spots_task() {

	multitasking::sleep(15);

	while (true) {

		if (op_closest_base != xy()) generate_defence_spots(op_closest_base);

		multitasking::sleep(90);
	}

}

void generate_control_spots_task() {

	multitasking::sleep(15);

	while (true) {

		generate_control_spots();

		multitasking::sleep(90);
	}

}

void update_best_defence_pos_task() {

	multitasking::sleep(20);

	while (true) {

		
		a_vector<xy> positions;
// 		for (unit* u : my_resource_depots) {
// 			for (auto*v : u->building->build_squares_occupied) {
// 				positions.push_back(v->pos);
// 			}
// 		}
// 		for (auto&b : build::build_tasks) {
// 			if (b.built_unit) continue;
// 			if (b.build_pos != xy() && b.type->unit && b.type->unit->is_resource_depot) positions.push_back(b.build_pos);
// 		}
		for (auto& v : defence_spots) {
			positions.push_back(v.pos);
		}
		if (!positions.empty()) {
			xy best_pos;
			double best_score = std::numeric_limits<double>::infinity();

			auto& pathing_map = square_pathing::get_pathing_map(unit_types::siege_tank_tank_mode);
			for (auto& v : pathing_map.path_nodes) {
				double s = 0.0;
				for (xy pos : positions) {
					double d = square_pathing::get_distance(pathing_map, v.pos, pos);
					s += d*d;
				}
				if (s < best_score) {
					best_score = s;
					best_pos = v.pos;
				}
				multitasking::yield_point();
			}

			best_defence_pos = best_pos;
		}

		multitasking::sleep(15 * 15);

		

	}

}

//tsc::dynamic_bitset enemy_siege_tank_area;

void generate_siege_tank_area() {

	for (auto& v : enemy_siege_tank_area) v = 0;

	a_deque<xy> open;
	std::array<bool, 26 * 26> visited;

	for (unit* e : enemy_units) {
		if (e->gone) continue;
		if (e->type != unit_types::siege_tank_siege_mode) continue;

		visited.fill(false);

		xy origin = e->pos - xy(32 * 13, 32 * 13);
		xy relpos = e->pos - origin;
		open.push_back(relpos);
		visited.at((size_t)relpos.x / 32 + (size_t)relpos.y / 32 * 26) = true;
		++enemy_siege_tank_area[grid::build_square_index(relpos + origin)];

		while (!open.empty()) {

			xy pos = open.front();
			open.pop_front();

			auto add = [&](xy npos) {
				if ((size_t)(origin.x + npos.x) >= (size_t)grid::map_width) return;
				if ((size_t)(origin.y + npos.y) >= (size_t)grid::map_height) return;

				if ((npos - relpos).length() > 32 * 12) return;

				grid::build_square& bs = grid::get_build_square(npos);

				size_t index = (size_t)npos.x / 32 + (size_t)npos.y / 32 * 26;
				if (visited.at(index)) return;
				visited[index] = true;
				++enemy_siege_tank_area[grid::build_square_index(npos + origin)];

				open.push_back(npos);
			};

			add(pos + xy(32, 0));
			add(pos + xy(0, 32));
			add(pos + xy(-32, 0));
			add(pos + xy(0, -32));

		}

	}

}

void generate_siege_tank_area_task() {

	while (true) {

		generate_siege_tank_area();

		multitasking::sleep(15);
	}

}


void generate_map_split() {

	map_split_my_area.reset();
	map_split_op_area.reset();

	struct open_t {
		xy pos;
		bool is_me;
		double distance;
	};

	struct border_t {
		double my_distance = 0.0;
		double op_distance = 0.0;
	};

	a_vector<border_t> border(grid::build_grid_width * grid::build_grid_height);
	map_split_border_area.reset();
	map_split_border_locations.clear();

	a_deque<open_t> open;
	a_deque<open_t> open2;

	for (auto* cs : active_control_spots) {
		if (cs->control_value >= 0.0) continue;
		open_t n;
		n.pos = cs->pos;
		n.is_me = cs->control_value >= 0.0;
		n.distance = 0.0;
		open.push_back(n);
		if (n.is_me) map_split_my_area.set(grid::build_square_index(n.pos));
		else map_split_op_area.set(grid::build_square_index(n.pos));
	}
	for (unit* u : my_resource_depots) {
		if (u->is_flying) continue;
		open_t n;
		n.pos = u->pos;
		n.is_me = true;
		n.distance = 0.0;
		open.push_back(n);
		map_split_my_area.set(grid::build_square_index(n.pos));
	}

	auto add_border = [&](bool is_me, xy pos, xy npos, double distance) {

		auto add = [&](xy border_pos, bool force) {
			size_t index = grid::build_square_index(border_pos);
			auto& b = border[index];
			bool existing = is_me ? b.op_distance != 0.0 : b.my_distance != 0.0;
			if (force || existing) {
				if (is_me) b.my_distance = distance;
				else b.op_distance = distance;
				if (!existing) {
					map_split_border_locations.push_back(border_pos);
					map_split_border_area.set(index);
				}
				return true;
			}
			return false;
		};

		if (add(pos, false)) return;
		add(npos, true);
	};

	while (!open.empty() || !open2.empty()) {
		open_t cn;
		if (!open.empty()) {
			cn = open.front();
			open.pop_front();
		} else {
			cn = open2.front();
			open2.pop_front();
		}

		auto add = [&](xy npos) {
			if ((size_t)npos.x >= (size_t)grid::map_width) return;
			if ((size_t)npos.y >= (size_t)grid::map_height) return;

			grid::build_square& bs = grid::get_build_square(npos);

			double n_distance = cn.distance + (bs.entirely_walkable ? 32.0 : 1024.0);

			size_t index = grid::build_square_index(npos);
			if (map_split_my_area.test(index)) {
				if (!cn.is_me) add_border(cn.is_me, cn.pos, npos, n_distance);
				return;
			}
			if (map_split_op_area.test(index)) {
				if (cn.is_me) add_border(cn.is_me, cn.pos, npos, n_distance);
				return;
			}
			if (cn.is_me) map_split_my_area.set(index);
			else map_split_op_area.set(index);

			open_t nn;
			nn.pos = npos;
			nn.is_me = cn.is_me;
			nn.distance = n_distance;

			if (bs.entirely_walkable) open.push_back(nn);
			else open2.push_back(nn);

		};

		add(cn.pos + xy(32, 0));
		add(cn.pos + xy(0, 32));
		add(cn.pos + xy(-32, 0));
		add(cn.pos + xy(0, -32));
	}


	double best_score = std::numeric_limits<double>::infinity();
	xy best_pos;

	for (xy pos : map_split_border_locations) {
		if (!grid::get_build_square(pos).entirely_walkable) continue;
		size_t index = grid::build_square_index(pos);

		auto& b = border[index];

		//log("b.my_distance %g b.op_distance %g\n", b.my_distance, b.op_distance);

		if (!b.my_distance || !b.op_distance) xcept("waa");

		double s = b.my_distance * b.my_distance + b.op_distance * b.op_distance;

		if (s < best_score) {
			best_score = s;
			best_pos = pos;
		}

	}

	if (best_pos != xy()) {
		double best_d = std::numeric_limits<double>::infinity();
		find_nearby_entirely_walkable_tiles(best_pos, [&](xy npos) {
			double d = unit_pathing_distance(unit_types::scv, npos, my_closest_base);
			if (d < best_d) {
				best_d = d;
				best_pos = npos;
			}
			return map_split_my_area.test(grid::build_square_index(npos));
		}, 32 * 20);
	}

	map_split_center_pos = best_pos;

}


void generate_map_split_task() {

	multitasking::sleep(15);

	while (true) {

		generate_map_split();

		multitasking::sleep(90);
	}

}

void scan_for_mines_task() {

	multitasking::sleep(15 * 60 * 10);

	int next_scan_allowed_at = 0;

	while (true) {

		auto add = [&](xy pos, int radius, int value) {
			for (int iy = -radius; iy <= radius; iy += 32) {
				for (int ix = -radius; ix <= radius; ix += 32) {
					int x = pos.x + ix;
					int y = pos.y + iy;
					if ((size_t)x >= (size_t)grid::map_width || (size_t)y >= (size_t)grid::map_height) continue;
					size_t index = grid::build_square_index(xy(x, y));
					auto& v = mine_threat_area[index];
					int nv = v + value;
					if (nv > 255) nv = 255;
					else if (nv < 0) nv = 0;
					v = nv;
				}
			}
			size_t index = grid::build_square_index(pos);
			if (mine_threat_area[index] >= 255 && scans_available > 1) {
				if (current_frame > next_scan_allowed_at) {
					unit*u = get_best_score(my_units_of_type[unit_types::comsat_station], [&](unit*u) {
						return -u->energy;
					});
					if (u && u->energy >= 50) {
						auto anyone_in_range = [&]() {
							for (unit* u : my_units) {
								if (u->type->is_non_usable) continue;
								if (u->type->is_hovering) continue;
								if (u->is_flying) continue;
								weapon_stats* w = u->stats->ground_weapon;
								if (!w) continue;
								if (diag_distance(u->pos - pos) > w->max_range) continue;
								return true;
							}
							return false;
						};
						if (anyone_in_range()) {
							next_scan_allowed_at = current_command_frame + 90;
							//printf("spider mines scan at %d %d\n", pos.x, pos.y);
							u->game_unit->useTech(BWAPI::TechTypes::Scanner_Sweep, BWAPI::Position(pos.x, pos.y));
						}
					}
				}
			}
		};

		for (unit* u : my_detector_units) {
			add(u->pos, (int)u->stats->sight_range, -256);
		}

		for (unit* e : enemy_units) {
			if (e->gone) continue;
			if (e->type == unit_types::vulture) add(e->pos, 32 * 4, 16);
			else if (e->type == unit_types::spider_mine) add(e->pos, 32 * 4, 64);
		}

// 		xy screen_pos = bwapi_pos(game->getScreenPosition());
 
// 		for (int y = screen_pos.y & -32; y < screen_pos.y + 400; y += 32) {
// 			for (int x = screen_pos.x & -32; x < screen_pos.x + 640; x += 32) {
// 				if ((size_t)x >= (size_t)grid::map_width || (size_t)y >= (size_t)grid::map_height) break;
 
// 				size_t index = grid::build_square_index(xy(x, y));
// 				int v = mine_threat_area[index];
// 				if (v) game->drawBoxMap(x + 1, y + 1, x + 32 - 1, y + 32 - 1, BWAPI::Color(v, 0, 0));
// 			}
// 		}

		multitasking::sleep(8);
	}

}

void clear_mines_for_builder_task() {
	multitasking::sleep(15 * 60 * 10);
	
	a_unordered_map<unit*, refcounter<combat_unit>> units;
	
	while (true) {
		
		for (auto i = units.begin(); i != units.end();) {
			if (i->second->u->dead || entire_threat_area.test(grid::build_square_index(i->second->u->pos))) i = units.erase(i);
			else ++i;
		}
		
		a_unordered_map<unit*, refcounter<combat_unit>> new_units;
		
		for (unit* u : my_workers) {
			if (u->controller->action != unit_controller::action_build) continue;
			if (u->controller->fail_build_count >= 3) {
				
				bool found = false;
				for (auto& v : units) {
					if (v.first == u) {
						new_units[u] = std::move(v.second);
						found = true;
						break;
					}
				}
				
				if (!found) {
					for (auto* a : live_combat_units) {
						if (current_frame < a->strategy_busy_until) continue;
						if (a->u->type == unit_types::zergling || a->u->type == unit_types::zealot || a->u->type == unit_types::hydralisk) {
							auto h = combat::request_control(a);
							if (h) {
								new_units[u] = std::move(h);
							}
						}
					}
				}
			}
		}
		
		units = std::move(new_units);
		new_units.clear();
		
		for (auto& v : units) {
			unit* u = v.first;
			auto* a = *v.second;
			
			a->subaction = combat::combat_unit::subaction_move;
			a->target_pos = u->pos;
			
			for (unit* u : visible_enemy_units) {
				if (u->type == unit_types::spider_mine && units_distance(u, a->u) <= 96 && u->order_target == a->u) {
					xy relpos = a->u->pos - u->pos;
					double d = sc_distance(relpos);
					a->target_pos = a->u->pos + xy(relpos.x / d * 96, relpos.y / d * 96);
				}
			}
			
		}
		
		if (units.empty()) {
			multitasking::sleep(60);
		} else {
			multitasking::sleep(3);
		}
		
	}
	
}

void update_run_score() {
	multitasking::sleep(10);

	while (true) {

		tsc::high_resolution_timer ht;

		std::fill(run_score.begin(), run_score.end(), 100000);

		struct node_t {
			xy pos;
			int distance;
		};

		a_deque<node_t> open;

		for (unit* u : my_resource_depots) {
			open.push_back({u->pos, 0});
			run_score[grid::build_square_index(u->pos)] = 0;
		}

		while (!open.empty()) {

			node_t cur = open.front();
			open.pop_front();

			auto add = [&](xy npos) {
				int d = cur.distance + sc_distance(npos);
				npos += cur.pos;
				if ((size_t)npos.x >= (size_t)grid::map_width) return;
				if ((size_t)npos.y >= (size_t)grid::map_height) return;

				grid::build_square& bs = grid::get_build_square(npos);

				if (!bs.entirely_walkable) return;
				if (bs.building && bs.building->owner == players::neutral_player) return;

				size_t index = grid::build_square_index(npos);
				if (run_score[index] != 100000) return;
				run_score[index] = d;

				open.push_back({npos, d});

			};

			add(xy(32, 0));
			add(xy(32, 32));
			add(xy(0, 32));
			add(xy(-32, 32));
			add(xy(-32, 0));
			add(xy(-32, -32));
			add(xy(0, -32));
			add(xy(32, -32));

		}

		log(log_level_test, " run_scores updated in %gms\n", ht.elapsed() * 1000.0);

		multitasking::sleep(120);
	}

}

// void air_nodes_task() {
// 	create_air_nodes();
// 	while (true) {
// 
// 		for (air_node& n : all_air_nodes) {
// 			n.cost = 0.0;
// 		}
// 		for (air_node& n : all_air_nodes) {
// 			if (entire_threat_area.test(grid::build_square_index(n.pos))) {
// 				n.cost += 32 * 20;
// 				for (air_node* nn : n.neighbors) {
// 					nn->cost += 32 * 10;
// 				}
// 			}
// 		}
// 
// 		multitasking::sleep(30);
// 	}
// }

void render() {

	xy screen_pos = bwapi_pos(game->getScreenPosition());

	for (auto& v: defence_spots) {

		game->drawCircleMap(v.pos.x, v.pos.y, 64, BWAPI::Colors::Green);
		game->drawTextMap(v.pos.x, v.pos.y, "weight %g", v.weight);

	}

	for (auto* cs : active_control_spots) {

		game->drawCircleMap(cs->pos.x, cs->pos.y, 64, BWAPI::Colors::Red);
		game->drawTextMap(cs->pos.x, cs->pos.y, "weight %g control %g", cs->weight, cs->control_value);
		if (cs->reachable) game->drawTextMap(cs->pos.x, cs->pos.y + 12, "reachable");
		else game->drawTextMap(cs->pos.x, cs->pos.y + 12, "not reachable");

		game->drawCircleMap(cs->incoming_pos.x, cs->incoming_pos.y, 8, BWAPI::Colors::Red);

		BWAPI::Color color = cs->control_value < 0.0 ? BWAPI::Color(255, 0, 0) : BWAPI::Color(64, 0, 0);

		for (int y = screen_pos.y & -32; y < screen_pos.y + 400; y += 32) {
			for (int x = screen_pos.x & -32; x < screen_pos.x + 640; x += 32) {
				if ((size_t)x >= (size_t)grid::map_width || (size_t)y >= (size_t)grid::map_height) break;

				size_t index = grid::build_square_index(xy(x, y));
				if (cs->area.test(index)) {
					game->drawBoxMap(x + 4, y + 4, x + 32 - 4, y + 32 - 4, color);
				}

			}
		}

	}

	for (int y = screen_pos.y & -32; y < screen_pos.y + 400; y += 32) {
		for (int x = screen_pos.x & -32; x < screen_pos.x + 640; x += 32) {
			if ((size_t)x >= (size_t)grid::map_width || (size_t)y >= (size_t)grid::map_height) break;

			BWAPI::Color color = BWAPI::Color(64, 64, 0);

			size_t index = grid::build_square_index(xy(x, y));
			int n = enemy_siege_tank_area[index];
			if (n) game->drawTextMap(x, y, "%d", n);

		}
	}

// 	for (int y = screen_pos.y & -32; y < screen_pos.y + 400; y += 32) {
// 		for (int x = screen_pos.x & -32; x < screen_pos.x + 640; x += 32) {
// 			if ((size_t)x >= (size_t)grid::map_width || (size_t)y >= (size_t)grid::map_height) break;
// 
// 			size_t index = grid::build_square_index(xy(x, y));
// 			if (map_split_my_area.test(index)) {
// 				game->drawBoxMap(x + 1, y + 1, x + 32 - 1, y + 32 - 1, BWAPI::Color(0, 64, 192));
// 			}
// 			if (map_split_op_area.test(index)) {
// 				game->drawBoxMap(x + 1, y + 1, x + 32 - 1, y + 32 - 1, BWAPI::Color(192, 64, 0));
// 			}
// 
// 		}
// 	}

	for (auto& g : groups) {
		if (!g.is_aggressive_group) continue;
		for (auto* c : g.allies) {
			if (c->assigned_control_spot) {
				game->drawLineMap(c->u->pos.x, c->u->pos.y, c->assigned_control_spot->pos.x, c->assigned_control_spot->pos.y, BWAPI::Colors::Orange);
			}

			bool okay = true;
			if (current_frame < c->strategy_busy_until) okay = false;
			if (c->reference_count) okay = false;
			if (c->subaction != combat_unit::subaction_move) okay = false;
			if (current_frame - c->last_run <= 15 * 5) okay = false;

			if (okay) game->drawCircleMap(c->u->pos.x, c->u->pos.y, 32, BWAPI::Colors::Green);
			else game->drawCircleMap(c->u->pos.x, c->u->pos.y, 32, BWAPI::Colors::Red);

			if (g.is_aggressive_group) render::draw_stacked_text(c->u->pos + xy(0, -20), "aggressive");
			if (g.is_defensive_group) render::draw_stacked_text(c->u->pos + xy(0, -20), "defensive");

			render::draw_stacked_text(c->u->pos + xy(0, -20), format("win ratio %g", c->last_win_ratio));
		}

	}

// 	BWAPI::Position screen_pos = game->getScreenPosition();
// 	for (int y = screen_pos.y; y < screen_pos.y + 400; y += 32) {
// 		for (int x = screen_pos.x; x < screen_pos.x + 640; x += 32) {
// 			if ((size_t)x >= (size_t)grid::map_width || (size_t)y >= (size_t)grid::map_height) break;
// 
// 			x &= -32;
// 			y &= -32;
// 
// 			size_t index = grid::build_square_index(xy(x, y));
// 			if (!my_base.test(index)) {
// 				game->drawBoxMap(x, y, x + 32, y + 32, BWAPI::Colors::Green);
// 			}
// 
// 		}
// 	}

// 	for (auto&g : groups) {
// 		xy pos = g.enemies.front()->pos;
// 		for (unit*e : g.enemies) {
// 			game->drawLineMap(e->pos.x, e->pos.y, pos.x, pos.y, BWAPI::Colors::Red);
// 		}
// 		for (auto*cu : g.allies) {
// 			game->drawLineMap(cu->u->pos.x, cu->u->pos.y, pos.x, pos.y, BWAPI::Colors::Green);
// 		}
// 	}

	for (auto&g : groups) {
		for (size_t idx : g.threat_area) {
			xy pos((idx % (size_t)grid::build_grid_width) * 32, (idx / (size_t)grid::build_grid_width) * 32);
			if (pos.x < screen_pos.x || pos.y < screen_pos.y) continue;
			if (pos.x + 32 >= screen_pos.x + 640 || pos.y + 32 >= screen_pos.y + 400) continue;
			
			game->drawBoxMap(pos.x, pos.y, pos.x + 32, pos.y + 32, BWAPI::Colors::Brown, false);
		}
	}

	for (auto&v : nuke_test) {
		xy pos = std::get<0>(v);
		double val = std::get<1>(v);
		game->drawTextMap(pos.x, pos.y, "%g", val);
	}

	if (true) {
		bwapi_pos screen_pos = game->getScreenPosition();
		xy lp;
		for (auto&v : defence_choke.path) {
			if (v.x < screen_pos.x || v.y < screen_pos.y) continue;
			if (v.x >= screen_pos.x + 640 || v.y >= screen_pos.y + 400) continue;
			if (lp != xy()) game->drawLineMap(lp.x, lp.y, v.x, v.y, BWAPI::Colors::Yellow);
			lp = v;
		}
		for (auto*bs : defence_choke.build_squares) {
			xy pos = bs->pos;
			game->drawBoxMap(pos.x, pos.y, pos.x + 32, pos.y + 32, BWAPI::Colors::Yellow);
		}
		game->drawCircleMap(defence_choke.inside.x, defence_choke.inside.y, 16, BWAPI::Color(128, 255, 0));
		game->drawCircleMap(defence_choke.center.x, defence_choke.center.y, 12, BWAPI::Color(255, 255, 0));
		game->drawCircleMap(defence_choke.outside.x, defence_choke.outside.y, 16, BWAPI::Color(255, 128, 0));


		for (auto*bs : defence_choke.inside_squares) {
			xy pos = bs->pos;
			game->drawBoxMap(pos.x, pos.y, pos.x + 32, pos.y + 32, BWAPI::Color(0, 128, 0));
		}
		for (auto*bs : defence_choke.outside_squares) {
			xy pos = bs->pos;
			game->drawBoxMap(pos.x, pos.y, pos.x + 32, pos.y + 32, BWAPI::Color(128, 0, 0));
		}

		lp = xy();
		for (auto&v : dont_siege_here_path) {
			if (v.x < screen_pos.x || v.y < screen_pos.y) continue;
			if (v.x >= screen_pos.x + 640 || v.y >= screen_pos.y + 400) continue;
			if (lp != xy()) game->drawLineMap(lp.x, lp.y, v.x, v.y, BWAPI::Color(192, 192, 0));
			lp = v;
		}
		for (auto& vi : inter_base_paths) {
			lp = xy();
			for (auto& v : vi) {
				auto inside = [&](xy pos) {
					if (pos.x < screen_pos.x || pos.y < screen_pos.y) return false;
					if (pos.x >= screen_pos.x + 640 || pos.y >= screen_pos.y + 400) return false;
					return true;
				};
				if (lp != xy() && (inside(lp) || inside(v))) game->drawLineMap(lp.x, lp.y, v.x, v.y, BWAPI::Color(192, 192, 80));
				lp = v;
			}
		}
	}
// 	if (true) {
// 		bwapi_pos screen_pos = game->getScreenPosition();
// 		for (int y = screen_pos.y&-32; y < screen_pos.y + 400; y += 32) {
// 			for (int x = screen_pos.x&-32; x < screen_pos.x + 640; x += 32) {
// 				if ((size_t)x >= (size_t)grid::map_width || (size_t)y >= (size_t)grid::map_height) continue;
// 				if (build_square_taken.test(grid::build_square_index(xy(x, y)))) {
// 					game->drawBoxMap(x, y, x + 32, y + 32, BWAPI::Colors::White);
// 				}
// 			}
// 
// 		}
// 	}

	if (true) {

		for (auto&v : focus_fire) {
			game->drawTextMap(v.first->pos.x, v.first->pos.y, "\x08%g", v.second);
		}

	}

	for (int y = screen_pos.y; y < screen_pos.y + 400; y += 32) {
		for (int x = screen_pos.x; x < screen_pos.x + 640; x += 32) {
			if ((size_t)x >= (size_t)grid::map_width || (size_t)y >= (size_t)grid::map_height) break;

			x &= -32;
			y &= -32;

			size_t index = grid::build_square_index(xy(x, y));
			if (dark_swarm_grid.test(index)) {
				game->drawBoxMap(x, y, x + 32, y + 32, BWAPI::Color(128, 128, 0));
			}

		}
	}

//	for (auto& v : use_dark_swarm_please) {
//		game->drawLineMap(v.first->u->pos.x, v.first->u->pos.y, v.second.x, v.second.y, BWAPI::Colors::Orange);
//
//		game->drawLineMap(v.first->u->pos.x + 32, v.first->u->pos.y, v.second.x, v.second.y, BWAPI::Colors::Orange);
//		game->drawLineMap(v.first->u->pos.x, v.first->u->pos.y + 32, v.second.x, v.second.y, BWAPI::Colors::Orange);
//		game->drawLineMap(v.first->u->pos.x - 32, v.first->u->pos.y, v.second.x, v.second.y, BWAPI::Colors::Orange);
//		game->drawLineMap(v.first->u->pos.x, v.first->u->pos.y - 32, v.second.x, v.second.y, BWAPI::Colors::Orange);
//	}


	game->drawCircleMap(my_closest_base.x, my_closest_base.y, 32 * 3, BWAPI::Color(128, 128, 0));
	game->drawCircleMap(op_closest_base.x, op_closest_base.y, 32 * 3, BWAPI::Color(192, 128, 0));

// 	double highest_cost = 0.0;
// 	for (auto& v : all_air_nodes) {
// 		if (v.cost > highest_cost) highest_cost = v.cost;
// 	}
// 	for (auto& v : all_air_nodes) {
// 		if (v.pos.x < screen_pos.x || v.pos.x >= screen_pos.x + 640) continue;
// 		if (v.pos.y < screen_pos.y || v.pos.y >= screen_pos.y + 400) continue;
// 		double s = v.cost / highest_cost;
// 		int cv = (int)(s * 255);
// 		//game->drawCircleMap(v.pos.x, v.pos.y, 32 * 4, BWAPI::Color(32, 96, 192));
// 		game->drawCircleMap(v.pos.x, v.pos.y, 32 * 4, BWAPI::Color(cv, 0, 32));
// 	}
	
}

void debug_render() {
	while (true) {
		multitasking::sleep(1);

		xy screen_pos = bwapi_pos(game->getScreenPosition());
		for (int y = screen_pos.y & -32; y < screen_pos.y + 400; y += 32) {
			for (int x = screen_pos.x & -32; x < screen_pos.x + 640; x += 32) {
				if ((size_t)x >= (size_t)grid::map_width || (size_t)y >= (size_t)grid::map_height) break;

				size_t index = grid::build_square_index(xy(x, y));
				if (entire_threat_area_edge.test(index)) {
					game->drawBoxMap(x + 1, y + 1, x + 32 - 1, y + 32 - 1, BWAPI::Color(0, 64, 192));
				}
			}
		}

		for (auto& v : use_dark_swarm_please) {
			game->drawLineMap(v.first->u->pos.x, v.first->u->pos.y, v.second.x, v.second.y, BWAPI::Colors::Orange);

			game->drawLineMap(v.first->u->pos.x + 32, v.first->u->pos.y, v.second.x, v.second.y, BWAPI::Colors::Orange);
			game->drawLineMap(v.first->u->pos.x, v.first->u->pos.y + 32, v.second.x, v.second.y, BWAPI::Colors::Orange);
			game->drawLineMap(v.first->u->pos.x - 32, v.first->u->pos.y, v.second.x, v.second.y, BWAPI::Colors::Orange);
			game->drawLineMap(v.first->u->pos.x, v.first->u->pos.y - 32, v.second.x, v.second.y, BWAPI::Colors::Orange);
		}
		
		for (auto& v : use_plague_please) {
			game->drawLineMap(v.first->u->pos.x, v.first->u->pos.y, v.second.x, v.second.y, BWAPI::Colors::Red);

			game->drawLineMap(v.first->u->pos.x + 32, v.first->u->pos.y, v.second.x, v.second.y, BWAPI::Colors::Red);
			game->drawLineMap(v.first->u->pos.x, v.first->u->pos.y + 32, v.second.x, v.second.y, BWAPI::Colors::Red);
			game->drawLineMap(v.first->u->pos.x - 32, v.first->u->pos.y, v.second.x, v.second.y, BWAPI::Colors::Red);
			game->drawLineMap(v.first->u->pos.x, v.first->u->pos.y - 32, v.second.x, v.second.y, BWAPI::Colors::Red);
		}
		
		for (auto& v : active_storms) {
			xy pos = v.second;
			game->drawBoxMap(pos.x - 48, pos.y - 48, pos.x + 48, pos.y + 48, BWAPI::Color(0, 0, 200));
		}

	}
}

void init() {

	my_base.resize(grid::build_grid_width*grid::build_grid_height);
	op_base.resize(grid::build_grid_width*grid::build_grid_height);

	my_base_defence.resize(grid::build_grid_width*grid::build_grid_height);

	entire_threat_area.resize(grid::build_grid_width*grid::build_grid_height);
	entire_threat_area_edge.resize(grid::build_grid_width*grid::build_grid_height);
	transfer_unsafe_area.resize(grid::build_grid_width*grid::build_grid_height);
	my_units_area.resize(grid::build_grid_width*grid::build_grid_height);
	build_square_taken.resize(grid::build_grid_width*grid::build_grid_height);
	dont_siege_here.resize(grid::build_grid_width*grid::build_grid_height);
	dark_swarm_grid.resize(grid::build_grid_width*grid::build_grid_height);
	existing_dark_swarm.resize(grid::build_grid_width*grid::build_grid_height);
	defence_choke_outside_bitset.resize(grid::build_grid_width*grid::build_grid_height);

	enemy_siege_tank_area.resize(grid::build_grid_width * grid::build_grid_height);

	map_split_my_area.resize(grid::build_grid_width * grid::build_grid_height);
	map_split_op_area.resize(grid::build_grid_width * grid::build_grid_height);
	map_split_border_area.resize(grid::build_grid_width * grid::build_grid_height);
	
	mine_threat_area.resize(grid::build_grid_width * grid::build_grid_height);

	run_score.resize(grid::build_grid_width * grid::build_grid_height);

	if (!game->isReplay()) {
		multitasking::spawn(combat_task, "combat");
		multitasking::spawn(update_combat_groups_task, "update combat groups");
		multitasking::spawn(update_defence_choke_task, "update defence choke spot");
		multitasking::spawn(update_aoe_spellcasting, "update aoe spellcasting");
		multitasking::spawn(drops_task, "drops");

		multitasking::spawn(generate_defence_spots_task, "generate defence spots");

		multitasking::spawn(generate_control_spots_task, "generate control spots");

		multitasking::spawn(generate_siege_tank_area_task, "generate siege tank area");
		
		multitasking::spawn(generate_map_split_task, "generate map split");

		multitasking::spawn(update_best_defence_pos_task, "update best defence pos");

		multitasking::spawn(scan_for_mines_task, "scan for mines");
		
		multitasking::spawn(clear_mines_for_builder_task, "clear mines for builder");

		multitasking::spawn(update_run_score, "update run score");

		//multitasking::spawn(air_nodes_task, "air nodes");

		if (output_enabled) {
			multitasking::spawn(0.1, debug_render, "debug render");
		}
	}

	render::add(render);

// 	combat_eval::eval eval;
// 	eval.teams[1].wait_until_attacked = true;
// 
// 	//for (int i = 0; i < 14; ++i) eval.add_unit(units::get_unit_stats(unit_types::probe, players::my_player), 0);
// 	for (int i = 0; i < 2; ++i) eval.add_unit(units::get_unit_stats(unit_types::zealot, players::my_player), 0);
// 	for (int i = 0; i < 8; ++i) eval.add_unit(units::get_unit_stats(unit_types::dragoon, players::my_player), 0);
// 
// 	for (int i = 0; i < 4; ++i) eval.add_unit(units::get_unit_stats(unit_types::marine, players::opponent_player), 1);
// 	for (int i = 0; i < 5; ++i) eval.add_unit(units::get_unit_stats(unit_types::medic, players::opponent_player), 1);
// 	for (int i = 0; i < 2; ++i) eval.add_unit(units::get_unit_stats(unit_types::firebat, players::opponent_player), 1);
// 	for (int i = 0; i < 3; ++i) eval.add_unit(units::get_unit_stats(unit_types::siege_tank_siege_mode, players::opponent_player), 1);
// 
// 	eval.run();
// 
// 	{
// 
// 		log("result: score %g %g  supply %g %g  damage %g %g  in %d frames\n", eval.teams[0].score, eval.teams[1].score, eval.teams[0].end_supply, eval.teams[1].end_supply, eval.teams[0].damage_dealt, eval.teams[1].damage_dealt, eval.total_frames);
// 	}

	//xcept("stop");

}

}
