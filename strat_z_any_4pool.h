
struct strat_z_any_4pool : strat_mod {
	
	int total_drones_made = 0;
	int total_zerglings_made = 0;

	virtual void mod_init() override {
		
		early_micro::start();

	}

	virtual void mod_tick() override {
		combat::no_aggressive_groups = false;
		
		scouting::scout_supply = 30;
		
		if (current_frame < 15 * 60 * 6) early_micro::keepalive_until = current_frame + 60;
		
		total_drones_made = my_units_of_type[unit_types::drone].size() + game->self()->deadUnitCount(unit_types::drone->game_unit_type);
		total_zerglings_made = my_units_of_type[unit_types::zergling].size() + game->self()->deadUnitCount(unit_types::zergling->game_unit_type);
		
		
		bool anything_to_attack = false;
		for (unit*e : enemy_units) {
			if (e->gone) continue;
			if (e->is_flying) continue;
			if (e->type->is_worker) continue;
			anything_to_attack = true;
			break;
		}
		if (anything_to_attack) {
			rm_all_scouts();
		} else {
			if (!my_units_of_type[unit_types::zergling].empty()) {
//				for (unit*u : my_units_of_type[unit_types::zergling]) {
//					if (!scouting::is_scout(u)) scouting::add_scout(u);
//				}
			} else {
				int worker_scouts = 0;
				for (auto& v : scouting::all_scouts) {
					if (v.scout_unit && v.scout_unit->type->is_worker) ++worker_scouts;
				}
				if (worker_scouts == 0 && !my_units_of_type[unit_types::spawning_pool].empty()) {
					if (true) {
						unit*u = get_best_score(my_workers, [&](unit*u) {
							if (u->controller->action != unit_controller::action_gather) return std::numeric_limits<double>::infinity();
							return 0.0;
						}, std::numeric_limits<double>::infinity());
						if (u) scouting::add_scout(u);
					}
				}
			}
		}
		
		if (current_frame < 15 * 60 * 10) resource_gathering::max_gas = std::max(resource_gathering::max_gas, 200.0);
		
		//if (!enemy_buildings.empty()) rm_all_scouts();

	}

	virtual void mod_build(buildpred::state& st) override {

		using namespace unit_types;
		using namespace upgrade_types;

		st.dont_build_refineries = count_units_plus_production(st, extractor) == 1 && st.frame < 15 * 60 * 8;
		st.auto_build_hatcheries = current_frame >= 15 * 60 * 9;

		if (total_zerglings_made < 8) {
			build_n(drone, 4);
			build(zergling);
			return;
		}
		
		build(hydralisk);
		build_n(drone, 14);
		build_n(hatchery, 2);
		build_n(extractor, 1);
		build_n(drone, 10);
		
	}

};
