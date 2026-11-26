// Strategic-object configuration and intact/destroyed map icons.
// Paths are authored strings; resolved textures are integer handles. [INFERRED] Source placement.
// [PLATFORM] String-containing layout differs between toolchains.

#ifndef GAME_OBJECTS_STRATEGICOBJECTCOMPTEMPLATE_H     // [INFERRED] guard style
#define GAME_OBJECTS_STRATEGICOBJECTCOMPTEMPLATE_H

#include <string>

#include "CompTemplateInterfaces.h"
#include "../../IO/Console/IConsoleSaveable.h"

namespace dice {
namespace hfe {
namespace world {

class IObjectComponent;

// Strategic-object names/icons use by-value setters and reference getters.
class StrategicObjectCompTemplate : public IStrategicObjectCompTemplate, public io::IConsoleSaveable
{
public:
    StrategicObjectCompTemplate(); // ELF 8228EB0 / BF2.exe 65E0E0
    virtual void  addRef();
    virtual int   getRef();
    virtual int   release();
    virtual void* queryInterface(unsigned int) const;
    virtual unsigned int getClassID() const;
    virtual unsigned int getInterfaceID();
    virtual IObjectComponent* createComponent();
    virtual void  objectCreated(IObject*);
    virtual void  objectDestroyed(IObject*);
    virtual void  setTemplate(IObjectTemplate*);
    virtual void* queryImplementation(unsigned int);
    virtual void setStrategicObjectName(std::string);
    virtual const std::string& getStrategicObjectName();
    virtual void  setDestroyedIcon(std::string);                     // resolves
    virtual const std::string& getDestroyedIcon();
    virtual int getDestroyedIconPtr();
    virtual void  setIntactIcon(std::string);                        // resolves
    virtual const std::string& getIntactIcon();
    virtual int getIntactIconPtr();
    virtual void  setPart(int);                  virtual int  getPart();
    virtual void  set3dMapIcon(int);             virtual int  get3dMapIcon() const;
    virtual bool  has3dMapIcon() const;
    virtual void  setIsOnePiece(bool);           virtual bool getIsOnePiece() const;
    virtual ~StrategicObjectCompTemplate();
    virtual bool  makeScript(io::IStream*);                          // +0x70 last

    // [CHOSEN, not recovered] Project Reality map-icon size, authored through
    // ObjectTemplate.strategicObject.iconSize. Both sides of the square icon; defaults to the
    // retail hard-coded 19.0. Non-virtual so the retail vtable is unchanged.
    void  setIconSize(float value) { m_iconSize = value; }
    float getIconSize() const      { return m_iconSize; }

protected:
    friend class StrategicObjectComp; // getHudInfo/getHudIcons return addresses within this payload
    /*  8 */ int   m_refCount;

    /* 12 */ std::string m_strategicObjectName;
    /* 16 */ int         m_part;

    /* 20 */ std::string m_destroyedIcon;         // authored path
    /* 24 */ std::string m_intactIcon;            // authored path
    /* 28 */ int         m_destroyedIconPtr;      // resolved texture handle
    /* 32 */ int         m_intactIconPtr;         // resolved texture handle

    /* 36 */ int   m_mapIcon3D;                   // console binding: strategicObject.3dMapIcon
    /* 40 */ bool  m_isOnePiece;
    /* 41 */ bool  _byte29;                       // [UNRESOLVED]
    /* 42 */ bool  _byte2A;
    /* 43 */ bool  _byte2B;

    // [CHOSEN, not recovered] Appended past the retail layout (rhooks grew the PE allocation from
    // 0x74 to 0x78 for this field).
    /* 44 */ float m_iconSize;
};

} // namespace world
} // namespace hfe
} // namespace dice

#endif // GAME_OBJECTS_STRATEGICOBJECTCOMPTEMPLATE_H
