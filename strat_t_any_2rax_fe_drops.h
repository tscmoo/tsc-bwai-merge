
struct strat_t_any_2rax_fe_drops : strat_mod {
	
	int total_scvs_made = 0;

	virtual void mod_init() override {

	}

	virtual void mod_tick() override {

		auto st = buildpred::get_my_current_state();
		combat::no_aggressive_groups = st.frame < 15 * 60 * 9;
		combat::aggressive_vultures = true;
		
		total_scvs_made = my_units_of_type[unit_types::scv].size() + game->self()->deadUnitCount(unit_types::scv->game_unit_type);
		
		if (st.bases.size() >= 4) enable_auto_upgrades = true;

	}

	virtual void mod_build(buildpred::state& st) override {

		using namespace unit_types;
		using namespace upgrade_types;

		st.dont_build_refineries = count_units_plus_production(st, refinery) == 1 && st.frame < 15 * 60 * 8;

		if (st.frame < 15 * 60 * 8 && my_units_of_type[unit_types::factory].empty()) {
			build_n(scv, 20);
			build_n(marine, 9);
			build_n(factory, 1);
			build_n(scv, 15);
			build_n(marine, 3);
			build_n(barracks, 1) && build_n(scv, 13) && build_n(refinery, 1);
			build_n(supply_depot, 1);
			build_n(scv, 11);
			return;
		}
		
		if (machine_shop_count < count_units(st, factory)) build(machine_shop);
		
		if (st.frame < 15 * 60 * 10) {
			
			maxprod(marine);
			build_n(medic, marine_count / 3);
			upgrade(stim_packs) && upgrade(u_238_shells);
			build_n(vulture, 2);
			
			build_n(dropship, 1);
			
			build_n(scv, 20);
			
			return;
		}
		
		maxprod(marine);
		if (marine_count > enemy_air_army_supply + 8.0) build_n(firebat, marine_count / 4);
		build_n(medic, (marine_count + firebat_count) / 3);
		build_n(dropship, (int)(marine_count + medic_count + firebat_count) / 8);
		
		upgrade(stim_packs) && upgrade(u_238_shells);
		
		if (enemy_air_army_supply && count_production(st, missile_turret) < 2) build_n(missile_turret, st.bases.size() * 3);
		
		if (st.frame >= 15 * 60 * 20) {
			build_n(science_vessel, (int)(army_supply / 30.0));
		}
		
		if (st.frame >= 15 * 60 * 5) {
			if (can_expand && scv_count >= max_mineral_workers + 8 && count_production(st, cc) == 0) {
				build(cc);
			} else if (count_production(st, scv) == 0 && total_scvs_made < 6 + st.frame / 15 * 30) {
				build_n(scv, 48);
			}
			
			if (enemy_cloaked_unit_count) build_n(science_vessel, 1);
		}
		
		if (army_supply > enemy_army_supply || army_supply >= 6.0) build_n(scv, 20);
		
		if (goliath_count) upgrade(charon_boosters);
	}

};
