#ifndef PATTERN_H
#define PATTERN_H

#ifdef REGEX_PCRE
    // Statically link pcre on Windows
    #if defined(TARGET_OS_WINDOWS) || defined(TARGET_OS_DOS)
        #define PCRE_STATIC
    #endif

    #include <pcre.h>
#endif

#ifdef REGEX_POSIX
    // Do we still need to include sys/types.h?
    #include <sys/types.h>
    #include <regex.h>
#endif

// Pattern matching
void *compile_pattern(const char *pattern, bool ignore_case = false);
void free_compiled_pattern(void *cp);
bool pattern_match(void *compiled_pattern, const char *text, int length);

// Globs are always available.
void *compile_glob_pattern(const char *pattern, bool ignore_case = false);
void free_compiled_glob_pattern(void *cp);
bool glob_pattern_match(void *compiled_pattern, const char *text, int length);

typedef void *(*p_compile)(const char *pattern, bool ignore_case);
typedef void (*p_free)(void *cp);
typedef bool (*p_match)(void *compiled_pattern, const char *text, int length);

class base_pattern
{
public:
    virtual ~base_pattern() { }

    virtual bool valid() const = 0;
    virtual bool matches(const std::string &s) const = 0;
};

template <p_compile pcomp, p_free pfree, p_match pmatch>
class basic_text_pattern : public base_pattern
{
public:
    basic_text_pattern(const std::string &s, bool icase = false)
        : pattern(s), compiled_pattern(NULL),
          isvalid(true), ignore_case(icase)
    {
    }

    basic_text_pattern()
        : pattern(), compiled_pattern(NULL),
         isvalid(false), ignore_case(false)
    {
    }

    basic_text_pattern(const basic_text_pattern &tp)
        : base_pattern(tp),
          pattern(tp.pattern),
          compiled_pattern(NULL),
          isvalid(tp.isvalid),
          ignore_case(tp.ignore_case)
    {
    }

    ~basic_text_pattern()
    {
        if (compiled_pattern)
            pfree(compiled_pattern);
    }

    const basic_text_pattern &operator= (const basic_text_pattern &tp)
    {
        if (this == &tp)
            return tp;

        if (compiled_pattern)
            pfree(compiled_pattern);
        pattern = tp.pattern;
        compiled_pattern = NULL;
        isvalid      = tp.isvalid;
        ignore_case  = tp.ignore_case;
        return *this;
    }

    const basic_text_pattern &operator= (const std::string &spattern)
    {
        if (pattern == spattern)
            return *this;

        if (compiled_pattern)
            pfree(compiled_pattern);
        pattern = spattern;
        compiled_pattern = NULL;
        isvalid = true;
        // We don't change ignore_case
        return *this;
    }

    bool compile() const
    {
        return !empty()?
            !!(compiled_pattern = pcomp(pattern.c_str(), ignore_case))
          : false;
    }

    bool empty() const
    {
        return !pattern.length();
    }

    bool valid() const
    {
        return isvalid
            && (compiled_pattern || (isvalid = compile()));
    }

    bool matches(const char *s, int length) const
    {
        return valid() && pmatch(compiled_pattern, s, length);
    }

    bool matches(const char *s) const
    {
        return matches(std::string(s));
    }

    bool matches(const std::string &s) const
    {
        return matches(s.c_str(), s.length());
    }

    const std::string &tostring() const
    {
        return pattern;
    }

private:
    std::string pattern;
    mutable void *compiled_pattern;
    mutable bool isvalid;
    bool ignore_case;
};

typedef
basic_text_pattern<compile_pattern,
                   free_compiled_pattern, pattern_match> text_pattern;

typedef
basic_text_pattern<compile_glob_pattern,
                   free_compiled_glob_pattern,
                   glob_pattern_match> glob_pattern;

#endif
