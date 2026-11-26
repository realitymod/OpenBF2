// Ammo storage and reload state machine. Magazine slot 0 is loaded ammo; later entries are spares.
// Construction always creates slot 0, using -1 for infinite ammo; reload consumes spares from the back.
// reloadAmount selects magazine or shell phases. Keep state values and independent swap/completion clocks unchanged.
// Named magazine links receive shared counts and are resolved lazily if absent during initialization.
// Header and implementation paths are attested by __FILE__.

#ifndef GAME_OBJECTS_WEAPONS_AMMOCOMPONENTS_AMMOCOMP_H
#define GAME_OBJECTS_WEAPONS_AMMOCOMPONENTS_AMMOCOMP_H

#include <vector>

#include "../../WeaponComp.h"
#include "../../AmmoCompTemplate.h"
#include "../FireComponents/FireComp.h"   // IWeaponComp, WeaponInitEvent, WeaponFireEvent, ReloadState

namespace dice {
namespace hfe {
namespace world {

class GenericFireArm;
class IWeaponObject;

// Ammo component interface; own slots follow the destructor after IWeaponComp's inherited surface.
// Keep the inherited reload defaults and external-resupply capability as concrete methods.
class IWeaponAmmoComp : public IWeaponComp
{
public:
    // ELF emits D1/D0 at +0x84/+0x88; MSVC emits one scalar-deleting entry at +0x84.
    virtual ~IWeaponAmmoComp();

    // Supplies the three defaults inherited by AmmoComp. Their slots stay in the IWeaponComp
    // prefix; they are not appended here.
    virtual void reload(ReloadState state);                          // +0x048
    virtual void postReload();                                       // +0x04C
    virtual void outOfAmmo(bool value);                              // +0x050

    virtual void reload() = 0;                                       // +0x08C
    virtual void queueReload() = 0;                                  // +0x090
    virtual bool shouldReload() const = 0;                           // +0x094
    virtual bool needsReload() const = 0;                            // +0x098
    virtual bool canBeRefilled();                                    // +0x09C
    virtual float refillPercent(float percent, bool testOnly) = 0;   // +0x0A0
    virtual unsigned int refillAbsolute(unsigned int amount,
                                        bool testOnly) = 0;          // +0x0A4
    virtual float getReloadTimeLeftInPercent() const = 0;            // +0x0A8
    virtual float getTotalReloadTimeLeftInPercent() const = 0;       // +0x0AC
    virtual bool getSignalReloadComplete() = 0;                      // +0x0B0
    virtual bool getSignalReload() = 0;                              // +0x0B4
    virtual bool getBulletInChamber() const = 0;                     // +0x0B8
    virtual bool getIsReloading() const = 0;                         // +0x0BC
    virtual float getReloadTime() const = 0;                         // +0x0C0
    virtual ReloadState getReloadState() const = 0;                  // +0x0C4
    virtual int getMagSize() const = 0;                              // +0x0C8
    virtual int getMaxNrOfMags() const = 0;                          // +0x0CC
    virtual bool getAutoReload() const = 0;                          // +0x0D0
    virtual int getMagVectorSize() const = 0;                        // +0x0D4
    virtual int getMag0Size() const = 0;                             // +0x0D8
    virtual std::vector<int>* getAmmo() = 0;                         // +0x0DC
};

// dice::hfe::world::AmmoComp - 92 bytes (0x5C), abstract: its createComponent is a Debug refusal
// ("Virtual type", AmmoComp.h:154). Two leaves, DefaultAmmoComp and ReplenishingAmmoComp, and the
// Default one adds NO behaviour at all - every body below is what a normal weapon runs.
class AmmoComp : public WeaponComp, public IWeaponAmmoComp
{
public:
    explicit AmmoComp(AmmoCompTemplate* objectTemplate);
    virtual ~AmmoComp();

    virtual unsigned int getInterfaceID() const;

    virtual void initComponent(WeaponInitEvent& event);
    virtual bool preUpdate();
    virtual void updateComponent(float seconds, unsigned int flags);
    virtual bool isSelectable() const;
    virtual float getAcceptDelay(bool alternate) const;
    virtual void fireStop(bool alternate);
    virtual bool requestFire(bool continuing, bool alternate);
    virtual void postFire(WeaponFireEvent& event);
    virtual void fireComplete(unsigned int tick);
    virtual void postSwitchFireRate(FireRate rate);
    virtual bool requestReload(bool automatic);
    virtual void stoppedTracking();
    virtual void postChangeTarget(IObject* target);
    virtual void projectileWasReset(IObject* projectile);
    virtual void enable();
    virtual void disable();
    virtual void stop();
    virtual bool busy();
    virtual void exit();
    virtual void destroy();
    virtual void pickup();
    virtual void reset();

    virtual void reload();
    virtual void queueReload();
    virtual bool shouldReload() const;
    virtual bool needsReload() const;
    virtual float refillPercent(float percent, bool testOnly);
    virtual unsigned int refillAbsolute(unsigned int amount,
                                        bool testOnly);
    virtual float getReloadTimeLeftInPercent() const;
    virtual float getTotalReloadTimeLeftInPercent() const;
    virtual bool getSignalReloadComplete();
    virtual bool getSignalReload();
    virtual bool getBulletInChamber() const;
    virtual bool getIsReloading() const;
    virtual float getReloadTime() const;
    virtual ReloadState getReloadState() const;
    virtual int getMagSize() const;
    virtual int getMaxNrOfMags() const;
    virtual bool getAutoReload() const;
    virtual int getMagVectorSize() const;
    virtual int getMag0Size() const;
    virtual std::vector<int>* getAmmo();

    // TWO virtuals that ARE NOT ON THE interface. reloadMag and canReload are introduced by
    // AmmoComp itself, land at the END of its primary table (+0xE4 and +0xE8), and appear in NO
    // adjusted table - so they are reachable only through a typed AmmoComp*, never through
    // IWeaponAmmoComp. updateComponent, enable, fireComplete and shouldReload all dispatch them,
    // and ReplenishingAmmoComp overrides canReload (0x825A230), which is why they are virtual and
    // why they must be declared after every interface override above.
    virtual void reloadMag();
    virtual bool canReload() const;

    // Non-virtual internals. All five are called directly, never through a table.
    void reloadComplete();
    void doReload(unsigned int startTick);
    unsigned int refillInternal(unsigned int amount, bool testOnly);
    bool shouldRefillBullets();
    bool needsRefill();
    int  getMaxRefillAmount();
    // [CHOSEN, not recovered] The retail refillPercent body, which refillPercent now wraps.
    float refillPercentUnscaled(float percent, bool testOnly);

protected:
    // THE magazine vector. [0] is the loaded magazine's remaining rounds; [1..] are spares, each
    // holding magSize. -1 in slot 0 is the infinite-ammo sentinel. Never empty - see the file note.
    /* +0x14 */ std::vector<int> m_magazines;

    // ctor 0, and the only other write is updateComponent clearing it just before reloadMag().
    // Nothing recovered ever sets it, so as shipped it is always false. [UNRESOLVED].
    /* +0x20 */ bool m_byte20;

    // THE post-shot gate. fireComplete sets it to `tick + minimumTimeToReloadTimes30`; nothing may
    // auto-reload until the tick count passes it, and updateComponent drops state 1 back to 0 there.
    /* +0x24 */ unsigned int m_acceptFireAtTick;
    // when THE rounds appear - reloadTime * changeMagAt into the reload. [CHOSEN] names for both.
    /* +0x28 */ unsigned int m_changeMagAtTick;
    // when THE component IS free again.
    /* +0x2C */ unsigned int m_reloadDoneAtTick;
    // postFire writes this only when minimumTimeToReloadTimes30 is nonzero.
    // [UNRESOLVED] No reader is known. [CHOSEN] Field name.
    /* +0x30 */ unsigned int m_lastFireTick;
    // The auto-reload clock. getTotalReloadTimeLeftInPercent measures from here.
    /* +0x34 */ unsigned int m_autoReloadStartTick;
    // The refill pair - see refillPercent. The carry is only honoured for 30 ticks (one second),
    // which is what turns a stream of tiny per-tick percentages into whole rounds.
    /* +0x38 */ unsigned int m_lastRefillTick;
    /* +0x3C */ float m_refillCarry;
    // shell counters. Left counts down per shell; Total is captured at request time so doReload
    // can tell the first shell (left == total) from the middle ones.
    /* +0x40 */ int m_reloadStepsLeft;
    /* +0x44 */ int m_reloadStepsTotal;
    /* +0x48 */ ReloadState m_reloadState;

    // ONE-shot latch FOR THE magazine swap. doReload clears it, updateComponent tests it so
    // reloadMag runs exactly once per reload, and reloadMag sets it itself on both exits.
    /* +0x4C */ bool m_magSwapped;
    // "the gun ran dry" - postFire raises it, enable and reloadMag clear it. [CHOSEN] name.
    /* +0x4D */ bool m_outOfAmmoNotified;
    /* +0x4E */ bool m_signalReloadComplete;   // consumed by getSignalReloadComplete
    /* +0x4F */ bool m_signalReload;           // consumed by getSignalReload; also gates busy()
    // "the shot that just landed emptied an auto-reloading weapon" - fireComplete acts on it.
    /* +0x50 */ bool m_autoReloadPending;

    // +0x51..+0x53 are NOT written by the constructor. Tail padding of the bool group; nothing
    // recovered reads them.

    // The magLinkWeapon target, resolved by initComponent. Null for every weapon whose template
    // leaves magLinkWeapon empty, which is nearly all of them.
    /* +0x54 */ IWeaponObject* m_linkedWeapon;
    /* +0x58 */ AmmoCompTemplate* m_template;

    // WeaponComp::m_initObject is the firearm at runtime. Named helper so derived source never
    // hard-codes the offset, matching FireComp::firearm().
    GenericFireArm* firearm() const
    {
        return reinterpret_cast<GenericFireArm*>(m_initObject);
    }
};

} // namespace world
} // namespace hfe
} // namespace dice

#endif // GAME_OBJECTS_WEAPONS_AMMOCOMPONENTS_AMMOCOMP_H
