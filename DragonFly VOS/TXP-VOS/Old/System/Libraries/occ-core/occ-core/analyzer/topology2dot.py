# Permission to use, copy, distribute and modify this software and its
# documentation for any purpose is hereby granted without fee, provided that
# the above copyright notice appears in all copies and that both that copyrig
# notice and this permission notice appear in supporting documentation.
#
# Grzegorz Jakacki make(s) no representations about the suitability of this
# software for any purpose. It is provided "as is" without express or implied
# warranty.
# 
# Copyright (C) 2004 Grzegorz Jakacki

import StringIO
import re
import string
import sys
import tokenize

def get_leaf(buf):
    name_match = re.match("[A-Za-z_][A-Za-z0-9_]*", buf)
    if not name_match: raise "node name expected, got '" + buf + "'"
    name = name_match.group(1)
    buf = buf[len(name):]

def get_string_value(quoted):
    assert(quoted[0] == '"')
    assert(quoted[-1] == '"')
    specials = {}
    specials['n'] = '\n'
    specials['t'] = '\t'
    
    i = 1
    result = []
    while i < len(quoted) - 1:
        if quoted[i] == '\\':
            i = i + 1
            if quoted[i] in specials:
                result.append(specials[quoted[i]])
            else:
                result.append(quoted[i])
        else:
            result.append(quoted[i])
        i = i + 1
    return string.join(result, '')
    

def get_leaves(leaves):

    def pi1((x,y)): return x
    def pi2((x,y)): return y
    
    input = StringIO.StringIO(leaves)  
    tokens = []
 
    for (type, value, start, end, line) in tokenize.generate_tokens(input.readline):
        token = tokenize.tok_name[type]
        if token == 'OP': tokens.append((token+value, None))
        else: tokens.append((token, value))
    
    result = []
    while tokens:
        if map(pi1, tokens[0:4]) == ['NAME','OP(','STRING','OP)']:
            value = tokens[2][1]
            result.append((tokens[0][1], get_string_value(value)))
            del tokens[0:4]
        elif map(pi1, tokens[0:1]) == ['NAME']:
            result.append((tokens[0][1], None))
            del tokens[0]
        elif map(pi1, tokens[0:1]) == ['ENDMARKER']:
            break
        else:
            raise "syntax error: `" + str(tokens) + "'"
    
    return result

def parse_line(line):
    match = re.match('([A-Za-z_][A-Za-z0-9_]*)( |\t)+(([RL*.N])+)( |\t)+(.*)\n', line)
    if not match:
        raise "cannot parse: " + line
    root = match.group(1)
    topo = match.group(3)
    raw_leaves = match.group(6)
    leaves = get_leaves(raw_leaves)
    return (root, topo, leaves)


def stringify(text):
    result = []
    i = 0
    for t in text:
        if   t == '\n': result.append("\\n")
        elif t == '\t': result.append("\\t")
        elif t == '\\': result.append("\\\\")
        elif t == '"' : result.append('\\"')
        else: result.append(t)
    return string.join(result,'')

class GraphvizOutputter:
    def __init__(self, out):
        self.out = out
        self.subgraph = 0

    def open_graph(self, name):
        print >> self.out, "digraph %s {" % name
        print >> self.out, "  node[shape=box,width=.1,height=.1];"
    
    def close_graph(self):
        print >> self.out, "}"
    
    def next_subgraph(self):
        self.subgraph = self.subgraph + 1
        
    def arrow(self, beg, end):
        print >>self.out, "  node%d_%d -> node%d_%d;" \
            % (beg, self.subgraph, end, self.subgraph)
    
    def non_leaf(self, index):
        print >>self.out, '  node%d_%d[label="NonLeaf",color=lightgray,fontcolor=lightgray];' \
            % (index, self.subgraph)

    def leaf(self, index, label):
        print >>self.out, '  node%d_%d[label="%s",style=filled,fillcolor=lightgray];' \
            % (index, self.subgraph, label)
    
    def ptree(self, index, label):
        print >>self.out, '  node%d_%d[label="%s",style=bold];' \
            % (index, self.subgraph, label)

    def nil(self, index):
        print >>self.out, '  node%d_%d[shape="point",label=""];' \
            % (index, self.subgraph)


def draw(outputter, root, topo, leaves):

    def recurse(outputter, parent, topo, leaves):
        assert(topo)
        index = len(topo)
        outputter.arrow(parent, index)
        if topo[0] == '.': 
            outputter.nil(index)
            del topo[0]
        elif topo[0] == '*':
            outputter.non_leaf(index)
            del topo[0]
            recurse(outputter, index, topo, leaves)
            recurse(outputter, index, topo, leaves)
        elif topo[0] == 'L':
            outputter.leaf(index, "%s\\n%s" % (leaves[0][0], stringify(leaves[0][1])))
            del topo[0]
            del leaves[0]
        elif topo[0] == 'N':
            outputter.ptree(index, "%s" % leaves[0][0])
            del topo[0]
            del leaves[0]
        else: raise "invalid symbol in topology: `%s'" % topo[0]

    topo_copy = list(topo)
    leaves_copy = list(leaves)
    root_index = len(topo)
    outputter.ptree(root_index, root)
    assert(topo_copy[0] == 'R')
    del topo_copy[0]
    recurse(outputter, root_index, topo_copy, leaves_copy)
    recurse(outputter, root_index, topo_copy, leaves_copy)
    assert(not leaves_copy)
    assert(not topo_copy)


if sys.argv[0:1] == ["--help"]:
    print """
USAGE: dump-ast FILES... | python topology2dot | dot -Tps -o OUTFILE

  Converts node topologies generated by `dump-ast' into
  GraphViz tree descriptions.
"""
    sys.exit(0)

topos = {}
outputter = GraphvizOutputter(sys.stdout)
outputter.open_graph("nodes")
line = sys.stdin.readline()
while line:
    (root,topo,leaves) = parse_line(line)
    draw(outputter, root,topo,leaves)
    #if (root,topo) in topos: 
    #    if leaves not in topos[(root,topo)]:
    #        topos[(root,topo)].append(leaves)
    #else: topos[(root,topo)] = [leaves]
    line = sys.stdin.readline()
    outputter.next_subgraph()
outputter.close_graph()

