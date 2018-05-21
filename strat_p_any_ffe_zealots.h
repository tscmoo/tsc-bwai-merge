
struct strat_p_any_ffe_zealots : strat_mod {
	
	int total_probes_made = 0;
	
	double total_shield_battery_energy = 0.0;

	virtual void mod_init() override {
		
		use_default_early_rush_defence = false;

	}

	virtual void mod_tick() override {

		auto st = buildpred::get_my_current_state();
		if (st.frame < 15 * 60 * 7) combat::no_aggressive_groups = true;
		else combat::no_aggressive_groups = false;
		
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
		
		place_ffe = true;
		
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
		
		maxprod(zealot);
		if (army_supply >= 40.0 || players::opponent_player->race != race_zerg) maxprod(dragoon);
		
		if (current_frame < 15 * 60 * 10) {
			build_total(probe, 22);
			build_total(probe, 18);
			build_n(zealot, 2);
			build_n(photon_cannon, 1);
			build_total(probe, 15);
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
		}
		
		if (st.minerals >= 400 && count_units_plus_production(st, pylon) < 4) build(pylon);
		
	}

};
