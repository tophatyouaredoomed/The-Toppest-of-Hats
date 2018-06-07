//@beginlicenses@
//@license{chiba-tokyo}{}@
//@license{Grzegorz Jakacki}{2004}@
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
//  Grzegorz Jakacki make(s) no representations about the suitability of this
//  software for any purpose. It is provided "as is" without express or implied
//  warranty.
//  
//  Copyright (C) 2004 Grzegorz Jakacki
//
//@endlicenses@

#include <iostream>
#include <cassert>
#include <occ-core/parser/Ptree.h>
#include <occ-core/parser/Encoding.h>
#include <occ-core/parser/PtreeAndEncodingPrinter.h>


namespace Opencxx {
namespace PtreeAndEncodingPrinter {

    void PrintWithEncodings(std::ostream& s, Ptree* node, int indent)
    {
        assert(node);
        
        char* type = node->GetEncodedType();
        if (type) {
            s << '#';
            Encoding::Print(s, type);
        }
        
        char* name = node->GetEncodedName();
        if (name) {
            s << '@';
            Encoding::Print(s, name);
        }
        
        if (dynamic_cast<NonLeaf*>(node)) {
            Ptree* rest = node;
            s << "[";
            while (rest) {
                if (rest->IsLeaf()) {
                    s << "@";
                    PrintWithEncodings(s, rest, indent);
                    rest = 0;
                }
                else {
                    Ptree* head = rest->Car();
                    if (head) {
                        PrintWithEncodings(s, indent, depth);
                    }
                    else {
                        s << "0";
                    }
                    rest = rest->Cdr();
                    if (rest != 0)
                    {
                        s << " ";
                    }
                }
            }
            s << "]";
        }
        else {
            assert(dynamic_cast<Leaf*>(node));
            node->Print(s, indent, 0);
        }
    }

}}
