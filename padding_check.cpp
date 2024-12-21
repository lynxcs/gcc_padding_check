/* FIXME: Missing features - */
/* Exclude certain dirs */
/* Optimize algo */

#include <chrono>
#include <gcc-plugin.h>

#include <tree.h>
#include <tree-check.h>
#include <tree-pass.h>
#include <plugin-version.h>
#include <cp/cp-tree.h>
#include <diagnostic.h>
#include <langhooks.h>
#include <print-tree.h>
#include <stringpool.h>
#include <attribs.h>
#include <cpplib.h>

#include <iostream>
#include <unistd.h>
#include <vector>
#include <algorithm>
#include <sstream>
#include <string_view>
#include <ostream>
#include <ranges>

#include <c-family/c-pretty-print.h>
#include <cp/cxx-pretty-print.h>

#include "stor-layout.h"

/* FIXME: Remove this - manual format instead - having 2 variants is cumbersome */
#ifdef USE_CXX_PRINT
    #define pretty_printer cxx_pretty_printer
    #define pretty_print_type(pp) \
        pp.declaration(name.field)
        
#else
    #define pretty_printer c_pretty_printer
    #define pretty_print_type(pp) \
        pp.declaration(name.field); \
        pp.direct_abstract_declarator(TREE_TYPE(name.field))
#endif

#define DECL_NAME_STRING(x) IDENTIFIER_POINTER(DECL_NAME(x))
#define BINFO_NAME_STRING(x) IDENTIFIER_POINTER(DECL_NAME(TYPE_NAME(x)))
#define TYPE_NAME_LOCATION(x) EXPR_LOCATION(TYPE_NAME(x))

#if 0
#define INFO(x, ...) fprintf(stderr, x __VA_OPT__(,) __VA_ARGS__);
struct ClockCalculator {
    using Hr = std::chrono::high_resolution_clock;
    Hr::time_point t1;
    tree type;

    ClockCalculator(tree type) noexcept : t1(Hr::now()), type(type) {}

    ~ClockCalculator() {
        auto t2 = Hr::now();
        std::cerr << "Analysis for " << type_name_or_anon(type) << " took: " << (t2 - t1) << std::endl;
    }
};
#define CALC_TIME(type) ClockCalculator(type)
#else
#define INFO(...)
#define CALC_TIME(type)
#endif

static bool is_anon_type(tree type) noexcept {
    return !TYPE_NAME(type) || TYPE_ANON_P(type);
}

static bool is_anon_decl(tree field) noexcept {
    return !DECL_NAME(field);
}

static const char *type_name_or_anon(tree type) noexcept {
    if (!is_anon_type(type)) {
        return TYPE_NAME_STRING(type);
    } else {
        return "<anonymous>";
    }
}

// We must assert that this plugin is GPL compatible
int plugin_is_GPL_compatible;

// Plugin info
static struct plugin_info padding_plugin_info = {
    .version = "1.0",
    .help = "Warns when structure padding can be reduced by reordering fields."
};

static constexpr struct attribute_spec ignore_padding_attr_spec = {
    "ignore_rem_padding",
    0,
    0,
    false,
    true,
    false,
    false,
    nullptr,
    nullptr
};

struct range_static_label_gen : public range_label
{
    const char *label;
    constexpr range_static_label_gen(const char *label) noexcept : label(label) {}

    label_text get_text(unsigned) const override
    {
        return label_text::borrow(label);
    }
};

/* Returns "TypeName<ptr/ref/const> MemberName" */
struct FieldNamer
{
    tree field;
    constexpr FieldNamer(tree f) noexcept : field(f) {}

    friend std::ostream &operator<<(std::ostream &s, const FieldNamer &name) noexcept;
};

std::ostream &operator<<(std::ostream &s, const FieldNamer &name) noexcept
{
    pretty_printer pp;
    auto type = TREE_TYPE(name.field);
    if (TREE_CODE(type) == REFERENCE_TYPE)
    {
        pretty_print_type(pp);
        s << pp_formatted_text(&pp);
    }
    else if (TREE_CODE(type) == POINTER_TYPE)
    {
        pretty_print_type(pp);
        s << pp_formatted_text(&pp);
    }
    else if (TREE_CODE(type) == UNION_TYPE) {
        s << "union ";
        s << type_name_or_anon(type);
        s << " { ... }";
        if (DECL_NAME(name.field)) {
            s << ' ' << DECL_NAME_STRING(name.field);
        }
    }
    else if (TREE_CODE(type) == ARRAY_TYPE) {
        pretty_print_type(pp);
        s << pp_formatted_text(&pp);
    }
    else
    {
        if (TYPE_READONLY(type))
        {
            s << "const ";
        }
        bool is_anon = is_anon_decl(name.field) || is_anon_type(type);
        if (!is_anon) {
            s << type_name_or_anon(type)
              << " "
              << (DECL_NAME(name.field) ? DECL_NAME_STRING(name.field) : "<anonymous>");
        } else {
            s << "struct "
              << type_name_or_anon(type)
              << " { ... } "
              << (DECL_NAME(name.field) ? DECL_NAME_STRING(name.field) : "");
        }
    }
    return s;
}

static location_t get_goto_location(tree type, location_t trigger) {
    auto loc = DECL_SOURCE_LOCATION(TREE_CHAIN(type));
    if (loc != UNKNOWN_LOCATION) {
        return loc;
    }

    if (trigger != UNKNOWN_LOCATION) {
        return trigger;
    }


    return input_location;
}

static bool is_base_class(tree type, tree field) {
    if (TREE_CODE(TREE_TYPE(field)) != RECORD_TYPE) {
        return false;
    }

    tree binfo = TYPE_BINFO(type);
    tree base_binfo;
    for (int i = 0; BINFO_BASE_ITERATE (binfo, i, base_binfo); ++i) {
        if (base_binfo) {
            unsigned HOST_WIDE_INT position_base = tree_to_shwi(BINFO_OFFSET(base_binfo));
            unsigned HOST_WIDE_INT position_field = int_byte_position(field);
            if (position_base == position_field) {
                return true;
            }
        }
    }

    return false;
}

static bool field_sort(tree a, tree b) noexcept {
    if (TYPE_ALIGN_UNIT(TREE_TYPE(a)) > TYPE_ALIGN_UNIT(TREE_TYPE(b))) {
        return true;
    }

    if (int_size_in_bytes(TREE_TYPE(a)) > int_size_in_bytes(TREE_TYPE(b))) {
        return true;
    }

    if (DECL_FIELD_OFFSET(a) && DECL_FIELD_OFFSET(b)) {
        if (int_byte_position(a) < int_byte_position(b)) {
            return true;
        }
    } else {
        /* They don't have offsets (for some reason), so just compare addresses & hope for the best.
         * This part is only for maintaining decl order if it's OK, so not that important.
         */
        if (a < b) {
            return true;
        }
    }

    return false;
}

template <class T>
concept InRange = std::ranges::input_range<T>;

static auto check_layout(const InRange auto& bases, const InRange auto &fields) {
    unsigned HOST_WIDE_INT biggest_alignment = 0;
    unsigned HOST_WIDE_INT total_new_size = 0;
    
    for (const auto &field : bases) {
        unsigned HOST_WIDE_INT field_size = int_size_in_bytes(TREE_TYPE(field));
        unsigned HOST_WIDE_INT field_alignment = TYPE_ALIGN_UNIT(TREE_TYPE(field));
        biggest_alignment = std::max(biggest_alignment, field_alignment);

        total_new_size = ROUND_UP(total_new_size, field_alignment);
        total_new_size += field_size;
    }

    for (const auto &field : fields) {
        unsigned HOST_WIDE_INT field_size = int_size_in_bytes(TREE_TYPE(field));
        unsigned HOST_WIDE_INT field_alignment = TYPE_ALIGN_UNIT(TREE_TYPE(field));
        biggest_alignment = std::max(biggest_alignment, field_alignment);

        total_new_size = ROUND_UP(total_new_size, field_alignment);
        total_new_size += field_size;
    }

    total_new_size = ROUND_UP(total_new_size, biggest_alignment);
    return total_new_size;
}

#define FIELD_ALIGN(field) TYPE_ALIGN_UNIT(TREE_TYPE(field))

struct TriggerInfo {
    tree type;
    size_t original_size;
    location_t trigger_location;
};

static void try_emit_warning(const TriggerInfo &info, size_t new_size, const InRange auto &bases,
                        const InRange auto &fields) {
  line_maps m{};
  rich_location loc{&m, get_goto_location(info.type, info.trigger_location)};
  range_static_label_gen label{"error triggered by"};

  if (new_size < info.original_size) {
    std::stringstream ss{};
    ss << "Possible optimal layout:\n";

    for (const auto &field : bases) {
      ss << "\t(Base) - " << FieldNamer{field} << '\n';
      auto field_loc = DECL_SOURCE_LOCATION(field);
      if (field_loc != UNKNOWN_LOCATION) {
        if (field_loc == info.trigger_location) {
          loc.add_range(info.trigger_location, SHOW_RANGE_WITH_CARET, &label);
        } else {
          loc.add_range(DECL_SOURCE_LOCATION(field), SHOW_RANGE_WITHOUT_CARET);
        }
      }
    }

    for (const auto &field : fields) {
      ss << '\t' << FieldNamer{field} << '\n';
      auto field_loc = DECL_SOURCE_LOCATION(field);
      if (field_loc != UNKNOWN_LOCATION) {
        if (field_loc == info.trigger_location) {
          loc.add_range(info.trigger_location, SHOW_RANGE_WITH_CARET, &label);
        } else {
          loc.add_range(DECL_SOURCE_LOCATION(field), SHOW_RANGE_WITHOUT_CARET);
        }
      }
    }

    auto s = ss.str();
    range_static_label_gen expanded{s.c_str()};
    /* FIXME: Maybe display as a fixit ? */
    loc.add_range(input_location, SHOW_LINES_WITHOUT_RANGE, &expanded);

    warning_at(&loc, 0, "Type '%s' padding can be reduced (%luB to %luB).\n",
               type_name_or_anon(info.type), info.original_size, new_size);
  }
}

/* Both input vars must be sorted from high -> low */
static void layout_testing(const TriggerInfo &info, std::vector<tree> &sorted_bases, std::vector<tree> &sorted_fields) {
    /* Downward traverse check (high -> low)*/
    if (sorted_bases.empty() || sorted_fields.empty() ||
        FIELD_ALIGN(sorted_bases.back()) >=
            FIELD_ALIGN(sorted_fields.front())) {
        /* guaranteed that downward traverse is most efficient */
        try_emit_warning(info, check_layout(sorted_bases, sorted_fields),
                         sorted_bases, sorted_fields);
        return;
    }
    
    if (FIELD_ALIGN(sorted_bases.front()) >= FIELD_ALIGN(sorted_fields.back())) {
        /* guaranteed that upward traverse is most efficient */
        auto sorted_bases_rev = sorted_bases | std::views::reverse;
        auto sorted_fields_rev = sorted_fields | std::views::reverse;
        try_emit_warning(info, check_layout(sorted_bases_rev, sorted_fields_rev), sorted_bases_rev, sorted_fields_rev);
        return;
    }

    /* Base class alignment is between min & max of member fields - toss up on whether we can improve */
    size_t downward_size = check_layout(sorted_bases, sorted_fields);
    size_t upward_size = check_layout(sorted_bases | std::views::reverse, sorted_fields | std::views::reverse);
    if (downward_size > info.original_size || upward_size > info.original_size) {
        if (downward_size >= upward_size) {
            try_emit_warning(info, check_layout(sorted_bases, sorted_fields), sorted_bases, sorted_fields);
        } else {
            auto sorted_bases_rev = sorted_bases | std::views::reverse;
            auto sorted_fields_rev = sorted_fields | std::views::reverse;
            try_emit_warning(info, check_layout(sorted_bases_rev, sorted_fields_rev), sorted_bases_rev, sorted_fields_rev);
        }
    }

    /*
    * TODO: Check 'mixed' traversal
    *   - If not at pow-2 - Insert exactly enough to align to maximum alignment pow-2
    *   - Check maximum alignment req pow-2
    *   - If at that - insert fields in align + size max -> low
    *   - Else use non max alignment structs to reach that maximum alignment req (if possible)
    */
    /* Also - check what's done in clang - they have some sort of best-try
    * optimizing layout thingy as well. */
}

// Function to calculate total size of a structure and check for padding
static void analyze_structure(tree type) {
    if (TREE_CODE(type) != RECORD_TYPE || TYPE_PACKED(type) == true) {
        return;
    }

    if (lookup_attribute("ignore_rem_padding", TYPE_ATTRIBUTES(type))) {
        return;
    }

    /* Don't analyze system headers since they throw away warning anyways */
    auto expanded_input_loc = expand_location(input_location);
    if (expanded_input_loc.sysp) {
        return;
    }

    CALC_TIME(type);
    inform(input_location, "Processing %s\n", type_name_or_anon(type));

    unsigned HOST_WIDE_INT alignment = TYPE_ALIGN_UNIT(type);
    unsigned HOST_WIDE_INT original_size = int_size_in_bytes(type);

    if (alignment == original_size) {
        /* Fast-path */
        return;
    }

    std::vector<tree> sorted_base_fields{};
    std::vector<tree> sorted_field_types{};

    bool is_upward_trend = false;
    bool is_downward_trend = false;
    bool should_perform_analysis = false;
    bool non_base_encountered = false;

    location_t trigger_location = UNKNOWN_LOCATION;

    for (tree last_field = nullptr, field = TYPE_FIELDS(type); field; field = TREE_CHAIN(field)) {
        if (TREE_CODE(field) != FIELD_DECL) {
            continue;
        }

        if (DECL_BIT_FIELD(field)) {
            /* Not analyzing structs with bitfields */
            return;
        }

        if (TREE_CODE(TREE_TYPE(field)) == TEMPLATE_TYPE_PARM) {
            /* Not analyzing structs that contain templated member vars */
            return;
        }

        if (!non_base_encountered && is_base_class(type, field)) {
            sorted_base_fields.emplace_back(field);
        } else {
            non_base_encountered = true;
            sorted_field_types.emplace_back(field);
        }

        unsigned HOST_WIDE_INT field_size = int_size_in_bytes(TREE_TYPE(field));
        unsigned HOST_WIDE_INT field_alignment = TYPE_ALIGN_UNIT(TREE_TYPE(field));
        if (field_size % field_alignment != 0) {
            /* Something iffy - field size isn't a multiple of field alignment */
            return;
        }
        if (!should_perform_analysis && last_field != nullptr) {
            unsigned HOST_WIDE_INT last_field_alignment = TYPE_ALIGN_UNIT(TREE_TYPE(last_field));
            if (last_field_alignment > field_alignment) {
                is_upward_trend = true;
                if (is_downward_trend == true) {
                    should_perform_analysis = true;
                    trigger_location = DECL_SOURCE_LOCATION(field);
                }
            } else if (last_field_alignment < field_alignment) {
                is_downward_trend = true;
                if (is_upward_trend == true) {
                    should_perform_analysis = true;
                    trigger_location = DECL_SOURCE_LOCATION(field);
                }
            }
        }
        last_field = field;
    }

    if (!should_perform_analysis || (sorted_base_fields.size() + sorted_field_types.size() < 3)) {
        /* In general, it has a trend of either: */
        /* x, y, z -> where x <= y <= z */
        /* x, y, z -> where x >= y >= z */
        /* This means we already have one of smallest layout */
        /* Also, if we have < 3 fields, then we can't really save on alignment anyways*/
        return;
    }

    std::sort(sorted_base_fields.begin(), sorted_base_fields.end(), field_sort);

    std::sort(sorted_field_types.begin(), sorted_field_types.end(), field_sort);

    layout_testing(TriggerInfo {
        .type = type,
        .original_size = original_size,
        .trigger_location = trigger_location,
    }, sorted_base_fields, sorted_field_types);
}

// Callback function for GCC's plugin system
static void finish_type_callback(void *event_data, void *user_data) {
    analyze_structure((tree)event_data);
}

int plugin_init (struct plugin_name_args *plugin_info,
	     struct plugin_gcc_version *version)
{
  if (!plugin_default_version_check (version, &gcc_version))
    {
        std::cerr << "This GCC plugin is for version " << GCCPLUGIN_VERSION_MAJOR
                  << "." << GCCPLUGIN_VERSION_MINOR << "\n";
        return 1;
    }

    register_callback(plugin_info->base_name, PLUGIN_INFO, NULL, &padding_plugin_info);
    register_callback(plugin_info->base_name, PLUGIN_FINISH_TYPE, finish_type_callback, NULL);
    register_callback(plugin_info->base_name, PLUGIN_ATTRIBUTES, [](auto, auto) {
        register_attribute(&ignore_padding_attr_spec);
    }, NULL);
    return 0;
}
