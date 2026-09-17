#pragma once
#include <algorithm>
#include <array>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>
#include <limits>

namespace lab {
using Bytes = std::vector<std::uint8_t>;
inline std::optional<std::vector<Bytes>> encode(int mtu, int kind, std::uint64_t id, const Bytes& payload) {
    if (mtu < 23 || kind < 1 || kind > 5 || id == 0 || id > 0xffffffffULL || payload.empty() || payload.size() > 1024) return {};
    const auto chunk = static_cast<std::size_t>(mtu - 19);
    const auto count = (payload.size() - 1) / chunk + 1;
    std::vector<Bytes> frames;
    frames.reserve(count);
    for (std::size_t i = 0; i < count; ++i) {
        auto offset = i * chunk, length = std::min(chunk, payload.size() - offset);
        Bytes f(16 + length);
        auto put = [&](int pos, std::uint64_t n, int size) { for (int j=0;j<size;++j) f[pos+j] = static_cast<std::uint8_t>(n >> (8*j)); };
        f[0]=0x46; f[1]=0x53; f[2]=1; f[3]=static_cast<std::uint8_t>(kind);
        put(4,id,4); put(8,i,2); put(10,count,2); put(12,payload.size(),2); put(14,offset,2);
        std::copy_n(payload.begin()+offset,length,f.begin()+16);
        frames.push_back(std::move(f));
    }
    return frames;
}
struct Result { std::string status; Bytes payload{}; };
class Reassembler {
    struct Slot {
        bool used = false;
        int kind = 0, count = 0, total = 0, received = 0;
        std::uint64_t id = 0;
        std::int64_t start = 0;
        std::array<std::uint8_t,1024> data{};
        std::array<std::uint16_t,256> offsets{}, lengths{};
        void clear() { used=false; lengths.fill(0); received=0; }
    };
    std::array<Slot,2> slots{};
    std::int64_t lastNow = 0;
    std::uint64_t generation_ = 0;
    bool advance(std::int64_t now) {
        if (now < lastNow) return false;
        lastNow=now;
        for (auto& s:slots) if (s.used && now-s.start >= 2000) s.clear();
        return true;
    }
public:
    int active() const { return static_cast<int>(std::count_if(slots.begin(),slots.end(),[](const Slot& s){return s.used;})); }
    std::uint64_t generation() const { return generation_; }
    Result tick(std::int64_t now) { return {advance(now)?"TICK":"CLOCK"}; }
    Result disconnect() {
        if (generation_ == std::numeric_limits<std::uint64_t>::max()) throw std::overflow_error("generation exhausted");
        for (auto& s:slots) s.clear();
        ++generation_; return {"DISCONNECTED"};
    }
    Result receive(std::int64_t now, std::uint64_t session, int characteristicKind, const Bytes& f) {
        if (!advance(now)) return {"CLOCK"};
        if (session != generation_) return {"STALE"};
        if (f.size()<17 || f.size()>1040) return {"INVALID"};
        auto u = [&](int p,int size) { std::uint64_t n=0; for(int j=0;j<size;++j) n |= std::uint64_t(f[p+j]) << (8*j); return n; };
        int kind=int(u(3,1)), index=int(u(8,2)), count=int(u(10,2)), total=int(u(12,2)), offset=int(u(14,2)), length=int(f.size()-16);
        auto id=u(4,4);
        if (u(0,2)!=0x5346 || u(2,1)!=1 || kind<1 || kind>5 || kind!=characteristicKind || id==0 ||
            total<1 || total>1024 || count<1 || count>std::min(256,total) || index>=count || offset+length>total) return {"INVALID"};
        Slot* s=nullptr;
        for(auto& item:slots) if(item.used && item.kind==kind && item.id==id) { s=&item; break; }
        if (!s) {
            for(auto& item:slots) if(!item.used) { s=&item; break; }
            if(!s) return {"CAPACITY"};
            s->clear(); s->used=true; s->kind=kind; s->id=id; s->start=now; s->count=count; s->total=total;
        }
        auto conflict=[&]() -> Result { s->clear(); return {"CONFLICT"}; };
        if(s->count!=count || s->total!=total) return conflict();
        if(s->lengths[index]) {
            if(s->offsets[index]!=offset || s->lengths[index]!=length || !std::equal(f.begin()+16,f.end(),s->data.begin()+offset)) return conflict();
            return {"DUPLICATE"};
        }
        for(int i=0;i<count;++i) if(s->lengths[i] && offset<s->offsets[i]+s->lengths[i] && s->offsets[i]<offset+length) return conflict();
        std::copy(f.begin()+16,f.end(),s->data.begin()+offset);
        s->offsets[index]=static_cast<std::uint16_t>(offset); s->lengths[index]=static_cast<std::uint16_t>(length); ++s->received;
        if(s->received!=count) return {"PENDING"};
        int end=0;
        for(int i=0;i<count;++i) { if(s->offsets[i]!=end) return conflict(); end+=s->lengths[i]; }
        if(end!=total) return conflict();
        Bytes payload(s->data.begin(),s->data.begin()+total); s->clear();
        return {"COMPLETE",std::move(payload)};
    }
};
}
