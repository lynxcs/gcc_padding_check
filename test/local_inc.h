#pragma once

/* This one is kinda questionable whether should trigger */

struct LocalInc {
    char a;
    int b;
    char c;
};

struct NamedAnonUnion {
    char a;
    /* Named union */
    union namedUnion {
        char d;
        int b;
    } name;
    char c;
};

struct KindaNamedAnonUnion {
    char a;
    /* Named union */
    union {
        char d;
        int b;
    } name;
    char c;
};

struct AnonUnion {
    char a;
    /* Anon union */
    union {
        char d;
        int b;
    };
    char c;
};

struct ExampleInner {
    int b;
};

struct Example {
    char a;
    struct ExampleInner inner;
    char c;
};

struct AnonStruct {
    char a;
    struct {
        int b;
    };
    char c;
};

struct NamedStruct {
    char a;
    struct namedS {
        int b;
    } name;
    struct namedS2 {
        int b;
    };
    struct namedS2 name2;
    char c;
};

struct KindaNamedStruct {
    char a;
    struct {
        int b;
    } name;
    char c;
};

struct VoidPtr {
    char a;
    void *b;
    char c;
};

struct ArrType {
    char a[123];
    char az[0];
    const char *ap[2];
    void (*ptr)(int);
    int b;
    char d;
};

#ifdef __cplusplus
template <typename T>
struct TemplateType {
    char a;
    T b;
    char c;
};

template <typename T>
struct TemplateTypeButNotVar {
    static T var;
    char a;
    int b;
    int c;
    char d;
};
#endif