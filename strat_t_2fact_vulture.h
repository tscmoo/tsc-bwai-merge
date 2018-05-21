

struct strat_t_2fact_vulture : strat_t_base {

	wall_in::wall_builder wall;

	virtual void init() override {

		combat::aggressive_vultures = true;
		combat::no_scout_around = false;

		get_upgrades::set_upgrade_value(upgrade_types::siege_mode, -1.0);
		get_upgrades::set_upgrade_value(upgrade_types::ion_thrusters, -1.0);
		get_upgrades::set_upgrade_value(upgrade_types::spider_mines, -1.0);
		get_upgrades::set_upgrade_order(upgrade_types::siege_mode, -3.0);
		get_upgrades::set_upgrade_order(upgrade_types::ion_thrusters, -2.0);
		get_upgrades::set_upgrade_order(upgrade_types::spider_mines, -1.0);

		default_upgrades = false;

	}
	bool is_attacking = false;
	bool wall_calculated = false;
	bool has_wall = false;
	virtual bool tick() override {
		if (opening_state == 0) {
			if (my_resource_depots.size() != 1 || my_workers.size() != 4) opening_state = -1;
			else {
				build::add_build_task(0.0, unit_types::scv);
				build::add_build_task(0.0, unit_types::scv);
				build::add_build_task(0.0, unit_types::scv);
				build::add_build_task(0.0, unit_types::scv);
				build::add_build_task(0.0, unit_types::scv);
				build::add_build_task(1.0, unit_types::supply_depot);
				build::add_build_task(2.0, unit_types::scv);
				build::add_build_task(2.0, unit_types::scv);
				build::add_build_task(3.0, unit_types::barracks);
				build::add_build_task(4.0, unit_types::refinery);
				++opening_state;
			}
		} else if (opening_state == 1) {
			if (bo_all_done()) {
				opening_state = -1;
			}
		}

		if (combat::defence_choke.center != xy() && !wall_calculated) {
			wall_calculated = true;

			wall.spot = wall_in::find_wall_spot_from_to(unit_types::zealot, combat::my_closest_base, combat::defence_choke.outside, false);
			wall.spot.outside = combat::defence_choke.outside;

			wall.against(unit_types::zealot);
			wall.add_building(unit_types::supply_depot);
			wall.add_building(unit_types::barracks);
			has_wall = true;
			if (!wall.find()) {
				wall.add_building(unit_types::supply_depot);
				if (!wall.find()) {
					log("failed to find wall in :(\n");
					has_wall = false;
				}
			}
		}
		if (has_wall) {
			wall.build();
		}

// 		if (my_units_of_type[unit_types::vulture].size() >= 6) {
// 			is_attacking = true;
// 		}
// 
// 		combat::no_aggressive_groups = !is_attacking;
// 		combat::aggressive_groups_done_only = false;
		
		if (enemy_air_army_supply) return true;
		if (current_used_total_supply >= 90) return true;
		return false;
	}
	virtual bool build(buildpred::state&st) override {
		using namespace buildpred;
		if (opening_state != -1) return false;

		army = [army = army](state&st) {
			return nodelay(st, unit_types::vulture, army);
		};

		if (tank_count < vulture_count / 4 + 2) {
			army = [army = army](state&st) {
				return nodelay(st, unit_types::siege_tank_tank_mode, army);
			};
		}

		if (count_units_plus_production(st, unit_types::factory) < (int)st.bases.size() + 1) {
			army = [army = army](state&st) {
				return nodelay(st, unit_types::factory, army);
			};
		}

		int machine_shops = count_production(st, unit_types::machine_shop);
		for (auto&v : st.units[unit_types::factory]) {
			if (v.has_addon) ++machine_shops;
		}
		if (machine_shops < 2) {
			army = [army = army](state&st) {
				return nodelay(st, unit_types::machine_shop, army);
			};
		}

		if (marine_count < 2) {
			army = [army = army](state&st) {
				return nodelay(st, unit_types::marine, army);
			};
		}

		if (scv_count < 60) {
			army = [army = army](state&st) {
				return nodelay(st, unit_types::scv, army);
			};
		}

		if (st.minerals >= 400) {
			if (count_units_plus_production(st, unit_types::cc) < 3) {
				army = [army = army](state&st) {
					return nodelay(st, unit_types::cc, army);
				};
			} else {
				army = [army = army](state&st) {
					return nodelay(st, unit_types::factory, army);
				};
			}
		}

		return army(st);
	}

};

