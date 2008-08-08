#include "ngx_buf_util.h"

ngx_chain_t *ngx_chain_append_buffer(ngx_pool_t *pool, ngx_chain_t *chain, ngx_buf_t *buf)
{
  /* add the new buffer to the last_buf in the chain */
  if( chain->buf ) {
    printf( "append to chain: [" ); debug_string( (const char*)buf->pos, ngx_buf_size(buf) );printf("]\n");

    /* allocate a new buffer link */
    chain->next = ngx_alloc_chain_link(pool);
    chain->next->buf = buf;
    /* mark this buffer as the last buffer */
    chain->next->next = NULL;
    /* advance the last buffer link */
    return chain->next;
    //ctx->last_buf = chain->next;
  }
  else {
    printf( "assign to chain: [" ); debug_string( (const char*)buf->pos, ngx_buf_size(buf) );printf("]\n");
    chain->buf = buf;
    chain->next = NULL;
    return chain;
  }
}

ngx_buf_t *ngx_buf_from_data(ngx_pool_t *pool, const void *data, size_t length)
{
  ngx_buf_t *b = ngx_create_temp_buf(pool,length);
  if (b == NULL) {
   return NULL;
  }

  ngx_memcpy( b->start, (u_char*)data, length );
  b->last = b->end;
  b->last_buf = 0;

  return b;
}

void debug_string( const char *msg, int length )
{
  if( msg && length > 0 ) {
    fwrite( msg, sizeof(char), length, stdout );
  }
  else {
    printf("(NULL)");
  }
}
