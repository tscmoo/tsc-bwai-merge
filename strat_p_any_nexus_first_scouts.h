
struct strat_p_any_nexus_first_scouts : strat_mod {
	
	int total_probes_made = 0;
	
	double total_shield_battery_energy = 0.0;

	virtual void mod_init() override {
		
		harassment::pick_up_dragoons = true;

	}

	virtual void mod_tick() override {

		auto st = buildpred::get_my_current_state();
		combat::no_aggressive_groups = st.frame < 15 * 60 * 8;
		
		total_probes_made = my_units_of_type[unit_types::probe].size() + game->self()->deadUnitCount(unit_types::probe->game_unit_type);
		
		if (st.bases.size() >= 4) enable_auto_upgrades = true;
		
		total_shield_battery_energy = 0.0;
		for (unit* u : my_completed_units_of_type[unit_types::shield_battery]) {
			total_shield_battery_energy += u->energy;
			if (current_frame - u->creation_frame <= 15 * 60 * 2) total_shield_battery_energy += 50.0;
		}
		
		//if (st.frame < 15 * 60 * 10) resource_gathering::max_gas = 400.0;

	}

	virtual void mod_build(buildpred::state& st) override {

		using namespace unit_types;
		using namespace upgrade_types;

		st.dont_build_refineries = count_units_plus_production(st, assimilator) == 1 && st.frame < 15 * 60 * 8;
		
		if (st.frame < 15 * 60 * 20) {
			
			maxprod(zealot);
			maxprod(dragoon);
			
			build(scout);
			build_n(corsair, enemy_air_army_supply - scout_count);
			
			if (enemy_marine_count >= 9) {
				build_n(reaver, enemy_marine_count / 6);
				build_n(shuttle, (reaver_count + 1) / 2);
			}
			
			if (scout_count >= 2) upgrade(gravitic_thrusters) && upgrade(apial_sensors);
			upgrade(singularity_charge);
			
			if (st.frame >= 15 * 60 * 16) {
				build_n(probe, 42);
				build_n(nexus, 3);
			}
			
			if (st.frame < 15 * 60 * 10) {
				build_n(gateway, 2);
				build_n(dragoon, 8);
				build_n(stargate, 2);
				
				build_n(probe, 28);
				
				build_n(dragoon, 2);
				
				build_n(probe, 20);
				
				if (army_supply < enemy_attacking_army_supply) {
					if (!st.units[cybernetics_core].empty()) {
						build(zealot);
						build(dragoon);
					} else build(zealot);
				}
				
				build_n(nexus, 2);
				build_n(probe, 14);
			}
			if (st.minerals >= 400 && count_units_plus_production(st, pylon) < 4) build(pylon);
			return;
			
		}
		
		if (st.frame < 15 * 60 * 12) {
			
			build_n(probe, 20);
			
			build(dragoon);
			build_n(gateway, 3);
			//build_n(nexus, 2);
			build_n(dragoon, 2);
			
			//build_n(reaver, 2);
			//upgrade(gravitic_drive);
			//build_n(reaver, 1);
			build_n(shuttle, 2);
			
			build_n(probe, 20);
			build_n(dragoon, 1);
			build_n(probe, 12);
			build_n(gateway, 1);
			//build_n(probe, 11);
			build_n(assimilator, 1);
			build_n(probe, 10);
			return;
			
		}

//		if (st.frame < 15 * 60 * 8 && my_units_of_type[unit_types::dragoon].size() < 2) {
//			build_n(dragoon, 2);
//			//build_n(gateway, 2);
//			build_n(probe, 14);
//			build_n(dragoon, 1);
//			build_n(probe, 12);
//			build_n(gateway, 1);
//			build_n(probe, 11);
//			build_n(assimilator, 1);
//			build_n(probe, 10);
//			return;
//		}
		
//		if (st.frame < 15 * 60 * 10) {
			
//			build_n(probe, 40);
//			build_n(nexus, 2);
			
//			build(scout);
			
//			build(reaver);
//			build_n(shuttle, (reaver_count + 1) / 2);
			
//			build_n(robotics_facility, 2);
			
//			build_n(shuttle, 1);
//			build_n(reaver, 1);
			
//			build_n(dragoon, 2);
//			build_n(probe, 20);
			
//			return;
//		}

		if (st.bases.size() >= 2 || !can_expand || enemy_army_supply > army_supply) {
			maxprod(zealot);
			//if (zealot_count > enemy_tank_count) {
				maxprod(dragoon);
			//}
			if (zealot_count <= enemy_tank_count || dragoon_count >= 18) {
				build(reaver);
				//build(high_templar);
				//upgrade(psionic_storm);
				//if (!has_upgrade(st, psionic_storm) || high_templar_count >= 6) build(archon);
				//if (reaver_count < high_templar_count / 2) build(reaver);
			}
		} else {
			build(nexus);
			build_n(gateway, 3);
			build(zealot);
			build(dragoon);
		}
		
//		if (st.bases.size() >= 3) {
//			maxprod(carrier);
//		}
		
		if (st.frame >= 15 * 60 * 25 || (st.frame >= 15 * 60 * 18 && army_supply >= enemy_army_supply + 32.0)) {
			
			build_n(reaver, 1 + (int)(army_supply / 20.0));
			
			if (drone_count >= 40) {
				upgrade(protoss_ground_weapons_1) && upgrade(protoss_ground_weapons_2);
				if (players::opponent_player->race == race_terran) {
					build_n(arbiter, 4);
					upgrade(stasis_field);
				}
				
				if (army_supply >= 80.0 && enemy_anti_air_army_supply - carrier_count * 4 < enemy_army_supply / 2) {
					maxprod(carrier);
				}
			}
		}
		
		if (enemy_wraith_count) {
			build_n(scout, enemy_wraith_count + 1);
			build_n(stargate, 2);
			build_n(corsair, scout_count);
		}
		
		if (st.frame >= 15 * 60 * 15) {
			if (scout_count >= 3) upgrade(gravitic_thrusters) && upgrade(apial_sensors);
			upgrade(gravitic_drive);
			build_n(shuttle, 1 + (int)(army_supply / 20.0));
			//build_n(shuttle, (reaver_count + 1) / 2);
		}
		
		if (st.bases.size() >= 3 && probe_count >= 30) {
			build_n(robotics_facility, 2);
		}
		
		if (st.frame >= 15 * 60 * 12 || zealot_count >= 6) upgrade(leg_enhancements);
		
		if (army_supply > enemy_army_supply) build_n(probe, 48);
		
		if (st.frame >= 15 * 60 * 10) {
			if (total_shield_battery_energy < 100) {
				if (st.frame >= 15 * 60 * 20) {
					build_n(shield_battery, 14);
				} else build_n(shield_battery, (scout_count + shuttle_count) / 2);
			}
			
			build_n(observer, 2);
			
			if (enemy_biological_army_supply >= std::max(8.0, enemy_army_supply * 0.66)) build_n(reaver, 2);
			
			if (players::my_player->minerals_lost > players::opponent_player->minerals_lost) {
				build_n(reaver, 2);
				//build_n(high_templar, 4);
				//upgrade(psionic_storm);
			}
			
			if (can_expand && probe_count >= max_mineral_workers + 8 && count_production(st, nexus) == 0) {
				build(nexus);
			} else if (count_production(st, probe) == 0 && total_probes_made < st.frame / 15 * 30) {
				build_n(probe, 48);
			}
			
			if (enemy_cloaked_unit_count) build_n(observer, 3);
		}
		
		if (army_supply > enemy_army_supply || army_supply >= 6.0) build_n(probe, 20);

		if (dragoon_count) upgrade(singularity_charge);
	}

};
