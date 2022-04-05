template <typename Enum, typename Predicate>
vector<Enum> filter_enum(Enum max, Predicate filter)
{
    vector<Enum> things;
    for (int i = 0; i < max; ++i)
    {
        const Enum thing(static_cast<Enum>(i));
        if (filter(thing))
            things.push_back(thing);
    }
    return things;
}
