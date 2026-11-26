// Particle-and-constraint ragdoll using fixed-step Verlet integration.
// The server owns networked particle positions; client mode interpolates those particles
// While simulating the remaining ones. applyOnSkeleton maps the result back to bone transforms.
// The awake flag is true while simulating and false after settling.

#ifndef ANIMATION_BONEANIMATION_RAGDOLL_H     // [INFERRED] guard style
#define ANIMATION_BONEANIMATION_RAGDOLL_H

#include <vector>

#include "../../Math/Vec3.h"
#include "../../Math/Mat4.h"

namespace dice {

namespace hfe {
    namespace io { class BitStream; }
    namespace world { class IObject; }
}

namespace anim {

class RagDollTemplate;
class Skeleton;

class RagDoll
{
public:

    // Particle state. Defaults use unit mass and rd_boneSize; reset places template particles on the skeleton.

    struct Particle
    {
        Particle();
        Particle(const hfe::Vec3& position, int bone, float mass, float size);

        /* +0x00 */ hfe::Vec3 m_position;
        /* +0x0C */ hfe::Vec3 m_oldPosition;      // verlet's previous position
        /* +0x18 */ hfe::Vec3 m_acceleration;     // rebuilt every step by accumulateForces
        /* +0x24 */ hfe::Vec3 m_lastMove;         // the previous step's displacement (verlet averages it in)
        /* +0x30 */ hfe::Vec3 m_netPosition;      // a client's interpolation target (readCurrentState)
        /* +0x3C */ hfe::Vec3 m_force;            // addForce accumulates here; verlet clears it
        /* +0x48 */ int   m_bone;                 // the skeleton bone id this particle stands for
        /* +0x4C */ float m_mass;                 // ctor 1.0 - the .con's second argument
        /* +0x50 */ float m_size;                 // capsule radius - rd_boneSize (1.5x for knees, body)
        /* +0x54 */ bool  m_netControlled;        // reset(): the bone is in the rd_netBoneEnable mask
        /* +0x55 */ bool  m_collided;             // checkCollisionConstraints: hit something this round
        /* +0x56 */ bool  m_byte56;               // never written - padding the copies carry along
        /* +0x57 */ bool  m_byte57;
    };

    // Records the collision the soldier's sound arm reads (getParticleCollision): +0x38 the impact
    // strength, +0x3C the material hit. Laid out as the two adjacent words the ELF keeps.
    struct ParticleCollision
    {
        float m_strength;
        int   m_material;
    };

    explicit RagDoll(RagDollTemplate& templ);
    ~RagDoll();

    // Restarts the sleep timer from rd_sleep and marks the ragdoll awake.
    void wakeUp();
    // bone >= 0: the force goes on that bone's particle. bone < 0: a "hit from the front" -
    // every particle takes the force, scaled by rd_expRotateForce, the head/spine/hip particles
    // (1, 2, 4, 6, 7, 9) taking it backwards along the ragdoll's forward axis instead. one-shot:
    // the first force after construction latches +0x18 and later ones are ignored.
    void addForce(int bone, const hfe::Vec3& force);
    // The verlet velocity (position - old position) of a bone's particle, or the shared zero.
    // Returned through a function-local static, as in the ELF.
    const hfe::Vec3& getParticleSpeed(int bone);
    // Hands out the pending collision once and clears it; null when the template's criteria are
    // off (criteria1 < 0) or nothing is pending.
    const ParticleCollision* getParticleCollision();

    void verlet(float dt);
    void accumulateForces();
    void addHitImpact(Particle& particle, const hfe::Vec3& normal, int material);
    void checkCapsuleConstraints();
    void getCollidingObjects();
    static void restoreCollidingObjects();
    void checkCollisionCapsulesAgainstObjects();
    void checkCollisionConstraints();
    void checkDistanceConstraints();
    void checkDistanceLessThanConstraints();
    void drawDebugLine(const hfe::Vec3& from, const hfe::Vec3& to,
                       const hfe::Vec4& colour);                       // [FRONTIER] stop
    void checkAngularConstraints();
    void checkLegAngularConstraints();
    void checkDihedralAngleConstraints();
    void satisfyConstraints();
    void drawConstraints();                                            // [FRONTIER] stop
    hfe::Vec3 getForward();
    hfe::Vec3 getRight();
    hfe::Vec3 getUp();
    // Rebuilds the particle set from the template, placing every particle on its bone's absolute
    // position under `transform`, and seeds the verlet velocity from `velocity` (0.015 s of it).
    void reset(const hfe::Mat4& transform, const hfe::Vec3& velocity);
    // the PER-frame step - see the .cpp. Returns the ragdoll's centre (the hip particle), which
    // is what Soldier::enableRagDoll parks the object at.
    const hfe::Vec3& update(float dt);
    void setIsClient(bool isClient);
    static void skipCurrentState(hfe::io::BitStream* stream);
    void writeCurrentState(hfe::io::BitStream* stream);
    void readCurrentState(hfe::io::BitStream* stream);
    // The particle of bone 6 (the hip). aborts when the template has none.
    Particle& getCenter();
    void applyOnSkeleton();
    // The lowest of the two feet (9, 4) and the head (47).
    hfe::Vec3 getLowestVisiblePosition();

    // [CHOSEN, not recovered] Project Reality's body dragging. While a drag is set, every step
    // pulls particle `particle` toward `target` with an acceleration of
    // acceleration * distance^exponent; setDrag also wakes the ragdoll. An out-of-range particle
    // index is ignored by the step.
    void setDrag(int particle, const hfe::Vec3& target, float acceleration, float exponent);
    void clearDrag();
    void applyDrag();

    // Shared collision candidates for one constraint solve; cleared by restoreCollidingObjects.
    static std::vector<hfe::world::IObject*> m_objects;

    // Public state consumed directly by templates, soldier lifecycle, and replication.
    /* +0x00 */ bool  m_byte00;                 // ctor 0. [UNRESOLVED] - nothing recovered reads it
    /* +0x04 */ std::vector<Particle> m_particles;   // begin +0x04, end +0x08, cap +0x0C
    /* +0x10 */ RagDollTemplate* m_template;
    /* +0x14 */ Skeleton* m_skeleton;           // not ctor-written; makeInstance stores its argument
    /* +0x18 */ bool  m_forceApplied;           // addForce's one-shot latch
    /* +0x19 */ bool  m_awake;                  // ctor 1 - see the file block
    /* +0x1A */ bool  m_byte1A;                 // not ctor-written. [UNRESOLVED]
    /* +0x1B */ bool  m_byte1B;                 // not ctor-written. [UNRESOLVED]
    /* +0x1C */ hfe::Vec3 m_position;           // the centre at the last update
    /* +0x28 */ hfe::Vec3 m_vec28;              // ctor 0. [UNRESOLVED] - no recovered reader
    /* +0x34 */ float m_sleepTimer;             // wakeUp: rd_sleep; runs down while still
    /* +0x38 */ ParticleCollision m_particleCollision;   // +0x38 strength, +0x3C material
    /* +0x40 */ bool  m_hasCollision;           // addHitImpact sets, getParticleCollision clears
    /* +0x41 */ bool  m_byte41;                 // not ctor-written
    /* +0x42 */ bool  m_byte42;
    /* +0x43 */ bool  m_byte43;
    /* +0x44 */ float m_collisionTimer;         // ctor 2 x criteria2 - the effect debounce
    /* +0x48 */ float m_stepAccumulator;        // frame time not yet stepped (asleep: the poll timer)
    /* +0x4C */ float m_gravityOverTime;        // ctor: the rd_gravityOverTime cvar
    /* +0x50 */ float m_gravityTime;            // ramps 0..m_gravityOverTime
    /* +0x54 */ int   m_updateMode;             // 0 full, 1 no capsule/object collision, 2 (client) skip
    /* +0x58 */ float m_clientTimer;            // readCurrentState: 0.3 s until setIsClient(false)
    /* +0x5C */ bool  m_exited;                 // Soldier::exit sets it; update() then only tracks the centre
    /* +0x5D */ bool  m_isClient;
    /* +0x5E */ bool  m_firstNetState;          // ctor 1; the first readCurrentState snaps positions
    /* +0x5F */ bool  m_settled;                // set the first time a particle hits anything

    // [CHOSEN, not recovered] setDrag's state; m_dragParticle is -1 while nothing is dragged.
    int       m_dragParticle;
    hfe::Vec3 m_dragTarget;
    float     m_dragAcceleration;
    float     m_dragExponent;
};

} // namespace anim
} // namespace dice

#endif // ANIMATION_BONEANIMATION_RAGDOLL_H
