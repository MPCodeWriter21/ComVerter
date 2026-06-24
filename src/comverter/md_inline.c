#include "md_core.h"

#define MAX_RUNS 1024
#define MAX_MATCHES 1024

typedef struct {
    char delim;
    size_t pos;
    int len;
    int can_open;
    int can_close;
    int used_open;
    int used_close;
} DelimRun;

typedef struct {
    int opener_run;
    int closer_run;
    int is_strong;
} EmMatch;

static int pos_inside_link(const char *text, size_t len, size_t pos) {
    size_t bracket = pos;
    while (bracket > 0 && text[bracket] != '[') {
        if (text[bracket] == '\\' && bracket > 0) bracket--;
        bracket--;
    }
    if (text[bracket] != '[') return 0;
    size_t close = bracket + 1;
    int depth = 1;
    while (close < len && depth > 0) {
        if (text[close] == '\\' && close + 1 < len) { close += 2; continue; }
        if (text[close] == '[') depth++;
        else if (text[close] == ']') { depth--; if (depth == 0) break; }
        close++;
    }
    if (depth != 0 || close >= len) return 0;
    if (close + 1 < len && text[close + 1] == '(') {
        int pd = 1;
        size_t k = close + 2;
        while (k < len && pd > 0) {
            if (text[k] == '\\' && k + 1 < len && (text[k + 1] == '(' || text[k + 1] == ')')) { k += 2; continue; }
            if (text[k] == '(') pd++;
            else if (text[k] == ')') pd--;
            k++;
        }
        if (pd == 0) return (pos >= bracket + 1 && pos < close) ? 1 : 0;
    }
    if (close + 1 < len && text[close + 1] == '[') {
        size_t r = close + 2;
        int rd = 1;
        while (r < len && rd > 0) {
            if (text[r] == '\\' && r + 1 < len) { r += 2; continue; }
            if (text[r] == '[') rd++;
            else if (text[r] == ']') rd--;
            r++;
        }
        if (rd == 0) return (pos >= bracket + 1 && pos < close) ? 1 : 0;
    }
    if (bracket > 0 && text[bracket - 1] == '!') return 0;
    if (pos >= bracket + 1 && pos < close) {
        size_t after_close = close + 1;
        return (after_close < len && text[after_close] == ']') ? 1 : 0;
    }
    return 0;
}

static int pos_inside_raw_html_or_autolink(const char *text, size_t len, size_t pos) {
    size_t lt = pos;
    while (lt > 0 && text[lt] != '<') lt--;
    if (text[lt] != '<') return 0;
    size_t gt = lt + 1;
    while (gt < len && text[gt] != '>') gt++;
    if (gt >= len) return 0;
    size_t consumed = 0;
    if (is_html_tag(text + lt, len - lt, &consumed) && lt + consumed >= gt + 1) {
        return (pos >= lt && pos <= gt) ? 1 : 0;
    }
    if (is_valid_uri_autolink(text + lt, len - lt, &consumed) && consumed > 0 && lt + consumed >= gt + 1) {
        return (pos >= lt && pos <= gt) ? 1 : 0;
    }
    if (is_valid_email_autolink(text + lt, len - lt, &consumed) && consumed > 0 && lt + consumed >= gt + 1) {
        return (pos >= lt && pos <= gt) ? 1 : 0;
    }
    return 0;
}

static int collect_delim_runs(const char *text, size_t len, DelimRun runs[], int max_runs) {
    int n = 0;
    size_t pos = 0;
    while (pos < len && n < max_runs) {
        if (text[pos] == '\\' && pos + 1 < len && (text[pos + 1] == '*' || text[pos + 1] == '_') && !is_backslash_escaped(text, len, pos)) {
            pos += 2; continue;
        }
        if (text[pos] == '*' || text[pos] == '_') {
            if (!pos_inside_code_span(text, len, pos)) {
                char delim = text[pos];
                size_t start = pos;
                int run_len = 0;
                while (pos < len && text[pos] == delim) { run_len++; pos++; }
                DelimRun *r = &runs[n++];
                r->delim = delim;
                r->pos = start;
                r->len = run_len;
                r->used_open = 0;
                r->used_close = 0;
                int inside_link = pos_inside_link(text, len, start);
                int inside_raw = pos_inside_raw_html_or_autolink(text, len, start);
                if (inside_link || inside_raw) {
                    r->can_open = 0;
                    r->can_close = 0;
                } else {
                    r->can_open = is_left_flanking(text, len, start);
                    r->can_close = is_right_flanking(text, len, start);
                    if (delim == '_') {
                        int left = r->can_open;
                        int right = r->can_close;
                        int preceded_by_punct = 0, followed_by_punct = 0;
                        if (start > 0) {
                            size_t prev = utf8_char_start(text, start - 1);
                            preceded_by_punct = is_unicode_punct(text, len, prev);
                        }
                        size_t after = start + run_len;
                        if (after < len)
                            followed_by_punct = is_unicode_punct(text, len, after);
                        r->can_open = left && (!right || preceded_by_punct);
                        r->can_close = right && (!left || followed_by_punct);
                    }
                }
            } else { pos++; }
        } else { pos++; }
    }
    return n;
}

static void render_inline_impl(StringBuilder *sb, const char *text, size_t len, int allow_emphasis, int allow_links, RefDef *refs, int n_refs) {
    DelimRun runs[MAX_RUNS];
    int n_runs = 0;
    int next_run = 0;

    EmMatch matches[MAX_MATCHES];
    int n_matches = 0;

    if (allow_emphasis) {
        n_runs = collect_delim_runs(text, len, runs, MAX_RUNS);

        int stack[MAX_RUNS];
        int sp = 0;
        int ob_star = 0, ob_und = 0;

        for (int ri = 0; ri < n_runs; ri++) {
            DelimRun *cl = &runs[ri];
            if (!cl->can_close) {
                if (cl->can_open) stack[sp++] = ri;
                continue;
            }

            int remaining_cl = cl->len - cl->used_open - cl->used_close;
            if (remaining_cl <= 0) {
                if (cl->can_open) stack[sp++] = ri;
                continue;
            }

            while (remaining_cl > 0) {
                int bottom = (cl->delim == '*') ? ob_star : ob_und;
                int found = -1;
                for (int si = sp - 1; si >= bottom; si--) {
                    int oi = stack[si];
                    DelimRun *op = &runs[oi];
                    int remaining_op = op->len - op->used_open - op->used_close;
                    if (op->delim == cl->delim && op->can_open && remaining_op > 0) {
                        int opener_can_both = op->can_open && op->can_close;
                        int closer_can_both = cl->can_open && cl->can_close;
                        if (opener_can_both || closer_can_both) {
                            int sum = remaining_op + remaining_cl;
                            if (sum % 3 == 0 && !(remaining_op % 3 == 0 && remaining_cl % 3 == 0)) {
                                continue;
                            }
                        }
                        found = si;
                        break;
                    }
                }
                if (found < 0) {
                    if (!cl->can_open) {
                        if (cl->delim == '*') ob_star = ri + 1;
                        else ob_und = ri + 1;
                    }
                    break;
                }

                int oi = stack[found];
                DelimRun *op = &runs[oi];
                int remaining_op = op->len - op->used_open - op->used_close;
                int is_strong = (remaining_cl >= 2 && remaining_op >= 2) ? 1 : 0;
                int take = is_strong ? 2 : 1;

                op->used_open += take;
                cl->used_close += take;
                remaining_cl -= take;

                if (n_matches < MAX_MATCHES) {
                    matches[n_matches].opener_run = oi;
                    matches[n_matches].closer_run = ri;
                    matches[n_matches].is_strong = is_strong;
                    n_matches++;
                }

                if (op->used_open + op->used_close >= op->len) {
                    sp = found;
                } else {
                    for (int si = found; si < sp - 1; si++) stack[si] = stack[si + 1];
                    sp--;
                    stack[sp++] = oi;
                }
            }

            if (cl->can_open && cl->used_open + cl->used_close < cl->len)
                stack[sp++] = ri;
        }
    }

    size_t i = 0;

    while (i < len) {
        if (allow_emphasis) {
            while (next_run < n_runs && runs[next_run].pos < i) next_run++;
        }
        if (allow_emphasis && next_run < n_runs && i == runs[next_run].pos) {
            DelimRun *r = &runs[next_run];
            /* Emit closing tags: matches where this run is closer, in forward order */
            for (int mi = 0; mi < n_matches; mi++) {
                if (matches[mi].closer_run == next_run)
                    sb_append(sb, matches[mi].is_strong ? "</strong>" : "</em>");
            }
            /* Emit unused delimiter chars as literal text */
            int unused = r->len - r->used_open - r->used_close;
            for (int k = 0; k < unused; k++)
                sb_append_char(sb, r->delim);
            /* Emit opening tags: matches where this run is opener, in reverse order */
            for (int mi = n_matches - 1; mi >= 0; mi--) {
                if (matches[mi].opener_run == next_run)
                    sb_append(sb, matches[mi].is_strong ? "<strong>" : "<em>");
            }
            i += r->len;
            next_run++;
            continue;
        }

        if (text[i] == '\\' && i + 1 < len) {
            char next = text[i + 1];
            if (next == '\n') {
                sb_append(sb, "<br />\n");
                i += 2;
            } else if (strchr("!\"#$%&'()*+,-./:;<=>?@[\\]^_`{|}~", next)) {
                switch (next) {
                    case '&': sb_append(sb, "&amp;"); break;
                    case '<': sb_append(sb, "&lt;"); break;
                    case '>': sb_append(sb, "&gt;"); break;
                    case '"': sb_append(sb, "&quot;"); break;
                    default: sb_append_char(sb, next); break;
                }
                i += 2;
            } else {
                sb_append_char(sb, text[i]);
                i++;
            }
            continue;
        }

        if (text[i] == '`') {
            size_t start = i;
            i++;
            int opener_ticks = 1;
            while (i < len && text[i] == '`') { opener_ticks++; i++; }
            size_t content_start = i;
            int found = 0;
            size_t j = i;
            while (j < len) {
                if (text[j] == '`') {
                    size_t k = j;
                    int closer_ticks = 0;
                    while (k < len && text[k] == '`') { closer_ticks++; k++; }
                    if (closer_ticks == opener_ticks) {
                        size_t content_end = j;
                        char norm[1024];
                        size_t norm_len = 0;
                        size_t max_norm = sizeof(norm);
                        if (content_end - content_start > max_norm) max_norm = 0;
                        int all_spaces = 1;
                        for (size_t m = content_start; m < content_end && norm_len < max_norm; m++) {
                            if (text[m] == '\r') {
                                if (m + 1 < content_end && text[m + 1] == '\n') m++;
                                norm[norm_len++] = ' ';
                            } else if (text[m] == '\n') { norm[norm_len++] = ' '; }
                            else { norm[norm_len++] = text[m]; if (text[m] != ' ') all_spaces = 0; }
                        }
                        size_t strip_start = 0;
                        size_t strip_end = norm_len;
                        if (norm_len > 1 && norm[0] == ' ' && norm[norm_len - 1] == ' ' && !all_spaces) {
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
                        i = k; found = 1; break;
                    }
                    j = k;
                } else { j++; }
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
            if (i >= 2 && text[i - 1] == ' ' && text[i - 2] == ' ') {
                while (sb->len > 0 && sb->data[sb->len - 1] == ' ') sb->len--;
                sb->data[sb->len] = '\0';
                sb_append(sb, "<br />\n");
            } else {
                while (sb->len > 0 && sb->data[sb->len - 1] == ' ') sb->len--;
                sb->data[sb->len] = '\0';
                sb_append(sb, "\n");
            }
            i++;
            continue;
        }

        if (text[i] == '!' && i + 1 < len && text[i + 1] == '[') {
            size_t link_start = i + 2;
            size_t j = link_start;
            int depth = 1;
            size_t alt_end = 0;
            while (j < len && depth > 0) {
                if (is_backslash_escaped(text, len, j)) { j++; continue; }
                if (text[j] == '[') depth++;
                else if (text[j] == ']') { depth--; if (depth == 0) alt_end = j; }
                j++;
            }
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
                    if (text[k] == '\\' && k + 1 < len && (text[k + 1] == '(' || text[k + 1] == ')' || text[k + 1] == '\\')) { k += 2; continue; }
                    if (text[k] == '(') pdepth++;
                    else if (text[k] == ')') { pdepth--; if (pdepth == 0) url_end = k; }
                    k++;
                }
                if (url_end > 0) {
                    size_t dest_start, dest_len, title_s, title_len;
                    if (parse_link_dest_title(text, paren_start, url_end, &dest_start, &dest_len, &title_s, &title_len)) {
                        sb_append(sb, "<img src=\"");
                        append_url_with_entities(sb, text + dest_start, dest_len);
                        sb_append(sb, "\" alt=\"");
                        render_alt_text(sb, text + link_start, alt_end - link_start, refs, n_refs);
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
            if (alt_end > 0 && refs && n_refs > 0) {
                size_t ref_end = 0;
                int is_collapsed = 0;
                int is_shortcut = 0;
                if (alt_end + 1 < len && text[alt_end + 1] == '[') {
                    size_t r = alt_end + 2;
                    int ref_depth = 1;
                    while (r < len && ref_depth > 0) {
                        if (is_backslash_escaped(text, len, r)) { r++; continue; }
                        if (text[r] == '[') ref_depth++;
                        else if (text[r] == ']') { ref_depth--; if (ref_depth == 0) ref_end = r; }
                        r++;
                    }
                    if (ref_end == 0) ref_end = len;
                    if (ref_end == alt_end + 2) is_collapsed = 1;
                } else if (alt_end + 1 < len && text[alt_end + 1] == ']') {
                    ref_end = alt_end + 1;
                    is_collapsed = 1;
                } else {
                    ref_end = alt_end;
                    is_collapsed = 1;
                    is_shortcut = 1;
                }
                if (ref_end > alt_end + 1 || is_shortcut) {
                    RefDef *found = NULL;
                    size_t search_start, search_len;
                    if (is_collapsed) {
                        search_start = link_start;
                        search_len = alt_end - link_start;
                    } else {
                        search_start = alt_end + 2;
                        search_len = ref_end - (alt_end + 2);
                    }
                    if (search_len > 0 && find_ref(refs, n_refs, text + search_start, search_len, &found)) {
                        sb_append(sb, "<img src=\"");
                        append_url_with_entities(sb, found->url, strlen(found->url));
                        sb_append(sb, "\" alt=\"");
                        render_alt_text(sb, text + link_start, alt_end - link_start, refs, n_refs);
                        sb_append(sb, "\"");
                        if (found->title) {
                            sb_append(sb, " title=\"");
                            append_entity_decoded(sb, found->title, strlen(found->title));
                            sb_append_char(sb, '"');
                        }
                        sb_append(sb, " />");
                        i = is_shortcut ? alt_end + 1 : ref_end + 1;
                        continue;
                    }
                }
            }
        }

        if (allow_links && text[i] == '[') {
            size_t link_start = i + 1;
            size_t j = link_start;
            int depth = 1;
            size_t text_end = 0;
            while (j < len && depth > 0) {
                if (is_backslash_escaped(text, len, j)) { j++; continue; }
                if (text[j] == '[') depth++;
                else if (text[j] == ']') { depth--; if (depth == 0) text_end = j; }
                j++;
            }
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
                    if (text[k] == '\\' && k + 1 < len && (text[k + 1] == '(' || text[k + 1] == ')' || text[k + 1] == '\\')) { k += 2; continue; }
                    if (text[k] == '(') pdepth++;
                    else if (text[k] == ')') { pdepth--; if (pdepth == 0) url_end = k; }
                    k++;
                }
                if (url_end > 0) {
                    size_t dest_start, dest_len, title_s, title_len;
                    if (parse_link_dest_title(text, paren_start, url_end, &dest_start, &dest_len, &title_s, &title_len)) {
                        sb_append(sb, "<a href=\"");
                        append_url_with_entities(sb, text + dest_start, dest_len);
                        sb_append(sb, "\"");
                        if (title_len > 0) {
                            sb_append(sb, " title=\"");
                            append_entity_decoded(sb, text + title_s, title_len);
                            sb_append_char(sb, '"');
                        }
                        sb_append(sb, ">");
                        render_inline(sb, text + link_start, text_end - link_start, 1, 0, refs, n_refs);
                        sb_append(sb, "</a>");
                        i = url_end + 1;
                        while (next_run < n_runs && runs[next_run].pos < i) next_run++;
                        continue;
                    }
                }
            }
            if (refs && n_refs > 0) {
                size_t ref_end = 0;
                int is_collapsed = 0;
                int is_shortcut = 0;
                if (text_end + 1 < len && text[text_end + 1] == '[') {
                    size_t r = text_end + 2;
                    int ref_depth = 1;
                    while (r < len && ref_depth > 0) {
                        if (is_backslash_escaped(text, len, r)) { r++; continue; }
                        if (text[r] == '[') ref_depth++;
                        else if (text[r] == ']') { ref_depth--; if (ref_depth == 0) ref_end = r; }
                        r++;
                    }
                    if (ref_end == 0) ref_end = len;
                    if (ref_end == text_end + 2) is_collapsed = 1;
                } else if (text_end + 1 < len && text[text_end + 1] == ']') {
                    ref_end = text_end + 1;
                    is_collapsed = 1;
                } else {
                    ref_end = text_end;
                    is_collapsed = 1;
                    is_shortcut = 1;
                }
                if (ref_end > text_end || is_shortcut) {
                    RefDef *found = NULL;
                    size_t search_start, search_len;
                    if (is_collapsed || is_shortcut) {
                        search_start = link_start;
                        search_len = text_end - link_start;
                    } else {
                        search_start = text_end + 2;
                        search_len = ref_end - (text_end + 2);
                    }
                    if (search_len > 0 && find_ref(refs, n_refs, text + search_start, search_len, &found)) {
                        sb_append(sb, "<a href=\"");
                        append_url_with_entities(sb, found->url, strlen(found->url));
                        sb_append(sb, "\"");
                        if (found->title) {
                            sb_append(sb, " title=\"");
                            append_entity_decoded(sb, found->title, strlen(found->title));
                            sb_append_char(sb, '"');
                        }
                        sb_append(sb, ">");
                        render_inline(sb, text + link_start, text_end - link_start, 1, 0, refs, n_refs);
                        sb_append(sb, "</a>");
                        i = ref_end + 1;
                        continue;
                    }
                }
            }
        }

        if (text[i] == '&') {
            size_t consumed = 0;
            size_t prev_len = sb->len;
            decode_entity_and_append(sb, text + i, len - i, &consumed);
            if (consumed > 0) { i += consumed; }
            else {
                sb->len = prev_len;
                sb->data[sb->len] = '\0';
                sb_append(sb, "&amp;");
                i++;
            }
            continue;
        }

        if (text[i] == '<') {
            size_t tag_consumed = 0;
            if (is_valid_uri_autolink(text + i, len - i, &tag_consumed)) {
                const char *uri = text + i + 1;
                size_t uri_len = tag_consumed - 2;
                sb_append(sb, "<a href=\"");
                for (size_t ui = 0; ui < uri_len; ui++) {
                    unsigned char uc = (unsigned char)uri[ui];
                    if (uc == '&') sb_append(sb, "&amp;");
                    else append_uri_encoded(sb, uc);
                }
                sb_append(sb, "\">");
                for (size_t ui = 0; ui < uri_len; ui++)
                    if (uri[ui] == '&') sb_append(sb, "&amp;");
                    else sb_append_char(sb, uri[ui]);
                sb_append(sb, "</a>");
                i += tag_consumed;
                continue;
            }
            if (is_valid_email_autolink(text + i, len - i, &tag_consumed)) {
                const char *email = text + i + 1;
                size_t email_len = tag_consumed - 2;
                sb_append(sb, "<a href=\"mailto:");
                for (size_t ei = 0; ei < email_len; ei++) {
                    unsigned char c = (unsigned char)email[ei];
                    if (c == '&') sb_append(sb, "&amp;");
                    else sb_append_char(sb, email[ei]);
                }
                sb_append(sb, "\">");
                for (size_t ei = 0; ei < email_len; ei++) {
                    unsigned char c = (unsigned char)email[ei];
                    if (c == '&') sb_append(sb, "&amp;");
                    else sb_append_char(sb, email[ei]);
                }
                sb_append(sb, "</a>");
                i += tag_consumed;
                continue;
            }
            if (is_html_tag(text + i, len - i, &tag_consumed)) {
                sb_append_n(sb, text + i, tag_consumed);
                i += tag_consumed;
                continue;
            }
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

void render_inline(StringBuilder *sb, const char *text, size_t len, int allow_emphasis, int allow_links, RefDef *refs, int n_refs) {
    while (len > 0 && (text[len - 1] == ' ' || text[len - 1] == '\t')) len--;
    render_inline_impl(sb, text, len, allow_emphasis, allow_links, refs, n_refs);
}
