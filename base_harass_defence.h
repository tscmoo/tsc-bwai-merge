
namespace base_harass_defence {

struct defender {
	refcounter<combat::combat_unit> h;
	combat::combat_unit* a = nullptr;
	xy target_pos;
	bool is_anti_air = false;
};

a_list<defender> defenders;

bool defend_enabled = true;

void move_defender(defender& d) {
	
	log(log_level_test, " moving defender %p to %d %d\n", d.a, d.target_pos.x, d.target_pos.y);
	
	auto* a = d.a;
	
	game->drawLineMap(a->u->pos.x, a->u->pos.y, d.target_pos.x, d.target_pos.y, BWAPI::Colors::Orange);
	
	a->subaction = combat::combat_unit::subaction_move;
	a->target_pos = d.target_pos;
	
	bool is_at_target_pos = diag_distance(a->u->pos - d.target_pos) <= 16;
	
	auto* target_building = grid::get_build_square(d.target_pos).building;
	if (target_building) {
		is_at_target_pos = units_distance(a->u, target_building) <= 16;
	}
	
	if (is_at_target_pos) a->target_pos = a->u->pos;
	
	if (a->u->type == unit_types::siege_tank_siege_mode) return;
	
	a_vector<unit*> enemies;
	for (unit* u : enemy_units) {
		if (!u->visible) continue;
		if (a->u->type == unit_types::siege_tank_siege_mode && u->is_flying) continue;
		if (diag_distance(u->pos - d.target_pos) >= 32 * 14) continue;
		//if (diag_distance(u->pos - a->u->pos) >= 32 * 14) continue;
		enemies.push_back(u);
	}
	
	a_vector<combat::combat_unit*> allies;
	allies.push_back(a);
	
	if (!enemies.empty()) {
		if (a->u->type == unit_types::siege_tank_tank_mode) combat::do_tank_line(allies, enemies, false, nullptr);
		else {
			unit* target = get_best_score(enemies, [&](unit* e) {
				auto* w = e->is_flying ? a->u->stats->air_weapon : a->u->stats->ground_weapon;
				if (!w) return std::numeric_limits<double>::infinity();
				return std::max(units_distance(a->u, e) - w->max_range, 0.0) + (e->shields + e->hp) / 50.0;
			});
			if (target) {
				a->subaction = combat::combat_unit::subaction_fight;
				a->target = target;
			}
		}
	}
	
	if (a->subaction == combat::combat_unit::subaction_move && diag_distance(a->u->pos - a->target_pos) > 32 * 4 && !a->u->is_flying) {
		auto nodes = square_pathing::get_nearest_path_nodes(square_pathing::get_pathing_map(a->u->type), a->u->pos, a->target_pos);
		auto* start_node = nodes.first;
		auto* end_node = nodes.second;
		if (start_node != end_node) {
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
					if (n == end_node) {
						goal_node = n;
						break;
					}
					size_t index = grid::build_square_index(n->pos);
					bool safe = true;
					for (auto& ng : combat::groups) {
						if (ng.ground_dpf >= 0.5) {
							if (ng.threat_area.test(index)) {
								safe = false;
								break;
							}
						}
					}
					if (!safe) continue;
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
	
}

a_map<xy, double> area_air_hp;

int last_clear_area_air_hp = 0;

void process_defenders() {
	
	if (current_frame - last_clear_area_air_hp >= 15 * 30) {
		last_clear_area_air_hp = current_frame;
		area_air_hp.clear();
	}
	
	a_vector<combat::combat_unit*> all_tanks;
	a_vector<combat::combat_unit*> all_anti_air;
	for (auto* a : combat::live_combat_units) {
		if (current_frame < a->strategy_busy_until) continue;
		if (a->u->type == unit_types::siege_tank_tank_mode || a->u->type == unit_types::siege_tank_siege_mode) {
			all_tanks.push_back(a);
		}
		if (!a->u->is_flying && a->u->stats->air_weapon) {
			all_anti_air.push_back(a);
		}
	}
	
	if (all_tanks.size() < 4) {
		bool enemy_has_reaver_or_shuttle = false;
		for (unit* e : enemy_units) {
			if (e->type == unit_types::reaver || e->type == unit_types::shuttle) {
				enemy_has_reaver_or_shuttle = true;
				break;
			}
		}
		if (!enemy_has_reaver_or_shuttle) {
			defenders.clear();
			return;
		}
	}
	
	a_list<defender> new_defenders;
	
	auto cur_st = buildpred::get_my_current_state();
	
	std::function<defender*(xy)> get_defender = [&](xy pos) {
		for (auto& v : defenders) {
			if (v.a->u->dead) continue;
			if (v.target_pos == pos && !v.is_anti_air) {
				new_defenders.push_back(std::move(v));
				return &new_defenders.back();
			}
		}
		auto* a = get_best_score(all_tanks, [&](auto* a) {
			return diag_distance(a->u->pos - pos);
		});
		if (!a) return (defender*)nullptr;
		auto h = combat::request_control(a);
		if (!h) {
			all_tanks.erase(std::find(all_tanks.begin(), all_tanks.end(), a));
			return get_defender(pos);
		}
		new_defenders.emplace_back();
		auto& n = new_defenders.back();
		n.h = std::move(h);
		n.a = a;
		n.target_pos = pos;
		return &n;
	};
	
	auto needs_anti_air = [&](xy pos) {
		double& hp = area_air_hp[pos];
		if (hp != 0.0) return true;
		
		for (unit* e : enemy_units) {
			if (e->gone) continue;
			if (e->is_flying && diag_distance(e->pos - pos) <= 32 * 14) hp += e->shields + e->hp;
		}
		
		return hp != 0.0;
	};
	
	std::function<void(xy, double)> get_aa_defenders = [&](xy pos, double hp) {
		for (auto& v : defenders) {
			if (v.a->u->dead) continue;
			if (v.target_pos == pos && v.is_anti_air) {
				new_defenders.push_back(std::move(v));
				return;
			}
		}
		auto* a = get_best_score(all_anti_air, [&](auto* a) {
			return diag_distance(a->u->pos - pos);
		});
		if (!a) {
			area_air_hp.clear();
			return;
		}
		auto h = combat::request_control(a);
		if (!h) {
			all_anti_air.erase(std::find(all_anti_air.begin(), all_anti_air.end(), a));
			return get_aa_defenders(pos, hp);
		}
		new_defenders.emplace_back();
		auto& n = new_defenders.back();
		n.h = std::move(h);
		n.a = a;
		n.target_pos = pos;
		n.is_anti_air = true;
		auto* w = a->u->stats->air_weapon;
		if (w) hp -= w->damage * a->u->stats->air_weapon_hits * 15 * 10 / w->cooldown;
		if (hp > 0.0) return get_aa_defenders(pos, hp);
	};
	
	if (defend_enabled) {
		for (auto& v : cur_st.bases) {
			xy pos = v.s->pos;
			get_defender(pos);
			
			if (needs_anti_air(pos)) {
				get_aa_defenders(pos, area_air_hp[pos]);
			}
		}
	}
	
	defenders = std::move(new_defenders);
	
	for (auto& v : defenders) {
		move_defender(v);
	}
	
}

void base_harass_defence_task() {
	
	while (true) {
		
		process_defenders();
		
		multitasking::sleep(15);
	}
}

a_vector<refcounter<combat::combat_unit>> scarab_dodgers;

void dodge_scarabs() {
	
	scarab_dodgers.clear();
	
	for (unit* e : visible_enemy_units) {
		if (e->type == unit_types::scarab || e->type == unit_types::reaver) {
			if (e->target && e->target->owner == players::my_player) {
				auto i = combat::combat_unit_map.find(e->target);
				if (i != combat::combat_unit_map.end()) {
					game->drawLineMap(e->pos.x, e->pos.y, e->target->pos.x, e->target->pos.y, BWAPI::Colors::Red);
					auto* a = &i->second;
					if (a->u == e->target && a->u->hp <= 100 && a->u->type->total_minerals_cost < 100) {
						auto h = combat::request_control(a);
						if (h) {
							xy best_pos;
							double best_d = 0.0;
							combat::find_path(a->u->type, a->u->pos, [&](xy pos, xy npos) {
								double d = diag_distance(npos - a->u->pos);
								if (d >= 32 * 8) return false;
								if (d >= 32 * 2) {
									double d2 = diag_distance(npos - e->pos);
									if (d2 > best_d) {
										best_pos = npos;
										best_d = d2;
									}
								}
								return true;
							}, [&](xy pos, xy npos) {
								return 0.0;
							}, [&](xy pos) {
								return false;
							});
							
							if (best_pos != xy()) {
								scarab_dodgers.push_back(h);
								
								a->subaction = combat::combat_unit::subaction_move;
								a->target_pos = best_pos;
							}
						}
					}
				}
			}
		}
	}
	
}

void dodge_scarabs_task() {
	
	while (true) {
		
		dodge_scarabs();
		
		multitasking::sleep(2);
	}
	
}

void init() {

	multitasking::spawn(base_harass_defence_task, "base harass defence");
	multitasking::spawn(dodge_scarabs_task, "dodge scarabs");
	
}

}
