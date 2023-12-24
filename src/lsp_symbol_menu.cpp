#include "lsp_include.h"
#include "lsp_symbol.h"
#include "lsp_goto_declaration.h"
#include "lsp_goto_definition.h"
#include "lsp_goto_implementation.h"
#include "lsp_find_references.h"

/* global variables */
yed_plugin *Self;
yed_frame  *last_frame = NULL;
array_t     symbols;
position    pos;
string      uri;
string      symbol_buffer_name = "*symbol-menu";
time_t      last_time;
time_t      wait_time;
int         sub = 0;
int         tot = 1;
array_t     references;
int         ref_loc;
int         has_declaration;

/* internal functions*/
static void _lsp_symbol_menu(int n_args, char **args);
static void _lsp_symbol_menu_init(void);
static void _lsp_open_symbol(int loc, symbol *s);
static void _lsp_close_symbol(void);
static void _lsp_symbol_menu_select(void);
static void _lsp_symbol_menu_line_handler(yed_event *event);
static void _lsp_symbol_menu_key_pressed_handler(yed_event *event);
static void _lsp_symbol_menu_update_handler(yed_event *event);
static void _lsp_symbol_menu_unload(yed_plugin *self);

/* internal helper functions */
static void _clear_symbols(void);

extern "C"
int yed_plugin_boot(yed_plugin *self) {
    yed_event_handler lsp_symbol_menu_key;
    yed_event_handler lsp_symbol_menu_line;
    yed_event_handler lsp_symbol_menu_update;

    YED_PLUG_VERSION_CHECK();

    Self = self;

    map<void(*)(yed_event*), vector<yed_event_kind_t> > event_handlers = {
        { symbol_pmsg,              { EVENT_PLUGIN_MESSAGE } },
        { goto_declaration_pmsg,    { EVENT_PLUGIN_MESSAGE } },
        { goto_definition_pmsg,     { EVENT_PLUGIN_MESSAGE } },
        { goto_implementation_pmsg, { EVENT_PLUGIN_MESSAGE } },
        { find_references_pmsg,     { EVENT_PLUGIN_MESSAGE } },
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
        { "lsp-symbol-menu",         _lsp_symbol_menu},
        { "lsp-goto-declaration",    goto_declaration_cmd},
        { "lsp-goto-definition",     goto_definition_cmd},
        { "lsp-find-references",     find_references_cmd},
        { "lsp-goto-next-reference", goto_next_reference_cmd},
        { "lsp-goto-prev-reference", goto_prev_reference_cmd},
    };

    for (auto &pair : cmds) {
        yed_plugin_set_command(self, pair.first, pair.second);
    }

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

    if (yed_get_var("tree-view-update-period") == NULL) {
        yed_set_var("tree-view-update-period", "5");
    }

    yed_plugin_set_unload_fn(self, _lsp_symbol_menu_unload);

    _lsp_symbol_menu_init();

    wait_time = atoi(yed_get_var("tree-view-update-period"));
    last_time = time(NULL);

    lsp_symbol_menu_key.kind = EVENT_KEY_PRESSED;
    lsp_symbol_menu_key.fn   = _lsp_symbol_menu_key_pressed_handler;
    yed_plugin_add_event_handler(self, lsp_symbol_menu_key);

    lsp_symbol_menu_line.kind = EVENT_LINE_PRE_DRAW;
    lsp_symbol_menu_line.fn   = _lsp_symbol_menu_line_handler;
    yed_plugin_add_event_handler(self, lsp_symbol_menu_line);

    lsp_symbol_menu_update.kind = EVENT_PRE_PUMP;
    lsp_symbol_menu_update.fn   = _lsp_symbol_menu_update_handler;
    yed_plugin_add_event_handler(self, lsp_symbol_menu_update);

    return 0;
}

static void _lsp_symbol_menu(int n_args, char **args) {
    if (array_len(symbols) == 0) {
        _lsp_symbol_menu_init();
    }

    string tmp_str1 = "special-buffer-prepare-focus";
    YEXE((char *) tmp_str1.c_str(), SYM_BUFFER);

    if (ys->active_frame) {
        string tmp_str2 = "buffer";
        YEXE((char *) tmp_str2.c_str(), SYM_BUFFER);
    }

    yed_set_cursor_far_within_frame(ys->active_frame, 1, 1);

    if (ys->active_frame != NULL
    &&  ys->active_frame->buffer != NULL) {
        symbol_request(ys->active_frame);
    }
}

static void _lsp_symbol_menu_init(void) {
    yed_buffer *buff;

    if (array_len(symbols) == 0) {
        symbols = array_make(symbol *);

    } else {
        _clear_symbols();
    }

    buff = _get_or_make_buff();
    buff->flags &= ~BUFF_RD_ONLY;
    yed_buff_clear_no_undo(buff);
    buff->flags |= BUFF_RD_ONLY;

    if (ys->active_frame != NULL
    &&  ys->active_frame->buffer != NULL) {
        symbol_request(ys->active_frame);
    }
}

static void _lsp_open_symbol(int loc, symbol *s) {
    yed_buffer *buffer;
    int         line_num;

    buffer   = _get_or_make_buff();
    line_num = 1;

    buffer->flags &= ~BUFF_RD_ONLY;
    yed_buff_clear(buffer);
    yed_buff_insert_string_no_undo(buffer, "<-- Back", line_num, 1);
    line_num++;
    line_num++;
    if ( s->type == 12 ) {
        yed_buff_insert_string_no_undo(buffer, "Function", line_num, 1);
    } else if ( s->type == 5 ) {
        yed_buff_insert_string_no_undo(buffer, "Class", line_num, 1);
    } else if ( s->type == 23 ) {
        yed_buff_insert_string_no_undo(buffer, "Struct", line_num, 1);
    }
    line_num++;
    yed_buff_insert_string_no_undo(buffer, s->name, line_num, 1);

    buffer->flags |= BUFF_RD_ONLY;

    pos.line      = s->row - 1;
    pos.character = s->col + 1;

    // get declaration
    // symbol declaration
    // string str;
//     DBG("goto_declaration");
    goto_declaration_request(last_frame);

    // get definition
    // symbol definition
    // str = "this";
//     DBG("goto_definition");
//     goto_definition_request(last_frame);

    // get type_definition

    // get implementation
    // trait impl in rust / java / go
//     DBG("goto_implementation");
//     goto_implementation_request(last_frame);

    // get references
    // any other location symbol is used
    // printf("%s\n", str);
//     DBG("find_references");
    s->ref_size = 0;
    find_references_request(last_frame);
}

static void _lsp_close_symbol(void) {
    yed_buffer *buffer;

    buffer = _get_or_make_buff();

    buffer->flags &= ~BUFF_RD_ONLY;
    yed_buff_clear(buffer);
    buffer->flags |= BUFF_RD_ONLY;
    _clear_symbols();
    tot = 1;

    if (last_frame != NULL) {
        symbol_request(last_frame);
    } else {
        symbol_request(ys->active_frame);
    }
}

static void _lsp_symbol_menu_select(void) {
    symbol *s;
    string  tmp_str1 = "special-buffer-prepare-unfocus";
    string  tmp_str2 = "buffer";
    string  tmp_str3 = "jump-stack-push";
    int     loc;

    if (sub == 0) {
        sub = ys->active_frame->cursor_line - 1;
        s   = *(symbol **)array_item(symbols, sub);
        if (s == NULL) {
            return;
        }

        s->declaration = NULL;
        s->definition  = NULL;

        DBG("opened: %s", s->name);
        _lsp_open_symbol(sub, s);

    } else {
        loc = ys->active_frame->cursor_line;
        s   = *(symbol **)array_item(symbols, sub);

        if (loc == 1) {
            // back
            sub = 0;
            DBG("closed: %s", s->name);
            _lsp_close_symbol();

        } else if (loc == 7 && s->declaration != NULL) {
            // goto declaration
            DBG("goto declaration");
            YEXE((char *) tmp_str1.c_str(), s->declaration->buffer->path);
            YEXE((char *) tmp_str3.c_str());
            YEXE((char *) tmp_str2.c_str(), s->declaration->buffer->path);
            if (ys->active_frame) {
                yed_move_cursor_within_frame(ys->active_frame, s->declaration->row - ys->active_frame->cursor_line, s->declaration->col - ys->active_frame->cursor_col);
            }

        } else if (loc == 10 && s->definition != NULL) {
            // goto definition
            DBG("goto definition");
            YEXE((char *) tmp_str1.c_str(), s->definition->buffer->path);
            YEXE((char *) tmp_str3.c_str());
            YEXE((char *) tmp_str2.c_str(), s->definition->buffer->path);
            if (ys->active_frame) {
                yed_move_cursor_within_frame(ys->active_frame, s->definition->row - ys->active_frame->cursor_line, s->definition->col - ys->active_frame->cursor_col);
            }

        } else if (loc >= 13 && loc <= (13 + s->ref_size - 1) && s->ref_size >= 1) {
            // goto reference
            DBG("goto references");
            int new_loc = loc - 13;
            YEXE((char *) tmp_str1.c_str(), s->references[new_loc]->buffer->path);
            YEXE((char *) tmp_str3.c_str());
            YEXE((char *) tmp_str2.c_str(), s->references[new_loc]->buffer->path);
            if (ys->active_frame) {
                yed_move_cursor_within_frame(ys->active_frame, s->references[new_loc]->row - ys->active_frame->cursor_line, s->references[new_loc]->col - ys->active_frame->cursor_col);
            }
        }
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

    if (sub == 0) {
        if (array_len(symbols) < event->row) {
            return;
        }

        if ((color_var = yed_get_var("lsp-symbol-menu-function-color"))) {
            attr     = yed_parse_attrs(color_var);
            attr_tmp = &attr;
        }

        line = yed_buff_get_line(event->frame->buffer, event->row);
        if (line == NULL) { return; }

        for (loc = 1; loc <= 6; loc += 1) {
            yed_eline_combine_col_attrs(event, loc, attr_tmp);
        }

    } else {
        s     = *(symbol **)array_item(symbols, sub);
        row   = event->row;
        start = 0;
        end   = 0;

        if (row == 3) {
            if ((color_var = yed_get_var("lsp-symbol-menu-function-color"))) {
                attr     = yed_parse_attrs(color_var);
                attr_tmp = &attr;
            }

//         } else if (row == 4 ) {
//             if ((color_var = yed_get_var("lsp-symbol-menu-symbol-color"))) {
//                 attr     = yed_parse_attrs(color_var);
//                 attr_tmp = &attr;
//             }

        } else if (row == 6 && s->declaration != NULL) {
            if ((color_var = yed_get_var("lsp-symbol-menu-declaration-color"))) {
                attr     = yed_parse_attrs(color_var);
                attr_tmp = &attr;
            }

        } else if (row == 7 && s->declaration != NULL) {
            if ((color_var = yed_get_var("lsp-symbol-menu-symbol-color"))) {
                attr     = yed_parse_attrs(color_var);
                attr_tmp = &attr;
                start    = s->declaration->start;
                end      = s->declaration->end;
                shift    = s->declaration->num_len;
            }

        } else if (row == 9 && s->definition != NULL) {
            if ((color_var = yed_get_var("lsp-symbol-menu-definition-color"))) {
                attr     = yed_parse_attrs(color_var);
                attr_tmp = &attr;
            }

        } else if (row == 10 && s->definition != NULL) {
            if ((color_var = yed_get_var("lsp-symbol-menu-symbol-color"))) {
                attr     = yed_parse_attrs(color_var);
                attr_tmp = &attr;
                start    = s->definition->start;
                end      = s->definition->end;
                shift    = s->definition->num_len;
            }

        } else if (row == 12 && s->ref_size > 0) {
            if ((color_var = yed_get_var("lsp-symbol-menu-reference-color"))) {
                attr     = yed_parse_attrs(color_var);
                attr_tmp = &attr;
            }

        } else if (row >= 13 && row <= (13 + s->ref_size - 1) && s->ref_size >= 1) {
            if ((color_var = yed_get_var("lsp-symbol-menu-symbol-color"))) {
                attr     = yed_parse_attrs(color_var);
                attr_tmp = &attr;
                start    = s->references[row - 13]->start;
                end      = s->references[row - 13]->end;
                shift    = s->references[row - 13]->num_len;
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
}

static void _lsp_symbol_menu_key_pressed_handler(yed_event *event) {
  yed_frame *eframe;

    eframe = ys->active_frame;

    if (event->key != ENTER
    ||  ys->interactive_command
    ||  !eframe
    ||  !eframe->buffer
    ||  strcmp(eframe->buffer->name, SYM_BUFFER)) {
        return;
    }

    _lsp_symbol_menu_select();

    event->cancel = 1;
}

static void _lsp_symbol_menu_update_handler(yed_event *event) {
    time_t    curr_time;

    curr_time = time(NULL);

    if (curr_time > last_time + wait_time) {
        if (ys->active_frame == NULL) {
            return;
        }
//         _lsp_symbol_menu_init();
        last_time = curr_time;
    }
}

static void _clear_symbols(void) {
    symbol **symbol_it;

    if (array_len(symbols) > 0) {
        array_traverse(symbols, symbol_it) {
            if ((*symbol_it)->declaration != NULL) {
                free((*symbol_it)->declaration->line);
                free((*symbol_it)->declaration);
            }

            if ((*symbol_it)->definition != NULL) {
                free((*symbol_it)->definition->line);
                free((*symbol_it)->definition);
            }

            for (int i = 0; i < (*symbol_it)->ref_size; i++) {
                if ((*symbol_it)->references[i] != NULL) {
                    free((*symbol_it)->references[i]->line);
//                     free((*symbol_it)->references[i]);
                }
            }

            free(*symbol_it);
        }
    }

    array_clear(symbols);
}

static void _lsp_symbol_menu_unload(yed_plugin *self) {
    _clear_symbols();
}
