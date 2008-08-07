#include "ngx_regex.h"
#include "ngx_esi_tag.h"

ESITag *esi_tag_new(esi_tag_t type)
{
  ESITag *t = (ESITag*)malloc(sizeof(ESITag));
  t->chain = NULL;
  t->type = type;
  t->next = NULL;
  t->depth = 0;
  return t;
}
void esi_tag_free(ESITag *tag)
{
  /* TODO: free ngx_chain_t */

  ESITag *n, *next = tag->next;
  while( next ) {
    n = next->next;
    free(next);
    next = n;
  }

  free(tag);
}

void esi_tag_start(ESITag *tag)
{
  printf("start tag\n");
}

void esi_tag_close(ESITag *tag)
{
  printf("close tag: ");
  esi_tag_debug( tag );
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

void esi_tag_close_children( ESITag *tag, esi_tag_t type )
{
  ESITag *n, *next, *ptr, *prev;

  ptr = tag;
  printf("prev:");esi_tag_debug(ptr);
  next = tag->next;
  printf("next:");esi_tag_debug(next);

  while( next ) {
    prev = ptr;
    printf("prev:");esi_tag_debug(prev);
    n = next->next;
    printf("next:");esi_tag_debug(n);
    if( next->type == type ) {
      esi_tag_close( next );
      prev->next = NULL;
      break;
    }
    ptr = next;
    next = n;
  }
}

void esi_tag_buffer(ESITag *tag, ngx_chain_t *chain)
{
  printf("buffer tag\n");
}

int esi_vars_filter( ngx_chain_t *chain )
{
  return 0;
}
