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
  ngx_array_t   *types;           /* array of ngx_str_t */

  size_t         min_file_chunk;  /* smallest size chunk */
  size_t         max_depth;       /* how many times to follow an esi:include redirect... */
} ngx_http_esi_loc_conf_t;

typedef struct {
  ESIParser *parser;
} ngx_http_esi_ctx_t;

static ngx_int_t ngx_http_esi_filter_init(ngx_conf_t *cf);
static char *ngx_http_esi_types(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);
static void *ngx_http_esi_create_loc_conf(ngx_conf_t *cf);
static char *ngx_http_esi_merge_loc_conf(ngx_conf_t *cf, void *parent, void *child);

/* modified from ssi module */
static ngx_command_t  ngx_http_esi_filter_commands[] = {

    { ngx_string("esi"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_HTTP_LIF_CONF
                        |NGX_CONF_FLAG,
      ngx_conf_set_flag_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_esi_loc_conf_t, enable),
      NULL },

    { ngx_string("esi_silent_errors"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_FLAG,
      ngx_conf_set_flag_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_esi_loc_conf_t, silent_errors),
      NULL },

    { ngx_string("esi_min_file_chunk"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_FLAG,
      ngx_conf_set_size_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_esi_loc_conf_t, min_file_chunk),
      NULL },

    { ngx_string("esi_max_depth"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_FLAG,
      ngx_conf_set_size_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_esi_loc_conf_t, max_depth),
      NULL },
    
    { ngx_string("esi_types"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_1MORE,
      ngx_http_esi_types,
      NGX_HTTP_LOC_CONF_OFFSET,
      0,
      NULL },


      ngx_null_command
};


static ngx_http_module_t  ngx_http_esi_filter_module_ctx = {
    NULL,                                  /* preconfiguration */
    ngx_http_esi_filter_init,              /* postconfiguration */

    NULL,                                  /* create main configuration */
    NULL,                                  /* init main configuration */

    NULL,                                  /* create server configuration */
    NULL,                                  /* merge server configuration */

    ngx_http_esi_create_loc_conf,          /* create location configuration */
    ngx_http_esi_merge_loc_conf            /* merge location configuration */
};


ngx_module_t  ngx_http_esi_filter_module = {
    NGX_MODULE_V1,
    &ngx_http_esi_filter_module_ctx,       /* module context */
    ngx_http_esi_filter_commands,          /* module directives */
    NGX_HTTP_MODULE,                       /* module type */
    NULL,                                  /* init master */
    NULL,                                  /* init module */
    NULL,                                  /* init process */
    NULL,                                  /* init thread */
    NULL,                                  /* exit thread */
    NULL,                                  /* exit process */
    NULL,                                  /* exit master */
    NGX_MODULE_V1_PADDING
};


static ngx_http_output_header_filter_pt  ngx_http_next_header_filter;
static ngx_http_output_body_filter_pt    ngx_http_next_body_filter;

static char *
ngx_http_esi_types(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_http_esi_loc_conf_t *slcf = conf;

    ngx_str_t   *value, *type;
    ngx_uint_t   i;

    if (slcf->types == NULL) {
        slcf->types = ngx_array_create(cf->pool, 4, sizeof(ngx_str_t));
        if (slcf->types == NULL) {
            return NGX_CONF_ERROR;
        }

        type = ngx_array_push(slcf->types);
        if (type == NULL) {
            return NGX_CONF_ERROR;
        }

        type->len = sizeof("text/html") - 1;
        type->data = (u_char *) "text/html";
    }

    value = cf->args->elts;

    for (i = 1; i < cf->args->nelts; i++) {

        if (ngx_strcmp(value[i].data, "text/html") == 0) {
            continue;
        }

        type = ngx_array_push(slcf->types);
        if (type == NULL) {
            return NGX_CONF_ERROR;
        }

        type->len = value[i].len;

        type->data = ngx_palloc(cf->pool, type->len + 1);
        if (type->data == NULL) {
            return NGX_CONF_ERROR;
        }

        ngx_cpystrn(type->data, value[i].data, type->len + 1);
    }

    return NGX_CONF_OK;
}

static void *
ngx_http_esi_create_loc_conf(ngx_conf_t *cf)
{
    ngx_http_esi_loc_conf_t  *slcf;

    slcf = ngx_pcalloc(cf->pool, sizeof(ngx_http_esi_loc_conf_t));
    if (slcf == NULL) {
        return NGX_CONF_ERROR;
    }

    /*
     * set by ngx_pcalloc():
     *
     *     conf->types = NULL;
     */

    slcf->enable         = NGX_CONF_UNSET;
    slcf->silent_errors  = NGX_CONF_UNSET;
    slcf->min_file_chunk = NGX_CONF_UNSET_SIZE;
    slcf->max_depth      = NGX_CONF_UNSET_SIZE;

    return slcf;
}


static char *
ngx_http_esi_merge_loc_conf(ngx_conf_t *cf, void *parent, void *child)
{
    ngx_http_esi_loc_conf_t *prev = parent;
    ngx_http_esi_loc_conf_t *conf = child;

    ngx_str_t  *type;

    ngx_conf_merge_value(conf->enable, prev->enable, 0);
    ngx_conf_merge_value(conf->silent_errors, prev->silent_errors, 0);

    ngx_conf_merge_size_value(conf->min_file_chunk, prev->min_file_chunk, 1024);
    ngx_conf_merge_size_value(conf->max_depth, prev->max_depth, 256);

    if (conf->types == NULL) {
        if (prev->types == NULL) {
            conf->types = ngx_array_create(cf->pool, 1, sizeof(ngx_str_t));
            if (conf->types == NULL) {
                return NGX_CONF_ERROR;
            }

            type = ngx_array_push(conf->types);
            if (type == NULL) {
                return NGX_CONF_ERROR;
            }

            type->len = sizeof("text/html") - 1;
            type->data = (u_char *) "text/html";

        } else {
            conf->types = prev->types;
        }
    }

    return NGX_CONF_OK;
}

static ngx_int_t
ngx_http_esi_header_filter(ngx_http_request_t *r)
{
  ngx_uint_t                i;
  ngx_str_t                *type;
  ngx_http_esi_ctx_t       *ctx;
  ngx_http_esi_loc_conf_t  *slcf;

  slcf = ngx_http_get_module_loc_conf(r, ngx_http_esi_filter_module);

  if (!slcf->enable
      || r->headers_out.content_type.len == 0
      || r->headers_out.content_length_n == 0)
  {
    return ngx_http_next_header_filter(r);
  }


  type = slcf->types->elts;
  for (i = 0; i < slcf->types->nelts; ++i) {
    if (r->headers_out.content_type.len >= type[i].len
        && ngx_strncasecmp(r->headers_out.content_type.data,
                           type[i].data, type[i].len) == 0)
    {
      goto found;
    }
  }

  return ngx_http_next_header_filter(r);


found:

  ctx = ngx_pcalloc(r->pool, sizeof(ngx_http_esi_ctx_t));
  if (ctx == NULL) {
      return NGX_ERROR;
  }

  ngx_http_set_ctx(r, ctx, ngx_http_esi_filter_module);

  /* TODO: add request info to ctx */

  if (r == r->main) {
    ngx_http_clear_content_length(r);
    ngx_http_clear_last_modified(r);
  }

  return ngx_http_next_header_filter(r);
}

static ngx_int_t
ngx_http_esi_body_filter(ngx_http_request_t *r, ngx_chain_t *in)
{
  return NGX_OK;
}


static ngx_int_t
ngx_http_esi_filter_init(ngx_conf_t *cf)
{
  ngx_http_next_header_filter = ngx_http_top_header_filter;
  ngx_http_top_header_filter = ngx_http_esi_header_filter;

  ngx_http_next_body_filter = ngx_http_top_body_filter;
  ngx_http_top_body_filter = ngx_http_esi_body_filter;

  return NGX_OK;
}
