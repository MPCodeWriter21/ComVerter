#include <stdio.h>
#define PY_SSIZE_T_CLEAN
#include <Python.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#define MAX_LIST_DEPTH 16

typedef struct {
    int type;          /* 1=ul, 2=ol */
    char marker;       /* '-', '+', '*' for ul; 0 for ol */
    char delimiter;    /* '.', ')' for ol; 0 for ul */
    int content_col;   /* column where item content starts */
    int start_num;     /* starting number for ol; 0 for ul */
    int is_loose;      /* 1=loose list */
    int after_blank;   /* 1=blank line since last item start */
    int item_open;     /* 1=<li> open */
    int marker_col;    /* column where marker starts */
    char *saved_data;  /* saved li_content when sub-list was pushed */
    size_t saved_len;
    size_t saved_cap;
    int first_item_content_pos;   /* sb position of first item's content (-1 if none) */
    int first_item_content_len;   /* length of first item's content in sb */
} ListEntry;

typedef struct {
    char *data;
    size_t len;
    size_t cap;
} StringBuilder;

static StringBuilder *sb_new(void) {
    StringBuilder *sb = (StringBuilder *)malloc(sizeof(StringBuilder));
    if (!sb) return NULL;
    sb->data = (char *)malloc(128);
    if (!sb->data) {
        free(sb);
        return NULL;
    }
    sb->data[0] = '\0';
    sb->len = 0;
    sb->cap = 128;
    return sb;
}

static int sb_ensure(StringBuilder *sb, size_t extra) {
    if (sb->len + extra + 1 > sb->cap) {
        size_t new_cap = sb->cap * 2;
        while (sb->len + extra + 1 > new_cap) new_cap *= 2;
        char *new_data = (char *)realloc(sb->data, new_cap);
        if (!new_data) return -1;
        sb->data = new_data;
        sb->cap = new_cap;
    }
    return 0;
}

static void sb_append(StringBuilder *sb, const char *s) {
    size_t slen = strlen(s);
    if (sb_ensure(sb, slen) == -1) return;
    memcpy(sb->data + sb->len, s, slen);
    sb->len += slen;
    sb->data[sb->len] = '\0';
}

static void sb_append_char(StringBuilder *sb, char c) {
    if (sb_ensure(sb, 1) == -1) return;
    sb->data[sb->len++] = c;
    sb->data[sb->len] = '\0';
}

static void sb_append_n(StringBuilder *sb, const char *s, size_t n) {
    if (sb_ensure(sb, n) == -1) return;
    memcpy(sb->data + sb->len, s, n);
    sb->len += n;
    sb->data[sb->len] = '\0';
}

static void sb_insert(StringBuilder *sb, size_t pos, const char *s, size_t n) {
    if (pos > sb->len) return;
    if (sb_ensure(sb, n) == -1) return;
    memmove(sb->data + pos + n, sb->data + pos, sb->len - pos);
    memcpy(sb->data + pos, s, n);
    sb->len += n;
    sb->data[sb->len] = '\0';
}

static void sb_free(StringBuilder *sb) {
    if (sb) {
        free(sb->data);
        free(sb);
    }
}

static char *sb_detach(StringBuilder *sb) {
    char *result = sb->data;
    free(sb);
    return result;
}

typedef struct {
    const char *name;
    unsigned long cps[4];
    int n_cps;
} EntityEntry;

static int entity_cmp(const void *a, const void *b) {
    return strcmp(((const EntityEntry *)a)->name, ((const EntityEntry *)b)->name);
}

/* HTML5 named entities sorted alphabetically for binary search */
static EntityEntry entities[] = {
    {                   "AElig",            {198}, 1},
    {"ClockwiseContourIntegral",         {0x2232}, 1},
    {                  "Dcaron",            {270}, 1},
    {           "DifferentialD",         {0x2146}, 1},
    {            "HilbertSpace",         {0x210B}, 1},
    {                   "acirc",            {226}, 1},
    {                   "acute",            {180}, 1},
    {                   "aelig",            {230}, 1},
    {                  "agrave",            {224}, 1},
    {                 "alefsym",         {0x2135}, 1},
    {                   "alpha",         {0x03B1}, 1},
    {                     "amp",             {38}, 1},
    {                    "apos",             {39}, 1},
    {                   "aring",            {229}, 1},
    {                  "atilde",            {227}, 1},
    {                    "auml",            {228}, 1},
    {                   "bdquo",         {0x201E}, 1},
    {                    "beta",         {0x03B2}, 1},
    {                  "brvbar",            {166}, 1},
    {                    "bull",         {0x2022}, 1},
    {                     "cap",         {0x2229}, 1},
    {                  "ccedil",            {231}, 1},
    {                   "cedil",            {184}, 1},
    {                    "cent",            {162}, 1},
    {                    "circ",         {0x02C6}, 1},
    {                    "copy",            {169}, 1},
    {                   "crarr",         {0x21B5}, 1},
    {                     "cup",         {0x222A}, 1},
    {                  "curren",            {164}, 1},
    {                   "delta",         {0x03B4}, 1},
    {                     "deg",            {176}, 1},
    {                   "diams",         {0x2666}, 1},
    {                  "divide",            {247}, 1},
    {                  "eacute",            {233}, 1},
    {                   "ecirc",            {234}, 1},
    {                  "egrave",            {232}, 1},
    {                   "empty",         {0x2205}, 1},
    {                    "emsp",         {0x2003}, 1},
    {                    "ensp",         {0x2002}, 1},
    {                 "epsilon",         {0x03B5}, 1},
    {                   "equiv",         {0x2261}, 1},
    {                     "eta",         {0x03B7}, 1},
    {                     "eth",            {240}, 1},
    {                    "euml",            {235}, 1},
    {                    "euro",         {0x20AC}, 1},
    {                   "exist",         {0x2203}, 1},
    {                    "fnof",         {0x0192}, 1},
    {                  "forall",         {0x2200}, 1},
    {                  "frac12",            {189}, 1},
    {                  "frac14",            {188}, 1},
    {                  "frac34",            {190}, 1},
    {                   "frasl",         {0x2044}, 1},
    {                   "gamma",         {0x03B3}, 1},
    {                      "ge",         {0x2265}, 1},
    {                      "gt",             {62}, 1},
    {                    "harr",         {0x2194}, 1},
    {                  "hearts",         {0x2665}, 1},
    {                  "hellip",         {0x2026}, 1},
    {                  "iacute",            {237}, 1},
    {                   "icirc",            {238}, 1},
    {                   "iexcl",            {161}, 1},
    {                  "igrave",            {236}, 1},
    {                   "image",         {0x2111}, 1},
    {                   "infin",         {0x221E}, 1},
    {                     "int",         {0x222B}, 1},
    {                    "iota",         {0x03B9}, 1},
    {                  "iquest",            {191}, 1},
    {                    "isin",         {0x2208}, 1},
    {                    "iuml",            {239}, 1},
    {                   "kappa",         {0x03BA}, 1},
    {                  "lambda",         {0x03BB}, 1},
    {                    "lang",         {0x2329}, 1},
    {                   "laquo",            {171}, 1},
    {                    "larr",         {0x2190}, 1},
    {                   "lceil",         {0x2308}, 1},
    {                   "ldquo",         {0x201C}, 1},
    {                      "le",         {0x2264}, 1},
    {                  "lfloor",         {0x230A}, 1},
    {                  "lowast",         {0x2217}, 1},
    {                     "loz",         {0x25CA}, 1},
    {                     "lrm",         {0x200E}, 1},
    {                  "lsaquo",         {0x2039}, 1},
    {                   "lsquo",         {0x2018}, 1},
    {                      "lt",             {60}, 1},
    {                    "macr",            {175}, 1},
    {                   "mdash",         {0x2014}, 1},
    {                   "micro",            {181}, 1},
    {                  "middot",            {183}, 1},
    {                   "minus",         {0x2212}, 1},
    {                      "mu",         {0x03BC}, 1},
    {                   "nabla",         {0x2207}, 1},
    {                    "nbsp",            {160}, 1},
    {                   "ndash",         {0x2013}, 1},
    {                      "ne",         {0x2260}, 1},
    {                     "ngE", {0x2267, 0x0338}, 2},
    {                      "ni",         {0x220B}, 1},
    {                     "not",            {172}, 1},
    {                   "notin",         {0x2209}, 1},
    {                    "nsub",         {0x2284}, 1},
    {                  "ntilde",            {241}, 1},
    {                      "nu",         {0x03BD}, 1},
    {                  "oacute",            {243}, 1},
    {                   "ocirc",            {244}, 1},
    {                   "oelig",         {0x0153}, 1},
    {                  "ograve",            {242}, 1},
    {                   "oline",         {0x203E}, 1},
    {                   "omega",         {0x03C9}, 1},
    {                 "omicron",         {0x03BF}, 1},
    {                   "oplus",         {0x2295}, 1},
    {                      "or",         {0x2228}, 1},
    {                    "ordf",            {170}, 1},
    {                    "ordm",            {186}, 1},
    {                  "oslash",            {248}, 1},
    {                  "otilde",            {245}, 1},
    {                  "otimes",         {0x2297}, 1},
    {                    "ouml",            {246}, 1},
    {                    "para",            {182}, 1},
    {                    "part",         {0x2202}, 1},
    {                  "percnt",             {37}, 1},
    {                  "permil",         {0x2030}, 1},
    {                    "perp",         {0x22A5}, 1},
    {                     "phi",         {0x03C6}, 1},
    {                      "pi",         {0x03C0}, 1},
    {                     "piv",         {0x03D6}, 1},
    {                  "plusmn",            {177}, 1},
    {                   "pound",            {163}, 1},
    {                   "prime",         {0x2032}, 1},
    {                    "prod",         {0x220F}, 1},
    {                    "prop",         {0x221D}, 1},
    {                     "psi",         {0x03C8}, 1},
    {                    "quot",             {34}, 1},
    {                    "rarr",         {0x2192}, 1},
    {                   "rceil",         {0x2309}, 1},
    {                   "rdquo",         {0x201D}, 1},
    {                    "real",         {0x211C}, 1},
    {                     "reg",            {174}, 1},
    {                  "rfloor",         {0x230B}, 1},
    {                     "rho",         {0x03C1}, 1},
    {                     "rlm",         {0x200F}, 1},
    {                  "rsaquo",         {0x203A}, 1},
    {                   "rsquo",         {0x2019}, 1},
    {                   "sbquo",         {0x201A}, 1},
    {                  "scaron",         {0x0161}, 1},
    {                    "sdot",         {0x22C5}, 1},
    {                    "sect",            {167}, 1},
    {                     "shy",            {173}, 1},
    {                   "sigma",         {0x03C3}, 1},
    {                  "sigmaf",         {0x03C2}, 1},
    {                     "sim",         {0x223C}, 1},
    {                  "spades",         {0x2660}, 1},
    {                     "sub",         {0x2282}, 1},
    {                    "sube",         {0x2286}, 1},
    {                     "sum",         {0x2211}, 1},
    {                     "sup",         {0x2283}, 1},
    {                    "sup1",            {185}, 1},
    {                    "sup2",            {178}, 1},
    {                    "sup3",            {179}, 1},
    {                    "supe",         {0x2287}, 1},
    {                   "szlig",            {223}, 1},
    {                     "tau",         {0x03C4}, 1},
    {                  "there4",         {0x2234}, 1},
    {                   "theta",         {0x03B8}, 1},
    {                "thetasym",         {0x03D1}, 1},
    {                  "thinsp",         {0x2009}, 1},
    {                   "thorn",            {254}, 1},
    {                   "tilde",         {0x02DC}, 1},
    {                   "times",            {215}, 1},
    {                   "trade",         {0x2122}, 1},
    {                  "uacute",            {250}, 1},
    {                   "ucirc",            {251}, 1},
    {                  "ugrave",            {249}, 1},
    {                     "uml",            {168}, 1},
    {                   "upsih",         {0x03D2}, 1},
    {                 "upsilon",         {0x03C5}, 1},
    {                    "uuml",            {252}, 1},
    {                  "weierp",         {0x2118}, 1},
    {                      "xi",         {0x03BE}, 1},
    {                  "yacute",            {253}, 1},
    {                     "yen",            {165}, 1},
    {                    "yuml",            {255}, 1},
    {                    "zeta",         {0x03B6}, 1},
    {                     "zwj",         {0x200D}, 1},
    {                    "zwnj",         {0x200C}, 1},
};
#define N_ENTITIES (int)(sizeof(entities) / sizeof(entities[0]))

static int try_decode_entity(
    const char *text,
    size_t len,
    size_t *consumed,
    unsigned long *out_cps,
    int *out_n_cps
) {
    if (len == 0 || text[0] != '&') return 0;
    size_t semi = 1;
    while (semi < len && text[semi] != ';') semi++;
    if (semi >= len || semi > 31) return 0;
    size_t name_len = semi - 1;

    /* Numeric entities: &#NNN; or &#xHHH; */
    if (name_len >= 2 && text[1] == '#') {
        char *endptr = NULL;
        unsigned long cp;
        if (name_len >= 3 && (text[2] == 'x' || text[2] == 'X')) {
            cp = strtoul(text + 3, &endptr, 16);
            if (endptr && (size_t)(endptr - text) != semi) return 0;
        }
        else {
            cp = strtoul(text + 2, &endptr, 10);
            if (endptr && (size_t)(endptr - text) != semi) return 0;
        }
        if (endptr && (size_t)(endptr - text) == semi) {
            if (cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF)) return 0;
            if (cp == 0) cp = 0xFFFD; /* U+0000 -> U+FFFD */
            *consumed = semi + 1;
            out_cps[0] = cp;
            *out_n_cps = 1;
            return 1;
        }
        return 0;
    }

    /* Named entities via binary search */
    EntityEntry key;
    key.name = text + 1;
    /* We need a null-terminated name, so copy it */
    char name_buf[32];
    if (name_len >= sizeof(name_buf)) return 0;
    memcpy(name_buf, text + 1, name_len);
    name_buf[name_len] = '\0';
    key.name = name_buf;

    EntityEntry *found = (EntityEntry *)bsearch(
        &key, entities, N_ENTITIES, sizeof(EntityEntry), entity_cmp
    );
    if (found) {
        *consumed = semi + 1;
        for (int i = 0; i < found->n_cps; i++) out_cps[i] = found->cps[i];
        *out_n_cps = found->n_cps;
        return 1;
    }
    return 0;
}

static void escape_html(StringBuilder *sb, const char *text, size_t len) {
    for (size_t i = 0; i < len; i++) {
        switch (text[i]) {
            case '&': sb_append(sb, "&amp;"); break;
            case '<': sb_append(sb, "&lt;"); break;
            case '>': sb_append(sb, "&gt;"); break;
            case '"': sb_append(sb, "&quot;"); break;
            default: sb_append_char(sb, text[i]); break;
        }
    }
}

static size_t trim_trailing(const char *line, size_t len) {
    while (len > 0 && (line[len - 1] == ' ' || line[len - 1] == '\t')) len--;
    return len;
}

static int is_thematic_break(const char *line, size_t len) {
    size_t i = 0;
    /* At most 3 spaces of indent allowed, no tabs */
    while (i < len && i < 3 && line[i] == ' ') i++;
    if (i < len && line[i] == '\t') return 0;
    if (len - i < 3) return 0;
    char c = line[i];
    if (c != '-' && c != '*' && c != '_') return 0;
    char first_char = c;
    size_t count = 0;
    for (; i < len; i++) {
        if (line[i] == ' ' || line[i] == '\t') continue;
        if (line[i] != first_char) return 0;
        count++;
    }
    return count >= 3;
}

static int is_atx_heading(const char *line, size_t len) {
    size_t i = 0;
    /* At most 3 spaces of indent allowed */
    while (i < len && i < 3 && (line[i] == ' ' || line[i] == '\t')) i++;
    if (i >= len) return 0;
    if (line[i] == ' ' || line[i] == '\t') return 0;
    int level = 0;
    while (i < len && line[i] == '#') {
        level++;
        i++;
    }
    if (level == 0 || level > 6) return 0;
    if (i < len && line[i] != ' ' && line[i] != '\t') return 0;
    return level;
}

static size_t atx_content_end(const char *line, size_t len) {
    /* Trim trailing spaces */
    size_t end = len;
    while (end > 0 && line[end - 1] == ' ') end--;
    /* Trim closing # sequences - must be preceded by a space */
    /* First check if there's a space before the # sequence */
    size_t saved = end;
    while (end > 0 && line[end - 1] == '#') {
        if (end >= 2 && line[end - 2] == '\\') break; /* escaped # */
        end--;
    }
    /* If no space before the # sequence, restore (not a closing sequence) */
    if (end == 0 || (end > 0 && line[end - 1] != ' ')) end = saved;
    else {
        /* There was a space before #, trim it */
        end--; /* remove the space */
    }
    /* Trim spaces before closing # */
    while (end > 0 && line[end - 1] == ' ') end--;
    return end;
}

static int is_table_row(const char *line, size_t len) {
    for (size_t i = 0; i < len; i++)
        if (line[i] == '|') return 1;
    return 0;
}

static int is_table_separator(const char *line, size_t len) {
    size_t i = 0;
    while (i < len && (line[i] == ' ' || line[i] == '\t')) i++;
    if (i >= len) return 0;
    int has_pipe = 0;
    int has_dash = 0;
    for (; i < len; i++) {
        if (line[i] == ' ') continue;
        if (line[i] == '|') {
            has_pipe = 1;
            continue;
        }
        if (line[i] == ':') continue;
        if (line[i] == '\t') continue;
        if (line[i] == '-') {
            has_dash = 1;
            continue;
        }
        return 0;
    }
    return has_pipe && has_dash;
}

static int is_setext_underline(const char *line, size_t len) {
    if (len == 0) return 0;
    char c = line[0];
    if (c != '=' && c != '-') return 0;
    if (c == '-' && len < 2) return 0;
    for (size_t i = 1; i < len; i++)
        if (line[i] != c) return 0;
    return (c == '=') ? 1 : 2;
}

/* --- Reference definitions --- */
typedef struct {
    char *label;
    size_t label_len;
    char *url;
    char *title;
} RefDef;

static void ref_def_free(RefDef *rd) {
    if (rd->label) free(rd->label);
    if (rd->url) free(rd->url);
    if (rd->title) free(rd->title);
}

/* Normalize a reference label: collapse whitespace, lowercase, trim */
static size_t normalize_label(
    const char *src, size_t src_len, char *dst, size_t dst_cap
) {
    size_t j = 0;
    int last_was_space = 1;
    for (size_t i = 0; i < src_len && j < dst_cap - 1; i++) {
        unsigned char c = (unsigned char)src[i];
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
            if (!last_was_space && j > 0) {
                dst[j++] = ' ';
                last_was_space = 1;
            }
        }
        else {
            dst[j++] = (char)tolower(c);
            last_was_space = 0;
        }
    }
    /* Trim trailing space */
    while (j > 0 && dst[j - 1] == ' ') j--;
    dst[j] = '\0';
    return j;
}

static int find_ref(
    RefDef *refs, int n_refs, const char *label, size_t label_len, RefDef **out
) {
    char norm[256];
    size_t norm_len = normalize_label(label, label_len, norm, sizeof(norm));
    for (int i = 0; i < n_refs; i++) {
        if (refs[i].label_len == norm_len &&
            memcmp(refs[i].label, norm, norm_len) == 0) {
            *out = &refs[i];
            return 1;
        }
    }
    return 0;
}

/* Try to parse a reference definition line of the form [label]: url "title"
   Returns non-zero if successfully parsed, fills in RefDef.
   The RefDef string fields are heap-allocated. */
static int parse_ref_def(const char *line, size_t line_len, RefDef *rd) {
    memset(rd, 0, sizeof(*rd));
    size_t i = 0;
    /* Skip up to 3 spaces of indent */
    while (i < line_len && i < 3 && line[i] == ' ') i++;
    if (i >= line_len || line[i] != '[') return 0;
    i++; /* skip [ */
    size_t label_start = i;
    int bracket_depth = 1;
    while (i < line_len && bracket_depth > 0) {
        if (line[i] == '[') bracket_depth++;
        else if (line[i] == ']')
            bracket_depth--;
        if (bracket_depth > 0) i++;
    }
    if (bracket_depth != 0) return 0;
    size_t label_end = i;
    i++; /* skip ] */
    /* Must be followed by : */
    while (i < line_len && (line[i] == ' ' || line[i] == '\t')) i++;
    if (i >= line_len || line[i] != ':') return 0;
    i++; /* skip : */
    /* Parse destination */
    while (i < line_len && (line[i] == ' ' || line[i] == '\t')) i++;
    if (i >= line_len) return 0;
    size_t dest_start, dest_len;
    int has_angle = (line[i] == '<');
    if (has_angle) {
        i++;
        dest_start = i;
        while (i < line_len && line[i] != '>') {
            if (line[i] == '\n' || line[i] == '\r') return 0;
            i++;
        }
        if (i >= line_len) return 0;
        dest_len = i - dest_start;
        i++; /* skip > */
    }
    else {
        dest_start = i;
        while (i < line_len) {
            unsigned char c = (unsigned char)line[i];
            if (c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == 0x0C ||
                c == 0x0B)
                break;
            if (c == '\\' && i + 1 < line_len &&
                (line[i + 1] == '(' || line[i + 1] == ')' || line[i + 1] == '\\')) {
                i += 2;
                continue;
            }
            if (c == '(') return 0;
            if (c == ')') return 0;
            i++;
        }
        dest_len = i - dest_start;
    }
    /* Skip whitespace before optional title */
    while (i < line_len && (line[i] == ' ' || line[i] == '\t')) i++;
    size_t title_start = 0, title_len = 0;
    if (i < line_len) {
        if (line[i] == '"' || line[i] == '\'') {
            char delim = line[i];
            i++;
            title_start = i;
            while (i < line_len && line[i] != delim)
                if (line[i] == '\\' && i + 1 < line_len &&
                    (line[i + 1] == delim || line[i + 1] == '\\'))
                    i += 2;
                else
                    i++;
            if (i >= line_len) return 0;
            title_len = i - title_start;
            i++; /* skip closing delimiter */
        }
        else if (line[i] == '(') {
            i++;
            title_start = i;
            int pdepth = 1;
            while (i < line_len && pdepth > 0) {
                if (line[i] == '(') pdepth++;
                else if (line[i] == ')')
                    pdepth--;
                if (pdepth > 0) {
                    if (line[i] == '\\' && i + 1 < line_len &&
                        (line[i + 1] == '(' || line[i + 1] == ')' ||
                         line[i + 1] == '\\'))
                        i += 2;
                    else
                        i++;
                }
            }
            if (pdepth != 0) return 0;
            title_len = i - title_start;
            i++;
        }
        /* Check for trailing junk */
        while (i < line_len && (line[i] == ' ' || line[i] == '\t')) i++;
        if (i < line_len) return 0;
    }
    /* Allocate and fill */
    rd->label = (char *)malloc(label_end - label_start + 1);
    if (!rd->label) return 0;
    rd->label_len =
        normalize_label(line + label_start, label_end - label_start, rd->label, 256);
    rd->url = (char *)malloc(dest_len + 1);
    if (!rd->url) {
        free(rd->label);
        return 0;
    }
    memcpy(rd->url, line + dest_start, dest_len);
    rd->url[dest_len] = '\0';
    if (title_len > 0) {
        rd->title = (char *)malloc(title_len + 1);
        if (!rd->title) {
            free(rd->label);
            free(rd->url);
            return 0;
        }
        memcpy(rd->title, line + title_start, title_len);
        rd->title[title_len] = '\0';
    }
    return 1;
}

static size_t get_indent(const char *line, size_t len) {
    size_t i = 0;
    while (i < len && line[i] == ' ') i++;
    return i;
}

static size_t get_indent_tab(const char *line, size_t len) {
    size_t col = 0;
    for (size_t i = 0; i < len; i++)
        if (line[i] == ' ') col++;
        else if (line[i] == '\t')
            col = (col + 4) & ~3;
        else
            break;
    return col;
}

static const char *skip_spaces(const char *p, const char *end) {
    while (p < end && (*p == ' ' || *p == '\t')) p++;
    return p;
}

/* --- Autolink and Raw HTML inline support --- */

/* Percent-encode a character and append it (as %XX) to the StringBuilder.
   Returns 0 on success, -1 on allocation failure. */
static int append_uri_encoded(StringBuilder *sb, unsigned char c) {
    const char hex[] = "0123456789ABCDEF";
    /* Unreserved characters (RFC 3986) are kept as-is */
    if (isalnum(c) || c == '-' || c == '.' || c == '_' || c == '~' || c == ':' ||
        c == '/' || c == '?' || c == '#' || c == '@' || c == '!' || c == '$' ||
        c == '&' || c == '\'' || c == '(' || c == ')' || c == '*' || c == '+' ||
        c == ',' || c == ';' || c == '=' || c == '%') {
        if (sb_ensure(sb, 1) == -1) return -1;
        sb_append_char(sb, (char)c);
    }
    else {
        if (sb_ensure(sb, 3) == -1) return -1;
        sb_append_char(sb, '%');
        sb_append_char(sb, hex[c >> 4]);
        sb_append_char(sb, hex[c & 0xF]);
    }
    return 0;
}

static int is_valid_uri_autolink(const char *text, size_t len, size_t *consumed) {
    /* Must start with < */
    if (len == 0 || text[0] != '<') return 0;
    /* Find the closing > */
    const char *end = (const char *)memchr(text, '>', len);
    if (!end) return 0;
    size_t content_len = (size_t)(end - text - 1);
    const char *content = text + 1;
    if (content_len == 0) return 0;
    /* URI autolink must have a scheme: letter then letter/digit/+-. then : */
    size_t s = 0;
    if (!isalpha((unsigned char)content[s])) return 0;
    s++;
    while (s < content_len &&
           (isalnum((unsigned char)content[s]) || content[s] == '+' ||
            content[s] == '-' || content[s] == '.'))
        s++;
    if (s >= content_len || content[s] != ':') return 0;
    /* Scheme must be at least 2 characters (RFC 3986 requires letter then
       letter/digit, but CommonMark spec example #609 rejects single-char) */
    if (s < 2) return 0;
    /* Check for ASCII control char or space in remaining URI */
    for (size_t i = s + 1; i < content_len; i++) {
        unsigned char c = (unsigned char)content[i];
        if (c <= 0x20 || c == 0x7F) return 0;
    }
    *consumed = (size_t)(end - text + 1);
    return 1;
}

static int is_valid_email_autolink(const char *text, size_t len, size_t *consumed) {
    if (len == 0 || text[0] != '<') return 0;
    const char *end = (const char *)memchr(text, '>', len);
    if (!end) return 0;
    size_t content_len = (size_t)(end - text - 1);
    const char *content = text + 1;
    if (content_len == 0 || content_len > 254) return 0;
    /* Find first @ - must not be at start or end of tag content */
    const char *at = (const char *)memchr(content, '@', content_len);
    if (!at) return 0;
    size_t local_len = (size_t)(at - content);
    size_t domain_len = content_len - local_len - 1;
    if (local_len == 0 || domain_len == 0) return 0;
    /* Validate local part (alphanumeric and ., %, +, -, _ only,
       no consecutive dots, no dot at start/end) */
    if (content[0] == '.') return 0;
    for (size_t i = 0; i < local_len; i++) {
        unsigned char c = (unsigned char)content[i];
        if (!isalnum(c) && c != '.' && c != '%' && c != '+' && c != '-' && c != '_')
            return 0;
        if (c == '.' && (i + 1 >= local_len || content[i + 1] == '.')) return 0;
    }
    /* Validate domain (alphanumeric, hyphen, dot; labels; no consecutive dots,
       TLD must be alpha) */
    const char *domain = at + 1;
    int has_dot = 0;
    for (size_t i = 0; i < domain_len; i++) {
        unsigned char c = (unsigned char)domain[i];
        if (!isalnum(c) && c != '.' && c != '-') return 0;
        if (c == '.') {
            has_dot = 1;
            if (i == 0 || i + 1 >= domain_len || domain[i + 1] == '.') return 0;
        }
    }
    if (!has_dot) return 0;
    /* TLD must end with alphanumeric */
    if (!isalnum((unsigned char)domain[domain_len - 1])) return 0;
    *consumed = (size_t)(end - text + 1);
    return 1;
}

/* Check if text[0..len) starts with a valid HTML tag.
   Returns 1 if it is, sets *consumed to the length of the tag.
   Implements CommonMark spec 6.7. */
static int is_html_tag(const char *text, size_t len, size_t *consumed) {
    if (len == 0 || text[0] != '<') return 0;
    /* Comment: <!-- ... --> */
    if (len >= 4 && memcmp(text, "<!--", 4) == 0) {
        const char *close = strstr(text + 4, "-->");
        if (close) {
            *consumed = (size_t)(close - text + 3);
            return 1;
        }
        return 0;
    }
    /* Processing instruction: <? ... ?> */
    if (len >= 2 && memcmp(text, "<?", 2) == 0) {
        const char *close = strstr(text + 2, "?>");
        if (close) {
            *consumed = (size_t)(close - text + 2);
            return 1;
        }
        return 0;
    }
    /* Declaration: <!...> (uppercase tag after <!) */
    if (len >= 2 && memcmp(text, "<!", 2) == 0) {
        /* Skip CDATA: <![CDATA[ ... ]]> */
        if (len >= 9 && memcmp(text, "<![CDATA[", 9) == 0) {
            const char *close = strstr(text + 9, "]]>");
            if (close) {
                *consumed = (size_t)(close - text + 3);
                return 1;
            }
            return 0;
        }
        /* Must be uppercase A-Z after <! */
        if (len > 2 && text[2] >= 'A' && text[2] <= 'Z') {
            const char *close = (const char *)memchr(text + 2, '>', len - 2);
            if (close) {
                *consumed = (size_t)(close - text + 1);
                return 1;
            }
        }
        return 0;
    }
    /* Close tag: </tagname ... > */
    if (len >= 3 && text[1] == '/') {
        /* Tag name must start with letter */
        if (!isalpha((unsigned char)text[2])) return 0;
        size_t j = 3;
        while (j < len && (isalnum((unsigned char)text[j]))) j++;
        /* After tag name, must have whitespace, '>', or end of string */
        if (j < len && text[j] != ' ' && text[j] != '\t' && text[j] != '>') return 0;
        /* Skip whitespace and optional attributes */
        while (j < len && (text[j] == ' ' || text[j] == '\t')) j++;
        while (j < len && text[j] != '>') {
            /* Skip attribute value */
            if (text[j] == '"' || text[j] == '\'') {
                char q = text[j];
                j++;
                while (j < len && text[j] != q) {
                    if (text[j] == '\n' || text[j] == '\r') return 0;
                    j++;
                }
                if (j < len) j++;
            }
            else if (text[j] == '<' || text[j] == '>' || text[j] == '`') { return 0; }
            else if (text[j] == '\n' || text[j] == '\r') { return 0; }
            else { j++; }
        }
        if (j < len && text[j] == '>') {
            *consumed = j + 1;
            return 1;
        }
        return 0;
    }
    /* Open tag or self-closing tag: <tagname ...> or <tagname .../> */
    if (text[1] == '/') return 0;
    if (!isalpha((unsigned char)text[1])) return 0;
    size_t j = 2;
    while (j < len && (isalnum((unsigned char)text[j]))) j++;
    /* After tag name, must have whitespace, '/', '>', or end of string */
    if (j < len && text[j] != ' ' && text[j] != '\t' && text[j] != '/' &&
        text[j] != '>')
        return 0;
    /* Skip whitespace and attributes */
    while (j < len && (text[j] == ' ' || text[j] == '\t')) j++;
    while (j < len && text[j] != '>' && text[j] != '/') {
        if (text[j] == '"' || text[j] == '\'') {
            char q = text[j];
            j++;
            while (j < len && text[j] != q) {
                if (text[j] == '\n' || text[j] == '\r') return 0;
                j++;
            }
            if (j < len) j++;
        }
        else if (text[j] == '<' || text[j] == '>' || text[j] == '`') { return 0; }
        else if (text[j] == '\n' || text[j] == '\r') { return 0; }
        else { j++; }
        while (j < len && (text[j] == ' ' || text[j] == '\t')) j++;
    }
    if (j < len && text[j] == '/') {
        j++;
        while (j < len && (text[j] == ' ' || text[j] == '\t')) j++;
    }
    if (j < len && text[j] == '>') {
        *consumed = j + 1;
        return 1;
    }
    return 0;
}

/* --- HTML block support --- */

/* CommonMark spec 6.8: HTML block types */
#define HTML_BLOCK_TYPE_1 1 /* <pre, <script, <style, <textarea */
#define HTML_BLOCK_TYPE_2 2 /* <!-- comment */
#define HTML_BLOCK_TYPE_3 3 /* <? processing instruction */
#define HTML_BLOCK_TYPE_4 4 /* <! declaration */
#define HTML_BLOCK_TYPE_5 5 /* <![CDATA[ */
#define HTML_BLOCK_TYPE_6 6 /* start tag */
#define HTML_BLOCK_TYPE_7 7 /* close tag (single line only) */

/* HTML block type 6/7 tag names (case-insensitive) */
static int is_html_block_tag(const char *tag, size_t remaining) {
    static const char *block_tags[] = {
        "address",  "article",    "aside",    "base",     "basefont", "blockquote",
        "body",     "caption",    "center",   "col",      "colgroup", "dd",
        "details",  "dialog",     "dir",      "div",      "dl",       "dt",
        "fieldset", "figcaption", "figure",   "footer",   "form",     "frame",
        "frameset", "h1",         "h2",       "h3",       "h4",       "h5",
        "h6",       "head",       "header",   "hgroup",   "hr",       "html",
        "iframe",   "legend",     "li",       "link",     "main",     "menu",
        "menuitem", "nav",        "noframes", "noscript", "object",   "ol",
        "p",        "param",      "section",  "source",   "summary",  "table",
        "tbody",    "td",         "tfoot",    "th",       "thead",    "title",
        "tr",       "track",      "ul",       NULL
    };
    /* Extract the tag name after < or </, up to whitespace, /, or > */
    size_t name_start = 0;
    if (tag[0] == '<' && tag[1] == '/') name_start = 2;
    else if (tag[0] == '<')
        name_start = 1;
    else
        return 0;
    size_t name_end = name_start;
    while (name_end < remaining && isalnum((unsigned char)tag[name_end])) name_end++;
    if (name_end == name_start) return 0;
    /* Compare case-insensitively */
    for (int ti = 0; block_tags[ti] != NULL; ti++) {
        size_t bt_len = strlen(block_tags[ti]);
        if (name_end - name_start == bt_len) {
            int match = 1;
            for (size_t ci = 0; ci < bt_len; ci++) {
                if (tolower((unsigned char)tag[name_start + ci]) !=
                    block_tags[ti][ci]) {
                    match = 0;
                    break;
                }
            }
            if (match) return 1;
        }
    }
    return 0;
}

/* Check if a line starts an HTML block. Returns the block type or 0. */
static int is_html_block_start(const char *line, size_t len) {
    size_t i = 0;
    /* Up to 3 spaces indent allowed */
    while (i < len && i < 3 && line[i] == ' ') i++;
    if (i >= len) return 0;
    if (line[i] != '<') return 0;
    const char *tag = line + i;
    size_t remaining = len - i;

    /* Type 1: <pre, <script, <style, <textarea (case-insensitive) */
    if (remaining > 5) {
        const char *t = tag + 1;
        size_t r = remaining - 1;
        int is_type1 = 0;
        if ((r >= 3) && (t[0] == 'p' || t[0] == 'P') && (t[1] == 'r' || t[1] == 'R') &&
            (t[2] == 'e' || t[2] == 'E'))
            is_type1 = 1;
        else if ((r >= 6) && (t[0] == 's' || t[0] == 'S') &&
                 (t[1] == 'c' || t[1] == 'C') && (t[2] == 'r' || t[2] == 'R') &&
                 (t[3] == 'i' || t[3] == 'I') && (t[4] == 'p' || t[4] == 'P') &&
                 (t[5] == 't' || t[5] == 'T'))
            is_type1 = 1;
        else if ((r >= 5) && (t[0] == 's' || t[0] == 'S') &&
                 (t[1] == 't' || t[1] == 'T') && (t[2] == 'y' || t[2] == 'Y') &&
                 (t[3] == 'l' || t[3] == 'L') && (t[4] == 'e' || t[4] == 'E'))
            is_type1 = 1;
        else if ((r >= 8) && (t[0] == 't' || t[0] == 'T') &&
                 (t[1] == 'e' || t[1] == 'E') && (t[2] == 'x' || t[2] == 'X') &&
                 (t[3] == 't' || t[3] == 'T') && (t[4] == 'a' || t[4] == 'A') &&
                 (t[5] == 'r' || t[5] == 'R') && (t[6] == 'e' || t[6] == 'E') &&
                 (t[7] == 'a' || t[7] == 'A'))
            is_type1 = 1;
        if (is_type1) {
            /* Find end of tag name to verify it's actually the tag */
            size_t j = 1;
            if ((t[0] == 'p' || t[0] == 'P')) j = 4;
            else if ((t[0] == 's' || t[0] == 'S') && (t[1] == 'c' || t[1] == 'C'))
                j = 7;
            else if ((t[0] == 's' || t[0] == 'S') && (t[1] == 't' || t[1] == 'T'))
                j = 6;
            else if ((t[0] == 't' || t[0] == 'T'))
                j = 9;
            if (j < remaining && tag[j] != '>' && tag[j] != ' ' && tag[j] != '\t' &&
                tag[j] != '\n' && tag[j] != '\r')
                return 0;
            return HTML_BLOCK_TYPE_1;
        }
    }

    /* Type 2: <!-- comment */
    if (remaining >= 4 && memcmp(tag, "<!--", 4) == 0) return HTML_BLOCK_TYPE_2;
    /* Type 3: <? processing instruction */
    if (remaining >= 2 && memcmp(tag, "<?", 2) == 0) return HTML_BLOCK_TYPE_3;
    /* Type 4: <! followed by uppercase letter */
    if (remaining >= 3 && tag[1] == '!' && tag[2] >= 'A' && tag[2] <= 'Z')
        return HTML_BLOCK_TYPE_4;
    /* Type 5: <![CDATA[ */
    if (remaining >= 9 && memcmp(tag, "<![CDATA[", 9) == 0) return HTML_BLOCK_TYPE_5;
    /* Type 6: start tag (specific block tags only) */
    if (is_html_block_tag(tag, remaining)) {
        if (tag[1] == '/') return HTML_BLOCK_TYPE_7;
        return HTML_BLOCK_TYPE_6;
    }
    return 0;
}

static size_t get_indent_tab_from(const char *line, size_t len, size_t start_col) {
    size_t col = start_col;
    for (size_t i = 0; i < len; i++)
        if (line[i] == ' ') col++;
        else if (line[i] == '\t')
            col = (col + 4) & ~3;
        else
            break;
    return col - start_col;
}

/* Consume up to 4 columns of indent starting from start_col.
   Returns number of raw bytes consumed.
   *leftover_spaces receives number of spaces from a partially-consumed
   tab that must be prepended to the content. */
static size_t consume_indent(
    const char *s, size_t len, size_t start_col, size_t *leftover_spaces
) {
    size_t col = start_col;
    size_t i = 0;
    size_t target = start_col + 4;
    *leftover_spaces = 0;
    while (i < len && col < target) {
        if (s[i] == ' ') {
            col++;
            i++;
        }
        else if (s[i] == '\t') {
            size_t next = (col + 4) & ~3;
            if (next > target) {
                /* tab expansion crosses the 4-column boundary */
                *leftover_spaces = next - target;
                i++;
                col = target;
                break;
            }
            col = next;
            i++;
        }
        else
            break;
    }
    if (col > target) *leftover_spaces = col - target;
    return i;
}

static int is_fence_start(
    const char *line, size_t len, char *fence_char, int *fence_len
) {
    const char *p = line;
    const char *end = line + len;
    p = skip_spaces(p, end);
    if (p >= end) return 0;
    if (*p != '`' && *p != '~') return 0;
    char c = *p;
    *fence_char = c;
    int count = 0;
    while (p < end && *p == c) {
        count++;
        p++;
    }
    if (count < 3) return 0;
    *fence_len = count;

    /* For backtick fences: if info string contains backtick chars,
       this line is NOT a fenced code block (see CommonMark spec 4.5) */
    if (c == '`') {
        while (p < end && *p == ' ') p++;
        for (const char *scan = p; scan < end; scan++)
            if (*scan == '`') return 0;
    }
    return 1;
}

static int is_fence_end(const char *line, size_t len, char fence_char, int fence_len) {
    const char *p = line;
    const char *end = line + len;
    p = skip_spaces(p, end);
    int count = 0;
    while (p < end && *p == fence_char) {
        count++;
        p++;
    }
    if (count < fence_len) return 0;
    if (fence_char == '`') {
        while (p < end) {
            if (*p != ' ') return 0;
            p++;
        }
    }
    return 1;
}

static void append_codepoint_utf8(StringBuilder *sb, unsigned long cp) {
    if (cp <= 0x7F) sb_append_char(sb, (char)cp);
    else if (cp <= 0x7FF) {
        sb_append_char(sb, (char)(0xC0 | (cp >> 6)));
        sb_append_char(sb, (char)(0x80 | (cp & 0x3F)));
    }
    else if (cp <= 0xFFFF) {
        sb_append_char(sb, (char)(0xE0 | (cp >> 12)));
        sb_append_char(sb, (char)(0x80 | ((cp >> 6) & 0x3F)));
        sb_append_char(sb, (char)(0x80 | (cp & 0x3F)));
    }
    else {
        sb_append_char(sb, (char)(0xF0 | (cp >> 18)));
        sb_append_char(sb, (char)(0x80 | ((cp >> 12) & 0x3F)));
        sb_append_char(sb, (char)(0x80 | ((cp >> 6) & 0x3F)));
        sb_append_char(sb, (char)(0x80 | (cp & 0x3F)));
    }
}

static void decode_entity_and_append(
    StringBuilder *sb, const char *text, size_t len, size_t *consumed
) {
    unsigned long cps[4];
    int n_cps = 0;
    if (try_decode_entity(text, len, consumed, cps, &n_cps)) {
        for (int i = 0; i < n_cps; i++) {
            unsigned long cp = cps[i];
            if (cp == '&') sb_append(sb, "&amp;");
            else if (cp == '<')
                sb_append(sb, "&lt;");
            else if (cp == '>')
                sb_append(sb, "&gt;");
            else if (cp == '"')
                sb_append(sb, "&quot;");
            else
                append_codepoint_utf8(sb, cp);
        }
    }
}

/* Check if position pos is inside a matching code span in text[0..len) */
static int pos_inside_code_span(const char *text, size_t len, size_t pos) {
    size_t i = 0;
    while (i < len) {
        if (text[i] == '`') {
            int open_ticks = 0;
            size_t start = i;
            while (i < len && text[i] == '`') {
                open_ticks++;
                i++;
            }
            size_t search = i; /* content starts here */
            while (search < len) {
                if (text[search] == '`') {
                    size_t k = search;
                    int close_ticks = 0;
                    while (k < len && text[k] == '`') {
                        close_ticks++;
                        k++;
                    }
                    if (close_ticks == open_ticks) {
                        /* Code span covers [start, k) */
                        if (pos >= start && pos < k) return 1;
                        i = k;
                        goto next_code_span;
                    }
                    search = k;
                }
                else { search++; }
            }
            /* Unmatched opener from start..i */
            if (pos >= start) return 0; /* unmatched backtick at pos */
        }
        else { i++; }
        continue;
    next_code_span:;
    }
    return 0;
}

/* Append text with entity references decoded */
static void append_entity_decoded(StringBuilder *sb, const char *text, size_t len) {
    size_t i = 0;
    while (i < len) {
        if (text[i] == '&') {
            size_t consumed = 0;
            size_t prev = sb->len;
            decode_entity_and_append(sb, text + i, len - i, &consumed);
            if (consumed > 0) { i += consumed; }
            else {
                sb->len = prev;
                sb->data[sb->len] = '\0';
                sb_append(sb, "&amp;");
                i++;
            }
        }
        else {
            sb_append_char(sb, text[i]);
            i++;
        }
    }
}

static int codepoint_to_utf8_buf(char *buf, unsigned long cp) {
    if (cp <= 0x7F) {
        buf[0] = (char)cp;
        return 1;
    }
    else if (cp <= 0x7FF) {
        buf[0] = (char)(0xC0 | (cp >> 6));
        buf[1] = (char)(0x80 | (cp & 0x3F));
        return 2;
    }
    else if (cp <= 0xFFFF) {
        buf[0] = (char)(0xE0 | (cp >> 12));
        buf[1] = (char)(0x80 | ((cp >> 6) & 0x3F));
        buf[2] = (char)(0x80 | (cp & 0x3F));
        return 3;
    }
    else {
        buf[0] = (char)(0xF0 | (cp >> 18));
        buf[1] = (char)(0x80 | ((cp >> 12) & 0x3F));
        buf[2] = (char)(0x80 | ((cp >> 6) & 0x3F));
        buf[3] = (char)(0x80 | (cp & 0x3F));
        return 4;
    }
}

/* Append URL with entities decoded and non-ASCII bytes percent-encoded */
static void append_url_with_entities(StringBuilder *sb, const char *text, size_t len) {
    size_t i = 0;
    while (i < len) {
        if (text[i] == '\\' && i + 1 < len &&
            (text[i + 1] == '(' || text[i + 1] == ')' || text[i + 1] == '\\')) {
            unsigned char c = (unsigned char)text[i + 1];
            if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                (c >= '0' && c <= '9') || c == '-' || c == '.' || c == '_' ||
                c == '~' || c == '!' || c == '#' || c == '$' || c == '\'' || c == '(' ||
                c == ')' || c == '*' || c == '+' || c == ',' || c == ';' || c == '=' ||
                c == ':' || c == '/' || c == '?' || c == '@' || c == '[' || c == ']' ||
                c == '%')
                sb_append_char(sb, (char)c);
            else {
                static const char hex[] = "0123456789ABCDEF";
                sb_append_char(sb, '%');
                sb_append_char(sb, hex[c >> 4]);
                sb_append_char(sb, hex[c & 0xF]);
            }
            i += 2;
            continue;
        }
        if (text[i] == '&') {
            size_t consumed = 0;
            unsigned long cps[4];
            int n_cps = 0;
            if (try_decode_entity(text + i, len - i, &consumed, cps, &n_cps)) {
                for (int j = 0; j < n_cps; j++) {
                    char buf[8];
                    int nbytes = codepoint_to_utf8_buf(buf, cps[j]);
                    for (int k = 0; k < nbytes; k++) {
                        unsigned char c = (unsigned char)buf[k];
                        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                            (c >= '0' && c <= '9') || c == '-' || c == '.' ||
                            c == '_' || c == '~' || c == '!' || c == '#' || c == '$' ||
                            c == '\'' || c == '(' || c == ')' || c == '*' || c == '+' ||
                            c == ',' || c == ';' || c == '=' || c == ':' || c == '/' ||
                            c == '?' || c == '@' || c == '[' || c == ']' || c == '%')
                            sb_append_char(sb, (char)c);
                        else {
                            static const char hex[] = "0123456789ABCDEF";
                            sb_append_char(sb, '%');
                            sb_append_char(sb, hex[c >> 4]);
                            sb_append_char(sb, hex[c & 0xF]);
                        }
                    }
                }
                i += consumed;
                continue;
            }
            sb_append(sb, "%26");
            i++;
        }
        else {
            unsigned char c = (unsigned char)text[i];
            if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                (c >= '0' && c <= '9') || c == '-' || c == '.' || c == '_' ||
                c == '~' || c == '!' || c == '#' || c == '$' || c == '\'' || c == '(' ||
                c == ')' || c == '*' || c == '+' || c == ',' || c == ';' || c == '=' ||
                c == ':' || c == '/' || c == '?' || c == '@' || c == '[' || c == ']' ||
                c == '%')
                sb_append_char(sb, (char)c);
            else {
                static const char hex[] = "0123456789ABCDEF";
                sb_append_char(sb, '%');
                sb_append_char(sb, hex[c >> 4]);
                sb_append_char(sb, hex[c & 0xF]);
            }
            i++;
        }
    }
}

static int parse_link_dest_title(
    const char *text,
    size_t content_start,
    size_t content_end,
    size_t *dest_start,
    size_t *dest_len,
    size_t *title_start,
    size_t *title_len
) {
    size_t pos = content_start;
    while (pos < content_end && (text[pos] == ' ' || text[pos] == '\t')) pos++;
    *title_start = content_end;
    *title_len = 0;
    *dest_start = pos;
    *dest_len = 0;
    if (pos >= content_end) return 1;
    if (text[pos] == '<') {
        pos++;
        *dest_start = pos;
        while (pos < content_end && text[pos] != '>') {
            if (text[pos] == '\n' || text[pos] == '\r') return 0;
            if (text[pos] == '\\') {
                if (pos + 1 < content_end &&
                    (text[pos + 1] == '\\' || text[pos + 1] == '<' ||
                     text[pos + 1] == '>')) {
                    pos += 2;
                    continue;
                }
                return 0;
            }
            pos++;
        }
        if (pos >= content_end) return 0;
        *dest_len = pos - *dest_start;
        pos++;
    }
    else {
        *dest_start = pos;
        while (pos < content_end) {
            if (text[pos] == '\\' && pos + 1 < content_end &&
                (text[pos + 1] == '(' || text[pos + 1] == ')' ||
                 text[pos + 1] == '\\')) {
                pos += 2;
            }
            else if (text[pos] == '(') {
                size_t match = pos + 1;
                int pd = 1;
                while (match < content_end && pd > 0) {
                    if (text[match] == '(') pd++;
                    else if (text[match] == ')')
                        pd--;
                    if (pd > 0) match++;
                }
                if (pd == 0) pos = match + 1;
                else
                    break;
            }
            else if (text[pos] != ' ' && text[pos] != '\t' && text[pos] != '\n' &&
                     text[pos] != '\r' && text[pos] != '\f' && text[pos] != '\v' &&
                     text[pos] != '"' && text[pos] != '\'' && text[pos] != ')' &&
                     text[pos] != '\\') {
                pos++;
            }
            else { break; }
        }
        *dest_len = pos - *dest_start;
    }
    while (pos < content_end && (text[pos] == ' ' || text[pos] == '\t')) pos++;
    if (pos < content_end) {
        if (text[pos] == '"' || text[pos] == '\'') {
            char delim = text[pos];
            *title_start = pos + 1;
            pos = *title_start;
            while (pos < content_end) {
                if (text[pos] == delim) {
                    /* Check if escaped */
                    if (pos > *title_start && text[pos - 1] == '\\') {
                        /* Remove the backslash from title by overwriting it */
                        pos++;
                        continue;
                    }
                    break;
                }
                if (text[pos] == '\n' || text[pos] == '\r') return 0;
                pos++;
            }
            if (pos >= content_end) return 0;
            *title_len = pos - *title_start;
            pos++;
        }
        else if (text[pos] == '(') {
            *title_start = pos + 1;
            int pdepth = 1;
            pos++;
            while (pos < content_end && pdepth > 0) {
                if (text[pos] == '(') pdepth++;
                else if (text[pos] == ')')
                    pdepth--;
                if (pdepth > 0) pos++;
            }
            if (pdepth > 0) return 0;
            *title_len = pos - *title_start;
            pos++;
        }
        else { return 0; }
        while (pos < content_end && (text[pos] == ' ' || text[pos] == '\t')) pos++;
    }
    return pos == content_end;
}

static void render_inline(
    StringBuilder *sb,
    const char *text,
    size_t len,
    int allow_emphasis,
    RefDef *refs,
    int n_refs
);

static void render_inline_impl(
    StringBuilder *sb,
    const char *text,
    size_t len,
    int allow_emphasis,
    RefDef *refs,
    int n_refs
) {
    size_t i = 0;

    while (i < len) {
        if (text[i] == '\\' && i + 1 < len) {
            char next = text[i + 1];
            if (next == '\n') {
                sb_append(sb, "<br />\n");
                i += 2;
            }
            else if (strchr("!\"#$%&'()*+,-./:;<=>?@[\\]^_`{|}~", next)) {
                switch (next) {
                    case '&': sb_append(sb, "&amp;"); break;
                    case '<': sb_append(sb, "&lt;"); break;
                    case '>': sb_append(sb, "&gt;"); break;
                    case '"': sb_append(sb, "&quot;"); break;
                    default: sb_append_char(sb, next); break;
                }
                i += 2;
            }
            else {
                sb_append_char(sb, text[i]);
                i++;
            }
            continue;
        }

        if (text[i] == '`') {
            size_t start = i;
            i++;
            int opener_ticks = 1;
            while (i < len && text[i] == '`') {
                opener_ticks++;
                i++;
            }
            size_t content_start = i;
            int found = 0;
            size_t j = i;
            while (j < len) {
                if (text[j] == '`') {
                    size_t k = j;
                    int closer_ticks = 0;
                    while (k < len && text[k] == '`') {
                        closer_ticks++;
                        k++;
                    }
                    if (closer_ticks == opener_ticks) {
                        size_t content_end = j;
                        /* Normalize content: replace newlines with spaces */
                        size_t cs = content_start;
                        size_t ce = content_end;
                        /* Build normalized content string */
                        char norm[1024];
                        size_t norm_len = 0;
                        size_t max_norm = sizeof(norm);
                        if (ce - cs > max_norm) max_norm = 0;
                        int all_spaces = 1;
                        for (size_t m = cs; m < ce && norm_len < max_norm; m++) {
                            if (text[m] == '\r') {
                                if (m + 1 < ce && text[m + 1] == '\n') m++;
                                norm[norm_len++] = ' ';
                            }
                            else if (text[m] == '\n') { norm[norm_len++] = ' '; }
                            else {
                                norm[norm_len++] = text[m];
                                if (text[m] != ' ') all_spaces = 0;
                            }
                        }
                        /* Strip one space from each end if applicable */
                        size_t strip_start = 0;
                        size_t strip_end = norm_len;
                        if (norm_len > 1 && norm[0] == ' ' &&
                            norm[norm_len - 1] == ' ' && !all_spaces) {
                            strip_start = 1;
                            strip_end = norm_len - 1;
                        }
                        sb_append(sb, "<code>");
                        for (size_t m = strip_start; m < strip_end; m++) {
                            switch (norm[m]) {
                                case '&': sb_append(sb, "&amp;"); break;
                                case '<': sb_append(sb, "&lt;"); break;
                                case '>': sb_append(sb, "&gt;"); break;
                                case '"': sb_append(sb, "&quot;"); break;
                                default: sb_append_char(sb, norm[m]); break;
                            }
                        }
                        sb_append(sb, "</code>");
                        i = k;
                        found = 1;
                        break;
                    }
                    j = k;
                }
                else { j++; }
            }
            if (!found)
                for (size_t m = start; m < i; m++) sb_append_char(sb, text[m]);
            continue;
        }

        if (text[i] == '\r') {
            sb_append(sb, "<br />\n");
            i++;
            if (i < len && text[i] == '\n') i++;
            continue;
        }

        if (text[i] == '\n') {
            /* Hard line break if preceded by two spaces */
            if (i >= 2 && text[i - 1] == ' ' && text[i - 2] == ' ') {
                /* Remove all trailing spaces that were already output */
                while (sb->len > 0 && sb->data[sb->len - 1] == ' ' && sb->len > 0)
                    sb->len--;
                sb->data[sb->len] = '\0';
                sb_append(sb, "<br />\n");
            }
            else { sb_append(sb, "\n"); }
            i++;
            continue;
        }

        if (text[i] == '!' && i + 1 < len && text[i + 1] == '[') {
            size_t link_start = i + 2;
            size_t j = link_start;
            int depth = 1;
            size_t alt_end = 0;
            while (j < len && depth > 0) {
                if (text[j] == '[') depth++;
                else if (text[j] == ']') {
                    depth--;
                    if (depth == 0) alt_end = j;
                }
                j++;
            }
            /* If ] is inside a code span, this is not an image */
            if (alt_end > 0 && pos_inside_code_span(text, len, alt_end)) alt_end = 0;
            if (alt_end > 0 && alt_end + 1 < len && text[alt_end + 1] == '(') {
                size_t paren_start = alt_end + 2;
                size_t k = paren_start;
                int pdepth = 1;
                size_t url_end = 0;
                size_t cs = paren_start;
                while (cs < len && (text[cs] == ' ' || text[cs] == '\t')) cs++;
                int has_angle = (cs < len && text[cs] == '<');
                while (k < len && pdepth > 0) {
                    if (has_angle && text[k] == '<') {
                        k++;
                        while (k < len && text[k] != '>') k++;
                        if (k < len) k++;
                        continue;
                    }
                    if (text[k] == '\\' && k + 1 < len &&
                        (text[k + 1] == '(' || text[k + 1] == ')' ||
                         text[k + 1] == '\\')) {
                        k += 2;
                        continue;
                    }
                    if (text[k] == '(') pdepth++;
                    else if (text[k] == ')') {
                        pdepth--;
                        if (pdepth == 0) url_end = k;
                    }
                    k++;
                }
                if (url_end > 0) {
                    size_t dest_start, dest_len, title_s, title_len;
                    if (parse_link_dest_title(
                            text, paren_start, url_end, &dest_start, &dest_len,
                            &title_s, &title_len
                        )) {
                        sb_append(sb, "<img src=\"");
                        append_url_with_entities(sb, text + dest_start, dest_len);
                        sb_append(sb, "\" alt=\"");
                        escape_html(sb, text + link_start, alt_end - link_start);
                        sb_append(sb, "\"");
                        if (title_len > 0) {
                            sb_append(sb, " title=\"");
                            append_entity_decoded(sb, text + title_s, title_len);
                            sb_append_char(sb, '"');
                        }
                        sb_append(sb, " />");
                        i = url_end + 1;
                        continue;
                    }
                }
            }
            /* Check for reference-style image: ![label] or ![label][] */
            if (alt_end > 0 && refs && n_refs > 0) {
                size_t ref_end = 0;
                int is_collapsed = 0;
                if (alt_end + 1 < len && text[alt_end + 1] == '[') {
                    /* ![label][ref] */
                    size_t r = alt_end + 2;
                    int ref_depth = 1;
                    while (r < len && ref_depth > 0) {
                        if (text[r] == '[') ref_depth++;
                        else if (text[r] == ']') {
                            ref_depth--;
                            if (ref_depth == 0) ref_end = r;
                        }
                        r++;
                    }
                    if (ref_end == 0) ref_end = len;
                }
                else if (alt_end + 1 < len && text[alt_end + 1] == ']') {
                    /* ![label][] */
                    ref_end = alt_end + 1;
                    is_collapsed = 1;
                }
                else if (alt_end + 1 < len &&
                         (text[alt_end + 1] == ' ' || text[alt_end + 1] == '\t' ||
                          text[alt_end + 1] == '\n' || text[alt_end + 1] == '\r')) {
                    /* Check for ![label] on its own line */
                    size_t r = alt_end + 1;
                    while (r < len && (text[r] == ' ' || text[r] == '\t' ||
                                       text[r] == '\n' || text[r] == '\r'))
                        r++;
                    if (r < len && text[r] == '[') {
                        r++;
                        int ref_depth = 1;
                        while (r < len && ref_depth > 0) {
                            if (text[r] == '[') ref_depth++;
                            else if (text[r] == ']') {
                                ref_depth--;
                                if (ref_depth == 0) ref_end = r;
                            }
                            r++;
                        }
                        if (ref_end > 0) ref_end = r;
                    }
                }
                if (ref_end > alt_end + 1) {
                    /* Explicit reference or collapsed */
                    RefDef *found = NULL;
                    size_t search_start, search_len;
                    if (is_collapsed) {
                        search_start = link_start;
                        search_len = alt_end - link_start;
                    }
                    else {
                        search_start = alt_end + 2;
                        search_len = ref_end - (alt_end + 2);
                    }
                    if (find_ref(
                            refs, n_refs, text + search_start, search_len, &found
                        )) {
                        sb_append(sb, "<img src=\"");
                        append_url_with_entities(sb, found->url, strlen(found->url));
                        sb_append(sb, "\" alt=\"");
                        escape_html(sb, text + link_start, alt_end - link_start);
                        sb_append(sb, "\"");
                        if (found->title) {
                            sb_append(sb, " title=\"");
                            append_entity_decoded(
                                sb, found->title, strlen(found->title)
                            );
                            sb_append_char(sb, '"');
                        }
                        sb_append(sb, " />");
                        i = (ref_end > alt_end + 1 ? ref_end + 1 : alt_end + 2);
                        continue;
                    }
                }
            }
        }

        if (text[i] == '[') {
            size_t link_start = i + 1;
            size_t j = link_start;
            int depth = 1;
            size_t text_end = 0;
            while (j < len && depth > 0) {
                if (text[j] == '[') depth++;
                else if (text[j] == ']') {
                    depth--;
                    if (depth == 0) text_end = j;
                }
                j++;
            }
            /* If ] is inside a code span, this is not a link */
            if (text_end > 0 && pos_inside_code_span(text, len, text_end)) text_end = 0;
            if (text_end > 0 && text_end + 1 < len && text[text_end + 1] == '(') {
                size_t paren_start = text_end + 2;
                size_t k = paren_start;
                int pdepth = 1;
                size_t url_end = 0;
                size_t cs = paren_start;
                while (cs < len && (text[cs] == ' ' || text[cs] == '\t')) cs++;
                int has_angle = (cs < len && text[cs] == '<');
                while (k < len && pdepth > 0) {
                    if (has_angle && text[k] == '<') {
                        k++;
                        while (k < len && text[k] != '>') k++;
                        if (k < len) k++;
                        continue;
                    }
                    if (text[k] == '\\' && k + 1 < len &&
                        (text[k + 1] == '(' || text[k + 1] == ')' ||
                         text[k + 1] == '\\')) {
                        k += 2;
                        continue;
                    }
                    if (text[k] == '(') pdepth++;
                    else if (text[k] == ')') {
                        pdepth--;
                        if (pdepth == 0) url_end = k;
                    }
                    k++;
                }
                if (url_end > 0) {
                    size_t dest_start, dest_len, title_s, title_len;
                    if (parse_link_dest_title(
                            text, paren_start, url_end, &dest_start, &dest_len,
                            &title_s, &title_len
                        )) {
                        sb_append(sb, "<a href=\"");
                        append_url_with_entities(sb, text + dest_start, dest_len);
                        sb_append(sb, "\"");
                        if (title_len > 0) {
                            sb_append(sb, " title=\"");
                            append_entity_decoded(sb, text + title_s, title_len);
                            sb_append_char(sb, '"');
                        }
                        sb_append(sb, ">");
                        render_inline(
                            sb, text + link_start, text_end - link_start, 1, refs,
                            n_refs
                        );
                        sb_append(sb, "</a>");
                        i = url_end + 1;
                        continue;
                    }
                }
            }
            /* Check for reference-style link: [text][ref], [text][], or [text] */
            if (refs && n_refs > 0) {
                size_t ref_end = 0;
                int is_collapsed = 0;
                if (text_end + 1 < len && text[text_end + 1] == '[') {
                    /* [text][ref] */
                    size_t r = text_end + 2;
                    int ref_depth = 1;
                    while (r < len && ref_depth > 0) {
                        if (text[r] == '[') ref_depth++;
                        else if (text[r] == ']') {
                            ref_depth--;
                            if (ref_depth == 0) ref_end = r;
                        }
                        r++;
                    }
                    if (ref_end == 0) ref_end = len;
                }
                else if (text_end + 1 < len && text[text_end + 1] == ']') {
                    /* [text][] */
                    ref_end = text_end + 1;
                    is_collapsed = 1;
                }
                size_t search_start, search_len;
                if (ref_end > text_end) {
                    if (is_collapsed) {
                        search_start = link_start;
                        search_len = text_end - link_start;
                    }
                    else {
                        search_start = text_end + 2;
                        search_len = ref_end - (text_end + 2);
                    }
                    RefDef *found = NULL;
                    if (find_ref(
                            refs, n_refs, text + search_start, search_len, &found
                        )) {
                        sb_append(sb, "<a href=\"");
                        append_url_with_entities(sb, found->url, strlen(found->url));
                        sb_append(sb, "\"");
                        if (found->title) {
                            sb_append(sb, " title=\"");
                            append_entity_decoded(
                                sb, found->title, strlen(found->title)
                            );
                            sb_append_char(sb, '"');
                        }
                        sb_append(sb, ">");
                        render_inline(
                            sb, text + link_start, text_end - link_start, 1, refs,
                            n_refs
                        );
                        sb_append(sb, "</a>");
                        i = (ref_end > text_end + 1 ? ref_end + 1 : text_end + 2);
                        continue;
                    }
                }
            }
        }

        if (allow_emphasis && text[i] == '*' && i + 1 < len && text[i + 1] == '*') {
            if (i == 0 || text[i - 1] != '\\') {
                size_t j = i + 2;
                int found = 0;
                while (j < len - 1) {
                    if (text[j] == '*' && text[j + 1] == '*') {
                        if (j > 0 && text[j - 1] == '\\') {
                            j += 2;
                            continue;
                        }
                        if (pos_inside_code_span(text, len, j)) {
                            j += 2;
                            continue;
                        }
                        sb_append(sb, "<strong>");
                        render_inline(sb, text + i + 2, j - i - 2, 1, refs, n_refs);
                        sb_append(sb, "</strong>");
                        i = j + 2;
                        found = 1;
                        break;
                    }
                    j++;
                }
                if (found) continue;
            }
            sb_append_char(sb, text[i]);
            i++;
            continue;
        }

        if (allow_emphasis && text[i] == '_' && i + 1 < len && text[i + 1] == '_') {
            if (i == 0 || text[i - 1] != '\\') {
                size_t j = i + 2;
                int found = 0;
                while (j < len - 1) {
                    if (text[j] == '_' && text[j + 1] == '_') {
                        if (j > 0 && text[j - 1] == '\\') {
                            j += 2;
                            continue;
                        }
                        if (pos_inside_code_span(text, len, j)) {
                            j += 2;
                            continue;
                        }
                        sb_append(sb, "<strong>");
                        render_inline(sb, text + i + 2, j - i - 2, 1, refs, n_refs);
                        sb_append(sb, "</strong>");
                        i = j + 2;
                        found = 1;
                        break;
                    }
                    j++;
                }
                if (found) continue;
            }
            sb_append_char(sb, text[i]);
            i++;
            continue;
        }

        if (allow_emphasis && (text[i] == '*' || text[i] == '_')) {
            if (i == 0 || text[i - 1] != '\\') {
                char delim = text[i];
                size_t j = i + 1;
                int found = 0;
                while (j < len) {
                    if (text[j] == delim) {
                        if (j + 1 < len && text[j + 1] == delim) {
                            j += 2;
                            continue;
                        }
                        if (j > 0 && text[j - 1] == '\\') {
                            j++;
                            continue;
                        }
                        if (j > i + 1) {
                            if (pos_inside_code_span(text, len, j)) {
                                j++;
                                continue;
                            }
                            sb_append(sb, "<em>");
                            render_inline(sb, text + i + 1, j - i - 1, 1, refs, n_refs);
                            sb_append(sb, "</em>");
                            i = j + 1;
                            found = 1;
                            break;
                        }
                        j++;
                    }
                    j++;
                }
                if (found) continue;
            }
            sb_append_char(sb, text[i]);
            i++;
            continue;
        }

        if (text[i] == '&') {
            /* Try to decode entity reference */
            size_t consumed = 0;
            size_t prev_len = sb->len;
            decode_entity_and_append(sb, text + i, len - i, &consumed);
            if (consumed > 0) { i += consumed; }
            else {
                /* Not a recognized entity: escape the & */
                sb->len = prev_len;
                sb->data[sb->len] = '\0';
                sb_append(sb, "&amp;");
                i++;
            }
            continue;
        }
        if (text[i] == '<') {
            size_t tag_consumed = 0;
            /* Try URI autolink */
            if (is_valid_uri_autolink(text + i, len - i, &tag_consumed)) {
                const char *uri = text + i + 1;
                size_t uri_len = tag_consumed - 2;
                /* Percent-encode URI for href, HTML-escape & in text */
                sb_append(sb, "<a href=\"");
                for (size_t ui = 0; ui < uri_len; ui++) {
                    unsigned char uc = (unsigned char)uri[ui];
                    if (uc == '&') sb_append(sb, "&amp;");
                    else
                        append_uri_encoded(sb, uc);
                }
                sb_append(sb, "\">");
                for (size_t ui = 0; ui < uri_len; ui++)
                    if (uri[ui] == '&') sb_append(sb, "&amp;");
                    else
                        sb_append_char(sb, uri[ui]);
                sb_append(sb, "</a>");
                i += tag_consumed;
                continue;
            }
            /* Try email autolink */
            if (is_valid_email_autolink(text + i, len - i, &tag_consumed)) {
                const char *email = text + i + 1;
                size_t email_len = tag_consumed - 2;
                sb_append(sb, "<a href=\"mailto:");
                for (size_t ei = 0; ei < email_len; ei++) {
                    unsigned char c = (unsigned char)email[ei];
                    if (c == '&') sb_append(sb, "&amp;");
                    else
                        sb_append_char(sb, email[ei]);
                }
                sb_append(sb, "\">");
                for (size_t ei = 0; ei < email_len; ei++) {
                    unsigned char c = (unsigned char)email[ei];
                    if (c == '&') sb_append(sb, "&amp;");
                    else
                        sb_append_char(sb, email[ei]);
                }
                sb_append(sb, "</a>");
                i += tag_consumed;
                continue;
            }
            /* Try raw HTML tag */
            if (is_html_tag(text + i, len - i, &tag_consumed)) {
                sb_append_n(sb, text + i, tag_consumed);
                i += tag_consumed;
                continue;
            }
            /* Not an autolink or HTML tag, escape */
            sb_append(sb, "&lt;");
            i++;
            continue;
        }
        switch (text[i]) {
            case '>': sb_append(sb, "&gt;"); break;
            case '"': sb_append(sb, "&quot;"); break;
            default: sb_append_char(sb, text[i]); break;
        }
        i++;
    }
}

static void render_inline(
    StringBuilder *sb,
    const char *text,
    size_t len,
    int allow_emphasis,
    RefDef *refs,
    int n_refs
) {
    /* Trim trailing whitespace from the text */
    while (len > 0 && (text[len - 1] == ' ' || text[len - 1] == '\t')) len--;
    render_inline_impl(sb, text, len, allow_emphasis, refs, n_refs);
}

/* --- List stack helpers --- */

/* Push a new list level onto the stack */
static void list_push(ListEntry *stack, int *depth, int type, char marker,
                       char delimiter, int marker_col, int content_col,
                       int start_num) {
    int d = *depth;
    if (d >= MAX_LIST_DEPTH) return;
    stack[d].type = type;
    stack[d].marker = marker;
    stack[d].delimiter = delimiter;
    stack[d].content_col = content_col;
    stack[d].start_num = start_num;
    stack[d].is_loose = 0;
    stack[d].after_blank = 0;
    stack[d].item_open = 0;
    stack[d].marker_col = marker_col;
    stack[d].saved_data = NULL;
    stack[d].saved_len = 0;
    stack[d].saved_cap = 0;
    stack[d].first_item_content_pos = -1;
    stack[d].first_item_content_len = 0;
    *depth = d + 1;
}

/* Save parent's li_content when pushing a sub-list */
static void list_save_content(StringBuilder *li_content, ListEntry *parent) {
    parent->saved_data = li_content->data;
    parent->saved_len = li_content->len;
    parent->saved_cap = li_content->cap;
    li_content->data = (char *)malloc(128);
    if (li_content->data) {
        li_content->data[0] = '\0';
        li_content->len = 0;
        li_content->cap = 128;
    }
}

static ListEntry *list_top(ListEntry *stack, int depth) {
    if (depth <= 0) return NULL;
    return &stack[depth - 1];
}

/* Start a new list item: emit <li> tag */
static void list_open_item(StringBuilder *sb, ListEntry *e) {
    sb_append(sb, "<li>");
    e->item_open = 1;
}

/* Flush item text content to sb (without wrapping) */
static void list_flush_content(StringBuilder *sb, StringBuilder *li_content,
                                ListEntry *e) {
    if (li_content->len > 0) {
        if (e->first_item_content_pos < 0) {
            e->first_item_content_pos = (int)sb->len;
            e->first_item_content_len = (int)li_content->len;
        }
        sb_append_n(sb, li_content->data, li_content->len);
        li_content->len = 0;
    }
}

/* Close current item: flush content (wrapping if loose) and emit </li> */
static void list_close_item(StringBuilder *sb, StringBuilder *li_content,
                             ListEntry *e) {
    if (e->item_open) {
        if (e->is_loose && e->first_item_content_pos >= 0 &&
            e->first_item_content_len > 0) {
            sb_insert(sb, (size_t)e->first_item_content_pos, "\n<p>", 4);
            sb_insert(sb, (size_t)(e->first_item_content_pos + 4 + e->first_item_content_len), "</p>\n", 5);
            e->first_item_content_pos = -1;
            e->first_item_content_len = 0;
        }
        if (e->is_loose && li_content->len > 0) {
            sb_append(sb, "\n<p>");
            sb_append_n(sb, li_content->data, li_content->len);
            sb_append(sb, "</p>\n");
        }
        else if (li_content->len > 0) {
            if (e->first_item_content_pos < 0) {
                e->first_item_content_pos = (int)sb->len;
                e->first_item_content_len = (int)li_content->len;
            }
            sb_append_n(sb, li_content->data, li_content->len);
        }
        sb_append(sb, "</li>\n");
        li_content->len = 0;
        e->item_open = 0;
    }
}

/* Pop the top list: close its item, emit </ul>/</ol> */
static void list_pop(StringBuilder *sb, StringBuilder *li_content,
                      ListEntry *stack, int *depth) {
    if (*depth <= 0) return;
    ListEntry *e = &stack[*depth - 1];
    int had_blank = e->after_blank;
    list_close_item(sb, li_content, e);
    if (e->type == 1) sb_append(sb, "</ul>\n");
    else
        sb_append(sb, "</ol>\n");
    (*depth)--;
    if (had_blank && *depth > 0) {
        stack[*depth - 1].after_blank = 1;
    }
}

/* Emit opening tag for a list level */
static void list_emit_open(StringBuilder *sb, ListEntry *e) {
    if (e->type == 1) sb_append(sb, "<ul>\n");
    else {
        if (e->start_num != 1) {
            char buf[32];
            int n = snprintf(buf, sizeof(buf), "<ol start=\"%d\">\n", e->start_num);
            if (n > 0) sb_append_n(sb, buf, (size_t)n);
        }
        else { sb_append(sb, "<ol>\n"); }
    }
}

/* Close all list levels down to (but not including) the given depth */
static void list_pop_to(StringBuilder *sb, StringBuilder *li_content,
                         ListEntry *stack, int *depth, int target) {
    while (*depth > target) {
        list_pop(sb, li_content, stack, depth);
    }
}

/* Check if a list with given type/marker/delimiter matches the top list */
static int list_matches(ListEntry *e, int type, char marker, char delimiter) {
    if (e->type != type) return 0;
    if (type == 1) return e->marker == marker;
    return e->delimiter == delimiter;
}

/* Process list item content, detecting and handling same-line nested list markers.
 * Walks the content and for each leading list marker found, creates a sub-list
 * (pushing entries onto the stack). Any remaining non-marker content is rendered
 * as inline text into the current (deepest) item's li_content.
 */
static void process_list_content(StringBuilder *sb, StringBuilder *li_content,
                                  ListEntry *stack, int *depth,
                                  const char *content, size_t content_len,
                                  int base_abs_col,
                                  RefDef *refs, int n_refs) {
    while (content_len > 0) {
        size_t ci = 0;
        while (ci < content_len &&
               (content[ci] == ' ' || content[ci] == '\t'))
            ci++;
        if (ci >= content_len) break;

        int found = 0;
        int sub_type = 0;
        char sub_marker = 0;
        char sub_delim = 0;
        int sub_start = 0;
        size_t sub_marker_end = 0;

        /* Check UL marker (-, *, +) */
        if (!found && (content[ci] == '-' || content[ci] == '*' || content[ci] == '+')) {
            size_t after = ci + 1;
            if (after >= content_len || content[after] == ' ' || content[after] == '\t') {
                found = 1;
                sub_type = 1;
                sub_marker = content[ci];
                sub_marker_end = after;
            }
        }

        /* Check OL marker (digits followed by . or )) */
        if (!found && isdigit((unsigned char)content[ci])) {
            size_t d = ci;
            int num = 0;
            int digits = 0;
            while (d < content_len && isdigit((unsigned char)content[d]) && digits < 10) {
                num = num * 10 + (content[d] - '0');
                d++; digits++;
            }
            if (digits > 0 && digits <= 9 && d < content_len &&
                (content[d] == '.' || content[d] == ')')) {
                size_t after = d + 1;
                if (after >= content_len || content[after] == ' ' || content[after] == '\t') {
                    found = 1;
                    sub_type = 2;
                    sub_delim = content[d];
                    sub_start = num;
                    sub_marker_end = after;
                }
            }
        }

        if (!found) break;

        /* Found a nested list marker — create sub-list */
        ListEntry *parent = list_top(stack, *depth);
        if (parent && parent->item_open) {
            list_flush_content(sb, li_content, parent);
            if (sb->len == 0 || sb->data[sb->len - 1] != '\n')
                sb_append_char(sb, '\n');
        }
        list_save_content(li_content, parent);

        int sub_mc = base_abs_col + (int)ci;
        int sub_cc;
        if (sub_marker_end >= content_len) {
            sub_cc = sub_mc + 1;
        } else {
            size_t sp = sub_marker_end;
            while (sp < content_len &&
                   (content[sp] == ' ' || content[sp] == '\t'))
                sp++;
            sub_cc = base_abs_col + (int)sp;
        }

        list_push(stack, depth, sub_type, sub_marker, sub_delim,
                  sub_mc, sub_cc, sub_start);
        list_emit_open(sb, &stack[*depth - 1]);
        list_open_item(sb, &stack[*depth - 1]);

        /* Advance content past marker and whitespace */
        size_t skip = sub_marker_end;
        while (skip < content_len &&
               (content[skip] == ' ' || content[skip] == '\t'))
            skip++;
        content += skip;
        content_len -= skip;
        base_abs_col += (int)skip;
    }

    if (content_len > 0) {
        /* Check for ATX heading (#) on the same line as the list marker */
        if (content[0] == '#') {
            size_t hp = 0;
            int hlevel = 0;
            while (hp < content_len && content[hp] == '#') {
                hlevel++;
                hp++;
            }
            if (hlevel >= 1 && hlevel <= 6 &&
                (hp >= content_len || content[hp] == ' ' || content[hp] == '\t')) {
                ListEntry *cur = list_top(stack, *depth);
                if (cur && cur->item_open) {
                    list_flush_content(sb, li_content, cur);
                    sb_append_char(sb, '\n');
                }
                while (hp < content_len &&
                       (content[hp] == ' ' || content[hp] == '\t'))
                    hp++;
                size_t h_end = content_len;
                while (h_end > hp && content[h_end - 1] == ' ') h_end--;
                size_t saved = h_end;
                while (h_end > hp && content[h_end - 1] == '#') h_end--;
                if (h_end <= hp || (h_end > hp && content[h_end - 1] != ' '))
                    h_end = saved;
                else if (h_end > hp)
                    h_end--;
                sb_append_char(sb, '<');
                sb_append_char(sb, 'h');
                sb_append_char(sb, (char)('0' + hlevel));
                sb_append_char(sb, '>');
                if (h_end > hp)
                    render_inline(sb, content + hp, h_end - hp, 1, refs, n_refs);
                sb_append(sb, "</h");
                sb_append_char(sb, (char)('0' + hlevel));
                sb_append(sb, ">\n");
                return;
            }
        }
        render_inline(li_content, content, content_len, 1, refs, n_refs);
    }
}

static char *markdown_to_html(const char *markdown, size_t md_len) {
    StringBuilder *sb = sb_new();
    StringBuilder *para_buf = sb_new();
    if (!sb || !para_buf) {
        sb_free(sb);
        sb_free(para_buf);
        return NULL;
    }

    const char *p = markdown;
    const char *end = markdown + md_len;

    int in_fence = 0;
    char fence_char = '`';
    int fence_len = 0;
    int in_blockquote = 0;
    int bq_has_content = 0;
    int in_indented_code = 0;
    int in_html_block = 0;
    int html_block_type = 0;
    int para_has_content = 0;

    /* List stack for nested lists */
    ListEntry list_stack[MAX_LIST_DEPTH];
    int list_depth = 0;
    StringBuilder *li_content = sb_new();

    /* Collect reference definitions */
    RefDef *refs = NULL;
    int n_refs = 0, cap_refs = 0;

    while (p < end) {
        const char *line_end = p;
        while (line_end < end && *line_end != '\n' && *line_end != '\r') line_end++;
        size_t line_len = line_end - p;
        const char *line = p;

        if (line_end < end && *line_end == '\r') {
            if (line_end + 1 < end && *(line_end + 1) == '\n') line_end++;
        }

        size_t trimmed_len = trim_trailing(line, line_len);

        if (in_fence) {
            if (is_fence_end(line, trimmed_len, fence_char, fence_len)) {
                in_fence = 0;
                sb_append(sb, "</code></pre>\n");
            }
            else {
                escape_html(sb, line, line_len);
                sb_append(sb, "\n");
            }
            p = line_end + 1;
            continue;
        }

        if (in_indented_code) {
            if (trimmed_len == 0) {
                /* Blank line inside indented code block: keep it as content */
                sb_append_char(sb, '\n');
                p = line_end + 1;
                continue;
            }
            size_t ic_indent = get_indent_tab(line, line_len);
            if (ic_indent < 4) {
                /* Non-indented line: close code block and re-process */
                in_indented_code = 0;
                sb_append(sb, "</code></pre>\n");
            }
            else {
                size_t content_start = 0;
                size_t col = 0;
                while (content_start < line_len && col < 4) {
                    if (line[content_start] == '\t') col = (col + 4) & ~3;
                    else
                        col++;
                    content_start++;
                }
                escape_html(sb, line + content_start, line_len - content_start);
                sb_append(sb, "\n");
                p = line_end + 1;
                continue;
            }
        }

        /* Start indented code block */
        if (!in_blockquote && list_depth == 0 && trimmed_len > 0 && !para_has_content &&
            !in_indented_code) {
            size_t indent_col = get_indent_tab(line, line_len);
            if (indent_col >= 4) {
                in_indented_code = 1;
                sb_append(sb, "<pre><code>");
                size_t content_start = 0;
                size_t col = 0;
                while (content_start < line_len && col < 4) {
                    if (line[content_start] == '\t') col = (col + 4) & ~3;
                    else
                        col++;
                    content_start++;
                }
                escape_html(sb, line + content_start, line_len - content_start);
                sb_append(sb, "\n");
                p = line_end + 1;
                continue;
            }
        }

        /* Check for setext heading underline */
        if (para_has_content && trimmed_len >= 1) {
            size_t se_start = 0;
            /* At most 3 spaces of indent allowed */
            while (se_start < trimmed_len && se_start < 3 && line[se_start] == ' ')
                se_start++;
            if (se_start < trimmed_len && line[se_start] == '\t')
                se_start = trimmed_len;
            int se_level = is_setext_underline(line + se_start, trimmed_len - se_start);
            if (se_level > 0) {
                const char *se_content = para_buf->data;
                size_t se_content_len = para_buf->len;
                while (se_content_len > 0 &&
                       (*se_content == ' ' || *se_content == '\t')) {
                    se_content++;
                    se_content_len--;
                }
                while (se_content_len > 0 && (se_content[se_content_len - 1] == ' ' ||
                                              se_content[se_content_len - 1] == '\t'))
                    se_content_len--;
                char tmp[4] = {'<', 'h', (char)('0' + se_level), '>'};
                sb_append_n(sb, tmp, 4);
                render_inline(sb, se_content, se_content_len, 1, refs, n_refs);
                sb_append(sb, "</h");
                sb_append_char(sb, (char)('0' + se_level));
                sb_append(sb, ">\n");
                para_has_content = 0;
                para_buf->len = 0;
                para_buf->data[0] = '\0';
                p = line_end + 1;
                continue;
            }
        }

        /* Table support (GFM pipe tables) */
        if (!para_has_content && !in_blockquote && list_depth == 0 && trimmed_len > 0 &&
            is_table_row(line, trimmed_len)) {
            const char *next_line = line_end + 1;
            if (*line_end == '\r' && next_line < end) next_line++;
            if (*line_end == '\n') next_line = line_end + 1;
            /* Check if it's really end of line (CR/LF) */
            while (next_line < end && (*next_line == '\n' || *next_line == '\r'))
                next_line++;
            if (next_line < end) {
                const char *sep_end = next_line;
                while (sep_end < end && *sep_end != '\n' && *sep_end != '\r') sep_end++;
                size_t sep_len = sep_end - next_line;
                if (is_table_separator(next_line, sep_len)) {
                    /* Count columns from separator row */
                    int n_cols = 0;
                    for (size_t ci = 0; ci < sep_len; ci++)
                        if (next_line[ci] == '|') n_cols++;
                    if (n_cols == 0) n_cols = 1;
                    else
                        n_cols--; /* pipes = cells-1 for leading|trailing pipe */

                    /* Detect alignment from separator */
                    char align[64];
                    int n_align = 0;
                    {
                        size_t ci = 0;
                        int cell_idx = 0;
                        while (ci < sep_len && cell_idx < 64) {
                            while (ci < sep_len &&
                                   (next_line[ci] == ' ' || next_line[ci] == '|'))
                                ci++;
                            size_t cell_start = ci;
                            while (ci < sep_len && next_line[ci] != ' ' &&
                                   next_line[ci] != '|')
                                ci++;
                            if (ci > cell_start) {
                                const char *s = next_line + cell_start;
                                size_t sl = ci - cell_start;
                                if (s[0] == ':' && s[sl - 1] == ':')
                                    align[cell_idx] = 'c';
                                else if (s[0] == ':')
                                    align[cell_idx] = 'l';
                                else if (sl > 0 && s[sl - 1] == ':')
                                    align[cell_idx] = 'r';
                                else
                                    align[cell_idx] = 0;
                                cell_idx++;
                            }
                        }
                        n_align = cell_idx;
                    }

                    sb_append(sb, "<table>\n<thead>\n<tr>\n");
                    /* Parse header row */
                    {
                        size_t ci = 0;
                        int cell_idx = 0;
                        while (ci < trimmed_len) {
                            while (ci < trimmed_len &&
                                   (line[ci] == ' ' || line[ci] == ' ' ||
                                    line[ci] == '|'))
                                ci++;
                            size_t cell_start = ci;
                            while (ci < trimmed_len && line[ci] != '|') ci++;
                            size_t cell_end = ci;
                            while (cell_end > cell_start && line[cell_end - 1] == ' ')
                                cell_end--;
                            while (cell_start < cell_end && line[cell_start] == ' ')
                                cell_start++;
                            if (cell_end > cell_start && cell_idx < 64) {
                                sb_append(sb, "<th");
                                if (cell_idx < n_align && align[cell_idx] == 'l')
                                    sb_append(sb, " align=\"left\"");
                                else if (cell_idx < n_align && align[cell_idx] == 'c')
                                    sb_append(sb, " align=\"center\"");
                                else if (cell_idx < n_align && align[cell_idx] == 'r')
                                    sb_append(sb, " align=\"right\"");
                                sb_append(sb, ">");
                                render_inline(
                                    sb, line + cell_start, cell_end - cell_start, 1,
                                    refs, n_refs
                                );
                                sb_append(sb, "</th>\n");
                                cell_idx++;
                            }
                            if (ci >= trimmed_len) break;
                            ci++;
                        }
                    }
                    sb_append(sb, "</tr>\n</thead>\n<tbody>\n");

                    /* Consume body rows */
                    p = sep_end + 1;
                    if (sep_end < end && *sep_end == '\r') {
                        if (sep_end + 1 < end && *(sep_end + 1) == '\n')
                            p = sep_end + 2;
                    }
                    while (p < end) {
                        const char *bline_end = p;
                        while (bline_end < end && *bline_end != '\n' &&
                               *bline_end != '\r')
                            bline_end++;
                        size_t bline_len = bline_end - p;
                        size_t btrimmed = trim_trailing(p, bline_len);
                        if (btrimmed == 0) break;
                        if (!is_table_row(p, btrimmed)) break;

                        sb_append(sb, "<tr>\n");
                        size_t ci = 0;
                        int cell_idx = 0;
                        while (ci < btrimmed) {
                            while (ci < btrimmed && (p[ci] == ' ' || p[ci] == '|'))
                                ci++;
                            size_t cell_start = ci;
                            while (ci < btrimmed && p[ci] != '|') ci++;
                            size_t cell_end = ci;
                            while (cell_end > cell_start && p[cell_end - 1] == ' ')
                                cell_end--;
                            while (cell_start < cell_end && p[cell_start] == ' ')
                                cell_start++;
                            if (cell_end > cell_start && cell_idx < 64) {
                                sb_append(sb, "<td");
                                if (cell_idx < n_align && align[cell_idx] == 'l')
                                    sb_append(sb, " align=\"left\"");
                                else if (cell_idx < n_align && align[cell_idx] == 'c')
                                    sb_append(sb, " align=\"center\"");
                                else if (cell_idx < n_align && align[cell_idx] == 'r')
                                    sb_append(sb, " align=\"right\"");
                                sb_append(sb, ">");
                                render_inline(
                                    sb, p + cell_start, cell_end - cell_start, 1, refs,
                                    n_refs
                                );
                                sb_append(sb, "</td>\n");
                                cell_idx++;
                            }
                            if (ci >= btrimmed) break;
                            ci++;
                        }
                        sb_append(sb, "</tr>\n");
                        p = bline_end + 1;
                        if (bline_end < end && *bline_end == '\r') {
                            if (bline_end + 1 < end && *(bline_end + 1) == '\n')
                                p = bline_end + 2;
                        }
                    }
                    sb_append(sb, "</tbody>\n</table>\n");
                    continue;
                }
            }
        }

        /* HTML block continuation (checked before blank line to capture type 6/7
         * termination) */
        if (in_html_block) {
            if (trimmed_len == 0) {
                /* Blank line terminates type 6/7 blocks */
                if (html_block_type > 5) {
                    in_html_block = 0;
                    /* Consume blank line, do not output it */
                    p = line_end + 1;
                    continue;
                }
                else {
                    /* Type 1-5: blank line is part of the HTML block */
                    sb_append_char(sb, '\n');
                    p = line_end + 1;
                    continue;
                }
            }
            /* Non-blank line: append and check for close */
            sb_append_n(sb, line, line_len);
            sb_append_char(sb, '\n');
            if (html_block_type <= 5) {
                int closed = 0;
                if (html_block_type == 1) {
                    const char *closes[] = {
                        "</pre>", "</script>", "</style>", "</textarea>"
                    };
                    for (int ci = 0; ci < 4 && !closed; ci++) {
                        const char *found = strstr(line, closes[ci]);
                        if (!found) {
                            char upper[32];
                            size_t ul = strlen(closes[ci]);
                            if (ul < sizeof(upper)) {
                                for (size_t ui = 0; ui < ul; ui++)
                                    upper[ui] = toupper((unsigned char)closes[ci][ui]);
                                upper[ul] = '\0';
                                found = strstr(line, upper);
                            }
                        }
                        if (found) closed = 1;
                    }
                }
                else if (html_block_type == 2) {
                    if (strstr(line, "-->")) closed = 1;
                }
                else if (html_block_type == 3) {
                    if (strstr(line, "?>")) closed = 1;
                }
                else if (html_block_type == 4) {
                    for (size_t ci = 0; ci < trimmed_len; ci++) {
                        if (line[ci] == '>') {
                            closed = 1;
                            break;
                        }
                    }
                }
                else if (html_block_type == 5) {
                    if (strstr(line, "]]>")) closed = 1;
                }
                if (closed) in_html_block = 0;
            }
            p = line_end + 1;
            continue;
        }

        /* Blank line: flush paragraph, close containers */
        if (trimmed_len == 0) {
            if (para_has_content) {
                /* Trim trailing whitespace from paragraph buffer */
                while (para_buf->len > 0 && (para_buf->data[para_buf->len - 1] == ' ' ||
                                             para_buf->data[para_buf->len - 1] == '\t'))
                    para_buf->len--;
                para_buf->data[para_buf->len] = '\0';
                sb_append(sb, "<p>");
                render_inline(sb, para_buf->data, para_buf->len, 1, refs, n_refs);
                sb_append(sb, "</p>\n");
                para_has_content = 0;
                para_buf->len = 0;
                para_buf->data[0] = '\0';
            }
            if (in_blockquote) {
                /* Blank line inside blockquote: flush paragraph, continue blockquote */
                bq_has_content = 0;
            }
            if (list_depth > 0) {
                ListEntry *cur = list_top(list_stack, list_depth);
                cur->after_blank = 1;
            }
            p = line_end + 1;
            continue;
        }

        /* Collect reference definitions (requires not in paragraph/container) */
        if (!para_has_content && !in_blockquote && list_depth == 0 && !in_fence &&
            !in_indented_code) {
            RefDef rd;
            if (parse_ref_def(line, line_len, &rd)) {
                /* Check for duplicate label */
                RefDef *existing = NULL;
                if (!find_ref(refs, n_refs, rd.label, rd.label_len, &existing)) {
                    if (n_refs >= cap_refs) {
                        int new_cap = cap_refs ? cap_refs * 2 : 8;
                        RefDef *new_refs =
                            (RefDef *)realloc(refs, new_cap * sizeof(RefDef));
                        if (new_refs) {
                            refs = new_refs;
                            cap_refs = new_cap;
                        }
                    }
                    if (n_refs < cap_refs) refs[n_refs++] = rd;
                    else
                        ref_def_free(&rd);
                }
                else { ref_def_free(&rd); }
                p = line_end + 1;
                continue;
            }
        }

        /* HTML block start detection */
        if (!para_has_content && !in_blockquote && list_depth == 0 && !in_fence &&
            !in_indented_code) {
            int hb_type = is_html_block_start(line, trimmed_len);
            if (hb_type > 0) {
                in_html_block = 1;
                html_block_type = hb_type;
                sb_append_n(sb, line, line_len);
                sb_append_char(sb, '\n');
                p = line_end + 1;
                continue;
            }
        }

        if (in_blockquote && line[0] != '>') {
            int is_list_item = 0;
            size_t indent = get_indent(line, trimmed_len);
            if (trimmed_len > indent + 1) {
                char c = line[indent];
                if ((c == '-' || c == '*' || c == '+') && indent + 1 < trimmed_len &&
                    line[indent + 1] == ' ')
                    is_list_item = 1;
            }
            if (!is_list_item) {
                int is_ol = 0;
                size_t d_start = get_indent(line, trimmed_len);
                int num = 0;
                size_t ds = d_start;
                while (ds < trimmed_len && isdigit((unsigned char)line[ds])) {
                    num = num * 10 + (line[ds] - '0');
                    ds++;
                }
                if (num > 0 && ds < trimmed_len && line[ds] == '.' &&
                    ds + 1 < trimmed_len && line[ds + 1] == ' ')
                    is_ol = 1;
                if (!is_ol && !is_atx_heading(line, trimmed_len) &&
                    !is_thematic_break(line, trimmed_len)) {
                    if (bq_has_content) {
                        sb_append(sb, "<p>");
                        render_inline(
                            sb, para_buf->data, para_buf->len, 1, refs, n_refs
                        );
                        sb_append(sb, "</p>\n");
                        bq_has_content = 0;
                        para_buf->len = 0;
                        para_buf->data[0] = '\0';
                    }
                    sb_append(sb, "</blockquote>\n");
                    in_blockquote = 0;
                }
            }
        }

        /* Detect blockquote marker (up to 3 leading spaces allowed) */
        size_t bq_start = 0;
        while (bq_start < trimmed_len && bq_start < 3 && line[bq_start] == ' ')
            bq_start++;
        int is_blockquote_line = (bq_start < trimmed_len && line[bq_start] == '>');

        /* Flush paragraph before block-level elements */
        if (para_has_content && is_blockquote_line) {
            sb_append(sb, "<p>");
            render_inline(sb, para_buf->data, para_buf->len, 1, refs, n_refs);
            sb_append(sb, "</p>\n");
            para_has_content = 0;
            para_buf->len = 0;
            para_buf->data[0] = '\0';
        }

        if (is_blockquote_line) {
            if (list_depth > 0) {
                list_pop_to(sb, li_content, list_stack, &list_depth, 0);
            }
            if (!in_blockquote) {
                sb_append(sb, "<blockquote>\n");
                in_blockquote = 1;
                bq_has_content = 0;
            }
            /* Parse blockquote marker with column-aware tab handling */
            size_t qpos = bq_start;
            size_t gt_col = bq_start;
            qpos++;
            size_t content_col = 0;
            if (qpos < trimmed_len) {
                if (line[qpos] == ' ') { qpos++; }
                else if (line[qpos] == '\t') {
                    size_t tab_col = gt_col + 1;
                    size_t next_stop = (tab_col + 4) & ~3;
                    size_t expansion = next_stop - tab_col;
                    content_col = expansion > 0 ? expansion - 1 : 0;
                    if (content_col == 0) qpos++;
                }
            }
            if (qpos < trimmed_len) {
                size_t rem = line_len > qpos ? line_len - qpos : 0;
                size_t indent = get_indent_tab_from(line + qpos, rem, content_col);
                if (indent >= 4) {
                    if (bq_has_content) {
                        sb_append(sb, "<p>");
                        render_inline(
                            sb, para_buf->data, para_buf->len, 1, refs, n_refs
                        );
                        sb_append(sb, "</p>\n");
                        bq_has_content = 0;
                        para_buf->len = 0;
                        para_buf->data[0] = '\0';
                    }
                    sb_append(sb, "<pre><code>");
                    size_t leftover = 0;
                    size_t consumed =
                        consume_indent(line + qpos, rem, content_col, &leftover);
                    for (size_t k = 0; k < leftover; k++) sb_append_char(sb, ' ');
                    sb_append_n(sb, line + qpos + consumed, rem - consumed);
                    sb_append(sb, "\n");
                    sb_append(sb, "</code></pre>\n");
                }
                else {
                    size_t ci = 0;
                    size_t col = content_col;
                    while (ci < rem &&
                           (line[qpos + ci] == ' ' || line[qpos + ci] == '\t')) {
                        if (line[qpos + ci] == ' ') col++;
                        else
                            col = (col + 4) & ~3;
                        ci++;
                    }
                    size_t end_pos = trimmed_len - qpos;
                    while (end_pos > 0 && (line[qpos + end_pos - 1] == ' ' ||
                                           line[qpos + end_pos - 1] == '\t'))
                        end_pos--;
                    size_t render_start = ci < end_pos ? ci : end_pos;
                    if (bq_has_content) sb_append_char(para_buf, '\n');
                    bq_has_content = 1;
                    sb_append_n(
                        para_buf, line + qpos + render_start, end_pos - render_start
                    );
                }
            }
            p = line_end + 1;
            continue;
        }

        int f_len = 0;
        char f_char = 0;
        if (is_fence_start(line, trimmed_len, &f_char, &f_len)) {
            if (para_has_content) {
                sb_append(sb, "<p>");
                render_inline(sb, para_buf->data, para_buf->len, 1, refs, n_refs);
                sb_append(sb, "</p>\n");
                para_has_content = 0;
                para_buf->len = 0;
                para_buf->data[0] = '\0';
            }
            if (list_depth > 0) {
                size_t fence_indent = get_indent(line, trimmed_len);
                ListEntry *fence_cur = list_top(list_stack, list_depth);
                if (fence_indent >= (size_t)fence_cur->content_col) {
                    /* Fence inside list item */
                    if (fence_cur->item_open) {
                        list_flush_content(sb, li_content, fence_cur);
                        sb_append_char(sb, '\n');
                    }
                    in_fence = 1;
                    fence_char = f_char;
                    fence_len = f_len;
                    /* The opening fence itself is not code content */
                    p = line_end + 1;
                    continue;
                }
                list_pop_to(sb, li_content, list_stack, &list_depth, 0);
            }
            if (in_blockquote) {
                if (bq_has_content) {
                    sb_append(sb, "<p>");
                    render_inline(sb, para_buf->data, para_buf->len, 1, refs, n_refs);
                    sb_append(sb, "</p>\n");
                    bq_has_content = 0;
                    para_buf->len = 0;
                    para_buf->data[0] = '\0';
                }
                sb_append(sb, "</blockquote>\n");
                in_blockquote = 0;
            }
            in_fence = 1;
            fence_char = f_char;
            fence_len = f_len;
            sb_append(sb, "<pre><code>");
            p = line_end + 1;
            continue;
        }

        /* Setext heading detection (inside list items with pending content) */
        if (list_depth > 0 && li_content->len > 0) {
            size_t se_indent = get_indent(line, trimmed_len);
            ListEntry *se_cur = list_top(list_stack, list_depth);
            if (se_indent >= (size_t)se_cur->content_col && se_cur->item_open) {
                size_t si = se_indent;
                char se_char = line[si];
                if ((se_char == '=' || se_char == '-') && si < trimmed_len) {
                    int se_cnt = 0, se_ok = 1;
                    while (si < trimmed_len) {
                        if (line[si] == ' ' || line[si] == '\t') { si++; continue; }
                        if (line[si] != se_char) { se_ok = 0; break; }
                        se_cnt++; si++;
                    }
                    if (se_ok && se_cnt >= 3) {
                        int se_level = (se_char == '=') ? 1 : 2;
                        sb_append_char(sb, '\n');
                        sb_append_char(sb, '<');
                        sb_append_char(sb, 'h');
                        sb_append_char(sb, (char)('0' + se_level));
                        sb_append_char(sb, '>');
                        render_inline(sb, li_content->data, li_content->len,
                                      1, refs, n_refs);
                        sb_append(sb, "</h");
                        sb_append_char(sb, (char)('0' + se_level));
                        sb_append(sb, ">\n");
                        li_content->len = 0;
                        p = line_end + 1;
                        continue;
                    }
                }
            }
        }

        if (is_thematic_break(line, trimmed_len)) {
            if (para_has_content) {
                sb_append(sb, "<p>");
                render_inline(sb, para_buf->data, para_buf->len, 1, refs, n_refs);
                sb_append(sb, "</p>\n");
                para_has_content = 0;
                para_buf->len = 0;
                para_buf->data[0] = '\0';
            }
            if (list_depth > 0) {
                size_t hr_indent = get_indent(line, trimmed_len);
                ListEntry *hr_cur = list_top(list_stack, list_depth);
                if (hr_indent >= (size_t)hr_cur->content_col) {
                    /* HR inside list item */
                    if (hr_cur->item_open) {
                        list_flush_content(sb, li_content, hr_cur);
                        sb_append_char(sb, '\n');
                    }
                    sb_append(sb, "<hr />\n");
                    p = line_end + 1;
                    continue;
                }
                list_pop_to(sb, li_content, list_stack, &list_depth, 0);
            }
            if (in_blockquote) {
                if (bq_has_content) {
                    sb_append(sb, "<p>");
                    render_inline(sb, para_buf->data, para_buf->len, 1, refs, n_refs);
                    sb_append(sb, "</p>\n");
                    bq_has_content = 0;
                    para_buf->len = 0;
                    para_buf->data[0] = '\0';
                }
                sb_append(sb, "</blockquote>\n");
                in_blockquote = 0;
            }
            sb_append(sb, "<hr />\n");
            p = line_end + 1;
            continue;
        }

        int heading_level = is_atx_heading(line, trimmed_len);
        if (heading_level > 0) {
            int heading_inside_item = 0;
            if (para_has_content) {
                sb_append(sb, "<p>");
                render_inline(sb, para_buf->data, para_buf->len, 1, refs, n_refs);
                sb_append(sb, "</p>\n");
                para_has_content = 0;
                para_buf->len = 0;
                para_buf->data[0] = '\0';
            }
            if (list_depth > 0) {
                size_t h_indent = get_indent(line, trimmed_len);
                ListEntry *h_cur = list_top(list_stack, list_depth);
                if (h_indent >= (size_t)h_cur->content_col) {
                    heading_inside_item = 1;
                    if (h_cur->item_open) {
                        list_flush_content(sb, li_content, h_cur);
                        if (sb->len == 0 || sb->data[sb->len - 1] != '\n')
                            sb_append_char(sb, '\n');
                    }
                }
                else {
                    list_pop_to(sb, li_content, list_stack, &list_depth, 0);
                }
            }
            if (in_blockquote) {
                if (bq_has_content) {
                    sb_append(sb, "<p>");
                    render_inline(sb, para_buf->data, para_buf->len, 1, refs, n_refs);
                    sb_append(sb, "</p>\n");
                    bq_has_content = 0;
                    para_buf->len = 0;
                    para_buf->data[0] = '\0';
                }
                sb_append(sb, "</blockquote>\n");
                in_blockquote = 0;
            }
            size_t h_start = 0;
            while (h_start < trimmed_len &&
                   (line[h_start] == ' ' || line[h_start] == '\t'))
                h_start++;
            while (h_start < trimmed_len && line[h_start] == '#') h_start++;
            while (h_start < trimmed_len &&
                   (line[h_start] == ' ' || line[h_start] == '\t'))
                h_start++;
            size_t h_end = atx_content_end(line, trimmed_len);

            sb_append_char(sb, '<');
            sb_append_char(sb, 'h');
            sb_append_char(sb, (char)('0' + heading_level));
            sb_append_char(sb, '>');
            if (h_end > h_start)
                render_inline(sb, line + h_start, h_end - h_start, 1, refs, n_refs);
            sb_append(sb, "</h");
            sb_append_char(sb, (char)('0' + heading_level));
            sb_append(sb, ">\n");
            if (heading_inside_item) {
                /* Heading inside list item — leave the <li> open */
            }
            p = line_end + 1;
            continue;
        }

        /* --- Detect unordered list items --- */
        {
            size_t ul_indent = get_indent(line, trimmed_len);
            if ((ul_indent < 4 || list_depth > 0) && trimmed_len > ul_indent) {
                char ul_c = line[ul_indent];
                int is_ul = 0;
                if (ul_c == '-' || ul_c == '*' || ul_c == '+') {
                    if (ul_indent + 1 == trimmed_len) {
                        is_ul = !para_has_content || list_depth > 0;
                    }
                    else if (line[ul_indent + 1] == ' ' || line[ul_indent + 1] == '\t') {
                        is_ul = 1;
                    }
                }
                if (is_ul) {
                    int mc = (int)ul_indent;
                    int cc = mc + 2;
                    size_t sp = ul_indent + 2;
                    int extra = 0;
                    while (sp < trimmed_len && (line[sp] == ' ' || line[sp] == '\t') && extra < 3) {
                        if (line[sp] == ' ') { cc++; extra++; }
                        else { cc = (cc + 4) & ~3; sp++; }
                        sp++;
                    }

                    if (para_has_content && list_depth == 0) {
                        sb_append(sb, "<p>");
                        render_inline(sb, para_buf->data, para_buf->len, 1, refs, n_refs);
                        sb_append(sb, "</p>\n");
                        para_has_content = 0;
                        para_buf->len = 0;
                        para_buf->data[0] = '\0';
                    }

                    /* Find where this marker belongs in the list stack */
                    int slot = -1;
                    int sub = 0;
                    if (list_depth > 0) {
                        /* Walk from deepest up to find the containing item */
                        for (int d = list_depth - 1; d >= 0; d--) {
                            if (mc >= list_stack[d].content_col) {
                                /* Sub-list of item d */
                                slot = d;
                                sub = 1;
                                break;
                            }
                            else if (mc >= list_stack[d].marker_col) {
                                /* Same level as item d */
                                slot = d;
                                sub = 0;
                                break;
                            }
                        }
                    }

                    if (slot >= 0 && sub) {
                        /* --- Sub-list --- */
                        list_pop_to(sb, li_content, list_stack, &list_depth, slot + 1);
                        ListEntry *parent = list_top(list_stack, list_depth);
                        if (parent->item_open) {
                            list_flush_content(sb, li_content, parent);
                            if (sb->len == 0 || sb->data[sb->len - 1] != '\n')
                                sb_append_char(sb, '\n');
                        }
                        list_save_content(li_content, parent);
                        list_push(list_stack, &list_depth, 1, ul_c, 0, mc, cc, 0);
                        list_emit_open(sb, &list_stack[list_depth - 1]);
                        list_open_item(sb, &list_stack[list_depth - 1]);
                        goto process_ul_content;
                    }
                    else {
                        /* --- Same level or new list --- */
                        list_pop_to(sb, li_content, list_stack, &list_depth, slot < 0 ? 0 : slot + 1);
                        if (list_depth > 0) {
                            ListEntry *e = list_top(list_stack, list_depth);
                            if (e->after_blank) {
                                e->is_loose = 1;
                                e->after_blank = 0;
                            }
                            list_close_item(sb, li_content, e);
                            if (list_matches(e, 1, ul_c, 0)) {
                                /* Continuing same list */
                                list_stack[list_depth - 1].content_col = cc;
                                list_open_item(sb, &list_stack[list_depth - 1]);
                                goto process_ul_content;
                            }
                            /* Different marker: close this list */
                            list_pop(sb, li_content, list_stack, &list_depth);
                        }
                        /* Start a new list */
                        list_push(list_stack, &list_depth, 1, ul_c, 0, mc, cc, 0);
                        list_emit_open(sb, &list_stack[list_depth - 1]);
                        list_open_item(sb, &list_stack[list_depth - 1]);

process_ul_content: ;
                        size_t ul_content_start = ul_indent + 2;
                        while (ul_content_start < trimmed_len &&
                               (line[ul_content_start] == ' ' || line[ul_content_start] == '\t'))
                            ul_content_start++;
                        if (ul_content_start < trimmed_len) {
                            process_list_content(sb, li_content, list_stack,
                                                 &list_depth,
                                                 line + ul_content_start,
                                                 trimmed_len - ul_content_start,
                                                 (int)ul_content_start,
                                                 refs, n_refs);
                        }
                        p = line_end + 1;
                        continue;
                    }
                }
            }
        }

        /* --- Detect ordered list items --- */
        {
            size_t ol_indent = get_indent(line, trimmed_len);
            if ((ol_indent < 4 || list_depth > 0) && trimmed_len > ol_indent) {
                size_t d_start = ol_indent;
                int num = 0;
                int digit_count = 0;
                while (d_start < trimmed_len && isdigit((unsigned char)line[d_start]) && digit_count < 10) {
                    num = num * 10 + (line[d_start] - '0');
                    d_start++;
                    digit_count++;
                }
                if (digit_count > 0 && digit_count <= 9 && d_start < trimmed_len &&
                    (line[d_start] == '.' || line[d_start] == ')') &&
                    ((d_start + 1 == trimmed_len && (list_depth > 0 || !para_has_content)) ||
                     (d_start + 1 < trimmed_len &&
                      (line[d_start + 1] == ' ' || line[d_start + 1] == '\t')))) {
                    char delim = line[d_start];

                    int mc = (int)ol_indent;
                    int marker_end = (int)(d_start + 1);
                    int cc;
                    if (d_start + 1 == trimmed_len) {
                        cc = marker_end + 1;
                    } else if (line[d_start + 1] == '\t') {
                        cc = (marker_end + 4) & ~3;
                    } else {
                        cc = marker_end + 1;
                        size_t sp = (size_t)(marker_end + 1);
                        while (sp < trimmed_len && cc < 5 &&
                               (line[sp] == ' ' || line[sp] == '\t')) {
                            if (line[sp] == ' ') { cc++; }
                            else { cc = (cc + 4) & ~3; }
                            sp++;
                        }
                    }
                    if (cc <= 4 || list_depth > 0 || !para_has_content) {
                    if (para_has_content && list_depth == 0) {
                        sb_append(sb, "<p>");
                        render_inline(sb, para_buf->data, para_buf->len, 1, refs, n_refs);
                        sb_append(sb, "</p>\n");
                        para_has_content = 0;
                        para_buf->len = 0;
                        para_buf->data[0] = '\0';
                    }

                    /* Find where this marker belongs in the list stack */
                    int slot = -1;
                    int sub = 0;
                    if (list_depth > 0) {
                        for (int d = list_depth - 1; d >= 0; d--) {
                            if (mc >= list_stack[d].content_col) {
                                slot = d;
                                sub = 1;
                                break;
                            }
                            else if (mc >= list_stack[d].marker_col) {
                                slot = d;
                                sub = 0;
                                break;
                            }
                        }
                    }

                    if (slot >= 0 && sub) {
                        /* --- Sub-list --- */
                        list_pop_to(sb, li_content, list_stack, &list_depth, slot + 1);
                        ListEntry *parent = list_top(list_stack, list_depth);
                        if (parent->item_open) {
                            list_flush_content(sb, li_content, parent);
                            if (sb->len == 0 || sb->data[sb->len - 1] != '\n')
                                sb_append_char(sb, '\n');
                        }
                        list_save_content(li_content, parent);
                        list_push(list_stack, &list_depth, 2, 0, delim, mc, cc, num);
                        list_emit_open(sb, &list_stack[list_depth - 1]);
                        list_open_item(sb, &list_stack[list_depth - 1]);
                        goto process_ol_content;
                    }
                    else {
                        /* --- Same level or new list --- */
                        list_pop_to(sb, li_content, list_stack, &list_depth, slot < 0 ? 0 : slot + 1);
                        if (list_depth > 0) {
                            ListEntry *e = list_top(list_stack, list_depth);
                            if (e->after_blank) {
                                e->is_loose = 1;
                                e->after_blank = 0;
                            }
                            list_close_item(sb, li_content, e);
                            if (list_matches(e, 2, 0, delim)) {
                                list_stack[list_depth - 1].content_col = cc;
                                list_open_item(sb, &list_stack[list_depth - 1]);
                                goto process_ol_content;
                            }
                            list_pop(sb, li_content, list_stack, &list_depth);
                        }
                        list_push(list_stack, &list_depth, 2, 0, delim, mc, cc, num);
                        list_emit_open(sb, &list_stack[list_depth - 1]);
                        list_open_item(sb, &list_stack[list_depth - 1]);

process_ol_content: ;
                        size_t ol_content_start = (size_t)(marker_end + 1);
                        while (ol_content_start < trimmed_len &&
                               (line[ol_content_start] == ' ' || line[ol_content_start] == '\t'))
                            ol_content_start++;
                        if (ol_content_start < trimmed_len) {
                            process_list_content(sb, li_content, list_stack,
                                                 &list_depth,
                                                 line + ol_content_start,
                                                 trimmed_len - ol_content_start,
                                                 (int)ol_content_start,
                                                 refs, n_refs);
                        }
                        p = line_end + 1;
                        continue;
                    }
                    }
                }
            }
        }

        /* --- List item continuation --- */
        int list_cont_handled = 0;
        if (list_depth > 0 && trimmed_len > 0) {
            if (para_has_content) {
                sb_append(sb, "<p>");
                render_inline(sb, para_buf->data, para_buf->len, 1, refs, n_refs);
                sb_append(sb, "</p>\n");
                para_has_content = 0;
                para_buf->len = 0;
                para_buf->data[0] = '\0';
            }

            /* Walk up from deepest level to find which item this line continues */
            while (list_depth > 0) {
                ListEntry *cur = list_top(list_stack, list_depth);
                size_t line_indent = get_indent_tab(line, line_len);

                if (line_indent >= (size_t)cur->content_col) {
                    /* This line continues the current list item */
                    size_t extra_indent = line_indent - (size_t)cur->content_col;

                    if (!cur->item_open) {
                        /* Item was closed; emit as new paragraph directly to sb */
                        sb_append(sb, "<p>");
                        size_t scol = 0;
                        while (scol < trimmed_len &&
                               (line[scol] == ' ' || line[scol] == '\t'))
                            scol++;
                        if (scol < trimmed_len) {
                            render_inline(sb, line + scol, trimmed_len - scol,
                                          1, refs, n_refs);
                        }
                        sb_append(sb, "</p>\n");
                    }
                    else if (cur->after_blank && li_content->len == 0) {
                        /* Empty item after blank line: close list, fall through */
                        list_pop_to(sb, li_content, list_stack, &list_depth, 0);
                        goto fallback_paragraph;
                    }
                    else if (cur->after_blank && extra_indent >= 4) {
                        /* Indented code block in list item */
                        size_t leftover = 0;
                        size_t consumed = consume_indent(line, line_len,
                                                         (size_t)cur->content_col, &leftover);
                        sb_append(sb, "<pre><code>");
                        for (size_t k = 0; k < leftover; k++) sb_append_char(sb, ' ');
                        escape_html(sb, line + consumed, line_len - consumed);
                        sb_append(sb, "\n</code></pre>\n");
                        cur->after_blank = 0;
                    }
                    else if (cur->after_blank) {
                        /* New paragraph within list item */
                        cur->after_blank = 0;
                        if (li_content->len > 0) {
                            cur->is_loose = 1;
                            sb_append(sb, "\n<p>");
                            sb_append_n(sb, li_content->data, li_content->len);
                            sb_append(sb, "</p>\n");
                            li_content->len = 0;
                        }
                        size_t scol = 0;
                        while (scol < trimmed_len &&
                               (line[scol] == ' ' || line[scol] == '\t'))
                            scol++;
                        sb_append(sb, "<p>");
                        render_inline(sb, line + scol, trimmed_len - scol, 1, refs, n_refs);
                        sb_append(sb, "</p>\n");
                    }
                    else {
                        /* Continuation without blank line: add to current content */
                        size_t scol = 0;
                        while (scol < trimmed_len &&
                               (line[scol] == ' ' || line[scol] == '\t'))
                            scol++;
                        if (li_content->len > 0 && scol > 0) {
                            sb_append_char(li_content, ' ');
                        }
                        if (scol < trimmed_len) {
                            render_inline(li_content, line + scol, trimmed_len - scol,
                                          1, refs, n_refs);
                        }
                    }
                    p = line_end + 1;
                    list_cont_handled = 1;
                    break;
                }

                /* Doesn't match deepest item — try parent */
                if (list_depth > 1) {
                    ListEntry *parent = &list_stack[list_depth - 2];
                    size_t p_indent = get_indent_tab(line, line_len);
                    if (p_indent >= (size_t)parent->content_col) {
                        /* Line continues parent item — pop deepest */
                        list_pop(sb, li_content, list_stack, &list_depth);
                        continue; /* retry with parent as cur */
                    }
                }

                /* Lazy continuation */
                if (cur->item_open && !cur->after_blank && list_depth == 1) {
                    size_t scol = 0;
                    while (scol < trimmed_len &&
                           (line[scol] == ' ' || line[scol] == '\t'))
                        scol++;
                    if (li_content->len > 0 && scol > 0) {
                        sb_append_char(li_content, ' ');
                    }
                    if (scol < trimmed_len) {
                        render_inline(li_content, line + scol, trimmed_len - scol,
                                      1, refs, n_refs);
                    }
                    p = line_end + 1;
                    list_cont_handled = 1;
                    break;
                }

                /* Close lists and fall through to paragraph handling */
                list_pop_to(sb, li_content, list_stack, &list_depth, 0);
                goto fallback_paragraph;
            }
        }
        if (list_cont_handled) continue;

fallback_paragraph:

        /* Fallback: paragraph content - buffer it */
        if (para_has_content &&
            (para_buf->len == 0 || para_buf->data[para_buf->len - 1] != '\n'))
            sb_append_char(para_buf, '\n');
        para_has_content = 1;

        /* Strip leading up-to-3-spaces indent from first line of paragraph */
        const char *content_start = line;
        size_t content_len = line_len;
        if (para_buf->len == 0 || para_buf->data[para_buf->len - 1] == '\n') {
            int indent_col = 0;
            while (content_len > 0 && indent_col < 3 &&
                   (*content_start == ' ' || *content_start == '\t')) {
                if (*content_start == ' ') indent_col++;
                else indent_col = (indent_col + 4) & ~3;
                content_start++;
                content_len--;
            }
        }
        /* Preserve original line content (including trailing spaces).
           Hard line breaks are detected during inline rendering
           by the pattern `  \n`. */
        sb_append_n(para_buf, content_start, content_len);

        p = line_end + 1;
    }

    /* Flush remaining buffered paragraph */
    if (para_has_content) {
        sb_append(sb, "<p>");
        render_inline(sb, para_buf->data, para_buf->len, 1, refs, n_refs);
        sb_append(sb, "</p>\n");
    }
    /* Close all open list levels */
    list_pop_to(sb, li_content, list_stack, &list_depth, 0);
    if (in_fence) sb_append(sb, "</code></pre>\n");
    if (in_indented_code) sb_append(sb, "</code></pre>\n");
    if (in_blockquote) {
        if (bq_has_content) {
            sb_append(sb, "<p>");
            render_inline(sb, para_buf->data, para_buf->len, 1, refs, n_refs);
            sb_append(sb, "</p>\n");
            bq_has_content = 0;
            para_buf->len = 0;
            para_buf->data[0] = '\0';
        }
        sb_append(sb, "</blockquote>\n");
    }

    sb_free(para_buf);
    sb_free(li_content);
    for (int ri = 0; ri < n_refs; ri++) ref_def_free(&refs[ri]);
    free(refs);
    return sb_detach(sb);
}

static PyObject *_comverter_markdown_to_html(PyObject *module, PyObject *args) {
    (void)module;
    const char *markdown;
    Py_ssize_t md_len;

    if (!PyArg_ParseTuple(args, "s#", &markdown, &md_len)) return NULL;

    if (md_len < 0 || (markdown == NULL && md_len > 0)) {
        PyErr_SetString(
            PyExc_ValueError, "Invalid input: markdown text must be a valid string."
        );
        return NULL;
    }

    if (md_len == 0 || markdown == NULL) return PyUnicode_FromString("");

    char *result = markdown_to_html(markdown, (size_t)md_len);
    if (!result) {
        PyErr_NoMemory();
        return NULL;
    }

    PyObject *py_result = PyUnicode_FromString(result);
    free(result);
    return py_result;
}

static PyMethodDef _comverter_methods[] = {
    {"markdown_to_html", _comverter_markdown_to_html, METH_VARARGS,
     "Convert Markdown text to HTML.\n\n"
     "Args:\n"
     "    markdown_text (str): The Markdown string to convert.\n"
     "Returns:\n"
     "    str: The resulting HTML string.\n"
     "Raises:\n"
     "    TypeError: If the input is not a string.\n"
     "    ValueError: If the input is invalid."                         },
    {              NULL,                        NULL,            0, NULL}
};

static PyModuleDef _comverter_module = {
    PyModuleDef_HEAD_INIT,
    .m_name = "_comverter",
    .m_doc = "ComVerter internal C extension module for format conversion.",
    .m_size = 0,
    .m_methods = _comverter_methods,
};

PyMODINIT_FUNC PyInit__comverter(void) { return PyModuleDef_Init(&_comverter_module); }
