

#include "nn2.h"

struct strat_t_new_nn2 : strat_t_base {

	nn2::nn_input current_nn_input;
	nn2::nn_output current_nn_output;
	nn2::nn_actions current_nn_actions;
	int last_query_nn = 0;

	static void static_train() {
		nn2::init(true);

		nn2::nn_output state;

		state.hidden_state_output.resize(nn2::network.layers);
		state.cell_state_output.resize(nn2::network.layers);
		for (size_t i = 0; i < nn2::network.layers; ++i) {
			state.hidden_state_output[i].clear();
			state.cell_state_output[i].clear();
			state.hidden_state_output[i].resize(nn2::network.state_size);
			state.cell_state_output[i].resize(nn2::network.state_size);
		}

// 		auto jv_state = strategy::read_json("tsc-bwai-nn2-state.txt");
// 		if (jv_state) nn2::jv_to(jv_state, state);
// 
// 		auto jv_weights = strategy::read_json("tsc-bwai-nn2-weights.txt");
// 		if (jv_weights) nn2::jv_to(jv_weights, nn2::weights);

		auto state_data = strategy::read_file("tsc-bwai-nn2-state");
		if (!state_data.empty()) {
			nn2::data_reader<> r((uint8_t*)state_data.data(), (uint8_t*)state_data.data() + state_data.size());
			get(r, state.output);
			get(r, state.hidden_state_output);
			get(r, state.cell_state_output);
		}
		auto weights_data = strategy::read_file("tsc-bwai-nn2-weights");
		if (!weights_data.empty()) {
			nn2::data_reader<> r((uint8_t*)weights_data.data(), (uint8_t*)weights_data.data() + weights_data.size());
			get(r, nn2::weights);
			get(r, nn2::input_mean);
			get(r, nn2::input_stddev);
		}

		nn2::load_history("");

		nn2::train(state);

	}

	//bool opponent_is_random = false;
	virtual void init() override {

		sleep_time = 15;

		nn2::init();

		current_nn_input.hidden_state_input.resize(nn2::network.layers);
		current_nn_input.cell_state_input.resize(nn2::network.layers);
		for (size_t i = 0; i < nn2::network.layers; ++i) {
			current_nn_input.hidden_state_input[i].clear();
			current_nn_input.cell_state_input[i].clear();
			current_nn_input.hidden_state_input[i].resize(nn2::network.state_size);
			current_nn_input.cell_state_input[i].resize(nn2::network.state_size);
		}
		current_nn_output.hidden_state_output = current_nn_input.hidden_state_input;
		current_nn_output.cell_state_output = current_nn_input.cell_state_input;

		//auto init_weights = std::move(nn2::weights);
		//nn2::weights.clear();
		current_nn_output.hidden_state_output.clear();
		nn2::load(current_nn_output);

		if (current_nn_output.hidden_state_output.empty()) {
// #include "strat_t_new_nn2_state.h"
// 			nn2::jv_to(json_parse(str), current_nn_output);
#ifdef _WIN32
			auto h = FindResourceA(nullptr, "tsc-bwai-nn2-state", "bindata");
			if (!h) xcept("failed to find resource");
			auto h2 = LoadResource(nullptr, h);
			if (!h2) xcept("failed to load resource");
			const void* data = LockResource(h2);
			size_t len = SizeofResource(nullptr, h);
			nn2::data_reader<> r((uint8_t*)data, (uint8_t*)data + len);
			get(r, current_nn_output.output);
			get(r, current_nn_output.hidden_state_output);
			get(r, current_nn_output.cell_state_output);
#else
			xcept("strat_t_new_nn2 win only fixme");
#endif
		}

		if (true) {
// #include "strat_t_new_nn2_weights.h"
// 			nn2::jv_to(json_parse(str), nn2::weights);
#ifdef _WIN32
			auto h = FindResourceA(nullptr, "tsc-bwai-nn2-weights", "bindata");
			if (!h) xcept("failed to find resource");
			auto h2 = LoadResource(nullptr, h);
			if (!h2) xcept("failed to load resource");
			const void* data = LockResource(h2);
			size_t len = SizeofResource(nullptr, h);
			nn2::data_reader<> r((uint8_t*)data, (uint8_t*)data + len);
			get(r, nn2::weights);
			get(r, nn2::input_mean);
			get(r, nn2::input_stddev);
#else
			xcept("strat_t_new_nn2 win only fixme");
#endif
		}

		double sum = 0.0;
		for (auto& v : nn2::weights) {
			sum += v;
		}
		log(log_level_test, " sum of all weights: %g\n", sum);

		strategy::on_end_funcs.push_back([&](bool won) {

			if (won) {
				if (enemy_army_supply > army_supply) won = false;
			}

			nn2::save(won, current_nn_output);

		});

		combat::defensive_spider_mine_count = 24;
		//opponent_is_random = players::opponent_player->random;

		if (output_enabled) {
			multitasking::spawn(0.1, [&]() {

				while (true) {
					multitasking::sleep(1);

					int x = 292;
					int y = 32;
					int size = 4;
					auto draw_state = [&](auto& outputs) {
						a_vector<nn2::value_t> log_values;
						log_values.reserve(outputs.size());
						nn2::value_t max_pos_value = 0.0;
						nn2::value_t max_neg_value = 0.0;
						for (auto& v : outputs) {
							nn2::value_t val = std::log(v);
							if (val > 0 && val > max_pos_value) max_pos_value = val;
							if (val < 0 && -val > max_neg_value) max_neg_value = -val;
							log_values.push_back(val);
						}

						for (auto& v : log_values) {

							double strength = v < 0 ? -v / max_neg_value : v / max_pos_value;
							int color = (int)std::round(strength * 255);

							BWAPI::Color c(v > 0 ? color : 0, 32, v < 0 ? color : 0);

							game->drawBoxScreen(x, y, x + size, y + size, c, true);

							x += size + 1;
							if (x >= 592) {
								x = 292;
								y += size + 2;
							}
						}
						x += size + 1;
						if (x >= 592) {
							x = 292;
							y += size + 2;
						}

					};

					y += 2;

					for (auto& v : current_nn_output.hidden_state_output) draw_state(v);
					for (auto& v : current_nn_output.cell_state_output) draw_state(v);

					int n = 0;
					auto add = [&](const char* text) {
						game->drawTextScreen(300, y + n * 12, "%s", text);
						++n;
					};

#define x(x) if (current_nn_actions.x) add(#x);
					x(prioritize_workers);
					x(two_rax_marines);
					x(attack);
					x(use_control_spots);
					x(aggressive_vultures);
					x(aggressive_wraiths);
					x(fast_expand);
					x(two_fact_vultures);
					x(one_fact_tanks);
					x(two_fact_goliaths);
					x(make_wraiths);
					x(two_starports);
					x(make_bunker);
					x(make_marines);
					x(army_comp_tank_vulture);
					x(army_comp_tank_goliath);
					x(army_comp_goliath_vulture);
					x(support_science_vessels);
					x(support_wraiths);
					x(research_siege_mode);
					x(research_vulture_upgrades);
#undef x
				}

			}, "nn2 actions output");
		}

	}

	xy get_early_bunker_pos() {
		auto pred = [&](grid::build_square&bs) {
			return build::build_is_valid(bs, unit_types::bunker);
		};
		xy cc_pos = my_start_location + xy(4 * 16, 3 * 16);
		xy barracks_pos = my_units_of_type[unit_types::barracks].empty() ? my_start_location : my_units_of_type[unit_types::barracks].front()->pos;
		auto score = [&](xy pos) {
			double r = 0.0;
			double cc_d = diag_distance(pos - cc_pos);
			double rax_d = diag_distance(pos - barracks_pos);
			r += cc_d*cc_d + rax_d*rax_d;
			return r;
		};

		std::array<xy, 1> starts;
		starts[0] = my_start_location;
		return build_spot_finder::find_best(starts, 64, pred, score);
	};

	int max_mineral_workers = 0;
	xy natural_pos;
	xy natural_cc_build_pos;
	combat::choke_t natural_choke;
	xy turret_cc_defence_pos;

	bool being_pool_rushed = false;
	bool built_rax_before_depot = false;
	bool build_early_bunker = false;

	virtual bool tick() override {
		if (opening_state == 0) {
			if (my_resource_depots.size() != 1 || my_workers.size() != 4) opening_state = -1;
			else {
				build::add_build_total(0.0, unit_types::scv, 9);
				if (players::opponent_player->race != race_zerg && !players::opponent_player->random) build::add_build_task(1.0, unit_types::supply_depot);

				++opening_state;
			}
		} else if (opening_state == 1) {
			if (bo_all_done()) {
				opening_state = -1;
			}
		}

		multitasking::yield_point();

		if (last_query_nn == 0 || current_frame - last_query_nn >= 15 * 30) {
			last_query_nn = current_frame;
			current_nn_input.hidden_state_input = current_nn_output.hidden_state_output;
			current_nn_input.cell_state_input = current_nn_output.cell_state_output;
			nn2::get_state(current_nn_input);
			current_nn_actions = nn2::forward(current_nn_input, current_nn_output);

			//static bool override = tsc::rng(2) == 0;
			//static bool override = false;

// 			static int n = tsc::rng(4);
// 			static bool go_mech_vs_z = n == 0;
// 			static bool go_bio_vs_z = n == 1;
// 			static bool go_4pool_def = n == 2;

			//static int n = tsc::rng(5);
// 			static int n = 2 + tsc::rng(3);
// 			static bool go_cc = n == 0;
// 			static bool go_vult = n == 1;
// 			static bool go_2_port_wraith = n == 2;
// 			static bool go_goliaths = n == 3;

			//static int n = tsc::rng(2);
			static bool go_cc = false;
			//static bool go_vult = tsc::rng(1.0) < 0.5;
			static bool go_vult = false;
			static bool go_2_port_wraith = false;
			static bool go_goliaths = false;

			static bool go_mech_vs_z = false;
			static bool go_bio_vs_z = false;
			static bool go_4pool_def = false;

			static bool go_bio_tank_push = false;

			static bool go_anti_lurker = false;

			static bool go_1_rax_fe = false;

			static bool go_anti_ualbertabot = false;

			static bool go_vult_vs_p = false;

			static bool exec_goliath_frame = false;
			static int goliath_frame = 15 * 60 * 6 + tsc::rng(15 * 60 * 10);

			if (true) {
				auto& r = current_nn_actions;

				if (exec_goliath_frame && current_frame >= goliath_frame) {
					r.two_fact_goliaths = true;
					r.two_fact_vultures = false;
					r.one_fact_tanks = false;
				}

// 				static bool test = rng(1.0) < 0.5;
// 				if (test && current_frame >= 15 * 60 * 10) {
// 					r.attack = true;
// 					r.support_science_vessels = true;
// 				}

				auto anti_lurker = [&]() {
					int cc_count = my_resource_depots.size();
					//r.prioritize_workers = (scv_count < 30 || army_supply >= 28.0) && (scv_count < 50 || army_supply >= 50.0);
					double desired_army_supply = std::max(scv_count*scv_count* 0.015 + scv_count * 0.8 - 16, 4.0);
					r.prioritize_workers = scv_count < 40 || army_supply >= desired_army_supply;
					//r.prioritize_workers = scv_count < 60 || army_supply >= desired_army_supply;
					//r.prioritize_workers = scv_count < 40 || (army_supply >= 65.0 && scv_count < army_supply);
					//r.prioritize_workers = army_supply >= enemy_army_supply;
					//r.two_rax_marines = (enemy_hydralisk_count >= 10 || enemy_air_army_supply >= 10) && tank_count >= enemy_lurker_count;
					r.two_rax_marines = false;
					//r.attack = army_supply >= enemy_army_supply + 8.0 && (enemy_static_defence_count < 6 || army_supply >= 30.0);
					//r.attack = army_supply >= 60 && army_supply > enemy_army_supply;
					//r.attack = false;
					r.attack = (army_supply >= 90 || (tank_count >= 8 && army_supply >= 60)) && army_supply > enemy_army_supply;
					r.use_control_spots = false;
					r.aggressive_vultures = true;
					r.aggressive_wraiths = true;
					r.fast_expand = !my_units_of_type[unit_types::factory].empty();
					r.two_fact_vultures = vulture_count < 8;
					bool make_wraiths = enemy_goliath_count + enemy_static_defence_count + enemy_dragoon_count + enemy_hydralisk_count / 2 < 6;
					make_wraiths = false;
					if (tank_count >= 8 && wraith_count < tank_count) make_wraiths = true;
					r.one_fact_tanks = current_frame < 15 * 60 * 15;
					r.two_fact_goliaths = false;
					r.make_wraiths = make_wraiths;
					r.two_starports = false;
					r.make_bunker = army_supply < 20.0 && cc_count >= 2 && marine_count >= 3;
					r.make_marines = scv_count < 20 && marine_count < 4;
					r.army_comp_tank_vulture = true;
					r.army_comp_tank_goliath = false;
					r.army_comp_goliath_vulture = false;
					r.support_science_vessels = scv_count >= 40 || (tank_count >= 4 && enemy_lurker_count);
					r.support_wraiths = scv_count >= 70 && wraith_count < 4 && army_supply > enemy_army_supply;
					r.research_siege_mode = tank_count > 0;
					r.research_vulture_upgrades = vulture_count > 0;
				};

				auto bio_vs_z = [&]() {

// 					double desired_army_supply = std::max(scv_count*scv_count* 0.015 + scv_count * 0.8 - 16, 4.0);
// 					//r.prioritize_workers = scv_count < 20 || army_supply >= desired_army_supply;
// 					r.prioritize_workers = scv_count < 40 || army_supply >= desired_army_supply || current_minerals >= 400;
// 					r.two_rax_marines = current_frame >= 15 * 60 * 10 && tank_count > enemy_lurker_count * 2 && tank_count + enemy_mutalisk_count >= marine_count / 6;
// 					//r.attack = army_supply >= 36.0 && army_supply >= enemy_army_supply + 16.0;
// 					//r.attack = false;
// 					r.attack = (army_supply >= 90 || (tank_count >= 8 && army_supply >= 60)) && army_supply > enemy_army_supply;
// 					r.use_control_spots = false;
// 					r.aggressive_vultures = true;
// 					r.aggressive_wraiths = true;
// 					r.fast_expand = true;
// 					r.two_fact_vultures = vulture_count < 2;
// 					r.one_fact_tanks = current_frame < 15 * 60 * 15;
// 					r.two_fact_goliaths = false;
// 					r.make_wraiths = tank_count >= 8 && wraith_count < tank_count;
// 					r.two_starports = false;
// 					r.make_bunker = current_frame < 15 * 60 * 10 && my_resource_depots.size() >= 2 && !my_units_of_type[unit_types::factory].empty();
// 					r.make_marines = current_frame < 15 * 60 * 8;
// 					r.army_comp_tank_vulture = true;
// 					r.army_comp_tank_goliath = false;
// 					r.army_comp_goliath_vulture = false;
// 					r.support_science_vessels = tank_count >= 2;
// 					r.support_wraiths = scv_count >= 60 && wraith_count < 4;
// 					r.research_siege_mode = machine_shop_count > 0;
// 					r.research_vulture_upgrades = vulture_count > 2;

// 					if ((marine_count >= 3 && tank_count < 1) || (enemy_lurker_count > enemy_mutalisk_count && marine_count >= enemy_mutalisk_count)) {
// 						anti_lurker();
// 					} else {
// 						double desired_army_supply = std::max(scv_count*scv_count* 0.015 + scv_count * 0.8 - 16, 4.0);
// 						r.prioritize_workers = scv_count < 20 || army_supply >= desired_army_supply;
// 						r.two_rax_marines = marine_count < 6 || !my_units_of_type[unit_types::factory].empty();
// 						if (enemy_lurker_count > enemy_mutalisk_count && tank_count < enemy_lurker_count * 2) r.two_rax_marines = false;
// 						r.attack = army_supply >= 36.0 && army_supply >= enemy_army_supply + 16.0;
// 						r.use_control_spots = false;
// 						r.aggressive_vultures = true;
// 						r.aggressive_wraiths = true;
// 						r.fast_expand = army_supply >= enemy_army_supply + 4.0;
// 						r.two_fact_vultures = false;
// 						r.one_fact_tanks = tank_count < enemy_lurker_count || (army_supply >= 20.0 && tank_count < army_supply / 20.0 && enemy_ground_army_supply >= enemy_air_army_supply);
// 						if (army_supply > enemy_army_supply && tank_count < 2) r.one_fact_tanks = true;
// 						r.two_fact_goliaths = false;
// 						r.make_wraiths = false;
// 						r.two_starports = false;
// 						r.make_bunker = !my_units_of_type[unit_types::factory].empty() && current_used_total_supply < 60;
// 						r.make_marines = false;
// 						r.army_comp_tank_vulture = true;
// 						r.army_comp_tank_goliath = false;
// 						r.army_comp_goliath_vulture = false;
// 						r.support_science_vessels = tank_count >= 2;
// 						r.support_wraiths = false;
// 						r.research_siege_mode = machine_shop_count > 0;
// 						r.research_vulture_upgrades = vulture_count >= 4;
// 					}

// 					//r.prioritize_workers = army_supply >= enemy_army_supply && (scv_count < 30 || scv_count < army_supply);
// 					r.prioritize_workers = army_supply >= enemy_army_supply && scv_count < army_supply + 20;
// 					r.two_rax_marines = true;
// 					r.attack = army_supply >= 36.0 && army_supply >= enemy_army_supply + 16.0;
// 					r.use_control_spots = false;
// 					r.aggressive_vultures = army_supply > enemy_army_supply;
// 					r.aggressive_wraiths = army_supply > enemy_army_supply && enemy_static_defence_count < 2 && wraith_count > enemy_hydralisk_count;
// 					r.fast_expand = army_supply >= enemy_army_supply + 4.0;
// 					//r.fast_expand = !my_units_of_type[unit_types::factory].empty();
// 					r.two_fact_vultures = false;
// 					r.one_fact_tanks = tank_count < enemy_lurker_count || (army_supply >= 20.0 && tank_count < army_supply / 20.0 && enemy_ground_army_supply >= enemy_air_army_supply);
// 					//r.one_fact_tanks = (enemy_spire_count + enemy_mutalisk_count == 0 && wraith_count >= 2) || tank_count < enemy_hydralisk_count / 2 + enemy_lurker_count;
// 					//r.one_fact_tanks = tank_count < enemy_lurker_count + (enemy_spire_count + enemy_mutalisk_count ? 0 : 2) || (scv_count >= 24 && army_supply > enemy_army_supply && enemy_spire_count + enemy_mutalisk_count == 0 && tank_count < 1 + marine_count / 12);
// 					r.two_fact_goliaths = false;
// 					r.make_wraiths = my_resource_depots.size() >= 2 && army_supply >= 26.0 && army_supply > enemy_army_supply + 8.0;
// 					r.two_starports = scv_count >= 40 && army_supply >= 20.0 && r.make_wraiths;
// 					r.make_bunker = true;
// 					r.make_marines = false;
// 					r.army_comp_tank_vulture = true;
// 					r.army_comp_tank_goliath = false;
// 					r.army_comp_goliath_vulture = false;
// 					r.support_science_vessels = my_resource_depots.size() >= 2 && army_supply > enemy_army_supply;
// 					//r.support_science_vessels = true;
// 					r.support_wraiths = my_workers.size() >= 20 && r.make_wraiths;
// 					r.research_siege_mode = tank_count > 0;
// 					r.research_vulture_upgrades = vulture_count >= 4;

					//r.prioritize_workers = army_supply >= enemy_army_supply && (scv_count < 30 || scv_count < army_supply);
					r.prioritize_workers = army_supply >= enemy_army_supply && scv_count < army_supply + 20;
					r.two_rax_marines = true;
					r.attack = army_supply >= 36.0 && army_supply >= enemy_army_supply + 16.0;
					r.use_control_spots = false;
					r.aggressive_vultures = army_supply > enemy_army_supply;
					r.aggressive_wraiths = army_supply > enemy_army_supply && enemy_static_defence_count < 2 && wraith_count > enemy_hydralisk_count;
					r.fast_expand = army_supply >= enemy_army_supply + 4.0;
					//r.fast_expand = !my_units_of_type[unit_types::factory].empty();
					r.two_fact_vultures = false;
					r.one_fact_tanks = tank_count < enemy_lurker_count || (army_supply >= 20.0 && tank_count < army_supply / 20.0 && enemy_ground_army_supply >= enemy_air_army_supply);
					//r.one_fact_tanks = (enemy_spire_count + enemy_mutalisk_count == 0 && wraith_count >= 2) || tank_count < enemy_hydralisk_count / 2 + enemy_lurker_count;
					//r.one_fact_tanks = tank_count < enemy_lurker_count + (enemy_spire_count + enemy_mutalisk_count ? 0 : 2) || (scv_count >= 24 && army_supply > enemy_army_supply && enemy_spire_count + enemy_mutalisk_count == 0 && tank_count < 1 + marine_count / 12);
					r.two_fact_goliaths = false;
					r.make_wraiths = current_used_total_supply >= 180;
					r.two_starports = false;
					r.make_bunker = current_frame < 15 * 60 * 8 && r.fast_expand;
					r.make_marines = false;
					r.army_comp_tank_vulture = true;
					r.army_comp_tank_goliath = false;
					r.army_comp_goliath_vulture = false;
					r.support_science_vessels = my_resource_depots.size() >= 2 && ((army_supply > enemy_army_supply && army_supply >= 28.0) || enemy_lurker_count);
					//r.support_science_vessels = true;
					r.support_wraiths = my_workers.size() >= 20 && r.make_wraiths;
					r.research_siege_mode = tank_count > 0;
					r.research_vulture_upgrades = vulture_count >= 4;
				};

				auto anti_4pool = [&]() {
					if (scv_count >= 15 && (army_supply >= 6.0 || current_minerals >= 300)) {
						bio_vs_z();
					} else {
						r.prioritize_workers = marine_count >= 4;
						//r.two_rax_marines = scv_count >= 15 || marine_count >= 8 || current_minerals >= 300;
						r.two_rax_marines = current_minerals >= 300;
						r.attack = false;
						r.use_control_spots = false;
						r.aggressive_vultures = army_supply > enemy_army_supply;
						r.aggressive_wraiths = army_supply > enemy_army_supply;
						r.fast_expand = false;
						r.two_fact_vultures = false;
						r.one_fact_tanks = false;
						r.two_fact_goliaths = false;
						r.make_wraiths = false;
						r.two_starports = false;
						r.make_bunker = false;
						r.make_marines = true;
						r.army_comp_tank_vulture = true;
						r.army_comp_tank_goliath = false;
						r.army_comp_goliath_vulture = false;
						r.support_science_vessels = false;
						r.support_wraiths = false;
						r.research_siege_mode = false;
						r.research_vulture_upgrades = false;
					}
				};

				auto one_rax_fe = [&]() {
					//r.prioritize_workers = scv_count < 30 || scv_count < army_supply || (tank_count >= 8 && scv_count < 55);
					//r.prioritize_workers = scv_count < 30 || (army_supply >= 20.0 && scv_count < 55) || army_supply >= 50.0;
					r.prioritize_workers = scv_count < 40 || (army_supply >= 65.0 && scv_count < army_supply);
					r.two_rax_marines = false;
					//r.attack = tank_count >= 14 && army_supply > enemy_army_supply;
					r.attack = army_supply >= 60.0 && enemy_army_supply <= army_supply - 20.0;
					r.use_control_spots = false;
					r.aggressive_vultures = army_supply > enemy_army_supply;
					r.aggressive_wraiths = false;
					bool zealot_rush_defence = current_frame < 15 * 60 * 8 && vulture_count + marine_count / 2 < enemy_zealot_count;
					r.fast_expand = !zealot_rush_defence;
// 					r.two_fact_vultures = zealot_rush_defence || (tank_count >= 4 && tank_count > enemy_dragoon_count && vulture_count < 4);
// 					//r.one_fact_tanks = scv_count < 30 && !zealot_rush_defence;
// 					r.one_fact_tanks = !zealot_rush_defence && tank_count < 12;
// 					//r.two_fact_goliaths = scv_count >= 50 && ((tank_count > enemy_dragoon_count && goliath_count < tank_count) || enemy_air_army_supply > enemy_ground_army_supply);
					bool need_more_tanks = tank_count < enemy_dragoon_count / 2 || tank_count < vulture_count / 2;
					r.two_fact_vultures = tank_count >= 2 && vulture_count < 16 && army_supply < 40.0 && !need_more_tanks;
					r.one_fact_tanks = tank_count < 2 || need_more_tanks;
					r.two_fact_goliaths = false;
//					r.two_fact_goliaths = scv_count >= 80 && goliath_count < tank_count * 2 + vulture_count / 2;
// 					if (enemy_static_defence_count && enemy_ground_army_supply < 16.0 && tank_count >= 2) {
// 						r.two_fact_goliaths = true;
// 					}
					r.make_wraiths = false;
					r.two_starports = false;
					r.make_bunker = current_frame < 15 * 60 * 8 && my_resource_depots.size() >= 2;
					r.make_marines = (army_supply < 10.0 && marine_count < 4) || zealot_rush_defence;
					r.army_comp_tank_vulture = true;
					r.army_comp_tank_goliath = false;
					r.army_comp_goliath_vulture = false;
					r.support_science_vessels = my_workers.size() >= 50 || enemy_dt_count;
					r.support_wraiths = current_used_total_supply >= 180;
					r.research_siege_mode = !my_units_of_type[unit_types::factory].empty() && !zealot_rush_defence;
					r.research_vulture_upgrades = vulture_count > 0;
				};

				if (go_cc) {
					// cc
					r.prioritize_workers = true;
					r.two_rax_marines = false;
					r.attack = false;
					r.use_control_spots = true;
					r.aggressive_vultures = true;
					r.aggressive_wraiths = false;
					r.fast_expand = true;
					r.two_fact_vultures = false;
					r.one_fact_tanks = false;
					r.two_fact_goliaths = players::opponent_player->race == race_protoss && enemy_proxy_building_count < 2 && enemy_static_defence_count >= 4 && goliath_count < 25 + enemy_air_army_supply / 3;
					int my_tanks = my_units_of_type[unit_types::siege_tank_tank_mode].size() + my_units_of_type[unit_types::siege_tank_tank_mode].size();
					r.make_wraiths = my_tanks >= 12;
					r.two_starports = my_workers.size() >= 80;
					r.make_bunker = false;
					r.make_marines = my_units_of_type[unit_types::marine].size() == 0;
					bool go_goliath_vulture = my_tanks >= 10 && current_minerals >= 500 && current_gas < 500;
					r.army_comp_tank_vulture = !go_goliath_vulture && my_workers.size() < 60;
					r.army_comp_tank_goliath = !go_goliath_vulture && my_workers.size() >= 60;
					r.army_comp_goliath_vulture = go_goliath_vulture;
					r.support_science_vessels = my_workers.size() >= 50;
					r.support_wraiths = my_workers.size() >= 20 && (air_army_supply > enemy_air_army_supply || current_used_total_supply >= 180);
					r.research_siege_mode = my_workers.size() >= 25;
					r.research_vulture_upgrades = my_workers.size() >= 35;
					//} else if (tsc::rng(1.0) < 0.5) {
				} else if (go_vult_vs_p) {
					// vult
					int cc_count = my_resource_depots.size();
					if (cc_count < 2) {
						one_rax_fe();
					} else {
						double desired_army_supply = std::max(scv_count*scv_count* 0.015 + scv_count * 0.8 - 16, 4.0);
						r.prioritize_workers = scv_count < 24 || army_supply >= desired_army_supply;
						r.two_rax_marines = false;
						//r.attack = (tank_count >= 12 || army_supply >= enemy_army_supply + 40.0) && army_supply > enemy_army_supply + 20.0;
						r.attack = false;
						//r.use_control_spots = current_frame < 15 * 60 * 15;
						r.use_control_spots = false;
						r.aggressive_vultures = army_supply > enemy_army_supply;
						r.aggressive_wraiths = false;
						r.fast_expand = false;
						//r.two_fact_vultures = (scv_count < 30 || vulture_count < 12) && (vulture_count < tank_count / 3 + 3) && (tank_count >= enemy_dragoon_count);
						r.two_fact_vultures = scv_count < 40 && (vulture_count < tank_count * 3 + 3) && (tank_count >= enemy_dragoon_count);
						r.one_fact_tanks = !r.two_fact_vultures && current_frame < 15 * 60 * 15;
						r.two_fact_goliaths = false;
						r.make_wraiths = false;
						r.two_starports = false;
						r.make_bunker = current_frame < 15 * 60 * 10 && cc_count >= 2;
						r.make_marines = scv_count < 30 && army_supply < 4;
						r.army_comp_tank_vulture = tank_count < enemy_dragoon_count + 6;
						r.army_comp_tank_goliath = false;
						r.army_comp_goliath_vulture = !r.army_comp_tank_vulture;
						r.support_science_vessels = scv_count >= 50;
						r.support_wraiths = false;
						r.research_siege_mode = tank_count > 0 || current_used_total_supply >= 60;
						r.research_vulture_upgrades = !my_units_of_type[unit_types::factory].empty();
					}
				} else if (go_vult) {
					// vult
					int cc_count = my_resource_depots.size();
					r.prioritize_workers = scv_count < 16 || (scv_count >= 30 && army_supply >= 20) || army_supply >= enemy_army_supply;
					//double desired_army_supply = std::max(scv_count*scv_count* 0.015 + scv_count * 0.8 - 16, 4.0);
					//r.prioritize_workers = scv_count < 20 || army_supply >= desired_army_supply;
					r.two_rax_marines = false;
					//r.attack = army_supply >= 30 && army_supply > enemy_army_supply;
					r.attack = current_frame >= 15 * 60 * 10 && army_supply > enemy_army_supply;
					r.use_control_spots = !r.attack;
					//r.aggressive_vultures = vulture_count >= 8;
					r.aggressive_vultures = current_frame >= 15 * 60 * 10;
					r.aggressive_wraiths = false;
					//r.fast_expand = scv_count >= 26;
					r.fast_expand = false;
					r.two_fact_vultures = scv_count < 30 || vulture_count < 12;
					r.one_fact_tanks = false;
					r.two_fact_goliaths = vulture_count >= 8 && goliath_count < 2;
					r.make_wraiths = false;
					r.two_starports = false;
					r.make_bunker = false;
					r.make_marines = scv_count < 30 && (marine_count == 0 || (vulture_count > marine_count && marine_count < 5));
					r.army_comp_tank_vulture = true;
					r.army_comp_tank_goliath = false;
					r.army_comp_goliath_vulture = false;
					r.support_science_vessels = scv_count >= 50;
					r.support_wraiths = scv_count >= 60 && army_supply >= enemy_army_supply - 8.0;
					r.research_siege_mode = tank_count > 0 || current_used_total_supply >= 60;
					r.research_vulture_upgrades = !my_units_of_type[unit_types::factory].empty();
				} else if (go_anti_lurker) {
					// anti lurker
					if (true) {
						anti_lurker();
					} else if (true) {
						int cc_count = my_resource_depots.size();
						r.prioritize_workers = scv_count < 24 || army_supply >= 28.0;
						r.two_rax_marines = false;
						r.attack = army_supply >= 60 && army_supply > enemy_army_supply;
						r.use_control_spots = false;
						r.aggressive_vultures = true;
						r.aggressive_wraiths = false;
						r.fast_expand = false;
						r.two_fact_vultures = cc_count < 2 && tank_count == 0;
						r.one_fact_tanks = vulture_count >= 4 && tank_count < 4;
						r.two_fact_goliaths = false;
						r.make_wraiths = false;
						r.two_starports = false;
						r.make_bunker = current_frame <= 15 * 60 * 10 && cc_count >= 2;
						r.make_marines = army_supply < 4.0 && marine_count < 4;
						r.army_comp_tank_vulture = true;
						r.army_comp_tank_goliath = false;
						r.army_comp_goliath_vulture = false;
						r.support_science_vessels = army_supply >= 20.0;
						r.support_wraiths = scv_count >= 40 && army_supply > enemy_army_supply + 16.0;
						r.research_siege_mode = tank_count > 0;
						r.research_vulture_upgrades = vulture_count >= 2;
					} else if (true) {
						int cc_count = my_resource_depots.size();
						r.prioritize_workers = scv_count < 24 || army_supply >= 28.0;
						r.two_rax_marines = cc_count >= 2 && (marine_count < 6 || tank_count >= 4 || (my_units_of_type[unit_types::factory].size() && my_units_of_type[unit_types::barracks].size() < 2) || scv_count >= 30);
						r.attack = army_supply >= 20.0;
						r.use_control_spots = false;
						r.aggressive_vultures = true;
						r.aggressive_wraiths = false;
						r.fast_expand = scv_count >= 16;
						r.two_fact_vultures = false;
						r.one_fact_tanks = cc_count >= 2 || r.fast_expand;
						r.two_fact_goliaths = false;
						r.make_wraiths = false;
						r.two_starports = false;
						//r.make_bunker = current_frame < 15 * 60 * 10 && cc_count >= 2;
						r.make_bunker = false;
						r.make_marines = marine_count < 6;
						r.army_comp_tank_vulture = false;
						r.army_comp_tank_goliath = true;
						r.army_comp_goliath_vulture = false;
						r.support_science_vessels = army_supply >= 20.0;
						r.support_wraiths = scv_count >= 40 && army_supply > enemy_army_supply + 16.0;
						r.research_siege_mode = tank_count > 0;
						r.research_vulture_upgrades = vulture_count >= 4;
					} else {
						r.prioritize_workers = scv_count < 36 || army_supply >= 28.0;
						r.two_rax_marines = false;
						r.attack = army_supply >= 60 && army_supply > enemy_army_supply;
						r.use_control_spots = false;
						r.aggressive_vultures = true;
						r.aggressive_wraiths = false;
						r.fast_expand = vulture_count >= 4 && army_supply > enemy_army_supply;
						r.two_fact_vultures = goliath_count == 0 && vulture_count < 4;
						r.one_fact_tanks = false;
						r.two_fact_goliaths = goliath_count < tank_count + 4;
						r.make_wraiths = false;
						r.two_starports = my_workers.size() >= 80;
						r.make_bunker = false;
						r.make_marines = my_units_of_type[unit_types::marine].size() == 0;
						r.army_comp_tank_vulture = false;
						r.army_comp_tank_goliath = true;
						r.army_comp_goliath_vulture = false;
						r.support_science_vessels = army_supply >= 30.0 || science_vessel_count == 0;
						r.support_wraiths = scv_count >= 40 && army_supply > enemy_army_supply + 16.0;
						r.research_siege_mode = tank_count > 0;
						r.research_vulture_upgrades = vulture_count >= 4;
					}
				} else if (go_goliaths) {
					// goliaths
					r.prioritize_workers = true;
					r.two_rax_marines = false;
					r.attack = army_supply >= 60 && army_supply > enemy_army_supply;
					r.use_control_spots = false;
					r.aggressive_vultures = vulture_count >= 8;
					r.aggressive_wraiths = true;
					r.fast_expand = scv_count >= 26;
					r.two_fact_vultures = goliath_count == 0 && vulture_count < 4;
					r.one_fact_tanks = false;
					r.two_fact_goliaths = goliath_count < tank_count + 4;
					r.make_wraiths = false;
					r.two_starports = my_workers.size() >= 80;
					r.make_bunker = false;
					r.make_marines = my_units_of_type[unit_types::marine].size() == 0;
					r.army_comp_tank_vulture = false;
					r.army_comp_tank_goliath = true;
					r.army_comp_goliath_vulture = false;
					r.support_science_vessels = scv_count >= 50 || enemy_lurker_count;
					r.support_wraiths = scv_count >= 40 && army_supply > enemy_army_supply + 16.0;
					r.research_siege_mode = tank_count > 0;
					r.research_vulture_upgrades = vulture_count >= 4;
				} else if (false) {
					// 2rax
					r.prioritize_workers = scv_count < 10 || army_supply > enemy_army_supply;
					bool go_tanks = scv_count >= 14 && army_supply > enemy_army_supply && enemy_proxy_building_count;
					if (enemy_marine_count >= marine_count - 4) go_tanks = false;
					r.two_rax_marines = !go_tanks && my_units_of_type[unit_types::marine].size() < 16 && marine_count < enemy_marine_count + 4;
					r.attack = current_used_total_supply >= 30 && army_supply > enemy_army_supply;
					r.use_control_spots = false;
					r.aggressive_vultures = true;
					r.aggressive_wraiths = false;
					r.fast_expand = my_workers.size() >= 20;
					r.two_fact_vultures = !go_tanks && !r.two_rax_marines && (my_workers.size() < 30 || my_units_of_type[unit_types::vulture].size() < 12);
					if (scv_count >= 13 && army_supply > enemy_army_supply && current_gas == 0.0) r.two_fact_vultures = true;
					r.one_fact_tanks = go_tanks;
					r.two_fact_goliaths = false;
					int my_tanks = my_units_of_type[unit_types::siege_tank_tank_mode].size() + my_units_of_type[unit_types::siege_tank_tank_mode].size();
					r.make_wraiths = my_tanks >= 12;
					r.two_starports = my_workers.size() >= 80;
					r.make_bunker = false;
					r.make_marines = marine_count < enemy_marine_count + 1;
					bool go_goliath_vulture = my_tanks >= 10 && current_minerals >= 500 && current_gas < 500;
					r.army_comp_tank_vulture = !go_goliath_vulture && my_workers.size() < 60;
					r.army_comp_tank_goliath = !go_goliath_vulture && my_workers.size() >= 60;
					r.army_comp_goliath_vulture = go_goliath_vulture;
					r.support_science_vessels = my_workers.size() >= 50;
					r.support_wraiths = my_workers.size() >= 20 && enemy_marine_count == 0;
					r.research_siege_mode = my_workers.size() >= 35 || go_tanks;
					r.research_vulture_upgrades = army_supply >= 20.0 && my_workers.size() >= 20 && vulture_count >= 2;
				} else if (go_2_port_wraith) {
					// 2 port wraith
					r.prioritize_workers = true;
					r.two_rax_marines = false;
					r.attack = army_supply >= enemy_army_supply + 8.0 && (enemy_static_defence_count < 6 || army_supply >= 30.0);
					r.use_control_spots = false;
					r.aggressive_vultures = false;
					r.aggressive_wraiths = false;
					r.fast_expand = my_workers.size() >= 30;
					r.two_fact_vultures = scv_count < 30 && enemy_marine_count + enemy_attacking_worker_count >= 6;
					bool make_wraiths = enemy_goliath_count + enemy_static_defence_count + enemy_dragoon_count + enemy_hydralisk_count / 2 < 6;
					if (tank_count >= 8 && wraith_count < tank_count) make_wraiths = true;
					r.one_fact_tanks = wraith_count >= 12 || !make_wraiths;
					r.two_fact_goliaths = false;
					int my_tanks = my_units_of_type[unit_types::siege_tank_tank_mode].size() + my_units_of_type[unit_types::siege_tank_tank_mode].size();
					r.make_wraiths = !my_units_of_type[unit_types::starport].empty() && make_wraiths;
					r.two_starports = make_wraiths;
					r.make_bunker = false;
					r.make_marines = scv_count < 20 && (my_units_of_type[unit_types::marine].size() == 0 || marine_count < (enemy_marine_count + enemy_attacking_worker_count));
					bool go_goliath_vulture = my_tanks >= 10 && current_minerals >= 500 && current_gas < 500;
					r.army_comp_tank_vulture = true;
					r.army_comp_tank_goliath = false;
					r.army_comp_goliath_vulture = false;
					r.support_science_vessels = my_workers.size() >= 50 || enemy_lurker_count;
					r.support_wraiths = my_workers.size() >= 20 && r.make_wraiths;
					r.research_siege_mode = my_workers.size() >= 35 && !r.make_marines;
					r.research_vulture_upgrades = ((vulture_count >= 2 && scv_count >= 25 && enemy_ground_large_army_supply) || my_workers.size() >= 35) && !r.make_marines;
				} else if (false) {
					// vp
					bool attack = army_supply > enemy_army_supply + 16.0;
					r.prioritize_workers = !attack || scv_count < army_supply;
					r.two_rax_marines = false;
					r.attack = attack;
					r.use_control_spots = true;
					r.aggressive_vultures = army_supply > enemy_army_supply;
					r.aggressive_wraiths = false;
					r.fast_expand = tank_count >= 1;
					bool zealot_rush_defence = current_frame < 15 * 60 * 8 && vulture_count + marine_count / 2 < enemy_zealot_count;
					r.two_fact_vultures = zealot_rush_defence || (tank_count >= 4 && tank_count > enemy_dragoon_count && vulture_count < 4);
					r.one_fact_tanks = scv_count < 30 && !zealot_rush_defence;
					//r.one_fact_tanks = !zealot_rush_defence && tank_count < 12;
					r.two_fact_goliaths = scv_count >= 50 && ((tank_count > enemy_dragoon_count && goliath_count < tank_count) || enemy_air_army_supply > enemy_ground_army_supply);
					r.make_wraiths = false;
					r.two_starports = false;
					r.make_bunker = current_frame < 15 * 60 * 8 && !my_units_of_type[unit_types::factory].empty() && (enemy_zealot_count == 0 || enemy_dragoon_count);
					r.make_marines = (army_supply < 10.0 && marine_count < 4) || zealot_rush_defence;
					r.army_comp_tank_vulture = true;
					r.army_comp_tank_goliath = false;
					r.army_comp_goliath_vulture = false;
					r.support_science_vessels = my_workers.size() >= 50;
					r.support_wraiths = current_used_total_supply >= 180;
					r.research_siege_mode = !my_units_of_type[unit_types::factory].empty() && !zealot_rush_defence;
					r.research_vulture_upgrades = vulture_count >= 6;
				} else if (go_1_rax_fe) {
				//} else if (override) {
					// 1 rax fe
					one_rax_fe();
				} else if (go_mech_vs_z) {
				//} else if (false) {
					// mech vs z
					//if (scv_count < 14 || (scv_count < 25 && enemy_zergling_count >= marine_count / 2)) {
					if (scv_count < 12) {
						anti_4pool();
					} else {
						r.prioritize_workers = scv_count < 30 || army_supply > scv_count * 0.75 || army_supply > enemy_army_supply + 8.0;
						r.two_rax_marines = false;
						r.attack = false;
						r.use_control_spots = true;
						r.aggressive_vultures = army_supply > enemy_army_supply;
						r.aggressive_wraiths = army_supply > enemy_army_supply;
						r.fast_expand = false;
// 						r.two_fact_vultures = vulture_count < 2;
// 						r.one_fact_tanks = false;
// 						r.two_fact_goliaths = vulture_count >= 2 && goliath_count < 4;
						r.two_fact_vultures = true;
						r.one_fact_tanks = false;
						r.two_fact_goliaths = false;
						r.make_wraiths = false;
						r.two_starports = false;
						r.make_bunker = current_frame < 15 * 60 * 10 && my_resource_depots.size() >= 2;
						r.make_marines = current_frame < 15 * 60 * 10 && marine_count < 4;
						r.army_comp_tank_vulture = enemy_ground_army_supply >= ground_army_supply - 32.0;
						r.army_comp_tank_goliath = !r.army_comp_tank_vulture;
						r.army_comp_goliath_vulture = false;
						r.support_science_vessels = my_workers.size() >= 50 || enemy_lurker_count;
						r.support_wraiths = scv_count >= 50 && army_supply > enemy_army_supply + 8.0 && army_supply >= 70.0;
						r.research_siege_mode = tank_count > 0;
						r.research_vulture_upgrades = vulture_count > 0;
					}
				} else if (go_bio_vs_z) {
					// bio vs z
					bio_vs_z();
				} else if (go_4pool_def) {
					// 4 pool defence
					anti_4pool();
				} else if (go_bio_tank_push) {
					r.prioritize_workers = scv_count < 30 || (current_frame >= 15 * 60 * 15 && (scv_count < 50 || army_supply >= scv_count));
					r.two_rax_marines = true;
					r.attack = current_frame >= 15 * 60 * 15;
					r.use_control_spots = false;
					r.aggressive_vultures = true;
					r.aggressive_wraiths = false;
					r.fast_expand = army_supply >= 4;
					r.two_fact_vultures = false;
					r.one_fact_tanks = current_frame < 15 * 60 * 15;
					r.two_fact_goliaths = false;
					r.make_wraiths = false;
					r.two_starports = false;
					r.make_bunker = false;
					r.make_marines = army_supply < 4.0;
					printf("r.make_marines is %d, army_supply %g\n", r.make_marines, army_supply);
					r.army_comp_tank_vulture = false;
					r.army_comp_tank_goliath = true;
					r.army_comp_goliath_vulture = false;
					r.support_science_vessels = my_workers.size() >= 50 || enemy_dt_count + enemy_lurker_count;
					r.support_wraiths = false;
					r.research_siege_mode = tank_count > 0;
					r.research_vulture_upgrades = false;
				} else if (go_anti_ualbertabot) {
					// anti ualbertabot
					bool maybe_zerg = players::opponent_player->race == race_zerg || players::opponent_player->random;
					if (maybe_zerg) {
						anti_4pool();
						if (players::opponent_player->random && scv_count >= 12) {
							r.two_fact_vultures = true;
						}
					} else {
						bool zealot_rush_defence = enemy_dragoon_count == 0 && vulture_count < 1 + enemy_zealot_count;
						if (players::opponent_player->race == race_protoss) zealot_rush_defence = vulture_count < 8;
						//r.prioritize_workers = (scv_count < 30 || scv_count < army_supply) && (my_units_of_type[unit_types::factory].empty() || !zealot_rush_defence);
						r.prioritize_workers = (scv_count < 30 || scv_count < army_supply) && !zealot_rush_defence;
						r.two_rax_marines = false;
						r.attack = (tank_count >= 14 && army_supply > enemy_army_supply) || (army_supply >= std::max(enemy_army_supply * 2, 16.0));
						r.use_control_spots = false;
						r.aggressive_vultures = army_supply > enemy_army_supply;
						r.aggressive_wraiths = false;
						//bool zealot_rush_defence = current_frame < 15 * 60 * 8 && vulture_count + marine_count / 2 < enemy_zealot_count;
						r.fast_expand = false;
						r.two_fact_vultures = zealot_rush_defence || (tank_count >= 4 && tank_count > enemy_dragoon_count && vulture_count < 4);
						//r.one_fact_tanks = scv_count < 30 && !zealot_rush_defence;
						r.one_fact_tanks = !zealot_rush_defence && tank_count < 12;
						//r.two_fact_goliaths = scv_count >= 50 && ((tank_count > enemy_dragoon_count && goliath_count < tank_count) || enemy_air_army_supply > enemy_ground_army_supply);
						r.two_fact_goliaths = scv_count >= 80 && goliath_count < tank_count * 2 + vulture_count / 2;
						if (enemy_static_defence_count && enemy_ground_army_supply < 16.0 && tank_count >= 2) {
							r.two_fact_goliaths = true;
						}
						r.make_wraiths = false;
						r.two_starports = false;
						r.make_bunker = current_frame < 15 * 60 * 8 && my_resource_depots.size() >= 2;
						r.make_marines = (army_supply < 10.0 && marine_count < 4) || zealot_rush_defence;
						r.army_comp_tank_vulture = true;
						r.army_comp_tank_goliath = false;
						r.army_comp_goliath_vulture = false;
						r.support_science_vessels = my_workers.size() >= 50 || enemy_dt_count;
						r.support_wraiths = current_used_total_supply >= 180;
						r.research_siege_mode = !my_units_of_type[unit_types::factory].empty() && !zealot_rush_defence;
						r.research_vulture_upgrades = vulture_count >= 6;
					}
				}

				if (being_pool_rushed) {
					if (scv_count < 15 || (army_supply < 6.0 && current_minerals < 300)) {
						anti_4pool();
					}
				}

				set_indices(r);

// 				log(log_level_test, "chosen actions are");
// 				for (auto& v : r.indices) log(log_level_test, " %d", v);
// 				log(log_level_test, "\n");
			}

			nn2::history.push_back({ current_nn_input, current_nn_output, current_nn_actions });

// 			log(log_level_test, " input -- ");
// 			for (auto& v : current_nn_input.input) log(log_level_test, " %g", v);
// 			log(log_level_test, "\n");
// 			log(log_level_test, " hidden state input -- ");
// 			for (auto& v : current_nn_input.hidden_state_input) {
// 				for (auto& v2 : v) log(log_level_test, " %g", v2);
// 			}
// 			log(log_level_test, "\n");
// 			log(log_level_test, " cell state input -- ");
// 			for (auto& v : current_nn_input.cell_state_input) {
// 				for (auto& v2 : v) log(log_level_test, " %g", v2);
// 			}
// 			log(log_level_test, "\n");
// 			log(log_level_test, " output -- ");
// 			for (auto& v : current_nn_output.output) log(log_level_test, " %g", v);
// 			log(log_level_test, "\n");

// 			log(log_level_test, " hidden state output -- ");
// 			for (auto& v : current_nn_output.hidden_state_output) {
// 				for (auto& v2 : v) log(log_level_test, " %g", v2);
// 			}
// 			log(log_level_test, "\n");
// 			log(log_level_test, " cell state output -- ");
// 			for (auto& v : current_nn_output.cell_state_output) {
// 				for (auto& v2 : v) log(log_level_test, " %g", v2);
// 			}
// 			log(log_level_test, "\n");

			//nn2::save();
		}

		multitasking::yield_point();

		max_mineral_workers = get_max_mineral_workers();

		auto cur_st = buildpred::get_my_current_state();
		size_t bases = cur_st.bases.size();

		if (current_frame < 15 * 60) {
			auto s = get_next_base();
			if (s) {
				natural_pos = s->pos;
				natural_cc_build_pos = s->cc_build_pos;

				natural_choke = combat::find_choke_from_to(unit_types::siege_tank_tank_mode, natural_pos, combat::op_closest_base, false, false, false, 32.0 * 10, 1, true);
			}
		}
		if (natural_pos != xy() && !my_units_of_type[unit_types::bunker].empty() && bases == 1) {
			combat::my_closest_base_override = natural_pos;
			combat::my_closest_base_override_until = current_frame + 15 * 10;
		}
		if (natural_pos != xy() && vulture_count >= 4 && bases == 1) {
			combat::my_closest_base_override = natural_pos;
			combat::my_closest_base_override_until = current_frame + 15 * 10;
		}

		combat::no_aggressive_groups = !current_nn_actions.attack;
		combat::use_control_spots = current_nn_actions.use_control_spots;
		combat::aggressive_vultures = current_nn_actions.aggressive_vultures;
		combat::aggressive_wraiths = current_nn_actions.aggressive_wraiths;

		default_upgrades = current_used_total_supply >= 60.0;
		default_bio_upgrades = current_used_total_supply >= 80 || my_units_of_type[unit_types::marine].size() >= 12;

		resource_gathering::max_gas = 100.0;
		if (!my_units_of_type[unit_types::factory].empty()) resource_gathering::max_gas = 250.0;
		if (my_workers.size() >= 24 || current_minerals >= 300) resource_gathering::max_gas = 0.0;
		if (!current_nn_actions.one_fact_tanks && (current_minerals < 100 || (current_nn_actions.make_marines && !current_nn_actions.research_vulture_upgrades && marine_count < 4 && current_minerals < 200))) {
			if (!my_units_of_type[unit_types::factory].empty() && current_nn_actions.two_fact_vultures && my_units_of_type[unit_types::vulture].empty()) {
				if (enemy_zealot_count && enemy_dragoon_count == 0 && army_supply < 16.0) resource_gathering::max_gas = 1.0;
			}
			if (my_workers.size() < 24 && my_units_of_type[unit_types::factory].size() >= 1 && current_nn_actions.two_fact_vultures && current_nn_actions.make_marines && marine_count < 4 && current_minerals < 200) {
				if (my_units_of_type[unit_types::vulture].size() == my_completed_units_of_type[unit_types::vulture].size()) {
					resource_gathering::max_gas = 1.0;
				}
			}
		}
		if (scv_count < 15 && current_minerals < 100.0) {
			if (!current_nn_actions.two_fact_vultures && !current_nn_actions.two_fact_goliaths && !current_nn_actions.one_fact_tanks && !current_nn_actions.two_starports) {
				resource_gathering::max_gas = 1.0;
			}
		}

		if (players::opponent_player->race == race_protoss && current_used_total_supply < 140) {
			get_upgrades::set_no_auto_upgrades(true);
			if (army_supply > enemy_army_supply) {
// 				if (current_used_total_supply >= 80) {
// 					get_upgrades::set_upgrade_value(upgrade_types::terran_vehicle_weapons_1, -1.0);
// 				}
			}
		}

		if (enemy_wraith_count && !my_units_of_type[unit_types::goliath].empty()) {
			scouting::comsat_supply = 30.0;
		}
		if (enemy_dt_count + enemy_lurker_count) {
			scouting::comsat_supply = 30.0;
		}

		if ((players::opponent_player->race == race_zerg || players::opponent_player->random) && army_supply < 4.0) {
			combat::force_defence_is_scared_until = current_frame + 15 * 10;
		}

		//if (bases >= 3 && players::opponent_player->race != race_protoss) {
		if (bases >= 3 && army_supply >= 80.0) {
			double min_defend_supply = 4.0 * bases;

			double defend_supply = 0.0;
			for (auto* a : combat::live_combat_units) {
				if (a->never_assign_to_aggressive_groups) defend_supply += a->u->type->required_supply;
			}
			log(log_level_info, "defend_supply is %g\n", defend_supply);

			if (defend_supply < min_defend_supply && !combat::live_combat_units.empty()) {
				for (int i = 0; i < 10 && defend_supply < min_defend_supply; ++i) {
					auto* a = combat::live_combat_units[tsc::rng(combat::live_combat_units.size())];
					if (a->never_assign_to_aggressive_groups) continue;
					if (a->u->type->is_worker) continue;
					//if (a->u->type != unit_types::vulture && a->u->stats->ground_weapon) {
					if (a->u->stats->ground_weapon) {
						a->never_assign_to_aggressive_groups = true;
						defend_supply += a->u->type->required_supply;
					}
				}
			}

			if (current_used_total_supply >= 180 && current_minerals >= 1000 && current_gas >= 800) {
				for (auto* a : combat::live_combat_units) {
					if (a->never_assign_to_aggressive_groups) a->never_assign_to_aggressive_groups = false;
				}
			}

		}

// 		if (opponent_is_random && current_used_total_supply < 18) {
// 			int scouts = 0;
// 			if (start_locations.size() <= 2) {
// 				if (current_used_total_supply >= 7) scouts = 1;
// 			} else {
// 				if (current_used_total_supply >= 6) scouts = 1;
// 				if (current_used_total_supply >= 8) scouts = 2;
// 			}
// 
// 			if ((int)scouting::all_scouts.size() < scouts) {
// 				unit*scout_unit = get_best_score(my_workers, [&](unit*u) {
// 					if (u->controller->action != unit_controller::action_gather) return std::numeric_limits<double>::infinity();
// 					return 0.0;
// 				}, std::numeric_limits<double>::infinity());
// 				if (scout_unit) scouting::add_scout(scout_unit);
// 			}
// 
// 			if (!players::opponent_player->random) {
// 				rm_all_scouts();
// 			}
// 		}

		if (scv_count < 20 && enemy_zergling_count / 4 + enemy_zealot_count > vulture_count && army_supply < 8.0) {
			rm_all_scouts();
		}

		multitasking::yield_point();

		auto get_turret_position_next_to = [&](unit* cc) {
			a_vector<xy> resources;
			for (unit* r : resource_units) {
				if (diag_distance(r->pos - cc->pos) > 32 * 12) continue;
				resources.push_back(r->pos);
			}
			if (resources.empty()) return xy();
			int n_turrets_near = 0;
			for (auto* u : my_units_of_type[unit_types::missile_turret]) {
				if (diag_distance(u->pos - cc->pos) <= 32 * 6) ++n_turrets_near;
			}
			if (n_turrets_near >= 3) return xy();
			a_vector<xy> potential_edges;
			for (auto i = resources.begin(); i != resources.end();) {
				xy r = *i;
				bool remove = false;
				int n_in_range = 0;
				for (auto* u : my_units_of_type[unit_types::missile_turret]) {
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

			int turret_w = unit_types::missile_turret->tile_width;
			int turret_h = unit_types::missile_turret->tile_width;
			int cc_w = cc->type->tile_width;
			int cc_h = cc->type->tile_height;
			xy cc_build_pos = cc->building->build_pos;

			xy best;
			double best_score = std::numeric_limits<double>::infinity();

			auto eval = [&](xy pos) {
				auto& bs = grid::get_build_square(pos);
				if (!build::build_is_valid(bs, unit_types::missile_turret)) {
					game->drawBoxMap(pos.x, pos.y, pos.x + turret_w * 32, pos.y + turret_h * 32, BWAPI::Colors::Red);
					return;
				}
				xy cpos = pos + xy(turret_w * 16, turret_h * 16);
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

			for (int y = cc_build_pos.y - turret_h * 32; y <= cc_build_pos.y + cc_h * 32; y += 32) {
				if (y == cc_build_pos.y + 32 || y == cc_build_pos.y + 64) continue;
				eval(xy(cc_build_pos.x - turret_w * 32, y));
				eval(xy(cc_build_pos.x + cc_w * 32, y));
			}
			for (int x = cc_build_pos.x - turret_w * 32; x <= cc_build_pos.x + cc_w * 32; x += 32) {
				if (x == cc_build_pos.x + 32 || x == cc_build_pos.x + 64) continue;
				eval(xy(x, cc_build_pos.y - turret_h * 32));
				eval(xy(x, cc_build_pos.y + cc_h * 32));
			}

			game->drawBoxMap(best.x, best.y, best.x + turret_w * 32, best.y + turret_h * 32, BWAPI::Colors::Green);

			return best;
		};

		if (my_units_of_type[unit_types::missile_turret].size() >= 2) {
			for (auto& b : build::build_tasks) {
				if (b.built_unit || b.dead) continue;
				if (b.type->unit == unit_types::missile_turret) {
					build::unset_build_pos(&b);
				}
			};
			turret_cc_defence_pos = xy();
			for (unit* u : my_resource_depots) {
				xy pos = get_turret_position_next_to(u);
				if (pos != xy()) {
					turret_cc_defence_pos = pos;
				}
			}
		}

		if (enemy_spire_count + enemy_mutalisk_count || enemy_air_army_supply >= 8.0) {
			if (my_resource_depots.size() >= 2 && scv_count >= 26) {
				combat::build_missile_turret_count = 8;
			}
		}

		multitasking::yield_point();

		if (my_units_of_type[unit_types::supply_depot].empty() && !my_units_of_type[unit_types::barracks].empty()) {
			if (!built_rax_before_depot) {
				log(log_level_test, " rax before depot\n");
				built_rax_before_depot = true;
			}
		}
		if (current_frame <= 15 * 60 * 5 && enemy_zergling_count) {
			if (!being_pool_rushed) {
				update_possible_start_locations();
				for (unit* u : enemy_units) {
					if (u->type != unit_types::zergling) continue;
					double neg_travel_distance = get_best_score_value(possible_start_locations, [&](xy pos) {
						return -unit_pathing_distance(u, pos);
					});
					if (neg_travel_distance) {
						int travel_time = frames_to_reach(u, -neg_travel_distance);
						int spawn_time = u->creation_frame - travel_time;
						if (spawn_time <= 15 * 60 * 3 + 15 * 30) {
							log(log_level_test, " being pool rushed\n");
							being_pool_rushed = true;
							rm_all_scouts();
							break;
						}
					}
				}
			}
		}
		if (being_pool_rushed) {
			if (current_used_total_supply < 20) {
				combat::my_closest_base_override = my_start_location;
				combat::force_defence_is_scared_until = current_frame + 15 * 10;
				current_nn_actions.fast_expand = false;
			}
			if (build_early_bunker == built_rax_before_depot) {
				build_early_bunker = !built_rax_before_depot;
				if (build_early_bunker && count_units_plus_production(cur_st, unit_types::bunker) == 0) {
					a_unordered_set<build::build_task*> keep;
					auto keep_one = [&](unit_type* ut) {
						if (my_completed_units_of_type[ut].empty()) {
							auto* t = get_best_score_p(build::build_tasks, [&](build::build_task* t) {
								if (t->type->unit != ut) return std::numeric_limits<double>::infinity();
								if (t->built_unit) return (double)t->built_unit->remaining_build_time;
								return (double)ut->build_time;
							}, std::numeric_limits<double>::infinity());
							if (t) keep.insert(t);
						}
					};
					keep_one(unit_types::supply_depot);
					keep_one(unit_types::barracks);
					for (auto& b : build::build_tasks) {
						if (keep.count(&b) == 0) {
							b.dead = true;
							if (b.built_unit) b.built_unit->game_unit->cancelConstruction();
						}
					}
// 					auto* t = build::add_build_task(-10.0, unit_types::bunker);
// 					xy pos = get_early_bunker_pos();
// 					build::set_build_pos(t, pos);
// 					unit* builder = get_best_score(my_units_of_type[unit_types::scv], [&](unit* u) {
// 						if (u->controller->action != unit_controller::action_gather) return std::numeric_limits<double>::infinity();
// 						return diag_distance(u->pos - pos);
// 					}, std::numeric_limits<double>::infinity());
// 					if (builder) {
// 						t->send_builder_immediately = true;
// 						build::set_builder(t, builder, true);
// 					}
				}
				log(log_level_test, " build_early_bunker is now %d\n", build_early_bunker);
			}

			
		}

		if (current_nn_actions.make_wraiths || current_nn_actions.support_wraiths) {
			if (wraith_count >= 5 && army_supply < 25.0) {
				current_nn_actions.prioritize_workers = false;
				current_nn_actions.make_wraiths = false;
				current_nn_actions.support_wraiths = false;
			}
// 			if (enemy_goliath_count * 2 > wraith_count || (wraith_count >= 9 && enemy_goliath_count >= 2)) {
// 				current_nn_actions.make_wraiths = false;
// 				current_nn_actions.support_wraiths = false;
// 			}
		}
		if (current_nn_actions.prioritize_workers) {
			if (scv_count >= 48 && enemy_army_supply > army_supply && army_supply < 32.0) {
				current_nn_actions.prioritize_workers = false;
			}
		}

		return false;
	}

	virtual bool build(buildpred::state&st) override {
		using namespace buildpred;
		if (opening_state != -1) return false;

		auto army = this->army;

		int cc_count = count_units_plus_production(st, unit_types::cc);

		if ((being_pool_rushed || (st.used_supply[race_terran] < 11 && !current_nn_actions.prioritize_workers)) && st.frame < 15 * 60 * 5) {
			if (st.used_supply[race_terran] < 18 || current_used_total_supply >= 18) {
				army = [army](state&st) {
					return nodelay(st, unit_types::scv, army);
				};
				army = [army](state&st) {
					return nodelay(st, unit_types::marine, army);
				};
			}
// 			if (build_early_bunker && count_units_plus_production(st, unit_types::bunker) == 0) {
// 				army = [army](state&st) {
// 					return nodelay(st, unit_types::bunker, army);
// 				};
// 			}
			return army(st);
		}

		st.dont_build_refineries = count_units_plus_production(st, unit_types::refinery) == 1 && army_supply < 12.0 && st.minerals < 300;

		if (current_nn_actions.two_rax_marines && count_units_plus_production(st, unit_types::barracks) < 2) st.dont_build_refineries = true;
		if (count_units_plus_production(st, unit_types::supply_depot) == 0) st.dont_build_refineries = true;

// 		if (!current_nn_actions.fast_expand) {
// 			if (count_units_plus_production(st, unit_types::supply_depot) <= 2) {
// 				if (my_units_of_type[unit_types::refinery].empty()) {
// 					if (st.minerals >= 200 && current_used_total_supply >= 14) return nodelay(st, unit_types::refinery, army);
// 				}
// 			}
// 		}

		if (!current_nn_actions.prioritize_workers) {
			if (scv_count < max_mineral_workers) {
				army = [army](state&st) {
					return nodelay(st, unit_types::scv, army);
				};
			}
			if (scv_count >= 30 && count_units_plus_production(st, unit_types::factory) && vulture_count + tank_count + goliath_count >= 4) {
				if (count_units_plus_production(st, unit_types::factory) < 5) {
					army = [army](state&st) {
						return nodelay(st, unit_types::factory, army);
					};
				}
			}
		}

// 		if (st.minerals >= 200 || scv_count >= 16) {
// 			army = [army](state&st) {
// 				return maxprod(st, unit_types::vulture, army);
// 			};
// 		}

		//if ((cc_count >= 2 || scv_count >= 24) && vulture_count + tank_count + goliath_count) {
		if (true) {
			if (current_nn_actions.army_comp_tank_vulture) {
				army = [army](state&st) {
					return nodelay(st, unit_types::vulture, army);
				};
				army = [army](state&st) {
					return maxprod(st, unit_types::siege_tank_tank_mode, army);
				};
			} else if (current_nn_actions.army_comp_tank_goliath) {
				if (goliath_count >= tank_count) {
					army = [army](state&st) {
						return maxprod(st, unit_types::siege_tank_tank_mode, army);
					};
				} else {
					army = [army](state&st) {
						return maxprod(st, unit_types::goliath, army);
					};
				}
			} else if (current_nn_actions.army_comp_goliath_vulture) {
				army = [army](state&st) {
					return nodelay(st, unit_types::vulture, army);
				};
				army = [army](state&st) {
					return maxprod(st, unit_types::goliath, army);
				};
			}
		}

		if (current_nn_actions.two_rax_marines && scv_count >= 30) {
			if (count_units_plus_production(st, unit_types::barracks) < 8) {
				army = [army](state&st) {
					return maxprod(st, unit_types::marine, army);
				};
			} else {
				army = [army](state&st) {
					return nodelay(st, unit_types::marine, army);
				};
			}
		}

		if (current_nn_actions.support_wraiths) {
			//if (science_vessel_count < army_supply / 15.0) {
				army = [army](state&st) {
					return nodelay(st, unit_types::wraith, army);
				};
			//}
		}

		if (current_nn_actions.support_science_vessels) {
			if (science_vessel_count < army_supply / 20.0) {
				army = [army](state&st) {
					return nodelay(st, unit_types::science_vessel, army);
				};
			}
		}

		if (st.minerals >= 800 && st.used_supply[race_terran] < 190.0) {
			if (count_units_plus_production(st, unit_types::factory) < (st.minerals >= 2000 ? 12 : 8)) {
				army = [army](state&st) {
					return nodelay(st, unit_types::factory, army);
				};
			}
			army = [army](state&st) {
				return nodelay(st, unit_types::vulture, army);
			};
		}

		if (current_nn_actions.make_wraiths) {
			army = [army](state&st) {
				return nodelay(st, unit_types::wraith, army);
			};
		}

		if (current_nn_actions.two_starports) {
			if (count_units_plus_production(st, unit_types::starport) < 2) {
				army = [army](state&st) {
					return nodelay(st, unit_types::starport, army);
				};
			}
		}

		if (current_nn_actions.make_marines) {
			army = [army](state&st) {
				return nodelay(st, unit_types::marine, army);
			};
		}

		if (current_nn_actions.one_fact_tanks && resource_gathering::max_gas != 1.0) {
			army = [army](state&st) {
				return nodelay(st, unit_types::siege_tank_tank_mode, army);
			};
			if ((players::opponent_player->race == race_zerg || players::opponent_player->random) && current_nn_actions.make_marines) {
				army = [army](state&st) {
					return nodelay(st, unit_types::marine, army);
				};
			}
		}

		if (current_nn_actions.two_fact_goliaths) {
			if (count_units_plus_production(st, unit_types::factory) < 2) {
				army = [army](state&st) {
					return nodelay(st, unit_types::factory, army);
				};
			}
			army = [army](state&st) {
				return nodelay(st, unit_types::goliath, army);
			};
		}

		if (current_nn_actions.two_fact_vultures && scv_count < 40) {
			if (count_units_plus_production(st, unit_types::factory) < 2) {
				army = [army](state&st) {
					return nodelay(st, unit_types::factory, army);
				};
			}
			if (current_nn_actions.make_marines && marine_count < 6) {
				army = [army](state&st) {
					return nodelay(st, unit_types::marine, army);
				};
			}
			army = [army](state&st) {
				return nodelay(st, unit_types::vulture, army);
			};
			//if (enemy_tank_count + enemy_goliath_count > tank_count + goliath_count) {
			if (current_nn_actions.army_comp_tank_vulture) {
				//if (tank_count < vulture_count / 6 || (players::opponent_player->race == race_terran && vulture_count >= enemy_vulture_count + 3 && tank_count < 2)) {
				if (tank_count < vulture_count / 6) {
					army = [army](state&st) {
						return maxprod(st, unit_types::siege_tank_tank_mode, army);
					};
				}
			} else if (current_nn_actions.army_comp_goliath_vulture) {
				if (goliath_count < vulture_count / 6) {
					army = [army](state&st) {
						return maxprod(st, unit_types::goliath, army);
					};
				}
			}
		}

		if (scv_count >= 40 && vulture_count >= 4 && !current_nn_actions.two_rax_marines && marine_count < 10 && tank_count < 4 && count_units_plus_production(st, unit_types::factory) >= 4) {
			army = [army](state&st) {
				return maxprod(st, unit_types::siege_tank_tank_mode, army);
			};
		}

		//if (current_nn_actions.support_science_vessels && (science_vessel_count < (enemy_lurker_count + enemy_dt_count + 2) / 3 || (science_vessel_count == 0 && players::opponent_player->race == race_zerg)) && army_supply > enemy_army_supply) {
		if (current_nn_actions.support_science_vessels && science_vessel_count < (enemy_lurker_count + enemy_dt_count + 2) / 3 && army_supply > enemy_army_supply) {
			if (science_vessel_count < army_supply / 20.0) {
				army = [army](state&st) {
					return nodelay(st, unit_types::science_vessel, army);
				};
			}
		}
		if (current_nn_actions.support_science_vessels && army_supply > enemy_army_supply && players::opponent_player->race == race_zerg) {
			if (science_vessel_count == 0) {
				army = [army](state&st) {
					return nodelay(st, unit_types::science_vessel, army);
				};
			}
			if (science_vessel_count && !has_or_is_upgrading(st, upgrade_types::irradiate) && army_supply >= 24.0) {
				army = [army](state&st) {
					return nodelay(st, upgrade_types::irradiate, army);
				};
			}
		}

		if (current_nn_actions.research_vulture_upgrades) {
			if (!has_or_is_upgrading(st, upgrade_types::spider_mines)) {
				army = [army](state&st) {
					return nodelay(st, upgrade_types::spider_mines, army);
				};
			} else {
				if (!has_or_is_upgrading(st, upgrade_types::ion_thrusters)) {
					army = [army](state&st) {
						return nodelay(st, upgrade_types::ion_thrusters, army);
					};
				}
			}
		}

		if (current_nn_actions.research_siege_mode || tank_count > 0) {
			if (!has_or_is_upgrading(st, upgrade_types::siege_mode)) {
				army = [army](state&st) {
					return nodelay(st, upgrade_types::siege_mode, army);
				};
			}
		}

		if (current_nn_actions.two_rax_marines) {
			if (scv_count < 30) {
				if (count_units_plus_production(st, unit_types::barracks) < 2 && st.max_supply[race_terran] > 10) {
					army = [army](state&st) {
						return nodelay(st, unit_types::barracks, army);
					};
				}
				army = [army](state&st) {
					return nodelay(st, unit_types::marine, army);
				};
			}

			if (marine_count >= 8 && count_units_plus_production(st, unit_types::academy)) {
				if (players::opponent_player->race == race_zerg) {
					if (firebat_count < marine_count / 6 && firebat_count < 2 + enemy_zergling_count / 6) {
						army = [army](state&st) {
							return nodelay(st, unit_types::firebat, army);
						};
					}
				}
			}

			if (marine_count >= 8 && cc_count >= 2) {
				if (medic_count < (marine_count - 4) / 4) {
					army = [army](state&st) {
						return nodelay(st, unit_types::medic, army);
					};
				}
				//if ((!current_nn_actions.one_fact_tanks || tank_count) && (!current_nn_actions.support_science_vessels || science_vessel_count)) {
				if (true) {
					if (!has_or_is_upgrading(st, upgrade_types::stim_packs)) {
						army = [army](state&st) {
							return nodelay(st, upgrade_types::stim_packs, army);
						};
					} else {
						if (!has_or_is_upgrading(st, upgrade_types::u_238_shells)) {
							army = [army](state&st) {
								return nodelay(st, upgrade_types::u_238_shells, army);
							};
						}
						if (army_supply > enemy_army_supply) {
							if (!has_or_is_upgrading(st, upgrade_types::terran_infantry_armor_1)) {
								army = [army](state&st) {
									return nodelay(st, upgrade_types::terran_infantry_armor_1, army);
								};
							}
							if (!has_or_is_upgrading(st, upgrade_types::terran_infantry_weapons_1)) {
								army = [army](state&st) {
									return nodelay(st, upgrade_types::terran_infantry_weapons_1, army);
								};
							}
						}
					}
				}
			}
		}

// 		if (current_nn_actions.two_rax_marines && players::opponent_player->race == race_zerg) {
// 			if (my_resource_depots.size() >= 2 && count_units_plus_production(st, unit_types::bunker)) {
// 				if (ghost_count < 4 || ghost_count < enemy_lurker_count * 3 || ghost_count < marine_count / 6) {
// 					army = [army](state&st) {
// 						return nodelay(st, unit_types::ghost, army);
// 					};
// 				}
// 			}
// 		}

		if (scv_count >= 30 && count_units_plus_production(st, unit_types::factory) >= 3 && (current_nn_actions.prioritize_workers || scv_count >= 40)) {
			if (!has_or_is_upgrading(st, upgrade_types::terran_vehicle_plating_1)) {
				army = [army](state&st) {
					return nodelay(st, upgrade_types::terran_vehicle_plating_1, army);
				};
			}
			if (!has_or_is_upgrading(st, upgrade_types::terran_vehicle_weapons_1)) {
				army = [army](state&st) {
					return nodelay(st, upgrade_types::terran_vehicle_weapons_1, army);
				};
			}
		}

		bool build_defensive_turrets = false;
		if (players::opponent_player->race == race_zerg && (tank_count >= 2 || enemy_mutalisk_count + enemy_spire_count)) build_defensive_turrets = true;
		if (cc_count >= 2 && army_supply >= enemy_army_supply + 20.0) build_defensive_turrets = true;
		if (cc_count >= 2 && enemy_air_army_supply > marine_count + goliath_count * 2 + wraith_count * 2) build_defensive_turrets = true;
		if (build_defensive_turrets) {
			int n_turrets = 2;
			if (turret_cc_defence_pos != xy()) n_turrets += cc_count * 2 + 1;
			if (count_units_plus_production(st, unit_types::missile_turret) < n_turrets) {
				if (count_production(st, unit_types::missile_turret) == 0) {
					army = [army](state&st) {
						return nodelay(st, unit_types::missile_turret, army);
					};
				}
			}
		}

		if (players::opponent_player->race == race_protoss && army_supply < 20.0) {
			if (count_units_plus_production(st, unit_types::factory)) {
				if (enemy_citadel_of_adun_count + enemy_templar_archives_count) {
					if (count_units_plus_production(st, unit_types::missile_turret) < 2) {
						army = [army](state&st) {
							return nodelay(st, unit_types::missile_turret, army);
						};
					}
				}
			}
		}

		if (cc_count >= 2 && army_supply > enemy_army_supply + 8.0 && players::opponent_player->race != race_terran) {
			if (current_frame >= 15 * 60 * 10 && count_units_plus_production(st, unit_types::missile_turret) == 0) {
				army = [army](state&st) {
					return nodelay(st, unit_types::missile_turret, army);
				};
			}
		}
		if (army_supply >= enemy_army_supply && enemy_dt_count + enemy_lurker_count) {
			if (count_units_plus_production(st, unit_types::missile_turret) < 2) {
				army = [army](state&st) {
					return nodelay(st, unit_types::missile_turret, army);
				};
			}
		}
		if (!my_units_of_type[unit_types::siege_tank_tank_mode].empty() && army_supply > enemy_army_supply) {
			if (players::opponent_player->race != race_terran) {
				if (count_units_plus_production(st, unit_types::engineering_bay) == 0) {
					army = [army](state&st) {
						return nodelay(st, unit_types::engineering_bay, army);
					};
				}
			}
		}

		double enemy_air_threat_supply = enemy_air_army_supply - enemy_queen_count * 2.0;
		double anti_air_supply = marine_count + goliath_count * 2 + wraith_count * 2 + valkyrie_count * 3;
		double desired_anti_air_supply = enemy_air_threat_supply + std::min(enemy_air_threat_supply * 0.5, 16.0);
		if (army_supply >= 30.0) desired_anti_air_supply = std::max(desired_anti_air_supply, 4.0);
		if (army_supply >= 80.0) desired_anti_air_supply = std::max(desired_anti_air_supply, 16.0);
		//if (army_supply >= enemy_ground_army_supply + 20.0) desired_anti_air_supply += std::max((army_supply - anti_air_supply - enemy_ground_army_supply) / 2, 0.0);
		if (enemy_spire_count) {
			if (desired_anti_air_supply < 10.0) desired_anti_air_supply = 10.0;
		}
		if (anti_air_supply < desired_anti_air_supply) {
			int starports = count_units_plus_production(st, unit_types::starport);
			if (starports >= 2 || (starports && (goliath_count + 2) * 2 < desired_anti_air_supply)) {
				if (anti_air_supply < enemy_air_threat_supply || army_supply >= 60.0) {
					army = [army](state&st) {
						return nodelay(st, unit_types::wraith, army);
					};
				}
			}
			if (current_nn_actions.two_rax_marines) {
				army = [army](state&st) {
					return maxprod(st, unit_types::marine, army);
				};
			} else {
				if (count_units_plus_production(st, unit_types::factory)) {
					army = [army](state&st) {
						return nodelay(st, unit_types::goliath, army);
					};
				}
				if (tank_count == 0 && goliath_count == 0 && army_supply < 30.0) {
					army = [army](state&st) {
						return nodelay(st, unit_types::marine, army);
					};
				}
			}
		}

		if (current_nn_actions.make_marines && current_nn_actions.fast_expand && cc_count == 1) {
			army = [army](state&st) {
				return nodelay(st, unit_types::marine, army);
			};
		}

		if (current_nn_actions.make_bunker) {
			if (count_units_plus_production(st, unit_types::bunker) < 1) {
				army = [army](state&st) {
					return nodelay(st, unit_types::bunker, army);
				};
			}
		}

		if (current_nn_actions.prioritize_workers) {
			bool wait_for_refinery = false;
			bool wait_for_factory = false;
			if (cc_count == 1 && army_supply < 6.0) {
				if (current_nn_actions.two_fact_vultures || current_nn_actions.two_fact_goliaths || current_nn_actions.one_fact_tanks || current_nn_actions.two_starports) {
					if (count_units_plus_production(st, unit_types::refinery) == 0) {
						if (scv_count >= 12 && st.minerals < 150 && !st.dont_build_refineries) {
							wait_for_refinery = true;
						}
					} else {
						if (scv_count >= 15 && st.minerals < 275 && count_units_plus_production(st, unit_types::factory) == 0) {
							wait_for_factory = true;
						}
					}
				}
			}
			if (!wait_for_refinery && !wait_for_factory && scv_count < 80 && count_production(st, unit_types::scv) < 3) {
				army = [army](state&st) {
					return nodelay(st, unit_types::scv, army);
				};
			}
		}

// 		if (cc_count == 1 && scv_count >= 15 && players::opponent_player->race != race_zerg && opponent_has_expanded && can_expand) {
// 			army = [army](state&st) {
// 				return nodelay(st, unit_types::cc, army);
// 			};
// 		}

		if (cc_count == 1 && scv_count >= 15 && current_nn_actions.fast_expand && can_expand) {
			army = [army](state&st) {
				return nodelay(st, unit_types::cc, army);
			};
		}

		if (force_expand && count_production(st, unit_types::cc) == 0 && scv_count > max_mineral_workers) {
			army = [army](state&st) {
				return nodelay(st, unit_types::cc, army);
			};
		}

		if (being_pool_rushed) {
			if (marine_count < 5) {
				army = [army](state&st) {
					return nodelay(st, unit_types::marine, army);
				};
			}
			if (!st.units[unit_types::factory].empty() && vulture_count == 0) {
				army = [army](state&st) {
					return nodelay(st, unit_types::vulture, army);
				};
			}
		}

		return army(st);
	}

	virtual void post_build() override {

		if (output_enabled) {
			for (auto* bs : natural_choke.build_squares) {
				game->drawBoxMap(bs->pos.x, bs->pos.y, bs->pos.x + 32, bs->pos.y + 32, BWAPI::Colors::Orange, true);
			}
		}

		if (!call_place_expos) {
			if (natural_pos != xy()) {
				auto st = buildpred::get_my_current_state();

				for (auto&b : build::build_tasks) {
					if (b.built_unit || b.dead) continue;
					if (b.type->unit && b.type->unit->is_resource_depot && b.type->unit->builder_type->is_worker) {
						xy pos = b.build_pos;
						build::unset_build_pos(&b);
					}
				}

				for (auto&b : build::build_tasks) {
					if (b.built_unit || b.dead) continue;
					if (b.type->unit && b.type->unit->is_resource_depot && b.type->unit->builder_type->is_worker) {
						xy pos = b.build_pos;
						build::unset_build_pos(&b);

						auto pred = [&](grid::build_square&bs) {
							return build::build_full_check(bs, b.type->unit);
						};
						auto score = [&](xy pos) {
							double spawn_d = unit_pathing_distance(unit_types::scv, pos, my_start_location);
							double nat_d = unit_pathing_distance(unit_types::scv, pos, natural_pos);
							return spawn_d*spawn_d + nat_d*nat_d;
						};

						std::array<xy, 1> starts;
						starts[0] = my_start_location;
						pos = build_spot_finder::find_best(starts, 128, pred, score);

						if (pos != xy()) build::set_build_pos(&b, pos);
						else build::unset_build_pos(&b);
					}
				}
			}
		}

		if (natural_pos != xy()) {
			combat::control_spot* cs = get_best_score(combat::active_control_spots, [&](combat::control_spot* cs) {
				return diag_distance(cs->pos - natural_pos);
			});
			if (cs) {
// 				test_one = natural_pos;
// 				test_two = cs->pos;
// 				test_three = cs->incoming_pos;
				size_t iterations = 0;
				for (auto&b : build::build_tasks) {
					if (b.built_unit || b.dead) continue;
					if (b.type->unit == unit_types::missile_turret && turret_cc_defence_pos != xy()) {
						build::unset_build_pos(&b);
						build::set_build_pos(&b, turret_cc_defence_pos);
						continue;
					}
					if (b.type->unit == unit_types::bunker && build_early_bunker && current_frame < 15 * 60 * 5) {
						build::unset_build_pos(&b);
						build::set_build_pos(&b, get_early_bunker_pos());
						continue;
					}
					if ((b.type->unit == unit_types::bunker && my_units_of_type[unit_types::bunker].empty()) || (b.type->unit == unit_types::missile_turret && my_units_of_type[unit_types::missile_turret].size() < 2)) {
						xy pos = b.build_pos;
						build::unset_build_pos(&b);

						size_t turrets = my_units_of_type[unit_types::missile_turret].size();

						a_vector<std::tuple<xy, double>> scores;

						auto pred = [&](grid::build_square&bs) {
							if (++iterations % 1000 == 0) multitasking::yield_point();
							if (b.type->unit == unit_types::missile_turret) {
								bool is_inside = test_pred(natural_choke.inside_squares, [&](auto* nbs) {
									return nbs == &bs;
								});
								if (!natural_choke.inside_squares.empty() && !is_inside) return false;
							}
							for (unit* u : enemy_units) {
								if (u->stats->ground_weapon && diag_distance(u->pos - bs.pos) <= u->stats->ground_weapon->max_range + 64) {
									return false;
								}
							}
							return build::build_full_check(bs, b.type->unit);
						};
						auto score = [&](xy pos) {
							double r = 0.0;
							xy center_pos = pos + xy(b.type->unit->tile_width * 16, b.type->unit->tile_height * 16);
							for (auto* nbs : natural_choke.build_squares) {
								double d = diag_distance(center_pos - nbs->pos);
								r += d*d;
							}
							return r;
							// 							double inc_d = unit_pathing_distance(unit_types::scv, pos, cs->incoming_pos);
							// 							double nat_d = unit_pathing_distance(unit_types::scv, pos, natural_pos);
							// 							//if (b.type->unit == unit_types::missile_turret && turrets == 0 && players::opponent_player->race == race_protoss) return inc_d*inc_d + nat_d*nat_d*2.0;
							// 							if (b.type->unit == unit_types::missile_turret && turrets == 0) return inc_d*inc_d + nat_d*nat_d*2.0;
							// 							scores.emplace_back(pos, inc_d*inc_d + nat_d*nat_d);
							// 							return inc_d*inc_d + nat_d*nat_d;
							//return inc_d*inc_d;
						};

						std::array<xy, 1> starts;
						starts[0] = natural_pos;
						pos = build_spot_finder::find_best(starts, 128, pred, score);

						double highest_score = 0.0;
						for (auto& v : scores) {
							if (std::get<1>(v) > highest_score) highest_score = std::get<1>(v);
						}
						for (auto& v : scores) {
							double s = std::get<1>(v) / highest_score;
							int r = (int)(s * 255);
							xy pos = std::get<0>(v);
							game->drawBoxMap(pos.x, pos.y, pos.x + 32, pos.y + 32, BWAPI::Color(r, r, r));
						}

						if (pos != xy()) build::set_build_pos(&b, pos);
						else build::unset_build_pos(&b);
					}
				}
			}
		}

	}

};
