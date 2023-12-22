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
static void _add_hidden_items(void);
static void _add_archive_extensions(void);
static void _add_image_extensions(void);
static void _clear_symbols(void);
static int  _cmpfunc(const void *a, const void *b);
// static file       *_init_file(int parent_idx, char *path, char *name,
//                               int if_dir, int num_tabs, int color_loc);

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

//     map<const char*, void(*)(int, char**)> cmds = { { "lsp-symbol-menu", lsp_symbol_menu} };

    for (auto &pair : event_handlers) {
        for (auto evt : pair.second) {
            yed_event_handler h;
            h.kind = evt;
            h.fn   = pair.first;
            yed_plugin_add_event_handler(self, h);
        }
    }

//     for (auto &pair : cmds) {
//         yed_plugin_set_command(self, pair.first, pair.second);
//     }

    /* Fake cursor move so that it works on startup/reload. */
//     yed_move_cursor_within_active_frame(0, 0);

//     yed_plugin_set_unload_fn(self, unload);






//     if (yed_get_var("tree-view-update-period") == NULL) {
//         yed_set_var("tree-view-update-period", "5");
//     }

//     if (yed_get_var("tree-view-hidden-items") == NULL) {
//         yed_set_var("tree-view-hidden-items", "");
//     }

//     if (yed_get_var("tree-view-image-extensions") == NULL) {
//         yed_set_var("tree-view-image-extensions", "");
//     }

//     if (yed_get_var("tree-view-archive-extensions") == NULL) {
//         yed_set_var("tree-view-archive-extensions", "");
//     }

//     if (yed_get_var("tree-view-child-char-l") == NULL) {
//         yed_set_var("tree-view-child-char-l", "└");
//     }

//     if (yed_get_var("tree-view-child-char-i") == NULL) {
//         yed_set_var("tree-view-child-char-i", "│");
//     }

//     if (yed_get_var("tree-view-child-char-t") == NULL) {
//         yed_set_var("tree-view-child-char-t", "├");
//     }

//     if (yed_get_var("tree-view-directory-color") == NULL) {
//         yed_set_var("tree-view-directory-color", "&blue");
//     }

//     if (yed_get_var("tree-view-exec-color") == NULL) {
//         yed_set_var("tree-view-exec-color", "&green");
//     }

//     if (yed_get_var("tree-view-symbolic-link-color") == NULL) {
//         yed_set_var("tree-view-symbolic-link-color", "&cyan");
//     }

//     if (yed_get_var("tree-view-device-color") == NULL) {
//         yed_set_var("tree-view-device-color", "&black swap &yellow.fg");
//     }

//     if (yed_get_var("tree-view-graphic-image-color") == NULL) {
//         yed_set_var("tree-view-graphic-image-color", "&magenta");
//     }

//     if (yed_get_var("tree-view-archive-color") == NULL) {
//         yed_set_var("tree-view-archive-color", "&red");
//     }

//     if (yed_get_var("tree-view-broken-link-color") == NULL) {
//         yed_set_var("tree-view-broken-link-color", "&black swap &red.fg");
//     }

    yed_plugin_set_command(self, "lsp-symbol-menu", _lsp_symbol_menu);

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
    /*
    symbol     *s;
    yed_attrs  *attr_tmp;
    char       *color_var;
    yed_attrs   attr_dir;
    yed_attrs   attr_exec;
    yed_attrs   attr_symb_link;
    yed_attrs   attr_device;
    yed_attrs   attr_graphic_img;
    yed_attrs   attr_archive;
    yed_attrs   attr_broken_link;
    yed_attrs   attr_file;
    int         loc;
    int         base;
    yed_line   *line;

    if (event->frame         == NULL
    ||  event->frame->buffer == NULL
    ||  event->frame->buffer != _get_or_make_buff()) {
        return;
    }

    if (array_len(files) < event->row) { return; }

    f = *(file **) array_item(files, event->row);

    if (f->path == NULL) { return; }

    attr_dir         = ZERO_ATTR;
    attr_exec        = ZERO_ATTR;
    attr_symb_link   = ZERO_ATTR;
    attr_device      = ZERO_ATTR;
    attr_graphic_img = ZERO_ATTR;
    attr_archive     = ZERO_ATTR;
    attr_broken_link = ZERO_ATTR;
    attr_file        = ZERO_ATTR;

    if ((color_var = yed_get_var("tree-view-directory-color"))) {
        attr_dir         = yed_parse_attrs(color_var);
    }

    if ((color_var = yed_get_var("tree-view-exec-color"))) {
        attr_exec         = yed_parse_attrs(color_var);
    }

    if ((color_var = yed_get_var("tree-view-symbolic-link-color"))) {
        attr_symb_link         = yed_parse_attrs(color_var);
    }

    if ((color_var = yed_get_var("tree-view-device-color"))) {
        attr_device         = yed_parse_attrs(color_var);
    }

    if ((color_var = yed_get_var("tree-view-graphic-image-color"))) {
        attr_graphic_img         = yed_parse_attrs(color_var);
    }

    if ((color_var = yed_get_var("tree-view-archive-color"))) {
        attr_archive         = yed_parse_attrs(color_var);
    }

    if ((color_var = yed_get_var("tree-view-broken-link-color"))) {
        attr_broken_link         = yed_parse_attrs(color_var);
    }

    base = 0;
    switch (f->flags) {
        case IS_DIR:
            attr_tmp = &attr_dir;
            break;
        case IS_LINK:
            attr_tmp = &attr_symb_link;
            break;
        case IS_B_LINK:
            attr_tmp = &attr_broken_link;
            break;
        case IS_DEVICE:
            attr_tmp = &attr_device;
            break;
        case IS_ARCHIVE:
            attr_tmp = &attr_archive;
            break;
        case IS_IMAGE:
            attr_tmp = &attr_graphic_img;
            break;
        case IS_EXEC:
            attr_tmp = &attr_exec;
            break;
        case IS_FILE:
        default:
            base = 1;
            attr_tmp = &attr_file;
            break;
    }

    if (event->frame->buffer == NULL) { return; }

    line = yed_buff_get_line(event->frame->buffer, event->row);
    if (line == NULL) { return; }

    for (loc = 1; loc <= line->visual_width; loc += 1) {
        if (loc > f->color_loc) {
            if (!base) {
                yed_eline_combine_col_attrs(event, loc, attr_tmp);
            }
        }
    }
    */
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

static void  _lsp_symbol_menu_update_handler(yed_event *event) {
//     file    **f;
//     char     *path;
//     array_t   open_dirs;
//     time_t    curr_time;
//     int       idx;

//     curr_time = time(NULL);

//     if (curr_time > last_time + wait_time) {

//         open_dirs = array_make(char *);

//         idx = 0;
//         array_traverse(files, f) {
//             if (idx == 0) { idx++; continue; }

//             if ((*f)->open_children) {
//                 path = strdup((*f)->path);
//                 array_push(open_dirs, path);
//             }

//             idx++;
//         }

//         _tree_view_init();

//         int found;
//         while (array_len(open_dirs) > 0) {
//             found = 0;
//             path = *(char **)array_item(open_dirs, 0);

//             idx = 0;
//             array_traverse(files, f) {
//                 if (idx == 0) { idx++; continue; }

//                 if (strcmp((*f)->path, path) == 0) {
//                     _tree_view_add_dir(idx);
//                     array_delete(open_dirs, 0);
//                     found = 1;
//                     break;
//                 }

//                 idx++;
//             }

//             if (!found) {
//                 array_delete(open_dirs, 0);
//             }
//         }

//         last_time = curr_time;
//     }
}

// static file *_init_file(int parent_idx, char *path, char *name, int if_dir, int num_tabs, int color_loc) {
//     file *f;

//     f = malloc(sizeof(file));
//     memset(f, 0, sizeof(file));

//     if (parent_idx == IS_ROOT) {
//         f->parent = NULL;
//     } else {
//         f->parent = *(struct file **) array_item(files, parent_idx);
//     }

//     memset(f->path, 0, sizeof(char[512]));
//     strcat(f->path, path);

//     memset(f->name, 0, sizeof(char[512]));
//     strcat(f->name, name);

//     f->flags         = if_dir;
//     f->num_tabs      = num_tabs;
//     f->open_children = 0;
//     f->color_loc     = color_loc;

//     return f;
// }

static void _clear_symbols(void) {
    symbol **symbol_it;

    if (array_len(symbols) > 0) {
        array_traverse(symbols, symbol_it) {
            free(*symbol_it);
        }
    }

    array_clear(symbols);
}

static int _cmpfunc(const void *a, const void *b) {
//     file *left_f;
//     file *right_f;
//     char  left_name[512];
//     char  right_name[512];
//     int   left;
//     int   right;
//     int   loc;

//     left_f  = *(file **)a;
//     right_f = *(file **)b;

//     left = 0;
//     if (left_f->flags == IS_DIR) {
//         left = 1;
//     }

//     right = 0;
//     if (right_f->flags == IS_DIR) {
//         right = 1;
//     }

//     if (left < right) {
//         return  1;
//     } else if (left > right) {
        return -1;
//     } else {
//         strcpy(left_name, left_f->name);
//         strcpy(right_name, right_f->name);

//         loc = 0;
//         while (left_name[loc]) {
//             left_name[loc] = tolower(left_name[loc]);
//             loc++;
//         }

//         loc = 0;
//         while (right_name[loc]) {
//             right_name[loc] = tolower(right_name[loc]);
//             loc++;
//         }

//     	return strcmp(left_name, right_name);
//     }

//     return ((file *)a)->flags - ((file *)b)->flags;
}

static void _lsp_symbol_menu_unload(yed_plugin *self) {
    _clear_symbols();
}
