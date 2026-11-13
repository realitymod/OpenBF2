// Supply-crate lifecycle, landing detection, and servicing.

#include "SupplyObject.h"

#include <cfloat>
#include <cmath>
#include <map>
#include <vector>

#include "../../../Core/Ids.h"
#include "../../../Core/Debug.h"
#include "../../../Core/StringUtil.h"            // formatString - getArmorInfo, and the Debug arm
#include "../../IGameLogic.h"                    // gameLogic - isServer (+0x13C, attested by use)
#include "SupplyDropManager.h"                   // the landing arms' manager
#include "../../../Scene/Object/ObjectManager.h" // objectManager - postMessage (+0xD0) /
                                                 // removeAllMessagesTo (+0xD4)
#include "../../../Scene/Object/ObjectTypes/Game/IPlayerControlObject.h"
#include "../../../Scene/Object/ObjectUtil.h"    // setNetworkableComp
#include "../../../Scene/Object/PlayerManager.h" // playerManager - workOnSoldiers' player walk
#include "../IControlPoint.h"
#include "../IPlayer.h"                          // getIsAlive / getDefaultVehicle / getTeam
#include "../IWeaponObject.h"
#include "../../IGame.h"                         // game->destroyObject (+0xA0) - the empty-crate arm
#include "../GameLogicComp/IArmorGLComp.h"       // isDestroyed (slot 21) - update's opening call

namespace dice {
namespace hfe {
namespace world {

// The supply-drop manager is a real class now - Game/Objects/Game/SupplyDropManager.h. Its TU-level
// initializer publishes world::supplyDropManager (ds:0x8AADAAC) before these landing arms run.

// Initializes team and landing state. Only servers seed supply gauges and enable update ticks.
SupplyObject::SupplyObject(const SupplyObjectTemplate* objectTemplate)
    : Bundle(objectTemplate)
{
    m_template = objectTemplate;                 // +0x13C
    m_team = objectTemplate->team;               // +0x140 <- template +0x180
    m_suppliesLanded = false;                    // +0x150 (byte store)

    if (gameLogic->isServer())
    {
        m_healStorage   = m_template->healStorageSize;    // +0x144 <- template +0x198
        m_ammoStorage   = m_template->ammoStorageSize;    // +0x148 <- template +0x19C
        m_sharedStorage = m_template->sharedStorageSize;  // +0x14C <- template +0x1A0
        Object::updateFlags(0x800, 0);
    }
}

// /0x81BDF30/0x81BDF60 - vptr restores + Bundle::~Bundle only. Nothing of its own dies.
SupplyObject::~SupplyObject() {}

// init - 0x81BDFA0, 66 B. Bundle::init, then the heartbeat is armed: message 29 to self in 1.0 s.
// handleMessage answers 29 with update(0.2f) and RE-arms at 0.2 s - so a live crate ticks at 5 Hz
// after a one-second grace. The message id is a TemplateMessage value; the enum is [UNRESOLVED]
// (IObjectComponent.h), so the cast is explicit. 29 = [CHOSEN name pending] the supply tick.
// During precache the object is forceDestroy()ed immediately and destroy's removeAllMessagesTo
// clears the pending message - the heartbeat never fires on a throwaway.
void SupplyObject::init()
{
    Bundle::init();
    objectManager->postMessage(this, static_cast<TemplateMessage>(29), 1.0f);
}

// straight through. Bundle does not override disableCachedObject, so this is the
// ordinary next-override call, stated so nobody "fixes" it into Bundle::.
void SupplyObject::disableCachedObject()
{
    SimpleObject::disableCachedObject();
}

// . Reset the gauges, wake the bundle, re-arm the heartbeat - the cached-object
// pool's "this crate is live again" path.
void SupplyObject::enableCachedObject()
{
    reset();
    Bundle::enableCachedObject();
    objectManager->postMessage(this, static_cast<TemplateMessage>(29), 1.0f);
}

// Cancels queued heartbeat messages before base destruction.
void SupplyObject::destroy(bool force)
{
    objectManager->removeAllMessagesTo(this);
    SimpleObject::destroy(force);
}

// reset - 0x81BE160, 97 B. NON-virtual. Re-copies everything the ctor's server arm copies - but
// ungated (no isServer test; the one caller in this class is enableCachedObject), and the flag
// write goes through the vtable this time where the ctor called Object::updateFlags directly.
void SupplyObject::reset()
{
    m_suppliesLanded = false;
    m_team          = m_template->team;
    m_healStorage   = m_template->healStorageSize;
    m_ammoStorage   = m_template->ammoStorageSize;
    m_sharedStorage = m_template->sharedStorageSize;
    updateFlags(0x800, 0);                       // vcall slot 24, as shipped
}

// Creates the simple-object network component when networkableInfo is configured.
// Reports a fatal error if the component cannot be installed.
void SupplyObject::initNetworkableComp()
{
    if (m_template && !m_template->getNetworkableInfo().empty()
        && !setNetworkableComp(this, CID_SimpleObjectNetworkable))
    {
        Debug(DEBUGTYPE_6, "Game/Objects/Game/SupplyObject.cpp", 72, __FUNCTION__)
            << formatString("Failed to create SimpleObjectNetworkable for SupplyObject");
    }
}

// Runs the server supply tick: selects team, expires exhausted crates, serves soldiers then vehicles,
// and regenerates heal/ammo storage. Shared fallback storage is never regenerated.
void SupplyObject::update(float dt)
{
    // 1. The nearest armor component up the parent chain - and the answer is discarded. See above.
    IObjectComponent* armor = 0;
    for (IObject* object = this; object; object = object->m_parent)
    {
        armor = object->m_armorComponent;                     // Object +0x3C
        if (armor)
            break;
    }
    if (armor)
        static_cast<IArmorGLComp*>(armor)->isDestroyed();     // slot 21 result unused, as shipped

    // 2. Server only.
    if (!gameLogic->isServer())
        return;

    // 3. Crates that take their side from the ground they stand on.
    if (m_template->teamFromClosestCP)                        // template +0x1BC, ctor false
        m_team = updateTeamFromClosestCP();

    // 4. Nothing left to give - destroy the whole tree and stop. The test is "every ability this
    // crate HAS is exhausted": an ability it does not have counts as exhausted, so a heal-only
    // crate dies on an empty heal pool alone.
    if (m_template->destroyWhenEmptyStorage)                  // template +0x1AC, ctor false
    {
        if (hasHealAbility() || hasAmmoAbility())
        {
            bool healExhausted = true;
            if (hasHealAbility())
                healExhausted = hasEmptyHealStorage();

            const bool ammoExhausted = hasAmmoAbility() ? hasEmptyAmmoStorage() : true;

            if (ammoExhausted && healExhausted)
            {
                game->destroyObject(getRootParent(this));     // IGame +0xA0
                return;
            }
        }
    }

    // 5. The work scans - soldiers first, then vehicles - and the regeneration.
    if (m_template->workOnSoldiers)                           // template +0x185, ctor true
        workOnSoldiers();
    if (m_template->workOnVehicles)                           // template +0x184, ctor false
        workOnVehicles();

    if (m_template->restoreSpeed > 0.0f)                      // template +0x194
    {
        const float slice = dt / 20.0f;
        m_healStorage += m_template->restoreSpeed * slice * m_template->healStorageSize;
        m_ammoStorage += slice * m_template->restoreSpeed * m_template->ammoStorageSize;
        // m_sharedStorage is deliberately absent - see the header block.
    }
}

// Heals and reloads living soldiers on foot or within this crate's object hierarchy.
// Requires a matching team unless the crate is neutral.
void SupplyObject::workOnSoldiers()
{
    const std::list<IPlayer*>& players = playerManager->getPlayers();   // PlayerManager +0x3C

    for (std::list<IPlayer*>::const_iterator it = players.begin(); it != players.end(); ++it)
    {
        IPlayer* player = *it;

        if (!player->getIsAlive())                                      // +0x6C
            continue;

        IObject* soldier = player->getDefaultVehicle();                 // +0xA4
        if (!soldier)
            continue;

        // Loose, or riding inside this crate's own tree.
        if (soldier->getParent() && getRootParent(soldier) != getRootParent(this))
            continue;

        const int team = player->getTeam();                             // +0xE8
        if (team == m_team || m_team == 0)
            healAndReload(soldier);
    }
}

// reloadAmmo - 0x81BE250, 362 B. Find the nearest player-control interface, then visit both weapon
// banks of every seat in its PCO map. A parented seat is skipped only while its child soldier is
// active. The void* getWeapons(int) return is the known vector seam tracked by
// [RETIRE-WHEN: pco-getweapons-vector]; the caller evidence fixes the cast here.
void SupplyObject::reloadAmmo(IObject* object)
{
    IPlayerControlObject* playerControlObject = 0;
    for (IObject* current = object; current; current = current->getParent())
    {
        playerControlObject = static_cast<IPlayerControlObject*>(
            current->queryInterface(IID_IPlayerControlObject));
        if (playerControlObject)
            break;
    }
    if (!playerControlObject)
        return;

    std::map<int, IPlayerControlObject*>* const pcos = playerControlObject->getPcos();
    for (std::map<int, IPlayerControlObject*>::iterator pco = pcos->begin();
         pco != pcos->end(); ++pco)
    {
        IPlayerControlObject* const seat = pco->second;
        if (!seat)
            continue;

        IObject* const seatObject =
            static_cast<IObject*>(seat->queryInterface(IID_IObject));
        if (seatObject->getParent() && seat->isChildSoldierActive())
            continue;

        // [CHOSEN, not recovered] Project Reality: all three fire banks, so weapons on the third
        // (PIFlareFire, e.g. flares) are rearmed too; retail stopped after banks 0 and 1.
        for (int bank = 0; bank <= 2; ++bank)
        {
            std::vector<IObject*>* const weapons =
                static_cast<std::vector<IObject*>*>(seat->getWeapons(bank));
            for (std::vector<IObject*>::iterator weaponIt = weapons->begin();
                 weaponIt != weapons->end(); ++weaponIt)
            {
                IObject* const weaponObject = *weaponIt;
                if (!weaponObject)
                    continue;

                IWeaponObject* const weapon = static_cast<IWeaponObject*>(
                    weaponObject->queryInterface(IID_IWeaponObject));
                if (!weapon || hasEmptyAmmoStorage())
                    continue;

                const float amount = m_template->refillAmmoSpeed / 5.0f;
                const float unused = weapon->reloadAmmoPercent(amount, false);
                SubtractAmmo(amount - unused);
            }
        }
    }
}

// heal - 0x81BE3C0, 811 B. The damage arm deliberately runs before the heal arm, makes a dead
// getHitPoints call, and re-reads this object's absolute position for giveDamage. The normalisation
// is the engine's epsilon form. Do not fold these observable calls together.
void SupplyObject::heal(IObject* object)
{
    IArmorGLComp* const armor = static_cast<IArmorGLComp*>(object->getArmorComponent());
    if (!armor)
        return;

    armor->getHitPoints();                                      // result discarded, as shipped

    if (m_template->damageSpeed > 0.0f)
    {
        const Vec3& targetPosition = object->getAbsolutePosition();
        const Vec3& supplyPosition = getAbsolutePosition();

        Vec3 direction;
        direction.x = targetPosition.x - supplyPosition.x;
        direction.y = targetPosition.y - supplyPosition.y;
        direction.z = targetPosition.z - supplyPosition.z;

        const float lengthSquared = direction.x * direction.x
                                  + direction.y * direction.y
                                  + direction.z * direction.z;
        if (std::fabs(lengthSquared - 1.0f) >= FLT_EPSILON)
        {
            if (std::fabs(lengthSquared) >= FLT_EPSILON)
            {
                const float inverseLength = 1.0f / std::sqrt(lengthSquared);
                direction.x *= inverseLength;
                direction.y *= inverseLength;
                direction.z *= inverseLength;
            }
            else
            {
                direction.x = 0.0f;
                direction.y = 0.0f;
                direction.z = 0.0f;
            }
        }

        const Vec3 source = getAbsolutePosition();              // second call is shipped
        gameLogic->giveDamage(object, m_template->damageSpeed / 5.0f,
                              0, 0, 0, 0, 0, 0,
                              source, direction, 12, false, true, false);
    }

    if (hasHealAbility() && !hasEmptyHealStorage())
    {
        const float amount = m_template->healSpeed / 5.0f;
        if (gameLogic->heal(object, amount, 0, 73, 24, 0, false, false))
            SubtractHeal(m_template->healSpeed / 5.0f);          // recomputed as shipped
    }
}

// workOnVehicles - 0x81BE7B0, 258 B. The retail map is assumed to contain non-null roots with an
// IPlayerControlObject somewhere in their ancestry; its null result is intentionally not guarded.
// SupplyObjectTemplate::testVehicleType applies the typed PCO-template vehicle-category filter.
void SupplyObject::workOnVehicles()
{
    std::map<unsigned int, IObject*>* const objects = gameLogic->getPcoMap();

    for (std::map<unsigned int, IObject*>::iterator it = objects->begin();
         it != objects->end(); ++it)
    {
        IObject* const object = it->second;
        if (object->getParent())
            continue;

        IPlayerControlObject* playerControlObject = 0;
        for (IObject* current = object; current; current = current->getParent())
        {
            playerControlObject = static_cast<IPlayerControlObject*>(
                current->queryInterface(IID_IPlayerControlObject));
            if (playerControlObject)
                break;
        }

        const int team = playerControlObject->getTeam();         // no null guard in retail
        if ((team == m_team || m_team == 0)
            && object->getTemplate()->getClassID() != CID_SoldierTemplate
            && m_template->testVehicleType(object))
        {
            healAndReload(object);
        }
    }
}

// healAndReload - 0x81BE8C0, 220 B. Distance includes the target object's physics radius. Within
// range, ammo is replenished before health/damage is processed.
void SupplyObject::healAndReload(IObject* object)
{
    const Vec3& targetPosition = object->getAbsolutePosition();
    const Vec3& supplyPosition = getAbsolutePosition();
    const float dx = targetPosition.x - supplyPosition.x;
    const float dy = targetPosition.y - supplyPosition.y;
    const float dz = targetPosition.z - supplyPosition.z;
    const float distance = std::sqrt(dx * dx + dy * dy + dz * dz);

    const SupplyObjectTemplate* const objectTemplate = m_template;
    const float objectRadius = object->getPhysicBoundingRadius();
    if (distance <= objectRadius + objectTemplate->radius)
    {
        reloadAmmo(object);
        heal(object);
    }
}

// updateTeamFromClosestCP - 0x81BED40, 303 B. Strictly closer wins, so equal-distance points keep
// map iteration order. The control-point interface is assumed present exactly as in retail.
int SupplyObject::updateTeamFromClosestCP()
{
    int team = 0;
    float closestDistance = 9999.0f;
    std::map<unsigned int, IObject*>* const points = gameLogic->getControlPointMap();

    for (std::map<unsigned int, IObject*>::iterator it = points->begin();
         it != points->end(); ++it)
    {
        IObject* const object = it->second;
        IControlPoint* const controlPoint = static_cast<IControlPoint*>(
            object->queryInterface(IID_IControlPoint));

        const Vec3& controlPointPosition = object->getAbsolutePosition();
        const Vec3& supplyPosition = getAbsolutePosition();
        const float dx = controlPointPosition.x - supplyPosition.x;
        const float dy = controlPointPosition.y - supplyPosition.y;
        const float dz = controlPointPosition.z - supplyPosition.z;
        const float distance = std::sqrt(dx * dx + dy * dy + dz * dz);

        if (closestDistance > distance)
        {
            closestDistance = distance;
            team = controlPoint->getTeam();
        }
    }
    return team;
}

void SupplyObject::handleMessage(TemplateMessage message, IPlayer*, bool)
{
    if (message == static_cast<TemplateMessage>(29))
    {
        update(0.2f);
        objectManager->postMessage(this, static_cast<TemplateMessage>(29), 0.2f);
    }
}

// On the server, latches landing for mobile crates and notifies SupplyDropManager.
// Forwards to Bundle with both face-info pointers cleared.
bool SupplyObject::handleCollision(IObject* object, const Vec3& a, const Vec3& b, const Vec3& c,
                                   int d, int e, CollisionFaceInfo*, CollisionFaceInfo*)
{
    if (gameLogic->isServer() && m_template->getHasMobilePhysics())
    {
        m_suppliesLanded = true;
        supplyDropManager->handleSuppliesLanding(this, object);
    }
    return Bundle::handleCollision(object, a, b, c, d, e, 0, 0);
}

// setSuppliesLanded - 0x81BEFA0, 115 B. The other side of the latch: a server returns immediately
// (its latch is set by handleCollision above); this is the client arm, fed by replication. A false
// -> plain store; a true on an un-landed crate -> latch + handleSuppliesLanding with NO collided
// object (the client doesn't know what was hit - the null is shipped).
void SupplyObject::setSuppliesLanded(bool landed)
{
    if (gameLogic->isServer())
        return;

    if (landed && !m_suppliesLanded)
    {
        m_suppliesLanded = true;
        supplyDropManager->handleSuppliesLanding(this, 0);
    }
    else
    {
        m_suppliesLanded = landed;
    }
}

// the template reads - friend access, offsets in the header
bool SupplyObject::getWorkOnVehicles() { return m_template->workOnVehicles; }
bool SupplyObject::getWorkOnSoldiers() { return m_template->workOnSoldiers; }
bool SupplyObject::hasHealAbility()    { return m_template->healSpeed > 0.0f; }
bool SupplyObject::hasAmmoAbility()    { return m_template->refillAmmoSpeed > 0.0f; }

// hasEmptyHealStorage / hasEmptyAmmoStorage - 0x81BEC00 / 0x81BEC60, 86 B each. "Empty" means both
// the own gauge and the shared pool are down to a fifth of one second's rate (speed / 5.0 - the
// 5 Hz tick's worth). An unlimited template (-1.0f storage) is never empty. These watch the
// correct gauges - the header's bug note lives in the Subtract pair below, not here.
bool SupplyObject::hasEmptyHealStorage()
{
    if (m_template->healStorageSize != -1.0f)
    {
        const float threshold = m_template->healSpeed / 5.0f;
        if (m_healStorage <= threshold)
            return m_sharedStorage <= threshold;
    }
    return false;
}

bool SupplyObject::hasEmptyAmmoStorage()
{
    if (m_template->ammoStorageSize != -1.0f)
    {
        const float threshold = m_template->refillAmmoSpeed / 5.0f;
        if (m_ammoStorage <= threshold)
            return m_sharedStorage <= threshold;
    }
    return false;
}

// Heal drains ammo storage; ammo drains heal storage. Keep this crossed pairing.
// If the gauge is not strictly greater than amount, charge shared storage without a lower clamp.
void SupplyObject::SubtractHeal(float amount)
{
    if (m_ammoStorage > amount)                  // the wrong gauge, as shipped
        m_ammoStorage -= amount;
    else
        m_sharedStorage -= amount;
}

void SupplyObject::SubtractAmmo(float amount)
{
    if (m_healStorage > amount)                  // the wrong gauge, as shipped
        m_healStorage -= amount;
    else
        m_sharedStorage -= amount;
}

// getArmorInfo - 0x81BEE70, 86 B. The sret slot IObject.h documents as this hierarchy's trap
// (`retn 4`, hidden return slot first). Three gauges then the team, through formatString's shared
// static buffer. Print order is heal/ammo/shared - the initialisation order, so on a bugged crate
// the first number is the one ammo spending shrinks.
std::string SupplyObject::getArmorInfo()
{
    return formatString("%.1f/%.1f/%.1f team:%d",
                        m_healStorage, m_ammoStorage, m_sharedStorage, m_team);
}

} // namespace world
} // namespace hfe
} // namespace dice
