#include "lsp_goto_definition.h"
#include "lsp_goto_declaration.h"

int lsp_goto_definition_now = 0;

void goto_definition_cmd(int n_args, char **args) {
    if (ys->active_frame         == NULL
    ||  ys->active_frame->buffer == NULL) {
        return;
    }

    uri                      = uri_for_buffer(ys->active_frame->buffer);
    pos                      = position_in_frame(ys->active_frame);
    lsp_goto_definition_now = 1;

    goto_definition_request(ys->active_frame);
}

void goto_definition_request(yed_frame *frame) {
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
    event.plugin_message.message_id  = "lsp-request:textDocument/definition";
    event.plugin_message.plugin_id   = "lsp_symbol_menu";
    event.plugin_message.string_data = text.c_str();
    event.ft                         = frame->buffer->ft;

//     DBG("Definition");

    yed_trigger_event(&event);
}

void goto_definition_get_range(const json &result, yed_event *event) {
    int         row  = 0;
    int         col  = 0;
    int         byte = 0;
    yed_buffer *buffer;
    yed_buffer *buffer1;
    char        buff[512];
    string      b;
    yed_line   *line;
    symbol     *s;

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

        if (lsp_goto_definition_now == 1) {
            string tmp_str2 = "buffer";
            YEXE((char *) tmp_str2.c_str(), buffer->path);

            if (ys->active_frame) {
                yed_move_cursor_within_frame(ys->active_frame, row - ys->active_frame->cursor_line, col - ys->active_frame->cursor_col);
            }
            return;
        }

        cur_symbol->definition           = (item *)malloc(sizeof(item));
        cur_symbol->definition->buffer   = buffer;
        cur_symbol->definition->line     = yed_get_line_text(buffer, row);
        cur_symbol->definition->row      = row;
        cur_symbol->definition->col      = col;
        cur_symbol->definition->start    = col;
        cur_symbol->definition->end      = yed_line_idx_to_col(line, byte);
        cur_symbol->definition->num_len  = to_string(row).length() + 1;

        string str(cur_symbol->definition->buffer->name);

        cur_symbol->definition->name_len = str.size();

//         DBG("set definition");

        char tmp_str[512];
        sprintf(tmp_str, "%s:%d:%s", buffer->name, row, cur_symbol->definition->line);

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

            if (yed_buff_n_lines(buffer1) >= 10) {
                yed_line_clear_no_undo(buffer1, 9);
                yed_line_clear_no_undo(buffer1, 10);
            }
            yed_buff_insert_string_no_undo(buffer1, "Definition", 9, 1);
            yed_buff_insert_string_no_undo(buffer1, tmp_str, 10, 1);

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

        pos.line      = cur_symbol->definition->row - 1;
        pos.character = cur_symbol->definition->col + 1;
        uri           = b;
    }
}

void goto_definition_pmsg(yed_event *event) {
    if (strcmp(event->plugin_message.plugin_id, "lsp") != 0
    ||  strcmp(event->plugin_message.message_id, "textDocument/definition") != 0) {
        return;
    }

    if (ys->active_frame == NULL) {
        return;
    }

    try {
//         DBG("Definition");
        auto j = json::parse(event->plugin_message.string_data);
//         DBG("%s",j.dump(2).c_str());
        const auto &r = j["result"];

        if (r.is_array()) {
            const json &result = j["result"][0];
            goto_definition_get_range(result, event);
        } else {
            const json &result = j["result"];
            goto_definition_get_range(result, event);
        }

    } catch (...) {}

    if (lsp_goto_definition_now == 0) {
        if (cur_symbol->declaration == NULL) {
            goto_declaration_request(last_frame);
        }

//         if (cur_symbol->definition == NULL && definition_num == 0) {
//             pos.line       = first_pos.line;
//             pos.character  = first_pos.character;
//             uri            = first_uri;
//             definition_num = 1;
//             goto_definition_request(last_frame);
//         }
    }

    lsp_goto_definition_now = 0;
    event->cancel           = 1;
}
