// -*- C++ -*-
// dbg.hpp
//

#pragma once

#include "bse.hpp"
#include "core.hpp"

#include <utils/common.hpp>
#include <utils/typeinfo.hpp>

#include <termios.h>
#include <unistd.h>

namespace lucid
{
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
