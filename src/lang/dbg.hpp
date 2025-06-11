// -*- C++ -*-
// dbg.hpp
//

#pragma once

#include "bse.hpp"
#include "core.hpp"

#include <utils/common.hpp>
#include <utils/typeinfo.hpp>

#include <fmt/core.h>
#include <fmt/format.h>
#include <fmt/ranges.h>

#include <algorithm>
#include <bitset>
#include <cerrno>
#include <charconv>
#include <cstdio>
#include <iostream>
#include <optional>
#include <ranges>
#include <string>

#include <termios.h>
#include <unistd.h>

namespace lucid::dbg
{
template <typename T>
struct prettify;

template <typename T, typename N>
struct prettify<Variable<T, N>>
{
    std::string
    operator()(bool type_signature, unsigned, unsigned) const noexcept
    {
        if(type_signature)
            return fmt::format("{}:{}",
                               Variable<T, N>::nameview,
                               TypeInfo<T>::chars);
        else
            return std::string(Variable<T, N>::nameview);
    }
};

template <typename Name>
struct prettify<Label<Name>>
{
    template <typename... _>
    std::string_view
    operator()(_...) const noexcept
    {
        return cs2sv<Name>;
    }
};

template <typename Op, typename Dst, typename... Args>
struct prettify<Compute<Op, Dst, Args...>>
{
    std::string
    operator()(bool ts, unsigned margin, unsigned width) const noexcept
    {
        return fmt::format(
            "{:<{}} = {}({})",
            prettify<Dst>{}(ts, margin, width),
            margin,
            prettify<Op>{}(ts, margin, width),
            prettify<type_sequence<Args...>>{}(ts, margin, width));
    }
};

template <typename Target>
struct prettify<Jump<Target>>
{
    template <typename... Ts>
    std::string
    operator()(Ts... args) const noexcept
    {
        return fmt::format("JUMP {}", prettify<Target>{}(args...));
    }
};

template <typename Var, typename Labels>
struct prettify<ParmJump<Var, Labels>>
{
    template <typename... Ts>
    std::string
    operator()(bool ts, unsigned margin, unsigned) const noexcept
    {
        return fmt::format("JUMP ({})[{}]",
                           prettify<typelist::enumerate<Labels>>{}(
                               ts,
                               margin,
                               magnitude(typelist::length<Labels>) + 1),
                           prettify<Var>{}(ts, margin, 0));
    }
};

template <typename Int, typename T1, Int val>
struct prettify<type_pair<T1, std::integral_constant<Int, val>>>
{
    template <typename... Ts>
    std::string
    operator()(bool ts, unsigned margin, unsigned width) const noexcept
    {
        return fmt::format("{:0{}d}: {}",
                           val,
                           width,
                           prettify<T1>{}(ts, margin, 0));
    }
};

template <typename... Ts>
struct prettify<type_sequence<Ts...>>
{
    template <typename... Args>
    std::string
    operator()(Args... args) const noexcept
    {
        return fmt::format(
            "{}",
            fmt::join(std::tuple{prettify<Ts>{}(args...)...}, ", "));
    }
};

template <>
struct prettify<ops::choice>
{
    static constexpr std::string_view value = "choice";

    template <typename... Ts>
    std::string_view
    operator()(Ts...) const noexcept
    {
        return value;
    }
};

template <>
struct prettify<std::identity>
{
    static constexpr std::string_view value = "mov";

    template <typename... Ts>
    std::string_view
    operator()(Ts...) const noexcept
    {
        return value;
    }
};

#define MK_STD_PRETTIFY(OP, NAME)                        \
    template <typename T>                                \
    struct prettify<std::OP<T>>                          \
    {                                                    \
        static constexpr std::string_view value = #NAME; \
        template <typename... Ts>                        \
        std::string_view                                 \
        operator()(Ts...) const noexcept                 \
        {                                                \
            return value;                                \
        }                                                \
    };

#define MK_OPS_PRETTIFY(OP)                            \
    template <typename... _>                           \
    struct prettify<ops::OP<_...>>                     \
    {                                                  \
        static constexpr std::string_view value = #OP; \
        template <typename... Ts>                      \
        std::string_view                               \
        operator()(Ts...) const noexcept               \
        {                                              \
            return value;                              \
        }                                              \
    };

MK_STD_PRETTIFY(plus, add)
MK_STD_PRETTIFY(minus, sub)
MK_STD_PRETTIFY(multiplies, mul)
MK_STD_PRETTIFY(divides, div)
MK_STD_PRETTIFY(modulus, mod)
MK_STD_PRETTIFY(negate, neg)
MK_STD_PRETTIFY(equal_to, eq)
MK_STD_PRETTIFY(not_equal_to, neq)
MK_STD_PRETTIFY(greater, gt)
MK_STD_PRETTIFY(less, lt)
MK_STD_PRETTIFY(greater_equal, ge)
MK_STD_PRETTIFY(less_equal, le)
MK_STD_PRETTIFY(logical_and, and)
MK_STD_PRETTIFY(logical_or, or)
MK_STD_PRETTIFY(logical_not, not)
MK_STD_PRETTIFY(bit_and, band)
MK_STD_PRETTIFY(bit_or, bor)
MK_STD_PRETTIFY(bit_xor, xor)
MK_STD_PRETTIFY(bit_not, bnot)

MK_OPS_PRETTIFY(lshift)
MK_OPS_PRETTIFY(rshift)
MK_OPS_PRETTIFY(abs)
MK_OPS_PRETTIFY(max)
MK_OPS_PRETTIFY(min)
MK_OPS_PRETTIFY(round)
MK_OPS_PRETTIFY(ceil)
MK_OPS_PRETTIFY(floor)
MK_OPS_PRETTIFY(trunc)
MK_OPS_PRETTIFY(copysign)
MK_OPS_PRETTIFY(sin)
MK_OPS_PRETTIFY(cos)

#undef MK_STD_PRETTIFY
#undef MK_OPS_PRETTIFY

/// @brief Instruction number width for a cell.
template <typename Cell>
struct instruction_number_width
{
    static constexpr unsigned value = magnitude(typelist::length<Cell>) + 1u;
};

template <typename Cell>
void
print_cell(bool ts = false, unsigned margin = 5)
{
    typelist::for_each(
        typelist::enumerate<Cell, unsigned, 1u>{},
        [=]<typename T>()
        {
            fmt::print("\t{}\n",
                       prettify<T>{}(ts,
                                     margin,
                                     instruction_number_width<Cell>::value));
        });
}

/// @brief Pretty print program.
/// @param ts print types
/// @param margin arrow margin
template <typename Program>
void
print_program(Program, bool ts = false, unsigned margin = 5) noexcept
{
    using labels = Program::labels;
    using cells  = Program::cells;
    typelist::for_each(typelist::zip<labels, cells>{},
                       [=]<typename LC>()
                       {
                           fmt::print("{}:\n",
                                      prettify<fst_t<LC>>{}(ts, margin, 0));
                           print_cell<snd_t<LC>>(ts, margin);
                       });
}

struct UnknownVar
{
    std::string_view value;
};

template <typename Variables, std::ranges::input_range R>
std::bitset<typelist::length<Variables>>
read_var_mask(Variables vars, R&& words)
{
    using ret_t = std::bitset<typelist::length<Variables>>;
    ret_t ret{};

    std::ranges::for_each(
        std::forward<R>(words),
        [&](const auto& v)
        {
            std::string_view s{std::ranges::begin(v), std::ranges::end(v)};
            std::size_t      n;

            // try to parse number
            auto r = std::from_chars(s.data(), s.data() + s.size(), n);

            if(r.ec == std::errc{})
                ret.set(n);
            else if(typelist::first_of(
                        vars,
                        []<typename Var>(std::string_view s)
                        { return s == Var::nameview; },
                        s))
                typelist::for_each(
                    vars,
                    []<typename Var>(ret_t& ret, std::string_view s)
                    {
                        if(s == Var::nameview)
                            ret.set(typelist::index<Var, Variables>);
                    },
                    ret,
                    s);
            else
                throw UnknownVar{s};
        });

    return ret;
}

template <typename Variables>
std::bitset<typelist::length<Variables>>
interactive_get_mask(Variables vars)
{
    constexpr typelist::enumerate<Variables> enumerated{};
    fmt::print(
        "Select variables. Enter numbers or names separated with space:\n");
    auto printer = []<typename Var>()
    { fmt::print("{}\n", prettify<Var>{}(true, 0, 0)); };
    typelist::for_each(enumerated, printer);

    using ret_t = std::bitset<typelist::length<Variables>>;
    ret_t ret;
    bool  success = false;
    while(!success) try
        {
            std::string line;
            std::getline(std::cin, line);
            ret = read_var_mask(vars, std::views::split(line, ' '));
            if(ret.any())
                success = true;
            else
                fmt::print("No variables selected. Try again:\n");
        }
        catch(const UnknownVar& ex)
        {
            fmt::print("Unknown variable {}. Try again:\n", ex.value);
        }

    fmt::print("Selected variables:\n");
    typelist::for_each_masked(enumerated, printer, ret);
    ret.reset(0); // guarantee unset CP variable
    return ret;
}

template <typename P>
input_type<P>
interactive_get_inputs(P)
{
    using input_vars = P::input_vars;

    return typelist::maybe_collect(input_vars{},
                                   []<typename Var>()
                                   {
                                       fmt::print("Set value for {}:\n",
                                                  prettify<Var>{}(true, 0, 0));
                                       vartype<Var> value;
                                       std::cin >> value;
                                       return value;
                                   });
}

template <typename _Label>
struct label_length : std::integral_constant<unsigned, _Label::name::size()>
{
};

// 3 is for "N/A"
template <typename Labels>
inline constexpr unsigned max_label_width =
    std::max(*std::ranges::max_element(
                 intlist::stdarray<typelist::map<label_length, Labels>>),
             3u);

template <typename S>
void
print_word(S&& s, unsigned cw) noexcept
{
    std::string w = fmt::format("{:<{}}", std::forward<S>(s), cw);
    if(w.size() == cw)
        fmt::print("{}|", w);
    else
        fmt::print("{}\u2026|", std::string_view(w).substr(0, cw - 1));
}

void
print_dash(unsigned cw) noexcept
{
    fmt::print("{:\u2015^{}}|", "", cw);
};

template <typename _Program, std::size_t N>
    requires(N == typelist::length<typename _Program::variables>)
void
print_header(_Program, const std::bitset<N>& mask, unsigned cw, bool ts)
{
    using labels    = _Program::labels;
    using variables = _Program::variables;

    // max label width
    constexpr unsigned mlw = max_label_width<labels>;

    print_word("CELL", mlw);

    typelist::for_each_masked(
        variables{},
        []<typename Var>(unsigned cw, bool ts)
        { print_word(prettify<Var>{}(ts, 0, 0), cw); },
        mask,
        cw,
        ts);

    fmt::print("\r\n");
    print_dash(std::max(mlw, 4u));
    typelist::for_each_masked(
        variables{},
        []<typename Var>(unsigned cw, bool ts)
        {
            print_dash(std::max(
                static_cast<unsigned>(prettify<Var>{}(ts, 0, 0).size()),
                cw));
        },
        mask,
        cw,
        ts);

    fmt::print("\r\n");
}

template <typename _Labels, std::unsigned_integral I>
std::string_view
get_label_name(_Labels, I i) noexcept
{
    return typelist::visit_at(
        i,
        []<typename L>() noexcept -> std::string_view
        { return prettify<L>{}(); },
        _Labels{},
        []() noexcept -> std::string_view { return "N/A"; });
}

template <typename _Program, std::size_t N, typename _Mem>
    requires(N == typelist::length<typename _Program::variables>)
void
print_state(_Program, const _Mem& mem, const std::bitset<N>& mask, unsigned cw)
{
    using labels    = _Program::labels;
    using variables = _Program::variables;

    print_word(get_label_name(labels{}, mem.cp()), max_label_width<labels>);
    typelist::for_each_masked(
        variables{},
        [&mem]<typename Var>(unsigned cw)
        { print_word(mem.template get<Var>(), cw); },
        mask,
        cw);

    fmt::print("\r\n");
}

constexpr char
mod_ctrl(char c) noexcept
{
    return c & 0x1f;
}

constexpr bool
quit_condition(char c) noexcept
{
    return c == mod_ctrl('q');
}

class RawTerminalReader
{
    termios m_orig;

    void
    die(const char* s) const noexcept
    {
        std::perror(s);
        std::exit(1);
    }

  public:
    RawTerminalReader() = default;

    RawTerminalReader(bool echo = false) noexcept
    {
        if(tcgetattr(STDIN_FILENO, &m_orig) == -1) die("tcgetattr");

        if(!echo)
        {
            termios raw = m_orig;

            raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
            raw.c_lflag &= ~(ECHO | ICANON | ISIG | IEXTEN);
            raw.c_oflag &= ~OPOST;
            raw.c_cflag |= CS8;

            if(tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) die("tcsetattr");
        }
    }

    char
    operator()() const noexcept
    {
        char c;
        if(read(STDIN_FILENO, &c, 1) == -1 && errno != EAGAIN) die("read");
        return c;
    }

    ~RawTerminalReader()
    {
        if(tcsetattr(STDIN_FILENO, TCSAFLUSH, &m_orig) == -1) die("tcsetattr");
    }
};

template <typename _Program, bool interactive>
    requires(!_Program::singular)
class debug_compiler
{
    using labelmap  = _Program::labelmap;
    using cells     = _Program::cells;
    using variables = _Program::variables;

    static constexpr auto halt_pos = _Program::halt_pos;

    template <typename C>
    using compile_cell = standard_compiler<C, labelmap>;

    using compiled_cells = typelist::map<compile_cell, cells>;
    using facade         = bse::MemoryFacade<_Program>;
    using var_mask       = std::bitset<typelist::length<variables>>;

    static constexpr char stop = 'q';
    static constexpr char step = ' ';

    class base : public facade
    {
        static constexpr typelist::to<compiled_cells, std::tuple> cells{};

        unsigned m_cw;
        bool     m_ts;
        var_mask m_mask;

      public:
        base(unsigned cw, bool ts, var_mask mask = var_mask{-1ull << 1}) :
            m_cw(cw), m_ts(ts), m_mask(mask)
        {
        }

        void
        header() const
        {
            print_header(_Program{}, m_mask, m_cw, m_ts);
        }

        void
        step()
        {
            visit_at(
                facade::cp(),
                [&](auto cell) noexcept { facade::step(cell); },
                cells,
                sink);
            facade::step([mask = m_mask, cw = m_cw](const auto& mem) noexcept
                         { print_state(_Program{}, mem, mask, cw); });
        }
    };

    class interactive_inspector : base
    {
        RawTerminalReader m_reader{false};

        void
        print_help() const noexcept
        {
            fmt::print("Key bindings:\r\n");
            fmt::print("SPACE: step\r\n");
            fmt::print("{}: stop\r\n", stop);
        }

      public:
        using base::base;

        template <typename... Args>
        void
        operator()(const Args&... args) noexcept
        {
            base::set_inputs(args...);

            print_help();
            base::header();

            char c = m_reader();
            while(base::cp() != halt_pos && c != stop)
            {
                if(c == step)
                    base::step();
                else if(c != stop)
                    fmt::print("Unknown command: {}\r\n", c);

                c = m_reader();
            }
        }
    };

    class fast_inspector : base
    {
        unsigned m_max_steps;
        unsigned m_step_number = 0;

      public:
        template <typename... Args>
        fast_inspector(unsigned max_steps, Args... args) :
            base(args...), m_max_steps(max_steps)
        {
        }

        template <typename... Args>
        void
        operator()(const Args&... args) noexcept
        {
            base::set_inputs(args...);

            base::header();

            while(base::cp() != halt_pos && m_step_number++ < m_max_steps)
                base::step();
        }
    };

  public:
    using inspector =
        std::conditional_t<interactive, interactive_inspector, fast_inspector>;
};
} // namespace lucid::dbg
