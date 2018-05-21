

namespace threats {

	tsc::dynamic_bitset desired_control_area;

	bool take_map_control = false;

	bool being_marine_rushed = false;
	bool has_evaled_first_rax = false;

	struct threat {
		bool imminent;
		xy position;
		double supply;
	};

	template<typename strat_T>
	void eval(strat_T& s) {

		if (current_frame < 15 * 60 * 10) {
			if (s.enemy_marine_count || s.enemy_barracks_count) {
				if (!has_evaled_first_rax) {
					has_evaled_first_rax = true;
					int earliest_rax_frame = current_frame - unit_types::marine->build_time * s.enemy_marine_count - unit_types::barracks->build_time;
					being_marine_rushed = earliest_rax_frame <= unit_types::scv->build_time * 5 + unit_types::supply_depot->build_time / 2;
				}
				if (s.is_defending || s.enemy_attacking_army_supply >= 2.0) {
					int earliest_rax_frame = current_frame - unit_types::marine->build_time * s.enemy_marine_count - unit_types::barracks->build_time;
					double rush_distance = std::numeric_limits<double>::infinity();
					for (xy pos : possible_enemy_start_locations) {
						double d = unit_pathing_distance(unit_types::marine, pos, my_start_location);
						if (d < rush_distance) rush_distance = d;
					}
					int rush_frames = frames_to_reach(units::get_unit_stats(unit_types::marine, players::opponent_player), 0.0, rush_distance);
					if (earliest_rax_frame <= unit_types::scv->build_time * 5 + unit_types::supply_depot->build_time / 2 + rush_frames) {
						being_marine_rushed = true;
					}
				}
			}
		} else {
			if (being_marine_rushed) being_marine_rushed = false;
		}

	}

	int last_update_attack_times = 0;
	double nearest_attack_time = 0.0;
	xy nearest_attack_pos;

	void threats_task() {

		while (true) {

			if (current_frame - last_update_attack_times >= 15) {

				nearest_attack_pos = xy();

				for (auto& ds : combat::defence_spots) {
					double total_f = 0;
					int n = 0;
					double supply = 0.0;
					for (unit* u : enemy_units) {
						if (!u->stats->ground_weapon || u->type->is_worker) continue;
						double d = unit_pathing_distance(u, ds.pos);
						double f = (double)frames_to_reach(u, d);
						total_f += f;
						++n;
					}
					double avg_f = total_f / n;

					if (nearest_attack_pos == xy() || avg_f < nearest_attack_time) {
						nearest_attack_pos = ds.pos;
						nearest_attack_time = avg_f;
					}
				}

				//log(log_level_test, "nearest attack pos %d %d time %g\n", nearest_attack_pos.x, nearest_attack_pos.y, nearest_attack_time);
			}


			multitasking::sleep(4);
		}

	}


	void init() {

		multitasking::spawn(threats_task, "threats");

	}


};

