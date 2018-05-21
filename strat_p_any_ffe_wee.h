
struct strat_p_any_ffe_wee : strat_mod {

	int total_probes_made = 0;

	double total_shield_battery_energy = 0.0;

	virtual void mod_init() override {

		use_default_early_rush_defence = false;

	}

	virtual void mod_tick() override {

		auto st = buildpred::get_my_current_state();
		if (st.frame < 15 * 60 * 12) combat::no_aggressive_groups = true;
		else if (st.frame >= 15 * 60 * 16) combat::no_aggressive_groups = false;
		else attack_interval = 15 * 60;

		if (st.frame < 15 * 60 * 20 && army_supply < enemy_army_supply + 16.0) combat::no_aggressive_groups = true;

		if (players::opponent_player->race != race_zerg) {
			if (st.frame < 15 * 60 * 16) combat::no_aggressive_groups = true;
			else attack_interval = 15 * 60;
		}

		combat::attack_bases = true;

		total_probes_made = my_units_of_type[unit_types::probe].size() + game->self()->deadUnitCount(unit_types::probe->game_unit_type);

		if (st.bases.size() >= 4) enable_auto_upgrades = true;
		enable_auto_upgrades = false;

		total_shield_battery_energy = 0.0;
		for (unit* u : my_completed_units_of_type[unit_types::shield_battery]) {
			total_shield_battery_energy += u->energy;
			if (current_frame - u->creation_frame <= 15 * 60 * 2) total_shield_battery_energy += 50.0;
		}

		scouting::no_proxy_scout = true;
		scouting::scout_supply = 9.0;

		resource_gathering::max_gas = 0.0;

		if (players::opponent_player->race == race_protoss && !players::opponent_player->random) {
			place_ffe = !my_units_of_type[unit_types::pylon].empty();
			place_ffe_all_pylons = my_units_of_type[unit_types::pylon].size() == 1;
		} else {
			place_ffe = true;
		}

		total_cannon_hp = 0;
		completed_cannon_count = 0;
		for (unit* u : my_completed_units_of_type[unit_types::photon_cannon]) {
			total_cannon_hp += u->hp;
			++completed_cannon_count;
		}

		if (current_frame < 15 * 60 * 10 && natural_pos != xy()) {
			if (is_defending || being_early_rushed) {
				int n_probes = 2 + (int)enemy_army_supply + enemy_zergling_count / 2 - (int)my_completed_units_of_type[unit_types::zealot].size();
				if (being_pool_rushed && n_probes < 9) {
					n_probes = probe_count - 2;
				}
				n_probes -= my_completed_units_of_type[unit_types::photon_cannon].size() * 2;
				if (n_probes > probe_count / 2) n_probes = probe_count / 2;
				if (being_zergling_rushed && my_completed_units_of_type[unit_types::photon_cannon].size() < 2) n_probes = probe_count - 4;
				a_unordered_set<combat::combat_unit*> selected;
				int i = 0;
				while (n_probes > 0) {
					++i;
					--n_probes;
					combat::combat_unit* a = get_best_score(combat::live_combat_units, [&](combat::combat_unit* a) {
							if (selected.find(a) != selected.end()) return std::numeric_limits<double>::infinity();
							return unit_pathing_distance(a->u, natural_pos);
					}, std::numeric_limits<double>::infinity());
					if (!a) break;
					selected.insert(a);
					if (my_completed_units_of_type[unit_types::nexus].size() == 1 && i <= 3) {
						a->strategy_busy_until = current_frame + 15 * 5;
						a->subaction = combat::combat_unit::subaction_move;
						a->target_pos = natural_pos;
						if (!natural_choke.build_squares.empty()) {
							size_t index = tsc::rng(natural_choke.build_squares.size());
							a->target_pos = natural_choke.build_squares[index]->pos + xy(16, 16);
						}
					}
					unit* target = get_best_score(enemy_units, [&](unit* e) {
						auto* w = e->is_flying ? a->u->stats->air_weapon : a->u->stats->ground_weapon;
						if (!w) return std::numeric_limits<double>::infinity();
						auto cannon_in_range = [&]() {
							for (unit* u : my_units_of_type[unit_types::photon_cannon]) {
								if (diag_distance(e->pos - u->pos) <= 32 * 5) return true;
							}
							return false;
						};
						if (!cannon_in_range()) return std::numeric_limits<double>::infinity();
						return diag_distance(e->pos - a->u->pos);
					}, std::numeric_limits<double>::infinity());
					if (target) {
						a->strategy_busy_until = current_frame + 15 * 5;
						a->subaction = combat::combat_unit::subaction_fight;
						a->target = target;
					}
				}
			}
		}

	}

	virtual void mod_build(buildpred::state& st) override {

		using namespace unit_types;
		using namespace upgrade_types;

		st.dont_build_refineries = count_units_plus_production(st, assimilator) == 1 && st.frame < 15 * 60 * 8;

		if (true) {
			maxprod(zealot);
			//if (zealot_count >= enemy_tank_count - 2 || dragoon_count < enemy_vulture_count) {
			if (st.frame < 15*60*9 || zealot_count >= dragoon_count) {
				build(dragoon);
			}
			if ((zealot_count >= enemy_tank_count * 2 - 2 || zealot_count > dragoon_count) && zealot_count >= dragoon_count / 2) {
				//build(dragoon);
				//if (dragoon_count < 12 || zealot_count >= dragoon_count + dragoon_count / 2) {
				//	build(dragoon);
				//}
			}
			if (st.frame >= 15*60*11) {
				build_n(robotics_facility, 2);
			}
			//build_n(stargate, 4);
			//build_n(robotics_facility, 2);
			//build(reaver);
			//build_n(stargate, 2);
			//build(carrier);
			//build_n(robotics_facility, 3);
			//build(reaver);
			//build_n(shuttle, reaver_count);
			if (reaver_count >= 12 || carrier_count || st.bases.size() >= 4) {
				maxprod(carrier);
			}
			if (st.bases.size() >= 3) {
				//build_n(stargate, 2);
				//build_n(arbiter, (int)(army_supply / 20.0) - carrier_count / 2);
				//upgrade(stasis_field);
				//maxprod(reaver);
				//build_n(stargate, 3);
				//build_n(scout, enemy_tank_count / 2);
				//build_n(high_templar, (int)(army_supply / 12.0));
				//upgrade(psionic_storm);
//				if ((army_supply > enemy_army_supply && army_supply >= 80.0) || carrier_count) build_n(stargate, 4);
//				build_n(arbiter, (int)(army_supply / 12.0));
//				//build_n(corsair, shuttle_count + arbiter_count);
//				//build_n(scout, shuttle_count + arbiter_count - enemy_anti_air_army_supply / 3);
//				//build_n(scout, shuttle_count + arbiter_count);
//				build_n(scout, (int)(army_supply / 16.0));
//				if (archon_count < shuttle_count + arbiter_count) {
//					//build(archon);
//					//build(high_templar);
//				}
//				//upgrade(disruption_web);
//				build_n(reaver, (int)(army_supply / 18.0));
//				build_n(shuttle, std::min(reaver_count, 3));
				if (army_supply > enemy_anti_air_army_supply && (army_supply >= 90.0 || carrier_count || st.bases.size() >= 7)) {
					//if (army_supply >= 75.0) build_n(stargate, 3);
					//build(carrier);
				}
			//	build_n(arbiter, (int)(army_supply / 18.0));
				//upgrade(recall);
//				build_n(forge, 2);
//				upgrade(protoss_ground_armor_3);
//				upgrade(protoss_ground_armor_2);
//				upgrade(protoss_ground_armor_1);
//				upgrade(protoss_ground_weapons_3);
//				upgrade(protoss_ground_weapons_2);
//				upgrade(protoss_ground_weapons_1);
				build(carrier);
				upgrade(carrier_capacity);
				if (carrier_count >= 8) {
//					if (scout_count < carrier_count / 3) {
//						build(scout);
//					}
					if (arbiter_count < carrier_count / 3) {
						upgrade(stasis_field);
						build(arbiter);
					}
				}
//				upgrade(stasis_field);
//				if (arbiter_count == 0) build(arbiter);
			}
			if (st.bases.size() >= 4 && carrier_count <= arbiter_count * 2) {
				//maxprod(carrier);
			}
			if (reaver_count >= 6) {
				//if (dragoon_count < reaver_count * 2) build(dragoon);
				//upgrade(psionic_storm);
				if (archon_count < reaver_count) {
					//build(high_templar);
					//build(archon);
				}
				//build_n(high_templar, reaver_count);
				//build_n(dark_templar, reaver_count);
			}
			//build_n(corsair, reaver_count);
//			if (scout_count >= 7 && reaver_count * 2 + dragoon_count / 2 < scout_count) {
//				build_n(gateway, 2);
//				build(dragoon);
//				build(reaver);
//			}

//			build_n(shield_battery, scout_count / 3);

//			int carrier_prod = carrier_count;
//			if ((army_supply >= 25.0 || scout_count >= 10) && carrier_prod == 0) carrier_prod = 1;
//			if (count_production(st, carrier) < carrier_prod) {
//				maxprod(carrier);
//			}

//			if (army_supply >= 60.0 && army_supply > enemy_army_supply) {
//				upgrade(protoss_air_armor_3);
//				upgrade(protoss_air_weapons_3);
//				upgrade(protoss_air_armor_2);
//				upgrade(protoss_air_weapons_2);
//				upgrade(protoss_air_armor_1);
//				upgrade(protoss_air_weapons_1);
//				//build_n(cybernetics_core, 2);
//			}

			if (reaver_count >= 2) upgrade(scarab_damage);
			if (shuttle_count >= 2) upgrade(gravitic_drive);
			if (zealot_count >= 10 || army_supply >= 40.0) {
				upgrade(leg_enhancements);
			}
			if (scout_count >= 2) upgrade(gravitic_thrusters) && upgrade(apial_sensors);
			if (carrier_count >= 2) upgrade(carrier_capacity);
			if (dragoon_count >= 2) upgrade(singularity_charge);

			if (players::opponent_player->race != race_protoss || (army_supply >= 50.0 || enemy_cloaked_unit_count)) {
				if (army_supply >= 30.0) {
					build_n(observer, (int)army_supply / 20.0);
				}
				if (enemy_cloaked_unit_count + enemy_lurker_count) build_n(observer, (enemy_cloaked_unit_count + 1) / 2);
			}

			if (st.frame >= 15 * 60 * 9) {
				if (static_defence_pos != xy() && (static_defence_pos_is_undefended || st.bases.size() >= 3 || enemy_spire_count)) {
					build_n(photon_cannon, st.bases.size() * 2 + 3);
				}
				//if (army_supply > enemy_army_supply + 4.0) upgrade(protoss_ground_weapons_2);
			}
			if (st.frame >= 15 * 60 * 6) {
				//if (army_supply > enemy_army_supply + 4.0) upgrade(protoss_ground_weapons_1);
				if (pylon_pos != xy() && count_production(st, pylon) == 0) build(pylon);
			}

//			build_n(forge, 2);
//			upgrade(protoss_ground_armor_3);
//			upgrade(protoss_ground_armor_2);
//			upgrade(protoss_ground_armor_1);
//			upgrade(protoss_ground_weapons_3);
//			upgrade(protoss_ground_weapons_2);
//			upgrade(protoss_ground_weapons_1);
			upgrade(protoss_air_armor_3);
			upgrade(protoss_air_armor_2);
			upgrade(protoss_air_armor_1);
			upgrade(protoss_air_weapons_3);
			upgrade(protoss_air_weapons_2);
			upgrade(protoss_air_weapons_1);
			build_n(cybernetics_core, 2);

			if (army_supply > enemy_army_supply && (players::opponent_player->race != race_protoss || probe_count < 24 || st.frame >= 15 * 60 * 15)) {
				if (army_supply >= enemy_army_supply + 16.0) build_n(probe, 50);
				else build_n(probe, 32);
			}
			if (army_supply >= enemy_army_supply * 0.75 && count_production(st, probe) == 0 && st.frame >= 24 * 60 * 12) {
				build_n(probe, 72);
			}

			if (probe_count >= 40 && army_supply >= enemy_attacking_army_supply * 2 && total_probes_made < 80) {
				build(carrier);
				if (st.bases.size() < 3 && st.frame < 15 * 60 * 15) {
					build_n(corsair, 2);
					build_n(templar_archives, 1);
				}
				build_n(probe, 60);
			}

			if (st.frame < 15 * 60 * 10) {
				build_n(gateway, 2);
				build_total(probe, 30);
				build_n(pylon, 4);
				build_n(photon_cannon, 2);
				//build_n(stargate, 1);
				build_n(pylon, 3);

				build_total(probe, 22);
				build_n(cybernetics_core, 1);
				build_total(probe, 18);
				build_n(photon_cannon, 1);
				build_total(probe, 15);
				build_n(assimilator, 1);
				build_total(probe, 14);
				build_n(gateway, 1);
				build_n(probe, 13);

				build_n(forge, 1);
				build_n(nexus, 2);
				build_n(pylon, 1);
				build_n(probe, 12);
			}

			if (st.frame >= 15 * 60 * 12 && count_production(st, nexus) == 0) {
				if (force_expand || (can_expand && probe_count >= max_mineral_workers + 8)) {
					build(nexus);
				}
				if (can_expand && probe_count >= 70 && probe_count >= max_mineral_workers / 2 && army_supply >= enemy_army_supply * 0.75) {
					build(nexus);
				}
			}
			return;
		}


		if (true) {
			maxprod(zealot);
			build(dragoon);
			build_n(stargate, 4);
			build_n(robotics_facility, 2);
			build(reaver);
			build_n(stargate, 2);
			build(scout);
			if (scout_count >= 7 && reaver_count * 2 + dragoon_count / 2 < scout_count) {
				build_n(gateway, 2);
				build(dragoon);
				build(reaver);
			}

			build_n(shield_battery, scout_count / 3);

			int carrier_prod = carrier_count;
			if ((army_supply >= 75.0 || scout_count >= 10) && carrier_prod == 0) carrier_prod = 1;
			if (count_production(st, carrier) < carrier_prod) {
				build(carrier);
			}

			if (army_supply >= 20.0 && army_supply > enemy_army_supply) {
				upgrade(protoss_air_armor_3);
				upgrade(protoss_air_weapons_3);
				upgrade(protoss_air_armor_2);
				upgrade(protoss_air_weapons_2);
				upgrade(protoss_air_armor_1);
				upgrade(protoss_air_weapons_1);
				//build_n(cybernetics_core, 2);
			}

			if (zealot_count >= 10 || army_supply >= 40.0) {
				upgrade(leg_enhancements);
			}
			if (scout_count >= 9) upgrade(gravitic_thrusters) && upgrade(apial_sensors);
			if (carrier_count >= 2) upgrade(carrier_capacity);
			if (dragoon_count >= 2) upgrade(singularity_charge);

			if (players::opponent_player->race != race_protoss || (army_supply >= 50.0 || enemy_cloaked_unit_count)) {
				if (army_supply >= 30.0) {
					build_n(observer, (int)army_supply / 20.0);
				}
				if (enemy_cloaked_unit_count + enemy_lurker_count) build_n(observer, (enemy_cloaked_unit_count + 1) / 2);
			}

			if (st.frame >= 15 * 60 * 9) {
				if (static_defence_pos != xy() && (static_defence_pos_is_undefended || st.bases.size() >= 3 || enemy_spire_count)) {
					build_n(photon_cannon, st.bases.size() * 3 + 3);
				}
			}
			if (st.frame >= 15 * 60 * 6) {
				if (pylon_pos != xy() && count_production(st, pylon) == 0) build(pylon);
			}

			if (army_supply > enemy_army_supply && (players::opponent_player->race != race_protoss || probe_count < 24 || st.frame >= 15 * 60 * 15)) {
				if (army_supply >= enemy_army_supply + 16.0) build_n(probe, 42);
				else build_n(probe, 32);
			}
			if (army_supply >= enemy_army_supply * 0.75 && count_production(st, probe) == 0 && st.frame >= 24 * 60 * 12) {
				build_n(probe, 72);
			}

			if (st.frame < 15 * 60 * 10) {
				build_n(gateway, 2);
				build_total(probe, 30);
				build_n(pylon, 4);
				build_n(photon_cannon, 2);
				build_n(stargate, 1);
				build_n(pylon, 3);

				build_total(probe, 22);
				build_n(cybernetics_core, 1);
				build_total(probe, 18);
				build_n(photon_cannon, 1);
				build_total(probe, 15);
				build_n(assimilator, 1);
				build_total(probe, 14);
				build_n(gateway, 1);
				build_n(probe, 13);

				build_n(forge, 1);
				build_n(nexus, 2);
				build_n(pylon, 1);
				build_n(probe, 12);
			}

			if (st.frame >= 15 * 60 * 12) {
				if ((force_expand || (can_expand && probe_count >= max_mineral_workers + 8)) && count_production(st, nexus) == 0) {
					build(nexus);
				}
			}
		}

		return;

		if (current_frame >= 15 * 60 * 10) {
			maxprod(zealot);
			build(dragoon);

			build_n(shield_battery, scout_count / 3);

			if (scout_count < carrier_count || (carrier_count == 0 && scout_count < 6)) maxprod(scout);
			else maxprod(carrier);
//			if (enemy_attacking_army_supply > army_supply * 0.75 && reaver_count < scout_count + carrier_count) {
//				build_n(reaver, 6);
//				build_n(shuttle, (reaver_count + 1) / 2);
//			}
			if (carrier_count < 10 && reaver_count < carrier_count + 6) {
				build_n(robotics_facility, 2);
				build(reaver);
			}
		}

		bool go_corsairs = false;
		if (players::opponent_player->race == race_zerg) {
			go_corsairs = corsair_count < 2;
			if (enemy_spire_count && corsair_count < 4) go_corsairs = true;
			if (enemy_mutalisk_count && corsair_count < 6) go_corsairs = true;
			if (enemy_ground_army_supply > ground_army_supply) go_corsairs = false;
		}

		if (go_corsairs) {
			build_n(corsair, 9);
		}

		if (army_supply >= 36.0 || enemy_reaver_count) {
			build_n(reaver, 6);
			build_n(shuttle, (reaver_count + 1) / 2);
		}

		int carrier_prod = carrier_count;
		if (army_supply >= 60.0 && carrier_prod == 0) carrier_prod = 1;
		if (count_production(st, carrier) < carrier_prod) {
			build(carrier);
		}

		if (army_supply >= 20.0 && army_supply > enemy_army_supply) {
			upgrade(protoss_air_armor_3);
			upgrade(protoss_air_weapons_3);
			upgrade(protoss_air_armor_2);
			upgrade(protoss_air_weapons_2);
			upgrade(protoss_air_armor_1);
			upgrade(protoss_air_weapons_1);
			build_n(cybernetics_core, 2);
		}

		if (zealot_count >= 10 || army_supply >= 40.0) {
			upgrade(leg_enhancements);
		}
		if (scout_count >= 2) upgrade(gravitic_thrusters) && upgrade(apial_sensors);
		if (carrier_count >= 2) upgrade(carrier_capacity);

		if (players::opponent_player->race != race_protoss || (army_supply >= 50.0 || enemy_cloaked_unit_count)) {
			if (army_supply >= 30.0) {
				build_n(observer, (int)army_supply / 20.0);
			}
			if (enemy_cloaked_unit_count + enemy_lurker_count) build_n(observer, (enemy_cloaked_unit_count + 1) / 2);
		}

		if (st.frame >= 15 * 60 * 9) {
			if (static_defence_pos != xy() && (static_defence_pos_is_undefended || st.bases.size() >= 3 || enemy_spire_count)) {
				build_n(photon_cannon, st.bases.size() * 3 + 3);
			}
		}
		if (st.frame >= 15 * 60 * 6) {
//			if (players::opponent_player->race == race_protoss) {
//				if (static_defence_pos != xy() && static_defence_pos_is_undefended) {
//					build_n(photon_cannon, st.bases.size() * 3 + 3);
//				}
//			}
			if (pylon_pos != xy() && count_production(st, pylon) == 0) build(pylon);
		}

		if (army_supply > enemy_army_supply && (players::opponent_player->race != race_protoss || probe_count < 24 || st.frame >= 15 * 60 * 15)) {
			if (army_supply >= enemy_army_supply + 16.0) build_n(probe, 42);
			else build_n(probe, 32);
		}
		if (army_supply >= enemy_army_supply * 0.75 && count_production(st, probe) == 0 && st.frame >= 24 * 60 * 12) {
			build_n(probe, 72);
		}
		if (players::opponent_player->race == race_protoss && army_supply < 16.0) {
			if (!opponent_has_expanded) build_n(photon_cannon, 3);
		}

		if (current_frame < 15 * 60 * 10) {

			build(probe);
			if (total_units_made_of_type[probe] >= 25) {
				//build_total(probe, 27) && build_n(gateway, 3) && upgrade(leg_enhancements);
//				if (enemy_attacking_army_supply + enemy_tank_count >= 6) {
//					build_n(reaver, 2);
//					build_n(shuttle, (reaver_count + 1) / 2);
//				} else {
//					build_n(reaver, 2);
//					build_n(shuttle, (reaver_count + 1) / 2);
//					build_n(scout, 2);
//				}
				//build_n(citadel_of_adun, 1);
				if (enemy_attacking_army_supply >= army_supply * 0.75) {
					build_n(gateway, 3);
					build(zealot);
					build_n(dragoon, zealot_count);
				}
			}
			//build_n(robotics_support_bay, 1);
			build_n(scout, 1);
			//upgrade(protoss_air_weapons_1);
			build_n(assimilator, 2);
			build_n(gateway, 2);
			build_total(probe, 30);
			build_n(pylon, 4);
			build_n(photon_cannon, 2);
			//build_n(robotics_facility, 1);
			build_n(stargate, 1);
			build_n(pylon, 3);

			build_total(probe, 22);
			build_n(cybernetics_core, 1);
			build_total(probe, 18);
			build_n(photon_cannon, 1);
			build_total(probe, 15);
			build_n(assimilator, 1);
			build_total(probe, 14);
			build_n(gateway, 1);
			build_n(probe, 13);
			if (opponent_has_expanded) {
				build_n(forge, 1);
				build_n(nexus, 2);
				build_n(pylon, 1);
				build_n(probe, 12);
			} else {
				build_n(nexus, 2);
				build_n(probe, 13);
				build_n(photon_cannon, 2);
				build_n(probe, 11);
				build_n(forge, 1);
				build_n(probe, 9);
				build_n(pylon, 1);
				build_n(probe, 8);
			}
			if (enemy_army_supply >= 6.0) build_n(photon_cannon, 4);
			else if (enemy_hydralisk_den_count) build_n(photon_cannon, 3);
			else if (enemy_zergling_count >= 5) build_n(photon_cannon, 3);
			if (total_cannon_hp < completed_cannon_count * 100) {
				build_n(photon_cannon, total_cannon_hp >= 200 ? 3 : 4);
				if (enemy_army_supply >= 6.0) build_n(photon_cannon, total_cannon_hp >= 300 ? 4 : 5);
			}
			if (total_cannon_hp < players::my_player->minerals_lost + players::opponent_player->minerals_lost) {
				build_n(photon_cannon, 5);
			}

		} else {
			if (st.frame >= 15 * 60 * 12) {
				if ((force_expand || (can_expand && probe_count >= max_mineral_workers + 8)) && count_production(st, nexus) == 0) {
					build(nexus);
				}
			}
		}

		if (st.minerals >= 400 && count_units_plus_production(st, pylon) < 4) build(pylon);

	}

};
