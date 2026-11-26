// Static explosion handling and deferred damage. Eight parallel vectors form each
// of two queues; flip the active index before draining so new explosions append
// to the other buffer. Resolve weak handles, apply damage, then clear every vector
// in the drained buffer. Position deltas point from explosion to target.

#ifndef GAME_GAMELOGIC_GAMELOGICEXPLOSION_H     // [INFERRED] guard style
#define GAME_GAMELOGIC_GAMELOGICEXPLOSION_H

#include <string>
#include <vector>

#include "../../Core/WeakPtr.h"
#include "../../Math/Vec3.h"

namespace dice {
namespace hfe {
namespace world {

class IObject;
class IPlayer;
class IArmorGLComp;

class GameLogicExplosion
{
public:
    // ALL static. There is no vtable and nothing constructs one of these.
    static void serverUpdateExplosionDamage();
    static bool serverHandleExplosion(const Vec3&, float, int, float, IPlayer*,
                                      IArmorGLComp*, int, IObject*, IObject*, float,
                                      IObject*, IObject*, float, bool, bool);
    static bool serverHandleExplosionOnObject(IObject*, const Vec3&, const Vec3&,
                                              const Vec3&, float, float, int, float,
                                              IPlayer*, IArmorGLComp*, int, IObject*,
                                              IObject*, IObject*, float, float&);
    //  / PE 0x56D420. The static helper takes const Vec3&, unlike its
    // ClientGameLogic virtual caller's by-value parameter (confirmed by ELF mangling).
    static void clientHandleExplosion(const Vec3&, float, int, float, IPlayer*, IArmorGLComp*, int,
                                      IObject*, IObject*, float, IObject*, IObject*, float, bool, bool);
    static void handleExplosion(const Vec3&, float, int, float, IPlayer*, IArmorGLComp*, int,
                                IObject*, IObject*, float, IObject*, IObject*, float,
                                bool, bool);
    static bool handleExplosionForceOnObject(IObject*, const Vec3&, const Vec3&,
                                             const Vec3&, float, float, IArmorGLComp*,
                                             float, IObject*, float, float, bool);
    static float checkExplosionOnSoldier(const Vec3&, IObject*, float, IObject*);
    static float getExplosionRadiusMod(float radius, float distance);

    // [CHOSEN, not recovered] Project Reality's anti-air override, not in retail: explosions of
    // projectiles with this template are not latency-compensated, and the projectile is removed two
    // ticks later. The name is matched case-insensitively.
    static void disableLatencyCompensationFor(const std::string& templateName);

    // The queue, and the buffer it is currently filling. Every name is the ELF's own symbol.
    static std::vector<WeakPtr<IObject> > m_explosionObject[2];
    static std::vector<WeakPtr<IObject> > m_explosionSourceObject[2];
    static std::vector<float>             m_explosionDamage[2];
    static std::vector<WeakPtr<IPlayer> > m_explosionPlayer[2];
    static std::vector<unsigned int>      m_explosionTeam[2];
    static std::vector<WeakPtr<IObject> > m_explosionWeapon[2];
    static std::vector<WeakPtr<IObject> > m_explosionVehicle[2];
    static std::vector<Vec3>              m_explosionPos[2];
    static int                            m_currentExplosion;
};

} // namespace world
} // namespace hfe
} // namespace dice

#endif // GAME_GAMELOGIC_GAMELOGICEXPLOSION_H
