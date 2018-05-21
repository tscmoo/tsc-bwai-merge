

namespace tactics {

	int last_eval_my_total_worth = 0;
	double my_total_worth = 0.0;

	a_vector<combat::combat_unit*> pulled_workers;
	void get_pulled_workers() {
		for (auto i = pulled_workers.begin(); i != pulled_workers.end();) {
			if ((*i)->u->dead) i = pulled_workers.erase(i);
			else ++i;
		}
		if (pulled_workers.size() + 3 < my_workers.size()) {
			for (auto* a : combat::live_combat_units) {
				if (!a->u->type->is_worker) continue;
				if (a->u->controller->action != unit_controller::action_gather) continue;
				if (std::find(pulled_workers.begin(), pulled_workers.end(), a) != pulled_workers.end()) {
					pulled_workers.push_back(a);
				}
				if (pulled_workers.size() + 3 >= my_workers.size()) break;
			}
		}
		log(log_level_test, "pulling %d workers\n", pulled_workers.size());
	}
	int pull_workers_to_buy_time_until = 0;
	void pull_workers_to_buy_time() {
		log(log_level_test, "pull_workers_to_buy_time()\n");
		get_pulled_workers();
		pull_workers_to_buy_time_until = current_frame + 15 * 2;
	}
	void pull_workers_to_fight() {
		log(log_level_test, "pull_workers_to_fight()\n");
		get_pulled_workers();
		for (auto* a : pulled_workers) a->u->force_combat_unit = true;
	}
	void reset_workers() {
		for (auto* a : combat::live_combat_units) {
			if (a->u->type->is_worker && a->u->force_combat_unit) a->u->force_combat_unit = false;
		}
	}

	a_unordered_map<unit_type*, a_vector<combat::combat_unit*>> force_attack_n_map;
	void force_attack_n(unit_type* ut, int n) {
		auto& vec = force_attack_n_map[ut];
		while ((int)vec.size() > n) {
			vec.back()->force_attack = false;
			vec.pop_back();
		}
		for (auto i = vec.begin(); i != vec.end();) {
			if ((*i)->u->dead) i = vec.erase(i);
			else ++i;
		}
		if ((int)vec.size() < n) {
			for (auto* a : combat::live_combat_units) {
				if (a->u->type != ut) continue;
				if (a->force_attack) continue;
				vec.push_back(a);
				if ((int)vec.size() >= n) break;
			}
		}
		for (auto* a : vec) {
			a->force_attack = true;
		}
	}

	void execute_pull_workers_to_buy_time() {

		for (auto* a : pulled_workers) {

			a->strategy_busy_until = current_frame + 15 * 60 * 5;
			if (combat::entire_threat_area.test(grid::build_square_index(a->u->pos))) {
				a_deque<xy> path = combat::find_path(a->u->type, a->u->pos, [&](xy pos, xy npos) {
					return diag_distance(npos - a->u->pos) <= 32 * 20;
				}, [&](xy pos, xy npos) {
					return 1.0;
				}, [&](xy pos) {
					size_t index = grid::build_square_index(pos);
					return combat::entire_threat_area_edge.test(index) && (a->u->is_flying || !combat::run_spot_taken.test(index));
				});

				a->subaction = combat::combat_unit::subaction_move;
				a->target_pos = path.empty() ? combat::op_closest_base : path.back();
				combat::run_spot_taken.set(grid::build_square_index(a->target_pos));

				log(log_level_test, "waaa run\n");
			} else {
				log(log_level_test, "moving workers\n");
				a->subaction = combat::combat_unit::subaction_move;
				a->target_pos = combat::op_closest_base;
			}
			game->drawLineMap(a->u->pos.x, a->u->pos.y, a->target_pos.x, a->target_pos.y, BWAPI::Colors::Orange);
		}
	}

	int enable_anti_push_until = 0;

	struct anti_push_info {
		//a_unordered_set<combat::combat_unit*> allies;
		//a_unordered_set<unit*> enemies;
		a_vector<combat::combat_unit*> allies;
		a_vector<unit*> enemies;
		xy pos;
		int creation_frame = 0;
		double my_losses_since_creation = 0.0;
		int last_combat_eval = 0;
		a_deque<double> combat_eval_results;
		double combat_eval_score = 0.0;
		bool is_engaging = false;
		int last_engage = 0;
	};

	a_vector<anti_push_info> anti_pushes;

	void process_anti_push(anti_push_info& ap) {

		bool engage = false;

		if (current_frame < ap.creation_frame + 15 * 20) {
			log(log_level_test, "ap %p: too early to engage (created %d ago)\n", &ap, current_frame - ap.creation_frame);
			engage = false;
		}
		if (ap.my_losses_since_creation >= my_total_worth / 10) {
			log(log_level_test, "ap %p: too many losses, engage!\n", &ap);
			engage = true;
		}

		if (current_frame - ap.last_combat_eval >= 30) {
			combat_eval::eval eval;
			eval.max_frames = 15 * 40;
			auto addu = [&](unit* u, int team) {
				auto& c = eval.add_unit(u, team);
			};
			for (auto* a : ap.allies) {
				log(log_level_test, "ap %p: ally %s\n", &ap, a->u->type->name);
				addu(a->u, 0);
			}
			for (unit* e : ap.enemies) {
				log(log_level_test, "ap %p: enemy %s\n", &ap, e->type->name);
				addu(e, 1);
			}
			eval.run();

			log(log_level_test, "ap %p: scores %g %g\n", &ap, eval.teams[0].score, eval.teams[1].score);

			double result = eval.teams[0].score / eval.teams[1].score;
			if (eval.teams[0].score == 0) result = 0.0;
			else if (eval.teams[1].score == 0) result = 2.0;
			else if (result > 2.0) result = 2.0;

			if (ap.combat_eval_results.size() >= 15) ap.combat_eval_results.pop_front();
			ap.combat_eval_results.push_back(result);

			double sum = 0.0;
			for (auto& v : ap.combat_eval_results) sum += v;
			double avg = sum / ap.combat_eval_results.size();

			ap.combat_eval_score = avg;
			log(log_level_test, "ap %p: combat_eval_score %g\n", &ap, ap.combat_eval_score);
		}

		if (ap.combat_eval_results.size() >= 10) {
			if (ap.combat_eval_score > 1.0) engage = true;
		}

		if (engage) {
			if (!ap.is_engaging) {
				ap.last_engage = current_frame;
				ap.is_engaging = true;
			}
		} else {
			if (ap.is_engaging) {
				if (current_frame - ap.last_engage >= 15 * 20) {
					ap.is_engaging = false;
				}
			}
		}

		log(log_level_test, "ap %p: engage is %d, is_engaging is %d\n", &ap, engage, ap.is_engaging);

		if (ap.is_engaging) {
			for (auto* a : ap.allies) {
				a->strategy_attack_until = current_frame + 15 * 60 * 5;
				a->strategy_run_until = 0;
			}
		} else {
			for (auto* a : ap.allies) {
				a->strategy_attack_until = 0;
				a->strategy_run_until = current_frame + 15 * 60 * 5;
			}
		}

	};

	anti_push_info ap;
	bool ap_valid = false;
	void execute_anti_push() {

		auto get_ap_group = [&]() {
			if (combat::groups.empty()) return (combat::group_t*)nullptr;
			auto& g = combat::groups.front();
			if (g.is_aggressive_group || g.is_defensive_group) return (combat::group_t*)nullptr;
			return &g;
		};

		auto* g = get_ap_group();
		if (!g) {
			ap_valid = false;
			return;
		}

		ap.allies = g->allies;
		ap.enemies = g->enemies;

		process_anti_push(ap);

	}

// 	void execute_anti_push() {
// 
// 		auto get = [&](xy pos) -> anti_push_info& {
// 			for (auto& v : anti_pushes) {
// 				if (diag_distance(pos - v.pos) <= 32 * 20) return v;
// 			}
// 			log(log_level_test, "new anti push group at %d %d\n", pos.x, pos.y);
// 			anti_pushes.emplace_back();
// 			anti_pushes.back().pos = pos;
// 			anti_pushes.back().creation_frame = current_frame;
// 			return anti_pushes.back();
// 		};
// 
// 		for (auto& g : combat::groups) {
// 			if (!g.is_aggressive_group && !g.is_defensive_group) {
// 				auto& ap = get(g.enemies.front()->pos);
// 				for (auto* a : g.allies) {
// 					if (a->u->is_in_anti_push_group) continue;
// 					a->u->is_in_anti_push_group = true;
// 					ap.allies.insert(a);
// 				}
// 				for (auto* e : g.enemies) {
// 					if (e->is_in_anti_push_group) continue;
// 					e->is_in_anti_push_group = true;
// 					ap.enemies.insert(e);
// 				}
// 			}
// 		}
// 
// 		for (auto i = anti_pushes.begin(); i != anti_pushes.end();) {
// 			if (i->allies.empty() && i->enemies.empty()) i = anti_pushes.erase(i);
// 			else {
// 				auto& ap = *i;
// 
// 				for (auto i = ap.enemies.begin(); i != ap.enemies.end();) {
// 					if ((*i)->dead) {
// 						(*i)->is_in_anti_push_group = false;
// 						i = ap.enemies.erase(i);
// 					} else {
// 						if (diag_distance((*i)->pos - ap.pos) >= 32 * 20) {
// 							(*i)->is_in_anti_push_group = false;
// 							i = ap.enemies.erase(i);
// 						} else ++i;
// 					}
// 				}
// 				for (auto i = ap.allies.begin(); i != ap.allies.end();) {
// 					if ((*i)->u->dead || ap.enemies.empty()) {
// 						(*i)->u->is_in_anti_push_group = false;
// 						i = ap.allies.erase(i);
// 					} else {
// 						if (diag_distance((*i)->u->pos - ap.pos) >= 32 * 20) {
// 							(*i)->u->is_in_anti_push_group = false;
// 							i = ap.allies.erase(i);
// 						} else ++i;
// 					}
// 				}
// 
// 				process_anti_push(*i);
// 				++i;
// 			}
// 		}
// 
// 	}

	void tactics_task() {
		while (true) {

			if (current_frame < pull_workers_to_buy_time_until) {
				execute_pull_workers_to_buy_time();
			}

			if (current_frame < enable_anti_push_until) {
				//execute_anti_push();
			}

			if (current_frame - last_eval_my_total_worth >= 30) {
				last_eval_my_total_worth = current_frame;
				double val = 0.0;
				for (unit* u : my_units) {
					val += u->type->total_minerals_cost + u->type->total_gas_cost;
				}
				my_total_worth = val;
				//log(log_level_test, "my_total_worth is %g\n", my_total_worth);
			}

			multitasking::sleep(4);
		}
	}

	void on_destroy(unit* u) {
		if (u->owner == players::my_player) {
			anti_push_info* ap = nullptr;
			for (auto& v : anti_pushes) {
				if (diag_distance(u->pos - v.pos) <= 32 * 20) {
					ap = &v;
					break;
				}
			}
			if (ap) {
				ap->my_losses_since_creation += u->type->total_minerals_cost + u->type->total_gas_cost;
			}
		}
	}

	void init() {

		units::on_destroy_callbacks.push_back(on_destroy);

		multitasking::spawn(tactics_task, "tactics");
	}

}

