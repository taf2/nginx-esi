#ifndef NGX_BUF_UTIL_H
#define NGX_BUF_UTIL_H

#include "ngx_http_esi_filter_module.h"

/* methods for working with ngx buffers (ngx_buf_t) and buffer chains (ngx_chain_t) */

ngx_chain_t *ngx_chain_append_buffer(ngx_pool_t *pool, ngx_chain_t *chain, ngx_buf_t *buf);
ngx_buf_t *ngx_buf_from_data(ngx_pool_t *pool, const void *data, size_t length);
void debug_string( const char *msg, int length );


#endif
