#ifndef guard_occ_core_parser_PtreeFstyleCastExpr_h
#define guard_occ_core_parser_PtreeFstyleCastExpr_h

//@beginlicenses@
//@license{chiba_tokyo}{}@
//@license{contributors}{}@
//
//  Permission to use, copy, distribute and modify this software and its  
//  documentation for any purpose is hereby granted without fee, provided that
//  the above copyright notice appears in all copies and that both that copyright
//  notice and this permission notice appear in supporting documentation.
// 
//  1997-2001 Shigeru Chiba, Tokyo Institute of Technology. make(s) no representations about the suitability of this
//  software for any purpose. It is provided "as is" without express or implied
//  warranty.
//  
//  Copyright (C)  1997-2001 Shigeru Chiba, Tokyo Institute of Technology.
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

#include <occ-core/parser/NonLeaf.h>

namespace Opencxx
{

class Encoding;
class Ptree;
class AbstractTranslatingWalker;
class AbstractTypingWalker;
class TypeInfo;

class PtreeFstyleCastExpr : public NonLeaf {
public:
    PtreeFstyleCastExpr(char*, Ptree*, Ptree*);
    int What();
    char* GetEncodedType();
    void Print(std::ostream&, int, int);
    Ptree* Translate(AbstractTranslatingWalker*);
    void Typeof(AbstractTypingWalker*, TypeInfo&);
    const char* DumpName() const { return "PtreeFstyleCastExpr"; }

OPENCXX_DEPRECATED_PUBLIC:
    PtreeFstyleCastExpr(Encoding& encType, Ptree*, Ptree*);

private:
    char* type;
};

}

#endif /* ! guard_occ_core_parser_PtreeFstyleCastExpr_h */
