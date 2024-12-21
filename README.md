# GCC Padding check plugin

GCC has -Wpadded for emitting a warning when padding bytes are added, however
sometimes padding is unavoidable, and a much more useful warning would be to
provide info about when struct padding could be improved by re-ordering
the member variables.

That is exactly what this plugin aims to accomplish.

So e.g. this:

```C
struct Struct {
    uint32_t a;
    uint16_t b;
    uint32_t c;
};
```

would emit a warning that by re-ordering b, the overall struct size would be reduced.

In case the layout is intentional, the attribute __attribute__((ignore_rem_padding)) can be applied to silence this warning.

Written for GCC 12.2.0 - but might work with older/newer versions.
