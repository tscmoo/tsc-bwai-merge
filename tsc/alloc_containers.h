

namespace tsc {
;

template<typename T>
using a_vector = std::vector<T, alloc<T>>;

template<typename T>
using a_deque = std::deque<T, alloc<T>>;
template<typename T>
using a_list = std::list<T, alloc<T>>;

template<typename T>
using a_set = std::set<T, std::less<T>, alloc<T>>;
template<typename T>
using a_multiset = std::multiset<T, std::less<T>, alloc<T>>;

template<typename key_T, typename value_T, typename pred_T = std::less<key_T>>
using a_map = std::map<key_T, value_T, pred_T, alloc<std::pair<const key_T, value_T>>>;
template<typename key_T, typename value_T, typename pred_T = std::less<key_T>>
using a_multimap = std::multimap<key_T, value_T, pred_T, alloc<std::pair<const key_T, value_T>>>;

template<typename T, typename hash_t = std::hash<T>, typename equal_to_t = std::equal_to<T>>
//using a_unordered_set = std::set<T, std::less<T>, alloc<T>>;
using a_unordered_set = std::unordered_set<T, hash_t, equal_to_t, alloc<T>>;
template<typename T, typename hash_t = std::hash<T>, typename equal_to_t = std::equal_to<T>>
using a_unordered_multiset = std::unordered_multiset<T, hash_t, equal_to_t, alloc<T>>;

template<typename key_T, typename value_T, typename hash_t = std::hash<key_T>, typename equal_to_t = std::equal_to<key_T>>
using a_unordered_map = std::unordered_map<key_T, value_T, hash_t, equal_to_t, alloc<std::pair<const key_T, value_T>>>;
//using a_unordered_map = std::map<key_T, value_T, std::less<key_T>, alloc<std::pair<const key_T, value_T>>>;
template<typename key_T, typename value_T, typename hash_t = std::hash<key_T>, typename equal_to_t = std::equal_to<key_T>>
using a_unordered_multimap = std::unordered_multimap<key_T, value_T, hash_t, equal_to_t, alloc<std::pair<const key_T, value_T>>>;


using a_string = std::basic_string<char, std::char_traits<char>, alloc<char>>;

}

