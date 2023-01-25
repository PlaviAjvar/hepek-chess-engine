#ifndef HEPEK_CHESS_ENGINE_RULES_H
#define HEPEK_CHESS_ENGINE_RULES_H

#include <vector>
#include <string>
#include <memory>

namespace chess {
    typedef unsigned long long bitmap;
    typedef int square;
    const square INVALID_SQUARE = -1;

    enum Player {
        WHITE = 0, BLACK = 1
    };
    enum Piece {
        KING = 0, QUEEN = 1, ROOK = 2, BISHOP = 3, KNIGHT = 4, PAWN = 5
    };
    enum CastlingVariant {
        KING_SIDE = 0, QUEEN_SIDE = 1
    };

    class GameState;

    class Move {
    protected:
        Player to_move;

    public:
        explicit Move(Player to_move) : to_move(to_move) {}

        virtual GameState transform(const GameState &state) const = 0;

        virtual ~Move() = default;
    };

    class NormalMove : public Move {
    protected:
        square start, finish;
        Piece piece;
        bool is_capture;

    public:
        NormalMove(square start, square finish, Piece piece, Player to_move, bool is_capture) :
                start(start), finish(finish), piece(piece), Move(to_move), is_capture(is_capture) {}

        GameState transform(const GameState &state) const override;

        ~NormalMove() override = default;
    };

    class PromotionMove : public NormalMove {
    private:
        Piece promoted_piece;

    public:
        PromotionMove(square start, square finish, Player to_move, Piece promoted_piece) :
                NormalMove(start, finish, Piece::PAWN, to_move, false), promoted_piece(promoted_piece) {}

        GameState transform(const GameState &state) const override;
    };

    class CastlingMove : public Move {
    private:
        CastlingVariant variant;
    public:
        CastlingMove(CastlingVariant variant, Player to_move) : Move(to_move), variant(variant) {}

        GameState transform(const GameState &state) const override;
    };

    class GameState {
    private:
        Player to_move;
        bitmap pieces[2][6]{};
        int half_move_counter;
        bool can_castle_king_side[2]{}, can_castle_queen_side[2]{};
        square en_passant_square;
        // Make sure that moves can access the GameState class
        friend Move;
        friend NormalMove;
        friend PromotionMove;
        friend CastlingMove;

    public:
        GameState();

        GameState(Player to_move, bitmap **pieces, int half_move_counter, bool *can_castle_king_side,
                  bool *can_castle_queen_side, square en_passant_square);

    private:
        bitmap span(square, Player, Piece) const;

        bitmap span_sliding(square, Player, const int *, Piece) const;

        bitmap span_jumping(square, Player, const int *, Piece) const;

        bitmap span_king(square, Player) const;

        bitmap span_queen(square, Player) const;

        bitmap span_rook(square, Player) const;

        bitmap span_bishop(square, Player) const;

        bitmap span_knight(square, Player) const;

        bitmap span_pawn(square, Player) const;

        bitmap attacking(square, Player, Piece) const;

        bitmap attacking_pawn(square, Player) const;

        bitmap get_occupancy_map() const;

        bool in_check_after_move(const std::unique_ptr<Move> &) const;

        bool king_side_castling_conditions_satisfied() const;

        bool queen_side_castling_conditions_satisfied() const;

        bool is_occupied(square) const;

        bool no_valid_moves() const;

        square get_king_position(Player player) const;

        square get_attack_map(Player player) const;

        Player square_ownership(square) const;

    public:
        bool is_check() const;
//
//        bool is_checkmate() const;
//
//        bool is_stalemate() const;

//    bool is_draw(const std::vector<GameState> &) const;

        std::vector<std::unique_ptr<Move>> get_valid_moves() const;

//    std::vector<GameState> reachable_positions() const;

        static square get_lowest_bit(bitmap);
    };
}


#endif //HEPEK_CHESS_ENGINE_RULES_H
