
struct strat_t_marine_micro_test : strat_t_base {

	virtual void init() override {

	}
	virtual bool tick() override {
		if (opening_state == 0) {
			if (my_resource_depots.size() != 1 || my_workers.size() != 4) opening_state = -1;
			else {
				build::add_build_total(0.0, unit_types::scv, 9);
				build::add_build_total(1.0, unit_types::supply_depot, 1);
				build::add_build_total(2.0, unit_types::scv, 10);
				build::add_build_task(3.0, unit_types::barracks);
				++opening_state;
			}
		} else if (opening_state == 1) {
			if (bo_all_done()) {
				opening_state = -1;
			}
		}

//		combat::no_aggressive_groups = army_supply < 40.0 && army_supply < enemy_army_supply + 16.0;

//		if (!my_units_of_type[unit_types::science_vessel].empty()) {
//			combat::no_aggressive_groups = false;
//		}

//		scouting::comsat_supply = 30.0;

		combat::no_aggressive_groups = army_supply < enemy_army_supply + 8.0;
		combat::use_control_spots = false;

		combat::no_scout_around = true;

		scouting::comsat_supply = 200.0;
		scouting::no_proxy_scout = true;

		get_upgrades::set_no_auto_upgrades(true);

		if (!enemy_buildings.empty() && scv_count < 28) rm_all_scouts();

		return false;
	}

	buildpred::state* build_st;
	std::function<bool(buildpred::state&)> build_army;
	void build(unit_type* ut) {
		using namespace buildpred;
		auto& army = this->build_army;
		army = [army = std::move(army), ut](state& st) {
			return nodelay(st, ut, army);
		};
	}
	void maxprod(unit_type* ut) {
		auto& army = this->build_army;
		army = [army = std::move(army), ut](buildpred::state& st) {
			return buildpred::maxprod(st, ut, army);
		};
	}
	bool upgrade(upgrade_type* upg) {
		using namespace buildpred;
		if (!has_or_is_upgrading(*build_st, upg)) {
			auto& army = this->build_army;
			army = [army = std::move(army), upg](state& st) {
				return nodelay(st, upg, army);
			};
			return false;
		}
		return true;
	}
	bool build_n(unit_type* ut, int n) {
		using namespace buildpred;
		if (count_units_plus_production(*build_st, ut) < n) {
			auto& army = this->build_army;
			army = [army = std::move(army), ut](state& st) {
				return nodelay(st, ut, army);
			};
			return false;
		}
		return true;
	}

	bool upgrade_wait(upgrade_type* upg) {
		using namespace buildpred;
		if (!has_or_is_upgrading(*build_st, upg)) {
			auto& army = this->build_army;
			army = [army = std::move(army), upg](state& st) {
				return nodelay(st, upg, army);
			};
			return false;
		}
		return has_upgrade(*build_st, upg);
	}

	virtual bool build(buildpred::state& st) override {
		using namespace buildpred;
		if (opening_state != -1) return false;

		build_army = army;
		build_st = &st;

		using namespace unit_types;
		using namespace upgrade_types;

		//st.dont_build_refineries = true;

		maxprod(marine);
		//maxprod(firebat);

		if (scv_count >= 16) {
			if (medic_count < 1 + marine_count / 5 + firebat_count / 3) {
				build(medic);
			}
			
			upgrade(stim_packs) && upgrade(u_238_shells);
		}

		if (army_supply > enemy_army_supply) {
			if (scv_count < 30 || army_supply >= 30.0) {
				if (scv_count < 70) build(scv);
			}
		}

//		if (scv_count >= 34) {
//			build_n(engineering_bay, 2);
//			upgrade_wait(terran_infantry_armor_1) && upgrade_wait(terran_infantry_armor_3) && upgrade_wait(terran_infantry_armor_3);
//			upgrade_wait(terran_infantry_weapons_1) && upgrade_wait(terran_infantry_weapons_2) && upgrade_wait(terran_infantry_weapons_3);
//		}
////		if (scv_count >= 22) {
////			//build_n(missile_turret, 2);
////			upgrade(terran_infantry_weapons_1);
////		}

//		if (army_supply >= 4.0) {
//			build_n(science_vessel, 1 + (int)(army_supply / 16.0));
//		}
////		if (scv_count >= 16 && marine_count >= 4) {
////			build_n(cc, 2);
////		}

		if (force_expand && count_production(st, cc) == 0) {
			build(cc);
		}

		return build_army(st);
	}
};

