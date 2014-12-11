#ifndef PATTERN_H
#define PATTERN_H

class base_pattern
{
public:
    virtual ~base_pattern() { }

    virtual bool valid() const = 0;
    virtual bool matches(const string &s) const = 0;
    virtual const string &tostring() const = 0;
};

class text_pattern : public base_pattern
{
public:
    text_pattern(const string &s, bool icase = false)
        : pattern(s), compiled_pattern(nullptr),
          isvalid(true), ignore_case(icase)
    {
    }

    text_pattern()
        : pattern(), compiled_pattern(nullptr),
         isvalid(false), ignore_case(false)
    {
    }

    text_pattern(const text_pattern &tp)
        : base_pattern(tp),
          pattern(tp.pattern),
          compiled_pattern(nullptr),
          isvalid(tp.isvalid),
          ignore_case(tp.ignore_case)
    {
    }

    ~text_pattern();
    const text_pattern &operator= (const text_pattern &tp);
    const text_pattern &operator= (const string &spattern);
    bool operator== (const text_pattern &tp) const;
    bool compile() const;

    bool empty() const { return !pattern.length(); }

    bool valid() const
    {
        return isvalid
            && (compiled_pattern || (isvalid = compile()));
    }

    bool matches(const char *s, int length) const;

    bool matches(const char *s) const
    {
        return matches(s, strlen(s));
    }

    bool matches(const string &s) const
    {
        return matches(s.c_str(), s.length());
    }

    const string &tostring() const
    {
        return pattern;
    }

private:
    string pattern;
    mutable void *compiled_pattern;
    mutable bool isvalid;
    bool ignore_case;
};
#endif
