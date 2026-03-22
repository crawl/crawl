#pragma once

#if defined(TARGET_COMPILER_VC) && defined(__has_include)
#if __has_include(<libintl.h>)
#include <libintl.h>
#define DCSS_GETTEXT_AVAILABLE 1
#endif
#endif

#ifndef DCSS_GETTEXT_AVAILABLE
#ifndef TARGET_COMPILER_VC
#include <libintl.h>
#define DCSS_GETTEXT_AVAILABLE 1
#endif
#endif

#ifndef DCSS_GETTEXT_AVAILABLE
#define gettext(String) (String)
#define bindtextdomain(Domain, Directory) (Domain)
#define bind_textdomain_codeset(Domain, Codeset) (Domain)
#define textdomain(Domain) (Domain)
#endif
