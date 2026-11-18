#pragma once
// Offline home-card copy and a deterministic, allocation-free shuffle rotation.
// Feed Update on every UI frame, including frames when the home card is hidden.
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace spatial_quotes {

// Keep the guard effective in the Android build, which enables fast-math.
inline bool FiniteClock(double value) {
    std::uint64_t bits;
    std::memcpy(&bits, &value, sizeof(bits));
    return (bits & UINT64_C(0x7ff0000000000000)) != UINT64_C(0x7ff0000000000000);
}

struct Quote {
    const char* title;
    const char* subtitle;
};

inline constexpr Quote Quotes[] = {
    {"把节奏，交还给自己。", "这里的快慢，都可以按你的习惯来。"},
    {"留一点空白给今天。", "设置不必太满，舒服就很好。"},
    {"刚刚好，也是一种答案。", "顺手的样子，可以很简单。"},
    {"此刻，只管自在。", "熟悉的选项，都在熟悉的位置。"},
    {"让眼前，清楚一点。", "少一点杂乱，多一点从容。"},
    {"今天也有自己的步调。", "这一小片空间，听你的安排。"},
    {"喜欢的样子，慢慢调。", "从一个顺眼的颜色开始。"},
    {"把日常，放得顺手。", "常用的东西，近一点就好。"},
    {"安静一点，也挺好。", "画面留白，思绪就有了位置。"},
    {"欢迎回来，位置给你留着。", "熟悉的空间，又见面了。"},
    {"不赶时间的时候。", "连一次点击，都可以很轻。"},
    {"简单，是留给自己的余地。", "眼前清爽一点，心情也松一点。"},
    {"今天，换个顺眼的颜色。", "小小的变化，也能让人舒展。"},
    {"有些舒服，不用解释。", "习惯怎样，就调成怎样。"},
    {"把注意力，留给喜欢的事。", "日常设置，在这里轻轻收好。"},
    {"这里没有标准姿势。", "顺手、顺眼，就已经很好。"},
    {"轻一点，刚好。", "少一些打扰，多一些自己的节奏。"},
    {"慢半拍，也能看清。", "细节一直都在，不用着急。"},
    {"让熟悉的事，更顺一点。", "一个小调整，也值得被认真对待。"},
    {"留个舒服的位置。", "把常用选项，放在触手可及的地方。"},
    {"今天的心情，是什么颜色？", "喜欢哪一种，就让它陪你一会儿。"},
    {"窗外很远，眼前很近。", "先照顾好这一小块画面。"},
    {"小小空间，大大自在。", "这里装得下你的使用习惯。"},
    {"不必每次都从头开始。", "顺手的设置，可以继续用下去。"},
    {"把复杂，收进整齐里。", "看得清的选项，用起来更轻松。"},
    {"清爽一点，思路就宽一点。", "给眼睛，也留一口呼吸。"},
    {"今天适合，舒服一点。", "连细节都可以温柔一些。"},
    {"松一口气，再点一下。", "不着急，这里一直都在。"},
    {"顺手，比花哨更耐看。", "常用的细节，值得慢慢打磨。"},
    {"一切就位，随你安排。", "你的习惯，就是这里的参考。"},
    {"留住一点小偏爱。", "颜色、大小，都可以有自己的答案。"},
    {"刚打开，就像回到老地方。", "熟悉一点，操作也轻松一点。"},
    {"今日份清爽，已到位。", "让常用的功能，安静待在身边。"},
    {"有空，给自己一点空。", "不用填满画面，也不用填满每一刻。"},
    {"让每一次打开，都顺心。", "一个熟悉的入口，一点轻松的心情。"},
    {"世界很忙，这里慢一点。", "照着自己的节奏来就好。"},
    {"小变化，也有小欢喜。", "试个新颜色，今天换种心情。"},
    {"自在，是最合适的尺寸。", "大一点、小一点，都由你决定。"},
    {"给眼前，添一点秩序。", "把常用的留下，把杂乱轻轻收起。"},
    {"日常的细节，也有分量。", "顺手的每一下，都算数。"},
    {"好用的东西，不必很吵。", "安静待着，需要时刚好在。"},
    {"今天，也给眼睛放个小假。", "画面清透一点，视线轻松一点。"},
    {"平常的一天，也能顺顺的。", "从一个舒服的界面开始。"},
    {"把偏好，放在明面上。", "不用迁就，调成自己喜欢的样子。"},
    {"习惯的方向，最省心。", "常用的入口，不用兜圈子。"},
    {"让操作少一点犹豫。", "一眼看清，一点就到。"},
    {"此处，允许一点随意。", "按自己的想法摆放这片空间。"},
    {"轻轻打开，慢慢进入。", "让画面和心情，都有个缓冲。"},
    {"颜色不多，喜欢就够。", "选中那一眼顺心的。"},
    {"不动声色，也很妥帖。", "细节到位，日常自然顺手。"},
    {"这一屏，留给你的习惯。", "常用什么，就让什么更靠近。"},
    {"今天的留白，有点好看。", "少一点拥挤，多一点空间。"},
    {"把舒适，调到合适。", "每个小选项，都可以慢慢来。"},
    {"心里有数，手上轻松。", "熟悉的设置，看一眼就明白。"},
    {"日常，也可以不费力。", "简单一点，也能安排得很好。"},
    {"一点清透，一点自在。", "让背景退后，让眼前更清楚。"},
    {"先把这一刻，放舒服。", "有时候，顺眼就是很好的开始。"},
    {"不必热闹，也能有趣。", "一点颜色，一点刚好的细节。"},
    {"常用的，都在手边。", "把来回寻找的时间省下来。"},
    {"从容一点，手感更好。", "每次打开，都少一点忙乱。"},
    {"今日心情：随手都顺。", "如果还差一点，就再微调一点。"},
    {"给生活，留个轻盈的角落。", "这里不大，放松刚好。"},
    {"这一页，今天也很安静。", "想做什么，慢慢点开就好。"},
    {"眼前的秩序，刚刚好。", "不多不少，留住需要的部分。"},
    {"一小步调整，一大点顺心。", "舒服的习惯，是慢慢找到的。"},
    {"熟悉的感觉，又回来了。", "不用重新适应，继续你的节奏。"},
    {"今天的灵感，也许是蓝色。", "也可能是绿的，喜欢就试试。"},
    {"放轻松，按钮不会跑。", "想好再点，随时都来得及。"},
    {"此刻宜：少点杂念。", "把视线交给清楚的画面。"},
    {"给自己，留一点选择。", "今天喜欢的，今天就用。"},
    {"这一屏，有自己的脾气。", "不过，最后还是听你的。"},
    {"设置已就座，等你开场。", "今天的节奏，由你来定。"},
    {"手指歇一歇，也没关系。", "常用功能都在，不会走远。"},
    {"今日小确幸：一切顺手。", "连一个小开关，都刚好合意。"},
    {"画面有留白，心情有余地。", "简单的安排，也能照顾细节。"},
    {"世界的音量，这里调小点。", "安安静静，把眼前看清。"},
    {"给日常，换个轻巧的开头。", "从一次顺手的打开开始。"},
    {"不用满格，也能很舒服。", "适合自己的分寸，慢慢找。"},
    {"今天，也按喜欢的方式来。", "这片空间，容得下你的小偏好。"},
    {"此刻的你，坐在正中间。", "画面围着习惯，习惯跟着你。"},
};

inline constexpr std::size_t Count = sizeof(Quotes) / sizeof(Quotes[0]);
inline constexpr double Interval = 15.0;
static_assert(Count == 80, "The home copy deck contains eighty entries.");

class Rotation {
public:
    explicit Rotation(std::uint64_t seed = UINT64_C(0x7614db57a82c039f)) {
        Seed(seed);
    }

    // The caller supplies a session seed. No clock, files or random devices are
    // read here. Reusing a seed deliberately reproduces the same sequence.
    void Seed(std::uint64_t seed) {
        random_ = seed;
        Shuffle(Count);
        cursor_ = 0;
        activeSeconds_ = 0.0;
        lastNow_ = 0.0;
        changedAt_ = 0.0;
        seenTime_ = false;
        wasActive_ = false;
    }

    const Quote& Current() const { return Quotes[Index()]; }
    std::size_t Index() const { return order_[cursor_]; }
    double ChangedAt() const { return changedAt_; }
    double SecondsUntilRefresh() const { return Interval - activeSeconds_; }

    // Manual refresh resets the visible-time interval and advances once.
    void Refresh(double now) {
        if (!FiniteClock(now)) return;
        const std::size_t previous = Index();
        if (++cursor_ == Count) {
            Shuffle(previous);
            cursor_ = 0;
        }
        activeSeconds_ = 0.0;
        lastNow_ = now;
        changedAt_ = now;
        seenTime_ = true;
    }

    // Only consecutive active frames add elapsed time. An inactive frame pauses
    // the remaining interval; returning never counts time spent on another page.
    // A long active gap advances at most once, avoiding a burst of skipped copy.
    void Update(double now, bool active) {
        if (!FiniteClock(now)) return;
        if (!seenTime_ || now < lastNow_) {
            if (!seenTime_) changedAt_ = now;
            lastNow_ = now;
            seenTime_ = true;
            wasActive_ = active;
            return;
        }
        const double elapsed = now - lastNow_;
        lastNow_ = now;
        if (active && wasActive_) activeSeconds_ += elapsed;
        wasActive_ = active;
        if (active && activeSeconds_ >= Interval) Refresh(now);
    }

private:
    std::uint64_t Next() {
        // SplitMix64 accepts every uint64 seed, including zero.
        std::uint64_t value = (random_ += UINT64_C(0x9e3779b97f4a7c15));
        value = (value ^ (value >> 30)) * UINT64_C(0xbf58476d1ce4e5b9);
        value = (value ^ (value >> 27)) * UINT64_C(0x94d049bb133111eb);
        return value ^ (value >> 31);
    }

    std::size_t Bounded(std::size_t size) {
        const std::uint64_t bound = static_cast<std::uint64_t>(size);
        const std::uint64_t threshold = (std::uint64_t(0) - bound) % bound;
        std::uint64_t value;
        do { value = Next(); } while (value < threshold);
        return static_cast<std::size_t>(value % bound);
    }

    void Shuffle(std::size_t previous) {
        for (std::size_t i = 0; i < Count; ++i) order_[i] = i;
        for (std::size_t i = Count; i > 1; --i) {
            const std::size_t other = Bounded(i);
            const std::size_t value = order_[i - 1];
            order_[i - 1] = order_[other];
            order_[other] = value;
        }
        if (order_[0] == previous) {
            const std::size_t other = 1 + Bounded(Count - 1);
            const std::size_t value = order_[0];
            order_[0] = order_[other];
            order_[other] = value;
        }
    }

    std::array<std::size_t, Count> order_{};
    std::uint64_t random_ = 0;
    std::size_t cursor_ = 0;
    double activeSeconds_ = 0.0;
    double lastNow_ = 0.0;
    double changedAt_ = 0.0;
    bool seenTime_ = false;
    bool wasActive_ = false;
};

} // namespace spatial_quotes
