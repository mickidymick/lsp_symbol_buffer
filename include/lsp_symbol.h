#ifndef LSP_SYMBOL_H
#define LSP_SYMBOL_H

#include "lsp_include.h"

static map<string, int>           mp;
static map<string, int>::iterator it;

void symbol_request(yed_frame *frame);
void symbol_get_range(const json &result, yed_event *event);
void symbol_pmsg(yed_event *event);

#endif
