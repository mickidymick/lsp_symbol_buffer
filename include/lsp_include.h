#ifndef LSP_INCLUDE_H
#define LSP_INCLUDE_H

extern "C" {
    #include <yed/plugin.h>
}

#include <time.h>
#include <string>
#include <iostream>
#include <sstream>
#include <map>
#include <vector>
#include <memory>

using namespace std;

#include "json.hpp"
using json = nlohmann::json;

#define DO_LOG
#define DBG__XSTR(x) #x
#define DBG_XSTR(x) DBG__XSTR(x)
#ifdef  DO_LOG
#define DBG(...)                                           \
do {                                                       \
    LOG_FN_ENTER();                                        \
    yed_log(__FILE__ ":" XSTR(__LINE__) ": " __VA_ARGS__); \
    LOG_EXIT();                                            \
} while (0)
#else
#define DBG(...) ;
#endif

#define SYM_BUFFER (char *) symbol_buffer_name.c_str()

typedef struct {
    size_t line;
    size_t character;
} position;

typedef struct {
    yed_buffer *buffer;
    char       *line;
    int         row;
    int         col;
    int         start;
    int         end;
    int         num_len;
    int         name_len;
} item;

typedef struct {
    //main
    yed_buffer *buffer;
    char        name[512];
    string      uri;
    position    pos;
    int         menu_row;
    int         row;
    int         col;
    int         type;

    //side
    item       *declaration;
    item       *definition;
    item       *references[1024];
    int         ref_size;
} symbol;

/* global variables */
extern yed_plugin *Self;
extern yed_frame  *last_frame;
extern symbol     *cur_symbol;
extern array_t     symbols;
extern position    pos;
extern position    first_pos;
extern string      uri;
extern string      first_uri;
extern string      symbol_buffer_name;
extern time_t      last_time;
extern time_t      wait_time;
extern int         sub;
extern int         tot;
extern array_t     references;
extern int         ref_loc;
extern int         has_declaration;
extern int         definition_num;

/* global functions */
static yed_buffer *_get_or_make_buff(void);
static string      uri_for_buffer(yed_buffer *buffer);
static position    position_in_frame(yed_frame *frame);
static char       *trim_leading_whitespace(char *str);
static void        _clear_symbols(void);
static void        _clear_symbol(void);
static void        _init_symbol(void);

static yed_buffer *_get_or_make_buff(void) {
    yed_buffer *buff;

    buff = yed_get_buffer(SYM_BUFFER);

    if (buff == NULL) {
        DBG("FUCK");
        buff = yed_create_buffer(SYM_BUFFER);
        buff->flags |= BUFF_RD_ONLY | BUFF_SPECIAL;
    }

    return buff;
}

static string uri_for_buffer(yed_buffer *buffer) {
    string uri = "";

    if (!(buffer->flags & BUFF_SPECIAL)
    &&  buffer->kind == BUFF_KIND_FILE) {
        if (buffer->path == NULL) {
            uri += "untitled:";
            uri += buffer->name;
        } else {
            uri += "file://";
            uri += buffer->path;
        }
    }

    return uri;
}

static position position_in_frame(yed_frame *frame) {
    yed_line *line;
    position  tmp_pos;

    if (frame == NULL || frame->buffer == NULL) {
        tmp_pos.line      = -1;
        tmp_pos.character = -1;
        return tmp_pos;
    }

    line = yed_buff_get_line(frame->buffer, frame->cursor_line);
    if (line == NULL) {
        tmp_pos.line      = -1;
        tmp_pos.character = -1;
        return tmp_pos;
    }

    tmp_pos.line      = frame->cursor_line - 1;
    tmp_pos.character = yed_line_col_to_idx(line, frame->cursor_col);
    return tmp_pos;
}

static char *trim_leading_whitespace(char *str) {
    while(isspace((unsigned char)*str)) str++;
    return str;
}

static void _clear_symbol(void) {
    if (cur_symbol->declaration != NULL) {
        free(cur_symbol->declaration->line);
        free(cur_symbol->declaration);
    }

    if (cur_symbol->definition != NULL) {
        free(cur_symbol->definition->line);
        free(cur_symbol->definition);
    }

    for (int i = 0; i < cur_symbol->ref_size; i++) {
        if (cur_symbol->references[i] != NULL) {
            free(cur_symbol->references[i]->line);
        }
    }

}

static void _init_symbol(void) {

    if (cur_symbol != NULL) {
        _clear_symbol();
    }

    cur_symbol = (symbol *)malloc(sizeof(symbol));
    cur_symbol->declaration     = NULL;
    cur_symbol->definition      = NULL;
    cur_symbol->ref_size        = 0;
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

#endif
