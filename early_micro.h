
namespace early_micro {

struct nunit {
	
	refcounter<combat::combat_unit> h;
	combat::combat_unit* a = nullptr;
	
	xy last_target_pos;
};

int keepalive_until = 0;
bool started = false;

a_vector<nunit> nunits;

void move(nunit& n, xy target_pos) {
	
	n.last_target_pos = target_pos;
	
	unit* u = n.a->u;
	
	a_vector<std::tuple<unit*, xy>> path_units;
	for (unit* nu : visible_units) {
		if (nu->is_flying || nu->burrowed) continue;
		if (diag_distance(nu->pos - u->pos) >= 32 * 10) continue;
		if (nu == u) continue;
		xy pos = nu->pos;
		path_units.emplace_back(nu, pos);
	}

	auto& dims = u->type->dimensions;
	
	a_unordered_set<unit*> potential_targets;

	auto can_move_to = [&](xy pos) {
		auto test = [&](xy pos) {
			int right = pos.x + dims[0];
			int bottom = pos.y + dims[1];
			int left = pos.x - dims[2];
			int top = pos.y - dims[3];
			for (auto& v : path_units) {
				unit*u;
				xy pos;
				std::tie(u, pos) = v;
				if (u->is_flying || u->burrowed) continue;
				int uright = pos.x + u->type->dimension_right();
				int ubottom = pos.y + u->type->dimension_down();
				int uleft = pos.x - u->type->dimension_left();
				int utop = pos.y - u->type->dimension_up();
				if (right >= uleft && bottom >= utop && uright >= left && ubottom >= top) {
					if (u->owner->is_enemy || u->owner == players::neutral_player) potential_targets.insert(u);
					return false;
				}
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
	
	xy move_to = square_pathing::get_move_to(u, target_pos, 0, xy());

	xy pos = square_pathing::get_pos_in_square(move_to, u->type);
	if (pos != xy()) move_to = pos;

	double best_d = std::numeric_limits<double>::infinity();
	xy best_pos;
	a_deque<xy> path = square_pathing::find_square_path(square_pathing::get_pathing_map(u->type), u->pos, [&](xy pos, xy npos) {
		if (diag_distance(pos - u->pos) >= 32 * 16) return false;
		if (!can_move_to(npos)) return false;
		return true;
	}, [&](xy pos, xy npos) {
		return 0.0;
	}, [&](xy pos) {
		double d = diag_distance(pos - move_to);
		if (d < best_d) {
			best_d = d;
			best_pos = pos;
		}
		return d <= 16;
	});
	
	game->drawLineMap(u->pos.x, u->pos.y, move_to.x, move_to.y, BWAPI::Colors::Black);
	game->drawLineMap(u->pos.x, u->pos.y, best_pos.x, best_pos.y, BWAPI::Colors::Green);
	
	if (path.empty()) game->drawCircleMap(u->pos.x, u->pos.y, 32, BWAPI::Colors::Blue);
	else {
		xy lpos = u->pos;
		for (auto& v : path) {
			game->drawLineMap(lpos.x, lpos.y, v.x, v.y, BWAPI::Colors::Blue);
			lpos = v;
		}
	}
	
	unit* target = get_best_score(potential_targets, [&](unit* e) {
		auto* w = e->is_flying ? u->stats->air_weapon : u->stats->ground_weapon;
		if (!w) return std::numeric_limits<double>::infinity();
		double d = units_distance(u, e) - w->max_range;
		if (d < 0.0) d = 0.0;
		double s = d + e->hp / 16.0;
		if (!e->stats->ground_weapon) s += 400.0;
		return s;
	}, std::numeric_limits<double>::infinity());
	
	n.a->strategy_busy_until = current_frame + 15;
	
	if (target && (path.empty() || path.size() <= 2)) {
		n.a->subaction = combat::combat_unit::subaction_fight;
		n.a->target = target;
	} else {
		n.a->subaction = combat::combat_unit::subaction_move;
		n.a->target_pos = best_pos;
	}
	
}

void update() {
	
	unit* main_target = get_best_score(enemy_units, [&](unit* e) {
		if (e->gone) return std::numeric_limits<double>::infinity();
		if (e->type->is_worker) return std::numeric_limits<double>::infinity();
		if (e->stats->ground_weapon) {
			if (!e->is_completed) return -e->hp;
			return e->hp;
		}
		return 200.0 + e->hp;
	}, std::numeric_limits<double>::infinity());
	xy main_target_pos;
	if (main_target) main_target_pos = main_target->pos;
	
	for (unit* e : enemy_units) {
		if (e->gone) continue;
		if (e->type->is_worker) continue;
		main_target_pos = e->pos;
		break;
	}
	
	a_map<xy, int> move_count;
	
	for (auto& n : nunits) {
		
		xy target_pos = main_target_pos;
		
		if (target_pos == xy()) {
			if (n.last_target_pos != xy() && diag_distance(n.a->u->pos - n.last_target_pos) > 32 * 4) target_pos = n.last_target_pos;
			else {
				target_pos = get_best_score(possible_enemy_start_locations, [&](xy pos) {
					return move_count[pos];
				});
			}
			++move_count[target_pos];
		}
		
		move(n, target_pos);
	}
	
	
	
	
}

void early_micro_task() {
	
	while (true) {
		
		while (current_frame > keepalive_until) {
			nunits.clear();
			multitasking::sleep(15);
		}
		
		for (auto i = nunits.begin(); i != nunits.end();) {
			if (i->a->u->dead) i = nunits.erase(i);
			else ++i;
		}
		
		for (auto* a : combat::live_combat_units) {
			if (a->u->type->is_worker) continue;
			if (!a->u->stats->ground_weapon) continue;
			if (a->u->is_flying) continue;
			auto h = combat::request_control(a);
			if (h) {
				nunits.emplace_back();
				auto& n = nunits.back();
				n.h = std::move(h);
				n.a = a;
			}
		}
		
		update();
		
		multitasking::sleep(1);
		
	}
	
}

void start() {
	
	if (!started) {
		started = true;
		multitasking::spawn(early_micro_task, "early micro");
	}
	
}

}
