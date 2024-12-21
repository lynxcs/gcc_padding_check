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
    char c;
};

struct MyThing3 { char a; int b; char c; };

struct MyThingBase {
    char b;
};

struct MyThingDerived {
    char a;
    char c;
    struct MyThingBase sss;
    int b;
};

struct MyThingBase2 {
    char a, b, c;
};

/* This should trigger!!! */
typedef struct Fwd Fwd;
struct MyThingFwd {
    char f;
    char d;
    char l;
    Fwd* ptr_fwd;
    const Fwd *const_ptr_fwd;
    Fwd *const ptr_const_fwd;
    char a;
    int b;
    char x;
    char xx;
};

struct __attribute__((ignore_rem_padding)) MyThingIgnore { char a; int b; char c; };
struct MyThingNotIgnore { char a; int b; char c; };

#include "local_inc.h"
#include "remote_inc.h"
#include "ignore_inc.h"
#include "sys_inc.h"