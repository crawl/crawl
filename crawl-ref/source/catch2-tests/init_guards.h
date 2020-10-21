#pragma once

#include <vector>

namespace details_init_guards
{
    class InitGuards;  // forward declaration
}

namespace init_guards
{
    /// \brief Initialization steps that should only be run once
    /// \note To add a new guarded initialization step, add a new entry in this
    /// enum and increase the SIZE by 1.
    /// \see \ref init_guards::InitStep and details::InitGuards
    enum InitStep : size_t
    {
        keybindings = 0,     ///< init_keybindings
        element_colours = 1, ///< init_keybindings
        SIZE = 2             ///< Number of guarded initialization steps
    };

    details_init_guards::InitGuards& get_init_guards_instance();
}

namespace details_init_guards
{
    class OneExecGuard;

    class InitGuards
    {
    public:
        explicit InitGuards();

        bool operator[](init_guards::InitStep step);

    private:
        std::vector<OneExecGuard> guards;
    };
}
