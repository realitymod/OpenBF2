// Path:     [INFERRED] Game/Objects/Weapons/RecoilComponents/SoldierBasedRecoilComp.cpp.
// Contents: complete soldier recoil family recovered from ELF 0x827CAD0..0x827E43A and
// cross-checked against PE 0x5A81A0..0x5A8768.

#include "SoldierBasedRecoilComp.h"

#include <cmath>
#include <sstream>
#include <string>

#include "../GenericFireArm.h"
#include "../../IObject.h"
#include "../../../../Core/Ids.h"
#include "../../../../Core/System.h"
#include "../../../IGame.h"
#include "../../../../IO/Stream/IStream.h"

namespace dice {
namespace hfe {
namespace world {

namespace {

float randomUnit(Random& random)
{
    const unsigned int raw = ((random.getRaw() >> 7) | 1u) + 1u;
    return static_cast<float>(static_cast<int>(raw)) * 0.000000059604645f;
}

float sampleDistribution(const RandomDistribution& distribution, Random& random)
{
    float result;
    switch (distribution.distribution)
    {
    case CRD_NONE:
        result = distribution.value;
        break;
    case CRD_UNIFORM:
        // Retail treats `value` and `variation` as the two endpoints, not centre/radius.
        result = distribution.value
               + (distribution.variation - distribution.value) * randomUnit(random);
        break;
    case CRD_EXPONENTIAL:
        result = -distribution.value * std::log(randomUnit(random));
        break;
    case CRD_NORMAL:
        result = distribution.value + random.getNormal() * distribution.variation;
        break;
    default:
        result = 0.0f;
        break;
    }

    if (distribution.relative && randomUnit(random) <= 0.5f)
        result = -result;
    return result;
}

} // anonymous namespace

SoldierBasedRecoilComp::SoldierBasedRecoilComp(
    SoldierBasedRecoilCompTemplate* objectTemplate)
    : RecoilComp(objectTemplate)
    , m_soldierRefCount(1)
    , m_currentRecoilIndex(-1)
    , m_zoomed(false)
    , m_pitchRecoil(0.0f)
    , m_yawRecoil(0.0f)
    , m_soldierTemplate(objectTemplate)
{
    // The three alignment bytes are deliberately untouched, as shipped. [CHOSEN, not recovered]
    // Retail leaves m_recoilTicks untouched too, but getRecoil now reads every entry, so unfired
    // entries start at tick 0, which is always too old to count.
    for (int i = 0; i < 10; ++i)
        m_recoilTicks[i] = 0;
}

SoldierBasedRecoilComp::~SoldierBasedRecoilComp()
{
}

void SoldierBasedRecoilComp::addRef() { ++m_soldierRefCount; }
int SoldierBasedRecoilComp::getRef() { return m_soldierRefCount; }

int SoldierBasedRecoilComp::release()
{
    const int remaining = --m_soldierRefCount;
    if (remaining == 0)
        delete this;
    return remaining;
}

void* SoldierBasedRecoilComp::queryInterface(unsigned int iid) const
{
    SoldierBasedRecoilComp* const self = const_cast<SoldierBasedRecoilComp*>(this);
    if (iid == IID_IBase || iid == IID_IObjectComponent)
        return static_cast<IObjectComponent*>(self);
    if (iid == IID_IWeaponComp || iid == IID_IWeaponRecoilComp)
        return static_cast<IWeaponRecoilComp*>(self);
    return 0;
}

// .  TickCount and the parent/template tests are intentionally repeated
// for every matching history entry; duplicate ticks therefore recalculate recoil more than once.
void SoldierBasedRecoilComp::updateComponent(float, unsigned int flags)
{
    if ((flags & 4u) == 0 || !getHasRandomRecoilForce())
        return;

    for (int i = 0; i != 10; ++i)
    {
        if (m_recoilTicks[i] == static_cast<unsigned int>(WorldPref::getTickCount()))
        {
            IObject* const parent = m_initObject->getParent();
            if (parent && parent->getTemplate()
                && parent->getTemplate()->getClassID() == CID_SoldierTemplate)
                calcRecoil();
        }
    }
}

// .  One Lehmer draw is deliberately discarded after seeding so the
// two distributions start at the same points as retail prediction.
void SoldierBasedRecoilComp::calcRecoil()
{
    if (!getHasRandomRecoilForce())
        return;

    Random random;
    random.m_seed = static_cast<unsigned int>(game->getCurrentInputIndex());
    (void)random.getRaw();

    m_pitchRecoil = sampleDistribution(getRandomRecoilForceUp(), random)
                  * getRecoilForceModifier();
    m_yawRecoil = sampleDistribution(getRandomRecoilForceLeftRight(), random)
                * getRecoilForceModifier();
}

float SoldierBasedRecoilComp::getPitchRecoil()
{
    return getRecoil(m_pitchRecoil);
}

float SoldierBasedRecoilComp::getYawRecoil()
{
    return getRecoil(m_yawRecoil);
}

// [CHOSEN, not recovered] Project Reality's cumulative recoil. Retail samples the graph for the
// newest shot only and scales by the firearm's getDevMod (always 1.0). PR sums the graph over the
// last 10 shots: pull-back (negative) frames from every shot, kick frames from the newest only. A
// shot fired this tick does not count yet, so the first kick lands one tick later. The graph is
// stored reversed, first frame last.
// [RETIRE-WHEN: rhooks-recoil] Also computed by the PR client's rhooks (getRecoilHook.cpp), which
// predicts with exactly this formula and without getDevMod. Keep it identical while old clients
// connect.
float SoldierBasedRecoilComp::getRecoil(float recoil)
{
    if (recoil == 0.0f)
        return 0.0f;

    const int newest = m_currentRecoilIndex;
    if (newest < 0)
        return 0.0f;

    const std::vector<float>& graph = getRecoilVector();
    const unsigned int frames = static_cast<unsigned int>(graph.size());
    const unsigned int now = static_cast<unsigned int>(WorldPref::getTickCount());
    if (now - m_recoilTicks[newest] > frames)
        return 0.0f;

    float sum = 0.0f;
    for (int i = 0; i < 10; ++i)
    {
        // Unsigned on purpose: a shot this tick or older than the graph gives an index >= frames.
        const unsigned int frame = frames - (now - m_recoilTicks[i]);
        if (frame >= frames)
            continue;
        const float value = graph[frame];
        if (value < 0.0f || i == newest)
            sum += value;
    }
    return recoil * sum;
}

// .  The ancestor walk starts at the weapon's parent (not the weapon),
// and the freshly calculated sample precedes both the ring advance and its tick write.
void SoldierBasedRecoilComp::postFire(WeaponFireEvent&)
{
    IObject* soldierObject = 0;
    for (IObject* object = m_initObject->getParent(); object && !soldierObject;
         object = object->getParent())
    {
        if (object->queryInterface(IID_ISoldier))
            soldierObject = object;
    }

    if (getHasRandomRecoilForce() && soldierObject)
    {
        calcRecoil();
        ++m_currentRecoilIndex;
        if (m_currentRecoilIndex > 9)
            m_currentRecoilIndex = 0;
        m_recoilTicks[m_currentRecoilIndex] =
            static_cast<unsigned int>(WorldPref::getTickCount());
    }
}

void SoldierBasedRecoilComp::setZoom(int zoom)
{
    m_zoomed = zoom > 0;
}

void SoldierBasedRecoilComp::reset()
{
    m_zoomed = false;
}

void SoldierBasedRecoilComp::disable()
{
    m_zoomed = false;
}

const std::vector<float>& SoldierBasedRecoilComp::getRecoilVector() const
{
    return m_soldierTemplate->getGoBackOnRecoil()
        ? m_soldierTemplate->getRecoilGraphGoBack()
        : m_soldierTemplate->getRecoilGraph();
}

bool SoldierBasedRecoilComp::getGoBackOnRecoil() const
{
    return m_soldierTemplate->getGoBackOnRecoil();
}

const RandomDistribution& SoldierBasedRecoilComp::getRandomRecoilForceUp() const
{
    return m_soldierTemplate->getRecoilForceUp();
}

const RandomDistribution& SoldierBasedRecoilComp::getRandomRecoilForceLeftRight() const
{
    return m_soldierTemplate->getRecoilForceLeftRight();
}

bool SoldierBasedRecoilComp::getHasRandomRecoilForce() const
{
    return m_soldierTemplate->getHasRecoilForce();
}

float SoldierBasedRecoilComp::getRecoilForceModifier() const
{
    return m_zoomed ? m_soldierTemplate->getZoomModifier() : 1.0f;
}

SoldierBasedRecoilCompTemplate::SoldierBasedRecoilCompTemplate()
    : RecoilCompTemplate()
    , m_soldierRefCount(1)
    , m_recoilForceUp()
    , m_recoilForceLeftRight()
    , m_hasRecoilForce(false)
    , m_goBackOnRecoil(true)
    , m_zoomModifier(1.0f)
    , m_recoilGraphFrameCount(s_defaultRecoilGraphFrameCount)
    , m_recoilGraphExponent(2.0f)
    , m_recoilGraphTotalMovement(2.0f)
    , m_recoilGraph()
    , m_recoilGraphGoBack()
{
    m_recoilForceLeftRight.value = 0.0f;
    // +0x36/+0x37 remain untouched alignment padding in the shipped constructor.
}

int SoldierBasedRecoilCompTemplate::s_defaultRecoilGraphFrameCount = 5;   // [CHOSEN, not recovered]

SoldierBasedRecoilCompTemplate::~SoldierBasedRecoilCompTemplate()
{
}

void SoldierBasedRecoilCompTemplate::addRef() { ++m_soldierRefCount; }
int SoldierBasedRecoilCompTemplate::getRef() { return m_soldierRefCount; }

int SoldierBasedRecoilCompTemplate::release()
{
    const int remaining = --m_soldierRefCount;
    if (remaining == 0)
        delete this;
    return remaining;
}

void* SoldierBasedRecoilCompTemplate::queryInterface(unsigned int iid) const
{
    SoldierBasedRecoilCompTemplate* const self =
        const_cast<SoldierBasedRecoilCompTemplate*>(this);
    if (iid == IID_IBase || iid == IID_IObjectTemplateComponent
        || iid == IID_IWeaponCompTemplate)
        return static_cast<IWeaponCompTemplate*>(self);
    if (iid == io::IID_IConsoleSaveable)
        return static_cast<io::IConsoleSaveable*>(self);
    return 0;
}

unsigned int SoldierBasedRecoilCompTemplate::getClassID() const
{
    return CID_SoldierBasedRecoilCompTemplate;
}

IObjectComponent* SoldierBasedRecoilCompTemplate::createComponent()
{
    return new SoldierBasedRecoilComp(this);
}

void* SoldierBasedRecoilCompTemplate::queryImplementation(unsigned int classId)
{
    if (classId == CID_SoldierBasedRecoilCompTemplate || classId == CID_RecoilCompTemplate)
        return this;
    return 0;
}

void SoldierBasedRecoilCompTemplate::setRecoilGraphNumFrames(int value)
{
    m_recoilGraphFrameCount = value;
}

void SoldierBasedRecoilCompTemplate::setRecoilGraphExponent(float value)
{
    m_recoilGraphExponent = value;
}

void SoldierBasedRecoilCompTemplate::setRecoilGraphTotalMove(float value)
{
    m_recoilGraphTotalMovement = value;
}

void SoldierBasedRecoilCompTemplate::setGoBackOnRecoil(bool value)
{
    m_goBackOnRecoil = value;
}

void SoldierBasedRecoilCompTemplate::setTemplate(IObjectTemplate*)
{
    calcRecoilVectors();
}

// , 470 B.  No validation is added: zero frame count,
// zero total movement and indices produced from authored negative values retain retail behavior.
void SoldierBasedRecoilCompTemplate::calcRecoilVectors()
{
    m_recoilGraph.resize(static_cast<std::size_t>(m_recoilGraphFrameCount), 0.0f);

    float total = 0.0f;
    for (int i = 0; i < m_recoilGraphFrameCount; ++i)
        total += std::pow(static_cast<float>(i) + 1.0f, m_recoilGraphExponent);

    const float divisor = total / m_recoilGraphTotalMovement;
    for (int i = 0; i < m_recoilGraphFrameCount; ++i)
    {
        m_recoilGraph[static_cast<std::size_t>(i)] =
            -std::pow(static_cast<float>(i) + 1.0f, m_recoilGraphExponent) / divisor;
    }

    if (m_goBackOnRecoil)
    {
        const float halfLast = -0.5f
            * m_recoilGraph[static_cast<std::size_t>(m_recoilGraphFrameCount - 1)];
        const int extraFrames = static_cast<int>(
            static_cast<float>(m_recoilGraphFrameCount) * 0.25f);
        const int graphSize = m_recoilGraphFrameCount + extraFrames;
        m_recoilGraphGoBack.resize(static_cast<std::size_t>(graphSize), 0.0f);

        float positiveTotal = 0.0f;
        float nonPositiveTotal = 0.0f;
        for (int i = 0; i < graphSize; ++i)
        {
            float value;
            if (i >= extraFrames)
            {
                value = 1.5f * m_recoilGraph[static_cast<std::size_t>(i - extraFrames)]
                      + halfLast;
            }
            else
            {
                value = halfLast
                      - 1.5f * m_recoilGraph[static_cast<std::size_t>(extraFrames - i)];
            }
            m_recoilGraphGoBack[static_cast<std::size_t>(i)] = value;
            if (value <= 0.0f)
                nonPositiveTotal += value;
            else
                positiveTotal += value;
        }

        const float positiveScale = positiveTotal / -nonPositiveTotal;
        for (int i = 0; i < graphSize; ++i)
        {
            float& value = m_recoilGraphGoBack[static_cast<std::size_t>(i)];
            if (value > 0.0f)
                value /= positiveScale;
        }
    }
    // When goBackOnRecoil is false, retail deliberately leaves any prior go-back graph intact.
}

bool SoldierBasedRecoilCompTemplate::makeScript(io::IStream* stream)
{
    std::ostringstream script;
    if (m_hasRecoilForce)
    {
        script << "ObjectTemplate.recoil.hasRecoilForce 1" << std::endl;
        if (!m_goBackOnRecoil)
            script << "ObjectTemplate.recoil.goBackOnRecoil 0" << std::endl;
        if (m_recoilForceUp.distribution != CRD_NONE || m_recoilForceUp.value != 1.0f)
            script << "ObjectTemplate.recoil.recoilForceUp " << m_recoilForceUp << std::endl;
        if (m_recoilForceLeftRight.distribution != CRD_NONE
            || m_recoilForceLeftRight.value != 0.0f)
        {
            script << "ObjectTemplate.recoil.recoilForceLeftRight "
                   << m_recoilForceLeftRight << std::endl;
        }
        if (m_zoomModifier != 1.0f)
            script << "ObjectTemplate.recoil.zoomModifier " << m_zoomModifier << std::endl;
        if (m_recoilGraphFrameCount != s_defaultRecoilGraphFrameCount)
        {
            script << "ObjectTemplate.recoil.recoilGraphFrameCount "
                   << m_recoilGraphFrameCount << std::endl;
        }
        if (m_recoilGraphExponent != 2.0f)
        {
            script << "ObjectTemplate.recoil.recoilGraphExponent "
                   << m_recoilGraphExponent << std::endl;
        }
        if (m_recoilGraphTotalMovement != 2.0f)
        {
            script << "ObjectTemplate.recoil.recoilGraphTotalMovement "
                   << m_recoilGraphTotalMovement << std::endl;
        }
    }

    const std::string text = script.str();
    return stream->write(text.c_str(), static_cast<unsigned int>(text.length())) == text.length();
}

} // namespace world
} // namespace hfe
} // namespace dice
