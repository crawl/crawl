#pragma once

#include <cstring>
#include <string>

#if defined(__has_include)
#if __has_include(<libintl.h>)
#include <libintl.h>
#define DCSS_GETTEXT_AVAILABLE 1
#endif
#endif

#if !defined(DCSS_GETTEXT_AVAILABLE) && !defined(__has_include) \
    && !defined(TARGET_COMPILER_VC)
#include <libintl.h>
#define DCSS_GETTEXT_AVAILABLE 1
#endif

#ifndef DCSS_GETTEXT_AVAILABLE
#define gettext(String) (String)
#define dgettext(Domain, String) (String)
#define bindtextdomain(Domain, Directory) (Domain)
#define bind_textdomain_codeset(Domain, Codeset) (Domain)
#define textdomain(Domain) (Domain)
#endif

#ifndef N_
#define N_(String) (String)
#endif

#ifndef NC_
#define NC_(Context, String) (String)
#endif

#ifndef pgettext
static inline const char *pgettext_aux(const char *context, const char *msgid)
{
    if (!msgid)
        return "";

#ifndef DCSS_GETTEXT_AVAILABLE
    (void)context;
    return msgid;
#else
    if (!context || !*context)
        return gettext(msgid);

    thread_local std::string key;
    key = context;
    key.push_back('\004');
    key += msgid;

    const char *translated = dgettext("crawl-data", key.c_str());
    return std::strchr(translated, '\004') ? msgid : translated;
#endif
}

#define pgettext(Context, String) pgettext_aux((Context), (String))
#endif
