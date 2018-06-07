//@beginlicenses@
//@license{Grzegorz Jakacki}{2004}@
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

#include <cassert>
#include <iostream>
#include <occ-core/parser/ProgramFromStdin.h>
#include <occ-core/parser/Lex.h>
#include <occ-core/parser/Parser.h>
#include <occ-core/parser/Ptree.h>
#include <occ-core/parser/ErrorLog.h>
#include <occ-core/parser/Msg.h>
#include <occ-core/parser/PtreeDumper.h>

using namespace Opencxx;

class MockErrorLog : public ErrorLog
{
public:
    void Report(const Msg& m) 
    { 
        m.PrintOn(std::cerr);
        assert(m.GetSeverity() != Msg::Error);
        assert(m.GetSeverity() != Msg::Fatal);
    }
};

int main()
{
    ProgramFromStdin program;
    MockErrorLog errorLog;
    Lex lexer(&program);
    Parser parser(&lexer, errorLog);
    
    Ptree* ptree;
    while (parser.rProgram(ptree))
    {
        PtreeDumper::Dump(std::cout, ptree);
    }
    assert(! parser.SyntaxErrorsFound());
}
