/// \file
/// \brief Ensure an initialization is performed exactly once for all tests.
/// \details Some initialization steps of crawl has no corresponding
/// clean-up function. We can't run those initialization steps twice. And,
/// we have to ensure all the tests together only run those steps exactly
/// once.
/// \warning This is a temporary solution. The real solution is of course write
/// the clean-up functions.
#include "init_guards.h"

namespace init_guards {
/// \brief Get an instance of InitGuards
/// \details Get an instance of the guards, which is a singleton.
/// The guards are indexed by \ref InitStep. We can use a guard to ensure
/// exactly one execution of an initialization step of crawl across all tests.
/// Here is an example:
/// `using namespace init_guards;`
/// `auto& guards = get_instance()  // get an instance of guards;`
/// `if (guards[keybindings]) _init_keybindings;  // run only if permitted`
/// \see \ref init_guards::InitStep and details::InitGuards
    details_init_guards::InitGuards& get_init_guards_instance() {
        static details_init_guards::InitGuards singleton;
        return singleton;
    }
}

namespace details_init_guards
{ // Implementation details. Please don't use this directly.
    /// \brief A thread-safe guard that permits exactly one execution
    class OneExecGuard
    {
    public:
        OneExecGuard();

        bool is_permitted();

    private:
        std::mutex m;
        bool have_permitted; ///< have we handed out the permission?
    };

/// \brief A thread-safe guard that permits exactly one execution
    class OneExecGuard;

    OneExecGuard::OneExecGuard() : have_permitted(false) {}

/// \brief Check for permission to run
/// \details Only one thread will have the permission to run. The other
/// threads won't get the permission.
/// \return If permitted to run, true. Otherwise, false.
    bool OneExecGuard::is_permitted()
    {
        if (have_permitted)
        {  // if we have already handed out the permission, say no.
            return false;
        } else
        {   // if we haven't handed out the permission at this moment ...
            // Lock and make the candidate threads form a line and come in
            // one by one.
            m.lock();
            if (!have_permitted)
            { // The first guy in the line will reach this part first.
                // Note to self: remember to say no to the other guys.
                have_permitted = true;
                m.unlock(); // Tell the next guy come in.
                return true; // Tell the first guy: you get it.
            } else
            { // The other guys in the line will come here.
                return false; // Tell the other guys: nope.
            }
        }
    }

/// \brief An array of guards indexed by initialization steps
/// \details Check permission by indexing
/// \see \ref OneExecGuard, \ref Init_steps
    class InitGuards;

    InitGuards::InitGuards() : guards(init_guards::InitStep::SIZE) {}

/// \brief Check permission to run a particular initialization step
/// \param step The initialization step. \see Init_step
    bool InitGuards::operator[](init_guards::InitStep step)
    {
        return guards[step].is_permitted();
    }
} // namespace details