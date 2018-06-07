#include <string>
#include <iostream>
#include <sstream>
#include <cassert>
#include <vector>
#include <utility>

#include <occ-core/parser/Lex.h>
#include <occ-core/parser/ProgramString.h>
#include <occ-core/parser/Token.h>

using namespace std;
using namespace Opencxx;

int main(int ac, char* av[])
{
    ProgramString p;
    p << "abc123";
    Lex lexer(&p);
    Token t;
    
    vector<pair<int,string> > result;
    vector<pair<int,string> > golden;
    
    golden.push_back(make_pair(258, "abc123"));
    
    
    int kind = lexer.GetToken(t);
    while (kind)
    {
        assert(kind == t.GetKind());
        result.push_back(make_pair(
            t.GetKind()
          , string(t.GetPtr(), t.GetPtr()+t.GetLen())
        ));
        kind = lexer.GetToken(t);
    }
    
    assert(result == golden);
    cout << "program-string-name: PASS" << endl;
}
