
struct player_t {
	BWAPI_Player game_player;
	bool is_enemy;
	double minerals_spent;
	double gas_spent;
	double minerals_lost;
	double gas_lost;
	int resource_depots_seen;
	int workers_seen;
	int race;
	bool random;
	bool rescuable;

	a_unordered_set<upgrade_type*> upgrades;
	bool has_upgrade(upgrade_type*t) {
		return upgrades.find(t) != upgrades.end();
	}
};

namespace players {
;

player_t*my_player;
player_t*opponent_player;
player_t*neutral_player;

a_deque<player_t> player_container;
a_unordered_map<BWAPI_Player, player_t*> player_map;

BWAPI_Player self;

player_t*get_player(BWAPI_Player game_player) {
	player_t*&p = player_map[game_player];
	if (!p) {
		player_container.emplace_back();
		p = &player_container.back();
		p->game_player = game_player;
		p->is_enemy = game_player->isEnemy(self);
		p->minerals_spent = 0.0;
		p->gas_spent = 0.0;
		p->minerals_lost = 0.0;
		p->gas_lost = 0.0;
		p->resource_depots_seen = 0;
		p->workers_seen = 0;
		p->race = race_terran;
		p->random = false;
		p->rescuable = game_player->getType() == BWAPI::PlayerTypes::RescuePassive;
		if (game_player->getRace() == BWAPI::Races::Terran) p->race = race_terran;
		else if (game_player->getRace() == BWAPI::Races::Protoss) p->race = race_protoss;
		else if (game_player->getRace() == BWAPI::Races::Zerg) p->race = race_zerg;
		else {
			p->random = true;
			p->race = race_protoss;
		}
	}
	return p;
}

void update_upgrades(player_t&p);

void update_players_task() {

	while (true) {

		for (auto&v : player_container) {
			update_upgrades(v);
		}

		multitasking::sleep(15);
	}

}


void init() {

	self = game->self();
	if (!self) {
		//self = *game->getPlayers().begin();
		for (auto&v : game->getPlayers()) {
			if (v->getRace() == BWAPI::Races::Protoss) {
				self = v;
				break;
			}
		}
	}
	if (!self) xcept("self is null");

	my_player = get_player(self);
	neutral_player = get_player(game->neutral());
	opponent_player = neutral_player;
	for (auto&v : game->getPlayers()) {
		auto*p = get_player(v);
		if (p->is_enemy) opponent_player = p;
	}

	multitasking::spawn(update_players_task, "update players");

}

}
