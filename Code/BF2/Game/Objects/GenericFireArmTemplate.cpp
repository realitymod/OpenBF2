// Firearm template lifecycle, item settings and projectile lookup. [INFERRED] Source placement.
// Precaching reaches each projectile's sixteen-instance pre-store and therefore affects network id allocation.

#include "GenericFireArmTemplate.h"

#include "../../Core/Ids.h"
#include "CompTemplateInterfaces.h"
#include "IObjectTemplateComponent.h"
#include "../../Scene/Object/ObjectTypes/Base/ObjectTemplateManager.h"
#include "Weapons/GenericFireArm.h"                    // the real product class

namespace dice {
namespace hfe {
namespace world {

// Leaves _dword1A4, bytes 425..431, and m_weaponClass uninitialized until configured.
GenericFireArmTemplate::GenericFireArmTemplate()
{
    m_refCount4 = 1;                                 // +408
    m_delayToUse = 0.0f;                             // +412
    m_delayToUseTimes30 = 0;                         // +416
    m_setHideFirstPerson = false;                    // +424
    m_heatAddWhenFire = 0.0f;                        // +432
    m_coolDownPerSec = 0.0f;                         // +436
    m_overheatPenalty = 0.0f;                        // +440
    m_inheritHeatFromPrimary = false;                // +444
    m_itemIndex = -1;                                // +448
    m_itemType = 0;                                  // +452
    m_velocity = 600.0f;                             // +456 - the shipped default muzzle velocity
    m_weaponType = 0;                                // +460
    m_worksInWater = false;                          // +468
    // the two name strings and m_blockInputOnRotBundle default-construct empty (the ELF's
    // shared-empty-string increfs)
    m_projectileTemplate = 0;                        // +480
    m_secondaryProjectileTemplate = 0;               // +484
    m_keepProjectiles = 5.0f;                        // +488
    m_isSelectable = true;                           // +492
    m_canInternalHandleMessage = false;              // +493
    m_isGasMask = false;                             // +494
    m_isNightVision = false;                         // +495
    m_deployAnimationTime = 1.0f;                    // +496
    m_canFireGuiIndex = 0;                           // +504
    m_cannotFireGuiIndex = 0;                        // +508
    m_cannotFireTimer = 1.0f;                        // +512
    m_crosshairFireTime = 3.0f;                      // +516
    m_proneOnlyReload = false;                       // +520 [CHOSEN, not recovered]
}

// Console setters that also update derived state.

// TWO stores, AND THE second IS A truncation. The tick cache is `cvttss2si` - truncate toward
// zero - not a rounding conversion, so 0.19s at 30Hz caches 5 ticks, not 6.
void GenericFireArmTemplate::setDelayToUse(float seconds)
{
    m_delayToUse = seconds;                          // +412
    m_delayToUseTimes30 = static_cast<int>(seconds * 30.0f);   // +416
}

// clamped AT 10.0f, in the shipped order: the ceiling is loaded first and the argument only
// replaces it when it compares less. A NaN fails that compare, so a NaN penalty stores 10.0f
// instead of propagating. Written as the ELF has it rather than as a std::min.
void GenericFireArmTemplate::setOverheatPenalty(float penalty)
{
    float value = 10.0f;
    if (penalty < 10.0f)
        value = penalty;
    m_overheatPenalty = value;                       // +440
}

// / 0x8256B10 / 0x8256B60. This is a distinct template refcount; release returns the
// decremented value and invokes the deleting destructor only at zero.
void GenericFireArmTemplate::addRef()
{
    ++m_refCount4;
}

int GenericFireArmTemplate::getRef()
{
    return m_refCount4;
}

int GenericFireArmTemplate::release()
{
    const int refs = --m_refCount4;
    if (refs == 0)
        delete this;
    return refs;
}

// . Flat shipped interface set: notably, this does not chain through
// AnimatedBundleTemplate and therefore does not answer IID_ISimpleObjectTemplate directly.
void* GenericFireArmTemplate::queryInterface(unsigned int iid) const
{
    GenericFireArmTemplate* self = const_cast<GenericFireArmTemplate*>(this);
    if (iid == IID_IBase || iid == IID_IObjectTemplate)
        return static_cast<IObjectTemplate*>(self);
    if (iid == IID_IBundleTemplate)
        return static_cast<IBundleTemplate*>(self);
    if (iid == io::IID_IConsoleSaveable)
        return static_cast<io::IConsoleSaveable*>(self);
    if (iid == IID_IItemTemplate)
        return static_cast<IItemTemplate*>(self);
    return 0;
}

unsigned int GenericFireArmTemplate::getClassID() const
{
    return CID_GenericFireArmTemplate;               // 38099
}

// Recognizes firearm and animated-bundle IDs before delegating to BundleTemplate.
void* GenericFireArmTemplate::queryImplementation(unsigned int cid)
{
    if (cid == CID_GenericFireArmTemplate || cid == CID_AnimatedBundleTemplate)
        return this;
    return BundleTemplate::queryImplementation(cid);
}

// ..0x8256D40. These are deliberately virtual calls through IItemTemplate in the
// generated ObjectTemplate.itemIndex/itemType console executors.
void GenericFireArmTemplate::setItemIndex(int itemIndex) { m_itemIndex = itemIndex; }
int GenericFireArmTemplate::getItemIndex() const { return m_itemIndex; }
void GenericFireArmTemplate::setItemType(int itemType) { m_itemType = itemType; }
int GenericFireArmTemplate::getItemType() const { return m_itemType; }

// Firearms intentionally leave initNetworkableComp empty, even when their template has networkableInfo.

// Constructs the weapon; its constructor handles precaching.
// [RETIRED: genericfirearm-class]
IObject* GenericFireArmTemplate::createObject() const
{
    return new GenericFireArm(this);
}

// Precaches the target component, then the resolved projectile template.
// Repeated calls have no local latch; the projectile pre-store enforces its own capacity.
void GenericFireArmTemplate::preCache() const
{
    GenericFireArmTemplate* self = const_cast<GenericFireArmTemplate*>(this);

    IWeaponCompTemplate* targetComp = static_cast<IWeaponCompTemplate*>(
        self->queryComponent(IID_IWeaponTargetCompTemplate));
    if (targetComp)
        targetComp->preCache();                      // +0x2C

    IObjectTemplate* projectile = self->getProjectileTemplate();
    if (projectile)
        projectile->preCache();                      // +0x6C
}

// the lazy authored-name -> resolved-pointer hop, cached in +480.
IObjectTemplate* GenericFireArmTemplate::getProjectileTemplate()
{
    if (!m_projectileTemplate)
        m_projectileTemplate = objectTemplateManager->getTemplate(m_projectileTemplateName);
    return m_projectileTemplate;
}

// the parallel lazy authored-name -> resolved-pointer hop, cached in +484.
IObjectTemplate* GenericFireArmTemplate::getSecondaryProjectileTemplate()
{
    if (!m_secondaryProjectileTemplate)
        m_secondaryProjectileTemplate =
            objectTemplateManager->getTemplate(m_secondaryProjectileTemplateName);
    return m_secondaryProjectileTemplate;
}

} // namespace world
} // namespace hfe
} // namespace dice
