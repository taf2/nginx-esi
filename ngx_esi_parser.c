#line 1 "ngx_esi_parser.rl"
/** 
 * Copyright (c) 2008 Todd A. Fisher
 * see LICENSE
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "ngx_esi_parser.h"

#ifdef DEBUG
static void debug_string( const char *msg, const char *str, size_t len )
{
  char *pstr = esi_strndup( str, len );
  printf( "%s :'%s'\n", msg, pstr );
  free( pstr );
}
#else
#define debug_string(m,s,l)
#endif

/* define default callbacks */
static void 
esi_parser_default_start_cb( const void *data,
                             const char *name_start,
                             size_t name_length,
                             ESIAttribute *attributes,
                             void *user_data )
{
}
static void 
esi_parser_default_end_cb( const void *data,
                           const char *name_start,
                           size_t name_length,
                           void *user_data )
{
}
static void 
esi_parser_default_output_cp(const void *data,
                             size_t length,
                             void *user_data)
{
}

/* 
 * flush output buffer
 */
static void esi_parser_flush_output( ESIParser *parser )
{
  if( parser->output_buffer_size > 0 ) {
    //debug_string( "esi_parser_flush_output:", parser->output_buffer, parser->output_buffer_size );
    parser->output_handler( (void*)parser->output_buffer, parser->output_buffer_size, parser->user_data );
    parser->output_buffer_size = 0;
  }
}
/* send the character to the output handler marking it 
 * as ready for consumption, e.g. not an esi tag
 */
static void esi_parser_echo_char( ESIParser *parser, char ch )
{
  parser->output_buffer[parser->output_buffer_size++] = ch;
  if( parser->output_buffer_size == ESI_OUTPUT_BUFFER_SIZE ) {
    // flush the buffer to the consumer
    esi_parser_flush_output( parser );
  }
}
/* send any buffered characters to the output handler. 
 * This happens when we enter a case such as <em>  where the
 * first two characters < and e  match the <esi:  expression
 */
static void esi_parser_echo_buffer( ESIParser *parser )
{
  size_t i = 0, len = parser->echobuffer_index + 1;;
  //debug_string( "echobuffer", parser->echobuffer, parser->echobuffer_index+1 );
  //parser->output_handler( parser->echobuffer, parser->echobuffer_index+1, parser->user_data );
  for( ; i < len; ++i ) {
    esi_parser_echo_char( parser, parser->echobuffer[i] );
  }
}
/*
 * clear the buffer, no buffered characters should be emitted .
 * e.g. we matched an esi tag completely and all buffered characters can be tossed out
 */
static void esi_parser_echobuffer_clear( ESIParser *parser )
{
  parser->echobuffer_index = -1;
}

/*
 * add a character to the echobuffer. 
 * this happens when we can't determine if the character is allowed to be sent to the client device
 * e.g. matching <e  it's not yet determined if these characters are safe to send or not
 */
static void esi_parser_concat_to_echobuffer( ESIParser *parser, char ch )
{
  parser->echobuffer_index++;

  if( parser->echobuffer_allocated <= parser->echobuffer_index ) {
    /* double the echobuffer size 
     * we're getting some crazy input if this case ever happens
     */
    parser->echobuffer_allocated *= 2;
    parser->echobuffer = (char*)realloc( parser->echobuffer, parser->echobuffer_allocated );
  }
  parser->echobuffer[parser->echobuffer_index] = ch;
//  debug_string( "echo buffer", parser->echobuffer, parser->echobuffer_index+1 );
}
/*
 * the mark boundary is not always going to be exactly on the attribute or tag name boundary
 * this trims characters from the left to right, advancing *ptr and reducing *len
 */
static void ltrim_pointer( const char **ptr, const char *bounds, size_t *len )
{
  // remove any spaces or = at the before the value
  while( (isspace( **ptr ) ||
         **ptr == '=' ||
         **ptr == '"' ||
         **ptr == '<' ||
         **ptr == '\'' ) && (*len > 0) && (*ptr != bounds) ) {
    (*ptr)++;
    (*len)--;
  }
}
/*
 * similar to ltrim_pointer, this walks from bounds to *ptr, reducing *len 
 */
static void rtrim_pointer( const char **ptr, const char *bounds, size_t *len )
{
  bounds = (*ptr+(*len-1));
  // remove any spaces or = at the before the value
  while( (isspace( *bounds ) ||
         *bounds == '=' ||
         *bounds == '"' ||
         *bounds == '>' ||
         *bounds == '\'') && (*len > 0) && (*ptr != bounds) ){
    bounds--;
    (*len)--;
  }
}

#line 322 "ngx_esi_parser.rl"



#line 147 "ngx_esi_parser.c"
static const char _esi_eof_actions[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 10, 10, 10, 10
};

static const int esi_start = 75;
static const int esi_first_final = 75;
static const int esi_error = -1;

static const int esi_en_main = 75;

#line 325 "ngx_esi_parser.rl"

/* dup the string up to len */
char *esi_strndup( const char *str, size_t len )
{
  char *s = (char*)malloc(sizeof(char)*(len+1));
  memcpy( s, str, len );
  s[len] = '\0';
  return s;
}

ESIAttribute *esi_attribute_new( const char *name, size_t name_length, const char *value, size_t value_length )
{
  ESIAttribute *attr = (ESIAttribute*)malloc(sizeof(ESIAttribute));
  attr->name = esi_strndup(name, name_length);
  attr->value = esi_strndup(value, value_length);
  attr->next = NULL;
  return attr;
}

ESIAttribute *esi_attribute_copy( ESIAttribute *attribute )
{
  ESIAttribute *head, *nattr;
  if( !attribute ){ return NULL; }

  // copy the first attribute
  nattr = esi_attribute_new( attribute->name, strlen( attribute->name ),
                             attribute->value, strlen( attribute->value ) );
  // save a pointer for return
  head = nattr;
  // copy next attributes
  attribute = attribute->next;
  while( attribute ) {
    // set the next attribute
    nattr->next = esi_attribute_new( attribute->name, strlen( attribute->name ),
                                     attribute->value, strlen( attribute->value ) );
    // next attribute
    nattr = nattr->next;
    attribute = attribute->next;
  }
  return head;
}

void esi_attribute_free( ESIAttribute *attribute )
{
  ESIAttribute *ptr;
  while( attribute ){
    free( attribute->name );
    free( attribute->value );
    ptr = attribute->next;
    free( attribute );
    attribute = ptr;
  }
}

ESIParser *esi_parser_new()
{
  ESIParser *parser = (ESIParser*)malloc(sizeof(ESIParser));
  parser->cs = esi_start;
  parser->mark = NULL;
  parser->tag_text = NULL;
  parser->attr_key = NULL;
  parser->attr_value = NULL;
  parser->overflow_data_size = 0;
  parser->overflow_data = NULL;

  /* allocate ESI_OUTPUT_BUFFER_SIZE bytes for the echobuffer */
  parser->echobuffer_allocated = ESI_OUTPUT_BUFFER_SIZE;
  parser->echobuffer_index = -1;
  parser->echobuffer = (char*)malloc(sizeof(char)*parser->echobuffer_allocated);

  parser->attributes = NULL;
  parser->last = NULL;

  parser->start_tag_handler = esi_parser_default_start_cb;
  parser->end_tag_handler = esi_parser_default_end_cb;
  parser->output_handler = esi_parser_default_output_cp;

  parser->output_buffer_size = 0;
  memset( parser->output_buffer, 0, ESI_OUTPUT_BUFFER_SIZE );

  return parser;
}
void esi_parser_free( ESIParser *parser )
{
  if( parser->overflow_data ){ free( parser->overflow_data ); }

  free( parser->echobuffer );
  esi_attribute_free( parser->attributes );

  free( parser );
}

void esi_parser_output_handler( ESIParser *parser, esi_output_cb output_handler )
{
  parser->output_handler = output_handler;
}

int esi_parser_init( ESIParser *parser )
{
  int cs;
  
#line 269 "ngx_esi_parser.c"
	{
	cs = esi_start;
	}
#line 426 "ngx_esi_parser.rl"
  parser->prev_state = parser->cs = cs;
  return 0;
}

static int compute_offset( const char *mark, const char *data )
{
  if( mark ) {
    return mark - data;
  }
  return -1;
}

/*
 * scans the data buffer for a start sequence /<$/, /<e$/, /<es$/, /<esi$/, /<esi:$/
 * returns index of if start sequence found else returns -1
 */
static int
esi_parser_scan_for_start( ESIParser *parser, const char *data, size_t length )
{
  int i, f = -2, s = -2, len = (int)length;
  char ch;

  for( i = 0; i < len; ++i ) {
    ch = data[i];
    switch( ch ) {
    case '<':
      f = s = i;
      break;
    case '/':
      if( s == (i-1) && f != -2 ) { s = i; }
      break;
    case 'e':
      if( s == (i-1) && f != -2 ) { s = i; }
      break;
    case 's':
      if( s == (i-1) && f != -2 ) { s = i; }
      break;
    case 'i':
      if( s == (i-1) && f != -2 ) { s = i; }
      break;
    case ':':
      if( s == (i-1) && f != -2 ) { s = i; return f; }
      break;
    default:
      f = s = -2;
      break;
    }
  }

  // if s and f are still valid at end of input return f
  if( f != -2 && s != -2 ) {
    return f;
  }
  else {
    return -1;
  }
}

/* accept an arbitrary length string buffer
 * when this methods exits it determines if an end state was reached
 * if no end state was reached it saves the full input into an internal buffer
 * when invoked next, it reuses that internable buffer copying all pointers into the 
 * newly allocated buffer. if it exits in a terminal state, e.g. 0 then it will dump these buffers
 */
int esi_parser_execute( ESIParser *parser, const char *data, size_t length )
{
  int cs = parser->cs;
  const char *p = data;
  const char *eof = NULL; // ragel 6.x compat
  const char *pe = data + length;
  int pindex;

  if( length == 0 || data == 0 ){ return cs; }

  /* scan data for any '<esi:' start sequences, /<$/, /<e$/, /<es$/, /<esi$/, /<esi:$/ */
  if( cs == 0 ) { 
    pindex = esi_parser_scan_for_start( parser, data, length );
    if( pindex == -1 ) {
      for( pindex = 0; pindex < (int)length; ++pindex ) {
        esi_parser_echo_char( parser, data[pindex] );
      }
      return cs;
    }
  }

  /* there's an existing overflow buffer data append the new data to the existing data */
  if( parser->overflow_data && parser->overflow_data_size > 0 ) {

    // recompute mark, tag_text, attr_key, and attr_value since they all exist within overflow_data
    int mark_offset = compute_offset( parser->mark, parser->overflow_data );
    int tag_text_offset = compute_offset( parser->tag_text, parser->overflow_data );
    int attr_key_offset = compute_offset( parser->attr_key, parser->overflow_data );
    int attr_value_offset = compute_offset( parser->attr_value, parser->overflow_data );

    parser->overflow_data = (char*)realloc( parser->overflow_data, sizeof(char)*(parser->overflow_data_size+length) );
    memcpy( parser->overflow_data+parser->overflow_data_size, data, length );

    p = parser->overflow_data + parser->overflow_data_size;
 
    // in our new memory space mark will now be
    parser->mark = ( mark_offset >= 0 ) ? parser->overflow_data + mark_offset : NULL;
    parser->tag_text = ( tag_text_offset >= 0 ) ? parser->overflow_data + tag_text_offset : NULL;
    parser->attr_key = ( attr_key_offset >= 0 ) ? parser->overflow_data + attr_key_offset : NULL;
    parser->attr_value = ( attr_value_offset >= 0 ) ? parser->overflow_data + attr_value_offset : NULL;
 
    data = parser->overflow_data;
    parser->overflow_data_size = length = length + parser->overflow_data_size;
//    printf( "grow overflow data: %ld\n", parser->overflow_data_size );
    pe = data + length;

  }

  if( !parser->mark ) {
    parser->mark = p;
  }

//  printf( "cs: %d, ", cs );

  
#line 393 "ngx_esi_parser.c"
	{
	if ( p == pe )
		goto _test_eof;
_resume:
	switch ( cs ) {
case 75:
	if ( (*p) == 60 )
		goto tr1;
	goto tr0;
case 0:
	if ( (*p) == 60 )
		goto tr1;
	goto tr0;
case 1:
	switch( (*p) ) {
		case 47: goto tr2;
		case 60: goto tr1;
		case 101: goto tr3;
	}
	goto tr0;
case 2:
	switch( (*p) ) {
		case 60: goto tr1;
		case 101: goto tr4;
	}
	goto tr0;
case 3:
	switch( (*p) ) {
		case 60: goto tr1;
		case 115: goto tr5;
	}
	goto tr0;
case 4:
	switch( (*p) ) {
		case 60: goto tr1;
		case 105: goto tr6;
	}
	goto tr0;
case 5:
	switch( (*p) ) {
		case 58: goto tr7;
		case 60: goto tr1;
	}
	goto tr0;
case 6:
	if ( (*p) == 60 )
		goto tr1;
	if ( 97 <= (*p) && (*p) <= 122 )
		goto tr8;
	goto tr0;
case 7:
	switch( (*p) ) {
		case 60: goto tr1;
		case 62: goto tr9;
	}
	if ( 97 <= (*p) && (*p) <= 122 )
		goto tr8;
	goto tr0;
case 8:
	switch( (*p) ) {
		case 60: goto tr1;
		case 115: goto tr10;
	}
	goto tr0;
case 9:
	switch( (*p) ) {
		case 60: goto tr1;
		case 105: goto tr11;
	}
	goto tr0;
case 10:
	switch( (*p) ) {
		case 58: goto tr12;
		case 60: goto tr1;
	}
	goto tr0;
case 11:
	if ( (*p) == 60 )
		goto tr1;
	if ( 97 <= (*p) && (*p) <= 122 )
		goto tr13;
	goto tr0;
case 12:
	switch( (*p) ) {
		case 32: goto tr14;
		case 60: goto tr1;
		case 62: goto tr15;
	}
	if ( (*p) > 13 ) {
		if ( 97 <= (*p) && (*p) <= 122 )
			goto tr13;
	} else if ( (*p) >= 9 )
		goto tr14;
	goto tr0;
case 13:
	switch( (*p) ) {
		case 32: goto tr16;
		case 45: goto tr17;
		case 47: goto tr18;
		case 60: goto tr1;
		case 62: goto tr19;
		case 95: goto tr17;
	}
	if ( (*p) < 65 ) {
		if ( 9 <= (*p) && (*p) <= 13 )
			goto tr16;
	} else if ( (*p) > 90 ) {
		if ( 97 <= (*p) && (*p) <= 122 )
			goto tr17;
	} else
		goto tr17;
	goto tr0;
case 14:
	switch( (*p) ) {
		case 32: goto tr16;
		case 45: goto tr17;
		case 47: goto tr18;
		case 60: goto tr1;
		case 95: goto tr17;
	}
	if ( (*p) < 65 ) {
		if ( 9 <= (*p) && (*p) <= 13 )
			goto tr16;
	} else if ( (*p) > 90 ) {
		if ( 97 <= (*p) && (*p) <= 122 )
			goto tr17;
	} else
		goto tr17;
	goto tr0;
case 15:
	switch( (*p) ) {
		case 45: goto tr17;
		case 60: goto tr1;
		case 61: goto tr20;
		case 95: goto tr17;
	}
	if ( (*p) < 65 ) {
		if ( 48 <= (*p) && (*p) <= 57 )
			goto tr17;
	} else if ( (*p) > 90 ) {
		if ( 97 <= (*p) && (*p) <= 122 )
			goto tr17;
	} else
		goto tr17;
	goto tr0;
case 16:
	switch( (*p) ) {
		case 32: goto tr21;
		case 34: goto tr22;
		case 39: goto tr23;
		case 60: goto tr1;
	}
	if ( 9 <= (*p) && (*p) <= 13 )
		goto tr21;
	goto tr0;
case 17:
	switch( (*p) ) {
		case 34: goto tr24;
		case 60: goto tr25;
		case 92: goto tr26;
	}
	goto tr22;
case 18:
	switch( (*p) ) {
		case 34: goto tr24;
		case 47: goto tr27;
		case 60: goto tr25;
		case 92: goto tr26;
		case 101: goto tr28;
	}
	goto tr22;
case 19:
	switch( (*p) ) {
		case 34: goto tr24;
		case 60: goto tr25;
		case 92: goto tr26;
		case 101: goto tr29;
	}
	goto tr22;
case 20:
	switch( (*p) ) {
		case 34: goto tr30;
		case 60: goto tr25;
		case 92: goto tr26;
	}
	goto tr22;
case 21:
	switch( (*p) ) {
		case 32: goto tr31;
		case 34: goto tr24;
		case 45: goto tr32;
		case 47: goto tr33;
		case 60: goto tr25;
		case 62: goto tr34;
		case 92: goto tr26;
		case 95: goto tr32;
	}
	if ( (*p) < 65 ) {
		if ( 9 <= (*p) && (*p) <= 13 )
			goto tr31;
	} else if ( (*p) > 90 ) {
		if ( 97 <= (*p) && (*p) <= 122 )
			goto tr32;
	} else
		goto tr32;
	goto tr22;
case 22:
	switch( (*p) ) {
		case 32: goto tr31;
		case 34: goto tr24;
		case 45: goto tr32;
		case 47: goto tr33;
		case 60: goto tr25;
		case 92: goto tr26;
		case 95: goto tr32;
	}
	if ( (*p) < 65 ) {
		if ( 9 <= (*p) && (*p) <= 13 )
			goto tr31;
	} else if ( (*p) > 90 ) {
		if ( 97 <= (*p) && (*p) <= 122 )
			goto tr32;
	} else
		goto tr32;
	goto tr22;
case 23:
	switch( (*p) ) {
		case 34: goto tr24;
		case 45: goto tr32;
		case 60: goto tr25;
		case 61: goto tr35;
		case 92: goto tr26;
		case 95: goto tr32;
	}
	if ( (*p) < 65 ) {
		if ( 48 <= (*p) && (*p) <= 57 )
			goto tr32;
	} else if ( (*p) > 90 ) {
		if ( 97 <= (*p) && (*p) <= 122 )
			goto tr32;
	} else
		goto tr32;
	goto tr22;
case 24:
	switch( (*p) ) {
		case 32: goto tr36;
		case 34: goto tr30;
		case 39: goto tr37;
		case 60: goto tr25;
		case 92: goto tr26;
	}
	if ( 9 <= (*p) && (*p) <= 13 )
		goto tr36;
	goto tr22;
case 25:
	switch( (*p) ) {
		case 34: goto tr38;
		case 39: goto tr30;
		case 60: goto tr39;
		case 92: goto tr40;
	}
	goto tr37;
case 26:
	switch( (*p) ) {
		case 32: goto tr41;
		case 39: goto tr24;
		case 45: goto tr42;
		case 47: goto tr43;
		case 60: goto tr44;
		case 62: goto tr45;
		case 92: goto tr46;
		case 95: goto tr42;
	}
	if ( (*p) < 65 ) {
		if ( 9 <= (*p) && (*p) <= 13 )
			goto tr41;
	} else if ( (*p) > 90 ) {
		if ( 97 <= (*p) && (*p) <= 122 )
			goto tr42;
	} else
		goto tr42;
	goto tr23;
case 27:
	switch( (*p) ) {
		case 39: goto tr24;
		case 60: goto tr44;
		case 92: goto tr46;
	}
	goto tr23;
case 28:
	switch( (*p) ) {
		case 39: goto tr24;
		case 47: goto tr47;
		case 60: goto tr44;
		case 92: goto tr46;
		case 101: goto tr48;
	}
	goto tr23;
case 29:
	switch( (*p) ) {
		case 39: goto tr24;
		case 60: goto tr44;
		case 92: goto tr46;
		case 101: goto tr49;
	}
	goto tr23;
case 30:
	switch( (*p) ) {
		case 39: goto tr38;
		case 60: goto tr44;
		case 92: goto tr46;
	}
	goto tr23;
case 31:
	switch( (*p) ) {
		case 39: goto tr24;
		case 60: goto tr44;
		case 92: goto tr46;
		case 115: goto tr50;
	}
	goto tr23;
case 32:
	switch( (*p) ) {
		case 39: goto tr24;
		case 60: goto tr44;
		case 92: goto tr46;
		case 105: goto tr51;
	}
	goto tr23;
case 33:
	switch( (*p) ) {
		case 39: goto tr24;
		case 58: goto tr52;
		case 60: goto tr44;
		case 92: goto tr46;
	}
	goto tr23;
case 34:
	switch( (*p) ) {
		case 39: goto tr24;
		case 60: goto tr44;
		case 92: goto tr46;
	}
	if ( 97 <= (*p) && (*p) <= 122 )
		goto tr53;
	goto tr23;
case 35:
	switch( (*p) ) {
		case 39: goto tr24;
		case 60: goto tr44;
		case 62: goto tr54;
		case 92: goto tr46;
	}
	if ( 97 <= (*p) && (*p) <= 122 )
		goto tr53;
	goto tr23;
case 36:
	switch( (*p) ) {
		case 39: goto tr24;
		case 60: goto tr44;
		case 92: goto tr46;
		case 115: goto tr55;
	}
	goto tr23;
case 37:
	switch( (*p) ) {
		case 39: goto tr24;
		case 60: goto tr44;
		case 92: goto tr46;
		case 105: goto tr56;
	}
	goto tr23;
case 38:
	switch( (*p) ) {
		case 39: goto tr24;
		case 58: goto tr57;
		case 60: goto tr44;
		case 92: goto tr46;
	}
	goto tr23;
case 39:
	switch( (*p) ) {
		case 39: goto tr24;
		case 60: goto tr44;
		case 92: goto tr46;
	}
	if ( 97 <= (*p) && (*p) <= 122 )
		goto tr58;
	goto tr23;
case 40:
	switch( (*p) ) {
		case 32: goto tr59;
		case 39: goto tr24;
		case 60: goto tr44;
		case 62: goto tr60;
		case 92: goto tr46;
	}
	if ( (*p) > 13 ) {
		if ( 97 <= (*p) && (*p) <= 122 )
			goto tr58;
	} else if ( (*p) >= 9 )
		goto tr59;
	goto tr23;
case 76:
	switch( (*p) ) {
		case 39: goto tr100;
		case 60: goto tr101;
		case 92: goto tr102;
	}
	goto tr99;
case 41:
	switch( (*p) ) {
		case 32: goto tr41;
		case 39: goto tr24;
		case 45: goto tr42;
		case 47: goto tr43;
		case 60: goto tr44;
		case 92: goto tr46;
		case 95: goto tr42;
	}
	if ( (*p) < 65 ) {
		if ( 9 <= (*p) && (*p) <= 13 )
			goto tr41;
	} else if ( (*p) > 90 ) {
		if ( 97 <= (*p) && (*p) <= 122 )
			goto tr42;
	} else
		goto tr42;
	goto tr23;
case 42:
	switch( (*p) ) {
		case 39: goto tr24;
		case 45: goto tr42;
		case 60: goto tr44;
		case 61: goto tr61;
		case 92: goto tr46;
		case 95: goto tr42;
	}
	if ( (*p) < 65 ) {
		if ( 48 <= (*p) && (*p) <= 57 )
			goto tr42;
	} else if ( (*p) > 90 ) {
		if ( 97 <= (*p) && (*p) <= 122 )
			goto tr42;
	} else
		goto tr42;
	goto tr23;
case 43:
	switch( (*p) ) {
		case 32: goto tr62;
		case 34: goto tr37;
		case 39: goto tr38;
		case 60: goto tr44;
		case 92: goto tr46;
	}
	if ( 9 <= (*p) && (*p) <= 13 )
		goto tr62;
	goto tr23;
case 44:
	switch( (*p) ) {
		case 39: goto tr24;
		case 60: goto tr44;
		case 62: goto tr63;
		case 92: goto tr46;
	}
	goto tr23;
case 45:
	switch( (*p) ) {
		case 34: goto tr38;
		case 39: goto tr30;
		case 47: goto tr64;
		case 60: goto tr39;
		case 92: goto tr40;
		case 101: goto tr65;
	}
	goto tr37;
case 46:
	switch( (*p) ) {
		case 34: goto tr38;
		case 39: goto tr30;
		case 60: goto tr39;
		case 92: goto tr40;
		case 101: goto tr66;
	}
	goto tr37;
case 47:
	switch( (*p) ) {
		case 34: goto tr67;
		case 39: goto tr67;
		case 60: goto tr39;
		case 92: goto tr40;
	}
	goto tr37;
case 48:
	switch( (*p) ) {
		case 32: goto tr68;
		case 34: goto tr38;
		case 39: goto tr30;
		case 45: goto tr69;
		case 47: goto tr70;
		case 60: goto tr39;
		case 62: goto tr71;
		case 92: goto tr40;
		case 95: goto tr69;
	}
	if ( (*p) < 65 ) {
		if ( 9 <= (*p) && (*p) <= 13 )
			goto tr68;
	} else if ( (*p) > 90 ) {
		if ( 97 <= (*p) && (*p) <= 122 )
			goto tr69;
	} else
		goto tr69;
	goto tr37;
case 49:
	switch( (*p) ) {
		case 32: goto tr68;
		case 34: goto tr38;
		case 39: goto tr30;
		case 45: goto tr69;
		case 47: goto tr70;
		case 60: goto tr39;
		case 92: goto tr40;
		case 95: goto tr69;
	}
	if ( (*p) < 65 ) {
		if ( 9 <= (*p) && (*p) <= 13 )
			goto tr68;
	} else if ( (*p) > 90 ) {
		if ( 97 <= (*p) && (*p) <= 122 )
			goto tr69;
	} else
		goto tr69;
	goto tr37;
case 50:
	switch( (*p) ) {
		case 34: goto tr38;
		case 39: goto tr30;
		case 45: goto tr69;
		case 60: goto tr39;
		case 61: goto tr72;
		case 92: goto tr40;
		case 95: goto tr69;
	}
	if ( (*p) < 65 ) {
		if ( 48 <= (*p) && (*p) <= 57 )
			goto tr69;
	} else if ( (*p) > 90 ) {
		if ( 97 <= (*p) && (*p) <= 122 )
			goto tr69;
	} else
		goto tr69;
	goto tr37;
case 51:
	switch( (*p) ) {
		case 32: goto tr73;
		case 34: goto tr67;
		case 39: goto tr67;
		case 60: goto tr39;
		case 92: goto tr40;
	}
	if ( 9 <= (*p) && (*p) <= 13 )
		goto tr73;
	goto tr37;
case 52:
	switch( (*p) ) {
		case 34: goto tr38;
		case 39: goto tr30;
		case 60: goto tr39;
		case 62: goto tr74;
		case 92: goto tr40;
	}
	goto tr37;
case 77:
	switch( (*p) ) {
		case 34: goto tr104;
		case 39: goto tr105;
		case 60: goto tr106;
		case 92: goto tr107;
	}
	goto tr103;
case 53:
	switch( (*p) ) {
		case 34: goto tr38;
		case 39: goto tr30;
		case 60: goto tr39;
		case 92: goto tr40;
		case 115: goto tr75;
	}
	goto tr37;
case 54:
	switch( (*p) ) {
		case 34: goto tr38;
		case 39: goto tr30;
		case 60: goto tr39;
		case 92: goto tr40;
		case 105: goto tr76;
	}
	goto tr37;
case 55:
	switch( (*p) ) {
		case 34: goto tr38;
		case 39: goto tr30;
		case 58: goto tr77;
		case 60: goto tr39;
		case 92: goto tr40;
	}
	goto tr37;
case 56:
	switch( (*p) ) {
		case 34: goto tr38;
		case 39: goto tr30;
		case 60: goto tr39;
		case 92: goto tr40;
	}
	if ( 97 <= (*p) && (*p) <= 122 )
		goto tr78;
	goto tr37;
case 57:
	switch( (*p) ) {
		case 34: goto tr38;
		case 39: goto tr30;
		case 60: goto tr39;
		case 62: goto tr79;
		case 92: goto tr40;
	}
	if ( 97 <= (*p) && (*p) <= 122 )
		goto tr78;
	goto tr37;
case 58:
	switch( (*p) ) {
		case 34: goto tr38;
		case 39: goto tr30;
		case 60: goto tr39;
		case 92: goto tr40;
		case 115: goto tr80;
	}
	goto tr37;
case 59:
	switch( (*p) ) {
		case 34: goto tr38;
		case 39: goto tr30;
		case 60: goto tr39;
		case 92: goto tr40;
		case 105: goto tr81;
	}
	goto tr37;
case 60:
	switch( (*p) ) {
		case 34: goto tr38;
		case 39: goto tr30;
		case 58: goto tr82;
		case 60: goto tr39;
		case 92: goto tr40;
	}
	goto tr37;
case 61:
	switch( (*p) ) {
		case 34: goto tr38;
		case 39: goto tr30;
		case 60: goto tr39;
		case 92: goto tr40;
	}
	if ( 97 <= (*p) && (*p) <= 122 )
		goto tr83;
	goto tr37;
case 62:
	switch( (*p) ) {
		case 32: goto tr84;
		case 34: goto tr38;
		case 39: goto tr30;
		case 60: goto tr39;
		case 62: goto tr85;
		case 92: goto tr40;
	}
	if ( (*p) > 13 ) {
		if ( 97 <= (*p) && (*p) <= 122 )
			goto tr83;
	} else if ( (*p) >= 9 )
		goto tr84;
	goto tr37;
case 63:
	switch( (*p) ) {
		case 34: goto tr24;
		case 60: goto tr25;
		case 62: goto tr86;
		case 92: goto tr26;
	}
	goto tr22;
case 78:
	switch( (*p) ) {
		case 34: goto tr100;
		case 60: goto tr109;
		case 92: goto tr110;
	}
	goto tr108;
case 64:
	switch( (*p) ) {
		case 34: goto tr24;
		case 60: goto tr25;
		case 92: goto tr26;
		case 115: goto tr87;
	}
	goto tr22;
case 65:
	switch( (*p) ) {
		case 34: goto tr24;
		case 60: goto tr25;
		case 92: goto tr26;
		case 105: goto tr88;
	}
	goto tr22;
case 66:
	switch( (*p) ) {
		case 34: goto tr24;
		case 58: goto tr89;
		case 60: goto tr25;
		case 92: goto tr26;
	}
	goto tr22;
case 67:
	switch( (*p) ) {
		case 34: goto tr24;
		case 60: goto tr25;
		case 92: goto tr26;
	}
	if ( 97 <= (*p) && (*p) <= 122 )
		goto tr90;
	goto tr22;
case 68:
	switch( (*p) ) {
		case 34: goto tr24;
		case 60: goto tr25;
		case 62: goto tr91;
		case 92: goto tr26;
	}
	if ( 97 <= (*p) && (*p) <= 122 )
		goto tr90;
	goto tr22;
case 69:
	switch( (*p) ) {
		case 34: goto tr24;
		case 60: goto tr25;
		case 92: goto tr26;
		case 115: goto tr92;
	}
	goto tr22;
case 70:
	switch( (*p) ) {
		case 34: goto tr24;
		case 60: goto tr25;
		case 92: goto tr26;
		case 105: goto tr93;
	}
	goto tr22;
case 71:
	switch( (*p) ) {
		case 34: goto tr24;
		case 58: goto tr94;
		case 60: goto tr25;
		case 92: goto tr26;
	}
	goto tr22;
case 72:
	switch( (*p) ) {
		case 34: goto tr24;
		case 60: goto tr25;
		case 92: goto tr26;
	}
	if ( 97 <= (*p) && (*p) <= 122 )
		goto tr95;
	goto tr22;
case 73:
	switch( (*p) ) {
		case 32: goto tr96;
		case 34: goto tr24;
		case 60: goto tr25;
		case 62: goto tr97;
		case 92: goto tr26;
	}
	if ( (*p) > 13 ) {
		if ( 97 <= (*p) && (*p) <= 122 )
			goto tr95;
	} else if ( (*p) >= 9 )
		goto tr96;
	goto tr22;
case 74:
	switch( (*p) ) {
		case 60: goto tr1;
		case 62: goto tr98;
	}
	goto tr0;
case 79:
	if ( (*p) == 60 )
		goto tr112;
	goto tr111;
	}

	tr0: cs = 0; goto f0;
	tr9: cs = 0; goto f2;
	tr111: cs = 0; goto f10;
	tr1: cs = 1; goto f1;
	tr112: cs = 1; goto f12;
	tr2: cs = 2; goto f0;
	tr4: cs = 3; goto f0;
	tr5: cs = 4; goto f0;
	tr6: cs = 5; goto f0;
	tr7: cs = 6; goto f0;
	tr8: cs = 7; goto f0;
	tr3: cs = 8; goto f0;
	tr10: cs = 9; goto f0;
	tr11: cs = 10; goto f0;
	tr12: cs = 11; goto f0;
	tr13: cs = 12; goto f0;
	tr14: cs = 13; goto f3;
	tr24: cs = 13; goto f7;
	tr100: cs = 13; goto f11;
	tr16: cs = 14; goto f0;
	tr17: cs = 15; goto f0;
	tr21: cs = 16; goto f0;
	tr20: cs = 16; goto f6;
	tr22: cs = 17; goto f0;
	tr91: cs = 17; goto f2;
	tr108: cs = 17; goto f10;
	tr25: cs = 18; goto f1;
	tr109: cs = 18; goto f12;
	tr27: cs = 19; goto f0;
	tr26: cs = 20; goto f0;
	tr110: cs = 20; goto f10;
	tr96: cs = 21; goto f3;
	tr30: cs = 21; goto f7;
	tr105: cs = 21; goto f11;
	tr31: cs = 22; goto f0;
	tr32: cs = 23; goto f0;
	tr36: cs = 24; goto f0;
	tr35: cs = 24; goto f6;
	tr37: cs = 25; goto f0;
	tr79: cs = 25; goto f2;
	tr103: cs = 25; goto f10;
	tr59: cs = 26; goto f3;
	tr38: cs = 26; goto f7;
	tr104: cs = 26; goto f11;
	tr23: cs = 27; goto f0;
	tr54: cs = 27; goto f2;
	tr99: cs = 27; goto f10;
	tr44: cs = 28; goto f1;
	tr101: cs = 28; goto f12;
	tr47: cs = 29; goto f0;
	tr46: cs = 30; goto f0;
	tr102: cs = 30; goto f10;
	tr49: cs = 31; goto f0;
	tr50: cs = 32; goto f0;
	tr51: cs = 33; goto f0;
	tr52: cs = 34; goto f0;
	tr53: cs = 35; goto f0;
	tr48: cs = 36; goto f0;
	tr55: cs = 37; goto f0;
	tr56: cs = 38; goto f0;
	tr57: cs = 39; goto f0;
	tr58: cs = 40; goto f0;
	tr41: cs = 41; goto f0;
	tr42: cs = 42; goto f0;
	tr62: cs = 43; goto f0;
	tr61: cs = 43; goto f6;
	tr43: cs = 44; goto f0;
	tr39: cs = 45; goto f1;
	tr106: cs = 45; goto f12;
	tr64: cs = 46; goto f0;
	tr40: cs = 47; goto f0;
	tr107: cs = 47; goto f10;
	tr84: cs = 48; goto f3;
	tr67: cs = 48; goto f7;
	tr68: cs = 49; goto f0;
	tr69: cs = 50; goto f0;
	tr73: cs = 51; goto f0;
	tr72: cs = 51; goto f6;
	tr70: cs = 52; goto f0;
	tr66: cs = 53; goto f0;
	tr75: cs = 54; goto f0;
	tr76: cs = 55; goto f0;
	tr77: cs = 56; goto f0;
	tr78: cs = 57; goto f0;
	tr65: cs = 58; goto f0;
	tr80: cs = 59; goto f0;
	tr81: cs = 60; goto f0;
	tr82: cs = 61; goto f0;
	tr83: cs = 62; goto f0;
	tr33: cs = 63; goto f0;
	tr29: cs = 64; goto f0;
	tr87: cs = 65; goto f0;
	tr88: cs = 66; goto f0;
	tr89: cs = 67; goto f0;
	tr90: cs = 68; goto f0;
	tr28: cs = 69; goto f0;
	tr92: cs = 70; goto f0;
	tr93: cs = 71; goto f0;
	tr94: cs = 72; goto f0;
	tr95: cs = 73; goto f0;
	tr18: cs = 74; goto f0;
	tr60: cs = 76; goto f4;
	tr45: cs = 76; goto f5;
	tr63: cs = 76; goto f8;
	tr85: cs = 77; goto f4;
	tr71: cs = 77; goto f5;
	tr74: cs = 77; goto f8;
	tr97: cs = 78; goto f4;
	tr34: cs = 78; goto f5;
	tr86: cs = 78; goto f8;
	tr15: cs = 79; goto f4;
	tr19: cs = 79; goto f5;
	tr98: cs = 79; goto f8;

f0:
#line 296 "ngx_esi_parser.rl"
	{
    //printf( "[%c:%d],", *p, cs );
    switch( cs ) {
    case 0: /* non matching state */
      if( parser->prev_state != 12 && parser->prev_state != 7 ){ /* states following a possible end state for a tag */
        if( parser->echobuffer && parser->echobuffer_index != (size_t)-1 ){
          /* send the echo buffer */
          esi_parser_echo_buffer( parser );
        }
        /* send the current character */
        esi_parser_echo_char( parser, *p );
      }
      /* clear the echo buffer */
      esi_parser_echobuffer_clear( parser );
      break;
    default:
      /* append to the echo buffer */
      esi_parser_concat_to_echobuffer( parser, *p );
    }
    /* save the previous state, necessary for end case detection such as />  and </esi:try>  the trailing > character
      is state 12 and 7
    */
    parser->prev_state = cs;
  }
	goto _again;
f1:
#line 296 "ngx_esi_parser.rl"
	{
    //printf( "[%c:%d],", *p, cs );
    switch( cs ) {
    case 0: /* non matching state */
      if( parser->prev_state != 12 && parser->prev_state != 7 ){ /* states following a possible end state for a tag */
        if( parser->echobuffer && parser->echobuffer_index != (size_t)-1 ){
          /* send the echo buffer */
          esi_parser_echo_buffer( parser );
        }
        /* send the current character */
        esi_parser_echo_char( parser, *p );
      }
      /* clear the echo buffer */
      esi_parser_echobuffer_clear( parser );
      break;
    default:
      /* append to the echo buffer */
      esi_parser_concat_to_echobuffer( parser, *p );
    }
    /* save the previous state, necessary for end case detection such as />  and </esi:try>  the trailing > character
      is state 12 and 7
    */
    parser->prev_state = cs;
  }
#line 144 "ngx_esi_parser.rl"
	{
    parser->mark = p;
    //debug_string( "begin", p, 1 );
  }
	goto _again;
f10:
#line 296 "ngx_esi_parser.rl"
	{
    //printf( "[%c:%d],", *p, cs );
    switch( cs ) {
    case 0: /* non matching state */
      if( parser->prev_state != 12 && parser->prev_state != 7 ){ /* states following a possible end state for a tag */
        if( parser->echobuffer && parser->echobuffer_index != (size_t)-1 ){
          /* send the echo buffer */
          esi_parser_echo_buffer( parser );
        }
        /* send the current character */
        esi_parser_echo_char( parser, *p );
      }
      /* clear the echo buffer */
      esi_parser_echobuffer_clear( parser );
      break;
    default:
      /* append to the echo buffer */
      esi_parser_concat_to_echobuffer( parser, *p );
    }
    /* save the previous state, necessary for end case detection such as />  and </esi:try>  the trailing > character
      is state 12 and 7
    */
    parser->prev_state = cs;
  }
#line 148 "ngx_esi_parser.rl"
	{
//    printf( "finish\n" );
  }
	goto _again;
f3:
#line 296 "ngx_esi_parser.rl"
	{
    //printf( "[%c:%d],", *p, cs );
    switch( cs ) {
    case 0: /* non matching state */
      if( parser->prev_state != 12 && parser->prev_state != 7 ){ /* states following a possible end state for a tag */
        if( parser->echobuffer && parser->echobuffer_index != (size_t)-1 ){
          /* send the echo buffer */
          esi_parser_echo_buffer( parser );
        }
        /* send the current character */
        esi_parser_echo_char( parser, *p );
      }
      /* clear the echo buffer */
      esi_parser_echobuffer_clear( parser );
      break;
    default:
      /* append to the echo buffer */
      esi_parser_concat_to_echobuffer( parser, *p );
    }
    /* save the previous state, necessary for end case detection such as />  and </esi:try>  the trailing > character
      is state 12 and 7
    */
    parser->prev_state = cs;
  }
#line 153 "ngx_esi_parser.rl"
	{
    parser->tag_text = parser->mark+1;
    parser->tag_text_length = p - (parser->mark+1);
    parser->mark = p;
  }
	goto _again;
f8:
#line 296 "ngx_esi_parser.rl"
	{
    //printf( "[%c:%d],", *p, cs );
    switch( cs ) {
    case 0: /* non matching state */
      if( parser->prev_state != 12 && parser->prev_state != 7 ){ /* states following a possible end state for a tag */
        if( parser->echobuffer && parser->echobuffer_index != (size_t)-1 ){
          /* send the echo buffer */
          esi_parser_echo_buffer( parser );
        }
        /* send the current character */
        esi_parser_echo_char( parser, *p );
      }
      /* clear the echo buffer */
      esi_parser_echobuffer_clear( parser );
      break;
    default:
      /* append to the echo buffer */
      esi_parser_concat_to_echobuffer( parser, *p );
    }
    /* save the previous state, necessary for end case detection such as />  and </esi:try>  the trailing > character
      is state 12 and 7
    */
    parser->prev_state = cs;
  }
#line 160 "ngx_esi_parser.rl"
	{
    /* trim the tag text */
    ltrim_pointer( &(parser->tag_text), p, &(parser->tag_text_length) );
    rtrim_pointer( &(parser->tag_text), p, &(parser->tag_text_length) );

    /* send the start tag and end tag message */
    esi_parser_flush_output( parser );
    parser->start_tag_handler( data, parser->tag_text, parser->tag_text_length, parser->attributes, parser->user_data );
    esi_parser_flush_output( parser );
    parser->end_tag_handler( data, parser->tag_text, parser->tag_text_length, parser->user_data );
    esi_parser_flush_output( parser );

    if( parser->attributes ) {
      esi_attribute_free( parser->attributes );
      parser->attributes = NULL;
    }

    /* mark the position */
    parser->tag_text = NULL;
    parser->tag_text_length = 0;
    parser->mark = p;

    /* clear out the echo buffer */
    esi_parser_echobuffer_clear( parser );
  }
	goto _again;
f5:
#line 296 "ngx_esi_parser.rl"
	{
    //printf( "[%c:%d],", *p, cs );
    switch( cs ) {
    case 0: /* non matching state */
      if( parser->prev_state != 12 && parser->prev_state != 7 ){ /* states following a possible end state for a tag */
        if( parser->echobuffer && parser->echobuffer_index != (size_t)-1 ){
          /* send the echo buffer */
          esi_parser_echo_buffer( parser );
        }
        /* send the current character */
        esi_parser_echo_char( parser, *p );
      }
      /* clear the echo buffer */
      esi_parser_echobuffer_clear( parser );
      break;
    default:
      /* append to the echo buffer */
      esi_parser_concat_to_echobuffer( parser, *p );
    }
    /* save the previous state, necessary for end case detection such as />  and </esi:try>  the trailing > character
      is state 12 and 7
    */
    parser->prev_state = cs;
  }
#line 187 "ngx_esi_parser.rl"
	{
    /* trim tag text */
    ltrim_pointer( &(parser->tag_text), p, &(parser->tag_text_length) );
    rtrim_pointer( &(parser->tag_text), p, &(parser->tag_text_length) );
    
    /* send the start and end tag message */
    esi_parser_flush_output( parser );
    parser->start_tag_handler( data, parser->tag_text, parser->tag_text_length, parser->attributes, parser->user_data );
    esi_parser_flush_output( parser );
    
    if( parser->attributes ) {
      esi_attribute_free( parser->attributes );
      parser->attributes = NULL;
    }

    /* mark the position */
    parser->tag_text = NULL;
    parser->tag_text_length = 0;
    parser->mark = p;
    
    /* clear out the echo buffer */
    esi_parser_echobuffer_clear( parser );
  }
	goto _again;
f6:
#line 296 "ngx_esi_parser.rl"
	{
    //printf( "[%c:%d],", *p, cs );
    switch( cs ) {
    case 0: /* non matching state */
      if( parser->prev_state != 12 && parser->prev_state != 7 ){ /* states following a possible end state for a tag */
        if( parser->echobuffer && parser->echobuffer_index != (size_t)-1 ){
          /* send the echo buffer */
          esi_parser_echo_buffer( parser );
        }
        /* send the current character */
        esi_parser_echo_char( parser, *p );
      }
      /* clear the echo buffer */
      esi_parser_echobuffer_clear( parser );
      break;
    default:
      /* append to the echo buffer */
      esi_parser_concat_to_echobuffer( parser, *p );
    }
    /* save the previous state, necessary for end case detection such as />  and </esi:try>  the trailing > character
      is state 12 and 7
    */
    parser->prev_state = cs;
  }
#line 212 "ngx_esi_parser.rl"
	{
    /* save the attribute  key start */
    parser->attr_key = parser->mark;
    /* compute the length of the key */
    parser->attr_key_length = p - parser->mark;
    /* save the position following the key */
    parser->mark = p;

    /* trim the attribute key */
    ltrim_pointer( &(parser->attr_key), p, &(parser->attr_key_length) );
    rtrim_pointer( &(parser->attr_key), p, &(parser->attr_key_length) );
  }
	goto _again;
f7:
#line 296 "ngx_esi_parser.rl"
	{
    //printf( "[%c:%d],", *p, cs );
    switch( cs ) {
    case 0: /* non matching state */
      if( parser->prev_state != 12 && parser->prev_state != 7 ){ /* states following a possible end state for a tag */
        if( parser->echobuffer && parser->echobuffer_index != (size_t)-1 ){
          /* send the echo buffer */
          esi_parser_echo_buffer( parser );
        }
        /* send the current character */
        esi_parser_echo_char( parser, *p );
      }
      /* clear the echo buffer */
      esi_parser_echobuffer_clear( parser );
      break;
    default:
      /* append to the echo buffer */
      esi_parser_concat_to_echobuffer( parser, *p );
    }
    /* save the previous state, necessary for end case detection such as />  and </esi:try>  the trailing > character
      is state 12 and 7
    */
    parser->prev_state = cs;
  }
#line 226 "ngx_esi_parser.rl"
	{
    ESIAttribute *attr;

    /* save the attribute value start */
    parser->attr_value = parser->mark;
    /* compute the length of the value */
    parser->attr_value_length = p - parser->mark;
    /* svae the position following the value */
    parser->mark = p;
    
    /* trim the attribute value */
    ltrim_pointer( &(parser->attr_value), p, &(parser->attr_value_length) );
    rtrim_pointer( &(parser->attr_value), p, &(parser->attr_value_length) );

    /* using the attr_key and attr_value, allocate a new attribute object */
    attr = esi_attribute_new( parser->attr_key, parser->attr_key_length, 
                              parser->attr_value, parser->attr_value_length );

    /* add the new attribute to the list of attributes */
    if( parser->attributes ) {
      parser->last->next = attr;
      parser->last = attr;
    }
    else {
      parser->last = parser->attributes = attr;
    }
  }
	goto _again;
f4:
#line 296 "ngx_esi_parser.rl"
	{
    //printf( "[%c:%d],", *p, cs );
    switch( cs ) {
    case 0: /* non matching state */
      if( parser->prev_state != 12 && parser->prev_state != 7 ){ /* states following a possible end state for a tag */
        if( parser->echobuffer && parser->echobuffer_index != (size_t)-1 ){
          /* send the echo buffer */
          esi_parser_echo_buffer( parser );
        }
        /* send the current character */
        esi_parser_echo_char( parser, *p );
      }
      /* clear the echo buffer */
      esi_parser_echobuffer_clear( parser );
      break;
    default:
      /* append to the echo buffer */
      esi_parser_concat_to_echobuffer( parser, *p );
    }
    /* save the previous state, necessary for end case detection such as />  and </esi:try>  the trailing > character
      is state 12 and 7
    */
    parser->prev_state = cs;
  }
#line 255 "ngx_esi_parser.rl"
	{

    parser->tag_text = parser->mark;
    parser->tag_text_length = p - parser->mark;

    parser->mark = p;

    ltrim_pointer( &(parser->tag_text), p, &(parser->tag_text_length) );
    rtrim_pointer( &(parser->tag_text), p, &(parser->tag_text_length) );

    esi_parser_flush_output( parser );
    parser->start_tag_handler( data, parser->tag_text, parser->tag_text_length, NULL, parser->user_data );
    esi_parser_flush_output( parser );
    
    if( parser->attributes ) {
      esi_attribute_free( parser->attributes );
      parser->attributes = NULL;
    }

    esi_parser_echobuffer_clear( parser );
  }
	goto _again;
f2:
#line 296 "ngx_esi_parser.rl"
	{
    //printf( "[%c:%d],", *p, cs );
    switch( cs ) {
    case 0: /* non matching state */
      if( parser->prev_state != 12 && parser->prev_state != 7 ){ /* states following a possible end state for a tag */
        if( parser->echobuffer && parser->echobuffer_index != (size_t)-1 ){
          /* send the echo buffer */
          esi_parser_echo_buffer( parser );
        }
        /* send the current character */
        esi_parser_echo_char( parser, *p );
      }
      /* clear the echo buffer */
      esi_parser_echobuffer_clear( parser );
      break;
    default:
      /* append to the echo buffer */
      esi_parser_concat_to_echobuffer( parser, *p );
    }
    /* save the previous state, necessary for end case detection such as />  and </esi:try>  the trailing > character
      is state 12 and 7
    */
    parser->prev_state = cs;
  }
#line 278 "ngx_esi_parser.rl"
	{
    /* offset by 2 to account for the </ characters */
    parser->tag_text = parser->mark+2;
    parser->tag_text_length = p - (parser->mark+2);

    parser->mark = p;
    
    ltrim_pointer( &(parser->tag_text), p, &(parser->tag_text_length) );
    rtrim_pointer( &(parser->tag_text), p, &(parser->tag_text_length) );
    
    esi_parser_flush_output( parser );
    parser->end_tag_handler( data, parser->tag_text, parser->tag_text_length, parser->user_data );
    esi_parser_flush_output( parser );

    esi_parser_echobuffer_clear( parser );
  }
	goto _again;
f12:
#line 296 "ngx_esi_parser.rl"
	{
    //printf( "[%c:%d],", *p, cs );
    switch( cs ) {
    case 0: /* non matching state */
      if( parser->prev_state != 12 && parser->prev_state != 7 ){ /* states following a possible end state for a tag */
        if( parser->echobuffer && parser->echobuffer_index != (size_t)-1 ){
          /* send the echo buffer */
          esi_parser_echo_buffer( parser );
        }
        /* send the current character */
        esi_parser_echo_char( parser, *p );
      }
      /* clear the echo buffer */
      esi_parser_echobuffer_clear( parser );
      break;
    default:
      /* append to the echo buffer */
      esi_parser_concat_to_echobuffer( parser, *p );
    }
    /* save the previous state, necessary for end case detection such as />  and </esi:try>  the trailing > character
      is state 12 and 7
    */
    parser->prev_state = cs;
  }
#line 144 "ngx_esi_parser.rl"
	{
    parser->mark = p;
    //debug_string( "begin", p, 1 );
  }
#line 148 "ngx_esi_parser.rl"
	{
//    printf( "finish\n" );
  }
	goto _again;
f11:
#line 296 "ngx_esi_parser.rl"
	{
    //printf( "[%c:%d],", *p, cs );
    switch( cs ) {
    case 0: /* non matching state */
      if( parser->prev_state != 12 && parser->prev_state != 7 ){ /* states following a possible end state for a tag */
        if( parser->echobuffer && parser->echobuffer_index != (size_t)-1 ){
          /* send the echo buffer */
          esi_parser_echo_buffer( parser );
        }
        /* send the current character */
        esi_parser_echo_char( parser, *p );
      }
      /* clear the echo buffer */
      esi_parser_echobuffer_clear( parser );
      break;
    default:
      /* append to the echo buffer */
      esi_parser_concat_to_echobuffer( parser, *p );
    }
    /* save the previous state, necessary for end case detection such as />  and </esi:try>  the trailing > character
      is state 12 and 7
    */
    parser->prev_state = cs;
  }
#line 226 "ngx_esi_parser.rl"
	{
    ESIAttribute *attr;

    /* save the attribute value start */
    parser->attr_value = parser->mark;
    /* compute the length of the value */
    parser->attr_value_length = p - parser->mark;
    /* svae the position following the value */
    parser->mark = p;
    
    /* trim the attribute value */
    ltrim_pointer( &(parser->attr_value), p, &(parser->attr_value_length) );
    rtrim_pointer( &(parser->attr_value), p, &(parser->attr_value_length) );

    /* using the attr_key and attr_value, allocate a new attribute object */
    attr = esi_attribute_new( parser->attr_key, parser->attr_key_length, 
                              parser->attr_value, parser->attr_value_length );

    /* add the new attribute to the list of attributes */
    if( parser->attributes ) {
      parser->last->next = attr;
      parser->last = attr;
    }
    else {
      parser->last = parser->attributes = attr;
    }
  }
#line 148 "ngx_esi_parser.rl"
	{
//    printf( "finish\n" );
  }
	goto _again;

_again:
	if ( ++p != pe )
		goto _resume;
	_test_eof: {}
	if ( p == eof )
	{
	switch ( _esi_eof_actions[cs] ) {
	case 10:
#line 148 "ngx_esi_parser.rl"
	{
//    printf( "finish\n" );
  }
	break;
#line 1830 "ngx_esi_parser.c"
	}
	}

	}
#line 545 "ngx_esi_parser.rl"

  parser->cs = cs;

  if( cs != esi_start && cs != 0 ) {

    /* reached the end and we're not at a termination point save the buffer as overflow */
    if( !parser->overflow_data ){
      // recompute mark, tag_text, attr_key, and attr_value since they all exist within overflow_data
      int mark_offset = compute_offset( parser->mark, data );
      int tag_text_offset = compute_offset( parser->tag_text, data );
      int attr_key_offset = compute_offset( parser->attr_key, data );
      int attr_value_offset = compute_offset( parser->attr_value, data );
      //debug_string( "mark before move", parser->mark, 1 );

      if( ESI_OUTPUT_BUFFER_SIZE > length ) {
        parser->echobuffer_allocated = ESI_OUTPUT_BUFFER_SIZE;
      }
      else {
        parser->echobuffer_allocated = length;
      }
      parser->overflow_data = (char*)malloc( sizeof( char ) * parser->echobuffer_allocated );
      memcpy( parser->overflow_data, data, length );
      parser->overflow_data_size = length;
      //printf( "allocate overflow data: %ld\n", parser->echobuffer_allocated );

      // in our new memory space mark will now be
      parser->mark = ( mark_offset >= 0 ) ? parser->overflow_data + mark_offset : NULL;
      parser->tag_text = ( tag_text_offset >= 0 ) ? parser->overflow_data + tag_text_offset : NULL;
      parser->attr_key = ( attr_key_offset >= 0 ) ? parser->overflow_data + attr_key_offset : NULL;
      parser->attr_value = ( attr_value_offset >= 0 ) ? parser->overflow_data + attr_value_offset : NULL;
      //if( parser->mark ){ debug_string( "mark after  move", parser->mark, 1 ); } else { printf( "mark is now empty\n" ); }
    }

  }else if( parser->overflow_data ) {
    /* dump the overflow buffer execution ended at a final state */
    free( parser->overflow_data );
    parser->overflow_data = NULL;
    parser->overflow_data_size = 0;
  }

  return cs;
}
int esi_parser_finish( ESIParser *parser )
{
  esi_parser_flush_output( parser );
  return 0;
}

void esi_parser_start_tag_handler( ESIParser *parser, esi_start_tag_cb callback )
{
  parser->start_tag_handler = callback;
}

void esi_parser_end_tag_handler( ESIParser *parser, esi_end_tag_cb callback )
{
  parser->end_tag_handler = callback;
}
