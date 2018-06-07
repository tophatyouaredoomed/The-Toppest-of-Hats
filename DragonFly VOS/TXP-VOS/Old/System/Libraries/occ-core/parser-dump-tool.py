import re
import string
import sys

"""
mapping_file = "occ-core/parser/token-names.h"
mapping = {}
f = file(mapping_file)
s = f.readline()
while s:
    match = re.match("([a-zA-Z_][a-zA-Z0-9_]*)( |\t)*=( |\t)*(\d+)", s)
    if match:
        name = match.group(1)
        code = match.group(4)
        try:
            mapping[int(code)] = name
        except ValueError:
            pass
    s = f.readline()
"""


class NonLeaf:
    def __init__(self, name, l, r):
        self.name = name
        self.l = l
        self.r = r
    
    def is_leaf(self): return False
    
class Leaf:
    def __init__(self, name, data):
        self.name = name
        self.data = data
        
    def is_leaf(self): return True

class DotPtreeOutputter:
    def __init__(self, stream, name = "ptree"):
        self.stream = stream
        self.counter = 0
        print >>self.stream, "digraph %s {" % name
        print >>self.stream, 'node[shape="box",width=.1,height=.1];'
        
    def close(self):
        print >>self.stream, "}"
        self.stream = None

    def make_name(self, i):
        return "node%d" % i

    def dump_node(self, p):
        if p:
            label = p.name
            if p.is_leaf(): label = label + "\\n" + self.escape_quotes(p.data)
            print >>self.stream, \
                '%s[label="%s"];' % (self.make_name(self.counter), label)
        else:
            print >>self.stream, \
                '%s[shape="point"];' % self.make_name(self.counter)
        self.counter = self.counter + 1
    
    def dump_ptree(self, p, parent = None):
        myself = self.counter
        if parent != None: 
            print >>self.stream, '%s -> %s;' \
                % (self.make_name(parent), self.make_name(myself))
        self.dump_node(p)
        if p and not p.is_leaf():
            self.dump_ptree(p.l, myself)
            self.dump_ptree(p.r, myself)
    
    def escape_quotes(self, s):
        return string.replace(s, '"', '\\"')


def ptree2str(p):
    if not p: return "0"
    else:
        if p.is_leaf(): return "@" + p.name + "[" + p.data + "]"
        else: return "@" + p.name + "[" + ptree2str(p.l) + "," + ptree2str(p.r) + "]"

"""For two given trees creates a tree that represents a diff between them.
   If trees are identical, diff is equal to them. Otherwise, diff is
   a tree with special "MISMATCH" binary nodes indicating differing
   subtrees."""
def ptree_diff(p1, p2):
    if p1 == None and p2 == None: return None
    if p1 == None or p2 == None: return NonLeaf("MISMATCH", p1, p2)
    if p1.is_leaf() and p2.is_leaf():
        if p1.name == p2.name  and  p1.data == p2.data: return p1
        else: return NonLeaf("MISMATCH", p1, p2)
    else:
        if p1.name != p2.name: return NonLeaf("MISMATCH", p1, p2)
        else: 
            return NonLeaf(p1.name, ptree_diff(p1.l, p2.l), ptree_diff(p1.r, p2.r))

""" Returns true iff given ptree has a MISMATCH node."""
def has_mismatch(ptree):
    if ptree == None: return False
    if ptree.is_leaf(): return False
    if ptree.name == "MISMATCH": return True
    return has_mismatch(ptree.l) or has_mismatch(ptree.r)

def parse_ptree(tokens):
    if tokens[0] == "*":
        if tokens[2] != '[': 
            raise "expected '[', got '%s' at %s" % (tokens[2], string.join(tokens[2:]))

        if tokens[4] != ']': 
            raise "expected ']', got '%s' at %s" %(tokens[2], string.join(tokens[4:]))
        result = Leaf(tokens[1], tokens[3])
        del tokens[0:5]
        return result
    elif tokens[0] == "@":
        name = tokens[1]
        if tokens[2] != '[':
            raise "expected '@<name>[', got '@%s%s' at %s" %(tokens[1], tokens[2], string.join(tokens[2:]))
        del tokens[0:3]
        l = parse_ptree(tokens)
        if tokens[0] != ",":
            raise "expected ',', got '%s' at %s" % (tokens[0], string.join(tokens))
        del tokens[0]
        r = parse_ptree(tokens)
        if tokens[0:1] != [']']:
            raise "expected ']', got '%s' at %s" % (string.join(tokens[0:1]), string.join(tokens))
        del tokens[0]
        return NonLeaf(name,l,r)
    elif tokens[0] == "0":
        del tokens[0]
        return None
    else: raise "expected '*' or '@', got '%s' at %s" (tokens[0], string.join(tokens))

def tokenize(s):
    s = str(s)
    toks = []
    while s:
        m = re.match("(\n|\t| )+", s)
        if m: s = s[len(m.group()):]
        else:
            if s[0] == '"':
                buf = ['"']
                s = s[1:]
                while s and s[0] != '"':
                    if s[0:1] == '\\"':
                        buf.append(s[0])
                        buf.append(s[1])
                        s = s[2:]
                    else:
                        buf.append(s[0])
                        s = s[1:]
                if not s: raise "unterminated string '" + string.join(buf,'') + "'"
                buf.append('"')
                s = s[1:]
                toks.append(string.join(buf,''))
            else:
                m = re.match("[*@0,]|\[|\]|([a-zA-Z_][a-zA-Z0-9_]*)", s)
                if m:
                    toks.append(m.group())
                    s = s[len(m.group()):]
                else:
                    raise "cannot match token at '" + string.join(s,'') + "'" 
    return toks
            

if sys.argv[1:2] == ["--help"]:
    print >>sys.stderr, """
    USAGE: parser-dump-tool dump2dot DUMPFILE [-o DOTFILE]
           parser-dump-tool diff     DUMPFILE DUMPFILE [-o DOTFILE]
           parser-dump-tool diff2dot DUMPFILE DUMPFILE [-o DOTFILE]
"""
    sys.exit(0)
elif sys.argv[1:2] == ["diff"] or sys.argv[1:2] == ["diff2dot"]:
    p1 = parse_ptree(tokenize(file(sys.argv[2]).read()))
    p2 = parse_ptree(tokenize(file(sys.argv[3]).read()))
    if sys.argv[4:5] == ['-o']: out = file(sys.argv[5], "w")
    else: out = sys.stdout
    diffptree = ptree_diff(p1, p2)
    if has_mismatch(diffptree): 
        if sys.argv[1] == "diff": print "trees differ"
        else: 
            o = DotPtreeOutputter(out, "diffptree")
            o.dump_ptree(diffptree)
            o.close()
        sys.exit(1)
elif sys.argv[1:2] == ["dump2dot"]:
    if sys.argv[3:4] == ['-o']: out = file(sys.argv[4], "w")
    else: out = sys.stdout
    o = DotPtreeOutputter(out)
    toks = tokenize(file(sys.argv[2]).read())
    while toks:
        o.dump_ptree(parse_ptree(toks))
    o.close()
else:
    print >>sys.stdout, "parser-dump-tool: invalid options, use '--help'"    
    sys.exit(1)

