
template<typename utype>
struct xy_typed {
	utype x {};
	utype y {};
	typedef xy_typed<utype> xy;
	xy_typed() = default;
	xy_typed(utype x,utype y) : x(x), y(y) {}
	bool operator<(const xy_typed&n) const {
		if (y==n.y) return x<n.x;
		return y<n.y;
	}
	bool operator>(const xy_typed&n) const {
		if (y==n.y) return x>n.x;
		return y>n.y;
	}
	bool operator<=(const xy_typed&n) const {
		if (y==n.y) return x<=n.x;
		return y<=n.y;
	}
	bool operator>=(const xy_typed&n) const {
		if (y==n.y) return x>=n.x;
		return y>=n.y;
	}
	bool operator==(const xy_typed&n) const {
		return x==n.x && y==n.y;
	}
	bool operator!=(const xy_typed&n) const {
		return x!=n.x || y!=n.y;
	}
	xy_typed operator-(const xy_typed&n) const {
		xy r(*this);
		return r-=n;
	}
	xy_typed&operator-=(const xy_typed&n) {
		x -= n.x;
		y -= n.y;
		return *this;
	}
	xy_typed operator+(const xy_typed&n) const {
		xy r(*this);
		return r+=n;
	}
	xy_typed&operator+=(const xy_typed&n) {
		x += n.x;
		y += n.y;
		return *this;
	}
	double length() const {
		return sqrt(x*x+y*y);
	}
	xy_typed operator -() const {
		return xy(-x,-y);
	}
	double angle() const {
		return atan2(y,x);
	}
	xy_typed operator/(const xy_typed&n) const {
		xy r(*this);
		return r/=n;
	}
	xy_typed&operator/=(const xy_typed&n) {
		x /= n.x;
		y /= n.y;
		return *this;
	}
	xy_typed operator/(utype d) const {
		xy r(*this);
		return r/=d;
	}
	xy_typed&operator/=(utype d) {
		x /= d;
		y /= d;
		return *this;
	}
	xy_typed operator*(const xy_typed&n) const {
		xy r(*this);
		return r*=n;
	}
	xy_typed&operator*=(const xy_typed&n) {
		x *= n.x;
		y *= n.y;
		return *this;
	}
	xy_typed operator*(utype d) const {
		xy r(*this);
		return r*=d;
	}
	xy_typed&operator*=(utype d) {
		x *= d;
		y *= d;
		return *this;
	}
};

typedef xy_typed<int> xy;


int frames_to_reach(double initial_speed,double acceleration,double max_speed,double stop_distance,double distance) {

	int f = 0;
	double s = initial_speed;
	double d = distance;
	if (d/max_speed>=15*600) return 15*600;
	while (d>0) {
		//if (d<stop_distance*(s/max_speed)) s -= max_speed/stop_distance;
		//if (d<stop_distance*(s/max_speed)) s = max_speed*(d/stop_distance) + acceleration;
		if (d<stop_distance*((s-acceleration)/max_speed)) s -= acceleration;
		//if (d<stop_distance) s-=max_speed/stop_distance;
		else if (s<max_speed) s+=acceleration;
		if (s>max_speed) s = max_speed;
		if (s<acceleration) s = acceleration;
		//d = std::floor(d*256.0)/256.0;
		//d = std::floor(d+0.5);
		if (s>=max_speed) {
			double dd = std::max(d,stop_distance) - stop_distance;
			if (dd==0) {
				d -= s;
				++f;
			} else {
				int t = std::max((int)(dd/s),1);
				d -= t*s;
				f += t;
			}
		} else {
			d -= s;
			++f;
		}
		//d = std::floor(d*256.0) / 256.0;
		//log("frames_to_reach: speed %g, distance %g, stop_distance is %g\n",s,d,stop_distance);
	}

	return f;
}

template<typename T = unit_stats>
int frames_to_reach(T* stats,double initial_speed,double distance) {
	return frames_to_reach(initial_speed,stats->acceleration,stats->max_speed,stats->stop_distance,distance);
}
template<typename T = unit>
int frames_to_reach(T* u,double distance) {
	return frames_to_reach(u->stats,u->speed,distance);
}

xy rectangles_difference(xy a0, xy a1, xy b0, xy b1) {
	int x, y;
	if (a0.x > b1.x) x = a0.x - b1.x;
	else if (b0.x > a1.x) x = b0.x - a1.x;
	else x = 0;
	if (a0.y > b1.y) y = a0.y - b1.y;
	else if (b0.y > a1.y) y = b0.y - a1.y;
	else y = 0;

	return xy(x, y);
}

xy units_difference(xy a_pos,std::array<int,4> a_dimensions,xy b_pos,std::array<int,4> b_dimensions) {
	xy a0 = a_pos + xy(-a_dimensions[2],-a_dimensions[3]);
	xy a1 = a_pos + xy(a_dimensions[0],a_dimensions[1]);
	xy b0 = b_pos + xy(-b_dimensions[2],-b_dimensions[3]);
	xy b1 = b_pos + xy(b_dimensions[0],b_dimensions[1]);

	int x, y;
	if (a0.x>b1.x) x = a0.x - b1.x;
	else if (b0.x>a1.x) x = b0.x - a1.x;
	else x = 0;
	if (a0.y>b1.y) y = a0.y - b1.y;
	else if (b0.y>a1.y) y = b0.y - a1.y;
	else y = 0;

	return xy(x,y);
}

double diag_distance(xy pos) {
	double d = std::min(std::abs(pos.x),std::abs(pos.y));
	double s = std::abs(pos.x)+std::abs(pos.y);

	return d*1.4142135623730950488016887242097 + (s-d*2);
}

double manhattan_distance(xy pos) {
	return std::abs(pos.x) + std::abs(pos.y);
}

double max_distance(xy pos) {
	return std::max(std::abs(pos.x), std::abs(pos.y));
}

double units_distance(unit* a, unit* b);

double units_distance(xy a_pos,unit* a,xy b_pos,unit* b);
double units_distance(xy a_pos,unit_type* at,xy b_pos,unit_type* bt);

double units_pathing_distance(unit* a, unit* b, bool include_enemy_buildings = true);

double units_pathing_distance(unit_type*ut,unit*a,unit*b);

double unit_pathing_distance(unit_type*ut,xy from,xy to);
double unit_pathing_distance(unit*u,xy to);

xy nearest_spot_in_square(xy pos,xy top_left,xy bottom_right) {
	if (top_left.x>pos.x) pos.x = top_left.x;
	else if (bottom_right.x<pos.x) pos.x = bottom_right.x;
	if (top_left.y>pos.y) pos.y = top_left.y;
	else if (bottom_right.y<pos.y) pos.y = bottom_right.y;
	return pos;
}

int sc_distance(xy vec) {
	unsigned int x = std::abs(vec.x);
	unsigned int y = std::abs(vec.y);
	if (x < y) std::swap(x, y);
	if (x / 4 < y) x = x - x / 16 + y * 3 / 8 - x / 64 + y * 3 / 256;
	return x;
}

template<typename T = unit>
double unit_distance(T* u, xy pos) {
	xy upper_left = u->pos - xy(u->type->dimension_left(), u->type->dimension_up());
	xy bottom_right = u->pos + xy(u->type->dimension_right(), u->type->dimension_down());
	return (double)sc_distance(nearest_spot_in_square(pos, upper_left, bottom_right) - pos);
}

template<typename T = unit>
double unit_distance(xy upos, T* u, xy pos) {
	xy upper_left = upos - xy(u->type->dimension_left(), u->type->dimension_up());
	xy bottom_right = upos + xy(u->type->dimension_right(), u->type->dimension_down());
	return (double)sc_distance(nearest_spot_in_square(pos, upper_left, bottom_right) - pos);
}

template<typename T = unit>
double unit_max_distance(T* u, xy pos) {
	xy upper_left = u->pos - xy(u->type->dimension_left(), u->type->dimension_up());
	xy bottom_right = u->pos + xy(u->type->dimension_right(), u->type->dimension_down());
	return max_distance(nearest_spot_in_square(pos, upper_left, bottom_right) - pos);
}

template<typename T>
struct refcounter {
	T* obj = nullptr;
	refcounter() = default;
	refcounter(std::nullptr_t) {}
	refcounter(T& arg_obj) noexcept : obj(&arg_obj) {
		obj->addref();
	}
	refcounter(refcounter&& n) noexcept {
		obj = n.obj;
		n.obj = nullptr;
	}
	refcounter(const refcounter& n) noexcept {
		obj = n.obj;
		if (obj) obj->addref();
	}
	refcounter& operator=(refcounter&& n) noexcept {
		std::swap(obj, n.obj);
		return *this;
	}
	refcounter& operator=(const refcounter& n) noexcept {
		obj = n.obj;
		if (obj) obj->addref();
		return *this;
	}
	~refcounter() {
		if (obj) obj->release();
	}
	explicit operator bool() const {
		return obj != nullptr;
	}
	operator T*() {
		return obj;
	}
	T* operator->() {
		return obj;
	}
	T* operator*() {
		return obj;
	}
};

struct refcounted {
	int reference_count;
	refcounted() : reference_count(0) {}
	void addref() {
		++reference_count;
	}
	int release() {
		return --reference_count;
	}
};

template<typename T>
struct refcounted_ptr {
	T*ptr = nullptr;
	refcounted_ptr() {}
	refcounted_ptr(T*ptr) : ptr(ptr) {
		if (ptr) ptr->addref();
	}
	refcounted_ptr(const refcounted_ptr&n) {
		*this = n;
	}
	refcounted_ptr(refcounted_ptr&&n) {
		*this = std::move(n);
	}
	~refcounted_ptr() {
		if (ptr) ptr->release();
	}
	operator T*() const {
		return ptr;
	}
	T*operator->() const {
		return ptr;
	}
	operator bool() const {
		return ptr ? true : false;
	}
	bool operator==(const refcounted_ptr&n) const {
		return ptr==n.ptr;
	}
	refcounted_ptr&operator=(const refcounted_ptr&n) {
		if (ptr) ptr->release();
		ptr = n.ptr;
		if (ptr) ptr->addref();
		return *this;
	}
	refcounted_ptr&operator=(refcounted_ptr&&n) {
		std::swap(ptr,n.ptr);
		return *this;
	}
};


a_string short_type_name(unit_type*type);

struct no_value_t {};
static const no_value_t no_value;
template<typename A,typename B>
bool is_same_but_not_no_value(A&&a,B&&b) {
	return a==b;
}
template<typename A>
bool is_same_but_not_no_value(A&&a,no_value_t) {
	return false;
}
template<typename B>
bool is_same_but_not_no_value(no_value_t,B&&b) {
	return false;
}
template<typename T>
struct identity {
	const T&operator()(const T&v) const {
		return v;
	}
};
struct identity_any {
	template<typename T>
	const T&operator()(const T&v) const {
		return v;
	}
};

template<typename cont_T,typename score_F=identity<typename cont_T::value_type>,typename invalid_score_T=no_value_t,typename best_possible_score_T=no_value_t>
typename std::remove_reference<cont_T>::type::value_type get_best_score(cont_T&& cont,score_F&& score=score_F(),const invalid_score_T& invalid_score = invalid_score_T(), const best_possible_score_T& best_possible_score = best_possible_score_T()) {
	typename std::remove_const<typename std::remove_reference<typename std::result_of<score_F(typename std::remove_reference<cont_T>::type::value_type)>::type>::type>::type best_score{};
	typename std::remove_reference<cont_T>::type::value_type best{};
	int n = 0;
	for (auto&& v : cont) {
		auto s = score(v);
		if (is_same_but_not_no_value(s,invalid_score)) continue;
		if (n==0 || s<best_score) {
			++n;
			best = v;
			best_score = s;
			if (is_same_but_not_no_value(s,best_possible_score)) break;
		}
	}
	return best;
}
template<typename cont_T,typename score_F,typename invalid_score_T=no_value_t,typename best_possible_score_T=no_value_t>
typename std::remove_reference<cont_T>::type::pointer get_best_score_p(cont_T&& cont,score_F&& score,const invalid_score_T& invalid_score = invalid_score_T(), const best_possible_score_T& best_possible_score = best_possible_score_T()) {
	return get_best_score(make_transform_filter(std::forward<cont_T>(cont),[&](typename std::remove_reference<cont_T>::type::reference v) {
		return &v;
	}),std::forward<score_F>(score),invalid_score,best_possible_score);
}
template<typename cont_T, typename score_F, typename invalid_score_T = no_value_t, typename best_possible_score_T = no_value_t>
typename std::result_of<score_F(typename std::remove_reference<cont_T>::type::reference)>::type get_best_score_value(cont_T&& cont, score_F&& score, const invalid_score_T& invalid_score = invalid_score_T(), const best_possible_score_T& best_possible_score = best_possible_score_T()) {
	auto t_cont = make_transform_filter(std::forward<cont_T>(cont), std::forward<score_F>(score));
	return get_best_score(t_cont, identity_any(), invalid_score, best_possible_score);
}

template<typename cont_T, typename val_T>
typename cont_T::iterator find_and_erase(cont_T&cont, val_T&&val) {
	return cont.erase(std::find(cont.begin(), cont.end(), std::forward<val_T>(val)));
}

template<typename cont_T, typename pred_T>
bool test_pred(cont_T&cont, pred_T&&pred) {
	for (auto&v : cont) {
		if (pred(v)) return true;
	}
	return false;
}

xy get_nearest_available_cc_build_pos(xy pos);

constexpr double inf = INFINITY;
