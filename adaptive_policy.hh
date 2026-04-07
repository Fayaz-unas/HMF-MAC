#ifndef __MEM_CACHE_ADAPTIVE_POLICY_HH__
#define __MEM_CACHE_ADAPTIVE_POLICY_HH__

#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <vector>

namespace gem5 {

class AdaptivePolicy {
public:
    enum Pattern { STREAMING, STRIDED, CONFLICT_HEAVY, RANDOM };

    AdaptivePolicy() : last_addr(0), last_stride(0), stride_streak(0), conflict_misses(0), current(RANDOM) {}

    Pattern update(uint64_t addr, bool hit, bool is_conflict) {
        int64_t stride = (int64_t)addr - (int64_t)last_addr;
        if (std::abs(stride - last_stride) <= 16) stride_streak++;
        else stride_streak = 1;
        last_stride = stride;
        last_addr = addr;

        if (!hit && is_conflict) conflict_misses++;
        else if (conflict_misses > 0) conflict_misses--;

        if (stride_streak > 4) {
            if (std::abs(stride) <= 64) current = STREAMING;
            else current = STRIDED;
        } else if (conflict_misses > 8) {
            current = CONFLICT_HEAVY;
        } else {
            current = RANDOM;
        }
        return current;
    }

    Pattern getPattern() const { return current; }

    // 🔥 TEMPLATE FUNCTION: Eliminates all namespace and include errors!
    template <typename EntryType>
    EntryType* selectVictim(const std::vector<EntryType*>& entries) {
        if (entries.empty()) return nullptr;
        switch (current) {
            case STREAMING: return entries.back();
            case STRIDED: return entries.back();
            case CONFLICT_HEAVY: return entries[std::rand() % entries.size()];
            case RANDOM:
            default: return entries[std::rand() % entries.size()];
        }
    }

private:
    uint64_t last_addr;
    int64_t last_stride;
    int stride_streak;
    int conflict_misses;
    Pattern current;
};

} // namespace gem5
#endif