#pragma once

#include <cstdint>
#ifdef _WIN32
#include <windows.h>
#endif

namespace stats {
struct PeerTraffic {
    bool connected=false;
    std::uint64_t sent=0,received=0;
};

inline PeerTraffic readPeerTraffic(const wchar_t* moduleName,const char* exportName) {
#ifdef _WIN32
    const auto module=GetModuleHandleW(moduleName);
    if(!module)return {};
    using Read=bool (__cdecl *)(std::uint64_t*,std::uint64_t*);
    const auto read=reinterpret_cast<Read>(GetProcAddress(module,exportName));
    if(!read)return {};
    PeerTraffic value;
    value.connected=read(&value.sent,&value.received);
    if(!value.connected)value.sent=value.received=0;
    return value;
#else
    (void)moduleName;(void)exportName;
    return {};
#endif
}
} // namespace stats
