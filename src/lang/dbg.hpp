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

#include <bitset>
#include <cerrno>
#include <cstdio>
#include <optional>
#include <string>

#include <termios.h>
#include <unistd.h>

namespace lucid
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
            return fmt::format("{}: {}",
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
            "{:<{}} <- {}({})",
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
                                     magnitude(typelist::length<Cell>) + 1u));
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

template <typename Program>
void
print_variables(Program, bool ts = true)
{
    using variables = Program::variables;
    typelist::for_each(variables{},
                       [=]<typename Var>()
                       { fmt::print("{}\n", prettify<Var>{}(ts, 0, 0)); });
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
    FORBID_COPY(RawTerminalReader);

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
} // namespace lucid
