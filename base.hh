#ifndef __MEM_CACHE_BASE_HH__
#define __MEM_CACHE_BASE_HH__

#include <unordered_map>
#include <cassert>
#include <cstdint>
#include <string>
#include <vector>
#include <memory>

#include "params/WriteAllocator.hh"
#include "mem/cache/adaptive_policy.hh"

#include "base/addr_range.hh"
#include "base/compiler.hh"
#include "base/statistics.hh"
#include "base/trace.hh"
#include "base/types.hh"
#include "debug/Cache.hh"
#include "debug/CachePort.hh"
#include "enums/Clusivity.hh"
#include "mem/cache/cache_blk.hh"
#include "mem/cache/compressors/base.hh"
#include "mem/cache/mshr_queue.hh"
#include "mem/cache/tags/base.hh"
#include "mem/cache/write_queue.hh"
#include "mem/cache/write_queue_entry.hh"
#include "mem/packet.hh"
#include "mem/packet_queue.hh"
#include "mem/qport.hh"
#include "mem/request.hh"
#include "sim/clocked_object.hh"
#include "sim/eventq.hh"
#include "sim/probe/probe.hh"
#include "sim/serialize.hh"
#include "sim/sim_exit.hh"
#include "sim/system.hh"

namespace gem5
{

class WriteAllocator; // ✅ Forward declare to fix Python bindings!
struct BaseCacheParams; // ✅ Forward declare to stop circular includes!

namespace prefetch { class Base; }
class MSHR;
class RequestPort;
class QueueEntry;

class BaseCache : public ClockedObject
{
  public:
    bool adaptiveEnabled() const { return is_adaptive; }
    AdaptivePolicy& getAdaptivePolicy() { return adaptive; }

  protected:
    enum MSHRQueueIndex {
        MSHRQueue_MSHRs,
        MSHRQueue_WriteBuffer
    };

  public:
    enum BlockedCause {
        Blocked_NoMSHRs = MSHRQueue_MSHRs,
        Blocked_NoWBBuffers = MSHRQueue_WriteBuffer,
        Blocked_NoTargets,
        NUM_BLOCKED_CAUSES
    };

    struct DataUpdate {
        Addr addr;
        bool isSecure;
        std::vector<uint64_t> oldData;
        std::vector<uint64_t> newData;

        DataUpdate(Addr _addr, bool is_secure)
            : addr(_addr), isSecure(is_secure) {}
    };

  protected:
    class CacheRequestPort : public QueuedRequestPort {
      public:
        void schedSendEvent(Tick time) {
            DPRINTF(CachePort, "Scheduling send event at %llu\n", time);
            reqQueue.schedSendEvent(time);
        }
      protected:
        CacheRequestPort(const std::string &_name,
                         ReqPacketQueue &_reqQueue,
                         SnoopRespPacketQueue &_snoopRespQueue)
            : QueuedRequestPort(_name, _reqQueue, _snoopRespQueue) {}

        virtual bool isSnooping() const { return true; }
    };

    class CacheReqPacketQueue : public ReqPacketQueue {
      protected:
        BaseCache &cache;
        SnoopRespPacketQueue &snoopRespQueue;

      public:
        CacheReqPacketQueue(BaseCache &cache, RequestPort &port,
                            SnoopRespPacketQueue &snoop_resp_queue,
                            const std::string &label)
            : ReqPacketQueue(cache, port, label),
              cache(cache), snoopRespQueue(snoop_resp_queue) {}

        virtual void sendDeferredPacket() override;

        bool checkConflictingSnoop(const PacketPtr pkt) {
            if (snoopRespQueue.checkConflict(pkt, cache.blkSize)) {
                Tick when = snoopRespQueue.deferredPacketReadyTime();
                schedSendEvent(when);
                return true;
            }
            return false;
        }
    };

    class MemSidePort : public CacheRequestPort {
      private:
        CacheReqPacketQueue _reqQueue;
        SnoopRespPacketQueue _snoopRespQueue;
        BaseCache *cache;

      protected:
        virtual void recvTimingSnoopReq(PacketPtr pkt) override;
        virtual bool recvTimingResp(PacketPtr pkt) override;
        virtual Tick recvAtomicSnoop(PacketPtr pkt) override;
        virtual void recvFunctionalSnoop(PacketPtr pkt) override;

      public:
        MemSidePort(const std::string &_name, BaseCache *_cache,
                    const std::string &_label);
    };

    class CacheResponsePort : public QueuedResponsePort {
      public:
        void setBlocked();
        void clearBlocked();
        bool isBlocked() const { return blocked; }

      protected:
        CacheResponsePort(const std::string &_name, BaseCache& _cache,
                          const std::string &_label);

        BaseCache& cache;
        RespPacketQueue queue;
        bool blocked;
        bool mustSendRetry;

      private:
        void processSendRetry();
        EventFunctionWrapper sendRetryEvent;
    };

    class CpuSidePort : public CacheResponsePort {
      protected:
        virtual bool recvTimingSnoopResp(PacketPtr pkt) override;
        virtual bool recvTimingReq(PacketPtr pkt) override;
        virtual Tick recvAtomic(PacketPtr pkt) override;
        virtual void recvFunctional(PacketPtr pkt) override;
        virtual AddrRangeList getAddrRanges() const override;
        
      public:
        bool tryTiming(PacketPtr pkt);
        CpuSidePort(const std::string &_name, BaseCache& _cache,
                    const std::string &_label);
    };

    CpuSidePort cpuSidePort;
    MemSidePort memSidePort;

  protected:
    MSHRQueue mshrQueue;
    WriteQueue writeBuffer;
    BaseTags *tags;

    compression::Base* compressor;
    prefetch::Base *prefetcher;

    WriteAllocator *writeAllocator;
    bool writebackClean;
    TempCacheBlk *tempBlock;
    PacketPtr tempBlockWriteback;
    EventFunctionWrapper writebackTempBlockAtomicEvent;
    void writebackTempBlockAtomic();

    std::unique_ptr<Packet> pendingDelete;

    const unsigned blkSize;
    const Cycles lookupLatency;
    const Cycles dataLatency;
    const Cycles forwardLatency;
    const Cycles fillLatency;
    const Cycles responseLatency;

    const bool sequentialAccess;
    const int numTarget;
    bool forwardSnoops;

    const enums::Clusivity clusivity;
    const bool isReadOnly;
    bool replaceExpansions;
    bool moveContractions;

    uint8_t blocked;
    uint64_t order;

    MSHR* noTargetMSHR;
    int missCount;
    std::vector<AddrRange> addrRanges;

    

  public:
    System *system;

    class CacheCmdStats : public statistics::Group {
      public:
        BaseCache &cache;
        statistics::Vector hits;
        statistics::Vector misses;
        statistics::Vector hitLatency;
        statistics::Vector missLatency;
        statistics::Formula accesses;
        statistics::Formula missRate;
        statistics::Formula avgMissLatency;
        statistics::Vector mshrHits;
        statistics::Vector mshrMisses;
        statistics::Vector mshrUncacheable;
        statistics::Vector mshrMissLatency;
        statistics::Vector mshrUncacheableLatency;
        statistics::Formula mshrMissRate;
        statistics::Formula avgMshrMissLatency;
        statistics::Formula avgMshrUncacheableLatency;

        CacheCmdStats(BaseCache &c, const std::string &name);
        void regStatsFromParent();
    };

    class CacheStats : public statistics::Group {
      public:
        BaseCache &cache;
        statistics::Formula demandHits;
        statistics::Formula overallHits;
        statistics::Formula demandHitLatency;
        statistics::Formula overallHitLatency;
        statistics::Formula demandMisses;
        statistics::Formula overallMisses;
        statistics::Formula demandMissLatency;
        statistics::Formula overallMissLatency;
        statistics::Formula demandAccesses;
        statistics::Formula overallAccesses;
        statistics::Formula demandMissRate;
        statistics::Formula overallMissRate;
        statistics::Formula demandAvgMissLatency;
        statistics::Formula overallAvgMissLatency;
        statistics::Vector blockedCycles;
        statistics::Vector blockedCauses;
        statistics::Formula avgBlocked;
        statistics::Vector writebacks;
        statistics::Formula demandMshrHits;
        statistics::Formula overallMshrHits;
        statistics::Formula demandMshrMisses;
        statistics::Formula overallMshrMisses;
        statistics::Formula overallMshrUncacheable;
        statistics::Formula demandMshrMissLatency;
        statistics::Formula overallMshrMissLatency;
        statistics::Formula overallMshrUncacheableLatency;
        statistics::Formula demandMshrMissRate;
        statistics::Formula overallMshrMissRate;
        statistics::Formula demandAvgMshrMissLatency;
        statistics::Formula overallAvgMshrMissLatency;
        statistics::Formula overallAvgMshrUncacheableLatency;
        statistics::Scalar replacements;
        statistics::Scalar dataExpansions;
        statistics::Scalar dataContractions;

        std::vector<std::unique_ptr<CacheCmdStats>> cmd;

        inline CacheCmdStats& cmdStats(const PacketPtr pkt) {
            return *cmd[pkt->cmdToIndex()];
        }

        CacheStats(BaseCache &c);
        void regStats() override;
    };

    CacheStats stats;

    ProbePointArg<PacketPtr> *ppHit;
    ProbePointArg<PacketPtr> *ppMiss;
    ProbePointArg<PacketPtr> *ppFill;
    ProbePointArg<DataUpdate> *ppDataUpdate;
    // 🔥 ADAPTIVE POLICY FIELDS
    AdaptivePolicy adaptive;
    bool is_adaptive;

  public:
    BaseCache(const BaseCacheParams &p, unsigned blk_size);
    virtual ~BaseCache();

    virtual void init() override;
    virtual Port &getPort(const std::string &if_name, PortID idx=InvalidPortID) override;

    AddrRangeList getAddrRanges() const { return AddrRangeList(addrRanges.begin(), addrRanges.end()); }

    Addr regenerateBlkAddr(CacheBlk* blk);
    bool inRange(Addr addr) const;

    // ✅ ADD THESE THREE FUNCTIONS HERE:
    bool inCache(Addr addr, bool is_secure) const {
        return tags->findBlock(addr, is_secure) != nullptr;
    }

    bool inMissQueue(Addr addr, bool is_secure) const {
        return mshrQueue.findMatch(addr, is_secure) != nullptr;
    }

    bool hasBeenPrefetched(Addr addr, bool is_secure) const {
        CacheBlk *blk = tags->findBlock(addr, is_secure);
        return (blk != nullptr) && blk->wasPrefetched();
    }
    // ✅ END ADDITIONS

    virtual void handleTimingReqHit(PacketPtr pkt, CacheBlk *blk, Tick request_time);
    
    // ✅ RESTORED: 5 argument signature!
    virtual void handleTimingReqMiss(PacketPtr pkt, MSHR *mshr, CacheBlk *blk, Tick forward_time, Tick request_time);
    
    virtual void recvTimingReq(PacketPtr pkt);
    void handleUncacheableWriteResp(PacketPtr pkt);
    virtual void recvTimingResp(PacketPtr pkt);
    virtual Tick recvAtomic(PacketPtr pkt);
    virtual void functionalAccess(PacketPtr pkt, bool from_cpu_side);

    void updateBlockData(CacheBlk *blk, const PacketPtr cpkt, bool has_old_data);
    void cmpAndSwap(CacheBlk *blk, PacketPtr pkt);

    QueueEntry* getNextQueueEntry();
    bool handleEvictions(std::vector<CacheBlk*> &evict_blks, PacketList &writebacks);
    bool updateCompressionData(CacheBlk *&blk, const uint64_t* data, PacketList &writebacks);

    virtual void satisfyRequest(PacketPtr pkt, CacheBlk *blk, bool deferred_response = false, bool pending_downgrade = false);

    Cycles calculateTagOnlyLatency(const uint32_t delay, const Cycles lookup_lat) const;
    Cycles calculateAccessLatency(const CacheBlk* blk, const uint32_t delay, const Cycles lookup_lat) const;
    virtual bool access(PacketPtr pkt, CacheBlk *&blk, Cycles &lat, PacketList &writebacks);

    void maintainClusivity(bool from_cache, CacheBlk *blk);
    CacheBlk* handleFill(PacketPtr pkt, CacheBlk *blk, PacketList &writebacks, bool allocate);
    CacheBlk* allocateBlock(const PacketPtr pkt, PacketList &writebacks);
    void invalidateBlock(CacheBlk *blk);
    virtual PacketPtr evictBlock(CacheBlk *blk) = 0;
    void evictBlock(CacheBlk *blk, PacketList &writebacks);

    PacketPtr writebackBlk(CacheBlk *blk);
    PacketPtr writecleanBlk(CacheBlk *blk, Request::Flags dest, PacketId id);

    virtual void doWritebacks(PacketList& writebacks, Tick forward_time) = 0;
    virtual void doWritebacksAtomic(PacketList& writebacks) = 0;
    virtual void serviceMSHRTargets(MSHR *mshr, const PacketPtr pkt, CacheBlk *blk) = 0;
    virtual void recvTimingSnoopReq(PacketPtr pkt) = 0;
    virtual void recvTimingSnoopResp(PacketPtr pkt) = 0;
    virtual Cycles handleAtomicReqMiss(PacketPtr pkt, CacheBlk *&blk, PacketList &writebacks) = 0;
    virtual Tick recvAtomicSnoop(PacketPtr pkt) = 0;
    virtual PacketPtr createMissPacket(PacketPtr cpu_pkt, CacheBlk *blk, bool needs_writable, bool is_whole_line_write) const = 0;
    
    // ✅ ADDED: allocateWriteBuffer
    void allocateWriteBuffer(PacketPtr pkt, Tick time);
    MSHR *allocateMissBuffer(PacketPtr pkt, Tick time, bool is_secure = true) {
        MSHR *mshr = mshrQueue.allocate(pkt->getBlockAddr(blkSize), blkSize,
                                        pkt, time, order++, allocOnFill(pkt->cmd));
        if (mshrQueue.isFull()) {
            setBlocked(Blocked_NoMSHRs);
        }
        return mshr;
    }

    void schedMemSideSendEvent(Tick time) {
        memSidePort.schedSendEvent(time);
    }

    bool allocOnFill(MemCmd cmd) const {
        return true;
    }

    void incMissCount(PacketPtr pkt) {
        assert(pkt->req->requestorId() < system->maxRequestors());
        stats.cmdStats(pkt).misses[pkt->req->requestorId()]++;
        pkt->req->incAccessDepth();
        if (missCount) {
            --missCount;
            if (missCount == 0)
                exitSimLoop("A cache reached the maximum miss count");
        }
    }

    void incHitCount(PacketPtr pkt) {
        assert(pkt->req->requestorId() < system->maxRequestors());
        stats.cmdStats(pkt).hits[pkt->req->requestorId()]++;
    }

    void markInService(MSHR *mshr, bool pending_modified_resp) {
        mshrQueue.markInService(mshr, pending_modified_resp);
    }

    void markInService(WriteQueueEntry *entry) {
        writeBuffer.markInService(entry);
    }

    void setBlocked(BlockedCause cause) { blocked |= cause; }
    void clearBlocked(BlockedCause cause) { blocked &= ~cause; }
    bool isBlocked() const { return blocked != 0; }

    void memWriteback() override;
    void memInvalidate() override;
    bool isDirty() const;
    bool coalesce() const;
    void writebackVisitor(CacheBlk &blk);
    void invalidateVisitor(CacheBlk &blk);
    Tick nextQueueReadyTime() const;

    virtual bool sendMSHRQueuePacket(MSHR* mshr);
    bool sendWriteQueuePacket(WriteQueueEntry* wq_entry);

    void serialize(CheckpointOut &cp) const override;
    void unserialize(CheckpointIn &cp) override;
    void regProbePoints() override;

    unsigned getBlockSize() const { return blkSize; }
};

} // namespace gem5

#endif // __MEM_CACHE_BASE_HH__