// Server configuration and reserved-slot lists load through console scripts.
// Settings getters can apply ranked-mode overrides or consult the settings repository;
// they are not all direct field reads.

#ifndef GAME_GAMESERVER_SERVERSETTINGS_H     // [INFERRED] guard style
#define GAME_GAMESERVER_SERVERSETTINGS_H

#include <iosfwd>
#include <string>
#include <vector>      // ReservedSlots::m_nicks - the +0x04 std::vector<std::string>

namespace dice {
namespace hfe {

// 42 members, complete.
class ServerSettings
{
public:
    // The complete default table - see the layout block below. Ends by calling setMaxPlayers(16),
    // which is where the shipped default of 16 players comes from (the `settings` repository is
    // null during static initialisation, so the command-line consult inside is skipped).
    ServerSettings();

    // Returns the function-local singleton, published through serverSettings before main.
    static ServerSettings* instance();
    bool load();                                       //   runs serversettings.con
    // save() returns bool - when the pair was recovered. Both images end
    // it with `al = (bytes written == script length)`; the void spelling here was a placeholder.
    // Every caller discards it, so nothing had caught the difference.
    bool save();
    void save(std::ostream&);                          //  / PE 0x425E80  overload, 70 rows

    // getters
    int  getCoopBotCount() const;
    // * these return float, NOT int - (see the note below getTeamRatioPercent).
    float getCoopBotDifficulty() const;
    float getCoopBotRatio() const;
    float getManDownTime() const;
    int  getMaxPlayers() const;
    // NOT const - the demangled name is `getNoVehicles(void)` with no const qualifier, and the body
    // writes [this+0x101] when the mode is not coop. A const declaration is wrong on both counts.
    bool getNoVehicles();
    int  getNumPlayersNeededToStart() const;
    int  getNumReservedSlots() const;                  //   the only tiny accessor
    const std::string& getPassword() const;
    // Effective-ranked query, not a raw field read: false for AI, LAN and gpm_coop; only then
    // returns m_ranked. ELF 0x80BBB80 / PE 0x425360, identical control flow in both ports.
    bool getRanked() const;
    // * returns float, NOT int - * The body ends `fld dword ptr [eax+68h]`
    // (or `fld ds:flt_888D944` on the ranked path) and leaves the value in ST0, which is the x86
    // float return convention. It is also NOT const - the demangled name carries no qualifier.
    float getSpawnTime();
    bool getSvPunkBuster() const;
    float getTeamRatioPercent() const;

    int  getTicketRatio() const;
    bool getUseGlobalRank() const;
    bool getUseGlobalUnlocks() const;

    // setters
    void setAllowExternalViews(bool);
    void setAllowFreeCam(bool);
    void setAllowNoseCam(bool);
    // [INFERRED name] Sets the configured coop bot count.
    void setCoopBotCount(int);                         //  (inlined in ELF)
    void setEndDelay(int);
    void setEndOfRoundDelay(int);
    void setFriendlyFireWithMines(bool);
    void setHitIndicator(bool);
    // Console-bound setters return an error message; an empty string means success.
    std::string setInternet(bool);
    // Returns an error message when running, otherwise an empty string on success.
    std::string setMaxPlayers(int);
    // Returns an error message on refusal, or an empty string on success.
    std::string setNoVehicles(bool);                   //  "Cannot change sv.noVehicles while running."
    std::string setNumReservedSlots(int);              //  "Cannot change sv.numReservedSlots while running."
    std::string setPassword(const std::string&);       //  "Cannot change sv.setPassword while running."
    void setRadioBlockedDurationTime(int);
    void setRadioMaxSpamFlagCount(int);
    void setRadioSpamInterval(int);
    void setRanked(bool);                              //   demoted by init, not refused
    std::string setServerPort(int);                    //  "illegal port value" / "...while running."
    void setTKPunishByDefault(bool);
    void setTeamVoteOnly(bool);
    void setTimeLimit(int);
    void setVotingEnabled(bool);
    void setWelcomeMessage(const std::string&);

    // Server configuration values. Internet mode is immutable while running and
    // controls online service initialization; LAN startup skips those services.
    // Use typed fields: STL layouts and member offsets differ between toolchains.
    std::string m_serverName;       // +0x04
    // Server bind address passed by reference to startup.
    std::string m_serverIP;         // +0x1C
    int         m_serverPort;       // +0x20  (BF2Engine::startGame reads it straight; setServerPort
                                    //          is the console-bound writer)
    bool  m_internet;               // +0x0C
    bool  m_allowFreeCam;           // +0x50
    bool  m_allowExternalViews;     // +0x51
    int   m_maxPlayers;             // +0x54
    // getMaxPlayers has a ranked floor: `ranked ? max(16, m_maxPlayers) : m_maxPlayers`
    // (0x80BC1C0, 37 B) - a fifth ranked-enforcement rule alongside the four documented above.
    // The console GET bypasses the accessor and reports the RAW field, floor and all.
    int   m_endDelay;               // +0x64   clamped to >= 10 by setEndDelay
    float m_spawnTime;              // +0x68
    float m_manDownTime;            // +0x6C
    int   m_ticketRatio;            // +0x70   forced to 100 when ranked
    int   m_roundsPerMap;           // +0x74

    // Friendly-fire percentages are integers. Damage calculation converts them to
    // floating point and divides by 100; preserve integer script round-tripping.
    int   m_soldierFriendlyFire;        // +0x80   sv.soldierFriendlyFire
    int   m_vehicleFriendlyFire;        // +0x84   sv.vehicleFriendlyFire
    int   m_soldierSplashFriendlyFire;  // +0x88   sv.soldierSplashFriendlyFire
    int   m_vehicleSplashFriendlyFire;  // +0x8C   sv.vehicleSplashFriendlyFire

    bool  m_ranked;                 // +0xF5
    int   m_numReservedSlots;       // +0xFC
    bool  m_friendlyFireWithMines;  // +0x100
    bool  m_noVehicles;             // +0x101

    // Stored server settings. Comments beside members describe constructor defaults;
    // getters may apply ranked-server rules or consult the settings repository.
    std::string m_configFile;            // +0x00  ""    [CHOSEN name] load() runs this file
                                         //              (empty => load() returns 0 - see load's
                                         //              length test); +serverSettingsFile's target
    // ... m_serverName +0x04 is declared above ...
    std::string m_password;              // +0x08  ""
    // ... m_internet +0x0C is declared above ...
    int   m_bandwidthChoke;              // +0x10  0
    // Stored as a plain dword (`mov [eax+5Ch], edi` in the PE binding); only the console metadata
    // spells the type "ConnectionType". GameServer keeps its copy as an int too, so int it stays.
    int   m_maxConnectionType;           // +0x14  5     sv.maxConnectionType; read by GameServer::init
    std::string m_welcomeMessage;        // +0x18  ""
    // ... m_serverIP +0x1C / m_serverPort +0x20 (default 16567) are declared above ...
    bool  m_votingEnabled;               // +0x24  true
    int   m_voteTime;                    // +0x28  60
    int   m_minPlayersForVoting;         // +0x2C  2
    bool  m_teamVoteOnly;                // +0x30  true
    bool  m_voipEnabled;                 // +0x31  true
    int   m_voipQuality;                 // +0x34  3
    bool  m_voipServerRemote;            // +0x38  false
    std::string m_voipServerRemoteIP;    // +0x3C  ""
    int   m_voipServerPort;              // +0x40  55125
    int   m_voipBFClientPort;            // +0x44  55123
    int   m_voipBFServerPort;            // +0x48  55124
    // âš  Constructed from the "" literal (0x80BAD2F string ctor), where the other nine strings
    // default-construct onto the shared empty rep - a real source-level distinction (dice wrote
    // `voipSharedPassword("")`), reproduced in the ctor's init list.
    std::string m_voipSharedPassword;    // +0x4C  ""
    // ... m_allowFreeCam +0x50 / m_allowExternalViews +0x51 are declared above ...
    bool  m_allowNoseCam;                // +0x52  true
    bool  m_hitIndicator;                // +0x53  true
    // ... m_maxPlayers +0x54 is declared above (ctor: 0, then setMaxPlayers(16) as its last act -
    //     with `settings` null during static init that stores the clamped 16) ...
    int   m_playersNeededToStart;        // +0x58  2
    float m_notEnoughPlayersRestartDelay;// +0x5C  5.0f
    int   m_startDelay;                  // +0x60  2
    // ... m_endDelay +0x64 (10) / m_spawnTime +0x68 (5.0f) / m_manDownTime +0x6C (10.0f) /
    //     m_ticketRatio +0x70 (100) / m_roundsPerMap +0x74 (3) are declared above ...
    int   m_timeLimit;                   // +0x78  0
    int   m_scoreLimit;                  // +0x7C  0
    // ... the four friendly-fire ints +0x80..+0x8C (all 100) are declared above ...
    char  m_gap90[8];                    // +0x90  âš  never initialised by the ctor - starts as
                                         //        whatever the allocation held. [UNRESOLVED] use.
    bool  m_tkPunishEnabled;             // +0x98  false
    int   m_tkNumPunishToKick;           // +0x9C  3
    bool  m_tkPunishDefault;             // +0xA0  true
    std::string m_interfaceIP;           // +0xA4  ""
    int   m_gameSpyPort;                 // +0xA8  29900
    bool  m_allowNATNegotiation;         // +0xAC  false
    bool  m_autoRecord;                  // +0xAD  false
    std::string m_demoIndexURL;          // +0xB0  ""
    std::string m_demoDownloadURL;       // +0xB4  ""
    std::string m_autoDemoHook;          // +0xB8  ""
    int   m_demoQuality;                 // +0xBC  1
    std::string m_adminScript;           // +0xC0  ""
    // âš  A float - the ctor stores 0x41F00000 = 30.0f (the IDB's struct typed it _DWORD; the bit
    // pattern and the neighbouring float stores settle it).
    float m_timeBeforeRestartMap;        // +0xC4  30.0f
    bool  m_autoBalanceTeam;             // +0xC8  false
    float m_teamRatioPercent;            // +0xCC; defaults to 100.0f.
    float m_coopBotRatio;                // +0xD0  50.0f
    int   m_coopBotCount;                // +0xD4  16
    float m_coopBotDifficulty;           // +0xD8  50.0f
    std::string m_sponsorText;           // +0xDC  ""
    std::string m_sponsorLogoURL;        // +0xE0  ""
    std::string m_communityLogoURL;      // +0xE4  ""
    int   m_radioSpamInterval;           // +0xE8  6
    int   m_radioMaxSpamFlagCount;       // +0xEC  6
    int   m_radioBlockedDuration;        // +0xF0  30
    // Consulted only for unranked internet servers; getSvPunkBuster also checks internet and ranked state.
    bool  m_punkBuster;                  // +0xF4  false
    // ... m_ranked +0xF5 (false) / m_numReservedSlots +0xFC (0, via xor) are declared above ...
    bool  m_useGlobalRank;               // +0xF6  true
    bool  m_useGlobalUnlocks;            // +0xF7  true
    int   m_endOfRoundDelay;             // +0xF8  15
    // ... m_friendlyFireWithMines +0x100 (false) / m_noVehicles +0x101 (false) declared above ...

    // [CHOSEN, not recovered] Project Reality setting appended past the retail layout. How often
    // GameServer repeats a connection's ghost-scope pass: after a pass the scope timer is reloaded
    // with (ticks - 1) * 0.1 s. Retail hard-codes 0.1 s, which is 2. At least 1.
    int   m_ghostScopeRefreshTicks;      //        4
};

// Reserved nicknames and their configuration filename.
class ReservedSlots
{
public:
    ReservedSlots();
    ~ReservedSlots();

    static ReservedSlots* instance();
    bool load();                                       //   runs reservedSlots.con
    // [FRONTIER] save and the nick-list operations below remain declared, not written. They are
    // outside the current boot read path; console mutation and handleClientInfo eventually use them.
    void save();
    void save(std::ostream&);                          //   overload
    void clear();
    void list();
    void addNick(const std::string&);
    void removeNick(const std::string&);
    bool isNickReserved(const std::string&) const;     //   (const per ELF)

    const std::string& getConfigFile() const;
    void setConfigFile(const std::string&);

public:
    /* +0x00 */ std::string              m_configFile; // reservedSlots.con; BF2Engine's region 1
                                                       //   defaults it, +rsconfig overrides it
    /* +0x04 */ std::vector<std::string> m_nicks;      // [INFERRED name] - addNick/removeNick/
                                                       //   isNickReserved are its only users
};

extern ServerSettings* serverSettings;   // ds:0x8AAB7A0
extern ReservedSlots*  reservedSlots;    // ds:0x8AAB768

} // namespace hfe
} // namespace dice

#endif
