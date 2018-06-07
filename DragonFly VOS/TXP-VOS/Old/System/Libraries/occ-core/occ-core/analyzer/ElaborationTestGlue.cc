#include <cassert>
#include <iostream>
#include <occ-core/parser/ProgramFromStdin.h>
#include <occ-core/parser/Lex.h>
#include <occ-core/parser/Parser.h>
#include <occ-core/parser/Ptree.h> 
#include <occ-core/parser/ErrorLog.h>
#include <occ-core/parser/Msg.h>
#include <occ-core/analyzer/ClassWalker.h>
#include <occ-core/analyzer/ClassArray.h>
#include <occ-core/analyzer/Class.h>
#include <occ-core/analyzer/MemberList.h>

using namespace Opencxx;
using namespace std;

class MockErrorLog : public ErrorLog
{
public:
    void Report(const Msg& m)
    { 
        m.PrintOn(cerr);
        cerr << endl;
        assert(m.GetSeverity() != Msg::Error);
        assert(m.GetSeverity() != Msg::Fatal);
    }
};   
                            

int main()
{
    MockErrorLog errorLog;
    ProgramFromStdin program;
    Lex lexer(&program);
    Parser parser(&lexer, errorLog);
    ClassWalker w(&parser);
    Ptree* def;
    while (parser.rProgram(def)) {
        w.Translate(def);
    }
    ClassArray& classes(Class::AllClasses());
    for(int i = 0; i < classes.Number(); ++i) {
        Class* cl(classes[i]);
        cout << "class " << cl->Name()->ToString() << " { ";
        MemberList* mlist(cl->GetMemberList());
        for(int j = 0; j < mlist->Number(); ++j) {
            MemberList::Mem* mem(mlist->Ref(j));
            cout << mem->name << " ";
        }
        cout << "} ";
    }
}
