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
    operator()() const noexcept
    {
        return fmt::format("{}: {}",
                           Variable<T, N>::nameview,
                           TypeInfo<T>::chars);
    }
};

template <typename Name>
struct prettify<Label<Name>>
{
    std::string_view
    operator()() const noexcept
    {
        return cs2sv<Name>;
    }
};

template <typename Op, typename Dst, typename... Args>
struct prettify<Compute<Op, Dst, Args...>>
{
    std::string
    operator()() const noexcept
    {
        return fmt::format("{} <- {}({})",
                           prettify<Dst>{}(),
                           prettify<Op>{}(),
                           prettify<type_sequence<Args...>>{}());
    }
};

template <typename Target>
struct prettify<Jump<Target>>
{
    std::string
    operator()() const noexcept
    {
        return fmt::format("JUMP {}", prettify<Target>{}());
    }
};

template <typename Var, typename Labels>
struct prettify<ParmJump<Var, Labels>>
{
    std::string
    operator()() const noexcept
    {
        return fmt::format("JUMP ({})[{}]",
                           prettify<typelist::enumerate<Labels>>{}(),
                           prettify<Var>{}());
    }
};

template <typename Int, typename T2, Int val>
struct prettify<type_pair<std::integral_constant<Int, val>, T2>>
{
    std::string
    operator()() const noexcept
    {
        return fmt::format("{}: {}", val, prettify<T2>{}());
    }
};

template <typename... Ts>
struct prettify<type_sequence<Ts...>>
{
    std::string
    operator()() const noexcept
    {
        return fmt::format("{}",
                           fmt::join(std::tuple{prettify<Ts>{}()...}, ", "));
    }
};

template <>
struct prettify<ops::choice>
{
    static constexpr std::string_view value = "choice";

    std::string_view
    operator()() const noexcept
    {
        return value;
    }
};

#define MK_STD_PRETTIFY(OP, NAME)                        \
    template <typename T>                                \
    struct prettify<std::OP<T>>                          \
    {                                                    \
        static constexpr std::string_view value = #NAME; \
        std::string_view                                 \
        operator()() const noexcept                      \
        {                                                \
            return value;                                \
        }                                                \
    };

#define MK_OPS_PRETTIFY(OP)                            \
    template <typename... _>                           \
    struct prettify<ops::OP<_...>>                     \
    {                                                  \
        static constexpr std::string_view value = #OP; \
        std::string_view                               \
        operator()() const noexcept                    \
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

class TerminalReader
{
    termios m_orig;

  public:
    FORBID_COPY(TerminalReader);

    TerminalReader(bool echo = false) noexcept
    {
        tcgetattr(STDIN_FILENO, &m_orig);

        if(!echo)
        {
            termios raw;
            raw.c_lflag &= ~(ECHO);
            tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
        }
    }

    bool
    operator()(char* c) const noexcept
    {
        return read(STDIN_FILENO, c, 1) == 1;
    }

    ~TerminalReader() { tcsetattr(STDIN_FILENO, TCSAFLUSH, &m_orig); }
};

} // namespace lucid
