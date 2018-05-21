
struct strat_p_any_2base_carriers : strat_mod {
	
	bool do_ffe = false;

	virtual void mod_init() override {
		
		do_ffe = players::opponent_player->race == race_zerg || players::opponent_player->random;
		if (do_ffe) {
			use_default_early_rush_defence = false;
		}
		
	}

	virtual void mod_tick() override {

		auto st = buildpred::get_my_current_state();
		//combat::no_aggressive_groups = st.frame < 15 * 60 * 15;
		if (st.frame < 15 *  60 * 10) combat::no_aggressive_groups = true;
		else if (st.frame > 15 * 60 * 15) combat::no_aggressive_groups = false;
		else attack_interval = 15 * 60;
		
		if (st.bases.size() >= 4) enable_auto_upgrades = true;
		
		scouting::no_proxy_scout = true;
		scouting::scout_supply = 9.0;
		
		resource_gathering::max_gas = 1300.0;
		
		if (do_ffe) place_ffe = true;
		else  if (players::opponent_player->race == race_protoss) {
			place_ffe = !my_units_of_type[unit_types::pylon].empty();
			place_ffe_all_pylons = my_units_of_type[unit_types::pylon].size() == 1;
		}
		
		if (place_ffe && players::opponent_player->race == race_zerg) {
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

	}

	virtual void mod_build(buildpred::state& st) override {

		using namespace unit_types;
		using namespace upgrade_types;

		st.dont_build_refineries = count_units_plus_production(st, assimilator) == 1 && st.frame < 15 * 60 * 8;
		
		auto ffe = [&]() {
			if (players::opponent_player->race == race_zerg || players::opponent_player->random) {
				if (probe_count >= 25) build_n(photon_cannon, 5);
				if (probe_count >= 24) build_n(photon_cannon, 3);
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
				if (probe_count >= 36) build_n(photon_cannon, std::min(5 + enemy_hydralisk_count / 3, 10));
			} else {
				
				build_total(probe, 22);
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
				
				build_n(nexus, 2);
				build_n(pylon, 1);
				build_n(probe, 12);
				
				if (count_units_plus_production(st, forge)) {
					if (enemy_army_supply >= 8.0 || probe_count >= 16) build_n(photon_cannon, 3);
					if (enemy_army_supply >= 11.0 || probe_count >= 20) build_n(photon_cannon, 4);
					if (enemy_army_supply >= 14.0 || probe_count >= 22) build_n(photon_cannon, 6);
				} else if (enemy_zealot_count > zealot_count) build(zealot);
			}
		};
		
		if (st.frame >= 15 * 60 * 5) {
			if (st.minerals >= 600) {
				maxprod(zealot);
			}
			if (st.gas >= 400) {
				build(dragoon);
			}
			build_n(probe, 40);
			maxprod(carrier);
			if (carrier_count >= 2) {
				upgrade(carrier_capacity);
				upgrade(protoss_air_weapons_1) && upgrade(protoss_air_weapons_2) && upgrade(protoss_air_weapons_3);
			}
			
			if (st.frame >= 15 * 60 * 14 && (army_supply > enemy_army_supply || st.frame >= 15 * 60 * 18)) {
				build_n(probe, 66);
				
				if ((force_expand || (can_expand && probe_count >= max_mineral_workers + 8)) && count_production(st, nexus) == 0) {
					build(nexus);
				}
			}
			
			if (carrier_count >= 12 && dragoon_count < 12 && army_supply >= enemy_army_supply + 32.0) build(dragoon);
			
			unit_type* defend_unit = players::opponent_player->race == race_zerg ? zealot : dragoon;
			if (enemy_attacking_army_supply && count_production(st, carrier) && count_production(st, defend_unit) < 2) {
				if (count_units_plus_production(st, defend_unit) < 8 && enemy_attacking_army_supply > ground_army_supply) {
					build(defend_unit);
				}
			}
			
			if (st.frame >= 15 * 60 * 17) build_n(observer, 1);
			if (enemy_cloaked_unit_count) build_n(observer, std::max((enemy_cloaked_unit_count + 1) / 2, 6));
			
			if (st.frame >= 15 * 60 * 12 && dragoon_count >= 4) upgrade(singularity_charge);
			
			if (st.frame >= 15 * 60 * 9) {
				if (static_defence_pos != xy() && static_defence_pos_is_undefended) {
					build_n(photon_cannon, st.bases.size() * 2);
				}
				if (pylon_pos != xy() && count_production(st, pylon) == 0) build(pylon);
			
			}
			
			if (st.frame < 15 * 60 * 10 && army_supply < enemy_attacking_army_supply) {
				if (!st.units[cybernetics_core].empty()) {
					build(zealot);
					if (players::opponent_player->race != race_zerg) build(dragoon);
				} else build(zealot);
			}
			
			if (st.frame < 15 * 60 * 20 && (do_ffe || players::opponent_player->race != race_terran)) ffe();
		} else {
			build_n(dragoon, 8);
			build_n(stargate, 2);
			build_n(fleet_beacon, 1);
			upgrade(protoss_air_weapons_1);
			build_n(stargate, 1);
			
			build_n(probe, 28);
			
			build_n(gateway, 2);
			build_n(dragoon, 2);
			
			build_n(assimilator, 2);
			build_n(probe, 20);
			
			if (players::opponent_player->race == race_terran && !do_ffe) {
				build_n(nexus, 2);
				build_n(probe, 14);
			} else ffe();
		}
		
		if (st.minerals >= 400 && count_units_plus_production(st, pylon) < 4) build(pylon);
	}

};
