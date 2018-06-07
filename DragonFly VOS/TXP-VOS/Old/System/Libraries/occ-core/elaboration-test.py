import os

def test(name, program, golden_output):
    print "Testing", name, "..."
    inp = file("elaboration-test.in", "w")
    print >>inp, program
    inp.close()
    ret = os.system("./bin/elaboration-test-glue <elaboration-test.in >elaboration-test.out")
    if ret != 0: raise "TEST FAILED"
    out = file("elaboration-test.out")
    real_output = out.read()
    if golden_output != real_output:
        print "EXPECTED:", golden_output
        print "RECEIVED:", real_output
        raise "TEST FAILED"
    print "PASS", name

test("empty_class", "class A {};", "class A { } ")
test("data_member", "class A { int a; };", "class A { a } ")
test("function_member", "class A { void f(int); };", "class A { f } ")
test("dereference", "class A { void f() { int x(*this); } };", "class A { f } ")
test("infix", "class A { void f() { int r = 1+(int)8; } };", "class A { f } ")