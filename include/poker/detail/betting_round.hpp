#pragma once

#include <poker/player.hpp>

#include "poker/detail/round.hpp"
#include "poker/detail/seat_array.hpp"

namespace poker {

struct chip_range {
    chips min;
    chips max;

    auto contains(chips amount) const noexcept -> bool {
        return min <= amount && amount <= max;
    }
};

} // namespace poker

namespace poker::detail {

class betting_round {
public:
    //
    // Constants
    //
    static constexpr auto max_players = 9;

    //
    // Types
    //
    /* using player_container = std::array<player*, max_players>; */

    enum class action { leave, match, raise };

    struct action_range {
        bool can_raise;
        poker::chip_range chip_range = {0, 0};
    };

    //
    // Special functions
    //
    betting_round()                     = default;
    betting_round(const betting_round&) = delete;
    betting_round(betting_round&&)      = delete;
    auto operator=(const betting_round&) -> betting_round& = delete;
    auto operator=(betting_round&&)      -> betting_round& = delete;

    //
    // Constructors
    //
    /* betting_round(const player_container& players, player_container::const_iterator current, chips min_raise) noexcept; */
    /* betting_round(seat_array& players, std::array<bool, max_players> active_players, seat_index current, chips min_raise) noexcept; */
    betting_round(seat_array_view players, seat_index current, chips min_raise) noexcept;

    //
    // Observers
    //
    auto in_progress()        const noexcept -> bool;
    /* auto player_to_act()      const noexcept -> player_container::const_iterator; */
    auto player_to_act() const noexcept -> seat_index;
    auto biggest_bet()        const noexcept -> chips;
    auto min_raise()          const noexcept -> chips;
    /* auto players()            const noexcept -> const player_container&; */
    auto players() const noexcept -> seat_array_view;
    auto active_players() const noexcept -> const std::array<bool,max_players>&;
    auto num_active_players() const noexcept -> std::size_t;
    auto legal_actions()      const noexcept -> action_range;

    //
    // Modifiers
    //
    void action_taken(action, chips bet = 0) noexcept;

private:
    auto is_raise_valid(chips bet) const noexcept -> bool;

public: // for testing only
    round _round;
private:
    /* player_container _players; */
    seat_array* _players = nullptr;
    chips _biggest_bet = 0;
    chips _min_raise = 0;
};

/* inline betting_round::betting_round(const player_container& players, player_container::const_iterator current, chips min_raise) noexcept */
/*     : _players{players} */
/*     , _biggest_bet{min_raise} */
/*     , _min_raise{min_raise} */
/* { */
/*     auto active_players = std::array<bool, max_players>{}; */
/*     std::transform(std::cbegin(_players), std::cend(_players), std::begin(active_players), [] (player* p) -> bool { return !!p; }); */
/*     auto first_to_act = static_cast<std::size_t>(std::distance(std::cbegin(players), current)); */
/*     _round = {active_players, first_to_act}; */
/* } */

/* inline betting_round::betting_round(seat_array& players, std::array<bool,max_players> active_players, seat_index current, chips min_raise) noexcept */
/*     : _round{active_players, current} */
/*     , _players{&players} */
/*     , _biggest_bet{min_raise} */
/*     , _min_raise{min_raise} */
/* { */
/* } */

inline betting_round::betting_round(seat_array_view players, seat_index current, chips min_raise) noexcept
    : _round{players.filter(), current}
    , _players{&players.underlying()}
    , _biggest_bet{min_raise}
    , _min_raise{min_raise}
{
}

inline auto betting_round::in_progress()        const noexcept -> bool                             { return _round.in_progress();        }

/* inline auto betting_round::player_to_act() const noexcept -> player_container::const_iterator { */
/*     return std::cbegin(_players) + _round.player_to_act(); */
/* } */

inline auto betting_round::player_to_act() const noexcept -> seat_index {
    return _round.player_to_act();
}

inline auto betting_round::biggest_bet()        const noexcept -> chips                            { return _biggest_bet;                }
inline auto betting_round::min_raise()          const noexcept -> chips                            { return _min_raise;                  }

/* inline auto betting_round::players() const noexcept -> const player_container& { */
/*     return _players; */
/* } */

inline auto betting_round::players() const noexcept -> seat_array_view {
    return {*_players, _round.active_players()};
}

inline auto betting_round::active_players() const noexcept -> const std::array<bool,max_players>& {
    return _round.active_players();
}

inline auto betting_round::num_active_players() const noexcept -> std::size_t                      { return _round.num_active_players(); }

inline auto betting_round::legal_actions() const noexcept -> action_range {
    // A player can raise if his stack+bet_size is greater than _biggest_bet
    const auto& player = (*_players)[_round.player_to_act()];
    const auto player_chips = player.total_chips();
    const auto can_raise = player_chips > _biggest_bet;
    if (can_raise) {
        const auto min_bet = _biggest_bet + _min_raise;
        const auto raise_range = chip_range{std::min(min_bet, player_chips), player_chips};
        return {can_raise, raise_range};
    } else {
        return {can_raise};
    }
}

inline void betting_round::action_taken(action a, chips bet/*= 0*/) noexcept {
    // chips bet is ignored when not needed
    auto& player = (*_players)[_round.player_to_act()];
    if (a == action::raise) {
        assert(is_raise_valid(bet));
        player.bet(bet);
        _min_raise = bet - _biggest_bet;
        _biggest_bet = bet;
        auto action_flag = round::action::aggressive;
        if (player.stack() == 0) {
            action_flag |= round::action::leave;
            /* _players[_round.player_to_act()] = nullptr; */
        }
        _round.action_taken(action_flag);
    } else if (a == action::match) {
        player.bet(std::min(_biggest_bet, player.total_chips()));
        auto action_flag = round::action::passive;
        if (player.stack() == 0) {
            action_flag |= round::action::leave;
            /* _players[_round.player_to_act()] = nullptr; */
        }
        _round.action_taken(action_flag);
    } else {
        assert(a == action::leave);
        /* _players[_round.player_to_act()] = nullptr; */
        _round.action_taken(round::action::leave);
    }
}

inline auto betting_round::is_raise_valid(chips bet) const noexcept -> bool {
    const auto& player = (*_players)[_round.player_to_act()];
    const auto player_chips = player.stack() + player.bet_size();
    const auto min_bet = _biggest_bet + _min_raise;
    if (player_chips > _biggest_bet && player_chips < min_bet)
        return bet == player_chips;
    else
        return bet >= min_bet && bet <= player_chips;
}

} // namespace poker::detail
