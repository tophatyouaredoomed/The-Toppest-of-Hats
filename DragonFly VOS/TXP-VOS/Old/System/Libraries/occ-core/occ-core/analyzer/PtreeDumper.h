#ifndef guard_occ_core_analyzer_PtreeDumper_h
#define guard_occ_core_analyzer_PtreeDumper_h
#include <iostream>
#include <vector>
#include <string>
#include <typeinfo>
#include <cassert>
#include <occ-core/parser/NonLeaf.h>

namespace Opencxx {

class PtreeDumper
{
public:
    PtreeDumper(std::ostream& output) 
      : output_(output) 
      , indent_(0)
    {
    }
    
    void Dump(Opencxx::Ptree* p)
    {
        using std::endl;
        using std::string;
        
        for(int i = 0; i < indent_; ++i) {
            output_ << "  ";
        }
        if ( !p ) {
            output_ << "-" << endl;
            return;
        }
        
        string demangled(Demangle(typeid(*p).name()));
        output_ << demangled;
        
        if (p->IsLeaf()) {
            char* position = p->GetPosition();
            if (position) {
                string text(position, position + p->GetLength());
                output_ << " \"" << Escaped(text) << "\"";
            }
        }
        
        output_ << endl;
        
        if (! p->IsLeaf()) {
            ++indent_;
            Dump(p->Car());
            Dump(p->Cdr());
            --indent_;
        }
    }
    
private:
    static std::string Escaped(const std::string& s)
    {
        std::vector<char> result;
        for(int i = 0; i < s.size(); ++i)
        {
            switch (s[i])
            {
                case '\n' : 
                case '\"' :
                case '\\' :
                    result.push_back('\\');
                default:    
                    result.push_back(s[i]);
            }
        }
        return std::string(result.begin(), result.end());
    }
    
    /** Kludge, kludge, kludge. But works. */
    static std::string Demangle(const std::string& s)
    {
        using std::string;
        string result(s);
        StripPrefix("N7Opencxx", result);
        StripLeadingDigits(result);
        string::iterator end = result.end();
        assert(end != result.begin());
        --end;
        return string(result.begin(), end);
    }
    
    static void StripPrefix(const std::string& prefix, std::string& s)
    {
        using std::string;
        string::const_iterator prefIter = prefix.begin();
        string::const_iterator prefEnd  = prefix.end();
        string::iterator iter = s.begin();
        string::iterator end  = s.end();
        
        while (iter != end  &&  prefIter != prefEnd  &&  *iter == *prefIter)
        {
            ++iter;
            ++prefIter;
        }
        
        if (prefIter == prefEnd) {
            s = string(iter, s.end());
        }
    }
    
    static void StripLeadingDigits(std::string& s)
    {
        using std::string;
        string::iterator iter = s.begin();
        string::iterator end  = s.end();
        while (iter != end  &&  '0' <= *iter &&  *iter <= '9') {
            ++iter;
        }
        s = string(iter, s.end());
    }

private:    
    std::ostream& output_;
    int           indent_;
};

}

#endif /* ! guard_occ_core_analyzer_PtreeDumper_h */
