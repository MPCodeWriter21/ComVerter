#include "md_core.h"

/* --- StringBuilder --- */

StringBuilder *sb_new(void) {
    StringBuilder *sb = (StringBuilder *)malloc(sizeof(StringBuilder));
    if (!sb) return NULL;
    sb->data = (char *)malloc(128);
    if (!sb->data) { free(sb); return NULL; }
    sb->data[0] = '\0';
    sb->len = 0;
    sb->cap = 128;
    return sb;
}

int sb_ensure(StringBuilder *sb, size_t extra) {
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

void sb_append(StringBuilder *sb, const char *s) {
    size_t slen = strlen(s);
    if (sb_ensure(sb, slen) == -1) return;
    memcpy(sb->data + sb->len, s, slen);
    sb->len += slen;
    sb->data[sb->len] = '\0';
}

void sb_append_char(StringBuilder *sb, char c) {
    if (sb_ensure(sb, 1) == -1) return;
    sb->data[sb->len++] = c;
    sb->data[sb->len] = '\0';
}

void sb_append_n(StringBuilder *sb, const char *s, size_t n) {
    if (sb_ensure(sb, n) == -1) return;
    memcpy(sb->data + sb->len, s, n);
    sb->len += n;
    sb->data[sb->len] = '\0';
}

void sb_insert(StringBuilder *sb, size_t pos, const char *s, size_t n) {
    if (pos > sb->len) return;
    if (sb_ensure(sb, n) == -1) return;
    memmove(sb->data + pos + n, sb->data + pos, sb->len - pos);
    memcpy(sb->data + pos, s, n);
    sb->len += n;
    sb->data[sb->len] = '\0';
}

void sb_free(StringBuilder *sb) {
    if (sb) { free(sb->data); free(sb); }
}

char *sb_detach(StringBuilder *sb) {
    char *result = sb->data;
    free(sb);
    return result;
}

/* --- Entity table --- */

static int entity_cmp(const void *a, const void *b) {
    return strcmp(((const EntityEntry *)a)->name, ((const EntityEntry *)b)->name);
}

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

int try_decode_entity(const char *text, size_t len, size_t *consumed, unsigned long *out_cps, int *out_n_cps) {
    if (len == 0 || text[0] != '&') return 0;
    size_t semi = 1;
    while (semi < len && text[semi] != ';') semi++;
    if (semi >= len || semi > 31) return 0;
    size_t name_len = semi - 1;

    if (name_len >= 2 && text[1] == '#') {
        char *endptr = NULL;
        unsigned long cp;
        if (name_len >= 3 && (text[2] == 'x' || text[2] == 'X')) {
            cp = strtoul(text + 3, &endptr, 16);
            if (endptr && (size_t)(endptr - text) != semi) return 0;
        } else {
            cp = strtoul(text + 2, &endptr, 10);
            if (endptr && (size_t)(endptr - text) != semi) return 0;
        }
        if (endptr && (size_t)(endptr - text) == semi) {
            if (cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF)) return 0;
            if (cp == 0) cp = 0xFFFD;
            *consumed = semi + 1;
            out_cps[0] = cp;
            *out_n_cps = 1;
            return 1;
        }
        return 0;
    }

    EntityEntry key;
    char name_buf[32];
    if (name_len >= sizeof(name_buf)) return 0;
    memcpy(name_buf, text + 1, name_len);
    name_buf[name_len] = '\0';
    key.name = name_buf;

    EntityEntry *found = (EntityEntry *)bsearch(&key, entities, N_ENTITIES, sizeof(EntityEntry), entity_cmp);
    if (found) {
        *consumed = semi + 1;
        for (int i = 0; i < found->n_cps; i++) out_cps[i] = found->cps[i];
        *out_n_cps = found->n_cps;
        return 1;
    }
    return 0;
}

void escape_html(StringBuilder *sb, const char *text, size_t len) {
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

size_t trim_trailing(const char *line, size_t len) {
    while (len > 0 && (line[len - 1] == ' ' || line[len - 1] == '\t')) len--;
    return len;
}

size_t get_indent(const char *line, size_t len) {
    size_t i = 0;
    while (i < len && line[i] == ' ') i++;
    return i;
}

size_t get_indent_tab(const char *line, size_t len) {
    size_t col = 0;
    for (size_t i = 0; i < len; i++)
        if (line[i] == ' ') col++;
        else if (line[i] == '\t') col = (col + 4) & ~3;
        else break;
    return col;
}

/* Find first non-whitespace character position (skipping both spaces and tabs) */
size_t skip_ws(const char *line, size_t len) {
    size_t i = 0;
    while (i < len && (line[i] == ' ' || line[i] == '\t')) i++;
    return i;
}

/* Compute column at a given character position */
size_t col_at_pos(const char *line, size_t pos) {
    size_t col = 0;
    for (size_t i = 0; i < pos; i++)
        if (line[i] == '\t') col = (col + 4) & ~3;
        else col++;
    return col;
}

const char *skip_spaces(const char *p, const char *end) {
    while (p < end && (*p == ' ' || *p == '\t')) p++;
    return p;
}

size_t get_indent_tab_from(const char *line, size_t len, size_t start_col) {
    size_t col = start_col;
    for (size_t i = 0; i < len; i++)
        if (line[i] == ' ') col++;
        else if (line[i] == '\t') col = (col + 4) & ~3;
        else break;
    return col - start_col;
}

size_t consume_indent(const char *s, size_t len, size_t start_col, size_t *leftover_spaces) {
    size_t col = start_col;
    size_t i = 0;
    size_t target = start_col + 4;
    *leftover_spaces = 0;
    while (i < len && col < target) {
        if (s[i] == ' ') { col++; i++; }
        else if (s[i] == '\t') {
            size_t next = (col + 4) & ~3;
            if (next > target) {
                *leftover_spaces = next - target;
                i++; col = target; break;
            }
            col = next; i++;
        } else break;
    }
    if (col > target) *leftover_spaces = col - target;
    return i;
}

void append_codepoint_utf8(StringBuilder *sb, unsigned long cp) {
    if (cp <= 0x7F) sb_append_char(sb, (char)cp);
    else if (cp <= 0x7FF) {
        sb_append_char(sb, (char)(0xC0 | (cp >> 6)));
        sb_append_char(sb, (char)(0x80 | (cp & 0x3F)));
    } else if (cp <= 0xFFFF) {
        sb_append_char(sb, (char)(0xE0 | (cp >> 12)));
        sb_append_char(sb, (char)(0x80 | ((cp >> 6) & 0x3F)));
        sb_append_char(sb, (char)(0x80 | (cp & 0x3F)));
    } else {
        sb_append_char(sb, (char)(0xF0 | (cp >> 18)));
        sb_append_char(sb, (char)(0x80 | ((cp >> 12) & 0x3F)));
        sb_append_char(sb, (char)(0x80 | ((cp >> 6) & 0x3F)));
        sb_append_char(sb, (char)(0x80 | (cp & 0x3F)));
    }
}

int codepoint_to_utf8_buf(char *buf, unsigned long cp) {
    if (cp <= 0x7F) { buf[0] = (char)cp; return 1; }
    else if (cp <= 0x7FF) {
        buf[0] = (char)(0xC0 | (cp >> 6));
        buf[1] = (char)(0x80 | (cp & 0x3F));
        return 2;
    } else if (cp <= 0xFFFF) {
        buf[0] = (char)(0xE0 | (cp >> 12));
        buf[1] = (char)(0x80 | ((cp >> 6) & 0x3F));
        buf[2] = (char)(0x80 | (cp & 0x3F));
        return 3;
    } else {
        buf[0] = (char)(0xF0 | (cp >> 18));
        buf[1] = (char)(0x80 | ((cp >> 12) & 0x3F));
        buf[2] = (char)(0x80 | ((cp >> 6) & 0x3F));
        buf[3] = (char)(0x80 | (cp & 0x3F));
        return 4;
    }
}

void decode_entity_and_append(StringBuilder *sb, const char *text, size_t len, size_t *consumed) {
    unsigned long cps[4];
    int n_cps = 0;
    if (try_decode_entity(text, len, consumed, cps, &n_cps)) {
        for (int i = 0; i < n_cps; i++) {
            unsigned long cp = cps[i];
            if (cp == '&') sb_append(sb, "&amp;");
            else if (cp == '<') sb_append(sb, "&lt;");
            else if (cp == '>') sb_append(sb, "&gt;");
            else if (cp == '"') sb_append(sb, "&quot;");
            else append_codepoint_utf8(sb, cp);
        }
    }
}

void append_entity_decoded(StringBuilder *sb, const char *text, size_t len) {
    size_t i = 0;
    while (i < len) {
        if (text[i] == '\\' && i + 1 < len &&
            strchr("!\"#$%&'()*+,-./:;<=>?@[\\]^_`{|}~", text[i + 1])) {
            unsigned char c = (unsigned char)text[i + 1];
            if (c == '"') sb_append(sb, "&quot;");
            else if (c == '&') sb_append(sb, "&amp;");
            else if (c == '<') sb_append(sb, "&lt;");
            else if (c == '>') sb_append(sb, "&gt;");
            else sb_append_char(sb, (char)c);
            i += 2;
            continue;
        }
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
        } else {
            unsigned char c = (unsigned char)text[i];
            if (c == '"') sb_append(sb, "&quot;");
            else if (c == '&') sb_append(sb, "&amp;");
            else if (c == '<') sb_append(sb, "&lt;");
            else if (c == '>') sb_append(sb, "&gt;");
            else sb_append_char(sb, (char)c);
            i++;
        }
    }
}

static void append_uri_encoded_inner(StringBuilder *sb, unsigned char c, const char *hex) {
    if (isalnum(c) || c == '-' || c == '.' || c == '_' || c == '~' || c == ':' ||
        c == '/' || c == '?' || c == '#' || c == '@' || c == '!' || c == '$' ||
        c == '&' || c == '\'' || c == '(' || c == ')' || c == '*' || c == '+' ||
        c == ',' || c == ';' || c == '=' || c == '%') {
        if (sb_ensure(sb, 1) == -1) return;
        sb_append_char(sb, (char)c);
    } else {
        if (sb_ensure(sb, 3) == -1) return;
        sb_append_char(sb, '%');
        sb_append_char(sb, hex[c >> 4]);
        sb_append_char(sb, hex[c & 0xF]);
    }
}

void append_uri_encoded(StringBuilder *sb, unsigned char c) {
    const char hex[] = "0123456789ABCDEF";
    append_uri_encoded_inner(sb, c, hex);
}

void append_url_with_entities(StringBuilder *sb, const char *text, size_t len) {
    const char hex[] = "0123456789ABCDEF";
    static int is_unreserved[256] = {0};
    static int init = 0;
    if (!init) {
        const char *unreserved = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-._~!#'()*+,;=:/?!@[]%$&";
        for (const char *p = unreserved; *p; p++) is_unreserved[(unsigned char)*p] = 1;
        init = 1;
    }

    size_t i = 0;
    while (i < len) {
        if (text[i] == '\\' && i + 1 < len &&
            strchr("!\"#$%&'()*+,-./:;<=>?@[\\]^_`{|}~", text[i + 1])) {
            unsigned char c = (unsigned char)text[i + 1];
            if (is_unreserved[c]) sb_append_char(sb, (char)c);
            else {
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
                        if (is_unreserved[c]) sb_append_char(sb, (char)c);
                        else {
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
        } else {
            unsigned char c = (unsigned char)text[i];
            if (is_unreserved[c]) sb_append_char(sb, (char)c);
            else {
                sb_append_char(sb, '%');
                sb_append_char(sb, hex[c >> 4]);
                sb_append_char(sb, hex[c & 0xF]);
            }
            i++;
        }
    }
}

int is_valid_uri_autolink(const char *text, size_t len, size_t *consumed) {
    if (len == 0 || text[0] != '<') return 0;
    const char *end = (const char *)memchr(text, '>', len);
    if (!end) return 0;
    size_t content_len = (size_t)(end - text - 1);
    const char *content = text + 1;
    if (content_len == 0) return 0;
    size_t s = 0;
    if (!isalpha((unsigned char)content[s])) return 0;
    s++;
    while (s < content_len && (isalnum((unsigned char)content[s]) || content[s] == '+' || content[s] == '-' || content[s] == '.')) s++;
    if (s >= content_len || content[s] != ':') return 0;
    if (s < 2) return 0;
    for (size_t i = s + 1; i < content_len; i++) {
        unsigned char c = (unsigned char)content[i];
        if (c <= 0x20 || c == 0x7F) return 0;
    }
    *consumed = (size_t)(end - text + 1);
    return 1;
}

int is_valid_email_autolink(const char *text, size_t len, size_t *consumed) {
    if (len == 0 || text[0] != '<') return 0;
    const char *end = (const char *)memchr(text, '>', len);
    if (!end) return 0;
    size_t content_len = (size_t)(end - text - 1);
    const char *content = text + 1;
    if (content_len == 0 || content_len > 254) return 0;
    const char *at = (const char *)memchr(content, '@', content_len);
    if (!at) return 0;
    size_t local_len = (size_t)(at - content);
    size_t domain_len = content_len - local_len - 1;
    if (local_len == 0 || domain_len == 0) return 0;
    if (content[0] == '.') return 0;
    for (size_t i = 0; i < local_len; i++) {
        unsigned char c = (unsigned char)content[i];
        if (!isalnum(c) && c != '.' && c != '%' && c != '+' && c != '-' && c != '_') return 0;
        if (c == '.' && (i + 1 >= local_len || content[i + 1] == '.')) return 0;
    }
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
    if (!isalnum((unsigned char)domain[domain_len - 1])) return 0;
    *consumed = (size_t)(end - text + 1);
    return 1;
}

/* --- Reference definitions --- */

void ref_def_free(RefDef *rd) {
    if (rd->label) free(rd->label);
    if (rd->url) free(rd->url);
    if (rd->title) free(rd->title);
}

size_t normalize_label(const char *src, size_t src_len, char *dst, size_t dst_cap) {
    size_t j = 0;
    int last_was_space = 1;
    for (size_t i = 0; i < src_len && j < dst_cap - 1; i++) {
        unsigned char c = (unsigned char)src[i];
        if (c == '\\' && i + 1 < src_len) {
            unsigned char next = (unsigned char)src[i + 1];
            if (next == ' ' || next == '\t' || next == '\n' || next == '\r') {
                if (!last_was_space && j > 0) { dst[j++] = ' '; last_was_space = 1; }
            } else {
                dst[j++] = (char)tolower(next);
                last_was_space = 0;
            }
            i++;
        } else if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
            if (!last_was_space && j > 0) { dst[j++] = ' '; last_was_space = 1; }
        } else {
            dst[j++] = (char)tolower(c);
            last_was_space = 0;
        }
    }
    while (j > 0 && dst[j - 1] == ' ') j--;
    dst[j] = '\0';
    return j;
}

int is_backslash_escaped(const char *text, size_t len, size_t pos) {
    if (pos == 0 || text[pos - 1] != '\\') return 0;
    int count = 0;
    size_t i = pos;
    while (i > 0 && text[i - 1] == '\\') { count++; i--; }
    return count % 2 == 1;
}

int find_ref(RefDef *refs, int n_refs, const char *label, size_t label_len, RefDef **out) {
    char norm[256];
    size_t norm_len = normalize_label(label, label_len, norm, sizeof(norm));
    for (int i = 0; i < n_refs; i++) {
        if (refs[i].label_len == norm_len && memcmp(refs[i].label, norm, norm_len) == 0) {
            *out = &refs[i];
            return 1;
        }
    }
    return 0;
}

int parse_ref_def(const char *line, size_t line_len, RefDef *rd) {
    memset(rd, 0, sizeof(*rd));
    size_t i = 0;
    while (i < line_len && i < 3 && line[i] == ' ') i++;
    if (i >= line_len || line[i] != '[') return 0;
    i++;
    size_t label_start = i;
    size_t label_end = 0;
    int has_bracket_inside = 0;
    while (i < line_len) {
        if (line[i] == '\\' && i + 1 < line_len) {
            i += 2; continue;
        }
        if (line[i] == '[') { has_bracket_inside = 1; i++; continue; }
        if (line[i] == ']') { label_end = i; break; }
        i++;
    }
    if (label_end == 0) return 0;
    if (has_bracket_inside) return 0;
    i++;
    while (i < line_len && (line[i] == ' ' || line[i] == '\t')) i++;
    if (i >= line_len || line[i] != ':') return 0;
    i++;
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
        i++;
    } else {
        dest_start = i;
        while (i < line_len) {
            unsigned char c = (unsigned char)line[i];
            if (c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == 0x0C || c == 0x0B) break;
            if (c == '\\' && i + 1 < line_len && strchr("!\"#$%&'()*+,-./:;<=>?@[\\]^_`{|}~", line[i + 1])) { i += 2; continue; }
            if (c == '(') return 0;
            if (c == ')') return 0;
            i++;
        }
        dest_len = i - dest_start;
    }
    while (i < line_len && (line[i] == ' ' || line[i] == '\t')) i++;
    size_t title_start = 0, title_len = 0;
    if (i < line_len) {
        if (line[i] == '"' || line[i] == '\'') {
            char delim = line[i]; i++;
            title_start = i;
            while (i < line_len && line[i] != delim)
                if (line[i] == '\\' && i + 1 < line_len && (line[i + 1] == delim || line[i + 1] == '\\')) i += 2;
                else i++;
            if (i >= line_len) return 0;
            title_len = i - title_start;
            i++;
        } else if (line[i] == '(') {
            i++; title_start = i;
            int pdepth = 1;
            while (i < line_len && pdepth > 0) {
                if (line[i] == '(') pdepth++;
                else if (line[i] == ')') pdepth--;
                if (pdepth > 0) {
                    if (line[i] == '\\' && i + 1 < line_len && (line[i + 1] == '(' || line[i + 1] == ')' || line[i + 1] == '\\')) i += 2;
                    else i++;
                }
            }
            if (pdepth != 0) return 0;
            title_len = i - title_start;
            i++;
        }
        while (i < line_len && (line[i] == ' ' || line[i] == '\t')) i++;
        if (i < line_len) return 0;
    }
    if (label_end == label_start) return 0;
    rd->label = (char *)malloc(label_end - label_start + 1);
    if (!rd->label) return 0;
    rd->label_len = normalize_label(line + label_start, label_end - label_start, rd->label, 256);
    if (rd->label_len == 0) { free(rd->label); return 0; }
    rd->url = (char *)malloc(dest_len + 1);
    if (!rd->url) { free(rd->label); return 0; }
    memcpy(rd->url, line + dest_start, dest_len);
    rd->url[dest_len] = '\0';
    if (title_len > 0) {
        rd->title = (char *)malloc(title_len + 1);
        if (!rd->title) { free(rd->label); free(rd->url); return 0; }
        memcpy(rd->title, line + title_start, title_len);
        rd->title[title_len] = '\0';
    }
    return 1;
}

static int is_next_unicode_ws(const char *text, size_t len, size_t pos) {
    if (pos >= len) return 0;
    unsigned char c = (unsigned char)text[pos];
    if (c <= 0x20) return 1;
    if (c == 0xC2 && pos + 1 < len && (unsigned char)text[pos + 1] == 0xA0) return 1;
    if (pos + 2 < len && (unsigned char)text[pos] == 0xE3 && (unsigned char)text[pos + 1] == 0x80 && (unsigned char)text[pos + 2] == 0x80) return 1; /* U+3000 */
    if (pos + 1 < len && (unsigned char)text[pos] == 0xC2 && (unsigned char)text[pos + 1] == 0x85) return 1; /* U+0085 */
    if (pos + 2 < len && (unsigned char)text[pos] == 0xE2 && (unsigned char)text[pos + 1] == 0x80 && (unsigned char)text[pos + 2] >= 0x80 && (unsigned char)text[pos + 2] <= 0x8A) return 1; /* U+2000-U+200A */
    if (pos + 2 < len && (unsigned char)text[pos] == 0xE2 && (unsigned char)text[pos + 1] == 0x80 && (unsigned char)text[pos + 2] == 0xA8) return 1; /* U+2028 */
    if (pos + 2 < len && (unsigned char)text[pos] == 0xE2 && (unsigned char)text[pos + 1] == 0x80 && (unsigned char)text[pos + 2] == 0xA9) return 1; /* U+2029 */
    if (pos + 2 < len && (unsigned char)text[pos] == 0xE2 && (unsigned char)text[pos + 1] == 0x81 && (unsigned char)text[pos + 2] == 0x9F) return 1; /* U+205F */
    return 0;
}

static int is_prev_unicode_ws(const char *text, size_t len, size_t pos) {
    if (pos >= len || pos == 0) return 0;
    unsigned char c = (unsigned char)text[pos - 1];
    if (c <= 0x20) return 1;
    if (c == 0xA0) return 1;
    if (pos >= 2 && (unsigned char)text[pos - 2] == 0xC2 && c == 0xA0) return 1;
    if (pos >= 3 && (unsigned char)text[pos - 3] == 0xE3 && (unsigned char)text[pos - 2] == 0x80 && c == 0x80) return 1; /* U+3000 */
    if (pos >= 2 && (unsigned char)text[pos - 2] == 0xC2 && c == 0x85) return 1; /* U+0085 */
    if (pos >= 3 && (unsigned char)text[pos - 3] == 0xE2 && (unsigned char)text[pos - 2] == 0x80 && c >= 0x80 && c <= 0x8A) return 1; /* U+2000-U+200A */
    if (pos >= 3 && (unsigned char)text[pos - 3] == 0xE2 && (unsigned char)text[pos - 2] == 0x80 && c == 0xA8) return 1; /* U+2028 */
    if (pos >= 3 && (unsigned char)text[pos - 3] == 0xE2 && (unsigned char)text[pos - 2] == 0x80 && c == 0xA9) return 1; /* U+2029 */
    if (pos >= 3 && (unsigned char)text[pos - 3] == 0xE2 && (unsigned char)text[pos - 2] == 0x81 && c == 0x9F) return 1; /* U+205F */
    return 0;
}

size_t utf8_char_start(const char *text, size_t pos) {
    if (pos == 0) return 0;
    while (pos > 0 && ((unsigned char)text[pos] & 0xC0) == 0x80) pos--;
    return pos;
}

static int is_unicode_alnum(const char *text, size_t len, size_t pos) __attribute__((unused));
static int is_unicode_alnum(const char *text, size_t len, size_t pos) {
    if (pos >= len) return 0;
    unsigned char c = (unsigned char)text[pos];
    if (c < 0x80) return isalnum(c) ? 1 : 0;
    unsigned long cp;
    if (c >= 0xC0 && c < 0xE0) {
        if (pos + 1 >= len || (text[pos + 1] & 0xC0) != 0x80) return 0;
        cp = ((unsigned long)(c & 0x1F) << 6) | (text[pos + 1] & 0x3F);
    } else if (c >= 0xE0 && c < 0xF0) {
        if (pos + 2 >= len || (text[pos + 1] & 0xC0) != 0x80 || (text[pos + 2] & 0xC0) != 0x80) return 0;
        cp = ((unsigned long)(c & 0x0F) << 12) | ((text[pos + 1] & 0x3F) << 6) | (text[pos + 2] & 0x3F);
    } else if (c >= 0xF0 && c < 0xF8) {
        if (pos + 3 >= len) return 0;
        for (int i = 1; i < 4; i++) if ((text[pos + i] & 0xC0) != 0x80) return 0;
        cp = ((unsigned long)(c & 0x07) << 18) | ((text[pos + 1] & 0x3F) << 12) | ((text[pos + 2] & 0x3F) << 6) | (text[pos + 3] & 0x3F);
    } else {
        return 0;
    }
    if (cp <= 0x7F) return isalnum((int)cp) ? 1 : 0;
    if (cp < 0xA0) return 0;
    if (cp >= 0x00D7 && cp <= 0x00D7) return 0; /* multiplication sign */
    if (cp >= 0x00F7 && cp <= 0x00F7) return 0; /* division sign */
    if (cp >= 0x037E && cp <= 0x037E) return 0; /* Greek question mark */
    if (cp >= 0x2000 && cp <= 0x206F) return 0; /* General Punctuation */
    if (cp >= 0x2100 && cp <= 0x214F) return 0; /* Letterlike Symbols */
    if (cp >= 0x2150 && cp <= 0x218F) return 0; /* Number Forms */
    if (cp >= 0x2190 && cp <= 0x21FF) return 0; /* Arrows */
    if (cp >= 0x2200 && cp <= 0x22FF) return 0; /* Math Operators */
    if (cp >= 0x2300 && cp <= 0x23FF) return 0; /* Misc Technical */
    if (cp >= 0x2400 && cp <= 0x243F) return 0; /* Control Pictures */
    if (cp >= 0x2440 && cp <= 0x245F) return 0; /* OCR */
    if (cp >= 0x2460 && cp <= 0x24FF) return 0; /* Enclosed Alphanumerics */
    if (cp >= 0x2500 && cp <= 0x257F) return 0; /* Box Drawing */
    if (cp >= 0x2580 && cp <= 0x259F) return 0; /* Block Elements */
    if (cp >= 0x25A0 && cp <= 0x25FF) return 0; /* Geometric Shapes */
    if (cp >= 0x2600 && cp <= 0x26FF) return 0; /* Misc Symbols */
    if (cp >= 0x2700 && cp <= 0x27BF) return 0; /* Dingbats */
    if (cp >= 0x27C0 && cp <= 0x27EF) return 0; /* Misc Math A */
    if (cp >= 0x27F0 && cp <= 0x27FF) return 0; /* Suppl Arrows A */
    if (cp >= 0x2800 && cp <= 0x28FF) return 0; /* Braille */
    if (cp >= 0x2900 && cp <= 0x297F) return 0; /* Suppl Arrows B */
    if (cp >= 0x2980 && cp <= 0x29FF) return 0; /* Misc Math B */
    if (cp >= 0x2A00 && cp <= 0x2AFF) return 0; /* Suppl Math Ops */
    if (cp >= 0x2B00 && cp <= 0x2BFF) return 0; /* Misc Arrows */
    if (cp >= 0x2E00 && cp <= 0x2E7F) return 0; /* Suppl Punctuation */
    if (cp >= 0x3000 && cp <= 0x303F) {
        if (cp >= 0x3021 && cp <= 0x3029) return 1; /* Hangzhou numerals */
        return 0;
    }
    if (cp >= 0xF900 && cp <= 0xFAFF) return 1; /* CJK Compatibility Ideographs */
    if (cp >= 0xFB00 && cp <= 0xFB4F) return 1; /* Alphabetic PF */
    if (cp >= 0xFB50 && cp <= 0xFDFF) return 1; /* Arabic PF A */
    if (cp >= 0xFE00 && cp <= 0xFE0F) return 0; /* Variation Selectors */
    if (cp >= 0xFE10 && cp <= 0xFE1F) return 0; /* Vertical Forms */
    if (cp >= 0xFE20 && cp <= 0xFE2F) return 0; /* Combining Half Marks */
    if (cp >= 0xFE30 && cp <= 0xFE4F) return 0; /* CJK Compatibility Forms */
    if (cp >= 0xFE50 && cp <= 0xFE6F) return 0; /* Small Form Variants */
    if (cp >= 0xFE70 && cp <= 0xFEFF) return 1; /* Arabic PF B */
    if (cp >= 0xFF00 && cp <= 0xFFEF) {
        if (cp >= 0xFF21 && cp <= 0xFF3A) return 1; /* Fullwidth A-Z */
        if (cp >= 0xFF41 && cp <= 0xFF5A) return 1; /* Fullwidth a-z */
        if (cp >= 0xFF10 && cp <= 0xFF19) return 1; /* Fullwidth 0-9 */
        return 0;
    }
    if (cp >= 0xFFF0 && cp <= 0xFFFF) return 0; /* Specials */
    if (cp >= 0x1D000 && cp <= 0x1D0FF) return 0; /* Byzantine Musical */
    if (cp >= 0x1D100 && cp <= 0x1D1FF) return 0; /* Musical */
    if (cp >= 0x1D200 && cp <= 0x1D24F) return 0; /* Ancient Greek Music */
    if (cp >= 0x1D300 && cp <= 0x1D35F) return 0; /* Tai Xuan Jing */
    if (cp >= 0x1D400 && cp <= 0x1D7FF) return 0; /* Math Alphanumeric */
    if (cp >= 0x1D800 && cp <= 0x1DAAF) return 0; /* Sutton SignWriting */
    if (cp >= 0x1E000 && cp <= 0x1E02F) return 0; /* Glagolitic Suppl */
    if (cp >= 0x1E900 && cp <= 0x1E95F) return 1; /* Adlam */
    if (cp >= 0x1EE00 && cp <= 0x1EEFF) return 1; /* Arabic Math */
    if (cp >= 0x1F000 && cp <= 0x1F02F) return 0; /* Mahjong */
    if (cp >= 0x1F030 && cp <= 0x1F09F) return 0; /* Domino */
    if (cp >= 0x1F0A0 && cp <= 0x1F0FF) return 0; /* Playing Cards */
    if (cp >= 0x1F100 && cp <= 0x1F1FF) return 0; /* Enclosed Alphanumeric Suppl */
    if (cp >= 0x1F200 && cp <= 0x1F2FF) return 0; /* Enclosed Ideographic */
    if (cp >= 0x1F300 && cp <= 0x1F5FF) return 0; /* Misc Pictographs */
    if (cp >= 0x1F600 && cp <= 0x1F64F) return 0; /* Emoticons */
    if (cp >= 0x1F650 && cp <= 0x1F67F) return 0; /* Ornamental Dingbats */
    if (cp >= 0x1F680 && cp <= 0x1F6FF) return 0; /* Transport */
    if (cp >= 0x1F700 && cp <= 0x1F77F) return 0; /* Alchemical */
    if (cp >= 0x1F780 && cp <= 0x1F7FF) return 0; /* Geometric Shapes Ext */
    if (cp >= 0x1F800 && cp <= 0x1F8FF) return 0; /* Suppl Arrows C */
    if (cp >= 0x1F900 && cp <= 0x1F9FF) return 0; /* Suppl Pictographs */
    if (cp >= 0x1FA00 && cp <= 0x1FA6F) return 0; /* Chess */
    if (cp >= 0x1FA70 && cp <= 0x1FAFF) return 0; /* Symbols Ext A */
    if (cp >= 0x1FB00 && cp <= 0x1FBFF) return 0; /* Symbols Ext B */
    if (cp >= 0x20000 && cp <= 0x2A6DF) return 1; /* CJK B */
    if (cp >= 0x2A700 && cp <= 0x2B73F) return 1; /* CJK C */
    if (cp >= 0x2B740 && cp <= 0x2B81F) return 1; /* CJK D */
    if (cp >= 0x2B820 && cp <= 0x2CEAF) return 1; /* CJK E */
    if (cp >= 0x2CEB0 && cp <= 0x2EBEF) return 1; /* CJK F */
    if (cp >= 0x2F800 && cp <= 0x2FA1F) return 1; /* CJK Comp Suppl */
    if (cp >= 0xE0000 && cp <= 0xE007F) return 0; /* Tags */
    if (cp >= 0xE0100 && cp <= 0xE01EF) return 0; /* Variation Selectors */
    return 1;
}

int is_unicode_punct(const char *text, size_t len, size_t pos) {
    if (pos >= len) return 0;
    unsigned char c = (unsigned char)text[pos];
    if (c < 0x80) {
        static const char ascii_punct[] = "!\"#$%&'()*+,-./:;<=>?@[\\]^_`{|}~";
        return strchr(ascii_punct, (char)c) != NULL;
    }
    unsigned long cp;
    if (c >= 0xC0 && c < 0xE0) {
        if (pos + 1 >= len || (text[pos + 1] & 0xC0) != 0x80) return 0;
        cp = ((unsigned long)(c & 0x1F) << 6) | (text[pos + 1] & 0x3F);
    } else if (c >= 0xE0 && c < 0xF0) {
        if (pos + 2 >= len || (text[pos + 1] & 0xC0) != 0x80 || (text[pos + 2] & 0xC0) != 0x80) return 0;
        cp = ((unsigned long)(c & 0x0F) << 12) | ((text[pos + 1] & 0x3F) << 6) | (text[pos + 2] & 0x3F);
    } else if (c >= 0xF0 && c < 0xF8) {
        if (pos + 3 >= len) return 0;
        for (int i = 1; i < 4; i++) if ((text[pos + i] & 0xC0) != 0x80) return 0;
        cp = ((unsigned long)(c & 0x07) << 18) | ((text[pos + 1] & 0x3F) << 12) | ((text[pos + 2] & 0x3F) << 6) | (text[pos + 3] & 0x3F);
    } else {
        return 0;
    }
    if (cp < 0xA0) {
        if (cp <= 0x20) return 1;
        if (cp >= 0x21 && cp <= 0x2F) return 1;
        if (cp >= 0x3A && cp <= 0x40) return 1;
        if (cp >= 0x5B && cp <= 0x60) return 1;
        if (cp >= 0x7B && cp <= 0x7E) return 1;
        if (cp >= 0x7F && cp <= 0x9F) return 0;
        return 0;
    }
    if (cp >= 0x00A1 && cp <= 0x00A9) return 1;
    if (cp >= 0x00AB && cp <= 0x00AC) return 1;
    if (cp >= 0x00AE && cp <= 0x00B1) return 1;
    if (cp >= 0x00B4 && cp <= 0x00B8) return 1;
    if (cp >= 0x00BB && cp <= 0x00BF) return 1;
    if (cp >= 0x00D7 && cp <= 0x00D7) return 1;
    if (cp >= 0x00F7 && cp <= 0x00F7) return 1;
    if (cp >= 0x02B0 && cp <= 0x02FF) return 0;
    if (cp >= 0x0300 && cp <= 0x036F) return 0;
    if (cp >= 0x037E && cp <= 0x037E) return 1;
    if (cp >= 0x0385 && cp <= 0x0385) return 1;
    if (cp >= 0x0387 && cp <= 0x0387) return 1;
    if (cp >= 0x0589 && cp <= 0x058A) return 1;
    if (cp >= 0x05BE && cp <= 0x05BE) return 1;
    if (cp >= 0x05C0 && cp <= 0x05C0) return 1;
    if (cp >= 0x05C3 && cp <= 0x05C3) return 1;
    if (cp >= 0x05C6 && cp <= 0x05C6) return 1;
    if (cp >= 0x05F3 && cp <= 0x05F4) return 1;
    if (cp >= 0x0606 && cp <= 0x0608) return 1;
    if (cp >= 0x0609 && cp <= 0x060A) return 1;
    if (cp >= 0x060C && cp <= 0x060D) return 1;
    if (cp >= 0x061B && cp <= 0x061B) return 1;
    if (cp >= 0x061D && cp <= 0x061F) return 1;
    if (cp >= 0x0620 && cp <= 0x063F) return 0;
    if (cp >= 0x0640 && cp <= 0x0640) return 1;
    if (cp >= 0x0641 && cp <= 0x064A) return 0;
    if (cp >= 0x064B && cp <= 0x0655) return 0;
    if (cp >= 0x0660 && cp <= 0x0669) return 0;
    if (cp >= 0x066A && cp <= 0x066D) return 1;
    if (cp >= 0x06D4 && cp <= 0x06D4) return 1;
    if (cp >= 0x06F0 && cp <= 0x06F9) return 0;
    if (cp >= 0x0700 && cp <= 0x070D) return 1;
    if (cp >= 0x070F && cp <= 0x070F) return 1;
    if (cp >= 0x0710 && cp <= 0x0710) return 0;
    if (cp >= 0x0711 && cp <= 0x0711) return 0;
    if (cp >= 0x07C0 && cp <= 0x07C9) return 0;
    if (cp >= 0x07CA && cp <= 0x07EA) return 0;
    if (cp >= 0x07F6 && cp <= 0x07F9) return 1;
    if (cp >= 0x0800 && cp <= 0x0815) return 0;
    if (cp >= 0x0830 && cp <= 0x083E) return 1;
    if (cp >= 0x085E && cp <= 0x085E) return 1;
    if (cp >= 0x0964 && cp <= 0x0965) return 1;
    if (cp >= 0x0970 && cp <= 0x0970) return 1;
    if (cp >= 0x09F2 && cp <= 0x09F3) return 1;
    if (cp >= 0x09FB && cp <= 0x09FB) return 1;
    if (cp >= 0x0AF0 && cp <= 0x0AF1) return 1;
    if (cp >= 0x0AF9 && cp <= 0x0AF9) return 1;
    if (cp >= 0x0BF3 && cp <= 0x0BFA) return 1;
    if (cp >= 0x0C78 && cp <= 0x0C7E) return 1;
    if (cp >= 0x0CF1 && cp <= 0x0CF2) return 1;
    if (cp >= 0x0E3F && cp <= 0x0E3F) return 1;
    if (cp >= 0x0E4F && cp <= 0x0E4F) return 1;
    if (cp >= 0x0E5A && cp <= 0x0E5B) return 1;
    if (cp >= 0x0F01 && cp <= 0x0F0A) return 1;
    if (cp >= 0x0F0D && cp <= 0x0F12) return 1;
    if (cp >= 0x0F14 && cp <= 0x0F14) return 1;
    if (cp >= 0x0F3A && cp <= 0x0F3D) return 1;
    if (cp >= 0x0F85 && cp <= 0x0F85) return 1;
    if (cp >= 0x0FD0 && cp <= 0x0FD4) return 1;
    if (cp >= 0x0FD9 && cp <= 0x0FDA) return 1;
    if (cp >= 0x0FFF && cp <= 0x0FFF) return 1;
    if (cp >= 0x1000 && cp <= 0x109F) return 0;
    if (cp >= 0x10A0 && cp <= 0x10FF) return 0;
    if (cp >= 0x104A && cp <= 0x104F) return 1;
    if (cp >= 0x10FB && cp <= 0x10FB) return 1;
    if (cp >= 0x1360 && cp <= 0x1368) return 1;
    if (cp >= 0x1400 && cp <= 0x1400) return 1;
    if (cp >= 0x166D && cp <= 0x166E) return 1;
    if (cp >= 0x169B && cp <= 0x169C) return 1;
    if (cp >= 0x16EB && cp <= 0x16ED) return 1;
    if (cp >= 0x1735 && cp <= 0x1736) return 1;
    if (cp >= 0x17D4 && cp <= 0x17D6) return 1;
    if (cp >= 0x17D8 && cp <= 0x17DB) return 1;
    if (cp >= 0x17F0 && cp <= 0x17F9) return 1;
    if (cp >= 0x1800 && cp <= 0x180A) return 1;
    if (cp >= 0x1944 && cp <= 0x1945) return 1;
    if (cp >= 0x19DE && cp <= 0x19DF) return 1;
    if (cp >= 0x1A1E && cp <= 0x1A1F) return 1;
    if (cp >= 0x1AA0 && cp <= 0x1AA6) return 1;
    if (cp >= 0x1AA8 && cp <= 0x1AAD) return 1;
    if (cp >= 0x1B5A && cp <= 0x1B60) return 1;
    if (cp >= 0x1BFC && cp <= 0x1BFF) return 1;
    if (cp >= 0x1C3B && cp <= 0x1C3F) return 1;
    if (cp >= 0x1C7E && cp <= 0x1C7F) return 1;
    if (cp >= 0x1CC0 && cp <= 0x1CC7) return 1;
    if (cp >= 0x1CD0 && cp <= 0x1CE8) return 0;
    if (cp >= 0x1CE9 && cp <= 0x1CFF) return 0;
    if (cp >= 0x2000 && cp <= 0x206F) return 1;
    if (cp >= 0x2070 && cp <= 0x207F) return 1;
    if (cp >= 0x2080 && cp <= 0x208F) return 1;
    if (cp >= 0x2090 && cp <= 0x209C) return 0;
    if (cp >= 0x20A0 && cp <= 0x20CF) return 1;
    if (cp >= 0x20D0 && cp <= 0x20FF) return 0;
    if (cp >= 0x2100 && cp <= 0x214F) return 1;
    if (cp >= 0x2150 && cp <= 0x218F) return 1;
    if (cp >= 0x2190 && cp <= 0x21FF) return 1;
    if (cp >= 0x2200 && cp <= 0x22FF) return 1;
    if (cp >= 0x2300 && cp <= 0x23FF) return 1;
    if (cp >= 0x2400 && cp <= 0x243F) return 1;
    if (cp >= 0x2440 && cp <= 0x245F) return 1;
    if (cp >= 0x2460 && cp <= 0x24FF) return 1;
    if (cp >= 0x2500 && cp <= 0x257F) return 1;
    if (cp >= 0x2580 && cp <= 0x259F) return 1;
    if (cp >= 0x25A0 && cp <= 0x25FF) return 1;
    if (cp >= 0x2600 && cp <= 0x26FF) return 1;
    if (cp >= 0x2700 && cp <= 0x27BF) return 1;
    if (cp >= 0x27C0 && cp <= 0x27EF) return 1;
    if (cp >= 0x27F0 && cp <= 0x27FF) return 1;
    if (cp >= 0x2800 && cp <= 0x28FF) return 1;
    if (cp >= 0x2900 && cp <= 0x29FF) return 1;
    if (cp >= 0x2A00 && cp <= 0x2AFF) return 1;
    if (cp >= 0x2B00 && cp <= 0x2BFF) return 1;
    if (cp >= 0x2C00 && cp <= 0x2C5F) return 0;
    if (cp >= 0x2E00 && cp <= 0x2E7F) return 1;
    if (cp >= 0x2E80 && cp <= 0x2EFF) return 0;
    if (cp >= 0x2FF0 && cp <= 0x2FFF) return 1;
    if (cp >= 0x3001 && cp <= 0x3003) return 1;
    if (cp >= 0x3008 && cp <= 0x3011) return 1;
    if (cp >= 0x3014 && cp <= 0x301F) return 1;
    if (cp >= 0x3020 && cp <= 0x3020) return 1;
    if (cp >= 0x3030 && cp <= 0x3030) return 1;
    if (cp >= 0x303D && cp <= 0x303D) return 1;
    if (cp >= 0x309B && cp <= 0x309C) return 1;
    if (cp >= 0x30A0 && cp <= 0x30A0) return 1;
    if (cp >= 0x30FB && cp <= 0x30FB) return 1;
    if (cp >= 0x3190 && cp <= 0x319F) return 0;
    if (cp >= 0x31C0 && cp <= 0x31EF) return 0;
    if (cp >= 0x3200 && cp <= 0x32FF) return 0;
    if (cp >= 0x3300 && cp <= 0x33FF) return 0;
    if (cp >= 0xA000 && cp <= 0xA48F) return 0;
    if (cp >= 0xA490 && cp <= 0xA4CF) return 1;
    if (cp >= 0xA4D0 && cp <= 0xA4FF) return 0;
    if (cp >= 0xA500 && cp <= 0xA63F) return 0;
    if (cp >= 0xA673 && cp <= 0xA673) return 1;
    if (cp >= 0xA67E && cp <= 0xA67E) return 1;
    if (cp >= 0xA700 && cp <= 0xA71F) return 0;
    if (cp >= 0xA720 && cp <= 0xA7FF) return 0;
    if (cp >= 0xA830 && cp <= 0xA835) return 1;
    if (cp >= 0xA836 && cp <= 0xA837) return 1;
    if (cp >= 0xA838 && cp <= 0xA838) return 1;
    if (cp >= 0xA839 && cp <= 0xA839) return 1;
    if (cp >= 0xA874 && cp <= 0xA877) return 1;
    if (cp >= 0xA8CE && cp <= 0xA8CF) return 1;
    if (cp >= 0xA8F8 && cp <= 0xA8FA) return 1;
    if (cp >= 0xA92E && cp <= 0xA92F) return 1;
    if (cp >= 0xA95F && cp <= 0xA95F) return 1;
    if (cp >= 0xA9C1 && cp <= 0xA9CD) return 1;
    if (cp >= 0xA9DE && cp <= 0xA9DF) return 1;
    if (cp >= 0xAA5C && cp <= 0xAA5F) return 1;
    if (cp >= 0xAADE && cp <= 0xAADF) return 1;
    if (cp >= 0xAAF0 && cp <= 0xAAF1) return 1;
    if (cp >= 0xABEB && cp <= 0xABEB) return 1;
    if (cp >= 0xD800 && cp <= 0xDFFF) return 0;
    if (cp >= 0xF000 && cp <= 0xFFFF) {
        if (cp >= 0xFD3E && cp <= 0xFD3F) return 1;
        if (cp >= 0xFDCF && cp <= 0xFDCF) return 1;
        if (cp >= 0xFDD0 && cp <= 0xFDEF) return 0;
        if (cp >= 0xFDFD && cp <= 0xFDFD) return 1;
        if (cp >= 0xFE10 && cp <= 0xFE19) return 1;
        if (cp >= 0xFE30 && cp <= 0xFE4F) return 1;
        if (cp >= 0xFE50 && cp <= 0xFE6F) return 1;
        if (cp >= 0xFF00 && cp <= 0xFFEF) {
            if (cp >= 0xFF01 && cp <= 0xFF0F) return 1;
            if (cp >= 0xFF1A && cp <= 0xFF20) return 1;
            if (cp >= 0xFF3B && cp <= 0xFF40) return 1;
            if (cp >= 0xFF5B && cp <= 0xFF65) return 1;
            return 0;
        }
        if (cp >= 0xFFF0 && cp <= 0xFFFF) return 1;
    }
    if (cp >= 0x10000 && cp <= 0x100FF) return 0;
    if (cp >= 0x10100 && cp <= 0x10102) return 1;
    if (cp >= 0x10107 && cp <= 0x10133) return 0;
    if (cp >= 0x10137 && cp <= 0x1013F) return 1;
    if (cp >= 0x10190 && cp <= 0x1019B) return 0;
    if (cp >= 0x101D0 && cp <= 0x101FC) return 0;
    if (cp >= 0x102E0 && cp <= 0x102FB) return 0;
    if (cp >= 0x10300 && cp <= 0x10323) return 0;
    if (cp >= 0x10330 && cp <= 0x1034A) return 0;
    if (cp >= 0x10350 && cp <= 0x1037A) return 0;
    if (cp >= 0x10380 && cp <= 0x1039D) return 0;
    if (cp >= 0x1039F && cp <= 0x1039F) return 1;
    if (cp >= 0x103A0 && cp <= 0x103C3) return 0;
    if (cp >= 0x103C8 && cp <= 0x103CF) return 0;
    if (cp >= 0x103D0 && cp <= 0x103D0) return 1;
    if (cp >= 0x103D1 && cp <= 0x103D5) return 0;
    if (cp >= 0x10400 && cp <= 0x1044F) return 0;
    if (cp >= 0x10450 && cp <= 0x1049D) return 0;
    if (cp >= 0x104A0 && cp <= 0x104A9) return 0;
    if (cp >= 0x104B0 && cp <= 0x104D3) return 0;
    if (cp >= 0x104D8 && cp <= 0x104FB) return 0;
    if (cp >= 0x10500 && cp <= 0x10527) return 0;
    if (cp >= 0x10530 && cp <= 0x10563) return 0;
    if (cp >= 0x1056F && cp <= 0x1056F) return 1;
    if (cp >= 0x10600 && cp <= 0x10736) return 0;
    if (cp >= 0x10740 && cp <= 0x10755) return 0;
    if (cp >= 0x10760 && cp <= 0x10767) return 0;
    if (cp >= 0x10800 && cp <= 0x10805) return 0;
    if (cp >= 0x10808 && cp <= 0x10808) return 0;
    if (cp >= 0x1080A && cp <= 0x10835) return 0;
    if (cp >= 0x10837 && cp <= 0x10838) return 0;
    if (cp >= 0x1083C && cp <= 0x1083C) return 0;
    if (cp >= 0x1083F && cp <= 0x10855) return 0;
    if (cp >= 0x10857 && cp <= 0x1085F) return 1;
    if (cp >= 0x10860 && cp <= 0x10876) return 0;
    if (cp >= 0x10880 && cp <= 0x1089E) return 0;
    if (cp >= 0x108E0 && cp <= 0x108F2) return 0;
    if (cp >= 0x108F4 && cp <= 0x108F5) return 0;
    if (cp >= 0x108FB && cp <= 0x108FF) return 1;
    if (cp >= 0x10900 && cp <= 0x10915) return 0;
    if (cp >= 0x10916 && cp <= 0x1091B) return 0;
    if (cp >= 0x1091F && cp <= 0x1091F) return 1;
    if (cp >= 0x10920 && cp <= 0x10939) return 0;
    if (cp >= 0x1093F && cp <= 0x1093F) return 1;
    if (cp >= 0x10980 && cp <= 0x109B7) return 0;
    if (cp >= 0x109BC && cp <= 0x109CF) return 1;
    if (cp >= 0x109D2 && cp <= 0x109FF) return 1;
    if (cp >= 0x10A00 && cp <= 0x10A03) return 0;
    if (cp >= 0x10A05 && cp <= 0x10A06) return 0;
    if (cp >= 0x10A0C && cp <= 0x10A13) return 0;
    if (cp >= 0x10A15 && cp <= 0x10A17) return 0;
    if (cp >= 0x10A19 && cp <= 0x10A35) return 0;
    if (cp >= 0x10A38 && cp <= 0x10A3A) return 0;
    if (cp >= 0x10A3F && cp <= 0x10A3F) return 1;
    if (cp >= 0x10A40 && cp <= 0x10A48) return 0;
    if (cp >= 0x10A50 && cp <= 0x10A58) return 1;
    if (cp >= 0x10A60 && cp <= 0x10A7C) return 0;
    if (cp >= 0x10A7D && cp <= 0x10A7E) return 1;
    if (cp >= 0x10A80 && cp <= 0x10A9C) return 0;
    if (cp >= 0x10A9D && cp <= 0x10A9F) return 1;
    if (cp >= 0x10AC0 && cp <= 0x10AC7) return 0;
    if (cp >= 0x10AC8 && cp <= 0x10ACE) return 1;
    if (cp >= 0x10AD0 && cp <= 0x10AE6) return 0;
    if (cp >= 0x10AEB && cp <= 0x10AF6) return 1;
    if (cp >= 0x10B00 && cp <= 0x10B35) return 0;
    if (cp >= 0x10B39 && cp <= 0x10B3F) return 1;
    if (cp >= 0x10B40 && cp <= 0x10B55) return 0;
    if (cp >= 0x10B58 && cp <= 0x10B5F) return 1;
    if (cp >= 0x10B60 && cp <= 0x10B72) return 0;
    if (cp >= 0x10B78 && cp <= 0x10B7F) return 1;
    if (cp >= 0x10B80 && cp <= 0x10B91) return 0;
    if (cp >= 0x10B99 && cp <= 0x10B9C) return 1;
    if (cp >= 0x10BA9 && cp <= 0x10BAF) return 1;
    if (cp >= 0x10C00 && cp <= 0x10C48) return 0;
    if (cp >= 0x10C80 && cp <= 0x10CB2) return 0;
    if (cp >= 0x10CC0 && cp <= 0x10CF2) return 0;
    if (cp >= 0x10CFA && cp <= 0x10CFF) return 1;
    if (cp >= 0x10D00 && cp <= 0x10D27) return 0;
    if (cp >= 0x10D30 && cp <= 0x10D39) return 0;
    if (cp >= 0x10E60 && cp <= 0x10E7E) return 1;
    if (cp >= 0x10E80 && cp <= 0x10EA9) return 0;
    if (cp >= 0x10EAD && cp <= 0x10EAD) return 1;
    if (cp >= 0x10EB0 && cp <= 0x10EB1) return 0;
    if (cp >= 0x10F00 && cp <= 0x10F27) return 0;
    if (cp >= 0x10F30 && cp <= 0x10F45) return 0;
    if (cp >= 0x10F51 && cp <= 0x10F54) return 1;
    if (cp >= 0x10F55 && cp <= 0x10F59) return 0;
    if (cp >= 0x10F70 && cp <= 0x10F81) return 0;
    if (cp >= 0x10F86 && cp <= 0x10F89) return 1;
    if (cp >= 0x10FB0 && cp <= 0x10FC4) return 0;
    if (cp >= 0x10FC5 && cp <= 0x10FCB) return 1;
    if (cp >= 0x11000 && cp <= 0x1104D) return 0;
    if (cp >= 0x11052 && cp <= 0x11065) return 0;
    if (cp >= 0x11066 && cp <= 0x1106F) return 0;
    if (cp >= 0x1107F && cp <= 0x11081) return 0;
    if (cp >= 0x1109A && cp <= 0x110B9) return 0;
    if (cp >= 0x110BB && cp <= 0x110BC) return 1;
    if (cp >= 0x110BD && cp <= 0x110BD) return 1;
    if (cp >= 0x110BE && cp <= 0x110C1) return 1;
    if (cp >= 0x110CD && cp <= 0x110CD) return 1;
    if (cp >= 0x110D0 && cp <= 0x110E8) return 0;
    if (cp >= 0x110F0 && cp <= 0x110F9) return 0;
    if (cp >= 0x11100 && cp <= 0x11132) return 0;
    if (cp >= 0x11133 && cp <= 0x11134) return 0;
    if (cp >= 0x11136 && cp <= 0x1113F) return 0;
    if (cp >= 0x11140 && cp <= 0x11143) return 1;
    if (cp >= 0x11144 && cp <= 0x11147) return 0;
    if (cp >= 0x11150 && cp <= 0x11172) return 0;
    if (cp >= 0x11173 && cp <= 0x11173) return 1;
    if (cp >= 0x11174 && cp <= 0x11175) return 0;
    if (cp >= 0x11176 && cp <= 0x11176) return 1;
    if (cp >= 0x11180 && cp <= 0x111B2) return 0;
    if (cp >= 0x111B3 && cp <= 0x111C0) return 0;
    if (cp >= 0x111C1 && cp <= 0x111C4) return 0;
    if (cp >= 0x111C5 && cp <= 0x111C8) return 1;
    if (cp >= 0x111C9 && cp <= 0x111CC) return 0;
    if (cp >= 0x111CD && cp <= 0x111CD) return 1;
    if (cp >= 0x111CE && cp <= 0x111CF) return 0;
    if (cp >= 0x111D0 && cp <= 0x111D9) return 0;
    if (cp >= 0x111DA && cp <= 0x111DB) return 0;
    if (cp >= 0x111DC && cp <= 0x111DC) return 1;
    if (cp >= 0x111DE && cp <= 0x111DF) return 1;
    if (cp >= 0x111E1 && cp <= 0x111F4) return 0;
    if (cp >= 0x11200 && cp <= 0x11211) return 0;
    if (cp >= 0x11213 && cp <= 0x1122E) return 0;
    if (cp >= 0x1122F && cp <= 0x11234) return 0;
    if (cp >= 0x11235 && cp <= 0x11235) return 1;
    if (cp >= 0x11236 && cp <= 0x11237) return 1;
    if (cp >= 0x11238 && cp <= 0x1123D) return 1;
    if (cp >= 0x1123E && cp <= 0x1123E) return 0;
    if (cp >= 0x11280 && cp <= 0x11286) return 0;
    if (cp >= 0x11288 && cp <= 0x11288) return 0;
    if (cp >= 0x1128A && cp <= 0x1128D) return 0;
    if (cp >= 0x1128F && cp <= 0x1129D) return 0;
    if (cp >= 0x1129F && cp <= 0x112A8) return 0;
    if (cp >= 0x112A9 && cp <= 0x112A9) return 1;
    if (cp >= 0x112B0 && cp <= 0x112DE) return 0;
    if (cp >= 0x112DF && cp <= 0x112E8) return 0;
    if (cp >= 0x112E9 && cp <= 0x112EA) return 0;
    if (cp >= 0x112F0 && cp <= 0x112F9) return 0;
    if (cp >= 0x11300 && cp <= 0x11303) return 0;
    if (cp >= 0x11305 && cp <= 0x1130C) return 0;
    if (cp >= 0x1130F && cp <= 0x11310) return 0;
    if (cp >= 0x11313 && cp <= 0x11328) return 0;
    if (cp >= 0x1132A && cp <= 0x11330) return 0;
    if (cp >= 0x11332 && cp <= 0x11333) return 0;
    if (cp >= 0x11335 && cp <= 0x11339) return 0;
    if (cp >= 0x1133B && cp <= 0x1133C) return 0;
    if (cp >= 0x1133D && cp <= 0x11344) return 0;
    if (cp >= 0x11347 && cp <= 0x11348) return 0;
    if (cp >= 0x1134B && cp <= 0x1134D) return 0;
    if (cp >= 0x11350 && cp <= 0x11350) return 0;
    if (cp >= 0x11357 && cp <= 0x11357) return 0;
    if (cp >= 0x1135D && cp <= 0x11363) return 0;
    if (cp >= 0x11366 && cp <= 0x1136C) return 0;
    if (cp >= 0x11370 && cp <= 0x11374) return 0;
    if (cp >= 0x11400 && cp <= 0x11434) return 0;
    if (cp >= 0x11435 && cp <= 0x11441) return 0; /* likely modifiers */
    if (cp >= 0x11442 && cp <= 0x11444) return 1;
    if (cp >= 0x11445 && cp <= 0x11445) return 0;
    if (cp >= 0x11446 && cp <= 0x11446) return 0;
    if (cp >= 0x11447 && cp <= 0x1144A) return 1;
    if (cp >= 0x11450 && cp <= 0x11459) return 0;
    if (cp >= 0x1145B && cp <= 0x1145B) return 1;
    if (cp >= 0x1145D && cp <= 0x1145D) return 1;
    if (cp >= 0x1145E && cp <= 0x1145E) return 0;
    if (cp >= 0x1145F && cp <= 0x11461) return 0;
    if (cp >= 0x11480 && cp <= 0x114AF) return 0;
    if (cp >= 0x114B0 && cp <= 0x114C1) return 0;
    if (cp >= 0x114C2 && cp <= 0x114C3) return 1;
    if (cp >= 0x114C4 && cp <= 0x114C5) return 0;
    if (cp >= 0x114C6 && cp <= 0x114C6) return 1;
    if (cp >= 0x114C7 && cp <= 0x114C7) return 0;
    if (cp >= 0x114D0 && cp <= 0x114D9) return 0;
    if (cp >= 0x11580 && cp <= 0x115AE) return 0;
    if (cp >= 0x115AF && cp <= 0x115B5) return 0;
    if (cp >= 0x115B8 && cp <= 0x115BC) return 0;
    if (cp >= 0x115BE && cp <= 0x115BE) return 1;
    if (cp >= 0x115BF && cp <= 0x115C0) return 0;
    if (cp >= 0x115C1 && cp <= 0x115D7) return 1;
    if (cp >= 0x115D8 && cp <= 0x115DB) return 0;
    if (cp >= 0x115DC && cp <= 0x115DD) return 0;
    if (cp >= 0x11600 && cp <= 0x1162F) return 0;
    if (cp >= 0x11630 && cp <= 0x11640) return 0;
    if (cp >= 0x11641 && cp <= 0x11643) return 1;
    if (cp >= 0x11644 && cp <= 0x11644) return 0;
    if (cp >= 0x11650 && cp <= 0x11659) return 0;
    if (cp >= 0x11660 && cp <= 0x1166C) return 1;
    if (cp >= 0x11680 && cp <= 0x116AA) return 0;
    if (cp >= 0x116AB && cp <= 0x116B5) return 0;
    if (cp >= 0x116B6 && cp <= 0x116B6) return 1;
    if (cp >= 0x116B7 && cp <= 0x116B7) return 0;
    if (cp >= 0x116B8 && cp <= 0x116B8) return 0;
    if (cp >= 0x116B9 && cp <= 0x116B9) return 1;
    if (cp >= 0x116C0 && cp <= 0x116C9) return 0;
    if (cp >= 0x11700 && cp <= 0x1171A) return 0;
    if (cp >= 0x1171D && cp <= 0x1172B) return 0;
    if (cp >= 0x11730 && cp <= 0x11739) return 0;
    if (cp >= 0x1173A && cp <= 0x1173B) return 1;
    if (cp >= 0x1173C && cp <= 0x1173E) return 1;
    if (cp >= 0x1173F && cp <= 0x1173F) return 1;
    if (cp >= 0x11740 && cp <= 0x11746) return 0;
    if (cp >= 0x11800 && cp <= 0x1182B) return 0;
    if (cp >= 0x1182C && cp <= 0x11837) return 0;
    if (cp >= 0x11838 && cp <= 0x11838) return 1;
    if (cp >= 0x11839 && cp <= 0x1183A) return 1;
    if (cp >= 0x1183B && cp <= 0x1183B) return 1;
    if (cp >= 0x118A0 && cp <= 0x118DF) return 0;
    if (cp >= 0x118E0 && cp <= 0x118E9) return 0;
    if (cp >= 0x118EA && cp <= 0x118F2) return 1;
    if (cp >= 0x118FF && cp <= 0x118FF) return 1;
    if (cp >= 0x11900 && cp <= 0x11906) return 0;
    if (cp >= 0x11909 && cp <= 0x11909) return 0;
    if (cp >= 0x1190C && cp <= 0x11913) return 0;
    if (cp >= 0x11915 && cp <= 0x11916) return 0;
    if (cp >= 0x11918 && cp <= 0x1192F) return 0;
    if (cp >= 0x11930 && cp <= 0x11935) return 0;
    if (cp >= 0x11937 && cp <= 0x11938) return 0;
    if (cp >= 0x1193B && cp <= 0x1193C) return 0;
    if (cp >= 0x1193D && cp <= 0x1193D) return 1;
    if (cp >= 0x1193E && cp <= 0x1193E) return 0;
    if (cp >= 0x1193F && cp <= 0x11942) return 0;
    if (cp >= 0x11943 && cp <= 0x11943) return 1;
    if (cp >= 0x11944 && cp <= 0x11946) return 1;
    if (cp >= 0x11950 && cp <= 0x11959) return 0;
    if (cp >= 0x119A0 && cp <= 0x119A7) return 0;
    if (cp >= 0x119AA && cp <= 0x119D0) return 0;
    if (cp >= 0x119D1 && cp <= 0x119D7) return 0;
    if (cp >= 0x119DA && cp <= 0x119DF) return 0;
    if (cp >= 0x119E0 && cp <= 0x119E0) return 1;
    if (cp >= 0x119E1 && cp <= 0x119E1) return 0;
    if (cp >= 0x119E2 && cp <= 0x119E3) return 1;
    if (cp >= 0x119E4 && cp <= 0x119E4) return 0;
    if (cp >= 0x11A00 && cp <= 0x11A00) return 0;
    if (cp >= 0x11A01 && cp <= 0x11A0A) return 0;
    if (cp >= 0x11A0B && cp <= 0x11A32) return 0;
    if (cp >= 0x11A33 && cp <= 0x11A38) return 0;
    if (cp >= 0x11A39 && cp <= 0x11A39) return 1;
    if (cp >= 0x11A3A && cp <= 0x11A3A) return 0;
    if (cp >= 0x11A3B && cp <= 0x11A3E) return 1;
    if (cp >= 0x11A3F && cp <= 0x11A46) return 0;
    if (cp >= 0x11A47 && cp <= 0x11A47) return 1;
    if (cp >= 0x11A50 && cp <= 0x11A50) return 0;
    if (cp >= 0x11A51 && cp <= 0x11A56) return 0;
    if (cp >= 0x11A57 && cp <= 0x11A58) return 1;
    if (cp >= 0x11A59 && cp <= 0x11A5B) return 0;
    if (cp >= 0x11A5C && cp <= 0x11A89) return 0;
    if (cp >= 0x11A8A && cp <= 0x11A96) return 0;
    if (cp >= 0x11A97 && cp <= 0x11A97) return 1;
    if (cp >= 0x11A98 && cp <= 0x11A99) return 0;
    if (cp >= 0x11A9A && cp <= 0x11A9C) return 1;
    if (cp >= 0x11A9D && cp <= 0x11A9D) return 0;
    if (cp >= 0x11A9E && cp <= 0x11AA0) return 1;
    if (cp >= 0x11AA1 && cp <= 0x11AAF) return 0;
    if (cp >= 0x11AB0 && cp <= 0x11AB0) return 1;
    if (cp >= 0x11AB1 && cp <= 0x11B00) return 0;
    if (cp >= 0x11B00 && cp <= 0x11B09) return 1;
    if (cp >= 0x11C00 && cp <= 0x11C08) return 0;
    if (cp >= 0x11C0A && cp <= 0x11C2E) return 0;
    if (cp >= 0x11C2F && cp <= 0x11C36) return 0;
    if (cp >= 0x11C38 && cp <= 0x11C3D) return 0;
    if (cp >= 0x11C3E && cp <= 0x11C45) return 0;
    if (cp >= 0x11C50 && cp <= 0x11C59) return 0;
    if (cp >= 0x11C5A && cp <= 0x11C6C) return 1;
    if (cp >= 0x11C70 && cp <= 0x11C71) return 1;
    if (cp >= 0x11C72 && cp <= 0x11C8F) return 0;
    if (cp >= 0x11C92 && cp <= 0x11CA7) return 0;
    if (cp >= 0x11CA9 && cp <= 0x11CB0) return 0;
    if (cp >= 0x11CB1 && cp <= 0x11CB1) return 1;
    if (cp >= 0x11CB2 && cp <= 0x11CB3) return 0;
    if (cp >= 0x11CB4 && cp <= 0x11CB4) return 1;
    if (cp >= 0x11CB5 && cp <= 0x11CB6) return 0;
    if (cp >= 0x11D00 && cp <= 0x11D06) return 0;
    if (cp >= 0x11D08 && cp <= 0x11D09) return 0;
    if (cp >= 0x11D0B && cp <= 0x11D30) return 0;
    if (cp >= 0x11D31 && cp <= 0x11D36) return 0;
    if (cp >= 0x11D3A && cp <= 0x11D3A) return 0;
    if (cp >= 0x11D3C && cp <= 0x11D3D) return 0;
    if (cp >= 0x11D3F && cp <= 0x11D45) return 0;
    if (cp >= 0x11D46 && cp <= 0x11D46) return 1;
    if (cp >= 0x11D47 && cp <= 0x11D47) return 0;
    if (cp >= 0x11D50 && cp <= 0x11D59) return 0;
    if (cp >= 0x11D60 && cp <= 0x11D65) return 0;
    if (cp >= 0x11D67 && cp <= 0x11D68) return 0;
    if (cp >= 0x11D6A && cp <= 0x11D89) return 0;
    if (cp >= 0x11D8A && cp <= 0x11D8E) return 0;
    if (cp >= 0x11D90 && cp <= 0x11D91) return 0;
    if (cp >= 0x11D93 && cp <= 0x11D96) return 0;
    if (cp >= 0x11D97 && cp <= 0x11D97) return 1;
    if (cp >= 0x11D98 && cp <= 0x11D98) return 0;
    if (cp >= 0x11DA0 && cp <= 0x11DA9) return 0;
    if (cp >= 0x11EE0 && cp <= 0x11EF2) return 0;
    if (cp >= 0x11EF3 && cp <= 0x11EF4) return 1;
    if (cp >= 0x11EF5 && cp <= 0x11EF8) return 0;
    if (cp >= 0x11FC0 && cp <= 0x11FD4) return 1;
    if (cp >= 0x11FD5 && cp <= 0x11FDC) return 0;
    if (cp >= 0x11FDD && cp <= 0x11FE0) return 1;
    if (cp >= 0x11FE1 && cp <= 0x11FF1) return 0;
    if (cp >= 0x11FFF && cp <= 0x11FFF) return 1;
    if (cp >= 0x12000 && cp <= 0x123FF) return 0;
    if (cp >= 0x12400 && cp <= 0x1246E) return 0;
    if (cp >= 0x12470 && cp <= 0x12474) return 1;
    if (cp >= 0x12480 && cp <= 0x12543) return 0;
    if (cp >= 0x12F90 && cp <= 0x12FF0) return 0;
    if (cp >= 0x12FF1 && cp <= 0x12FF2) return 1;
    if (cp >= 0x13000 && cp <= 0x1342E) return 0;
    if (cp >= 0x13430 && cp <= 0x13438) return 0;
    if (cp >= 0x14400 && cp <= 0x14646) return 0;
    if (cp >= 0x16800 && cp <= 0x16A38) return 0;
    if (cp >= 0x16A40 && cp <= 0x16A5E) return 0;
    if (cp >= 0x16A60 && cp <= 0x16A69) return 0;
    if (cp >= 0x16A6E && cp <= 0x16A6F) return 1;
    if (cp >= 0x16A70 && cp <= 0x16ABE) return 0;
    if (cp >= 0x16AC0 && cp <= 0x16AC9) return 0;
    if (cp >= 0x16AD0 && cp <= 0x16AED) return 0;
    if (cp >= 0x16AF0 && cp <= 0x16AF4) return 0;
    if (cp >= 0x16AF5 && cp <= 0x16AF5) return 1;
    if (cp >= 0x16B00 && cp <= 0x16B2F) return 0;
    if (cp >= 0x16B30 && cp <= 0x16B36) return 0;
    if (cp >= 0x16B37 && cp <= 0x16B3B) return 1;
    if (cp >= 0x16B3C && cp <= 0x16B3F) return 0;
    if (cp >= 0x16B40 && cp <= 0x16B43) return 0;
    if (cp >= 0x16B44 && cp <= 0x16B44) return 1;
    if (cp >= 0x16B45 && cp <= 0x16B45) return 1;
    if (cp >= 0x16B50 && cp <= 0x16B59) return 0;
    if (cp >= 0x16B5B && cp <= 0x16B5B) return 1;
    if (cp >= 0x16B63 && cp <= 0x16B77) return 0;
    if (cp >= 0x16B7D && cp <= 0x16B8F) return 0;
    if (cp >= 0x16E40 && cp <= 0x16E7F) return 0;
    if (cp >= 0x16E80 && cp <= 0x16E96) return 1;
    if (cp >= 0x16E97 && cp <= 0x16E9A) return 1;
    if (cp >= 0x16F00 && cp <= 0x16F4A) return 0;
    if (cp >= 0x16F4F && cp <= 0x16F4F) return 0;
    if (cp >= 0x16F50 && cp <= 0x16F50) return 0;
    if (cp >= 0x16F51 && cp <= 0x16F87) return 0;
    if (cp >= 0x16F8F && cp <= 0x16F92) return 0;
    if (cp >= 0x16F93 && cp <= 0x16F9F) return 1;
    if (cp >= 0x16FE0 && cp <= 0x16FE1) return 0;
    if (cp >= 0x16FE2 && cp <= 0x16FE3) return 1;
    if (cp >= 0x16FE4 && cp <= 0x16FE4) return 0;
    if (cp >= 0x16FF0 && cp <= 0x16FF1) return 0;
    if (cp >= 0x17000 && cp <= 0x187F7) return 0;
    if (cp >= 0x18800 && cp <= 0x18AF2) return 0;
    if (cp >= 0x1B000 && cp <= 0x1B0FF) return 0;
    if (cp >= 0x1B100 && cp <= 0x1B122) return 0;
    if (cp >= 0x1B170 && cp <= 0x1B2FB) return 0;
    if (cp >= 0x1BC00 && cp <= 0x1BC6A) return 0;
    if (cp >= 0x1BC70 && cp <= 0x1BC7C) return 0;
    if (cp >= 0x1BC80 && cp <= 0x1BC88) return 0;
    if (cp >= 0x1BC90 && cp <= 0x1BC99) return 0;
    if (cp >= 0x1BC9C && cp <= 0x1BC9C) return 1;
    if (cp >= 0x1BC9D && cp <= 0x1BC9E) return 0;
    if (cp >= 0x1BC9F && cp <= 0x1BC9F) return 1;
    if (cp >= 0x1BCA0 && cp <= 0x1BCA3) return 0;
    if (cp >= 0x1D000 && cp <= 0x1D0FF) return 1;
    if (cp >= 0x1D100 && cp <= 0x1D126) return 1;
    if (cp >= 0x1D129 && cp <= 0x1D164) return 0;
    if (cp >= 0x1D165 && cp <= 0x1D169) return 0;
    if (cp >= 0x1D16A && cp <= 0x1D16C) return 0;
    if (cp >= 0x1D16D && cp <= 0x1D172) return 0;
    if (cp >= 0x1D173 && cp <= 0x1D182) return 0;
    if (cp >= 0x1D183 && cp <= 0x1D184) return 1;
    if (cp >= 0x1D185 && cp <= 0x1D18B) return 0;
    if (cp >= 0x1D18C && cp <= 0x1D1A9) return 0;
    if (cp >= 0x1D1AA && cp <= 0x1D1AD) return 0;
    if (cp >= 0x1D1AE && cp <= 0x1D1E8) return 1;
    if (cp >= 0x1D200 && cp <= 0x1D241) return 0;
    if (cp >= 0x1D242 && cp <= 0x1D244) return 0;
    if (cp >= 0x1D245 && cp <= 0x1D245) return 1;
    if (cp >= 0x1D300 && cp <= 0x1D356) return 1;
    if (cp >= 0x1D360 && cp <= 0x1D378) return 0;
    if (cp >= 0x1D400 && cp <= 0x1D454) return 0;
    if (cp >= 0x1D456 && cp <= 0x1D49C) return 0;
    if (cp >= 0x1D49E && cp <= 0x1D49F) return 0;
    if (cp >= 0x1D4A2 && cp <= 0x1D4A2) return 0;
    if (cp >= 0x1D4A5 && cp <= 0x1D4A6) return 0;
    if (cp >= 0x1D4A9 && cp <= 0x1D4AC) return 0;
    if (cp >= 0x1D4AE && cp <= 0x1D4B9) return 0;
    if (cp >= 0x1D4BB && cp <= 0x1D4BB) return 0;
    if (cp >= 0x1D4BD && cp <= 0x1D4C3) return 0;
    if (cp >= 0x1D4C5 && cp <= 0x1D505) return 0;
    if (cp >= 0x1D507 && cp <= 0x1D50A) return 0;
    if (cp >= 0x1D50D && cp <= 0x1D514) return 0;
    if (cp >= 0x1D516 && cp <= 0x1D51C) return 0;
    if (cp >= 0x1D51E && cp <= 0x1D539) return 0;
    if (cp >= 0x1D53B && cp <= 0x1D53E) return 0;
    if (cp >= 0x1D540 && cp <= 0x1D544) return 0;
    if (cp >= 0x1D546 && cp <= 0x1D546) return 0;
    if (cp >= 0x1D54A && cp <= 0x1D550) return 0;
    if (cp >= 0x1D552 && cp <= 0x1D6A5) return 0;
    if (cp >= 0x1D6A8 && cp <= 0x1D6C0) return 0;
    if (cp >= 0x1D6C1 && cp <= 0x1D6C1) return 1;
    if (cp >= 0x1D6C2 && cp <= 0x1D6DA) return 0;
    if (cp >= 0x1D6DB && cp <= 0x1D6DB) return 1;
    if (cp >= 0x1D6DC && cp <= 0x1D6FA) return 0;
    if (cp >= 0x1D6FB && cp <= 0x1D6FB) return 1;
    if (cp >= 0x1D6FC && cp <= 0x1D714) return 0;
    if (cp >= 0x1D715 && cp <= 0x1D715) return 1;
    if (cp >= 0x1D716 && cp <= 0x1D734) return 0;
    if (cp >= 0x1D735 && cp <= 0x1D735) return 1;
    if (cp >= 0x1D736 && cp <= 0x1D74E) return 0;
    if (cp >= 0x1D74F && cp <= 0x1D74F) return 1;
    if (cp >= 0x1D750 && cp <= 0x1D76E) return 0;
    if (cp >= 0x1D76F && cp <= 0x1D76F) return 1;
    if (cp >= 0x1D770 && cp <= 0x1D788) return 0;
    if (cp >= 0x1D789 && cp <= 0x1D789) return 1;
    if (cp >= 0x1D78A && cp <= 0x1D7A8) return 0;
    if (cp >= 0x1D7A9 && cp <= 0x1D7A9) return 1;
    if (cp >= 0x1D7AA && cp <= 0x1D7C2) return 0;
    if (cp >= 0x1D7C3 && cp <= 0x1D7C3) return 1;
    if (cp >= 0x1D7C4 && cp <= 0x1D7CB) return 0;
    if (cp >= 0x1D7CE && cp <= 0x1D7FF) return 0;
    if (cp >= 0x1D800 && cp <= 0x1D9FF) return 0;
    if (cp >= 0x1DA00 && cp <= 0x1DA36) return 0;
    if (cp >= 0x1DA37 && cp <= 0x1DA3A) return 0;
    if (cp >= 0x1DA3B && cp <= 0x1DA6C) return 0;
    if (cp >= 0x1DA6D && cp <= 0x1DA74) return 0;
    if (cp >= 0x1DA75 && cp <= 0x1DA75) return 0;
    if (cp >= 0x1DA76 && cp <= 0x1DA83) return 0;
    if (cp >= 0x1DA84 && cp <= 0x1DA84) return 0;
    if (cp >= 0x1DA85 && cp <= 0x1DA86) return 0;
    if (cp >= 0x1DA87 && cp <= 0x1DA8B) return 1;
    if (cp >= 0x1DA9B && cp <= 0x1DA9F) return 0;
    if (cp >= 0x1DAA1 && cp <= 0x1DAAF) return 0;
    if (cp >= 0x1E000 && cp <= 0x1E006) return 0;
    if (cp >= 0x1E008 && cp <= 0x1E018) return 0;
    if (cp >= 0x1E01B && cp <= 0x1E021) return 0;
    if (cp >= 0x1E023 && cp <= 0x1E024) return 0;
    if (cp >= 0x1E026 && cp <= 0x1E02A) return 0;
    if (cp >= 0x1E100 && cp <= 0x1E12C) return 0;
    if (cp >= 0x1E130 && cp <= 0x1E136) return 0;
    if (cp >= 0x1E137 && cp <= 0x1E13D) return 0;
    if (cp >= 0x1E140 && cp <= 0x1E149) return 0;
    if (cp >= 0x1E14E && cp <= 0x1E14E) return 1;
    if (cp >= 0x1E14F && cp <= 0x1E14F) return 0;
    if (cp >= 0x1E2C0 && cp <= 0x1E2EB) return 0; /* likely letters */
    if (cp >= 0x1E2EC && cp <= 0x1E2EF) return 0;
    if (cp >= 0x1E2F0 && cp <= 0x1E2F9) return 0;
    if (cp >= 0x1E2FF && cp <= 0x1E2FF) return 1;
    if (cp >= 0x1E800 && cp <= 0x1E8C4) return 0;
    if (cp >= 0x1E8C7 && cp <= 0x1E8CF) return 1;
    if (cp >= 0x1E8D0 && cp <= 0x1E8D6) return 0;
    if (cp >= 0x1E900 && cp <= 0x1E94A) return 0;
    if (cp >= 0x1E950 && cp <= 0x1E959) return 0;
    if (cp >= 0x1E95E && cp <= 0x1E95F) return 1;
    if (cp >= 0x1EC71 && cp <= 0x1ECAB) return 0;
    if (cp >= 0x1ECAC && cp <= 0x1ECAC) return 1;
    if (cp >= 0x1ECAD && cp <= 0x1ECAF) return 0;
    if (cp >= 0x1ECB0 && cp <= 0x1ECB0) return 1;
    if (cp >= 0x1ECB1 && cp <= 0x1ECB4) return 0;
    if (cp >= 0x1ED01 && cp <= 0x1ED2D) return 0;
    if (cp >= 0x1ED2E && cp <= 0x1ED2E) return 1;
    if (cp >= 0x1ED2F && cp <= 0x1ED3D) return 0;
    if (cp >= 0x1ED3F && cp <= 0x1ED3F) return 1;
    if (cp >= 0x1ED40 && cp <= 0x1ED48) return 0;
    if (cp >= 0x1EE00 && cp <= 0x1EE03) return 0;
    if (cp >= 0x1EE05 && cp <= 0x1EE1F) return 0;
    if (cp >= 0x1EE21 && cp <= 0x1EE22) return 0;
    if (cp >= 0x1EE24 && cp <= 0x1EE24) return 0;
    if (cp >= 0x1EE27 && cp <= 0x1EE27) return 0;
    if (cp >= 0x1EE29 && cp <= 0x1EE32) return 0;
    if (cp >= 0x1EE34 && cp <= 0x1EE37) return 0;
    if (cp >= 0x1EE39 && cp <= 0x1EE39) return 0;
    if (cp >= 0x1EE3B && cp <= 0x1EE3B) return 0;
    if (cp >= 0x1EE42 && cp <= 0x1EE42) return 0;
    if (cp >= 0x1EE47 && cp <= 0x1EE47) return 0;
    if (cp >= 0x1EE49 && cp <= 0x1EE49) return 0;
    if (cp >= 0x1EE4B && cp <= 0x1EE4B) return 0;
    if (cp >= 0x1EE4D && cp <= 0x1EE4F) return 0;
    if (cp >= 0x1EE51 && cp <= 0x1EE52) return 0;
    if (cp >= 0x1EE54 && cp <= 0x1EE54) return 0;
    if (cp >= 0x1EE57 && cp <= 0x1EE57) return 0;
    if (cp >= 0x1EE59 && cp <= 0x1EE59) return 0;
    if (cp >= 0x1EE5B && cp <= 0x1EE5B) return 0;
    if (cp >= 0x1EE5D && cp <= 0x1EE5D) return 0;
    if (cp >= 0x1EE5F && cp <= 0x1EE5F) return 0;
    if (cp >= 0x1EE61 && cp <= 0x1EE62) return 0;
    if (cp >= 0x1EE64 && cp <= 0x1EE64) return 0;
    if (cp >= 0x1EE67 && cp <= 0x1EE6A) return 0;
    if (cp >= 0x1EE6C && cp <= 0x1EE72) return 0;
    if (cp >= 0x1EE74 && cp <= 0x1EE77) return 0;
    if (cp >= 0x1EE79 && cp <= 0x1EE7C) return 0;
    if (cp >= 0x1EE7E && cp <= 0x1EE7E) return 0;
    if (cp >= 0x1EE80 && cp <= 0x1EE89) return 0;
    if (cp >= 0x1EE8B && cp <= 0x1EE9B) return 0;
    if (cp >= 0x1EEA1 && cp <= 0x1EEA3) return 0;
    if (cp >= 0x1EEA5 && cp <= 0x1EEA9) return 0;
    if (cp >= 0x1EEAB && cp <= 0x1EEBB) return 0;
    if (cp >= 0x1EEF0 && cp <= 0x1EEF1) return 1;
    if (cp >= 0x1F000 && cp <= 0x1F02F) return 1;
    if (cp >= 0x1F030 && cp <= 0x1F09F) return 1;
    if (cp >= 0x1F0A0 && cp <= 0x1F0FF) return 1;
    if (cp >= 0x1F100 && cp <= 0x1F1FF) return 1;
    if (cp >= 0x1F200 && cp <= 0x1F2FF) return 1;
    if (cp >= 0x1F300 && cp <= 0x1F5FF) return 1;
    if (cp >= 0x1F600 && cp <= 0x1F64F) return 1;
    if (cp >= 0x1F650 && cp <= 0x1F67F) return 1;
    if (cp >= 0x1F680 && cp <= 0x1F6FF) return 1;
    if (cp >= 0x1F700 && cp <= 0x1F77F) return 1;
    if (cp >= 0x1F780 && cp <= 0x1F7FF) return 1;
    if (cp >= 0x1F800 && cp <= 0x1F8FF) return 1;
    if (cp >= 0x1F900 && cp <= 0x1F9FF) return 1;
    if (cp >= 0x1FA00 && cp <= 0x1FA6F) return 1;
    if (cp >= 0x1FA70 && cp <= 0x1FAFF) return 1;
    if (cp >= 0x1FB00 && cp <= 0x1FBFF) return 1;
    if (cp >= 0xE0001 && cp <= 0xE007F) return 0;
    if (cp >= 0xE0100 && cp <= 0xE01EF) return 0;
    return 0;
}

int is_left_flanking(const char *text, size_t len, size_t pos) {
    if (pos >= len) return 0;
    if (pos + 1 >= len) return 0;
    char delim = text[pos];
    size_t run_end = pos + 1;
    while (run_end < len && text[run_end] == delim) run_end++;
    if (run_end >= len) return 0;
    if (is_next_unicode_ws(text, len, run_end)) return 0;
    int result = 0;
    if (!is_unicode_punct(text, len, run_end)) {
        result = 1;
    } else {
        if (pos == 0) result = 1;
        else {
            size_t prev_char = utf8_char_start(text, pos - 1);
            if (is_prev_unicode_ws(text, len, pos)) result = 1;
            else if (is_unicode_punct(text, len, prev_char)) result = 1;
        }
    }
    return result;
}

int is_right_flanking(const char *text, size_t len, size_t pos) {
    if (pos >= len) return 0;
    if (pos == 0) return 0;
    char delim = text[pos];
    size_t run_end = pos + 1;
    while (run_end < len && text[run_end] == delim) run_end++;
    size_t prev_char = utf8_char_start(text, pos - 1);
    if (is_prev_unicode_ws(text, len, pos)) return 0;
    int result = 0;
    if (!is_unicode_punct(text, len, prev_char)) {
        result = 1;
    } else {
        if (run_end >= len) result = 1;
        else if (is_next_unicode_ws(text, len, run_end)) result = 1;
        else if (is_unicode_punct(text, len, run_end)) result = 1;
    }
    return result;
}

int pos_inside_code_span(const char *text, size_t len, size_t pos) {
    size_t i = 0;
    while (i < len) {
        if (text[i] == '`') {
            int open_ticks = 0;
            size_t start = i;
            while (i < len && text[i] == '`') { open_ticks++; i++; }
            size_t search = i;
            while (search < len) {
                if (text[search] == '`') {
                    size_t k = search;
                    int close_ticks = 0;
                    while (k < len && text[k] == '`') { close_ticks++; k++; }
                    if (close_ticks == open_ticks) {
                        if (pos >= start && pos < k) return 1;
                        i = k; goto next_code_span;
                    }
                    search = k;
                } else { search++; }
            }
            if (pos >= start) return 0;
        } else { i++; }
        continue;
    next_code_span:;
    }
    return 0;
}

int parse_link_dest_title(const char *text, size_t content_start, size_t content_end, size_t *dest_start, size_t *dest_len, size_t *title_start, size_t *title_len) {
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
                if (pos + 1 < content_end && (text[pos + 1] == '\\' || text[pos + 1] == '<' || text[pos + 1] == '>')) { pos += 2; continue; }
                return 0;
            }
            pos++;
        }
        if (pos >= content_end) return 0;
        *dest_len = pos - *dest_start;
        pos++;
    } else {
        *dest_start = pos;
        while (pos < content_end) {
            if (text[pos] == '\\' && pos + 1 < content_end && strchr("!\"#$%&'()*+,-./:;<=>?@[\\]^_`{|}~", text[pos + 1])) { pos += 2; }
            else if (text[pos] == '(') {
                size_t match = pos + 1;
                int pd = 1;
                while (match < content_end && pd > 0) {
                    if (text[match] == '(') pd++;
                    else if (text[match] == ')') pd--;
                    if (pd > 0) match++;
                }
                if (pd == 0) pos = match + 1;
                else break;
            }
            else if (text[pos] != ' ' && text[pos] != '\t' && text[pos] != '\n' && text[pos] != '\r' && text[pos] != '\f' && text[pos] != '\v' && text[pos] != '"' && text[pos] != '\'' && text[pos] != ')' && text[pos] != '\\') { pos++; }
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
                    if (pos > *title_start && text[pos - 1] == '\\') { pos++; continue; }
                    break;
                }
                if (text[pos] == '\n' || text[pos] == '\r') return 0;
                pos++;
            }
            if (pos >= content_end) return 0;
            *title_len = pos - *title_start;
            pos++;
        } else if (text[pos] == '(') {
            *title_start = pos + 1;
            int pdepth = 1;
            pos++;
            while (pos < content_end && pdepth > 0) {
                if (text[pos] == '(') pdepth++;
                else if (text[pos] == ')') pdepth--;
                if (pdepth > 0) pos++;
            }
            if (pdepth > 0) return 0;
            *title_len = pos - *title_start;
            pos++;
        } else { return 0; }
        while (pos < content_end && (text[pos] == ' ' || text[pos] == '\t')) pos++;
    }
    return pos == content_end;
}

int collect_refs(const char *markdown, size_t md_len, RefDef **out_refs, int *out_n) {
    RefDef *refs = NULL;
    int n_refs = 0, cap_refs = 0;

    const char *p = markdown;
    const char *end = markdown + md_len;
    int in_fence = 0;
    char fence_char = '`';
    int fence_len = 0;
    int in_indented_code = 0;
    int in_html_block = 0;
    int html_block_type = 0;

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
            if (is_fence_end(line, trimmed_len, fence_char, fence_len))
                in_fence = 0;
            p = line_end + 1;
            continue;
        }
        if (in_indented_code) {
            if (trimmed_len == 0) {
                p = line_end + 1;
                continue;
            }
            size_t ind = get_indent_tab(line, line_len);
            if (ind < 4) in_indented_code = 0;
            else { p = line_end + 1; continue; }
        }
        if (!in_html_block && trimmed_len > 0 && !in_indented_code) {
            size_t ind = get_indent_tab(line, line_len);
            if (ind >= 4) { in_indented_code = 1; p = line_end + 1; continue; }
        }
        if (in_html_block) {
            if (trimmed_len == 0 && html_block_type > 5) { in_html_block = 0; }
            else {
                int closed = 0;
                if (html_block_type == 1) {
                    const char *closes[] = {"</pre>", "</script>", "</style>", "</textarea>"};
                    for (int ci = 0; ci < 4 && !closed; ci++) {
                        const char *f = strstr(line, closes[ci]);
                        if (!f) {
                            char upper[32];
                            size_t ul = strlen(closes[ci]);
                            if (ul < sizeof(upper)) {
                                for (size_t ui = 0; ui < ul; ui++) upper[ui] = toupper((unsigned char)closes[ci][ui]);
                                upper[ul] = 0;
                                f = strstr(line, upper);
                            }
                        }
                        if (f) closed = 1;
                    }
                } else if (html_block_type == 2) { if (strstr(line, "-->")) closed = 1; }
                else if (html_block_type == 3) { if (strstr(line, "?>")) closed = 1; }
                else if (html_block_type == 4) { for (size_t ci = 0; ci < trimmed_len; ci++) { if (line[ci] == '>') { closed = 1; break; } } }
                else if (html_block_type == 5) { if (strstr(line, "]]>")) closed = 1; }
                if (closed) in_html_block = 0;
            }
            p = line_end + 1;
            continue;
        }
        if (!in_fence && !in_indented_code && !in_html_block) {
            if (!in_html_block && trimmed_len > 0) {
                size_t bq_indent = 0;
                while (bq_indent < trimmed_len && bq_indent < 3 && line[bq_indent] == ' ') bq_indent++;
                if (bq_indent < trimmed_len && line[bq_indent] == '>') {
                    p = line_end + 1;
                    continue;
                }
            }
            RefDef rd;
            if (parse_ref_def(line, line_len, &rd)) {
                RefDef *existing = NULL;
                if (!find_ref(refs, n_refs, rd.label, rd.label_len, &existing)) {
                    if (n_refs >= cap_refs) {
                        int new_cap = cap_refs ? cap_refs * 2 : 8;
                        RefDef *new_refs = (RefDef *)realloc(refs, new_cap * sizeof(RefDef));
                        if (!new_refs) { ref_def_free(&rd); continue; }
                        refs = new_refs;
                        cap_refs = new_cap;
                    }
                    refs[n_refs++] = rd;
                } else { ref_def_free(&rd); }
            }
        }
        /* Check for fence start or html block start */
        if (!in_fence && !in_indented_code && !in_html_block && trimmed_len > 0) {
            char fc = 0;
            int fl = 0, fi = 0;
            if (is_fence_start(line, trimmed_len, &fc, &fl, &fi, NULL, NULL)) {
                in_fence = 1;
                fence_char = fc;
                fence_len = fl;
            } else if (line[0] == '<' && is_html_block_start(line, trimmed_len)) {
                int hb = is_html_block_start(line, trimmed_len);
                if (hb > 0) { in_html_block = 1; html_block_type = hb; }
            }
        }
        p = line_end + 1;
    }

    *out_refs = refs;
    *out_n = n_refs;
    return n_refs;
}

void render_alt_text(StringBuilder *sb, const char *text, size_t len, RefDef *refs, int n_refs) {
    StringBuilder *tmp = sb_new();
    if (!tmp) return;
    render_inline(tmp, text, len, 1, 1, refs, n_refs);
    const char *p = tmp->data;
    const char *end = tmp->data + tmp->len;
    while (p < end) {
        if (*p == '<') {
            const char *tag_start = p;
            while (p < end && *p != '>') p++;
            if (p < end) p++;
            if (end - tag_start >= 4 && tag_start[1] == 'i' && tag_start[2] == 'm' && tag_start[3] == 'g') {
                const char *ap = tag_start + 4;
                while (ap < end && *ap != '>') {
                    if (*ap == 'a' && ap + 1 < end && ap[1] == 'l' && ap[2] == 't' && ap[3] == '=') {
                        ap += 4;
                        if (*ap == '"') {
                            ap++;
                            while (ap < end && *ap != '"') {
                                if (*ap != '>') sb_append_char(sb, *ap);
                                ap++;
                            }
                        }
                    }
                    if (ap < end) ap++;
                }
            }
            continue;
        }
        sb_append_char(sb, *p);
        p++;
    }
    sb_free(tmp);
}
