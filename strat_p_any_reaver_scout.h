
struct strat_p_any_reaver_scout : strat_mod {
	
	double total_shield_battery_energy = 0.0;

	virtual void mod_init() override {

	}

	virtual void mod_tick() override {

		//combat::no_aggressive_groups = false;
		attack_interval = 15 * 60;

		if (current_frame >= 15 * 60 * 13) {
			//combat::no_aggressive_groups = false;
			//attack_interval = 0;
		}

		combat::use_map_split_defence = true;

		//if (current_used_total_supply < 160) {
		if (current_frame < 15 * 60 * 15) {
			combat::no_aggressive_groups = true;
			attack_interval = 0;
		}
		
		total_shield_battery_energy = 0.0;
		for (unit* u : my_completed_units_of_type[unit_types::shield_battery]) {
			total_shield_battery_energy += u->energy;
		}

	}

	virtual void mod_build(buildpred::state& st) override {

		using namespace unit_types;
		using namespace upgrade_types;

		st.dont_build_refineries = count_units_plus_production(st, assimilator) == 1 && st.frame < 15 * 60 * 8;

		if (st.frame < 15 * 60 * 10 && st.bases.size() < 2) {
			build(dragoon);
			build_n(probe, 30);
			build_n(nexus, 2);
			build_n(probe, 14);
			return;
		}

		maxprod(zealot);
		if (zealot_count >= dragoon_count / 2) maxprod(dragoon);

		if (army_supply >= 20.0) {
			if (st.frame >= 15 * 60 * 12) {
				build_n(robotics_facility, 2);
			}
			build_n(high_templar, (int)(army_supply / 20.0));
			build_n(archon, (int)(army_supply / 10.0));
			
			if (st.frame >= 15 * 60 * 12) {
				if (high_templar_count >= 4) {
					build_n(reaver, 4);
				}
			}
			
			upgrade(protoss_ground_weapons_1) && upgrade(protoss_ground_weapons_2) && upgrade(protoss_ground_weapons_3);
			build_n(forge, 1);
			upgrade(psionic_storm);
			upgrade(leg_enhancements);
		}

		if (dragoon_count >= 2) upgrade(singularity_charge);
		
		if (army_supply >= 20.0) {
			build_n(dark_templar, (int)(army_supply / 12.0));
			
			build_n(reaver, dark_templar_count / 2);
			
			build_n(scout, dark_templar_count / 2);
		}
		
		if (st.frame >= 15 * 60 * 18 && army_supply >= 30.0) {
			upgrade(mind_control);
			build_n(dark_archon, dark_templar_count / 4);
		}

//		if (army_supply >= 30.0) {
//			upgrade(apial_sensors) && upgrade(gravitic_thrusters);
//		}

		build_n(observer, (int)(army_supply / 10.0));
		
		if (observer_count >= 3) {
			upgrade(gravitic_boosters) && upgrade(sensor_array);
		}
		
		//build_n(stargate, 2);
		build_n(robotics_facility, 3);
		if (reaver_count < 4 || st.gas < 300 || st.minerals >= 300) {
			build_n(reaver, 11);
		}
		
//		if (st.frame >= 15 * 60 * 12) {
//			upgrade(mind_control);
//			build_n(dark_templar, 4);
//			build_n(dark_archon, 16);
//		}
		
		build_n(high_templar, reaver_count);
//		build_n(archon, 10);
		upgrade(psionic_storm);
		upgrade(leg_enhancements);
		upgrade(singularity_charge);
		build_n(dragoon, reaver_count * 6);
		build_n(zealot, dragoon_count);
		
//		if (st.frame >= 15 * 60 * 10) {
//			build_n(scout, reaver_count);
//			upgrade(gravitic_thrusters) && upgrade(apial_sensors);
//		}
		
		build_n(shuttle, (high_templar_count + reaver_count + dark_templar_count + dark_archon_count + dragoon_count / 2) / 2);
		if (shuttle_count) upgrade(gravitic_drive);
		
		if (scout_count + shuttle_count >= 2) {
			if (total_shield_battery_energy < 100) {
				if (st.frame >= 15 * 60 * 20) {
					build_n(shield_battery, 14);
				} else build_n(shield_battery, (scout_count + shuttle_count) / 2);
			}
		}
		
		if (st.frame < 15 * 60 * 10 && dragoon_count < 2 + enemy_attacking_army_supply / 2) build_n(dragoon, 8);

		if (static_defence_pos != xy() && static_defence_pos_is_undefended) {
			if (st.bases.size() >= 3) build_n(photon_cannon, st.bases.size() * 2);
		}
		
		if (players::opponent_player->race != race_terran && st.frame < 15 * 60 * 10) {
			build_n(probe, 20);
			if (army_supply >= enemy_army_supply + 12.0) build_n(probe, 26);
			return;
		}

		int probes_inprod = 0;
		if (st.frame < 15 * 60 * 10) probes_inprod = 2;
		else if (army_supply > enemy_army_supply) probes_inprod = 3;
		else probes_inprod = 1;
		if (probe_count >= 40 && enemy_army_supply > army_supply) probes_inprod = 1;
		if (probe_count >= 40 && st.frame <= 15 * 60 * 15) probes_inprod = 0;
		if (st.bases.size() >= 3 && probes_inprod == 0) probes_inprod = 1;
		if (count_production(st, probe) < probes_inprod) build_n(probe, 62);

		if (st.frame >= 15 * 60 * 15 || st.bases.size() < (probe_count < 42 ? 2 : 3) + (army_supply > enemy_army_supply + 12.0 ? 1 : 0)) {
			if (st.frame > 15 * 60 * 12 || st.bases.size() < 2) {
				if (probe_count >= max_mineral_workers * (probe_count >= 40 ? 1.0 : 1.0)) {
					if (can_expand && count_production(st, nexus) == 0) {
						build(nexus);
					}
				}
			}
		}

	}

};
