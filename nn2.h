
#include "tscnn.h"

namespace nn2 {

	using value_t = float;

	struct lstm {

		struct net_info {
			tscnn::nn<> n;

			value_t* input;
			a_vector<value_t*> hidden_state_input;
			a_vector<value_t*> cell_state_input;

			value_t* output;
			a_vector<value_t*> hidden_state_output;
			a_vector<value_t*> cell_state_output;

			value_t* input_gradients;
			a_vector<value_t*> hidden_state_input_gradients;
			a_vector<value_t*> cell_state_input_gradients;

			value_t* output_gradients;
			a_vector<value_t*> hidden_state_output_gradients;
			a_vector<value_t*> cell_state_output_gradients;
		};

		size_t inputs = 1;
		size_t outputs = 1;
		size_t state_size = 1;
		size_t layers = 1;
		size_t seq_length = 1;

		tscnn::nn<> a;

		a_vector<size_t> forget_gate_offsets;

		a_vector<std::pair<tscnn::unit_ref, tscnn::unit_ref>> state_inputs;
		a_vector<std::pair<tscnn::unit_ref, tscnn::unit_ref>> state_outputs;

		a_vector<tscnn::vector_ref> hidden_state_output_gradients;
		a_vector<tscnn::vector_ref> cell_state_output_gradients;

		a_vector<net_info> nets;

		void create() {

			using namespace tscnn;

			auto nn_input = a.make_input(inputs);

			for (size_t i = 0; i < layers; ++i) {
				state_inputs.emplace_back(a.make_input(state_size), a.make_input(state_size));
			}

			for (size_t i = 0; i < layers; ++i) {

				unit_ref nn_processed_input;
				if (i == 0) {
					forget_gate_offsets.push_back(a.total_weights);
					auto nn_processed_input_a = a.make_linear(state_size * 4, nn_input);
					auto nn_processed_input_b = a.make_linear(state_size * 4, state_inputs[0].first);
					nn_processed_input = a.make_add(nn_processed_input_a, nn_processed_input_b);
				} else {
					forget_gate_offsets.push_back(a.total_weights);
					auto nn_processed_input_a = a.make_linear(state_size * 4, state_outputs.back().first);
					auto nn_processed_input_b = a.make_linear(state_size * 4, state_inputs[i].first);
					nn_processed_input = a.make_add(nn_processed_input_a, nn_processed_input_b);
				}

				auto nn_cell_state_input = state_inputs[i].second;

				auto nn_forget_gate = a.make_sigmoid(a.make_select(0, state_size, nn_processed_input));
				auto nn_in_gate = a.make_sigmoid(a.make_select(state_size, state_size, nn_processed_input));
				auto nn_in_scale_gate = a.make_tanh(a.make_select(state_size * 2, state_size, nn_processed_input));
				auto nn_out_gate = a.make_sigmoid(a.make_select(state_size * 3, state_size, nn_processed_input));

				auto nn_cell_state_post_forget = a.make_mul(nn_cell_state_input, nn_forget_gate);

				auto nn_add_cell_state = a.make_mul(nn_in_gate, nn_in_scale_gate);

				auto cell_output = a.make_add(nn_cell_state_post_forget, nn_add_cell_state);

				auto hidden_output = a.make_mul(nn_out_gate, a.make_tanh(cell_output));

				state_outputs.emplace_back(hidden_output, cell_output);
			}

			auto nn_a_output = a.make_linear(outputs, state_outputs.back().first);

			a.make_output(nn_a_output);
			for (auto& v : state_outputs) {
				v.first = a.make_output(v.first);
				v.second = a.make_output(v.second);
			}

			auto a_input = a.inputs[0];

			auto a_output = a.outputs[0];
			auto a_output_gradients = a.new_gradient(a_output.gradients_index);
			a_vector<vector_ref> hidden_state_output_gradients;
			a_vector<vector_ref> cell_state_output_gradients;
			for (auto& v : state_outputs) {
				hidden_state_output_gradients.push_back(a.new_gradient(v.first.gradients_index));
				cell_state_output_gradients.push_back(a.new_gradient(v.second.gradients_index));
			}

			a.construct();

			nets.resize(seq_length + 1);

			for (auto& v : nets) {
				v.n = a;

				v.input = v.n.get_values(a_input.output);
				v.output = v.n.get_values(a_output.output);

				for (size_t i = 0; i < state_outputs.size(); ++i) {
					v.hidden_state_input.push_back(v.n.get_values(state_inputs[i].first.output));
					v.cell_state_input.push_back(v.n.get_values(state_inputs[i].second.output));

					v.hidden_state_output.push_back(v.n.get_values(state_outputs[i].first.output));
					v.cell_state_output.push_back(v.n.get_values(state_outputs[i].second.output));
				}
				v.input_gradients = v.n.get_values(v.n.get_input_gradient(a_input));
				v.output_gradients = v.n.get_values(a_output_gradients);
				for (size_t i = 0; i < state_outputs.size(); ++i) {
					v.hidden_state_output_gradients.push_back(v.n.get_values(hidden_state_output_gradients[i]));
					v.cell_state_output_gradients.push_back(v.n.get_values(cell_state_output_gradients[i]));

					v.hidden_state_input_gradients.push_back(v.n.get_values(v.n.get_input_gradient(state_inputs[i].first)));
					v.cell_state_input_gradients.push_back(v.n.get_values(v.n.get_input_gradient(state_inputs[i].second)));
				}
				for (size_t i = 0; i < a_output_gradients.size; ++i) {
					v.output_gradients[i] = 0.0;
				}
			}
		}
	};

	lstm network;
	a_vector<value_t> weights;

	a_vector<value_t> input_mean;
	a_vector<value_t> input_stddev;

	struct nn_input {
		a_vector<value_t> input;
		a_vector<a_vector<value_t>> hidden_state_input;
		a_vector<a_vector<value_t>> cell_state_input;
	};

	struct nn_output {
		a_vector<value_t> output;
		a_vector<a_vector<value_t>> hidden_state_output;
		a_vector<a_vector<value_t>> cell_state_output;
	};

	a_vector<double> current_minerals_history;
	a_vector<double> current_gas_history;

#ifndef NN2_TRAIN_ONLY
	void state_avg_task() {
		while (true) {

			if (current_minerals_history.size() >= 0x100) current_minerals_history.clear();
			if (current_gas_history.size() >= 0x100) current_gas_history.clear();

			current_minerals_history.push_back(current_minerals);
			current_gas_history.push_back(current_gas);

			multitasking::sleep(4);
		}
	}

	void get_state(nn_input& r) {
		r.input.resize(60);
		r.input[0] = (value_t)my_workers.size();
		r.input[1] = (value_t)current_used_total_supply - my_workers.size();
		r.input[2] = 0.0;
		r.input[3] = 0.0;
		for (unit* e : enemy_units) {
			r.input[2] += (value_t)e->type->required_supply;
			if (e->type->is_building) r.input[3] += (value_t)1.0;
		}
		r.input[4] = (value_t)(std::accumulate(current_minerals_history.begin(), current_minerals_history.end(), 0.0) / std::max(current_minerals_history.size(), (size_t)1));
		current_minerals_history.clear();
		r.input[5] = (value_t)(std::accumulate(current_gas_history.begin(), current_gas_history.end(), 0.0) / std::max(current_gas_history.size(), (size_t)1));
		current_gas_history.clear();
		r.input[6] = (value_t)current_minerals_per_frame;
		r.input[7] = (value_t)current_gas_per_frame;
		r.input[8] = (value_t)my_units_of_type[unit_types::barracks].size();
		r.input[9] = (value_t)my_units_of_type[unit_types::factory].size();
		r.input[10] = (value_t)my_units_of_type[unit_types::starport].size();
		r.input[11] = (value_t)my_units_of_type[unit_types::marine].size();
		r.input[12] = (value_t)my_units_of_type[unit_types::medic].size();
		r.input[13] = (value_t)my_units_of_type[unit_types::vulture].size();
		r.input[14] = (value_t)(my_units_of_type[unit_types::siege_tank_tank_mode].size() + my_units_of_type[unit_types::siege_tank_siege_mode].size());
		r.input[15] = (value_t)my_units_of_type[unit_types::goliath].size();
		r.input[16] = (value_t)my_units_of_type[unit_types::wraith].size();
		r.input[17] = (value_t)my_units_of_type[unit_types::science_vessel].size();

		int enemy_marine_count = 0;
		int enemy_firebat_count = 0;
		int enemy_ghost_count = 0;
		int enemy_vulture_count = 0;
		int enemy_tank_count = 0;
		int enemy_goliath_count = 0;
		int enemy_wraith_count = 0;
		int enemy_valkyrie_count = 0;
		int enemy_bc_count = 0;
		int enemy_science_vessel_count = 0;
		int enemy_dropship_count = 0;
		double enemy_army_supply = 0.0;
		double enemy_air_army_supply = 0.0;
		double enemy_ground_army_supply = 0.0;
		double enemy_ground_large_army_supply = 0.0;
		double enemy_ground_small_army_supply = 0.0;
		int enemy_static_defence_count = 0;
		int enemy_proxy_building_count = 0;
		int enemy_non_gas_proxy_building_count = 0;
		double enemy_attacking_army_supply = 0.0;
		int enemy_attacking_worker_count = 0;
		int enemy_dt_count = 0;
		int enemy_lurker_count = 0;
		int enemy_arbiter_count = 0;
		int enemy_lair_count = 0;
		int enemy_spire_count = 0;
		int enemy_factory_count = 0;
		int enemy_zealot_count = 0;
		int enemy_zergling_count = 0;
		int enemy_dragoon_count = 0;
		int enemy_mutalisk_count = 0;
		int enemy_shuttle_count = 0;
		int enemy_robotics_facility_count = 0;
		int enemy_robotics_support_bay_count = 0;
		int enemy_citadel_of_adun_count = 0;
		int enemy_templar_archives_count = 0;
		int enemy_hydralisk_count = 0;

		update_possible_start_locations();
		for (unit* e : enemy_units) {
			if (e->type == unit_types::marine) ++enemy_marine_count;
			if (e->type == unit_types::firebat) ++enemy_firebat_count;
			if (e->type == unit_types::ghost) ++enemy_ghost_count;
			if (e->type == unit_types::vulture) ++enemy_vulture_count;
			if (e->type == unit_types::siege_tank_tank_mode) ++enemy_tank_count;
			if (e->type == unit_types::siege_tank_siege_mode) ++enemy_tank_count;
			if (e->type == unit_types::goliath) ++enemy_goliath_count;
			if (e->type == unit_types::wraith) ++enemy_wraith_count;
			if (e->type == unit_types::valkyrie) ++enemy_valkyrie_count;
			if (e->type == unit_types::battlecruiser) ++enemy_bc_count;
			if (e->type == unit_types::science_vessel) ++enemy_science_vessel_count;
			if (e->type == unit_types::dropship) ++enemy_dropship_count;
			if (!e->type->is_worker && !e->building) {
				if (e->is_flying) enemy_air_army_supply += e->type->required_supply;
				else {
					enemy_ground_army_supply += e->type->required_supply;
					if (e->type->size == unit_type::size_large) enemy_ground_large_army_supply += e->type->required_supply;
					if (e->type->size == unit_type::size_small) enemy_ground_small_army_supply += e->type->required_supply;
				}
				enemy_army_supply += e->type->required_supply;
			}
			if (e->type == unit_types::missile_turret) ++enemy_static_defence_count;
			if (e->type == unit_types::photon_cannon) ++enemy_static_defence_count;
			if (e->type == unit_types::sunken_colony) ++enemy_static_defence_count;
			if (e->type == unit_types::spore_colony) ++enemy_static_defence_count;
			if (true) {
				double e_d = get_best_score_value(possible_start_locations, [&](xy pos) {
					return unit_pathing_distance(unit_types::scv, e->pos, pos);
				});
				if (unit_pathing_distance(unit_types::scv, e->pos, combat::my_closest_base) < e_d) {
					if (e->building) ++enemy_proxy_building_count;
					if (e->building && !e->type->is_gas) ++enemy_non_gas_proxy_building_count;
					if (!e->type->is_worker) enemy_attacking_army_supply += e->type->required_supply;
					else ++enemy_attacking_worker_count;
				}
			}
			if (e->type == unit_types::dark_templar) ++enemy_dt_count;
			if (e->type == unit_types::lurker || e->type == unit_types::lurker_egg) ++enemy_lurker_count;
			if (e->type == unit_types::arbiter) ++enemy_arbiter_count;
			if (e->type == unit_types::lair) ++enemy_lair_count;
			if (e->type == unit_types::spire) ++enemy_spire_count;
			if (e->type == unit_types::factory) ++enemy_factory_count;
			if (e->type == unit_types::zealot) ++enemy_zealot_count;
			if (e->type == unit_types::zergling) ++enemy_zergling_count;
			if (e->type == unit_types::dragoon) ++enemy_dragoon_count;
			if (e->type == unit_types::mutalisk) ++enemy_mutalisk_count;
			if (e->type == unit_types::shuttle) ++enemy_mutalisk_count;
			if (e->type == unit_types::robotics_facility) ++enemy_mutalisk_count;
			if (e->type == unit_types::robotics_support_bay) ++enemy_robotics_support_bay_count;
			if (e->type == unit_types::citadel_of_adun) ++enemy_citadel_of_adun_count;
			if (e->type == unit_types::templar_archives) ++enemy_templar_archives_count;
			if (e->type == unit_types::hydralisk) ++enemy_hydralisk_count;
		}

		r.input[18] = (value_t)enemy_marine_count;
		r.input[19] = (value_t)enemy_firebat_count;
		r.input[20] = (value_t)enemy_ghost_count;
		r.input[21] = (value_t)enemy_vulture_count;
		r.input[22] = (value_t)enemy_tank_count;
		r.input[23] = (value_t)enemy_goliath_count;
		r.input[24] = (value_t)enemy_wraith_count;
		r.input[25] = (value_t)enemy_valkyrie_count;
		r.input[26] = (value_t)enemy_bc_count;
		r.input[27] = (value_t)enemy_science_vessel_count;
		r.input[28] = (value_t)enemy_dropship_count;
		r.input[29] = (value_t)enemy_army_supply;
		r.input[30] = (value_t)enemy_air_army_supply;
		r.input[31] = (value_t)enemy_ground_army_supply;
		r.input[32] = (value_t)enemy_ground_large_army_supply;
		r.input[33] = (value_t)enemy_ground_small_army_supply;
		r.input[34] = (value_t)enemy_static_defence_count;
		r.input[35] = (value_t)enemy_proxy_building_count;
		r.input[36] = (value_t)enemy_non_gas_proxy_building_count;
		r.input[37] = (value_t)enemy_attacking_army_supply;
		r.input[38] = (value_t)enemy_attacking_worker_count;
		r.input[39] = (value_t)enemy_dt_count;
		r.input[40] = (value_t)enemy_lurker_count;
		r.input[41] = (value_t)enemy_arbiter_count;
		r.input[42] = (value_t)enemy_lair_count;
		r.input[43] = (value_t)enemy_spire_count;
		r.input[44] = (value_t)enemy_factory_count;
		r.input[45] = (value_t)enemy_zealot_count;
		r.input[46] = (value_t)enemy_zergling_count;
		r.input[47] = (value_t)enemy_dragoon_count;
		r.input[48] = (value_t)enemy_mutalisk_count;
		r.input[50] = (value_t)enemy_shuttle_count;
		r.input[51] = (value_t)enemy_robotics_facility_count;
		r.input[52] = (value_t)enemy_robotics_support_bay_count;
		r.input[53] = (value_t)enemy_citadel_of_adun_count;
		r.input[54] = (value_t)enemy_templar_archives_count;
		r.input[55] = (value_t)enemy_hydralisk_count;
		r.input[56] = (value_t)(!players::opponent_player->random && players::opponent_player->race == race_terran);
		r.input[57] = (value_t)(!players::opponent_player->random && players::opponent_player->race == race_protoss);
		r.input[58] = (value_t)(!players::opponent_player->random && players::opponent_player->race == race_zerg);
		r.input[59] = (value_t)players::opponent_player->random;

	}

#endif

	void init(bool for_training = false) {

#ifndef NN2_TRAIN_ONLY
		if (!for_training) multitasking::spawn(state_avg_task, "state avg task");
#endif

		network.inputs = 60;
		network.outputs = 41;
		network.state_size = 256;
		network.layers = 2;
		network.seq_length = 1000;

		if (!for_training) network.seq_length = 1;

		network.create();

		weights.resize(network.a.total_weights);
		for (auto& v : weights) {
			v = -(value_t)0.1 + tsc::rng((value_t)0.2);
		}

		network.a.init_weights([&](size_t weight_offset, size_t weight_n, size_t bias_offset, size_t bias_n, size_t inputs, size_t outputs) {
			//float variance = std::sqrt(2.0f / (inputs + outputs)) * std::sqrt(3.0f);;
			float variance = 2.0f / (inputs + outputs);
			float stddev = std::sqrt(variance);
			std::normal_distribution<float> dis(0.0, stddev);
			for (size_t i = 0; i < weight_n; ++i) {
				weights[weight_offset + i] = dis(tsc::rng_engine);
			}
			for (size_t i = 0; i < bias_n; ++i) {
				weights[bias_offset + i] = 0;
			}
		});

		for (size_t offset : network.forget_gate_offsets) {
			value_t* w = weights.data() + offset;
			for (size_t oi = 0; oi < network.state_size; ++oi) {
				*w++ = 1.0;
			}
		}

		double sum = 0.0;
		for (auto& v : nn2::weights) {
			sum += v;
		}
		log(log_level_test, " sum of all initial weights: %g\n", sum);

	}

	struct nn_actions {
		a_vector<size_t> indices;
		bool prioritize_workers = false;
		bool two_rax_marines = false;
		bool attack = false;
		bool use_control_spots = false;
		bool aggressive_vultures = false;
		bool aggressive_wraiths = false;
		bool fast_expand = false;
		bool two_fact_vultures = false;
		bool one_fact_tanks = false;
		bool two_fact_goliaths = false;
		bool make_wraiths = false;
		bool two_starports = false;
		bool make_bunker = false;
		bool make_marines = false;
		bool army_comp_tank_vulture = false;
		bool army_comp_tank_goliath = false;
		bool army_comp_goliath_vulture = false;
		bool support_science_vessels = false;
		bool support_wraiths = false;
		bool research_siege_mode = false;
		bool research_vulture_upgrades = false;
	};

	struct history_entry {
		nn_input input;
		nn_output output;
		nn_actions actions;
		bool won;
	};

	a_vector<a_vector<history_entry>> loaded_history;
	//a_vector<history_entry> loaded_history;
	a_deque<history_entry> history;

	void set_action(bool set_index, nn_actions& r, bool& b, size_t indices_index, int n, int nn) {
		if (set_index) {
			if (b) r.indices[indices_index] = n;
			else if (nn != -1) r.indices[indices_index] = nn;
		} else b = r.indices[indices_index] == n;
	}

	void set_actions(nn_actions& r, bool set_index = false) {
		if (r.indices.size() != 20) xcept("indices size mismatch");
		set_action(set_index, r, r.prioritize_workers, 0, 0, 1);
		set_action(set_index, r, r.two_rax_marines, 1, 0, 1);
		set_action(set_index, r, r.attack, 2, 0, 1);
		set_action(set_index, r, r.use_control_spots, 3, 0, 1);
		set_action(set_index, r, r.aggressive_vultures, 4, 0, 1);
		set_action(set_index, r, r.aggressive_wraiths, 5, 0, 1);
		// 6 unused
		set_action(set_index, r, r.fast_expand, 7, 0, 1);
		set_action(set_index, r, r.two_fact_vultures, 8, 0, 1);
		set_action(set_index, r, r.one_fact_tanks, 9, 0, 1);
		set_action(set_index, r, r.two_fact_goliaths, 10, 0, 1);
		set_action(set_index, r, r.make_wraiths, 11, 0, 1);
		set_action(set_index, r, r.two_starports, 12, 0, 1);
		set_action(set_index, r, r.make_bunker, 13, 0, 1);
		set_action(set_index, r, r.make_marines, 14, 0, 1);
		set_action(set_index, r, r.army_comp_tank_vulture, 15, 0, -1);
		set_action(set_index, r, r.army_comp_tank_goliath, 15, 1, -1);
		set_action(set_index, r, r.army_comp_goliath_vulture, 15, 2, -1);
		set_action(set_index, r, r.support_science_vessels, 16, 0, 1);
		set_action(set_index, r, r.support_wraiths, 17, 0, 1);
		set_action(set_index, r, r.research_siege_mode, 18, 0, 1);
		set_action(set_index, r, r.research_vulture_upgrades, 19, 0, 1);
		// 		r.prioritize_workers = r.indices[0] == 0;
		// 		r.two_rax_marines = r.indices[1] == 0;
		// 		r.attack = r.indices[2] == 0;
		// 		r.use_control_spots = r.indices[3] == 0;
		// 		r.aggressive_vultures = r.indices[4] == 0;
		// 		r.aggressive_wraiths = r.indices[5] == 0;
		// 		r.fast_expand = r.indices[7] == 0;
		// 		r.two_fact_vultures = r.indices[8] == 0;
		// 		r.one_fact_tanks = r.indices[9] == 0;
		// 		r.two_fact_goliaths = r.indices[10] == 0;
		// 		r.make_wraiths = r.indices[11] == 0;
		// 		r.two_starports = r.indices[12] == 0;
		// 		r.make_bunker = r.indices[13] == 0;
		// 		r.make_marines = r.indices[14] == 0;
		// 		r.army_comp_tank_vulture = r.indices[15] == 0;
		// 		r.army_comp_tank_goliath = r.indices[15] == 1;
		// 		r.army_comp_goliath_vulture = r.indices[15] == 2;
		// 		r.support_science_vessels = r.indices[16] == 0;
		// 		r.support_wraiths = r.indices[17] == 0;
		// 		r.research_siege_mode = r.indices[18] == 0;
		// 		r.research_vulture_upgrades = r.indices[19] == 0;
	}

	void set_indices(nn_actions& r) {
		set_actions(r, true);
	}

	nn_actions forward(const nn_input& input, nn_output& output) {

		auto& n = network.nets[network.seq_length];

		if (input.input.size() != network.inputs) xcept("input size mismatch");
		if (input.hidden_state_input.size() != network.layers) xcept("hidden_state_input size mismatch");
		if (input.cell_state_input.size() != network.layers) xcept("cell_state_input size mismatch");

		if (weights.size() != network.a.total_weights) xcept("weights size mismatch");

		for (size_t i = 0; i < network.inputs; ++i) {
			n.input[i] = input.input[i];

			n.input[i] -= input_mean[i];
			if (input_stddev[i]) n.input[i] /= input_stddev[i];

			//log(log_level_test, "input %d: value %g  mean %g stddev %g\n", i, n.input[i], input_mean[i], input_stddev[i]);
		}
		for (size_t i = 0; i < network.layers; ++i) {
			if (input.hidden_state_input[i].size() != network.state_size) xcept("hidden_state_input size mismatch");
			if (input.cell_state_input[i].size() != network.state_size) xcept("cell_state_input size mismatch");
			for (size_t i2 = 0; i2 < network.state_size; ++i2) {
				n.hidden_state_input[i][i2] = input.hidden_state_input[i][i2];
				n.cell_state_input[i][i2] = input.cell_state_input[i][i2];
			}
		}
		n.n.set_evaluating();
		n.n.forward(n.n, weights.data());

		output.output.resize(network.outputs);
		for (size_t i = 0; i < network.outputs; ++i) {
			output.output[i] = n.output[i];
		}
		output.hidden_state_output.resize(network.layers);
		output.cell_state_output.resize(network.layers);
		for (size_t i = 0; i < network.layers; ++i) {
			output.hidden_state_output[i].resize(network.state_size);
			output.cell_state_output[i].resize(network.state_size);
			for (size_t i2 = 0; i2 < network.state_size; ++i2) {
				output.hidden_state_output[i][i2] = n.hidden_state_output[i][i2];
				output.cell_state_output[i][i2] = n.cell_state_output[i][i2];
			}
		}

		auto get_index = [&](value_t* output, size_t output_n) {
			size_t r = 0;
			value_t best = output[0];
			for (size_t i = 1; i < output_n; ++i) {
				if (output[i] > best) {
					best = output[i];
					r = i;
				}
			}
			return r;

			a_vector<value_t> probabilities(network.outputs);
			value_t temperature = 1.0;
			value_t sum = 0.0;
			for (size_t i = 0; i < output_n; ++i) {
				probabilities[i] = std::exp(output[i] / temperature);
				sum += probabilities[i];
			}
			double val = tsc::rng(sum);
			double n = 0.0;
			for (size_t i = 0; i < output_n; ++i) {
				n += probabilities[i];
				if (n >= val) return i;
			}
			return network.outputs - 1;
		};



		nn_actions r;
		r.indices.resize(20);
		r.indices[0] = get_index(n.output, 2);
		r.indices[1] = get_index(n.output + 2, 2);
		r.indices[2] = get_index(n.output + 4, 2);
		r.indices[3] = get_index(n.output + 6, 2);
		r.indices[4] = get_index(n.output + 8, 2);
		r.indices[5] = get_index(n.output + 10, 2);
		r.indices[6] = get_index(n.output + 12, 2);
		r.indices[7] = get_index(n.output + 14, 2);
		r.indices[8] = get_index(n.output + 16, 2);
		r.indices[9] = get_index(n.output + 18, 2);
		r.indices[10] = get_index(n.output + 20, 2);
		r.indices[11] = get_index(n.output + 22, 2);
		r.indices[12] = get_index(n.output + 24, 2);
		r.indices[13] = get_index(n.output + 26, 2);
		r.indices[14] = get_index(n.output + 28, 2);
		r.indices[15] = get_index(n.output + 30, 3);
		r.indices[16] = get_index(n.output + 33, 2);
		r.indices[17] = get_index(n.output + 35, 2);
		r.indices[18] = get_index(n.output + 37, 2);
		r.indices[19] = get_index(n.output + 39, 2);

		set_actions(r);

		return r;
	}


	template<typename T>
	T jv_to(json_value& jv);

	void jv_to(json_value& jv, size_t& r) {
		r = (size_t)(double)jv;
	}

	void jv_to(json_value& jv, value_t& r) {
		r = jv;
	}

	template<typename T>
	void jv_to(json_value& jv, a_vector<T>& r) {
		r.resize(jv.vector.size());
		for (size_t i = 0; i < r.size(); ++i) {
			r[i] = jv_to<T>(jv[i]);
		}
	}

	template<typename T>
	T jv_to(json_value& jv) {
		T tmp;
		jv_to(jv, tmp);
		return tmp;
	}

	void jv_to(json_value& jv, nn_input& r) {
		jv_to(jv["input"], r.input);
		jv_to(jv["hidden_state_input"], r.hidden_state_input);
		jv_to(jv["cell_state_input"], r.cell_state_input);
	}

	void jv_to(json_value& jv, nn_output& r) {
		jv_to(jv["output"], r.output);
		jv_to(jv["hidden_state_output"], r.hidden_state_output);
		jv_to(jv["cell_state_output"], r.cell_state_output);
	}

	void jv_to(json_value& jv, nn_actions& r) {
		jv_to(jv["indices"], r.indices);
		set_actions(r);
	}

	json_value jv_from(size_t v) {
		json_value r;
		r = (double)v;
		return r;
	}

	json_value jv_from(value_t v) {
		json_value r;
		r = v;
		return r;
	}

	template<typename T>
	json_value jv_from(const a_vector<T>& vec) {
		json_value r;
		r.type = json_value::t_array;
		r.vector.resize(vec.size());
		for (size_t i = 0; i < vec.size(); ++i) {
			r.vector[i] = jv_from(vec[i]);
		}
		return r;
	}

	json_value jv_from(const nn_input& v) {
		json_value r;
		r["input"] = jv_from(v.input);
		r["hidden_state_input"] = jv_from(v.hidden_state_input);
		r["cell_state_input"] = jv_from(v.cell_state_input);
		return r;
	}

	json_value jv_from(const nn_output& v) {
		json_value r;
		r["output"] = jv_from(v.output);
		r["hidden_state_output"] = jv_from(v.hidden_state_output);
		r["cell_state_output"] = jv_from(v.cell_state_output);
		return r;
	}

	json_value jv_from(const nn_actions& v) {
		json_value r;
		r["indices"] = jv_from(v.indices);
		return r;
	}

	template<typename T>
	struct is_std_array : std::false_type {};
	template<typename T, size_t N>
	struct is_std_array<std::array<T, N>> : std::true_type {};

	template<bool default_little_endian = true>
	struct data_reader {
		uint8_t*ptr = nullptr;
		uint8_t*end = nullptr;
		data_reader() = default;
		data_reader(uint8_t* ptr, uint8_t* end) : ptr(ptr), end(end) {}
		template<typename T, bool little_endian, typename std::enable_if<is_std_array<T>::value>::type* = nullptr>
		T value_at(uint8_t* ptr) {
			T r;
			for (auto&v : r) {
				v = value_at<std::remove_reference<decltype(v)>::type, little_endian>(ptr);
				ptr += sizeof(v);
			}
			return r;
		}
		template<typename T, bool little_endian, typename std::enable_if<!is_std_array<T>::value>::type* = nullptr>
		T value_at(uint8_t* ptr) {
			static_assert(std::is_integral<T>::value, "can only read integers and arrays of integers");
			T r = 0;
			for (size_t i = 0; i < sizeof(T); ++i) {
				r |= (T)ptr[i] << ((little_endian ? i : sizeof(T) - 1 - i) * 8);
			}
			return r;
		}
		template<typename T, bool little_endian = default_little_endian>
		T get() {
			if (ptr + sizeof(T) > end) xcept("data_reader: attempt to read past end");
			ptr += sizeof(T);
			return value_at<T, little_endian>(ptr - sizeof(T));
		}
		uint8_t* get_n(size_t n) {
			uint8_t*r = ptr;
			if (ptr + n > end || ptr + n < ptr) xcept("data_reader: attempt to read past end");
			ptr += n;
			return r;
		}
		template<typename T, bool little_endian = default_little_endian>
		a_vector<T> get_vec(size_t n) {
			uint8_t* data = get_n(n*sizeof(T));
			a_vector<T> r(n);
			for (size_t i = 0; i < n; ++i, data += sizeof(T)) {
				r[i] = value_at<T, little_endian>(data);
			}
			return r;
		}
		void skip(size_t n) {
			if (ptr + n > end || ptr + n < ptr) xcept("data_reader: attempt to seek past end");
			ptr += n;
		}
		size_t left() {
			return end - ptr;
		}
	};

	template<bool default_little_endian = true>
	struct data_writer {
		a_vector<uint8_t> vec_data;
		size_t mysize = 0;
		uint8_t* put_n(size_t n) {
			size_t newsize = mysize + n;
			if (vec_data.size() < newsize) vec_data.resize(std::max(newsize, vec_data.size() + std::max(newsize / 2, (size_t)16)));
			mysize = newsize;
			return vec_data.data() + newsize - n;
		}
		template<typename T, bool little_endian = default_little_endian>
		void put(T value) {
			uint8_t* ptr = put_n(sizeof(T));
			static_assert(std::is_integral<T>::value, "can only write integers");
			for (size_t i = 0; i < sizeof(T); ++i) {
				ptr[i] |= (uint8_t)(value >> ((little_endian ? i : sizeof(T) - 1 - i) * 8));
			}
		}
		uint8_t* data() {
			return vec_data.data();
		}
		size_t size() {
			return mysize;
		}
	};

	template<typename writer_T, typename T>
	void put(writer_T& writer, const T& value) {
		writer.template put<T>(value);
	}

	template<typename writer_T, typename std::enable_if<sizeof(float) == sizeof(uint32_t)>::type* = nullptr>
	void put(writer_T& writer, float value) {
		writer.template put<uint32_t>((uint32_t&)value);
	}

	template<typename writer_T, typename T>
	void put(writer_T& writer, const a_vector<T>& value) {
		writer.template put<uint64_t>((uint64_t)value.size());
		for (auto& v : value) {
			put(writer, v);
		}
	}

	template<typename reader_T, typename T>
	void get(reader_T& reader, T& value) {
		value = reader.template get<T>();
	}

	template<typename reader_T, typename std::enable_if<sizeof(float) == sizeof(uint32_t)>::type* = nullptr>
	void get(reader_T& reader, float& value) {
		auto v = reader.template get<uint32_t>();
		value = (float&)v;
	}

	template<typename reader_T, typename T>
	void get(reader_T& reader, a_vector<T>& value) {
		uint64_t size = reader.template get<uint64_t>();
		if ((uint64_t)(size_t)size != size) xcept("get: stored array too large");
		value.resize((size_t)size);
		for (size_t i = 0; i < (size_t)size; ++i) {
			get(reader, value[i]);
		}
	}

	uint64_t state_file_sig = 0x332193fefdf32a1e;
	uint64_t weights_file_sig = 0x1a6e87b5bbf0d729;

	// 	void save_weights(int iteration, const nn_output& state) {
	// 
	// // 		auto jv_state = jv_from(state);
	// // 		strategy::write_json(format("bwapi-data/write/tsc-bwai-nn2-state-%d.txt", iteration), jv_state);
	// // 		auto jv_weights = jv_from(weights);
	// // 		strategy::write_json(format("bwapi-data/write/tsc-bwai-nn2-weights-%d.txt", iteration), jv_weights);
	// 
	// 		if (true) {
	// 			data_writer<> w;
	// 			put(w, state.output);
	// 			put(w, state.hidden_state_output);
	// 			put(w, state.cell_state_output);
	// 			write_file(format("bwapi-data/write/tsc-bwai-nn2-state-%d", iteration), w.data(), w.size());
	// 		}
	// 
	// 		if (true) {
	// 			data_writer<> w;
	// 			put(w, weights);
	// 			put(w, input_mean);
	// 			put(w, input_stddev);
	// 			write_file(format("bwapi-data/write/tsc-bwai-nn2-weights-%d", iteration), w.data(), w.size());
	// 		}
	// 	}

	void train(const nn_output& initial_state) {

		using namespace tscnn;

		rmsprop<> opt;
		// 		opt.learning_rate = (value_t)1e-3;
		// 		opt.alpha = (value_t)0.95;
		// 		opt.weight_decay = (value_t)1e-5;
		// 		opt.momentum = (value_t)0.1;

		opt.learning_rate = (value_t)1e-4;
		opt.alpha = (value_t)0.95;
		//opt.weight_decay = (value_t)1e-5;
		opt.weight_decay = (value_t)0.0;
		opt.momentum = (value_t)0.1;

// 		opt.learning_rate = (value_t)1e-3;
// 		opt.alpha = (value_t)0.99;
// 		//opt.weight_decay = (value_t)1e-5;
// 		opt.weight_decay = (value_t)0.0;
// 		opt.momentum = (value_t)0.0;

		// 		opt.learning_rate = (value_t)2e-3;
		// 		opt.alpha = (value_t)0.99;
		// 		//opt.weight_decay = (value_t)1e-5;
		// 		opt.weight_decay = (value_t)0.0;
		// 		opt.momentum = (value_t)0.0;

		auto& nets = network.nets;
		size_t max_seq_length = network.seq_length;

		size_t inputs = network.inputs;
		size_t outputs = network.outputs;
		size_t layers = network.layers;
		size_t state_size = network.state_size;
		size_t seq_length = network.seq_length;

		double epoch = 0.0;

		auto unpack = [&](const a_vector<a_vector<history_entry>>& history) {
			a_vector<history_entry> r;
			for (auto& v : history) {
				for (auto& v2 : v) r.push_back(v2);
			}
			return r;
		};

		if (true) {
			input_mean.clear();
			input_mean.resize(inputs);
			input_stddev.resize(inputs);
			a_vector<double> sqsum(inputs);
			a_vector<value_t> minval(inputs, std::numeric_limits<value_t>::infinity());
			a_vector<value_t> maxval(inputs, -std::numeric_limits<value_t>::infinity());
			size_t n = 0;
			for (auto& v : loaded_history) {
				for (auto& v2 : v) {
					if (v2.input.input.size() != inputs) xcept("inputs size mismatch");
					for (size_t i = 0; i < inputs; ++i) {
						input_mean[i] += v2.input.input[i];
						if (v2.input.input[i] < minval[i]) minval[i] = v2.input.input[i];
						if (v2.input.input[i] > maxval[i]) maxval[i] = v2.input.input[i];
					}
					++n;
				}
			}
			for (size_t i = 0; i < inputs; ++i) {
				input_mean[i] /= n;
			}
			for (auto& v : loaded_history) {
				for (auto& v2 : v) {
					for (size_t i = 0; i < inputs; ++i) {
						sqsum[i] += ((double)v2.input.input[i] - (double)input_mean[i]) * ((double)v2.input.input[i] - (double)input_mean[i]);
					}
				}	
			}
			log(log_level_test, "n is %d\n", n);
			for (size_t i = 0; i < inputs; ++i) {
				double variance = sqsum[i] / n;
				input_stddev[i] = (value_t)std::sqrt(variance);
				log(log_level_test, "input %d mean: %g stddev: %g  variance %g min %g max %g\n", i, input_mean[i], input_stddev[i], variance, minval[i], maxval[i]);
			}
			for (auto& v : loaded_history) {
				for (auto& v2 : v) {
					if (v2.input.input.size() != inputs) xcept("inputs size mismatch");
					for (size_t i = 0; i < inputs; ++i) {
						v2.input.input[i] -= input_mean[i];
						if (input_stddev[i]) v2.input.input[i] /= input_stddev[i];
					}
					++n;
				}
			}
		}

		auto train_history = unpack(loaded_history);
		size_t entire_train_history_size = train_history.size();
		//auto train_history = loaded_history;
		//		a_vector<a_vector<history_entry>> train_history = loaded_history;
		if (seq_length + 1 > train_history.size()) {
			log(log_level_test, "history.size() is only %d, %d required for training\n", train_history.size(), seq_length + 1);
			return;
		}
		// 		if (2 > train_history.size()) {
		// 			log(log_level_test, "history.size() is only %d, %d required for training\n", train_history.size(), 2);
		// 			return;
		// 		}

		if (weights.size() != network.a.total_weights) xcept("weights size mismatch");

		double sum = 0.0;
		for (auto& v : nn2::weights) {
			sum += v;
		}
		log(log_level_test, " sum of all weights: %g\n", sum);

		if (true) {
			int t_count = 0;
			int p_count = 0;
			int z_count = 0;
			int r_count = 0;
			for (auto& v2 : train_history) {
				//for (auto& v2 : v) {
				auto t = v2.input.input[56];
				auto p = v2.input.input[57];
				auto z = v2.input.input[58];
				auto r = v2.input.input[59];
				if (t == 1) ++t_count;
				if (p == 1) ++p_count;
				if (z == 1) ++z_count;
				if (r == 1) ++r_count;
				//}
			}
			log(log_level_test, "t %d p %d z %d r %d\n", t_count, p_count, z_count, r_count);
		}

		// 		for (size_t i = 0; i < layers; ++i) {
		// 			for (size_t i2 = 0; i2 < state_size; ++i2) {
		// 				nets[0].hidden_state_input[i][i2] = 0.0;
		// 				nets[0].cell_state_input[i][i2] = 0.0;
		// 			}
		// 		}
		for (size_t i = 0; i < network.layers; ++i) {
			if (initial_state.hidden_state_output[i].size() != network.state_size) xcept("hidden_state_input size mismatch");
			if (initial_state.cell_state_output[i].size() != network.state_size) xcept("cell_state_input size mismatch");
			for (size_t i2 = 0; i2 < network.state_size; ++i2) {
				nets[0].hidden_state_input[i][i2] = initial_state.hidden_state_output[i][i2];
				nets[0].cell_state_input[i][i2] = initial_state.cell_state_output[i][i2];
			}
		}

		for (size_t i = 0; i < layers; ++i) {
			for (size_t i2 = 0; i2 < state_size; ++i2) {
				nets[seq_length - 1].hidden_state_output_gradients[i][i2] = 0.0;
				nets[seq_length - 1].cell_state_output_gradients[i][i2] = 0.0;
			}
		}

		a_vector<value_t> target_output(outputs);
		size_t history_offset = 0;
		double avg = 0.0;

		criterion_cross_entropy<> criterion;

		//for (int iteration = 0; iteration < 1000; ++iteration) {
		for (int iteration = 0;; ++iteration) {

			// 			for (auto& v : nets) {
			// 				for (auto& v2 : v.n.values) v2 = std::numeric_limits<value_t>::signaling_NaN();
			// 			}

			// 			for (size_t i = 0; i != train_history.size(); ++i) {
			// 				if (i == train_history.size() - 1) break;
			// 				if (tsc::rng(1.0) <= 0.5) {
			// 					std::swap(train_history[i], train_history[i + 1]);
			// 				}
			// 			}

			int a = 0;

			tsc::high_resolution_timer ht;

			a_vector<value_t> grad(weights.size());

			auto shuffle = [&](a_vector<a_vector<history_entry>> shuffle_history) {
				if (false) {
					std::normal_distribution<double> dist(0.0, 2);
					for (size_t i = 0; i != shuffle_history.size(); ++i) {
						int add = (int)std::round(dist(tsc::rng_engine));
						size_t ni = i + add;
						if (add < 0 && (size_t)-add > i) ni = 0;
						if (add > 0 && (size_t)add >= shuffle_history.size() - i) ni = shuffle_history.size() - 1;
						if (i != ni) std::swap(shuffle_history[i], shuffle_history[ni]);
					}
				}
				if (false) {
					std::normal_distribution<double> dist(0.0, 4);
					for (size_t gi = 0; gi != shuffle_history.size(); ++gi) {
						auto& game_history = shuffle_history[gi];
						for (size_t i2 = 0; i2 != game_history.size(); ++i2) {
							int add = (int)std::round(dist(tsc::rng_engine));
							size_t ni = i2 + add;
							if (add < 0 && (size_t)-add > i2) ni = 0;
							if (add > 0 && (size_t)add >= game_history.size() - i2) ni = game_history.size() - 1;
							if (i2 != ni) std::swap(game_history[i2], game_history[ni]);
						}
					}
				}
				return unpack(shuffle_history);
			};

			if (true) {
				a_vector<history_entry> r;
				if (loaded_history.size() < 8) xcept("loaded_history too small");
				size_t begin_index = tsc::rng(loaded_history.size() - 8);
				size_t end_index = begin_index + 8;
				a_vector<a_vector<history_entry>> this_history;
				for (size_t i = begin_index; i != end_index; ++i) {
					this_history.push_back(loaded_history[i]);
				}

				train_history = shuffle(std::move(this_history));
				history_offset = 0;
			}
			size_t this_seq_length = train_history.size();

			// 			size_t this_seq_length = train_history.size() - (history_offset + 1);
			// 			if (this_seq_length == 0) {
			// 				history_offset = 0;
			// 				this_seq_length = train_history.size() - (history_offset + 1);
			// 
			// 				if (true) {
			// 					train_history = shuffle(loaded_history);
			// 				}
			// 			}

			// 			if (this_seq_length > seq_length) this_seq_length = seq_length;
			// //if (history_offset + seq_length + 1 > train_history.size()) history_offset -= train_history.size() - (seq_length + 1);
			// if (history_offset + seq_length + 1 > train_history.size()) history_offset = 0;

			value_t loss = 0.0;

			if (this_seq_length > seq_length) xcept("seq_length (%d) is too small, training data requires %d\n", seq_length, this_seq_length);

			epoch += (double)this_seq_length / entire_train_history_size;

			// 				for (size_t i = 0; i < layers; ++i) {
			// 					for (size_t i2 = 0; i2 < state_size; ++i2) {
			// 						nets[0].hidden_state_input[i][i2] = 0.0;
			// 						nets[0].cell_state_input[i][i2] = 0.0;
			// 					}
			// 				}
			size_t wins_n = 0;

			for (size_t n = 0; n < this_seq_length; ++n) {

				auto& cur_net = nets[n];

				auto& history_input = train_history[history_offset].input;
				auto& history_actions = train_history[history_offset].actions;
				bool won = train_history[history_offset].won;
				++history_offset;

				for (size_t i = 0; i < inputs; ++i) {
					cur_net.input[i] = history_input.input[i];
				}

				if (n != 0) {
					auto& prev_net = nets[n - 1];
					for (size_t i = 0; i < layers; ++i) {
						for (size_t i2 = 0; i2 < state_size; ++i2) {
							cur_net.hidden_state_input[i][i2] = prev_net.hidden_state_output[i][i2];
							cur_net.cell_state_input[i][i2] = prev_net.cell_state_output[i][i2];
						}
					}
				}

				cur_net.n.set_training();
				cur_net.n.forward(cur_net.n, weights.data());

				for (size_t i = 0; i < outputs; ++i) {
					if (std::isnan(cur_net.output[i])) xcept("nan output");
				}
				for (size_t i = 0; i < layers; ++i) {
					for (size_t i2 = 0; i2 < state_size; ++i2) {
						if (std::isnan(cur_net.hidden_state_output[i][i2])) xcept("nan output");
						if (std::isnan(cur_net.cell_state_output[i][i2])) xcept("nan output");
					}
				}

				auto set_target = [&](size_t offset, size_t n, int target_index) {
					for (size_t i = 0; i < n; ++i) {
						target_output[i] = target_index == i ? (value_t)1.0 : (value_t)0.0;
					}

					std::array<value_t, 1> loss_arr;

					criterion.forward(n, cur_net.output + offset, target_output.data(), loss_arr.data());

					loss += loss_arr[0];

					criterion.backward(n, cur_net.output + offset, target_output.data(), cur_net.output_gradients + offset);
				};

				for (size_t i = 0; i < outputs; ++i) {
					cur_net.output_gradients[i] = 0.0;
				}

				if (won) {
					++wins_n;
					set_target(0, 2, won ? history_actions.indices[0] : -1);
					set_target(2, 2, won ? history_actions.indices[1] : -1);
					set_target(4, 2, won ? history_actions.indices[2] : -1);
					set_target(6, 2, won ? history_actions.indices[3] : -1);
					set_target(8, 2, won ? history_actions.indices[4] : -1);
					set_target(10, 2, won ? history_actions.indices[5] : -1);
					set_target(12, 2, won ? history_actions.indices[6] : -1);
					set_target(14, 2, won ? history_actions.indices[7] : -1);
					set_target(16, 2, won ? history_actions.indices[8] : -1);
					set_target(18, 2, won ? history_actions.indices[9] : -1);
					set_target(20, 2, won ? history_actions.indices[10] : -1);
					set_target(22, 2, won ? history_actions.indices[11] : -1);
					set_target(24, 2, won ? history_actions.indices[12] : -1);
					set_target(26, 2, won ? history_actions.indices[13] : -1);
					set_target(28, 2, won ? history_actions.indices[14] : -1);
					set_target(30, 3, won ? history_actions.indices[15] : -1);
					set_target(33, 2, won ? history_actions.indices[16] : -1);
					set_target(35, 2, won ? history_actions.indices[17] : -1);
					set_target(37, 2, won ? history_actions.indices[18] : -1);
					set_target(39, 2, won ? history_actions.indices[19] : -1);
				}
				// 				set_target(0, 2, won ? history_actions.indices[0] : -1);
				// 				set_target(2, 2, won ? history_actions.indices[1] : -1);
				// 				if (history_actions.indices[2] != -1) set_target(4, 2, won ? history_actions.indices[2] : -1);

			}

			for (size_t n = this_seq_length; n;) {
				--n;

				auto& cur_net = nets[n];

				if (n != this_seq_length - 1) {
					auto& next_net = nets[n + 1];

					for (size_t i = 0; i < layers; ++i) {
						for (size_t i2 = 0; i2 < state_size; ++i2) {
							cur_net.hidden_state_output_gradients[i][i2] = next_net.hidden_state_input_gradients[i][i2];
							cur_net.cell_state_output_gradients[i][i2] = next_net.cell_state_input_gradients[i][i2];
						}
					}
				} else {
					for (size_t i = 0; i < layers; ++i) {
						for (size_t i2 = 0; i2 < state_size; ++i2) {
							cur_net.hidden_state_output_gradients[i][i2] = 0.0;
							cur_net.cell_state_output_gradients[i][i2] = 0.0;
						}
					}
				}

				cur_net.n.backward(cur_net.n, weights.data(), grad.data());
			}

			if (true) {
				auto& first_net = nets[0];
				auto& last_net = nets[this_seq_length - 1];
				for (size_t i = 0; i < layers; ++i) {
					for (size_t i2 = 0; i2 < state_size; ++i2) {
						first_net.hidden_state_input[i][i2] = last_net.hidden_state_output[i][i2];
						first_net.cell_state_input[i][i2] = last_net.cell_state_output[i][i2];
					}
				}
			}
			if (wins_n == 0) wins_n = 1;
			loss /= wins_n;

// 			for (size_t i = 0; i < weights.size(); ++i) {
// 				value_t& g = grad[i];
// 				g /= wins_n;
// 				if (g < -5.0) g = -5.0;
// 				if (g > 5.0) g = 5.0;
// 			}

			opt(weights.data(), grad.data(), grad.size());

			double t = ht.elapsed() * 1000;

			avg += t;

			//log(log_level_test, "%d: epoch %.02f loss %.02f  avg %gms\n", iteration, epoch, loss, avg / (iteration + 1));
			log(log_level_test, "%d: epoch %.02f loss %g  avg %gms\n", iteration, epoch, loss, avg / (iteration + 1));

			if (iteration % 100 == 0) {
				auto& n = nets[0];
				nn_output output;
				output.output.resize(network.outputs);
				output.hidden_state_output.resize(network.layers);
				output.cell_state_output.resize(network.layers);
				for (size_t i = 0; i < network.layers; ++i) {
					output.hidden_state_output[i].resize(network.state_size);
					output.cell_state_output[i].resize(network.state_size);
					for (size_t i2 = 0; i2 < network.state_size; ++i2) {
						output.hidden_state_output[i][i2] = n.hidden_state_input[i][i2];
						output.cell_state_output[i][i2] = n.cell_state_input[i][i2];
					}
				}
				//save_weights(iteration, output);

				int save_iteration = iteration;
				//if (save_iteration % 10000 != 0) save_iteration += 10000 - (save_iteration % 10000);
				if (save_iteration % 1000 != 0) save_iteration += 1000 - (save_iteration % 1000);

				if (true) {
					data_writer<> w;
					put(w, output.output);
					put(w, output.hidden_state_output);
					put(w, output.cell_state_output);
					write_file(format("tsc-bwai-nn2-state-%d", iteration), w.data(), w.size());
				}

				if (true) {
					data_writer<> w;
					put(w, weights);
					put(w, input_mean);
					put(w, input_stddev);
					write_file(format("tsc-bwai-nn2-weights-%d", iteration), w.data(), w.size());
				}

				// 				auto jv_state = jv_from(output);
				// 				strategy::write_json(format("tsc-bwai-nn2-state-%d.txt", save_iteration), jv_state);
				// 
				// 				auto jv_weights = jv_from(weights);
				// 				strategy::write_json(format("tsc-bwai-nn2-weights-%d.txt", save_iteration), jv_weights);
				//strategy::write_json(format("tsc-bwai-nn2-weights-%d-%.02f.txt", save_iteration, validation_loss), jv_weights);
			}
		}

	}

	uint64_t file_sig = 0xa9da1e2b9531b299;

	void load_history(a_string path) {

		size_t total_size = 0;
		size_t max_size = 0;
		size_t min_size = std::numeric_limits<size_t>::max();
		for (int i = 0;; ++i) {

			//if (i >= 10000) xcept("failed to write stratm output (failed to create file, error %d)", GetLastError());
			if (i >= 10000) break;

			//a_string fn = format("bwapi-data/read/nn2_%s_%04d", strategy::escape_filename(strategy::opponent_name), i);
			//a_string fn = format("bwapi-data/nn2/nn2_%04d", i);
			a_string fn = format("%snn2_%04d", path, i);

			a_string buf;
#ifdef _WIN32
			HANDLE h = CreateFileA(fn.c_str(), GENERIC_READ, 0, nullptr, OPEN_EXISTING, 0, nullptr);
			if (h == INVALID_HANDLE_VALUE) continue;
			buf.resize(GetFileSize(h, nullptr));
			DWORD read;
			if (!ReadFile(h, (void*)buf.data(), buf.size(), &read, nullptr)) {
				CloseHandle(h);
				xcept("failed to read nn2 data (ReadFile failed with error %d)", GetLastError());
			}
			CloseHandle(h);
#else
			xcept("nn2 win only fixme");
#endif

			data_reader<> r((uint8_t*)buf.data(), (uint8_t*)buf.data() + buf.size());
			if (r.get<uint64_t>() == file_sig) {

				bool won = r.get<uint8_t>() ? true : false;

				static_assert(sizeof(float) == sizeof(uint32_t), "float is not 32 bits");

				loaded_history.emplace_back();

				uint32_t history_size = r.get<uint32_t>();
				for (size_t i = 0; i < history_size; ++i) {
					uint32_t input_size = r.get<uint32_t>();
					nn_input inp;
					inp.input.resize(input_size);
					for (size_t i = 0; i < input_size; ++i) {
						auto val = r.get<uint32_t>();
						inp.input[i] = (float&)val;
					}
					uint32_t indices_size = r.get<uint32_t>();
					auto indices = r.get_vec<uint32_t>(indices_size);
					nn_actions actions;
					//actions.indices = std::move(indices);
					actions.indices.resize(indices.size());
					for (size_t i = 0; i < indices.size(); ++i) actions.indices[i] = (size_t)indices[i];

					inp.input.shrink_to_fit();
					actions.indices.shrink_to_fit();

					loaded_history.back().push_back({ std::move(inp), nn_output(), std::move(actions), won });
					//loaded_history.push_back({ std::move(inp), nn_output(), std::move(actions), won });
				}

			} else {

				auto jv = json_parse(buf);

				bool won = jv["won"];
				//if (!won) continue;

				a_vector<history_entry> hist;

				loaded_history.emplace_back();

				auto& jv_data = jv["data"];
				for (size_t i = 0; i < jv_data.vector.size(); ++i) {
					//hist.push_back({ jv_to<nn_input>(jv_data[i]["input"]), jv_to<nn_output>(jv_data[i]["output"]), jv_to<nn_actions>(jv_data[i]["actions"]), won });
					loaded_history.back().push_back({ jv_to<nn_input>(jv_data[i]["input"]), jv_to<nn_output>(jv_data[i]["output"]), jv_to<nn_actions>(jv_data[i]["actions"]), won });
					//loaded_history.push_back({ jv_to<nn_input>(jv_data[i]["input"]), jv_to<nn_output>(jv_data[i]["output"]), jv_to<nn_actions>(jv_data[i]["actions"]), won });
					auto& v = loaded_history.back().back();
					//auto& v = loaded_history.back();
					v.input.input.shrink_to_fit();
					v.input.hidden_state_input.clear();
					v.input.cell_state_input.clear();
					v.output = nn_output();
					v.actions.indices.shrink_to_fit();
				}
			}
		}

		log(log_level_test, "loaded %d history\n", loaded_history.size());

		//log(log_level_test, "size avg %d min %d max %d\n", total_size / loaded_history.size(), min_size, max_size);
	}

#ifndef NN2_TRAIN_ONLY
	void load(nn_output& state) {

		auto jv_state = strategy::read_json(format("bwapi-data/read/tsc-bwai-nn2-%s-state2.txt", strategy::escape_filename(strategy::opponent_name)));
		if (jv_state) jv_to(jv_state, state);

	}

	void save(bool won, const nn_output& state) {

		auto jv_state = jv_from(state);
		strategy::write_json(format("bwapi-data/write/tsc-bwai-nn2-%s-state2.txt", strategy::escape_filename(strategy::opponent_name)), jv_state);

// 		data_writer<> w;
// 		w.put<uint64_t>(file_sig);
// 
// 		w.put<uint8_t>(won ? 1 : 0);
// 		w.put<uint32_t>(history.size());
// 		for (size_t i = 0; i < history.size(); ++i) {
// 			w.put<uint32_t>(history[i].input.input.size());
// 			for (auto& v : history[i].input.input) {
// 				w.put<uint32_t>((uint32_t&)v);
// 			}
// 			w.put<uint32_t>(history[i].actions.indices.size());
// 			for (auto& v : history[i].actions.indices) {
// 				w.put<uint32_t>((uint32_t)v);
// 			}
// 		}
// 
// 		for (int i = 0;; ++i) {
// 
// 			if (i >= 10000) xcept("failed to write nn2 output (failed to create file, error %d)", GetLastError());
// 
// 			a_string fn = format("bwapi-data/read/nn2_%s_%04d", strategy::escape_filename(strategy::opponent_name), i);
// 			//a_string fn = format("nn2/nn2_%04d", i);
// 			//a_string fn = format("bwapi-data/write/nn2_%04d", i);
// 
// 			HANDLE h = CreateFileA(fn.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, 0, nullptr);
// 			if (h == INVALID_HANDLE_VALUE) continue;
// 			CloseHandle(h);
// 			fn = format("bwapi-data/write/nn2_%s_%04d", strategy::escape_filename(strategy::opponent_name), i);
// 			h = CreateFileA(fn.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, 0, nullptr);
// 			if (h == INVALID_HANDLE_VALUE) continue;
// 
// 			//a_string str = jv_history.dump();
// 			auto& str = w;
// 
// 			DWORD written;
// 			if (!WriteFile(h, str.data(), str.size(), &written, nullptr) || written != str.size()) {
// 				CloseHandle(h);
// 				xcept("failed to write nn2 output (WriteFile failed with error %d)", GetLastError());
// 			}
// 
// 			CloseHandle(h);
// 
// 			break;
// 		}
	}
#endif

}
