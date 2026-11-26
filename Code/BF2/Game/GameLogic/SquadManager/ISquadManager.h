// Squad and commander interface. Keep its virtual order and nonvirtual destructor.
// [INFERRED] Header placement. Unverified return types remain [CHOSEN]; setSquadName
// returns whether the rename succeeded.

#ifndef GAME_GAMELOGIC_SQUADMANAGER_ISQUADMANAGER_H   // [INFERRED] guard style
#define GAME_GAMELOGIC_SQUADMANAGER_ISQUADMANAGER_H

#include <string>

#include "../../../Core/IBase.h"
#include "../../Utility/Vars.h"
#include "ISquadInfo.h"

namespace dice {
namespace hfe {

namespace world { class IPlayer; }

class ISquadManager : public IBase
{
public:
    // the 34 slots, in vtable order. DO NOT reorder.
    virtual void reset()                                                          = 0; // +0x10 read
    virtual void reAddDynamicSpawnGroups()                                        = 0; // +0x14
    virtual void sendSquadManagerState(int)                                       = 0; // +0x18 read
    virtual bool setSquadName(int, int, int, const std::string&, bool)            = 0; // +0x1C read
    virtual void setIsPrivate(int, int, bool, bool)                               = 0; // +0x20
    virtual bool setIsAccepted(int, int, bool, OrderType, SquadOrderType, bool)   = 0; // +0x24 read
    virtual ISquadInfo* getSquadInfo(int, int)                                    = 0; // +0x28 read
    virtual void inviteToSquad(int, int, int, bool, bool)                         = 0; // +0x2C
    virtual bool addToSquad(int, int, int)                                        = 0; // +0x30; BFSquad tests AL
    virtual void removeFromSquad(int, int, int, bool)                             = 0; // +0x34
    virtual void setSquadLeader(int, int, int, bool)                              = 0; // +0x38
    virtual int  getSquadLeader(int, int)                                         = 0; // +0x3C
    virtual int  getFirstUnusedSquad(int) const                                   = 0; // +0x40
    virtual void issueSquadOrder(int, int, SquadOrder&, bool)                     = 0; // +0x44
    virtual void issueCommanderOrder(int, int, SquadOrder&, bool)                 = 0; // +0x48
    virtual void issueRequest(int, int, SquadOrder&, bool)                        = 0; // +0x4C
    virtual void resetSquad(int, int)                                             = 0; // +0x50
    virtual void findNewSquadLeader(int, int, int)                                = 0; // +0x54
    virtual void applyForCommander(int, int)                                      = 0; // +0x58
    virtual int  popNextCommanderApplicant(int)                                   = 0; // +0x5C
    virtual void acceptCommander(int, int)                                        = 0; // +0x60
    virtual void declineCommander(int, int)                                       = 0; // +0x64
    virtual void resignCommander(int)                                             = 0; // +0x68
    virtual void setCommander(int, int)                                           = 0; // +0x6C
    virtual int  getCommander(int)                                                = 0; // +0x70 read
    virtual void* getCommanderCam(int)                                            = 0; // +0x74
    virtual void setCommanderToSoldierMode(int)                                   = 0; // +0x78
    virtual void setCommanderToOverheadCamMode(int)                               = 0; // +0x7C
    virtual void setCommanderOverheadCamZoomMode(int, bool)                       = 0; // +0x80
    virtual void removeCommanderApplicant(int, int)                               = 0; // +0x84
    virtual void addOrderMarker(int, SquadOrder&)                                 = 0; // +0x88
    virtual void removeAllOrderMarkers()                                          = 0; // +0x8C
    virtual void removeOrderMarker(int)                                           = 0; // +0x90
    virtual void updateOrderMarkers(world::IPlayer*)                              = 0; // +0x94

protected:
    // Non-virtual, like ~IBase and for the same reason: the destructor pair sits at +0x98/+0x9C,
    // after these slots, which is only consistent with it not being part of this interface.
    ~ISquadManager() {}
};

// [CHOSEN, not recovered] Project Reality's squad size; retail hard-codes 6. An engine Var, not a
// server setting, so the server's join gate and the client's HUD read the same value from the mod.
inline int getMaxSquadMembers()
{
    return *Vars::getInt("squad-maxMembers", 8);
}

} // namespace hfe
} // namespace dice

#endif // GAME_GAMELOGIC_SQUADMANAGER_ISQUADMANAGER_H
