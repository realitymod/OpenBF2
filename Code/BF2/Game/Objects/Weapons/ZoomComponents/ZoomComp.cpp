// Path:     [INFERRED] from the recovered ZoomComponents family placement.
// Contents: complete ZoomComp and ZoomCompTemplate bases, cross-read from the symbolized Linux
// ELF and Win32 PE.  All permissive defaults below are shipped bodies.

#include "ZoomComp.h"

#include <sstream>
#include <string>

#include "../GenericFireArm.h"
#include "../../Game/Soldier.h"
#include "../../IObject.h"
#include "../../IObjectTemplate.h"
#include "../../IPlayer.h"
#include "../../../../Core/Ids.h"
#include "../../../../IO/Stream/IStream.h"
#include "../../../../Scene/Object/PlayerManager.h"

namespace dice {
namespace hfe {
namespace world {

IWeaponZoomComp::~IWeaponZoomComp()
{
}

ZoomComp::ZoomComp(ZoomCompTemplate* objectTemplate)
    : m_template(objectTemplate)
{
    // WeaponComp has no emitted constructor.  The zoom base writes only this count and leaves the
    // two object pointers untouched until the component attachment path calls setObject/init.
    m_refCount = 1;
}

ZoomComp::~ZoomComp()
{
}

unsigned int ZoomComp::getInterfaceID() const { return IID_IWeaponZoomComp; }
void ZoomComp::initComponent(WeaponInitEvent&) {}
bool ZoomComp::preUpdate() { return false; }
void ZoomComp::updateComponent(float, unsigned int) {}
bool ZoomComp::isSelectable() const { return true; }
void ZoomComp::fireStop(bool) {}
bool ZoomComp::requestFire(bool, bool) { return true; }
void ZoomComp::postFire(WeaponFireEvent&) {}
void ZoomComp::fireComplete(unsigned int) {}
void ZoomComp::postSwitchFireRate(FireRate) {}
bool ZoomComp::requestReload(bool) { return true; }
void ZoomComp::reload(ReloadState) {}
void ZoomComp::postReload() {}
void ZoomComp::outOfAmmo(bool) {}
void ZoomComp::stoppedTracking() {}
void ZoomComp::postChangeTarget(IObject*) {}
void ZoomComp::projectileWasReset(IObject*) {}
void ZoomComp::enable() {}
void ZoomComp::disable() { reset(); }
void ZoomComp::stop() {}
bool ZoomComp::busy() { return false; }
void ZoomComp::exit() {}
void ZoomComp::destroy() {}
void ZoomComp::pickup() {}
void ZoomComp::reset() {}
int ZoomComp::getPrevCameraId() const { return -1; }
int ZoomComp::getChangeCameraId() const { return -1; }
bool ZoomComp::getSignalToggle() { return false; }
void ZoomComp::toggle(TemplateMessage) {}
void ZoomComp::changeTarget(int) {}
void ZoomComp::setCameraNr(int) {}
int ZoomComp::getCameraNr() const { return 0; }
bool ZoomComp::allowTargetSeek() const { return true; }
void ZoomComp::changeViewMode() {}
float ZoomComp::getZoomFactor(int) const { return -1.0f; }
bool ZoomComp::allowEffects() const { return true; }
io::PlayerInputMap ZoomComp::getInput() const { return m_template->getCameraInput(); }

bool ZoomComp::canZoom()
{
    if (!m_template->getOnlyZoomWhenProne())
        return true;

    // [CHOSEN, not recovered] Project Reality always lets a zoomed weapon zoom out; retail refuses
    // that too when the player is not prone.
    if (getCameraNr() != 0)
        return true;

    IPlayer* const player = firearm()->getPlayer();
    if (!player)
        return false;

    IObject* const vehicle = player->getVehicle();
    if (!vehicle || vehicle->getTemplate()->getClassID() != CID_SoldierTemplate)
        return false;

    return static_cast<Soldier*>(vehicle)->getPose() == SoldierPose_Prone;
}

bool ZoomComp::is1P() const
{
    IPlayer* const player = firearm()->getPlayer();
    if (!player)
        return false;
    if (player != playerManager->getLocalHumanPlayer())
        return false;
    return player->hasFirstPersonView();
}

ZoomCompTemplate::ZoomCompTemplate()
    : m_cameraInput(io::PIAltFire)
    , m_onlyZoomWhenProne(false)
{
    // The three trailing bytes are deliberately not initialized in either shipped constructor.
}

unsigned int ZoomCompTemplate::getClassID() const { return CID_ZoomCompTemplate; }
unsigned int ZoomCompTemplate::getInterfaceID() { return IID_IWeaponZoomCompTemplate; }
void ZoomCompTemplate::objectCreated(IObject*) {}
void ZoomCompTemplate::objectDestroyed(IObject*) {}
void ZoomCompTemplate::setTemplate(IObjectTemplate*) {}

void* ZoomCompTemplate::queryImplementation(unsigned int classId)
{
    return classId == CID_ZoomCompTemplate ? this : 0;
}

bool ZoomCompTemplate::makeScript(io::IStream* stream)
{
    std::ostringstream script;
    if (m_cameraInput != io::PIAltFire)
        script << "ObjectTemplate.zoom.cameraInput " << m_cameraInput << std::endl;
    if (m_onlyZoomWhenProne)
        script << "ObjectTemplate.zoom.onlyZoomWhenProne 1" << std::endl;

    const std::string text = script.str();
    return stream->write(text.c_str(), static_cast<unsigned int>(text.length())) == text.length();
}

} // namespace world
} // namespace hfe
} // namespace dice
