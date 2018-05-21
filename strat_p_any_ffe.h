
struct strat_p_any_ffe : strat_mod {
	
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
		
		total_shield_battery_energy = 0.0;
		for (unit* u : my_completed_units_of_type[unit_types::shield_battery]) {
			total_shield_battery_energy += u->energy;
			if (current_frame - u->creation_frame <= 15 * 60 * 2) total_shield_battery_energy += 50.0;
		}
		
		scouting::no_proxy_scout = true;
		scouting::scout_supply = 9.0;
		
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
		
		if (current_frame >= 15 * 60 * 10) {
			maxprod(zealot);
			build(high_templar);
			build(archon);
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

		if (players::opponent_player->race == race_protoss) {
			maxprod(zealot);
			build(dragoon);
			if (army_supply >= 36.0 || enemy_reaver_count) {
				build_n(reaver, 6);
				build_n(shuttle, (reaver_count + 1) / 2);
			}
			if (dragoon_count >= 3) upgrade(singularity_charge);
		} else {
			upgrade(leg_enhancements);
		}
		//upgrade(protoss_ground_weapons_1) && upgrade(protoss_ground_weapons_2);
		upgrade_wait(protoss_ground_weapons_1) && upgrade(protoss_ground_armor_1);
		
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
		if (players::opponent_player->race == race_protoss && army_supply < 16.0) {
			if (!opponent_has_expanded) build_n(photon_cannon, 3);
		}
		
		if (current_frame < 15 * 60 * 10) {

			if (players::opponent_player->race == race_protoss && !players::opponent_player->random) {

				maxprod(dragoon);
				build_total(probe, 21);
				build(dragoon);
				build_total(probe, 18);
				build_n(photon_cannon, 2);
				build_n(cybernetics_core, 1);
				if (opponent_has_expanded) build_total(probe, 18);
				build_n(assimilator, 1);
				build_total(probe, 15);
				build_n(zealot, 5);
				build_n(gateway, 2);
				build_total(probe, 14);
				build_n(gateway, 1);
				build_n(probe, 13);

				//build_n(forge, 1);
				build_n(nexus, 2);
				build_n(pylon, 1);
				build_n(probe, 12);

				if (count_units_plus_production(st, forge)) {
					if (enemy_army_supply >= 8.0) build_n(photon_cannon, 3);
					if (enemy_army_supply >= 11.0) build_n(photon_cannon, 4);
					if (enemy_army_supply >= 14.0) build_n(photon_cannon, 5);
				} else if (enemy_zealot_count > zealot_count) build(zealot);

			} else {
				build(probe);
				if (total_units_made_of_type[probe] >= 25) {
					build_total(probe, 27) && build_n(gateway, 3) && upgrade(leg_enhancements);
					build_n(corsair, 2);
					build_n(citadel_of_adun, 1);
				}
				upgrade(protoss_ground_weapons_1);
				build_n(stargate, 1);
				build(zealot);

				build_total(probe, 22);
				build_n(cybernetics_core, 1);
				build_total(probe, 18);
				build_n(zealot, 2);
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
			}

		} else {
			if (st.frame >= 15 * 60 * 20) {
				if ((force_expand || (can_expand && probe_count >= max_mineral_workers + 8)) && count_production(st, nexus) == 0) {
					build(nexus);
				}
			}
		}
		
		if (st.minerals >= 400 && count_units_plus_production(st, pylon) < 4) build(pylon);
		
	}

};
