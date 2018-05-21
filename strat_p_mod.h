

struct strat_p_mod : strat_p_base {

	virtual void init() override {

		sleep_time = 15;

		call_place_static_defence = false;
		default_upgrades = false;

	}

	xy natural_pos;
	xy natural_cc_build_pos;
	combat::choke_t natural_choke;
	int max_mineral_workers = 0;

	int gas_state = 0;

	xy pylon_pos;
	xy static_defence_pos;
	xy build_nydus_pos;
	bool has_nydus_without_exit = false;

	void mod_test_tick();
	void mod_test_build(buildpred::state& st);

	virtual bool tick() override {
		if (opening_state == 0) {
			if (my_resource_depots.size() != 1 || my_workers.size() != 4) opening_state = -1;
			else {
				build::add_build_total(0.0, unit_types::probe, 8);
				build::add_build_total(1.0, unit_types::pylon, 1);
				build::add_build_total(2.0, unit_types::probe, 10);
				//build::add_build_total(3.0, unit_types::gateway, 1);
				
				if (players::opponent_player->race == race_protoss || players::opponent_player->random) {
					build::add_build_total(3.0, unit_types::gateway, 1);
					build::add_build_total(4.0, unit_types::probe, 12);
					build::add_build_total(5.0, unit_types::assimilator, 1);
					build::add_build_total(6.0, unit_types::probe, 13);
					build::add_build_total(7.0, unit_types::zealot, 1);
					build::add_build_total(8.0, unit_types::probe, 14);
					build::add_build_total(9.0, unit_types::pylon, 2);
					build::add_build_total(10.0, unit_types::probe, 15);
					build::add_build_total(11.0, unit_types::zealot, 2);
					build::add_build_total(11.5, unit_types::cybernetics_core, 1);
					build::add_build_total(12.0, unit_types::gateway, 2);
					build::add_build_total(12.0, unit_types::dragoon, 1);
					build::add_build_total(13.0, unit_types::probe, 16);
					build::add_build_total(13.5, unit_types::pylon, 3);
				}
				
				++opening_state;
			}
		} else if (opening_state != -1) {
			if (bo_all_done()) {
				opening_state = -1;
			}
		}

		max_mineral_workers = get_max_mineral_workers();

		auto cur_st = buildpred::get_my_current_state();
		size_t bases = cur_st.bases.size();

		if (current_frame < 15 * 60) {
			auto s = get_next_base();
			for (auto& v : resource_spots::spots) {
				auto& bs = grid::get_build_square(v.cc_build_pos);
				if ((bs.building && bs.building->owner == players::my_player) || bs.reserved.first) {
					if (diag_distance(bs.pos - my_start_location) >= 32 * 10) {
						s = &v;
						break;
					}
				}
			}
			if (s) {
				natural_pos = s->pos;
				natural_cc_build_pos = s->cc_build_pos;
			}
			if (natural_pos != xy()) {
				natural_choke = combat::find_choke_from_to(unit_types::siege_tank_tank_mode, natural_pos, combat::op_closest_base, false, false, false, 32.0 * 10, 1, true);
			}
		}


		threats::eval(*this);


		if (current_used_total_supply < 140) {
			get_upgrades::set_no_auto_upgrades(true);
		}


		//log(log_level_test, "army supply %g, enemy army supply %g\n", army_supply, enemy_army_supply);

		auto get_static_defence_position_next_to = [&](unit* cc, unit_type* ut) {
			a_vector<xy> resources;
			for (unit* r : resource_units) {
				if (diag_distance(r->pos - cc->pos) > 32 * 12) continue;
				resources.push_back(r->pos);
			}
			if (resources.empty()) return xy();
			int n_near = 0;
			for (auto* u : my_units_of_type[ut]) {
				if (diag_distance(u->pos - cc->pos) <= 32 * 6) ++n_near;
			}
			if (n_near >= 3) return xy();
			a_vector<xy> potential_edges;
			for (auto i = resources.begin(); i != resources.end();) {
				xy r = *i;
				bool remove = false;
				int n_in_range = 0;
				for (auto* u : my_units_of_type[ut]) {
					if (diag_distance(u->pos - r) <= 32 * 7) {
						++n_in_range;
						if (n_in_range >= 2) {
							remove = true;
							break;
						}
					}
				}
				if (remove) i = resources.erase(i);
				else {
					if (n_in_range == 0) {
						potential_edges.push_back(r);
					}
					++i;
				}
			}
			if (resources.empty()) return xy();
			xy edge_a;
			xy edge_b;
			double max_d = 0.0;
			for (xy r : potential_edges) {
				for (xy r2 : resources) {
					if (r == r2) continue;
					double d = diag_distance(r - r2);
					if (d > max_d) {
						max_d = d;
						edge_a = r;
						edge_b = r2;
					}
				}
			}

			int ut_w = ut->tile_width;
			int ut_h = ut->tile_width;
			int cc_w = cc->type->tile_width;
			int cc_h = cc->type->tile_height;
			xy cc_build_pos = cc->building->build_pos;

			xy best;
			double best_score = std::numeric_limits<double>::infinity();

			auto eval = [&](xy pos) {
				auto& bs = grid::get_build_square(pos);
				if (!build::build_is_valid(bs, ut)) {
					game->drawBoxMap(pos.x, pos.y, pos.x + ut_w * 32, pos.y + ut_h * 32, BWAPI::Colors::Red);
					return;
				}
				xy cpos = pos + xy(ut_w * 16, ut_h * 16);
				double s = 0.0;
				if (edge_a != xy()) s += diag_distance(edge_a - cpos) * 1000.0;
				if (edge_b != xy()) s += diag_distance(edge_b - cpos) * 1000.0;
				for (xy r : resources) {
					double d = diag_distance(r - cpos);
					s += d*d;
				}
				if (s < best_score) {
					best_score = s;
					best = pos;
				}
			};

			for (int y = cc_build_pos.y - ut_h * 32; y <= cc_build_pos.y + cc_h * 32; y += 32) {
				if (y == cc_build_pos.y + 32 || y == cc_build_pos.y + 64) continue;
				eval(xy(cc_build_pos.x - ut_w * 32, y));
				eval(xy(cc_build_pos.x + cc_w * 32, y));
			}
			for (int x = cc_build_pos.x - ut_w * 32; x <= cc_build_pos.x + cc_w * 32; x += 32) {
				if (x == cc_build_pos.x + 32 || x == cc_build_pos.x + 64) continue;
				eval(xy(x, cc_build_pos.y - ut_h * 32));
				eval(xy(x, cc_build_pos.y + cc_h * 32));
			}

			game->drawBoxMap(best.x, best.y, best.x + ut_w * 32, best.y + ut_h * 32, BWAPI::Colors::Green);

			return best;
		};

		static_defence_pos = xy();
		if (bases >= 3) {
			for (auto& b : build::build_tasks) {
				if (b.built_unit || b.dead) continue;
				if (b.type->unit == unit_types::photon_cannon) {
					build::unset_build_pos(&b);
				}
			};
			//for (unit* u : my_resource_depots) {
			for (auto& v : cur_st.bases) {
				auto& bs = grid::get_build_square(v.s->cc_build_pos);
				if (bs.building && bs.building->owner == players::my_player) {
					xy pos = get_static_defence_position_next_to(bs.building, unit_types::photon_cannon);
					if (pos != xy()) {
						static_defence_pos = pos;
					}
				}
			}
		}
		pylon_pos = xy();
		if (bases >= 3) {
			for (auto& b : build::build_tasks) {
				if (b.built_unit || b.dead) continue;
				if (b.type->unit == unit_types::photon_cannon) {
					build::unset_build_pos(&b);
				}
			};
			for (auto& v : cur_st.bases) {
				auto& bs = grid::get_build_square(v.s->cc_build_pos);
				if (bs.building && bs.building->owner == players::my_player) {
					double d = get_best_score_value(my_units_of_type[unit_types::pylon], [&](unit* u) {
						return diag_distance(u->pos - v.s->pos);
					});
					if (d <= 32 * 6) continue;
					double d2 = get_best_score_value(build::build_tasks, [&](auto& b) {
						if (b.type->unit != unit_types::pylon) return std::numeric_limits<double>::infinity();
						return diag_distance(b.build_pos);
					});
					if (d2 <= 32 * 6) continue;
					xy pos = get_static_defence_position_next_to(bs.building, unit_types::pylon);
					if (pos != xy()) {
						pylon_pos = pos;
					}
				}
			}
		}

		mod_test_tick();

		return false;
	}
	
	void merge_high_templars() {
		a_vector<combat::combat_unit*> hts;
		for (auto* a : combat::live_combat_units) {
			if (a->u->type != unit_types::high_templar) continue;
			//if (current_frame - a->last_fight >= 15*30) continue;
			hts.push_back(a);
		}
		for (auto* a : hts) {
			if (current_frame <= a->u->controller->nospecial_until) continue;
			//if (current_frame - a->last_fight >= 15*30 && a->u->energy >= 60) continue;
			if (a->u->energy >= 50) continue;
			combat::combat_unit* n = nullptr;
			for (auto* a2 : hts) {
				if (current_frame <= a2->u->controller->nospecial_until) continue;
				if (a2 == a) continue;
				if (a2->u->energy >= 60) continue;
				if (diag_distance(a->u->pos - a2->u->pos) >= 32 * 8) continue;
				n = a2;
				break;
			}
			if (n) {
				auto tech = BWAPI::TechTypes::Archon_Warp;
				a->u->game_unit->useTech(tech, n->u->game_unit);
				a->u->controller->noorder_until = current_frame + 30;
				a->u->controller->nospecial_until = current_frame + 30;
				n->u->game_unit->useTech(tech, a->u->game_unit);
				n->u->controller->noorder_until = current_frame + 30;
				n->u->controller->nospecial_until = current_frame + 30;
			}
		}
	}

	buildpred::state* build_st;
	std::function<bool(buildpred::state&)> build_army;
	void build(unit_type* ut) {
		using namespace buildpred;
		auto& army = this->build_army;
		army = [army = std::move(army), ut](state& st) {
			return nodelay(st, ut, army);
		};
	}
	void maxprod(unit_type* ut) {
		using namespace buildpred;
		auto& army = this->build_army;
		army = [army = std::move(army), ut](state& st) {
			return buildpred::maxprod(st, ut, army);
		};
	}
	bool upgrade(upgrade_type* upg) {
		using namespace buildpred;
		if (!has_or_is_upgrading(*build_st, upg)) {
			auto& army = this->build_army;
			army = [army = std::move(army), upg](state& st) {
				return nodelay(st, upg, army);
			};
			return false;
		}
		return true;
	}
	bool build_n(unit_type* ut, int n) {
		using namespace buildpred;
		if (count_units_plus_production(*build_st, ut) < n) {
			auto& army = this->build_army;
			army = [army = std::move(army), ut](state& st) {
				return nodelay(st, ut, army);
			};
			return false;
		}
		return true;
	}

	bool upgrade_wait(upgrade_type* upg) {
		using namespace buildpred;
		if (!has_or_is_upgrading(*build_st, upg)) {
			auto& army = this->build_army;
			army = [army = std::move(army), upg](state & st)
			{
				return nodelay(st, upg, army);
			};
			return false;
		}
		return has_upgrade(*build_st, upg);
	}

	virtual bool build(buildpred::state&st) override {
		using namespace buildpred;
		if (opening_state != -1) return false;

		build_army = army;
		build_st = &st;

		mod_test_build(st);

		return build_army(st);
	}
	
	bool forge_fast_expand = false;

	virtual void post_build() override {

		if (output_enabled) {
			for (auto* bs : natural_choke.build_squares) {
				game->drawBoxMap(bs->pos.x, bs->pos.y, bs->pos.x + 32, bs->pos.y + 32, BWAPI::Colors::Orange, true);
			}
		}

		if (natural_pos != xy()) {
			combat::control_spot* cs = get_best_score(combat::active_control_spots, [&](combat::control_spot* cs) {
				return diag_distance(cs->pos - natural_pos);
			});
			if (cs) {
				bool place_pylon = false;
				if (forge_fast_expand) {
					size_t n_pylons = my_units_of_type[unit_types::pylon].size();
					if (n_pylons == 0 || n_pylons == 2) place_pylon = true;
				}
				size_t iterations = 0;
				for (auto&b : build::build_tasks) {
					if (b.built_unit || b.dead) continue;
					if (b.type->unit == unit_types::photon_cannon && static_defence_pos != xy()) {
						//build::unset_build_pos(&b);
						//build::set_build_pos(&b, static_defence_pos);
						if (!build::set_build_pos(&b, static_defence_pos)) b.dead = true;
						continue;
					}
					if (b.type->unit == unit_types::pylon && pylon_pos != xy()) {
						//build::unset_build_pos(&b);
						build::set_build_pos(&b, pylon_pos);
						continue;
					}
					if (b.type->unit == unit_types::photon_cannon || (place_pylon && b.type->unit == unit_types::pylon) || (forge_fast_expand && b.type->unit == unit_types::forge)) {
						xy pos = b.build_pos;
						build::unset_build_pos(&b);

						a_vector<std::tuple<xy, double>> scores;

						auto pred = [&](grid::build_square&bs) {
							if (++iterations % 1000 == 0) multitasking::yield_point();
	
							for (unit* u : enemy_units) {
								if (u->gone) continue;
								if (u->stats->ground_weapon && diag_distance(u->pos - bs.pos) <= u->stats->ground_weapon->max_range + 64) {
									return false;
								}
							}
							if (b.type->unit->requires_creep && !build::build_has_existing_creep(bs, b.type->unit)) return false;
							return build::build_full_check(bs, b.type->unit);
						};
						auto score = [&](xy pos) {
							double r = 0.0;
							auto& bs = grid::get_build_square(pos);
							bool is_inside = test_pred(natural_choke.inside_squares, [&](auto* nbs) {
								return nbs == &bs;
							});
							if (!natural_choke.inside_squares.empty() && !is_inside) r += (32 * 12) * (32 * 12);
							xy center_pos = pos + xy(b.type->unit->tile_width * 16, b.type->unit->tile_height * 16);
							for (auto* nbs : natural_choke.build_squares) {
								double d = diag_distance(center_pos - nbs->pos);
								r += d*d;
							}
							return r;
						};

						std::array<xy, 1> starts;
						//starts[0] = threats::nearest_attack_pos != xy() ? threats::nearest_attack_pos : natural_pos;
						starts[0] = natural_pos;
						pos = build_spot_finder::find_best(starts, 128, pred, score);

						if (pos != xy()) build::set_build_pos(&b, pos);
						else {
							b.dead = true;
						}
					}
				}
			}
		}

	}

};

#include "strat_p_mod_test.h"
