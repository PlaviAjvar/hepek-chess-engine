#include <exception>
#include <cassert>
#include <stdexcept>
#include "rules.h"

namespace chess {
    /*****************************
     * GameState constructors
     *****************************/
    GameState::GameState() {
        to_move = Player::WHITE;
        half_move_counter = 0;
        std::fill(can_castle_king_side, can_castle_king_side + 2, true);
        std::fill(can_castle_queen_side, can_castle_queen_side + 2, true);
        en_passant_square = INVALID_SQUARE;

        // Fill starting board
        std::fill(&pieces[0][0], &pieces[0][0] + 12, 0ULL);

        pieces[Player::WHITE][Piece::KING] |= (1ULL << 4);
        pieces[Player::BLACK][Piece::KING] |= (1ULL << 60);

        pieces[Player::WHITE][Piece::QUEEN] |= (1ULL << 3);
        pieces[Player::BLACK][Piece::QUEEN] |= (1ULL << 59);

        pieces[Player::WHITE][Piece::ROOK] |= ((1ULL << 0) | (1ULL << 7));
        pieces[Player::BLACK][Piece::ROOK] |= ((1ULL << 56) | (1ULL << 63));

        pieces[Player::WHITE][Piece::BISHOP] |= ((1ULL << 2) | (1ULL << 5));
        pieces[Player::BLACK][Piece::BISHOP] |= ((1ULL << 58) | (1ULL << 61));

        pieces[Player::WHITE][Piece::KNIGHT] |= ((1ULL << 1) | (1ULL << 6));
        pieces[Player::BLACK][Piece::KNIGHT] |= ((1ULL << 57) | (1ULL << 62));

        for (int i = 8; i < 16; ++i) {
            pieces[Player::WHITE][Piece::PAWN] |= (1ULL << i);
            pieces[Player::BLACK][Piece::PAWN] |= (1ULL << (63 - i));
        }
    }

    GameState::GameState(Player to_move, bitmap **pieces, int half_move_counter, bool *can_castle_king_side,
                         bool *can_castle_queen_side, square en_passant_square)
            : to_move(to_move), half_move_counter(half_move_counter),
              en_passant_square(en_passant_square) {
        std::copy(&pieces[0][0], &pieces[0][0] + 12, &(this->pieces[0][0]));
        std::copy(can_castle_king_side, can_castle_king_side + 2, this->can_castle_king_side);
        std::copy(can_castle_queen_side, can_castle_queen_side + 2, this->can_castle_queen_side);
    }


    /*****************************
     * GameState member functions
     *****************************/

    // NOTE: Should be optimized
    square GameState::get_lowest_bit(const bitmap map) {
        square lowest_bit = 0;
        bitmap lowest_power_of_two = map & (-map);

        while (lowest_power_of_two > 0) {
            lowest_power_of_two >>= 1;
            ++lowest_bit;
        }

        return lowest_bit;
    }

    bitmap GameState::get_occupancy_map() const {
        bitmap mask = 0;
        for (int i = 0; i < 6; ++i) {
            mask |= (pieces[0][i] | pieces[1][i]);
        }
        return mask;
    }

    square GameState::get_attack_map(const Player player) const {
        bitmap attack_map = 0;

        for (int i = 0; i < 6; ++i) {
            bitmap piece_locations = pieces[player][i];
            const Piece piece_type(static_cast<Piece>(i));

            while (piece_locations > 0) {
                const square start = get_lowest_bit(piece_locations);
                bitmap piece_attack = attacking(start, player, piece_type);
                attack_map |= piece_attack;
                piece_locations ^= (1ULL << start);
            }
        }
    }

    square GameState::get_king_position(const Player player) const {
        return get_lowest_bit(pieces[player][0]);
    }

    // NOTE: Should be optimized
    bool GameState::in_check_after_move(const std::unique_ptr<Move> &move) const {
        GameState new_state = move->transform(*this);
        const bitmap attack_map = new_state.get_attack_map(static_cast<Player>(to_move ^ 1));
        const square king_position = new_state.get_king_position(to_move);
        return attack_map & (1ULL << king_position);
    }

    bool GameState::king_side_castling_conditions_satisfied() const {
        bitmap in_between_squares, passing_squares;
        if (to_move == Player::WHITE) {
            in_between_squares = (1ULL << 5) | (1ULL << 6);
            passing_squares = (1ULL << 4) | (1ULL << 5) | (1ULL << 6);
        } else {
            in_between_squares = (1ULL << 61) | (1ULL << 62);
            passing_squares = (1ULL << 60) | (1ULL << 61);
        }

        const bitmap attack_map = get_attack_map(static_cast<Player>(to_move ^ 1));
        const bitmap occupancy_map = get_occupancy_map();

        if (passing_squares & attack_map) return false;
        if (in_between_squares & occupancy_map) return false;
        if (!can_castle_king_side[to_move]) return false;
        return true;
    }

    bool GameState::queen_side_castling_conditions_satisfied() const {
        bitmap in_between_squares, passing_squares;
        if (to_move == Player::WHITE) {
            in_between_squares = (1ULL << 1) | (1ULL << 2) | (1ULL << 3);
            passing_squares = (1ULL << 2) | (1ULL << 3) | (1ULL << 4);
        } else {
            in_between_squares = (1ULL << 57) | (1ULL << 58) | (1ULL << 59);
            passing_squares = (1ULL << 58) | (1ULL << 59) | (1ULL << 60);
        }

        const bitmap attack_map = get_attack_map(static_cast<Player>(to_move ^ 1));
        const bitmap occupancy_map = get_occupancy_map();

        if (passing_squares & attack_map) return false;
        if (in_between_squares & occupancy_map) return false;
        if (!can_castle_queen_side[to_move]) return false;
        return true;
    }

    std::vector<std::unique_ptr<Move>> GameState::get_valid_moves() const {
        std::vector<std::unique_ptr<Move>> valid_moves;

        // Check non-castling moves
        for (int i = 0; i < 6; ++i) {
            bitmap piece_locations = pieces[to_move][i];
            const Piece piece_type(static_cast<Piece>(i));

            while (piece_locations > 0) {
                const square start = get_lowest_bit(piece_locations);
                bitmap piece_span = span(start, to_move, piece_type);

                while (piece_span > 0) {
                    const square finish = get_lowest_bit(piece_span);

                    // Check if the move promotes a pawn
                    if (piece_type == Piece::PAWN && (finish < 8 || finish >= 56)) {
                        for (const Piece promoted_piece: {Piece::QUEEN, Piece::ROOK, Piece::BISHOP, Piece::KNIGHT}) {
                            std::unique_ptr<Move> promotion_move = std::make_unique<PromotionMove>(
                                    start, finish, to_move, promoted_piece);

                            if (!in_check_after_move(promotion_move)) {
                                valid_moves.emplace_back(std::move(promotion_move));
                            }
                        }
//                    Promotion queen_promotion(start, finish, to_move, Piece::QUEEN);
//                    Promotion rook_promotion(start, finish, to_move, Piece::ROOK);
//                    Promotion bishop_promotion(start, finish, to_move, Piece::BISHOP);
//                    Promotion knight_promotion(start, finish, to_move, Piece::KNIGHT);
//                    candidate_move.emplace_back(queen_promotion);
//                    candidate_move.emplace_back(rook_promotion);
//                    candidate_move.emplace_back(bishop_promotion);
//                    candidate_move.emplace_back(knight_promotion);

                    } else {
                        // Also check if destination is occupied (by opposing piece)
                        bool is_capture = is_occupied(finish);

                        std::unique_ptr<Move> candidate_move = std::make_unique<NormalMove>(
                                start, finish, piece_type, to_move, is_capture);
                        if (!in_check_after_move(candidate_move)) {
                            valid_moves.emplace_back(std::move(candidate_move));
                        }
                    }

                    piece_span ^= finish;
                }

                piece_locations ^= (1ULL << start);
            }
        }

        // Check castling
        if (king_side_castling_conditions_satisfied()) {
            std::unique_ptr<Move> castling_move = std::make_unique<CastlingMove>(
                    CastlingVariant::KING_SIDE, to_move);
            valid_moves.emplace_back(std::move(castling_move));
        }

        if (queen_side_castling_conditions_satisfied()) {
            std::unique_ptr<Move> castling_move = std::make_unique<CastlingMove>(
                    CastlingVariant::QUEEN_SIDE, to_move);
            valid_moves.emplace_back(std::move(castling_move));
        }

        return valid_moves;
    }

    bitmap GameState::span(const square start, const Player player, const Piece piece_type) const {
        assert(pieces[to_move][piece_type] & (1ULL << start));
        if (piece_type == Piece::KING) return span_king(start, player);
        if (piece_type == Piece::QUEEN) return span_queen(start, player);
        if (piece_type == Piece::ROOK) return span_rook(start, player);
        if (piece_type == Piece::BISHOP) return span_bishop(start, player);
        if (piece_type == Piece::KNIGHT) return span_knight(start, player);
        if (piece_type == Piece::PAWN) return span_pawn(start, player);
        throw std::runtime_error("Something went horribly wrong. None of the valid pieces selected.");
    }

    bitmap GameState::span_pawn(const square start, const Player player) const {
        assert(pieces[player][Piece::PAWN] & (1ULL << start));
        bitmap span_mask;
        square finish;
        int direction_modifier = (player == Player::WHITE) ? 1 : -1;

        // Normal pawn forward
        bool can_move_forward = false;
        finish = start + direction_modifier * 8;
        if (!is_occupied(finish)) {
            span_mask |= finish;
            can_move_forward = true;
        }

        // Pawn captures
        const int direction_offset[] = {7, 9};
        for (int i = 0; i < 2; ++i) {
            finish = start + direction_modifier * direction_offset[i];
            if (is_occupied(finish) && square_ownership(finish) != player || finish == en_passant_square)
                span_mask |= finish;
        }

        // Moving forward two squares if pawn is on back rank
        if ((player == Player::WHITE && start >= 8 && start < 16) ||
            (player == Player::BLACK && start >= 48 && start < 56)) {
            finish = start + direction_modifier * 16;
            if (can_move_forward && !is_occupied(finish)) {
                span_mask |= finish;
            }
        }

        return span_mask;
    }

    bitmap GameState::span_jumping(const square start, const Player player, const int *direction_offset,
                                   const Piece piece_type) const {
        assert(pieces[player][piece_type] & (1ULL << start));
        bitmap span_mask;

        for (int i = 0; i < 8; ++i) {
            square current = start + direction_offset[i];
            if (current >= 0 && current <= 63 && (!is_occupied(current) || square_ownership(current) != player)) {
                span_mask |= (1ULL << current);
            }
        }

        return span_mask;
    }

    bitmap GameState::span_king(const square start, const Player player) const {
        const int direction_offset[] = {-1, 7, 8, 9, 1, -7, -8, -9};
        return span_jumping(start, player, direction_offset, Piece::KING);
    }

    bitmap GameState::span_knight(const square start, const Player player) const {
        const int direction_offset[] = {6, 15, 17, 10, -6, -15, -17, -10};
        return span_jumping(start, player, direction_offset, Piece::KNIGHT);
    }

    bitmap GameState::span_sliding(const square start, const Player player, const int *direction_offset,
                                   const Piece piece_type) const {
        assert(pieces[player][piece_type] & (1ULL << start));
        bitmap span_mask;
        const int num_directions = (piece_type == Piece::QUEEN) ? 8 : 4;

        for (int i = 0; i < num_directions; ++i) {
            square current = start;
            while (true) {
                current = current + direction_offset[i];
                if (current < 0 || current > 63) break;
                if (is_occupied(current)) {
                    if (square_ownership(current) != player) span_mask |= (1ULL << current);
                    break;
                }
            }
        }

        return span_mask;
    }

    bitmap GameState::span_queen(const square start, const Player player) const {
        const int direction_offset[] = {-1, 7, 8, 9, 1, -7, -8, -9};
        return span_sliding(start, player, direction_offset, Piece::QUEEN);
    }

    bitmap GameState::span_rook(const square start, const Player player) const {
        const int direction_offset[] = {-1, 8, 1, -8};
        return span_sliding(start, player, direction_offset, Piece::ROOK);
    }

    bitmap GameState::span_bishop(const square start, const Player player) const {
        const int direction_offset[] = {7, 9, -7, -9};
        return span_sliding(start, player, direction_offset, Piece::BISHOP);
    }

    bitmap GameState::attacking(const square start, const Player player, const Piece piece) const {
        // For all non-pawn pieces, the span and the attacked squares are the same
        if (piece != Piece::PAWN) return span(start, player, piece);
        return attacking_pawn(start, player);
    }

    bitmap GameState::attacking_pawn(const square start, const Player player) const {
        assert(pieces[player][Piece::PAWN] & (1ULL << start));
        int direction_modifier = (player == Player::WHITE) ? 1 : -1;
        return (1ULL << (start + direction_modifier * 7)) | (1ULL << (start + direction_modifier * 9));
    }

    bool GameState::is_check() const {
        bitmap attack_map = get_attack_map(static_cast<Player>(to_move ^ 1));
        const bitmap king_position = get_king_position(to_move);
        return attack_map & (1ULL << king_position);
    }

    bool GameState::is_checkmate() const {
        return is_check() && no_valid_moves();
    }

    bool GameState::is_stalemate() const {
        return !is_check() && no_valid_moves();
    }

    // NOTE: Should be optimized
    bool GameState::no_valid_moves() const {
        return !get_valid_moves().empty();
    }

    bool GameState::is_occupied(const square query) const {
        for (int i = 0; i < 6; ++i) {
            if (pieces[Player::WHITE][i] & (1ULL << query))
                return true;
            if (pieces[Player::BLACK][i] & (1ULL << query))
                return true;
        }
        return false;
    }

    /*****************************
     * Move member functions
     *****************************/
    GameState NormalMove::transform(const GameState &state) const {
        // Flip turn player
        Player to_move = static_cast<Player>(state.to_move ^ 1);

        // Update bitboards
        bitmap pieces[2][6];
        std::copy(&state.pieces[0][0], &state.pieces[0][0] + 12, &pieces[0][0]);
        if (is_capture) {
            for (int i = 0; i < 6; ++i) {
                pieces[state.to_move ^ 1][i] &= (~(1ULL << finish));
            }
        }
        pieces[state.to_move][piece] ^= (1ULL << start);
        pieces[state.to_move][piece] |= (1ULL << finish);

        // Update fifty-move rule counter
        int half_move_counter;
        if (is_capture || piece == Piece::PAWN)
            half_move_counter = 0;
        else
            half_move_counter = state.half_move_counter + 1;

        // Update castling permissions
        bool can_castle_king_side[2], can_castle_queen_side[2];
        std::copy(state.can_castle_king_side, state.can_castle_king_side + 2, can_castle_king_side);
        std::copy(state.can_castle_queen_side, state.can_castle_queen_side + 2, can_castle_queen_side);
        const square king_side_rook_square = (state.to_move == Player::WHITE) ? 7 : 63;
        const square queen_side_rook_square = (state.to_move == Player::WHITE) ? 0 : 56;

        if ((pieces[state.to_move][Piece::ROOK] & (1ULL << king_side_rook_square)) == 0) {
            can_castle_king_side[state.to_move] = false;
        }
        if ((pieces[state.to_move][Piece::ROOK] & (1ULL << queen_side_rook_square)) == 0) {
            can_castle_queen_side[state.to_move] = false;
        }

        // Check is en passant condition is met
        square en_passant_square = INVALID_SQUARE;
        if (piece == Piece::PAWN) {
            int travel_distance = std::abs(finish - start);
            if (travel_distance == 16) {
                en_passant_square = std::min(start, finish) + 8;
            }
        }

        return GameState(to_move, (bitmap **) pieces, half_move_counter, can_castle_king_side, can_castle_queen_side,
                         en_passant_square);
    }

    GameState PromotionMove::transform(const GameState &state) const {
        assert(piece == Piece::PAWN);

        // Flip turn player
        Player to_move = static_cast<Player>(state.to_move ^ 1);

        // Update bitboards
        bitmap pieces[2][6];
        std::copy(&state.pieces[0][0], &state.pieces[0][0] + 12, &pieces[0][0]);
        pieces[state.to_move][Piece::PAWN] ^= (1ULL << start);
        pieces[state.to_move][promoted_piece] |= (1ULL << finish);

        return GameState(to_move, (bitmap **) pieces, 0, state.can_castle_king_side, state.can_castle_queen_side,
                         INVALID_SQUARE);
    }

    GameState CastlingMove::transform(const GameState &state) const {
        // Flip turn player
        Player to_move = static_cast<Player>(state.to_move ^ 1);

        // Update bitboards
        bitmap pieces[2][6];
        std::copy(&state.pieces[0][0], &state.pieces[0][0] + 12, &pieces[0][0]);

        const square king_square = (state.to_move == Player::WHITE) ? 4 : 60;
        square rook_square, new_king_square, new_rook_square;

        if (variant == CastlingVariant::KING_SIDE) {
            assert(state.can_castle_king_side[state.to_move]);
            rook_square = (state.to_move == Player::WHITE) ? 7 : 63;
            new_king_square = king_square + 2;
            new_rook_square = rook_square - 2;
        } else {
            assert(state.can_castle_queen_side[state.to_move]);
            rook_square = (state.to_move == Player::WHITE) ? 0 : 56;
            new_king_square = king_square - 2;
            new_rook_square = rook_square + 3;
        }

        pieces[state.to_move][Piece::KING] ^= (1ULL << king_square);
        pieces[state.to_move][Piece::KING] |= (1ULL << new_king_square);
        pieces[state.to_move][Piece::ROOK] ^= (1ULL << rook_square);
        pieces[state.to_move][Piece::ROOK] |= (1ULL << new_rook_square);

        // Update fifty-move rule counter
        int half_move_counter = state.half_move_counter + 1;

        // Update castling permissions
        bool can_castle_king_side[2], can_castle_queen_side[2];
        std::copy(state.can_castle_king_side, state.can_castle_king_side + 2, can_castle_king_side);
        std::copy(state.can_castle_queen_side, state.can_castle_queen_side + 2, can_castle_queen_side);
        can_castle_king_side[state.to_move] = false;
        can_castle_queen_side[state.to_move] = false;

        return GameState(to_move, (bitmap **) pieces, half_move_counter, can_castle_king_side, can_castle_queen_side,
                         INVALID_SQUARE);
    }

    Player GameState::square_ownership(square query) const {
        for (int i = 0; i < 6; ++i) {
            if (pieces[Player::WHITE][i] & (1ULL << query))
                return Player::WHITE;
            if (pieces[Player::BLACK][i] & (1ULL << query))
                return Player::BLACK;
        }
        throw std::logic_error("Square is not owned by either player");
    }

}