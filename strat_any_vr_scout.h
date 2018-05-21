
struct strat_any_vr_scout : strat_mod {
	
	double total_shield_battery_energy = 0.0;

	virtual void mod_init() override {
		
		sleep_time = 8;

	}

	virtual void mod_tick() override {
		
		int scouts = 0;
		if (start_locations.size() <= 2) {
			if (current_used_total_supply >= 7) scouts = 1;
		} else {
			if (current_used_total_supply >= 6) scouts = 1;
			if (current_used_total_supply >= 8) scouts = 2;
		}

		if ((int)scouting::all_scouts.size() < scouts) {
			unit*scout_unit = get_best_score(my_workers, [&](unit*u) {
				if (u->controller->action != unit_controller::action_gather) return std::numeric_limits<double>::infinity();
				return 0.0;
			}, std::numeric_limits<double>::infinity());
			if (scout_unit) scouting::add_scout(scout_unit);
		}

		if (!players::opponent_player->random) {
			rm_all_scouts();
		}
		
		strat_is_done = !players::opponent_player->random || current_frame >= 15 * 60 * 5;
	}

	virtual void mod_build(buildpred::state& st) override {

		using namespace unit_types;
		using namespace upgrade_types;

		if (players::my_player->race == race_terran) {
			build_n(cc, 2);
			build_n(scv, 18);
			build_n(marine, 3);
			build_n(factory, 1);
			build_n(scv, 14);
			build_n(marine, 1);
			build_n(barracks, 1) && build_n(refinery, 1);
			build_n(supply_depot, 1);
			build_n(scv, 11);
			build_n(barracks, 1) && build_n(marine, 3 + enemy_attacking_army_supply / 2.0);
			build_n(scv, 8);
		} else if (players::my_player->race == race_protoss) {
			build_n(nexus, 2);
			build_n(probe, 20);
			build_n(gateway, 2);
			build_n(probe, 14);
			build_n(zealot, 1);
			build_n(probe, 12);
			build_n(gateway, 1);
			build_n(probe, 10);
		} else if (players::my_player->race == race_zerg) {
			build_n(hatchery, 2);
			build_n(drone, 12);
			build_n(spawning_pool, 1);
			build_n(overlord, 2);
			build_n(drone, 9);
		}

	}

};
