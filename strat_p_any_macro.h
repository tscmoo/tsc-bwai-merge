
struct strat_p_any_macro : strat_mod {
	
	int total_probes_made = 0;
	
	double total_shield_battery_energy = 0.0;
	bool zealot_rushed = false;

	virtual void mod_init() override {
		scouting::scout_supply = 9;
	}

	virtual void mod_tick() override {

		auto st = buildpred::get_my_current_state();
		
		total_probes_made = my_units_of_type[unit_types::probe].size() + game->self()->deadUnitCount(unit_types::probe->game_unit_type);
		
		if (st.bases.size() >= 4) enable_auto_upgrades = true;
		
		total_shield_battery_energy = 0.0;
		for (unit* u : my_completed_units_of_type[unit_types::shield_battery]) {
			total_shield_battery_energy += u->energy;
			if (current_frame - u->creation_frame <= 15 * 60 * 2) total_shield_battery_energy += 50.0;
		}

//		if (st.frame < 15 * 60 * 8) {
//			place_ffe = true;
//			place_ffe_everything = true;
//		} else {
//			place_ffe = false;
//		}

		if (current_frame < 15 * 60 * 10 && enemy_zealot_count >= 5) zealot_rushed = true;
		else if (current_frame < 15 * 60 * 12 && enemy_zealot_count > zealot_count) zealot_rushed = true;
		
		combat::no_aggressive_groups = false;
		combat::attack_bases = true;

		combat::use_decisive_engagements = st.frame >= 15 * 60 * 15;

		bool anything_to_attack = false;
		for (unit*e : enemy_units) {
			if (e->gone) continue;
			if (e->is_flying) continue;
			if (e->type->is_worker) continue;
			anything_to_attack = true;
			break;
		}
		if (anything_to_attack) {
			rm_all_scouts();
		} else {
			if (!my_units_of_type[unit_types::zealot].empty()) {
				for (unit* u : my_units_of_type[unit_types::zealot]) {
					if (!scouting::is_scout(u)) scouting::add_scout(u);
				}
			}
		}

	}

	virtual void mod_build(buildpred::state& st) override {

		using namespace unit_types;
		using namespace upgrade_types;

		st.dont_build_refineries = count_units_plus_production(st, assimilator) == 1 && st.frame < 15 * 60 * 6;
		
		if (current_frame < 15 * 60 * 10 && my_units_of_type[dragoon].size() < 2) {
			build_n(dragoon, 2);
			if (!zealot_rushed) build_n(nexus, 2);
			build_n(cybernetics_core, 1);
			build_n(probe, 18);
			build_n(assimilator, 1);
			build_n(probe, 13);
			build_n(zealot, zealot_rushed || enemy_army_supply >= army_supply ? 6 : 3);
			build_n(gateway, 2);
			build_n(zealot, 1);
			build_n(probe, 12);
			build_n(gateway, 1);
			build_n(probe, 9);
			build_n(pylon, 1);
			build_n(probe, 8);
			return;
		}

		if (current_frame < 15 * 60 * 15) {
			build_n(probe, 70);
			if (can_expand && probe_count >= 40) build_n(nexus, 4);
			build_n(reaver, 4);
			build_n(gateway, 4);
			build_n(dragoon, 14);
			build_n(observer, 1);
			build_n(dragoon, 5);
			//build_n(dark_templar, 4);
			//build_n(dragoon, 2);
			if ((dark_templar_count && army_supply > enemy_army_supply) || probe_count < 24) build_n(probe, 40);
			if (army_supply > enemy_army_supply * 0.75) build_n(nexus, 2);
			if (!zealot_rushed || dragoon_count >= 2) upgrade(singularity_charge);
			return;
		}

		bool go_goons = false;
		if (players::opponent_player->race != race_zerg) go_goons = true;
		else {
			go_goons = st.frame >= 15 * 60 * 12;
		}

		if (zealot_count < enemy_tank_count * 3 && zealot_count < dragoon_count * 1.5) go_goons = false;

		if (go_goons) {
			if (st.minerals >= 400) maxprod(zealot);
			build(dragoon);
			upgrade(singularity_charge);
		} else {
			maxprod(zealot);
		}

		if (st.bases.size() >= 3) {
			if (army_supply >= enemy_army_supply) {
				if (army_supply >= enemy_army_supply * 1.25 || carrier_count >= 4) {
					build_n(stargate, 2);
					build(carrier);
				} else if (carrier_count) {
					build_n(stargate, 2);
					build_n(carrier, 5);
				} else {
					build_n(carrier, 1);
				}
				if (carrier_count >= 2) {
					upgrade(carrier_capacity);
					upgrade(protoss_air_weapons_1) && upgrade(protoss_air_weapons_2) && upgrade(protoss_air_weapons_3);
				}
			}

			build_n(observer, (int)(army_supply / 30.0));
		}
		if (st.bases.size() >= 4 && army_supply >= 40.0) {
			build_n(shuttle, reaver_count / 2);
			build_n(reaver, 4);
		}

		if (probe_count >= 30) {
			upgrade(leg_enhancements);
		}

		if (st.frame < 15 * 60 * 13 && (army_supply >= enemy_army_supply || army_supply >= enemy_army_supply * 0.66)) {
			build_n(dark_templar, 4);
		}

		if (st.frame >= 15 * 60 * 13) {
			if (st.bases.size() >= 3 && st.frame >= 15 * 60 * 25) {
				upgrade_wait(protoss_ground_weapons_1) && upgrade_wait(protoss_ground_weapons_2) && upgrade_wait(protoss_ground_weapons_3);
				upgrade_wait(protoss_ground_armor_1) && upgrade_wait(protoss_ground_armor_2) && upgrade_wait(protoss_ground_armor_3);
				build_n(forge, 2);
			} else {
				upgrade_wait(protoss_ground_weapons_1) && upgrade_wait(protoss_ground_weapons_2) && upgrade_wait(protoss_ground_weapons_3);
			}
		}

		if (st.frame >= 15 * 60 * 5) {
			bool allow_probes= true;
			if (players::opponent_player->race == race_zerg) {
				if (probe_count >= 48 && army_supply < enemy_army_supply + 20.0) allow_probes = false;
			}
			if (can_expand && probe_count >= max_mineral_workers + 8 && count_production(st, nexus) == 0) {
				if (players::opponent_player->race != race_terran || opponent_has_expanded || st.frame >= 15 * 60 * 10) {
					if (st.frame >= 15 * 60 * 17 || st.bases.size() < 2) {
						if (n_mineral_patches < 8 * 3 || army_supply > enemy_army_supply) build(nexus);
					}
				}
			} else if (count_production(st, probe) == 0 && total_probes_made < 6 + st.frame / (15 * 20)) {
				if (allow_probes) build_n(probe, 48);
			} else if (((players::opponent_player->race == race_terran && tank_count >= 8) || st.frame >= 15 * 60 * 20) && total_probes_made < 6 + st.frame / (15 * 10)) {
				if (allow_probes) build_n(probe, 62);
			}

			if (enemy_cloaked_unit_count) build_n(observer, 2);
		}

	}

};
