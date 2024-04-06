#include "lsp_find_references.h"

int lsp_find_references_now = 0;

void find_references_cmd(int n_args, char **args) {
    if (ys->active_frame         == NULL
    ||  ys->active_frame->buffer == NULL) {
        return;
    }

    uri                     = uri_for_buffer(ys->active_frame->buffer);
    pos                     = position_in_frame(ys->active_frame);
    lsp_find_references_now = 1;
    ref_loc                 = 0;

    find_references_request(ys->active_frame);
}

void goto_next_reference_cmd(int n_args, char **args) {
    item   *i;
    string  tmp_str2 = "buffer";
    int     len;

    len = cur_symbol->ref_size;

    if (len < 2) {
        return;
    }

    ref_loc++;

    i = cur_symbol->references[ref_loc%len];
    YEXE((char *) tmp_str2.c_str(), i->buffer->path);

    if (ys->active_frame) {
        yed_move_cursor_within_frame(ys->active_frame, i->row - ys->active_frame->cursor_line, i->col - ys->active_frame->cursor_col);
    }
}

void goto_prev_reference_cmd(int n_args, char **args) {
    item   *i;
    string  tmp_str2 = "buffer";
    int     len;

    len = cur_symbol->ref_size;

    if (len < 2) {
        return;
    }

    if (ref_loc == 0) {
        ref_loc = len - 1;
    } else {
        ref_loc--;
    }

    i = cur_symbol->references[ref_loc%len];
    YEXE((char *) tmp_str2.c_str(), i->buffer->path);

    if (ys->active_frame) {
        yed_move_cursor_within_frame(ys->active_frame, i->row - ys->active_frame->cursor_line, i->col - ys->active_frame->cursor_col);
    }
}

void find_references_request(yed_frame *frame) {
    if (frame == NULL
    ||  frame->buffer == NULL
    ||  frame->buffer->kind != BUFF_KIND_FILE
    ||  frame->buffer->flags & BUFF_SPECIAL) {
        return;
    }

    last_frame = frame;

    json params = {
        { "textDocument", {
            { "uri", uri },
        }},
        { "position", {
            { "line",      pos.line      },
            { "character", pos.character },
        }},
    };

    yed_event event;
    string    text = params.dump();

    event.kind                       = EVENT_PLUGIN_MESSAGE;
    event.plugin_message.message_id  = "lsp-request:textDocument/references";
    event.plugin_message.plugin_id   = "lsp_symbol_menu";
    event.plugin_message.string_data = text.c_str();
    event.ft                         = frame->buffer->ft;

    yed_trigger_event(&event);
}

void find_references_get_range(const json &result, yed_event *event) {
    int         row  = 0;
    int         col  = 0;
    int         byte = 0;
    yed_buffer *buffer;
    yed_buffer *buffer1;
    char        buff[512];
    string      b;
    yed_line   *line;
    symbol     *s;
    item       *i;

    if (result.contains("range")) {
        const auto &range = result["range"];
        row = range["start"]["line"];
        row += 1;
        byte = range["start"]["character"];
        b = result["uri"].dump(2);
        b = b.substr(8, b.size() - 9);
        strcpy(buff, b.c_str());

        char a_path[512];
        char r_path[512];
        char h_path[512];
        char *name;
        abs_path(buff, a_path);
        relative_path_if_subtree(a_path, r_path);
        if (homeify_path(r_path, h_path)) {
            name = h_path;
        } else {
            name = r_path;
        }

        string tmp = "buffer-hidden";
        YEXE((char *) tmp.c_str(), name);
        buffer = yed_get_buffer(name);

        if (buffer == NULL) {
            return;
        }

        line = yed_buff_get_line(buffer, row);
        if (line == NULL) {
            return;
        }

        col  = yed_line_idx_to_col(line, byte);
        byte = range["end"]["character"];

        i         = (item *)malloc(sizeof(item));
        i->buffer = buffer;
        i->line   = yed_get_line_text(buffer, row);
        i->row    = row;
        i->col    = col;

        if (lsp_find_references_now == 1) {
            string tmp_str2 = "buffer";
            YEXE((char *) tmp_str2.c_str(), buffer->path);

            if (ys->active_frame) {
                yed_move_cursor_within_frame(ys->active_frame, row - ys->active_frame->cursor_line, col - ys->active_frame->cursor_col);
            }

            array_push(references, i);

            return;
        }

        string str(buffer->name);

        cur_symbol->references[cur_symbol->ref_size]           = i;
        cur_symbol->references[cur_symbol->ref_size]->start    = col;
        cur_symbol->references[cur_symbol->ref_size]->end      = yed_line_idx_to_col(line, byte);
        cur_symbol->references[cur_symbol->ref_size]->num_len  = to_string(row).length() + 1;
        cur_symbol->references[cur_symbol->ref_size]->name_len = str.size();

        char  tmp_str[512];
        char *tmp_str1;
        char *tmp_str2;
        tmp_str1 = strdup(cur_symbol->references[cur_symbol->ref_size]->line);
        tmp_str2 = trim_leading_whitespace(tmp_str1);
        sprintf(tmp_str, "%s:%d:%s", buffer->name, row, tmp_str2);

        int num = tmp_str2 - tmp_str1;
        cur_symbol->references[cur_symbol->ref_size]->start = cur_symbol->references[cur_symbol->ref_size]->start - num;
        cur_symbol->references[cur_symbol->ref_size]->end   = cur_symbol->references[cur_symbol->ref_size]->end - num;
        cur_symbol->ref_size++;

        buffer1 = _get_or_make_buff();
        if (buffer1 != NULL) {
            buffer1->flags &= ~BUFF_RD_ONLY;

            yed_buff_insert_string_no_undo(buffer1, tmp_str, 13 + loc, 1);

            buffer1->flags |= BUFF_RD_ONLY;
        }
    }
}

void find_references_pmsg(yed_event *event) {
    if (strcmp(event->plugin_message.plugin_id, "lsp") != 0
    ||  strcmp(event->plugin_message.message_id, "textDocument/references") != 0) {
        return;
    }

    if (ys->active_frame == NULL) {
        return;
    }

    try {
//         DBG("References");
        auto j = json::parse(event->plugin_message.string_data);
//         DBG("%s",j.dump(2).c_str());
        const auto &r = j["result"];

        yed_buffer *buffer1 = _get_or_make_buff();
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

            if (yed_buff_n_lines(buffer1) >= 13) {
                yed_line_clear_no_undo(buffer1, 12);
                yed_line_clear_no_undo(buffer1, 13);
            }
            yed_buff_insert_string_no_undo(buffer1, "References", 12, 1);

            buffer1->flags |= BUFF_RD_ONLY;
        }

        if (r.is_array()) {
            for(loc = 0; loc < r.size(); loc++) {
                const json &result = j["result"][loc];
                find_references_get_range(result, event);
            }
        } else {
            const json &result = j["result"];
            find_references_get_range(result, event);
        }

    } catch (...) {}

    lsp_find_references_now = 0;
    event->cancel           = 1;
}
