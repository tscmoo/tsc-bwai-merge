

struct strat_t_vz_fake_mech : strat_t_base {


	virtual void init() override {

		sleep_time = 15;

		get_upgrades::set_upgrade_value(upgrade_types::spider_mines, -1.0);
		get_upgrades::set_upgrade_value(upgrade_types::siege_mode, -1.0);
		get_upgrades::set_upgrade_order(upgrade_types::spider_mines, -10.0);
		get_upgrades::set_upgrade_order(upgrade_types::siege_mode, -9.0);

	}
	xy natural_pos;
	virtual bool tick() override {
		if (opening_state == 0) {
			if (my_resource_depots.size() != 1 || my_workers.size() != 4) opening_state = -1;
			else {
				build::add_build_total(0.0, unit_types::scv, 9);
				build::add_build_task(1.0, unit_types::supply_depot);
				build::add_build_total(2.0, unit_types::scv, 11);
				build::add_build_task(3.0, unit_types::barracks);
				build::add_build_task(4.0, unit_types::refinery);
				build::add_build_total(5.0, unit_types::scv, 15);
				build::add_build_task(4.0, unit_types::marine);
				build::add_build_task(6.0, unit_types::supply_depot);
				build::add_build_total(7.0, unit_types::scv, 16);
				build::add_build_task(8.0, unit_types::factory);
				++opening_state;
			}
		} else if (opening_state == 1) {
			if (bo_all_done()) {
				opening_state = -1;
			}
		}

		if (my_units_of_type[unit_types::vulture].empty() && current_used_total_supply < 30.0) {
			combat::force_defence_is_scared_until = current_frame + 15 * 10;
		}

		if (being_rushed) rm_all_scouts();

		combat::no_aggressive_groups = true;
		combat::aggressive_groups_done_only = false;
		combat::aggressive_wraiths = enemy_air_army_supply == 0.0;
		combat::aggressive_vultures = true;

		return current_used_total_supply >= 90;
	}
	virtual bool build(buildpred::state&st) override {
		using namespace buildpred;
		if (opening_state != -1) return false;

		st.dont_build_refineries = count_units_plus_production(st, unit_types::cc) < 2;

		army = [army = army](state&st) {
			return maxprod(st, unit_types::marine, army);
		};
		if (medic_count < marine_count / 8) {
			army = [army = army](state&st) {
				return maxprod(st, unit_types::medic, army);
			};
		}

		army = [army = army](state&st) {
			return nodelay(st, unit_types::siege_tank_tank_mode, army);
		};

		if (valkyrie_count * 2 < enemy_air_army_supply / 2) {
			army = [army = army](state&st) {
				return nodelay(st, unit_types::valkyrie, army);
			};
		}

		if (count_units_plus_production(st, unit_types::cc) < 2) {
			army = [army = army](state&st) {
				return nodelay(st, unit_types::cc, army);
			};
		}

		if (enemy_lurker_count || (enemy_lair_count && enemy_spire_count == 0)) {
			if (science_vessel_count == 0) {
				army = [army = army](state&st) {
					return nodelay(st, unit_types::science_vessel, army);
				};
			}
		}
		
		if (wraith_count) {
			if (count_units_plus_production(st, unit_types::engineering_bay) == 0) {
				army = [army = army](state&st) {
					return nodelay(st, unit_types::engineering_bay, army);
				};
			} else {
				if (science_vessel_count == 0) {
					if (count_units_plus_production(st, unit_types::missile_turret) < (enemy_lurker_count || enemy_lair_count ? 2 : 1)) {
						army = [army = army](state&st) {
							return nodelay(st, unit_types::missile_turret, army);
						};
					}
				}
			}
		}

		if (wraith_count == 0) {
			army = [army = army](state&st) {
				return nodelay(st, unit_types::wraith, army);
			};
		} else {
			if (control_tower_count == 0) {
				army = [army = army](state&st) {
					return nodelay(st, unit_types::control_tower, army);
				};
			}
		}

		if (machine_shop_count == 0) {
			army = [army = army](state&st) {
				return nodelay(st, unit_types::machine_shop, army);
			};
		}
		if (vulture_count < 2) {
			army = [army = army](state&st) {
				return nodelay(st, unit_types::vulture, army);
			};
		}

		if (marine_count < 2) {
			army = [army = army](state&st) {
				return nodelay(st, unit_types::marine, army);
			};
		}

		if (tank_count) {
			army = [army = army](state&st) {
				return nodelay(st, unit_types::siege_tank_tank_mode, army);
			};
		}

		if (scv_count < 40) {
			army = [army = army](state&st) {
				return nodelay(st, unit_types::scv, army);
			};
		}

		return army(st);
	}

};

