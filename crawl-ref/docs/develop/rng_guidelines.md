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

## 3. Specific coding guidelines and caveats

The biggest challenge in ensuring stable seed to dungeon mappings involves
ensuring that random numbers are drawn from the generators in the same order.
There are two specific areas where care is required.

### 3.1 random call evaluation order

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

### 3.2 choosing from lists

When you are randomly choosing an item from a list (e.g. by randomly picking
an index in the list range), be careful about the underlying list order. This
is a case where the RNG behavior can be fine, but the list order itself could
lead to divergence. For example, when choosing from a list of files provided
by the OS, you cannot assume that all OSs and OS versions will give you these
files in a stable order - sort it yourself.

### 3.3 other sources of randomization

Don't use other sources of randomization in crawl besides crawl RNGs. For
example, don't use lua's `math.rand`, or C stdlib's `rand`.
