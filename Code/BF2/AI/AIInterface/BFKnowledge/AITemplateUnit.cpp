
#include "AITemplateUnit.h"

#include <vector>

#include "AIObjectUnit.h"
#include "BFGame.h"                                          // IAIGame::getIsPrecaching (+0x68)
#include "IAIObject.h"
#include "IWeapon.h"

#include "../../AIDLL/AI/AIUtils/checkPointer.h"
#include "../../AIDLL/AI/Main/AISettingsInterfaces.h"   // IAISettings
#include "../../AIDLL/AI/Main/CommonSingletonServer.h"

#include "../../../Core/Ids.h"
#include "../../../Game/Objects/IObject.h"
#include "../../../Scene/Object/ObjectTypes/Game/IPlayerControlObject.h"

namespace dice {
namespace hfe {
namespace ai {

namespace {
// Every use re-reads the roster static (the image reloads ds: before each call), and
// none tests it: retail's ordering builds AISettings (BF2AI::init step 9) long before any object
// script runs.
IAISettings* settings()
{
    return static_cast<IAISettings*>(CommonSingletonServer::m_iaiSettings);
}

// Maps control-object ids 0..7 to position-select inputs 1..8. Other ids emit the
// original fatal diagnostic, with PINone following if diagnostics are disabled.
io::PlayerInputMap pcoIdToPositionSelect(int pcoId)
{
    switch (pcoId)
    {
    case 0: return io::PIPositionSelect1;
    case 1: return io::PIPositionSelect2;
    case 2: return io::PIPositionSelect3;
    case 3: return io::PIPositionSelect4;
    case 4: return io::PIPositionSelect5;
    case 5: return io::PIPositionSelect6;
    case 6: return io::PIPositionSelect7;
    case 7: return io::PIPositionSelect8;
    default:
        // [CHOSEN, not recovered] Project Reality: seats past the eighth (vehicles with nine or
        // more seats in coop) get the first seat's key. Retail stopped on a "No valid PcoId"
        // error here.
        return io::PIPositionSelect1;
    }
}
} // anonymous namespace

// Allocates battle then strategic strength arrays, zeroing strategic before battle.
// Re-queries the configured strength count for each loop iteration.
AITemplateUnit::AITemplateUnit()
    : m_maxAmmo(-1),
      m_battleStrengths(checkPointer(new int[settings()->getNBattleStrengths()])),
      m_strategicStrengths(checkPointer(new int[settings()->getNStrategicStrengths()])),
      m_equipmentType(777), m_initialized(false), m_useNoPathfindingToGetToObject(false),
      m_hasExposedSoldier(false), m_alwaysTargetSoldier(false), m_cullDistance(1000.0f),
      m_cullDistanceSquared(1000000.0f), m_selectKey(io::PIPositionSelect1)
{
    for (int i = 0; i < settings()->getNStrategicStrengths(); ++i)
        m_strategicStrengths[i] = 0;
    for (int i = 0; i < settings()->getNBattleStrengths(); ++i)
        m_battleStrengths[i] = 0;
}

// the battle array, then the strategic one.
AITemplateUnit::~AITemplateUnit()
{
    delete[] m_battleStrengths;
    delete[] m_strategicStrengths;
}

// createPlugIn - , : `new AIObjectUnit(object, this)` (0x30 B) through checkPointer.
IAIObjectPlugIn* AITemplateUnit::createPlugIn(IAIObject* object, AITemplate* /*aiTemplate*/)
{
    return checkPointer(new AIObjectUnit(object, this));
}

// Initializes ammunition from the object hierarchy until a control object completes template initialization.
void AITemplateUnit::initFromObject(world::IObject* object)
{
    if (m_initialized)
        return;

    m_maxAmmo = 0;
    const int battleCount = settings()->getNBattleStrengths();                      // +0x34
    for (int i = 0; i < battleCount; ++i)
        m_battleStrengths[i] = 0;
    traverseObjects(object);

    IAIObject* const aiObject = static_cast<IAIObject*>(object->getAIComponent());   // +0x34
    if (aiObject->isPrimary())                                                      // +0xF8
    {
        world::IPlayerControlObject* const pco = static_cast<world::IPlayerControlObject*>(
            object->queryInterface(world::IID_IPlayerControlObject));
        m_selectKey = pcoIdToPositionSelect(pco->getPcoId());                       // +0x30
        int iterator;
        for (IAIObject* secondary = aiObject->getFirstSecondaryObject(iterator); secondary;   // +0x11C
             secondary = aiObject->getSecondaryObject(iterator))                    // +0x114
        {
            world::IPlayerControlObject* const seat = static_cast<world::IPlayerControlObject*>(
                secondary->getObject()->queryInterface(world::IID_IPlayerControlObject));
            const io::PlayerInputMap key = pcoIdToPositionSelect(seat->getPcoId());
            static_cast<AIObjectUnit*>(secondary->getUnit())->m_template->m_selectKey = key;   // +0x98
        }
    }
}

// Initializes once outside precaching from the player-control object's own weapons.
// Adds nonnegative ammunition counts and raises battle strengths to weapon maxima.
// Does not visit child objects.
void AITemplateUnit::traverseObjects(world::IObject* object)
{
    if (m_initialized ||
        static_cast<IAIGame*>(CommonSingletonServer::m_iaiGame)->getIsPrecaching())     // +0x68
        return;
    if (!object->queryInterface(world::IID_IObject))
        return;
    world::IPlayerControlObject* const pco = static_cast<world::IPlayerControlObject*>(
        object->queryInterface(world::IID_IPlayerControlObject));
    if (!pco)
        return;

    m_initialized = true;
    const std::vector<world::IObject*>* const weapons =
        static_cast<const std::vector<world::IObject*>*>(pco->getWeapons());       // ELF +0x6C
    const int battleCount = settings()->getNBattleStrengths();
    for (std::vector<world::IObject*>::const_iterator it = weapons->begin(); it != weapons->end(); ++it)
    {
        IWeapon* const weapon =
            static_cast<IWeapon*>((*it)->queryComponent(IID_IWeapon, IID_IWeapon));  // +0x5C
        if (!weapon)
            continue;
        const int ammo = weapon->getAmmo();                                         // +0xB8
        if (ammo)
        {
            if (ammo >= 0)
                m_maxAmmo += ammo;
            for (int j = 0; j < battleCount; ++j)
            {
                const int mine = m_battleStrengths[j];
                const int theirs = weapon->getStrength(j);                          // +0xA4
                m_battleStrengths[j] = (mine >= theirs) ? mine : theirs;
            }
        }
    }
}

// `mov eax, 5`.
int AITemplateUnit::getId()
{
    return 5;
}

// getEquipmentTypeName - , : IAISettings slot 20 (PE slot 21 because of the ABI's
// destructor-slot difference), getVehicle(int), on the stored id, returned as it comes back.
// The return type is now paired across the DLL body and this engine consumer: it is a borrowed
// reference to the settings object's vehicle-name vector element.
const std::string& AITemplateUnit::getEquipmentTypeName()
{
    return settings()->getVehicle(m_equipmentType);
}

// IAISettings slot 21, getVehicle(const std::string&) - -1 for an unknown name.
void AITemplateUnit::setEquipmentTypeName(const std::string& name)
{
    m_equipmentType = settings()->getVehicle(name);
}

} // namespace ai
} // namespace hfe
} // namespace dice
