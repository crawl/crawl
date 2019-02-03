# Developer guidelines for using random numbers in DCSS

Crawl attempts to be deterministically seeded: given a seed (that may be
chosen randomly), dungeon generation (and ideally, everything) is deterministic
thereafter. It does this by using a random number generator that has
"reproducible" behavior,
[http://www.pcg-random.org/](http://www.pcg-random.org/). This leads to certain
practices and caveats that help maintain the usefulness of seeding.

## 1. Background

First, it may help to know a bit about how our RNG works. Given some seed,
a PCG generator generates a sequence of 32-bit unsigned integers, that is
deterministic but hard to predict. You can think of this like a code book:
for a given seed, the integer at index i will always be the same. The various
random functions (e.g. `random2`) then package information from these integers
in a way that is a bit more usable while programming. This means that the state
of the RNG is equivalent to the *number of draws from the generator*. If this
number of draws diverges for any reason between e.g. two versions of crawl,
then the randomization behavior will sharply diverge, probably permanently,
very soon after that. In most cases, the results of a single random draw at
some point in code execution will impact the number of random draws at a later
point. For example, during dungeon generation, the position of a vault might
be chosen by two `random2` calls (which usually does correspond to two random
draws from the raw generator). The position of this vault will then impact what
other vaults can be placed in the same layout, which is checked (roughly) by
randomly trying vaults until one fits -- which will lead to varying numbers of
rng draws.

The dungeon generator RNGs are partitioned by branch, and separate from the UI
and gameplay RNGs. However, vault choice is not fully independent across branch,
so the order of branch generation matters quite a bit. Given a fixed order, a
seed *should* fully determine a dungeon, but this mapping is quite delicate.

## 2. Breaking seed mappings

The following changes (which are often desireable changes) can break the seed
to dungeon mappings, in some way:

* adding or removing a vault
* adding or removing monster types or uniques
* adding or removing items, unique or otherwise

It is extremely hard to predict how big the break will be -- it may be small,
or it may result in an entirely different dungeon. (For example, changing D:1
arrival vaults will likely drastically change the mapping.)

For this reason, seeds should be assumed to be tied to a specific version of
dungeon crawl. Changes that break seeds for stable releases should be avoided
without introducing a new stable release right away -- because online servers
usually update right away, this will cause seeds to diverge between online and
offline versions.

All bets are off for trunk seed->dungeon mappings, which might at best be
specific to a single commit. Similarly, upgrading a game (trunk or otherwise)
where the dungeon is not pregenerated will lead to divergence from *both* seed
to dungeon mappings.

## 2.1. Detecting unwanted seed divergence

Detecting this can be quite hard, but there are a few built-in tests that run
in Travis CI and can be run locally. These are `d1_vaults.lua` (which prints
vaults for a few dungeon levels for 10 seeds), and the deeper but less broad
`vault_catalog.lua` test, which prints the entire vault catalog for one seed,
and does a bunch of seed stability tests by comparing that vault catalog across
multiple runs, for several fixed and randomly chosen seeds. This test is quite
time and cpu-intensive (a bit like mapstat), and the default settings are quite
modest, but you can tweek the number of seeds, runs, and use it to hand check
custom seeds by running it locally. To directly run this test in the most
useful fashion, the magic invocation is (can be further combined with pipes or
redirects):

    util/fake_pty ./crawl -test vault_catalog.lua 2>&1

If you haven't already, you may need to run `make util/fake_pty` first.

## 3. Specific coding guidelines and caveats

The biggest challenge in ensuring stable seed to dungeon mappings involves
ensuring that random numbers are drawn from the generators in the same order.
There are two specific areas where care is required.

### 3.1. random call evaluation order

C++, in many cases, does not guarantee stable evaluation order for function
calls, and this can result in different behaviors across compilers, OSs, and
potentially even execution instances. For background, see:

* [https://en.cppreference.com/w/cpp/language/eval_order](https://en.cppreference.com/w/cpp/language/eval_order)
* [https://wiki.sei.cmu.edu/confluence/display/cplusplus/EXP50-CPP.+Do+not+depend+on+the+order+of+evaluation+for+side+effects](https://wiki.sei.cmu.edu/confluence/display/cplusplus/EXP50-CPP.+Do+not+depend+on+the+order+of+evaluation+for+side+effects)

Because the RNG system works by side-effect, this means that there are various
cases where the compiler doesn't guarantee the order of random call side-effect.
The two most important cases to watch out for are:

* order of calls within arithmetic expressions, and
* order of calls within comma-separated arguments to functions, including
  constructors.

For example, expressions like the following *does not have a guaranteed order of
function calls* in any version of C or C++:

    const int x = 10 + random2(5) - random2(3); // bad
    auto y = f(random2(5), random2(3)); // bad

A particularly tempting paradigm that was common in dcss code before seeding
that instantiates the latter is:

    const coord_def pos(random2(5), random2(5)); // bad

This is not a theoretical concern: we have found that the order of calls in
cases like these does vary in practice between different compilers on different
OSs, and so any of these calls might get different results. (Because of the
layers of abstraction, the results of swapping call order are not as simple as
swapping the random number results, even if the calls appear the same.)

In order to get stable behavior, what you need to do is ensure a "sequence
point" between any random number calls (see the links above). This unfortunately
leads to less compact code, and cases where `const`ing is not possible. For
example, here is a better version of two of the above cases:

    int x = 10 + random2(5); // sequence point
    x -= random2(3);

    coord_def pos;
    pos.x = random2(5);
    pos.y = random2(5);

Even operators that are commutative (e.g. `+`), because of the way `random2` is
implemented, in principle aren't safe to have in the same expression.

What sorts of expressions *do* ensure evaluation order? See the above links for
the gory details, but the main safe one is boolean expressions, which do have a
stable order. This does include `? :` expressions. So for example the following
is ok (at least in terms of RNG call sequencing):

    if (x_chance_in_y(1,5) && coinflip()) // stable sequencing
        do_something();

For everything else, when in doubt, break it into multiple expressions
that are sequenced by `;`s.

### 3.2. choosing from lists

When you are randomly choosing an item from a list (e.g. by randomly picking
an index in the list range), be careful about both the underlying list order,
and the iteration order.

**Underlying list order**: This is a case where the RNG behavior can be fine,
but the list order itself could lead to divergence. For example, when choosing
from a list of files provided by the OS, you cannot assume that all OSs and OS
versions will give you these files in a stable order - sort it yourself.

**Iteration order**: If you are using some sort of reservoir-sampling style of
random choice algorithm, you will iterate through the container, stopping the
iteration at some point determined by one or more random draws. If the iteration
order is not guaranteed to be defined, this can lead to different results from
run to run. For C++ code, this generally isn't an issue (or at least, hasn't
come up so far), but it does show up in lua code and in the lua-C++ interface.
In Lua, iteration via `pairs` or the C equivalents over a table is not
guaranteed to have a stable order, and we have found that in practice it doesn't
-- leading to different choices with the same random number draws. The `ipairs`
iterator is safe, but places some obvious constraints on the structure you are
using. Sorting the underlying container may be a solution as well.

Some specific lua tips: rather than rolling your own weighted randomness, if at
all possible use one of the following functions: `util.random_choose_weighted`,
`util.random_choose_weighted_i`, and `util.random_weighted_from` (which all
operator on an array of some strip and use a stable iteration order), or
`util.random_random_weighted_keys`, which requires sortable keys and will use
the sort order of the keys in its iteration. If you have an unweighted list,
you can use `util.random_from`. If the items you are choosing from are
sortable (e.g, strings), you can still use a weight table as long as you turn
it into a stably-ordered array with `util.sorted_weight_table`. This takes a
table mapping values to weights, and produces an array of value-weight pairs
that then can be used for `util.random_choose_weighted`.

When writing Lua-C++ interface code, be aware that `lua_next` does not guarantee
order any more than `pairs`, *even if* the table is an array. Bugs from this
will be fairly indeterminate and hard to spot, but do show up in practice.
Either use Lua's facilities for `ipairs`-style iteration (which are a bit weak
in the version of Lua we use), or sort the results on the C side (in a way that
doesn't rely on runtime information). For example, loading an array of strings
into a `map<int,string>` will give you a stable sort order. Or, if the order
in the array is not important, you can use `lua_next` with a vector and
`push_back`, and then sort the results.

### 3.3. other sources of randomization

Don't use other sources of randomization in crawl besides crawl RNGs. For
example, don't use lua's `math.rand`, or C stdlib's `rand`.

### 3.4. Keeping the .des cache stable

Try to avoid vault design elements that will lead to variable .des caches; any
random choices made here will be outside of levelgen rng, and potentially
set at the time the player first opens crawl. Especially try to avoid
determining a set of tags for a map randomly using lua `tags` calls.

## 4. Dealing with seed divergence

Seed divergence is when the same seed, with the same generation order, leads to
distinct dungeons under some circumstance. There are two main ways that seed
divergence can present:

1. Seed instability on a single device
2. Seed instability across devices

### 4.1. Seed instability on a single device

This presents itself as extra "randomness" that is not determined by the seed.
The simplest case to find would be where some system-specific information is
directly interacting with one or more random draws: e.g. don't multiply a
random number by the current system time. More subtle instances of this,
mentioned above in section 3.2, are things like choosing randomly from a list
of files that might change from run to run. While some effort has been made
to partition player ghost generation off from levelgen, since player ghosts
can vary from one time to another, this is one potential source of divergence.
(For example, early in the implementation of strong seeding, a bug was present
where whether a ghost was generated with an enchantment would change the number
of random draws needed to place the ghost, leading to different rng state for
the next levelgen decision.)

The more subtle kind of problem of this kind is when some sort of undefined
behavior is not so obviously undefined to the developer, but can lead to
run-to-run variation in corner cases. *By far* the biggest example so far has
been iteration order in lua, discussed above in section 3.2. (In general,
undefined behavior in C++ code is usually defined one way or the other by a
particular compiler and stdlib, and leads to seed instability across devices
rather than within devices; I wouldn't take this as a hard rule though.)

This is, believe it or not, the easiest case to debug because it can usually be
replicated. However, before spending a lot of time trying to replicate a case of
this, first check the following issue: is the generation behavior different when
running crawl with a clean .des cache and bones file as later? Many early
seeding bugs presented themselves this way, and to replicate a bug of this kind
you will need to clear the caches.

### 4.2. Seed instability across devices

Seed instabilities across devices are often quite hard to replicate and debug,
so fire up the VM / container. In practice, most of the cases of this that we
have found involve things where C++ or stdlib behavior is undefined and
different compilers or build options (or OSs, perhaps) lead to different
choices. The biggest example is the ordering of random calls in arithmetic
expressions, discussed in section 4.1, so this is definitely the first thing
to look for. The ##crawl-dev population in aggregate has pretty detailed
knowledge of the C++ specs, so if you aren't sure whether somethig in particular
is defined, this is a good place to ask.

Before opening the VM, however, if what you are seeing is a difference between
your device and Travis CI results - keep in mind that Travis runs its tests
from a clean state, and that this may be a case of seed instability within
a device that is dependent on the cache state. Also, many tests run first
before the vault catalog test, so double check if there's some game state factor
that is affected by earlier tests. (E.g. compare running the vault catalog test
directly with running `make test`).
