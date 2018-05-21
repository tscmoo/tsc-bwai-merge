
struct strat_p_any_dragoon_pressure : strat_mod {
	
	int total_probes_made = 0;
	
	double total_shield_battery_energy = 0.0;
	bool zealot_rushed = false;

	virtual void mod_init() override {

	}

	virtual void mod_tick() override {

		auto st = buildpred::get_my_current_state();
		if (st.frame < 15 * 60 * 11) combat::no_aggressive_groups = true;
		else if (st.frame >= 15 * 60 * 16) combat::no_aggressive_groups = false;
		else attack_interval = 15 * 60;
		
		total_probes_made = my_units_of_type[unit_types::probe].size() + game->self()->deadUnitCount(unit_types::probe->game_unit_type);
		
		if (st.bases.size() >= 4) enable_auto_upgrades = true;
		
		total_shield_battery_energy = 0.0;
		for (unit* u : my_completed_units_of_type[unit_types::shield_battery]) {
			total_shield_battery_energy += u->energy;
			if (current_frame - u->creation_frame <= 15 * 60 * 2) total_shield_battery_energy += 50.0;
		}
		
		if (current_frame < 15 * 60 * 10 && enemy_zealot_count >= 5) zealot_rushed = true;
		else if (current_frame < 15 * 60 * 12 && enemy_zealot_count > std::max(dragoon_count, 4)) zealot_rushed = true;
		
		if (zealot_rushed && dark_templar_count) combat::no_aggressive_groups = false;
		combat::attack_bases = true;

	}

	virtual void mod_build(buildpred::state& st) override {

		using namespace unit_types;
		using namespace upgrade_types;

		st.dont_build_refineries = count_units_plus_production(st, assimilator) == 1 && st.frame < 15 * 60 * 8;
		
		if (current_frame < 15 * 60 * 4 && my_units_of_type[cybernetics_core].empty()) {
			build_n(cybernetics_core, 1);
			build_n(probe, 13);
			build_n(assimilator, 1);
			build_n(zealot, 1);
			build_n(probe, 12);
			build_n(gateway, 1);
			build_n(probe, 10);
			return;
		}

		if (st.frame < 15 * 60 * 10) {
			if (st.frame >= 15 * 60 * 8 && !zealot_rushed) build_n(observer, 1);
			if (st.frame < 15 * 60 * 8 && my_units_of_type[unit_types::dragoon].size() < 2) {
				build_n(probe, 20);
				build_n(dragoon, 2);
				build_n(gateway, 2);
				build_n(probe, 14);
				build_n(dragoon, 1);
				build_n(probe, 12);
				build_n(gateway, 1);
				build_n(probe, 10);
			} else {
				build_n(gateway, 3);
				build(dragoon);
				
				if (dragoon_count) upgrade(singularity_charge);
				
				build_n(probe, 20);
			}
			
			if (gateway_count) {
				if (players::opponent_player->race != race_zerg) {
					if (being_rushed && army_supply < enemy_army_supply + 2.0) {
						build_n(shield_battery, 1);
						if (!st.units[gateway].empty()) build(dragoon);
						else build(zealot);
					}
				} else {
					if (being_rushed && army_supply < enemy_army_supply + 4.0) {
						build_n(gateway, 2);
						build_n(probe, 24);
						build(zealot);
					}
				}
				build_n(zealot, 1);
			}
			
			if (zealot_rushed) {
				build_n(gateway, 3);
				build(zealot);
				build(dragoon);
				if (army_supply >= 16.0) build(dark_templar);
			} else {
				if (army_supply >= 8.0 && army_supply >= enemy_army_supply + 4.0) build_n(observer, 1);
			}
			return;
		}

		if (st.bases.size() >= 2 || !can_expand || enemy_army_supply > army_supply) {
			maxprod(zealot);
			//if (zealot_count > enemy_tank_count) {
				maxprod(dragoon);
			//}
			if (zealot_count <= enemy_tank_count || dragoon_count >= 18) {
				if (players::opponent_player->race != race_protoss || !my_units_of_type[unit_types::observer].empty()) {
					build(reaver);
				}
				//build(high_templar);
				//upgrade(psionic_storm);
				//if (!has_upgrade(st, psionic_storm) || high_templar_count >= 6) build(archon);
				//if (reaver_count < high_templar_count / 2) build(reaver);
			}
		} else {
			if (players::opponent_player->race != race_protoss) build(nexus);
			build_n(gateway, 3);
			build(zealot);
			build(dragoon);
		}
		if (players::opponent_player->race == race_protoss) {
			if (enemy_observer_count + enemy_static_defence_count == 0 && (zealot_rushed || st.frame >= 15 * 60 * 16)) build(dark_templar);
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
		
		if (st.frame >= 15 * 60 * 15) {
			upgrade(gravitic_drive);
			//build_n(shuttle, 1 + (int)(army_supply / 20.0));
			
			build_n(observer, (int)(army_supply / 20.0));
			build_n(scout, (int)(enemy_air_army_supply / 2.0));
		}
		build_n(shuttle, (reaver_count + 1) / 2);
		
		if (st.bases.size() >= 3 && probe_count >= 30) {
			build_n(robotics_facility, 2);
		}
		
		if (st.frame >= 15 * 60 * 18 || zealot_count >= 6) upgrade(leg_enhancements);
		
		if (st.frame >= 15 * 60 * 10) {
			if (army_supply >= 18.0) {
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
				
				if (enemy_observer_count + enemy_static_defence_count == 0 && (zealot_rushed || st.frame >= 15 * 60 * 16)) build(dark_templar);
			}
			
			if (can_expand && probe_count >= max_mineral_workers + 8 && count_production(st, nexus) == 0) {
				if (players::opponent_player->race != race_protoss || opponent_has_expanded || st.frame >= 15 * 60 * 15) {
					build(nexus);
				}
			} else if (count_production(st, probe) == 0 && total_probes_made < st.frame / 15 * 30) {
				build_n(probe, 48);
			}
			
			if (enemy_cloaked_unit_count) build_n(observer, std::min(enemy_cloaked_unit_count - enemy_observer_count, 4));
		}
		
		if (army_supply > enemy_army_supply || army_supply >= 6.0) build_n(probe, 20);

		if (dragoon_count) upgrade(singularity_charge);
		
		if (army_supply >= 8.0 && army_supply >= enemy_army_supply + 4.0 && (!zealot_rushed || army_supply >= 20.0)) build_n(observer, 1);
	}

};
