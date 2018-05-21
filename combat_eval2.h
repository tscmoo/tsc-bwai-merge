
namespace combat_eval2 {
;


struct combatant {

	std::pair<combatant*, combatant*> alive_link;
	std::pair<combatant*, combatant*> team_alive_link;

	int id = -1;

	unit_stats*st = nullptr;
	unit_type*type = nullptr;

	int team = 0;
	xy pos;
	double energy = 0.0;
	double shields = 0.0;
	double hp = 0.0;
	int cooldown_until = 0;
	int wait_until = 0;
	double move_frames = 0.0;
	int marines_loaded = 0;
	int spider_mine_count = 0;

	combatant*target = nullptr;

	a_deque<xy> path;
	int last_find_path = 0;
	int last_find_target = 0;
	int last_find_nodes_path = 0;
	xy find_nodes_path_dst;

};

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


struct eval {

	struct team_t {
		a_deque<combatant> units;
		double resources_lost = 0.0;
		double resources_value = 0.0;
		bool run_away = false;
		bool has_stim = false;
		bool has_spider_mines = false;
		weapon_stats* spider_mine_weapon = nullptr;
	};

	std::array<team_t, 2> teams;
	int total_frames = 0;
	int max_frames = 0;
	int next_unit_id = 0;

	eval() {
		max_frames = 15 * 40;
		//max_frames = 15 * 120;

		teams[0].has_stim = players::my_player->has_upgrade(upgrade_types::stim_packs);
		teams[1].has_stim = players::opponent_player->has_upgrade(upgrade_types::stim_packs);
		teams[0].has_spider_mines = players::my_player->has_upgrade(upgrade_types::spider_mines);
		teams[1].has_spider_mines = players::opponent_player->has_upgrade(upgrade_types::spider_mines);
		teams[0].spider_mine_weapon = units::get_weapon_stats(BWAPI::WeaponTypes::Spider_Mines, players::my_player);
		teams[1].spider_mine_weapon = units::get_weapon_stats(BWAPI::WeaponTypes::Spider_Mines, players::opponent_player);
	}

	combatant&add(unit_stats*st, int team) {
		teams[team].units.emplace_back();
		auto&c = teams[team].units.back();

		c.id = next_unit_id++;
		c.st = st;
		c.type = st->type;
		c.team = team;

		if (st->type == unit_types::vulture) c.spider_mine_count = 3;

		return c;
	}
	
	combatant&add(unit* u, int team) {
		auto&c = add(u->stats, team);

		c.pos = u->pos;
		c.energy = u->energy;
		c.shields = u->shields;
		c.hp = u->hp;
		c.cooldown_until = u->weapon_cooldown;

		c.spider_mine_count = u->spider_mine_count;

// 		c.pos.x &= -8;
// 		c.pos.x += c.type->dimension_left() % 8;
// 		c.pos.y &= -8;
// 		c.pos.y += c.type->dimension_up() % 8;

		return c;
	}

	void run() {

		tsc::intrusive_list<combatant, &combatant::alive_link> alive;
		tsc::intrusive_list<combatant, &combatant::team_alive_link> team_alive[2];

		for (auto&t : teams) {
			for (auto&v : t.units) {
				alive.push_back(v);
				team_alive[v.team].push_back(v);
			}
		}

		weapon_stats*marine_weapon[2];
		marine_weapon[0] = units::get_weapon_stats(BWAPI::WeaponTypes::Gauss_Rifle, players::my_player);
		marine_weapon[1] = units::get_weapon_stats(BWAPI::WeaponTypes::Gauss_Rifle, players::opponent_player);

		int frame = 0;

		log("combat_eval2 running -- \n");

		a_vector<combatant*> path_units;

// 		json_value jv;
// 		auto&jv_frames = jv["frames"];

		tsc::high_resolution_timer ht;

		double total_pathfinding = 0.0;
		double sleep_time = 0.0;

		while (frame < max_frames) {
			multitasking::yield_point();

			//log("frame %d --\n", frame);

// 			auto&jv_frame = jv["frames"][frame];
// 
// 			auto&jv_units = jv_frame["units"];
// 			auto&jv_attacks = jv_frame["attacks"];
// 			for (auto&c : alive) {
// 				auto&jv_u = jv_units[c.id];
// 				jv_u["id"] = c.id;
// 				jv_u["type"] = c.type->name;
// 				jv_u["team"] = c.team;
// 				jv_u["x"] = c.pos.x;
// 				jv_u["y"] = c.pos.y;
// 				jv_u["energy"] = c.energy;
// 				jv_u["shields"] = c.shields;
// 				jv_u["hp"] = c.hp;
// 				jv_u["max_energy"] = c.st->energy;
// 				jv_u["max_shields"] = c.st->shields;
// 				jv_u["max_hp"] = c.st->hp;
// 				int cooldown = c.cooldown_until - frame;
// 				if (cooldown < 0) cooldown = 0;
// 				int wait = c.wait_until - frame;
// 				if (wait < 0) wait = 0;
// 				jv_u["cooldown"] = cooldown;
// 				jv_u["wait"] = wait;
// 				jv_u["move"] = c.move_frames;
// 
// 				jv_u["pos_left"] = c.pos.x - c.type->dimension_left();
// 				jv_u["pos_right"] = c.pos.x + c.type->dimension_right();
// 				jv_u["pos_top"] = c.pos.y - c.type->dimension_up();
// 				jv_u["pos_bottom"] = c.pos.y + c.type->dimension_down();
// 			}

// 			for (int i = 0; i < 2; ++i) {
// 				log("team %d - \n", i);
// 				for (auto&c : team_alive[i]) {
// 					int cooldown = c.cooldown_until - frame;
// 					if (cooldown < 0) cooldown = 0;
// 					int wait = c.wait_until - frame;
// 					if (wait < 0) wait = 0;
// 					log("  %d - %s at %d %d, energy %g, shields %g, hp %g, cooldown %d, wait %d, move %g\n", c.id, c.type->name, c.pos.x, c.pos.y, c.energy, c.shields, c.hp, cooldown, wait, c.move_frames);
// 				}
// 			}

			bool anyone_doing_anything = false;

			for (auto&c : alive) {
				if (c.wait_until > frame) continue;
				if (c.move_frames > 0.0) {
					c.move_frames -= 1.0;
					anyone_doing_anything = true;
					continue;
				}

				int my_team = c.team;
				int other_team = c.team ^ 1;

				bool is_in_range = false;

				if (c.target && !teams[my_team].run_away) {
					weapon_stats*w = c.target->type->is_flyer ? c.st->air_weapon : c.st->ground_weapon;
					int hits = w == c.st->air_weapon ? c.st->air_weapon_hits : c.st->ground_weapon_hits;
					if (c.st->type == unit_types::bunker && c.marines_loaded) {
						w = marine_weapon[my_team];
						hits = c.marines_loaded;
					}
					if (!w || c.target->hp <= 0.0) c.target = nullptr;
					else {
						double distance = units_distance(c.pos, c.type, c.target->pos, c.target->type);
						//log("%d (%s) distance to target %d (%s) is %g (weapon range %g-%g)\n", c.id, c.type->name, c.target->id, c.target->type->name, distance, w->min_range, w->max_range);
						if (distance <= w->max_range && distance >= w->min_range) {
							is_in_range = true;
							auto&other_team_alive = team_alive[other_team];
							if (frame >= c.cooldown_until) {

								combatant*best_target = get_best_score_p(other_team_alive, [&](combatant*cn) {
									weapon_stats*w = cn->type->is_flyer ? c.st->air_weapon : c.st->ground_weapon;
									if (!w || cn->hp <= 0.0) return std::numeric_limits<double>::infinity();
									double d = units_distance(c.pos, c.type, cn->pos, cn->type);
									if (d < w->min_range || d > w->max_range) return std::numeric_limits<double>::infinity();

									double hp = cn->hp / get_damage_type_modifier(w->damage_type, cn->type->size);
									return cn->shields + hp;
								}, std::numeric_limits<double>::infinity());
								if (best_target) c.target = best_target;

								int cooldown = w->cooldown;
								if (c.st->type == unit_types::marine && teams[my_team].has_stim) cooldown /= 2;
								if (c.st->type == unit_types::vulture && c.spider_mine_count && teams[my_team].has_spider_mines) {
									if (!c.target->st->type->is_building && c.target->st->type->size == unit_type::size_large) {
										cooldown = 15;
										w = teams[my_team].spider_mine_weapon;
									}
								}
								c.cooldown_until = frame + cooldown;
								if (c.st->type == unit_types::reaver) c.cooldown_until = frame + 60;
								if (c.st->type == unit_types::interceptor) c.cooldown_until = frame + 37;
								int wait_frames = 2;

								if (c.type == unit_types::marine) wait_frames = 7;
								if (c.type == unit_types::ghost) wait_frames = 6;
								if (c.type == unit_types::firebat) wait_frames = 7;
								if (c.type == unit_types::goliath) wait_frames = 6;

								if (c.type == unit_types::hydralisk) wait_frames = 6;

								if (c.type == unit_types::dragoon) wait_frames = 7;

								c.wait_until = frame + wait_frames;

								auto hit = [&](combatant*target, double damage) {
									double shield_damage_dealt = 0.0;
									double hp_damage_dealt = 0.0;
									double target_shields = target->shields;
									double target_hp = target->hp;
									if (target_shields > 0.0) {
										target_shields -= damage;
										if (target_shields < 0.0) {
											shield_damage_dealt = damage + target_shields;
											damage = -target_shields;
											target_shields = 0.0;
										} else {
											shield_damage_dealt = damage;
											damage = 0.0;
										}
										target->shields = target_shields;
									}
									if (damage) {
										hp_damage_dealt = damage * get_damage_type_modifier(w->damage_type, target->type->size);
										hp_damage_dealt -= target->st->armor;
										if (hp_damage_dealt < 1.0) hp_damage_dealt = 1.0;
										if (hp_damage_dealt > target_hp) hp_damage_dealt = target_hp;
										target_hp -= hp_damage_dealt;
										target->hp = target_hp;

										if (target_hp <= 0.0 && hp_damage_dealt) {
											teams[other_team].resources_lost += target->type->total_minerals_cost + target->type->total_gas_cost;
										}
									}

									//log(" %d (%s) attacked %d (%s) for %g/%g, %g/%g left\n", c.id, c.type->name, target->id, target->type->name, shield_damage_dealt, hp_damage_dealt, target_shields, target_hp);

//									size_t attack_idx = jv_attacks.vector.size();
// 									jv_attacks[attack_idx][0] = c.id;
// 									jv_attacks[attack_idx][1] = target->id;
// 									jv_attacks[attack_idx][2] = shield_damage_dealt;
// 									jv_attacks[attack_idx][3] = hp_damage_dealt;
								};

								//log("%d (%s) hits is %d\n", c.id, c.type->name, hits);

								for (int i = 0; i < hits; ++i) {

									if (w->explosion_type == weapon_stats::explosion_type_radial_splash || w->explosion_type == weapon_stats::explosion_type_enemy_splash) {

										bool can_hit_allies = true;
										if (w->explosion_type == weapon_stats::explosion_type_enemy_splash) can_hit_allies = false;
										if (w->explosion_type == weapon_stats::explosion_type_air_splash) can_hit_allies = false;

										auto weapon_can_target_unit = [](weapon_stats*w, combatant&c) {
											if (!w->targets_air && c.type->is_flyer) return false;
											if (!w->targets_ground && !c.type->is_flyer) return false;
											return true;
										};

										auto unit_distance = [](xy target, xy u_pos, std::array<int, 4> u_dimensions) {
											xy u0 = u_pos + xy(-u_dimensions[2], -u_dimensions[3]);
											xy u1 = u_pos + xy(u_dimensions[0], u_dimensions[1]);

											int x, y;
											if (target.x > u1.x) x = target.x - u1.x;
											else if (u0.x > target.x) x = u0.x - target.x;
											else x = 0;
											if (target.y > u1.y) y = target.y - u1.y;
											else if (u0.y > target.y) y = u0.y - target.y;
											else y = 0;

											return xy(x, y).length();
										};

										auto proc = [&](xy pos, combatant&ct) {
											if (&ct == &c) return false;
											if (!can_hit_allies && ct.team == c.team && &ct != c.target) return false;
											if (!weapon_can_target_unit(w, ct)) return false;
											if (ct.pos == pos) {
												hit(&ct, w->damage);
												return false;
											}
											if (unit_distance(pos, ct.pos, ct.type->dimensions) > w->outer_splash_radius) return false;
											if (unit_distance(pos, ct.pos, ct.type->dimensions) <= w->inner_splash_radius) {
												hit(&ct, w->damage);
												return false;
											}
											if (unit_distance(pos, ct.pos, ct.type->dimensions) <= w->median_splash_radius) {
												hit(&ct, w->damage / 2);
												return false;
											}
											hit(&ct, w->damage / 4);
											return false;
										};

										if (can_hit_allies) {
											for (auto&v : alive) proc(c.target->pos, v);
										} else {
											for (auto&v : other_team_alive) proc(c.target->pos, v);
										}
									
									} else {
										hit(c.target, w->damage);
									}


									
								}

								if (c.target->hp <= 0.0) {
									//log(" %d (%s) killed %d (%s)\n", c.id, c.type->name, c.target->id, c.target->type->name);
									c.target = nullptr;
								}

							}
						}
					}
				}

				if (!c.target || (!is_in_range && frame - c.last_find_target >= 15)) {
					c.last_find_target = frame;
					auto&other_team_alive = team_alive[other_team];
					c.target = get_best_score_p(other_team_alive, [&](combatant*cn) {
						if (teams[my_team].run_away) return units_distance(c.pos, c.type, cn->pos, cn->type);
						weapon_stats*w = cn->type->is_flyer ? c.st->air_weapon : c.st->ground_weapon;
						if (c.st->type == unit_types::bunker && c.marines_loaded) w = marine_weapon[my_team];
						if (!w || cn->hp <= 0.0) return std::numeric_limits<double>::infinity();
						double d = units_distance(c.pos, c.type, cn->pos, cn->type);
						if (d < w->min_range) return std::numeric_limits<double>::infinity();
						return d;
					}, std::numeric_limits<double>::infinity());
					//if (c.target) log("%d (%s) target set to %d (%s)\n", c.id, c.type->name, c.target->id, c.target->type->name);
					//else log("%d (%s) target set to null\n", c.id, c.type->name);
				}
				if (c.target && !is_in_range && c.st->max_speed > 1.0) {

					if (c.st->type->is_flyer) {

						xy diffpos = c.target->pos - c.pos;
						double len = diffpos.length();

						xy movepos((int)(diffpos.x / len * 8.0), (int)(diffpos.y / len * 8.0));
						double duration = movepos.length() / c.st->max_speed;

						if (teams[my_team].run_away) movepos = -movepos;

						c.pos += movepos;
						c.move_frames += duration;

					} else {
						path_units.clear();

						for (auto&cn : alive) {
							if (&cn == &c) continue;
							if (diag_distance(c.pos - cn.pos) >= 32 * 4) continue;
							path_units.push_back(&cn);
						}

						auto&dims = c.type->dimensions;

						weapon_stats*w = c.target->type->is_flyer ? c.st->air_weapon : c.st->ground_weapon;
						double max_range = 32.0;
						if (w) max_range = w->max_range;

						auto can_move_to = [&](bool is_first, xy pos) {
							auto test = [&](xy pos) {
								int right = pos.x + dims[0];
								int bottom = pos.y + dims[1];
								int left = pos.x - dims[2];
								int top = pos.y - dims[3];
								for (auto*v : path_units) {
									if (!is_first && v->move_frames > 0.0) continue;
									int uright = v->pos.x + v->type->dimension_right();
									int ubottom = v->pos.y + v->type->dimension_down();
									int uleft = v->pos.x - v->type->dimension_left();
									int utop = v->pos.y - v->type->dimension_up();
									if (right >= uleft && bottom >= utop && uright >= left && ubottom >= top) return false;
								}
								return true;
							};
							pos.x &= -8;
							pos.y &= -8;
							// This is inefficient. Instead, we should find the left/right/top/bottom unit which
							// is blocking us and see if there is enough room between them, instead of checking
							// every pixel as is done here.
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

						if (!c.last_find_path || frame - c.last_find_path >= 15) {
							c.last_find_path = frame;

							//log("can_move_to(c.pos) is %d\n", can_move_to(c.pos));

							auto&pathing_map = square_pathing::get_pathing_map(c.type);
							while (pathing_map.path_nodes.empty()) {
								tsc::high_resolution_timer ht;
								multitasking::sleep(1);
								sleep_time += ht.elapsed();
							}

							tsc::high_resolution_timer ht;

							xy dst = c.target->pos;

							auto nodes = square_pathing::get_nearest_path_nodes(pathing_map, c.pos, dst);
							if (nodes.first && nodes.second) {
								if (nodes.first != nodes.second) {
									bool is_neighbors = test_pred(nodes.first->neighbors, [&](auto*n) {
										return n == nodes.second;
									});
									if (!is_neighbors) {
										if (!c.last_find_nodes_path || frame - c.last_find_nodes_path >= 60) {
											c.last_find_nodes_path = frame;
											auto path = square_pathing::find_path(pathing_map, nodes.first, nodes.second);
											if (path.size() >= 2) {
												dst = path[1]->pos;
												c.find_nodes_path_dst = dst;
											}
										} else {
											if (c.find_nodes_path_dst != xy()) dst = c.find_nodes_path_dst;
										}
									}
								} else c.find_nodes_path_dst = xy();
							} else c.find_nodes_path_dst = xy();

							struct node_data_t {
								double len = 0.0;
							};
							double path_len = 0.0;
							a_deque<xy> path = square_pathing::find_square_path<node_data_t>(square_pathing::get_pathing_map(c.type), c.pos, [&](xy pos, xy npos, node_data_t&n) {
								if (diag_distance(pos - c.pos) >= 32 * 3) return false;
								if (!can_move_to(pos == c.pos, npos)) return false;
								n.len += diag_distance(npos - pos);
								return true;
							}, [&](xy pos, xy npos, const node_data_t&n) {
								//return unit_pathing_distance(c.type, npos, c.target->pos);
								//return units_distance(pos, c.type, c.target->pos, c.target->type);
								if (teams[my_team].run_away) return -diag_distance(npos - dst);
								return diag_distance(npos - dst);
							}, [&](xy pos, const node_data_t&n) {
								if (teams[my_team].run_away) return false;
								bool r = units_distance(pos, c.type, c.target->pos, c.target->type) <= max_range;
								if (r) path_len = n.len;
								return r;
							}, true);

							//log("path.size() is %d\n", path.size());

							c.path = path;

							total_pathfinding += ht.elapsed();
						}

						if (!c.path.empty()) {

							xy npos = c.path.front();
							if (can_move_to(true, npos)) {
								c.path.pop_front();
								double distance = diag_distance(c.pos - npos);
								double duration = distance / c.st->max_speed;

								//log("%d (%s) move %g distance from %d %d to %d %d, will take %g frames\n", c.id, c.type->name, distance, c.pos.x, c.pos.y, npos.x, npos.y, duration);

								c.pos = npos;
								c.pos.x &= -8;
								c.pos.x += c.type->dimension_left() % 8;
								c.pos.y &= -8;
								c.pos.y += c.type->dimension_up() % 8;
								c.move_frames += duration;
							}

						}
					}

					anyone_doing_anything = true;

				}

			}

			for (auto i = alive.begin(); i != alive.end();) {
				if (i->hp <= 0.0) {
					team_alive[i->team].erase(*i);
					i = alive.erase(i);
				} else ++i;
			}

			for (auto&c : alive) {
				if (c.wait_until >= frame || c.cooldown_until >= frame) {
					anyone_doing_anything = true;
					break;
				}
			}

			//log("anyone_doing_anything is %d\n", anyone_doing_anything);

			if (!anyone_doing_anything) break;

			++frame;
		}

		for (int i = 0; i < 2; ++i) {
			teams[i].resources_value = 0.0;
			for (auto&v : team_alive[i]) {
				teams[i].resources_value += v.type->total_minerals_cost + v.type->total_gas_cost;
			}
		}

// 		auto write_json = [](a_string filename, a_string pre_text, const json_value&val) {
// 			FILE*f = fopen(filename.c_str(), "wb");
// 			if (!f) {
// 				a_string str = format(" !! error - failed to open %s for writing\n", filename);
// 				log("%s\n", str);
// 				send_text(str);
// 			} else {
// 				fwrite(pre_text.data(), pre_text.size(), 1, f);
// 				a_string data = val.dump();
// 				fwrite(data.data(), data.size(), 1, f);
// 				fclose(f);
// 			}
// 		};

		if (teams[0].run_away) log(" (team 0 run away)\n");
		if (teams[1].run_away) log(" (team 1 run away)\n");
		log("resources lost: %g / %g\n", teams[0].resources_lost, teams[1].resources_lost);
		log("resources value: %g / %g\n", teams[0].resources_value, teams[1].resources_value);

// 		log(log_level_info, "sleep: %g\n", sleep_time);
// 		log(log_level_info, "pathfinding: %g\n", total_pathfinding);
// 		log(log_level_info, "took %g\n", ht.elapsed());
// 
// 		write_json("combat_eval2.txt", "data = ", jv);
// 
// 		xcept("stop");

	}

};


}

