/*
 * Copyright (C) Todd A. Fisher
 */


#ifndef _NGX_HTTP_ESI_FILTER_H_INCLUDED_
#define _NGX_HTTP_ESI_FILTER_H_INCLUDED_

#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_event.h>
#include <ngx_http.h>
#include "ngx_esi_parser.h"
#include <stdlib.h>
#include <string.h>

typedef struct {
  ESIParser *parser;
  struct _ESITag *root_tag; /* If any tags are being actively processed there is always a root node */
  struct _ESITag *open_tag; /* The deepest nested tag open */
  ngx_http_request_t *request;
  ngx_chain_t *chain; /* store buffered content */
  ngx_chain_t *last_buf;

  unsigned exception_raised:1; /* this is toggled to 1 if an exception is raised while processing an attempt tag */
  unsigned ignore_tag:1; /* this is toggled to 1 when for some reason typically no exception was raised so the tags should not be processed */

} ngx_http_esi_ctx_t;

#endif /* _NGX_HTTP_ESI_FILTER_H_INCLUDED_ */
