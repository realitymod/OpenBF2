// Infantry avatar with player control, inventory, movement, animation and collision rigs.
// Both rigs run on the server where required for hit detection. Source placement is attested.

#ifndef GAME_OBJECTS_GAME_SOLDIER_H     // [INFERRED] guard style
#define GAME_OBJECTS_GAME_SOLDIER_H

#include <list>
#include <map>
#include <string>
#include <vector>

#include "../Base/Bundle.h"
#include "../ISoldier.h"
#include "../IItemContainerObject.h"
#include "../../../Scene/Object/ObjectTypes/Game/IPlayerControlObject.h"
#include "PlayerControlObject.h"    // the real hfe::SprintState and hfe::FireButtonState

namespace dice {

// dice::anim IS A sibling OF dice::hfe, NOT A child - `_ZN4dice4anim8Skeleton...`, with no
// `3hfe` between. Declaring it inside hfe makes a different type that silently fails to match
// Animation/BoneAnimation/Skeleton.h; that mistake was made here first and caught by the compiler.
namespace anim { class IAnimationSystem; class RagDoll; class Skeleton; }

namespace hfe {

class Player;
class ServerGameLogic;
class SoldierSound;          // dedicated no-op sound voice; see Sound/SoldierSound.h
class ISound;
class SoldierNetworkable;    // the wire wrapper - a friend below; see Networkables.h
namespace geom { class NametagManager; }   // RendDX9's - a friend below

namespace world {

class SoldierTemplate;
class ICameraObject;


class Soldier : public Bundle,
                public IPlayerControlObject,
                public IItemContainerObject,
                public ISoldier
{
public:
    explicit Soldier(const SoldierTemplate* objectTemplate);
    virtual ~Soldier();

    // the quartet, its OWN refcount at +0x148 (not Object's +0x84)
    virtual void  addRef();
    virtual int   getRef();
    virtual int   release();
    virtual void* queryInterface(unsigned int) const;// five answers

    // the seventeen overrides of inherited slots, in slot order
    virtual void init();                                       // +0x020
    virtual void disableCachedObject();                        // +0x024
    virtual void enableCachedObject();                         // +0x028
    virtual void destroy(bool force);                          // +0x034
    virtual void handleVisualUpdate(float, float);             // +0x0A8
    virtual void handleFrameUpdate(float); // +0x0AC
    virtual void handleUpdate(float, unsigned int);            // +0x0B0
    virtual bool handleCollision(IObject*, const Vec3&, const Vec3&, const Vec3&,
                                 int, int, CollisionFaceInfo*, CollisionFaceInfo*);
    virtual void removeChild(IObject* child);                  // +0x0F8
    // Processes look, movement, stance and weapon input using a local copy of the supplied input.
    // Requires initialized camera, armor and movement components on the active paths.
    virtual void handlePlayerInput(IPlayer*, const io::PlayerInput&, float, unsigned int);
    // Disabled soldiers process only death and revive messages.
    virtual void handleMessage(TemplateMessage, IPlayer*, bool);
                                                               // +0x104
    virtual void initNetworkableComp();                        // +0x178

    // Interface tags: [P] controlled object, [I] item container, [S] soldier.
    virtual IPlayer* getOccupingPlayer() const;      // +0x180 [P][S] dice's spelling
    virtual bool isOccupied() const;                 // +0x184 [P]
    // the whole occupancy handshake. See the .cpp.
    virtual bool enter(IPlayer* player);             // +0x188 [P]
    virtual void exit();                             // +0x18C [P]
    virtual void exitPlayer();                       // +0x190 [P]
    virtual void onEnter(IObject* entering);         // +0x194 [P]
    virtual void onExit();                           // +0x198 [P]
    virtual int  getPcoId() const;                   // +0x19C [P] constant 0
    virtual void setPcoId(int);                      // +0x1A0 [P] empty
    virtual std::map<int, ICameraObject*>* getCameras();        // +0x1A4 [P]
    virtual std::map<int, ICameraObject*>* getOwnCameras();     // +0x1A8 [P] the same map
    virtual void nextCamera();                       // +0x1AC [P] empty
    virtual ICameraObject* getActiveCamera();        // +0x1B0 [P]
    virtual void setActiveCamera(int);               // +0x1B4 [P] empty
    virtual std::map<int, IPlayerControlObject*>* getPcos();    // +0x1B8 [P]
    virtual const std::vector<int>* getPcoPosId();   // +0x1BC [P] PE 0x552430
    virtual void addWeapon(IObject* weapon);         // +0x1C0 [P]
    virtual void* getWeapons();                      // +0x1C4 [P]
    virtual void setActiveWeapon(IObject*, int);     // +0x1C8 [P]
    virtual IObject* getActiveWeapon(int) const;     // +0x1CC [P][S]
    virtual void* getWeapons(int);                   // +0x1D0 [P]
    virtual IObject* getUsedVehicle() const;         // +0x1D4 [S]
    virtual int  getActiveItemIndex();               // +0x1D8 [S]
    virtual void toggleOnWeaponLock(IObject*);       // +0x1DC [P] empty
    virtual int  getSubPos();                        // +0x1E0 constant 0
    virtual int  getSonarPos();                      // +0x1E4 constant 0
    virtual int  getArtPos();                        // +0x1E8 [P] constant 0
    virtual void setTeam(int team);                  // +0x1EC [P]
    virtual int  getTeam();                          // +0x1F0 [P]
    virtual void setDefaultTeam(int);                // +0x1F4 [P] empty
    virtual void addPlayerInputObject(IObject*);     // +0x1F8 [P] PE 0x5520E0
    virtual void removePlayerInputObject(IObject*);  // +0x1FC [P] PE 0x5503D0
    virtual void setTimeToLiveUnused(float);         // +0x200 [P] empty
    virtual void setObjectSpawnerId(int);            // +0x204 [P] empty
    virtual int  getObjectSpawnerId() const;         // +0x208 [P] constant -1
    virtual void setObjectSpawnerHolding(bool);      // +0x20C [P] empty
    virtual bool getObjectSpawnerHolding() const;    // +0x210 [P] constant false
    virtual const Vec3& getNameTagOffset();          // +0x214 [P][S] from the template
    virtual const Vec3& getRidingWithOffset();       // +0x218 [P] from the template
    virtual float getRidingWithDistMod();            // +0x21C [P] from the template
    virtual void addVehicleMod(VehicleMod);          // +0x220 [P] empty
    virtual void removeVehicleMod(VehicleMod);       // +0x224 [P] empty
    virtual int  getVehicleMod(VehicleMod);          // +0x228 [P] constant 0
    virtual const std::string& getSoundFilter() const;          // +0x22C [P] from the template
    virtual float getShakeFactor() const;            // +0x230 [P] constant 1.0f
    virtual bool getIsSprinting() const;             // +0x234 [P]
    virtual void setIsSprinting(bool);               // +0x238 [P]
    virtual float getSprintStamina() const;          // +0x23C [P]
    virtual void setSprintStamina(float);            // +0x240 [P] > SprintState
    virtual void setSprintStaminaDissipationFactor(float);      // +0x244 [P]
    virtual float getSprintFactor();                 // +0x248 [P] constant 0.0f
    virtual void increaseSoldierCameraTransformation(float, float);  // +0x24C [S]
    virtual int  getAmmoStatus();                    // +0x250 [P]
    virtual void setAmmoStatus(int);                 // +0x254 [P]
    virtual bool addItem(IObject* item, int index);  // +0x258 [I][S] [FRONTIER]
    virtual IObject* getItem(int index);             // +0x25C [I][S]
    virtual IObject* removeItem(int index);          // +0x260 [I][S] (IObject*)
    virtual IObject* removeItem(IObject* item);      // +0x264 [I][S]
    virtual int  getItemContainerType() const;       // +0x268 [I] from the template
    virtual void handlePickupOnItem(IPlayer*, IObject*);        // +0x26C [I]
    virtual void handleDropOnItem(IPlayer*, IObject*);          // +0x270 [I]
    virtual bool testIfValidTransformation(SoldierPose, const Mat4&, bool);  // +0x274 [S]
    virtual SoldierPose getPose() const;             // +0x278 [S]
    virtual SoldierPose getRequestedPose() const;    // +0x27C [S]
    virtual void setRequestedPose(SoldierPose);      // +0x280
    virtual void executePoseChange();                // +0x284 [S]
    virtual bool getIsNewPoseRequested() const;      // +0x288 [S]
    virtual const Vec3& getPoseCameraPosition(int) const;       // +0x28C
    virtual bool enableItem(char index);             // +0x290 [S] (bool)
    virtual void disableItem(char index);            // +0x294 [S] [FRONTIER]
    virtual bool isNotFireingOrHaveRecoil();         // +0x298 [S] constant true
    virtual int addItem(const std::string&, int);    // +0x29C [S] 81AD680 / PE 552370: index / -1
    virtual Kit* getKit();                           // +0x2A0 [S]
    virtual int  getKitType() const;                 // +0x2A4 [S]
    virtual int  getLastContactMaterial() const;     // +0x2A8 [S]
    virtual void bulletFlyBy(IObject*, const Vec3&, const Vec3&);    // +0x2AC [S]
    virtual bool getIsUnderwater() const;            // +0x2B0 [P] constant false
    virtual bool getIsSwimming() const;              // +0x2B4
    virtual float getNormalizedUnderwaterTime() const;          // +0x2B8 [P] constant 1.0f
    virtual float getSoldierRadius() const;          // +0x2BC [S] , coll-soldier-radius = .25
    virtual float getStandingHeight() const;         // +0x2C0 [S] , coll-soldier-stand-height = 1.7
    virtual Vec3 getLowestVisiblePosition() const;              // +0x2C4 [S] SoldierAnimation.cpp (by value)
    virtual void setLadderId(unsigned int);          // +0x2C8 [S]
    virtual unsigned int getLadderId() const;        // +0x2CC [S]
    virtual void setZiplineId(unsigned int);         // +0x2D0 [S]
    virtual unsigned int getZiplineId() const;       // +0x2D4 [S]
    virtual void setGrapplingHookId(unsigned int);   // +0x2D8 [S]
    virtual unsigned int getGrapplingHookId() const; // +0x2DC [S]
    virtual void setReadyToClimbTime(float);         // +0x2E0 [S]
    virtual bool applyCameraAnimation(const Mat4&, Mat4&);      // / client PE 0x61CB60
    virtual bool getIsSkyDiving() const;             // +0x2E8 [S]
    virtual void dropWeaponFx(bool, Vec3&, int);     // +0x2EC [S] empty
    virtual float getCurrentAbility() const;         // +0x2F0 [S]
    virtual void setCurrentAbility(float);           // +0x2F4 [S]
    virtual bool taxAbility(float);                  // +0x2F8 [S]
    virtual float getFireDelayAfterJump() const;     // +0x2FC [S]
    virtual bool getCanFireWeapon() const;           // +0x300 [S]

    // Resets instance state; aborts if the soldier or its used vehicle remains occupied.
    void  resetInstance();
    // TWO complete rigs - a first-person and a third-person skeleton
    // and animation system - and the third-person skeleton is the collision skeleton.
    void  initAnimation();
    void  releaseAnimation();
    void  resetHeadMovements();
    void  disableInverseKinematic(bool);
    void  updateCameraTransformation();
    void  updateWeaponIsActiveState();
    void  updateTransformation();
    void  setLookUpDownAngle(float);
    void  setLookLeftRightAngle(float);
    ICameraObject* getSoldierCamera();
    const Mat4& getMovementMatrix();
    bool  hasHandWeaponActive();
    void  setIsSkyDiving(bool);
    IObject* getPreviousWeapon() const;
    IObject* getWeaponFromIndex(int) const;
    void  sendMessageToItem(int, TemplateMessage, IPlayer*, bool);
    void  setHitByVehicleSpeed(const Vec3&);
    void  applyExplosionForce(const Vec3&);                   // SoldierAnimation.cpp
    void  wakeUpRagdoll();                                    // SoldierAnimation.cpp
    void  dropHelmetFx();
    void  stopDrowningSound();
    void  triggerFootStepEffect(int, int);                    // empty
    void  updateHoldBreathUnderwaterTime(float);              // empty
    void  removeFromLadder();                                 // empty
    // Handles message 6 using the occupant's fire input; alternate fire rearms its latch.
    void  handleManDownActions(TemplateMessage, IPlayer*);

    float getYawRecoil();
    float getPitchRecoil();
    void  setCharacterMode(int mode);
    float getSoldierSpeed(int speedState) const;
    void  resetWeaponAnimations();
    // returns bool, not void - the ELF materialises `mov eax, 1` after the
    // stack adjustment on the single exit. It is always true: every failure path aborts instead.
    bool  pickupKit(IPlayer*, IObject*);
    // Returns false without a kit or when the dropped kit hits a ladder; true otherwise.
    bool  dropKit();
    void  updateSoldierPose(bool prone, bool crouch);
    void  updateSpeedState();
    void  updateSoldierSpeed(bool inAir, float forwardInput, const Vec3& forwardAxis,
                             float strafeInput, const Vec3& strafeAxis, float dt);
    void  updateSoldierSpawnPoint(SoldierPose);
    bool  soldierCanGoProne();
    // Both shipped callers consume ST0 / XMM0; the old void declaration was ABI-wrong.
    float soldierInAirAction(const Vec3&);                    // SoldierAnimation.cpp
    float getMouseSpeedMod();
    // returns int, not void - handleUpdate does `m_ammoStatus =
    // calcAmmoStatus()` and the ELF materialises the value on both exits. The declaration was wrong.
    int   calcAmmoStatus();
    // THE name IS A LIE - see the .cpp. It answers "is the active weapon ZOOMED".
    bool  isWalking() const;
    void  stopTinnitus();
    void  startManDownEffect();
    void  stopManDownEffect();
    void  removeWeapon(IObject*);
    // the voice-over channel, and it is stop-and-replace: `say(0, 0)` is
    // how a soldier is silenced. See the .cpp.
    void  say(ISound* sound, int priority);
    // Nested type is attested by
    // `_ZN4dice3hfe5world7Soldier23getSkeletonRayTestPointENS2_20SkeletonRayTestPointE`.
    // Enumerator spellings are [INFERRED]; the values and their bone mapping are recovered.
    enum SkeletonRayTestPoint
    {
        SkeletonRayTestPointHead  = 0,                         // collision-skeleton bone 47
        SkeletonRayTestPointTorso = 1,                         // collision-skeleton bone 11
        SkeletonRayTestPointFeet  = 2                          // collision-skeleton bone 3
    };

    anim::Skeleton* getCollisionSkeleton(int);
    Vec3 getSkeletonRayTestPoint(SkeletonRayTestPoint);
    void  enableRagDoll();                                    // SoldierAnimation.cpp
    bool  hasRagdollLanded() const;
    void  triggerDieAnimation();                              // SoldierAnimation.cpp
    void  playDamageAnimations();
    void  reviveAnimations();
    void  updateThirdPersonAnimations(float, bool);           // SoldierAnimation.cpp
    void  updateFirstPersonAnimations(float);                 // / client PE 0x61D8A0, SoldierAnimation.cpp
    void  updateAnimationSystems(float);                      // SoldierAnimation.cpp
    void  applyAnimationsOnSkeleton(float, bool);             // SoldierAnimation.cpp
    void  getPlayingAnimationInfo(float&, float&);
    // Finds and caches the ridden vehicle's IK attachment, returning its template component or null.
    // Requires a live used vehicle. The Kinamatic spelling is part of the original symbol name.
    IObjectTemplateComponent* findInverseKinamaticObject();

    // [CHOSEN, not recovered] Script overrides of the sprint times enter() copies from the
    // template, for Python's soldier.setStaminaDissipation/setStaminaRecovery.
    void setSprintDissipationTime(float seconds) { m_sprintState.m_constant0 = seconds; }
    void setSprintRecoverTime(float seconds)     { m_sprintState.m_constant1 = seconds; }

private:
    // Fields without identified roles retain offset-based names.
    /* +0x148 */ int    m_refCount;                  // ctor 1 - ITS OWN, not Object's +0x84
    /* +0x14C */ const SoldierTemplate* m_soldierTemplate;      // the ctor argument, typed
    friend class ::dice::hfe::Player;                // gas-mask sprint factor, setIsGasMaskOn
    friend class ::dice::hfe::ServerGameLogic;       // drop retrieval reads m_readyToClimbTime
    // Shared retain-before-release update for the occupant's weak control block.
    void setOccupyingPlayerWeak(void* block);

    /* +0x150 */ void*  m_occupyingPlayerWeak;       // 0 - a weak block; getOccupingPlayer drops it
    /* +0x154 */ std::map<int, IPlayerControlObject*> m_playerControlObjects;   // ctor inserts this
    /* +0x160 */ std::vector<IObject*> m_weaponVec;             // getWeapons() hands out its address
    /* +0x16C */ std::vector<IObject*> m_weapons[3];            // getWeapons(i) - 3 x 12 B
    /* +0x190 */ IObject* m_activeWeapons[3];        // getActiveWeapon(i); the ctor's last loop
    /* +0x19C */ IObject* m_weapon1;                 // getPreviousWeapon's first choice
    /* +0x1A0 */ IObject* m_weapon2;                 // ... and its fallback
    /* +0x1A4 */ std::map<int, ICameraObject*> m_ownCameras;    // getCameras AND getOwnCameras
    /* +0x1B0 */ bool   m_isOccupied;
    /* +0x1B4 */ int    m_team;
    /* +0x1B8 */ int    m_activeItemIndex;           // -1 means "none" - setIsSkyDiving tests it
    /* +0x1BC */ std::list<IObject*> m_playerInputs;            // addPlayerInputObject's list
    /* +0x1C0 */ Kit*   m_kit;
    // Temporary pickup context, set while Kit::handlePickup adds the kit's items.
    /* +0x1C4 */ IObject* m_kitBeingPickedUp;
    // Sized from the template and initially null; getItem excludes slot 0.
    /* +0x1C8 */ std::vector<IObject*> m_itemObjects;
    /* +0x1D4 */ void*  m_usedVehicleWeak;           // a weak block, dropped like the player's
    // Refcounted block released during reset; its target remains [UNRESOLVED].
    /* +0x1D8 */ void*  m_refBlock1D8;
    /* +0x1DC */ Vec3   m_hitByVehicleSpeed;         // setHitByVehicleSpeed's three floats
    /* +0x1E8 */ unsigned int m_hitByVehicleSpeedTicks;   // = getTickCount() + 16 a 16-tick window
    /* +0x1EC */ int    m_dword1EC;                  // [UNRESOLVED]
    // by enter(): the soldier's facing angle IN degrees, derived from the
    // forward axis of its absolute transformation at the moment a player enters it.
    /* +0x1F0 */ float  m_facingDegrees;             // [CHOSEN] name
    // Physics disabling clears both smoothed movement axes.
    friend void disablePhysicsOnSoldier(IObject* object);
    // Replication accesses the serialized fields directly.
    friend class dice::hfe::SoldierNetworkable;
    // ZiplineTemplate::checkForEntry's very first gate is a RAW read of m_byte399 (0x81D2270:
    // `cmp byte ptr [eax+399h], 0`) with no accessor call in either port - a non-zero latch
    // abandons the whole zipline search. Same argument as the two friend lines above.
    friend class ZiplineTemplate;
    // Parachute::detachSoldier raises +0x27F immediately before enableRagDoll, by raw field access
    // in both ports (ELF 0x81955D0 / PE 0x599DA0). Narrow friend, no new public setter.
    friend class Parachute;
    // PlayerControlObject::internalExit writes the soldier's raw +0x1D8 weak block and +0x27F
    // latch in both ports (ELF 0x81994F0 / PE 0x598810). Same narrow access treatment.
    friend class PlayerControlObject;
    // Ladder::detachSoldier substitutes the pending-turn accumulator into the active camera's
    // script-facing X rotation (ELF 0x8188BFE / PE 0x59C6xx), by raw field access in both ports.
    friend class Ladder;
    // GrapplingHookRope::checkForEntry reads the facing angle and ready-to-climb timer directly
    // in both ports (ELF 0x817AA0A/+0x817AAB3; PE 0x5784AF/+0x578563).
    friend class GrapplingHookRope;
    // GrapplingHookRopeContainer::handlePlayerInput reads the raw pitch to reverse its clamped
    // climb input (ELF 0x817E888, Soldier +0x204; PE 0x577CD3, Soldier +0x238).
    friend class GrapplingHookRopeContainer;
    // GenericFireArm::checkAreaToFireZipline reads the raw collision latch at +0x398 (PE +0x3D0)
    // before its terrain-clearance test (ELF 0x8254940 / PE 0x5847C0). Narrow friend, no getter.
    friend class GenericFireArm;
    // nt_update: the nametag HUD hint's ammo test reads m_weapon2 (the kit's default
    // weapon) - RendDX9 1.50 NametagManager::updateHudTarget, `mov ecx,[eax+74h]` at 0x1012D4B3.
    friend class ::dice::hfe::geom::NametagManager;
    // ZiplineContainer::handleUpdate raises the raw +0x399 latch immediately after a PIUse detach
    // in both ports (ELF 0x81D3430 / PE 0x57E4C0). Narrow friend, no setter.
    friend class ZiplineContainer;
    /* +0x1F4 */ float  m_smoothedStrafe;            // [CHOSEN] name
    /* +0x1F8 */ float  m_smoothedForward;           // [CHOSEN] name
    /* +0x1FC */ int    m_pose;                      // SoldierPose
    /* +0x200 */ int    m_requestedPose;             // getIsNewPoseRequested = pose != this
    /* +0x204 */ float  m_lookUpDownAngle;
    /* +0x208 */ float  m_cameraPitch;               // [CHOSEN] name - increaseSoldierCamera... adds
    /* +0x20C */ float  m_lookLeftRightAngle;
    /* +0x210 */ float  m_cameraYaw;                 // [CHOSEN] name - ... to these two
    // Pending turn paid out at a capped rate; original name remains [UNRESOLVED].
    /* +0x214 */ float  m_float214;                  // the pending-turn accumulator
    // [CHOSEN] name. Held pose-input bits: crouch 0, prone 1; transitions are edge-triggered.
    /* +0x218 */ int    m_poseInputLatch;
    // by say(): the soldier's ONE voice-over sound. It is a channel, not a
    // list - a soldier says one thing at a time, and starting a new line stops the old one.
    /* +0x21C */ ISound* m_voiceSound;               // ctor 0
    /* +0x220 */ anim::RagDoll* m_ragDoll;           // NON-NULL gates OUT the look angles
    /* +0x224 */ int    m_dword224;                  // ctor 0
    // Countdown to re-enabling the active item.
    /* +0x228 */ float  m_timeToEnableWeapon;        // IDB name g_soldierTimeToEnableWeapon
    // Counts down until the helmet-drop effect.
    /* +0x22C */ float  m_helmetTime;                // [CHOSEN] name
    /* +0x230 */ float  m_float230;                  // [UNRESOLVED]
    /* +0x234 */ Vec3   m_headPosition;              // resetHeadMovements seeds (1, 0, 0)
    /* +0x240 */ float  m_soldierLookIdleTime;
    /* +0x244 */ Vec3   m_explosionForce;            // applyExplosionForce's three floats, ctor 0
    // Counts down by dt, clearing the explosion force when it expires.
    /* +0x250 */ float  m_explosionForceTime;        // [CHOSEN] name
    /* +0x254 */ void*  m_animSkeleton;              // the first-person skeleton (skeleton1P)
    // [CHOSEN] names for owned animation-system instances.
    /* +0x258 */ anim::IAnimationSystem* m_animationSystem1P;
    // THE third-person skeleton, and hit detection runs against it - initAnimation copies
    // the template's skeleton3P here.
    /* +0x25C */ anim::Skeleton* m_collisionSkeleton;
    // +0x260 is the stored 3P instance pointer. Both shipped ports load this dword before
    // dispatching clearTriggerMessage; taking its address treats Soldier storage as the object and
    // crashes. The following four dwords are independent, still-unresolved Soldier state.
    /* +0x260 */ anim::IAnimationSystem* m_animationSystem3P;
    // Vehicle IK target and its component, cleared by construction and reset.
    /* +0x264 */ IObject*                  m_ikObject;
    /* +0x268 */ IObjectTemplateComponent* m_ikTemplateComponent;
    /* +0x26C */ int    m_dword26C;                  // [UNRESOLVED]
    /* +0x270 */ int    m_dword270;                  // resetInstance seeds 4
    // [UNRESOLVED] names. Jump timers for steep-contact steering and airborne speed.
    /* +0x274 */ float  m_float274;
    /* +0x278 */ float  m_float278;
    /* +0x27C */ bool   m_byte27C;                   // [UNRESOLVED]
    /* +0x27D */ bool   m_byte27D;                   // [UNRESOLVED]
    /* +0x27E */ bool   m_inverseKinematicDisabled;  // disableInverseKinematic's latch
    /* +0x27F */ bool   m_byte27F[5];                // [UNRESOLVED] +0x27F..+0x283
    /* +0x284 */ int    m_characterMode;             // setCharacterMode; a change re-skins the anim
    // Camera spring displacement and current-frame yaw/pitch deltas, including recoil.
    /* +0x288 */ Vec3   m_cameraSpringDisplacement;  // [INFERRED] name, unchanged 12-byte storage
    /* +0x294 */ float  m_yawDeltaThisTick;          // [CHOSEN] name
    /* +0x298 */ float  m_pitchDeltaThisTick;        // [CHOSEN] name
    /* +0x29C */ int    m_dword29C;                  // [UNRESOLVED]
    /* +0x2A0 */ bool   m_isSwimming;                // setLadderId clears it
    // Speed-table index: prone, crouch, walk, run, sprint, swim, swim-sprint (0..6).
    /* +0x2A4 */ int    m_soldierSpeedIndex;         // getSoldierSpeed's default index
    // [CHOSEN] names for pose-transition progress and duration.
    /* +0x2A8 */ float  m_poseChangeProgress;        // zeroed on every pose change
    // Duration from the 7x7 old/new speed-index transition table.
    /* +0x2AC */ float  m_poseChangeDuration;        // [CHOSEN] name
    /* +0x2B0 */ float  m_soldierSpeed;
    // by handleMessage: raised on TM 24 (die) and cleared on TM 25
    // (revive). Only the low byte is ever written. [CHOSEN] name.
    /* +0x2B4 */ int    m_isManDown;
    /* +0x2B8 */ SoldierSound* m_soldierSound;       // ctor: operator new(0x38)
    /* +0x2BC */ bool   m_isSkyDiving;
    /* +0x2C0 */ int    m_lastContactMaterial;
    // [CHOSEN] name. Prevents duplicate medic calls until revive clears it.
    /* +0x2C4 */ int    m_medicCalled;
    /* +0x2C8 */ float    m_float2C8;                  // triggerDieAnimation stamps -1.0e9 (0xCE6E6B28)
                                                     // on a ragdoll-less death. [UNRESOLVED] reader
    /* +0x2CC */ float  m_currentAbility;            // the kit's consumable meter - taxAbility
    // Two object-grid ids parked until revival. reviveAnimations resolves each in this order,
    // calls destroy(true) when it still exists, then writes -1. Their higher-level roles remain
    // [UNRESOLVED].
    /* +0x2D0 */ int    m_dword2D0;
    /* +0x2D4 */ int    m_dword2D4;
    /* +0x2D8 */ int    m_dword2D8;                  // ctor 0
    /* +0x2DC */ int    m_dword2DC;                  // ctor 0
    /* +0x2E0 */ int    m_dword2E0;                  // ctor 0
    /* +0x2E4 */ unsigned int m_ladderId;
    /* +0x2E8 */ unsigned int m_ziplineId;           // ctor 0
    /* +0x2EC */ unsigned int m_grapplingHookId;     // ctor 0
    /* +0x2F0 */ float  m_readyToClimbTime;          // ctor 0
    // Two fire-input latches followed by the embedded sprint state.
    /* +0x2F4 */ FireButtonState m_fireButtonState[2];   // +0x2F4 +0x2F8, ctor-constructed
    /* +0x2FC */ SprintState m_sprintState;              // subsumes +0x2FC..+0x313
    // by handlePlayerInput: a ONE-shot third-scaling of the forward axis.
    // While set, forward input is divided by three and the flag is cleared - one tick, then gone.
    /* +0x314 */ bool   m_thirdSpeedOnce;                // [CHOSEN] name - not ctor-written
    /* +0x315 */ bool   m_byte315;                       // ctor 0 - Soldier's own byte
    // Ragdoll camera position, negated ZXY angles, and yaw-rotated offset.
    // [UNRESOLVED] original field names.
    /* +0x318 */ float    m_float318;                  // ctor 0
    /* +0x31C */ float    m_float31C[2];               // ctor 0
    /* +0x324 */ float    m_float324;                  // ctor 0
    /* +0x328 */ float    m_float328[2];               // ctor 0
    /* +0x330 */ float  m_float330;                  // ctor 0
    /* +0x334 */ float    m_float334[2];               // ctor 0
    /* +0x33C */ Mat4   m_mat33C;                    // [UNRESOLVED] purpose; ctor identity
    /* +0x37C */ bool   m_byte37C;                   // resetInstance 0; reviveAnimations 1
    // Gas-mask/night-vision deployment state and remaining animation time. [CHOSEN] Member names.
    /* +0x37D */ bool   m_deployAnimationActive;     // resetInstance 0
    /* +0x380 */ float  m_deployAnimationTimer;      // resetInstance 0
    /* +0x384 */ bool   m_byte384;                   // resetInstance 0
    /* +0x385 */ bool   m_byte385;                   // resetInstance 0
    /* +0x388 */ int    m_dword388;                  // [UNRESOLVED]
    // [CHOSEN] stowed-state latch; weapon activation changes only on transitions.
    /* +0x38C */ bool   m_weaponStowed;              // ctor 0
    /* +0x38D */ bool   m_byte38D;                   // ctor 0
    /* +0x390 */ int    m_ammoStatus;
    // Skips input processing while positive, counting down by dt.
    /* +0x394 */ float  m_inputSuppressTimer;        // [CHOSEN] name
    /* +0x398 */ bool   m_byte398;                   // ctor 0
    /* +0x399 */ bool   m_byte399;                   // ctor 0
    /* +0x39C */ float  m_fireDelayAfterJump;        // getCanFireWeapon's first test, ctor 0
    // Pose and jump delays, including the sprint recharge delay.
    // Positive timers are decremented without clamping at zero.
    /* +0x3A0 */ float  m_poseTimers[5];             // ctor 0, +0x3A0..+0x3B0
    /* +0x3B4 */ float  m_float3B4;                  // getCanFireWeapon's second test, ctor 0
};

// Sets geometry FOV recursively; once excludes sibling traversal.
void hackSetFov(IObject* object, float fov, bool once);

} // namespace world
} // namespace hfe
} // namespace dice

#endif // GAME_OBJECTS_GAME_SOLDIER_H
