#ifndef __MEM_CACHE_TAGS_BASE_SET_ASSOC_HH__
#define __MEM_CACHE_TAGS_BASE_SET_ASSOC_HH__

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "base/logging.hh"
#include "base/types.hh"
#include "mem/cache/base.hh"
#include "mem/cache/cache_blk.hh"
#include "mem/cache/replacement_policies/base.hh"
#include "mem/cache/replacement_policies/replaceable_entry.hh"
#include "mem/cache/tags/base.hh"
#include "mem/cache/tags/indexing_policies/base.hh"
#include "mem/packet.hh"
#include "params/BaseSetAssoc.hh"

// ✅ ADAPTIVE POLICY INCLUDED
#include "mem/cache/adaptive_policy.hh"

namespace gem5
{

class BaseSetAssoc : public BaseTags
{
  protected:
    unsigned allocAssoc;
    std::vector<CacheBlk> blks;
    const bool sequentialAccess;

    replacement_policy::Base *replacementPolicy;

    // ✅ ADAPTIVE TRACKER INSTANCE
    AdaptivePolicy adaptive;
    bool use_adaptive = true; // ✅ Enable adaptive policy by default

  public:
    typedef BaseSetAssocParams Params;

    BaseSetAssoc(const Params &p);

    virtual ~BaseSetAssoc() {};

    void tagsInit() override;

    void invalidate(CacheBlk *blk) override;

    CacheBlk* accessBlock(const PacketPtr pkt, Cycles &lat) override
    {
        CacheBlk *blk = findBlock(pkt->getAddr(), pkt->isSecure());

        stats.tagAccesses += allocAssoc;

        if (sequentialAccess) {
            if (blk != nullptr) {
                stats.dataAccesses += 1;
            }
        } else {
            stats.dataAccesses += allocAssoc;
        }

        if (blk != nullptr) {
            blk->increaseRefCount();

            replacementPolicy->touch(blk->replacementData, pkt);

            // ✅ mark access for adaptive tracking
            adaptive.update(pkt->getAddr(), true, false);
        } else {
            // ✅ miss tracking
            adaptive.update(pkt->getAddr(), false, true);
        }

        lat = lookupLatency;

        return blk;
    }

    CacheBlk* findVictim(Addr addr, const bool is_secure,
                         const std::size_t size,
                         std::vector<CacheBlk*>& evict_blks) override
    {
        // Use the standard ReplacementCandidates vector type
        const std::vector<ReplaceableEntry*> entries =
            indexingPolicy->getPossibleEntries(addr);

        CacheBlk* victim = nullptr;

        // 🔥 ADAPTIVE DECISION
        // ✅ Fixed: We only check `use_adaptive` here, no need for the `cache` pointer
        if (use_adaptive) {

            if (adaptive.getPattern() == AdaptivePolicy::CONFLICT_HEAVY) {
                victim = static_cast<CacheBlk*>(entries[0]);

                for (auto entry : entries) {
                    CacheBlk* blk = static_cast<CacheBlk*>(entry);

                    // Prefer block with lowest reference count
                    if (blk->getRefCount() < victim->getRefCount()) {
                        victim = blk;
                    }
                }
            } else {
                // fallback to normal policy
                victim = static_cast<CacheBlk*>(
                    replacementPolicy->getVictim(entries));
            }

        } else {
            victim = static_cast<CacheBlk*>(
                replacementPolicy->getVictim(entries));
        }

        evict_blks.push_back(victim);

        return victim;
    }

    void insertBlock(const PacketPtr pkt, CacheBlk *blk) override
    {
        BaseTags::insertBlock(pkt, blk);

        stats.tagsInUse++;

        replacementPolicy->reset(blk->replacementData, pkt);
    }

    void moveBlock(CacheBlk *src_blk, CacheBlk *dest_blk) override;

    virtual void setWayAllocationMax(int ways) override
    {
        fatal_if(ways < 1, "Allocation limit must be greater than zero");
        allocAssoc = ways;
    }

    virtual int getWayAllocationMax() const override
    {
        return allocAssoc;
    }

    Addr regenerateBlkAddr(const CacheBlk* blk) const override
    {
        return indexingPolicy->regenerateAddr(blk->getTag(), blk);
    }

    void forEachBlk(std::function<void(CacheBlk &)> visitor) override {
        for (CacheBlk& blk : blks) {
            visitor(blk);
        }
    }

    bool anyBlk(std::function<bool(CacheBlk &)> visitor) override {
        for (CacheBlk& blk : blks) {
            if (visitor(blk)) {
                return true;
            }
        }
        return false;
    }
};

} // namespace gem5

#endif // __MEM_CACHE_TAGS_BASE_SET_ASSOC_HH__