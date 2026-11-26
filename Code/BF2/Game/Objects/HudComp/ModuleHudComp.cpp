// HUD-component console registration.
#include "ModuleHudComp.h"
#include "../../../Core/ConsoleObjectMacro.h"
#include "../../../Scene/Object/ObjectTemplateUtil.h"
#include "../AbilityHudCompTemplate.h"
#include "../HelpHudCompTemplate.h"
#include "../PostProcessCompTemplate.h"
#include "../StrategicObjectCompTemplate.h"
#include "../VehicleHudCompTemplate.h"
#include "../WarningHudCompTemplate.h"
#include "../WeaponHudCompTemplate.h"
#include "../../../Core/Ids.h"

namespace dice { namespace hfe { namespace world {
namespace {
const char* const hudCompConsoleTag = "world.hudComp"; // PE9E8940 / ELF8B0CC84

// PE8525C6 installs these defaults before the first generated constructor.
BF2_CONSOLE_OBJECT("ObjectTemplate", 2, 0)

#if !DEDICATED

// BF2.exe 5846DC..58BEF7: 92 generated bindings. The ordinary rows use the
// shared generator's get/set or fixed-arity method bodies. All 92 tables also
// override getObjectDeltaAsString with PE 410C61 ("No Delta"), so this TU keeps
// narrow local forms rather than using the otherwise-identical global macros.
PostProcessCompTemplate* activePostProcess()
{
    return static_cast<PostProcessCompTemplate*>(
        getActiveTemplateComponent(IID_IPostProcessCompTemplate));
}

VehicleHudCompTemplate* activeVehicleHud()
{
    return static_cast<VehicleHudCompTemplate*>(
        getActiveTemplateComponent(IID_IVehicleHudCompTemplate));
}

WeaponHudCompTemplate* activeWeaponHud()
{
    return static_cast<WeaponHudCompTemplate*>(
        getActiveTemplateComponent(IID_IWeaponHudCompTemplate));
}

AbilityHudCompTemplate* activeAbilityHud()
{
    return static_cast<AbilityHudCompTemplate*>(
        getActiveTemplateComponent(IID_IAbilityHudCompTemplate));
}

WarningHudCompTemplate* activeWarningHud()
{
    return static_cast<WarningHudCompTemplate*>(
        getActiveTemplateComponent(IID_IWarningHudCompTemplate));
}

HelpHudCompTemplate* activeHelpHud()
{
    return static_cast<HelpHudCompTemplate*>(
        getActiveTemplateComponent(IID_IHelpHudCompTemplate));
}

// PE 584E94/58512C call the indexed accessors with index zero for the property
// GET arm. The accessors return the selected vector element's address.
const std::string& vehicleHudVehicleIcon()
{
    return *static_cast<std::string*>(activeVehicleHud()->getVehicleIcon(0));
}

const std::string& vehicleHudAbilityIcon()
{
    return *static_cast<std::string*>(activeVehicleHud()->getAbilityIcon(0));
}

// PE constructors append the object before installing the one argument's type
// metadata. Static initialization is single-threaded here, but retain that order.
#define HUD_CC_TAIL(T)                                                                            \
            consoleBindingList(hudCompConsoleTag).push_back(this);                                \
            m_argTypes[0] = #T;                                                                   \
            m_argNeedsConversion[0] = (std::string(m_argTypes[0]) == "std::string") ? 0 : -1;    \
        }

#define HUD_PROPERTY(fnName, T, receiver, setFn, getFn)                                           \
class BF2_CC_NAME : public ConsoleObjectBaseImpl                                                  \
{                                                                                                 \
    BF2_CC_HEAD(fnName, 1, 0, 1, #T, true)                                                        \
    HUD_CC_TAIL(T)                                                                                \
    bool isObjectActive() const override { return (receiver) != 0; }                              \
    void executeObjectMethod() override                                                           \
    {                                                                                             \
        if (m_argCount == 1)                                                                      \
        {                                                                                         \
            T value = BF2_CC_ARG(T, 0);                                                          \
            m_hasReturnValue = false;                                                             \
            (receiver)->setFn(value);                                                             \
        }                                                                                         \
        else if (m_argCount == 0)                                                                 \
        {                                                                                         \
            m_hasReturnValue = true;                                                              \
            m_returnValue = (receiver)->getFn();                                                  \
        }                                                                                         \
    }                                                                                             \
    void setArgFromString(int index, const std::string& value) override                           \
    { BF2_CC_CONVERT(T, 1) }                                                                      \
    BF2_CC_RETURN_AS_STRING                                                                       \
    BF2_CC_COMMON_HOOKS                                                                           \
    std::string getObjectDeltaAsString() const override { return "No Delta"; }                   \
private:                                                                                          \
    T m_returnValue;                                                                              \
};                                                                                                \
static BF2_CC_NAME BF2_CC_INSTANCE;

#define HUD_PROPERTY_FN(fnName, T, activeExpr, getExpr, setCall)                                  \
class BF2_CC_NAME : public ConsoleObjectBaseImpl                                                  \
{                                                                                                 \
    BF2_CC_HEAD(fnName, 1, 0, 1, #T, true)                                                        \
    HUD_CC_TAIL(T)                                                                                \
    bool isObjectActive() const override { return (activeExpr); }                                 \
    void executeObjectMethod() override                                                           \
    {                                                                                             \
        if (m_argCount == 1)                                                                      \
        {                                                                                         \
            T value = BF2_CC_ARG(T, 0);                                                          \
            m_hasReturnValue = false;                                                             \
            setCall(value);                                                                       \
        }                                                                                         \
        else if (m_argCount == 0)                                                                 \
        {                                                                                         \
            m_hasReturnValue = true;                                                              \
            m_returnValue = (getExpr);                                                            \
        }                                                                                         \
    }                                                                                             \
    void setArgFromString(int index, const std::string& value) override                           \
    { BF2_CC_CONVERT(T, 1) }                                                                      \
    BF2_CC_RETURN_AS_STRING                                                                       \
    BF2_CC_COMMON_HOOKS                                                                           \
    std::string getObjectDeltaAsString() const override { return "No Delta"; }                   \
private:                                                                                          \
    T m_returnValue;                                                                              \
};                                                                                                \
static BF2_CC_NAME BF2_CC_INSTANCE;

#define HUD_METHOD_1_STATIC(fnName, target, activeExpr, A0)                                       \
class BF2_CC_NAME : public ConsoleObjectBaseImpl                                                  \
{                                                                                                 \
    BF2_CC_HEAD(fnName, 0, 1, 1, "void", false)                                                  \
    HUD_CC_TAIL(A0)                                                                               \
    bool isObjectActive() const override { return (activeExpr); }                                 \
    void executeObjectMethod() override                                                           \
    {                                                                                             \
        if (m_argCount != 1) return;                                                              \
        A0 value = BF2_CC_ARG(A0, 0);                                                             \
        m_hasReturnValue = false;                                                                 \
        target(value);                                                                            \
    }                                                                                             \
    void setArgFromString(int index, const std::string& value) override                           \
    { BF2_CC_CONVERT(A0, 1) }                                                                     \
    BF2_CC_NO_RETURN_AS_STRING                                                                    \
    BF2_CC_COMMON_HOOKS                                                                           \
    std::string getObjectDeltaAsString() const override { return "No Delta"; }                   \
};                                                                                                \
static BF2_CC_NAME BF2_CC_INSTANCE;

#define HUD_METHOD_1_RET(fnName, target, RET, activeExpr, A0)                                     \
class BF2_CC_NAME : public ConsoleObjectBaseImpl                                                  \
{                                                                                                 \
    BF2_CC_HEAD(fnName, 0, 1, 1, #RET, true)                                                      \
    HUD_CC_TAIL(A0)                                                                               \
    bool isObjectActive() const override { return (activeExpr); }                                 \
    void executeObjectMethod() override                                                           \
    {                                                                                             \
        if (m_argCount != 1) return;                                                              \
        A0 value = BF2_CC_ARG(A0, 0);                                                             \
        m_hasReturnValue = true;                                                                  \
        m_returnValue = target(value);                                                            \
    }                                                                                             \
    void setArgFromString(int index, const std::string& value) override                           \
    { BF2_CC_CONVERT(A0, 1) }                                                                     \
    BF2_CC_RETURN_AS_STRING                                                                       \
    BF2_CC_COMMON_HOOKS                                                                           \
    std::string getObjectDeltaAsString() const override { return "No Delta"; }                   \
private:                                                                                          \
    RET m_returnValue;                                                                            \
};                                                                                                \
static BF2_CC_NAME BF2_CC_INSTANCE;

// showOnCamMode is neither a normal property nor a method: both PE constructors
// store type 3, return type int and dynamic=true, while the executor has only the
// argc-one SET arm and keeps an otherwise-unused int return member/formatter.
#define HUD_CAMERA_MODE_PROPERTY(fnName, receiver, setFn)                                         \
class BF2_CC_NAME : public ConsoleObjectBaseImpl                                                  \
{                                                                                                 \
    BF2_CC_HEAD(fnName, 3, 1, 1, "int", true)                                                    \
    HUD_CC_TAIL(int)                                                                              \
    bool isObjectActive() const override { return (receiver) != 0; }                              \
    void executeObjectMethod() override                                                           \
    {                                                                                             \
        if (m_argCount != 1) return;                                                              \
        int value = BF2_CC_ARG(int, 0);                                                           \
        m_hasReturnValue = false;                                                                 \
        (receiver)->setFn(value);                                                                 \
    }                                                                                             \
    void setArgFromString(int index, const std::string& value) override                           \
    { BF2_CC_CONVERT(int, 1) }                                                                    \
    BF2_CC_RETURN_AS_STRING                                                                       \
    BF2_CC_COMMON_HOOKS                                                                           \
    std::string getObjectDeltaAsString() const override { return "No Delta"; }                   \
private:                                                                                          \
    int m_returnValue;                                                                            \
};                                                                                                \
static BF2_CC_NAME BF2_CC_INSTANCE;

HUD_PROPERTY(postProcess.tvInterference, float, activePostProcess(), setTvInterference, getTvInterference)
HUD_PROPERTY(postProcess.tvDistortionRoll, float, activePostProcess(), setTvDistortionRoll, getTvDistortionRoll)
HUD_PROPERTY(postProcess.tvDistortionFreq, float, activePostProcess(), setTvDistortionFreq, getTvDistortionFreq)
HUD_PROPERTY(postProcess.tvDistortionScale, float, activePostProcess(), setTvDistortionScale, getTvDistortionScale)

HUD_PROPERTY(vehicleHud.guiIndex, int, activeVehicleHud(), setGuiIndex, getGuiIndex)
HUD_PROPERTY(vehicleHud.hudName, std::string, activeVehicleHud(), setHudName, getHudName)
HUD_PROPERTY_FN(vehicleHud.vehicleIcon, std::string, activeVehicleHud() != 0, vehicleHudVehicleIcon(), activeVehicleHud()->setVehicleIcon)
HUD_METHOD_1_STATIC(vehicleHud.addVehicleIcon, activeVehicleHud()->addVehicleIcon, activeVehicleHud() != 0, std::string)
HUD_PROPERTY_FN(vehicleHud.AbilityIcon, std::string, activeVehicleHud() != 0, vehicleHudAbilityIcon(), activeVehicleHud()->setAbilityIcon)
HUD_METHOD_1_STATIC(vehicleHud.addAbilityIcon, activeVehicleHud()->addAbilityIcon, activeVehicleHud() != 0, std::string)
HUD_PROPERTY(vehicleHud.vehicleIconPos, Vec2, activeVehicleHud(), setVehicleIconPos, getVehicleIconPos)
HUD_PROPERTY(vehicleHud.usePlayerIcon, bool, activeVehicleHud(), setUsePlayerIcon, getUsePlayerIcon)
HUD_PROPERTY(vehicleHud.useChildHud, bool, activeVehicleHud(), setUseChildHud, getUseChildHud)
HUD_PROPERTY(vehicleHud.standardHelpEnabled, bool, activeVehicleHud(), setStandardHelpEnabled, getStandardHelpEnabled)
HUD_PROPERTY(vehicleHud.hasTurretIcon, bool, activeVehicleHud(), setHasTurretIcon, getHasTurretIcon)
HUD_PROPERTY(vehicleHud.showCrossHair, bool, activeVehicleHud(), setShowCrossHair, getShowCrossHair)
HUD_PROPERTY(vehicleHud.isCoDriver, bool, activeVehicleHud(), setIsCoDriver, getIsCoDriver)
HUD_PROPERTY(vehicleHud.showInfo, bool, activeVehicleHud(), setShowInfo, getShowInfo)
HUD_PROPERTY(vehicleHud.disableOnSprint, bool, activeVehicleHud(), setDisableOnSprint, getDisableOnSprint)
HUD_PROPERTY(vehicleHud.enablePostProcessingOnGuiIndex, bool, activeVehicleHud(), setEnablePostProcessingOnGuiIndex, getEnablePostProcessingOnGuiIndex)
HUD_PROPERTY(vehicleHud.miniMapIcon, std::string, activeVehicleHud(), setMiniMapIcon, getMiniMapIcon)
HUD_PROPERTY(vehicleHud.miniMapIconSize, int, activeVehicleHud(), setMiniMapIconSize, getMiniMapIconSize)
HUD_PROPERTY(vehicleHud.miniMapIconLeaderSize, std::string, activeVehicleHud(), setMiniMapIconLeaderSize, getMiniMapIconLeaderSize)
HUD_PROPERTY(vehicleHud.spottedIcon, std::string, activeVehicleHud(), setSpottedIcon, getSpottedIcon)
HUD_PROPERTY(vehicleHud.spottedIconSize, int, activeVehicleHud(), setSpottedIconSize, getSpottedIconSize)
HUD_PROPERTY(vehicleHud.maxClimbSpeed, int, activeVehicleHud(), setMaxClimbSpeed, getMaxClimbSpeed)
HUD_CAMERA_MODE_PROPERTY(vehicleHud.showOnCamMode, activeVehicleHud(), setShowOnCamMode)
HUD_METHOD_1_RET(vehicleHud.getShowOnCamMode, activeVehicleHud()->getShowOnCamMode, bool, activeVehicleHud() != 0, int)
HUD_METHOD_1_STATIC(vehicleHud.addShowOnCamMode, activeVehicleHud()->addShowOnCamMode, activeVehicleHud() != 0, int)
HUD_PROPERTY(vehicleHud.vehicleType, int, activeVehicleHud(), setVehicleType, getVehicleType)
HUD_PROPERTY(vehicleHud.typeIcon, std::string, activeVehicleHud(), setTypeIcon, getTypeIcon)
HUD_PROPERTY(vehicleHud.useSelectionIcons, bool, activeVehicleHud(), setUseSelectionIcons, getUseSelectionIcons)
HUD_PROPERTY(vehicleHud.displaySelectionOnEnter, bool, activeVehicleHud(), setDisplaySelectionOnEnter, getDisplaySelectionOnEnter)
HUD_PROPERTY(vehicleHud.useVehicleCommRose, bool, activeVehicleHud(), setUseVehicleCommRose, getUseVehicleCommRose)
HUD_PROPERTY(vehicleHud.pantingSound, std::string, activeVehicleHud(), setPantingSound, getPantingSound)
HUD_PROPERTY(vehicleHud.injurySound, std::string, activeVehicleHud(), setInjurySound, getInjurySound)

HUD_PROPERTY(weaponHud.guiIndex, int, activeWeaponHud(), setGuiIndex, getGuiIndex)
HUD_PROPERTY(weaponHud.altGuiIndex, int, activeWeaponHud(), setAltGuiIndex, getAltGuiIndex)
HUD_PROPERTY(weaponHud.hudName, std::string, activeWeaponHud(), setHudName, getHudName)
HUD_PROPERTY(weaponHud.enableMouse, bool, activeWeaponHud(), setEnableMouse, getEnableMouse)
HUD_PROPERTY(weaponHud.crosshairIcon, std::string, activeWeaponHud(), setCrosshairIcon, getCrosshairIcon)
HUD_PROPERTY(weaponHud.crosshairIconSize, int, activeWeaponHud(), setCrosshairIconSize, getCrosshairIconSize)
HUD_PROPERTY(weaponHud.altCrosshairIcon, std::string, activeWeaponHud(), setAltCrosshairIcon, getAltCrosshairIcon)
HUD_PROPERTY(weaponHud.clipIcon, std::string, activeWeaponHud(), setClipIcon, getClipIcon)
HUD_PROPERTY(weaponHud.ammoIcon, std::string, activeWeaponHud(), setAmmoIcon, getAmmoIcon)
HUD_PROPERTY(weaponHud.ammoBarFrontIcon, std::string, activeWeaponHud(), setAmmoBarFrontIcon, getAmmoBarFrontIcon)
HUD_PROPERTY(weaponHud.ammoBarBackIcon, std::string, activeWeaponHud(), setAmmoBarBackIcon, getAmmoBarBackIcon)
HUD_PROPERTY(weaponHud.weaponIcon, std::string, activeWeaponHud(), setWeaponIcon, getWeaponIcon)
HUD_PROPERTY(weaponHud.altWeaponIcon, std::string, activeWeaponHud(), setAltWeaponIcon, getAltWeaponIcon)
HUD_PROPERTY(weaponHud.selectIcon, std::string, activeWeaponHud(), setSelectIcon, getSelectIcon)
HUD_PROPERTY(weaponHud.specialAbilityIcon, std::string, activeWeaponHud(), setSpecialAbilityIcon, getSpecialAbilityIcon)
HUD_PROPERTY(weaponHud.hasMag, bool, activeWeaponHud(), setHasMag, getHasMag)
HUD_PROPERTY(weaponHud.showAmmo, bool, activeWeaponHud(), setShowAmmo, getShowAmmo)
HUD_PROPERTY(weaponHud.ShowClips, bool, activeWeaponHud(), setShowClips, getShowClips)
HUD_PROPERTY(weaponHud.hasFireRate, bool, activeWeaponHud(), setHasFireRate, getHasFireRate)
HUD_PROPERTY(weaponHud.hasRangeFinder, bool, activeWeaponHud(), setHasRangeFinder, getHasRangeFinder)
HUD_PROPERTY(weaponHud.switchGuiOnCamera, bool, activeWeaponHud(), setSwitchGuiOnCamera, getSwitchGuiOnCamera)
HUD_PROPERTY(weaponHud.showInfo, bool, activeWeaponHud(), setShowInfo, getShowInfo)
HUD_PROPERTY(weaponHud.disableOnSprint, bool, activeWeaponHud(), setDisableOnSprint, getDisableOnSprint)
HUD_PROPERTY(weaponHud.displaySelectOnActivation, bool, activeWeaponHud(), setDisplaySelectOnActivation, getDisplaySelectOnActivation)
HUD_PROPERTY(weaponHud.deviationFactor, int, activeWeaponHud(), setDeviationFactor, getDeviationFactor)
HUD_PROPERTY(weaponHud.maxVisualDeviation, int, activeWeaponHud(), setMaxVisualDeviation, getMaxVisualDeviation)
HUD_PROPERTY(weaponHud.minVisualDeviation, int, activeWeaponHud(), setMinVisualDeviation, getMinVisualDeviation)
HUD_PROPERTY(weaponHud.enablePostProcessingOnGuiIndex, int, activeWeaponHud(), setEnablePostProcessingOnGuiIndex, getEnablePostProcessingOnGuiIndex)
HUD_PROPERTY(weaponHud.firstLockSound, std::string, activeWeaponHud(), setFirstLockSound, getFirstLockSound)
HUD_PROPERTY(weaponHud.secondLockSound, std::string, activeWeaponHud(), setSecondLockSound, getSecondLockSound)
HUD_PROPERTY(weaponHud.overheatSound, std::string, activeWeaponHud(), setOverheatSound, getOverheatSound)
HUD_CAMERA_MODE_PROPERTY(weaponHud.showOnCamMode, activeWeaponHud(), setShowOnCamMode)
HUD_METHOD_1_RET(weaponHud.getShowOnCamMode, activeWeaponHud()->getShowOnCamMode, bool, activeWeaponHud() != 0, int)
HUD_METHOD_1_STATIC(weaponHud.addShowOnCamMode, activeWeaponHud()->addShowOnCamMode, activeWeaponHud() != 0, int)

HUD_PROPERTY(abilityHud.healingSound, std::string, activeAbilityHud(), setHealingSound, getHealingSound)
HUD_PROPERTY(abilityHud.repairingSound, std::string, activeAbilityHud(), setRepairingSound, getRepairingSound)
HUD_PROPERTY(abilityHud.ammoSound, std::string, activeAbilityHud(), setAmmoSound, getAmmoSound)

HUD_PROPERTY(warningHud.warningType, int, activeWarningHud(), setWarningType, getWarningType)
HUD_PROPERTY(warningHud.firstWarningSound, std::string, activeWarningHud(), setFirstWarningSound, getFirstWarningSound)
HUD_PROPERTY(warningHud.secondWarningSound, std::string, activeWarningHud(), setSecondWarningSound, getSecondWarningSound)
HUD_PROPERTY(warningHud.warningIcon, std::string, activeWarningHud(), setWarningIcon, getWarningIcon)

HUD_PROPERTY(helpHud.helpStringKey, std::string, activeHelpHud(), setHelpStringKey, getHelpStringKey)
HUD_PROPERTY(helpHud.helpSoundKey, std::string, activeHelpHud(), setHelpSoundKey, getHelpSoundKey)
HUD_PROPERTY(helpHud.altHelpStringKey, std::string, activeHelpHud(), setAltHelpStringKey, getAltHelpStringKey)
HUD_PROPERTY(helpHud.altHelpSoundKey, std::string, activeHelpHud(), setAltHelpSoundKey, getAltHelpSoundKey)
HUD_PROPERTY(helpHud.closeToHelpStringKeyNeutral, std::string, activeHelpHud(), setCloseToHelpStringKeyNeutral, getCloseToHelpStringKeyNeutral)
HUD_PROPERTY(helpHud.closeToHelpSoundKeyNeutral, std::string, activeHelpHud(), setCloseToHelpSoundKeyNeutral, getCloseToHelpSoundKeyNeutral)
HUD_PROPERTY(helpHud.closeToHelpStringKeyFriendly, std::string, activeHelpHud(), setCloseToHelpStringKeyFriendly, getCloseToHelpStringKeyFriendly)
HUD_PROPERTY(helpHud.closeToHelpSoundKeyFriendly, std::string, activeHelpHud(), setCloseToHelpSoundKeyFriendly, getCloseToHelpSoundKeyFriendly)
HUD_PROPERTY(helpHud.closeToHelpStringKeyEnemy, std::string, activeHelpHud(), setCloseToHelpStringKeyEnemy, getCloseToHelpStringKeyEnemy)
HUD_PROPERTY(helpHud.closeToHelpSoundKeyEnemy, std::string, activeHelpHud(), setCloseToHelpSoundKeyEnemy, getCloseToHelpSoundKeyEnemy)
HUD_PROPERTY(helpHud.closeToDistance, int, activeHelpHud(), setCloseToDistance, getCloseToDistance)
HUD_PROPERTY(helpHud.lowArmorHelpStringKey, std::string, activeHelpHud(), setLowArmorHelpStringKey, getLowArmorHelpStringKey)
HUD_PROPERTY(helpHud.lowArmorHelpSoundKey, std::string, activeHelpHud(), setLowArmorHelpSoundKey, getLowArmorHelpSoundKey)
HUD_PROPERTY(helpHud.lowAmmoHelpStringKey, std::string, activeHelpHud(), setLowAmmoHelpStringKey, getLowAmmoHelpStringKey)
HUD_PROPERTY(helpHud.lowAmmoHelpSoundKey, std::string, activeHelpHud(), setLowAmmoHelpSoundKey, getLowAmmoHelpSoundKey)

#undef HUD_CAMERA_MODE_PROPERTY
#undef HUD_METHOD_1_RET
#undef HUD_METHOD_1_STATIC
#undef HUD_PROPERTY_FN
#undef HUD_PROPERTY
#undef HUD_CC_TAIL
#endif

StrategicObjectCompTemplate* activeStrategicObject()
{
    return static_cast<StrategicObjectCompTemplate*>(
        getActiveTemplateComponent(IID_IStrategicObjectCompTemplate));
}

// BF2.exe 58BFF6/58C16F/58C2E8/58C4A2/58C5B9/58C6E2, tables
// 8E4E88..8E51B8. Keep the argument copy before clearing the result flag,
// repeated active-component lookup, and the generated "No Delta" hook.
#define STRATEGIC_PROPERTY(fnName, T, setFn, getFn)                                \
class BF2_CC_NAME : public ConsoleObjectBaseImpl                                  \
{                                                                                \
    BF2_CC_HEAD(fnName, 1, 0, 1, #T, true)                                        \
        m_argTypes[0] = #T;                                                       \
    BF2_CC_TAIL(1, hudCompConsoleTag)                                             \
    bool isObjectActive() const override { return activeStrategicObject() != 0; } \
    void executeObjectMethod() override                                          \
    {                                                                            \
        if (m_argCount == 1)                                                     \
        {                                                                        \
            T value = BF2_CC_ARG(T, 0);                                          \
            m_hasReturnValue = false;                                            \
            activeStrategicObject()->setFn(value);                               \
        }                                                                        \
        else if (m_argCount == 0)                                                \
        {                                                                        \
            m_hasReturnValue = true;                                             \
            m_returnValue = activeStrategicObject()->getFn();                    \
        }                                                                        \
    }                                                                            \
    void setArgFromString(int index, const std::string& value) override           \
    { BF2_CC_CONVERT(T, 1) }                                                     \
    BF2_CC_RETURN_AS_STRING                                                      \
    BF2_CC_COMMON_HOOKS                                                          \
    std::string getObjectDeltaAsString() const override { return "No Delta"; }    \
private:                                                                         \
    T m_returnValue;                                                             \
};                                                                               \
static BF2_CC_NAME BF2_CC_INSTANCE;

STRATEGIC_PROPERTY(strategicObject.strategicObjectName, std::string, setStrategicObjectName, getStrategicObjectName)
STRATEGIC_PROPERTY(strategicObject.destroyedIcon, std::string, setDestroyedIcon, getDestroyedIcon)
STRATEGIC_PROPERTY(strategicObject.intactIcon, std::string, setIntactIcon, getIntactIcon)
STRATEGIC_PROPERTY(strategicObject.part, int, setPart, getPart)
STRATEGIC_PROPERTY(strategicObject.3dMapIcon, int, set3dMapIcon, get3dMapIcon)
STRATEGIC_PROPERTY(strategicObject.isOnePiece, bool, setIsOnePiece, getIsOnePiece)
// [CHOSEN, not recovered] Project Reality map-icon size. Not a retail property.
STRATEGIC_PROPERTY(strategicObject.iconSize, float, setIconSize, getIconSize)
#undef STRATEGIC_PROPERTY
}

std::list<ConsoleObject*>& hudCompConsoleObjects()
{
    return consoleBindingList(hudCompConsoleTag);
}
} } }
