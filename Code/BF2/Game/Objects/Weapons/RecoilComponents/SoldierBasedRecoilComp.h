// Path:     [INFERRED] beside the attested weapon recoil family.
// Contents: complete SoldierBasedRecoilComp runtime/template contract recovered from BF2 1.50.

#ifndef GAME_OBJECTS_WEAPONS_RECOILCOMPONENTS_SOLDIERBASEDRECOILCOMP_H
#define GAME_OBJECTS_WEAPONS_RECOILCOMPONENTS_SOLDIERBASEDRECOILCOMP_H

#include <vector>

#include "WeaponBasedRecoilComp.h"

namespace dice {
namespace hfe {
namespace world {

class SoldierBasedRecoilCompTemplate;

class SoldierBasedRecoilComp : public RecoilComp
{
public:
    explicit SoldierBasedRecoilComp(SoldierBasedRecoilCompTemplate* objectTemplate);
    virtual ~SoldierBasedRecoilComp();

    virtual void addRef();
    virtual int getRef();
    virtual int release();
    virtual void* queryInterface(unsigned int iid) const;

    virtual void updateComponent(float seconds, unsigned int flags);
    virtual void postFire(WeaponFireEvent& event);
    virtual void setZoom(int zoom);
    virtual float getPitchRecoil();
    virtual float getYawRecoil();
    virtual bool getGoBackOnRecoil() const;
    virtual const RandomDistribution& getRandomRecoilForceUp() const;
    virtual const RandomDistribution& getRandomRecoilForceLeftRight() const;
    virtual bool getHasRandomRecoilForce() const;
    virtual float getRecoilForceModifier() const;
    virtual const std::vector<float>& getRecoilVector() const;

    virtual void disable();
    virtual void reset();

private:
    void calcRecoil();
    float getRecoil(float recoil);

    /* +0x5C */ int m_soldierRefCount;
    // Retail deliberately leaves the complete ten-entry history untouched in its constructor.
    /* +0x60 */ unsigned int m_recoilTicks[10];
    /* +0x88 */ int m_currentRecoilIndex;
    /* +0x8C */ bool m_zoomed;
    /* +0x8D */ unsigned char m_padding8D[3];
    /* +0x90 */ float m_pitchRecoil;
    /* +0x94 */ float m_yawRecoil;
    /* +0x98 */ SoldierBasedRecoilCompTemplate* m_soldierTemplate;
};

class SoldierBasedRecoilCompTemplate : public RecoilCompTemplate
{
public:
    SoldierBasedRecoilCompTemplate();
    virtual ~SoldierBasedRecoilCompTemplate();

    virtual void addRef();
    virtual int getRef();
    virtual int release();
    virtual void* queryInterface(unsigned int iid) const;
    virtual unsigned int getClassID() const;
    virtual IObjectComponent* createComponent();
    virtual void setTemplate(IObjectTemplate* objectTemplate);
    virtual void* queryImplementation(unsigned int classId);
    virtual bool makeScript(io::IStream* stream);

    // Accessors without an address comment are [CHOSEN] typed homes for recovered console stores
    // and runtime loads; only the address-labelled setters are separate retail functions.
    void setHasRecoilForce(bool value) { m_hasRecoilForce = value; }
    bool getHasRecoilForce() const { return m_hasRecoilForce; }
    void setGoBackOnRecoil(bool value);
    bool getGoBackOnRecoil() const { return m_goBackOnRecoil; }
    void setRecoilForceUp(const RandomDistribution& value) { m_recoilForceUp = value; }
    const RandomDistribution& getRecoilForceUp() const { return m_recoilForceUp; }
    void setRecoilForceLeftRight(const RandomDistribution& value)
    {
        m_recoilForceLeftRight = value;
    }
    const RandomDistribution& getRecoilForceLeftRight() const
    {
        return m_recoilForceLeftRight;
    }
    void setZoomModifier(float value) { m_zoomModifier = value; }
    float getZoomModifier() const { return m_zoomModifier; }
    void setRecoilGraphNumFrames(int value);
    int getRecoilGraphFrameCount() const { return m_recoilGraphFrameCount; }
    void setRecoilGraphExponent(float value);
    float getRecoilGraphExponent() const { return m_recoilGraphExponent; }
    void setRecoilGraphTotalMove(float value);
    float getRecoilGraphTotalMovement() const { return m_recoilGraphTotalMovement; }
    const std::vector<float>& getRecoilGraph() const { return m_recoilGraph; }
    const std::vector<float>& getRecoilGraphGoBack() const { return m_recoilGraphGoBack; }

    // [CHOSEN, not recovered] The recoilGraphFrameCount of templates created from now on, set by
    // pr.setRecoilGraphSize. Project Reality uses 5; retail hard-codes 8.
    static void setDefaultRecoilGraphFrameCount(int value) { s_defaultRecoilGraphFrameCount = value; }

private:
    void calcRecoilVectors();

    static int s_defaultRecoilGraphFrameCount;

    /* +0x08 */ int m_soldierRefCount;
    /* +0x0C */ RandomDistribution m_recoilForceUp;
    /* +0x20 */ RandomDistribution m_recoilForceLeftRight;
    /* +0x34 */ bool m_hasRecoilForce;
    /* +0x35 */ bool m_goBackOnRecoil;
    /* +0x36 */ unsigned char m_padding36[2];
    /* +0x38 */ float m_zoomModifier;
    /* +0x3C */ int m_recoilGraphFrameCount;
    /* +0x40 */ float m_recoilGraphExponent;
    /* +0x44 */ float m_recoilGraphTotalMovement;
    // STL source operations are recovered; concrete vector layout is compiler-specific.
    /* ELF +0x48 */ std::vector<float> m_recoilGraph;
    /* ELF +0x54 */ std::vector<float> m_recoilGraphGoBack;
};

#if defined(_M_IX86) || defined(__i386__)
static_assert(sizeof(SoldierBasedRecoilComp) == 0x9C,
              "SoldierBasedRecoilComp must match the shipped x86 layout");
#endif
#if defined(__i386__) && !defined(_MSC_VER)
static_assert(sizeof(SoldierBasedRecoilCompTemplate) == 0x60,
              "SoldierBasedRecoilCompTemplate must match the shipped ELF layout");
#endif

} // namespace world
} // namespace hfe
} // namespace dice

#endif // GAME_OBJECTS_WEAPONS_RECOILCOMPONENTS_SOLDIERBASEDRECOILCOMP_H
