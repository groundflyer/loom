// -*- C++ -*-
/// @file dbg.hpp
/// @brief Utilities for kernel debugging.

#pragma once

#include "bse.hpp"
#include "core.hpp"
#include "operators.hpp"

#include <utils/argparse.hpp>
#include <utils/common.hpp>
#include <utils/string.hpp>
#include <utils/typeinfo.hpp>

#include <fmt/core.h>
#include <fmt/format.h>
#include <fmt/ranges.h>

#include <fast_float/fast_float.h>

#include <algorithm>
#include <bitset>
#include <cctype>
#include <cerrno>
#include <charconv>
#include <cstdio>
#include <iostream>
#include <optional>
#include <ranges>
#include <string>
#include <vector>

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

template <>
struct prettify<ops::fma>
{
    static constexpr std::string_view value = "fma";

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

/// @brief Instruction number width for a block.
template <typename Block>
struct instruction_number_width
{
    static constexpr unsigned value = magnitude(typelist::length<Block>) + 1u;
};

template <typename Block>
void
print_block(bool ts = false, unsigned margin = 5)
{
    typelist::for_each(
        typelist::enumerate<Block, unsigned, 1u>{},
        [=]<typename T>()
        {
            fmt::print("\t{}\n",
                       prettify<T>{}(ts,
                                     margin,
                                     instruction_number_width<Block>::value));
        });
}

/// @brief Pretty print program.
/// @param ts print types
/// @param margin arrow margin
template <Kernel K>
void
print_program(K, bool ts = false, unsigned margin = 5) noexcept
{
    using labels = K::labels;
    using blocks = K::blocks;
    typelist::for_each(typelist::zip<labels, blocks>{},
                       [=]<typename LC>()
                       {
                           fmt::print("{}:\n",
                                      prettify<fst_t<LC>>{}(ts, margin, 0));
                           print_block<snd_t<LC>>(ts, margin);
                       });
}

struct UnknownVar
{
    std::string_view value;
};

template <typename VariablesTail, std::ranges::input_range R>
std::bitset<typelist::length<VariablesTail>>
read_var_mask(VariablesTail vars, R&& words)
{
    using ret_t = std::bitset<typelist::length<VariablesTail>>;
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
                            ret.set(typelist::index<Var, VariablesTail>);
                    },
                    ret,
                    s);
            else
                throw UnknownVar{s};
        });

    return ret;
}

void
print_errc(std::errc ec, std::string_view sv, std::FILE* file = stdout) noexcept
{
    switch(ec)
    {
    case std::errc::invalid_argument:
        fmt::print(file, "Invalid argument: '{}'. Try again.\n", sv);
        break;
    case std::errc::result_out_of_range:
        fmt::print(file, "Result out of range: '{}'. Try again.\n", sv);
    default:
    {
    }
    }
};

struct SizeEx
{
};

template <typename InputVars>
auto
read_input_vars(InputVars input_vars, std::string_view line)
{
    auto rng = std::views::split(line, ' ') |
               std::views::transform(
                   [](const auto& r) -> std::string_view
                   { return {std::ranges::begin(r), std::ranges::end(r)}; });
    std::vector<std::string_view> v{std::ranges::begin(rng),
                                    std::ranges::end(rng)};

    if(v.size() != typelist::length<InputVars>) throw SizeEx{};

    return typelist::maybe_collect(input_vars,
                                   [&v]<typename Var>()
                                   {
                                       return read_number<vartype<Var>, true>(
                                           v[typelist::index<Var, InputVars>]);
                                   });
}

struct VarPrinter
{
    template <typename Var>
    void
    operator()(std::FILE* file = stdout) const noexcept
    {
        fmt::print(file, "{}\n", prettify<Var>{}(true, 0, 0));
    }
};

inline constexpr VarPrinter var_printer{};

template <typename VariablesTail>
constexpr inline typelist::enumerate<VariablesTail> enumerated{};

template <typename VariablesTail>
void
print_variables(VariablesTail, std::FILE* file = stdout) noexcept
{
    typelist::for_each(enumerated<VariablesTail>, var_printer, file);
}

template <typename VariablesTail>
std::bitset<typelist::length<VariablesTail>>
interactive_get_mask(VariablesTail vars)
{
    fmt::print("[Inspected Variables] Enter numbers or names separated with "
               "space (leave empty to track all variables):\n");
    print_variables(VariablesTail{});

    using ret_t = std::bitset<typelist::length<VariablesTail>>;
    ret_t ret;
    bool  success = false;
    while(!success) try
        {
            std::string line;
            std::getline(std::cin, line);
            ret = read_var_mask(vars, std::views::split(line, ' '));
            if(!ret.any()) ret.set();
            success = true;
        }
        catch(const UnknownVar& ex)
        {
            fmt::print("Unknown variable {}. Try again:\n", ex.value);
        }

    fmt::print("Selected variables:\n");
    typelist::for_each_masked(enumerated<VariablesTail>, var_printer, ret);
    return ret;
}

template <typename P>
input_type<P>
interactive_get_inputs(P)
{
    using input_vars = P::input_vars;

    bool          success = false;
    input_type<P> ret;

    if constexpr(typelist::length<input_vars> > 1)
    {
        while(!success)
        {
            fmt::print("[Inputs: {}] Enter values separated with space:\n",
                       fmt::join(typelist::maybe_collect(
                                     input_vars{},
                                     []<typename Var>()
                                     { return prettify<Var>{}(true, 0, 0); }),
                                 " "));

            std::string line;
            std::getline(std::cin, line);

            try
            {
                ret     = read_input_vars(input_vars{}, line);
                success = true;
            }
            catch(const SizeEx&)
            {
                fmt::print(stderr,
                           "Number of values doesn't match. Try again.\n");
                continue;
            }
            catch(const ReadException& ex)
            {
                print_errc(ex.ec, ex.sv);
                continue;
            }
        }
    }
    else
    {
        while(!success)
        {
            using var = typelist::head<input_vars>;
            fmt::print("[Input: {}] Enter value:\n",
                       prettify<var>{}(true, 0, 0));
            std::string line;
            std::getline(std::cin, line);
            if(line.empty())
            {
                fmt::print("Try again.\n");
                continue;
            }

            auto pr = fast_float::from_chars(line.data(),
                                             line.data() + line.size(),
                                             ret);
            success = pr.ec == std::errc{};
            print_errc(pr.ec, line);
        }
    }

    fmt::print("Input values: {}\n", ret);
    return ret;
}

template <typename _Label>
struct label_length : std::integral_constant<unsigned, _Label::name::size()>
{
};

// 4 is for "HALT"
template <typename Labels>
inline constexpr unsigned max_label_width =
    std::max(*std::ranges::max_element(
                 intlist::stdarray<typelist::map<label_length, Labels>>),
             4u);

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

template <Kernel K, std::size_t N>
    requires(N == typelist::length<typename K::variables> - 1)
void
print_header(K, const std::bitset<N>& mask, unsigned cw, bool ts)
{
    using labels = K::labels;
    constexpr typelist::tail<typename K::variables> vars{};

    // max label width
    constexpr unsigned mlw = max_label_width<labels>;

    print_word("BLOCK", mlw);

    typelist::for_each_masked(
        vars,
        []<typename Var>(unsigned cw, bool ts)
        { print_word(prettify<Var>{}(ts, 0, 0), cw); },
        mask,
        cw,
        ts);

    fmt::print("\r\n");
    print_dash(std::max(mlw, 4u));
    typelist::for_each_masked(
        vars,
        []<typename Var>(unsigned cw) { print_dash(cw); },
        mask,
        cw);

    fmt::print("\r\n");
}

template <typename _Labels, std::unsigned_integral I>
std::string_view
get_label_name(_Labels, I i, I halt_pos) noexcept
{
    return typelist::visit_at(
        i,
        []<typename L>() noexcept -> std::string_view
        { return prettify<L>{}(); },
        _Labels{},
        [=]() noexcept -> std::string_view
        { return i == halt_pos ? "HALT" : "N/A"; });
}

template <Kernel K, std::size_t N, typename _Mem>
    requires(N == typelist::length<typename K::variables> - 1)
void
print_state(K, const _Mem& mem, const std::bitset<N>& mask, unsigned cw)
{
    using labels    = K::labels;
    using variables = typelist::tail<typename K::variables>;

    print_word(get_label_name(labels{}, mem.bp(), K::halt_pos),
               max_label_width<labels>);
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

template <Kernel K, bool interactive>
    requires(!K::singular)
class debug_compiler
{
    using labelmap  = K::labelmap;
    using blocks    = K::blocks;
    using variables = K::variables;

    static constexpr auto halt_pos = K::halt_pos;

    template <typename C>
    using compile_block = standard_compiler<C, labelmap>;

    using compiled_blocks = typelist::map<compile_block, blocks>;
    using facade          = bse::MemoryFacade<K>;
    using var_mask        = std::bitset<typelist::length<variables> - 1>;

    static constexpr char stop = 'q';
    static constexpr char step = ' ';

    class base : public facade
    {
        static constexpr typelist::to<compiled_blocks, std::tuple> blocks{};

        unsigned m_cw;
        bool     m_ts;
        var_mask m_mask;

      public:
        base(unsigned cw, bool ts, var_mask mask = var_mask{-1ull}) :
            m_cw(cw), m_ts(ts), m_mask(mask)
        {
        }

        void
        header() const
        {
            print_header(K{}, m_mask, m_cw, m_ts);
        }

        void
        step()
        {
            visit_at(
                facade::bp(),
                [&](auto block) noexcept { facade::step(block); },
                blocks,
                sink);
            facade::step([mask = m_mask, cw = m_cw](const auto& mem) noexcept
                         { print_state(K{}, mem, mask, cw); });
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
            while(base::bp() != halt_pos && c != stop)
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

            while(base::bp() != halt_pos && m_step_number++ < m_max_steps)
                base::step();
        }
    };

  public:
    using inspector =
        std::conditional_t<interactive, interactive_inspector, fast_inspector>;
};

template <Kernel K, typename InputOpt>
int
inspect(InputOpt inopt, int argc, char* argv[]) noexcept
{
    // ignore BP
    constexpr auto vars = typelist::tail<typename K::variables>{};

    const auto pr = argparse::parse(
        std::tuple{
            inopt,
            argparse::flag<'p', "print">("print program and exit"),
            argparse::flag<'T', "type-annotations">("show type annotations"),
            argparse::flag<'i', "interactive">("run interactive"),
            argparse::option<'w', "column-width">("column width. Default is 20",
                                                  "N",
                                                  20u,
                                                  read_number<unsigned, true>),
            argparse::option<'s', "max-steps">(
                "maximum number of steps. Default is 100",
                "N",
                100u,
                read_number<unsigned, true>),
            argparse::option_list<'t', "track">("variables to track", "vars")},
        {argc, argv});
    constexpr K p{};

    unsigned cw = 20;
    unsigned ms = 100;

    const bool ts = pr.template get_opt<'T'>();

    decltype(pr.template get_opt<"input">()) fi;

    try
    {
        cw = pr.template get_opt<'w'>();
        ms = pr.template get_opt<'s'>();
        fi = pr.template get_opt<"input">();
    }
    catch(const ReadException& ex)
    {
        print_errc(ex.ec, ex.sv, stderr);
        return 1;
    }

    if(pr.template get_opt<'p'>())
    {
        fmt::print("Program:\n");
        print_program(p, ts);
        return 0;
    }

    if(pr.template get_opt<'i'>())
    {
        auto mask       = interactive_get_mask(vars);
        auto inputs     = interactive_get_inputs(K{});
        using Inspector = debug_compiler<K, true>::inspector;
        Inspector insp{cw, ts, mask};
        invopply(insp, inputs);
    }
    else
    {
        using Inspector = debug_compiler<K, false>::inspector;
        try
        {
            auto mask = read_var_mask(vars, pr.template get_opt<'t'>());
            if(!mask.any())
            {
                mask.set();
                mask.reset(0);
            }
            Inspector insp{ms, cw, ts, mask};
            invopply(insp, fi);
        }
        catch(const UnknownVar& ex)
        {
            fmt::print(stderr, "Unknown variable: {}\n", ex.value);
            fmt::print(stderr,
                       "Use variable number or name. Available variables:\n");
            print_variables(vars, stderr);
            return 2;
        }
    }

    return 0;
}
} // namespace lucid::dbg
