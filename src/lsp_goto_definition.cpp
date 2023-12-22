#include "lsp_goto_definition.h"
#include "lsp_goto_declaration.h"

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
        buffer = yed_get_buffer(name);

        if (buffer == NULL) {
            return;
        }

        line = yed_buff_get_line(buffer, row);
        if (line == NULL) {
            return;
        }

        col = yed_line_idx_to_col(line, byte);

        s                     = *(symbol **)array_item(symbols, sub);
        s->definition         = (item *)malloc(sizeof(item));
        s->definition->buffer = buffer;
        s->definition->line   = yed_get_line_text(buffer, row);
        s->definition->row    = row;
        s->definition->col    = col;

        DBG("set definition");

        char tmp_str[512];
        sprintf(tmp_str, "%d %s", row, s->definition->line);

        buffer1 = _get_or_make_buff();
        if (buffer1 != NULL) {
            buffer1->flags &= ~BUFF_RD_ONLY;

            yed_buff_insert_string_no_undo(buffer1, "Definition", 9, 1);
            yed_buff_insert_string_no_undo(buffer1, tmp_str, 10, 1);
            tot++;
            buffer1->flags |= BUFF_RD_ONLY;
        }

        pos.line      = s->definition->row - 1;
        pos.character = s->definition->col + 1;
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

    symbol *s = *(symbol **)array_item(symbols, sub);
    if (s->declaration == NULL) {
        goto_declaration_request(last_frame);
    }

    event->cancel = 1;
}
