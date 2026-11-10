// Engine input, lifecycle, and sound-template setup.

#include "EngineBase.h"
#include "../GameLogicComp/IArmorGLComp.h"        // isSendingMessage - message 24

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <string>

#include "../../../Core/Ids.h"
#include "../../../Core/IBase.h"
#include "../../../Game/IGame.h"                             // game (Core/Globals.cpp) - the IGameClient query
#include "../../../Game/IGameClient.h"
#include "../../../Game/Utility/Vars.h"                      // Vars::getFloat("EngineRunningTimer")
#include "../../../Physics/HeightmapCluster.h"               // heightmapCluster->getHeightInWorldCoords
#include "../../../Physics/IPhysicsNode.h"                   // getPositionalSpeed (+0x60)
#include "../../../Physics/PhysicsUtil.h"                    // findDriveWheels / getWheelMaterials
#include "../../../Scene/Object/ObjectUtil.h"                // setNetworkableComp
#include "../../../Sound/EngineSound.h"
#include "../EngineTemplate.h"
#include "../IObjectComponent.h"                             // the wind affector / armor component words
#include "../IWindAffector.h"                                // setIsActive (+0x40)
#include "../ObjectLayoutAccess.h"                          // +0x3C armorComponent, +0x44 physicsNode

namespace dice {
namespace hfe {

extern IGame* game;                                          // ds: Core/Globals.cpp

namespace world {

extern IBase* heightmapCluster;                              // ds:0x8ABAF24 - Core/Globals.cpp

namespace {
    // The TU's static initialiser (0x82A3AC0), in the image's order. (tank-square -> ds:0x8AB0808 and
    // flying_height -> ds:0x8AB0814 belong to other unrecovered engine paths and are not declared.)
    float* const g_oldTurnMod         = hfe::Vars::getFloat("old-turn-mod", 1.5f);       // ds:0x8AB080C
    float* const g_oldTurnLimit       = hfe::Vars::getFloat("old-turn-limit", 1.0f);     // ds:0x8AB0810
    float* const g_engineRunningTimer = hfe::Vars::getFloat("EngineRunningTimer", 5.0f); // ds:0x8AB0818

    // The static gear table flt_8989EA0 - EngineBase::getGearRatio / CarEngine::getGearRatio (the
    // non-car branch) index it 1..5; index 0 is never read (a denormal in the image).
    const float g_gearRatios[6] = { 0.0f, 4.0f, 2.0f, 1.5f, 1.25f, 1.0f };

    inline float readEngineMappedInput(const io::PlayerInput& input, io::PlayerInputMap map)
    {
        const int index = static_cast<int>(map);
        if (index > 63)
            return 0.0f;
        return ((input.mappingFlags >> index) & 1ull) ? input.inputs[index] : 0.0f;
    }

    // Client feedback is reached only through IGameClient. Armor message 24 marks wreckage while the armor is sending its transition.
    inline void windAffectorSetActive(IObjectComponent* windAffector, bool active)
    {
        static_cast<IWindAffector*>(windAffector)->setIsActive(active);
    }
}

// The constructor - 0x82A2A30 (C2) / 0x82A2B30 (C1), 252 B: RotationalBundle(template); the
// template pointer; the whole own block zeroed except m_gear = 1; then updateFlags(0, 0x800) -
// an engine does NOT tick through the per-tick flag its RotationalBundle ctor may have raised.
EngineBase::EngineBase(const EngineTemplate* objectTemplate)
    : RotationalBundle(objectTemplate)
    , m_template(objectTemplate)                                                     // +0x1B0
{
    m_dword1E0 = 0;                                                                  // +0x1E0
    m_gear = 1;                                                                      // +0x1E8
    m_dword1F0 = 0;                                                                  // +0x1F0
    m_heat = 0.0f; m_accumulatedInput = 0.0f;
    m_soundRpm = 0.0f; m_input = 0.0f; m_float1C8 = 0.0f; m_float1CC = 0.0f;
    m_float1D0 = 0.0f; m_float1D4 = 0.0f; m_currentAngle = 0.0f;
    m_soundInput = 0.0f;                                                             // +0x1E4
    m_dword1EC = 0.0f;                                                                  // +0x1EC
    m_engineSound = 0;                                                               // +0x1F8
    m_lightSource = 0;                                                               // +0x1FC
    m_dword204 = 0;                                                                  // +0x204
    m_runningTimer = 0.0f;                                                           // +0x20C
    m_engineRunning = false;                                                         // +0x1B4
    m_wrecked = false;                                                               // +0x1B5
    m_byte1DC = false;                                                               // +0x1DC
    m_byte208 = false;                                                               // +0x208
    m_float1F4 = 0.0f;                                                               // (untouched by the ctor)
    updateFlags(0u, 0x800u);
}

// / 0x82A2CA0 / 0x82A2D10: the drive-wheel weak pointer's control block released, the
// engine sound deleted (its vslot +0x0C - the deleting destructor), ~RotationalBundle.
EngineBase::~EngineBase()
{
    if (m_engineSound)
        delete m_engineSound;
}

void EngineBase::init()
{
    RotationalBundle::init();
    initSound();
    if (IObject* light = findFirstMatchingChild(this, CID_LightSource))
        m_lightSource = light;                                                       // +0x1FC
    if (m_windAffector)                                                              // +0x70
        windAffectorSetActive(m_windAffector, false);                                // +0x40
    findDriveWheels(&m_driveWheel, 0, this);                                         // +0x200
}

// the sound node is the root object when the template's audio says
// transformationRelativeRoot (+0x671), else this engine; the new EngineSound replaces (deletes)
// any previous one.
void EngineBase::initSound()
{
    IObject* root = m_template->m_engineSound.m_byte01 ? getRootParent(this) : this;
    EngineSound* sound = new EngineSound(m_template, this, root, &m_template->m_engineSound, &m_soundRpm);
    if (sound != m_engineSound)
    {
        if (m_engineSound)
            delete m_engineSound;                                                    // vslot +0x0C
        m_engineSound = sound;
    }
}

// the forwarder EngineTemplate::templateInitialize tail-jumps to.
void EngineBase::initSoundTemplates(EngineTemplate* objectTemplate)
{
    EngineSound::initTemplates(objectTemplate);
}

void EngineBase::initNetworkableComp()
{
    if (m_template && m_template->getNetworkableInfo().compare("") != 0)
        setNetworkableComp(this, CID_EngineNetworkable);                             // 39011
}

// reset - 0x82A2E90, 261 B: the own block back to its ctor values (the light-source pointer too),
// +0x1F4 = -1.0; the sound stopped hard (setIsRunning(false, true, false)) and the sound state
// refreshed; then RotationalBundle's +0x196 flag SET, the 0x800 tick flag cleared, and the wind
// affector deactivated.
void EngineBase::reset()
{
    m_float1F4 = -1.0f;                                                              // +0x1F4
    m_heat = 0.0f; m_accumulatedInput = 0.0f; m_soundRpm = 0.0f; m_float1D4 = 0.0f;
    m_input = 0.0f; m_float1C8 = 0.0f; m_float1CC = 0.0f; m_float1D0 = 0.0f;
    m_gear = 1;
    m_soundInput = 0.0f;
    m_currentAngle = 0.0f;
    m_dword1EC = 0;
    m_dword204 = 0;
    m_dword1E0 = 0;
    m_dword1F0 = 0;
    m_lightSource = 0;
    m_engineRunning = false;
    m_wrecked = false;
    m_byte1DC = false;
    m_byte208 = false;
    m_engineSound->setIsRunning(false, true, false);
    updateEngineSound();
    m_rotatesContinuously = true;                                                    // +0x196
    updateFlags(0u, 0x800u);                                                         // vslot +0x60
    if (m_windAffector)
        windAffectorSetActive(m_windAffector, false);                                // +0x40
}

// / 0x82A2E40, 72 B - the object-cache pair.
void EngineBase::disableCachedObject()
{
    RotationalBundle::disableCachedObject();
    reset();                                                                         // vslot +0x1A0
    if (m_wrecked)
        std::abort();
}

void EngineBase::enableCachedObject()
{
    RotationalBundle::enableCachedObject();
    if (m_windAffector)
        windAffectorSetActive(m_windAffector, false);
    findDriveWheels(&m_driveWheel, 0, this);
}

// the running timer counts down; at zero the engine stops (flag 0x2000 cleared,
// +0x1F4 = 0.5).
void EngineBase::handleFrameUpdate(float dt)
{
    if (m_runningTimer > 0.0f)
    {
        m_runningTimer -= dt;
        if (m_runningTimer <= 0.0f)
        {
            m_engineRunning = false;
            m_float1F4 = 0.5f;
            updateFlags(0u, 0x2000u);
        }
    }
}

// . Clamp the authored input to [-1, 1], linearly map it from the default angle
// toward maxAngle, then convert degrees to radians. The doubled expression in the retail x87 body
// is algebraically this degrees-to-radians form.
float EngineBase::mapInputToWantedAngle(float input, float maxAngle, float defaultAngle)
{
    if (input < -1.0f)
        input = -1.0f;
    else if (input > 1.0f)
        input = 1.0f;
    return (defaultAngle + input * (maxAngle - defaultAngle)) * 0.01745329251994329577f;
}

// . Approach the wanted angle by at most speed degrees per second this tick.
float EngineBase::mapWantedAngleToAngle(float wantedAngle, float dt, float angle, float speed)
{
    const float step = 0.01745329251994329577f * speed * dt;
    if (angle <= wantedAngle)
    {
        const float advanced = angle + step;
        return wantedAngle > advanced ? advanced : wantedAngle;
    }

    const float advanced = angle - step;
    return wantedAngle <= advanced ? advanced : wantedAngle;
}

// . The player argument is unused in retail. Accumulation applies only to z and
// persists the clamped result at +0x1BC; the function-local scale is the image's 2.5f constant.
void EngineBase::accumulate(IPlayer*, Vec3& input, float dt)
{
    static const float scale = 2.5f;
    m_accumulatedInput += input.z * scale * dt;
    if (m_accumulatedInput < -1.0f)
        input.z = -1.0f;
    else if (m_accumulatedInput > 1.0f)
        input.z = 1.0f;
    else
        input.z = m_accumulatedInput;
    m_accumulatedInput = input.z;
}

// . This differs intentionally from RotationalBundle::handlePlayerInput: engines
// do not claim/wipe the caller's input and do not run the regulator. They map their three authored
// axes, optionally replace yaw with remoteEngineInput, optionally accumulate z, then enforce the
// canReverse gate against the template acceleration signs. The flags argument is unused.
void EngineBase::handlePlayerInput(IPlayer* player, const io::PlayerInput& input,
                                   float dt, unsigned int)
{
    if (m_engineRunning == true && !m_wrecked)
    {
        m_rotationInput.x = readEngineMappedInput(input, m_template->getInputToYaw());
        m_rotationInput.y = readEngineMappedInput(input, m_template->getInputToPitch());
        m_rotationInput.z = readEngineMappedInput(input, m_template->getInputToRoll());

        if (m_template->m_remoteEngineInput != io::PINone)
            m_rotationInput.x = readEngineMappedInput(input, m_template->m_remoteEngineInput);

        // [CHOSEN, not recovered] Project Reality's speedhack guard: a c_ETNewCar2 engine accepts
        // input only within [-1, 1]. PR's scripts polled these values and kicked the driver; a
        // client can only exceed the range by modifying its input, so it is clamped instead.
        if (m_template->getEngineType() == 194)
        {
            float* const axes = &m_rotationInput.x;
            for (int axis = 0; axis < 3; ++axis)
                axes[axis] = std::max(-1.0f, std::min(1.0f, axes[axis]));
        }

        if (m_template->getAccumulate() != 0.0f)
            accumulate(player, m_rotationInput, dt);
    }
    else
    {
        m_rotationInput.x = 0.0f;
        m_rotationInput.y = 0.0f;
        m_rotationInput.z = 0.0f;
    }

    if (!m_template->m_canReverse)
    {
        float* const mapped = &m_rotationInput.x;
        const float* const acceleration = &m_template->m_acceleration.x;
        for (int axis = 0; axis < 3; ++axis)
        {
            if (acceleration[axis] * mapped[axis] < 0.0f)
                mapped[axis] = 0.0f;
        }
    }
}

// Forwards the message to RotationalBundle, then updates engine running, wind and wreckage state.
// Relevant lifecycle and entry/exit messages also update sound; destroyed engines force sound to stop.
void EngineBase::handleMessage(TemplateMessage message, IPlayer* player, bool flag)
{
    RotationalBundle::handleMessage(message, player, flag);
    const int m = static_cast<int>(message);
    const int engineType = m_template->getEngineType();                              // vslot +0x144
    bool windOff = false;
    switch (m)
    {
    case 0:
        if (!(engineType & 0x10)) break;
        /* fall through: START */
    case 4:
        if (!m_wrecked)
        {
            m_engineRunning = true;
            m_float1F4 = -1.0f;
            updateFlags(0x800u, 0u);
            if (m_windAffector)
                windAffectorSetActive(m_windAffector, true);
            m_runningTimer = 0.0f;
            updateFlags(0u, 0x2000u);
        }
        break;
    case 1:
        if (!(engineType & 0x10)) break;
        m_engineRunning = false;                                                     // stop
        m_float1F4 = 0.5f;
        windOff = true;
        break;
    case 5:
        if (!(engineType & 0x21))
        {
            m_engineRunning = false;                                                 // stop
            m_float1F4 = 0.5f;
        }
        else
        {
            const Vec3& pos = getAbsolutePosition();                                 // vslot +0x6C
            static_cast<IHeightmapCluster*>(heightmapCluster)->getHeightInWorldCoords(pos.x, pos.z);   // +0x6C result dropped
            Vec3 normal; normal.x = normal.y = normal.z = 0.0f;
            // It overwrites `normal`
            // whichever way it answers; nothing here reads the vector back.
            if (!isObjectCloseToGround(getRootParent(this), normal, 0.0f))
            {
                m_engineRunning = true;
                if (engineType == 33)
                {
                    m_runningTimer = *g_engineRunningTimer;
                    updateFlags(0x2000u, 0u);
                }
            }
            else
            {
                m_engineRunning = false;
                m_float1F4 = 0.5f;
            }
        }
        windOff = true;
        break;
    case 24:
    {
        m_engineRunning = false;
        m_float1F4 = 0.5f;
        IObjectComponent* armor = 0;
        for (IObject* walk = this; walk; walk = walk->getParent())
        {
            armor = object_detail::LayoutAccess::armorComponent(walk);               // +0x3C
            if (armor)
                break;
        }
        if (armor && static_cast<IArmorGLComp*>(armor)->isSendingMessage())          // +0x50 slot 20
            m_wrecked = true;
        windOff = true;
        break;
    }
    default:
        break;
    }
    if (windOff && m_windAffector)
        windAffectorSetActive(m_windAffector, false);
    if (m == 0 || m == 4 || m == 1 || m == 5 || m == 24)
    {
        IGameClient* client = game
            ? static_cast<IGameClient*>(game->queryInterface(IID_IGameClient)) : 0;
        const bool notLocal = !client || client->getIsReady();
        const bool force = m_template->m_constantRpm != 0 || m == 24;                // template +0x5C8
        m_engineSound->setIsRunning(m_engineRunning, force, notLocal);
    }
}

// updateEngineSound - 0x82A37B0, 284 B: the sound's velocity from the root's physics node; the
// sound rpm from the angle (bit 0x20 engines: 2 x angle clamped to [-0.5, 0.5], + 0.5) or the input
// (clamped [-1, 1]); with the engine running the EngineSound updates (input z != 0, the sound
// input, the drive wheel's material) and the rotational sound gets the rpm, else 0.
void EngineBase::updateEngineSound()
{
    IObject* root = getRootParent(this);
    if (IPhysicsNode* node = static_cast<IPhysicsNode*>(object_detail::LayoutAccess::physicsNode(root)))   // +0x44
        m_engineSound->setVelocity(node->getPositionalSpeed());                      // +0x60
    float rpm;
    if (m_template->getEngineType() & 0x20)
    {
        float a = m_currentAngle + m_currentAngle;
        if (a < -0.5f) a = -0.5f; else if (a > 0.5f) a = 0.5f;
        rpm = a + 0.5f;
    }
    else
    {
        rpm = m_input;
        if (rpm < -1.0f) rpm = -1.0f; else if (rpm > 1.0f) rpm = 1.0f;
    }
    m_soundRpm = rpm;                                                                // +0x1C0
    float rotational = 0.0f;
    if (m_engineRunning)
    {
        const int material = getWheelMaterials(m_driveWheel, 0);
        m_engineSound->update(m_rotationInput.z != 0.0f, m_soundInput, material);           // +0x160 +0x1E4
        rotational = m_soundRpm;
    }
    RotationalBundle::updateRotationalSound(false, rotational);
}

// |angle / maxAngleOfAttack(rad)| for bit-0x20 engines, |input| otherwise,
// clamped to [-1, 1] before the abs.
float EngineBase::getEngineIntensity()
{
    float v;
    if (m_template->getEngineType() & 0x20)
        v = m_currentAngle / (m_template->getMaxAngleOfAttack() * 0.017453292f);     // vslot +0x154
    else
        v = m_input;
    if (v < -1.0f) v = -1.0f; else if (v > 1.0f) v = 1.0f;
    return std::fabs(v);
}

// Returns side-dependent differential RPM, fading steering effect with vehicle speed.
// Engines without differential support and wheels exactly on the center line return unmodified input.
float EngineBase::getCurrentDifferentialRPM(float relativeX) const
{
    static const float c1 = 1.0f;                             // getCurrentDifferentialRPM(float)const::c1
    static const float c2_maxSpeed = 20.0f;                   // ...::c2_maxSpeed

    float turn = *g_oldTurnMod;                                                      // ds:0x8AB080C

    const IPhysicsNode* node = static_cast<const IPhysicsNode*>(
        object_detail::LayoutAccess::physicsNode(getRootParent(this)));              // root +0x44
    if (node)
    {
        const Vec3& v = node->getPositionalSpeed();                                  // +0x60
        float fade = c1 - std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z) / c2_maxSpeed;
        if (fade < 0.0f)      fade = 0.0f;
        else if (fade > 1.0f) fade = 1.0f;
        turn = fade * turn;
    }

    if (!(m_template->getEngineType() & 4))                                          // vslot +0x144
        return m_input;                                                              // +0x1C4

    float rpm;
    if (relativeX >= 0.0f)
    {
        if (relativeX <= 0.0f)
            return m_input;                                                          // see the header
        rpm = (1.0f - turn * m_float1D4) * m_input;                                   // +0x1D4
        if (rpm < -*g_oldTurnLimit) rpm = -*g_oldTurnLimit;
        if (rpm >  *g_oldTurnLimit) return *g_oldTurnLimit;
    }
    else
    {
        rpm = (turn * m_float1D4 + 1.0f) * m_input;
        if (rpm < -*g_oldTurnLimit) rpm = -*g_oldTurnLimit;
        if (rpm >  *g_oldTurnLimit) return *g_oldTurnLimit;
    }
    return rpm;
}

// The small slots.
float EngineBase::getCurrentTorque() const     { return m_template->m_torque; }              // template +0x3D0
int   EngineBase::getEngineType()              { return m_template->getEngineType(); }       // vslot +0x144
float EngineBase::getGearRatio(int gear) const
{
    int i = 1;
    if (gear > 0)
        i = gear >= 6 ? 5 : gear;
    return g_gearRatios[i];
}
int   EngineBase::getGearChange() const        { return 0; }
void  EngineBase::setGearChange(int)           {}

} // namespace world
} // namespace hfe
} // namespace dice
