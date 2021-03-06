#pragma once
#include "hash.hpp"
#include "search.hpp"
#include "types.hpp"
#include "position.hpp"
#include <fstream>
#include <string>


namespace Tests
{
    class PerftTest
    {
        std::string m_fen;
        Depth m_depth;
        int64_t m_result;

    public:
        PerftTest(std::string fen, Depth depth, int64_t result)
            : m_fen(fen), m_depth(depth), m_result(result)
        {}

        std::string fen() const { return m_fen; }
        Depth depth() const { return m_depth; }
        int64_t result() const { return m_result; }
    };


    std::vector<PerftTest> test_suite();


    int perft_tests();


    template<bool USE_ORDER, bool TT, bool LEGALITY>
    int perft_techniques_tests()
    {
        auto tests = test_suite();

        // Allocate TT
        if (TT)
            perft_table.resize(16);

        int n_failed = 0;
        for (auto& test : tests)
        {
            Position pos(test.fen());
            auto result_base = Search::perft<false>(pos, test.depth() - 1);
            auto result_test = Search::template perft<false, USE_ORDER, TT, LEGALITY>(pos, test.depth() - 1);
            if (result_base == result_test)
            {
                std::cout << "[ OK ] " << test.fen() << " (" << result_test << ")" << std::endl;
            }
            else
            {
                std::cout << "[FAIL] " << test.fen() << " (base " << result_base << ", test " << result_test << ")" << std::endl;
                n_failed++;
            }
        }

        // Deallocate TT
        if (TT)
            perft_table.resize(0);

        std::cout << "\nFailed/total tests: " << n_failed << "/" << tests.size() << std::endl;
        return n_failed;
    }


    int legality_tests();
}
