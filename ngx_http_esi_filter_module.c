/*
 * Copyright (C) Todd A. Fisher
 *
 * This is a first attempt at nginx module.  The idea will be to use mongrel-esi ragel based parser to detect and 
 * tigger esi events.
 *
 * This module is model'ed following the ssi module
 */

#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_event.h>
#include <ngx_http.h>
#include "ngx_esi_parser.h"

typedef struct {
    ngx_flag_t     enable;          /* enable esi filter */
    ngx_flag_t     silent_errors;   /* ignore include errors, don't raise exceptions */
    size_t         min_file_chunk;  /* smallest size chunk */
    size_t         max_depth;       /* how many times to follow an esi:include redirect... */
} ngx_http_esi_loc_conf_t;
