// -*- C++ -*-
/// @file dbg.hpp
/// @brief Utilities for kernel debugging.
// This file is part of
// Loom: a proof-of-concept virtual machine for SPMD execution
// Copyright (C) 2026 Roman Saldygashev (sldg.roman@gmail.com)
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 3 of the License.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.


#pragma once

#include "bse.hpp"
#include "core.hpp"
#include "operators.hpp"

#include <common/argparse.hpp>
#include <common/forbid.hpp>
#include <common/string.hpp>
#include <common/typeinfo.hpp>

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

namespace loom::vm::dbg
{
template <typename T>
struct shorten : std::false_type
{
};

template <typename T>
struct comp_type;

template <typename T>
struct prettify;

template <typename T, typename N>
struct prettify<Variable<T, N>>
{
    std::string
    operator()(bool type_signature, unsigned) const noexcept
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
    static constexpr std::string_view value = cs2sv<Name>;

    template <typename... _>
    std::string_view
    operator()(_...) const noexcept
    {
        return value;
    }
};

template <typename Op, typename Dst, typename... Args>
struct prettify<Compute<Op, Dst, Args...>>
{
    using t = Compute<Op, Dst, Args...>;

    std::string
    operator()(bool ts, unsigned width) const noexcept
    {
        if constexpr(shorten<t>::value)
        {
            if(ts)
                return fmt::format(
                    "{} @{} {} {}",
                    prettify<Op>{}(false, width),
                    TypeInfo<typename comp_type<t>::type>::chars,
                    prettify<Dst>{}(false, width),
                    prettify<type_sequence<Args...>>{}(false, width));
            else
                return fmt::format(
                    "{} {} {}",
                    prettify<Op>{}(ts, width),
                    prettify<Dst>{}(ts, width),
                    prettify<type_sequence<Args...>>{}(ts, width));
        }
        else
            return fmt::format("{} {} {}",
                               prettify<Op>{}(ts, width),
                               prettify<Dst>{}(ts, width),
                               prettify<type_sequence<Args...>>{}(ts, width));
    }
};

template <typename Target>
struct prettify<Jump<Target>>
{
    std::string
    operator()(bool, unsigned) const noexcept
    {
        return fmt::format("jmp {}", prettify<Target>{}(false, 0));
    }
};

template <typename Var, typename Labels>
struct prettify<ParmJump<Var, Labels>>
{
    std::string
    operator()(bool, unsigned) const noexcept
    {
        return fmt::format("jmp {} {}",
                           prettify<Var>{}(false, 0),
                           prettify<typelist::enumerate<Labels>>{}(
                               false,
                               magnitude(typelist::length<Labels>) + 1));
    }
};

template <typename Int, typename T1, Int val>
struct prettify<type_pair<T1, std::integral_constant<Int, val>>>
{
    std::string
    operator()(bool ts, unsigned width) const noexcept
    {
        return fmt::format("{:{}d}: {}", val, width, prettify<T1>{}(ts, 0));
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
            fmt::join(std::tuple{prettify<Ts>{}(args...)...}, " "));
    }
};

template <typename Name, typename... Ts>
struct prettify<macro_inv<Name, Ts...>>
{
    using info = macro_inv<Name, Ts...>;

    template <typename... Args>
    std::string
    operator()(Args... args) const noexcept
    {
        return fmt::format("{} {}",
                           info::nameview,
                           prettify<typename info::args>{}(args...));
    }
};

template <CharSequence S>
struct prettify<S>
{
    static constexpr std::string_view value = cs2sv<S>;

    template <typename... Ts>
    std::string_view
    operator()(Ts...) const noexcept
    {
        return value;
    }
};

template <>
struct prettify<ops::cmov>
{
    static constexpr std::string_view value = "cmov";

    template <typename... Ts>
    std::string_view
    operator()(Ts...) const noexcept
    {
        return value;
    }
};

template <typename Dst, typename C, typename A, typename B>
struct shorten<Compute<ops::cmov, Dst, C, A, B>> : std::true_type
{
};

template <typename Dst, typename C, typename A, typename B>
struct comp_type<Compute<ops::cmov, Dst, C, A, B>> :
    std::type_identity<vartype<Dst>>
{
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

template <typename A, typename B>
struct shorten<Compute<std::identity, A, B>> :
    std::bool_constant<std::is_same_v<vartype<A>, vartype<B>>>
{
};

template <typename A, typename B>
struct comp_type<Compute<std::identity, A, B>> : std::type_identity<vartype<A>>
{
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

template <typename... Operands>
struct shorten<Compute<ops::fma, Operands...>> : std::true_type
{
};

template <typename Dst, typename... Operands>
struct comp_type<Compute<ops::fma, Dst, Operands...>> :
    std::type_identity<vartype<Dst>>
{
};

template <>
struct prettify<ops::fms>
{
    static constexpr std::string_view value = "fma";

    template <typename... Ts>
    std::string_view
    operator()(Ts...) const noexcept
    {
        return value;
    }
};

template <typename... Operands>
struct shorten<Compute<ops::fms, Operands...>> : std::true_type
{
};

template <typename Dst, typename... Operands>
struct comp_type<Compute<ops::fms, Dst, Operands...>> :
    std::type_identity<vartype<Dst>>
{
};

template <typename Dst, typename C, typename T, typename I, typename L>
struct prettify<Compute<ops::sct<T, I, L>, Dst, C>>
{
    std::string
    operator()(bool ts, unsigned) const noexcept
    {
        if(ts)
            return fmt::format("sct @{} {} {} {}",
                               TypeInfo<T>::chars,
                               prettify<Dst>{}(false, 0),
                               prettify<C>{}(false, 0),
                               ops::sct<T, I, L>::values);
        else
            return fmt::format("sct {} {} {}",
                               prettify<Dst>{}(false, 0),
                               prettify<C>{}(false, 0),
                               ops::sct<T, I, L>::values);
    }
};

template <typename T, typename I, typename... Operands>
struct shorten<Compute<ops::slt<T, I>, Operands...>> : std::true_type
{
};

template <typename T, typename I, typename Dst, typename... Operands>
struct comp_type<Compute<ops::slt<T, I>, Dst, Operands...>> :
    std::type_identity<vartype<Dst>>
{
};

template <typename T, typename I>
struct prettify<ops::slt<T, I>>
{
    static constexpr std::string_view value = "slt";

    template <typename... Ts>
    std::string_view
    operator()(Ts...) const noexcept
    {
        return value;
    }
};

template <typename T,
          std::integral I,
          template <typename> typename Acc,
          template <typename> typename Cmp,
          typename... Operands>
struct shorten<Compute<ops::pacc<T, I, Acc, Cmp>, Operands...>> : std::true_type
{
};

template <typename T,
          std::integral I,
          template <typename> typename Acc,
          template <typename> typename Cmp,
          typename... Operands>
struct comp_type<Compute<ops::pacc<T, I, Acc, Cmp>, Operands...>> :
    std::type_identity<T>
{
};

template <typename T,
          std::integral I,
          template <typename> typename Acc,
          template <typename> typename Cmp>
struct prettify<ops::pacc<T, I, Acc, Cmp>>
{
    std::string
    operator()(bool ts, unsigned w) const noexcept
    {
        return fmt::format("{}{}",
                           prettify<Acc<I>>{}(ts, w),
                           prettify<Cmp<T>>{}(ts, w));
    }
};

#define MK_STD_PRETTIFY(OP, NAME)                                     \
    template <typename T>                                             \
    struct prettify<std::OP<T>>                                       \
    {                                                                 \
        static constexpr std::string_view value = #NAME;              \
        template <typename... Ts>                                     \
        std::string_view                                              \
        operator()(Ts...) const noexcept                              \
        {                                                             \
            return value;                                             \
        }                                                             \
    };                                                                \
    template <typename T, typename... Operands>                       \
    struct shorten<Compute<std::OP<T>, Operands...>> : std::true_type \
    {                                                                 \
    };                                                                \
    template <typename T, typename Dst, typename... Operands>         \
    struct comp_type<Compute<std::OP<T>, Dst, Operands...>> :         \
        std::type_identity<vartype<Dst>>                              \
    {                                                                 \
    };

#define MK_OPS_PRETTIFY(OP)                                              \
    template <typename... _>                                             \
    struct prettify<ops::OP<_...>>                                       \
    {                                                                    \
        static constexpr std::string_view value = #OP;                   \
        template <typename... Ts>                                        \
        std::string_view                                                 \
        operator()(Ts...) const noexcept                                 \
        {                                                                \
            return value;                                                \
        }                                                                \
    };                                                                   \
    template <typename... Operands, typename... _>                       \
    struct shorten<Compute<ops::OP<_...>, Operands...>> : std::true_type \
    {                                                                    \
    };                                                                   \
    template <typename Dst, typename... Operands, typename... _>         \
    struct comp_type<Compute<ops::OP<_...>, Dst, Operands...>> :         \
        std::type_identity<vartype<Dst>>                                 \
    {                                                                    \
    };

// clang-format off
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
MK_OPS_PRETTIFY(sqrt)
MK_OPS_PRETTIFY(rsqrt)
// clang-format on

#undef MK_STD_PRETTIFY
#undef MK_OPS_PRETTIFY

template <typename MacroDecl, template <typename...> typename MacroDef>
struct pretty_macro
{
    static constexpr std::string_view nameview = MacroDecl::nameview;

    using parameters = MacroDecl::parameters;
    using statements = typelist::to<parameters, MacroDef>::type;
};

template <typename T,
          typename MacroDecl,
          template <typename...> typename MacroDef>
struct pretty_tmacro
{
    static constexpr std::string_view nameview = MacroDecl::nameview;

    using parameters = MacroDecl::parameters;
    using statements =
        typelist::to<typelist::cons<T, parameters>, MacroDef>::type;
};

#define PRETTIFY_MACRO_PREFIX(PREFIX, MACRO_NAME) \
    pretty_macro<PREFIX##MACRO_NAME, MACRO_NAME>
#define PRETTIFY_MACRO(MACRO_NAME) \
    PRETTIFY_MACRO_PREFIX(__macro_decl_, MACRO_NAME)

#define PRETTIFY_TMACRO_PREFIX(PREFIX, T, MACRO_NAME) \
    pretty_tmacro<T, PREFIX##MACRO_NAME, MACRO_NAME>
#define PRETTIFY_TMACRO(T, MACRO_NAME) \
    PRETTIFY_TMACRO_PREFIX(__macro_decl_, T, MACRO_NAME)

/// @brief Pretty print list of instructions.
/// @param ts add type signatures.
/// @param ln add line numbers.
/// @tparam Instructions list of instructions to print.
/// @tparam start first line number.
/// @tparam w line number padding width.
template <typelist::List Instructions,
          unsigned       start = 1,
          unsigned       w = magnitude(typelist::length<Instructions>) + start>
void
print_instructions(const bool ts = false, const bool ln = true)
{
    if(ln)
        typelist::for_each(
            typelist::enumerate<Instructions, unsigned, start>{},
            [=]<typename TP>()
            {
                using t              = fst_t<TP>;
                constexpr unsigned n = snd_t<TP>::value;

                if constexpr(is_label<t>::value)
                    fmt::print("{:{}d} {}:\n", n, w, prettify<t>{}(ts, 0));
                else
                    fmt::print("{:{}d}\t{}\n", n, w, prettify<t>{}(ts, 0));
            });
    else
        typelist::for_each(Instructions{},
                           [=]<typename T>()
                           {
                               if constexpr(is_label<T>::value)
                                   fmt::print("{}:\n", prettify<T>{}(ts, 0));
                               else
                                   fmt::print("\t{}\n", prettify<T>{}(ts, 0));
                           });
}

template <typename T>
struct is_not_ts : std::negation<is_type_sequence<T>>
{
};

template <typelist::List Statements>
using remove_ts = typelist::filter<is_not_ts, Statements>;

template <typelist::List Instructions>
struct list_mag :
    std::integral_constant<
        unsigned,
        magnitude(typelist::length<remove_ts<Instructions>>) + 1u>
{
};

template <typelist::List Macros>
inline constexpr unsigned max_mag = *std::ranges::max_element(
    intlist::stdarray<
        typelist::to_integer_sequence<typelist::map<list_mag, Macros>>>);

template <typename Macro>
using macro_statements = Macro::statements;

/// @brief Pretty print kernel.
/// @tparam K kernel to pretty print.
/// @tparam Ms list of macros to pretty print.
/// @param ts enable type signatures.
/// @param bs show program as separate blocks.
/// @param em expand macros.
/// @param ln add line numbers.
template <Kernel K, typelist::List Ms = type_sequence<>>
void
pretty_print_kernel(const bool ts = false,
                    const bool bs = true,
                    const bool em = false,
                    const bool ln = true) noexcept
{
    using input_vars  = K::input_vars;
    using output_vars = K::output_vars;

    using labels = K::labels;
    using blocks = K::blocks;

    using statements           = K::statements;
    using statements_no_macros = remove_ts<statements>;
    using instructions         = K::instructions;

    const std::string inputs  = prettify<input_vars>{}(ts, 0);
    const std::string outputs = prettify<output_vars>{}(ts, 0);

    fmt::print("Input variables: {}\n", inputs);
    fmt::print("Output variables: {}\n\n", outputs);

    if(bs)
        typelist::for_each(typelist::zip<labels, blocks>{},
                           [=]<typename LC>()
                           {
                               fmt::print("{}:\n",
                                          prettify<fst_t<LC>>{}(ts, 0));
                               print_instructions<snd_t<LC>>(ts, ln);
                           });
    else if(em)
        print_instructions<instructions>(ts, ln);
    else
    {
        constexpr unsigned w =
            max_mag<typelist::cons<statements_no_macros,
                                   typelist::map<macro_statements, Ms>>>;

        typelist::for_each(
            Ms{},
            [=]<typename Macro>()
            {
                const std::string pps =
                    prettify<typename Macro::parameters>{}(ts, 0);

                using mstatements    = remove_ts<typename Macro::statements>;
                constexpr unsigned l = typelist::length<mstatements>;

                if(ln)
                    fmt::print("{:{}d} %macro {} {}\n",
                               1u,
                               w,
                               Macro::nameview,
                               pps);
                else
                    fmt::print("%macro {} {}\n", Macro::nameview, pps);

                print_instructions<mstatements, 2, w>(ts, ln);

                if(ln)
                    fmt::print("{:{}d} %endmacro\n\n", l + 2u, w);
                else
                    fmt::print("%endmacro\n\n");
            });

        if(ln)
            fmt::print("{:{}d} %kernel {} {}\n", 1u, w, K::nameview, inputs);
        else
            fmt::print("%kernel {} {}\n", K::nameview, inputs);

        print_instructions<statements_no_macros, 2u, w>(ts, ln);

        constexpr unsigned l = typelist::length<statements_no_macros>;

        if(ln)
            fmt::print("{:{}d} %endkernel {}\n", l + 2u, w, outputs);
        else
            fmt::print("%endkernel {}\n", outputs);
    }
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
        fmt::print(file, "{}\n", prettify<Var>{}(true, 0));
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
                                     { return prettify<Var>{}(true, 0); }),
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
            fmt::print("[Input: {}] Enter value:\n", prettify<var>{}(true, 0));
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
inline constexpr unsigned max_label_width = std::max(
    *std::ranges::max_element(intlist::stdarray<typelist::to_integer_sequence<
                                  typelist::map<label_length, Labels>>>),
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
        { print_word(prettify<Var>{}(ts, 0), cw); },
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

    print_word(get_label_name(labels{}, mem.pc(), K::halt_pos),
               max_label_width<labels>);
    typelist::for_each_masked(
        variables{},
        [&mem]<typename Var>(unsigned cw)
        { print_word(mem.template get<Var>(), cw); },
        mask,
        cw);

    fmt::print("\r\n");
}

template <std::size_t N, typename Registers>
std::array<std::string, N>
registers_list(Registers regs) noexcept
{
    std::array<std::string, N> ret =
        typelist::materialize_with<std::array<std::string, N>>(
            regs,
            []<typename S>() noexcept { return std::string(prettify<S>{}()); });
    std::ranges::sort(
        ret,
        [](const std::string& a, const std::string& b) noexcept
        { return !(b == vm::_pc_char) && (a == vm::_pc_char || a < b); });
    return ret;
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
    requires(!K::is_straightline)
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
                facade::pc(),
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
            while(base::pc() != halt_pos && c != stop)
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

            while(base::pc() != halt_pos && m_step_number++ < m_max_steps)
                base::step();
        }
    };

  public:
    using inspector =
        std::conditional_t<interactive, interactive_inspector, fast_inspector>;
};

/// @brief Kernel inspection main function.
/// @tparam K kernel to inspect
/// @tparam list of macros specification
/// @param argc pass here argc from main()
/// @param argv pass here argv from main()
/// @param inopts tuple of argparse options to provide input for kernel @p K
/// @param inget function that takes parse results and returns value or tuple of
/// input values
template <Kernel         K,
          typelist::List Macros = type_sequence<>,
          typename InputOpts,
          typename InputGet>
    requires(is_tuple_v<InputOpts>)
int
inspect(int              argc,
        char*            argv[],
        const InputOpts& inopts,
        InputGet         inget) noexcept
{
    // ignore PC
    constexpr auto vars = typelist::tail<typename K::variables>{};

    const auto pr = argparse::parse(
        std::tuple_cat(
            inopts,
            std::tuple{argparse::flag<'I', "info">("print program info"),
                       argparse::flag<'p', "print">("print program and exit"),
                       argparse::flag<'T', "type-annotations">(
                           "show type annotations"),
                       argparse::flag<'b', "blocks">("separate blocks"),
                       argparse::flag<'m', "macros">("expand macros"),
                       argparse::flag<'l', "line-numbers">("add line numbers"),
                       argparse::flag<'i', "interactive">("run interactive"),
                       argparse::option<'w', "column-width">(
                           "column width. Default is 20",
                           "N",
                           20u,
                           read_number<unsigned, true>),
                       argparse::option<'s', "max-steps">(
                           "maximum number of steps. Default is 100",
                           "N",
                           100u,
                           read_number<unsigned, true>),
                       argparse::option_list<'t', "track">("variables to track",
                                                           "vars")}),
        {argc, argv});

    const bool ts = pr.template get_opt<'T'>();
    const bool bs = pr.template get_opt<'b'>();
    const bool em = pr.template get_opt<'m'>();
    const bool ln = pr.template get_opt<'l'>();
    const bool pi = pr.template get_opt<'I'>();
    const bool pp = pr.template get_opt<'p'>();

    if(pi)
    {
        using reg_names                = K::reg_names;
        constexpr std::size_t num_regs = typelist::length<reg_names>;

        fmt::print("Basic blocks: {:>4}\n"
                   "Instructions: {:>4}\n"
                   "Registers:    {:>4}\n"
                   "Variables:    {:>4}\n"
                   "State size:   {:>4} bytes\n\n"
                   "Registers list: {}\n\n",
                   K::num_blocks,
                   typelist::length<typename K::stated_comps>,
                   num_regs,
                   typelist::length<typename K::variables>,
                   K::state_size,
                   fmt::join(registers_list<num_regs>(reg_names{}), " "));
        if(!pp) return 0;
    }

    if(pp)
    {
        pretty_print_kernel<K, Macros>(ts, bs, em, ln);
        return 0;
    }

    try
    {
        const unsigned cw = pr.template get_opt<'w'>();
        const unsigned ms = pr.template get_opt<'s'>();

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

            auto mask = read_var_mask(vars, pr.template get_opt<'t'>());
            if(!mask.any())
            {
                mask.set();
                mask.reset(0);
            }
            Inspector insp{ms, cw, ts, mask};
            invopply(insp, inget(pr));
        }
    }
    catch(const UnknownVar& ex)
    {
        fmt::print(stderr, "Unknown variable: {}\n", ex.value);
        fmt::print(stderr,
                   "Use variable number or name. Available variables:\n");
        print_variables(vars, stderr);
        return 2;
    }
    catch(const ReadException& ex)
    {
        print_errc(ex.ec, ex.sv, stderr);
        return 1;
    }

    return 0;
}
} // namespace loom::vm::dbg
