#pragma once

enum rc_line_type
{
    RCFILE_LINE_EQUALS, ///< foo = bar
    RCFILE_LINE_PLUS,   ///< foo += bar
    RCFILE_LINE_MINUS,  ///< foo -= bar
    RCFILE_LINE_CARET,  ///< foo ^= bar
    RCFILE_LINE_DIRECTIVE, /// line that does not set an option (e.g., alias)
    NUM_RCFILE_LINE_TYPES,
};
