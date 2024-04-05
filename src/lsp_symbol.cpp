#include "lsp_symbol.h"

void symbol_request(yed_frame *frame) {
    if (frame == NULL
    ||  frame->buffer == NULL
    ||  frame->buffer->kind != BUFF_KIND_FILE
    ||  frame->buffer->flags & BUFF_SPECIAL) {

        return;
    }

    last_frame = frame;

    uri = uri_for_buffer(frame->buffer);
    pos = position_in_frame(frame);

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
    event.plugin_message.message_id  = "lsp-request:textDocument/documentSymbol";
    event.plugin_message.plugin_id   = "lsp_symbol_menu";
    event.plugin_message.string_data = text.c_str();
    event.ft                         = frame->buffer->ft;

    yed_trigger_event(&event);
}

void symbol_get_range(const json &result, yed_event *event) {
    int         row  = 0;
    int         col  = 0;
    int         byte = 0;
    yed_buffer *buffer;
    yed_buffer *buffer1;
    char        buff[512];
    char        write_name[512];
    char        write_name2[512];
    string      tmp_name;
    string      b;
    yed_line   *line;
    symbol     *tmp_symbol;

    if (result.contains("location") && result["location"].contains("range")) {
        tmp_name = result["name"].dump();
        if (mp.find(tmp_name) == mp.end()) {
            //not found
            mp.insert({tmp_name, 0});
        } else {
            //found
            return;
        }

        const auto &range = result["location"]["range"];
        row = range["start"]["line"];
        row += 1;
        byte = range["start"]["character"];
        b = result["location"]["uri"].dump(2);
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

        tmp_symbol = (symbol *)malloc(sizeof(symbol));
        memset(tmp_symbol, 0, sizeof(symbol));

        tmp_symbol->buffer   = buffer;

        memset(write_name, 0, sizeof(char[512]));
        sprintf(write_name, "%s", result["name"].dump().c_str());
        char *write_name1 = write_name + 1;
        write_name1[strlen(write_name1)-1] = '\0';
        memset(tmp_symbol->name, 0, sizeof(char[512]));
        strcpy(tmp_symbol->name, write_name1);

        tmp_symbol->menu_row = tot;
        tmp_symbol->row      = row;
        tmp_symbol->col      = col;
        tmp_symbol->type     = result["kind"];

        array_push(symbols, tmp_symbol);

        buffer1 = _get_or_make_buff();
        if (buffer1 != NULL) {
            buffer1->flags &= ~BUFF_RD_ONLY;
            if (tmp_symbol->type == 12) {
                sprintf(write_name2, "  func: %s", tmp_symbol->name);
            } else if (tmp_symbol->type == 5) {
                sprintf(write_name2, " class: %s", tmp_symbol->name);
            } else if (tmp_symbol->type == 23) {
                sprintf(write_name2, "struct: %s", tmp_symbol->name);
            }
            yed_buff_insert_string_no_undo(buffer1, write_name2, tot, 1);
            tot++;
            buffer1->flags |= BUFF_RD_ONLY;
        }
    }
}

void symbol_pmsg(yed_event *event) {
    if (strcmp(event->plugin_message.plugin_id, "lsp") != 0
    ||  strcmp(event->plugin_message.message_id, "textDocument/documentSymbol") != 0) {
        return;
    }

    if (ys->active_frame == NULL) {
        return;
    }

    try {
//         DBG("Symbol");
        auto j = json::parse(event->plugin_message.string_data);
//         DBG("%s", j.dump(2).c_str());
        const auto &r = j["result"];

        mp.clear();
        _clear_symbols();

        yed_buffer *buff;
        buff = _get_or_make_buff();
        buff->flags &= ~BUFF_RD_ONLY;
        yed_buff_clear_no_undo(buff);
        buff->flags |= BUFF_RD_ONLY;

        tot = 0;

        if (r.is_array()) {
            for(int i = 0; i < r.size(); i++) {
                if (j["result"][i]["kind"] == 12
                ||  j["result"][i]["kind"] == 5
                ||  j["result"][i]["kind"] == 23) {
//                     DBG("%s",j["result"][i].dump(2).c_str());
                    const json &result = j["result"][i];
                    symbol_get_range(result, event);
                }
            }
        } else {
            const json &result = j["result"];
            symbol_get_range(result, event);
        }

        event->cancel = 1;
    } catch (...) {}
}
