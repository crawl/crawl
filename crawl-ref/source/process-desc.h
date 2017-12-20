/**
 * @file
 * @brief Templates used in describe.cc and in some tiles-only sources
 */

#pragma once

#include "describe.h"
#include "lang-fake.h"
#include "stringutil.h"

template<class T> void process_description(T &proc, const describe_info &inf);
template<class T> void process_quote(T &proc, const describe_info &inf);

/* ***********************************************************************
 * template implementations
 * *********************************************************************** */
// My kingdom for a closure.
template<class T>
void process_quote(T &proc, const describe_info &inf)
{
    const unsigned int line_width = proc.width();
    const          int height     = proc.height();

    string desc;

    // How many lines is the title; we also seem to be adding 1 to
    // start with.
    int num_lines = count_desc_lines(inf.title, line_width) + 1;

    int body_lines   = count_desc_lines(inf.quote, line_width);

    // Maybe skip the body if body + title would be too many lines.
    if (inf.title.empty())
    {
        desc = inf.quote;
        // There is a default 1 line addition for some reason.
        num_lines = body_lines + 1;
    }
    else if (body_lines + num_lines + 2 <= height)
    {
        desc = inf.title + "\n\n";
        desc += inf.quote;
        // Got 2 lines from the two \ns that weren't counted yet.
        num_lines += body_lines + 2;
    }
    else
        desc = inf.title + "\n";

    if (num_lines <= height)
    {
        const int bottom_line = min(max(24, num_lines + 2), height);
        const int newlines = bottom_line - num_lines;

        if (newlines >= 0)
            desc.append(newlines, '\n');
    }

    while (!desc.empty())
    {
        proc.print(wordwrap_line(desc, line_width, false, true));
        if (!desc.empty())
            proc.nextline();
    }
}
