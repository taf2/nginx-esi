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

typedef struct {
    ngx_flag_t     enable;
    ngx_flag_t     silent_errors;
    ngx_flag_t     ignore_recycled_buffers;
    ngx_array_t   *types;
    
    size_t         min_file_chunk;
} ngx_http_esi_loc_conf_t;
