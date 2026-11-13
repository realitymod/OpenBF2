// Supply crate that heals and rearms nearby soldiers and vehicles.
// The original spending rules cross the heal/ammo gauges; each side's empty check observes the other's spending.

#ifndef GAME_OBJECTS_GAME_SUPPLYOBJECT_H     // [INFERRED] guard style
#define GAME_OBJECTS_GAME_SUPPLYOBJECT_H

#include "../Base/Bundle.h"
#include "../SupplyObjectTemplate.h"

namespace dice {
namespace hfe {
class SimpleObjectNetworkable;
namespace world {

// Bundle with supply storage, team and landing state; it adds no interface base or reference counter.
class SupplyObject : public Bundle
{
public:
    explicit SupplyObject(const SupplyObjectTemplate* objectTemplate);  // read in full
    virtual ~SupplyObject();                         // (D1) / 51 B (D0) - vptr restore only

    // vtable overrides, slot-verified against the 96-slot primary table
    virtual void init();                             // slot 8  -
    virtual void disableCachedObject();              // slot 9  -
    virtual void enableCachedObject();               // slot 10 -
    virtual void destroy(bool force);                // slot 13 -
    virtual bool handleCollision(IObject*, const Vec3&, const Vec3&, const Vec3&,
                                 int, int, CollisionFaceInfo*, CollisionFaceInfo*);
                                                     // slot 45 -
    virtual void handleMessage(TemplateMessage, IPlayer*, bool);
                                                     // slot 65 - (calls update)
    virtual std::string getArmorInfo();              // slot 83 - (sret, IObject.h's
                                                     // documented trap; three gauges + team)
    virtual void initNetworkableComp();              // slot 94 -

    // non-virtual lifecycle + gauge arithmetic, all written
    void reset();                                    // re-copies the template, ungated
    bool getWorkOnVehicles();                        // template +0x184
    bool getWorkOnSoldiers();                        // template +0x185
    bool hasHealAbility();                           // template healSpeed > 0
    bool hasAmmoAbility();                           // template refillAmmoSpeed > 0
    bool hasEmptyHealStorage();                      // see the bug note above
    bool hasEmptyAmmoStorage();
    void SubtractHeal(float amount);                 // dice's capital S, and THE BUG
    void SubtractAmmo(float amount);                 // likewise
    void setSuppliesLanded(bool landed);             // client-side arm (replication path)

    // Servicing runs on a 0.2-second heartbeat; vehicles use the template's category filter.
    void update(float dt);
    void workOnSoldiers();

    void heal(IObject* object);                      // calls SubtractHeal
    void reloadAmmo(IObject* object);                // calls SubtractAmmo
    void workOnVehicles();                           // applies the template's typed
                                                     // vehicle-category filter
    void healAndReload(IObject* object);             // workOnSoldiers' per-player payload
    int  updateTeamFromClosestCP();                  // gated on teamFromClosestCP.
                                                     // Returns the team stored by update().

    // [CHOSEN, not recovered] The shared pool, for Python's supply.getSupplyAmmo/setSupplyAmmo.
    float getSharedStorage() const     { return m_sharedStorage; }
    void  setSharedStorage(float value) { m_sharedStorage = value; }

private:
    // The networkable snapshots the landed byte directly in both ports. Use member access because
    // Bundle's native STL/layout differs from the retail ELF (+0x150) and PE (+0x168).
    friend class ::dice::hfe::SimpleObjectNetworkable;
    // Client construction leaves storage quantities uninitialized until reset or replication.
    /* 0x13C */ const SupplyObjectTemplate* m_template;  // shadows Object's +0x14 pointer, the
                                                         // ControlPoint precedent (same value by
                                                         // construction; initNetworkableComp's
                                                         // ELF body reads the base copy, every
                                                         // other method reads this one)
    /* 0x140 */ int   m_team;                        // ctor/reset <- template team (+0x180)
    /* 0x144 */ float m_healStorage;                 // <- template healStorageSize   (+0x198)
    /* 0x148 */ float m_ammoStorage;                 // <- template ammoStorageSize   (+0x19C)
    /* 0x14C */ float m_sharedStorage;               // <- template sharedStorageSize (+0x1A0) -
                                                     // the common fallback pool both Subtract
                                                     // arms spill into, with NO floor: it goes
                                                     // negative as shipped
    /* 0x150 */ bool  m_suppliesLanded;              // ctor false; the supply-drop landing latch
    /* 0x151 */ char  _pad151[3];
};

} // namespace world
} // namespace hfe
} // namespace dice

#endif // GAME_OBJECTS_GAME_SUPPLYOBJECT_H
