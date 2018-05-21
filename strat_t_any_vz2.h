
struct strat_t_any_vz2 : strat_mod {
	
	int total_scvs_made = 0;

	virtual void mod_init() override {
		//combat::defensive_spider_mine_count = 40;
	}

	virtual void mod_tick() override {

		auto st = buildpred::get_my_current_state();
		combat::no_aggressive_groups = st.frame < 15 * 60 * 6 && my_units_of_type[unit_types::vulture].empty();
		
		total_scvs_made = my_units_of_type[unit_types::scv].size() + game->self()->deadUnitCount(unit_types::scv->game_unit_type);
		
		combat::use_decisive_engagements = false;
		combat::attack_bases = true;
		
		//combat::defensive_spider_mine_count = 40;
		
		//if (scv_count >= 12) resource_gathering::max_gas = 1000.0;
		
		scouting::comsat_supply = 80.0;
		scouting::scan_disabled = true;
		
		base_harass_defence::defend_enabled = false;
		
		place_static_defence_only_at_main = !my_units_of_type[unit_types::bunker].empty() && my_units_of_type[unit_types::missile_turret].size() < 2;
		
		if (army_supply >= 60.0) combat::build_missile_turret_count = 5;
		if (army_supply >= 90.0) combat::build_missile_turret_count = 15;
		
		//if (enemy_air_army_supply > valkyrie_count * 3 + 4.0) combat::build_missile_turret_count = 5;
		
		bool keep_valkyries_back = valkyrie_count < enemy_air_army_supply / 2.5;
		
		for (auto* a : combat::live_combat_units) {
			if (a->u->type == unit_types::marine) {
				a->never_assign_to_aggressive_groups = true;
			}
			if (a->u->type == unit_types::valkyrie) {
				a->never_assign_to_aggressive_groups = keep_valkyries_back;
			}
		}

	}

	virtual void mod_build(buildpred::state& st) override {

		using namespace unit_types;
		using namespace upgrade_types;

		st.dont_build_refineries = count_units_plus_production(st, refinery) == 1 &&st.frame < 15 * 60 * 11;
		
		if (st.frame < 15 * 60 * 8 && my_units_of_type[unit_types::factory].empty()) {
			build_n(marine, 3);
			build_n(factory, 1);
			build_n(scv, players::opponent_player->race == race_terran ? 16 : 14);
			build_n(marine, 1);
			build_n(barracks, 1) && build_n(refinery, 1);
			build_n(supply_depot, 1);
			build_n(scv, 11);
			if (players::opponent_player->race == race_zerg || players::opponent_player->random) {
				build_n(barracks, 1) && build_n(marine, 3 + enemy_attacking_army_supply / 2.0);
				build_n(scv, 8);
			}
			return;
		}

		
		if (count_units_plus_production(st, starport) < 2) {
			build_n(starport, 2);
			if (control_tower_count < count_units_plus_production(st, starport)) build(control_tower);
			build_n(factory, 3);
			build(vulture);
		} else {
			maxprod(vulture);
			//if (ground_army_supply > enemy_ground_army_supply) build(wraith);
		}
		upgrade(ion_thrusters);
		upgrade(spider_mines);
		
		build(siege_tank_tank_mode);
		upgrade(siege_mode);
		
		if (enemy_air_army_supply > 4 + valkyrie_count * 3 + goliath_count * 1.5) {
			build_n(goliath, vulture_count / 2 - enemy_hydralisk_count / 4);
		}
		if (vulture_count > enemy_ground_army_supply && enemy_air_army_supply > (valkyrie_count + goliath_count) * 1.5) {
			build(goliath);
		}
		
//		if (enemy_air_army_supply > valkyrie_count * 3 + goliath_count * 1.5) {
//			build_n(missile_turret, st.bases.size() * 3);
//			build_n(starport, 2);
//		}
		
		if (valkyrie_count < enemy_air_army_supply / 1.5 + (ground_army_supply > enemy_ground_army_supply ? 4 : 1)) {
			build(valkyrie);
		}
		
		if (goliath_count >= 5) upgrade(charon_boosters);
		
		if (st.frame >= 15 * 60 * 5) {
			if (can_expand && scv_count >= max_mineral_workers + 8 && count_production(st, cc) == 0) {
				if (st.frame >= 15 * 60 * 17 || st.bases.size() < 2) {
					if (n_mineral_patches < 8 * 3) build(cc);
				}
			} else if (count_production(st, scv) == 0 && total_scvs_made < 6 + st.frame / 15 * 30) {
				//build_n(scv, 48);
			} else if (((players::opponent_player->race == race_terran && tank_count >= 8) || st.frame >= 15 * 60 * 20) && total_scvs_made < 6 + st.frame / 15 * 10) {
				//build_n(scv, 62);
			}
			
			//if (enemy_cloaked_unit_count) build_n(science_vessel, 1);
		}
		
		if (st.frame >= 15 * 60 * 14 || army_supply >= enemy_army_supply + 16.0) {
			if (count_production(st, scv) < 2) {
				if (army_supply > enemy_army_supply || army_supply >= 6.0) build_n(scv, 20);
				if (army_supply > enemy_army_supply * 1.5) build_n(scv, st.frame < 15 * 60 * 17 ? 52 : 72);
			}
		}
		
		if (st.used_supply[race_terran] >= 186 && wraith_count < 6) build(wraith);
		
		if (st.frame < 15 * 60 * 14) {
//			if (valkyrie_count >= 2 && ground_army_supply > enemy_ground_army_supply && valkyrie_count * 3 > enemy_air_army_supply) {
//				//build_n(wraith, 2);
//			} else {
//				build_n(valkyrie, ground_army_supply > enemy_ground_army_supply ? 3 : 1);
//			}
			if (vulture_count >= 7) build_n(marine, 8);
			build_n(valkyrie, ground_army_supply > enemy_ground_army_supply ? 3 : 1);
			if (count_production(st, vulture) == 0) {
				build(vulture);
			} else build(vulture);
			if (vulture_count >= 2) {
				upgrade(ion_thrusters);
				//build_n(engineering_bay, 1);
				if (ground_army_supply > enemy_ground_army_supply && !my_units_of_type[armory].empty()) build_n(valkyrie, 2);
				if (army_supply > enemy_army_supply) build_n(scv, 24);
				upgrade(spider_mines);
				
				build_n(factory, 2);
				build_n(valkyrie, 1);
				build_n(armory, 1);
				build_n(starport, 1);
				if (n_mineral_patches < 12) build_n(cc, 2);
				build_n(scv, 20);
				
				if (valkyrie_count && marine_count >= 4) build_n(bunker, 1);
			}
		}
		//if (enemy_mutalisk_count) build_n(missile_turret, std::min((int)st.bases.size() * 3, enemy_mutalisk_count));
	}

};
