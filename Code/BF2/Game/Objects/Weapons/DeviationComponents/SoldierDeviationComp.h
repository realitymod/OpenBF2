// Path:     attested by the retail Debug diagnostic in SoldierDeviationComp.cpp.
// Contents: complete soldier movement/pose deviation runtime recovered from BF2 1.50.

#ifndef GAME_OBJECTS_WEAPONS_DEVIATIONCOMPONENTS_SOLDIERDEVIATIONCOMP_H
#define GAME_OBJECTS_WEAPONS_DEVIATIONCOMPONENTS_SOLDIERDEVIATIONCOMP_H

#include "DeviationComp.h"

namespace dice {
namespace hfe {
namespace world {

class SoldierDeviationComp : public DeviationComp
{
public:
    explicit SoldierDeviationComp(SoldierDeviationCompTemplate* objectTemplate);
    virtual ~SoldierDeviationComp();

    virtual void addRef();
    virtual int getRef();
    virtual int release();
    virtual void* queryInterface(unsigned int iid) const;
    virtual unsigned int getInterfaceID() const;

    virtual void updateComponent(float seconds, unsigned int flags);
    virtual void updateDeviation(io::PlayerInput input, bool movementState, bool firing);
    virtual void setZoom(int zoom);
    virtual void disable();
    virtual void reset();
    virtual float getMinDeviation() const;
    virtual float getMaxDeviation() const;
    virtual void setAIDeviation(float deviation);

    // [CHOSEN, not recovered] Project Reality's scripts read and set the decaying turn, speed and
    // misc penalties (deviation on weapon switch) and their template limits.
    void getPenalties(float& turn, float& speed, float& misc) const
        { turn = m_turnDeviation; speed = m_speedDeviation; misc = m_miscDeviation; }
    void setPenalties(float turn, float speed, float misc)
        { m_turnDeviation = turn; m_speedDeviation = speed; m_miscDeviation = misc; }
    const SoldierDeviationCompTemplate* getSoldierTemplate() const { return m_soldierTemplate; }

private:
    /* +0x2C */ int m_soldierRefCount;
    /* +0x30 */ float m_turnDeviation;
    /* +0x34 */ float m_speedDeviation;
    /* +0x38 */ float m_miscDeviation;
    /* +0x3C */ float m_poseModifier;
    /* +0x40 */ float m_zoomModifier;
    /* +0x44 */ bool m_zoomed;
    /* +0x45 */ unsigned char m_padding45[3];
    /* +0x48 */ Random m_soldierRandom; // Retail constructs it; no recovered body samples it.
    /* +0x4C */ float m_aiDeviation;
    /* +0x50 */ SoldierDeviationCompTemplate* m_soldierTemplate;
};

#if defined(_M_IX86) || defined(__i386__)
static_assert(sizeof(SoldierDeviationComp) == 0x54,
              "SoldierDeviationComp must match the shipped x86 layout");
#endif

} // namespace world
} // namespace hfe
} // namespace dice

#endif // GAME_OBJECTS_WEAPONS_DEVIATIONCOMPONENTS_SOLDIERDEVIATIONCOMP_H
