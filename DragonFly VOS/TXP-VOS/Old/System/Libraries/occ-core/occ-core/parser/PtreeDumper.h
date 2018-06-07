#include <iosfwd>
#include <occ-core/parser/Ptree.h>

namespace Opencxx
{
    namespace PtreeDumper
    {
        void Dump(std::ostream& os, const Ptree* p);
    }
}
