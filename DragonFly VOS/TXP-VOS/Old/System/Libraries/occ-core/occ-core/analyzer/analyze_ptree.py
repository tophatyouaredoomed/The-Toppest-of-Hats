import string
import sys
import re

class PtreeNode:
    def __init__(self, name, l, r, text=None):
        self.name = name
        self.l = l
        self.r = r
        self.text = text

    def term_name(self):
        if self.text:
            return self.name + '"' + escaped(self.text) + '"'
        else:
            return self.name

def dump(ptree, indent = 0):
    if ptree: print indent * "  " + ptree.term_name()
    else: print ident * "  " + "-"
    
def term(ptree):
    if ptree: 
        text = ptree.term_name()
        if not re.match("Leaf", text):
            text = text + '(' + term(ptree.l) + ',' + term(ptree.r) + ')'
        return text
    else: return "nil"

def find_all_paths(ptree, path=[]):
    if ptree:
        name = ptree.term_name()
        if re.match("Leaf", name):
            print path + [name]
        elif re.match("NonLeaf", name):
            find_all_paths(ptree.l, path + ["L"])
            find_all_paths(ptree.r, path + ["R"])
        else:
            print path + [name]
            find_all_paths(ptree.l, [name,"L"]);
            find_all_paths(ptree.r, [name,"R"]);
    else:
        print path + ["nil"]
    

def term_up_to_typed_nodes(ptree, top=True):
    if ptree: 
        text = ptree.term_name()
        if re.match("NonLeaf", text):
            text = text + '(' + term_up_to_typed_nodes(ptree.l) \
                + ',' + term_up_to_typed_nodes(ptree.r) + ')'
        return text
    else: return "nil"



class TokensEndPrematurely:
    def __init__(self): self.trace=[]
    def add(self, node): self.trace.append(node)
    def __repr__(self): return str(self.trace)
        
def escaped(text):
    result = ""
    i = 0
    while i < len(text):
        if   text[i] == '\n': result = result + '\\n'
        elif text[i] == '\t': result = result + '\\t'
        elif text[i] == '"' : result = result + '\\"'
        else: result = result + text[i]
        i = i+1
    return result

def unescape(text):
    result = ""
    i = 0;
    while i < len(text):
        if text[i] == '\\':
            i = i + 1
            if text[i] == 'n': result = result + '\n'
            elif text[i] == 't': result = result + '\t'
            else: result = result + text[i]
        else: result = result + text[i]
        i = i+1
    return result

def read_ptree(tokens):
    if tokens:
        name = tokens[0]
        del tokens[0]
        if name == '-': 
            return None
        else:
            try:
                if tokens and tokens[0][0] == '"':
                    assert(tokens[0][len(tokens[0])-1] == '"')
                    text = tokens[0][1:-1]
                    del tokens[0]
                    return PtreeNode(name, None, None, text)
                else:
                    left = read_ptree(tokens)
                    return PtreeNode(name, left, read_ptree(tokens))
            except TokensEndPrematurely, e:
                e.add(name)
                raise e
    else:
        raise TokensEndPrematurely()

text = sys.stdin.read()
tokens = string.split(text)
while tokens:
    tree = read_ptree(string.split(text))
    find_all_paths(tree)

#tree.dump()
#print term_up_to_typed_nodes(tree)
