#pragma once

#include <cstdint>
#ifdef _WIN32
#include <windows.h>
#endif

namespace stats {
enum class PeerState {NotLoaded,Unsupported,NoResponse,Connected};
struct PeerTraffic {
    bool connected=false;
    std::uint64_t sent=0,received=0;
    PeerState state=PeerState::NotLoaded;
};

inline PeerTraffic readPeerTraffic(const wchar_t* moduleName,const char* exportName) {
#ifdef _WIN32
    const auto module=GetModuleHandleW(moduleName);
    if(!module)return {};
    using Read=bool (__cdecl *)(std::uint64_t*,std::uint64_t*);
    const auto read=reinterpret_cast<Read>(GetProcAddress(module,exportName));
    if(!read){PeerTraffic value;value.state=PeerState::Unsupported;return value;}
    PeerTraffic value;
    value.connected=read(&value.sent,&value.received);
    value.state=value.connected?PeerState::Connected:PeerState::NoResponse;
    if(!value.connected)value.sent=value.received=0;
    return value;
#else
    (void)moduleName;(void)exportName;
    return {};
#endif
}
} // namespace stats
