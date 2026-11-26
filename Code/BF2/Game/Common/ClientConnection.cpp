// Client connection and its stream managers. Source path is attested.

#include "ClientConnection.h"
#include "DataBlockManager.h"
#include "StringManager.h"

#include <cstdlib>                     // rand - initChallengeString
#include "IStreamManager.h"
#include "PlayerActionManager.h"
#include "GameEventManager.h"                    // ditto

#include <cmath>                                 // the ping guard - see bf2IsFinite below
#include <cfloat>
#include <sstream>                               // outputStreamManagerStats' formatter
#include <ios>

#include "../../Core/Debug.h"
#include "../../Core/System.h"                   // getExactTime
#include "../../IO/BitStream.h"
#include "../Objects/IPlayer.h"
#include "GameEvent.h"                          // the events the packet loop drains
#include "GhostManager.h"                       // the constructor is written now
#include "../../IO/Network/NetworkManager.h"    // io::networkManager - transmitMsgs' local-latency arm
#include "../../Physics/LatencyCompensation.h"  // world::LatencyCompensation::getLocalLatency (local only)
#include "../GameServer/ServerSettings.h"       // sv.demoQuality - demoQualityFromSettings' single read

namespace dice {
namespace hfe {

extern System* g_system;                         // ds:0x8AC0D20 - the getExactTime source

// [PLATFORM] glibc `finite()` vs MSVC `_finite()`. One name for the one test the ELF makes.
namespace { inline bool bf2IsFinite(double v) { return v == v && v * 0.0 == 0.0; } }

namespace {

inline DataBlockManager* newDataBlockManager(GameEventManager* events, bool rec)
{ return new DataBlockManager(events, rec); }                                // new(0x3C)
inline StringManager* newStringManager(GameEventManager* events)
{ return new StringManager(events); }                                        // new(0x38)

inline void deleteDataBlockManager(DataBlockManager* m) { delete m; }
inline void deleteStringManager(StringManager* m) { delete m; }

// Shared IStreamManager view for connection-owned concrete stream managers.
inline IStreamManager* asStream(void* manager)
{
    return reinterpret_cast<IStreamManager*>(manager);
}

// ..8084D09 / PE 548A98..548AAA: NetworkManager's two native int fields.
// The second store is a fixed 500, and both images reload the global before it.
void predictionSettingsSetLatency(int ms)
{
    io::networkManager->setInterpolationTime(ms);
    io::networkManager->setExtrapolationTime(500);
}

// Returns the configured demo quality used to scale recording-connection limits.
int demoQualityFromSettings()
{
    return serverSettings->m_demoQuality;                    // ds:0x8AAB7A0 +0xBC
}
inline unsigned int packetStatusManagerMask(const PacketStatus& status)
{
    return status.m_streamManagerMask;
}

} // namespace

// Connection bandwidth tiers. Columns 0/2 are client rate/packet size, 1/3 are
// server rate/packet size, and 4/5 are interpolation/extrapolation milliseconds.
// [CHOSEN, not recovered] Rows 0, 4 and 5 carry Project Reality's server values; retail has
// row 0 = row 1, row 4's packet 512 and row 5 = { 30, 20, 320, 640 }. GameServer overrides row 5's
// packet size with sv.maxPacketSize.
const int g_connectionTypes[6][6] =
{
    { 10, 30, 160, 896, 200, 200 },     // 0
    { 10,  4, 160, 320, 200, 200 },     // 1
    { 30, 10, 160, 350, 200, 200 },     // 2
    { 30, 20, 320, 400, 200, 200 },     // 3
    { 30, 20, 320, 896, 200, 200 },     // 4
    { 30, 30, 320, 896, 200, 200 }      // 5
};

const int* connectionTypeRow(int type) { return g_connectionTypes[type]; }


// A static. handlePacketStatusNotification's auto-ping arm is gated on it; nothing recovered
// writes it, so on this build it is false and the arm never runs. [UNRESOLVED] its writer.
bool ClientConnection::m_useAutoPingAdjust = false;

// Creates connection-owned streams and managers according to live, recording or playback mode.
// Registers action, event and ghost managers in that fixed wire order. Recording mode
// scales packet and ghost capacities from demoQuality.
// [UNRESOLVED] The supported demoQuality range is not established.
ClientConnection::ClientConnection(ConnectionDemoMode demoMode)
    : m_player(0)                                        // +0x008
    , m_connectionState(0)                               // +0x00C
    , m_bReset(true)                                     // +0x010  the only byte seeded true
    , m_dword14(20)                                      // +0x014
    , m_packetSizeOverride(1400)                         // +0x018
    , m_timer1C(0.0f)                                    // +0x01C
    , m_timer20(0.0f)                                    // +0x020
    , m_playerActionManager(0)                           // +0x024
    , m_gameEventManager(0)                              // +0x028
    , m_ghostManager(0)                                  // +0x02C
    , m_dataBlockManager(0)                              // +0x034
    , m_dword78(0)                                       // +0x078
    , m_dword7C(0)                                       // +0x07C
    , m_sentData(false)                                  // +0x080  the back-pressure flag
    , m_bitStream(0)                                     // +0x084
    , m_connectionId(0)                                  // +0x088  setConnectionId overwrites it
    , m_byte8C(false)                                    // +0x08C
    , m_curPingIndex(0)                                  // +0x110
    , m_dword114(0)                                      // +0x114
    , m_pingValue(0.0)                                   // +0x118
    , m_connectionType(static_cast<ConnectionType>(0))   // +0x120  setConnectionType(0) at the tail
    , m_byte146(false)                                   // +0x146
    , m_byte147(false)                                   // +0x147
    , m_lastPingMs(0)                                    // +0x148
    , m_lastUpdateTime(0.0)                              // +0x14C
    , m_nbProcessedPackets(0)                            // +0x154
    , m_dword158(-1)                                     // +0x158
    , m_dword15C(-1)                                     // +0x15C
    , m_nbConnections(0)                                 // +0x160
    , m_byte164(false)                                   // +0x164
    , m_float168(0.0f)                                   // +0x168
    , m_dword16C(0)                                      // +0x16C
    , m_executeServer(false)                             // +0x170  ServerConnection: true
    , m_demoMode(static_cast<int>(demoMode))             // +0x174
    , m_contentValid(false)                              // +0x178
{
    // Ping samples remain uninitialized until collected; averaging starts only after all 32 are written.
    // Recording uses a staging stream 64 times larger than normal traffic.
    // [CHOSEN, not recovered] 0x580, Project Reality's size, fits its 0x540-byte packets; retail 0x400.
    m_bitStream = new io::BitStream(m_demoMode == 1 ? 0x10000u : 0x580u);   // new(0x64)
    // `if (!m_bitStream)` + Debug(6, ..., 81) "failed to create bitstream" - unreachable, see above.

    // 2. the four managers, in the ELF's order.
    m_playerActionManager = new PlayerActionManager();                       // line 85's check
    m_gameEventManager    = new GameEventManager(m_demoMode == 2);           // line 90's check
    m_dataBlockManager    = newDataBlockManager(m_gameEventManager,
                                                m_demoMode == 1);            // line 95's check

    // 3. the GhostManager, and the recording arm is where demoQuality lands.
    if (m_demoMode == 1)
    {
        // [FRONTIER] ds:0x8AAB7A0's demoQuality. The three derived values are recorded in the
        // block above; the two dead intermediate products are not reproduced.
        const int quality = demoQualityFromSettings() - 1;                   // [FRONTIER]
        m_dword14            = quality + 4;
        m_packetSizeOverride = quality + 260;
        m_ghostManager = new GhostManager(static_cast<GhostManagerMode>(1),
            static_cast<int>(std::floor(quality / 9.0f * 215.0f)) + 40);
    }
    else
    {
        m_ghostManager = new GhostManager(
            static_cast<GhostManagerMode>(m_demoMode == 2 ? 2 : 0), 40);
    }
    // `if (!m_ghostManager)` + Debug(6, ..., 126) - unreachable.

    m_stringManager = newStringManager(m_gameEventManager);                  // line 131's check

    // 4. THE order IS frozen - it is what fixes each manager's wire index.
    addStreamManager(m_playerActionManager);                // stream manager id 0
    addStreamManager(m_gameEventManager);                   // stream manager id 1
    addStreamManager(asStream(m_ghostManager));             // stream manager id 2

    m_lastUpdateTime = g_system->getExactTime();
    setConnectionType(static_cast<ConnectionType>(0));
}

// Outputs connection statistics before releasing managers in dependency-sensitive
// order: bit stream, player actions, data blocks, events, ghosts, then strings.
ClientConnection::~ClientConnection()
{
    g_system->getExactTime();                            // result discarded - see above
    outputStreamManagerStats();
    if (m_bitStream)
    {
        delete m_bitStream;                              // ~BitStream + operator delete
        m_bitStream = 0;
    }
    if (m_playerActionManager) { delete m_playerActionManager;   m_playerActionManager = 0; }
    if (m_dataBlockManager)    { deleteDataBlockManager(m_dataBlockManager);       m_dataBlockManager = 0; }
    if (m_gameEventManager)    { delete m_gameEventManager;      m_gameEventManager = 0; }
    if (m_ghostManager)        { delete m_ghostManager;                            m_ghostManager = 0; }
    if (m_stringManager)       { deleteStringManager(m_stringManager);             m_stringManager = 0; }

    // the two vectors and the deque tear themselves down.
}

// Registers up to six stream managers, assigning an index before insertion. Order
// defines packet-status mask bits. Capacity is checked before the null guard, so
// even a null seventh registration raises a fatal diagnostic.
void ClientConnection::addStreamManager(IStreamManager* manager)
{
    const int count = static_cast<int>(m_streamManagers.size());

    if (count > 5)                                       // `>` 5, and it precedes the null test
    {
        Debug(DEBUGTYPE_6, "Game/Common/ClientConnection.cpp", 499)
            << "Too many streamManagers";
        return;                                          // unreachable - type 6 aborts
    }
    if (!manager)
        return;

    // write-once ON THE manager'S side - see IStreamManager.h. A manager registered twice
    // keeps the id it was given first.
    manager->setStreamManagerId(count);                  // vslot +0x14
    m_streamManagers.push_back(manager);

    StreamManagerStats zero;
    zero.m_opaque[0] = zero.m_opaque[1] = zero.m_opaque[2] = zero.m_opaque[3] = 0;
    m_streamManagerStats.push_back(zero);
}

// Returns null for negative IDs or IDs above size(). The original inclusive
// upper bound remains: id == size() performs an out-of-bounds read.
IStreamManager* ClientConnection::getStreamManager(int managerId)
{
    if (managerId < 0)
        return 0;
    if (managerId > static_cast<int>(m_streamManagers.size()))   // `>`, not `>=` - see above
        return 0;
    return m_streamManagers[managerId];                          // reads [size()] when equal
}

// The trivial accessors. Each is its own tiny function in the ELF and each pins one offset.
DataBlockManager* ClientConnection::getDataBlockManager() { return m_dataBlockManager; }
StringManager*    ClientConnection::getStringManager()    { return m_stringManager; }

// Returns the internal challenge buffer. A fixed challenge copies exactly ten bytes
// without adding a terminator; null generates nine lowercase rand() letters and a
// terminator. This class does not seed the random generator.
const char* ClientConnection::initChallengeString(const char* fixed)
{
    if (fixed)
    {
        for (int i = 0; i <= 9; ++i)                 // ten bytes, no terminator - see above
            m_challengeString[i] = fixed[i];
    }
    else
    {
        for (int j = 0; j <= 9; ++j)
            m_challengeString[j] = static_cast<char>(rand() % 26 + 97);
        m_challengeString[9] = 0;                    // the tenth byte becomes the terminator
    }
    return m_challengeString;
}
world::IPlayer*   ClientConnection::getPlayer()           { return m_player; }
int  ClientConnection::getDemoMode() const                { return m_demoMode; }
bool ClientConnection::getContentValid() const            { return m_contentValid; }
void ClientConnection::setContentValid(bool valid)        { m_contentValid = valid; }
ConnectionType ClientConnection::getConnectionType() const
    { return static_cast<ConnectionType>(m_connectionType); }
void ClientConnection::setServerUpdateRate(int rate)      { m_profile[1] = rate; }         // +0x128
int  ClientConnection::getServerUpdateRate() const        { return m_profile[1]; }         // +0x128
void ClientConnection::setServerPacketSize(int size)      { m_profile[3] = size; }         // +0x130
int  ClientConnection::getServerPacketSize() const        { return m_profile[3]; }         // +0x130

// The base connection never records demos; ServerConnection overrides this based on demo mode.
bool ClientConnection::getIsRecordingConnection() const { return false; }

// Stores player and forwards it to each available action/ghost manager.
// Other managers are not notified.
void ClientConnection::setPlayer(world::IPlayer* player)
{
    m_player = player;
    if (m_playerActionManager)
        m_playerActionManager->setPlayer(player);
    if (m_ghostManager)
        m_ghostManager->setPlayer(player);                               // tail-jump
}

// Returns -1 without a ghost manager, distinguishing absence from an empty manager.
int ClientConnection::debugGetNumActiveGhosts()
{
    if (!m_ghostManager)
        return -1;                                       // a sentinel, not zero
    return static_cast<int>(m_ghostManager->m_activeGhostVec.size());
}

void ClientConnection::debugShowActiveGhosts()
{
    if (m_ghostManager)
        m_ghostManager->debugShowActiveGhosts();         //  tail-jump -> 0x8099030
                                                         // [FRONTIER] - the body is unwritten
}

// Stores the connection ID and forwards it to every stream manager for packet generation.
void ClientConnection::setConnectionId(int connectionId)
{
    m_connectionId = connectionId;                       //   [edi+88h]
    for (unsigned int i = 0; i < m_streamManagers.size(); ++i)
        m_streamManagers[i]->setConnectionId(connectionId);                // vslot +0x24
}

// Clears outgoing data, selects write mode, and sets the packet limit in bytes.
void ClientConnection::newPacket(int sizeBytes)
{
    m_bitStream->clear();                                // > m_highWaterBits = 0
    m_bitStream->setWriteMode();                         //   [eax+34h] = 0

    // Set capacity in bits while leaving the cleared high-water mark at zero;
    // an empty packet must remain empty for the send loop.
    const unsigned int limitBits = static_cast<unsigned int>(sizeBytes) * 8;
    if (m_bitStream->m_bitLimit != limitBits)            // the ELF's redundant compare-then-store
        m_bitStream->m_bitLimit = limitBits;
}

// Builds a packet from registered stream managers, reserving one bit for each remaining manager.
// Back-pressure from a pending send returns success without sending. Stream or manager errors return false.
// Records delivery state only when a manager contributes; unused opaque status fields remain uninitialized.
bool ClientConnection::transmitMsgs()
{
    if (m_sentData)                              // +0x80 - the send ring was full last frame
        return true;

    // local-player latency - dead on a dedicated server (the player is remote). See the note above.
    if (m_player && !m_player->getIsRemote())    // +0x08, vtable +0x5C
    {
        const int interpTime = io::networkManager->getInterpolationTime();           // +0x50, default 100
        const float latency = world::LatencyCompensation::getLocalLatency(m_player); // local only
        m_playerActionManager->m_playerInterpolationTime =                           // +0x24 -> +0x80
            static_cast<int>(latency * 1000.0f) + interpTime;                        // seconds -> ms, truncated
    }

    // The packet-status record, built on the stack. The ELF sets exactly these fields; the stream
    // managers fill m_eventCount/m_blockIndex/m_streamManagerMask, and +0x20/+0x24/+0x2C stay as stack
    // residue. Set by raw dword offset to match the ELF's partial initialisation byte for byte.
    PacketStatus status;
    unsigned int* const s = reinterpret_cast<unsigned int*>(&status);
    s[3] = 0;                                    // +0x0C
    s[4] = 0;                                    // +0x10 m_eventCount
    s[5] = 0;                                    // +0x14 m_blockIndex
    s[6] = 0;                                    // +0x18
    s[7] = 0;                                    // +0x1C m_streamManagerMask - the "recorded?" flag

    newPacket(m_packetSizeOverride);             // +0x18

    s[0]  = static_cast<unsigned int>(m_dword78);   // +0x00 the packet counter
    s[10] = static_cast<unsigned int>(m_dword78);   // +0x28 the packet counter (again, as the ELF)

    m_dataBlockManager->update();                // +0x34 - announce pending data blocks
    m_stringManager->update();                   // +0x30 - announce pending strings

    const unsigned int count = static_cast<unsigned int>(m_streamManagers.size());   // +0x60
    const unsigned int reserved = m_bitStream->m_bitLimit - count;   // original limit, less the reserve
    for (unsigned int i = 0; i < count; ++i)
    {
        m_bitStream->m_bitLimit = reserved + i;                       // +0x08 - the per-manager budget
        IStreamManager* const mgr = m_streamManagers[i];
        const unsigned int bits = mgr->transmit(m_bitStream, status, 0);    // vtable +0x20
        if (mgr->getError())                                               // vtable +0x2C
            return false;                        // a manager errored -> the caller disconnects
        const unsigned int bytes = bits >> 3;
        m_streamManagerStats[mgr->getStreamManagerId()].m_opaque[0]  = bytes;   // +0x00 bytes this frame
        m_streamManagerStats[mgr->getStreamManagerId()].m_opaque[1] += bytes;   // +0x04 accumulated
    }

    if (m_bitStream->hasError())                 // +0x1C - a write overran; drop the packet
    {
        m_bitStream->clearError();               // +0x20 = 0, +0x1C = false
        return false;
    }

    if (status.m_streamManagerMask)              // +0x1C - a manager put something in this packet
    {
        // stamp the send time (a float, promoted into the 8-byte slot) and file the record.
        *reinterpret_cast<double*>(&status.m_opaque00[4]) = g_system->getExactTime();   // +0x04
        m_packetStatuses.push_back(status);      // +0x38, the delivery-tracking deque
        ++m_dword78;                             // ++packet counter
    }
    else
    {
        newPacket(m_packetSizeOverride);         // nothing was queued - start a fresh packet
    }
    return true;
}

// Clears every stream-manager statistics record, rereading the vector size each iteration.
void ClientConnection::resetStreamManagerStats()
{
    for (unsigned int i = 0; i < m_streamManagerStats.size(); ++i)
    {
        m_streamManagerStats[i].m_opaque[0] = 0;
        m_streamManagerStats[i].m_opaque[1] = 0;
        m_streamManagerStats[i].m_opaque[2] = 0;
        m_streamManagerStats[i].m_opaque[3] = 0;
    }
}

// Copies the selected bandwidth tier into this connection for local overrides.
// Requires type 0–5; table indexing is unchecked.
void ClientConnection::setConnectionType(ConnectionType type)
{
    m_connectionType = static_cast<int>(type);
    const int* const row = connectionTypeRow(static_cast<int>(type));   // ds:0x888C1A0, extracted
    for (int i = 0; i < 6; ++i)
        m_profile[i] = row[i];
}

// Processes actions, queued events and ghosts in wire order, retaining any manager failure.
// A null stream only drains and releases queued events; an empty drain does not increment
// the processed-packet counter. Event execution follows this connection's client/server direction.
bool ClientConnection::processReceivedPacket(io::BitStream* stream)
{
    bool ok          = true;
    bool ranAnEvent  = false;

    if (stream)
    {
        // Both managers run: assign the first result, then combine the second.
        ok = m_playerActionManager->processReceivedPacket(stream);                // vslot +0x18
        if (!m_gameEventManager->processReceivedPacket(stream))                   // vslot +0x18
            ok = false;
    }

    for (;;)
    {
        GameEvent* const event = m_gameEventManager->getNextRcvdEvent();         // vslot +0x40
        if (!event)
            break;

        event->logInfo(false, m_connectionId);           // vslot - the connection id, not a ping
        if (m_executeServer)
            event->executeServer(m_connectionId);
        else
            event->executeClient();
        ranAnEvent = true;
        event->release();
    }

    if (stream)
    {
        if (!asStream(m_ghostManager)->processReceivedPacket(stream))     // vslot +0x18
            ok = false;
    }
    else if (!ranAnEvent)
    {
        return ok;                                       // the counter is NOT bumped
    }

    ++m_nbProcessedPackets;
    return ok;
}


// Retire the oldest packet status after notifying its selected stream managers.
// Return false for an empty status queue or a non-finite smoothed ping.
// Delivered packets contribute samples in seconds; smoothing retains falling
// samples and publishes the remote player's ping in milliseconds.
bool ClientConnection::handlePacketStatusNotification(bool& delivered)
{
    if (m_packetStatuses.empty())
    {
        // playback only: a demo being played back has no real latency, so the prediction settings
        // are pinned on the real NetworkManager.
        if (m_demoMode == 2)
            predictionSettingsSetLatency(0);                 // ds:0x8AC3D1C +0x50 = 0, +0x54 = 500
        return false;                                        // the only ordinary false
    }

    PacketStatus status = m_packetStatuses.front();          // by value, before the walk

    for (unsigned int i = 0; i < m_streamManagers.size(); ++i)
    {
        const int id = m_streamManagers[i]->getStreamManagerId();                 // vslot +0x10
        if ((1 << id) & packetStatusManagerMask(status))     // PacketStatus +0x1C
            m_streamManagers[i]->handlePacketStatus(status, delivered);           // vslot +0x1C
    }

    if (delivered)
    {
        m_pingSamples[m_curPingIndex++] = m_lastPingMs / 1000.0f;   // ms -> seconds

        if (m_curPingIndex > 31)
        {
            // the falling-edge filter. See the warning above.
            float prev = 1.5f;                               // the seed, in seconds
            float sum  = 0.0f;
            int   n    = 0;
            for (int i = 0; i < 32; ++i)
            {
                const float sample = m_pingSamples[i];
                if (prev > sample)                           // only falling samples count
                {
                    sum += sample;
                    ++n;
                }
                prev = sample;
            }
            if (n > 0)
                m_pingValue = (sum / n + m_pingValue) * 0.5;

            // [PLATFORM] the ELF calls glibc's `finite()`; MSVC spells the same predicate
            // `_finite()`. Same test, different name - not a behaviour change.
            if (!bf2IsFinite(m_pingValue))                   // a NaN disconnects the connection
                return false;

            ++m_dword114;
            m_curPingIndex = 0;

            // [FRONTIER] Local auto-ping adjustment requires m_useAutoPingAdjust. It averages
            // action-buffer depth every eight passes before updating latency.

            if (m_player && m_player->getIsRemote())
                m_player->setPing(static_cast<int>(1000.0 * m_pingValue));   // back to ms
        }
    }

    m_packetStatuses.pop_front();                            // after every manager was told
    return true;
}

// Formats stream statistics without emitting the text; output was compiled out
// of the original. Retain formatting work, including the destructor's call.
void ClientConnection::outputStreamManagerStats()
{
    for (unsigned int i = 0; i < m_streamManagerStats.size(); ++i)
    {
        // the first pass - a loop whose body did not survive the release build
    }
    for (unsigned int i = 0; i < m_streamManagerStats.size(); ++i)
    {
        std::ostringstream text;
        text.precision(2);
        text << std::fixed
             << static_cast<double>(m_streamManagerStats[i].m_opaque[1]) * 0.00000095367432;   // [+4] -> MiB
        // discarded, as shipped
    }
}

} // namespace hfe
} // namespace dice
