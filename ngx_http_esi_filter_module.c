/*
 * Copyright (C) Todd A. Fisher
 *
 * This is a first attempt at nginx module.  The idea will be to use mongrel-esi ragel based parser to detect and 
 * tigger esi events.
 *
 * This module is model'ed after ssi module
 */

#include "ngx_esi_tag.h"
#include "ngx_http_esi_filter_module.h"
#include "ngx_buf_util.h"

typedef struct {
    ngx_hash_t                hash;
    ngx_hash_keys_arrays_t    commands;
} ngx_http_esi_main_conf_t;

typedef struct {
  ngx_flag_t     enable;          /* enable esi filter */
  ngx_flag_t     silent_errors;   /* ignore include errors, don't raise exceptions */
  ngx_array_t   *types;           /* array of ngx_str_t */

  size_t         min_file_chunk;  /* smallest size chunk */
  size_t         max_depth;       /* how many times to follow an esi:include redirect... */
} ngx_http_esi_loc_conf_t;


static ngx_int_t ngx_http_esi_preconfiguration(ngx_conf_t *cf);
static ngx_int_t ngx_http_esi_filter_init(ngx_conf_t *cf);
static char *ngx_http_esi_types(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);
static void *ngx_http_esi_create_main_conf(ngx_conf_t *cf);
static char *ngx_http_esi_init_main_conf(ngx_conf_t *cf, void *conf);
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
    ngx_http_esi_preconfiguration,         /* preconfiguration */
    ngx_http_esi_filter_init,              /* postconfiguration */

    ngx_http_esi_create_main_conf,         /* create main configuration */
    ngx_http_esi_init_main_conf,           /* init main configuration */

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

static ngx_int_t
ngx_http_esi_preconfiguration(ngx_conf_t *cf)
{
    ngx_http_esi_main_conf_t  *smcf;

    smcf = ngx_http_conf_get_module_main_conf(cf, ngx_http_esi_filter_module);



    return NGX_OK;
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
    //debug_string((const char*)type[i].data, type[i].len);
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

  ctx->request = r;
  ctx->root_tag = NULL;
  ctx->open_tag = NULL;
  /* allocate some output buffers */
  ctx->last_buf = ctx->chain = ngx_alloc_chain_link(r->pool);
  ctx->last_buf->buf = ctx->chain->buf = NULL;
  ctx->last_buf->next = ctx->chain->next = NULL;
  ctx->dcount = 0;

  r->filter_need_in_memory = 1;

  if (r == r->main) {
    ngx_http_clear_content_length(r);
    ngx_http_clear_last_modified(r);
  }

  return ngx_http_next_header_filter(r);
}

static void
esi_start_tag( const void *data, const char *name_start, size_t length, ESIAttribute *attributes, void *context )
{
  ESITag *tag = NULL;
  ngx_http_esi_ctx_t *ctx = (ngx_http_esi_ctx_t*)context;

  if( !strncmp("esi:try",name_start,length) ) {
    tag = esi_tag_new(ESI_TRY, ctx);
  }
  else if( !strncmp("esi:attempt",name_start,length) ) {
    tag = esi_tag_new(ESI_ATTEMPT, ctx);
  }
  else if( !strncmp("esi:except",name_start,length) ) {
    tag = esi_tag_new(ESI_EXCEPT, ctx);
  }
  else if( !strncmp("esi:include",name_start,length) ) {
    tag = esi_tag_new(ESI_INCLUDE, ctx);
  }
  else if( !strncmp("esi:invalidate",name_start,length) ) {
    tag = esi_tag_new(ESI_INVALIDATE, ctx);
  }
  else if( !strncmp("esi:vars",name_start,length) ) {
    tag = esi_tag_new(ESI_VARS, ctx);
  }
  else if( !strncmp("esi:remove",name_start,length) ) {
    tag = esi_tag_new(ESI_REMOVE, ctx);
  }
  else {
    //XXX: invalid tag report it
    printf("invalid start tag\n");debug_string( name_start, length ); printf("\n" );
    return;
  }
  if( ctx->root_tag ) {
    ESITag *last = ctx->root_tag;
    while( last->next ) {
      last = last->next;
    }
    last->next = tag;
  }
  else {
    ctx->root_tag = tag;
  }
  ctx->open_tag = tag;
  esi_tag_start( tag );
//  printf("start tag:%d ", (int)length ); debug_string( name_start, length ); printf("\n" );
}

static void
esi_end_tag( const void *data, const char *name_start, size_t length, void *context )
{
  ngx_http_esi_ctx_t *ctx = (ngx_http_esi_ctx_t*)context;

  if( !strncmp("esi:try",name_start,length) && ctx->root_tag ) {
    if( ctx->root_tag->type == ESI_TRY ) {
      esi_tag_close( ctx->root_tag );
      ctx->open_tag = ctx->root_tag = NULL;
    }
    else {
      ctx->open_tag = esi_tag_close_children( ctx->root_tag, ESI_TRY );
    }
  }
  else if( !strncmp("esi:attempt",name_start,length) && ctx->root_tag ) {
    if( ctx->root_tag->type == ESI_ATTEMPT ) {
      esi_tag_close( ctx->root_tag );
      ctx->open_tag = ctx->root_tag = NULL;
    }
    else {
      ctx->open_tag = esi_tag_close_children( ctx->root_tag, ESI_ATTEMPT );
    }
  }
  else if( !strncmp("esi:except",name_start,length) && ctx->root_tag ) {
    if( ctx->root_tag->type == ESI_EXCEPT ) {
      esi_tag_close( ctx->root_tag );
      ctx->open_tag = ctx->root_tag = NULL;
    }
    else {
      ctx->open_tag = esi_tag_close_children( ctx->root_tag, ESI_EXCEPT );
    }
  }
  else if( !strncmp("esi:include",name_start,length) && ctx->root_tag ) {
    if( ctx->root_tag->type == ESI_INCLUDE ) {
      esi_tag_close( ctx->root_tag );
      ctx->open_tag = ctx->root_tag = NULL;
    }
    else {
      ctx->open_tag = esi_tag_close_children( ctx->root_tag, ESI_INCLUDE );
    }
  }
  else if( !strncmp("esi:invalidate",name_start,length) && ctx->root_tag ) {
    if( ctx->root_tag->type == ESI_INVALIDATE ) {
      esi_tag_close( ctx->root_tag );
      ctx->open_tag = ctx->root_tag = NULL;
    }
    else {
      ctx->open_tag = esi_tag_close_children( ctx->root_tag, ESI_INVALIDATE );
    }
  }
  else if( !strncmp("esi:vars",name_start,length) && ctx->root_tag ) {
    if( ctx->root_tag->type == ESI_VARS ) {
      esi_tag_close( ctx->root_tag );
      ctx->open_tag = ctx->root_tag = NULL;
    }
    else {
      ctx->open_tag = esi_tag_close_children( ctx->root_tag, ESI_VARS );
    }
  }
  else if( !strncmp("esi:remove",name_start,length) && ctx->root_tag ) {
    if( ctx->root_tag->type == ESI_REMOVE ) {
      esi_tag_close( ctx->root_tag );
      ctx->open_tag = ctx->root_tag = NULL;
    }
    else {
      ctx->open_tag = esi_tag_close_children( ctx->root_tag, ESI_REMOVE );
    }
  }
  else {
    printf("invalid end tag");debug_string( name_start, length ); printf("\n" );
  }
//  printf("end tag:%d ", (int)length ); debug_string( name_start, length ); printf("\n" );
}

static void
esi_output( const void *data, size_t length, void *context )
{
  ngx_buf_t *buf;
  ngx_http_esi_ctx_t *ctx = (ngx_http_esi_ctx_t*)context;

  if( ctx->root_tag && ctx->open_tag ) {
    buf = esi_tag_buffer( ctx->open_tag, data, length );
  }
  else {
    buf = ngx_buf_from_data( ctx->request->pool, data, length );
  }

  if( buf ) {
    ctx->dcount++;
    ctx->last_buf = ngx_chain_append_buffer( ctx->request->pool, ctx->last_buf, buf );
  }
  //printf("output char len: %d \n", (int)length );debug_string( (const char*)data, (int)length );printf("\n");
}

static ngx_int_t
ngx_http_esi_body_filter(ngx_http_request_t *r, ngx_chain_t *in)
{
  off_t size;
  int count = 0;
  ngx_chain_t *chain_link;
  short has_last_buffer = 0;
  ngx_http_esi_ctx_t   *ctx;

  ctx = ngx_http_get_module_ctx(r, ngx_http_esi_filter_module);

  if( ctx == NULL || in == NULL || r->header_only ) { 
    return ngx_http_next_body_filter(r, in);
  }

  ctx->request = r;
  ctx->root_tag = NULL;
  ctx->open_tag = NULL;

  if( !ctx->parser ) {
    ctx->parser = esi_parser_new();
    esi_parser_init( ctx->parser );
    ctx->parser->user_data = (void*)ctx;
    esi_parser_start_tag_handler( ctx->parser, esi_start_tag );
    esi_parser_end_tag_handler( ctx->parser, esi_end_tag );
    esi_parser_output_handler( ctx->parser, esi_output );
  }

//  printf( "called esi body filter\n" );

  for( chain_link = in; chain_link != NULL; chain_link = chain_link->next ) {
    size = ngx_buf_size(chain_link->buf);

    //printf("buf size: %d, link: %d, buf: '", (int)size, count ); debug_string( (const char*)chain_link->buf->start, (int)size ); printf("'\n");
    esi_parser_execute( ctx->parser, (const char*)chain_link->buf->pos, (size_t)size );

    if( chain_link->buf->last_buf ) {
      esi_parser_finish( ctx->parser );
      has_last_buffer  = 1;
      break;
    }
    else {
      // hrm... ?
    }
    ++count;
  }

  ctx->last_buf->buf->last_buf = 1;

#if 0
  printf( "debugging buffer\n" );
/*
 * Copyright (C) Todd A. Fisher
 *
 * This is a first attempt at nginx module.  The idea will be to use mongrel-esi ragel based parser to detect and 
 * tigger esi events.
 *
 * This module is model'ed after ssi module
 */

#include "ngx_esi_tag.h"
#include "ngx_http_esi_filter_module.h"
#include "ngx_buf_util.h"

typedef struct {
    ngx_hash_t                hash;
    ngx_hash_keys_arrays_t    commands;
} ngx_http_esi_main_conf_t;

typedef struct {
  ngx_flag_t     enable;          /* enable esi filter */
  ngx_flag_t     silent_errors;   /* ignore include errors, don't raise exceptions */
  ngx_array_t   *types;           /* array of ngx_str_t */

  size_t         min_file_chunk;  /* smallest size chunk */
  size_t         max_depth;       /* how many times to follow an esi:include redirect... */
} ngx_http_esi_loc_conf_t;


static ngx_int_t ngx_http_esi_preconfiguration(ngx_conf_t *cf);
static ngx_int_t ngx_http_esi_filter_init(ngx_conf_t *cf);
static char *ngx_http_esi_types(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);
static void *ngx_http_esi_create_main_conf(ngx_conf_t *cf);
static char *ngx_http_esi_init_main_conf(ngx_conf_t *cf, void *conf);
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
    ngx_http_esi_preconfiguration,         /* preconfiguration */
    ngx_http_esi_filter_init,              /* postconfiguration */

    ngx_http_esi_create_main_conf,         /* create main configuration */
    ngx_http_esi_init_main_conf,           /* init main configuration */

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

static ngx_int_t
ngx_http_esi_preconfiguration(ngx_conf_t *cf)
{
    ngx_http_esi_main_conf_t  *smcf;

    smcf = ngx_http_conf_get_module_main_conf(cf, ngx_http_esi_filter_module);



    return NGX_OK;
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
    //debug_string((const char*)type[i].data, type[i].len);
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

  ctx->request = r;
  ctx->root_tag = NULL;
  ctx->open_tag = NULL;
  /* allocate some output buffers */
  ctx->last_buf = ctx->chain = ngx_alloc_chain_link(r->pool);
  ctx->last_buf->buf = ctx->chain->buf = NULL;
  ctx->last_buf->next = ctx->chain->next = NULL;
  ctx->dcount = 0;

  r->filter_need_in_memory = 1;

  if (r == r->main) {
    ngx_http_clear_content_length(r);
    ngx_http_clear_last_modified(r);
  }

  return ngx_http_next_header_filter(r);
}

static void
esi_start_tag( const void *data, const char *name_start, size_t length, ESIAttribute *attributes, void *context )
{
  ESITag *tag = NULL;
  ngx_http_esi_ctx_t *ctx = (ngx_http_esi_ctx_t*)context;

  if( !strncmp("esi:try",name_start,length) ) {
    tag = esi_tag_new(ESI_TRY, ctx);
  }
  else if( !strncmp("esi:attempt",name_start,length) ) {
    tag = esi_tag_new(ESI_ATTEMPT, ctx);
  }
  else if( !strncmp("esi:except",name_start,length) ) {
    tag = esi_tag_new(ESI_EXCEPT, ctx);
  }
  else if( !strncmp("esi:include",name_start,length) ) {
    tag = esi_tag_new(ESI_INCLUDE, ctx);
  }
  else if( !strncmp("esi:invalidate",name_start,length) ) {
    tag = esi_tag_new(ESI_INVALIDATE, ctx);
  }
  else if( !strncmp("esi:vars",name_start,length) ) {
    tag = esi_tag_new(ESI_VARS, ctx);
  }
  else if( !strncmp("esi:remove",name_start,length) ) {
    tag = esi_tag_new(ESI_REMOVE, ctx);
  }
  else {
    //XXX: invalid tag report it
    printf("invalid start tag\n");debug_string( name_start, length ); printf("\n" );
    return;
  }
  if( ctx->root_tag ) {
    ESITag *last = ctx->root_tag;
    while( last->next ) {
      last = last->next;
    }
    last->next = tag;
  }
  else {
    ctx->root_tag = tag;
  }
  ctx->open_tag = tag;
  esi_tag_start( tag );
//  printf("start tag:%d ", (int)length ); debug_string( name_start, length ); printf("\n" );
}

static void
esi_end_tag( const void *data, const char *name_start, size_t length, void *context )
{
  ngx_http_esi_ctx_t *ctx = (ngx_http_esi_ctx_t*)context;

  if( !strncmp("esi:try",name_start,length) && ctx->root_tag ) {
    if( ctx->root_tag->type == ESI_TRY ) {
      esi_tag_close( ctx->root_tag );
      ctx->open_tag = ctx->root_tag = NULL;
    }
    else {
      ctx->open_tag = esi_tag_close_children( ctx->root_tag, ESI_TRY );
    }
  }
  else if( !strncmp("esi:attempt",name_start,length) && ctx->root_tag ) {
    if( ctx->root_tag->type == ESI_ATTEMPT ) {
      esi_tag_close( ctx->root_tag );
      ctx->open_tag = ctx->root_tag = NULL;
    }
    else {
      ctx->open_tag = esi_tag_close_children( ctx->root_tag, ESI_ATTEMPT );
    }
  }
  else if( !strncmp("esi:except",name_start,length) && ctx->root_tag ) {
    if( ctx->root_tag->type == ESI_EXCEPT ) {
      esi_tag_close( ctx->root_tag );
      ctx->open_tag = ctx->root_tag = NULL;
    }
    else {
      ctx->open_tag = esi_tag_close_children( ctx->root_tag, ESI_EXCEPT );
    }
  }
  else if( !strncmp("esi:include",name_start,length) && ctx->root_tag ) {
    if( ctx->root_tag->type == ESI_INCLUDE ) {
      esi_tag_close( ctx->root_tag );
      ctx->open_tag = ctx->root_tag = NULL;
    }
    else {
      ctx->open_tag = esi_tag_close_children( ctx->root_tag, ESI_INCLUDE );
    }
  }
  else if( !strncmp("esi:invalidate",name_start,length) && ctx->root_tag ) {
    if( ctx->root_tag->type == ESI_INVALIDATE ) {
      esi_tag_close( ctx->root_tag );
      ctx->open_tag = ctx->root_tag = NULL;
    }
    else {
      ctx->open_tag = esi_tag_close_children( ctx->root_tag, ESI_INVALIDATE );
    }
  }
  else if( !strncmp("esi:vars",name_start,length) && ctx->root_tag ) {
    if( ctx->root_tag->type == ESI_VARS ) {
      esi_tag_close( ctx->root_tag );
      ctx->open_tag = ctx->root_tag = NULL;
    }
    else {
      ctx->open_tag = esi_tag_close_children( ctx->root_tag, ESI_VARS );
    }
  }
  else if( !strncmp("esi:remove",name_start,length) && ctx->root_tag ) {
    if( ctx->root_tag->type == ESI_REMOVE ) {
      esi_tag_close( ctx->root_tag );
      ctx->open_tag = ctx->root_tag = NULL;
    }
    else {
      ctx->open_tag = esi_tag_close_children( ctx->root_tag, ESI_REMOVE );
    }
  }
  else {
    printf("invalid end tag");debug_string( name_start, length ); printf("\n" );
  }
//  printf("end tag:%d ", (int)length ); debug_string( name_start, length ); printf("\n" );
}

static void
esi_output( const void *data, size_t length, void *context )
{
  ngx_buf_t *buf;
  ngx_http_esi_ctx_t *ctx = (ngx_http_esi_ctx_t*)context;

  if( ctx->root_tag && ctx->open_tag ) {
    buf = esi_tag_buffer( ctx->open_tag, data, length );
  }
  else {
    buf = ngx_buf_from_data( ctx->request->pool, data, length );
  }

  if( buf ) {
    ctx->dcount++;
    ctx->last_buf = ngx_chain_append_buffer( ctx->request->pool, ctx->last_buf, buf );
  }
  //printf("output char len: %d \n", (int)length );debug_string( (const char*)data, (int)length );printf("\n");
}

static ngx_int_t
ngx_http_esi_body_filter(ngx_http_request_t *r, ngx_chain_t *in)
{
  off_t size;
  int count = 0;
  ngx_chain_t *chain_link;
  short has_last_buffer = 0;
  ngx_http_esi_ctx_t   *ctx;

  ctx = ngx_http_get_module_ctx(r, ngx_http_esi_filter_module);

  if( ctx == NULL || in == NULL || r->header_only ) { 
    return ngx_http_next_body_filter(r, in);
  }

  ctx->request = r;
  ctx->root_tag = NULL;
  ctx->open_tag = NULL;

  if( !ctx->parser ) {
    ctx->parser = esi_parser_new();
    esi_parser_init( ctx->parser );
    ctx->parser->user_data = (void*)ctx;
    esi_parser_start_tag_handler( ctx->parser, esi_start_tag );
    esi_parser_end_tag_handler( ctx->parser, esi_end_tag );
    esi_parser_output_handler( ctx->parser, esi_output );
  }

//  printf( "called esi body filter\n" );

  for( chain_link = in; chain_link != NULL; chain_link = chain_link->next ) {
    size = ngx_buf_size(chain_link->buf);

    //printf("buf size: %d, link: %d, buf: '", (int)size, count ); debug_string( (const char*)chain_link->buf->start, (int)size ); printf("'\n");
    esi_parser_execute( ctx->parser, (const char*)chain_link->buf->pos, (size_t)size );

    if( chain_link->buf->last_buf ) {
      esi_parser_finish( ctx->parser );
      has_last_buffer  = 1;
      break;
    }
    else {
      // hrm... ?
    }
    ++count;
  }

  ctx->last_buf->buf->last_buf = 1;

#if 0
  printf( "debugging buffer\n" );
  count = 0;
  for( chain_link = ctx->chain; chain_link != NULL; chain_link = chain_link->next ) {
    size = ngx_buf_size(chain_link->buf);
    printf("buf size: %d, link: %d of %d, buf: '", (int)size, count, ctx->dcount ); debug_string( (const char*)chain_link->buf->pos, (int)size ); printf("'\n");
    ++count;
  }
#endif
  
  if( has_last_buffer ) {
    esi_parser_free( ctx->parser );
    ctx->parser = NULL;
  }

  //return ngx_http_next_body_filter(r, in);
  return ngx_http_next_body_filter(r, ctx->chain);
}

static void *
ngx_http_esi_create_main_conf(ngx_conf_t *cf)
{
    ngx_http_esi_main_conf_t  *smcf;

    smcf = ngx_pcalloc(cf->pool, sizeof(ngx_http_esi_main_conf_t));
    if (smcf == NULL) {
        return NGX_CONF_ERROR;
    }

    smcf->commands.pool = cf->pool;
    smcf->commands.temp_pool = cf->temp_pool;

    if (ngx_hash_keys_array_init(&smcf->commands, NGX_HASH_SMALL) != NGX_OK) {
        return NGX_CONF_ERROR;
    }

    return smcf;
}


static char *
ngx_http_esi_init_main_conf(ngx_conf_t *cf, void *conf)
{
    ngx_http_esi_main_conf_t *smcf = conf;

    ngx_hash_init_t  hash;


    hash.hash = &smcf->hash;
    hash.key = ngx_hash_key;
    hash.max_size = 1024;
    hash.bucket_size = ngx_cacheline_size;
    hash.name = "esi_command_hash";
    hash.pool = cf->pool;
    hash.temp_pool = NULL;

    if (ngx_hash_init(&hash, smcf->commands.keys.elts,
                      smcf->commands.keys.nelts)
        != NGX_OK)
    {
        return NGX_CONF_ERROR;
    }

    return NGX_CONF_OK;
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
  count = 0;
  for( chain_link = ctx->chain; chain_link != NULL; chain_link = chain_link->next ) {
    size = ngx_buf_size(chain_link->buf);
    printf("buf size: %d, link: %d of %d, buf: '", (int)size, count, ctx->dcount ); debug_string( (const char*)chain_link->buf->pos, (int)size ); printf("'\n");
    ++count;
  }
#endif
  
  if( has_last_buffer ) {
    esi_parser_free( ctx->parser );
    ctx->parser = NULL;
  }

  //return ngx_http_next_body_filter(r, in);
  return ngx_http_next_body_filter(r, ctx->chain);
}

static void *
ngx_http_esi_create_main_conf(ngx_conf_t *cf)
{
    ngx_http_esi_main_conf_t  *smcf;

    smcf = ngx_pcalloc(cf->pool, sizeof(ngx_http_esi_main_conf_t));
    if (smcf == NULL) {
        return NGX_CONF_ERROR;
    }

    smcf->commands.pool = cf->pool;
    smcf->commands.temp_pool = cf->temp_pool;

    if (ngx_hash_keys_array_init(&smcf->commands, NGX_HASH_SMALL) != NGX_OK) {
        return NGX_CONF_ERROR;
    }

    return smcf;
}


static char *
ngx_http_esi_init_main_conf(ngx_conf_t *cf, void *conf)
{
    ngx_http_esi_main_conf_t *smcf = conf;

    ngx_hash_init_t  hash;


    hash.hash = &smcf->hash;
    hash.key = ngx_hash_key;
    hash.max_size = 1024;
    hash.bucket_size = ngx_cacheline_size;
    hash.name = "esi_command_hash";
    hash.pool = cf->pool;
    hash.temp_pool = NULL;

    if (ngx_hash_init(&hash, smcf->commands.keys.elts,
                      smcf->commands.keys.nelts)
        != NGX_OK)
    {
        return NGX_CONF_ERROR;
    }

    return NGX_CONF_OK;
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
