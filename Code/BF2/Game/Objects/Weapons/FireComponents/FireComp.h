// Path:     attested - Game/Objects/Weapons/FireComponents/FireComp.h occurs in both retail
// source-path maps.
// Contents: the typed weapon-fire component boundary recovered from the ELF secondary vtable and
// cross-checked against the PE vtable, plus FireComp's source-level runtime layout.
// The secondary IWeaponFireComp face is at +0x10 in both shipped x86 images.  Do not dispatch it by
// raw slot: Itanium has two destructor entries while MSVC has one, shifting every fire-specific
// entry after the destructor by four bytes.  Typed virtual calls below preserve that real ABI split.

#ifndef GAME_OBJECTS_WEAPONS_FIRECOMPONENTS_FIRECOMP_H
#define GAME_OBJECTS_WEAPONS_FIRECOMPONENTS_FIRECOMP_H

#include <cstddef>
#include <list>
#include <vector>

#include "../../WeaponComp.h"
#include "../../FireCompTemplate.h"
#include "../../IWeaponObject.h"
#include "../../../../Core/Random.h"
#include "../../../../Core/WeakPtr.h"
#include "../../../../Math/Mat4.h"

namespace dice {
namespace hfe {
namespace world {

class GenericFireArm;
class GenericFireArmTemplate;
class IObject;
class IObjectTemplate;
class IPlayerControlObject;
class IProjectileObject;

// Type names and 32-bit representation are attested by mangled signatures.  No enumerator names
// survive for these three, so keep them opaque and use evidenced integer arms in implementations.
enum FireState : int;
enum ReloadState : int;

// These names and values are recovered from their ostream switch tables at ELF 0x857FFC0 and
// .  They are the vocabulary accepted by ObjectTemplate.fire.addTimeEvent.
enum WeaponTrigger : int
{
    WTPostFire = 0,
    WTFireComplete = 1,
    WTReload = 2
};

enum WeaponAction : int
{
    WAShow = 0,
    WAHide = 1,
    WADrop = 2
};

struct WeaponInitEvent
{
    const GenericFireArmTemplate* firearmTemplate;
    int roundsPerMinute;
};

// GenericFireArm::postFire's stack aggregate, recovered end-to-end at ELF 0x8251540 and
// cross-checked against the PE dispatcher/component consumers.  Keep this a POD: retail leaves
// both padding ranges and maxProjectilesToFire untouched until their consuming arms are reached.
struct WeaponFireEvent
{
    /* +0x00 */ int fireCount;
    /* +0x04 */ IObject* projectile;
    /* +0x08 */ int projectileIndex;
    /* +0x0C */ int sequenceNumber;
    /* +0x10 */ bool dummyProjectile;
    /* +0x11 */ unsigned char padding11[3];
    /* +0x14 */ int maxProjectilesToFire;
    /* +0x18 */ FireRate fireRate;
    /* +0x1C */ bool requestedStateAboveTwo;
    /* +0x1D */ bool componentState;
    /* +0x1E */ unsigned char padding1E[2];
    /* +0x20 */ Vec3 start;
    /* +0x2C */ Vec3 end;
    /* +0x38 */ float fireStrength;
};

#if defined(_M_IX86) || defined(__i386__)
static_assert(sizeof(WeaponFireEvent) == 0x3C, "WeaponFireEvent must match the shipped x86 ABI");
static_assert(offsetof(WeaponFireEvent, maxProjectilesToFire) == 0x14,
              "WeaponFireEvent max-projectile offset drifted");
static_assert(offsetof(WeaponFireEvent, start) == 0x20,
              "WeaponFireEvent vector payload offset drifted");
#endif

struct WEventStruct
{
    WeaponAction action;
    WeaponTrigger trigger;
    float seconds;
    int ticks;
};

// Slots 0..32, recovered by commonality across all weapon-component implementations.  Five
// permissive defaults have emitted interface bodies; every other member is pure.
class IWeaponComp : public IBase
{
public:
    virtual void initComponent(WeaponInitEvent&) = 0;                  // +0x10
    virtual bool preUpdate() = 0;                                     // +0x14
    virtual void updateComponent(float, unsigned int) = 0;            // +0x18
    virtual bool isSelectable() const = 0;                            // +0x1C

    virtual void* getAbilityObjectList() { return 0; }                // +0x20
    virtual bool getAbilityWorking() { return false; }                // +0x24
    virtual float getAbility() const { return 0.0f; }                  // +0x28
    virtual float getAcceptDelay(bool) const { return 0.0f; }         // +0x2C

    virtual void fireStop(bool) = 0;                                  // +0x30
    virtual bool requestFire(bool, bool) = 0;                         // +0x34
    virtual void postFire(WeaponFireEvent&) = 0;                      // +0x38
    virtual void fireComplete(unsigned int) = 0;                      // +0x3C
    virtual void postSwitchFireRate(FireRate) = 0;                    // +0x40
    virtual bool requestReload(bool) = 0;                             // +0x44
    virtual void reload(ReloadState) = 0;                             // +0x48
    virtual void postReload() = 0;                                   // +0x4C
    virtual void outOfAmmo(bool) = 0;                                // +0x50
    virtual void stoppedTracking() = 0;                              // +0x54
    virtual void postChangeTarget(IObject*) = 0;                      // +0x58
    virtual void projectileWasReset(IObject*) = 0;                    // +0x5C
    virtual void enable() = 0;                                       // +0x60
    virtual void disable() = 0;                                      // +0x64
    virtual void stop() = 0;                                         // +0x68
    virtual bool busy() = 0;                                         // +0x6C
    virtual void exit() = 0;                                         // +0x70
    virtual void destroy() = 0;                                      // +0x74
    virtual void pickup() = 0;                                       // +0x78
    virtual void reset() = 0;                                        // +0x7C
    virtual bool preEnable() { return true; }                         // +0x80
};

// These three secondary faces are the bounded tails used by GenericFireArm's ordinary projectile
// path.  Their declaration order is read from the concrete ELF secondary vtables and checked
// against PE call slots.  In particular, the two Itanium destructor entries shift the first
// specialised call by four bytes relative to MSVC; typed dispatch is required here.
class IWeaponSoundComp : public IWeaponComp
{
public:
    virtual ~IWeaponSoundComp();                                      // ELF +0x84/+0x88
    virtual void toggleCamera() = 0;                                  // ELF +0x8C, PE +0x88
    virtual void setAlternateParent(GenericFireArm*) = 0;             // ELF +0x90, PE +0x8C
    virtual bool getNoisy() const = 0;                                // ELF +0x94, PE +0x90
    virtual int getMaxSoundsPerBurst() const = 0;                     // ELF +0x98, PE +0x94
};

class IWeaponTargetComp : public IWeaponComp
{
public:
    // These are the two genuine empty interface defaults at ELF 0x8279970/0x8279980.
    virtual void stoppedTracking();
    virtual void postChangeTarget(IObject*);

    virtual ~IWeaponTargetComp();                                     // ELF +0x84/+0x88
    virtual void setWeaponState(unsigned int) = 0;                    // ELF +0x8C
    virtual unsigned int getWeaponState() const = 0;                  // ELF +0x90
    virtual void setZoom(int) = 0;                                    // ELF +0x94
    virtual void setAllowTargetSeek(bool) = 0;                        // ELF +0x98
    virtual IObject* getTargetObject() const = 0;                     // ELF +0x9C
    virtual bool hasTargetLock() const = 0;                           // ELF +0xA0
    virtual void setTargetObject(IObject*, bool, IObject*) = 0;       // ELF +0xA4
    virtual void setTargetDirection(Vec3, Vec3) = 0;                  // ELF +0xA8
    virtual bool getSignalChangeTarget() = 0;                         // ELF +0xAC
    virtual void setCurrentProjectile(IObject*) = 0;                  // ELF +0xB0
    virtual int getTargetSystem() const = 0;                          // ELF +0xB4
    virtual float getMaxTargetDistance() const = 0;                   // ELF +0xB8
    virtual IObject* getProjectile() const = 0;                       // ELF +0xBC
    virtual float getDynamicVelocity() const = 0;                     // ELF +0xC0
    virtual bool hasMissileConnection() const = 0;                    // ELF +0xC4
    virtual io::PlayerInputMap getTargetInput() const = 0;            // ELF +0xC8
};

class IWeaponDeviationComp : public IWeaponComp
{
public:
    virtual ~IWeaponDeviationComp();                                  // ELF +0x84/+0x88
    virtual void updateDeviation(io::PlayerInput, bool, bool) = 0;    // ELF +0x8C
    virtual void setZoom(int) = 0;                                    // ELF +0x90
    // Returns a Vec3 by value. The hidden x86 sret pointer is visible in all three concrete
    // implementations and in GenericFireArm's forwarding thunk.
    virtual Vec3 deviatePosition(Vec3&, int) = 0;                     // ELF +0x94
    virtual void setProjectileDeviationFixed(Vec3&, int) = 0;        // ELF +0x98
    virtual float getMinDeviation() const = 0;                        // ELF +0x9C
    virtual float getMaxDeviation() const = 0;                        // ELF +0xA0
    virtual float getDeviation() const = 0;                           // ELF +0xA4
    virtual void setAIDeviation(float) = 0;                           // ELF +0xA8
};

class IWeaponFireComp : public IWeaponComp
{
public:
    // Supplies the three empty defaults inherited by FireComp.  Their slots remain in the
    // IWeaponComp prefix; they are not appended here.
    virtual void postFire(WeaponFireEvent&);
    virtual void fireComplete(unsigned int);
    virtual void postSwitchFireRate(FireRate);

    // ELF emits D1/D0 at +0x84/+0x88; MSVC emits one scalar-deleting entry at +0x84.
    virtual ~IWeaponFireComp();

    virtual IObject* getAlternateObject() const = 0;
    // Number of projectiles launched. MultiFireComp returns its barrel count, so this is not bool.
    virtual int fire(bool, int) = 0;
    virtual void switchFireRate() = 0;
    virtual bool isFireRateValid(FireRate) = 0;
    virtual void setFirebuttonLastHeldAtTick(io::PlayerInputMap, unsigned int) = 0;
    virtual void setWeaponState(unsigned int) = 0;
    virtual unsigned int getWeaponState() const = 0;
    virtual Vec3 getStartPos() const = 0;
    virtual Vec3 getEndPos() const = 0;
    virtual bool launchesProjectiles(int) const = 0;
    virtual void addProjectileReference(IObject*) = 0;
    virtual bool isWaitingForMissileConfirm() const = 0;
    virtual void setCurrentProjectile(IObject*) = 0;
    virtual bool getSignalFire() = 0;
    virtual bool getSignalComplete() = 0;
    virtual unsigned int getSignalCompleteAtTick() = 0;
    virtual void setMaxShots(int, int) = 0;
    virtual void setAllowEffects(bool) = 0;
    virtual const Mat4& getProjectileOrigin() const = 0;
    virtual FireState getFireState() const = 0;
    virtual FireRate getFireRate() const = 0;
    virtual float getFireStrength() const { return 1.0f; }
    virtual void setFireRate(FireRate) = 0;
    virtual void setFireAtTick(unsigned int) = 0;
    virtual unsigned int getFireAtTick() const = 0;
    virtual void setFireStartDelay(bool) = 0;
    virtual bool getFireStartDelay() const = 0;
    virtual bool ownsProjectile(IObject*) const = 0;
    virtual bool releaseProjectile(IObject*) = 0;
    virtual int getBurstSize() const = 0;
    virtual std::vector<IObject*>* getFiredProjectiles() = 0;
    virtual int getDummyShotsFired() const = 0;
    virtual int getLastFiredProjectileId() const = 0;
    virtual unsigned int getSequenceNr() = 0;
    virtual io::PlayerInputMap getFireInput() const = 0;
    virtual io::PlayerInputMap getAltFireInput() const = 0;
    virtual int getRoundsPerMinute() const = 0;
    virtual bool isFireLaunchDelay() const { return false; }
};

class FireComp : public WeaponComp, public IWeaponFireComp
{
public:
    explicit FireComp(FireCompTemplate*);
    virtual ~FireComp();

    virtual unsigned int getInterfaceID() const;

    virtual void initComponent(WeaponInitEvent&);
    virtual bool preUpdate();
    virtual void updateComponent(float, unsigned int);
    virtual bool isSelectable() const;
    virtual void fireStop(bool);
    virtual bool requestFire(bool, bool);
    virtual void fireComplete(unsigned int);
    virtual bool requestReload(bool);
    virtual void reload(ReloadState);
    virtual void postReload();
    virtual void outOfAmmo(bool);
    virtual void stoppedTracking();
    virtual void postChangeTarget(IObject*);
    virtual void projectileWasReset(IObject*);
    virtual void enable();
    virtual void disable();
    virtual void stop();
    virtual bool busy();
    virtual void exit();
    virtual void destroy();
    virtual void pickup();
    virtual void reset();

    virtual IObject* getAlternateObject() const;
    virtual int fire(bool, int);
    virtual void switchFireRate();
    virtual bool isFireRateValid(FireRate);
    virtual void setFirebuttonLastHeldAtTick(io::PlayerInputMap, unsigned int);
    virtual void setWeaponState(unsigned int);
    virtual unsigned int getWeaponState() const;
    virtual Vec3 getStartPos() const;
    virtual Vec3 getEndPos() const;
    virtual bool launchesProjectiles(int) const;
    virtual void addProjectileReference(IObject*);
    virtual bool isWaitingForMissileConfirm() const;
    virtual void setCurrentProjectile(IObject*);
    virtual bool getSignalFire();
    virtual bool getSignalComplete();
    virtual unsigned int getSignalCompleteAtTick();
    virtual void setMaxShots(int, int);
    virtual void setAllowEffects(bool);
    virtual const Mat4& getProjectileOrigin() const;
    virtual FireState getFireState() const;
    virtual FireRate getFireRate() const;
    virtual void setFireRate(FireRate);
    virtual void setFireAtTick(unsigned int);
    virtual unsigned int getFireAtTick() const;
    virtual void setFireStartDelay(bool);
    virtual bool getFireStartDelay() const;
    virtual bool ownsProjectile(IObject*) const;
    virtual bool releaseProjectile(IObject*);
    virtual int getBurstSize() const;
    virtual std::vector<IObject*>* getFiredProjectiles();
    virtual int getDummyShotsFired() const;
    virtual int getLastFiredProjectileId() const;
    virtual unsigned int getSequenceNr();
    virtual io::PlayerInputMap getFireInput() const;
    virtual io::PlayerInputMap getAltFireInput() const;
    virtual int getRoundsPerMinute() const;
    virtual bool isFireLaunchDelay() const;

    int getFireStartDelayInTicks();
    void setFireState(FireState);

    // [CHOSEN, not recovered] Project Reality's projectile-created filter: fired projectiles of this
    // template (case-insensitive) raise the ProjectileCreated script event on the server.
    static void notifyScriptsOnCreate(const std::string& projectileTemplateName);

protected:
    bool launchProjectile(int);
    IObject* setupProjectile(const Mat4&, Vec3&);
    // [CHOSEN, not recovered] Raises ProjectileCreated(weapon, projectile) for a listed template.
    void notifyProjectileCreated(IObject* projectile);
    bool getUseLocalPrediction(const IObjectTemplate*) const;
    bool ignoreNetworkedProjectileOnClient(const IObjectTemplate*) const;
    virtual IObject* createProjectile(const IObjectTemplate*, const Mat4&);
    void setProjectileParameters(IProjectileObject*, IPlayerControlObject*);

    std::list<WeakPtr<IObject> > m_projectiles;
    unsigned int m_weaponState;
    int m_lastMagazineIndex;
    FireRate m_fireRate;
    unsigned int m_fireAtTick;
    unsigned int m_reloadAtTick;
    unsigned int m_sequenceNumber;
    int m_maxShots;

    // [UNRESOLVED source type].  No constructor/destructor work touches this 44-byte POD region in
    // either port.  Preserve the observed extent until its users identify the original aggregate.
    unsigned char m_fireData[44];

    unsigned int m_fireCompletionState;
    int m_lastFiredProjectileId;
    bool m_fireStartDelay;
    unsigned int m_fireStartDelayTicks;
    int m_postReloadState;
    bool m_fireLaunchDelayPending;
    bool m_signalFire;
    bool m_signalComplete;
    unsigned int m_signalCompleteAtTick;

    std::vector<IObject*> m_firedProjectiles;
    Random m_random;
    FireCompTemplate* m_template;
    mutable Mat4 m_projectileOrigin;
    Mat4 m_lockedCameraTransform;
    int m_dummyShotsFired;
    unsigned int m_fireButtonLastHeldAtTick;
    unsigned int m_altFireButtonLastHeldAtTick;
    bool m_allowEffects;
    // Despite setCurrentProjectile's pointer-shaped public API, both shipped bodies use this as a
    // WorldPref tick stamp.  The setter ignores its pointer argument and clears the stamp.
    unsigned int m_currentProjectile;
    FireState m_fireState;

    // WeaponComp::m_initObject is the firearm pointer at runtime; this named helper avoids
    // hard-coded offsets in derived source while preserving the two-pointer WeaponComp contract.
    GenericFireArm* firearm() const
    {
        return reinterpret_cast<GenericFireArm*>(m_initObject);
    }
};

} // namespace world
} // namespace hfe
} // namespace dice

#endif // GAME_OBJECTS_WEAPONS_FIRECOMPONENTS_FIRECOMP_H
