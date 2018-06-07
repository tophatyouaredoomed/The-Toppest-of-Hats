#include <string>
#include <iostream>
#include <sstream>
#include <cassert>

#include <occ-core/parser/Lex.h>
#include <occ-core/parser/ProgramFromStdin.h>
#include <occ-core/parser/Token.h>

using namespace std;
using namespace Opencxx;

string encode(unsigned char c)
{
    if (c < 32  ||  c > 127)
    {
        stringstream buf;
        buf << "\\x" << hex << c;
        return buf.str();
    }
    else
    {
        return string(c, 1);
    }
}

string encode(const string& s)
{
    string result;
    for(int i = 0; i < s.length(); ++i)
    {
        result += encode(s[i]);
    }
    return result;
}

int main()
{
    ProgramFromStdin p;
    Lex lexer(&p);
    Token t;
    
    int kind = lexer.GetToken(t);
    while (kind)
    {
        assert(kind == t.GetKind());
        cout << t.GetKind() << "\t" << string(t.GetPtr(), t.GetPtr()+t.GetLen()) << endl;
        kind = lexer.GetToken(t);
    }
}
