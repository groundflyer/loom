// -*- C++ -*-
/// @file logging.hpp
/// Logging facilities.

#pragma once

#include <fmt/chrono.h>
#include <fmt/color.h>

#include <cstdio>

namespace lucid
{
/// @brief Simple logger with classic verbosity schema, timestamps and colorful
/// output.
///
/// Uses fmt to format messages.
///
/// Prints only into stdout and stderr.
class Logger
{
  public:
    /// @brief Indicates what kind of messages should be printed.
    ///
    /// The lower the level the more messages will be printed.
    /// For instance, if level is WARNING, only warnings, errors
    /// and critical errors will be printed.
    enum Level
    {
        DEBUG    = 10, /*!< all kind of messages */
        INFO     = 20, /*!< without debug messages */
        WARNING  = 30, /*!< warning and errors */
        ERROR    = 40, /*!< errors */
        CRITICAL = 50  /*!< only critical errors */
    };

    /// @brief Empty class tag type that used to indicate Logger
    /// if the output line should be rewritten in place.
    struct Flush
    {
    };

    /// @brief An instance of Flush.
    static constexpr Flush flush{};

    Logger() = delete;

    /// @param lvl_ Minimum verbosity level.
    explicit Logger(const Level lvl_) : lvl(lvl_) {}

    /// @brief Print a debug message to stdout.
    ///
    /// @param fs format string.
    /// @param args items to format.
    ///
    /// The message is printed only if the verbosity level is less or equal than
    /// Level::DEBUG.
    template <typename... Args>
    constexpr void
    debug(fmt::format_string<Args...> fs, Args&&... args) const noexcept
    {
        if(lvl <= Level::DEBUG)
            message<'D'>(stdout,
                         fmt::fg(fmt::terminal_color::white),
                         fs,
                         args...);
    }

    /// @brief Print an info message to stdout.
    ///
    /// @param fs format string.
    /// @param args items to format.
    ///
    /// The message is printed only if the verbosity level is less or equal than
    /// Level::INFO.
    ///
    /// The message prefix is highlighted with green.
    template <typename... Args>
    constexpr void
    info(fmt::format_string<Args...> fs, Args&&... args) const noexcept
    {
        if(lvl <= Level::INFO)
            message<'I'>(stdout,
                         fmt::fg(fmt::terminal_color::green),
                         fs,
                         args...);
    }

    /// @brief Print a warning to stderr.
    ///
    /// @param fs format string.
    /// @param args items to format.
    ///
    /// The message is printed only if the verbosity level is less or equal than
    /// Level::WARNING.
    ///
    /// The message prefix is highlighted with yellow.
    template <typename... Args>
    constexpr void
    warning(fmt::format_string<Args...> fs, Args&&... args) const noexcept
    {
        if(lvl <= Level::WARNING)
            message<'W'>(stderr,
                         fmt::fg(fmt::terminal_color::yellow),
                         fs,
                         args...);
    }

    /// @brief Report an error to stderr.
    ///
    /// @param fs format string.
    /// @param args items to format.
    ///
    /// The message is printed only if the verbosity level is less or equal than
    /// Level::ERROR.
    ///
    /// The message prefix is highlighted with red.
    template <typename... Args>
    constexpr void
    error(fmt::format_string<Args...> fs, Args&&... args) const noexcept
    {
        if(lvl <= Level::ERROR)
            message<'E'>(stderr,
                         fmt::fg(fmt::terminal_color::red),
                         fs,
                         args...);
    }

    /// @brief Report a critical error to stderr.
    ///
    /// @param fs format string.
    /// @param args items to format.
    ///
    /// The message is printed only if the verbosity level is less or equal than
    /// Level::CRITICAL.
    ///
    /// The message prefix is highlighted with bold red.
    template <typename... Args>
    constexpr void
    critical(fmt::format_string<Args...> fs, Args&&... args) const noexcept
    {
        if(lvl <= Level::CRITICAL)
            message<'C'>(stderr,
                         fmt::emphasis::bold |
                             fmt::fg(fmt::terminal_color::red),
                         fs,
                         args...);
    }

  private:
    Level lvl;

    template <char i>
    constexpr static void
    timestamp(std::FILE* out, const fmt::text_style& ts) noexcept
    {
        const std::time_t date = std::time(nullptr);
        fmt::print(out, ts, "[{:%T}][{}] ", *std::localtime(&date), i);
    }

    template <char i, char e, typename... Args>
    constexpr static void
    submessage(std::FILE*             out,
               const fmt::text_style& ts,
               Args&&... args) noexcept
    {
        timestamp<i>(out, ts);
        fmt::print(out, args...);
        if constexpr(e == '\r')
            // use VT100 clearline EL2 code
            std::fputs("\33[2K", out);
        std::putc(e, out);
    }

    template <char i, typename... Args>
    constexpr static void
    message(std::FILE* out, const fmt::text_style& ts, Args&&... args) noexcept
    {
        submessage<i, '\n'>(out, ts, args...);
    }

    template <char i, typename... Args>
    constexpr static void
    message(std::FILE*             out,
            const fmt::text_style& ts,
            Flush,
            Args&&... args) noexcept
    {
        submessage<i, '\r'>(out, ts, i, args...);
    }
};
} // namespace lucid
