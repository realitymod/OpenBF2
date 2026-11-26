// Build-number and mod-name helpers. BuildNrUtil is a namespace, not a static class;
// that distinction affects MSVC symbol names.

#ifndef GAME_UTILITY_BUILDNRUTIL_H     // [INFERRED] guard style
#define GAME_UTILITY_BUILDNRUTIL_H

#include <string>                      // getModName's return

namespace dice {
namespace hfe {

namespace BuildNrUtil {

// . Lazily formats "1.5.3153-802.0" into a file-scope buffer and returns it.
const char* getBuildNrString();

// . Retail `return 0x150C5100;`, PR's 0x150C5101 here - the network protocol version, not the build number.
// GameServer::startServer hands it to io::NetServer::setVersion, and a server reporting a different
// value is rejected by stock clients (see Game/GameServer/GameServer.cpp:345).
int getNetVersionNumber();

} // namespace BuildNrUtil

// Returns the mod directory's leaf name for challenge messages (bf2), while
// connection checks use the full directory path (mods/bf2).
std::string getModName();
std::string getServerName();
std::string getOS();

// dice::hfe::isExpansionPack - 0x814A0A0, 193 B. Adjacent to getModName in the ELF (0x8149DF0),
// same TU. Returns true iff the mod directory (getModName, path stripped) is exactly "xpack",
// case-insensitively. authOk reads it to decide the datablock quirk; see BuildNrUtil.cpp.
bool isExpansionPack();

} // namespace hfe
} // namespace dice

#endif // GAME_UTILITY_BUILDNRUTIL_H
