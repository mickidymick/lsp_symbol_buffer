#include "lsp_include.h"
#include "lsp_goto_declaration.h"
#include "lsp_goto_definition.h"
#include "lsp_find_references.h"

/* global variables */
yed_plugin *Self;
yed_frame  *last_frame = NULL;
symbol     *cur_symbol;
array_t     symbols;
position    pos;
position    first_pos;
string      uri;
string      first_uri;
string      symbol_buffer_name = "*symbol-menu";
time_t      last_time;
time_t      wait_time;
int         sub = -1;
int         tot = 1;
array_t     references;
int         ref_loc;
int         has_declaration;
int         definition_num;

/* internal functions*/
static void _lsp_search_symbol(int n_args, char **args);
static void _lsp_search_txt(int n_args, char **args);
static void _lsp_search_txt_pmsg(yed_event *event);
static int  _lsp_search_txt_completion(char *name, struct yed_completion_results_t *comp_res);
static void _lsp_find_symbol_cmd(int n_args, char **args);
static void _lsp_symbol_menu_select(void);
static void _lsp_symbol_menu_line_handler(yed_event *event);
static void _lsp_symbol_menu_key_pressed_handler(yed_event *event);
static void _lsp_symbol_menu_unload(yed_plugin *self);

static string      _ws_query;
static int         _ws_pending = 0;
static array_t     _ws_symbol_names;
static set<string> _ws_symbol_name_set;
static array_t     _buf_words;
static int         _buf_words_dirty = 1;
static yed_buffer *_buf_words_buf   = NULL;
static set<int>    _ws_seeded_fts;

extern "C"
int yed_plugin_boot(yed_plugin *self) {
    YED_PLUG_VERSION_CHECK();

    Self = self;

    map<void(*)(yed_event*), vector<yed_event_kind_t> > event_handlers = {
        { goto_declaration_pmsg,                { EVENT_PLUGIN_MESSAGE }   },
        { goto_definition_pmsg,                 { EVENT_PLUGIN_MESSAGE }   },
        { find_references_pmsg,                 { EVENT_PLUGIN_MESSAGE }   },
        { _lsp_search_txt_pmsg,                 { EVENT_PLUGIN_MESSAGE }   },
        { _lsp_symbol_menu_key_pressed_handler, { EVENT_KEY_PRESSED }      },
        { _lsp_symbol_menu_line_handler,        { EVENT_LINE_PRE_DRAW }    },
    };

    for (auto &pair : event_handlers) {
        for (auto evt : pair.second) {
            yed_event_handler h;
            h.kind = evt;
            h.fn   = pair.first;
            yed_plugin_add_event_handler(self, h);
        }
    }

    map<const char*, void(*)(int, char**)> cmds = {
        { "lsp-search-symbol",        _lsp_search_symbol},
        { "lsp-search-txt",           _lsp_search_txt},
        { "lsp-find-symbol",          _lsp_find_symbol_cmd},
        { "lsp-goto-declaration",     goto_declaration_cmd},
        { "lsp-goto-definition",      goto_definition_cmd},
        { "lsp-find-references",      find_references_cmd},
        { "lsp-goto-next-reference",  goto_next_reference_cmd},
        { "lsp-goto-prev-reference",  goto_prev_reference_cmd},
    };

    for (auto &pair : cmds) {
        yed_plugin_set_command(self, pair.first, pair.second);
    }

    _ws_symbol_names = array_make(char *);
    _buf_words       = array_make(char *);
    yed_plugin_set_completion(self, "lsp-search-txt-compl-arg-0", _lsp_search_txt_completion);

    /* Fake cursor move so that it works on startup/reload. */
    yed_move_cursor_within_active_frame(0, 0);

    yed_plugin_set_unload_fn(self, _lsp_symbol_menu_unload);

    if (yed_get_var("lsp-symbol-menu-function-color") == NULL) {
        yed_set_var("lsp-symbol-menu-function-color", "&magenta");
    }

    if (yed_get_var("lsp-symbol-menu-symbol-color") == NULL) {
        yed_set_var("lsp-symbol-menu-symbol-color", "&red swap");
    }

    if (yed_get_var("lsp-symbol-menu-declaration-color") == NULL) {
        yed_set_var("lsp-symbol-menu-declaration-color", "&gray swap");
    }

    if (yed_get_var("lsp-symbol-menu-definition-color") == NULL) {
        yed_set_var("lsp-symbol-menu-definition-color", "&gray swap");
    }

    if (yed_get_var("lsp-symbol-menu-reference-color") == NULL) {
        yed_set_var("lsp-symbol-menu-reference-color", "&gray swap");
    }

    return 0;
}

static void _lsp_search_symbol(int n_args, char **args) {
    yed_buffer *buffer1;
    yed_frame  *frame;

    frame = ys->active_frame;

    if (array_len(symbols) == 0) {
        symbols = array_make(symbol *);
    }

    string tmp_str1 = "special-buffer-prepare-focus";
    YEXE((char *) tmp_str1.c_str(), SYM_BUFFER);

    if (ys->active_frame) {
        string tmp_str2 = "buffer";
        YEXE((char *) tmp_str2.c_str(), SYM_BUFFER);
    }

    yed_set_cursor_far_within_frame(ys->active_frame, 1, 1);

    _init_symbol();

    definition_num = 0;
    uri = uri_for_buffer(frame->buffer);
    pos = position_in_frame(frame);

    buffer1 = _get_or_make_buff();
    if (buffer1 != NULL) {
        buffer1->flags &= ~BUFF_RD_ONLY;

        yed_buff_clear(buffer1);
        yed_buff_insert_string_no_undo(buffer1, "Symbol Menu", 1, 1);

        buffer1->flags |= BUFF_RD_ONLY;
    }

    // fire all three requests from the original position simultaneously
    goto_declaration_request(frame);
    goto_definition_request(frame);

    cur_symbol->ref_size = 0;
    find_references_request(frame);

    buffer1 = _get_or_make_buff();
    if (buffer1 != NULL) {
        buffer1->flags &= ~BUFF_RD_ONLY;

        if (cur_symbol->declaration == NULL) {
            if (yed_buff_n_lines(buffer1) >= 7) {
                yed_line_clear_no_undo(buffer1, 6);
                yed_line_clear_no_undo(buffer1, 7);
            }
            yed_buff_insert_string_no_undo(buffer1, "Declaration", 6, 1);
            yed_buff_insert_string_no_undo(buffer1, "None Found", 7, 1);
        }

        if (cur_symbol->definition == NULL) {
            if (yed_buff_n_lines(buffer1) >= 10) {
                yed_line_clear_no_undo(buffer1, 9);
                yed_line_clear_no_undo(buffer1, 10);
            }
            yed_buff_insert_string_no_undo(buffer1, "Definition", 9, 1);
            yed_buff_insert_string_no_undo(buffer1, "None Found", 10, 1);
        }

        if (cur_symbol->ref_size == 0) {
            if (yed_buff_n_lines(buffer1) >= 13) {
                yed_line_clear_no_undo(buffer1, 12);
                yed_line_clear_no_undo(buffer1, 13);
            }
            yed_buff_insert_string_no_undo(buffer1, "References", 12, 1);
            yed_buff_insert_string_no_undo(buffer1, "None Found", 13, 1);
        }
        buffer1->flags |= BUFF_RD_ONLY;
    }
}

static void _lsp_search_txt(int n_args, char **args) {
    yed_buffer *buffer1;
    yed_frame  *frame;

    if (n_args < 1) { return; }

    frame = ys->active_frame;
    if (frame == NULL || frame->buffer == NULL) { return; }

    last_frame       = frame;
    _ws_query        = args[0];
    _ws_pending      = 1;
    _buf_words_buf   = frame->buffer;
    _buf_words_dirty = 1;

    if (array_len(symbols) == 0) {
        symbols = array_make(symbol *);
    }

    string tmp_str1 = "special-buffer-prepare-focus";
    YEXE((char *) tmp_str1.c_str(), SYM_BUFFER);

    if (ys->active_frame) {
        string tmp_str2 = "buffer";
        YEXE((char *) tmp_str2.c_str(), SYM_BUFFER);
    }

    yed_set_cursor_far_within_frame(ys->active_frame, 1, 1);

    _init_symbol();

    buffer1 = _get_or_make_buff();
    if (buffer1 != NULL) {
        buffer1->flags &= ~BUFF_RD_ONLY;
        yed_buff_clear(buffer1);
        yed_buff_insert_string_no_undo(buffer1, "Symbol Menu", 1, 1);
        buffer1->flags |= BUFF_RD_ONLY;
    }

    json params = {
        { "query", _ws_query },
    };

    yed_event event;
    string    text = params.dump();

    event.kind                       = EVENT_PLUGIN_MESSAGE;
    event.plugin_message.message_id  = "lsp-request:workspace/symbol";
    event.plugin_message.plugin_id   = "lsp_symbol_menu";
    event.plugin_message.string_data = text.c_str();
    event.ft                         = frame->buffer->ft;

    yed_trigger_event(&event);
}

static void _rebuild_buf_words_from(yed_buffer *buf) {
    char       **it;
    set<string>  seen;

    array_traverse(_buf_words, it) { free(*it); }
    array_clear(_buf_words);

    if (buf == NULL || (buf->flags & BUFF_SPECIAL)) { return; }

    int nlines = yed_buff_n_lines(buf);
    for (int row = 1; row <= nlines; row++) {
        char *text = yed_get_line_text(buf, row);
        if (text == NULL) { continue; }
        char *p = text;
        while (*p) {
            if (isalnum((unsigned char)*p) || *p == '_') {
                char *start = p;
                while (isalnum((unsigned char)*p) || *p == '_') { p++; }
                int len = (int)(p - start);
                if (len > 1 && len < 256) {
                    char word[256];
                    memcpy(word, start, len);
                    word[len] = '\0';
                    if (seen.insert(string(word)).second) {
                        char *cpy = strdup(word);
                        array_push(_buf_words, cpy);
                    }
                }
            } else {
                p++;
            }
        }
        free(text);
    }
}

static yed_buffer *_get_or_make_find_sym_buff(void) {
    yed_buffer *buff = yed_get_buffer("*lsp-find-symbol");
    if (buff == NULL) {
        buff = yed_create_buffer("*lsp-find-symbol");
        buff->flags |= BUFF_RD_ONLY | BUFF_SPECIAL;
    }
    return buff;
}

static void _lsp_find_symbol_run(void) {
    yed_buffer       *buff = _get_or_make_find_sym_buff();
    char            **it;
    set<string>       shown;
    vector<string>    matches;

    array_zero_term(ys->cmd_buff);
    char   *pattern = (char *)array_data(ys->cmd_buff);
    string  pat(pattern);

    buff->flags &= ~BUFF_RD_ONLY;
    yed_buff_clear_no_undo(buff);

    if (pat.size() > 0) {
        array_traverse(_ws_symbol_names, it) {
            string name(*it);
            if (name.find(pat) == 0 && shown.insert(name).second) {
                matches.push_back(name);
            }
        }
        array_traverse(_buf_words, it) {
            string name(*it);
            if (name.find(pat) == 0 && shown.insert(name).second) {
                matches.push_back(name);
            }
        }

        sort(matches.begin(), matches.end());

        int row = 1;
        for (const string &m : matches) {
            if (row > 50) { break; }
            yed_buff_insert_string_no_undo(buff, (char *)m.c_str(), row, 1);
            row++;
        }
    }

    buff->flags |= BUFF_RD_ONLY;
}

static void _lsp_find_symbol_select(void) {
    char query_copy[512];

    array_zero_term(ys->cmd_buff);
    snprintf(query_copy, sizeof(query_copy), "%s", (char *)array_data(ys->cmd_buff));

    ys->interactive_command = NULL;
    yed_clear_cmd_buff();
    YEXE("special-buffer-prepare-unfocus", "*lsp-find-symbol");

    if (strlen(query_copy) > 0) {
        YEXE("lsp-search-txt", query_copy);
    }
}

static void _lsp_find_symbol_take_key(int key) {
    switch (key) {
        case ESC:
        case CTRL_C:
            ys->interactive_command = NULL;
            ys->current_search      = NULL;
            yed_clear_cmd_buff();
            YEXE("special-buffer-prepare-unfocus", "*lsp-find-symbol");
            break;
        case ENTER:
            _lsp_find_symbol_select();
            break;
        default:
            yed_cmd_line_readline_take_key(NULL, key);
            array_zero_term(ys->cmd_buff);
            _lsp_find_symbol_run();
            break;
    }
}

static void _lsp_find_symbol_cmd(int n_args, char **args) {
    yed_frame *frame;
    int        key;

    if (!ys->interactive_command) {
        frame = ys->active_frame;

        // Build buf_words now before we switch away from the code buffer
        if (frame && frame->buffer && !(frame->buffer->flags & BUFF_SPECIAL)) {
            _buf_words_buf   = frame->buffer;
            _buf_words_dirty = 0;
            _rebuild_buf_words_from(frame->buffer);
        }

        ys->interactive_command = "lsp-find-symbol";
        ys->cmd_prompt          = "(lsp-find-symbol) ";

        yed_buffer *buff = _get_or_make_find_sym_buff();
        buff->flags &= ~BUFF_RD_ONLY;
        yed_buff_clear_no_undo(buff);
        buff->flags |= BUFF_RD_ONLY;

        YEXE("special-buffer-prepare-focus", "*lsp-find-symbol");
        yed_frame_set_buff(ys->active_frame, buff);
        yed_set_cursor_far_within_frame(ys->active_frame, 1, 1);
        yed_clear_cmd_buff();
    } else {
        sscanf(args[0], "%d", &key);
        _lsp_find_symbol_take_key(key);
    }
}

static void _lsp_search_txt_pmsg(yed_event *event) {
    yed_buffer *buffer1;

    if (strcmp(event->plugin_message.plugin_id, "lsp") != 0
    ||  strcmp(event->plugin_message.message_id, "workspace/symbol") != 0) {
        return;
    }

    try {
        auto j = json::parse(event->plugin_message.string_data);
        const auto &r = j["result"];

        // always update completion cache with any symbol names returned
        if (r.is_array()) {
            for (const auto &sym : r) {
                if (sym.contains("name")) {
                    string sname = sym["name"];
                    if (_ws_symbol_name_set.insert(sname).second) {
                        char *cpy = strdup(sname.c_str());
                        array_push(_ws_symbol_names, cpy);
                    }
                }
            }
        }

        if (!_ws_pending) {
            event->cancel = 1;
            return;
        }
        _ws_pending = 0;

        if (!r.is_array() || r.empty()) {
            buffer1 = _get_or_make_buff();
            if (buffer1 != NULL) {
                buffer1->flags &= ~BUFF_RD_ONLY;
                yed_buff_insert_string_no_undo(buffer1, "Declaration", 6, 1);
                yed_buff_insert_string_no_undo(buffer1, "None Found",  7, 1);
                yed_buff_insert_string_no_undo(buffer1, "Definition",  9, 1);
                yed_buff_insert_string_no_undo(buffer1, "None Found",  10, 1);
                yed_buff_insert_string_no_undo(buffer1, "References",  12, 1);
                yed_buff_insert_string_no_undo(buffer1, "None Found",  13, 1);
                buffer1->flags |= BUFF_RD_ONLY;
            }
            event->cancel = 1;
            return;
        }

        // prefer exact name match, fall back to first result
        const json *best = &r[0];
        for (const auto &sym : r) {
            if (sym.contains("name") && sym["name"] == _ws_query) {
                best = &sym;
                break;
            }
        }

        if (!best->contains("location")) {
            event->cancel = 1;
            return;
        }

        const auto &loc_json = (*best)["location"];
        uri           = loc_json["uri"];
        pos.line      = loc_json["range"]["start"]["line"];
        pos.character = loc_json["range"]["start"]["character"];

        definition_num = 0;

        // Populate definition directly from workspace/symbol location.
        // textDocument/definition from a definition site returns null in clangd.
        {
            // Convert URI to path: strip "file://" prefix
            string raw_uri = uri;
            string path_str;
            if (raw_uri.substr(0, 7) == "file://") {
                path_str = raw_uri.substr(7);
            } else {
                path_str = raw_uri;
            }

            char a_path[512], r_path[512], h_path[512];
            char *name;
            abs_path((char *)path_str.c_str(), a_path);
            relative_path_if_subtree(a_path, r_path);
            if (homeify_path(r_path, h_path)) {
                name = h_path;
            } else {
                name = r_path;
            }

            string tmp_hidden = "buffer-hidden";
            YEXE((char *)tmp_hidden.c_str(), name);
            yed_buffer *def_buf = yed_get_buffer(name);

            if (def_buf != NULL) {
                int def_row  = (int)pos.line + 1;      // pos.line is 0-indexed
                int def_byte = (int)pos.character;
                yed_line *def_line = yed_buff_get_line(def_buf, def_row);

                if (def_line != NULL) {
                    int def_col     = yed_line_idx_to_col(def_line, def_byte);
                    int def_end_col = def_col; // end unknown from workspace/symbol; use same

                    cur_symbol->definition           = (item *)malloc(sizeof(item));
                    cur_symbol->definition->buffer   = def_buf;
                    cur_symbol->definition->line     = yed_get_line_text(def_buf, def_row);
                    cur_symbol->definition->row      = def_row;
                    cur_symbol->definition->col      = def_col;
                    cur_symbol->definition->start    = def_col;
                    cur_symbol->definition->end      = def_end_col;
                    cur_symbol->definition->num_len  = (int)to_string(def_row).length() + 1;

                    string str(def_buf->name);
                    cur_symbol->definition->name_len = (int)str.size();

                    char tmp_str[512];
                    char *raw_line = cur_symbol->definition->line;
                    char *trimmed  = trim_leading_whitespace(raw_line);
                    int   ws_off   = (int)(trimmed - raw_line);
                    sprintf(tmp_str, "%s:%d:%s", def_buf->name, def_row, trimmed);

                    cur_symbol->definition->start = def_col - ws_off;
                    cur_symbol->definition->end   = def_end_col - ws_off;

                    buffer1 = _get_or_make_buff();
                    if (buffer1 != NULL) {
                        buffer1->flags &= ~BUFF_RD_ONLY;
                        if (yed_buff_n_lines(buffer1) >= 10) {
                            yed_line_clear_no_undo(buffer1, 9);
                            yed_line_clear_no_undo(buffer1, 10);
                        }
                        yed_buff_insert_string_no_undo(buffer1, "Definition", 9, 1);
                        yed_buff_insert_string_no_undo(buffer1, tmp_str, 10, 1);
                        buffer1->flags |= BUFF_RD_ONLY;
                    }
                }
            }
        }

        // server is confirmed running — seed the completion cache if not done yet
        if (last_frame != NULL && last_frame->buffer != NULL) {
            int ft = last_frame->buffer->ft;
            if (ft != FT_UNKNOWN && !_ws_seeded_fts.count(ft)) {
                _ws_seeded_fts.insert(ft);

                json      seed_params = { { "query", "" } };
                yed_event seed_event;
                string    seed_text   = seed_params.dump();

                seed_event.kind                       = EVENT_PLUGIN_MESSAGE;
                seed_event.plugin_message.message_id  = "lsp-request:workspace/symbol";
                seed_event.plugin_message.plugin_id   = "lsp_symbol_menu";
                seed_event.plugin_message.string_data = seed_text.c_str();
                seed_event.ft                         = ft;

                yed_trigger_event(&seed_event);
            }
        }

        goto_declaration_request(last_frame);

        cur_symbol->ref_size = 0;
        find_references_request(last_frame);

        buffer1 = _get_or_make_buff();
        if (buffer1 != NULL) {
            buffer1->flags &= ~BUFF_RD_ONLY;

            if (cur_symbol->declaration == NULL) {
                if (yed_buff_n_lines(buffer1) >= 7) {
                    yed_line_clear_no_undo(buffer1, 6);
                    yed_line_clear_no_undo(buffer1, 7);
                }
                yed_buff_insert_string_no_undo(buffer1, "Declaration", 6, 1);
                yed_buff_insert_string_no_undo(buffer1, "None Found",  7, 1);
            }

            if (cur_symbol->definition == NULL) {
                if (yed_buff_n_lines(buffer1) >= 10) {
                    yed_line_clear_no_undo(buffer1, 9);
                    yed_line_clear_no_undo(buffer1, 10);
                }
                yed_buff_insert_string_no_undo(buffer1, "Definition", 9, 1);
                yed_buff_insert_string_no_undo(buffer1, "None Found", 10, 1);
            }

            if (cur_symbol->ref_size == 0) {
                if (yed_buff_n_lines(buffer1) >= 13) {
                    yed_line_clear_no_undo(buffer1, 12);
                    yed_line_clear_no_undo(buffer1, 13);
                }
                yed_buff_insert_string_no_undo(buffer1, "References", 12, 1);
                yed_buff_insert_string_no_undo(buffer1, "None Found", 13, 1);
            }

            buffer1->flags |= BUFF_RD_ONLY;
        }

    } catch (...) {}

    event->cancel = 1;
}

static int _lsp_search_txt_completion(char *name, struct yed_completion_results_t *comp_res) {
    int      ret;
    array_t  combined;
    char   **it;

    // rebuild buf_words from the active buffer only on the first Tab of each command invocation
    if (_buf_words_dirty) {
        _buf_words_dirty = 0;

        yed_buffer *scan_buf = _buf_words_buf;
        if (scan_buf == NULL && ys->active_frame && ys->active_frame->buffer
        && !(ys->active_frame->buffer->flags & BUFF_SPECIAL)) {
            scan_buf = ys->active_frame->buffer;
        }
        _rebuild_buf_words_from(scan_buf);
    }

    // merge: LSP symbols first, then buffer words not already in LSP list
    combined = array_make(char *);
    set<string> combined_seen;

    array_traverse(_ws_symbol_names, it) {
        if (combined_seen.insert(string(*it)).second) {
            array_push(combined, *it);
        }
    }
    array_traverse(_buf_words, it) {
        if (combined_seen.insert(string(*it)).second) {
            array_push(combined, *it);
        }
    }

    FN_BODY_FOR_COMPLETE_FROM_ARRAY(name, array_len(combined), (char **)array_data(combined), comp_res, ret);

    array_free(combined);   // strings are owned by _ws_symbol_names / _buf_words, not freed here
    return ret;
}


static void _lsp_symbol_menu_select(void) {
    symbol *s;
//     string  tmp_str1 = "special-buffer-prepare-unfocus";
    string  tmp_str1 = "frame-delete";
    string  tmp_str2 = "buffer";
    string  tmp_str3 = "jump-stack-push";
    int     loc;

    loc = ys->active_frame->cursor_line;

    if (loc == 7 && cur_symbol->declaration != NULL) {
        // goto declaration
//         DBG("goto declaration");
//         YEXE((char *) tmp_str1.c_str(), cur_symbol->declaration->buffer->path);
//         YEXE((char *) tmp_str1.c_str(), SYM_BUFFER);
        YEXE((char *) tmp_str1.c_str());
        YEXE((char *) tmp_str3.c_str());
        YEXE((char *) tmp_str2.c_str(), cur_symbol->declaration->buffer->path);
        yed_move_cursor_within_frame(ys->active_frame, cur_symbol->declaration->row - ys->active_frame->cursor_line, cur_symbol->declaration->col - ys->active_frame->cursor_col);

    } else if (loc == 10 && cur_symbol->definition != NULL) {
        // goto definition
//         DBG("goto definition");
//         YEXE((char *) tmp_str1.c_str(), cur_symbol->definition->buffer->path);
//         YEXE((char *) tmp_str1.c_str(), SYM_BUFFER);
        YEXE((char *) tmp_str1.c_str());
        YEXE((char *) tmp_str3.c_str());
        YEXE((char *) tmp_str2.c_str(), cur_symbol->definition->buffer->path);
        yed_move_cursor_within_frame(ys->active_frame, cur_symbol->definition->row - ys->active_frame->cursor_line, cur_symbol->definition->col - ys->active_frame->cursor_col);

    } else if (loc >= 13 && loc <= (13 + cur_symbol->ref_size - 1) && cur_symbol->ref_size >= 1) {
        // goto reference
//         DBG("goto references");
        int new_loc = loc - 13;
//         YEXE((char *) tmp_str1.c_str(), cur_symbol->references[new_loc]->buffer->path);
//         YEXE((char *) tmp_str1.c_str(), SYM_BUFFER);
        YEXE((char *) tmp_str1.c_str());
        YEXE((char *) tmp_str3.c_str());
        YEXE((char *) tmp_str2.c_str(), cur_symbol->references[new_loc]->buffer->path);
        yed_move_cursor_within_frame(ys->active_frame, cur_symbol->references[new_loc]->row - ys->active_frame->cursor_line, cur_symbol->references[new_loc]->col - ys->active_frame->cursor_col);
    }
}

static void _lsp_symbol_menu_line_handler(yed_event *event) {
    symbol    *s;
    yed_attrs *attr_tmp;
    char      *color_var;
    yed_attrs  attr;
    int        loc;
    int        start;
    int        end;
    int        shift;
    int        row;
    yed_line  *line;

    if (event->frame         == NULL
    ||  event->frame->buffer == NULL
    ||  event->frame->buffer != _get_or_make_buff()) {
        return;
    }

    row   = event->row;
    start = 0;
    end   = 0;

    if (row == 6 && cur_symbol->declaration != NULL) {
        if ((color_var = yed_get_var("lsp-symbol-menu-declaration-color"))) {
            attr     = yed_parse_attrs(color_var);
            attr_tmp = &attr;
        }

    } else if (row == 7 && cur_symbol->declaration != NULL) {
        if ((color_var = yed_get_var("lsp-symbol-menu-symbol-color"))) {
            attr     = yed_parse_attrs(color_var);
            attr_tmp = &attr;
            start    = cur_symbol->declaration->start;
            end      = cur_symbol->declaration->end;
//             shift    = cur_symbol->declaration->num_len;
            shift    = cur_symbol->declaration->num_len + 1 + cur_symbol->declaration->name_len;
        }

    } else if (row == 9 && cur_symbol->definition != NULL) {
        if ((color_var = yed_get_var("lsp-symbol-menu-definition-color"))) {
            attr     = yed_parse_attrs(color_var);
            attr_tmp = &attr;
        }

    } else if (row == 10 && cur_symbol->definition != NULL) {
        if ((color_var = yed_get_var("lsp-symbol-menu-symbol-color"))) {
            attr     = yed_parse_attrs(color_var);
            attr_tmp = &attr;
            start    = cur_symbol->definition->start;
            end      = cur_symbol->definition->end;
//             shift    = cur_symbol->definition->num_len;
            shift    = cur_symbol->definition->num_len + 1 + cur_symbol->definition->name_len;
        }

    } else if (row == 12 && cur_symbol->ref_size > 0) {
        if ((color_var = yed_get_var("lsp-symbol-menu-reference-color"))) {
            attr     = yed_parse_attrs(color_var);
            attr_tmp = &attr;
        }

    } else if (row >= 13 && row <= (13 + cur_symbol->ref_size - 1) && cur_symbol->ref_size >= 1) {
        if ((color_var = yed_get_var("lsp-symbol-menu-symbol-color"))) {
            attr     = yed_parse_attrs(color_var);
            attr_tmp = &attr;
            start    = cur_symbol->references[row - 13]->start;
            end      = cur_symbol->references[row - 13]->end;
//             shift    = cur_symbol->references[row - 13]->num_len;
            shift    = cur_symbol->references[row - 13]->num_len + 1 + cur_symbol->references[row - 13]->name_len;
        }

    } else {
        return;
    }

    line = yed_buff_get_line(event->frame->buffer, row);
    if (line == NULL) { return; }

    if (start == 0) {
        for (loc = 1; loc <= line->visual_width; loc += 1) {
            yed_eline_combine_col_attrs(event, loc, attr_tmp);
        }
    } else {
        start = start + shift;
        end   = end + shift - 1;
        for (loc = start; loc <= end; loc += 1) {
            yed_eline_combine_col_attrs(event, loc, attr_tmp);
        }
    }
}

static void _lsp_symbol_menu_key_pressed_handler(yed_event *event) {
    yed_frame *eframe;

    eframe = ys->active_frame;

    if (event->key != ENTER
    ||  ys->interactive_command
    ||  !eframe
    ||  !eframe->buffer) {
        return;
    }

    if (strcmp(eframe->buffer->name, SYM_BUFFER) == 0) {
        _lsp_symbol_menu_select();
        event->cancel = 1;
    } else if (strcmp(eframe->buffer->name, "*lsp-find-symbol") == 0) {
        _lsp_find_symbol_select();
        event->cancel = 1;
    }
}

static void _lsp_symbol_menu_unload(yed_plugin *self) {
    char **n;

    if (cur_symbol != NULL) {
        _clear_symbol();
    }

    array_traverse(_ws_symbol_names, n) { free(*n); }
    array_free(_ws_symbol_names);
    _ws_symbol_name_set.clear();

    array_traverse(_buf_words, n) { free(*n); }
    array_free(_buf_words);
}
