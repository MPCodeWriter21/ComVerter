#include "md_core.h"

/* --- Block-level helpers --- */

int is_thematic_break(const char *line, size_t len) {
    size_t i = 0;
    while (i < len && i < 3 && (line[i] == ' ' || line[i] == '\t')) i++;
    if (len - i < 3) return 0;
    char c = line[i];
    if (c != '-' && c != '*' && c != '_') return 0;
    size_t count = 0;
    for (; i < len; i++) {
        if (line[i] == ' ' || line[i] == '\t') continue;
        if (line[i] != c) return 0;
        count++;
    }
    return count >= 3;
}

int is_atx_heading(const char *line, size_t len) {
    size_t i = 0;
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

size_t atx_content_end(const char *line, size_t len) {
    size_t end = len;
    while (end > 0 && line[end - 1] == ' ') end--;
    size_t saved = end;
    while (end > 0 && line[end - 1] == '#') {
        if (end >= 2 && line[end - 2] == '\\') break;
        end--;
    }
    if (end == 0 || (end > 0 && line[end - 1] != ' ')) end = saved;
    else
        end--;
    while (end > 0 && line[end - 1] == ' ') end--;
    return end;
}

int is_table_row(const char *line, size_t len) {
    for (size_t i = 0; i < len; i++) {
        if (line[i] == '\\' && i + 1 < len && line[i + 1] == '|') {
            i++;
            continue;
        }
        if (line[i] == '|') return 1;
    }
    return 0;
}

int is_table_separator(const char *line, size_t len) {
    size_t i = 0;
    while (i < len && (line[i] == ' ' || line[i] == '\t')) i++;
    if (i >= len) return 0;
    int has_pipe = 0, has_dash = 0;
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

/* Unescape \| -> | in a table cell's content (before inline rendering) */
static size_t unescape_cell_content(
    char *dst, size_t dst_cap, const char *src, size_t src_len
) {
    size_t j = 0;
    for (size_t i = 0; i < src_len && j < dst_cap - 1; i++) {
        if (src[i] == '\\' && i + 1 < src_len && src[i + 1] == '|') {
            dst[j++] = '|';
            i++;
        }
        else { dst[j++] = src[i]; }
    }
    dst[j] = '\0';
    return j;
}

/* Compute character position reaching target_col columns (tabs as tab stops) */
static size_t content_start_at_col(
    const char *line, size_t line_len, size_t target_col
) {
    size_t pos = 0, col = 0;
    while (pos < line_len && col < target_col) {
        if (line[pos] == '\t') col = (col + 4) & ~3;
        else
            col++;
        pos++;
    }
    return pos;
}

int is_setext_underline(const char *line, size_t len) {
    if (len == 0) return 0;
    char c = line[0];
    if (c != '=' && c != '-') return 0;
    if (c == '-' && len < 2) return 0;
    for (size_t i = 1; i < len; i++)
        if (line[i] != c) return 0;
    return (c == '=') ? 1 : 2;
}

int is_fence_start(
    const char *line,
    size_t len,
    char *fence_char,
    int *fence_len,
    int *fence_indent,
    const char **info_start,
    size_t *info_len
) {
    const char *p = line;
    const char *end = line + len;
    int indent = 0;
    while (p < end && *p == ' ' && indent < 3) {
        indent++;
        p++;
    }
    if (p >= end) return 0;
    if (*p != '`' && *p != '~') return 0;
    *fence_indent = indent;
    char c = *p;
    *fence_char = c;
    int count = 0;
    while (p < end && *p == c) {
        count++;
        p++;
    }
    if (count < 3) return 0;
    *fence_len = count;
    if (c == '`') {
        while (p < end && *p == ' ') p++;
        for (const char *scan = p; scan < end; scan++)
            if (*scan == '`') return 0;
    }
    if (info_start) *info_start = p;
    if (info_len) *info_len = end - p;
    return 1;
}

int is_fence_end(const char *line, size_t len, char fence_char, int fence_len) {
    const char *p = line;
    const char *end = line + len;
    /* Only allow up to 3 spaces of indentation on the closing fence */
    size_t indent = 0;
    while (p < end && *p == ' ' && indent < 3) {
        indent++;
        p++;
    }
    if (p >= end) return 0;
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

/* --- List stack helpers --- */

void list_push(
    ListEntry *stack,
    int *depth,
    int type,
    char marker,
    char delimiter,
    int marker_col,
    int content_col,
    int start_num
) {
    int d = *depth;
    if (d >= MAX_LIST_DEPTH) return;
    stack[d].type = type;
    stack[d].marker = marker;
    stack[d].delimiter = delimiter;
    stack[d].content_col = content_col;
    if (type == 1) { stack[d].min_content_col = marker_col + 2; }
    else if (type == 2) {
        int digits = 1, n = start_num;
        while (n >= 10) {
            digits++;
            n /= 10;
        }
        stack[d].min_content_col = marker_col + digits + 2;
    }
    else { stack[d].min_content_col = content_col; }
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
    stack[d].had_content = 0;
    stack[d].had_block_content = 0;
    stack[d].first_block_code = 0;
    *depth = d + 1;
}

void list_save_content(StringBuilder *li_content, ListEntry *parent) {
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

ListEntry *list_top(ListEntry *stack, int depth) {
    if (depth <= 0) return NULL;
    return &stack[depth - 1];
}

void list_open_item(StringBuilder *sb, ListEntry *e) {
    sb_append(sb, "<li>");
    e->item_open = 1;
}

void list_flush_content(StringBuilder *sb, StringBuilder *li_content, ListEntry *e) {
    if (li_content->len > 0) {
        if (e->first_item_content_pos < 0) {
            e->first_item_content_pos = (int)sb->len;
            e->first_item_content_len = (int)li_content->len;
        }
        sb_append_n(sb, li_content->data, li_content->len);
        li_content->len = 0;
    }
}

void list_close_item(StringBuilder *sb, StringBuilder *li_content, ListEntry *e) {
    if (e->item_open) {
        if (e->is_loose && e->first_item_content_pos >= 0 &&
            e->first_item_content_len > 0) {
            sb_insert(sb, (size_t)e->first_item_content_pos, "\n<p>", 4);
            size_t after_pos =
                (size_t)(e->first_item_content_pos + 4 + e->first_item_content_len);
            if (after_pos < sb->len && sb->data[after_pos] == '\n')
                sb_insert(sb, after_pos, "</p>", 4);
            else
                sb_insert(sb, after_pos, "</p>\n", 5);
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

void list_pop(
    StringBuilder *sb, StringBuilder *li_content, ListEntry *stack, int *depth
) {
    if (*depth <= 0) return;
    ListEntry *e = &stack[*depth - 1];
    int had_blank = e->after_blank;
    list_close_item(sb, li_content, e);
    if (e->type == 1) sb_append(sb, "</ul>\n");
    else
        sb_append(sb, "</ol>\n");
    (*depth)--;
    if (had_blank && *depth > 0) stack[*depth - 1].after_blank = 1;
    /* Restore parent's saved content if any */
    if (*depth > 0) {
        ListEntry *parent = &stack[*depth - 1];
        if (parent->saved_data) {
            li_content->data = parent->saved_data;
            li_content->len = parent->saved_len;
            li_content->cap = parent->saved_cap;
            parent->saved_data = NULL;
            parent->saved_len = 0;
            parent->saved_cap = 0;
        }
    }
}

void list_emit_open(StringBuilder *sb, ListEntry *e) {
    if (e->type == 1) sb_append(sb, "<ul>\n");
    else if (e->start_num != 1) {
        char buf[32];
        int n = snprintf(buf, sizeof(buf), "<ol start=\"%d\">\n", e->start_num);
        if (n > 0) sb_append_n(sb, buf, (size_t)n);
    }
    else { sb_append(sb, "<ol>\n"); }
}

void list_pop_to(
    StringBuilder *sb,
    StringBuilder *li_content,
    ListEntry *stack,
    int *depth,
    int target
) {
    while (*depth > target) list_pop(sb, li_content, stack, depth);
}

int list_matches(ListEntry *e, int type, char marker, char delimiter) {
    if (e->type != type) return 0;
    if (type == 1) return e->marker == marker;
    return e->delimiter == delimiter;
}

/* --- Content processing for list items --- */

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
) {
    /* Check for thematic break first (before sublist marker detection) */
    if (content_len > 0 && is_thematic_break(content, content_len)) {
        ListEntry *cur = list_top(stack, *depth);
        if (cur && cur->item_open) {
            list_flush_content(sb, li_content, cur);
            sb_append_char(sb, '\n');
        }
        sb_append(sb, "<hr />\n");
        return;
    }

    while (content_len > 0) {
        size_t ci = 0;
        while (ci < content_len && (content[ci] == ' ' || content[ci] == '\t')) ci++;
        if (ci >= content_len) break;

        int found = 0;
        int sub_type = 0;
        char sub_marker = 0;
        char sub_delim = 0;
        int sub_start = 0;
        size_t sub_marker_end = 0;

        if (!found &&
            (content[ci] == '-' || content[ci] == '*' || content[ci] == '+')) {
            size_t after = ci + 1;
            if (after >= content_len || content[after] == ' ' ||
                content[after] == '\t') {
                found = 1;
                sub_type = 1;
                sub_marker = content[ci];
                sub_marker_end = after;
            }
        }

        if (!found && isdigit((unsigned char)content[ci])) {
            size_t d = ci;
            int num = 0, digits = 0;
            while (d < content_len && isdigit((unsigned char)content[d]) && digits < 10
            ) {
                num = num * 10 + (content[d] - '0');
                d++;
                digits++;
            }
            if (digits > 0 && digits <= 9 && d < content_len &&
                (content[d] == '.' || content[d] == ')')) {
                size_t after = d + 1;
                if (after >= content_len || content[after] == ' ' ||
                    content[after] == '\t') {
                    found = 1;
                    sub_type = 2;
                    sub_delim = content[d];
                    sub_start = num;
                    sub_marker_end = after;
                }
            }
        }

        if (!found) break;

        ListEntry *parent = list_top(stack, *depth);
        if (parent && parent->item_open) {
            list_flush_content(sb, li_content, parent);
            parent->had_block_content = 1;
            if (sb->len == 0 || sb->data[sb->len - 1] != '\n') sb_append_char(sb, '\n');
        }
        list_save_content(li_content, parent);

        int sub_mc = base_abs_col + (int)ci;
        int sub_cc;
        if (sub_marker_end >= content_len) { sub_cc = sub_mc + 1; }
        else {
            size_t sp = sub_marker_end;
            while (sp < content_len && (content[sp] == ' ' || content[sp] == '\t'))
                sp++;
            sub_cc = base_abs_col + (int)sp;
        }

        list_push(
            stack, depth, sub_type, sub_marker, sub_delim, sub_mc, sub_cc, sub_start
        );
        list_emit_open(sb, &stack[*depth - 1]);
        list_open_item(sb, &stack[*depth - 1]);

        size_t skip = sub_marker_end;
        while (skip < content_len && (content[skip] == ' ' || content[skip] == '\t'))
            skip++;
        content += skip;
        content_len -= skip;
        base_abs_col += (int)skip;
    }

    if (content_len > 0) {
        if (is_thematic_break(content, content_len)) {
            ListEntry *cur = list_top(stack, *depth);
            if (cur && cur->item_open) {
                list_flush_content(sb, li_content, cur);
                sb_append_char(sb, '\n');
            }
            sb_append(sb, "<hr />\n");
            return;
        }
        /* Check for HTML block inside list item */
        {
            int li_hb = is_html_block_start(content, content_len);
            if (li_hb > 0) {
                ListEntry *cur = list_top(stack, *depth);
                if (cur && cur->item_open) {
                    list_flush_content(sb, li_content, cur);
                    if (sb->len == 0 || sb->data[sb->len - 1] != '\n')
                        sb_append_char(sb, '\n');
                }
                sb_append_n(sb, content, content_len);
                sb_append_char(sb, '\n');
                return;
            }
        }
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
                while (hp < content_len && (content[hp] == ' ' || content[hp] == '\t'))
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
                    render_inline(sb, content + hp, h_end - hp, 1, 1, refs, n_refs);
                sb_append(sb, "</h");
                sb_append_char(sb, (char)('0' + hlevel));
                sb_append(sb, ">\n");
                return;
            }
        }
        ListEntry *cur = list_top(stack, *depth);
        if (cur) cur->had_content = 1;
        render_inline(li_content, content, content_len, 1, 1, refs, n_refs);
    }
}

static void bq_flush_content(
    StringBuilder *sb,
    StringBuilder *para_buf,
    int *bq_has_content,
    int *bq_in_indented_code,
    RefDef *refs,
    int n_refs
) {
    if (*bq_in_indented_code) {
        sb_append(sb, "</code></pre>\n");
        *bq_in_indented_code = 0;
    }
    if (!*bq_has_content) return;
    const char *c = para_buf->data;
    size_t len = para_buf->len;
    /* Strip trailing newline if present */
    while (len > 0 && c[len - 1] == '\n') len--;

    /* Check if content looks like a list by scanning each line */
    int is_list = 0;
    int list_type = 0; /* 1 = ul, 2 = ol */
    size_t pos = 0;
    /* Look at first non-empty line */
    while (pos < len && (c[pos] == ' ' || c[pos] == '\n')) pos++;
    if (pos < len) {
        if (c[pos] == '-' || c[pos] == '*' || c[pos] == '+') {
            size_t after = pos + 1;
            if (after < len && (c[after] == ' ' || c[after] == '\t'))
                is_list = 1, list_type = 1;
        }
        else if (isdigit((unsigned char)c[pos])) {
            size_t d = pos;
            while (d < len && isdigit((unsigned char)c[d])) d++;
            if (d > pos && d < len && (c[d] == '.' || c[d] == ')')) {
                size_t after = d + 1;
                if (after >= len || c[after] == ' ' || c[after] == '\t')
                    is_list = 1, list_type = 2;
            }
        }
    }

    if (!is_list) {
        sb_append(sb, "<p>");
        render_inline(sb, c, len, 1, 1, refs, n_refs);
        sb_append(sb, "</p>\n");
    }
    else if (list_type == 1) {
        sb_append(sb, "<ul>\n");
        size_t i = 0;
        while (i < len) {
            /* Skip leading whitespace */
            while (i < len && (c[i] == ' ' || c[i] == '\t')) i++;
            if (i >= len) break;
            /* Detect list marker (-, *, +) */
            if (c[i] == '-' || c[i] == '*' || c[i] == '+') {
                size_t after = i + 1;
                if (after < len && (c[after] == ' ' || c[after] == '\t')) {
                    while (after < len && (c[after] == ' ' || c[after] == '\t'))
                        after++;
                    sb_append(sb, "<li>");
                    /* Content until next list marker or end */
                    size_t start = after;
                    while (after < len) {
                        size_t nl = after;
                        while (nl < len && c[nl] != '\n') nl++;
                        /* Check if next line is a new list item */
                        size_t next = nl;
                        if (next < len) next++; /* skip \n */
                        size_t check = next;
                        int is_new_item = 0;
                        while (check < len && (c[check] == ' ' || c[check] == '\t'))
                            check++;
                        if (check < len &&
                            (c[check] == '-' || c[check] == '*' || c[check] == '+')) {
                            size_t ca = check + 1;
                            if (ca < len && (c[ca] == ' ' || c[ca] == '\t'))
                                is_new_item = 1;
                        }
                        if (is_new_item) {
                            render_inline(
                                sb, c + start, nl - start, 1, 1, refs, n_refs
                            );
                            sb_append(sb, "</li>\n");
                            i = check;
                            break;
                        }
                        after = nl + 1;
                    }
                    if (after >= len) {
                        /* Last item - render remaining content */
                        render_inline(sb, c + start, len - start, 1, 1, refs, n_refs);
                        sb_append(sb, "</li>\n");
                        break;
                    }
                    continue;
                }
            }
            /* Skip to next line */
            while (i < len && c[i] != '\n') i++;
            if (i < len) i++;
        }
        sb_append(sb, "</ul>\n");
    }
    else if (list_type == 2) {
        sb_append(sb, "<ol>\n");
        size_t i = 0;
        while (i < len) {
            while (i < len && (c[i] == ' ' || c[i] == '\t')) i++;
            if (i >= len) break;
            if (isdigit((unsigned char)c[i])) {
                size_t d = i;
                while (d < len && isdigit((unsigned char)c[d])) d++;
                if (d < len && (c[d] == '.' || c[d] == ')')) {
                    size_t after = d + 1;
                    if (after >= len || c[after] == ' ' || c[after] == '\t') {
                        while (after < len && (c[after] == ' ' || c[after] == '\t'))
                            after++;
                        sb_append(sb, "<li>");
                        size_t start = after;
                        while (after < len) {
                            size_t nl = after;
                            while (nl < len && c[nl] != '\n') nl++;
                            size_t next = nl;
                            if (next < len) next++;
                            size_t check = next;
                            int is_new_item = 0;
                            while (check < len && (c[check] == ' ' || c[check] == '\t'))
                                check++;
                            if (check < len && isdigit((unsigned char)c[check])) {
                                size_t cd = check;
                                while (cd < len && isdigit((unsigned char)c[cd])) cd++;
                                if (cd < len && (c[cd] == '.' || c[cd] == ')')) {
                                    size_t ca = cd + 1;
                                    if (ca >= len || c[ca] == ' ' || c[ca] == '\t')
                                        is_new_item = 1;
                                }
                            }
                            if (is_new_item) {
                                render_inline(
                                    sb, c + start, nl - start, 1, 1, refs, n_refs
                                );
                                sb_append(sb, "</li>\n");
                                i = check;
                                break;
                            }
                            after = nl + 1;
                        }
                        if (after >= len) {
                            render_inline(
                                sb, c + start, len - start, 1, 1, refs, n_refs
                            );
                            sb_append(sb, "</li>\n");
                            break;
                        }
                        continue;
                    }
                }
            }
            while (i < len && c[i] != '\n') i++;
            if (i < len) i++;
        }
        sb_append(sb, "</ol>\n");
    }
    *bq_has_content = 0;
    para_buf->len = 0;
    para_buf->data[0] = '\0';
}

int g_gfm_extensions = 0;

/* --- Main conversion --- */

char *markdown_to_html(const char *markdown, size_t md_len) {
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
    int fence_open_indent = 0;
    int fence_in_blockquote = 0;
    int in_blockquote = 0;
    int bq_depth = 0;
    int bq_has_content = 0;
    int bq_in_html_block = 0;
    int bq_is_paragraph = 0;
    int bq_in_indented_code = 0;
    int bq_in_list = 0; /* 1=inside a UL/OL emitted directly from blockquote handler */
    int bq_in_list_type = 0;    /* 1=UL, 2=OL */
    int bq_in_list_bq_open = 0; /* 1=inner <blockquote> is open within the list item */
    int bq_is_loose = 0;        /* 1=list item has become loose (blank line inside) */
    int bq_had_blank_same_depth = 0; /* 1=blank line seen at current blockquote depth */
    size_t bq_list_content_col = 0;  /* content column of current bq_in_list item */
    int in_indented_code = 0;
    size_t ic_target_col = 4;
    int code_pending_newlines = 0;
    int in_html_block = 0;
    int html_block_type = 0;
    int para_has_content = 0;

    ListEntry list_stack[MAX_LIST_DEPTH];
    int list_depth = 0;
    StringBuilder *li_content = sb_new();

    RefDef *refs = NULL;
    int n_refs = 0, cap_refs = 0;

    /* Collect all reference definitions first (two-pass: refs before inline rendering)
     */
    collect_refs(markdown, md_len, &refs, &n_refs);
    cap_refs = n_refs > 0 ? n_refs : 0;
    char *expanded_line = NULL;

    while (p < end) {
        free(expanded_line);
        expanded_line = NULL;
        const char *line_end = p;
        while (line_end < end && *line_end != '\n' && *line_end != '\r') line_end++;
        size_t line_len = line_end - p;
        const char *line = p;
        const char *orig_line = line;
        size_t orig_line_len = line_len;

        if (line_end < end && *line_end == '\r') {
            if (line_end + 1 < end && *(line_end + 1) == '\n') line_end++;
        }

        /* Expand tabs to spaces (tab stops every 4 columns) for indent analysis.
           The original line is preserved in orig_line for code content output. */
        {
            int has_tab = 0;
            for (size_t ti = 0; ti < line_len && !has_tab; ti++)
                if (line[ti] == '\t') has_tab = 1;
            if (has_tab) {
                size_t max_exp = line_len * 4 + 1;
                expanded_line = (char *)malloc(max_exp);
                if (expanded_line) {
                    size_t di = 0, col = 0;
                    for (size_t si = 0; si < line_len && di < max_exp - 1; si++) {
                        unsigned char c = (unsigned char)line[si];
                        if (c == '\t') {
                            size_t tab_stop = (col + 4) & ~3;
                            while (col < tab_stop && di < max_exp - 1) {
                                expanded_line[di++] = ' ';
                                col++;
                            }
                        }
                        else {
                            expanded_line[di++] = c;
                            col++;
                        }
                    }
                    expanded_line[di] = '\0';
                    line = expanded_line;
                    line_len = di;
                }
            }
        }

        size_t trimmed_len = trim_trailing(line, line_len);

        if (in_fence) {
            if (fence_in_blockquote) {
                size_t bq_skip = 0;
                while (bq_skip < trimmed_len && bq_skip < 3 && line[bq_skip] == ' ')
                    bq_skip++;
                int has_bq = (bq_skip < trimmed_len && line[bq_skip] == '>');
                if (has_bq) {
                    size_t after = bq_skip + 1;
                    if (after < trimmed_len && line[after] == ' ') after++;
                    if (is_fence_end(
                            line + after, trimmed_len - after, fence_char, fence_len
                        )) {
                        in_fence = 0;
                        fence_in_blockquote = 0;
                        sb_append(sb, "</code></pre>\n");
                    }
                    else {
                        size_t content_start = 0;
                        int remaining = fence_open_indent;
                        size_t after_len = trimmed_len - after;
                        while (content_start < after_len && remaining > 0 &&
                               line[after + content_start] == ' ') {
                            content_start++;
                            remaining--;
                        }
                        escape_html(
                            sb, line + after + content_start, after_len - content_start
                        );
                        sb_append(sb, "\n");
                    }
                    p = line_end + 1;
                    continue;
                }
                /* Line without >: end fence and blockquote */
                in_fence = 0;
                fence_in_blockquote = 0;
                sb_append(sb, "</code></pre>\n");
                while (bq_depth > 0) {
                    sb_append(sb, "</blockquote>\n");
                    bq_depth--;
                }
                in_blockquote = 0;
                /* Re-process this line by falling through */
            }
            else {
                size_t fc_start = 0;
                int fc_rem = fence_open_indent;
                while (fc_start < line_len && fc_rem > 0 && line[fc_start] == ' ') {
                    fc_start++;
                    fc_rem--;
                }
                if (is_fence_end(
                        line + fc_start, trimmed_len - fc_start, fence_char, fence_len
                    )) {
                    in_fence = 0;
                    sb_append(sb, "</code></pre>\n");
                }
                else {
                    escape_html(sb, line + fc_start, line_len - fc_start);
                    sb_append(sb, "\n");
                }
                p = line_end + 1;
                continue;
            }
        }

        if (in_indented_code) {
            size_t ic_indent = get_indent_tab(line, line_len);
            if (trimmed_len > 0 && ic_indent < ic_target_col) {
                in_indented_code = 0;
                code_pending_newlines = 0;
                sb_append(sb, "</code></pre>\n");
            }
            else {
                size_t content_start =
                    content_start_at_col(orig_line, orig_line_len, ic_target_col);
                size_t content_len = orig_line_len - content_start;
                if (content_len == 0) { code_pending_newlines++; }
                else {
                    for (int pi = 0; pi < code_pending_newlines; pi++)
                        sb_append_char(sb, '\n');
                    code_pending_newlines = 0;
                    escape_html(sb, orig_line + content_start, content_len);
                    sb_append(sb, "\n");
                }
                p = line_end + 1;
                continue;
            }
        }

        if (!in_html_block && !in_blockquote && list_depth == 0 && trimmed_len > 0 &&
            !para_has_content && !in_indented_code) {
            size_t indent_col = get_indent_tab(line, line_len);
            if (indent_col >= 4) {
                in_indented_code = 1;
                ic_target_col = 4;
                sb_append(sb, "<pre><code>");
                size_t content_start =
                    content_start_at_col(orig_line, orig_line_len, ic_target_col);
                escape_html(
                    sb, orig_line + content_start, orig_line_len - content_start
                );
                sb_append(sb, "\n");
                p = line_end + 1;
                continue;
            }
        }

        if (para_has_content && trimmed_len >= 1) {
            size_t se_start = 0;
            while (se_start < trimmed_len && se_start < 3 && line[se_start] == ' ')
                se_start++;
            if (se_start < trimmed_len && line[se_start] == '\t')
                se_start = trimmed_len;
            int se_level = is_setext_underline(line + se_start, trimmed_len - se_start);
            if (se_level > 0) {
                const char *se_content = para_buf->data;
                size_t se_content_len = para_buf->len;
                while (se_content_len > 0 && (*se_content == ' ' || *se_content == '\t')
                ) {
                    se_content++;
                    se_content_len--;
                }
                while (se_content_len > 0 && (se_content[se_content_len - 1] == ' ' ||
                                              se_content[se_content_len - 1] == '\t'))
                    se_content_len--;
                char tmp[4] = {'<', 'h', (char)('0' + se_level), '>'};
                sb_append_n(sb, tmp, 4);
                render_inline(sb, se_content, se_content_len, 1, 1, refs, n_refs);
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

        if (!para_has_content && !in_blockquote && list_depth == 0 && trimmed_len > 0 &&
            is_table_row(line, trimmed_len)) {
            const char *next_line = line_end + 1;
            if (*line_end == '\r' && next_line < end) next_line++;
            if (*line_end == '\n') next_line = line_end + 1;
            while (next_line < end && (*next_line == '\n' || *next_line == '\r'))
                next_line++;
            if (next_line < end) {
                const char *sep_end = next_line;
                while (sep_end < end && *sep_end != '\n' && *sep_end != '\r') sep_end++;
                size_t sep_len = sep_end - next_line;
                if (is_table_separator(next_line, sep_len)) {
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
                    int n_cols = n_align;

                    int hdr_cells = 0;
                    {
                        size_t ci = 0;
                        while (ci < trimmed_len) {
                            while (ci < trimmed_len &&
                                   (line[ci] == ' ' || line[ci] == '|'))
                                ci++;
                            if (ci >= trimmed_len) break;
                            size_t cell_start = ci;
                            while (ci < trimmed_len &&
                                   !(line[ci] == '|' &&
                                     (ci == 0 || line[ci - 1] != '\\')))
                                ci++;
                            if (ci > cell_start && hdr_cells < 64) hdr_cells++;
                            if (ci >= trimmed_len) break;
                            ci++;
                        }
                    }
                    if (hdr_cells > n_cols) goto not_a_table;

                    sb_append(sb, "<table>\n<thead>\n<tr>\n");
                    {
                        size_t ci = 0;
                        int cell_idx = 0;
                        while (ci < trimmed_len && cell_idx < n_cols) {
                            while (ci < trimmed_len &&
                                   (line[ci] == ' ' || line[ci] == '|'))
                                ci++;
                            if (ci >= trimmed_len) break;
                            size_t cell_start = ci;
                            while (ci < trimmed_len &&
                                   !(line[ci] == '|' &&
                                     (ci == 0 || line[ci - 1] != '\\')))
                                ci++;
                            if (ci == cell_start) break;
                            size_t cell_end = ci;
                            while (cell_end > cell_start && line[cell_end - 1] == ' ')
                                cell_end--;
                            while (cell_start < cell_end && line[cell_start] == ' ')
                                cell_start++;
                            if (cell_end > cell_start) {
                                char cell_buf[4096];
                                size_t cell_len = unescape_cell_content(
                                    cell_buf, sizeof(cell_buf), line + cell_start,
                                    cell_end - cell_start
                                );
                                sb_append(sb, "<th");
                                if (cell_idx < n_align && align[cell_idx] == 'l')
                                    sb_append(sb, " align=\"left\"");
                                else if (cell_idx < n_align && align[cell_idx] == 'c')
                                    sb_append(sb, " align=\"center\"");
                                else if (cell_idx < n_align && align[cell_idx] == 'r')
                                    sb_append(sb, " align=\"right\"");
                                sb_append(sb, ">");
                                render_inline(
                                    sb, cell_buf, cell_len, 1, 1, refs, n_refs
                                );
                                sb_append(sb, "</th>\n");
                            }
                            else { sb_append(sb, "<th></th>\n"); }
                            cell_idx++;
                            if (ci >= trimmed_len) break;
                            ci++;
                        }
                        while (cell_idx < n_cols) {
                            sb_append(sb, "<th></th>\n");
                            cell_idx++;
                        }
                    }
                    sb_append(sb, "</tr>\n</thead>\n");
                    int has_body = 0;

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
                        {
                            size_t ws = 0;
                            while (ws < btrimmed && p[ws] == ' ') ws++;
                            if (ws < btrimmed && p[ws] == '>') break;
                        }

                        if (!has_body) {
                            sb_append(sb, "<tbody>\n");
                            has_body = 1;
                        }

                        sb_append(sb, "<tr>\n");
                        size_t ci = 0;
                        int cell_idx = 0;
                        while (ci < btrimmed && cell_idx < n_cols) {
                            while (ci < btrimmed && (p[ci] == ' ' || p[ci] == '|'))
                                ci++;
                            if (ci >= btrimmed) break;
                            size_t cell_start = ci;
                            while (ci < btrimmed &&
                                   !(p[ci] == '|' && (ci == 0 || p[ci - 1] != '\\')))
                                ci++;
                            if (ci == cell_start) break;
                            size_t cell_end = ci;
                            while (cell_end > cell_start && p[cell_end - 1] == ' ')
                                cell_end--;
                            while (cell_start < cell_end && p[cell_start] == ' ')
                                cell_start++;
                            if (cell_end > cell_start) {
                                char cell_buf[4096];
                                size_t cell_len = unescape_cell_content(
                                    cell_buf, sizeof(cell_buf), p + cell_start,
                                    cell_end - cell_start
                                );
                                sb_append(sb, "<td");
                                if (cell_idx < n_align && align[cell_idx] == 'l')
                                    sb_append(sb, " align=\"left\"");
                                else if (cell_idx < n_align && align[cell_idx] == 'c')
                                    sb_append(sb, " align=\"center\"");
                                else if (cell_idx < n_align && align[cell_idx] == 'r')
                                    sb_append(sb, " align=\"right\"");
                                sb_append(sb, ">");
                                render_inline(
                                    sb, cell_buf, cell_len, 1, 1, refs, n_refs
                                );
                                sb_append(sb, "</td>\n");
                            }
                            else { sb_append(sb, "<td></td>\n"); }
                            cell_idx++;
                            if (ci >= btrimmed) break;
                            ci++;
                        }
                        while (cell_idx < n_cols) {
                            sb_append(sb, "<td></td>\n");
                            cell_idx++;
                        }
                        sb_append(sb, "</tr>\n");
                        p = bline_end + 1;
                        if (bline_end < end && *bline_end == '\r') {
                            if (bline_end + 1 < end && *(bline_end + 1) == '\n')
                                p = bline_end + 2;
                        }
                    }
                    if (has_body) sb_append(sb, "</tbody>\n");
                    sb_append(sb, "</table>\n");
                    continue;
                }
            }
        not_a_table:;
        }

        if (in_html_block) {
            if (trimmed_len == 0) {
                if (html_block_type > 5) {
                    in_html_block = 0;
                    p = line_end + 1;
                    continue;
                }
                else {
                    sb_append_char(sb, '\n');
                    p = line_end + 1;
                    continue;
                }
            }
            if (g_gfm_extensions & GFM_ENABLE_TAGFILTER)
                sb_append_with_disallowed_escaped(sb, line, line_len);
            else
                sb_append_n(sb, line, line_len);
            sb_append_char(sb, '\n');
            if (html_block_type <= 5) {
                int closed = 0;
                if (html_block_type == 1) {
                    const char *closes[] = {
                        "</pre>", "</script>", "</style>", "</textarea>"
                    };
                    for (int ci = 0; ci < 4 && !closed; ci++) {
                        const char *found = nstrstr(line, trimmed_len, closes[ci]);
                        if (!found) found = nistrstr(line, trimmed_len, closes[ci]);
                        if (found) closed = 1;
                    }
                }
                else if (html_block_type == 2) {
                    if (nstrstr(line, trimmed_len, "-->")) closed = 1;
                }
                else if (html_block_type == 3) {
                    if (nstrstr(line, trimmed_len, "?>")) closed = 1;
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
                    if (nstrstr(line, trimmed_len, "]]>")) closed = 1;
                }
                if (closed) in_html_block = 0;
            }
            p = line_end + 1;
            continue;
        }

        if (trimmed_len == 0) {
            if (para_has_content) {
                while (para_buf->len > 0 && (para_buf->data[para_buf->len - 1] == ' ' ||
                                             para_buf->data[para_buf->len - 1] == '\t'))
                    para_buf->len--;
                para_buf->data[para_buf->len] = '\0';
                sb_append(sb, "<p>");
                render_inline(sb, para_buf->data, para_buf->len, 1, 1, refs, n_refs);
                sb_append(sb, "</p>\n");
                para_has_content = 0;
                para_buf->len = 0;
                para_buf->data[0] = '\0';
            }
            if (in_blockquote) {
                bq_in_html_block = 0;
                bq_flush_content(
                    sb, para_buf, &bq_has_content, &bq_in_indented_code, refs, n_refs
                );
                while (bq_depth > 0) {
                    sb_append(sb, "</blockquote>\n");
                    bq_depth--;
                }
                in_blockquote = 0;
            }
            if (list_depth > 0) {
                ListEntry *cur = list_top(list_stack, list_depth);
                cur->after_blank = 1;
            }
            p = line_end + 1;
            continue;
        }

        if (!para_has_content && !in_blockquote && list_depth == 0 && !in_fence &&
            !in_indented_code) {
            RefDef rd;
            if (parse_ref_def(line, line_len, &rd)) {
                int title_on_next = 0;
                if (!rd.title) {
                    const char *nl = line_end + 1;
                    if (nl < end && *nl == '\r') {
                        if (nl + 1 < end && *(nl + 1) == '\n') nl++;
                    }
                    if (nl < end && (*nl == '\n' || *nl == '\r')) nl++;
                    const char *nl_end = nl;
                    while (nl_end < end && *nl_end != '\n' && *nl_end != '\r') nl_end++;
                    size_t nl_len = nl_end - nl;
                    size_t nl_trimmed = trim_trailing(nl, nl_len);
                    if (nl_trimmed > 0) {
                        size_t nl_ind = 0;
                        while (nl_ind < nl_trimmed && nl[nl_ind] == ' ') nl_ind++;
                        if (nl_ind < nl_trimmed) {
                            const char *nc = nl + nl_ind;
                            size_t ncl = nl_trimmed - nl_ind;
                            if (ncl >= 2 && (nc[0] == '"' || nc[0] == '\'') &&
                                nc[ncl - 1] == nc[0]) {
                                rd.title = (char *)malloc(ncl - 1);
                                if (rd.title) {
                                    memcpy(rd.title, nc + 1, ncl - 2);
                                    rd.title[ncl - 2] = '\0';
                                    title_on_next = 1;
                                }
                            }
                        }
                    }
                }
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
                if (title_on_next) {
                    const char *nl_end = line_end + 1;
                    if (nl_end < end && *nl_end == '\r') {
                        if (nl_end + 1 < end && *(nl_end + 1) == '\n') nl_end++;
                    }
                    if (nl_end < end && (*nl_end == '\n' || *nl_end == '\r')) nl_end++;
                    while (nl_end < end && *nl_end != '\n' && *nl_end != '\r') nl_end++;
                    p = nl_end;
                    if (nl_end < end && *nl_end == '\r')
                        if (nl_end + 1 < end && *(nl_end + 1) == '\n') p = nl_end + 2;
                        else
                            p = nl_end + 1;
                    else if (nl_end < end && *nl_end == '\n')
                        p = nl_end + 1;
                }
                else { p = line_end + 1; }
                continue;
            }
            else if (trimmed_len > 0 && line_len >= 1) {
                size_t s = 0;
                while (s < line_len && s < 3 && line[s] == ' ') s++;
                if (s < line_len && line[s] == '[') {
                    char accum[8192];
                    size_t alen = line_len;
                    if (alen >= sizeof(accum)) alen = sizeof(accum) - 1;
                    memcpy(accum, line, alen);
                    const char *scan = line_end;
                    const char *last_line_end = line_end;
                    int accum_success = 0;
                    while (scan < end) {
                        RefDef rd_accum;
                        if (parse_ref_def(accum, alen, &rd_accum)) {
                            int title_on_next = 0;
                            const char *title_line_end = NULL;
                            if (!rd_accum.title) {
                                const char *nl2 = last_line_end;
                                if (nl2 < end && *nl2 == '\r') {
                                    if (nl2 + 1 < end && *(nl2 + 1) == '\n') nl2++;
                                }
                                if (nl2 < end && (*nl2 == '\n' || *nl2 == '\r')) nl2++;
                                const char *nl2_end = nl2;
                                while (nl2_end < end && *nl2_end != '\n' &&
                                       *nl2_end != '\r')
                                    nl2_end++;
                                size_t nl2_len = nl2_end - nl2;
                                size_t nl2_trimmed = trim_trailing(nl2, nl2_len);
                                if (nl2_trimmed > 0) {
                                    size_t nl2_ind = 0;
                                    while (nl2_ind < nl2_trimmed && nl2[nl2_ind] == ' ')
                                        nl2_ind++;
                                    if (nl2_ind < nl2_trimmed) {
                                        const char *nc2 = nl2 + nl2_ind;
                                        size_t ncl2 = nl2_trimmed - nl2_ind;
                                        if (ncl2 >= 2 &&
                                            (nc2[0] == '"' || nc2[0] == '\'') &&
                                            nc2[ncl2 - 1] == nc2[0]) {
                                            rd_accum.title = (char *)malloc(ncl2 - 1);
                                            if (rd_accum.title) {
                                                memcpy(
                                                    rd_accum.title, nc2 + 1, ncl2 - 2
                                                );
                                                rd_accum.title[ncl2 - 2] = '\0';
                                                title_on_next = 1;
                                                title_line_end = nl2_end;
                                            }
                                        }
                                    }
                                }
                            }
                            if (title_on_next && title_line_end) {
                                p = title_line_end;
                                if (title_line_end < end && *title_line_end == '\r') {
                                    if (title_line_end + 1 < end &&
                                        *(title_line_end + 1) == '\n')
                                        p = title_line_end + 2;
                                    else
                                        p = title_line_end + 1;
                                }
                                else if (title_line_end < end &&
                                         *title_line_end == '\n') {
                                    p = title_line_end + 1;
                                }
                            }
                            else {
                                p = last_line_end;
                                if (last_line_end < end && *last_line_end == '\r')
                                    if (last_line_end + 1 < end &&
                                        *(last_line_end + 1) == '\n')
                                        p = last_line_end + 2;
                                    else
                                        p = last_line_end + 1;
                                else if (last_line_end < end && *last_line_end == '\n')
                                    p = last_line_end + 1;
                            }
                            RefDef *existing = NULL;
                            if (!find_ref(
                                    refs, n_refs, rd_accum.label, rd_accum.label_len,
                                    &existing
                                )) {
                                if (n_refs >= cap_refs) {
                                    int new_cap = cap_refs ? cap_refs * 2 : 8;
                                    RefDef *new_refs = (RefDef *)realloc(
                                        refs, new_cap * sizeof(RefDef)
                                    );
                                    if (new_refs) {
                                        refs = new_refs;
                                        cap_refs = new_cap;
                                    }
                                }
                                if (n_refs < cap_refs) refs[n_refs++] = rd_accum;
                                else
                                    ref_def_free(&rd_accum);
                            }
                            else { ref_def_free(&rd_accum); }
                            accum_success = 1;
                            break;
                        }
                        if (scan >= end) break;
                        if (*scan == '\r') {
                            scan++;
                            if (scan < end && *scan == '\n') scan++;
                        }
                        else if (*scan == '\n') { scan++; }
                        else { break; }
                        const char *next_start = scan;
                        const char *next_end = next_start;
                        while (next_end < end && *next_end != '\n' && *next_end != '\r')
                            next_end++;
                        size_t next_trimmed =
                            trim_trailing(next_start, next_end - next_start);
                        if (next_trimmed == 0) break;
                        size_t add_len = next_end - next_start;
                        if (alen + 1 + add_len >= sizeof(accum)) break;
                        accum[alen++] = '\n';
                        memcpy(accum + alen, next_start, add_len);
                        alen += add_len;
                        scan = next_end;
                        last_line_end = next_end;
                    }
                    if (accum_success) continue;
                }
            }
        }

        if (!para_has_content && !in_blockquote && list_depth == 0 && !in_fence &&
            !in_indented_code) {
            int hb_type = is_html_block_start(line, trimmed_len);
            if (hb_type > 0) {
                in_html_block = 1;
                html_block_type = hb_type;
                if (g_gfm_extensions & GFM_ENABLE_TAGFILTER)
                    sb_append_with_disallowed_escaped(sb, line, line_len);
                else
                    sb_append_n(sb, line, line_len);
                sb_append_char(sb, '\n');
                if (hb_type <= 5) {
                    int closed = 0;
                    if (hb_type == 1) {
                        const char *closes[] = {
                            "</pre>", "</script>", "</style>", "</textarea>"
                        };
                        for (int ci = 0; ci < 4 && !closed; ci++) {
                            const char *found = nstrstr(line, trimmed_len, closes[ci]);
                            if (!found) found = nistrstr(line, trimmed_len, closes[ci]);
                            if (found) closed = 1;
                        }
                    }
                    else if (hb_type == 2) {
                        if (nstrstr(line, trimmed_len, "-->")) closed = 1;
                    }
                    else if (hb_type == 3) {
                        if (nstrstr(line, trimmed_len, "?>")) closed = 1;
                    }
                    else if (hb_type == 4) {
                        for (size_t ci = 0; ci < trimmed_len; ci++) {
                            if (line[ci] == '>') {
                                closed = 1;
                                break;
                            }
                        }
                    }
                    else if (hb_type == 5) {
                        if (nstrstr(line, trimmed_len, "]]>")) closed = 1;
                    }
                    if (closed) in_html_block = 0;
                }
                p = line_end + 1;
                continue;
            }
        }

        if (in_blockquote && line[0] != '>') {
            /* Check if this line has a blockquote marker (>) after leading spaces */
            size_t bq_check = 0;
            while (bq_check < trimmed_len && bq_check < 3 && line[bq_check] == ' ')
                bq_check++;
            int has_bq_marker = (bq_check < trimmed_len && line[bq_check] == '>');
            if (has_bq_marker) { /* let the regular blockquote code handle it */
            }
            else {
                int is_new_list_item = 0;
                size_t indent = get_indent(line, trimmed_len);
                if (indent < 4 && trimmed_len > indent + 1) {
                    char c = line[indent];
                    if ((c == '-' || c == '*' || c == '+') &&
                        indent + 1 < trimmed_len && line[indent + 1] == ' ')
                        is_new_list_item = 1;
                }
                if (is_new_list_item) {
                    /* Close the blockquote and let this line be processed normally as a
                     * list */
                    if (bq_in_list) {
                        if (bq_is_loose || bq_had_blank_same_depth ||
                            bq_in_list_bq_open) {
                            if (sb->len > 0 && sb->data[sb->len - 1] != '\n')
                                sb_append_char(sb, '\n');
                            bq_flush_content(
                                sb, para_buf, &bq_has_content, &bq_in_indented_code,
                                refs, n_refs
                            );
                        }
                        else if (bq_has_content) {
                            render_inline(
                                sb, para_buf->data, para_buf->len, 1, 1, refs, n_refs
                            );
                            bq_has_content = 0;
                            para_buf->len = 0;
                            para_buf->data[0] = '\0';
                        }
                        if (bq_in_list_bq_open) {
                            sb_append(sb, "</blockquote>\n");
                            bq_in_list_bq_open = 0;
                        }
                        sb_append(sb, "</li>\n");
                        if (bq_in_list_type == 1) sb_append(sb, "</ul>\n");
                        else
                            sb_append(sb, "</ol>\n");
                        bq_in_list = 0;
                        bq_is_loose = 0;
                        bq_had_blank_same_depth = 0;
                    }
                    else {
                        bq_flush_content(
                            sb, para_buf, &bq_has_content, &bq_in_indented_code, refs,
                            n_refs
                        );
                    }
                    while (bq_depth > 0) {
                        sb_append(sb, "</blockquote>\n");
                        bq_depth--;
                    }
                    in_blockquote = 0;
                }
                else {
                    /* Check if this line is a continuation of a list item inside a
                       blockquote. If so, close the blockquote and let the list
                       continuation handler process it. */
                    int bq_closed_for_list = 0;
                    if (list_depth > 0) {
                        ListEntry *bq_lc = list_top(list_stack, list_depth);
                        size_t bq_lc_indent = get_indent_tab(line, line_len);
                        if (bq_lc_indent >= (size_t)bq_lc->content_col) {
                            bq_flush_content(
                                sb, para_buf, &bq_has_content, &bq_in_indented_code,
                                refs, n_refs
                            );
                            while (bq_depth > 0) {
                                sb_append(sb, "</blockquote>\n");
                                bq_depth--;
                            }
                            in_blockquote = 0;
                            bq_is_paragraph = 0;
                            bq_closed_for_list = 1;
                        }
                    }
                    if (!bq_closed_for_list) {
                        if (bq_in_list) {
                            /* Check if this line starts a new list item (which should
                             * close the blockquote) */
                            size_t bq_li_indent = get_indent(line, trimmed_len);
                            int bq_new_li = 0, bq_new_ol = 0;
                            if (bq_li_indent < 4 && trimmed_len > bq_li_indent + 1) {
                                char c = line[bq_li_indent];
                                if ((c == '-' || c == '*' || c == '+') &&
                                    bq_li_indent + 1 < trimmed_len &&
                                    line[bq_li_indent + 1] == ' ')
                                    bq_new_li = 1;
                                else {
                                    size_t bq_ds = bq_li_indent;
                                    int num = 0, dc = 0;
                                    while (bq_ds < trimmed_len &&
                                           isdigit((unsigned char)line[bq_ds]) &&
                                           dc < 10) {
                                        num = num * 10 + (line[bq_ds] - '0');
                                        bq_ds++;
                                        dc++;
                                    }
                                    if (dc > 0 && bq_ds < trimmed_len &&
                                        (line[bq_ds] == '.' || line[bq_ds] == ')') &&
                                        bq_ds + 1 < trimmed_len &&
                                        (line[bq_ds + 1] == ' ' ||
                                         line[bq_ds + 1] == '\t'))
                                        bq_new_ol = 1;
                                }
                            }
                            if (bq_new_li || bq_new_ol) {
                                /* Close current list and blockquote entirely */
                                if (bq_is_loose || bq_had_blank_same_depth ||
                                    bq_in_list_bq_open) {
                                    bq_flush_content(
                                        sb, para_buf, &bq_has_content,
                                        &bq_in_indented_code, refs, n_refs
                                    );
                                }
                                else if (bq_has_content) {
                                    render_inline(
                                        sb, para_buf->data, para_buf->len, 1, 1, refs,
                                        n_refs
                                    );
                                    bq_has_content = 0;
                                    para_buf->len = 0;
                                    para_buf->data[0] = '\0';
                                }
                                if (bq_in_list_bq_open) {
                                    sb_append(sb, "</blockquote>\n");
                                    bq_in_list_bq_open = 0;
                                }
                                sb_append(sb, "</li>\n");
                                if (bq_in_list_type == 1) sb_append(sb, "</ul>\n");
                                else
                                    sb_append(sb, "</ol>\n");
                                bq_in_list = 0;
                                bq_is_loose = 0;
                                bq_had_blank_same_depth = 0;
                                while (bq_depth > 0) {
                                    sb_append(sb, "</blockquote>\n");
                                    bq_depth--;
                                }
                                in_blockquote = 0;
                                /* Fall through — let normal processing handle the line
                                 * as a regular list item */
                            }
                            else {
                                /* Lazy continuation: line continues the list item's
                                 * paragraph */
                                if (trimmed_len > 0) {
                                    sb_append_char(para_buf, '\n');
                                    const char *lc = line;
                                    size_t lcl = trimmed_len;
                                    while (lcl > 0 && (*lc == ' ' || *lc == '\t')) {
                                        lc++;
                                        lcl--;
                                    }
                                    sb_append_n(para_buf, lc, lcl);
                                    bq_has_content = 1;
                                    bq_is_paragraph = 1;
                                }
                                p = line_end + 1;
                                continue;
                            }
                        }
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
                        char fence_dc;
                        int fence_dl, fence_di;
                        const char *fence_di2;
                        size_t fence_dil;
                        int bq_is_fence = is_fence_start(
                            line, trimmed_len, &fence_dc, &fence_dl, &fence_di,
                            &fence_di2, &fence_dil
                        );
                        if (!is_ol && !is_atx_heading(line, trimmed_len) &&
                            !is_thematic_break(line, trimmed_len) && !bq_is_fence) {
                            if (trimmed_len > 0 && bq_is_paragraph) {
                                /* Lazy continuation: line is part of the blockquote
                                 * paragraph */
                                sb_append_char(para_buf, '\n');
                                const char *lc = line;
                                size_t lcl = trimmed_len;
                                while (lcl > 0 && (*lc == ' ' || *lc == '\t')) {
                                    lc++;
                                    lcl--;
                                }
                                sb_append_n(para_buf, lc, lcl);
                                bq_has_content = 1;
                                p = line_end + 1;
                                continue;
                            }
                            /* Not a paragraph: flush content, then close blockquote */
                            bq_flush_content(
                                sb, para_buf, &bq_has_content, &bq_in_indented_code,
                                refs, n_refs
                            );
                            while (bq_depth > 0) {
                                sb_append(sb, "</blockquote>\n");
                                bq_depth--;
                            }
                            in_blockquote = 0;
                            continue;
                        }
                        else {
                            if (bq_in_indented_code) {
                                sb_append(sb, "</code></pre>\n");
                                bq_in_indented_code = 0;
                            }
                            bq_flush_content(
                                sb, para_buf, &bq_has_content, &bq_in_indented_code,
                                refs, n_refs
                            );
                            while (bq_depth > 0) {
                                sb_append(sb, "</blockquote>\n");
                                bq_depth--;
                            }
                            in_blockquote = 0;
                            continue;
                        }
                    }
                }
            } /* end else (!has_bq_marker) */
        }

        size_t bq_start = 0;
        while (bq_start < trimmed_len && bq_start < 3 && line[bq_start] == ' ')
            bq_start++;
        int is_blockquote_line = (bq_start < trimmed_len && line[bq_start] == '>');

        if (para_has_content && is_blockquote_line) {
            sb_append(sb, "<p>");
            render_inline(sb, para_buf->data, para_buf->len, 1, 1, refs, n_refs);
            sb_append(sb, "</p>\n");
            para_has_content = 0;
            para_buf->len = 0;
            para_buf->data[0] = '\0';
        }

        if (is_blockquote_line) {
            if (bq_in_html_block) {
                /* Strip the blockquote prefix (> or > ) for HTML block content */
                size_t bq_skip = 0;
                while (bq_skip < trimmed_len && bq_skip < 3 && line[bq_skip] == ' ')
                    bq_skip++;
                if (bq_skip < trimmed_len && line[bq_skip] == '>') {
                    bq_skip++;
                    if (bq_skip < trimmed_len && line[bq_skip] == ' ') bq_skip++;
                }
                size_t rem_len = line_len > bq_skip ? line_len - bq_skip : 0;
                if (g_gfm_extensions & GFM_ENABLE_TAGFILTER)
                    sb_append_with_disallowed_escaped(sb, line + bq_skip, rem_len);
                else
                    sb_append_n(sb, line + bq_skip, rem_len);
                sb_append_char(sb, '\n');
                p = line_end + 1;
                continue;
            }
            if (list_depth > 0) {
                ListEntry *bq_lc = list_top(list_stack, list_depth);
                size_t bq_lc_indent = get_indent_tab(line, line_len);
                if (bq_lc_indent >= (size_t)bq_lc->content_col) {
                    if (li_content->len > 0) {
                        if (bq_lc->after_blank || bq_lc->is_loose) {
                            sb_append(sb, "\n<p>");
                            sb_append_n(sb, li_content->data, li_content->len);
                            sb_append(sb, "</p>\n");
                        }
                        else {
                            sb_append_n(sb, li_content->data, li_content->len);
                            sb_append_char(sb, '\n');
                        }
                        li_content->len = 0;
                    }
                }
                else { list_pop_to(sb, li_content, list_stack, &list_depth, 0); }
            }
            size_t qpos = bq_start;
            int marker_count = 0;
            /* Count consecutive > markers (matching cmark: for each level, skip
             * whitespace, check for >) */
            if (qpos < trimmed_len && line[qpos] == '>') {
                marker_count = 1;
                qpos++;
                if (qpos < trimmed_len && (line[qpos] == ' ' || line[qpos] == '\t'))
                    qpos++;
                while (qpos < trimmed_len) {
                    size_t wp = qpos;
                    while (wp < trimmed_len && (line[wp] == ' ' || line[wp] == '\t'))
                        wp++;
                    if (wp >= trimmed_len || line[wp] != '>') break;
                    marker_count++;
                    qpos = wp + 1;
                    if (qpos < trimmed_len && (line[qpos] == ' ' || line[qpos] == '\t'))
                        qpos++;
                }
            }
            /* Adjust nesting depth: open or close blockquote tags */
            while (bq_depth < marker_count) {
                sb_append(sb, "<blockquote>\n");
                bq_depth++;
            }
            int bq_lazy = (bq_is_paragraph && marker_count < bq_depth);
            if (!bq_lazy) {
                while (bq_depth > marker_count) {
                    if (bq_in_list) {
                        if (bq_is_loose || bq_in_list_bq_open) {
                            if (sb->len > 0 && sb->data[sb->len - 1] != '\n')
                                sb_append_char(sb, '\n');
                            bq_flush_content(
                                sb, para_buf, &bq_has_content, &bq_in_indented_code,
                                refs, n_refs
                            );
                        }
                        else if (bq_has_content) {
                            render_inline(
                                sb, para_buf->data, para_buf->len, 1, 1, refs, n_refs
                            );
                            bq_has_content = 0;
                            para_buf->len = 0;
                            para_buf->data[0] = '\0';
                        }
                        if (bq_in_list_bq_open) {
                            sb_append(sb, "</blockquote>\n");
                            bq_in_list_bq_open = 0;
                        }
                        sb_append(sb, "</li>\n");
                        if (bq_in_list_type == 1) sb_append(sb, "</ul>\n");
                        else
                            sb_append(sb, "</ol>\n");
                        bq_in_list = 0;
                        bq_is_loose = 0;
                        bq_had_blank_same_depth = 0;
                    }
                    else {
                        bq_flush_content(
                            sb, para_buf, &bq_has_content, &bq_in_indented_code, refs,
                            n_refs
                        );
                    }
                    bq_is_paragraph = 0;
                    sb_append(sb, "</blockquote>\n");
                    bq_depth--;
                }
            }
            in_blockquote = (bq_depth > 0);
            if (marker_count > 0 && !bq_lazy) bq_in_html_block = 0;
            /* Check if a non-blank line at same depth continues the current bq_in_list
               item. Only close the list when bq_had_blank_same_depth is set (blank line
               preceded this content). Without a blank line, the content is a lazy
               continuation of the paragraph. */
            if (bq_in_list && marker_count == bq_depth && qpos < trimmed_len) {
                size_t bq_rem = line_len > qpos ? line_len - qpos : 0;
                size_t bq_ci = 0;
                while (bq_ci < bq_rem &&
                       (line[qpos + bq_ci] == ' ' || line[qpos + bq_ci] == '\t'))
                    bq_ci++;
                if (bq_had_blank_same_depth && bq_ci < bq_rem &&
                    bq_ci < bq_list_content_col) {
                    /* Close current list item — line doesn't continue the item */
                    if (bq_is_loose || bq_in_list_bq_open) {
                        if (sb->len > 0 && sb->data[sb->len - 1] != '\n')
                            sb_append_char(sb, '\n');
                        bq_flush_content(
                            sb, para_buf, &bq_has_content, &bq_in_indented_code, refs,
                            n_refs
                        );
                    }
                    else if (bq_has_content) {
                        render_inline(
                            sb, para_buf->data, para_buf->len, 1, 1, refs, n_refs
                        );
                        bq_has_content = 0;
                        para_buf->len = 0;
                        para_buf->data[0] = '\0';
                    }
                    if (bq_in_list_bq_open) {
                        sb_append(sb, "</blockquote>\n");
                        bq_in_list_bq_open = 0;
                    }
                    sb_append(sb, "</li>\n");
                    if (bq_in_list_type == 1) sb_append(sb, "</ul>\n");
                    else
                        sb_append(sb, "</ol>\n");
                    bq_in_list = 0;
                    bq_is_loose = 0;
                    bq_had_blank_same_depth = 0;
                }
            }
            /* content_col tracks the column offset after the last > marker */
            size_t content_col = 0;
            if (qpos < trimmed_len) {
                size_t rem = line_len > qpos ? line_len - qpos : 0;
                size_t indent = get_indent_tab_from(line + qpos, rem, content_col);
                if (indent >= 4 && !bq_in_list) {
                    if (bq_in_indented_code) {
                        size_t leftover = 0;
                        size_t consumed =
                            consume_indent(line + qpos, rem, content_col, &leftover);
                        for (size_t k = 0; k < leftover; k++) sb_append_char(sb, ' ');
                        sb_append_n(sb, line + qpos + consumed, rem - consumed);
                        sb_append(sb, "\n");
                    }
                    else {
                        bq_flush_content(
                            sb, para_buf, &bq_has_content, &bq_in_indented_code, refs,
                            n_refs
                        );
                        bq_in_indented_code = 1;
                        bq_is_paragraph = 0;
                        sb_append(sb, "<pre><code>");
                        size_t leftover = 0;
                        size_t consumed =
                            consume_indent(line + qpos, rem, content_col, &leftover);
                        for (size_t k = 0; k < leftover; k++) sb_append_char(sb, ' ');
                        sb_append_n(sb, line + qpos + consumed, rem - consumed);
                        sb_append(sb, "\n");
                    }
                }
                else {
                    if (bq_in_indented_code) {
                        sb_append(sb, "</code></pre>\n");
                        bq_in_indented_code = 0;
                    }
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
                    size_t content_start_pos = qpos + render_start;
                    size_t content_len = end_pos - render_start;
                    const char *bq_content = line + content_start_pos;

                    /* Check for fenced code inside blockquote */
                    if (!fence_in_blockquote) {
                        char fc;
                        int fl, fi;
                        const char *finfo;
                        size_t finfo_len;
                        if (is_fence_start(
                                bq_content, content_len, &fc, &fl, &fi, &finfo,
                                &finfo_len
                            )) {
                            bq_flush_content(
                                sb, para_buf, &bq_has_content, &bq_in_indented_code,
                                refs, n_refs
                            );
                            in_fence = 1;
                            fence_char = fc;
                            fence_len = fl;
                            fence_open_indent = fi;
                            fence_in_blockquote = 1;
                            sb_append(sb, "<pre><code>");
                            /* Opening fence line itself is not content */
                            p = line_end + 1;
                            continue;
                            p = line_end + 1;
                            continue;
                        }
                    }

                    /* Check for ATX heading inside blockquote */
                    int bq_heading = is_atx_heading(bq_content, content_len);
                    if (bq_heading > 0) {
                        bq_flush_content(
                            sb, para_buf, &bq_has_content, &bq_in_indented_code, refs,
                            n_refs
                        );
                        size_t hs = 0;
                        while (hs < content_len &&
                               (bq_content[hs] == ' ' || bq_content[hs] == '\t'))
                            hs++;
                        while (hs < content_len && bq_content[hs] == '#') hs++;
                        while (hs < content_len &&
                               (bq_content[hs] == ' ' || bq_content[hs] == '\t'))
                            hs++;
                        size_t he = atx_content_end(bq_content, content_len);
                        sb_append_char(sb, '<');
                        sb_append_char(sb, 'h');
                        sb_append_char(sb, (char)('0' + bq_heading));
                        sb_append_char(sb, '>');
                        if (he > hs)
                            render_inline(
                                sb, bq_content + hs, he - hs, 1, 1, refs, n_refs
                            );
                        sb_append(sb, "</h");
                        sb_append_char(sb, (char)('0' + bq_heading));
                        sb_append(sb, ">\n");
                    }
                    else if (is_thematic_break(bq_content, content_len)) {
                        bq_flush_content(
                            sb, para_buf, &bq_has_content, &bq_in_indented_code, refs,
                            n_refs
                        );
                        sb_append(sb, "<hr />\n");
                    }
                    else {
                        /* Check for HTML block inside blockquote */
                        int bq_hb = is_html_block_start(bq_content, content_len);
                        if (bq_hb > 0) {
                            bq_flush_content(
                                sb, para_buf, &bq_has_content, &bq_in_indented_code,
                                refs, n_refs
                            );
                            bq_in_html_block = 1;
                            if (g_gfm_extensions & GFM_ENABLE_TAGFILTER)
                                sb_append_with_disallowed_escaped(
                                    sb, bq_content, content_len
                                );
                            else
                                sb_append_n(sb, bq_content, content_len);
                            sb_append_char(sb, '\n');
                            p = line_end + 1;
                            continue;
                        }
                        if (bq_in_indented_code) {
                            sb_append(sb, "</code></pre>\n");
                            bq_in_indented_code = 0;
                        }
                        RefDef bq_rd;
                        if (parse_ref_def(bq_content, content_len, &bq_rd)) {
                            ref_def_free(&bq_rd);
                            if (!bq_has_content) bq_has_content = 0;
                        }
                        else {
                            /* Check for list markers in blockquote content */
                            size_t bc_indent = 0;
                            while (bc_indent < content_len && bc_indent < 3 &&
                                   bq_content[bc_indent] == ' ')
                                bc_indent++;
                            int bq_is_list = 0;
                            int bq_list_type = 0;
                            size_t bq_list_am = 0;
                            if (bc_indent < 4 && content_len > bc_indent) {
                                char c = bq_content[bc_indent];
                                if ((c == '-' || c == '*' || c == '+') &&
                                    bc_indent + 1 < content_len &&
                                    (bq_content[bc_indent + 1] == ' ' ||
                                     bq_content[bc_indent + 1] == '\t')) {
                                    bq_is_list = 1;
                                    bq_list_type = 1;
                                    bq_list_am = bc_indent + 2;
                                    while (bq_list_am < content_len &&
                                           (bq_content[bq_list_am] == ' ' ||
                                            bq_content[bq_list_am] == '\t'))
                                        bq_list_am++;
                                }
                                else {
                                    size_t ds = bc_indent;
                                    int num = 0, digit_count = 0;
                                    while (ds < content_len &&
                                           isdigit((unsigned char)bq_content[ds]) &&
                                           digit_count < 10) {
                                        num = num * 10 + (bq_content[ds] - '0');
                                        ds++;
                                        digit_count++;
                                    }
                                    if (digit_count > 0 && digit_count <= 9 &&
                                        ds < content_len &&
                                        (bq_content[ds] == '.' || bq_content[ds] == ')'
                                        ) &&
                                        ds + 1 < content_len &&
                                        (bq_content[ds + 1] == ' ' ||
                                         bq_content[ds + 1] == '\t')) {
                                        bq_is_list = 1;
                                        bq_list_type = 2;
                                        bq_list_am = ds + 2;
                                        while (bq_list_am < content_len &&
                                               (bq_content[bq_list_am] == ' ' ||
                                                bq_content[bq_list_am] == '\t'))
                                            bq_list_am++;
                                    }
                                }
                            }
                            if (bq_is_list) {
                                /* Close any existing paragraph and start list */
                                if (bq_has_content)
                                    bq_flush_content(
                                        sb, para_buf, &bq_has_content,
                                        &bq_in_indented_code, refs, n_refs
                                    );
                                bq_is_paragraph = 0;
                                if (bq_in_list) {
                                    /* Close previous list item */
                                    if (bq_in_list_bq_open) {
                                        bq_flush_content(
                                            sb, para_buf, &bq_has_content,
                                            &bq_in_indented_code, refs, n_refs
                                        );
                                    }
                                    if (bq_in_list_bq_open) {
                                        sb_append(sb, "</blockquote>\n");
                                        bq_in_list_bq_open = 0;
                                    }
                                    /* Flush item content — loose wraps in <p>, tight
                                     * renders inline */
                                    if (bq_has_content) {
                                        if (bq_is_loose || bq_had_blank_same_depth ||
                                            bq_in_list_bq_open) {
                                            if (sb->len > 0 &&
                                                sb->data[sb->len - 1] != '\n')
                                                sb_append_char(sb, '\n');
                                            bq_flush_content(
                                                sb, para_buf, &bq_has_content,
                                                &bq_in_indented_code, refs, n_refs
                                            );
                                            bq_is_loose = 1;
                                        }
                                        else {
                                            render_inline(
                                                sb, para_buf->data, para_buf->len, 1, 1,
                                                refs, n_refs
                                            );
                                            bq_has_content = 0;
                                            para_buf->len = 0;
                                            para_buf->data[0] = '\0';
                                        }
                                    }
                                    bq_had_blank_same_depth = 0;
                                    sb_append(sb, "</li>\n<li>");
                                }
                                else {
                                    if (bq_list_type == 1) sb_append(sb, "<ul>\n<li>");
                                    else
                                        sb_append(sb, "<ol>\n<li>");
                                }
                                bq_in_list = 1;
                                bq_in_list_type = bq_list_type;
                                bq_in_list_bq_open = 0;
                                bq_is_loose = 0;
                                bq_had_blank_same_depth = 0;
                                bq_list_content_col = bq_list_am;
                                /* Check for inner blockquote marker after list marker
                                 */
                                int bq_has_inner_bq =
                                    (bq_list_am < content_len &&
                                     bq_content[bq_list_am] == '>');
                                if (bq_has_inner_bq) {
                                    sb_append(sb, "\n<blockquote>\n");
                                    bq_in_list_bq_open = 1;
                                    bq_list_am++;
                                    if (bq_list_am < content_len &&
                                        bq_content[bq_list_am] == ' ')
                                        bq_list_am++;
                                }
                                size_t bq_re = content_len;
                                while (bq_re > bq_list_am &&
                                       (bq_content[bq_re - 1] == ' ' ||
                                        bq_content[bq_re - 1] == '\t'))
                                    bq_re--;
                                if (bq_list_am < bq_re) {
                                    /* Always store in para_buf to handle
                                     * blank-line-induced looseness */
                                    sb_append_n(
                                        para_buf, bq_content + bq_list_am,
                                        bq_re - bq_list_am
                                    );
                                    bq_has_content = 1;
                                    bq_is_paragraph = 1;
                                }
                            }
                            else if (bq_in_list) {
                                /* Still in list context — content within the current
                                 * list item */
                                if (bq_in_list_bq_open) {
                                    if (bq_has_content) sb_append_char(para_buf, '\n');
                                    bq_has_content = 1;
                                    bq_is_paragraph = 1;
                                    sb_append_n(para_buf, bq_content + 0, content_len);
                                }
                                else {
                                    if (bq_had_blank_same_depth) {
                                        if (sb->len > 0 &&
                                            sb->data[sb->len - 1] != '\n')
                                            sb_append_char(sb, '\n');
                                        bq_flush_content(
                                            sb, para_buf, &bq_has_content,
                                            &bq_in_indented_code, refs, n_refs
                                        );
                                        bq_is_loose = 1;
                                        bq_had_blank_same_depth = 0;
                                    }
                                    if (bq_has_content) sb_append_char(para_buf, '\n');
                                    bq_has_content = 1;
                                    bq_is_paragraph = 1;
                                    sb_append_n(para_buf, bq_content + 0, content_len);
                                }
                            }
                            else {
                                /* Normal paragraph content within blockquote */
                                if (bq_has_content) sb_append_char(para_buf, '\n');
                                bq_has_content = 1;
                                bq_is_paragraph = 1;
                                sb_append_n(para_buf, bq_content + 0, content_len);
                            }
                        }
                    }
                }
            }
            else if (bq_has_content) {
                /* Empty blockquote line (> with no content) */
                if (bq_in_list) { bq_had_blank_same_depth = 1; }
                else {
                    bq_flush_content(
                        sb, para_buf, &bq_has_content, &bq_in_indented_code, refs,
                        n_refs
                    );
                }
                bq_is_paragraph = 0;
            }
            p = line_end + 1;
            continue;
        }

        int f_len = 0;
        char f_char = 0;
        int f_indent = 0;
        const char *f_info = NULL;
        size_t f_info_len = 0;
        if (is_fence_start(
                line, trimmed_len, &f_char, &f_len, &f_indent, &f_info, &f_info_len
            )) {
            if (para_has_content) {
                sb_append(sb, "<p>");
                render_inline(sb, para_buf->data, para_buf->len, 1, 1, refs, n_refs);
                sb_append(sb, "</p>\n");
                para_has_content = 0;
                para_buf->len = 0;
                para_buf->data[0] = '\0';
            }
            if (list_depth > 0) {
                size_t fence_indent = get_indent(line, trimmed_len);
                ListEntry *fence_cur = list_top(list_stack, list_depth);
                if (fence_indent >= (size_t)fence_cur->content_col) {
                    if (fence_cur->item_open) {
                        int had_content = li_content->len > 0;
                        list_flush_content(sb, li_content, fence_cur);
                        if (had_content || sb->len == 0 ||
                            sb->data[sb->len - 1] != '\n')
                            sb_append_char(sb, '\n');
                    }
                    in_fence = 1;
                    fence_char = f_char;
                    fence_len = f_len;
                    fence_open_indent = f_indent;
                    sb_append(sb, "<pre><code");
                    if (f_info_len > 0) {
                        size_t ws = 0;
                        while (ws < f_info_len &&
                               (f_info[ws] == ' ' || f_info[ws] == '\t'))
                            ws++;
                        size_t word_start = ws;
                        size_t word_end = ws;
                        while (word_end < f_info_len && f_info[word_end] != ' ' &&
                               f_info[word_end] != '\t') {
                            if (f_info[word_end] == '\\' && word_end + 1 < f_info_len)
                                word_end += 2;
                            else
                                word_end++;
                        }
                        if (word_end > word_start) {
                            sb_append(sb, " class=\"language-");
                            for (size_t wi = word_start; wi < word_end; wi++)
                                sb_append_char(sb, f_info[wi]);
                            sb_append_char(sb, '"');
                        }
                    }
                    sb_append(sb, ">");
                    p = line_end + 1;
                    continue;
                }
                list_pop_to(sb, li_content, list_stack, &list_depth, 0);
            }
            if (in_blockquote) {
                bq_flush_content(
                    sb, para_buf, &bq_has_content, &bq_in_indented_code, refs, n_refs
                );
                while (bq_depth > 0) {
                    sb_append(sb, "</blockquote>\n");
                    bq_depth--;
                }
                in_blockquote = 0;
            }
            in_fence = 1;
            fence_char = f_char;
            fence_len = f_len;
            fence_open_indent = f_indent;
            sb_append(sb, "<pre><code");
            if (f_info_len > 0) {
                size_t ws = 0;
                while (ws < f_info_len && (f_info[ws] == ' ' || f_info[ws] == '\t'))
                    ws++;
                size_t word_start = ws;
                size_t word_end = ws;
                while (word_end < f_info_len && f_info[word_end] != ' ' &&
                       f_info[word_end] != '\t') {
                    if (f_info[word_end] == '\\' && word_end + 1 < f_info_len) {
                        word_end += 2;
                    }
                    else if (f_info[word_end] == '&') {
                        size_t consumed = 0;
                        unsigned long cps[4];
                        int n_cps = 0;
                        if (try_decode_entity(
                                f_info + word_end, f_info_len - word_end, &consumed,
                                cps, &n_cps
                            )) {
                            word_end += consumed;
                        }
                        else { word_end++; }
                    }
                    else { word_end++; }
                }
                if (word_end > word_start) {
                    sb_append(sb, " class=\"language-");
                    for (size_t wi = word_start; wi < word_end; wi++) {
                        if (f_info[wi] == '\\' && wi + 1 < f_info_len) {
                            char nxt = f_info[wi + 1];
                            if (strchr("!\"#$%&'()*+,-./:;<=>?@[\\]^_`{|}~", nxt)) {
                                sb_append_char(sb, nxt);
                                wi++;
                            }
                            else { sb_append_char(sb, f_info[wi]); }
                        }
                        else if (f_info[wi] == '&') {
                            size_t consumed = 0;
                            size_t prev = sb->len;
                            decode_entity_and_append(
                                sb, f_info + wi, f_info_len - wi, &consumed
                            );
                            if (consumed > 0) { wi += consumed - 1; }
                            else {
                                sb->len = prev;
                                sb->data[sb->len] = '\0';
                                sb_append_char(sb, '&');
                            }
                        }
                        else { sb_append_char(sb, f_info[wi]); }
                    }
                    sb_append_char(sb, '"');
                }
            }
            sb_append(sb, ">");
            p = line_end + 1;
            continue;
        }

        if (list_depth > 0 && li_content->len > 0) {
            size_t se_indent = get_indent(line, trimmed_len);
            ListEntry *se_cur = list_top(list_stack, list_depth);
            if (se_indent >= (size_t)se_cur->content_col && se_cur->item_open) {
                size_t si = se_indent;
                char se_char = line[si];
                if ((se_char == '=' || se_char == '-') && si < trimmed_len) {
                    int se_cnt = 0, se_ok = 1;
                    while (si < trimmed_len) {
                        if (line[si] == ' ' || line[si] == '\t') {
                            si++;
                            continue;
                        }
                        if (line[si] != se_char) {
                            se_ok = 0;
                            break;
                        }
                        se_cnt++;
                        si++;
                    }
                    if (se_ok && se_cnt >= 3) {
                        int se_level = (se_char == '=') ? 1 : 2;
                        sb_append_char(sb, '\n');
                        sb_append_char(sb, '<');
                        sb_append_char(sb, 'h');
                        sb_append_char(sb, (char)('0' + se_level));
                        sb_append_char(sb, '>');
                        render_inline(
                            sb, li_content->data, li_content->len, 1, 1, refs, n_refs
                        );
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
                render_inline(sb, para_buf->data, para_buf->len, 1, 1, refs, n_refs);
                sb_append(sb, "</p>\n");
                para_has_content = 0;
                para_buf->len = 0;
                para_buf->data[0] = '\0';
            }
            if (list_depth > 0) {
                size_t hr_indent = get_indent(line, trimmed_len);
                ListEntry *hr_cur = list_top(list_stack, list_depth);
                if (hr_indent >= (size_t)hr_cur->content_col) {
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
                bq_flush_content(
                    sb, para_buf, &bq_has_content, &bq_in_indented_code, refs, n_refs
                );
                while (bq_depth > 0) {
                    sb_append(sb, "</blockquote>\n");
                    bq_depth--;
                }
                in_blockquote = 0;
            }
            sb_append(sb, "<hr />\n");
            p = line_end + 1;
            continue;
        }

        int heading_level = is_atx_heading(line, trimmed_len);
        if (heading_level > 0) {
            if (para_has_content) {
                sb_append(sb, "<p>");
                render_inline(sb, para_buf->data, para_buf->len, 1, 1, refs, n_refs);
                sb_append(sb, "</p>\n");
                para_has_content = 0;
                para_buf->len = 0;
                para_buf->data[0] = '\0';
            }
            if (list_depth > 0) {
                size_t h_indent = get_indent(line, trimmed_len);
                ListEntry *h_cur = list_top(list_stack, list_depth);
                if (h_indent >= (size_t)h_cur->content_col) {
                    if (h_cur->item_open) {
                        list_flush_content(sb, li_content, h_cur);
                        if (sb->len == 0 || sb->data[sb->len - 1] != '\n')
                            sb_append_char(sb, '\n');
                    }
                }
                else { list_pop_to(sb, li_content, list_stack, &list_depth, 0); }
            }
            if (in_blockquote) {
                bq_flush_content(
                    sb, para_buf, &bq_has_content, &bq_in_indented_code, refs, n_refs
                );
                while (bq_depth > 0) {
                    sb_append(sb, "</blockquote>\n");
                    bq_depth--;
                }
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
                render_inline(sb, line + h_start, h_end - h_start, 1, 1, refs, n_refs);
            sb_append(sb, "</h");
            sb_append_char(sb, (char)('0' + heading_level));
            sb_append(sb, ">\n");
            p = line_end + 1;
            continue;
        }

        /* Unordered list items */
        {
            /* Try space-based indent first, then tab-aware for tab-indented lists */
            size_t ul_indent = get_indent(line, trimmed_len);
            size_t ul_mpos = ul_indent; /* character position of potential marker */
            size_t ul_mcol = ul_indent; /* column position of potential marker */
            char ul_c = 0;
            int is_ul = 0;
            int ul_marker_char_pos = (int)ul_indent; /* for ul_content_start calc */

            if ((ul_indent < 4 || list_depth > 0) && trimmed_len > ul_indent) {
                ul_c = line[ul_mpos];
                if (ul_c == '-' || ul_c == '*' || ul_c == '+') {
                    if (ul_mpos + 1 == trimmed_len)
                        is_ul = !para_has_content || list_depth > 0;
                    else if (line[ul_mpos + 1] == ' ' || line[ul_mpos + 1] == '\t')
                        is_ul = 1;
                }
            }

            /* If space-based indent found no marker and there are tabs, try tab-aware
             */
            if (!is_ul && (list_depth > 0 || !para_has_content)) {
                size_t ws_pos = skip_ws(line, trimmed_len);
                if (ws_pos > ul_indent && ws_pos < trimmed_len) {
                    char c = line[ws_pos];
                    if ((c == '-' || c == '*' || c == '+') &&
                        (ws_pos + 1 >= trimmed_len || line[ws_pos + 1] == ' ' ||
                         line[ws_pos + 1] == '\t')) {
                        size_t ws_col = col_at_pos(line, ws_pos);
                        if (ws_col < 4 || list_depth > 0) {
                            is_ul = 1;
                            ul_c = c;
                            ul_mcol = ws_col;
                            ul_marker_char_pos = (int)ws_pos;
                        }
                    }
                }
            }

            if (is_ul) {
                int mc = (int)ul_mcol;
                /* Check if marker indent exceeds sibling range but is below content
                 * column → continuation text */
                if (list_depth > 0) {
                    ListEntry *top = list_top(list_stack, list_depth);
                    if (mc >= top->marker_col + 4 && mc < top->content_col) is_ul = 0;
                }
                if (!is_ul) goto after_ul;
                int cc;
                size_t sp;
                size_t marker_end = (size_t)(ul_marker_char_pos);
                size_t sep_pos = marker_end + 1;
                if (sep_pos < orig_line_len && orig_line[sep_pos] == '\t') {
                    /* Tab after marker: content column is mc+2 (treating tab as a
                       space) Extra virtual columns from tab expansion become part of
                       content indent */
                    cc = mc + 2;
                    sp = sep_pos + 1;
                }
                else if (sep_pos < trimmed_len && line[sep_pos] == ' ') {
                    cc = mc + 2;
                    sp = sep_pos + 1;
                    while (sp < trimmed_len && (line[sp] == ' ' || line[sp] == '\t')) {
                        if (line[sp] == ' ') cc++;
                        else
                            cc = (cc + 4) & ~3;
                        sp++;
                    }
                }
                else {
                    /* No space/tab after marker — use mc+2 for empty items */
                    cc = mc + 2;
                    sp = sep_pos;
                }

                if (para_has_content && list_depth == 0) {
                    sb_append(sb, "<p>");
                    render_inline(
                        sb, para_buf->data, para_buf->len, 1, 1, refs, n_refs
                    );
                    sb_append(sb, "</p>\n");
                    para_has_content = 0;
                    para_buf->len = 0;
                    para_buf->data[0] = '\0';
                }

                int slot = -1, sub = 0;
                if (list_depth > 0) {
                    for (int d = list_depth - 1; d >= 0; d--) {
                        if (mc >= list_stack[d].marker_col &&
                            mc < list_stack[d].marker_col + 4) {
                            /* Marker is within 3 spaces of this list's start — valid
                             * sibling range */
                            if (mc >= list_stack[d].content_col) {
                                if (!list_stack[d].after_blank ||
                                    list_stack[d].type != 1 ||
                                    list_stack[d].marker != ul_c) {
                                    slot = d;
                                    sub = 1;
                                    break;
                                }
                                slot = d;
                                sub = 0;
                                break;
                            }
                            slot = d;
                            sub = 0;
                            break;
                        }
                        else if (mc >= list_stack[d].content_col) {
                            /* Past sibling range but at or past content column —
                             * sublist */
                            slot = d;
                            sub = 1;
                            break;
                        }
                        else if (mc >= list_stack[d].marker_col) {
                            /* Past sibling range and below content column —
                             * continuation text */
                            break;
                        }
                    }
                }

                if (slot >= 0 && sub) {
                    list_pop_to(sb, li_content, list_stack, &list_depth, slot + 1);
                    ListEntry *parent = list_top(list_stack, list_depth);
                    if (parent->after_blank) {
                        parent->is_loose = 1;
                        parent->after_blank = 0;
                    }
                    if (parent->item_open) {
                        list_flush_content(sb, li_content, parent);
                        if (!parent->is_loose &&
                            (sb->len == 0 || sb->data[sb->len - 1] != '\n'))
                            sb_append_char(sb, '\n');
                    }
                    list_save_content(li_content, parent);
                    list_push(list_stack, &list_depth, 1, ul_c, 0, mc, cc, 0);
                    list_emit_open(sb, &list_stack[list_depth - 1]);
                    list_open_item(sb, &list_stack[list_depth - 1]);
                }
                else {
                    list_pop_to(
                        sb, li_content, list_stack, &list_depth, slot < 0 ? 0 : slot + 1
                    );
                    if (list_depth > 0) {
                        ListEntry *e = list_top(list_stack, list_depth);
                        if (e->after_blank) {
                            e->is_loose = 1;
                            e->after_blank = 0;
                        }
                        list_close_item(sb, li_content, e);
                        if (list_matches(e, 1, ul_c, 0)) {
                            list_stack[list_depth - 1].content_col = cc;
                            list_open_item(sb, &list_stack[list_depth - 1]);
                        }
                        else { list_pop(sb, li_content, list_stack, &list_depth); }
                    }
                    if (list_depth == 0 && mc >= 4) goto fallback_paragraph;
                    if (list_depth == 0 ||
                        !list_matches(&list_stack[list_depth - 1], 1, ul_c, 0)) {
                        list_push(list_stack, &list_depth, 1, ul_c, 0, mc, cc, 0);
                        list_emit_open(sb, &list_stack[list_depth - 1]);
                        list_open_item(sb, &list_stack[list_depth - 1]);
                    }
                }

                /* Content extraction with indented code detection */
                size_t ul_content_start = sep_pos;
                while (ul_content_start < trimmed_len &&
                       (line[ul_content_start] == ' ' || line[ul_content_start] == '\t')
                )
                    ul_content_start++;
                if (ul_content_start < trimmed_len) {
                    size_t content_col = col_at_pos(line, ul_content_start);
                    if (content_col >= (size_t)(mc + 6)) {
                        /* Indented code block within list item */
                        size_t target = (size_t)(mc + 6);
                        size_t code_col = 0, code_pos = 0;
                        size_t leftover = 0;
                        while (code_pos < orig_line_len && code_col < target) {
                            if (orig_line[code_pos] == '\t') {
                                size_t next_col = (code_col + 4) & ~3;
                                if (next_col > target) {
                                    leftover = target - code_col;
                                    code_col = target;
                                }
                                else { code_col = next_col; }
                                code_pos++;
                            }
                            else {
                                code_col++;
                                code_pos++;
                            }
                        }
                        ListEntry *cur = list_top(list_stack, list_depth);
                        if (cur && cur->item_open) {
                            if (!cur->had_content && !cur->had_block_content)
                                cur->first_block_code = 1;
                            cur->had_block_content = 1;
                            list_flush_content(sb, li_content, cur);
                            sb_append_char(sb, '\n');
                        }
                        sb_append(sb, "<pre><code>");
                        for (size_t k = 0; k < leftover; k++) sb_append_char(sb, ' ');
                        escape_html(sb, orig_line + code_pos, orig_line_len - code_pos);
                        sb_append(sb, "\n</code></pre>\n");
                        p = line_end + 1;
                        continue;
                    }
                    else if (line[ul_content_start] == '>') {
                        /* Blockquote as initial content of a list item */
                        ListEntry *bq_init = list_top(list_stack, list_depth);
                        if (bq_init && bq_init->item_open) {
                            list_flush_content(sb, li_content, bq_init);
                            sb_append_char(sb, '\n');
                        }
                        in_blockquote = 1;
                        bq_has_content = 0;
                        bq_is_paragraph = 0;
                        bq_in_indented_code = 0;
                        size_t bq_pos = ul_content_start;
                        int bq_count = 0;
                        while (bq_pos < trimmed_len && line[bq_pos] == '>') {
                            bq_count++;
                            bq_pos++;
                            while (bq_pos < trimmed_len && line[bq_pos] == ' ')
                                bq_pos++;
                        }
                        bq_depth = bq_count;
                        for (int bi = 0; bi < bq_count; bi++)
                            sb_append(sb, "<blockquote>\n");
                        size_t bq_rem = trimmed_len - bq_pos;
                        if (bq_rem > 0) {
                            size_t bq_end = bq_rem;
                            while (bq_end > 0 && (line[bq_pos + bq_end - 1] == ' ' ||
                                                  line[bq_pos + bq_end - 1] == '\t'))
                                bq_end--;
                            if (bq_end > 0) {
                                sb_append_n(para_buf, line + bq_pos, bq_end);
                                bq_has_content = 1;
                                bq_is_paragraph = 1;
                            }
                        }
                    }
                    else if (is_fence_start(
                                 line + ul_content_start,
                                 trimmed_len - ul_content_start, &f_char, &f_len,
                                 &f_indent, &f_info, &f_info_len
                             )) {
                        ListEntry *fcur = list_top(list_stack, list_depth);
                        if (fcur && fcur->item_open) {
                            list_flush_content(sb, li_content, fcur);
                            sb_append_char(sb, '\n');
                        }
                        in_fence = 1;
                        fence_char = f_char;
                        fence_len = f_len;
                        fence_open_indent =
                            (int)col_at_pos(line, ul_content_start) + f_indent;
                        sb_append(sb, "<pre><code");
                        if (f_info_len > 0) {
                            size_t ws = 0;
                            while (ws < f_info_len &&
                                   (f_info[ws] == ' ' || f_info[ws] == '\t'))
                                ws++;
                            size_t word_start = ws;
                            size_t word_end = ws;
                            while (word_end < f_info_len && f_info[word_end] != ' ' &&
                                   f_info[word_end] != '\t') {
                                if (f_info[word_end] == '\\' &&
                                    word_end + 1 < f_info_len)
                                    word_end += 2;
                                else
                                    word_end++;
                            }
                            if (word_end > word_start) {
                                sb_append(sb, " class=\"language-");
                                for (size_t wi = word_start; wi < word_end; wi++)
                                    sb_append_char(sb, f_info[wi]);
                                sb_append_char(sb, '"');
                            }
                        }
                        sb_append(sb, ">");
                        if (fcur) fcur->had_content = 1;
                        p = line_end + 1;
                        continue;
                    }
                    else {
                        process_list_content(
                            sb, li_content, list_stack, &list_depth,
                            line + ul_content_start, trimmed_len - ul_content_start,
                            (int)ul_content_start, refs, n_refs
                        );
                    }
                }
                p = line_end + 1;
                continue;
            }
        }
    after_ul:;

        /* Ordered list items */
        {
            size_t ol_indent = get_indent(line, trimmed_len);
            if ((ol_indent < 4 || list_depth > 0) && trimmed_len > ol_indent) {
                size_t d_start = ol_indent;
                int num = 0, digit_count = 0;
                while (d_start < trimmed_len && isdigit((unsigned char)line[d_start]) &&
                       digit_count < 10) {
                    num = num * 10 + (line[d_start] - '0');
                    d_start++;
                    digit_count++;
                }
                if (digit_count > 0 && digit_count <= 9 && d_start < trimmed_len &&
                    (line[d_start] == '.' || line[d_start] == ')') &&
                    ((d_start + 1 == trimmed_len &&
                      (list_depth > 0 || !para_has_content)) ||
                     (d_start + 1 < trimmed_len &&
                      (line[d_start + 1] == ' ' || line[d_start + 1] == '\t')))) {
                    char delim = line[d_start];
                    int mc = (int)ol_indent;
                    int marker_end = (int)(d_start + 1);
                    int cc;
                    if (d_start + 1 == trimmed_len) cc = marker_end + 2;
                    else if (line[d_start + 1] == '\t')
                        cc = (marker_end + 4) & ~3;
                    else {
                        cc = marker_end + 1;
                        size_t sp = (size_t)(marker_end + 1);
                        int max_cc = marker_end + 5;
                        while (sp < trimmed_len && cc < max_cc &&
                               (line[sp] == ' ' || line[sp] == '\t')) {
                            if (line[sp] == ' ') cc++;
                            else
                                cc = (cc + 4) & ~3;
                            sp++;
                        }
                    }

                    if (para_has_content && list_depth == 0) {
                        if (num != 1) goto fallback_paragraph;
                        sb_append(sb, "<p>");
                        render_inline(
                            sb, para_buf->data, para_buf->len, 1, 1, refs, n_refs
                        );
                        sb_append(sb, "</p>\n");
                        para_has_content = 0;
                        para_buf->len = 0;
                        para_buf->data[0] = '\0';
                    }

                    int slot = -1, sub = 0;
                    if (list_depth > 0) {
                        for (int d = list_depth - 1; d >= 0; d--) {
                            if (mc >= list_stack[d].content_col) {
                                if (!list_stack[d].after_blank ||
                                    list_stack[d].type != 2 ||
                                    list_stack[d].delimiter != delim) {
                                    slot = d;
                                    sub = 1;
                                    break;
                                }
                                slot = d;
                                sub = 0;
                                break;
                            }
                            else if (mc >= list_stack[d].marker_col) {
                                if (mc >= 4 && list_stack[d].after_blank &&
                                    mc < list_stack[d].content_col) {
                                    break;
                                }
                                slot = d;
                                sub = 0;
                                break;
                            }
                        }
                    }

                    if (slot >= 0 && sub) {
                        list_pop_to(sb, li_content, list_stack, &list_depth, slot + 1);
                        ListEntry *parent = list_top(list_stack, list_depth);
                        if (parent->after_blank) {
                            parent->is_loose = 1;
                            parent->after_blank = 0;
                        }
                        if (parent->item_open) {
                            list_flush_content(sb, li_content, parent);
                            if (!parent->is_loose &&
                                (sb->len == 0 || sb->data[sb->len - 1] != '\n'))
                                sb_append_char(sb, '\n');
                        }
                        list_save_content(li_content, parent);
                        list_push(list_stack, &list_depth, 2, 0, delim, mc, cc, num);
                        list_emit_open(sb, &list_stack[list_depth - 1]);
                        list_open_item(sb, &list_stack[list_depth - 1]);
                    }
                    else {
                        list_pop_to(
                            sb, li_content, list_stack, &list_depth,
                            slot < 0 ? 0 : slot + 1
                        );
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
                            }
                            else { list_pop(sb, li_content, list_stack, &list_depth); }
                        }
                        if (list_depth == 0 && mc >= 4) goto fallback_paragraph;
                        if (list_depth == 0 ||
                            !list_matches(&list_stack[list_depth - 1], 2, 0, delim)) {
                            list_push(
                                list_stack, &list_depth, 2, 0, delim, mc, cc, num
                            );
                            list_emit_open(sb, &list_stack[list_depth - 1]);
                            list_open_item(sb, &list_stack[list_depth - 1]);
                        }
                    }

                    size_t ol_content_start = (size_t)(marker_end + 1);
                    while (ol_content_start < trimmed_len &&
                           (line[ol_content_start] == ' ' ||
                            line[ol_content_start] == '\t'))
                        ol_content_start++;
                    if (ol_content_start < trimmed_len) {
                        size_t li_content_col = col_at_pos(line, ol_content_start);
                        if (li_content_col >= (size_t)(marker_end + 5)) {
                            /* Indented code block within list item */
                            size_t target = (size_t)(marker_end + 5);
                            size_t code_col = 0, code_pos = 0;
                            size_t leftover = 0;
                            while (code_pos < orig_line_len && code_col < target) {
                                if (orig_line[code_pos] == '\t') {
                                    size_t next_col = (code_col + 4) & ~3;
                                    if (next_col > target) {
                                        leftover = target - code_col;
                                        code_col = target;
                                    }
                                    else { code_col = next_col; }
                                    code_pos++;
                                }
                                else {
                                    code_col++;
                                    code_pos++;
                                }
                            }
                            ListEntry *cur = list_top(list_stack, list_depth);
                            if (cur && cur->item_open) {
                                if (!cur->had_content && !cur->had_block_content)
                                    cur->first_block_code = 1;
                                cur->had_block_content = 1;
                                list_flush_content(sb, li_content, cur);
                                sb_append_char(sb, '\n');
                            }
                            sb_append(sb, "<pre><code>");
                            for (size_t k = 0; k < leftover; k++)
                                sb_append_char(sb, ' ');
                            escape_html(
                                sb, orig_line + code_pos, orig_line_len - code_pos
                            );
                            sb_append(sb, "\n</code></pre>\n");
                            p = line_end + 1;
                            continue;
                        }
                        else if (line[ol_content_start] == '>') {
                            /* Blockquote as initial content of a list item */
                            ListEntry *bq_init = list_top(list_stack, list_depth);
                            if (bq_init && bq_init->item_open) {
                                list_flush_content(sb, li_content, bq_init);
                                sb_append_char(sb, '\n');
                            }
                            in_blockquote = 1;
                            bq_has_content = 0;
                            bq_is_paragraph = 0;
                            bq_in_indented_code = 0;
                            size_t bq_pos = ol_content_start;
                            int bq_count = 0;
                            while (bq_pos < trimmed_len && line[bq_pos] == '>') {
                                bq_count++;
                                bq_pos++;
                                while (bq_pos < trimmed_len && line[bq_pos] == ' ')
                                    bq_pos++;
                            }
                            bq_depth = bq_count;
                            for (int bi = 0; bi < bq_count; bi++)
                                sb_append(sb, "<blockquote>\n");
                            size_t bq_rem = trimmed_len - bq_pos;
                            if (bq_rem > 0) {
                                size_t bq_end = bq_rem;
                                while (bq_end > 0 &&
                                       (line[bq_pos + bq_end - 1] == ' ' ||
                                        line[bq_pos + bq_end - 1] == '\t'))
                                    bq_end--;
                                if (bq_end > 0) {
                                    sb_append_n(para_buf, line + bq_pos, bq_end);
                                    bq_has_content = 1;
                                    bq_is_paragraph = 1;
                                }
                            }
                        }
                        else if (is_fence_start(
                                     line + ol_content_start,
                                     trimmed_len - ol_content_start, &f_char, &f_len,
                                     &f_indent, &f_info, &f_info_len
                                 )) {
                            ListEntry *fcur = list_top(list_stack, list_depth);
                            if (fcur && fcur->item_open) {
                                list_flush_content(sb, li_content, fcur);
                                sb_append_char(sb, '\n');
                            }
                            in_fence = 1;
                            fence_char = f_char;
                            fence_len = f_len;
                            fence_open_indent =
                                (int)col_at_pos(line, ol_content_start) + f_indent;
                            sb_append(sb, "<pre><code");
                            if (f_info_len > 0) {
                                size_t ws = 0;
                                while (ws < f_info_len &&
                                       (f_info[ws] == ' ' || f_info[ws] == '\t'))
                                    ws++;
                                size_t word_start = ws;
                                size_t word_end = ws;
                                while (word_end < f_info_len &&
                                       f_info[word_end] != ' ' &&
                                       f_info[word_end] != '\t') {
                                    if (f_info[word_end] == '\\' &&
                                        word_end + 1 < f_info_len)
                                        word_end += 2;
                                    else
                                        word_end++;
                                }
                                if (word_end > word_start) {
                                    sb_append(sb, " class=\"language-");
                                    for (size_t wi = word_start; wi < word_end; wi++)
                                        sb_append_char(sb, f_info[wi]);
                                    sb_append_char(sb, '"');
                                }
                            }
                            sb_append(sb, ">");
                            if (fcur) fcur->had_content = 1;
                            p = line_end + 1;
                            continue;
                        }
                        else {
                            process_list_content(
                                sb, li_content, list_stack, &list_depth,
                                line + ol_content_start, trimmed_len - ol_content_start,
                                (int)ol_content_start, refs, n_refs
                            );
                        }
                    }
                    p = line_end + 1;
                    continue;
                }
            }
        }

        /* List item continuation */
        int list_cont_handled = 0;
        if (list_depth > 0 && trimmed_len > 0) {
            if (para_has_content) {
                sb_append(sb, "<p>");
                render_inline(sb, para_buf->data, para_buf->len, 1, 1, refs, n_refs);
                sb_append(sb, "</p>\n");
                para_has_content = 0;
                para_buf->len = 0;
                para_buf->data[0] = '\0';
            }

            while (list_depth > 0) {
                ListEntry *cur = list_top(list_stack, list_depth);
                size_t line_indent = get_indent_tab(line, line_len);

                if (line_indent >= (size_t)cur->content_col) {
                    size_t extra_indent = line_indent - (size_t)cur->content_col;
                    /* Check for blockquote marker after the list indent */
                    size_t bq_mpos = 0;
                    while (bq_mpos < line_len && bq_mpos < line_indent &&
                           line[bq_mpos] == ' ')
                        bq_mpos++;
                    int has_bq_marker = (bq_mpos < trimmed_len && line[bq_mpos] == '>');
                    if (has_bq_marker) {
                        if (li_content->len > 0) {
                            if (cur->after_blank || cur->is_loose) {
                                cur->is_loose = 1;
                                sb_append(sb, "\n<p>");
                                sb_append_n(sb, li_content->data, li_content->len);
                                sb_append(sb, "</p>\n");
                            }
                            else {
                                sb_append_n(sb, li_content->data, li_content->len);
                                sb_append_char(sb, '\n');
                            }
                            li_content->len = 0;
                        }
                        cur->after_blank = 0;
                        bq_has_content = 0;
                        bq_is_paragraph = 0;
                        bq_in_indented_code = 0;
                        bq_in_html_block = 0;
                        /* Parse all consecutive > markers for nested blockquotes */
                        {
                            size_t bq_scan = bq_mpos;
                            int bq_count = 0;
                            while (bq_scan < trimmed_len && line[bq_scan] == '>') {
                                bq_count++;
                                bq_scan++;
                                while (bq_scan < trimmed_len && line[bq_scan] == ' ')
                                    bq_scan++;
                            }
                            if (!in_blockquote) in_blockquote = 1;
                            while (bq_depth < bq_count) {
                                sb_append(sb, "<blockquote>\n");
                                bq_depth++;
                            }
                            while (bq_depth > bq_count) {
                                sb_append(sb, "</blockquote>\n");
                                bq_depth--;
                            }
                        }
                        size_t after_mark = bq_mpos;
                        while (after_mark < trimmed_len && line[after_mark] == '>') {
                            after_mark++;
                            while (after_mark < trimmed_len && line[after_mark] == ' ')
                                after_mark++;
                        }
                        size_t bq_rem = trimmed_len - after_mark;
                        if (bq_rem > 0) {
                            size_t bq_end = bq_rem;
                            while (bq_end > 0 &&
                                   (line[after_mark + bq_end - 1] == ' ' ||
                                    line[after_mark + bq_end - 1] == '\t'))
                                bq_end--;
                            if (bq_end > 0) {
                                sb_append_n(para_buf, line + after_mark, bq_end);
                                bq_has_content = 1;
                                bq_is_paragraph = 1;
                            }
                        }
                        p = line_end + 1;
                        list_cont_handled = 1;
                        break;
                    }
                    /* Skip all leading whitespace before fence detection */
                    size_t fence_skip = 0;
                    size_t fence_skip_col = 0;
                    while (fence_skip < trimmed_len &&
                           (line[fence_skip] == ' ' || line[fence_skip] == '\t')) {
                        if (line[fence_skip] == ' ') fence_skip_col++;
                        else
                            fence_skip_col = (fence_skip_col + 4) & ~3;
                        fence_skip++;
                    }
                    char fence_dc = 0;
                    int fence_dl = 0, fence_di = 0;
                    const char *fence_di2 = NULL;
                    size_t fence_dil = 0;
                    int is_fence = is_fence_start(
                        line + fence_skip, trimmed_len - fence_skip, &fence_dc,
                        &fence_dl, &fence_di, &fence_di2, &fence_dil
                    );
                    if (is_fence) {
                        cur->had_content = 1;
                        if (li_content->len > 0) {
                            if (cur->after_blank || cur->is_loose) {
                                cur->is_loose = 1;
                                sb_append(sb, "\n<p>");
                                sb_append_n(sb, li_content->data, li_content->len);
                                sb_append(sb, "</p>\n");
                            }
                            else {
                                sb_append_n(sb, li_content->data, li_content->len);
                                sb_append_char(sb, '\n');
                            }
                            li_content->len = 0;
                        }
                        cur->after_blank = 0;
                        in_fence = 1;
                        fence_char = fence_dc;
                        fence_len = fence_dl;
                        fence_open_indent = (int)fence_skip_col + fence_di;
                        sb_append(sb, "<pre><code");
                        if (fence_dil > 0) {
                            size_t ws = 0;
                            while (ws < fence_dil &&
                                   (fence_di2[ws] == ' ' || fence_di2[ws] == '\t'))
                                ws++;
                            size_t word_start = ws;
                            size_t word_end = ws;
                            while (word_end < fence_dil && fence_di2[word_end] != ' ' &&
                                   fence_di2[word_end] != '\t') {
                                if (fence_di2[word_end] == '\\' &&
                                    word_end + 1 < fence_dil)
                                    word_end += 2;
                                else
                                    word_end++;
                            }
                            if (word_end > word_start) {
                                sb_append(sb, " class=\"language-");
                                for (size_t wi = word_start; wi < word_end; wi++)
                                    sb_append_char(sb, fence_di2[wi]);
                                sb_append_char(sb, '"');
                            }
                        }
                        sb_append(sb, ">");
                        p = line_end + 1;
                        list_cont_handled = 1;
                        break;
                    }
                    if (!cur->item_open) {
                        sb_append(sb, "<p>");
                        size_t scol = 0;
                        while (scol < trimmed_len &&
                               (line[scol] == ' ' || line[scol] == '\t'))
                            scol++;
                        if (scol < trimmed_len)
                            render_inline(
                                sb, line + scol, trimmed_len - scol, 1, 1, refs, n_refs
                            );
                        sb_append(sb, "</p>\n");
                    }
                    else if (cur->after_blank && li_content->len == 0) {
                        /* Check if line starts a new sibling list item */
                        size_t lscol = 0;
                        while (lscol < trimmed_len &&
                               (line[lscol] == ' ' || line[lscol] == '\t'))
                            lscol++;
                        if (lscol < trimmed_len) {
                            int same_type = 0;
                            if (cur->type == 1) {
                                if (line[lscol] == '-' || line[lscol] == '*' ||
                                    line[lscol] == '+') {
                                    size_t la = lscol + 1;
                                    if (la >= trimmed_len || line[la] == ' ' ||
                                        line[la] == '\t')
                                        same_type = 1;
                                }
                            }
                            else {
                                size_t d_start = lscol;
                                int num = 0, digit_count = 0;
                                while (d_start < trimmed_len &&
                                       isdigit((unsigned char)line[d_start]) &&
                                       digit_count < 10) {
                                    num = num * 10 + (line[d_start] - '0');
                                    d_start++;
                                    digit_count++;
                                }
                                if (digit_count > 0 && digit_count <= 9 &&
                                    d_start < trimmed_len &&
                                    (line[d_start] == '.' || line[d_start] == ')') &&
                                    line[d_start] == cur->delimiter) {
                                    same_type = 1;
                                }
                            }
                            if (same_type) {
                                /* Close current item, start a new sibling */
                                cur->is_loose = 1;
                                list_close_item(sb, li_content, cur);
                                list_open_item(sb, cur);
                                list_stack[list_depth - 1].content_col =
                                    (int)(lscol + 2);
                                /* Skip to content after marker */
                                size_t skip = lscol + 1;
                                while (skip < trimmed_len &&
                                       (line[skip] == ' ' || line[skip] == '\t'))
                                    skip++;
                                if (skip < trimmed_len) {
                                    process_list_content(
                                        sb, li_content, list_stack, &list_depth,
                                        line + skip, trimmed_len - skip, (int)skip,
                                        refs, n_refs
                                    );
                                }
                                p = line_end + 1;
                                list_cont_handled = 1;
                                break;
                            }
                        }
                        if (!cur->had_content) {
                            list_pop_to(sb, li_content, list_stack, &list_depth, 0);
                            goto fallback_paragraph;
                        }
                        if (extra_indent >= 4) {
                            cur->had_content = 1;
                            size_t target_col = (size_t)cur->content_col + 4;
                            size_t consumed = 0, col = 0, leftover = 0;
                            while (consumed < orig_line_len && col < target_col) {
                                if (orig_line[consumed] == '\t') {
                                    size_t next = (col + 4) & ~3;
                                    if (next > target_col) {
                                        leftover = target_col - col;
                                        col = target_col;
                                    }
                                    else { col = next; }
                                    consumed++;
                                }
                                else if (orig_line[consumed] == ' ') {
                                    col++;
                                    consumed++;
                                }
                                else
                                    break;
                            }
                            if (col > target_col) leftover = 0;
                            in_indented_code = 1;
                            ic_target_col = target_col;
                            sb_append(sb, "<pre><code>");
                            for (size_t k = 0; k < leftover; k++)
                                sb_append_char(sb, ' ');
                            escape_html(
                                sb, orig_line + consumed, orig_line_len - consumed
                            );
                            sb_append(sb, "\n");
                            cur->after_blank = 0;
                        }
                        else {
                            cur->is_loose = 1;
                            size_t scol = 0;
                            while (scol < trimmed_len &&
                                   (line[scol] == ' ' || line[scol] == '\t'))
                                scol++;
                            sb_append(sb, "<p>");
                            render_inline(
                                sb, line + scol, trimmed_len - scol, 1, 1, refs, n_refs
                            );
                            sb_append(sb, "</p>\n");
                            cur->after_blank = 0;
                        }
                    }
                    else if (li_content->len == 0 && extra_indent >= 4) {
                        cur->had_content = 1;
                        size_t target_col = (size_t)cur->content_col + 4;
                        size_t consumed = 0, col = 0, leftover = 0;
                        while (consumed < orig_line_len && col < target_col) {
                            if (orig_line[consumed] == '\t') {
                                size_t next = (col + 4) & ~3;
                                if (next > target_col) {
                                    leftover = target_col - col;
                                    col = target_col;
                                }
                                else { col = next; }
                                consumed++;
                            }
                            else if (orig_line[consumed] == ' ') {
                                col++;
                                consumed++;
                            }
                            else
                                break;
                        }
                        if (col > target_col) leftover = 0;
                        in_indented_code = 1;
                        ic_target_col = target_col;
                        sb_append_char(sb, '\n');
                        sb_append(sb, "<pre><code>");
                        for (size_t k = 0; k < leftover; k++) sb_append_char(sb, ' ');
                        escape_html(sb, orig_line + consumed, orig_line_len - consumed);
                        sb_append(sb, "\n");
                        cur->after_blank = 0;
                    }
                    else if (cur->after_blank &&
                             (extra_indent >= 4 ||
                              (cur->first_block_code &&
                               line_indent >= (size_t)cur->min_content_col + 4))) {
                        cur->had_content = 1;
                        if (li_content->len > 0) {
                            cur->is_loose = 1;
                            if (sb->len == 0 || sb->data[sb->len - 1] != '\n')
                                sb_append_char(sb, '\n');
                            sb_append(sb, "<p>");
                            sb_append_n(sb, li_content->data, li_content->len);
                            sb_append(sb, "</p>\n");
                            li_content->len = 0;
                        }
                        size_t target_col = extra_indent >= 4
                                                ? (size_t)cur->content_col + 4
                                                : (size_t)cur->min_content_col + 4;
                        size_t consumed = 0, col = 0, leftover = 0;
                        while (consumed < orig_line_len && col < target_col) {
                            if (orig_line[consumed] == '\t') {
                                size_t next = (col + 4) & ~3;
                                if (next > target_col) {
                                    leftover = target_col - col;
                                    col = target_col;
                                }
                                else { col = next; }
                                consumed++;
                            }
                            else if (orig_line[consumed] == ' ') {
                                col++;
                                consumed++;
                            }
                            else
                                break;
                        }
                        if (col > target_col) leftover = 0;
                        in_indented_code = 1;
                        ic_target_col = target_col;
                        sb_append(sb, "<pre><code>");
                        for (size_t k = 0; k < leftover; k++) sb_append_char(sb, ' ');
                        escape_html(sb, orig_line + consumed, orig_line_len - consumed);
                        sb_append(sb, "\n");
                        cur->after_blank = 0;
                    }
                    else if (cur->after_blank) {
                        cur->had_content = 1;
                        cur->after_blank = 0;
                        if (li_content->len > 0) {
                            cur->is_loose = 1;
                            if (sb->len == 0 || sb->data[sb->len - 1] != '\n')
                                sb_append_char(sb, '\n');
                            sb_append(sb, "<p>");
                            sb_append_n(sb, li_content->data, li_content->len);
                            sb_append(sb, "</p>\n");
                            li_content->len = 0;
                        }
                        /* Check for reference definition inside list item */
                        {
                            RefDef rd;
                            if (parse_ref_def(line, line_len, &rd)) {
                                RefDef *existing = NULL;
                                if (!find_ref(
                                        refs, n_refs, rd.label, rd.label_len, &existing
                                    )) {
                                    if (n_refs >= cap_refs) {
                                        int new_cap = cap_refs ? cap_refs * 2 : 8;
                                        RefDef *new_refs = (RefDef *)realloc(
                                            refs, new_cap * sizeof(RefDef)
                                        );
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
                                list_cont_handled = 1;
                                break;
                            }
                        }
                        size_t scol = 0;
                        while (scol < trimmed_len &&
                               (line[scol] == ' ' || line[scol] == '\t'))
                            scol++;
                        sb_append(sb, "<p>");
                        render_inline(
                            sb, line + scol, trimmed_len - scol, 1, 1, refs, n_refs
                        );
                        sb_append(sb, "</p>\n");
                    }
                    else {
                        size_t scol = 0;
                        while (scol < trimmed_len &&
                               (line[scol] == ' ' || line[scol] == '\t'))
                            scol++;
                        if (li_content->len > 0 && scol < trimmed_len)
                            sb_append_char(li_content, '\n');
                        if (scol < trimmed_len) {
                            cur->had_content = 1;
                            render_inline(
                                li_content, line + scol, trimmed_len - scol, 1, 1, refs,
                                n_refs
                            );
                        }
                    }
                    p = line_end + 1;
                    list_cont_handled = 1;
                    break;
                }

                if (list_depth > 1) {
                    ListEntry *parent = &list_stack[list_depth - 2];
                    size_t p_indent = get_indent_tab(line, line_len);
                    if (p_indent >= (size_t)parent->content_col) {
                        list_pop(sb, li_content, list_stack, &list_depth);
                        continue;
                    }
                }

                if (cur->item_open && list_depth == 1) {
                    if (cur->after_blank) {
                        if ((cur->first_block_code &&
                             line_indent >= (size_t)cur->min_content_col) ||
                            line_indent >= (size_t)cur->content_col) {
                            size_t scol = 0;
                            while (scol < trimmed_len &&
                                   (line[scol] == ' ' || line[scol] == '\t'))
                                scol++;
                            if (li_content->len > 0 && scol < trimmed_len)
                                sb_append_char(li_content, '\n');
                            if (scol < trimmed_len)
                                render_inline(
                                    li_content, line + scol, trimmed_len - scol, 1, 1,
                                    refs, n_refs
                                );
                            p = line_end + 1;
                            list_cont_handled = 1;
                            break;
                        }
                    }
                    else {
                        size_t scol = 0;
                        while (scol < trimmed_len &&
                               (line[scol] == ' ' || line[scol] == '\t'))
                            scol++;
                        if (li_content->len > 0 && scol < trimmed_len)
                            sb_append_char(li_content, '\n');
                        if (scol < trimmed_len)
                            render_inline(
                                li_content, line + scol, trimmed_len - scol, 1, 1, refs,
                                n_refs
                            );
                        p = line_end + 1;
                        list_cont_handled = 1;
                        break;
                    }
                }

                /* Check for HTML block start (e.g. <!-- --> between list items) */
                {
                    int hb_check = is_html_block_start(line, trimmed_len);
                    if (hb_check > 0) {
                        list_pop_to(sb, li_content, list_stack, &list_depth, 0);
                        in_html_block = 1;
                        html_block_type = hb_check;
                        sb_append_n(sb, line, line_len);
                        sb_append_char(sb, '\n');
                        if (hb_check == 2 && nstrstr(line, trimmed_len, "-->"))
                            in_html_block = 0;
                        p = line_end + 1;
                        list_cont_handled = 1;
                        break;
                    }
                }
                list_pop_to(sb, li_content, list_stack, &list_depth, 0);
                goto fallback_paragraph;
            }
        }
        if (list_cont_handled) continue;

        /* Check for HTML block start while paragraph is open (interrupts the
           paragraph). Types 1-5 can always interrupt; types 6-7 can also interrupt
           (spec match). */
        if (para_has_content && !in_blockquote && !in_fence) {
            int hb_type = is_html_block_start(line, trimmed_len);
            if (hb_type > 0 && hb_type != 7) {
                sb_append(sb, "<p>");
                render_inline(sb, para_buf->data, para_buf->len, 1, 1, refs, n_refs);
                sb_append(sb, "</p>\n");
                para_has_content = 0;
                para_buf->len = 0;
                para_buf->data[0] = '\0';
                in_html_block = 1;
                html_block_type = hb_type;
                if (g_gfm_extensions & GFM_ENABLE_TAGFILTER)
                    sb_append_with_disallowed_escaped(sb, line, line_len);
                else
                    sb_append_n(sb, line, line_len);
                sb_append_char(sb, '\n');
                p = line_end + 1;
                continue;
            }
        }

        /* Check for block-level HTML tag (closing or opening) that is NOT an HTML block
           but still interrupts a paragraph (e.g. </pre>, </td></tr></table>).
           After closing the paragraph, output the line as raw HTML. */
        if (trimmed_len > 0) {
            size_t blk_i = 0;
            while (blk_i < trimmed_len && blk_i < 3 && line[blk_i] == ' ') blk_i++;
            if (blk_i < trimmed_len && line[blk_i] == '<') {
                int hb2 = is_html_block_start(line, trimmed_len);
                if (hb2 == 0 && is_html_block_tag(line + blk_i, trimmed_len - blk_i)) {
                    if (para_has_content) {
                        sb_append(sb, "<p>");
                        render_inline(
                            sb, para_buf->data, para_buf->len, 1, 1, refs, n_refs
                        );
                        sb_append(sb, "</p>\n");
                        para_has_content = 0;
                        para_buf->len = 0;
                        para_buf->data[0] = '\0';
                    }
                    if (g_gfm_extensions & GFM_ENABLE_TAGFILTER)
                        sb_append_with_disallowed_escaped(sb, line, line_len);
                    else
                        sb_append_n(sb, line, line_len);
                    sb_append_char(sb, '\n');
                    p = line_end + 1;
                    continue;
                }
            }
        }

    fallback_paragraph:
        if (list_depth == 0 && !para_has_content && !in_blockquote && !in_fence &&
            !in_indented_code && !in_html_block) {
            size_t fb_indent = get_indent_tab(line, line_len);
            if (fb_indent >= 4) {
                in_indented_code = 1;
                sb_append(sb, "<pre><code>");
                size_t fb_content = content_start_at_col(orig_line, orig_line_len, 4);
                escape_html(sb, orig_line + fb_content, orig_line_len - fb_content);
                sb_append(sb, "\n");
                p = line_end + 1;
                continue;
            }
        }
        if (para_has_content &&
            (para_buf->len == 0 || para_buf->data[para_buf->len - 1] != '\n'))
            sb_append_char(para_buf, '\n');
        para_has_content = 1;

        const char *content_start = orig_line;
        size_t content_len = orig_line_len;
        if (para_buf->len == 0 || para_buf->data[para_buf->len - 1] == '\n') {
            while (content_len > 0 && (*content_start == ' ' || *content_start == '\t')
            ) {
                content_start++;
                content_len--;
            }
        }
        if (content_len > 0) sb_append_n(para_buf, content_start, content_len);
        p = line_end + 1;
    }

    if (in_blockquote) {
        if (bq_in_list) {
            if (bq_is_loose || bq_in_list_bq_open) {
                bq_flush_content(
                    sb, para_buf, &bq_has_content, &bq_in_indented_code, refs, n_refs
                );
            }
            else if (bq_has_content) {
                render_inline(sb, para_buf->data, para_buf->len, 1, 1, refs, n_refs);
                bq_has_content = 0;
                para_buf->len = 0;
                para_buf->data[0] = '\0';
            }
            if (bq_in_list_bq_open) {
                sb_append(sb, "</blockquote>\n");
                bq_in_list_bq_open = 0;
            }
            sb_append(sb, "</li>\n");
            if (bq_in_list_type == 1) sb_append(sb, "</ul>\n");
            else
                sb_append(sb, "</ol>\n");
            bq_in_list = 0;
            bq_is_loose = 0;
            bq_had_blank_same_depth = 0;
        }
        else {
            bq_flush_content(
                sb, para_buf, &bq_has_content, &bq_in_indented_code, refs, n_refs
            );
        }
        while (bq_depth > 0) {
            sb_append(sb, "</blockquote>\n");
            bq_depth--;
        }
    }
    if (para_has_content) {
        sb_append(sb, "<p>");
        render_inline(sb, para_buf->data, para_buf->len, 1, 1, refs, n_refs);
        sb_append(sb, "</p>\n");
    }
    if (in_indented_code) {
        in_indented_code = 0;
        sb_append(sb, "</code></pre>\n");
    }
    if (in_fence) {
        in_fence = 0;
        sb_append(sb, "</code></pre>\n");
    }
    list_pop_to(sb, li_content, list_stack, &list_depth, 0);

    free(expanded_line);
    sb_free(para_buf);
    sb_free(li_content);
    for (int ri = 0; ri < n_refs; ri++) ref_def_free(&refs[ri]);
    free(refs);
    return sb_detach(sb);
}
