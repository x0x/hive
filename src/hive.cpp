#include "../include/types.hpp"
#include "../include/position.hpp"
#include "../include/tests.hpp"
#include "../include/zobrist.hpp"
#include "../include/search.hpp"
#include "../include/uci.hpp"
#include "../include/thread.hpp"
#include <chrono>

int main()
{
    Bitboards::init_bitboards();
    Zobrist::build_rnd_hashes();
    UCI::init_options();
    ttable = HashTable<TranspositionEntry>(16);
    pool = new ThreadPool();

    UCI::main_loop();
    pool->kill_threads();
}
