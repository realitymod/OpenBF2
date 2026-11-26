// Server protocol implementation. [INFERRED] Header placement.
// Preserve all 36 interface slots without a virtual destructor; release owns teardown. Native STL layouts determine size.

#ifndef IO_NETWORK_PROTOCOLLAYER_NETSERVER_H         // [INFERRED] guard style
#define IO_NETWORK_PROTOCOLLAYER_NETSERVER_H

#include <string>
#include <deque>
#include <list>
#include <map>

#include "../../../Core/Mutex.h"                     // three by-value Mutexes, sizeof 4 each
#include "../PacketFilter.h"                         // by-value member at +0x4F0
#include "Packet.h"                                 // io::Packet - by-value member at +0x448
#include "PResult.h"                                // storeConnectionEvent's reason

namespace dice {
namespace hfe {

class Thread;

namespace io {

class BitStream;
class UDPSocket;                                     // +0x464 / +0x468 - the two listen sockets
class SWConnection;                                  // the per-connection sliding-window object
// Local connection telemetry, not a wire record. [INFERRED] Field names.
struct ConnectionStatus
{
    float bytesSentPerSecond;              // +0x00
    float bytesReceivedPerSecond;          // +0x04
    float packetsSentPerSecond;            // +0x08
    float packetsReceivedPerSecond;        // +0x0C
    float packetsLostSendPerSecond;        // +0x10 - reversed relative to connstatsentry
    float packetsLostReceivePerSecond;     // +0x14
    unsigned int ping;                     // +0x18
};
static_assert(sizeof(ConnectionStatus) == 0x1C, "ConnectionStatus telemetry layout");

// Server protocol interface. Preserve virtual order and marked [CHOSEN] return contracts. init returns zero on success.
class INetServer
{
public:
    // The IBase quartet
    virtual int   addRef()                                                        = 0;  // +0x00 - returns incremented count
    virtual int   getRef()                                                        = 0;  // +0x04
    virtual int   release()                                                       = 0;  // +0x08
    virtual void* queryInterface(unsigned int) const                              = 0;  // +0x0C

    // lifecycle
    virtual int   init(const std::string& address, unsigned int port, bool)        = 0;  // +0x10
    virtual void  shutDown()                                                      = 0;  // +0x14

    // Packet flow
    virtual void  bufferPacket(int, unsigned char*, unsigned int, unsigned int)   = 0;  // +0x18
    // returns int - from GameServer::processGameStateAndSendPackets, which
    // branches on it three ways: 0 = queued, 15 = the send buffer is full (the connection's
    // m_sentData latches and it skips building a packet next frame), anything else = disconnect
    // with reason 28. [UNRESOLVED] the other values.
    virtual int   bufferPacket(int, BitStream*, unsigned int)                     = 0;  // +0x1C
    virtual void  send()                                                          = 0;  // +0x20
    virtual void  send(int)                                                       = 0;  // +0x24
    virtual void  receive(int*, unsigned char*, unsigned int*)                    = 0;  // +0x28
    virtual void  update(int*)                                                    = 0;  // +0x2C

    // Timeout / version
    virtual void         setConnectionTimeOut(unsigned int)                       = 0;  // +0x30
    virtual unsigned int getConnectionTimeOut() const                             = 0;  // +0x34
    virtual void         setVersion(unsigned int)                                 = 0;  // +0x38
    virtual unsigned int getVersion() const                                       = 0;  // +0x3C

    // Sets the connection password, returning false when the supplied value is refused.
    virtual bool setPassword(const std::string&)                                  = 0;  // +0x40
    // Both materialise bool in AL in the PE (0x662B10 / 0x662AD0); a rejected
    // overlength password leaves the password, slot counts, and timer unchanged.
    virtual bool setReconnectPassword(const std::string&, int)                    = 0;  // +0x44
    virtual bool setReservedPassword(const std::string&, int)                     = 0;  // +0x48
    virtual const std::string& getPassword() const                                = 0;  // +0x4C

    // Connections
    virtual void         setMaxNumberOfConnections(unsigned int)                  = 0;  // +0x50
    virtual unsigned int getMaxNumberOfConnections() const                        = 0;  // +0x54
    virtual bool         getPacketDeliveryStatus(int, bool&)                      = 0;  // +0x58
    virtual unsigned int closeConnection(int, unsigned int, const std::string&)   = 0;  // +0x5C - 0 queued, 9 absent/already queued
    virtual unsigned int getNumberOfConnections() const                           = 0;  // +0x60

    virtual int          getTime() const                                          = 0;  // +0x64
    virtual unsigned int getMaxPacketSize() const                                 = 0;  // +0x68
    virtual float        getPacketDropRatio() const                               = 0;  // +0x6C
    virtual void         setPacketDropRatio(float)                                = 0;  // +0x70

    virtual void setStatsGatheringEnabled(int, bool)                              = 0;  // +0x74
    virtual bool getStatsGatheringEnabled(int)                                    = 0;  // +0x78
    virtual bool getConnectionStatus(int, ConnectionStatus&)                      = 0;  // +0x7C
    virtual bool getPing(int, int*) const                                         = 0;  // +0x80

    virtual bool        getConnectionIp(int, std::string&)                        = 0;  // +0x84
    virtual unsigned int getChallengeIp(int) const                                = 0;  // +0x88  returns an IP (unsigned int), not std::string - see the .cpp
    virtual std::string getServerAddress() const                                  = 0;  // +0x8C

protected:
    // NON-virtual and protected, deliberately. See the header block: the vtable proves this
    // interface has no virtual destructor, and adding one would shift all 36 slots above.
    ~INetServer() {}
};

// Basic wire header: four type bits followed by eight connection-id bits. Id 255 is the no-connection sentinel.
bool readBasicHeader(BitStream* stream, unsigned char& connectionId, unsigned int& type);
bool writeBasicHeader(BitStream* stream, unsigned char connectionId, unsigned int type);

// Extended wire header: six-bit local counter, six-bit remote counter and a 32-bit acknowledgement field.
bool writeExtendedHeader(BitStream* stream, unsigned int localCounter,
                         unsigned int remoteCounter, unsigned int delayScaleFactor);
bool readExtendedHeader(BitStream* stream, unsigned int& localCounter,
                        unsigned int& remoteCounter, unsigned int& delayScaleFactor);

class NetServer : public INetServer
{
public:
    NetServer();
    virtual ~NetServer();                                                               // +0x90/+0x94  written - empty; the members do it all

    // INetServer, in slot order
    virtual int   addRef();
    virtual int   getRef();
    virtual int   release();
    virtual void* queryInterface(unsigned int) const;
    virtual int   init(const std::string&, unsigned int, bool);
    virtual void  shutDown();
    virtual void  bufferPacket(int, unsigned char*, unsigned int, unsigned int);
    virtual int   bufferPacket(int, BitStream*, unsigned int);
    virtual void  send();
    virtual void  send(int);
    virtual void  receive(int*, unsigned char*, unsigned int*);
    virtual void  update(int*);
    virtual void         setConnectionTimeOut(unsigned int);
    virtual unsigned int getConnectionTimeOut() const;
    virtual void         setVersion(unsigned int);
    virtual unsigned int getVersion() const;
    virtual bool setPassword(const std::string&);
    virtual bool setReconnectPassword(const std::string&, int);
    virtual bool setReservedPassword(const std::string&, int);
    virtual const std::string& getPassword() const;
    virtual void         setMaxNumberOfConnections(unsigned int);
    virtual unsigned int getMaxNumberOfConnections() const;
    virtual bool         getPacketDeliveryStatus(int, bool&);
    virtual unsigned int closeConnection(int, unsigned int, const std::string&);
    virtual unsigned int getNumberOfConnections() const;
    virtual int          getTime() const;
    virtual unsigned int getMaxPacketSize() const;
    virtual float        getPacketDropRatio() const;
    virtual void         setPacketDropRatio(float);
    virtual void setStatsGatheringEnabled(int, bool);
    virtual bool getStatsGatheringEnabled(int);
    virtual bool getConnectionStatus(int, ConnectionStatus&);
    virtual bool getPing(int, int*) const;
    virtual bool        getConnectionIp(int, std::string&);
    virtual unsigned int getChallengeIp(int) const;
    virtual std::string getServerAddress() const;

    // Non-virtual BitStream receive overload; the byte-buffer interface overload is virtual.
    int  receive(int*, BitStream*);
                                                                                       // returns int

    SWConnection* allocConnection();
    void freeConnection(SWConnection*);
    // returns int, not void - . NetServerThread::run drains it with
    // `do { r = _update(&s); } while (r != 8 && r != 14);`, so the result is not only materialised
    // but branched on. The mangled name encodes no return type, which is why the earlier `void` was
    // plausible; the caller settles it. 8 and 14 are [UNRESOLVED] enumerators.
    int  _update(int*);
    // [CHOSEN] Returns elapsed milliseconds using the shared timing expression.
    unsigned int elapsedMsSinceInit() const;

    int  _updateConnections(int*);
    void setName(std::string);
    std::string getName() const;                         // 0x8475830 / PE 0x662A60 - hidden return object, not a reference
    unsigned int _closeConnection(int, unsigned int, const std::string&);               // 0x8476570  written - 0 or 9

    unsigned int sendPingRequest(int);
    unsigned int sendPingResponse(int, unsigned int);                                   // 0x8475B40  returns unsigned int (handlePingRequest branches on it); the old `void` was wrong
    void handlePingRequest(int);
    void handlePingResponse(int);
    void pingConfirmation(SWConnection*, unsigned int);

    // return types both senders materialise a result code - 0 sent,
    // 5 no such connection, 6 the socket refused, 9 the packet overflowed the stream.
    unsigned int sendConnectAccept(int);
    unsigned int sendConnectDenied(unsigned int);
    // Handles admission and returns zero on acceptance or a protocol denial code.
    unsigned int handleConnectRequest();
    // Accepts the connection acknowledgement. Returns 0 on success or 11 for an absent or ineligible connection.
    unsigned int handleConnectAcceptAck(int);
    void handleDisconnect(int);
    void sendDisconnect(unsigned int, const std::string&);
    // Queues a connection event with its protocol result code.
    void storeConnectionEvent(int connectionId, PResult reason);

    void sendServerInfo();                                                              // written - empty as shipped
    void handleServerInfoRequest();
    void handleDataPacket(int, unsigned int);
    void sendRawPacket(const char*, unsigned short, const char*, int);

    bool checkPassword(std::string);
    bool checkReservedPassword(std::string);
    bool checkReconnectPassword(char*);
    // Returns zero for an accepted version, 23 for an older client, or 24 for a newer client.
    unsigned int checkVersion(unsigned int, unsigned int);

    // returns bool, not void - init tests it and returns error 9 on failure (0x84742A4).
    bool startReceiveThread();
    void endReceiveThread();

public:
    // [INFERRED] Member names; reference offsets do not define the native STL layout.
    /* +0x004 */ int          m_refCount;                 // = 1
    // Connections keyed by the id carried in the packet header.
    /* +0x008 */ std::map<int, SWConnection*> m_connections;   // 12 bytes, +0x008..+0x013:
    /* +0x00C */ //   +0x008 _M_header (allocate(24))   +0x00C _M_node_count
                 //   +0x00C is read as A live-connection count: handleConnectRequest's capacity
                 //   test is `m_dword4BC + m_reconnectPasswordSize + m_connections.size() >=
                 //   m_maxNumberOfConnections`, i.e. free reserved slots and reserved reconnect
                 //   slots both count as occupied against the cap. Spell it size(), not the field.
    /* +0x010 */ //   +0x010 _M_key_compare - never written, the empty comparator
    // Persistent round-robin receive cursor, initialized for use by init.
    // [PLATFORM] Native construction uses end() instead of a singular iterator.
    /* +0x014 */ std::map<int, SWConnection*>::iterator m_receiveCursor;
    // Available connection IDs, consumed from the back when admitting clients.
    /* +0x018 */ std::deque<int> m_freeConnectionIds;     // push_back'd in init(); popped per accept
    // Pending connection removals, bounded by the maximum connection limit.
    /* +0x040 */ int          m_pendingRemovals[256];      // [INFERRED name] connection ids awaiting
                                                           //   freeConnection + erase
    /* +0x440 */ unsigned int m_pendingRemovalCount;        // resolved - how many of the above are
                                                           //   live; zeroed after the sweep
    /* +0x444 */ unsigned int m_maxNumberOfConnections;   // = 64
    /* +0x448 */ Packet       m_dataPacket;               // by value, 0x18
    /* +0x460 */ unsigned int m_port;                     // = 16567 - the stock BF2 port
    /* +0x464 */ UDPSocket*   m_listenSocket2;            // = 0  two sockets, both cleared here
    /* +0x468 */ UDPSocket*   m_listenSocket;             // = 0
    // Connection-ID value written in outbound accept, denial and disconnect headers.
    /* +0x46C */ bool         m_byte46C;                  // = 0
    /* +0x470 */ unsigned char* m_packetOutBuffer;        // = 0
    /* +0x474 */ BitStream*   m_bitStreamOut;             // = 0  - built by init()
    /* +0x478 */ BitStream*   m_bitStreamIn;              // = 0  - built by init()
    /* +0x47C */ bool         m_bInit;                    // = 0
    /* +0x47D */ bool         m_shuttingDown;             // the receive
                                                          //   thread's exit flag, read under m_mutex.
                                                          //   endReceiveThread sets it first.
    /* +0x480 */ std::string  m_name;                     // = "" - init() sets "<address>:<port>"
    /* +0x484 */ unsigned int m_version;                  // = 0  - the protocol compatibility gate
    /* +0x488 */ unsigned int m_connectionTimeOut;        // = 10000
    /* +0x48C */ std::string  m_password;                 // = ""
    /* +0x490 */ std::string  m_reconnectPassword;        // = ""
    /* +0x494 */ std::string  m_reservedPassword;         // = ""
    // [PLATFORM] Initialization time as seconds and microseconds; populated by the platform clock adapter.
    /* +0x498 */ struct TimeStamp { long tv_sec; long tv_usec; };
    /* +0x498 */ TimeStamp    m_initTime;
    /* +0x4A0 */ // gap4A0[8] - a second 8-byte slot beside it, untouched by the constructor.
                 //   [UNRESOLVED] whether it is a second timeval.
    /* +0x4A8 */ unsigned int m_time;                     // = 0
    /* +0x4AC */ unsigned int m_processedPacketCount;     // = 0
    /* +0x4B0 */ unsigned int m_dword4B0;                 // = 0  [UNRESOLVED]
    /* +0x4B4 */ unsigned int m_reconnectPasswordSize;    // = 0
    /* +0x4B8 */ unsigned int m_reservedPasswordSize;     // = 0
    // the free-reserved-slot count. _closeConnection gives a slot back
    // when a connection that held one (SWConnection +0x38) closes, then clamps to
    // m_reservedPasswordSize; handleConnectRequest spends from it.
    /* +0x4BC */ unsigned int m_dword4BC;                 // = 0
    /* +0x4C0 */ std::string  m_string4C0;                // = ""  [UNRESOLVED]
    /* +0x4C4 */ Thread*      m_netServerThread;          // = 0  - startReceiveThread's target
    /* +0x4C8 */ Mutex        m_mutex;                    // guards the connection update - held
                                                          //   across _updateConnections + _update
    /* +0x4CC */ Mutex        m_stopMutex;                // resolved - guards only m_stopRequested
    /* +0x4D0 */ unsigned int m_totalTimeSeconds;         // = 0
    /* +0x4D4 */ unsigned int m_lastUpdateMs;             // resolved - ms since m_initTime at the
                                                          //   previous _updateConnections; the
                                                          //   reconnect timeout is decremented by
                                                          //   the delta against it
    /* +0x4D8 */ std::string  m_listenAddrPort;           // = ""
    /* +0x4DC */ unsigned int m_reconnectTimeout;         // = 0
    /* +0x4E0 */ bool         m_bDebugNetwork;            // = 0  - init() reads "GSDebugNetwork"
    /* +0x4E4 */ Mutex        m_mutex4E4;                 // by value  [UNRESOLVED role]
    // [CHOSEN, not recovered] Retail pools 64 connections; PR's 100-player server scans 100
    // (prbf2_l64ded NetServer::allocConnection), so sv.maxPlayers 100 has a connection per player.
    static const int ConnectionPoolSize = 100;
    // Pool of ConnectionPoolSize connection pointers allocated by init.
    /* +0x4E8 */ SWConnection** m_swConnections;           // = 0; init allocates the pool
    /* +0x4EC */ bool         m_stopRequested;            // resolved - the receive thread's stop
                                                          //   flag, tested under m_stopMutex
    /* +0x4F0 */ PacketFilter m_packetFilter;             // by value, 0x10 - see the size note
};

} // namespace io
} // namespace hfe
} // namespace dice

#endif // IO_NETWORK_PROTOCOLLAYER_NETSERVER_H
