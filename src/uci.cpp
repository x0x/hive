#include "../include/evaluation.hpp"
#include "../include/search.hpp"
#include "../include/tests.hpp"
#include "../include/hash.hpp"
#include "../include/types.hpp"
#include "../include/uci.hpp"
#include "../include/thread.hpp"
#include <array>
#include <iostream>
#include <sstream>
#include <string>


namespace UCI
{
    std::map<std::string, Option> OptionsMap;


    namespace Options
    {
        int Hash;
        int MultiPV;
        bool Ponder;
        int Threads;
    }


    void Option::set(std::string value)
    {
        if (m_type == OptionType::CHECK)
        {
            *std::get<bool*>(m_data) = (value == "true");
        }
        else if (m_type == OptionType::SPIN)
        {
            *std::get<int*>(m_data) = std::clamp(std::stoi(value), m_min, m_max);
        }
        else if (m_type == OptionType::COMBO)
        {
            auto val = std::find(m_var.begin(), m_var.end(), value);
            if (val != m_var.end())
                *std::get<std::string*>(m_data) = *val;
        }
        else if (m_type == OptionType::STRING)
        {
            *std::get<std::string*>(m_data) = value;
        }

        // On-change callback
        if (std::visit([](auto func){ return func != nullptr; }, m_change))
        {
            if (m_type == CHECK)
                std::get<OnChange<bool>>(m_change)(*std::get<bool*>(m_data));
            else if (m_type == SPIN)
                std::get<OnChange<int>>(m_change)(*std::get<int*>(m_data));
            else if (m_type == BUTTON)
                std::get<OnChange<>>(m_change)();
            else
                std::get<OnChange<std::string>>(m_change)(*std::get<std::string*>(m_data));
        }
    }



    std::ostream& operator<<(std::ostream& out, const Option& option)
    {
        std::array<std::string, 5> types{ "check", "spin", "combo", "button", "string" };
        out << " type " << types[option.m_type];
        
        // Button type has no default
        if (option.m_type != BUTTON)
        {
            out << " default ";
            // Ensure we print true or false for check types
            if (option.m_type == CHECK)
                out << (std::get<bool>(option.m_default) ? "true" : "false");
            else
                std::visit([&out](auto value){ out << value; }, option.m_default);

            // Spin has min and max
            if (option.m_type == SPIN)
                out << " min " << option.m_min << " max " << option.m_max;
            // Combo has the list of vars
            else if (option.m_type == COMBO)
                for (std::string var : option.m_var)
                    out << " var " << var;
        }

        return out;
    }



    void init_options()
    {
        OptionsMap.emplace("Clear Hash", Option(OnChange<>([]() { ttable.clear(); })));
        OptionsMap.emplace("Hash",       Option(&Options::Hash, 16, 1, ttable.max_size(),
                                                [](int v) { ttable.resize(v); }));
        OptionsMap.emplace("MultiPV",    Option(&Options::MultiPV, 1, 1, 255));
        OptionsMap.emplace("Threads",    Option(&Options::Threads, 1, 1, 512,
                                                [](int v) { pool->resize(v); }));
        OptionsMap.emplace("Ponder",     Option(&Options::Ponder, false));
    }



    void main_loop()
    {
        std::string token;
        while (token != "quit")
        {
            std::string cmd;
            std::getline(std::cin, cmd);
            Stream stream(cmd);

            // Read the first token
            stream >> token;

            // Switch on the received token
            if (token == "quit")
                quit(stream);
            else if (token == "stop")
                stop(stream);
            else if (token == "uci")
                uci(stream);
            else if (token == "setoption")
                setoption(stream);
            else if (token == "isready")
                isready(stream);
            else if (token == "ucinewgame")
                ucinewgame(stream);
            else if (token == "go")
                go(stream);
            else if (token == "position")
                position(stream);
            else if (token == "ponderhit")
                ponderhit(stream);

            // Non-UCI commands
            else if (token == "board")
                std::cout << pool->position().board() << std::endl;
            else if (token == "eval")
                evaluate<true>(pool->position());
            else if (token == "test")
            {
                int t1 = Tests::perft_tests();
                int t2 = Tests::perft_techniques_tests<false, true, false>();
                int t3 = Tests::perft_techniques_tests<true, false, false>();
                int t4 = Tests::perft_techniques_tests<true,  true, false>();
                int t5 = Tests::perft_techniques_tests<false, false, true>();

                std::cout << "\nTest summary" << std::endl;
                std::cout << "  Perft:        " << t1 << " failed cases" << std::endl;
                std::cout << "  TT:           " << t2 << " failed cases" << std::endl;
                std::cout << "  Orderer:      " << t3 << " failed cases" << std::endl;
                std::cout << "  TT + Orderer: " << t4 << " failed cases" << std::endl;
                std::cout << "  Legality:     " << t5 << " failed cases" << std::endl;
            }
        }
    }



    void uci(Stream& stream)
    {
        std::cout << "id name hive" << std::endl;
        std::cout << "id author NULL" << std::endl;

        // Send options
        std::cout << std::endl;
        for (const auto &[name, option] : OptionsMap)
            std::cout << "option name " << name << option << std::endl;

        // Mandatory uciok at the end
        std::cout << "uciok" << std::endl;
    }



    void setoption(Stream& stream)
    {
        std::string token;
        stream >> token;
        if (stream && token == "name")
        {
            // Read option name (can contain spaces)
            std::string name;
            while (stream >> token && token != "value")
                name += name.empty() ? token : (" " + token);

            // Read value (can also contain spaces)
            std::string value;
            while (stream >> token)
                value += value.empty() ? token : (" " + token);

            OptionsMap.at(name).set(value);
        }
    }



    void go(Stream& stream)
    {
        std::string token;
        int perft_depth = 0;
        Search::Timer timer;
        Search::Limits limits;

        while (stream >> token)
            if (token == "searchmoves")
                while (stream >> token)
                    limits.searchmoves.push_back(move_from_uci(pool->position(), token));
            else if (token == "wtime")
                stream >> limits.time[WHITE];
            else if (token == "btime")
                stream >> limits.time[BLACK];
            else if (token == "winc")
                stream >> limits.incr[WHITE];
            else if (token == "binc")
                stream >> limits.incr[BLACK];
            else if (token == "movestogo")
                stream >> limits.movestogo;
            else if (token == "depth")
                stream >> limits.depth;
            else if (token == "nodes")
                stream >> limits.nodes;
            else if (token == "movetime")
                stream >> limits.movetime;
            else if (token == "mate")
                stream >> limits.mate;
            else if (token == "infinite")
                limits.infinite = true;
            else if (token == "ponder")
                limits.ponder = true;
            else if (token == "perft")
                stream >> perft_depth;

        // Check if perft search
        if (perft_depth > 0)
        {
            int64_t nodes = Search::perft<true>(pool->position(), perft_depth);
            std::cout << "\nNodes searched: " << nodes << std::endl;
            return;
        }

        pool->search(timer, limits);
    }



    void stop(Stream& stream)
    {
        pool->stop();
    }



    void quit(Stream& stream)
    {
        pool->kill_threads();
    }



    void position(Stream& stream)
    {
        Position& pos =  pool->position();

        std::string token;
        stream >> token;

        if (token == "startpos")
        {
            pos = Position();

            // Consume moves token, if passed
            while (stream >> token && token != "moves");
        }
        else if (token == "fen")
        {
            // Build FEN string
            std::string fen;
            while (stream >> token && token != "moves")
                fen += token + " ";
            pos = Position(fen);
        }
        else
            return;

        // Push moves to the position
        Move move;
        while (stream >> token && (move = move_from_uci(pos, token)) != MOVE_NULL)
        {
            pos.make_move(move);
            pos.set_init_ply();
        }

        pool->update_position_threads();
    }



    void ponderhit(Stream& stream)
    {
        pool->ponderhit();
    }



    void ucinewgame(Stream& stream)
    {
        ttable.clear();
    }



    void isready(Stream& stream)
    {
        // Mandatory readyok output when all set
        std::cout << "readyok" << std::endl;
    }



    Move move_from_uci(Position& position, std::string move_str)
    {
        // Get moves for current position
        auto list = position.generate_moves(MoveGenType::LEGAL);

        // Search for the move in the move list
        for (auto& move : list)
            if (move.to_uci() == move_str)
                return move;

        return MOVE_NULL;
    }
}
