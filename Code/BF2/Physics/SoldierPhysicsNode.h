// Infantry point mass with averaged contact friction and a 16-entry latency history.
// Header path [INFERRED]. Soldiers never sleep; rotation updates affect yaw only.
// Parented soldiers report zero local positional/rotational speed and use the root's tangent speed.
// Deck speed expires after 20 reads. Destruction releases the owned latency ring.

#ifndef PHYSICS_SOLDIERPHYSICSNODE_H     // [INFERRED] guard style
#define PHYSICS_SOLDIERPHYSICSNODE_H

#include "PointPhysicsNode.h"
#include "../Math/Quat.h"

namespace dice {
namespace hfe {
namespace world {

class SoldierPhysicsNode : public PointPhysicsNode
{
public:
    // The latency ring entry - 32 B, layout from initLatencyCompensation / reset / updateLatencyState.
    struct LatencyEntry
    {
        /* +0x00 */ Vec3  position;
        /* +0x0C */ Quat  rotation;                  // (0,0,0,1) at init and reset
        // Pose occupies one byte; initialization and reset clear it together with the three padding bytes.
        /* +0x1C */ unsigned char pose;
        /* +0x1D */ unsigned char pad[3];            // the entry is 32 B; zeroed with `pose`
    };

    SoldierPhysicsNode();
    virtual ~SoldierPhysicsNode();                   // 58 B - delete[] the ring

    virtual void  addRef();                          // the +0x90 counter
    virtual int   getRef();
    virtual int   release();
    virtual void* queryInterface(unsigned int) const;

    virtual IPhysicsNode* getParent() const;         // m_object->parent's node, or 0
    virtual void  setAbsoluteTransformation(const Mat4&);    // -> m_object
    virtual void  setAbsolutePosition(const Vec3&);          // -> m_object
    virtual const Mat4& getPrevTransformation() const;       // +0xC0, or the object's current
    virtual void  setPrevTransformation(const Mat4&);
    virtual const Vec3& getPositionalSpeed() const;          // zero while parented
    virtual const Vec3& getRotationalSpeed() const;          // zero while parented
    virtual void  setRotationalSpeed(const Vec3&);           // Y only, then snap
    virtual void  addRotationalSpeed(const Vec3&);           // Y only, then snap
    virtual void  setPositionalFriction(const Vec3&);        // +0xA0, snap
    virtual void  addFrictionAtAbsolutePosition(const Vec3&, const Vec3&);   // the running mean
    virtual const Vec3& getTangentSpeed(const Vec3&);        // root node's positional speed
    virtual const Vec3& getPositionalFriction() const;
    virtual void  updatePhysics(float dt);                   // 188 B - see the header
    virtual void  setHasSeparatePhysicsUpdate(bool);         // +0x60 (the base field, re-overridden)
    virtual void  reset(bool full);
    virtual bool  getIsSleeping() const;                     // false
    virtual void  setIsSleeping();                           // empty
    virtual void  setSleepiness(int);                        // empty
    virtual int   getSleepiness() const;                     // 1
    virtual void  setIsSleepier();                           // empty
    virtual void  setIsAwake();                              // -> PointPhysicsNode::setIsAwake
    virtual bool  getIsPoint() const;                        // false
    virtual void  initLatencyCompensation();                 // 104 B - the ring
    virtual bool  hasLatencyCompensation();
    virtual void  updateLatencyState();
    virtual const Mat4& getLatencyCompensatedTransformation(float, int*);
    virtual IPhysicsNode* getRootNode();                     // getRootParent(m_object)'s node

    // Non-virtual.
    void setCollidingMeshTangentSpeed(const Vec3&);
    void getCollidingMeshTangentSpeed(Vec3& out);
    void printDebug() const;                                 // empty
    float getDragMod(float underWater);                      // 1 - u + 25u
    void updatePositionalDragSimple(float, float);           // ELF 0x8375AA0 / PE 0x69EB30 (hides the base's)
    void updatePositionalDragAdvanced(float, float, float, float);
    void updatePositionalPhysics(float dt); // Hides the base implementation.

    // [CHOSEN, not recovered] Friction contacts added since the last updatePhysics; nonzero while
    // the soldier stands on something. Read by Python's soldier.getOnGround.
    int getFrictionCount() const { return m_frictionCount; }

protected:
    // Layout 0x90..0x10B, every default from the 238-byte ctor.
    /* +0x090 */ int     m_refCount;                 // ctor 1
    /* +0x094 */ Vec3    m_rotationalSpeed;          // 0
    /* +0x0A0 */ Vec3    m_positionalFriction;       // 0 (copied from the base's zeroed acceleration)
    /* +0x0AC */ int     m_frictionCount;            // 0 - the running mean's n
    /* +0x0B0 */ Vec3    m_collidingMeshTangentSpeed;// 0
    /* +0x0BC */ int     m_tangentSpeedTtl;          // 0; 20 after each set, -1 per get
    /* +0x0C0 */ Mat4    m_prevTransformation;       // identity
    /* +0x100 */ bool    m_prevTransformationIsObjects;   // true - as PhysicsNode's
    /* +0x104 */ LatencyEntry* m_latency;            // 0; [16] once initLatencyCompensation ran
    /* +0x108 */ int     m_latencyIndex;             // 0; the ring cursor, mod 16
};

} // namespace world
} // namespace hfe
} // namespace dice

#endif // PHYSICS_SOLDIERPHYSICSNODE_H
