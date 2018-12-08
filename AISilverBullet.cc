#include "Player.hh"
#include <list>

#ifdef DEBUG
#define LOG(x) do { cerr << "debug: " << x << endl; } while(0);
#else
#define LOG(x)
#endif

/**
 * Write the name of your player and save this file
 * with the same name and .cc extension.
 */
#define PLAYER_NAME SilverBullet


struct PLAYER_NAME : public Player {

    /**
     * Factory: returns a new instance of this class.
     * Do not modify this function.
     */
    static Player* factory () {
        return new PLAYER_NAME;
    }

    /**
     * Types and attributes for your player can be defined here.
     */

    // Config
    const int INF=1e8;

    const char CITY_FIGHT_RATIO=75; // Percentatge of probability to win when taking fight
    const char WATER_MARGIN=8;
    const char FOOD_MARGIN=7;

    // Types
    struct Warrior_t;
    struct Car_t;

    typedef vector<vector<int> > dmap;

    // Global attributes
    dmap water_map, fuel_map, fuel_map_empty;

    dmap nearest_city;
    vector<dmap> cities_map;
    vector<vector<Pos> > cities;

    map<int, Warrior_t> registered_warriors;
    map<int, Car_t> registered_cars;

    dmap movements;

    // Helper functions

    void bfs(queue<pair<Pos, int> > &q, dmap &m, const bool &cross_city=false);
    void compute_maps();
    void explore_city(const Pos &p, const int &city, queue<pair<Pos, int> > &q);
    void map_nearest_city(queue<pair<Pos, int> > &q);

    typedef pair<int, Pos> fq_t;
    void compute_fuel_map(priority_queue<fq_t, vector<fq_t>, greater<fq_t> > &q);

    list<Dir> get_dir_from_dmap(const Unit &u, const dmap &m);
    void remove_unsafe_dirs(const Unit &u, list<Dir> &l);

    inline bool fight(const int &attacker_id, const int &victim_id);
    bool fight_desert(const Unit &attacker_id, const Unit &victim_id);
    inline bool fight_city(const Unit &attacker_id, const Unit &victim_id);

    void assign_job(Warrior_t &w, const Pos &p);
    void assign_job(Car_t &w, const Pos &p);

    void check_suplies(Warrior_t &w, const Unit &u);
    inline bool needs_water(const Unit &u);
    inline bool needs_food(const Unit &u);

    inline void register_and_move(const int &id, const Pos &p, const Dir &d);

    // Game routines
    void init();
    void move_cars();
    void move_warriors();

    void move_car(const int &car_id);
    void move_warrior(const int &warrior_id);

    /**
     * Play method, invoked once per each round.
     */
    virtual void play () {
        if (round() == 0) init();
        move_cars();
        move_warriors();
    }

#ifdef DEBUG
    void show_dmap(const dmap &m) {
        for (int i=0; i < rows(); ++i) {
            for (int j=0; j < cols(); ++j) {
                const int &v = m[i][j];
                if (v == INF) cerr << "++";
                else if (v == -1) cerr << "[]";
                else if (v < 10) cerr << ' ' << v;
                else cerr << v;
                cerr << ' ';
            }
            cerr << endl;
        }
    }
#endif

};

struct PLAYER_NAME::Warrior_t {
    enum state_t {
        NONE        =0,
        FOLLOW_DMAP =1<<0,
        WATERING    =1<<1,
        FEEDING     =1<<2
    };
    short state;
    short last_seen;

    stack<dmap*> dmaps;

    Warrior_t(): state(NONE), last_seen(0) {};

    inline void set_bit(const state_t &bit) {
        state |= bit;
    }
    inline void clear_bit(const state_t &bit) {
        if (get_bit(bit)) state ^= bit;
    }
    inline bool get_bit(const state_t &bit) {
        return state & bit;
    }
};

struct PLAYER_NAME::Car_t {
    enum state_t {
        NONE        =0,
        FOLLOW_DMAP =1<<0,
        ATTACK      =1<<1,
        PATROL      =1<<2,
        HUNT        =1<<3
    };
    short state;
    short last_seen;

    int unit_id; // For hunt
    stack<dmap*> dmaps;

    Car_t(): state(HUNT|ATTACK), last_seen(0) {};

    inline void set_bit(const state_t &bit) {
        state |= bit;
    }
    inline void clear_bit(const state_t &bit) {
        if (get_bit(bit)) state ^= bit;
    }
    inline bool get_bit(const state_t &bit) {
        return state & bit;
    }
};

void PLAYER_NAME::init() {
    movements = dmap(rows(), vector<int> (cols(), -1));
    compute_maps();
#ifdef DEBUG
    LOG("-- WATER_MAP --")
        show_dmap(water_map);
    LOG("-- FUEL_MAP EMPTY --")
        show_dmap(fuel_map_empty);
    LOG("-- FUEL_MAP --")
        show_dmap(fuel_map);
    LOG("-- NEAREST CITY MAP --")
        show_dmap(nearest_city);
    for (int i = 0; i < nb_cities(); ++i) {
        LOG("-- CITY " << i << " --");
        show_dmap(cities_map[i]);
    }
#endif
}

void PLAYER_NAME::compute_maps() {
    queue<pair<Pos, int> > w, f; // water & fuel queues

    priority_queue<fq_t, vector<fq_t>, greater<fq_t> > fq;

    vector<queue<pair<Pos, int> > > cq(nb_cities());
    nearest_city = dmap(rows(), vector<int> (cols(), INF));
    cities_map = vector<dmap> (nb_cities(), dmap(rows(), vector<int> (cols(), INF)));
    cities = vector<vector<Pos> > (nb_cities());

    int city = 0;

    for (int i=0; i < rows(); ++i) {
        for (int j=0; j < cols(); ++j) {
            const CellType ct = cell(Pos(i, j)).type;
            if (ct == Water) w.emplace(Pos(i, j), -1); //-1 since we cannot get into water
            else if (ct == Station) {
                f.emplace(Pos(i, j), -1);  // pari<Pos, int>
                fq.emplace(-1, Pos(i, j)); // pair<int, Pos>
            } else if (ct == City) {
                if (nearest_city[i][j] != INF) continue;
                explore_city(Pos(i, j), city, cq[city]);
                ++city;
            }
        }
    }

    LOG("FOUND " << city << "/" << nb_cities() << " cities ");

    compute_fuel_map(fq);

    bfs(w, water_map, true);
    bfs(f, fuel_map_empty, false);

    queue<pair<Pos, int> > ncq; // nearest_city queue

    for (int i = 0; i < nb_cities(); ++i) {
        for (const Pos &p : cities[i]) ncq.emplace(p, i);
        bfs(cq[i], cities_map[i], true);
    }

    map_nearest_city(ncq);
}

void PLAYER_NAME::compute_fuel_map(priority_queue<fq_t, vector<fq_t>, greater<fq_t> > &q) {
    fuel_map = dmap(rows(), vector<int>(cols(), INF));
    while(!q.empty()) {
        const int d = q.top().first;
        const Pos p = q.top().second;
        q.pop();

        if (fuel_map[p.i][p.j] != INF) continue;
        fuel_map[p.i][p.j]=d;

        for (int i = 0; i < DirSize-1; ++i) {
            const Pos p2 = p + Dir(i);
            if (pos_ok(p2)) {
                const CellType ct = cell(p2).type;

                if (d == -1) q.emplace(0, p2);
                else if (ct == Desert) q.emplace(d + nb_players(), p2);
                else if (ct == Road) q.emplace(d + 1, p2);
            }
        }
    }
}

void PLAYER_NAME::map_nearest_city(queue<pair<Pos, int> > &q) {
    while (!q.empty()) {
        const Pos p = q.front().first;
        const int city = q.front().second;
        q.pop();

        if (nearest_city[p.i][p.j] < INF) continue; // !! care explore_city use of nearest_city
        nearest_city[p.i][p.j] = city;

        for (int i = 0; i < DirSize-1; ++i) {
            const Pos p2 = p + Dir(i);
            if (pos_ok(p2)) {
                const CellType ct = cell(p2).type;
                if (ct == Road or ct == Desert) q.emplace(p2, city);
            }
        }
    }
}

void PLAYER_NAME::bfs(queue<pair<Pos, int> > &q, dmap &m, const bool &cross_city) {
    m = dmap(rows(), vector<int>(cols(), INF));
    while(!q.empty()) {
        const Pos p = q.front().first;
        const int d = q.front().second;
        q.pop();

        if (m[p.i][p.j] != INF) continue;

        m[p.i][p.j] = d;

        for (int i = 0; i < DirSize-1; ++i) {
            const Pos p2 = p + Dir(i);
            if (pos_ok(p2)) {
                const CellType ct = cell(p2).type;
                if (ct == Road or ct == Desert or (ct == City and cross_city))
                    q.emplace(p2, d+1);
            }
        }
    }
}

void PLAYER_NAME::explore_city(const Pos &_p, const int &city, queue<pair<Pos, int> > &q) {
    queue<Pos> Q;
    Q.push(_p);

    while (!Q.empty()) {
        const Pos p = Q.front();
        Q.pop();

        if (nearest_city[p.i][p.j] != INF) continue;
        nearest_city[p.i][p.j] = INF+1; // !! care with map_nearest_city

        cities[city].push_back(p);

        for (int i = 0; i < DirSize-1; ++i) {
            const Pos p2 = p + Dir(i);
            if (pos_ok(p2)) {
                if (cell(p2).type == City) {
                    Q.push(p2);
                    q.emplace(p2, 0);
                }
            }
        }
    }
}

void PLAYER_NAME::move_cars() {
    for (const int &car_id : cars(me()))
        if (can_move(car_id))
            move_car(car_id);
}

void PLAYER_NAME::move_warriors() {
    if (round()% 4 != me()) return; // This line makes a lot of sense.

    for (const int &warrior_id : warriors(me()))
        move_warrior(warrior_id);
}

void PLAYER_NAME::move_car(const int &car_id) { }
void PLAYER_NAME::assign_job(Car_t &w, const Pos &p) { }

void PLAYER_NAME::move_warrior(const int &warrior_id) {
    Warrior_t &w = registered_warriors[warrior_id];
    const Unit u = unit(warrior_id);

    LOG("Warrior ID: " << warrior_id);

    // If we haven't seen him for 4(nb_players) rounds he has died and respawned
    if (w.last_seen+nb_players() < round()) {
        w = Warrior_t();
    }

    w.last_seen = round();

    LOG("state: " << w.state << ' ' << Warrior_t::NONE);

    if (w.state == Warrior_t::NONE) assign_job(w, u.pos);

    check_suplies(w, u);

    if (w.get_bit(Warrior_t::FOLLOW_DMAP)) {
        if (w.dmaps.empty()) {
            LOG("Empty dmap Stack for id: " << warrior_id)
                return;
        }

        dmap &m = *w.dmaps.top();

        // Empty (except last) dmap stack if we reached our destinations
        while (w.dmaps.size() > 1 and m[u.pos.i][u.pos.j]==0) {
            w.dmaps.pop();
            m = *w.dmaps.top();
        }

        list<Dir> l = get_dir_from_dmap(u, m);
        if (l.empty()) {
            LOG("No safe moves " << warrior_id);
            return;
        }
        register_and_move(warrior_id, u.pos, l.front());
    }
}

void PLAYER_NAME::check_suplies(Warrior_t &w, const Unit &u) {

    bool wat = needs_water(u), food = needs_food(u);

    if (w.get_bit(Warrior_t::WATERING)) {
        if (!wat) w.clear_bit(Warrior_t::WATERING);
    } else if (wat) {
        LOG("WATER NEEDED")
        w.dmaps.push(&water_map);
        w.set_bit(Warrior_t::WATERING);
    }

    if (w.get_bit(Warrior_t::FEEDING)) {
        if (!food) w.clear_bit(Warrior_t::FEEDING);
    } else if (food) {
        LOG("FOOD NEEDED")
        w.dmaps.push(&cities_map[nearest_city[u.pos.i][u.pos.j]]);
        w.set_bit(Warrior_t::FEEDING);
    }
}

bool PLAYER_NAME::needs_water(const Unit &u) {
    const int d = water_map[u.pos.i][u.pos.j];
    return u.water - WATER_MARGIN < d;
}

bool PLAYER_NAME::needs_food(const Unit &u) {
    const int c = nearest_city[u.pos.i][u.pos.j];
    const int d = cities_map[c][u.pos.i][u.pos.j];
    return u.food - FOOD_MARGIN < d;
}

void PLAYER_NAME::register_and_move(const int &id, const Pos &p, const Dir &d) {
    const Pos p2 = p + d;
    movements[p2.i][p2.j] = round();
    command(id, d);
}

void PLAYER_NAME::assign_job(Warrior_t &w, const Pos &p) {
    LOG("JOB ASSIGNMENT");
    if (random(0, 1) == 1) {
        const int city = nearest_city[p.i][p.j];
        w.dmaps.push(&cities_map[city]);
    } else {
        LOG("ASSIGNING RANDOM CITY")
        w.dmaps.push(&cities_map[random(0, nb_cities()-1)]);
    }
    w.set_bit(Warrior_t::FOLLOW_DMAP);
}

list<Dir> PLAYER_NAME::get_dir_from_dmap(const Unit &u, const dmap &m) {
    const Pos p = u.pos;
    const int d = m[p.i][p.j];
    list<Dir> ls, eq;
    for (const int &i : random_permutation(DirSize-1)) {
        const Pos p2 = p + Dir(i);
        if (pos_ok(p2)) {
            if (m[p2.i][p2.j] < d ) ls.push_back(Dir(i));
            else if (m[p2.i][p2.j] == d) eq.push_back(Dir(i));
        }
    }
    remove_unsafe_dirs(u, ls);
    if (!ls.empty()) return ls;

    remove_unsafe_dirs(u, eq);
    return eq;
}

void PLAYER_NAME::remove_unsafe_dirs(const Unit &u, list<Dir> &l) {
    auto it = l.begin();
    while (it != l.end()) {
        const Pos p = u.pos + *it;
        const int u_id = cell(p).id;
        bool del = movements[p.i][p.j] == round();
        if (!del and u_id != -1) {
            if (unit(u_id).player == me()) del=true;
            else del = !fight(u.id, u_id);
        }
        if (!del) {
            for (int i = 0; i < DirSize-1; ++i) {
                const Pos p2 = p + Dir(i);
                if (pos_ok(p2)) {
                    const int u_id2 = cell(p2).id;
                    if (u_id2 != -1) {
                        if (unit(u_id2).player != me())
                            del=!fight(u_id2, u.id);
                    }
                }
            }
        }
        if (del) it = l.erase(it);
        else ++it;
    }
}

bool PLAYER_NAME::fight(const int &attacker_id, const int &victim_id) {
    const Unit attacker=unit(attacker_id), victim=unit(victim_id);
    if (cell(victim.pos).type == City) return fight_city(attacker, victim);
    return fight_desert(attacker, victim);
}

bool PLAYER_NAME::fight_desert(const Unit &attacker, const Unit &victim) {
    if (victim.type == Car) return false; // Never attack cars
    if (attacker.type == Car) return true; // Car wins
    if (victim.food <= 6 or victim.water <= 6) return true;

    if (victim.food > attacker.food and victim.water > attacker.water)
        return false;

    // TODO: check this is correct
    if (victim.food < victim.water)
        return attacker.food > victim.food and attacker.water > victim.food;
    return attacker.water > victim.water and attacker.food > victim.water;
}

bool PLAYER_NAME::fight_city(const Unit &attacker, const Unit &victim) {
    return attacker.water*100/(attacker.water + victim.water) >= CITY_FIGHT_RATIO;
}

/**
 * Do not modify the following line.
 */
RegisterPlayer(PLAYER_NAME);

// vim:sw=4:ts=4:et:foldmethod=indent: