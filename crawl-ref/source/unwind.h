#ifndef UNWIND_H
#define UNWIND_H

template <typename T>
class unwind_var
{
public:
    unwind_var(T &val_, T newval, T reset_to) : val(val_), oldval(reset_to)
    {
        val = newval;
    }
    unwind_var(T &val_, T newval) : val(val_), oldval(val_)
    {
        val = newval;
    }
    unwind_var(T &val_) : val(val_), oldval(val_) { }
    ~unwind_var()
    {
        val = oldval;
    }

    T value() const
    {
        return val;
    }

    T original_value() const
    {
        return oldval;
    }

private:
    T &val;
    T oldval;
};

typedef unwind_var<bool> unwind_bool;

#endif
