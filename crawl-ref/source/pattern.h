#pragma once

class pattern_match
{
public:
    static pattern_match succeeded(const string &s, int start = 0, int end = 0)
    {
        return pattern_match(true, s, start, end);
    }

    static pattern_match failed(const string &s = string())
    {
        return pattern_match(false, s, -1, -1);
    }

    operator bool () const
    {
        return matched;
    }

    string annotate_string(const string &color) const;

    const string &matched_text() const
    {
        return text;
    }

private:

    pattern_match(bool _matched, const string &_text, int _start, int _end)
        : matched(_matched), text(_text), start(_start), end(_end)
    {
    }

    bool matched;
    string text;
    int start;
    int end;
};

class base_pattern
{
public:
    virtual ~base_pattern() { }

    virtual bool valid() const = 0;
    virtual bool matches(const string &s) const = 0;
    virtual pattern_match match_location(const string &s) const = 0;
    virtual const string &tostring() const = 0;
};

class text_pattern : public base_pattern
{
public:
    text_pattern(const string &s, bool icase = false)
        : pattern(s), validated(false),
          isvalid(true), ignore_case(icase)
    {
    }

    text_pattern()
        : pattern(), validated(false),
         isvalid(false), ignore_case(false)
    {
    }

    text_pattern(const text_pattern &tp)
        : base_pattern(tp),
          pattern(tp.pattern),
          validated(tp.validated),
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

    bool valid() const override;
    bool matches(const string &s) const override;
    pattern_match match_location(const string &s) const override;

    const string &tostring() const override
    {
        return pattern;
    }

private:
    string pattern;
    mutable bool validated;
    mutable bool isvalid;
    bool ignore_case;
};

class plaintext_pattern : public base_pattern
{
public:
    plaintext_pattern(const string &s, bool icase = false)
        : pattern(s), ignore_case(icase)
    {
    }

    plaintext_pattern()
        : pattern(), ignore_case(false)
    {
    }

    const plaintext_pattern &operator= (const string &spattern);
    bool operator== (const plaintext_pattern &tp) const;

    bool empty() const { return !pattern.length(); }

    bool valid() const override { return true; }
    bool matches(const string &s) const override;
    pattern_match match_location(const string &s) const override;

    const string &tostring() const override
    {
        return pattern;
    }

private:
    string pattern;
    bool ignore_case;
};
