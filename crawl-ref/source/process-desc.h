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
void process_description(T &proc, const describe_info &inf)
{
    const unsigned int line_width = proc.width();
    const          int height     = proc.height();

    string desc;
    // most stuff goes through translate() twice unnecessarily, yadda yadda yadda

    // How many lines is the title; we also seem to be adding 1 to
    // start with.
    int num_lines = count_desc_lines(filtered_lang(inf.title), line_width) + 1;

    int body_lines   = count_desc_lines(filtered_lang(inf.body.str()), line_width);
    const int suffix_lines = count_desc_lines(filtered_lang(inf.suffix), line_width);
    const int prefix_lines = count_desc_lines(filtered_lang(inf.prefix), line_width);
    const int footer_lines = count_desc_lines(filtered_lang(inf.footer), line_width)
                             + (inf.footer.empty() ? 0 : 1);

    if (inf.title.empty())
    {
        desc = filtered_lang(inf.body.str());
        // There is a default 1 line addition for some reason.
        num_lines = body_lines + 1;
    }
    else
    {
        desc = filtered_lang(inf.title) + "\n\n";
        desc += filtered_lang(inf.body.str());
        // Got 2 lines from the two \ns that weren't counted yet.
        num_lines += body_lines + 2;
    }

    // Prefer the footer over the suffix.
    if (num_lines + suffix_lines + footer_lines <= height)
    {
        desc = desc + filtered_lang(inf.suffix);
        num_lines += suffix_lines;
    }

    // Prefer the footer over the prefix.
    if (num_lines + prefix_lines + footer_lines <= height)
    {
        desc = filtered_lang(inf.prefix) + desc;
        num_lines += prefix_lines;
    }

    if (!inf.footer.empty() && num_lines + footer_lines <= height)
    {
        const int bottom_line = min(max(24, num_lines + 2),
                                    height - footer_lines + 1);
        const int newlines = bottom_line - num_lines;

        if (newlines >= 0)
        {
            desc.append(newlines, '\n');
            desc = desc + filtered_lang(inf.footer);
        }
    }

    // Display as many lines as will fit.
    int lineno = 0;
    while (!desc.empty())
    {
        const string line = wordwrap_line(desc, line_width, false, true);

        // If this is the bottom line of the display but not
        // the last line of text, print an ellipsis instead.
        if (++lineno < height || desc.empty())
        {
            proc.print(line);
            if (!desc.empty())
                proc.nextline();
        }
        else
        {
            proc.print("...");
            break;
        }
    }
}

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
