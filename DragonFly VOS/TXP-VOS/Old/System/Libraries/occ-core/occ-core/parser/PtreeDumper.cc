#include <string>
#include <iostream>
#include <occ-core/parser/Ptree.h>

namespace Opencxx {

    namespace PtreeDumper
    {
        void Dump(std::ostream& os, const Ptree* p)
        {
            using std::string;
            if (p == 0)
            {
                os << "0";
            }
            else
            {
                if (p->IsLeaf())
                {
                    os << "*" << p->DumpName() << "[";
                    string txt(
                        p->GetPosition(), p->GetPosition() + p->GetLength());
                    os << '\"' << txt << '\"';
                    os << "]";
                }
                else
                {
                    os << "@" << p->DumpName() << "[";
                    Dump(os, p->Car());
                    os << ",";
                    Dump(os, p->Cdr());
                    os << "]";
                }
            }
        }
    }
}
