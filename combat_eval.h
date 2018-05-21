
namespace combat_eval {
	;

	struct combatant {
		unit_stats*st;
		double move;
		double energy;
		double shields;
		double hp;
		int cooldown;
		combatant*target = nullptr;
		int loaded_until;
		bool force_target;
		int stim_pack_timer = 0;
		int heal_frame = 0;
		int spider_mine_count = 0;
		int irradiate_timer = 0;
		int target_count = 0;
		double next_damage_override = 0.0;
		bool ensnared = false;
	};

	double get_damage_type_modifier(int damage_type, int target_size) {
		if (damage_type == weapon_stats::damage_type_concussive) {
			if (target_size == unit_type::size_small) return 1.0;
			if (target_size == unit_type::size_medium) return 0.5;
			if (target_size == unit_type::size_large) return 0.25;
		}
		if (damage_type == weapon_stats::damage_type_normal) return 1.0;
		if (damage_type == weapon_stats::damage_type_explosive) {
			if (target_size == unit_type::size_small) return 0.5;
			if (target_size == unit_type::size_medium) return 0.75;
			if (target_size == unit_type::size_large) return 1.0;
		}
		return 1.0;
	}

	struct eval {

		struct team_t {
			a_vector<combatant> units;
			double start_supply = 0;
			double end_supply = 0;
			double damage_dealt = 0;
			double damage_taken = 0;
			double score = 0;
			bool attacked = false;
			bool has_stim = false;
			bool has_spider_mines = false;
			weapon_stats*spider_mine_weapon;
			int bunker_count = 0;
			bool has_static_defence = false;
			bool run = false;
			bool wait_until_attacked = false;
			double start_hp = 0.0;
			double end_hp = 0.0;
			double shield_battery_energy = 0.0;
		};
		std::array<team_t, 2> teams;
		int total_frames = 0;
		int max_frames = 0;

		eval() {
			max_frames = 15 * 20;
			teams[0].has_stim = players::my_player->has_upgrade(upgrade_types::stim_packs);
			teams[1].has_stim = players::opponent_player->has_upgrade(upgrade_types::stim_packs);
			teams[1].has_stim = false;
			teams[0].has_spider_mines = players::my_player->has_upgrade(upgrade_types::spider_mines);
			teams[1].has_spider_mines = players::opponent_player->has_upgrade(upgrade_types::spider_mines);
			teams[0].spider_mine_weapon = units::get_weapon_stats(BWAPI::WeaponTypes::Spider_Mines, players::my_player);
			teams[1].spider_mine_weapon = units::get_weapon_stats(BWAPI::WeaponTypes::Spider_Mines, players::opponent_player);
		}

		combatant&add_unit(unit_stats*st, int team) {
			auto&vec = teams[team].units;
			vec.emplace_back();
			auto&c = vec.back();
			c.st = st;
			c.move = 32 * 4;
			c.move += st->ground_weapon ? st->ground_weapon->max_range : st->air_weapon ? st->air_weapon->max_range : 0;
			c.energy = st->energy;
			c.shields = st->shields;
			c.hp = st->hp;
			if (st->type == unit_types::bunker) c.hp *= 2;
			c.cooldown = 0;
			c.loaded_until = 0;
			c.force_target = false;
			if (st->type == unit_types::vulture) c.spider_mine_count = 3;
			return c;
		}
		combatant&add_unit(unit*u, int team) {
			auto&c = add_unit(u->stats, team);
			set_unit_stuff(c, u);
			return c;
		}
		void set_unit_stuff(combatant&c, unit*u) {
			c.energy = u->energy;
			c.shields = u->shields;
			c.hp = u->hp;
			c.cooldown = u->weapon_cooldown;
			c.stim_pack_timer = u->game_unit->getStimTimer();
			//c.stim_pack_timer = 0;
			c.spider_mine_count = u->spider_mine_count;
			if (u->lockdown_timer) c.cooldown += u->lockdown_timer;
			if (u->maelstrom_timer) c.cooldown += u->maelstrom_timer;
			if (u->defensive_matrix_hp) c.hp += u->defensive_matrix_hp;
			if (u->irradiate_timer) c.irradiate_timer = u->irradiate_timer;
			if (u->type == unit_types::marine) {
				c.ensnared = u->game_unit->isEnsnared();
			}
			if (u->type == unit_types::lurker && (!u->burrowed || u->game_order == BWAPI::Orders::Burrowing) && u->owner != players::my_player) c.cooldown = 15 * 10;
		}

		void run() {
			total_frames = 0;
			for (int i = 0; i < 2; ++i) {
				team_t&t = teams[i];
				std::sort(t.units.begin(), t.units.end(), [&](const combatant&a, const combatant&b) {
					double ar = a.st->ground_weapon ? a.st->ground_weapon->max_range : a.st->air_weapon ? a.st->air_weapon->max_range : 0;
					double br = b.st->ground_weapon ? b.st->ground_weapon->max_range : b.st->air_weapon ? b.st->air_weapon->max_range : 0;
					double am = (a.move - ar) / a.st->max_speed;
					double bm = (b.move - br) / b.st->max_speed;
					if (std::abs(am - bm) <= 15 * 2) return a.hp < b.hp;
					return am < bm;
				});
				//for (auto&v : t.units) {
				//	log(log_level_info, "team %d - %s with %g hp\n", i, v.st->type->name, v.hp);
				//}
				t.start_supply = 0;
				t.start_hp = 0.0;
				int static_defence_count = 0;
				for (auto&v : t.units) {
					if (v.hp <= 0) continue;
					t.start_supply += v.st->type->required_supply;
					t.start_hp += v.shields + v.hp;
					if (v.st->type == unit_types::bunker) ++t.bunker_count;
					if (v.st->type == unit_types::bunker || v.st->type == unit_types::photon_cannon || v.st->type == unit_types::sunken_colony) ++static_defence_count;
					if (v.st->type == unit_types::bunker || v.st->type == unit_types::photon_cannon || v.st->type == unit_types::sunken_colony) t.start_supply += 2;
				}
				t.has_static_defence = static_defence_count >= 2;
			}
			while (true) {
				//int frame_resolution = 8;
				int frame_resolution = 1;
				total_frames += frame_resolution;
				if (max_frames && total_frames >= max_frames) break;
				int target_count = 0;
				for (int i = 0; i < 2; ++i) {
					for (auto&c : teams[i].units) {
						c.target_count = 0;
					}
				}
				for (int i = 0; i < 2; ++i) {
					team_t&my_team = teams[i];
					team_t&enemy_team = teams[i ^ 1];
					double max_width = 32 * 4;
					double acc_width = 0.0;
					for (auto&c : my_team.units) {
						if (c.st->type == unit_types::spider_mine) continue;
						if (c.hp <= 0) continue;
						if (c.st->type == unit_types::scarab) continue;
						//if (acc_width >= max_width) continue;
						if (c.energy < c.st->energy) c.energy += 8.0 / 256 * frame_resolution;
						if (c.st->type->race == race_zerg && c.hp < c.st->hp) c.hp += 4.0 / 256 * frame_resolution;
						//if ((c.st->type->is_worker || c.st->type == unit_types::zergling) && acc_width >= max_width) continue;
						if (c.loaded_until > total_frames) {
							++target_count;
							continue;
						}
						if (!c.st->type->is_flyer) acc_width += c.st->type->width;
						bool can_target = true;
						if (c.st->type == unit_types::medic) {
							can_target = false;
							if (c.energy > 100.0 / 256) {
								for (auto&ac : my_team.units) {
									if (ac.hp <= 0) continue;
									if (ac.st->type->is_biological) {
										if (ac.heal_frame == total_frames) continue;
										if (ac.hp >= ac.st->hp) continue;
										ac.heal_frame = total_frames;
										ac.hp += 200.0 / 256 * frame_resolution;
										my_team.damage_taken -= 200.0 / 256 * frame_resolution;
										c.energy -= 100.0 / 256 * frame_resolution;
										c.move = ac.move;
										break;
									}
								}
							}
						}
						combatant*target = c.target;
						if (can_target && (!target || target->hp <= 0)) {
							target = nullptr;
							for (int i = 0; i < 2 && !target; ++i) {
								for (auto&ec : enemy_team.units) {
									if (ec.hp <= 0) continue;
									if (i == 0) {
										if (!ec.st->air_weapon && !ec.st->ground_weapon) continue;
										if (c.force_target && ec.st->type->is_flyer) continue;
										weapon_stats*ew = c.st->type->is_flyer ? ec.st->air_weapon : ec.st->ground_weapon;
										if (!ew) continue;
										if (ec.cooldown >= 15 * 5) continue;
									}
									bool inc_target_count = false;
									if (!c.force_target && (!c.irradiate_timer || !ec.st->type->is_biological)) {
										weapon_stats*w = ec.st->type->is_flyer ? c.st->air_weapon : c.st->ground_weapon;
										if (!w) continue;
// 										if (w->max_range <= 32 && i == 0) {
// 											if (ec.target_count >= 3) continue;
// 										}
										if (w->max_range <= 32) {
											if (ec.target_count >= 3) continue;
											inc_target_count = true;
										}
									}
									if (ec.loaded_until > total_frames) continue;
									if (enemy_team.bunker_count && ec.st->type == unit_types::marine) continue;
									//++ec.target_count;
									if (inc_target_count) ++ec.target_count;
									target = &ec;
									break;
								}
							}
							c.target = target;
						}
						if (c.cooldown > 0) c.cooldown -= frame_resolution;
						if (c.irradiate_timer > 0) c.irradiate_timer -= frame_resolution;
						//if (c.st->type == unit_types::dropship) log("frame %d: %s: target is %p (force_target %d)\n", total_frames, c.st->type->name, target, c.force_target);
						if (my_team.run) {
							c.move += c.st->max_speed * frame_resolution;
						} else if (target) {

							double speed = c.st->max_speed;
							if (c.st->type == unit_types::siege_tank_siege_mode) speed = 2;
							if (c.st->type == unit_types::interceptor && c.move <= 0) speed /= 4;

							weapon_stats*w = target->st->type->is_flyer ? c.st->air_weapon : c.st->ground_weapon;
							bool use_spider_mine = c.spider_mine_count && my_team.has_spider_mines && !target->st->type->is_hovering && !target->st->type->is_flyer && !target->st->type->is_building && target->st->type != unit_types::lurker;
							if (use_spider_mine && target->st->type == unit_types::lurker) use_spider_mine = false;
							if (use_spider_mine && enemy_team.units.size() >= 8) use_spider_mine = false;
							if (use_spider_mine && target->st->type == unit_types::dragoon) use_spider_mine = false;
							if (!w && !c.irradiate_timer) {
								if (c.st->max_speed > 0 && c.move > 0) {
									++target_count;
									if (my_team.has_static_defence) c.move -= speed * frame_resolution / 4;
									else c.move -= speed * frame_resolution;
									my_team.score += speed / 1000.0 * frame_resolution;
								}
							} else if (c.move + target->move > (use_spider_mine ? 0 : (w ? w->max_range : 32 * 2))) {
								if (speed > 0) {
									++target_count;
// 									if (my_team.has_static_defence) c.move -= speed * frame_resolution / 4;
// 									else c.move -= speed * frame_resolution;
									if (my_team.wait_until_attacked && !my_team.attacked) c.move -= speed * frame_resolution / 100.0;
									else c.move -= speed * frame_resolution;
									my_team.score += speed / 1000.0 * frame_resolution;
								}
							} else {
								++target_count;
								if (c.cooldown <= 0) {
									int hits = w == c.st->ground_weapon ? c.st->ground_weapon_hits : c.st->air_weapon_hits;
									auto attack = [&](combatant*target, double damage_mult) {
										double damage;
										if (w) {
											damage = w->damage;
											if (target->shields <= 0) damage *= get_damage_type_modifier(w->damage_type, target->st->type->size);
											damage -= target->st->armor;
											if (damage <= 0) damage = 1.0;
											damage *= hits;
										} else {
											damage = 120.0 / 256.0; // fixme: use the correct irradiate damage
											damage *= frame_resolution;
										}
										damage *= damage_mult;
										if (c.next_damage_override) {
											damage = c.next_damage_override;
											c.next_damage_override = 0.0;
										}
										//if (target->move + c.move < (w ? w->min_range : 0.0)) damage /= 2;
										if (enemy_team.shield_battery_energy && target->st->shields) {
											double heal = std::min(enemy_team.shield_battery_energy * 2.0, damage);
											damage -= heal;
											enemy_team.shield_battery_energy -= heal / 2.0;
											//log(log_level_info, "%s dealt %g damage to %s after healing %g by shield battery\n", c.st->type->name, damage, target->st->type->name, heal);
										}
										if (target->shields > 0) {
											target->shields -= damage;
											if (target->shields < 0) {
												target->hp += target->shields;
												target->shields = 0.0;
											}
										} else target->hp -= damage;
										if (c.st->type == unit_types::mutalisk) damage /= 2;
										double damage_dealt = target->hp < 0 ? damage + target->hp : damage;
										//log(log_level_info, "%s dealt %g damage to %s (%g hp left)\n", c.st->type->name, damage_dealt, target->st->type->name, target->hp);
										my_team.damage_dealt += damage_dealt;
										enemy_team.damage_taken += damage_dealt;
										if (!enemy_team.attacked) enemy_team.attacked = true;
										int cooldown = w ? w->cooldown : frame_resolution;
										if (damage_dealt < damage) {
											cooldown = 1;
											c.next_damage_override = damage - damage_dealt;
										}
										if (c.st->type == unit_types::interceptor) cooldown = i == 0 ? 30 : 60;
										if (c.st->type == unit_types::marine) {
											if (my_team.has_stim && c.stim_pack_timer == 0 && c.hp > 10) {
												c.stim_pack_timer = 220;
												c.hp -= 10;
												my_team.damage_taken += 10;
											}
										}
										if (c.stim_pack_timer) cooldown /= 2;
										if (c.ensnared) cooldown *= 2;
										c.cooldown += cooldown;
//										my_team.score += damage_dealt / 100;
//										if (target->hp <= 0) {
//											//log(log_level_info, "%s killed\n", target->st->type->name);
//											double value = target->st->type->total_minerals_cost + target->st->type->total_gas_cost;
//											my_team.score += value;
//											if (target->st->type->is_worker) my_team.score += value;
//											if (target->st->type == unit_types::bunker) --enemy_team.bunker_count;
//										}
										if (true) {
											double value = target->st->type->total_minerals_cost + target->st->type->total_gas_cost;
											my_team.score += value * (damage / (target->st->shields + target->st->hp));
										}
										if (target->hp <= 0) {
											if (target->st->type == unit_types::bunker) --enemy_team.bunker_count;
										}
									};
									if (c.st->type == unit_types::siege_tank_siege_mode || c.st->type == unit_types::valkyrie || c.st->type == unit_types::firebat || c.st->type == unit_types::lurker) {
										attack(target, 1.0);
										c.cooldown = c.cooldown - (c.cooldown + 3) / 4;
										//if (c.st->type == unit_types::siege_tank_siege_mode && target->st->hp <= 100.0) c.cooldown -= c.cooldown / 2;
										if (c.st->type == unit_types::siege_tank_siege_mode) c.cooldown -= c.cooldown / 2;
									//	if (c.st->type == unit_types::lurker && target->st->hp <= 50.0) c.cooldown -= c.cooldown / 2;
										//if (c.st->type == unit_types::siege_tank_siege_mode && target->st->type == unit_types::zealot) c.cooldown *= 2;
									} else if (use_spider_mine) {
										w = my_team.spider_mine_weapon;
										hits = 1;
										attack(target, 0.5);
										--c.spider_mine_count;
									} else if (c.st->type == unit_types::reaver) {
										attack(target, 0.01);
									} else attack(target, 1.0);
// 									if (c.st->type == unit_types::siege_tank_siege_mode || c.st->type == unit_types::valkyrie || c.st->type == unit_types::firebat || c.st->type == unit_types::lurker || c.st->type == unit_types::firebat) {
// 										combatant*ntarget = target + 1;
// 										int max_n = 1;
// 										if (c.st->type == unit_types::valkyrie) max_n = 3;
// 										if (c.st->type == unit_types::lurker) max_n = 3;
// 										if (c.st->type == unit_types::firebat) {
// 											if (target->st->ground_weapon && target->st->ground_weapon->max_range > 64) max_n = 0;
// 											else max_n = 2;
// 										}
// 										for (int i = 0; i < max_n; ++i) {
// 											if (ntarget < enemy_team.units.data() + enemy_team.units.size()) {
// 												weapon_stats*nw = ntarget->st->type->is_flyer ? c.st->air_weapon : c.st->ground_weapon;
// 												if (nw == w && c.move + ntarget->move < w->max_range && ntarget->hp > 0) {
// 													attack(ntarget, 1.0);
// 												}
// 											}
// 											++ntarget;
// 										}
// 									}
								} else {
									if (c.st->type == unit_types::dragoon && (target->st->type == unit_types::marine || target->st->type == unit_types::firebat)) {
										if (speed > 0 && c.move < 32 * 4) {
											c.move += speed * frame_resolution;
										}
									}
// 									if (c.st->type == unit_types::mutalisk) {
// 										if (speed > 0 && (target->st->type == unit_types::archon)) {
// 											c.move += speed * frame_resolution;
// 										}
// 									}
// 									if (c.st->type == unit_types::marine) {
// 										if (speed > 0 && (target->st->type == unit_types::zergling)) {
// 											c.move += speed * frame_resolution / 2;
// 										}
// 									}
								}
							}
						}
					}
				}
				if (target_count == 0) break;
			}
			for (auto&t : teams) {
				t.end_supply = 0;
				t.end_hp = 0.0;
				for (auto&v : t.units) {
					if (v.hp <= 0) continue;
					t.end_hp += v.shields + v.hp;
					t.end_supply += v.st->type->required_supply;
					if (v.st->type == unit_types::bunker || v.st->type == unit_types::photon_cannon || v.st->type == unit_types::sunken_colony) t.end_supply += 2;
				}
			}

		}

	};


}
