#ifndef LSP_GOTO_DEFINITION_H
#define LSP_GOTO_DEFINITION_H

#include "lsp_include.h"

void goto_definition_cmd(int n_args, char **args);
void goto_definition_request(yed_frame *frame);
void goto_definition_get_range(const json &result, yed_event *event);
void goto_definition_pmsg(yed_event *event);

#endif
