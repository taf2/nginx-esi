#include "ngx_regex.h"
#include "ngx_esi_tag.h"
#include "ngx_buf_util.h"

ESITag *esi_tag_new(esi_tag_t type, ngx_http_esi_ctx_t *ctx)
{
  ESITag *t = (ESITag*)malloc(sizeof(ESITag));
  t->type = type;
  t->ctx = ctx;
  t->next = NULL;
  t->depth = 0;
  return t;
}
void esi_tag_free(ESITag *tag)
{
  ESITag *n, *next = tag->next;
  while( next ) {
    n = next->next;
    free(next);
    next = n;
  }

  //ngx_free_chain(tag->pool, tag->chain);
  free(tag);
}

void esi_tag_start(ESITag *tag)
{
  printf("start "); esi_tag_debug(tag);
}
void esi_tag_close(ESITag *tag)
{
  printf("close tag: "); esi_tag_debug( tag );
  esi_tag_free( tag );
}

void esi_tag_debug(ESITag *tag)
{
  if( !tag ) { printf("tag:(NULL)\n"); return; }

  switch(tag->type) {
  case ESI_TRY:
    printf("esi:try\n");
    break;
  case ESI_ATTEMPT:
    printf("esi:attempt\n");
    break;
  case ESI_EXCEPT:
    printf("esi:except\n");
    break;
  case ESI_INCLUDE:
    printf("esi:include\n");
    break;
  case ESI_INVALIDATE:
    printf("esi:invalidate\n");
    break;
  case ESI_VARS:
    printf("esi:vars\n");
    break;
  case ESI_REMOVE:
    printf("esi:remove\n");
    break;
  default:
    printf("unknown esi type\n");
    break;
  }

}

ESITag *esi_tag_close_children( ESITag *tag, esi_tag_t type )
{
  ESITag *n, *next, *ptr, *prev;

  ptr = tag;
  next = tag->next;

  while( next ) {
    prev = ptr;
    n = next->next;
    if( next->type == type ) {
      esi_tag_close( next );
      prev->next = NULL;
      return prev;
    }
    ptr = next;
    next = n;
  }
  return NULL;
}

static ngx_buf_t *esi_tag_alloc_buf(ESITag *tag, const void *data, size_t length)
{
  ngx_buf_t *b;
  b = ngx_pcalloc(tag->ctx->request->pool, sizeof(ngx_buf_t));
  if (b == NULL) {
    ngx_log_error(NGX_LOG_ERR, tag->ctx->request->connection->log, 0, "Failed to allocate response buffer.");
   // return NGX_HTTP_INTERNAL_SERVER_ERROR;
   return NULL;
  }
  b->pos = (u_char*)(data); /* first position in memory of the data */
  b->last = (((u_char*)data) + length); /* last position */

  b->memory = 1; /* content is in read-only memory */
               /* (i.e., filters should copy it rather than rewrite in place) */
  b->last_buf = 0;
  return b;
}

ngx_buf_t *esi_tag_buffer(ESITag *tag, const void *data, size_t length)
{
  //printf("buffer: %lu for tag: ", length); esi_tag_debug(tag);
  switch( tag->type ) {
    case ESI_VARS:
    case ESI_ATTEMPT:
      return ngx_buf_from_data( tag->ctx->request->pool, data, length );
    case ESI_EXCEPT:
      /* TODO: check if an exception occurred while processing the attempt before returing this buffer */
      return ngx_buf_from_data( tag->ctx->request->pool, data, length );
    case ESI_INCLUDE:
    case ESI_INVALIDATE:
    case ESI_REMOVE:
    case ESI_TRY:
    default:
      return NULL;
  }
}

int esi_vars_filter( ngx_chain_t *chain )
{
  return 0;
}
