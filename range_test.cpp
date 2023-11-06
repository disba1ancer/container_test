#include <set>
#include <iostream>

template <typename T>
struct ComparatorTraits {
    struct ThreeWay {
        template <typename A, typename B>
        requires std::invocable<T, const A&, const B&>
        auto operator()(const A& a, const B& b) const
        {
            T comp;
            return comp(a, b);
        }
        template <typename A, typename B>
        requires (
            !std::invocable<T, const A&, const B&> &&
            std::invocable<T, const B&, const A&>
        )
        auto operator()(const A& a, const B& b) const
        {
            T comp;
            return 0 <=> comp(b, a);
        }
    };
    struct Less {
        template <typename A, typename B>
        requires std::invocable<ThreeWay, const A&, const B&>
        bool operator()(const A& a, const B& b) const
        {
            ThreeWay comp;
            return comp(a, b) < 0;
        }
    };
};

struct Range {
    unsigned begin;
    unsigned end;
    using ByBegin = unsigned;
    enum ByEnd : unsigned {};
};

struct TWComp {
    auto operator()(const Range& a, const Range& b) const
    {
        return a.begin <=> b.begin;
    }
    auto operator()(const Range& a, unsigned b) const
    {
        return a.begin <=> b;
    }
    auto operator()(const Range& a, Range::ByEnd b) const
    {
        return a.end <=> unsigned(b);
    }
};

struct Comp {
    using is_transparent = std::true_type;
    auto operator()(const Range& a, const Range& b) const
    {
        return a.begin < b.begin;
    }
    auto operator()(const Range& a, unsigned b) const
    {
        return a.begin < b;
    }
    auto operator()(const Range& a, Range::ByEnd b) const
    {
        return a.end < unsigned(b);
    }
    auto operator()(unsigned a, const Range& b) const
    {
        return a < b.begin;
    }
    auto operator()(Range::ByEnd a, const Range& b) const
    {
        return unsigned(a) < b.end;
    }
};

using RangeSet = std::set<Range, Comp>;

int main(int, char*[])
{
    RangeSet ranges;
    ranges.emplace(0, 10);
    ranges.emplace(20, 30);
    ranges.emplace(40, 50);
    ranges.emplace(60, 70);
    ranges.emplace(80, 90);
    auto begin = ranges.lower_bound(Range::ByEnd(30));
    auto end = ranges.upper_bound(Range::ByBegin(60));
    std::cout << begin->begin << " " << begin->end << "\n";
    std::cout << end->begin << " " << end->end << std::endl;
}
