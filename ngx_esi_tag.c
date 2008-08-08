#include "ngx_regex.h"
#include "ngx_esi_tag.h"
#include "ngx_buf_util.h"

ESITag *esi_tag_new(esi_tag_t type, ngx_http_esi_ctx_t *ctx)
{
  ESITag *t = (ESITag*)malloc(sizeof(ESITag));
  t->type = type;
  t->ctx = ctx;
  t->next = NULL;
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

esi_tag_t esi_tag_str_to_type( const char *tag_name, size_t length )
{
  if( !strncmp("esi:try",tag_name,length) ) {
    return ESI_TRY;
  }
  else if( !strncmp("esi:attempt",tag_name,length) ) {
    return ESI_ATTEMPT;
  }
  else if( !strncmp("esi:except",tag_name,length) ) {
    return ESI_EXCEPT;
  }
  else if( !strncmp("esi:include",tag_name,length) ) {
    return ESI_INCLUDE;
  }
  else if( !strncmp("esi:invalidate",tag_name,length) ) {
    return ESI_INVALIDATE;
  }
  else if( !strncmp("esi:vars",tag_name,length) ) {
    return ESI_VARS;
  }
  else if( !strncmp("esi:remove",tag_name,length) ) {
    return ESI_REMOVE;
  }
  return ESI_NONE;
}

static void esi_tag_start_include(ESITag *tag, ESIAttribute *attributes)
{
  ESIAttribute *attr = attributes;
//  printf( "esi:include\n" );
  while( attr ) {
//    printf( "\t%s => %s\n", attr->name, attr->value );
    attr = attr->next;
  }
}

void esi_tag_start(ESITag *tag, ESIAttribute *attributes)
{
  switch(tag->type) {
    case ESI_TRY:
      break;
    case ESI_ATTEMPT:
      tag->ctx->exception_raised = 0; /* reset the exception raised state to 0 */
      break;
    case ESI_EXCEPT:
      break;
    case ESI_INCLUDE:
      esi_tag_start_include( tag, attributes );
      break;
    case ESI_INVALIDATE:
      break;
    case ESI_VARS:
      break;
    case ESI_REMOVE:
      break;
    default:
      break;
  }
//  printf("start "); esi_tag_debug(tag);
}
void esi_tag_close(ESITag *tag)
{
//  printf("close tag: "); esi_tag_debug( tag );
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
      if( tag->ctx->exception_raised ) {
        return ngx_buf_from_data( tag->ctx->request->pool, data, length );
      }
      /* fall through */
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
