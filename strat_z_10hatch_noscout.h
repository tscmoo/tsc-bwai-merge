

struct strat_z_10hatch_noscout : strat_z_base {


	virtual void init() override {

		sleep_time = 15;

		scouting::scout_supply = 100;
		scouting::no_proxy_scout = true;

	}
	virtual bool tick() override {
		if (opening_state == 0) {
			if (my_resource_depots.size() != 1 || my_workers.size() > 9) opening_state = -1;
			else {
				build::add_build_total(0.0, unit_types::drone, 9);
				++opening_state;
			}
		} else if (opening_state == 1) {
			bo_gas_trick();
		} else if (opening_state == 2) {
			build::add_build_task(0.0, unit_types::hatchery);
			build::add_build_task(1.0, unit_types::spawning_pool);
			build::add_build_total(2.0, unit_types::drone, 9);
			++opening_state;
		} else if (opening_state == 3) {
			bo_gas_trick();
		} else if (opening_state == 4) {
			build::add_build_total(3.0, unit_types::overlord, 2);
			build::add_build_task(5.0, unit_types::zergling);
			build::add_build_task(5.0, unit_types::zergling);
			build::add_build_task(5.0, unit_types::zergling);
			++opening_state;
// 		} else if (opening_state == 3) {
// 			//bo_gas_trick();
// 			if (bo_all_done()) {
// 				scouting::scout_supply = 10.0;
// 				build::add_build_task(3.0, unit_types::drone);
// 				build::add_build_task(4.0, unit_types::drone);
// 				build::add_build_task(5.0, unit_types::drone);
// 				++opening_state;
// 			}
// 		} else if (opening_state == 4) {
// 			if (bo_all_done()) {
// 				build::add_build_task(0.0, unit_types::zergling);
// 				build::add_build_task(0.0, unit_types::zergling);
// 				++opening_state;
// 			}
		} else if (opening_state != -1) {
			if (bo_all_done()) {
				opening_state = -1;
			}
		}

		if (enemy_proxy_building_count && army_supply == 0.0) max_bases = 1;

		return opening_state == -1;
	}
	virtual bool build(buildpred::state&st) override {
		return false;
	}

};

