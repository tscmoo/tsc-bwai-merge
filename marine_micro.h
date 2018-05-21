

template<size_t bits>
using int_fastn_t = typename std::conditional<bits <= 8, std::int_fast8_t,
		typename std::conditional<bits <= 16, int_fast16_t,
				typename std::conditional<bits <= 32, int_fast32_t,
						typename std::enable_if<bits <= 64, int_fast64_t>::type>::type>::type>::type;
template<size_t bits>
using uint_fastn_t = typename std::conditional<bits <= 8, std::uint_fast8_t,
		typename std::conditional<bits <= 16, uint_fast16_t,
				typename std::conditional<bits <= 32, uint_fast32_t,
						typename std::enable_if<bits <= 64, uint_fast64_t>::type>::type>::type>::type;

template<typename T, std::enable_if<std::is_integral<T>::value && std::numeric_limits<T>::radix == 2>* = nullptr>
using int_bits = std::integral_constant<size_t, std::numeric_limits<T>::digits + std::is_signed<T>::value>;

template<typename T>
using is_native_fast_int = std::integral_constant<bool, std::is_integral<T>::value && std::is_literal_type<T>::value && sizeof(T) <= sizeof(void*)>;

template<size_t t_integer_bits, size_t t_fractional_bits, bool t_is_signed, bool t_exact_integer_bits = false>
struct fixed_point {
	static const bool is_signed = t_is_signed;
	static const bool exact_integer_bits = t_exact_integer_bits;
	static const size_t integer_bits = t_integer_bits;
	static const size_t fractional_bits = t_fractional_bits;
	static const size_t total_bits = integer_bits + fractional_bits;
	using raw_unsigned_type = uint_fastn_t<total_bits>;
	using raw_signed_type = int_fastn_t<total_bits>;
	using raw_type = typename std::conditional<is_signed, raw_signed_type, raw_unsigned_type>::type;
	raw_type raw_value;

	using double_size_fixed_point = fixed_point<total_bits * 2 - fractional_bits, fractional_bits, is_signed>;

	void wrap() {
		if (!exact_integer_bits) return;
		raw_value <<= int_bits<raw_type>::value - total_bits;
		raw_value >>= int_bits<raw_type>::value - total_bits;
	}

	static fixed_point from_raw(raw_type raw_value) {
		fixed_point r;
		r.raw_value = raw_value;
		r.wrap();
		return r;
	}

	template<typename T, typename std::enable_if<std::is_integral<T>::value>::type* = nullptr>
	static fixed_point integer(T integer_value) {
		return from_raw((raw_type)integer_value << fractional_bits);
	}

	static fixed_point zero() {
		return integer(0);
	}
	static fixed_point one() {
		return integer(1);
	}

	raw_type integer_part() const {
		return raw_value >> fractional_bits;
	}
	raw_type fractional_part() const {
		return raw_value & (((raw_type)1 << fractional_bits) - 1);
	}

	template<size_t n_integer_bits, size_t n_fractional_bits, bool n_exact_integer_bits, size_t result_integer_bits = integer_bits, size_t result_fractional_bits = fractional_bits, typename std::enable_if<((result_integer_bits < n_integer_bits || result_fractional_bits < n_fractional_bits) && result_fractional_bits <= n_fractional_bits)>::type* = nullptr>
	static auto truncate(const fixed_point<n_integer_bits, n_fractional_bits, is_signed, n_exact_integer_bits>& n) {
		using result_type = fixed_point<result_integer_bits, result_fractional_bits, is_signed, exact_integer_bits>;
		typename result_type::raw_type raw_value = (typename result_type::raw_type)(n.raw_value >> (n_fractional_bits - result_fractional_bits));
		return result_type::from_raw(raw_value);
	}

	template<size_t n_integer_bits, size_t n_fractional_bits, bool n_exact_integer_bits, size_t result_integer_bits = integer_bits, size_t result_fractional_bits = fractional_bits, typename std::enable_if<((result_integer_bits > n_integer_bits || result_fractional_bits > n_fractional_bits) && result_fractional_bits >= n_fractional_bits)>::type* = nullptr>
	static auto extend(const fixed_point<n_integer_bits, n_fractional_bits, is_signed, n_exact_integer_bits>& n) {
		using result_type = fixed_point<result_integer_bits, result_fractional_bits, is_signed, exact_integer_bits>;
		typename result_type::raw_type raw_value = n.raw_value;
		raw_value <<= result_fractional_bits - n_fractional_bits;
		return result_type::from_raw(raw_value);
	}

	fixed_point floor() const {
		return integer(integer_part());
	}
	fixed_point ceil() const {
		return (*this + integer(1) - from_raw(1)).floor();
	}
	fixed_point abs() const {
		if (*this >= zero()) return *this;
		else return from_raw(-raw_value);
	}

	auto as_signed() const {
		return fixed_point<integer_bits, fractional_bits, true, exact_integer_bits>::from_raw(raw_value);
	}
	auto as_unsigned() const {
		return fixed_point<integer_bits, fractional_bits, false, exact_integer_bits>::from_raw(raw_value);
	}

	bool operator==(const fixed_point& n) const {
		return raw_value == n.raw_value;
	}
	bool operator!=(const fixed_point& n) const {
		return raw_value != n.raw_value;
	}
	bool operator<(const fixed_point& n) const {
		return raw_value < n.raw_value;
	}
	bool operator<=(const fixed_point& n) const {
		return raw_value <= n.raw_value;
	}
	bool operator>(const fixed_point& n) const {
		return raw_value > n.raw_value;
	}
	bool operator>=(const fixed_point& n) const {
		return raw_value >= n.raw_value;
	}

	fixed_point operator-() const {
		static_assert(is_signed, "fixed_point: cannot negate an unsigned number");
		return from_raw(-raw_value);
	}

	fixed_point& operator+=(const fixed_point& n) {
		raw_value += n.raw_value;
		wrap();
		return *this;
	}
	fixed_point operator+(const fixed_point& n) const {
		return from_raw(raw_value + n.raw_value);
	}
	fixed_point& operator-=(const fixed_point& n) {
		raw_value -= n.raw_value;
		wrap();
		return *this;
	}
	fixed_point operator-(const fixed_point& n) const {
		return from_raw(raw_value - n.raw_value);
	}

	template<typename T, typename std::enable_if<std::is_integral<T>::value>::type* = nullptr>
	fixed_point& operator/=(T integer_value) {
		static_assert(std::is_signed<T>::value == is_signed, "fixed_point: cannot mix signed/unsigned in division");
		raw_value /= integer_value;
		wrap();
		return *this;
	}
	template<typename T, typename std::enable_if<std::is_integral<T>::value>::type* = nullptr>
	fixed_point operator/(T integer_value) const {
		static_assert(std::is_signed<T>::value == is_signed, "fixed_point: cannot mix signed/unsigned in division");
		return from_raw(raw_value / integer_value);
	}
	template<typename T, typename std::enable_if<std::is_integral<T>::value>::type* = nullptr>
	fixed_point& operator*=(T integer_value) {
		static_assert(std::is_signed<T>::value == is_signed, "fixed_point: cannot mix signed/unsigned in multiplication");
		raw_value *= integer_value;
		wrap();
		return *this;
	}
	template<typename T, typename std::enable_if<std::is_integral<T>::value>::type* = nullptr>
	auto operator*(T integer_value) const {
		static_assert(std::is_signed<T>::value == is_signed, "fixed_point: cannot mix signed/unsigned in multiplication");
		return from_raw(raw_value * integer_value);
	}

	template<size_t n_integer_bits>
	fixed_point& operator*=(const fixed_point<n_integer_bits, fractional_bits, is_signed, exact_integer_bits>& n) {
		return *this = *this * n;
	}

	template<size_t n_integer_bits>
	fixed_point& operator/=(const fixed_point<n_integer_bits, fractional_bits, is_signed, exact_integer_bits>& n) {
		return *this = *this / n;
	}

	// rounds towards negative infinity
	template<size_t n_integer_bits, size_t n_fractional_bits>
	auto operator*(const fixed_point<n_integer_bits, n_fractional_bits, is_signed, exact_integer_bits>& n) {
		using result_type = fixed_point<(integer_bits > n_integer_bits ? integer_bits : n_integer_bits), (fractional_bits > n_fractional_bits ? fractional_bits : n_fractional_bits), is_signed, exact_integer_bits>;
		using tmp_t = typename fixed_point<result_type::integer_bits, fractional_bits + n_fractional_bits, is_signed>::raw_type;
		tmp_t tmp = (tmp_t)raw_value * (tmp_t)n.raw_value >> n_fractional_bits;
		return result_type::from_raw((typename result_type::raw_type)tmp);
	}

	// rounds towards 0
	template<size_t n_integer_bits, size_t n_fractional_bits>
	auto operator/(const fixed_point<n_integer_bits, n_fractional_bits, is_signed, exact_integer_bits>& n) const {
		using result_type = fixed_point<(integer_bits > n_integer_bits ? integer_bits : n_integer_bits), (fractional_bits > n_fractional_bits ? fractional_bits : n_fractional_bits), is_signed, exact_integer_bits>;
		using tmp_t = typename fixed_point<result_type::integer_bits, fractional_bits + n_fractional_bits, is_signed>::raw_type;
		tmp_t tmp = ((tmp_t)raw_value << n_fractional_bits) / (tmp_t)n.raw_value;
		return result_type::from_raw((typename result_type::raw_type)tmp);
	}

	// returns a * b / c
	// rounds towards 0
	static fixed_point multiply_divide(fixed_point a, fixed_point b, fixed_point c) {
		constexpr raw_type max_value_no_overflow = std::numeric_limits<raw_type>::max() >> (int_bits<raw_type>::value / 2);
		using tmp_t = typename fixed_point<integer_bits, fractional_bits + fractional_bits, is_signed>::raw_type;
		if (!is_native_fast_int<tmp_t>::value && a.raw_value <= max_value_no_overflow && b.raw_value <= max_value_no_overflow) {
			return from_raw(a.raw_value * b.raw_value / c.raw_value);
		} else {
			return from_raw((tmp_t)a.raw_value * b.raw_value / c.raw_value);
		}
	}

	// returns a / b * c
	// rounds towards 0
	static fixed_point divide_multiply(fixed_point a, fixed_point b, fixed_point c) {
		return from_raw(a.raw_value / b.raw_value * c.raw_value);
	}

};

using fp8 = fixed_point<24, 8, true>;
using ufp8 = fixed_point<24, 8, false>;

using direction_t = fixed_point<0, 8, true, true>;

using xy_fp8 = xy_typed<fp8>;

static const std::array<xy_fp8, 256> direction_table = []() {
	std::array<xy_fp8, 256> direction_table;
	// This function returns (int)std::round(std::sin(PI / 128 * i) * 256) for i [0, 63]
	// using only integer arithmetic.
	auto int_sin = [&](int x) {
		int x2 = x * x;
		int x3 = x2 * x;
		int x4 = x3 * x;
		int x5 = x4 * x;

		int64_t a0 = 26980449732;
		int64_t a1 = 1140609;
		int64_t a2 = -2785716;
		int64_t a3 = 2159;
		int64_t a4 = 58;

		return (int)((x * a0 + x2 * a1 + x3 * a2 + x4 * a3 + x5 * a4 + ((int64_t)1 << 31)) >> 32);
	};

	// The sin lookup table is hardcoded into Broodwar. We generate it here.
	for (int i = 0; i <= 64; ++i) {
		auto v = fp8::from_raw(int_sin(i));
		direction_table[i].x = v;
		direction_table[64 - i].y = -v;
		direction_table[64 + (64 - i)].x = v;
		direction_table[64 + i].y = v;
		direction_table[128 + i].x = -v;
		direction_table[128 + (64 - i)].y = v;
		direction_table[(192 + (64 - i)) % 256].x = -v;
		direction_table[(192 + i) % 256].y = -v;
	}

	return direction_table;
}();

static const std::array<unsigned int, 64> arctan_inv_table = {
		7, 13, 19, 26, 32, 38, 45, 51, 58, 65, 71, 78, 85, 92,
		99, 107, 114, 122, 129, 137, 146, 154, 163, 172, 181,
		190, 200, 211, 221, 233, 244, 256, 269, 283, 297, 312,
		329, 346, 364, 384, 405, 428, 452, 479, 509, 542, 578,
		619, 664, 716, 775, 844, 926, 1023, 1141, 1287, 1476,
		1726, 2076, 2600, 3471, 5211, 10429, std::numeric_limits<unsigned int>::max()
};

int xy_length(xy vec) {
	unsigned int x = std::abs(vec.x);
	unsigned int y = std::abs(vec.y);
	if (x < y) std::swap(x, y);
	if (x / 4 < y) x = x - x / 16 + y * 3 / 8 - x / 64 + y * 3 / 256;
	return x;
}

fp8 xy_length(xy_fp8 vec) {
	return fp8::from_raw(xy_length({vec.x.raw_value, vec.y.raw_value}));
}

xy_fp8 to_xy_fp8(xy position) {
	return {fp8::integer(position.x), fp8::integer(position.y)};
}
xy to_xy(xy_fp8 position) {
	return {position.x.integer_part(), position.y.integer_part()};
}

template<size_t integer_bits>
direction_t atan(fixed_point<integer_bits, 8, true> x) {
	bool negative = x < decltype(x)::zero();
	if (negative) x = -x;
	int r;
	if ((typename decltype(x)::raw_unsigned_type)x.raw_value > std::numeric_limits<unsigned int>::max()) r = 64;
	else r = std::lower_bound(arctan_inv_table.begin(), arctan_inv_table.end(), (unsigned int)x.raw_value) - arctan_inv_table.begin();
	return negative ? -direction_t::from_raw(r) : direction_t::from_raw(r);
};

direction_t xy_direction(xy_fp8 pos) {
	if (pos.x == fp8::zero()) return pos.y <= fp8::zero() ? direction_t::zero() : direction_t::from_raw(-128);
	direction_t r = atan(pos.y / pos.x);
	if (pos.x > fp8::zero()) r += direction_t::from_raw(64);
	else r = direction_t::from_raw(-64) + r;
	return r;
}

direction_t xy_direction(xy pos) {
	if (pos.x == 0) return pos.y <= 0 ? direction_t::zero() : direction_t::from_raw(-128);
	direction_t r = atan(fp8::integer(pos.y) / pos.x);
	if (pos.x > 0) r = direction_t::from_raw(64) + r;
	else r = direction_t::from_raw(-64) + r;
	return r;
}

size_t direction_index(direction_t dir) {
	auto v = dir.fractional_part();
	if (v < 0) return 256 + v;
	else return v;
}

xy_fp8 direction_xy(direction_t dir, fp8 length) {
	return direction_table[direction_index(dir)] * length;
}

xy_fp8 direction_xy(direction_t dir) {
	return direction_table[direction_index(dir)];
}

direction_t direction_from_index(size_t index) {
	int v = index;
	if (v >= 128) v = -(256 - v);
	return direction_t::from_raw(v);
}


namespace marine_micro {

	struct target_group: refcounted {
		a_vector<unit*> targets;
		a_vector<int> costgrid;
		int range;
	};

	struct control {
		refcounter<combat::combat_unit> h = nullptr;
		combat::combat_unit* a = nullptr;
		unit* u = nullptr;
		xy dodge_to;
		int dodge_to_reach_frame = 0;
		a_deque<xy> moving_to;

		enum {order_stop, order_move, order_attack};
		int order = order_stop;
		xy order_target_pos;
		unit* order_target = nullptr;
		int order_frame = 0;
		int order_command_frame = 0;
		int next_order = order_stop;
		xy next_order_target_pos;
		unit* next_order_target = nullptr;

		int stim_active_until = 0;
		int next_use_stim = 0;

		xy pred_pos;
		direction_t pred_heading;

		fp8 top_speed;
		fp8 turn_rate;

		int move_priority = 0;
		int re_move_priority = 0;
		xy pred_4_pos;

		refcounter<target_group> g_h = nullptr;
		target_group* g = nullptr;
		target_group* next_g = nullptr;
	};

	a_list<control> all;

	xy restrict_to_map_bounds(xy pos) {
		if (pos.x < 0) pos.x = 0;
		if (pos.y < 0) pos.y = 0;
		if (pos.x >= grid::map_width) pos.x = grid::map_width - 1;
		if (pos.y >= grid::map_height) pos.y = grid::map_height - 1;
		return pos;
	};

	template<typename T>
	auto rotate(T point, T rot) {
		return T{point.x * rot.x + point.y * -rot.y, point.x * rot.y + point.y * rot.x};
	}

	struct lurker_attack {
		unit* source;
		bool from_bullet;
		int start_frame;
		xy from;
		xy to;
		std::array<xy, 6> spines;
	};

	a_vector<lurker_attack> lurker_attacks;

	void advance_move(xy_fp8& pos, direction_t& heading, xy_fp8 target, fp8 top_speed, fp8 turn_rate) {
		direction_t desired_heading = xy_direction(target - pos);
		direction_t heading_error = desired_heading - heading;
		fp8 turn = fp8::extend(heading_error);
		if (turn > turn_rate) turn = turn_rate;
		else if (turn < -turn_rate) turn = -turn_rate;
		heading += direction_t::truncate(turn);
		if (heading_error.abs() < direction_t::from_raw(32)) {
			pos += direction_xy(heading, top_speed);
		}
	}

	void update_costgrid(target_group& g) {

		g.costgrid.clear();
		g.costgrid.resize(grid::build_grid_width * grid::build_grid_height);

		using open_t = std::pair<xy, int>;
		struct cmp_t {
			bool operator()(const open_t& a, const open_t& b) const {
				return a.second > b.second;
			}
		};
		std::priority_queue<open_t, a_vector<open_t>, cmp_t> open;

		tsc::dynamic_bitset is_in_range(grid::build_grid_width * grid::build_grid_height);

		auto add_open = [&](xy pos, int value) {
			size_t index = grid::build_square_index(pos);
			//s.costgrid[index] = value;
			open.emplace(pos, value);
		};

		int max_range = g.range;

		a_deque<grid::build_square*> range_open;
		tsc::dynamic_bitset range_visited(grid::build_grid_width * grid::build_grid_height);
		for (unit* u : g.targets) {
			range_visited.reset();
			xy pos = u->pos;

			//add_open(pos, 1);

			auto* start_bs = &grid::get_build_square(pos);
			range_open.push_back(start_bs);
			range_visited.set(grid::build_square_index(*start_bs));
			while (!range_open.empty()) {
				auto* bs = range_open.front();
				range_open.pop_front();
				for (int i = 0; i != 4; ++i) {
					auto* n = bs->get_neighbor(i);
					if (n && n->entirely_walkable && !n->building) {
						size_t index = grid::build_square_index(*n);
						if (range_visited.test(index)) continue;
						range_visited.set(index);
						int d = sc_distance(pos - n->pos);
						if (d >= max_range) {
							add_open(n->pos, 1);
						} else {
							is_in_range.set(index);
							range_open.push_back(n);
						}
					}
				}
			}
		}
		int iterations = 0;
		while (!open.empty()) {
			++iterations;
			if (iterations % 1024 == 0) multitasking::yield_point();
			auto cur = open.top();
			open.pop();
			auto& bs = grid::get_build_square(cur.first);
			if (is_in_range.test(grid::build_square_index(bs))) continue;
			auto add = [&](xy npos) {
				if ((size_t)npos.x >= grid::map_width || (size_t)npos.y >= grid::map_height) return;
				auto* n = &grid::get_build_square(npos);
				size_t index = grid::build_square_index(*n);
				auto& cost = g.costgrid[index];
				//if (cost == 0 && !is_in_range.test(index)) {
				if (cost == 0 && n->entirely_walkable && !n->building) {
					cost = cur.second + 1;
					open.emplace(n->pos, cur.second + 1);
				}
			};
			add(bs.pos + xy(32, 0));
			add(bs.pos + xy(32, 32));
			add(bs.pos + xy(0, 32));
			add(bs.pos + xy(-32, 32));
			add(bs.pos + xy(-32, 0));
			add(bs.pos + xy(-32, -32));
			add(bs.pos + xy(0, -32));
			add(bs.pos + xy(32, -32));
		}
	}

	void prepare_move(control& s) {
		unit* u = s.u;

		if (!s.moving_to.empty()) s.moving_to.pop_front();

		while (s.moving_to.size() < latency_frames) {
			if (!s.moving_to.empty()) s.moving_to.push_back(s.moving_to.back());
			else s.moving_to.push_back(u->pos);
		}

		if (current_frame >= s.order_command_frame && u->pos == u->prev_pos) {
			if (current_frame - s.order_frame >= 15) {
				for (auto& v : s.moving_to) v = u->pos;
			}
		}

		fp8 top_speed = fp8::from_raw((int)(u->stats->max_speed * 256));
		if (current_command_frame <= s.stim_active_until) top_speed += top_speed / fp8::integer(2);
		fp8 turn_rate = fp8::from_raw((int)(u->stats->turn_rate / PI * 128));
		s.top_speed = top_speed;
		s.turn_rate = turn_rate;

		xy_fp8 my_pos = to_xy_fp8(u->pos);
		direction_t my_heading = direction_t::from_raw((direction_t::raw_type)((u->heading >= PI ? u->heading - PI * 2 : u->heading) / PI * 128));

		for (xy dst : s.moving_to) {
			if (to_xy(my_pos) == dst) continue;
			advance_move(my_pos, my_heading, to_xy_fp8(dst), top_speed, turn_rate);
		}

		s.pred_pos = to_xy(my_pos);
		s.pred_heading = my_heading;

		s.move_priority = 0;
		s.re_move_priority = 0;
		s.pred_4_pos = s.pred_pos;

		if (s.next_g) {
			s.g_h = refcounter<target_group>(*s.next_g);
			s.g = s.next_g;
		} else {
			s.g_h = nullptr;
			s.g = nullptr;
		}

	}

	int last_scan = 0;

	void move_unit(control& s, xy move_to, int move_priority) {
		unit* u = s.u;
		int order = control::order_stop;
		xy order_target_pos;
		unit* order_target = nullptr;

		s.move_priority = move_priority;
		if (move_to == s.pred_pos) {
			order = control::order_stop;
			order_target_pos = xy();
			order_target = nullptr;
		} else {
			order = control::order_move;
			order_target_pos = move_to;
			order_target = nullptr;

			direction_t h = s.pred_heading;
			xy_fp8 pos = to_xy_fp8(s.pred_pos);
			for (size_t i = 0; i != 4; ++i) advance_move(pos, h, to_xy_fp8(move_to), s.top_speed, s.turn_rate);
			s.pred_4_pos = to_xy(pos);
		}

		s.next_order = order;
		s.next_order_target_pos = order_target_pos;
		s.next_order_target = order_target;

		if (!u->is_flying) {
			for (auto& ns : all) {
				if (&ns == &s) continue;
				if (ns.u->is_flying) continue;
				if (ns.move_priority == s.move_priority && s.re_move_priority >= ns.move_priority) continue;
				if (sc_distance(s.pred_pos - ns.pred_pos) >= 128) continue;
				bool collision = false;
				if (units_distance(s.pred_pos, u->type, ns.pred_pos, ns.u->type) <= 4) collision = true;
				if (!collision && units_distance(s.pred_4_pos, u->type, ns.pred_pos, ns.u->type) <= 4) collision = true;
				if (!collision && units_distance(s.pred_pos, u->type, ns.pred_4_pos, ns.u->type) <= 4) collision = true;
				if (!collision && units_distance(s.pred_4_pos, u->type, ns.pred_4_pos, ns.u->type) <= 4) collision = true;
				if (collision) {
					//log(log_level_test, "detected collision between %s and %s\n", u->type->name, ns.u->type->name);
					if (ns.move_priority >= s.move_priority) {
						s.re_move_priority = ns.move_priority;
						xy dst = s.pred_pos - to_xy(direction_xy(xy_direction(ns.pred_pos - s.pred_pos), fp8::integer(32)));
						move_unit(s, dst, ns.move_priority);
					} else {
						ns.re_move_priority = s.move_priority;
						xy dst = ns.pred_pos - to_xy(direction_xy(xy_direction(s.pred_pos - ns.pred_pos), fp8::integer(32)));
						move_unit(ns, dst, s.move_priority);
					}
				}
			}
		}
	}

	void move(control& s) {
		unit* u = s.u;

		s.a->subaction = combat::combat_unit::subaction_idle;

		s.a->strategy_busy_until = current_frame + 15;
		s.a->subaction = combat::combat_unit::subaction_idle;

		if (u->type != unit_types::medic && u->type != unit_types::marine) return;

		if (u->type == unit_types::marine || u->type == unit_types::firebat) {
			if (s.next_use_stim && current_command_frame >= s.next_use_stim) {
				if (u->hp > 10 || u->stim_timer) {
					s.stim_active_until = s.next_use_stim + 220;
					s.next_use_stim = 0;
				}
			}
		}

		xy my_pos = s.pred_pos;
		direction_t my_heading = s.pred_heading;

		game->drawLineMap(u->pos.x, u->pos.y, my_pos.x, my_pos.y, BWAPI::Colors::Teal);

		xy move_to = my_pos;

		int spine_radius = 20 + 1;

		int min_distance = spine_radius + sc_distance(xy{(int)u->type->width, (int)u->type->width}) / 2 + 4;

		auto lurker_attack_at = [&](xy pos) -> lurker_attack* {
			xy upper_left = pos - xy(u->type->dimension_left(), u->type->dimension_up());
			xy bottom_right = pos + xy(u->type->dimension_right(), u->type->dimension_down());
			lurker_attack* r = nullptr;
			int lurker_attack_frame;
			for (auto& v : lurker_attacks) {
				auto attack_bb_from = v.from;
				auto attack_bb_to = v.to;
				if (attack_bb_to.x < attack_bb_from.x) std::swap(attack_bb_from.x, attack_bb_to.x);
				if (attack_bb_to.y < attack_bb_from.y) std::swap(attack_bb_from.y, attack_bb_to.y);
				attack_bb_from -= xy(20, 20);
				attack_bb_to += xy(20, 20);
				if (sc_distance(rectangles_difference(upper_left, bottom_right, attack_bb_from, attack_bb_to)) <= spine_radius) {
					for (size_t i = 0; i < v.spines.size(); ++i) {
						if (sc_distance(nearest_spot_in_square(v.spines[i], upper_left, bottom_right) - v.spines[i]) <= spine_radius) {
							int f = v.start_frame + 1 + (int)i * 2;
							if (r == nullptr || f < lurker_attack_frame) {
								lurker_attack_frame = f;
								r = &v;
							}
							break;
						}
					}
				}
			}
			return r;
		};

		for (size_t i = 0; i < lurker_attacks.size(); ++i) {
			auto& v = lurker_attacks[i];
			xy line = v.to - v.from;
			//double a = std::abs(line.y * u->pos.x - line.x * u->pos.y + v.to.x * v.from.y - v.to.y * v.from.x);
			//double d = a / sc_distance(line);
			//log(log_level_test, " distance from lurker attack %d is %g\n", i, d);
			int nearest_spine_distance = 1000;
			xy upper_left = u->pos - xy(u->type->dimension_left(), u->type->dimension_up());
			xy bottom_right = u->pos + xy(u->type->dimension_right(), u->type->dimension_down());
			for (size_t i2 = 0; i2 != 6; ++i2) {
				auto& pos = v.spines[i2];
				//log(log_level_test, " spine %d is at %d %d\n", i2, pos.x, pos.y);
				int d = sc_distance(nearest_spot_in_square(pos, upper_left, bottom_right) - pos);
				if (d < nearest_spine_distance) nearest_spine_distance = d;
			}
			//log(log_level_test, " nearest spine distance %d\n", nearest_spine_distance);
		}

		auto get_dodge_pos = [&](xy dpos) {

			int best_wait_n;
			xy best_move;

			auto get_turn_frames = [&](direction_t heading, direction_t a) {
				direction_t da = (heading - a).abs();
				if (da < direction_t::from_raw(32)) return 0;
				return (fp8::extend(da) / s.turn_rate).ceil().integer_part();
			};

			std::function<void(int, direction_t, xy, xy, int, int)> visit = [&](int depth, direction_t heading, xy vpos, xy first_move, int frame, int wait_n) {

				if (!square_pathing::can_fit_at(vpos, u->type->dimensions)) return;

				//log(log_level_test, "visit - depth %d pos %d %d first_move %d %d frame %d wait_n %d\n", depth, vpos.x, vpos.y, first_move.x, first_move.y, frame, wait_n);
				auto* la = depth < 2 ? lurker_attack_at(vpos) : nullptr;

				if (la) {
					xy line = la->to - la->from;

					xy pline = rotate(line, xy(0, 1));
					double x1 = la->from.x;
					double y1 = la->from.y;
					double x2 = la->to.x;
					double y2 = la->to.y;
					double x3 = vpos.x;
					double y3 = vpos.y;
					double x4 = x3 + pline.x;
					double y4 = y3 + pline.y;

					xy intersect;
					intersect.x = (int)(((x1 * y2 - y1 * x2) * (x3 - x4) - (x1 - x2) * (x3 * y4 - y3 * x4)) / ((x1 - x2) * (y3 - y4) - (y1 - y2) * (x3 - x4)));
					intersect.y = (int)(((x1 * y2 - y1 * x2) * (y3 - y4) - (y1 - y2) * (x3 * y4 - y3 * x4)) / ((x1 - x2) * (y3 - y4) - (y1 - y2) * (x3 - x4)));

					//log(log_level_test, "intersect at %d %d\n", intersect.x, intersect.y);
					game->drawLineMap(vpos.x, vpos.y, intersect.x, intersect.y, BWAPI::Colors::Green);

					xy relpos = intersect - vpos;
					int d = sc_distance(relpos);
					//log(log_level_test, " distance is %d\n", d);

					fp8 bullet_speed = fp8::from_raw(256 * 18 + 192);

					//int bullet_travel_f = (int)std::ceil((sc_distance(intersect - la->from) - min_distance) / 18.75) - (frame - la->start_frame);
					int bullet_travel_f = ((xy_length(to_xy_fp8(intersect) - to_xy_fp8(la->from)) - fp8::integer(min_distance)) / bullet_speed).ceil().integer_part() - (frame - la->start_frame);

					//log(log_level_test, "bullet_travel_f %d\n", bullet_travel_f);

					//int min_escape_frames = (int)std::ceil(min_distance / u->stats->max_speed);
					//int max_escape_frames = (int)std::ceil(PI * 2 / u->stats->turn_rate + min_distance / u->stats->max_speed);
					int min_escape_frames = (fp8::integer(min_distance) / s.top_speed).ceil().integer_part();

					auto add_escape = [&](xy_typed<double> pu) {

						xy pos{intersect.x + (int)pu.x, intersect.y + (int)pu.y};
						while (lurker_attack_at(pos) == la) {
							pu *= 1.10;
							pos = xy{intersect.x + (int)pu.x, intersect.y + (int)pu.y};
						}

						game->drawLineMap(vpos.x, vpos.y, pos.x, pos.y, BWAPI::Colors::Blue);

						xy relpos = pos - vpos;
						int distance = sc_distance(relpos);
						direction_t a = xy_direction(relpos);
						int turn_f = get_turn_frames(heading, a);
						int move_f = (fp8::integer(distance) / s.top_speed).ceil().integer_part();

						int escape = 1 + turn_f + move_f;

						int wait = bullet_travel_f - escape;

						//log(log_level_test, "escape %d %d distance %d turn_f %d move_f %d escape %d wait %d\n", pos.x, pos.y, distance, turn_f, move_f, escape, wait);

						visit(depth + 1, a, pos, first_move == xy() ? pos : first_move, frame + escape, std::min(wait, wait_n));

					};


					xy_typed<double> pu(pline.x, pline.y);
					pu /= pu.length();

					pu *= min_distance;
					pu.x = std::ceil(pu.x);
					pu.y = std::ceil(pu.y);
					add_escape(pu);
					add_escape(-pu);
//
//					add_escape({intersect.x + (int)pu.x, intersect.y + (int)pu.y});
//					add_escape({intersect.x - (int)pu.x, intersect.y - (int)pu.y});

				} else if (first_move != xy()) {
					//log(log_level_test, "end - depth %d pos %d %d first_move %d %d frame %d wait_n %d\n", depth, vpos.x, vpos.y, first_move.x, first_move.y, frame, wait_n);
					if (best_move == xy() || wait_n > best_wait_n) {
						best_wait_n = wait_n;
						best_move = first_move;
					}
				}
			};

			if (dpos != my_pos) {
				xy relpos = dpos - my_pos;
				int distance = sc_distance(relpos);
				direction_t a = xy_direction(relpos);
				int turn_f = get_turn_frames(my_heading, a);
				int move_f = (fp8::integer(distance) / s.top_speed).ceil().integer_part();
				visit(0, a, dpos, dpos, current_frame + latency_frames + turn_f + move_f, 100);
				//log(log_level_test, " best_wait_n %d\n", best_wait_n);
				if (best_wait_n > 2) return dpos;
			}

			visit(0, my_heading, my_pos, xy(), current_frame + latency_frames, 100);

			//log(log_level_test, " best move is %d %d\n", best_move.x, best_move.y);
			if (best_move == my_pos || best_move == xy()) return dpos;
			//log(log_level_test, " best_wait_n %d\n", best_wait_n);
			if (best_wait_n > 2) return dpos;

			if (s.order == control::order_move && s.order_target_pos != my_pos && s.order_target_pos != dpos) {
				xy relpos = s.order_target_pos - my_pos;
				int distance = sc_distance(relpos);
				direction_t a = xy_direction(relpos);
				int turn_f = get_turn_frames(my_heading, a);
				int move_f = (fp8::integer(distance) / s.top_speed).ceil().integer_part();
				visit(0, a, s.order_target_pos, s.order_target_pos, current_frame + latency_frames + turn_f + move_f, 100);
				//log(log_level_test, " best_wait_n %d\n", best_wait_n);
				if (best_wait_n > 2) return s.order_target_pos;
			}
			//if (best_wait_n < 0) return dpos;
			return best_move;
		};

		unit* attack_target = nullptr;
		if (move_to == my_pos && s.g) {

			auto* sq = &grid::get_build_square(my_pos);
			auto* initial_sq = sq;
			int sq_cost = s.g->costgrid[grid::build_square_index(*sq)];
			for (int i = 0; i < 2; ++i) {
				grid::build_square* best_n = sq;
				int best_cost = std::numeric_limits<int>::max();
				for (int i2 = 0; i2 != 4; ++i2) {
					auto* n = sq->get_neighbor(i2);
					if (n) {
						int cost = s.g->costgrid[grid::build_square_index(*n)];
						if (cost && cost < best_cost) {
							best_cost = cost;
							best_n = n;
						}
					}
				}
				sq = best_n;
				sq_cost = best_cost;
			}
			move_to = sq->pos + xy(16, 16);

			//log(log_level_test, "%s sq_cost %d\n", u->type->name, sq_cost);

			if (sq_cost <= 3) {
				unit* e = nullptr;
				if (u->type == unit_types::medic) {
					e = get_best_score(my_units, [&](unit* e) {
						if (e == u) return inf;
						if (!e->type->is_biological) return inf;
						if (e->hp >= e->stats->hp) return inf;
						double d = units_distance(e->pos, e->type, my_pos, u->type);
						if (d >= 32 * 6) return inf;
						return d;
					}, inf);
				} else {
					e = get_best_score(s.g->targets, [&](unit* e) {
						if (e->cloaked && !e->detected && (combat::scans_available == 0 || current_frame - last_scan < 30)) return inf;
						if (!e->visible) return inf;
						return units_distance(e->pos, e->type, my_pos, u->type);
					}, inf);
				}
				if (e) {
					if (e->cloaked && !e->detected && current_frame - last_scan >= 30) {
						last_scan = current_frame;
						unit* u = get_best_score(my_units_of_type[unit_types::comsat_station], [&](unit* u) {
							return -u->energy;
						});
						if (u) {
							u->game_unit->useTech(BWAPI::TechTypes::Scanner_Sweep, BWAPI::Position(e->pos.x, e->pos.y));
						}
					}
					//auto* w = e->is_flying ? u->stats->air_weapon : u->stats->ground_weapon;
					double d = units_distance(e->pos, e->type, my_pos, u->type);
					//if (w && d <= w->max_range) {
					if (d <= s.g->range) {
						attack_target = e;
						//if (d <= s.g->range - 32 && e->owner->is_enemy && (u->is_flying ? e->stats->air_weapon : e->stats->ground_weapon)) {
						if (d <= s.g->range - 32 && e->owner->is_enemy && (u->is_flying ? e->stats->air_weapon : e->stats->ground_weapon) && (e->type == unit_types::lurker || fp8::from_raw((int)(e->stats->max_speed * 256)) < s.top_speed)) {
							if (sq == initial_sq) {
								move_to = my_pos - to_xy(direction_xy(xy_direction(e->pos - my_pos), fp8::integer(32)));
							}
						} else {
							move_to = my_pos;
						}
					} else {
						xy pos = my_pos + to_xy(direction_xy(xy_direction(e->pos - my_pos), fp8::integer(std::min((int)d - s.g->range, 32))));
						if (square_pathing::can_fit_at(pos, u->type->dimensions)) move_to = pos;
					}
				}
			}
		}

		bool is_dodging = false;
		xy pre_dodge_move_to = move_to;
		move_to = get_dodge_pos(move_to);
		if (move_to != pre_dodge_move_to) is_dodging = true;

		int move_priority = 0;
		if (move_to != my_pos) move_priority = is_dodging ? 2 : 1;
		move_unit(s, move_to, move_priority);

		if (s.next_order == control::order_stop) {
			if (attack_target) {
				s.next_order = control::order_attack;
				s.next_order_target_pos = xy();
				s.next_order_target = attack_target;
			}
		}

		if (is_dodging || attack_target) {
			if (players::my_player->has_upgrade(upgrade_types::stim_packs)) {
				if (u->type == unit_types::marine || u->type == unit_types::firebat) {
					if (current_command_frame >= s.stim_active_until && u->hp > 10) {
						u->game_unit->useTech(upgrade_types::stim_packs->game_tech_type);
						s.next_use_stim = current_command_frame;
					}
				}
			}
		}

//		xy prev;
//		for (auto& v : draw_lines) {
//			if (prev != xy()) game->drawLineMap(prev.x, prev.y, v.x, v.y, BWAPI::Colors::Purple);
//			prev = v;
//		}

	}

	auto apply_order(control& s) {
		unit* u = s.u;
		int order = s.next_order;
		xy order_target_pos = s.next_order_target_pos;
		unit* order_target = s.next_order_target;
		if (order == control::order_move) {
			s.moving_to.push_back(order_target_pos);
		} else {
			s.moving_to.push_back(s.pred_pos);
		}
		log(log_level_test, "%p: apply order %d\n", s.u, order);

//		if (order == control::order_stop && u->prev_pos == u->pos && current_frame - s.order_frame > latency_frames) {
//			if (xy_length(u->pos - s.pred_pos) >= 2) {
//				order = control::order_move;
//				order_target_pos = s.pred_pos;
//				order_target = nullptr;
//			}
//		}
//
		bool apply = s.order != order || s.order_target_pos != order_target_pos || s.order_target != order_target;
//		if (!apply && order == control::order_move && u->prev_pos == u->pos && current_frame - s.order_frame > latency_frames) apply = true;

		if (apply) {
			log(log_level_test, "%p: order %d %d %d %p -> %d %d %d %p\n", s.u, s.order, s.order_target_pos.x, s.order_target_pos.y, s.order_target, order, order_target_pos.x, order_target_pos.y, order_target);
			if (order == control::order_stop) {
				u->game_unit->holdPosition();
			} else if (order == control::order_move) {
				game->drawLineMap(u->pos.x, u->pos.y, order_target_pos.x, order_target_pos.y, BWAPI::Colors::White);
				u->game_unit->move(BWAPI::Position(order_target_pos.x, order_target_pos.y));
			} else if (order == control::order_attack) {
				if (u->type == unit_types::medic) {
					u->game_unit->useTech(upgrade_types::healing->game_tech_type, order_target->game_unit);
				} else {
					u->game_unit->attack(order_target->game_unit);
				}
			}
			s.order_frame = current_frame;
			s.order_command_frame = current_command_frame;
			s.order = order;
			s.order_target_pos = order_target_pos;
			s.order_target = order_target;
		}
	}

	a_unordered_set<int> lurker_spine_exists;

	void set_spines(lurker_attack& n) {
		fp8 speed = fp8::from_raw(256 * 18 + 192);
		auto pos = to_xy_fp8(n.from);
		auto to = to_xy_fp8(n.to);
		for (size_t i = 0; i != 6; ++i) {
			pos += direction_xy(xy_direction(to - pos), speed);
			n.spines[i] = to_xy(pos);
			n.to = to_xy(pos);
			pos += direction_xy(xy_direction(to - pos), speed);
		}
	}

	a_unordered_map<unit*, control*> control_for_unit;

	void update_lurker_attacks() {
		lurker_attacks.erase(std::remove_if(lurker_attacks.begin(), lurker_attacks.end(), [&](auto& v) {
			return current_frame >= v.start_frame + 20;
		}), lurker_attacks.end());

		lurker_attacks.erase(std::remove_if(lurker_attacks.begin(), lurker_attacks.end(), [&](auto& v) {
			return !v.from_bullet && current_frame - v.start_frame >= latency_frames + 2;
		}), lurker_attacks.end());

		for (unit* e : visible_enemy_units) {
			if (e->type == unit_types::lurker) {
				log(log_level_test, "there is a lurker at %d %d\n", e->pos.x, e->pos.y);
				log(log_level_test, " target %p order target %p\n", e->game_unit->getTarget(), e->game_unit->getOrderTarget());
				if (e->burrowed && (!e->burrowing || current_frame - e->burrowing_start_frame >= 50 - latency_frames)) {
					if (e->weapon_cooldown > latency_frames) {
						lurker_attacks.erase(std::remove_if(lurker_attacks.begin(), lurker_attacks.end(), [&](auto& v) {
							return !v.from_bullet && v.source == e;
						}), lurker_attacks.end());
					}
					auto spawn = [&](unit* target) {
						auto* w = target->is_flying ? e->stats->air_weapon : e->stats->ground_weapon;
						if (!w) return;
						xy target_pos = target->pos;
						bool in_range = units_distance(e, target) <= w->max_range;
						if (!in_range) {
							auto* s = control_for_unit[target];
							if (s) {
								target_pos = s->pred_pos;
								in_range = units_distance(e->pos, e->type, target_pos, target->type) <= w->max_range;
							}
						}
						if (in_range) {
							auto target_direction = xy_direction(target_pos - e->pos);
							xy from = e->pos;
							xy to = from + to_xy(direction_xy(target_direction, fp8::integer(212)));
							//xy to = target_pos;
							lurker_attacks.push_back({e, false, current_frame + std::max(e->weapon_cooldown - latency_frames, 0) - 1, from, to});
							set_spines(lurker_attacks.back());

							int lurker_heading = (int)(e->heading / PI * 128);
							int target_heading = (int)((target->pos - e->pos).angle() / PI * 128);
							log(log_level_test, " lurker heading %d target heading %d\n", lurker_heading, target_heading);
							log(log_level_test, " maybe attack from %d %d to %d %d\n", from.x, from.y, to.x, to.y);
						} else log(log_level_test, " lurker is out of range from %s\n", target->type->name);
					};
					unit* target = e->order_target;
					if (!target) target = e->target;
					if (target) spawn(target);
					else {
						for (unit* u : my_units) {
							if (u->is_flying) continue;
							if (units_distance(e, u) <= 256) {
								spawn(u);
							}
						}
					}
				}
			}
		}

		auto targetunit = [&](BWAPI_Unit gu) -> unit* {
			if (!gu) return nullptr;
			if (!gu->isVisible(players::self)) return nullptr;
			if (gu->getID() < 0) xcept("(bullet target) %s->getId() is %d\n", gu->getType().getName(), gu->getID());
			return units::get_unit(gu);
		};

		for (BWAPI_Bullet b : game->getBullets()) {
			if (b->getType() == BWAPI::BulletTypes::Subterranean_Spines) {
				if (lurker_spine_exists.insert(b->getID()).second) {
					unit* source = targetunit(b->getSource());
					unit* target = targetunit(b->getTarget());
					if (source && target) {
						lurker_attacks.erase(std::remove_if(lurker_attacks.begin(), lurker_attacks.end(), [&](auto& v) {
							return !v.from_bullet && v.source == source;
						}), lurker_attacks.end());
						auto target_direction = xy_direction(target->pos - source->pos);
						xy from = source->pos;
						xy to = from + to_xy(direction_xy(target_direction, fp8::integer(212)));
						lurker_attacks.push_back({source, true, current_frame - 1, from, to});
						log(log_level_test, "lurker attack created, from %d %d to %d %d\n", from.x, from.y, to.x, to.y);
						set_spines(lurker_attacks.back());

						log(log_level_test, " bullet angle %d\n", (int)(b->getAngle() / PI * 128));
					}
				}
			}
		}
	}

	bool any_lurkers_nearby(xy pos) {
		for (unit* e : visible_enemy_units) {
			if (e->type == unit_types::lurker) {
				if (sc_distance(pos - e->pos) <= 32 * 10) return true;
			}
		}
		return false;
	}

	void update() {

		for (auto* a : combat::live_combat_units) {
			if (current_frame < a->strategy_busy_until) continue;
			if (current_frame - a->last_fight > 15) continue;
			if (!any_lurkers_nearby(a->u->pos)) continue;
			//if (a->u->type == unit_types::medic || a->u->type == unit_types::marine || a->u->type == unit_types::science_vessel) {
			if (a->u->type == unit_types::medic || a->u->type == unit_types::marine) {
				auto h = combat::request_control(a);
				if (h) {
					all.emplace_back();
					auto& n = all.back();
					n.h = std::move(h);
					n.a = a;
					n.u = a->u;
					control_for_unit[n.u] = &n;
				}
				break;
			}
		}

		for (auto i = all.begin(); i != all.end();) {
			if (!i->a || i->a->u->dead || !any_lurkers_nearby(i->u->pos)) i = all.erase(i);
			else ++i;
		}

		for (auto& v : all) {
			prepare_move(v);
		}

		update_lurker_attacks();

		for (auto& v : all) {
			tsc::high_resolution_timer ht;
			move(v);
			log(log_level_test, " move took %gms\n", ht.elapsed() * 1000);
		}

		for (auto& v : all) {
			apply_order(v);
		}

		for (BWAPI_Bullet b : game->getBullets()) {
			if (b->getType() == BWAPI::BulletTypes::Subterranean_Spines) {
				log(log_level_test, " bullet is at %d %d\n", b->getPosition().x, b->getPosition().y);
				log(log_level_test, " remove timer is %d\n", b->getRemoveTimer());
				xy_typed<double> velocity(b->getVelocityX(), b->getVelocityY());
				log(log_level_test, " velocity is %g %g (%g)\n", velocity.x, velocity.y, velocity.length());
			}
		}

	}

	void marine_micro_task() {

		while (true) {

			update();

			multitasking::sleep(1);

		}

	}

	void target_groups_task() {

		target_group marine_targets;
		target_group medic_targets;

		while (true) {
			marine_targets.targets = enemy_units;
			auto* marine_stats = units::get_unit_stats(unit_types::marine, players::my_player);
			marine_targets.range = (int)marine_stats->ground_weapon->max_range;
			update_costgrid(marine_targets);
			medic_targets.targets = my_completed_units_of_type[unit_types::marine];
			medic_targets.range = 32;
			update_costgrid(medic_targets);

			for (auto& v : all) {
				v.next_g = nullptr;
				if (v.u->type == unit_types::marine) v.next_g = &marine_targets;
				else if (v.u->type == unit_types::medic) v.next_g = &medic_targets;
			}

			multitasking::sleep(4);
		}

	}

	void marine_micro_drawing_task() {

		while (true) {

			for (auto& v : lurker_attacks) {
				game->drawLineMap(v.from.x, v.from.y, v.to.x, v.to.y, BWAPI::Colors::Red);
				game->drawTextMap(v.from.x, v.from.y, "%d", v.start_frame - current_frame);

				for (size_t i = 0; i != 6; ++i) {
					xy pos = v.spines[i];
					game->drawLineMap(pos.x - 4, pos.y - 4, pos.x + 4, pos.y + 4, BWAPI::Colors::Red);
					game->drawLineMap(pos.x + 4, pos.y - 4, pos.x - 4, pos.y + 4, BWAPI::Colors::Red);
				}
			}

			multitasking::sleep(1);
		}

	}

	void init() {

		multitasking::spawn(marine_micro_task, "marine micro");

		multitasking::spawn(target_groups_task, "target groups");

		multitasking::spawn(0.1, marine_micro_drawing_task, "marine micro drawing");


	}

}