
struct strat_t_any_air : strat_mod {
	
	int total_scvs_made = 0;

	virtual void mod_init() override {
		combat::defensive_spider_mine_count = 40.0;
	}

	virtual void mod_tick() override {

		auto st = buildpred::get_my_current_state();
		//combat::no_aggressive_groups = st.frame < 15 * 60 * 9 || (st.frame > 15 * 60 * 11 && st.frame < 15 * 60 * 22);
		combat::no_aggressive_groups = st.frame < 15 * 60 * 22;
		if (players::opponent_player->race == race_zerg) {
			combat::no_aggressive_groups = st.frame < 15 * 60 * 16;
		}
		combat::aggressive_vultures = players::opponent_player->race == race_zerg;
		if (players::opponent_player->race == race_zerg) {
			if (vulture_count > enemy_dragoon_count && tank_count < 6) combat::aggressive_vultures = true;
		}
		combat::use_control_spots = false;
		combat::use_map_split_defence = st.frame >= 15 * 60 * 14 && players::opponent_player->race == race_terran;
		//combat::use_map_split_defence = players::opponent_player->race == race_terran;
		combat::attack_bases = true;
		
		//combat::defend_in_control_spots = st.frame < 15 * 60 * 15 && army_supply < 40.0;
		
//		combat::combat_mult_override = 0.0;
//		combat::combat_mult_override_until = current_frame + 15 * 10;
		
		total_scvs_made = my_units_of_type[unit_types::scv].size() + game->self()->deadUnitCount(unit_types::scv->game_unit_type);
		
		//if (st.bases.size() >= 4) enable_auto_upgrades = true;
		
		scouting::comsat_supply = 60.0;
		if (players::opponent_player->race == race_zerg) scouting::comsat_supply = 48.0;
		if (players::opponent_player->race == race_terran) scouting::comsat_supply = 100.0;
		if (st.frame >= 15 * 60 * 18) scouting::comsat_supply = 30.0;
		
		//combat::attack_bases = !combat::no_aggressive_groups && (st.frame < 15 * 60 * 20 || army_supply < enemy_army_supply * 2 + 64.0);
		combat::attack_bases = !combat::no_aggressive_groups && (st.frame < 15 * 60 * 20 || enemy_army_supply >= 8.0);
		
//		if (players::opponent_player->race == race_zerg && st.frame < 15 * 60 * 15) {
//			if (st.bases.size() >= 2 && tank_count >= 2) {
//				combat::build_missile_turret_count = 2;
//			}
//		}
		
		base_harass_defence::defend_enabled = st.bases.size() >= 3;
		//base_harass_defence::defend_enabled = st.bases.size() >= 3 && players::opponent_player->race == race_terran;
		
		if (my_units_of_type[unit_types::factory].empty()) resource_gathering::max_gas = 100.0;
		else if (my_units_of_type[unit_types::starport].empty()) resource_gathering::max_gas = 150.0;

	}

	virtual void mod_build(buildpred::state& st) override {

		using namespace unit_types;
		using namespace upgrade_types;

		st.dont_build_refineries = count_units_plus_production(st, assimilator) == 1 && st.frame < 15 * 60 * 8;

		if (st.frame < 15 * 60 * 8 && my_units_of_type[unit_types::factory].empty()) {
			build_n(factory, 1);
			build_n(scv, 20);
			build_n(marine, 4);
			build_n(bunker, 1);
			build_n(cc, 2);
			build_n(scv, 13);
			return;
		}
		
		if (st.frame < 15 * 60 * 9) {
			if (players::opponent_player->race != race_terran) build(marine);
			build(wraith);
			build_n(starport, 2);
			if (air_army_supply < 6.0) build(vulture);
			build_n(factory, 1);
			build(scv);
			if (players::opponent_player->race != race_terran) {
				if (vulture_count == 0) build_n(marine, 6);
			}
			if (players::opponent_player->race == race_protoss || vulture_count >= 6) build(siege_tank_tank_mode);
			
			if (st.frame >= 15 * 60 * 9) build_n(scv, 24);
			
			if (enemy_attacking_army_supply - enemy_vulture_count * 3 >= army_supply) build(marine);
			
			build_n(scv, 16);
			
			if (vulture_count < enemy_zealot_count) build(vulture);
			
			if (army_supply > enemy_army_supply + 4.0) build_n(scv, 40);
			
			return;
		}
		
//		build(vulture);
//		//build(battlecruiser);
//		maxprod(wraith);
		
//		if (count_production(st, wraith) >= 2 || battlecruiser_count >= 4) maxprod(battlecruiser);
		
		maxprod(vulture);
		upgrade(ion_thrusters) && upgrade(spider_mines);
		
		if (valkyrie_count < 2 + enemy_air_army_supply / 2) {
			build(valkyrie);
		}
		
		if (st.frame >= 15 * 60 * 5) {
			if (can_expand && scv_count >= max_mineral_workers + 8 && count_production(st, cc) == 0) {
				if (players::opponent_player->race != race_terran || opponent_has_expanded || st.frame >= 15 * 60 * 10) {
					if (st.frame >= 15 * 60 * 17 || st.bases.size() < 2) {
						if (n_mineral_patches < 8 * 3 || army_supply > enemy_army_supply) build(cc);
					}
				}
			} else if (count_production(st, scv) == 0 && total_scvs_made < 6 + st.frame / 15 * 30) {
				build_n(scv, 48);
			} else if (((players::opponent_player->race == race_terran && tank_count >= 8) || st.frame >= 15 * 60 * 20) && total_scvs_made < 6 + st.frame / 15 * 10) {
				build_n(scv, 62);
			}
			
			if (enemy_cloaked_unit_count) build_n(science_vessel, 1);
		}
		
		if (st.frame < 15 * 60 * 12 && vulture_count < enemy_zealot_count && enemy_dragoon_count == 0) {
			build_n(marine, 4);
			build(vulture);
		}
		
		if (count_production(st, scv) < 2) {
			if (army_supply > enemy_army_supply || army_supply >= 6.0) build_n(scv, 20);
			if (army_supply > enemy_army_supply * 1.5) build_n(scv, st.frame < 15 * 60 * 17 ? 52 : 72);
		}
		
		if (goliath_count) upgrade(charon_boosters);
	}

};
