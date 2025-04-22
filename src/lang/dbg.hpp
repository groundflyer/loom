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
    operator()(bool type_signature, unsigned) const noexcept
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
    operator()(bool ts, unsigned a) const noexcept
    {
        return fmt::format("{:<{}} <- {}({})",
                           prettify<Dst>{}(ts, a),
                           a,
                           prettify<Op>{}(ts, a),
                           prettify<type_sequence<Args...>>{}(ts, a));
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
    operator()(Ts... args) const noexcept
    {
        return fmt::format("JUMP ({})[{}]",
                           prettify<typelist::enumerate<Labels>>{}(args...),
                           prettify<Var>{}(args...));
    }
};

template <typename Int, typename T1, Int val>
struct prettify<type_pair<T1, std::integral_constant<Int, val>>>
{
    template <typename... Ts>
    std::string
    operator()(Ts... args) const noexcept
    {
        return fmt::format("{}: {}", val, prettify<T1>{}(args...));
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

template <typename T>
struct decorator
{
    template <typename... Ts>
    std::string
    operator()(Ts... args) const noexcept
    {
        return fmt::format("\t{}", prettify<T>{}(args...));
    }
};

template <typename Name>
struct decorator<Label<Name>>
{
    template <typename... Ts>
    std::string
    operator()(Ts... args) const noexcept
    {
        return fmt::format("{}:", prettify<Label<Name>>{}(args...));
    }
};

template <typename... Ts>
void
print_program(Program<Ts...>) noexcept
{
    using instructions = Program<Ts...>::instructions;
    typelist::for_each(typelist::map<decorator, instructions>{},
                       []<typename D>() { fmt::print("{}\n", D{}(false, 5)); });
}

template <typename... _>
std::optional<std::string>
get_var_value(const bse::Memory<_...>& mem, const unsigned var) noexcept
{
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

    void
    operator()(char* c) const noexcept
    {
        if(read(STDIN_FILENO, c, 1) == -1 && errno != EAGAIN) die("read");
    }

    ~RawTerminalReader()
    {
        if(tcsetattr(STDIN_FILENO, TCSAFLUSH, &m_orig) == -1) die("tcsetattr");
    }
};

} // namespace lucid
