
struct strat_t_any_tank_drop : strat_mod {

	virtual void mod_init() override {

	}

	virtual void mod_tick() override {

		auto st = buildpred::get_my_current_state();
		combat::no_aggressive_groups = st.frame < 15 * 60 * 9;
		combat::aggressive_vultures = false;
		combat::use_control_spots = false;
		combat::use_map_split_defence = false;
		
		if (st.bases.size() >= 4) enable_auto_upgrades = true;
		
		harassment::pick_up_anything = true;
//		harassment::use_dropships = false;
		
//		tank_drop::start();
//		tank_drop::keepalive_until = current_frame + 15 * 10;

	}

	virtual void mod_build(buildpred::state& st) override {

		using namespace unit_types;
		using namespace upgrade_types;

		st.dont_build_refineries = count_units_plus_production(st, assimilator) == 1 && st.frame < 15 * 60 * 8;

		if (st.frame < 15 * 60 * 8 && my_units_of_type[unit_types::factory].empty()) {
			build_n(factory, 1);
			build_n(scv, 15);
			build_n(marine, 1);
			build_n(barracks, 1) && build_n(scv, 12) && build_n(refinery, 1);
			build_n(supply_depot, 1);
			build_n(scv, 11);
			if (players::opponent_player->race == race_zerg || players::opponent_player->random) {
				build_n(barracks, 1) && build_n(marine, 3 + enemy_attacking_army_supply / 2.0);
				build_n(scv, 8);
			}
			return;
		}
		
		if (machine_shop_count < count_units(st, factory)) build(machine_shop);
		
		if (st.frame < 15 * 60 * 15) {
			
			maxprod(vulture);
			
			build_n(dropship, 2);
			
			build_n(siege_tank_tank_mode, 2);
			build_n(vulture, 4);
			
			build_n(scv, 20);
			
			build_n(starport, 1);
			build_n(vulture, 1);
			
			if (count_units_plus_production(st, dropship)) upgrade(siege_mode);
			
			return;
		}
		
		if (st.frame >= 15 * 60 * 10) {
			maxprod(vulture);
			if (goliath_count < tank_count / 3 + enemy_air_army_supply / 2) {
				build(goliath);
			}
			build(siege_tank_tank_mode);
		}
		if (st.frame < 15 * 60 * 15) {
			//build_n(barracks, 2);
			//build_n(factory, 3);
			maxprod(vulture);
			build_n(armory, 1);
			build(scv);
			build_n(vulture, 3);
			//build_n(bunker, 1);
			build_n(cc, 2);
			build_n(vulture, 1);
			build_n(factory, 1);
			build_n(barracks, 1);
			//build_n(marine, 3);
		}
		
		if (count_units_plus_production(st, starport) && enemy_anti_air_army_supply < std::max(wraith_count * 2, std::min((int)army_supply, 6))) {
			build(wraith);
		}
		
		if (marine_count + goliath_count * 2 < enemy_air_army_supply * 1.5) {
			build(goliath);
		}
		
		if (vulture_count >= 4) {
			upgrade(ion_thrusters) && upgrade(spider_mines);
		}
		if (tank_count >= 1) {
			upgrade(siege_mode);
		}
		
		if (st.frame >= 15 * 60 * 25 || (st.frame >= 15 * 60 * 18 && army_supply >= enemy_army_supply + 32.0)) {
			

		}
		
		if (enemy_air_army_supply && count_production(st, missile_turret) < 2) build_n(missile_turret, st.bases.size() * 3);
		
		if (st.frame >= 15 * 60 * 20) {
			build_n(science_vessel, (int)(army_supply / 30.0));
		}
		
		if (st.frame >= 15 * 60 * 5) {
			if (can_expand && scv_count >= max_mineral_workers + 8 && count_production(st, cc) == 0) {
				build(cc);
			} else if (count_production(st, scv) == 0 && total_units_made_of_type[scv] < 6 + st.frame / 15 * 30) {
				build_n(scv, 48);
			}
			
			if (enemy_cloaked_unit_count) build_n(science_vessel, 1);
		}
		
		if (army_supply > enemy_army_supply || army_supply >= 6.0) build_n(scv, 20);
		
		if (goliath_count) upgrade(charon_boosters);
	}

};
