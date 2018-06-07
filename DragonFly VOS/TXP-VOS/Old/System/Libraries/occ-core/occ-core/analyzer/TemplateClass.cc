//@beginlicenses@
//@license{chiba-tokyo}{}@
//@license{contributors}{}@
//
//  Copyright (C) 1997-2001 Shigeru Chiba, Tokyo Institute of Technology.
//
//  Permission to use, copy, distribute and modify this software and
//  its documentation for any purpose is hereby granted without fee,
//  provided that the above copyright notice appears in all copies and that
//  both that copyright notice and this permission notice appear in
//  supporting documentation.
//
//  Shigeru Chiba makes no representations about the suitability of this
//  software for any purpose.  It is provided "as is" without express or
//  implied warranty.
//
//  -----------------------------------------------------------------
//
//  Permission to use, copy, distribute and modify this software and its  
//  documentation for any purpose is hereby granted without fee, provided that
//  the above copyright notice appears in all copies and that both that copyright
//  notice and this permission notice appear in supporting documentation.
// 
//  Other Contributors (see file AUTHORS) make(s) no representations about the suitability of this
//  software for any purpose. It is provided "as is" without express or implied
//  warranty.
//  
//  Copyright (C)  Other Contributors (see file AUTHORS)
//
//@endlicenses@

#include <occ-core/analyzer/TemplateClass.h>
#include <occ-core/analyzer/Walker.h>
#include <occ-core/analyzer/PtreeTypeUtil.h>

namespace Opencxx
{

void TemplateClass::InitializeInstance(Ptree* def, Ptree* margs)
{
    Class::InitializeInstance(GetClassInTemplate(def), margs);
    template_definition = def;
}

/*
  This is needed because TemplateClass may be instantiated for a class.
*/
Ptree* TemplateClass::GetClassInTemplate(Ptree* def)
{
    Ptree* decl = PtreeUtil::Nth(def, 4);
    if(decl == 0)
	return def;

    Ptree* cdef = PtreeTypeUtil::GetClassTemplateSpec(decl);
    if(cdef == 0)
	return def;
    else
	return cdef;
}

bool TemplateClass::Initialize()
{
    return true;
}

char* TemplateClass::MetaclassName()
{
    return "TemplateClass";
}

Ptree* TemplateClass::TemplateArguments()
{
    return PtreeUtil::Third(template_definition);
}

bool TemplateClass::AcceptTemplate()
{
    return true;
}

}
