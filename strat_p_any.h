
struct strat_p_any : strat_mod {

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

	}

	virtual void mod_build(buildpred::state& st) override {

		using namespace unit_types;
		using namespace upgrade_types;

		st.dont_build_refineries = false;

		if (st.frame < 15 * 60 * 10 && st.bases.size() < 2) {
			build(dragoon);
			build_n(probe, 30);
			build_n(nexus, 2);
			build_n(probe, 14);
			return;
		}

		maxprod(zealot);
		//if (zealot_count > enemy_tank_count * 2 || dragoon_count < zealot_count / 2) maxprod(dragoon);
		if (zealot_count >= dragoon_count / 2) maxprod(dragoon);
		//maxprod(archon);
		
		if (st.frame >= 15 * 60 * 16) {
			//maxprod(carrier);
			build(arbiter);
			upgrade(stasis_field);
		}

//		if (army_supply >= 80.0) {
//			build(arbiter);
//			upgrade(stasis_field);
//		}

		if (army_supply >= 20.0) {
			if (st.frame >= 15 * 60 * 12) {
				build_n(robotics_facility, 3);
			}
			build_n(high_templar, (int)(army_supply / 20.0));
			//build_n(archon, (int)(army_supply / 10.0));
			build_n(shuttle, (high_templar_count + reaver_count) / 2);
			
			if (st.frame >= 15 * 60 * 12) {
				if (high_templar_count >= 4) {
					build_n(reaver, 4);
				}
			}
			
			if (shuttle_count) upgrade(gravitic_drive);
		}
		
		if (st.frame >= 15 * 60 * 12) {
		//build_n(stargate, 2);
		//upgrade(stasis_field);
		//build_n(arbiter, (int)(army_supply / 20.0));
		
		//      build_n(reaver, (int)(army_supply / 20.0));
		//      build_n(shuttle, reaver_count);
		}

//		if (army_supply >= 20.0) {
//			upgrade(psionic_storm);
//			upgrade(leg_enhancements);
//			upgrade(protoss_ground_weapons_1) && upgrade(protoss_ground_weapons_2) && upgrade(protoss_ground_weapons_3);
//			upgrade(protoss_ground_armor_1) && upgrade(protoss_ground_armor_2) && upgrade(protoss_ground_armor_3);
//			upgrade(protoss_plasma_shields_1) && upgrade(protoss_plasma_shields_2) && upgrade(protoss_plasma_shields_3);
//			//upgrade(gravitic_drive);
//			build_n(forge, 3);
//		}

		if (dragoon_count >= 2) upgrade(singularity_charge);

		if (army_supply >= 20.0) {
			upgrade(protoss_ground_weapons_1) && upgrade(protoss_ground_weapons_2) && upgrade(protoss_ground_weapons_3);
			build_n(forge, 1);
			upgrade(psionic_storm);
			upgrade(leg_enhancements);
		}

		build_n(observer, (int)(army_supply / 20.0));

		if (static_defence_pos != xy() && static_defence_pos_is_undefended) {
			if (st.bases.size() >= 3) build_n(photon_cannon, st.bases.size() * 2);
		}

		int probes_inprod = 0;
		if (st.frame < 15 * 60 * 10) probes_inprod = 2;
		else if (army_supply > enemy_army_supply) probes_inprod = 3;
		else probes_inprod = 1;
		if (probe_count >= 40 && enemy_army_supply > army_supply) probes_inprod = 1;
		if (probe_count >= 40 && st.frame <= 15 * 60 * 15) probes_inprod = 0;
		if (st.bases.size() >= 3 && probes_inprod == 0) probes_inprod = 1;
		if (count_production(st, probe) < probes_inprod) build_n(probe, 72);

		if (st.frame >= 15 * 60 * 15 || st.bases.size() < (probe_count < 42 ? 2 : 3) + (army_supply > enemy_army_supply + 12.0 ? 1 : 0)) {
			if (probe_count >= max_mineral_workers * (probe_count >= 40 ? 0.75 : 1.0)) {
				if (can_expand && count_production(st, nexus) == 0) {
					build(nexus);
				}
			}
		}

	}

};
