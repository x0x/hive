#pragma once
#include "types.hpp"
#include "position.hpp"
#include "move.hpp"
#include "hash.hpp"


enum class MoveStage
{
    HASH,
    CAPTURES_INIT,
    CAPTURES,
    CAPTURES_END,
    COUNTERMOVES,
    KILLERS,
    QUIET_INIT,
    QUIET,
    NO_MOVES
};

constexpr MoveStage operator++(MoveStage& current);


constexpr int NUM_KILLERS = 3;
constexpr int NUM_LOW_PLY = 5;


class Histories
{
    Move m_killers[NUM_KILLERS][NUM_MAX_DEPTH];
    int m_butterfly[NUM_COLORS][NUM_SQUARES][NUM_SQUARES];
    int m_piece_type[NUM_PIECE_TYPES][NUM_SQUARES];
    Move m_countermoves[NUM_SQUARES][NUM_SQUARES];

public:
    Histories();

    void clear();

    void add_bonus(Move move, Turn turn, PieceType piece, int bonus);
    void fail_high(Move move, Move prev_move, Turn turn, Depth depth, Depth ply, PieceType piece);

    bool is_killer(Move move, Depth ply) const;
    int butterfly_score(Move move, Turn turn) const;
    int piece_type_score(Move move, PieceType piece) const;
    Move countermove(Move move) const;
    Move get_killer(int index, Depth ply) const;
};


class MoveOrder
{
    Position& m_position;
    Depth m_ply;
    Depth m_depth;
    Move m_hash_move;
    const Histories& m_histories;
    Move m_prev_move;
    bool m_quiescence;
    MoveList m_moves;
    MoveStage m_stage;
    Move m_countermove;
    Move m_killer;
    Move* m_curr;

    bool hash_move(Move& move);


    template<bool CAPTURES>
    int move_score(Move move) const
    {
        if (CAPTURES)
            return capture_score(move);
        else
            return quiet_score(move);
    }


    bool next(Move& move)
    {
        if (m_curr == m_moves.end())
            return false;

        move = *(m_curr++);
        return true;
    }


    template<bool CAPTURES>
    MoveList threshold_moves(MoveList& list, int threshold)
    {
        Move* pos = list.begin();
        for (auto list_move = list.begin(); list_move != list.end(); list_move++)
        {
            if (move_score<CAPTURES>(*list_move) > threshold)
            {
                if (pos != list_move)
                    std::swap(*pos, *list_move);
                pos++;
            }
        }

        return MoveList(list.begin(), pos);
    }


    template<bool CAPTURES>
    void sort_moves(MoveList list) const
    {
        std::sort(list.begin(), list.end(), [this](Move a, Move b)
                  {
                      return move_score<CAPTURES>(a) > move_score<CAPTURES>(b);
                  });
    }


public:
    MoveOrder(Position& pos, Depth ply, Depth depth, Move hash_move, const Histories& histories, Move prev_move, bool quiescence = false);

    Move next_move();

    int capture_score(Move move) const;
    int quiet_score(Move move) const;
};
