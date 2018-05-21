

struct strat_t_new : strat_t_base {


	virtual void init() override {

		sleep_time = 15;

		combat::defensive_spider_mine_count = 16;

// 		get_upgrades::set_upgrade_value(upgrade_types::terran_vehicle_weapons_1, -1.0);
// 		get_upgrades::set_upgrade_value(upgrade_types::terran_vehicle_plating_1, -1.0);
// 		get_upgrades::set_upgrade_value(upgrade_types::terran_vehicle_weapons_2, -1.0);
// 		get_upgrades::set_upgrade_value(upgrade_types::terran_vehicle_plating_2, -1.0);
// 		get_upgrades::set_upgrade_value(upgrade_types::terran_vehicle_weapons_3, -1.0);
// 		get_upgrades::set_upgrade_value(upgrade_types::terran_vehicle_plating_3, -1.0);

		get_upgrades::set_upgrade_value(upgrade_types::stim_packs, 1000.0);
		get_upgrades::set_upgrade_value(upgrade_types::u_238_shells, 1000.0);

		get_upgrades::set_upgrade_value(upgrade_types::terran_infantry_weapons_1, 1000.0);
		get_upgrades::set_upgrade_value(upgrade_types::terran_infantry_armor_1, 1000.0);

	}
	int max_mineral_workers = 0;
	bool has_lifted_cc = false;
	bool cc_to_lift_is_busy = false;
	bool has_cc_to_lift = false;
	xy natural_pos;
	xy natural_cc_build_pos;
	combat::choke_t natural_choke;
	bool has_built_bunker = false;
	//wall_in::wall_builder wall;
	bool wall_calculated = false;
	bool has_wall = false;
	virtual bool tick() override {
// 		if (opening_state == 0) {
// 			if (my_resource_depots.size() != 1 || my_workers.size() != 4) opening_state = -1;
// 			else {
// 				build::add_build_total(0.0, unit_types::scv, 9);
// 				build::add_build_task(1.0, unit_types::supply_depot);
// 
// 				build::add_build_total(2.0, unit_types::scv, 12);
// 				build::add_build_task(3.0, unit_types::barracks);
// 				build::add_build_task(4.0, unit_types::refinery);
// 
// 				build::add_build_total(5.0, unit_types::scv, 14);
// 
// 				build::add_build_total(5.0, unit_types::marine, 1);
// 
// 				build::add_build_task(6.0, unit_types::supply_depot);
// 				build::add_build_total(6.5, unit_types::scv, 16);
// 				//build::add_build_total(7.0, unit_types::marine, 2);
// 				build::add_build_task(8.0, unit_types::factory);
// 
// 				++opening_state;
// 			}
// 		} else if (opening_state == 1) {
// 			if (bo_all_done()) {
// 				build::add_build_total(9.0, unit_types::marine, 2);
// 				build::add_build_total(10.0, unit_types::scv, 18);
// 
// 				//build::add_build_task(10.5, unit_types::bunker);
// 
// 				build::add_build_task(11.0, unit_types::machine_shop);
// 
// 				build::add_build_total(12.0, unit_types::scv, 17);
// 
// 				build::add_build_total(12.0, unit_types::scv, 20);
// 
// 				build::add_build_task(13.0, unit_types::siege_tank_tank_mode);
// 				build::add_build_task(14.0, upgrade_types::siege_mode);
// 
// 				//build::add_build_task(14.5, unit_types::supply_depot);
// 				//build::add_build_task(14.5, unit_types::cc);
// 
// 				build::add_build_total(15.0, unit_types::marine, 4);
// 				//build::add_build_total(15.0, unit_types::marine, 6);
// 
// 				++opening_state;
// 			}
		if (opening_state == 0) {
			if (my_resource_depots.size() != 1 || my_workers.size() != 4) opening_state = -1;
			else {
				build::add_build_total(0.0, unit_types::scv, 9);
				build::add_build_task(1.0, unit_types::supply_depot);

				++opening_state;
			}
		} else if (opening_state == 1) {
			if (bo_all_done()) {
				if (players::opponent_player->race == race_zerg) {
// 					build::add_build_total(2.0, unit_types::scv, 11);
// 					build::add_build_total(3.0, unit_types::barracks, 1);
// 					build::add_build_total(4.0, unit_types::scv, 13);
// 					build::add_build_total(5.0, unit_types::refinery, 1);
// 					build::add_build_total(6.0, unit_types::marine, 4);
// 					build::add_build_total(7.0, unit_types::scv, 16);
// 					build::add_build_total(8.0, unit_types::factory, 1);

// 					build::add_build_total(2.0, unit_types::scv, 11);
// 					build::add_build_total(3.0, unit_types::barracks, 1);
// 					build::add_build_total(4.0, unit_types::refinery, 1);
// 					build::add_build_total(5.0, unit_types::scv, 15);
// 					build::add_build_total(6.0, unit_types::supply_depot, 2);
// 					build::add_build_total(6.0, unit_types::factory, 1);
// 					build::add_build_total(7.0, unit_types::vulture, 1);
// 					build::add_build_total(8.0, unit_types::machine_shop, 1);
// 					build::add_build_total(9.0, unit_types::scv, 18);
// 					build::add_build_total(10.0, unit_types::starport, 1);
// 					build::add_build_total(12.0, unit_types::scv, 21);
// 					build::add_build_total(13.0, unit_types::supply_depot, 3);
// 					build::add_build_total(13.5, unit_types::vulture, 2);
// 					build::add_build_task(14.0, upgrade_types::spider_mines);
// 					build::add_build_task(15.0, upgrade_types::ion_thrusters);

					build::add_build_total(2.0, unit_types::scv, 11);
					build::add_build_total(3.0, unit_types::barracks, 1);
					build::add_build_total(4.0, unit_types::refinery, 1);
					build::add_build_total(5.0, unit_types::scv, 15);
					build::add_build_total(5.5, unit_types::marine, 1);
					build::add_build_total(6.0, unit_types::supply_depot, 2);
					build::add_build_total(6.0, unit_types::factory, 1);
					build::add_build_total(7.5, unit_types::marine, 4);
					build::add_build_total(7.0, unit_types::vulture, 1);
					build::add_build_total(8.0, unit_types::machine_shop, 1);
					build::add_build_total(9.0, unit_types::scv, 18);
					//build::add_build_total(10.0, unit_types::factory, 2);
					//build::add_build_total(11.0, unit_types::machine_shop, 2);
					build::add_build_total(12.0, unit_types::scv, 21);
					build::add_build_total(13.0, unit_types::supply_depot, 3);
					//build::add_build_task(14.0, upgrade_types::ion_thrusters);
					build::add_build_task(15.0, upgrade_types::spider_mines);

				}
				if (players::opponent_player->race == race_terran) {
					build::add_build_total(2.0, unit_types::scv, 11);
					build::add_build_total(3.0, unit_types::barracks, 1);
					build::add_build_total(4.0, unit_types::refinery, 1);
					build::add_build_total(5.0, unit_types::scv, 15);
					build::add_build_total(5.5, unit_types::marine, 1);
					build::add_build_total(6.0, unit_types::supply_depot, 2);
					build::add_build_total(6.0, unit_types::factory, 1);
					build::add_build_total(7.0, unit_types::vulture, 1);
					build::add_build_total(8.0, unit_types::machine_shop, 1);
					build::add_build_total(9.0, unit_types::scv, 18);
					build::add_build_total(10.0, unit_types::factory, 2);
					build::add_build_total(11.0, unit_types::machine_shop, 2);
					build::add_build_total(12.0, unit_types::scv, 21);
					build::add_build_total(13.0, unit_types::supply_depot, 3);
					build::add_build_task(14.0, upgrade_types::ion_thrusters);
					build::add_build_task(15.0, upgrade_types::spider_mines);
				}
				if (players::opponent_player->race == race_protoss) {
					build::add_build_total(2.0, unit_types::scv, 11);
					build::add_build_total(3.0, unit_types::barracks, 1);
					build::add_build_total(4.0, unit_types::scv, 15);
					build::add_build_total(5.0, unit_types::cc, 2);
					build::add_build_total(5.5, unit_types::marine, 1);
					build::add_build_total(6.0, unit_types::scv, 16);
					build::add_build_total(7.0, unit_types::refinery, 1);
					build::add_build_total(8.0, unit_types::supply_depot, 2);
					build::add_build_total(9.0, unit_types::scv, 17);
					build::add_build_total(10.0, unit_types::bunker, 1);
					build::add_build_total(11.0, unit_types::marine, 2);
					build::add_build_total(12.0, unit_types::scv, 18);
					build::add_build_total(13.0, unit_types::factory, 1);
					build::add_build_total(14.0, unit_types::scv, 22);
					build::add_build_total(15.0, unit_types::marine, 5);
					build::add_build_total(16.0, unit_types::machine_shop, 1);
					build::add_build_total(17.0, unit_types::scv, 23);
					build::add_build_total(18.0, unit_types::siege_tank_tank_mode, 1);
					build::add_build_task(18.5, upgrade_types::siege_mode);
					build::add_build_total(19.0, unit_types::marine, 8);
					build::add_build_total(20.0, unit_types::scv, 26);
					build::add_build_total(21.0, unit_types::supply_depot, 3);
					build::add_build_total(22.0, unit_types::refinery, 2);
					//build::add_build_total(20.0, unit_types::engineering_bay, 1);
				}
				if (false) {
					build::add_build_total(2.0, unit_types::scv, 11);
					build::add_build_total(3.0, unit_types::barracks, 1);

					build::add_build_total(4.0, unit_types::scv, 15);
					build::add_build_total(5.0, unit_types::cc, 2);

					build::add_build_total(5.5, unit_types::marine, 1);

					build::add_build_total(6.0, unit_types::scv, 16);
					build::add_build_total(7.0, unit_types::supply_depot, 2);

					build::add_build_total(8.0, unit_types::scv, 17);

					build::add_build_total(9.0, unit_types::bunker, 1);

					build::add_build_total(10.0, unit_types::marine, 2);
					build::add_build_total(11.0, unit_types::refinery, 1);
					build::add_build_total(12.0, unit_types::academy, 1);

					build::add_build_total(12.5, unit_types::barracks, 2);

					build::add_build_total(13.0, unit_types::scv, 20);

					build::add_build_task(15.0, upgrade_types::u_238_shells);
					build::add_build_task(15.5, upgrade_types::stim_packs);

					build::add_build_total(15.75, unit_types::factory, 1);
					build::add_build_total(7.0, unit_types::supply_depot, 3);

					build::add_build_total(16.0, unit_types::scv, 22);
					build::add_build_total(17.0, unit_types::marine, 8);
					build::add_build_total(17.1, unit_types::medic, 2);

					build::add_build_total(17.25, unit_types::scv, 30);
					build::add_build_total(17.5, unit_types::marine, 16);

					build::add_build_total(18.0, unit_types::machine_shop, 1);
					build::add_build_total(19.0, unit_types::siege_tank_tank_mode, 1);

					build::add_build_task(20.0, upgrade_types::siege_mode);

				}
				if (false) {
					build::add_build_total(2.0, unit_types::scv, 11);
					build::add_build_total(3.0, unit_types::barracks, 1);

					build::add_build_total(4.0, unit_types::scv, 15);
					build::add_build_total(5.0, unit_types::cc, 2);

					build::add_build_total(5.5, unit_types::marine, 1);

					build::add_build_total(6.0, unit_types::scv, 16);
					build::add_build_total(7.0, unit_types::refinery, 1);
					build::add_build_total(8.0, unit_types::supply_depot, 2);

					build::add_build_total(9.0, unit_types::scv, 17);

					build::add_build_total(10.0, unit_types::bunker, 1);

					build::add_build_total(11.0, unit_types::marine, 2);
					build::add_build_total(12.0, unit_types::academy, 1);

					build::add_build_total(13.0, unit_types::scv, 20);
					build::add_build_total(14.0, unit_types::factory, 1);

					build::add_build_task(15.0, upgrade_types::u_238_shells);

					build::add_build_total(16.0, unit_types::scv, 22);
					build::add_build_total(17.0, unit_types::marine, 8);

					build::add_build_total(18.0, unit_types::machine_shop, 1);
					build::add_build_total(19.0, unit_types::siege_tank_tank_mode, 1);

					build::add_build_task(20.0, upgrade_types::siege_mode);

				}
				if (false) {
					build::add_build_total(2.0, unit_types::scv, 10);
					build::add_build_task(3.0, unit_types::barracks);
					build::add_build_total(4.0, unit_types::scv, 11);
					build::add_build_task(5.0, unit_types::refinery);

					build::add_build_total(6.0, unit_types::scv, 13);

					build::add_build_task(7.0, unit_types::academy);

					build::add_build_total(8.0, unit_types::scv, 15);

					build::add_build_task(9.0, upgrade_types::u_238_shells);
					build::add_build_task(9.25, unit_types::supply_depot);
					build::add_build_task(9.5, unit_types::factory);

					build::add_build_total(10.0, unit_types::scv, 17);

					build::add_build_total(11.0, unit_types::marine, 2);
					build::add_build_task(11.5, unit_types::bunker);

					build::add_build_task(12.0, unit_types::machine_shop);
					build::add_build_task(12.0, unit_types::supply_depot);

					build::add_build_task(13.0, unit_types::cc);

					build::add_build_total(11.0, unit_types::marine, 4);
				}
				if (false) {
					build::add_build_total(2.0, unit_types::scv, 11);
					build::add_build_task(3.0, unit_types::barracks);
					build::add_build_task(4.0, unit_types::refinery);

					build::add_build_total(5.0, unit_types::scv, 15);

					build::add_build_task(6.0, unit_types::supply_depot);
					build::add_build_total(7.0, unit_types::scv, 16);
					build::add_build_task(8.0, unit_types::factory);
					build::add_build_total(9.0, unit_types::scv, 17);
					//build::add_build_total(10.0, unit_types::marine, has_wall ? 1 : 6);
					build::add_build_total(10.0, unit_types::marine, 1);

					build::add_build_task(11.0, unit_types::machine_shop);

					build::add_build_total(12.0, unit_types::scv, 21);

					build::add_build_task(13.0, unit_types::bunker);
					//build::add_build_task(13.0, unit_types::cc);
					//build::add_build_total(13.0, unit_types::marine, 2);
					//build::add_build_total(14.0, unit_types::supply_depot, 3);

					build::add_build_task(14.0, unit_types::siege_tank_tank_mode);
					build::add_build_task(15.0, upgrade_types::siege_mode);

					//build::add_build_total(15.25, unit_types::marine, 3);
					//build::add_build_task(15.5, unit_types::cc);

					//build::add_build_total(16.0, unit_types::supply_depot, 3);
					//build::add_build_total(16.0, unit_types::scv, 24);
					build::add_build_total(16.0, unit_types::scv, 26);

					build::add_build_task(16.5, unit_types::cc);

					build::add_build_total(16.75, unit_types::supply_depot, 3);
					build::add_build_total(16.75, unit_types::marine, 4);

					build::add_build_task(17.0, unit_types::siege_tank_tank_mode);
					build::add_build_task(18.0, unit_types::engineering_bay);
				} else if (false) {
					call_place_expos = false;

					build::add_build_total(2.0, unit_types::scv, 10);
					build::add_build_task(3.0, unit_types::barracks);
					build::add_build_task(4.0, unit_types::refinery);

					build::add_build_total(5.0, unit_types::scv, 14);
					build::add_build_total(6.0, unit_types::marine, 2);

					build::add_build_task(7.0, unit_types::factory);
					build::add_build_task(8.0, unit_types::supply_depot);
					build::add_build_task(9.0, unit_types::machine_shop);
					build::add_build_total(10.0, unit_types::scv, 18);
					build::add_build_task(11.0, unit_types::supply_depot);

					build::add_build_total(11.5, unit_types::marine, 6);

					build::add_build_total(12.0, unit_types::scv, 22);

					build::add_build_task(13.0, unit_types::siege_tank_tank_mode);

					build::add_build_task(14.0, upgrade_types::spider_mines);

					build::add_build_total(14.5, unit_types::scv, 24);

					build::add_build_task(15.0, unit_types::cc);

					build::add_build_task(13.0, unit_types::siege_tank_tank_mode);
					build::add_build_total(16.0, unit_types::vulture, 2);

					build::add_build_total(16.5, unit_types::scv, 28);

					build::add_build_total(17.0, unit_types::factory, 2);
					build::add_build_total(18.0, unit_types::marine, 8);

					build::add_build_total(19.0, unit_types::siege_tank_tank_mode, 3);

					build::add_build_task(20.0, upgrade_types::siege_mode);
				} else if (false) {
					build::add_build_total(2.0, unit_types::scv, 10);
					build::add_build_task(3.0, unit_types::barracks);
					build::add_build_task(4.0, unit_types::refinery);

					build::add_build_total(5.0, unit_types::scv, 15);

					build::add_build_task(7.0, unit_types::factory);
					build::add_build_total(7.5, unit_types::scv, 16);
					build::add_build_task(8.0, unit_types::supply_depot);
					build::add_build_task(9.0, unit_types::machine_shop);
					build::add_build_total(10.0, unit_types::scv, 18);
					build::add_build_task(11.0, unit_types::factory);

					build::add_build_total(11.5, unit_types::marine, 4);
					//build::add_build_task(11.5, unit_types::machine_shop);

					build::add_build_total(12.0, unit_types::scv, 22);

					build::add_build_total(16.0, unit_types::factory, 2);
					build::add_build_total(12.5, unit_types::supply_depot, 3);

					build::add_build_task(13.0, upgrade_types::spider_mines);
					//build::add_build_task(14.0, upgrade_types::ion_thrusters);

					build::add_build_total(15.0, unit_types::siege_tank_tank_mode, 2);
					build::add_build_total(15.0, unit_types::vulture, 4);

					//build::add_build_total(16.0, unit_types::factory, 4);

					build::add_build_total(17.0, unit_types::supply_depot, 4);

					build::add_build_total(18.0, unit_types::vulture, 12);

					build::add_build_total(19.0, unit_types::scv, 26);

					build::add_build_total(20.0, unit_types::supply_depot, 5);
					//build::add_build_total(21.0, unit_types::cc, 2);

				}

				++opening_state;
			}
		} else if (opening_state == 2) {
			//scouting::scout_supply = 15.0;
			//call_place_expos = false;
 			resource_gathering::max_gas = 100.0;
			if (!my_units_of_type[unit_types::academy].empty()) resource_gathering::max_gas = 150.0;
			if (!my_units_of_type[unit_types::factory].empty()) resource_gathering::max_gas = 250.0;
// 			auto get_gas_when_unit_finishes = [&](double gas, unit_type* ut) {
// 				resource_gathering::max_gas = -1.0;
// 				int remaining_build_time = my_units_of_type[ut].front()->remaining_build_time;
// 				auto st = buildpred::get_my_current_state();
// 				double max_gas_income_rate = 0.0;
// 				for (auto& b : st.bases) {
// 					for (auto& r : b.s->resources) {
// 						if (r.u->type->is_gas) max_gas_income_rate += r.income_sum[players::my_player->race];
// 					}
// 				}
// 				printf("max_gas_income_rate is %g\n", max_gas_income_rate);
// 				if (remaining_build_time < gas / max_gas_income_rate) resource_gathering::max_gas = 250.0;
// 			};
// 			if (!my_units_of_type[unit_types::factory].empty()) {
// 				get_gas_when_unit_finishes(50.0, unit_types::factory);
// 			}
// 			if (!my_units_of_type[unit_types::machine_shop].empty()) {
// 				get_gas_when_unit_finishes(250.0, unit_types::machine_shop);
// 			}
			if (enemy_army_supply >= 6.0) rm_all_scouts();
			if (enemy_zergling_count > 6) bo_cancel_all();
			if (enemy_zergling_count >= (int)my_units_of_type[unit_types::marine].size() + 3) bo_cancel_all();
			if (enemy_zealot_count > (int)my_units_of_type[unit_types::marine].size() / 2) bo_cancel_all();
			if (opponent_has_expanded || (!being_rushed && enemy_static_defence_count)) {
				//bo_cancel_all();
			}
			if (current_minerals >= 500) bo_cancel_all();
			if (bo_all_done()) {
				++opening_state;
			}
		} else if (opening_state == 3) {
			if (bo_all_done()) {
				opening_state = -1;
			}
		}

		size_t bases = buildpred::get_my_current_state().bases.size();

		if (current_frame < 15 * 60) {
			auto s = get_next_base();
			if (s) {
				natural_pos = s->pos;
				natural_cc_build_pos = s->cc_build_pos;

				natural_choke = combat::find_choke_from_to(unit_types::siege_tank_tank_mode, natural_pos, combat::op_closest_base, false, false, false, 32.0 * 10, 1, true);
			}
		} else {
// 			if (combat::defence_choke.center != xy() && !wall_calculated) {
// 				wall_calculated = true;
// 
// 				wall.spot = wall_in::find_wall_spot_from_to(unit_types::zealot, combat::my_closest_base, combat::defence_choke.outside, false);
// 				wall.spot.outside = combat::defence_choke.outside;
// 
// 				wall.against(unit_types::zealot);
// 				wall.add_building(unit_types::supply_depot);
// 				wall.add_building(unit_types::barracks);
// 				has_wall = true;
// 				if (!wall.find()) {
// 					wall.add_building(unit_types::supply_depot);
// 					if (!wall.find()) {
// 						log("failed to find wall in :(\n");
// 						has_wall = false;
// 					}
// 				}
// 			}
// 			if (has_wall) {
// 				wall.build();
// 
// 				if (bases >= 2 && army_supply >= 20.0) {
// 					wall = wall_in::wall_builder();
// 					has_wall = false;
// 				}
// 			}
		}

// 		if (my_units_of_type[unit_types::marine].size() >= 1 && players::opponent_player->race != race_terran) {
// 			if (natural_pos == xy()) {
// 				auto s = get_next_base();
// 				if (s) {
// 					natural_pos = s->pos;
// 					natural_cc_build_pos = s->cc_build_pos;
// 				}
// 			}
// // 			combat::build_bunker_count = 1;
// // 			if (enemy_zealot_count) combat::build_bunker_count = 0;
// 		}
// 		if (natural_pos != xy()) {
// 			//combat::my_closest_base_override = natural_pos;
// 			//combat::my_closest_base_override_until = current_frame + 15 * 10;
// 		}

		if (natural_pos != xy() && !my_units_of_type[unit_types::bunker].empty() && bases == 1) {
			combat::my_closest_base_override = natural_pos;
			combat::my_closest_base_override_until = current_frame + 15 * 10;
		}

		max_mineral_workers = get_max_mineral_workers();

		if (my_units_of_type[unit_types::vulture].empty() && my_units_of_type[unit_types::marine].size() < 4 && current_used_total_supply < 30.0) {
			if (my_units_of_type[unit_types::siege_tank_tank_mode].empty() && my_units_of_type[unit_types::siege_tank_siege_mode].empty()) {
				if (combat::build_bunker_count == 0) combat::force_defence_is_scared_until = current_frame + 15 * 10;
			}
		}

		if (being_rushed) rm_all_scouts();

		if ((opponent_has_expanded && (players::opponent_player->race != race_zerg || current_frame >= 15 * 8)) || enemy_static_defence_count) {
			if (current_used_total_supply < 80.0) rm_all_scouts();
		}

		if (opening_state == -1) {
			resource_gathering::max_gas = 100.0;
			//if (!my_units_of_type[unit_types::factory].empty() && my_completed_units_of_type[unit_types::factory].empty()) resource_gathering::max_gas = 1.0;
			if (!my_units_of_type[unit_types::factory].empty()) resource_gathering::max_gas = 250.0;
			if (my_workers.size() >= 24 || current_minerals >= 300) resource_gathering::max_gas = 0.0;
		}
		//if (!my_completed_units_of_type[unit_types::factory].empty()) resource_gathering::max_gas = 0.0;
		//if (players::opponent_player->race == race_protoss) resource_gathering::max_gas = 0.0;
// 		if (players::opponent_player->race == race_protoss) {
// 			if (!my_completed_units_of_type[unit_types::factory].empty() && buildpred::get_my_current_state().bases.size() < 2) {
// 				resource_gathering::max_gas = 100.0;
// 			}
// 		}

		//combat::no_aggressive_groups = true;
		combat::aggressive_vultures = true;
		//combat::aggressive_vultures = false;

		combat::use_control_spots = army_supply >= 20.0;

		combat::no_aggressive_groups = false;
		if (players::opponent_player->race == race_protoss || players::opponent_player->race == race_zerg) {
		//if (players::opponent_player->race == race_protoss) {
			//if (bases < 3) combat::no_aggressive_groups = true;
			//combat::aggressive_vultures = false;
			combat::no_aggressive_groups = true;
			if (players::opponent_player->race == race_zerg) {
				//combat::aggressive_wraiths = true;
				//combat::aggressive_vultures = false;
				//combat::use_control_spots = false;
				if (my_resource_depots.size() >= 2) {
					combat::no_aggressive_groups = false;
					if (tank_count && tank_count < 8 && army_supply < 60.0) {
						combat::aggressive_wraiths = true;
						combat::no_aggressive_groups = true;
					}
				}
// 				if (my_resource_depots.size() >= 3) {
// 					combat::no_aggressive_groups = false;
// 				}
// 				if (my_units_of_type[unit_types::valkyrie].size() >= 2 || my_units_of_type[unit_types::wraith].size() >= 3) {
// 					combat::no_aggressive_groups = false;
// 				}
			}
		}
		if (my_completed_units_of_type[unit_types::vulture].empty() && current_used_total_supply - my_workers.size() < 6) combat::no_aggressive_groups = true;

		has_lifted_cc = false;
		has_cc_to_lift = false;
		cc_to_lift_is_busy = false;
		if (players::opponent_player->race == race_protoss && false) {
		//if (players::opponent_player->race == race_protoss) {
// 			if (my_resource_depots.size() < 3) {
// 				call_place_expos = false;
// 			} else call_place_expos = true;
			//if (my_resource_depots.size() >= 2) call_place_expos = true;
			//if (opponent_has_expanded || (enemy_static_defence_count && enemy_proxy_building_count == 0)) call_place_expos = true;

// 			if (my_resource_depots.size() == 2) call_place_expos = false;
// 			else call_place_expos = true;

			for (unit* u : my_completed_units_of_type[unit_types::cc]) {
				bool is_placed = false;
				for (auto& s : resource_spots::spots) {
					if (!u->building->is_lifted && s.cc_build_pos == u->building->build_pos) {
						is_placed = true;
						break;
					}
				}
				if (!is_placed) {
					has_cc_to_lift = true;
					if (army_supply > enemy_army_supply + 8.0 || army_supply >= 12.0) {
						//auto& nat = get_next_base();
						//if (nat) {
						xy cc_build_pos;
						if (bases == 1) {
							cc_build_pos = natural_cc_build_pos;
						} else {
							auto nat = get_next_base();
							if (nat) cc_build_pos = nat->cc_build_pos;
						}
						if (cc_build_pos != xy()) {
							u->controller->action = unit_controller::action_building_movement;
							u->controller->timeout = current_frame + 15 * 5;
							//u->controller->target_pos = nat->cc_build_pos;
							u->controller->target_pos = cc_build_pos;
							u->controller->lift = !u->building->is_lifted;

							if (my_resource_depots.size() <= 2) {
								//combat::my_closest_base_override = nat->cc_build_pos;
								combat::my_closest_base_override = cc_build_pos;
								combat::my_closest_base_override_until = current_frame + 15 * 10;
							}

							cc_to_lift_is_busy = u->remaining_whatever_time != 0;
							has_lifted_cc = true;
						}
					}
				}
			}
		}

		if (players::opponent_player->race == race_terran) {
			if (my_units_of_type[unit_types::cc].size() >= 4 || is_defending || current_used_total_supply >= 140.0) {
				combat::no_aggressive_groups = false;
			}
		}
		if (players::opponent_player->race == race_terran || (players::opponent_player->race == race_zerg && (enemy_lurker_count + enemy_mutalisk_count || enemy_army_supply > army_supply - 4.0))) {
			if (current_used_total_supply - my_workers.size() < 20.0) {
				bool any_running = false;
				for (auto* c : combat::live_combat_units) {
					if (current_frame - c->last_run <= 15 * 30) {
						combat::no_aggressive_groups = true;
						break;
					}
				}
			}
		}

		if (bases < 2) {
			if (enemy_attacking_army_supply < 4.0) combat::no_aggressive_groups = true;
		}

		//if (enemy_dt_count && bases >= 2) combat::build_missile_turret_count = 2;
		if (current_used_total_supply >= 120.0) combat::build_missile_turret_count = 4;
		//if (bases >= 4) combat::build_missile_turret_count = 12;
		if (combat::build_missile_turret_count < (enemy_dt_count + enemy_lurker_count) / 3) combat::build_missile_turret_count = (enemy_dt_count + enemy_lurker_count) / 3;
		//if (current_used_total_supply >= 120.0 || enemy_wraith_count) combat::build_missile_turret_count = 4;
		if (enemy_wraith_count && !my_units_of_type[unit_types::goliath].empty()) {
			scouting::comsat_supply = 30.0;
		}
		if (enemy_dt_count + enemy_lurker_count) {
			scouting::comsat_supply = 30.0;
		}

		if (current_used_total_supply - my_workers.size() >= 20 || enemy_attacking_army_supply >= 8.0) {
			bool just_vultures = true;
			if (current_used_total_supply >= 130) just_vultures = false;
			for (auto*a : combat::live_combat_units) {
				if (just_vultures && a->u->type != unit_types::vulture) continue;
				if (!a->u->type->is_flyer && !a->u->type->is_worker) a->use_for_drops_until = current_frame + 15 * 10;
			}
		}

		default_upgrades = false;
		default_bio_upgrades = false;
		if (current_used_total_supply >= 60) {
			default_upgrades = true;
		}
		//if (players::opponent_player->race == race_zerg) default_bio_upgrades = true;

		if (opening_state == -1) {
			if (my_units_of_type[unit_types::siege_tank_tank_mode].size() >= 1) get_upgrades::set_upgrade_value(upgrade_types::siege_mode, -1.0);
			if (my_units_of_type[unit_types::vulture].size() >= 2) get_upgrades::set_upgrade_value(upgrade_types::spider_mines, -1.0);
			if (my_units_of_type[unit_types::vulture].size() >= 6) get_upgrades::set_upgrade_value(upgrade_types::ion_thrusters, -1.0);
			if (my_units_of_type[unit_types::goliath].size() >= 2) get_upgrades::set_upgrade_value(upgrade_types::charon_boosters, -1.0);
		}

		//if (bases >= 2 && !being_rushed) {
		if (bases >= 3 && players::opponent_player->race != race_protoss) {
			double min_defend_supply = 4.0 * bases;

			double defend_supply = 0.0;
			for (auto* a : combat::live_combat_units) {
				if (a->never_assign_to_aggressive_groups) defend_supply += a->u->type->required_supply;
			}
			log(log_level_info, "defend_supply is %g\n", defend_supply);

			if (defend_supply < min_defend_supply && !combat::live_combat_units.empty()) {
				for (int i = 0; i < 10 && defend_supply < min_defend_supply; ++i) {
					auto* a = combat::live_combat_units[tsc::rng(combat::live_combat_units.size())];
					if (a->never_assign_to_aggressive_groups) continue;
					if (a->u->type->is_worker) continue;
					//if (a->u->type != unit_types::vulture && a->u->stats->ground_weapon) {
					if (a->u->stats->ground_weapon) {
						a->never_assign_to_aggressive_groups = true;
						defend_supply += a->u->type->required_supply;
					}
				}
			}

			if (current_used_total_supply >= 180 && current_minerals >= 1000 && current_gas >= 800) {
				for (auto* a : combat::live_combat_units) {
					if (a->never_assign_to_aggressive_groups) a->never_assign_to_aggressive_groups = false;
				}
			}

		}

		if (players::opponent_player->race == race_protoss && being_rushed && enemy_zealot_count >= marine_count / 2 + vulture_count) {
			for (auto&b : build::build_order) {
				if (b.type->unit && b.type->unit->is_resource_depot) {
					b.dead = true;
				}
			}
			for (unit* u : my_resource_depots) {
				if (!u->is_completed) u->game_unit->cancelConstruction();
			}
		}

		if (!my_units_of_type[unit_types::bunker].empty()) {
			has_built_bunker = true;
		}

		if (players::opponent_player->race == race_zerg && current_used_total_supply < 170) {
			get_upgrades::set_no_auto_upgrades(true);
		}

		return false;
	}
	virtual bool build(buildpred::state&st) override {
		using namespace buildpred;
		if (opening_state != -1) return false;

		auto army = this->army;

		int cc_count = count_units_plus_production(st, unit_types::cc);
		//double desired_army_supply = std::max(scv_count*scv_count* 0.015 + scv_count * 0.8 + cc_count * 8.0 - 16, 4.0);
		double desired_army_supply = std::max(scv_count*scv_count* 0.015 + scv_count * 0.8 - 16, 4.0);

		//log(log_level_info, " cc_count is %d\n", cc_count);

		st.dont_build_refineries = count_units_plus_production(st, unit_types::refinery) == 1 && army_supply < 12.0 && st.minerals < 300;

		if (((being_rushed && army_supply < 8.0) || (enemy_zergling_count * 0.5 + enemy_zealot_count * 2 > enemy_marine_count + enemy_vulture_count * 2 && army_supply < 16.0)) && st.minerals < 500) {
			if (count_units_plus_production(st, unit_types::bunker) < 2) {
				army = [army](state&st) {
					return nodelay(st, unit_types::bunker, army);
				};
			}
			army = [army](state&st) {
				return nodelay(st, unit_types::marine, army);
			};
			if (!st.units[unit_types::factory].empty() || st.minerals >= 200) {
				army = [army](state&st) {
					return nodelay(st, unit_types::vulture, army);
				};
			}
			if (army_supply >= 8.0 || scv_count < 12) {
				army = [army](state&st) {
					return nodelay(st, unit_types::scv, army);
				};
			}
			return army(st);
		}

		if (players::opponent_player->race == race_zerg && cc_count >= 2) {

// 			if (st.minerals >= 400) {
// 				if (count_units_plus_production(st, unit_types::barracks) < 3) {
// 					army = [army](state&st) {
// 						return nodelay(st, unit_types::barracks, army);
// 					};
// 				}
// 				army = [army](state&st) {
// 					return nodelay(st, unit_types::marine, army);
// 				};
// 			}

			if (count_units_plus_production(st, unit_types::factory) < 4) {
				army = [army](state&st) {
					return nodelay(st, unit_types::factory, army);
				};
			}

			army = [army](state&st) {
				return nodelay(st, unit_types::vulture, army);
			};

			if (count_units_plus_production(st, unit_types::starport) < 4) {
				army = [army](state&st) {
					return nodelay(st, unit_types::starport, army);
				};
			}

			army = [army](state&st) {
				return nodelay(st, unit_types::wraith, army);
			};

			if (wraith_count >= 8 && (enemy_mutalisk_count > wraith_count || enemy_air_army_supply > enemy_ground_army_supply)) {
				army = [army](state&st) {
					return nodelay(st, unit_types::valkyrie, army);
				};
			}

			if (enemy_ground_army_supply > ground_army_supply) {
				army = [army](state&st) {
					return nodelay(st, unit_types::vulture, army);
				};
			}

			if (scv_count >= 60) {
				army = [army](state&st) {
					return maxprod(st, unit_types::goliath, army);
				};
				if (goliath_count * 2 >= enemy_air_army_supply) {
					army = [army](state&st) {
						return maxprod(st, unit_types::siege_tank_tank_mode, army);
					};
				}
			}

			bool need_more_tanks = tank_count < (enemy_hydralisk_count + 3) / 4 + enemy_lurker_count;
			if (need_more_tanks) {
				army = [army](state&st) {
					return nodelay(st, unit_types::siege_tank_tank_mode, army);
				};
			}

			if ((army_supply >= 20.0 && science_vessel_count < (enemy_lurker_count + 3) / 4 && !need_more_tanks) || (army_supply >= 60.0 && science_vessel_count < army_supply / 30.0)) {
				army = [army](state&st) {
					return nodelay(st, unit_types::science_vessel, army);
				};
				if (army_supply >= 45.0 && science_vessel_count) {
					if (!has_or_is_upgrading(st, upgrade_types::irradiate)) {
						army = [army](state&st) {
							return nodelay(st, upgrade_types::irradiate, army);
						};
					}
				}
			}

			//if (scv_count < 32 || army_supply > scv_count) {
			if (count_production(st, unit_types::scv) >= 2 || army_supply > enemy_army_supply) {
				if (st.bases.size() != 2 || scv_count < max_mineral_workers + 3 || army_supply >= scv_count * 0.75) {
					if (scv_count < 70) {
						army = [army](state&st) {
							return nodelay(st, unit_types::scv, army);
						};
					}
				}
			}

			if (force_expand && count_production(st, unit_types::cc) == 0 && !has_cc_to_lift && scv_count > max_mineral_workers) {
				army = [army](state&st) {
					return nodelay(st, unit_types::cc, army);
				};
			}

			return army(st);
		}

// 		if (players::opponent_player->race == race_zerg && cc_count >= 2) {
// 
// 			army = [army](state&st) {
// 				return maxprod(st, unit_types::marine, army);
// 			};
// 
// 			army = [army](state&st) {
// 				return nodelay(st, unit_types::vulture, army);
// 			};
// 
// // 			if (control_tower_count < count_units_plus_production(st, unit_types::starport)) {
// // 				army = [army](state&st) {
// // 					return nodelay(st, unit_types::control_tower, army);
// // 				};
// // 			}
// 
// // 			army = [army](state&st) {
// // 				return nodelay(st, unit_types::valkyrie, army);
// // 			};
// 
// 			//if (valkyrie_count >= enemy_mutalisk_count / 2) {
// 			if (true) {
// 				army = [army](state&st) {
// 					return maxprod(st, unit_types::wraith, army);
// 				};
// 			}
// 
// 			if (wraith_count && firebat_count < 2) {
// 				army = [army](state&st) {
// 					return nodelay(st, unit_types::firebat, army);
// 				};
// 			}
// 
// // 			if (wraith_count >= 10) {
// // 				if (machine_shop_count < 2) {
// // 					army = [army](state&st) {
// // 						return nodelay(st, unit_types::machine_shop, army);
// // 					};
// // 				}
// // 				if (count_units_plus_production(st, unit_types::factory) < 3) {
// // 					army = [army](state&st) {
// // 						return nodelay(st, unit_types::factory, army);
// // 					};
// // 				}
// // 			}
// 
// 			if (tank_count < (enemy_hydralisk_count + 3) / 4 + enemy_lurker_count) {
// 				army = [army](state&st) {
// 					return nodelay(st, unit_types::siege_tank_tank_mode, army);
// 				};
// 			}
// 
// 			//if ((scv_count >= 60 || army_supply >= 60.0) && players::my_player->has_upgrade(upgrade_types::terran_ship_weapons_1)) {
// // 			if (scv_count >= 40) {
// // 				army = [army](state&st) {
// // 					return nodelay(st, unit_types::battlecruiser, army);
// // 				};
// // 			}
// 
// // 			if (!players::my_player->has_upgrade(upgrade_types::terran_ship_weapons_1)) {
// // 				army = [army](state&st) {
// // 					return nodelay(st, upgrade_types::terran_ship_weapons_1, army);
// // 				};
// // 			}
// 
// 			if (marine_count >= 20 && medic_count < marine_count / 6) {
// 				army = [army](state&st) {
// 					return nodelay(st, unit_types::medic, army);
// 				};
// 			}
// 
// 			if ((army_supply >= 20.0 && enemy_lurker_count) || (marine_count >= 20 && science_vessel_count < marine_count / 10) || army_supply >= 40.0)  {
// 				if (science_vessel_count == 0) {
// 					army = [army](state&st) {
// 						return nodelay(st, unit_types::science_vessel, army);
// 					};
// 				}
// 			}
// 			if (science_vessel_count >= 2) {
// 				if (!has_or_is_upgrading(st, upgrade_types::irradiate)) {
// 					army = [army](state&st) {
// 						return nodelay(st, upgrade_types::irradiate, army);
// 					};
// 				}
// 			}
// 
// 			if (army_supply >= 40.0 || marine_count >= 10) {
// 				if (!has_or_is_upgrading(st, upgrade_types::stim_packs)) {
// 					army = [army](state&st) {
// 						return nodelay(st, upgrade_types::stim_packs, army);
// 					};
// 				}
// 				if (!has_or_is_upgrading(st, upgrade_types::u_238_shells)) {
// 					army = [army](state&st) {
// 						return nodelay(st, upgrade_types::u_238_shells, army);
// 					};
// 				}
// 			}
// 
// 			if (army_supply >= 40.0) {
// 				if (count_units_plus_production(st, unit_types::engineering_bay) < 2) {
// 					army = [army](state&st) {
// 						return nodelay(st, unit_types::engineering_bay, army);
// 					};
// 				}
// 				if (!has_or_is_upgrading(st, upgrade_types::terran_infantry_armor_1)) {
// 					army = [army](state&st) {
// 						return nodelay(st, upgrade_types::terran_infantry_armor_1, army);
// 					};
// 				} else if (st.upgrades.count(upgrade_types::terran_infantry_armor_1)) {
// 					if (!has_or_is_upgrading(st, upgrade_types::terran_infantry_armor_2)) {
// 						army = [army](state&st) {
// 							return nodelay(st, upgrade_types::terran_infantry_armor_1, army);
// 						};
// 					}
// 				}
// 				if (!has_or_is_upgrading(st, upgrade_types::terran_infantry_weapons_1)) {
// 					army = [army](state&st) {
// 						return nodelay(st, upgrade_types::terran_infantry_weapons_1, army);
// 					};
// 				} else if (st.upgrades.count(upgrade_types::terran_infantry_weapons_1)) {
// 					if (!has_or_is_upgrading(st, upgrade_types::terran_infantry_weapons_2)) {
// 						army = [army](state&st) {
// 							return nodelay(st, upgrade_types::terran_infantry_weapons_1, army);
// 						};
// 					}
// 				}
// 			}
// 
// 			if (scv_count < 28 || army_supply > scv_count) {
// 				if (scv_count < 70) {
// 					army = [army](state&st) {
// 						return nodelay(st, unit_types::scv, army);
// 					};
// 				}
// 			}
// 
// 			if (force_expand && count_production(st, unit_types::cc) == 0 && !has_cc_to_lift) {
// 				if (cc_count != 2 || max_mineral_workers < 40 || army_supply > 50.0) {
// 					army = [army](state&st) {
// 						return nodelay(st, unit_types::cc, army);
// 					};
// 				}
// 			}
// 
// 			return army(st);
// 		}
// 
// 		if (players::opponent_player->race == race_zerg && cc_count >= 2) {
// 			double anti_air_supply = marine_count + goliath_count * 2 + wraith_count * 2 + valkyrie_count * 3;
// 			if (count_units_plus_production(st, unit_types::barracks) < 2) {
// 				army = [army](state&st) {
// 					return nodelay(st, unit_types::barracks, army);
// 				};
// 			}
// 			if (army_supply < 40.0) {
// 				army = [army](state&st) {
// 					return nodelay(st, unit_types::marine, army);
// 				};
// 			} else {
// 				if (control_tower_count < count_units_plus_production(st, unit_types::starport)) {
// 					army = [army](state&st) {
// 						return nodelay(st, unit_types::control_tower, army);
// 					};
// 				}
// 				army = [army](state&st) {
// 					return nodelay(st, unit_types::valkyrie, army);
// 				};
// 				army = [army](state&st) {
// 					return nodelay(st, unit_types::vulture, army);
// 				};
// 			}
// 			if (army_supply >= 50.0 || tank_count < (enemy_hydralisk_count + 3) / 4 + enemy_lurker_count) {
// 				army = [army](state&st) {
// 					return maxprod(st, unit_types::siege_tank_tank_mode, army);
// 				};
// 				if (anti_air_supply < enemy_air_army_supply * 1.5) {
// 					army = [army](state&st) {
// 						return nodelay(st, unit_types::valkyrie, army);
// 					};
// 				}
// 			}
// 			if (marine_count >= 10) {
// 				if (medic_count < marine_count / 8) {
// 					army = [army](state&st) {
// 						return nodelay(st, unit_types::medic, army);
// 					};
// 				}
// 				if (!has_or_is_upgrading(st, upgrade_types::stim_packs)) {
// 					army = [army](state&st) {
// 						return nodelay(st, upgrade_types::stim_packs, army);
// 					};
// 				}
// 				if (!has_or_is_upgrading(st, upgrade_types::u_238_shells)) {
// 					army = [army](state&st) {
// 						return nodelay(st, upgrade_types::u_238_shells, army);
// 					};
// 				}
// 			}
// 			if (anti_air_supply > enemy_air_army_supply && army_supply < 30.0) {
// 				army = [army](state&st) {
// 					return nodelay(st, unit_types::vulture, army);
// 				};
// 			}
// 			if (army_supply < 50.0) {
// 				if (count_units_plus_production(st, unit_types::starport) < 3) {
// 					army = [army](state&st) {
// 						return maxprod(st, unit_types::wraith, army);
// 					};
// 				} else {
// 					army = [army](state&st) {
// 						return nodelay(st, unit_types::wraith, army);
// 					};
// 				}
// 			}
// 
// 			if (army_supply >= 30.0 && (science_vessel_count < (enemy_lurker_count + 2) / 3 || science_vessel_count < army_supply / 30.0 || (army_supply >= 60.0 && science_vessel_count < valkyrie_count - 4))) {
// 				if (army_supply >= 60.0 || army_supply > enemy_army_supply) {
// 					army = [army](state&st) {
// 						return nodelay(st, unit_types::science_vessel, army);
// 					};
// 					if (!has_or_is_upgrading(st, upgrade_types::irradiate)) {
// 						army = [army](state&st) {
// 							return nodelay(st, upgrade_types::irradiate, army);
// 						};
// 					}
// 				}
// 			}
// 
// 			if (scv_count < 28 || army_supply > scv_count) {
// 				if (scv_count < 70) {
// 					army = [army](state&st) {
// 						return nodelay(st, unit_types::scv, army);
// 					};
// 				}
// 			}
// 
// 			if (force_expand && count_production(st, unit_types::cc) == 0 && !has_cc_to_lift) {
// 				if (cc_count != 2 || max_mineral_workers < 40 || army_supply > 50.0) {
// 					army = [army](state&st) {
// 						return nodelay(st, unit_types::cc, army);
// 					};
// 				}
// 			}
// 
// 			return army(st);
// 		}

// 		if (tank_count == 0 && false) {
// 
// 			army = [army](state&st) {
// 				return nodelay(st, unit_types::marine, army);
// 			};
// 
// 			if (!has_or_is_upgrading(st, upgrade_types::u_238_shells)) {
// 				army = [army](state&st) {
// 					return nodelay(st, upgrade_types::u_238_shells, army);
// 				};
// 			} else {
// 
// 				if (!has_or_is_upgrading(st, upgrade_types::stim_packs)) {
// 					army = [army](state&st) {
// 						return nodelay(st, upgrade_types::stim_packs, army);
// 					};
// 				}
// 
// 				army = [army](state&st) {
// 					return nodelay(st, unit_types::siege_tank_tank_mode, army);
// 				};
// 			}
// 
// 			if (marine_count < 1) {
// 				army = [army](state&st) {
// 					return nodelay(st, unit_types::marine, army);
// 				};
// 			} else {
// 				if (count_units_plus_production(st, unit_types::bunker) == 0) {
// 					army = [army](state&st) {
// 						return nodelay(st, unit_types::bunker, army);
// 					};
// 				}
// 			}
// 
// 			if (cc_count < 2 && count_units_plus_production(st, unit_types::barracks)) {
// 				army = [army](state&st) {
// 					return nodelay(st, unit_types::cc, army);
// 				};
// 			}
// 
// 			army = [army](state&st) {
// 				return nodelay(st, unit_types::scv, army);
// 			};
// 
// 			return army(st);
// 		}

		if (cc_count == 1) {
			army = [army](state&st) {
				return nodelay(st, unit_types::cc, army);
			};
		}

		if (army_supply >= 20.0) {
			army = [army](state&st) {
				return maxprod(st, unit_types::vulture, army);
			};
			if (players::opponent_player->race == race_protoss || players::opponent_player->race == race_zerg) {
				//if (vulture_count >= enemy_zealot_count) {
				if (vulture_count >= tank_count) {
					army = [army](state&st) {
						return maxprod(st, unit_types::siege_tank_tank_mode, army);
					};
				}
			}
			if (players::opponent_player->race == race_terran) {
				if (tank_count < enemy_tank_count || tank_count < vulture_count / 2) {
					army = [army](state&st) {
						return maxprod(st, unit_types::siege_tank_tank_mode, army);
					};
				} else {
					if (machine_shop_count == 0) {
						army = [army](state&st) {
							return nodelay(st, unit_types::machine_shop, army);
						};
					}
				}
			}
		} else {
			//if (is_defending || enemy_army_supply > army_supply || (players::opponent_player->race != race_terran && tank_count)) {
			//if (is_defending || enemy_army_supply > army_supply || (players::opponent_player->race != race_terran && count_units_plus_production(st, unit_types::factory))) {
			if (is_defending || enemy_army_supply > army_supply || players::opponent_player->race == race_zerg) {
				if (players::opponent_player->race != race_protoss || marine_count < 6) {
					army = [army](state&st) {
						return nodelay(st, unit_types::marine, army);
					};
				}
			}
			if (players::opponent_player->race != race_protoss || vulture_count < enemy_zealot_count || tank_count >= 2) {
				army = [army](state&st) {
					return nodelay(st, unit_types::vulture, army);
				};
			}
			if (players::opponent_player->race == race_protoss || players::opponent_player->race == race_zerg) {
				if (vulture_count >= enemy_zealot_count) {
					army = [army](state&st) {
						return nodelay(st, unit_types::siege_tank_tank_mode, army);
					};
				}
			}
// 			if (players::opponent_player->race == race_terran) {
// 				if (tank_count < enemy_tank_count || (vulture_count >= 8 && tank_count < vulture_count / 2)) {
// 					army = [army](state&st) {
// 						return nodelay(st, unit_types::siege_tank_tank_mode, army);
// 					};
// 				}
// 			}
			if (tank_count && machine_shop_count == 1) {
				army = [army](state&st) {
					return nodelay(st, unit_types::machine_shop, army);
				};
			}
		}

		double anti_air_supply = marine_count + goliath_count * 2 + wraith_count * 2 + valkyrie_count * 3;
		double desired_anti_air_supply = enemy_air_army_supply * 1.5 - enemy_shuttle_count * 1.5 - enemy_arbiter_count * 3;
		//if (army_supply >= 60.0 || scv_count >= 60) desired_anti_air_supply += 8.0;
		//if (army_supply >= 90.0) desired_anti_air_supply += 8.0;
		if (army_supply >= enemy_ground_army_supply + 20.0) desired_anti_air_supply += std::max((army_supply - anti_air_supply - enemy_ground_army_supply) / 2, 0.0);
		if (enemy_spire_count) {
			if (desired_anti_air_supply < 10.0) desired_anti_air_supply = 10.0;
		}
		if (anti_air_supply < desired_anti_air_supply) {
			if (count_units_plus_production(st, unit_types::starport) >= 2 || (goliath_count + 2) * 2 < desired_anti_air_supply) {
				if (anti_air_supply < enemy_air_army_supply || army_supply >= 60.0) {
					army = [army](state&st) {
						return maxprod(st, unit_types::wraith, army);
					};
				}
			}
			army = [army](state&st) {
				return nodelay(st, unit_types::goliath, army);
			};
			if (tank_count == 0 && army_supply < 30.0) {
				army = [army](state&st) {
					return nodelay(st, unit_types::marine, army);
				};
			}
		}
		if (st.used_supply[race_terran] >= 180 && wraith_count < 2) {
			army = [army](state&st) {
				return nodelay(st, unit_types::wraith, army);
			};
		}

		if (players::opponent_player->race == race_zerg) {
// 			if (scv_count >= 28) {
// 				if (count_units_plus_production(st, unit_types::science_facility) == 0) {
// 					army = [army](state&st) {
// 						return nodelay(st, unit_types::science_facility, army);
// 					};
// 				}
// 			}
// 
// 			if (enemy_mutalisk_count + enemy_spire_count) {
// 				if (valkyrie_count < 1 + enemy_mutalisk_count / 2) {
// 					army = [army](state&st) {
// 						return nodelay(st, unit_types::valkyrie, army);
// 					};
// 				}
// 			}

// 			if (cc_count >= 2) {
// 				if (count_units_plus_production(st, unit_types::armory) == 0) {
// 					army = [army](state&st) {
// 						return nodelay(st, unit_types::armory, army);
// 					};
// 				}
// 			}
			if (army_supply > enemy_army_supply && goliath_count == 0) {
				army = [army](state&st) {
					return nodelay(st, unit_types::goliath, army);
				};
			}
		}

		if (army_supply >= 25.0) {
			int n = 1 + enemy_dt_count / 3 + enemy_lurker_count / 3 + enemy_arbiter_count;
			if (science_vessel_count < n || (army_supply >= 70.0 && science_vessel_count < army_supply / 30.0)) {
				army = [army](state&st) {
					return maxprod(st, unit_types::science_vessel, army);
				};
			}
			if (players::opponent_player->race == race_zerg && army_supply >= 40.0) {
				if (science_vessel_count < army_supply / 20.0) {
					army = [army](state&st) {
						return maxprod(st, unit_types::science_vessel, army);
					};
				}
			}
		}

		//if (cc_count >= 2 && !is_defending && army_supply > enemy_army_supply + 8.0 && players::opponent_player->race == race_zerg) {
		if (cc_count >= 2 && army_supply > enemy_army_supply + 8.0 && players::opponent_player->race != race_terran) {
			if (count_units_plus_production(st, unit_types::missile_turret) == 0) {
				army = [army](state&st) {
					return nodelay(st, unit_types::missile_turret, army);
				};
			}
		}
		if (army_supply >= enemy_army_supply && enemy_dt_count + enemy_lurker_count) {
			if (count_units_plus_production(st, unit_types::missile_turret) < 2) {
				army = [army](state&st) {
					return nodelay(st, unit_types::missile_turret, army);
				};
			}
		}
		if (!my_units_of_type[unit_types::siege_tank_tank_mode].empty() && army_supply > enemy_army_supply) {
			if (players::opponent_player->race != race_terran) {
				if (count_units_plus_production(st, unit_types::engineering_bay) == 0) {
					army = [army](state&st) {
						return nodelay(st, unit_types::engineering_bay, army);
					};
				}
			}
		}

		if (cc_count >= 3 || army_supply >= 45.0) {
			if (!has_or_is_upgrading(st, upgrade_types::terran_vehicle_weapons_1)) {
				army = [army](state&st) {
					return nodelay(st, upgrade_types::terran_vehicle_weapons_1, army);
				};
			}
			if (st.upgrades.count(upgrade_types::terran_vehicle_weapons_1)) {
				if (!has_or_is_upgrading(st, upgrade_types::terran_vehicle_plating_1)) {
					army = [army](state&st) {
						return nodelay(st, upgrade_types::terran_vehicle_plating_1, army);
					};
				}
			}
		}

		//if (players::opponent_player->race == race_terran || players::opponent_player->race == race_zerg || tank_count >= 2 || scv_count >= 32) {
		if (players::opponent_player->race == race_terran || tank_count >= 2 || scv_count >= 32) {
		//if (players::opponent_player->race == race_terran || !my_units_of_type[unit_types::siege_tank_tank_mode].empty() || scv_count >= 32) {
			//if (cc_count >= 2 && army_supply >= 8.0 && count_units_plus_production(st, unit_types::factory) < 3) {
			//if (cc_count >= 2 && scv_count >= 24.0 && count_units_plus_production(st, unit_types::factory) < (army_supply >= enemy_army_supply ? 2 : 3)) {
			if (cc_count >= 2 && scv_count >= 24.0 && count_units_plus_production(st, unit_types::factory) < 2) {
				army = [army](state&st) {
					return nodelay(st, unit_types::factory, army);
				};
			}
		}

		//if (cc_count >= 2 || (marine_count >= 2 && !is_defending)) {
		if (marine_count >= 4 && !is_defending) {
			//if (players::opponent_player->race != race_terran) {
			if (players::opponent_player->race == race_protoss) {
				if (!has_built_bunker && count_units_plus_production(st, unit_types::bunker) == 0) {
					army = [army](state&st) {
						return nodelay(st, unit_types::bunker, army);
					};
				}
			}
		}

		if (army_supply < 16.0 && marine_count < 4) {
			army = [army](state&st) {
				return nodelay(st, unit_types::marine, army);
			};
		}

		if (players::opponent_player->race == race_protoss && army_supply < 20.0) {
			if (count_units_plus_production(st, unit_types::factory)) {
// 				if (enemy_shuttle_count + enemy_robotics_facility_count + enemy_robotics_support_bay_count) {
// 					if (count_units_plus_production(st, unit_types::missile_turret) < 4) {
// 						army = [army](state&st) {
// 							return nodelay(st, unit_types::missile_turret, army);
// 						};
// 					}
// 				}
				if (enemy_shuttle_count + enemy_robotics_facility_count + enemy_robotics_support_bay_count) {
					army = [army](state&st) {
						return nodelay(st, unit_types::marine, army);
					};
				}
				if (enemy_citadel_of_adun_count + enemy_templar_archives_count) {
					if (count_units_plus_production(st, unit_types::missile_turret) < 2) {
						army = [army](state&st) {
							return nodelay(st, unit_types::missile_turret, army);
						};
					}
				}
			}
		}

		if (players::opponent_player->race == race_protoss && army_supply < 30.0) {
			if (((cc_count >= 2 && scv_count >= 18) || scv_count >= 22) && tank_count < 2) {
				if (is_defending && (enemy_dragoon_count == 0 || enemy_zealot_count < enemy_dragoon_count)) {
					army = [army](state&st) {
						return nodelay(st, unit_types::vulture, army);
					};
				} else {
					army = [army](state&st) {
						return nodelay(st, unit_types::siege_tank_tank_mode, army);
					};
				}
			}
			if (tank_count >= 2 && enemy_tank_count > enemy_dragoon_count) {
				if (vulture_count < 2 + enemy_zealot_count + enemy_dt_count * 3) {
					army = [army](state&st) {
						return nodelay(st, unit_types::vulture, army);
					};
				}
				if (vulture_count >= enemy_zealot_count && !has_or_is_upgrading(st, upgrade_types::spider_mines)) {
					army = [army](state&st) {
						return nodelay(st, upgrade_types::spider_mines, army);
					};
				}
			}
		}
		
		if (scv_count < 28 || count_production(st, unit_types::scv) < 2 || army_supply >= desired_army_supply) {
		//if (scv_count < 28 || army_supply >= desired_army_supply) {
			if (!cc_to_lift_is_busy || scv_count < 24) {
				if (scv_count < 70 && (scv_count < 55 || (army_supply > 35.0 && army_supply > enemy_army_supply) || army_supply >= 45.0)) {
					if (scv_count < 20 || (scv_count < 28 && !is_defending) || (is_defending && army_supply > enemy_army_supply) || army_supply > 10.0 || opponent_has_expanded || st.minerals >= 200) {
						if (players::opponent_player->race != race_protoss || st.bases.size() != 2 || scv_count < max_mineral_workers + 3 || army_supply >= scv_count * 0.75) {
							army = [army](state&st) {
								return nodelay(st, unit_types::scv, army);
							};
						}
					}
				}
			}
		}

		if (enemy_shuttle_count || (is_defending && enemy_army_supply >= army_supply) || is_defending_with_workers) {
			if (army_supply < 20.0) {
				if (scv_count < 32) {
					army = [army](state&st) {
						return nodelay(st, unit_types::scv, army);
					};
				}
				army = [army](state&st) {
					return nodelay(st, unit_types::marine, army);
				};
				if (enemy_zealot_count > enemy_dragoon_count) {
					army = [army](state&st) {
						return nodelay(st, unit_types::vulture, army);
					};
				} else {
					army = [army](state&st) {
						return nodelay(st, unit_types::siege_tank_tank_mode, army);
					};
				}
			}
		}

		if (players::opponent_player->race == race_zerg && cc_count >= 2 && army_supply < 90.0) {
			//if ((scv_count >= 26 && army_supply < 20.0) || (army_supply < desired_army_supply && army_supply < 60.0)) {
			if (army_supply < desired_army_supply || anti_air_supply < desired_anti_air_supply) {
				if (anti_air_supply < desired_anti_air_supply) {
					army = [army](state&st) {
						return nodelay(st, unit_types::marine, army);
					};
				} else if (army_supply < desired_army_supply) {
					army = [army](state&st) {
						return nodelay(st, unit_types::vulture, army);
					};
				}
				army = [army](state&st) {
					return maxprod(st, unit_types::wraith, army);
				};
				if (valkyrie_count < enemy_mutalisk_count + 1) {
					army = [army](state&st) {
						return nodelay(st, unit_types::valkyrie, army);
					};
				}

// 				if (enemy_air_army_supply > anti_air_supply) {
// 					army = [army](state&st) {
// 						return nodelay(st, unit_types::goliath, army);
// 					};
// 				}

// 				if (valkyrie_count > enemy_mutalisk_count + 1 || (enemy_spire_count == 0 && wraith_count == 0)) {
// 					army = [army](state&st) {
// 						return maxprod(st, unit_types::wraith, army);
// 					};
// 				} else {
// 					if (control_tower_count < count_units_plus_production(st, unit_types::starport) - 1) {
// 						army = [army](state&st) {
// 							return nodelay(st, unit_types::control_tower, army);
// 						};
// 					}
// 					army = [army](state&st) {
// 						return maxprod(st, unit_types::valkyrie, army);
// 					};
// 				}

// 				if (machine_shop_count < count_units_plus_production(st, unit_types::factory) - 1) {
// 					army = [army](state&st) {
// 						return nodelay(st, unit_types::machine_shop, army);
// 					};
// 				}

				if (anti_air_supply > enemy_air_army_supply && army_supply >= 20.0) {
					if (firebat_count < 2) {
						army = [army](state&st) {
							return nodelay(st, unit_types::firebat, army);
						};
					}
				}
				if (tank_count < (enemy_hydralisk_count + 3) / 4 + enemy_lurker_count || (army_supply >= 60.0 && anti_air_supply >= enemy_air_army_supply && tank_count < 10)) {
					army = [army](state&st) {
						return nodelay(st, unit_types::siege_tank_tank_mode, army);
					};
				}
				//if (army_supply >= 40.0 && anti_air_supply > enemy_air_army_supply && science_vessel_count < 10) {
				if (army_supply >= 40.0 && science_vessel_count < 10) {
					army = [army](state&st) {
						return nodelay(st, unit_types::science_vessel, army);
					};
					if (!has_or_is_upgrading(st, upgrade_types::irradiate)) {
						army = [army](state&st) {
							return nodelay(st, upgrade_types::irradiate, army);
						};
					}
				}
				if (vulture_count < 4 && anti_air_supply >= enemy_air_army_supply && enemy_lurker_count <= enemy_zergling_count / 4) {
					army = [army](state&st) {
						return nodelay(st, unit_types::vulture, army);
					};
				}
			}
			if (army_supply > enemy_army_supply && wraith_count + valkyrie_count >= 4) {
				if (!has_or_is_upgrading(st, upgrade_types::terran_ship_weapons_1)) {
					army = [army](state&st) {
						return nodelay(st, upgrade_types::terran_ship_weapons_1, army);
					};
				}
			}
		}

		//if (force_expand && count_production(st, unit_types::cc) == 0 && !has_lifted_cc) {
		if (force_expand && count_production(st, unit_types::cc) == 0 && !has_cc_to_lift && scv_count > max_mineral_workers) {
			if (cc_count != 2 || max_mineral_workers < 40 || army_supply > 50.0) {
				army = [army](state&st) {
					return nodelay(st, unit_types::cc, army);
				};
			}
		}
		if (cc_count == 1 && (opponent_has_expanded || (!being_rushed && enemy_static_defence_count))) {
			if (players::opponent_player->race != race_zerg || scv_count >= 16) {
				army = [army](state&st) {
					return nodelay(st, unit_types::cc, army);
				};
			}
		}

		return army(st);
	}

	virtual void post_build() override {

		for (auto* bs : natural_choke.build_squares) {
			game->drawBoxMap(bs->pos.x, bs->pos.y, bs->pos.x + 32, bs->pos.y + 32, BWAPI::Colors::Orange, true);
		}

		if (!call_place_expos) {
			if (natural_pos != xy()) {
				auto st = buildpred::get_my_current_state();

				for (auto&b : build::build_tasks) {
					if (b.built_unit || b.dead) continue;
					if (b.type->unit && b.type->unit->is_resource_depot && b.type->unit->builder_type->is_worker) {
						xy pos = b.build_pos;
						build::unset_build_pos(&b);
					}
				}

				for (auto&b : build::build_tasks) {
					if (b.built_unit || b.dead) continue;
					if (b.type->unit && b.type->unit->is_resource_depot && b.type->unit->builder_type->is_worker) {
						xy pos = b.build_pos;
						build::unset_build_pos(&b);

						auto pred = [&](grid::build_square&bs) {
							return build::build_full_check(bs, b.type->unit);
						};
						auto score = [&](xy pos) {
							double spawn_d = unit_pathing_distance(unit_types::scv, pos, my_start_location);
							double nat_d = unit_pathing_distance(unit_types::scv, pos, natural_pos);
							return spawn_d*spawn_d + nat_d*nat_d;
						};

						std::array<xy, 1> starts;
						starts[0] = my_start_location;
						pos = build_spot_finder::find_best(starts, 128, pred, score);

						if (pos != xy()) build::set_build_pos(&b, pos);
						else build::unset_build_pos(&b);
					}
				}
			}
		}

		if (natural_pos != xy()) {
			combat::control_spot* cs = get_best_score(combat::active_control_spots, [&](combat::control_spot* cs) {
				return diag_distance(cs->pos - natural_pos);
			});
			if (cs) {
// 				test_one = natural_pos;
// 				test_two = cs->pos;
// 				test_three = cs->incoming_pos;
				size_t iterations = 0;
				for (auto&b : build::build_tasks) {
					if (b.built_unit || b.dead) continue;
					if ((b.type->unit == unit_types::bunker && my_units_of_type[unit_types::bunker].empty()) || (b.type->unit == unit_types::missile_turret && my_units_of_type[unit_types::missile_turret].size() < 2)) {
						xy pos = b.build_pos;
						build::unset_build_pos(&b);

						size_t turrets = my_units_of_type[unit_types::missile_turret].size();

						a_vector<std::tuple<xy, double>> scores;

						auto pred = [&](grid::build_square&bs) {
							if (++iterations % 1000 == 0) multitasking::yield_point();
							if (b.type->unit == unit_types::missile_turret) {
								bool is_inside = test_pred(natural_choke.inside_squares, [&](auto* nbs) {
									return nbs == &bs;
								});
								if (!natural_choke.inside_squares.empty() && !is_inside) return false;
							}
							for (unit* u : enemy_units) {
								if (u->stats->ground_weapon && diag_distance(u->pos - bs.pos) <= u->stats->ground_weapon->max_range + 64) {
									return false;
								}
							}
							return build::build_full_check(bs, b.type->unit);
						};
						auto score = [&](xy pos) {
							double r = 0.0;
							xy center_pos = pos + xy(b.type->unit->tile_width * 16, b.type->unit->tile_height * 16);
							for (auto* nbs : natural_choke.build_squares) {
								double d = diag_distance(center_pos - nbs->pos);
								r += d*d;
							}
							return r;
// 							double inc_d = unit_pathing_distance(unit_types::scv, pos, cs->incoming_pos);
// 							double nat_d = unit_pathing_distance(unit_types::scv, pos, natural_pos);
// 							//if (b.type->unit == unit_types::missile_turret && turrets == 0 && players::opponent_player->race == race_protoss) return inc_d*inc_d + nat_d*nat_d*2.0;
// 							if (b.type->unit == unit_types::missile_turret && turrets == 0) return inc_d*inc_d + nat_d*nat_d*2.0;
// 							scores.emplace_back(pos, inc_d*inc_d + nat_d*nat_d);
// 							return inc_d*inc_d + nat_d*nat_d;
							//return inc_d*inc_d;
						};

						std::array<xy, 1> starts;
						starts[0] = natural_pos;
						pos = build_spot_finder::find_best(starts, 128, pred, score);

						double highest_score = 0.0;
						for (auto& v : scores) {
							if (std::get<1>(v) > highest_score) highest_score = std::get<1>(v);
						}
						for (auto& v : scores) {
							double s = std::get<1>(v) / highest_score;
							int r = (int)(s * 255);
							xy pos = std::get<0>(v);
							game->drawBoxMap(pos.x, pos.y, pos.x + 32, pos.y + 32, BWAPI::Color(r, r, r));
						}

						if (pos != xy()) build::set_build_pos(&b, pos);
						else build::unset_build_pos(&b);
					}
				}
			}
		}

	}

};

