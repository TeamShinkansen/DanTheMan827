
// DO NOT EDIT THIS FILE - it is machine generated -*- c++ -*-

#ifndef __java_awt_MenuContainer__
#define __java_awt_MenuContainer__

#pragma interface

#include <java/lang/Object.h>
extern "Java"
{
  namespace java
  {
    namespace awt
    {
        class Event;
        class Font;
        class MenuComponent;
        class MenuContainer;
    }
  }
}

class java::awt::MenuContainer : public ::java::lang::Object
{

public:
  virtual ::java::awt::Font * getFont() = 0;
  virtual void remove(::java::awt::MenuComponent *) = 0;
  virtual jboolean postEvent(::java::awt::Event *) = 0;
  static ::java::lang::Class class$;
} __attribute__ ((java_interface));

#endif // __java_awt_MenuContainer__