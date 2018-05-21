namespace unit_prediction {

	tsc::dynamic_bitset any_unit_at(grid::build_grid_width * grid::build_grid_height);

	xy find_nearby_move_pos(xy src, bool flying, unit_type* ut) {
		const double max_distance = 32*12;
		tsc::dynamic_bitset visited(grid::build_grid_width * grid::build_grid_height);
		a_deque<std::tuple<grid::build_square*, xy>> open;
		open.emplace_back(&grid::get_build_square(src), src);
		size_t index = grid::build_square_index(src);
		visited.set(index);
		while (!open.empty()) {
			grid::build_square* bs;
			xy origin;
			std::tie(bs, origin) = open.front();
			open.pop_front();

			if (!any_unit_at.test(grid::build_square_index(*bs)) && current_frame - bs->last_seen >= 15 * 20) {
				return bs->pos;
			}

			auto add = [&](int n) {
				grid::build_square* nbs = bs->get_neighbor(n);
				if (!nbs) return;
				if (!flying && !nbs->entirely_walkable) return;
				if (diag_distance(nbs->pos - origin) >= max_distance) return;
				size_t index = grid::build_square_index(*nbs);
				if (visited.test(index)) return;
				if (nbs->pos.x - ut->dimension_left() < 0) return;
				if (nbs->pos.y - ut->dimension_up() < 0) return;
				if (nbs->pos.x + ut->dimension_right() >= grid::map_width) return;
				if (nbs->pos.y + ut->dimension_down() >= grid::map_height) return;
				visited.set(index);

				open.emplace_back(nbs, origin);
			};
			add(0);
			add(1);
			add(2);
			add(3);
		}
		return xy();
	}

	a_unordered_map<unit*, a_unordered_set<unit*>> nearby_units;

	void update_nearby_units() {
		for (unit* u : visible_enemy_units) {
			if (u->building || u->type->is_worker) continue;
			auto& n = nearby_units[u];
			for (unit* u2 : enemy_units) {
				if (u2->type->is_non_usable) continue;
				if (u2->building || u2->type->is_worker) continue;
				if (u2->gone) continue;
				double d = diag_distance(u->pos - u2->pos);
				if (d <= 32*6) {
					n.insert(u2);
				}
			}
		}
	}

	void unit_prediction_task() {

		int last_update_nearby_units = 0;

		while (true) {

			any_unit_at.reset();
			for (unit* u : live_units) {
				if (u->gone) continue;
				any_unit_at.set(grid::build_square_index(u->pos));
			}

			auto has_detection_at = [&](xy pos) {
				for (unit* u : my_detector_units) {
					if (!u->is_completed) continue;
					if ((pos - u->pos).length() <= u->stats->sight_range) return true;
				}
				return false;
			};

			for (unit* u : invisible_units) {
				if (!u->gone && grid::get_build_square(u->pos).visible) {
					if (u->type == unit_types::spider_mine && !has_detection_at(u->pos)) {
						u->burrowed = true;
						u->cloaked = true;
						u->detected = false;
					}
					if (!u->burrowed || has_detection_at(u->pos)) {
						if (u->type->is_non_usable) {
							u->gone = true;
						} else {
							xy new_pos = find_nearby_move_pos(u->last_seen_pos, u->is_flying, u->type);
							if (new_pos != xy()) {
								any_unit_at.reset(grid::build_square_index(u->pos));
								u->pos = new_pos;
								any_unit_at.set(grid::build_square_index(u->pos));
							} else u->gone = true;
						}
					}
				}
			}
			if (current_frame - last_update_nearby_units >= 30) {
				update_nearby_units();
			}

			multitasking::sleep(6);
		}

	}

	void on_show(unit* u) {
		if (!u->owner->is_enemy) return;
		if (u->building) return;
		for (auto* u2 : nearby_units[u]) {
			if (u2->visible || current_frame - u2->last_seen <= 15*10) continue;
			double d = diag_distance(u->pos - u2->last_seen_pos);
			if (d / u2->stats->max_speed >= current_frame - u2->last_seen) continue;
			if (tsc::rng(1.0) < 0.5) continue;
			xy new_pos = find_nearby_move_pos(u->pos, u2->is_flying, u2->type);
			if (new_pos != xy()) {
				any_unit_at.reset(grid::build_square_index(u2->pos));
				u2->pos = new_pos;
				any_unit_at.set(grid::build_square_index(u2->pos));
			}
		}

	}

	void unit_prediction_draw_task() {
		while (true) {
			for (unit* e : enemy_units) {

				BWAPI::Color c = e->visible ? BWAPI::Colors::Green : e->gone ? BWAPI::Colors::Red : BWAPI::Colors::Blue;

				game->drawBoxMap(e->pos.x - e->type->dimension_left(), e->pos.y - e->type->dimension_up(), e->pos.x + e->type->dimension_right(), e->pos.y + e->type->dimension_down(), c);
				game->drawTextMap(e->pos.x - e->type->dimension_left(), e->pos.y, "%s", e->type->name.c_str());

			}

			for (auto& g : combat::groups) {
				if (g.is_defensive_group) continue;
				a_string str = format("group %d", g.idx);
				if (g.is_defensive_group) str += " defensive";
				if (g.is_aggressive_group) str += " aggressive";
				if (g.is_main_army) str += " main army";
				str += format(" value %g", g.value);
				str += format(" %d allies, %d enemies", g.allies.size(), g.enemies.size());
				xy pos = g.enemies.front()->pos;
				game->drawTextMap(pos.x, pos.y + 12, "%s", str.c_str());
				for (auto* a: g.allies) {
					game->drawLineMap(a->u->pos.x, a->u->pos.y, pos.x, pos.y, BWAPI::Colors::Green);
				}
				for (unit* u : g.enemies) {
					game->drawLineMap(u->pos.x, u->pos.y, pos.x, pos.y, BWAPI::Colors::Red);
				}
			}

			multitasking::sleep(1);
		}
	}

	void init() {

		any_unit_at.resize(grid::build_grid_width * grid::build_grid_height);

		units::on_show_callbacks.push_back(on_show);

		multitasking::spawn(unit_prediction_task, "unit prediction");

		//multitasking::spawn(0.1, unit_prediction_draw_task, "unit prediction draw");

	}
}
