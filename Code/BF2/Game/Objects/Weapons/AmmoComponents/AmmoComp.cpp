// m_magazines[0] is the loaded magazine; later entries are spares. A loaded count of -1 means infinite ammo.

#include "AmmoComp.h"

#include <sstream>
#include <string>

#include "../GenericFireArm.h"
#include "../../IObject.h"
#include "../../IObjectTemplate.h"
#include "../../IPlayer.h"
#include "../../IWeaponObject.h"
#include "../../Game/Soldier.h"
#include "../../../IGameLogic.h"
#include "../../../../Core/Debug.h"
#include "../../../../Core/Ids.h"
#include "../../../../Core/StringUtil.h"
#include "../../../../Core/System.h"
#include "../../../../IO/Stream/IStream.h"
#include "../../../../Scene/Object/ObjectUtil.h"

namespace dice {
namespace hfe {
namespace world {

namespace {

// Ammo consumes the first word of GenericFireArm's now-complete 0x3C-byte WeaponFireEvent.  ELF
// and the PE AmmoComp::postFire both read this same +0x00 field before subtraction.
int weaponFireEventRounds(const WeaponFireEvent& event)
{
    return event.fireCount;
}

// The six ReloadState values this component uses. The enum ships opaque (no enumerator names
// survive), so the numbers are spelled here once instead of being cast at thirty call sites.
// Their meanings are established in AmmoComp.h; they are NOT arbitrary.
const ReloadState kReloadIdle       = static_cast<ReloadState>(0);
const ReloadState kReloadCooldown   = static_cast<ReloadState>(1);
const ReloadState kReloadRequested  = static_cast<ReloadState>(2);
const ReloadState kReloadMagazine   = static_cast<ReloadState>(3);
const ReloadState kReloadFirstShell = static_cast<ReloadState>(4);
const ReloadState kReloadShell      = static_cast<ReloadState>(5);
const ReloadState kReloadLastShell  = static_cast<ReloadState>(6);

// The refill carry survives exactly 30 ticks - one second at the fixed timestep. Past that the
// accumulated fraction of a round is thrown away and the next refill starts from zero.
const unsigned int kRefillCarryTicks = 30;

} // namespace

IWeaponAmmoComp::~IWeaponAmmoComp() {}

void IWeaponAmmoComp::reload(ReloadState) {}
void IWeaponAmmoComp::postReload() {}
void IWeaponAmmoComp::outOfAmmo(bool) {}
bool IWeaponAmmoComp::canBeRefilled() { return true; }

// THE vector IS filled three different ways AND IS never left empty, which is the invariant the
// whole class leans on. nrOfMags copies of magSize is the normal case; nrOfMags == -1 with a real
// magSize gives ONE magazine; and if neither pushed anything a -1 goes in, which is the sentinel
// postFire reads as "infinite, never subtract".
// +0x51..+0x53 and m_lastFireTick's neighbours are simply not among the stores - the constructor
// is enumerable and they are absent, not overlooked.
AmmoComp::AmmoComp(AmmoCompTemplate* objectTemplate)
    : m_byte20(false)
    , m_acceptFireAtTick(0)
    , m_changeMagAtTick(0)
    , m_reloadDoneAtTick(0)
    , m_lastFireTick(0)
    , m_autoReloadStartTick(0)
    , m_lastRefillTick(0)
    , m_refillCarry(0.0f)
    , m_reloadStepsLeft(0)
    , m_reloadStepsTotal(0)
    , m_reloadState(kReloadIdle)
    , m_magSwapped(false)
    , m_outOfAmmoNotified(false)
    , m_signalReloadComplete(false)
    , m_signalReload(false)
    , m_autoReloadPending(false)
    , m_linkedWeapon(0)
    , m_template(objectTemplate)
{
    m_refCount = 1;

    const int nrOfMags = m_template->getNrOfMags();
    if (nrOfMags == -1)
    {
        if (m_template->getMagSize() != -1)
            m_magazines.push_back(m_template->getMagSize());
    }
    else
    {
        for (int i = 0; i < m_template->getNrOfMags(); ++i)
            m_magazines.push_back(m_template->getMagSize());
    }

    // THE sentinel. Nothing else in this class ever checks the vector's size before indexing [0].
    if (m_magazines.empty())
        m_magazines.push_back(-1);
}

// . The vptr resets plus the vector's own teardown; no owned pointers.
AmmoComp::~AmmoComp() {}

unsigned int AmmoComp::getInterfaceID() const { return IID_IWeaponAmmoComp; }

// initComponent - 0x8257160, 175 B. THE shared-ammo link, and it is the only thing this method
// does. A template naming `magLinkWeapon` makes this component push its round count at another
// weapon on the same soldier - which is how a rifle and its underslung launcher share a HUD number.
// IT walks THE soldier'S children, NOT THE firearm'S: the parent is taken first, then getChild()
// and the sibling chain. And it does NOT stop at the first match - the loop runs to the end of the
// sibling list, so a duplicate name leaves the last match linked.
// THE event argument IS never read. postFire relies on that when it re-runs this lazily.
void AmmoComp::initComponent(WeaponInitEvent&)
{
    if (m_template->getMagLinkWeapon().empty())
        return;

    IObject* const soldier = firearm()->getParent();
    if (!soldier)
        return;

    for (IObject* child = soldier->getChild(); child; child = child->getSibling())
    {
        if (compareIgnoreCase(child->getTemplate()->getName(),
                              m_template->getMagLinkWeapon()) == 0)
        {
            m_linkedWeapon = static_cast<IWeaponObject*>(
                child->queryInterface(IID_IWeaponObject));
        }
    }
}

bool AmmoComp::preUpdate() { return false; }

// Advances reload, automatic reload and post-shot cooldown through independent tests.
// Magazine swap and completion can occur in the same tick; busy(false) checks the whole firearm.
// Keep the separate getTickCount calls rather than caching one value.
void AmmoComp::updateComponent(float, unsigned int)
{
    if (m_reloadState > kReloadRequested)
    {
        if (!m_magSwapped &&
            m_changeMagAtTick <= static_cast<unsigned int>(WorldPref::getTickCount()))
        {
            m_byte20 = false;
            reloadMag();
        }

        if (m_reloadDoneAtTick <= static_cast<unsigned int>(WorldPref::getTickCount()))
            reloadComplete();
    }

    if (m_reloadState == kReloadRequested &&
        !firearm()->busyExcept(this, false))
    {
        m_signalReload = true;
    }

    if (m_template->getAutoReload() &&
        m_acceptFireAtTick < static_cast<unsigned int>(WorldPref::getTickCount()) &&
        m_reloadState <= kReloadCooldown &&
        !firearm()->busy(false) &&
        needsReload() &&
        canReload())
    {
        m_autoReloadStartTick = static_cast<unsigned int>(WorldPref::getTickCount());
        m_signalReload = true;
    }

    if (m_acceptFireAtTick < static_cast<unsigned int>(WorldPref::getTickCount()) &&
        m_reloadState == kReloadCooldown)
    {
        m_reloadState = kReloadIdle;
    }
}

// isSelectable - 0x8257E60, 69 B. A weapon only becomes unselectable IF ITS template asked TO BE.
// Without toggleWhenNoAmmo an empty gun stays in the carousel; with it, an empty gun that also
// cannot reload drops out - which is what makes a spent grenade or C4 pack disappear from the
// weapon wheel.
bool AmmoComp::isSelectable() const
{
    if (!m_template->getToggleWhenNoAmmo())
        return true;
    if (!needsReload())
        return true;
    return canReload();
}

// . ignores ITS argument entirely - both the alternate and primary fire paths get
// the same number, the sum of the two template times.
float AmmoComp::getAcceptDelay(bool) const
{
    return m_template->getMinimumTimeUntilReload() + m_template->getReloadTime();
}

void AmmoComp::fireStop(bool) {}
void AmmoComp::postSwitchFireRate(FireRate) {}
void AmmoComp::stoppedTracking() {}
void AmmoComp::postChangeTarget(IObject*) {}
void AmmoComp::projectileWasReset(IObject*) {}
void AmmoComp::stop() {}
void AmmoComp::exit() {}
void AmmoComp::destroy() {}
void AmmoComp::pickup() {}
void AmmoComp::reset() {}

// requestFire - 0x8257330, 47 B. this IS THE veto that stops AN empty GUN firing, and it is two
// lines. GenericFireArm::doFire asks every component for permission; this one says no when the
// chamber is empty, and no while a reload has been requested but not yet started.
// IT permits firing IN states 3..6 - mid-reload. The magazine-swap latch, not this test, is what
// keeps a shotgun from firing a shell it has not yet loaded.
bool AmmoComp::requestFire(bool, bool)
{
    if (needsReload())
        return false;
    return m_reloadState != kReloadRequested;
}

// Consumes the event's round count unless the loaded magazine uses the -1 infinite-ammo sentinel.
// A positive request on an empty magazine diagnoses and returns before bookkeeping; a zero request continues.
// Lazily resolves a missing named magazine link before updating shared ammo state.
void AmmoComp::postFire(WeaponFireEvent& event)
{
    if (m_magazines[0] != -1)
    {
        const int rounds = weaponFireEventRounds(event);

        if (m_magazines[0] == 0 && rounds > 0)
        {
            Debug(DEBUGTYPE_6,
                  "Game/Objects/Weapons/AmmoComponents/AmmoComp.cpp", 314, "")
                << "Trying to subtract ammo when already 0! obj="
                << firearm()->getTemplate()->getName();
            return;
        }

        m_magazines[0] -= rounds;          // the round leaves the gun
    }

    if (!m_template->getMagLinkWeapon().empty() && !m_linkedWeapon)
    {
        WeaponInitEvent lazyEvent;
        lazyEvent.firearmTemplate = 0;
        initComponent(lazyEvent);
    }

    if (m_linkedWeapon)
        m_linkedWeapon->setAmmoInMag(0, m_magazines[0]);

    if (m_template->getMinimumTimeToReloadTimes30())
        m_lastFireTick = static_cast<unsigned int>(WorldPref::getTickCount());

    if (!m_magazines[0])
        m_outOfAmmoNotified = true;

    // Only an auto-reloading weapon arms this; fireComplete is what acts on it.
    m_autoReloadPending = m_template->getAutoReload() && m_magazines[0] == 0;
}

// Switches an exhausted, unselectable weapon to the previous weapon when configured, preserving
// the discarded interface query. Sets the post-shot gate and starts or signals a needed reload.
// An immediately started reload uses the gate tick, not the current tick.
void AmmoComp::fireComplete(unsigned int tick)
{
    if (m_template->getToggleWhenNoAmmo() &&
        needsReload() &&
        !canReload() &&
        !firearm()->isSelectable() &&
        firearm()->getPlayer())
    {
        IObject* const holder = findClosestParentHavingNetworkable(firearm());
        if (holder && holder->getTemplate()->getClassID() == CID_SoldierTemplate)
        {
            Soldier* const soldier = static_cast<Soldier*>(holder);
            soldier->queryInterface(IID_IPlayerControlObject);   // result discarded in retail
            // A direct field write in retail (ELF 0x8258060 stores to the firearm's +0x1F4).
            // GenericFireArm has no setter for it; the friendship mirrors FireComp's.
            firearm()->m_pendingActiveWeapon = soldier->getPreviousWeapon();
        }
    }

    m_reloadState = kReloadCooldown;
    m_acceptFireAtTick = m_template->getMinimumTimeToReloadTimes30() + tick;

    if (m_outOfAmmoNotified && needsReload() && canReload())
        m_autoReloadStartTick = m_acceptFireAtTick;

    if (m_autoReloadPending && needsReload() && canReload())
    {
        if (m_acceptFireAtTick < static_cast<unsigned int>(WorldPref::getTickCount()))
        {
            m_reloadState = kReloadRequested;
            doReload(m_acceptFireAtTick);
        }
        else
        {
            m_signalReload = true;
        }
    }
}

// requestReload - 0x8257A30, 128 B. THE `automatic` argument IS A refusal, not a mode: an
// automatic request on an auto-reloading weapon is dropped, because updateComponent already owns
// that case and two paths would double-count the shells.
// this IS where THE shell count IS decided. A magazine weapon always books exactly one step; a
// shell weapon books (magSize - rounds left) and remembers the total so doReload can tell the first
// shell from the rest. An already-running shell reload is not re-counted.
bool AmmoComp::requestReload(bool automatic)
{
    if (automatic && m_template->getAutoReload())
        return false;

    if (getIsReloading())
        return false;

    if (m_template->getReloadAmount())
    {
        if (!m_reloadStepsLeft)
        {
            const int steps = m_template->getMagSize() - m_magazines[0];
            m_reloadStepsLeft = steps;
            m_reloadStepsTotal = steps;
        }
    }
    else
    {
        m_reloadStepsLeft = 1;
        m_reloadStepsTotal = 1;
    }

    return canReload();
}

// reload - 0x8257AB0, 51 B. IT takes TWO calls TO start A reload. The first only moves the state
// to "requested"; the second, arriving once the weapon is actually in the reload state, is what
// runs doReload. That is the handshake GenericFireArm::doReload drives.
void AmmoComp::reload()
{
    if (m_reloadState == kReloadRequested)
        doReload(static_cast<unsigned int>(WorldPref::getTickCount()));
    else
        m_reloadState = kReloadRequested;
}

// . One more shell, please. The shotgun reload's "keep going" hook.
void AmmoComp::queueReload() { ++m_reloadStepsLeft; }

// Schedules magazine or first/middle/final shell reload phases from startTick.
// The final shell phase is animation-only. Swap offsets truncate toward zero.
// The first-shell delay counts twice in the swap tick and once in the completion tick.
void AmmoComp::doReload(unsigned int startTick)
{
    const int reloadTicks = m_template->getReloadTimeTimes30();
    const int swapOffset =
        static_cast<int>(static_cast<float>(reloadTicks) * m_template->getChangeMagAt());

    if (m_template->getReloadAmount())
    {
        int doneOffset;
        if (m_reloadStepsLeft == m_reloadStepsTotal)
        {
            m_reloadState = kReloadFirstShell;
            m_magSwapped = false;
            m_changeMagAtTick =
                startTick + m_template->getExtraReloadTimeFirstTimes30() + swapOffset;
            startTick += reloadTicks;
            doneOffset = m_template->getExtraReloadTimeFirstTimes30();
        }
        else if (m_reloadStepsLeft)
        {
            m_reloadState = kReloadShell;
            m_magSwapped = false;
            m_changeMagAtTick = swapOffset + startTick;
            doneOffset = reloadTicks;
        }
        else
        {
            m_reloadState = kReloadLastShell;
            m_magSwapped = true;                 // no magazine swap for the closing animation
            doneOffset = m_template->getExtraReloadTimeLastTimes30();
        }
        m_reloadDoneAtTick = doneOffset + startTick;
    }
    else
    {
        m_reloadState = kReloadMagazine;
        m_magSwapped = false;
        m_changeMagAtTick = swapOffset + startTick;
        m_reloadDoneAtTick = reloadTicks + startTick;
    }

    --m_reloadStepsLeft;
    if (m_reloadStepsLeft <= 0)
        m_reloadStepsLeft = 0;

    m_autoReloadPending = false;
}

// Refills the loaded magazine, consuming a spare from the back unless magazine count is infinite.
// With no spare, only latches completion. Shell reloads add one round; magazine reloads replace the count.
void AmmoComp::reloadMag()
{
    if (m_template->getNrOfMags() != -1)
    {
        if (getAmmo()->size() <= 1)
        {
            m_magSwapped = true;
            return;
        }
        m_magazines.erase(m_magazines.end() - 1);
    }

    if (m_template->getReloadAmount())
        ++m_magazines[0];
    else
        m_magazines[0] = m_template->getMagSize();

    m_magSwapped = true;

    if (m_linkedWeapon)
    {
        m_linkedWeapon->setCurrentNrOfMags(static_cast<int>(m_magazines.size()));
        m_linkedWeapon->setAmmoInMag(0, m_magazines[0]);
    }

    // Only a full magazine clears the dry flag - a shotgun part-way through its shells stays
    // flagged, which is what keeps its auto-reload chain running.
    if (m_magazines[0] == m_template->getMagSize())
        m_outOfAmmoNotified = false;
}

// reloadComplete - 0x8257E30, 47 B. IT RE-arms itself. Shells still owed, or a first/middle shell
// just finished, raise m_signalReload again - that loop is the entire shotgun reload cycle, and it
// is why the last shell has its own state: state 6 is the only one that does NOT re-arm.
void AmmoComp::reloadComplete()
{
    m_signalReloadComplete = true;

    if (m_reloadStepsLeft > 0 ||
        m_reloadState == kReloadFirstShell ||
        m_reloadState == kReloadShell)
    {
        m_signalReload = true;
    }

    m_reloadState = kReloadIdle;
}

// getSignalReload - 0x8257EB0, 78 B. A consuming read with A PUT-back. It clears the flag, then -
// if the post-shot gate has not passed - restores it and reports false, so the signal survives
// until the weapon is allowed to act on it. Once the gate passes the gate itself is zeroed.
bool AmmoComp::getSignalReload()
{
    const bool signalled = m_signalReload;
    m_signalReload = false;

    if (signalled && m_acceptFireAtTick)
    {
        if (static_cast<unsigned int>(WorldPref::getTickCount()) < m_acceptFireAtTick)
        {
            m_signalReload = true;
            return false;
        }
        m_acceptFireAtTick = 0;
    }

    return signalled;
}

// . A plain consuming read - no put-back.
bool AmmoComp::getSignalReloadComplete()
{
    const bool signalled = m_signalReloadComplete;
    m_signalReloadComplete = false;
    return signalled;
}

// . Only a manual-reload weapon ever asks the player to reload; an auto-reloading
// one handles it in updateComponent and reports false here forever.
bool AmmoComp::shouldReload() const
{
    return m_reloadState == kReloadIdle &&
           needsReload() &&
           canReload() &&
           !m_template->getAutoReload();
}

// . Reads getMag0Size(), not getBulletInChamber() - the int getter, through the
// vtable, so a leaf overriding either changes this.
bool AmmoComp::needsReload() const { return getMag0Size() == 0; }

// canReload - 0x82579A0, 55 B. Two conditions, and the second is the infinite-magazine escape.
// A full magazine cannot reload even with spares available - which is what stops a shell weapon
// topping itself up forever.
bool AmmoComp::canReload() const
{
    return m_magazines[0] != m_template->getMagSize() &&
           (m_magazines.size() > 1 || m_template->getNrOfMags() == -1);
}

// Adds loose rounds or whole magazines and returns the amount that did not fit.
// Preserve the partial loose-round behavior: it changes slot 0 even when testOnly is true.
unsigned int AmmoComp::refillAbsolute(unsigned int amount, bool testOnly)
{
    if (needsRefill())
        return refillInternal(amount, testOnly);
    return amount;
}

unsigned int AmmoComp::refillInternal(unsigned int amount, bool testOnly)
{
    unsigned int left = amount;

    if (shouldRefillBullets())
    {
        const unsigned int room =
            static_cast<unsigned int>(m_template->getMagSize() - m_magazines[0]);
        if (amount < room)
        {
            left = 0;
            m_magazines[0] += static_cast<int>(amount);   // runs even when testOnly - see above
        }
        else
        {
            left = amount - room;
            if (!testOnly)
            {
                m_magazines[0] = m_template->getMagSize();
                m_outOfAmmoNotified = false;
            }
        }
    }
    else
    {
        // An empty loaded magazine does not count towards the carried total, so a soldier with a
        // spent gun and two spares is treated as holding two - and can take a third.
        int held = static_cast<int>(m_magazines.size());
        if (held && !m_magazines[0] && m_template->getMagSize() > 0)
            --held;

        int room = m_template->getNrOfMags() - held;
        while (room > 0 && left)
        {
            if (!testOnly)
                m_magazines.push_back(m_template->getMagSize());
            --room;
            --left;
        }
    }

    if (m_linkedWeapon && !testOnly)
    {
        m_linkedWeapon->setCurrentNrOfMags(static_cast<int>(m_magazines.size()));
        m_linkedWeapon->setAmmoInMag(0, m_magazines[0]);
    }

    return left;
}

// . ONE magazine or infinite magazines both mean "there is no spare-magazine pool,
// so refill the rounds directly".
bool AmmoComp::shouldRefillBullets()
{
    const int nrOfMags = m_template->getNrOfMags();
    return nrOfMags == 1 || nrOfMags == -1;
}

// . The unit the percentage is measured in - rounds for a bullet refill, magazines
// otherwise.
int AmmoComp::getMaxRefillAmount()
{
    if (shouldRefillBullets())
        return m_template->getMagSize();
    return m_template->getNrOfMags();
}

// needsRefill - 0x8257700, 107 B. magSize == -1 IS checked first AND wins: an infinite-ammo weapon
// can never be refilled, whatever else its template says.
// The magazine arm decrements for an empty loaded magazine without the `magSize > 0` guard that
// refillInternal applies to the same count. The two disagree for a zero-size magazine; both are
// reproduced as shipped rather than reconciled.
bool AmmoComp::needsRefill()
{
    const bool bullets = shouldRefillBullets();

    if (m_template->getMagSize() == -1)
        return false;

    if (!bullets)
    {
        if (m_template->getNrOfMags() == -1)
            return false;

        int held = static_cast<int>(m_magazines.size());
        if (held && !m_magazines[0])
            --held;
        return held < m_template->getNrOfMags();
    }

    return m_magazines[0] < m_template->getMagSize();
}

// [CHOSEN, not recovered] Project Reality reads the template's ammoType as a rearm rate: -1 turns
// rearming off, and N > 1 scales each refill by 100/N (50 rearms twice as fast, 200 half as fast).
// The returned amount is scaled back, so the supply source is charged unscaled. testOnly calls,
// which only ask whether a refill is possible, are not scaled.
float AmmoComp::refillPercent(float percent, bool testOnly)
{
    const int rearmRate = m_template->getAmmoType();
    if (rearmRate == -1)
        return percent;
    if (testOnly)
        return refillPercentUnscaled(percent, true);

    const float scale = rearmRate > 1 ? 100.0f / static_cast<float>(rearmRate) : 1.0f;
    return refillPercentUnscaled(percent * scale, false) / scale;
}

// Accumulates fractional refill until whole rounds can be supplied; carry expires after 30 ticks.
// Returns the value for the next weapon, whose units vary between input percent and integer leftovers.
// testOnly returns zero when supply is possible without storing carry or the refill tick.
float AmmoComp::refillPercentUnscaled(float percent, bool testOnly)
{
    float carry = m_refillCarry;

    if (!needsRefill())
        return percent;

    // [CHOSEN, not recovered] Project Reality keeps a carry of half a round or more; retail drops
    // any carry after 30 ticks.
    if (carry < 0.5f
        && m_lastRefillTick + kRefillCarryTicks <
           static_cast<unsigned int>(WorldPref::getTickCount()))
    {
        carry = 0.0f;
    }

    const float amount = static_cast<float>(getMaxRefillAmount()) * (percent * 0.01f);
    float total = amount + carry;

    float result = percent;
    int whole = 0;
    if (total > 1.0f)
    {
        whole = static_cast<int>(total);
        total -= static_cast<float>(whole);
        if (whole)
            result = static_cast<float>(refillInternal(static_cast<unsigned int>(whole), testOnly));
    }

    if (total > m_refillCarry || static_cast<float>(whole) > result)
        result = 0.0f;

    if (testOnly)
    {
        if (amount > 0.0f)
            return 0.0f;
        return result;
    }

    // THE resupply score hook. Only when whole rounds actually changed hands, only on the server,
    // and it is the player holding the refilled weapon that gets stamped.
    if (whole)
    {
        IPlayer* const player = firearm()->getPlayer();
        if (player && gameLogic->isServer())
        {
            player->setGaveAmmoAtTick(
                static_cast<unsigned int>(WorldPref::getTickCount()), false);
        }
    }

    m_refillCarry = total;
    if (amount > 0.0f)
        m_lastRefillTick = static_cast<unsigned int>(WorldPref::getTickCount());

    return result;
}

// enable / disable - 0x8258070 / 0x82581C0.
// reloadWithoutPlayer IS what lets A vehicle weapon keep reloading while nobody IS IN THE seat.
// Both bodies ask the same question - is this firearm parented to something that is NOT a soldier -
// and both skip their reset when the answer is yes. A soldier-held weapon has its reload state
// wiped on every draw and holster; a turret's survives.
// enable() calls updateFlags(2, 0) twice on the same object when a bullet is chambered. Not a
// transcription slip - the two calls are at different addresses in the shipped body.
void AmmoComp::enable()
{
    bool keepState = false;
    if (m_template->getReloadWithoutPlayer())
    {
        IObject* const holder = firearm()->getParent();
        if (holder && holder->getTemplate()->getClassID() != CID_SoldierTemplate)
            keepState = true;
    }

    if (keepState)
    {
        if (m_signalReload)
        {
            if (m_acceptFireAtTick < static_cast<unsigned int>(WorldPref::getTickCount()) &&
                m_reloadState == kReloadCooldown)
            {
                m_reloadState = kReloadRequested;
                doReload(m_acceptFireAtTick);
            }
        }
    }
    else
    {
        m_acceptFireAtTick = 0;
        m_autoReloadStartTick = 0;
        m_reloadState = kReloadIdle;
        m_changeMagAtTick = 0;
        m_reloadDoneAtTick = 0;
        m_signalReload = false;
    }

    firearm()->updateFlags(2, 0);

    // THE instant TOP-UP ON draw. Five conditions deep, and the last is a template opt-in.
    if (m_reloadState == kReloadIdle &&
        m_template->getAutoReload() &&
        needsReload() &&
        canReload() &&
        m_template->getInstantReloadOnEnable())
    {
        reloadMag();
    }

    if (getBulletInChamber())
    {
        m_outOfAmmoNotified = false;
        firearm()->updateFlags(2, 0);
    }
}

void AmmoComp::disable()
{
    if (m_template->getReloadWithoutPlayer())
    {
        IObject* const holder = firearm()->getParent();
        if (holder && holder->getTemplate()->getClassID() != CID_SoldierTemplate)
            return;
    }

    m_reloadState = kReloadIdle;
    m_reloadStepsLeft = 0;
}

// . A pending signal counts AS busy even before the reload starts, which is what
// stops the firearm switching weapons out from under a reload that has been asked for.
bool AmmoComp::busy()
{
    return m_reloadState > kReloadRequested || m_signalReload;
}

// The reload-progress pair - 0x8257C30 / 0x8257C80. Both feed the HUD bar.
// neither clamps. getReloadTimeLeftInPercent goes negative once the done tick has passed but the
// state has not yet been cleared, and its denominator is reloadTimeTimes30 even for a shell reload
// whose real duration is the extra-first/extra-last time. Shipped as-is.
// getTotalReloadTimeLeftInPercent is the auto-reload variant and measures against the whole
// gate-plus-reload window; a manual weapon falls straight through to the other one.
float AmmoComp::getReloadTimeLeftInPercent() const
{
    if (!getIsReloading())
        return 0.0f;

    return static_cast<float>(
               static_cast<int>(m_reloadDoneAtTick) - WorldPref::getTickCount()) /
           static_cast<float>(m_template->getReloadTimeTimes30());
}

float AmmoComp::getTotalReloadTimeLeftInPercent() const
{
    if (!m_template->getAutoReload())
        return getReloadTimeLeftInPercent();

    const int endTick =
        static_cast<int>(m_autoReloadStartTick) + m_template->getReloadTimeTimes30();
    if (endTick < WorldPref::getTickCount())
        return 0.0f;

    return static_cast<float>(endTick - WorldPref::getTickCount()) /
           static_cast<float>(m_template->getReloadTimeTimes30() +
                              m_template->getMinimumTimeToReloadTimes30());
}

// the plain accessors
bool  AmmoComp::getBulletInChamber() const { return m_magazines[0] != 0; }
bool  AmmoComp::getIsReloading() const { return m_reloadState > kReloadCooldown; }
ReloadState AmmoComp::getReloadState() const { return m_reloadState; }
int   AmmoComp::getMagSize() const { return m_template->getMagSize(); }
int   AmmoComp::getMaxNrOfMags() const { return m_template->getNrOfMags(); }
bool  AmmoComp::getAutoReload() const { return m_template->getAutoReload(); }
int   AmmoComp::getMag0Size() const { return m_magazines[0]; }
std::vector<int>* AmmoComp::getAmmo() { return &m_magazines; }

// . The magazine count, loaded magazine included.
int AmmoComp::getMagVectorSize() const { return static_cast<int>(m_magazines.size()); }

// . THE larger OF THE TWO times, not the reload time alone - a weapon whose
// minimum-time-until-reload exceeds its reload time reports the minimum.
float AmmoComp::getReloadTime() const
{
    const float minimum = m_template->getMinimumTimeUntilReload();
    const float reload = m_template->getReloadTime();
    return minimum > reload ? minimum : reload;
}


// dice::hfe::world::AmmoCompTemplate - 72 bytes, the load-time half, and the class every
// `ObjectTemplate.ammo.*` line writes into. Abstract: its createComponent refuses.
// every duration IS stored twice - once in seconds as the console wrote it, once as a tick
// count cached at (seconds * 30). The runtime never multiplies; it compares tick stamps. Five
// setters exist for exactly that reason, and a rebuild that stores only the float would have to
// re-derive the cache at every comparison - and would round differently, because these casts
// truncate toward zero.
AmmoCompTemplate::AmmoCompTemplate()
    : m_ammoType(0)
    , m_firstShotExtraTime(0.0f)
    , m_lastShotExtraTime(0.0f)
    , m_changeMagAt(0.7f)                 // 3F333333h in the image - 70% through the reload
    , m_autoReload(false)
    , m_instantReloadOnEnable(false)
    , m_nrOfMags(5)
    , m_magSize(30)
    , m_reloadAmount(0)
    , m_toggleWhenNoAmmo(false)
    , m_reloadWithoutPlayer(true)         // the one bool that starts true
{
    m_reloadTime = 2.0f;
    m_reloadTimeTimes30 = static_cast<int>(m_reloadTime * 30.0f);
    setMinimumTimeToReload(0.0f);
    setExtraReloadTimeFirst(0.0f);
    setExtraReloadTimeLast(0.0f);
}

unsigned int AmmoCompTemplate::getClassID() const { return CID_AmmoCompTemplate; }
unsigned int AmmoCompTemplate::getInterfaceID() { return IID_IWeaponAmmoCompTemplate; }

void AmmoCompTemplate::objectCreated(IObject*) {}
void AmmoCompTemplate::objectDestroyed(IObject*) {}
void AmmoCompTemplate::setTemplate(IObjectTemplate*) {}

void* AmmoCompTemplate::queryImplementation(unsigned int classId)
{
    if (classId == CID_AmmoCompTemplate)
        return this;
    return 0;
}

// createComponent - 0x82593D0, 239 B. NOT A stub - IT IS A diagnosed refusal. The base class is
// abstract in intent, and reaching this means a template registered AmmoCompTemplate itself rather
// than one of its two leaves. Both leaves override it.
IObjectComponent* AmmoCompTemplate::createComponent()
{
    Debug(DEBUGTYPE_6, "Game/Objects/Weapons/AmmoComponents/AmmoComp.h", 154, "") << "Virtual type";
    return 0;
}

// the five caching setters
// THE cast truncates. `reloadTime 0.99` gives 29 ticks, not 30.

// . THE only ONE that clamps, and it clamps to [0, 1] because it is a fraction of
// the reload, not a duration. A NaN takes the first arm and lands on 1.0.
void AmmoCompTemplate::setChangeMagAt(float value)
{
    float clamped = 1.0f;
    if (value <= 1.0f)
    {
        clamped = 0.0f;
        if (value >= 0.0f)
            clamped = value;
    }
    m_changeMagAt = clamped;
}

void AmmoCompTemplate::setReloadTime(float seconds)
{
    m_reloadTime = seconds;
    m_reloadTimeTimes30 = static_cast<int>(seconds * 30.0f);
}

void AmmoCompTemplate::setExtraReloadTimeFirst(float seconds)
{
    m_firstShotExtraTime = seconds;
    m_extraReloadTimeFirstTimes30 = static_cast<int>(seconds * 30.0f);
}

void AmmoCompTemplate::setExtraReloadTimeLast(float seconds)
{
    m_lastShotExtraTime = seconds;
    m_lastShotExtraTimeTimes30 = static_cast<int>(seconds * 30.0f);
}

void AmmoCompTemplate::setMinimumTimeToReload(float seconds)
{
    m_minimumTimeUntilReload = seconds;
    m_minimumTimeToReloadTimes30 = static_cast<int>(seconds * 30.0f);
}

// (1,996 B) / PE 0x5C1CF0 (855 B).  Both ports emit the same fourteen properties in
// this exact order and perform one final byte-counted write.  Notice the shipped asymmetry:
// reloadWithoutPlayer defaults true and is emitted when true, so a default template writes that
// one line; every other bool defaults false and is omitted.
bool AmmoCompTemplate::makeScript(io::IStream* stream)
{
    std::ostringstream script;

    if (m_ammoType != 0)
        script << "ObjectTemplate.ammo.ammoType " << m_ammoType << std::endl;
    if (m_nrOfMags != 5)
        script << "ObjectTemplate.ammo.nrOfMags " << m_nrOfMags << std::endl;
    if (m_magSize != 30)
        script << "ObjectTemplate.ammo.magSize " << m_magSize << std::endl;
    if (m_reloadAmount != 0)
        script << "ObjectTemplate.ammo.reloadAmount " << m_reloadAmount << std::endl;
    if (m_changeMagAt != 0.7f)
        script << "ObjectTemplate.ammo.changeMagAt " << m_changeMagAt << std::endl;
    if (m_reloadTime != 2.0f)
        script << "ObjectTemplate.ammo.reloadTime " << m_reloadTime << std::endl;
    if (m_firstShotExtraTime != 0.0f)
        script << "ObjectTemplate.ammo.firstShotExtraTime " << m_firstShotExtraTime << std::endl;
    if (m_lastShotExtraTime != 0.0f)
        script << "ObjectTemplate.ammo.lastShotExtraTime " << m_lastShotExtraTime << std::endl;
    if (m_minimumTimeUntilReload != 0.0f)
    {
        script << "ObjectTemplate.ammo.minimumTimeUntilReload "
               << m_minimumTimeUntilReload << std::endl;
    }
    if (m_autoReload)
        script << "ObjectTemplate.ammo.autoReload " << m_autoReload << std::endl;
    if (m_instantReloadOnEnable)
    {
        script << "ObjectTemplate.ammo.instantReloadOnEnable "
               << m_instantReloadOnEnable << std::endl;
    }
    if (!m_magLinkWeapon.empty())
        script << "ObjectTemplate.ammo.magLinkWeapon " << m_magLinkWeapon << std::endl;
    if (m_toggleWhenNoAmmo)
        script << "ObjectTemplate.ammo.toggleWhenNoAmmo " << m_toggleWhenNoAmmo << std::endl;
    if (m_reloadWithoutPlayer)
    {
        script << "ObjectTemplate.ammo.reloadWithoutPlayer "
               << m_reloadWithoutPlayer << std::endl;
    }

    const std::string text = script.str();
    return stream->write(text.c_str(), static_cast<unsigned int>(text.length())) == text.length();
}

} // namespace world
} // namespace hfe
} // namespace dice
