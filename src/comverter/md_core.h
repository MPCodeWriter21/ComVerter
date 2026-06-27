#ifndef MD_CORE_H
#define MD_CORE_H

#include <stdio.h>
#define PY_SSIZE_T_CLEAN
#include <Python.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#define MAX_LIST_DEPTH 16
#define GFM_ENABLE_TAGFILTER 1
#define GFM_ENABLE_AUTOLINK 2

extern int g_gfm_extensions;

/* Bounded string search: like strstr but only searches within the first n bytes */
static inline const char *nstrstr(const char *haystack, size_t n, const char *needle) {
    size_t nlen = strlen(needle);
    if (nlen == 0) return haystack;
    if (n < nlen) return NULL;
    size_t max_i = n - nlen;
    for (size_t i = 0; i <= max_i; i++) {
        size_t j;
        for (j = 0; j < nlen; j++)
            if (haystack[i + j] != needle[j]) break;
        if (j == nlen) return haystack + i;
    }
    return NULL;
}

/* Bounded case-insensitive string search */
static inline const char *nistrstr(const char *haystack, size_t n, const char *needle) {
    size_t nlen = strlen(needle);
    if (nlen == 0) return haystack;
    if (n < nlen) return NULL;
    size_t max_i = n - nlen;
    for (size_t i = 0; i <= max_i; i++) {
        size_t j;
        for (j = 0; j < nlen; j++)
            if (tolower((unsigned char)haystack[i + j]) !=
                tolower((unsigned char)needle[j]))
                break;
        if (j == nlen) return haystack + i;
    }
    return NULL;
}

typedef struct {
    int type;            /* 1=ul, 2=ol */
    char marker;         /* '-', '+', '*' for ul; 0 for ol */
    char delimiter;      /* '.', ')' for ol; 0 for ul */
    int content_col;     /* column where item content starts */
    int min_content_col; /* minimum continuation indent (marker_col + marker_width + 1)
                          */
    int start_num;       /* starting number for ol; 0 for ul */
    int is_loose;        /* 1=loose list */
    int after_blank;     /* 1=blank line since last item start */
    int item_open;       /* 1=<li> open */
    int marker_col;      /* column where marker starts */
    char *saved_data;    /* saved li_content when sub-list was pushed */
    size_t saved_len;
    size_t saved_cap;
    int first_item_content_pos;
    int first_item_content_len;
    int had_block_content; /* 1=block content (fence, bq) emitted directly to sb */
    int had_content;       /* 1=item has ever had content (tight or block) */
    int first_block_code;  /* 1=first block was indented code (extra spaces >=4) */
} ListEntry;

typedef struct {
    char *data;
    size_t len;
    size_t cap;
} StringBuilder;

typedef struct {
    const char *name;
    unsigned long cps[4];
    int n_cps;
} EntityEntry;

typedef struct {
    char *label;
    size_t label_len;
    char *url;
    char *title;
} RefDef;

/* StringBuilder */
StringBuilder *sb_new(void);
int sb_ensure(StringBuilder *sb, size_t extra);
void sb_append(StringBuilder *sb, const char *s);
void sb_append_char(StringBuilder *sb, char c);
void sb_append_n(StringBuilder *sb, const char *s, size_t n);
void sb_insert(StringBuilder *sb, size_t pos, const char *s, size_t n);
void sb_free(StringBuilder *sb);
char *sb_detach(StringBuilder *sb);

/* Utilities */
size_t trim_trailing(const char *line, size_t len);
void escape_html(StringBuilder *sb, const char *text, size_t len);
size_t get_indent(const char *line, size_t len);
size_t get_indent_tab(const char *line, size_t len);
size_t get_indent_tab_from(const char *line, size_t len, size_t start_col);
size_t skip_ws(const char *line, size_t len);
size_t col_at_pos(const char *line, size_t pos);
size_t consume_indent(
    const char *s, size_t len, size_t start_col, size_t *leftover_spaces
);
const char *skip_spaces(const char *p, const char *end);
void append_codepoint_utf8(StringBuilder *sb, unsigned long cp);
int codepoint_to_utf8_buf(char *buf, unsigned long cp);
void append_uri_encoded(StringBuilder *sb, unsigned char c);
void append_url_with_entities(StringBuilder *sb, const char *text, size_t len);
void append_entity_decoded(StringBuilder *sb, const char *text, size_t len);
void decode_entity_and_append(
    StringBuilder *sb, const char *text, size_t len, size_t *consumed
);
int try_decode_entity(
    const char *text,
    size_t len,
    size_t *consumed,
    unsigned long *out_cps,
    int *out_n_cps
);

/* Reference definitions */
void ref_def_free(RefDef *rd);
size_t normalize_label(const char *src, size_t src_len, char *dst, size_t dst_cap);
int find_ref(
    RefDef *refs, int n_refs, const char *label, size_t label_len, RefDef **out
);
int parse_ref_def(const char *line, size_t line_len, RefDef *rd);

/* Unicode helpers */
size_t utf8_char_start(const char *text, size_t pos);
int is_unicode_punct(const char *text, size_t len, size_t pos);

/* Autolinks */
int is_valid_uri_autolink(const char *text, size_t len, size_t *consumed);
int is_valid_email_autolink(const char *text, size_t len, size_t *consumed);

/* HTML inline */
int is_html_tag(const char *text, size_t len, size_t *consumed);

/* HTML block */
int is_html_block_tag(const char *tag, size_t remaining);
int is_html_block_start(const char *line, size_t len);
void sb_append_with_disallowed_escaped(StringBuilder *sb, const char *text, size_t len);

/* Inline rendering */
int pos_inside_code_span(const char *text, size_t len, size_t pos);
int parse_link_dest_title(
    const char *text,
    size_t content_start,
    size_t content_end,
    size_t *dest_start,
    size_t *dest_len,
    size_t *title_start,
    size_t *title_len
);
int is_backslash_escaped(const char *text, size_t len, size_t pos);
int is_left_flanking(const char *text, size_t len, size_t pos);
int is_right_flanking(const char *text, size_t len, size_t pos);
void render_inline(
    StringBuilder *sb,
    const char *text,
    size_t len,
    int allow_emphasis,
    int allow_links,
    RefDef *refs,
    int n_refs
);
void render_alt_text(
    StringBuilder *sb, const char *text, size_t len, RefDef *refs, int n_refs
);

/* Block-level helpers */
int is_thematic_break(const char *line, size_t len);
int is_atx_heading(const char *line, size_t len);
size_t atx_content_end(const char *line, size_t len);
int is_table_row(const char *line, size_t len);
int is_table_separator(const char *line, size_t len);
int is_setext_underline(const char *line, size_t len);
int is_fence_start(
    const char *line,
    size_t len,
    char *fence_char,
    int *fence_len,
    int *fence_indent,
    const char **info_start,
    size_t *info_len
);
int is_fence_end(const char *line, size_t len, char fence_char, int fence_len);

/* List stack */
void list_push(
    ListEntry *stack,
    int *depth,
    int type,
    char marker,
    char delimiter,
    int marker_col,
    int content_col,
    int start_num
);
void list_save_content(StringBuilder *li_content, ListEntry *parent);
ListEntry *list_top(ListEntry *stack, int depth);
void list_open_item(StringBuilder *sb, ListEntry *e);
void list_flush_content(StringBuilder *sb, StringBuilder *li_content, ListEntry *e);
void list_close_item(StringBuilder *sb, StringBuilder *li_content, ListEntry *e);
void list_pop(
    StringBuilder *sb, StringBuilder *li_content, ListEntry *stack, int *depth
);
void list_emit_open(StringBuilder *sb, ListEntry *e);
void list_pop_to(
    StringBuilder *sb,
    StringBuilder *li_content,
    ListEntry *stack,
    int *depth,
    int target
);
int list_matches(ListEntry *e, int type, char marker, char delimiter);

/* Content processing */
void process_list_content(
    StringBuilder *sb,
    StringBuilder *li_content,
    ListEntry *stack,
    int *depth,
    const char *content,
    size_t content_len,
    int base_abs_col,
    RefDef *refs,
    int n_refs
);

/* Main conversion */
int collect_refs(const char *markdown, size_t md_len, RefDef **out_refs, int *out_n);
char *markdown_to_html(const char *markdown, size_t md_len);

#endif /* MD_CORE_H */
