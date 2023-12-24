#ifndef LSP_FIND_REFERENCES_H
#define LSP_FIND_REFERENCES_H

#include "lsp_include.h"

static int loc;

void find_references_cmd(int n_args, char **args);
void goto_next_reference_cmd(int n_args, char **args);
void goto_prev_reference_cmd(int n_args, char **args);
void find_references_request(yed_frame *frame);
void find_references_get_range(const json &result, yed_event *event);
void find_references_pmsg(yed_event *event);

#endif
