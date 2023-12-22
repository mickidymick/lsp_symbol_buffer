#ifndef LSP_GOTO_IMPLEMENTATION_H
#define LSP_GOTO_IMPLEMENTATION_H

#include "lsp_include.h"

void goto_implementation_request(yed_frame *frame);
void goto_implementation_get_range(const json &result, yed_event *event);
void goto_implementation_pmsg(yed_event *event);

#endif
