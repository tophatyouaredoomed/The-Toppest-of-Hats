#ifndef guard_occ_core_analyzer_BindTemplateFunction_h
#define guard_occ_core_analyzer_BindTemplateFunction_h

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

#include <occ-core/analyzer/Bind.h>

namespace Opencxx
{

class TypeInfo;
class Environment;
class Ptree;

class BindTemplateFunction : public Bind {
public:
    BindTemplateFunction(Ptree* d) { decl = d; }
    Kind What();
    void GetType(TypeInfo&, Environment*);
    bool IsType();

private:
    Ptree* decl;
};

}

#endif /* ! guard_occ_core_analyzer_BindTemplateFunction_h */
