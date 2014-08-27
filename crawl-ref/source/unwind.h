#ifndef UNWIND_H
#define UNWIND_H

/** Type that gives an lvalue a dynamically-scoped temporary value.  An
 *  unwind_var wraps a variable or other writable lvalue, assigns it a
 *  temporary value, and restores the original (or a specified) value when
 *  the unwind_var goes out of scope or is otherwise destroyed.
 */
template <typename T>
class unwind_var
{
public:
    /** Wrap the lvalue val_ and on unwinding restore its original value. */
    unwind_var(T &val_) : val(val_), oldval(val_) { }

    /** Wrap the lvalue val_, assign it the temporary value newval, and
     *  on unwinding restore its original value.
     */
    unwind_var(T &val_, T newval) : val(val_), oldval(val_)
    {
        val = newval;
    }

    /** Wrap the lvalue val_, assign it the temporary value newval, and
     *  on unwinding assign it the value reset_to.
     */
    unwind_var(T &val_, T newval, T reset_to) : val(val_), oldval(reset_to)
    {
        val = newval;
    }

    ~unwind_var()
    {
        val = oldval;
    }

    /** Get the current (temporary) value of the wrapped lvalue. */
    T value() const
    {
        return val;
    }

    /** Get the value that will be used to restore the wrapped lvalue. */
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
