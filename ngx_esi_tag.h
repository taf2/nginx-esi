/*
 * Copyright (C) Todd A. Fisher
 */
#ifndef NGX_ESI_TAG_H
#define NGX_ESI_TAG_H

#include "ngx_http_esi_filter_module.h"

typedef enum {
  ESI_TRY,
  ESI_ATTEMPT,
  ESI_EXCEPT,
  ESI_INCLUDE,
  ESI_INVALIDATE,
  ESI_VARS,
  ESI_REMOVE,
  ESI_NONE
}esi_tag_t;

typedef struct _ESITag {
  ngx_http_esi_ctx_t *ctx; /* context stores request info */
  esi_tag_t type; /* tag type */
  int depth; /* request depth */
  struct _ESITag *next; /* children of this tag e.g.
                           <esi:try><esi:attempt><esi:include/></esi:attempt></esi:try>,
                           try has 2 children attempt and include, and include has one child include */
} ESITag;

ESITag *esi_tag_new(esi_tag_t tag, ngx_http_esi_ctx_t *ctx);
void esi_tag_free(ESITag *tag);
void esi_tag_start(ESITag *tag);
void esi_tag_close(ESITag *tag);
ESITag *esi_tag_close_children( ESITag *tag, esi_tag_t type );
ngx_buf_t *esi_tag_buffer(ESITag *tag, const void *data, size_t length);
void esi_tag_debug(ESITag *tag);

int esi_vars_filter( ngx_chain_t *chain );


#endif
