#include <stdint.h>

struct MyThing {
    int a;
    int b;
    int d;
    char dda;
    char ddba;
    char dd;
    char c;
};

struct MyThing2 {
    char a;
    int b;
    void my_func() {

    }
    char c;
};

struct MyThing3 { char a; int b; char c; };

struct MyThingBase {
    char b;
};

struct MyThingDerived : public MyThingBase {
    char a;
    char c;
    MyThingBase sss;
    int b;
};

struct MyThingBase2 {
    char a, b, c;
};

/* This should trigger!!! */
typedef struct Fwd Fwd;
struct MyThingFwd : public MyThingBase2 {
    char f;
    char d;
    char l;
    Fwd& fwd;
    const Fwd& const_fwd;
    Fwd* ptr_fwd;
    const Fwd *const_ptr_fwd;
    Fwd *const ptr_const_fwd;
    char a;
    char x;
    int b;
    char xx;
};

struct MyThingRef {
    char x;
    Fwd &fwd;
    const Fwd& const_fwd;
    Fwd* ptr_fwd;
    const Fwd *const_ptr_fwd;
    Fwd *const ptr_const_fwd;
    char y;
};

struct __attribute__((ignore_rem_padding)) MyThingIgnore { char a; int b; char c; };
struct MyThingNotIgnore { char a; int b; char c; };

#include "sys_inc.h"

#include "remote_inc.h"

#include "ignore_inc.h"

#include "local_inc.h"

struct HoldingStruct {
    char c;
    TemplateType<int> a;
    TemplateTypeButNotVar<double> b;
};