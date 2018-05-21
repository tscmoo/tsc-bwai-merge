
struct strat_t_any_vz_bio : strat_mod {
	
	int total_scvs_made = 0;

	virtual void mod_init() override {

	}

	virtual void mod_tick() override {

		auto st = buildpred::get_my_current_state();
		//combat::no_aggressive_groups = st.frame < 15 * 60 * 14;
		combat::no_aggressive_groups = st.frame < 15 * 60 * 8;
		if (st.frame < 15 * 60 * 20) {
			//if (is_defending && enemy_army_supply >= army_supply * 0.5) combat::no_aggressive_groups = true;
			//if (enemy_lurker_count >= 4 && science_vessel_count < 3) combat::no_aggressive_groups = true;
		}
		combat::aggressive_vultures = true;
		
		total_scvs_made = my_units_of_type[unit_types::scv].size() + game->self()->deadUnitCount(unit_types::scv->game_unit_type);
		
		combat::use_decisive_engagements = false;
		combat::attack_bases = true;
		
		//if (scv_count >= 12) resource_gathering::max_gas = 1000.0;
		
		scouting::comsat_supply = 50.0;
		
		place_static_defence_only_at_main = !my_units_of_type[unit_types::bunker].empty() && my_units_of_type[unit_types::missile_turret].size() < 2;

	}

	virtual void mod_build(buildpred::state& st) override {

		using namespace unit_types;
		using namespace upgrade_types;

		st.dont_build_refineries = count_units_plus_production(st, refinery) == 1 &&st.frame < 15 * 60 * 11;
		
		maxprod(marine);
		//build(ghost);
		build(siege_tank_tank_mode);
		build_n(medic, marine_count / 4);
		//if (marine_count > enemy_air_army_supply * 2) build_n(firebat, marine_count / 3);
		
		//build(siege_tank_tank_mode);
		//build(ghost);
		build_n(science_vessel, tank_count / 2 + std::max((int)st.bases.size() - 2, 0));
		
		if (army_supply >= 40.0 || tank_count >= 4) {
			if (science_vessel_count) upgrade(irradiate);
		}
		
		if (tank_count < 2) build(siege_tank_tank_mode);
		
		if (st.frame >= 15 * 60 * 16) {
			build_n(engineering_bay, 2);
			upgrade_wait(terran_infantry_armor_1) && upgrade_wait(terran_infantry_armor_2) && upgrade_wait(terran_infantry_armor_3);
			upgrade_wait(terran_infantry_weapons_1) && upgrade_wait(terran_infantry_weapons_2) && upgrade_wait(terran_infantry_weapons_3);
		}
		
		if (tank_count) upgrade(siege_mode);
		upgrade_wait(stim_packs) && upgrade(u_238_shells);
		
		if (st.frame >= 15 * 60 * 5) {
			if (can_expand && scv_count >= max_mineral_workers + 8 && count_production(st, cc) == 0) {
				if (st.frame >= 15 * 60 * 17 || st.bases.size() < 2) {
					if (n_mineral_patches < 8 * 3) build(cc);
				}
			} else if (count_production(st, scv) == 0 && total_scvs_made < 6 + st.frame / 15 * 30) {
				build_n(scv, 48);
			} else if (((players::opponent_player->race == race_terran && tank_count >= 8) || st.frame >= 15 * 60 * 20) && total_scvs_made < 6 + st.frame / 15 * 10) {
				build_n(scv, 62);
			}
			
			if (enemy_cloaked_unit_count) build_n(science_vessel, 1);
		}
		
		if (count_production(st, scv) < 2) {
			if (army_supply > enemy_army_supply || army_supply >= 6.0) build_n(scv, 20);
			if (army_supply > enemy_army_supply * 1.5) build_n(scv, st.frame < 15 * 60 * 17 ? 52 : 72);
		}
		
		if (st.used_supply[race_terran] >= 186 && wraith_count < 6) build(wraith);
		
		if (st.frame < 15 * 60 * 14) {
			
			maxprod(marine);
			
			upgrade(terran_infantry_armor_1);
			build_n(engineering_bay, 2);
			upgrade(terran_infantry_weapons_2);
			
			//build(ghost);
			build_n(science_vessel, 3);
			
			build(siege_tank_tank_mode);
			//build(ghost);
			if (tank_count) upgrade(siege_mode);
			upgrade(terran_infantry_weapons_1);
			upgrade_wait(stim_packs) && upgrade(u_238_shells);
			
			build_n(engineering_bay, 1);
			build_n(academy, 1);
			build_n(barracks, 2);
			build_n(refinery, 1);
			
			build_n(scv, 30);
			
			build_n(marine, 16);
			build_n(firebat, 2);
			build_n(marine, 8);
			if (count_units_plus_production(st, academy)) build_n(medic, marine_count / 3);
			build_n(bunker, 1);
			
			if (opponent_has_expanded) {
				build_n(cc, 2);
				build_n(scv, 15);
			} else {
				build_n(cc, 2);
				build_n(scv, 18);
				build_n(marine, 4);
			}
			
			build_n(marine, 1);
			build_n(scv, 8);
			
			if (st.frame >= 15 * 60 * 7 && army_supply > enemy_army_supply) build(siege_tank_tank_mode);
			
			if (enemy_lurker_count) {
				build(siege_tank_tank_mode);
				upgrade(siege_mode);
				build_n(missile_turret, 1);
				build_n(bunker, 2);
			}
		}
	}

};
