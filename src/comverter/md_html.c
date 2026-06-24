#include "md_core.h"

static int skip_ws_newline(const char *text, size_t len, size_t *pos, int *has_line) {
    while (*pos < len) {
        unsigned char c = (unsigned char)text[*pos];
        if (c == ' ' || c == '\t') {
            (*pos)++;
        } else if (c == '\n' && !*has_line) {
            *has_line = 1;
            (*pos)++;
        } else if (c == '\r' && !*has_line) {
            *has_line = 1;
            (*pos)++;
            if (*pos < len && text[*pos] == '\n') (*pos)++;
        } else {
            break;
        }
    }
    return *pos;
}

int is_html_tag(const char *text, size_t len, size_t *consumed) {
    if (len == 0 || text[0] != '<') return 0;
    /* Comment: <!-->, <!--->, or <!-- ... --> (CommonMark 0.31.2) */
    if (len >= 4 && memcmp(text, "<!--", 4) == 0) {
        if (len >= 5 && text[4] == '>') { *consumed = 5; return 1; }
        if (len >= 6 && text[4] == '-' && text[5] == '>') { *consumed = 6; return 1; }
        const char *close = strstr(text + 4, "-->");
        if (close) { *consumed = (size_t)(close - text + 3); return 1; }
        return 0;
    }
    /* Processing instruction: <? ... ?> */
    if (len >= 2 && memcmp(text, "<?", 2) == 0) {
        const char *close = strstr(text + 2, "?>");
        if (close) { *consumed = (size_t)(close - text + 2); return 1; }
        return 0;
    }
    /* Declaration: <!...> */
    if (len >= 2 && memcmp(text, "<!", 2) == 0) {
        if (len >= 9 && memcmp(text, "<![CDATA[", 9) == 0) {
            const char *close = strstr(text + 9, "]]>");
            if (close) { *consumed = (size_t)(close - text + 3); return 1; }
            return 0;
        }
        if (len > 2 && text[2] >= 'A' && text[2] <= 'Z') {
            const char *close = (const char *)memchr(text + 2, '>', len - 2);
            if (close) { *consumed = (size_t)(close - text + 1); return 1; }
        }
        return 0;
    }
    /* Close tag: </tagname> — no attributes allowed per spec */
    if (len >= 3 && text[1] == '/') {
        if (!isalpha((unsigned char)text[2])) return 0;
        size_t j = 3;
        while (j < len && (isalnum((unsigned char)text[j]) || text[j] == '-')) j++;
        if (j >= len) return 0;
        if (text[j] != ' ' && text[j] != '\t' && text[j] != '>') return 0;
        while (j < len && (text[j] == ' ' || text[j] == '\t')) j++;
        if (j < len && text[j] == '>') {
            *consumed = j + 1;
            return 1;
        }
        return 0;
    }
    /* Open tag or self-closing: <tagname ...> */
    if (text[1] == '/') return 0;
    if (!isalpha((unsigned char)text[1])) return 0;
    size_t j = 2;
    while (j < len && (isalnum((unsigned char)text[j]) || text[j] == '-')) j++;
    if (j >= len) return 0;

    size_t tag_end = j;
    int has_line = 0;
    skip_ws_newline(text, len, &j, &has_line);
    if (j >= len) return 0;

    /* Check if this is a self-closing tag (no attributes) */
    if (text[j] == '>' || text[j] == '/') {
        if (text[j] == '/') {
            j++;
            /* No whitespace allowed between / and > */
        }
        if (j < len && text[j] == '>') {
            *consumed = j + 1;
            return 1;
        }
        return 0;
    }

    /* Require whitespace between tag name and first attribute */
    if (j == tag_end) return 0;

    /* Parse attributes */
    int value_parsed = 0;  /* 1 if previous iteration consumed a value (ws required before next attr) */
    while (j < len && text[j] != '>' && text[j] != '/') {
        size_t ws_start = j;
        skip_ws_newline(text, len, &j, &has_line);
        if (j >= len || text[j] == '>' || text[j] == '/') break;
        /* Require whitespace between attributes when previous had a value */
        if (value_parsed && j == ws_start) return 0;
        value_parsed = 0;

        /* Attribute name: must start with letter, :, or _ */
        unsigned char c = (unsigned char)text[j];
        if (c != ':' && c != '_' && !isalpha(c)) return 0;
        j++;
        while (j < len && (isalnum((unsigned char)text[j]) || text[j] == ':' || text[j] == '.' || text[j] == '_' || text[j] == '-')) j++;
        if (j >= len) return 0;

        /* After attribute name: check for value or end */
        skip_ws_newline(text, len, &j, &has_line);
        if (j >= len) return 0;
        if (text[j] == '>' || text[j] == '/') break;
        if (text[j] != '=') continue;  /* boolean attribute, no value */

        /* Parse = value */
        j++;
        skip_ws_newline(text, len, &j, &has_line);
        if (j >= len) return 0;

        if (text[j] == '"' || text[j] == '\'') {
            char q = text[j];
            j++;
            while (j < len && text[j] != q) {
                j++;
            }
            if (j >= len) return 0;
            j++;
        } else if (text[j] != '>' && text[j] != ' ' && text[j] != '\t' && text[j] != '\n' && text[j] != '\r') {
            while (j < len && text[j] != '>' && text[j] != ' ' && text[j] != '\t' && text[j] != '\n' && text[j] != '\r') {
                unsigned char uc = (unsigned char)text[j];
                if (uc == '"' || uc == '\'' || uc == '=' || uc == '<' || uc == '>' || uc == '`') return 0;
                j++;
            }
        } else { return 0; }
        value_parsed = 1;
    }

    skip_ws_newline(text, len, &j, &has_line);
    if (j < len && text[j] == '/') {
        j++;
    }
    if (j < len && text[j] == '>') {
        *consumed = j + 1;
        return 1;
    }
    return 0;
}

int is_html_block_tag(const char *tag, size_t remaining) {
    static const char *block_tags[] = {
        "address", "article", "aside", "base", "basefont", "blockquote",
        "body", "caption", "center", "col", "colgroup", "dd",
        "details", "dialog", "dir", "div", "dl", "dt",
        "fieldset", "figcaption", "figure", "footer", "form", "frame",
        "frameset", "h1", "h2", "h3", "h4", "h5",
        "h6", "head", "header", "hgroup", "hr", "html",
        "iframe", "legend", "li", "link", "main", "menu",
        "menuitem", "nav", "noframes", "noscript", "object", "ol",
        "p", "param", "section", "source", "summary", "table",
        "tbody", "td", "tfoot", "th", "thead", "title",
        "tr", "track", "ul", NULL
    };
    size_t name_start = 0;
    if (tag[0] == '<' && tag[1] == '/') name_start = 2;
    else if (tag[0] == '<') name_start = 1;
    else return 0;
    size_t name_end = name_start;
    while (name_end < remaining && (isalnum((unsigned char)tag[name_end]) || tag[name_end] == '-')) name_end++;
    if (name_end == name_start) return 0;
    for (int ti = 0; block_tags[ti] != NULL; ti++) {
        size_t bt_len = strlen(block_tags[ti]);
        if (name_end - name_start == bt_len) {
            int match = 1;
            for (size_t ci = 0; ci < bt_len; ci++) {
                if (tolower((unsigned char)tag[name_start + ci]) != block_tags[ti][ci]) { match = 0; break; }
            }
            if (match) return 1;
        }
    }
    return 0;
}

int is_html_block_start(const char *line, size_t len) {
    size_t i = 0;
    while (i < len && i < 3 && line[i] == ' ') i++;
    if (i >= len) return 0;
    if (line[i] != '<') return 0;
    const char *tag = line + i;
    size_t remaining = len - i;

    /* Type 1: <pre, <script, <style, <textarea */
    if (remaining > 5) {
        const char *t = tag + 1;
        size_t r = remaining - 1;
        int is_type1 = 0;
        if ((r >= 3) && (t[0] == 'p' || t[0] == 'P') && (t[1] == 'r' || t[1] == 'R') && (t[2] == 'e' || t[2] == 'E'))
            is_type1 = 1;
        else if ((r >= 6) && (t[0] == 's' || t[0] == 'S') && (t[1] == 'c' || t[1] == 'C') && (t[2] == 'r' || t[2] == 'R') && (t[3] == 'i' || t[3] == 'I') && (t[4] == 'p' || t[4] == 'P') && (t[5] == 't' || t[5] == 'T'))
            is_type1 = 1;
        else if ((r >= 5) && (t[0] == 's' || t[0] == 'S') && (t[1] == 't' || t[1] == 'T') && (t[2] == 'y' || t[2] == 'Y') && (t[3] == 'l' || t[3] == 'L') && (t[4] == 'e' || t[4] == 'E'))
            is_type1 = 1;
        else if ((r >= 8) && (t[0] == 't' || t[0] == 'T') && (t[1] == 'e' || t[1] == 'E') && (t[2] == 'x' || t[2] == 'X') && (t[3] == 't' || t[3] == 'T') && (t[4] == 'a' || t[4] == 'A') && (t[5] == 'r' || t[5] == 'R') && (t[6] == 'e' || t[6] == 'E') && (t[7] == 'a' || t[7] == 'A'))
            is_type1 = 1;
        if (is_type1) {
            size_t j = 1;
            if ((t[0] == 'p' || t[0] == 'P')) j = 4;
            else if ((t[0] == 's' || t[0] == 'S') && (t[1] == 'c' || t[1] == 'C')) j = 7;
            else if ((t[0] == 's' || t[0] == 'S') && (t[1] == 't' || t[1] == 'T')) j = 6;
            else if ((t[0] == 't' || t[0] == 'T')) j = 9;
            if (j < remaining && tag[j] != '>' && tag[j] != ' ' && tag[j] != '\t' && tag[j] != '\n' && tag[j] != '\r') return 0;
            return 1;
        }
    }

    if (remaining >= 4 && memcmp(tag, "<!--", 4) == 0) return 2;
    if (remaining >= 2 && memcmp(tag, "<?", 2) == 0) return 3;
    if (remaining >= 3 && tag[1] == '!' && tag[2] >= 'A' && tag[2] <= 'Z') return 4;
    if (remaining >= 9 && memcmp(tag, "<![CDATA[", 9) == 0) return 5;
    if (is_html_block_tag(tag, remaining)) {
        if (tag[1] == '/') return 7;
        return 6;
    }
    /* Type 7: complete open/close tag followed only by whitespace or end */
    {
        const char *rest = tag;
        size_t rest_len = remaining;
        size_t tag_consumed = 0;
        if (is_html_tag(rest, rest_len, &tag_consumed) && tag_consumed > 0) {
            size_t after = tag_consumed;
            while (after < rest_len && (rest[after] == ' ' || rest[after] == '\t')) after++;
            if (after == rest_len) return 7;
        }
    }
    return 0;
}
