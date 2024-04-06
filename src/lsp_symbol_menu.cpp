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
static void _lsp_symbol_menu_select(void);
static void _lsp_symbol_menu_line_handler(yed_event *event);
static void _lsp_symbol_menu_key_pressed_handler(yed_event *event);
static void _lsp_symbol_menu_unload(yed_plugin *self);

extern "C"
int yed_plugin_boot(yed_plugin *self) {
    YED_PLUG_VERSION_CHECK();

    Self = self;

    map<void(*)(yed_event*), vector<yed_event_kind_t> > event_handlers = {
        { goto_declaration_pmsg,                { EVENT_PLUGIN_MESSAGE } },
        { goto_definition_pmsg,                 { EVENT_PLUGIN_MESSAGE } },
        { find_references_pmsg,                 { EVENT_PLUGIN_MESSAGE } },
        { _lsp_symbol_menu_key_pressed_handler, { EVENT_KEY_PRESSED }    },
        { _lsp_symbol_menu_line_handler,        { EVENT_LINE_PRE_DRAW }  },
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
        { "lsp-goto-declaration",     goto_declaration_cmd},
        { "lsp-goto-definition",      goto_definition_cmd},
        { "lsp-find-references",      find_references_cmd},
        { "lsp-goto-next-reference",  goto_next_reference_cmd},
        { "lsp-goto-prev-reference",  goto_prev_reference_cmd},
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

    // get declaration
    // symbol declaration
    goto_declaration_request(frame);

    // get references
    // any other location symbol is used
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
            shift    = cur_symbol->declaration->num_len;
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
            shift    = cur_symbol->definition->num_len;
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
            shift    = cur_symbol->references[row - 13]->num_len;
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
    ||  !eframe->buffer
    ||  strcmp(eframe->buffer->name, SYM_BUFFER)) {
        return;
    }

    _lsp_symbol_menu_select();

    event->cancel = 1;
}

static void _lsp_symbol_menu_unload(yed_plugin *self) {
    if (cur_symbol != NULL) {
        _clear_symbol();
    }
}
