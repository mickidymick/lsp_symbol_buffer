#ifndef LSP_GOTO_DECLARATION_H
#define LSP_GOTO_DECLARATION_H

#include "lsp_include.h"

void goto_declaration_cmd(int n_args, char **args);
void goto_declaration_request(yed_frame *frame);
void goto_declaration_get_range(const json &result, yed_event *event);
void goto_declaration_pmsg(yed_event *event);

#endif
