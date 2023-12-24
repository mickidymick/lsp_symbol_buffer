#include "lsp_goto_declaration.h"
#include "lsp_goto_definition.h"

int lsp_goto_declaration_now = 0;

void goto_declaration_cmd(int n_args, char **args) {
    if (ys->active_frame         == NULL
    ||  ys->active_frame->buffer == NULL) {
        return;
    }

    uri                      = uri_for_buffer(ys->active_frame->buffer);
    pos                      = position_in_frame(ys->active_frame);
    lsp_goto_declaration_now = 1;

    goto_declaration_request(ys->active_frame);
}

void goto_declaration_request(yed_frame *frame) {
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
    event.plugin_message.message_id  = "lsp-request:textDocument/declaration";
    event.plugin_message.plugin_id   = "lsp_symbol_menu";
    event.plugin_message.string_data = text.c_str();
    event.ft                         = frame->buffer->ft;

    yed_trigger_event(&event);
}

void goto_declaration_get_range(const json &result, yed_event *event) {
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

        if (lsp_goto_declaration_now == 1) {
            string tmp_str2 = "buffer";
            YEXE((char *) tmp_str2.c_str(), buffer->path);

            if (ys->active_frame) {
                yed_move_cursor_within_frame(ys->active_frame, row - ys->active_frame->cursor_line, col - ys->active_frame->cursor_col);
            }
            return;
        }

        s                       = *(symbol **)array_item(symbols, sub);
        s->declaration          = (item *)malloc(sizeof(item));
        s->declaration->buffer  = buffer;
        s->declaration->line    = yed_get_line_text(buffer, row);
        s->declaration->row     = row;
        s->declaration->col     = col;
        s->declaration->start   = col;
        s->declaration->end     = yed_line_idx_to_col(line, byte);
        s->declaration->num_len = to_string(row).length() + 1;

//         DBG("start:%d end:%d len:%d", s->declaration->start, s->declaration->end, s->declaration->num_len);

//         DBG("set declaration");

        char tmp_str[512];
        sprintf(tmp_str, "%d %s", row, s->declaration->line);

        buffer1 = _get_or_make_buff();
        if (buffer1 != NULL) {
            buffer1->flags &= ~BUFF_RD_ONLY;

            yed_buff_insert_string_no_undo(buffer1, "Declaration", 6, 1);
            yed_buff_insert_string_no_undo(buffer1, tmp_str, 7, 1);
            tot++;
            buffer1->flags |= BUFF_RD_ONLY;
        }

        pos.line      = s->declaration->row - 1;
        pos.character = s->declaration->col + 1;
    }
}

void goto_declaration_pmsg(yed_event *event) {
    if (strcmp(event->plugin_message.plugin_id, "lsp") != 0
    ||  strcmp(event->plugin_message.message_id, "textDocument/declaration") != 0) {
        return;
    }

    if (ys->active_frame == NULL) {
        return;
    }

    try {
//         DBG("Declaration");
        auto j = json::parse(event->plugin_message.string_data);
//         DBG("%s",j.dump(2).c_str());
        const auto &r = j["result"];

        if (r.is_array()) {
            const json &result = j["result"][0];
            goto_declaration_get_range(result, event);
        } else {
            const json &result = j["result"];
            goto_declaration_get_range(result, event);
        }

    } catch (...) {}

    if (lsp_goto_declaration_now == 0 && array_len(symbols) > 0) {
        symbol *s = *(symbol **)array_item(symbols, sub);
        if (s->definition == NULL) {
            goto_definition_request(last_frame);
        }
    }

    lsp_goto_declaration_now = 0;
    event->cancel            = 1;
}
