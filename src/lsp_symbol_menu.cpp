#include "lsp_include.h"
#include "lsp_symbol.h"
#include "lsp_goto_declaration.h"
#include "lsp_goto_definition.h"
#include "lsp_goto_implementation.h"
#include "lsp_find_references.h"


/* internal functions*/
static void        _lsp_symbol_menu(int n_args, char **args);
static void        _lsp_symbol_menu_init(void);
static void        _lsp_open_symbol(int loc, symbol *s);
static void        _lsp_close_symbol(void);
// static void        _lsp_symbol_menu_add_dir(int idx);
// static void        _lsp_symbol_menu_remove_dir(int idx);
static void        _lsp_symbol_menu_select(void);
static void        _lsp_symbol_menu_line_handler(yed_event *event);
static void        _lsp_symbol_menu_key_pressed_handler(yed_event *event);
static void        _lsp_symbol_menu_update_handler(yed_event *event);
static void        _lsp_symbol_menu_unload(yed_plugin *self);

/* internal helper functions */
static void        _add_hidden_items(void);
static void        _add_archive_extensions(void);
static void        _add_image_extensions(void);
static void        _clear_symbols(void);
static int         _cmpfunc(const void *a, const void *b);
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

    YEXE("special-buffer-prepare-focus", "*symbol-menu");

    if (ys->active_frame) {
        YEXE("buffer", "*symbol-menu");
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
    DBG("goto_declaration");
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
    DBG("find_references");
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

/*
static void _tree_view_add_dir(int idx) {
    char          **str_it;
    file          **f_it;
    file           *f;
    file           *new_f;
    yed_buffer     *buff;
    struct dirent  *de;
    DIR            *dr;
    FILE           *fs;
    int             new_idx;
    int             dir;
    int             tabs;
    int             i;
    int             j;
    int             color_loc;
    char            path[1024];
    char            name[512];
    char            write_name[512];
    struct stat     statbuf;
    array_t         tmp_files;
    int             loc;

    tmp_files = array_make(file *);

    buff = _get_or_make_buff();
    f    = *(file **)array_item(files, idx);
    dr   = opendir(f->path);

    if (dr == NULL) { return; }

    buff->flags &= ~BUFF_RD_ONLY;

    new_idx = idx+1;
    while ((de = readdir(dr)) != NULL) {
        if (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0) {
            continue;
        }

        array_traverse(hidden_items, str_it) {
            if (strstr(de->d_name, (*str_it))) {
                goto cont;
            }
        }

        if (new_idx > 1) {
            yed_buff_insert_line_no_undo(buff, new_idx);
        }

        f->open_children = 1;

        memset(path, 0, sizeof(char[512]));
        sprintf(path, "%s/%s", f->path, de->d_name);

        tabs = f->num_tabs+1;
        memset(name, 0, sizeof(char[512]));
        strcat(name, de->d_name);

        dir = 0;
        if (lstat(path, &statbuf) == 0) {
            switch (statbuf.st_mode & S_IFMT) {
                case S_IFDIR:
                    dir = IS_DIR;
                    break;
                case S_IFLNK:
                    fs = fopen(path, "r");
                    if (!fs && errno == 2) {
                        dir = IS_B_LINK;
                    } else {
                        dir = IS_LINK;
                    }
                    break;
                case S_IFBLK:
                case S_IFCHR:
                    dir = IS_DEVICE;
                    break;
                default:
                    if (statbuf.st_mode & S_IXUSR) {
                        dir = IS_EXEC;
                    } else {
                        array_traverse(archive_extensions, str_it) {
                            if (strstr(de->d_name, (*str_it))) {
                                dir = IS_ARCHIVE;
                                goto break_switch;
                            }
                        }

                        array_traverse(image_extensions, str_it) {
                            if (strstr(de->d_name, (*str_it))) {
                                dir = IS_IMAGE;
                                goto break_switch;
                            }
                        }

                        dir = IS_FILE;
                    }
                    break;
            }
        }
break_switch:;
        color_loc = 0;
        new_f = _init_file(idx, path, name, dir, tabs, color_loc);

        array_push(tmp_files, new_f);

        new_idx++;

cont:;
    }

    closedir(dr);

    qsort(array_data(tmp_files), array_len(tmp_files), sizeof(file *), _cmpfunc);

    new_idx = idx+1;
    loc = 0;
    array_traverse(tmp_files, f_it) {
        color_loc = (*f_it)->num_tabs * yed_get_tab_width();
        memset(write_name, 0, sizeof(char[512]));

        if ((*f_it)->num_tabs > 0) {
            color_loc += 1;

            for  (i = 0; i < (*f_it)->num_tabs; i++) {
                strcat(write_name, yed_get_var("tree-view-child-char-i"));
                for (j = 0; j < yed_get_tab_width()-1; j++) {
                    strcat(write_name, " ");
                }
            }

            if (loc == array_len(tmp_files)-1) {
                strcat(write_name, yed_get_var("tree-view-child-char-l"));
            } else {
                strcat(write_name, yed_get_var("tree-view-child-char-t"));
            }

            strcat(write_name, (*f_it)->name);
            yed_buff_insert_string_no_undo(buff, write_name, new_idx, 1);
        } else {
            yed_buff_insert_string_no_undo(buff, (*f_it)->name, new_idx, 1);
        }

        (*f_it)->color_loc = color_loc;

        if (new_idx >= array_len(files)) {
            array_push(files, (*f_it));
        } else {
            array_insert(files, new_idx, (*f_it));
        }

        new_idx++;
        loc++;
    }

    buff->flags |= BUFF_RD_ONLY;
    array_free(tmp_files);
}

static void _tree_view_remove_dir(int idx) {
    yed_buffer *buff;
    file       *f;
    file       *remove;
    int         next_idx;

    next_idx = idx + 1;

    buff = _get_or_make_buff();
    buff->flags &= ~BUFF_RD_ONLY;

    f = *(file **)array_item(files, idx);

    remove = *(file **)array_item(files, next_idx);
    while (array_len(files) > next_idx && remove && remove->num_tabs > f->num_tabs) {
        free(remove);
        array_delete(files, next_idx);
        yed_buff_delete_line(buff, next_idx);

        if (array_len(files) <= next_idx) {
            break;
        }
        remove = *(file **)array_item(files, next_idx);
    }

    f->open_children = 0;

    buff->flags |= BUFF_RD_ONLY;
}
*/

static void _lsp_symbol_menu_select(void) {
    symbol *s;

    if (sub == 0) {
        sub = ys->active_frame->cursor_line - 1;
        s   = *(symbol **)array_item(symbols, sub);
        DBG("%d", sub);
        if (s == NULL) {
            return;
        }

        s->declaration = NULL;
        s->definition  = NULL;

        DBG("opened: %s", s->name);
        _lsp_open_symbol(sub, s);

    } else {
        if (ys->active_frame->cursor_line == 1) {
            sub = 0;
            s   = *(symbol **)array_item(symbols, sub);
            DBG("closed: %s", s->name);
            _lsp_close_symbol();
        }
    }

//     if (f->flags == IS_DIR) {
//         if (f->open_children) {
//             _tree_view_remove_dir(ys->active_frame->cursor_line);
//         } else {
//             _tree_view_add_dir(ys->active_frame->cursor_line);
//         }
//     } else {
//         YEXE("special-buffer-prepare-jump-focus", f->path);
//         YEXE("buffer", f->path);
//     }
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
    ||  strcmp(eframe->buffer->name, "*symbol-menu")) {
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
