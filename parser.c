#line 1 "parser.rl"
/** 
 * Copyright (c) 2008 Todd A. Fisher
 * see LICENSE
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "parser.h"

#ifdef DEBUG
static void debug_string( const char *msg, const char *str, size_t len )
{
  char *pstr = esi_strndup( str, len );
  printf( "%s :'%s'\n", msg, pstr );
  free( pstr );
}
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

/* send the character to the output handler marking it 
 * as ready for consumption, e.g. not an esi tag
 */
static void esi_parser_echo_char( ESIParser *parser, char ch )
{
  parser->output_handler( (void*)&ch, 1, parser->user_data );
}
/* send any buffered characters to the output handler. 
 * This happens when we enter a case such as <em>  where the
 * first two characters < and e  match the <esi:  expression
 */
static void esi_parser_echo_buffer( ESIParser *parser )
{
//  debug_string( "echobuffer", parser->echobuffer, parser->echobuffer_index+1 );
  parser->output_handler( parser->echobuffer, parser->echobuffer_index+1, parser->user_data );
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

  if( parser->echobuffer_allocated <= parser->echobuffer_index ){
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
         **ptr == '\'' ) && (*len > 0) && (*ptr != bounds) ){
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

#line 276 "parser.rl"



#line 126 "parser.c"
static const int esi_start = 309;
static const int esi_first_final = 309;
static const int esi_error = -1;

static const int esi_en_main = 309;

#line 279 "parser.rl"

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
  parser->overflow_data_size = 0;
  parser->overflow_data = NULL;

  /* allocate 1024 bytes for the echobuffer */
  parser->echobuffer_allocated = 1024; /* NOTE: change this value, to reduce memory consumtion or allocations */
  parser->echobuffer_index = -1;
  parser->echobuffer = (char*)malloc(sizeof(char)*parser->echobuffer_allocated);

  parser->attributes = NULL;
  parser->last = NULL;

  parser->start_tag_handler = esi_parser_default_start_cb;
  parser->end_tag_handler = esi_parser_default_end_cb;
  parser->output_handler = esi_parser_default_output_cp;

  return parser;
}
void esi_parser_free( ESIParser *parser )
{
  if( parser->overflow_data ){ free( parser->overflow_data ); }

  free( parser->echobuffer );
  esi_attribute_free( parser->attributes );

  free( parser );
}

void esi_parser_output_handler( ESIParser *parser, output_cb output_handler )
{
  parser->output_handler = output_handler;
}

int esi_parser_init( ESIParser *parser )
{
  int cs;
  
#line 229 "parser.c"
	{
	cs = esi_start;
	}
#line 374 "parser.rl"
  parser->prev_state = parser->cs = cs;
  return 0;
}

static int compute_offset( const char *mark, const char *data )
{
  if( mark ){
    return mark - data;
  }
  return -1;
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
  const char *pe = data + length;

  if( length == 0 ){ return cs; }

  /* there's an existing overflow buffer data append the new data to the existing data */
  if( parser->overflow_data && parser->overflow_data_size > 0 ){

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

  if( !parser->mark ){
    parser->mark = p;
  }

//  printf( "cs: %d, ", cs );

  
#line 294 "parser.c"
	{
	if ( p == pe )
		goto _out;
_resume:
	switch ( cs ) {
case 309:
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
case 310:
	switch( (*p) ) {
		case 39: goto tr403;
		case 60: goto tr404;
		case 92: goto tr405;
	}
	goto tr402;
case 41:
	switch( (*p) ) {
		case 39: goto tr62;
		case 60: goto tr63;
		case 92: goto tr64;
	}
	goto tr61;
case 42:
	switch( (*p) ) {
		case 32: goto tr65;
		case 45: goto tr66;
		case 47: goto tr18;
		case 60: goto tr1;
		case 62: goto tr19;
		case 95: goto tr66;
	}
	if ( (*p) < 65 ) {
		if ( 9 <= (*p) && (*p) <= 13 )
			goto tr65;
	} else if ( (*p) > 90 ) {
		if ( 97 <= (*p) && (*p) <= 122 )
			goto tr66;
	} else
		goto tr66;
	goto tr0;
case 43:
	switch( (*p) ) {
		case 32: goto tr65;
		case 45: goto tr66;
		case 47: goto tr18;
		case 60: goto tr1;
		case 95: goto tr66;
	}
	if ( (*p) < 65 ) {
		if ( 9 <= (*p) && (*p) <= 13 )
			goto tr65;
	} else if ( (*p) > 90 ) {
		if ( 97 <= (*p) && (*p) <= 122 )
			goto tr66;
	} else
		goto tr66;
	goto tr0;
case 44:
	switch( (*p) ) {
		case 45: goto tr66;
		case 60: goto tr1;
		case 61: goto tr67;
		case 95: goto tr66;
	}
	if ( (*p) < 65 ) {
		if ( 48 <= (*p) && (*p) <= 57 )
			goto tr66;
	} else if ( (*p) > 90 ) {
		if ( 97 <= (*p) && (*p) <= 122 )
			goto tr66;
	} else
		goto tr66;
	goto tr0;
case 45:
	switch( (*p) ) {
		case 32: goto tr68;
		case 34: goto tr69;
		case 39: goto tr61;
		case 60: goto tr1;
	}
	if ( 9 <= (*p) && (*p) <= 13 )
		goto tr68;
	goto tr0;
case 46:
	switch( (*p) ) {
		case 34: goto tr62;
		case 60: goto tr70;
		case 92: goto tr71;
	}
	goto tr69;
case 47:
	switch( (*p) ) {
		case 34: goto tr62;
		case 47: goto tr72;
		case 60: goto tr70;
		case 92: goto tr71;
		case 101: goto tr73;
	}
	goto tr69;
case 48:
	switch( (*p) ) {
		case 34: goto tr62;
		case 60: goto tr70;
		case 92: goto tr71;
		case 101: goto tr74;
	}
	goto tr69;
case 49:
	switch( (*p) ) {
		case 34: goto tr75;
		case 60: goto tr70;
		case 92: goto tr71;
	}
	goto tr69;
case 50:
	switch( (*p) ) {
		case 32: goto tr76;
		case 34: goto tr62;
		case 45: goto tr77;
		case 47: goto tr78;
		case 60: goto tr70;
		case 62: goto tr34;
		case 92: goto tr71;
		case 95: goto tr77;
	}
	if ( (*p) < 65 ) {
		if ( 9 <= (*p) && (*p) <= 13 )
			goto tr76;
	} else if ( (*p) > 90 ) {
		if ( 97 <= (*p) && (*p) <= 122 )
			goto tr77;
	} else
		goto tr77;
	goto tr69;
case 51:
	switch( (*p) ) {
		case 32: goto tr76;
		case 34: goto tr62;
		case 45: goto tr77;
		case 47: goto tr78;
		case 60: goto tr70;
		case 92: goto tr71;
		case 95: goto tr77;
	}
	if ( (*p) < 65 ) {
		if ( 9 <= (*p) && (*p) <= 13 )
			goto tr76;
	} else if ( (*p) > 90 ) {
		if ( 97 <= (*p) && (*p) <= 122 )
			goto tr77;
	} else
		goto tr77;
	goto tr69;
case 52:
	switch( (*p) ) {
		case 34: goto tr62;
		case 45: goto tr77;
		case 60: goto tr70;
		case 61: goto tr79;
		case 92: goto tr71;
		case 95: goto tr77;
	}
	if ( (*p) < 65 ) {
		if ( 48 <= (*p) && (*p) <= 57 )
			goto tr77;
	} else if ( (*p) > 90 ) {
		if ( 97 <= (*p) && (*p) <= 122 )
			goto tr77;
	} else
		goto tr77;
	goto tr69;
case 53:
	switch( (*p) ) {
		case 32: goto tr80;
		case 34: goto tr75;
		case 39: goto tr81;
		case 60: goto tr70;
		case 92: goto tr71;
	}
	if ( 9 <= (*p) && (*p) <= 13 )
		goto tr80;
	goto tr69;
case 54:
	switch( (*p) ) {
		case 34: goto tr82;
		case 39: goto tr75;
		case 60: goto tr83;
		case 92: goto tr84;
	}
	goto tr81;
case 55:
	switch( (*p) ) {
		case 32: goto tr85;
		case 39: goto tr62;
		case 45: goto tr86;
		case 47: goto tr87;
		case 60: goto tr63;
		case 62: goto tr45;
		case 92: goto tr64;
		case 95: goto tr86;
	}
	if ( (*p) < 65 ) {
		if ( 9 <= (*p) && (*p) <= 13 )
			goto tr85;
	} else if ( (*p) > 90 ) {
		if ( 97 <= (*p) && (*p) <= 122 )
			goto tr86;
	} else
		goto tr86;
	goto tr61;
case 56:
	switch( (*p) ) {
		case 32: goto tr85;
		case 39: goto tr62;
		case 45: goto tr86;
		case 47: goto tr87;
		case 60: goto tr63;
		case 92: goto tr64;
		case 95: goto tr86;
	}
	if ( (*p) < 65 ) {
		if ( 9 <= (*p) && (*p) <= 13 )
			goto tr85;
	} else if ( (*p) > 90 ) {
		if ( 97 <= (*p) && (*p) <= 122 )
			goto tr86;
	} else
		goto tr86;
	goto tr61;
case 57:
	switch( (*p) ) {
		case 39: goto tr62;
		case 45: goto tr86;
		case 60: goto tr63;
		case 61: goto tr88;
		case 92: goto tr64;
		case 95: goto tr86;
	}
	if ( (*p) < 65 ) {
		if ( 48 <= (*p) && (*p) <= 57 )
			goto tr86;
	} else if ( (*p) > 90 ) {
		if ( 97 <= (*p) && (*p) <= 122 )
			goto tr86;
	} else
		goto tr86;
	goto tr61;
case 58:
	switch( (*p) ) {
		case 39: goto tr62;
		case 47: goto tr89;
		case 60: goto tr63;
		case 92: goto tr64;
		case 101: goto tr90;
	}
	goto tr61;
case 59:
	switch( (*p) ) {
		case 39: goto tr62;
		case 60: goto tr63;
		case 92: goto tr64;
		case 101: goto tr91;
	}
	goto tr61;
case 60:
	switch( (*p) ) {
		case 39: goto tr82;
		case 60: goto tr63;
		case 92: goto tr64;
	}
	goto tr61;
case 61:
	switch( (*p) ) {
		case 39: goto tr62;
		case 60: goto tr63;
		case 92: goto tr64;
		case 115: goto tr92;
	}
	goto tr61;
case 62:
	switch( (*p) ) {
		case 39: goto tr62;
		case 60: goto tr63;
		case 92: goto tr64;
		case 105: goto tr93;
	}
	goto tr61;
case 63:
	switch( (*p) ) {
		case 39: goto tr62;
		case 58: goto tr94;
		case 60: goto tr63;
		case 92: goto tr64;
	}
	goto tr61;
case 64:
	switch( (*p) ) {
		case 39: goto tr62;
		case 60: goto tr63;
		case 92: goto tr64;
	}
	if ( 97 <= (*p) && (*p) <= 122 )
		goto tr95;
	goto tr61;
case 65:
	switch( (*p) ) {
		case 39: goto tr62;
		case 60: goto tr63;
		case 62: goto tr96;
		case 92: goto tr64;
	}
	if ( 97 <= (*p) && (*p) <= 122 )
		goto tr95;
	goto tr61;
case 66:
	switch( (*p) ) {
		case 39: goto tr62;
		case 60: goto tr63;
		case 92: goto tr64;
		case 115: goto tr97;
	}
	goto tr61;
case 67:
	switch( (*p) ) {
		case 39: goto tr62;
		case 60: goto tr63;
		case 92: goto tr64;
		case 105: goto tr98;
	}
	goto tr61;
case 68:
	switch( (*p) ) {
		case 39: goto tr62;
		case 58: goto tr99;
		case 60: goto tr63;
		case 92: goto tr64;
	}
	goto tr61;
case 69:
	switch( (*p) ) {
		case 39: goto tr62;
		case 60: goto tr63;
		case 92: goto tr64;
	}
	if ( 97 <= (*p) && (*p) <= 122 )
		goto tr100;
	goto tr61;
case 70:
	switch( (*p) ) {
		case 32: goto tr101;
		case 39: goto tr62;
		case 60: goto tr63;
		case 62: goto tr102;
		case 92: goto tr64;
	}
	if ( (*p) > 13 ) {
		if ( 97 <= (*p) && (*p) <= 122 )
			goto tr100;
	} else if ( (*p) >= 9 )
		goto tr101;
	goto tr61;
case 71:
	switch( (*p) ) {
		case 32: goto tr103;
		case 39: goto tr62;
		case 45: goto tr104;
		case 47: goto tr105;
		case 60: goto tr63;
		case 62: goto tr106;
		case 92: goto tr64;
		case 95: goto tr104;
	}
	if ( (*p) < 65 ) {
		if ( 9 <= (*p) && (*p) <= 13 )
			goto tr103;
	} else if ( (*p) > 90 ) {
		if ( 97 <= (*p) && (*p) <= 122 )
			goto tr104;
	} else
		goto tr104;
	goto tr61;
case 72:
	switch( (*p) ) {
		case 32: goto tr103;
		case 39: goto tr62;
		case 45: goto tr104;
		case 47: goto tr105;
		case 60: goto tr63;
		case 92: goto tr64;
		case 95: goto tr104;
	}
	if ( (*p) < 65 ) {
		if ( 9 <= (*p) && (*p) <= 13 )
			goto tr103;
	} else if ( (*p) > 90 ) {
		if ( 97 <= (*p) && (*p) <= 122 )
			goto tr104;
	} else
		goto tr104;
	goto tr61;
case 73:
	switch( (*p) ) {
		case 39: goto tr62;
		case 45: goto tr104;
		case 60: goto tr63;
		case 61: goto tr107;
		case 92: goto tr64;
		case 95: goto tr104;
	}
	if ( (*p) < 65 ) {
		if ( 48 <= (*p) && (*p) <= 57 )
			goto tr104;
	} else if ( (*p) > 90 ) {
		if ( 97 <= (*p) && (*p) <= 122 )
			goto tr104;
	} else
		goto tr104;
	goto tr61;
case 74:
	switch( (*p) ) {
		case 32: goto tr108;
		case 34: goto tr109;
		case 39: goto tr110;
		case 60: goto tr63;
		case 92: goto tr64;
	}
	if ( 9 <= (*p) && (*p) <= 13 )
		goto tr108;
	goto tr61;
case 75:
	switch( (*p) ) {
		case 34: goto tr111;
		case 39: goto tr112;
		case 60: goto tr113;
		case 92: goto tr114;
	}
	goto tr109;
case 76:
	switch( (*p) ) {
		case 32: goto tr115;
		case 34: goto tr24;
		case 45: goto tr116;
		case 47: goto tr33;
		case 60: goto tr25;
		case 62: goto tr34;
		case 92: goto tr26;
		case 95: goto tr116;
	}
	if ( (*p) < 65 ) {
		if ( 9 <= (*p) && (*p) <= 13 )
			goto tr115;
	} else if ( (*p) > 90 ) {
		if ( 97 <= (*p) && (*p) <= 122 )
			goto tr116;
	} else
		goto tr116;
	goto tr22;
case 77:
	switch( (*p) ) {
		case 32: goto tr115;
		case 34: goto tr24;
		case 45: goto tr116;
		case 47: goto tr33;
		case 60: goto tr25;
		case 92: goto tr26;
		case 95: goto tr116;
	}
	if ( (*p) < 65 ) {
		if ( 9 <= (*p) && (*p) <= 13 )
			goto tr115;
	} else if ( (*p) > 90 ) {
		if ( 97 <= (*p) && (*p) <= 122 )
			goto tr116;
	} else
		goto tr116;
	goto tr22;
case 78:
	switch( (*p) ) {
		case 34: goto tr24;
		case 45: goto tr116;
		case 60: goto tr25;
		case 61: goto tr117;
		case 92: goto tr26;
		case 95: goto tr116;
	}
	if ( (*p) < 65 ) {
		if ( 48 <= (*p) && (*p) <= 57 )
			goto tr116;
	} else if ( (*p) > 90 ) {
		if ( 97 <= (*p) && (*p) <= 122 )
			goto tr116;
	} else
		goto tr116;
	goto tr22;
case 79:
	switch( (*p) ) {
		case 32: goto tr118;
		case 34: goto tr119;
		case 39: goto tr109;
		case 60: goto tr25;
		case 92: goto tr26;
	}
	if ( 9 <= (*p) && (*p) <= 13 )
		goto tr118;
	goto tr22;
case 80:
	switch( (*p) ) {
		case 32: goto tr120;
		case 34: goto tr62;
		case 45: goto tr121;
		case 47: goto tr122;
		case 60: goto tr70;
		case 62: goto tr123;
		case 92: goto tr71;
		case 95: goto tr121;
	}
	if ( (*p) < 65 ) {
		if ( 9 <= (*p) && (*p) <= 13 )
			goto tr120;
	} else if ( (*p) > 90 ) {
		if ( 97 <= (*p) && (*p) <= 122 )
			goto tr121;
	} else
		goto tr121;
	goto tr69;
case 81:
	switch( (*p) ) {
		case 32: goto tr120;
		case 34: goto tr62;
		case 45: goto tr121;
		case 47: goto tr122;
		case 60: goto tr70;
		case 92: goto tr71;
		case 95: goto tr121;
	}
	if ( (*p) < 65 ) {
		if ( 9 <= (*p) && (*p) <= 13 )
			goto tr120;
	} else if ( (*p) > 90 ) {
		if ( 97 <= (*p) && (*p) <= 122 )
			goto tr121;
	} else
		goto tr121;
	goto tr69;
case 82:
	switch( (*p) ) {
		case 34: goto tr62;
		case 45: goto tr121;
		case 60: goto tr70;
		case 61: goto tr124;
		case 92: goto tr71;
		case 95: goto tr121;
	}
	if ( (*p) < 65 ) {
		if ( 48 <= (*p) && (*p) <= 57 )
			goto tr121;
	} else if ( (*p) > 90 ) {
		if ( 97 <= (*p) && (*p) <= 122 )
			goto tr121;
	} else
		goto tr121;
	goto tr69;
case 83:
	switch( (*p) ) {
		case 32: goto tr125;
		case 34: goto tr112;
		case 39: goto tr126;
		case 60: goto tr70;
		case 92: goto tr71;
	}
	if ( 9 <= (*p) && (*p) <= 13 )
		goto tr125;
	goto tr69;
case 84:
	switch( (*p) ) {
		case 34: goto tr110;
		case 39: goto tr119;
		case 60: goto tr127;
		case 92: goto tr128;
	}
	goto tr126;
case 85:
	switch( (*p) ) {
		case 32: goto tr129;
		case 39: goto tr24;
		case 45: goto tr130;
		case 47: goto tr43;
		case 60: goto tr44;
		case 62: goto tr45;
		case 92: goto tr46;
		case 95: goto tr130;
	}
	if ( (*p) < 65 ) {
		if ( 9 <= (*p) && (*p) <= 13 )
			goto tr129;
	} else if ( (*p) > 90 ) {
		if ( 97 <= (*p) && (*p) <= 122 )
			goto tr130;
	} else
		goto tr130;
	goto tr23;
case 86:
	switch( (*p) ) {
		case 32: goto tr129;
		case 39: goto tr24;
		case 45: goto tr130;
		case 47: goto tr43;
		case 60: goto tr44;
		case 92: goto tr46;
		case 95: goto tr130;
	}
	if ( (*p) < 65 ) {
		if ( 9 <= (*p) && (*p) <= 13 )
			goto tr129;
	} else if ( (*p) > 90 ) {
		if ( 97 <= (*p) && (*p) <= 122 )
			goto tr130;
	} else
		goto tr130;
	goto tr23;
case 87:
	switch( (*p) ) {
		case 39: goto tr24;
		case 45: goto tr130;
		case 60: goto tr44;
		case 61: goto tr131;
		case 92: goto tr46;
		case 95: goto tr130;
	}
	if ( (*p) < 65 ) {
		if ( 48 <= (*p) && (*p) <= 57 )
			goto tr130;
	} else if ( (*p) > 90 ) {
		if ( 97 <= (*p) && (*p) <= 122 )
			goto tr130;
	} else
		goto tr130;
	goto tr23;
case 88:
	switch( (*p) ) {
		case 32: goto tr132;
		case 34: goto tr126;
		case 39: goto tr111;
		case 60: goto tr44;
		case 92: goto tr46;
	}
	if ( 9 <= (*p) && (*p) <= 13 )
		goto tr132;
	goto tr23;
case 89:
	switch( (*p) ) {
		case 39: goto tr24;
		case 60: goto tr44;
		case 62: goto tr133;
		case 92: goto tr46;
	}
	goto tr23;
case 90:
	switch( (*p) ) {
		case 34: goto tr110;
		case 39: goto tr119;
		case 47: goto tr134;
		case 60: goto tr127;
		case 92: goto tr128;
		case 101: goto tr135;
	}
	goto tr126;
case 91:
	switch( (*p) ) {
		case 34: goto tr110;
		case 39: goto tr119;
		case 60: goto tr127;
		case 92: goto tr128;
		case 101: goto tr136;
	}
	goto tr126;
case 92:
	switch( (*p) ) {
		case 34: goto tr137;
		case 39: goto tr138;
		case 60: goto tr127;
		case 92: goto tr128;
	}
	goto tr126;
case 93:
	switch( (*p) ) {
		case 32: goto tr139;
		case 34: goto tr110;
		case 39: goto tr119;
		case 45: goto tr140;
		case 47: goto tr141;
		case 60: goto tr127;
		case 62: goto tr142;
		case 92: goto tr128;
		case 95: goto tr140;
	}
	if ( (*p) < 65 ) {
		if ( 9 <= (*p) && (*p) <= 13 )
			goto tr139;
	} else if ( (*p) > 90 ) {
		if ( 97 <= (*p) && (*p) <= 122 )
			goto tr140;
	} else
		goto tr140;
	goto tr126;
case 94:
	switch( (*p) ) {
		case 32: goto tr139;
		case 34: goto tr110;
		case 39: goto tr119;
		case 45: goto tr140;
		case 47: goto tr141;
		case 60: goto tr127;
		case 92: goto tr128;
		case 95: goto tr140;
	}
	if ( (*p) < 65 ) {
		if ( 9 <= (*p) && (*p) <= 13 )
			goto tr139;
	} else if ( (*p) > 90 ) {
		if ( 97 <= (*p) && (*p) <= 122 )
			goto tr140;
	} else
		goto tr140;
	goto tr126;
case 95:
	switch( (*p) ) {
		case 34: goto tr110;
		case 39: goto tr119;
		case 45: goto tr140;
		case 60: goto tr127;
		case 61: goto tr143;
		case 92: goto tr128;
		case 95: goto tr140;
	}
	if ( (*p) < 65 ) {
		if ( 48 <= (*p) && (*p) <= 57 )
			goto tr140;
	} else if ( (*p) > 90 ) {
		if ( 97 <= (*p) && (*p) <= 122 )
			goto tr140;
	} else
		goto tr140;
	goto tr126;
case 96:
	switch( (*p) ) {
		case 32: goto tr144;
		case 34: goto tr137;
		case 39: goto tr145;
		case 60: goto tr127;
		case 92: goto tr128;
	}
	if ( 9 <= (*p) && (*p) <= 13 )
		goto tr144;
	goto tr126;
case 97:
	switch( (*p) ) {
		case 32: goto tr146;
		case 34: goto tr82;
		case 39: goto tr75;
		case 45: goto tr147;
		case 47: goto tr148;
		case 60: goto tr83;
		case 62: goto tr149;
		case 92: goto tr84;
		case 95: goto tr147;
	}
	if ( (*p) < 65 ) {
		if ( 9 <= (*p) && (*p) <= 13 )
			goto tr146;
	} else if ( (*p) > 90 ) {
		if ( 97 <= (*p) && (*p) <= 122 )
			goto tr147;
	} else
		goto tr147;
	goto tr81;
case 98:
	switch( (*p) ) {
		case 32: goto tr146;
		case 34: goto tr82;
		case 39: goto tr75;
		case 45: goto tr147;
		case 47: goto tr148;
		case 60: goto tr83;
		case 92: goto tr84;
		case 95: goto tr147;
	}
	if ( (*p) < 65 ) {
		if ( 9 <= (*p) && (*p) <= 13 )
			goto tr146;
	} else if ( (*p) > 90 ) {
		if ( 97 <= (*p) && (*p) <= 122 )
			goto tr147;
	} else
		goto tr147;
	goto tr81;
case 99:
	switch( (*p) ) {
		case 34: goto tr82;
		case 39: goto tr75;
		case 45: goto tr147;
		case 60: goto tr83;
		case 61: goto tr150;
		case 92: goto tr84;
		case 95: goto tr147;
	}
	if ( (*p) < 65 ) {
		if ( 48 <= (*p) && (*p) <= 57 )
			goto tr147;
	} else if ( (*p) > 90 ) {
		if ( 97 <= (*p) && (*p) <= 122 )
			goto tr147;
	} else
		goto tr147;
	goto tr81;
case 100:
	switch( (*p) ) {
		case 34: goto tr82;
		case 39: goto tr75;
		case 47: goto tr151;
		case 60: goto tr83;
		case 92: goto tr84;
		case 101: goto tr152;
	}
	goto tr81;
case 101:
	switch( (*p) ) {
		case 34: goto tr82;
		case 39: goto tr75;
		case 60: goto tr83;
		case 92: goto tr84;
		case 101: goto tr153;
	}
	goto tr81;
case 102:
	switch( (*p) ) {
		case 34: goto tr154;
		case 39: goto tr154;
		case 60: goto tr83;
		case 92: goto tr84;
	}
	goto tr81;
case 103:
	switch( (*p) ) {
		case 32: goto tr155;
		case 34: goto tr82;
		case 39: goto tr75;
		case 45: goto tr156;
		case 47: goto tr157;
		case 60: goto tr83;
		case 62: goto tr158;
		case 92: goto tr84;
		case 95: goto tr156;
	}
	if ( (*p) < 65 ) {
		if ( 9 <= (*p) && (*p) <= 13 )
			goto tr155;
	} else if ( (*p) > 90 ) {
		if ( 97 <= (*p) && (*p) <= 122 )
			goto tr156;
	} else
		goto tr156;
	goto tr81;
case 104:
	switch( (*p) ) {
		case 32: goto tr155;
		case 34: goto tr82;
		case 39: goto tr75;
		case 45: goto tr156;
		case 47: goto tr157;
		case 60: goto tr83;
		case 92: goto tr84;
		case 95: goto tr156;
	}
	if ( (*p) < 65 ) {
		if ( 9 <= (*p) && (*p) <= 13 )
			goto tr155;
	} else if ( (*p) > 90 ) {
		if ( 97 <= (*p) && (*p) <= 122 )
			goto tr156;
	} else
		goto tr156;
	goto tr81;
case 105:
	switch( (*p) ) {
		case 34: goto tr82;
		case 39: goto tr75;
		case 45: goto tr156;
		case 60: goto tr83;
		case 61: goto tr159;
		case 92: goto tr84;
		case 95: goto tr156;
	}
	if ( (*p) < 65 ) {
		if ( 48 <= (*p) && (*p) <= 57 )
			goto tr156;
	} else if ( (*p) > 90 ) {
		if ( 97 <= (*p) && (*p) <= 122 )
			goto tr156;
	} else
		goto tr156;
	goto tr81;
case 106:
	switch( (*p) ) {
		case 32: goto tr160;
		case 34: goto tr154;
		case 39: goto tr154;
		case 60: goto tr83;
		case 92: goto tr84;
	}
	if ( 9 <= (*p) && (*p) <= 13 )
		goto tr160;
	goto tr81;
case 107:
	switch( (*p) ) {
		case 34: goto tr82;
		case 39: goto tr75;
		case 60: goto tr83;
		case 62: goto tr161;
		case 92: goto tr84;
	}
	goto tr81;
case 311:
	switch( (*p) ) {
		case 34: goto tr407;
		case 39: goto tr408;
		case 60: goto tr409;
		case 92: goto tr410;
	}
	goto tr406;
case 108:
	switch( (*p) ) {
		case 34: goto tr82;
		case 39: goto tr75;
		case 60: goto tr83;
		case 92: goto tr84;
		case 115: goto tr162;
	}
	goto tr81;
case 109:
	switch( (*p) ) {
		case 34: goto tr82;
		case 39: goto tr75;
		case 60: goto tr83;
		case 92: goto tr84;
		case 105: goto tr163;
	}
	goto tr81;
case 110:
	switch( (*p) ) {
		case 34: goto tr82;
		case 39: goto tr75;
		case 58: goto tr164;
		case 60: goto tr83;
		case 92: goto tr84;
	}
	goto tr81;
case 111:
	switch( (*p) ) {
		case 34: goto tr82;
		case 39: goto tr75;
		case 60: goto tr83;
		case 92: goto tr84;
	}
	if ( 97 <= (*p) && (*p) <= 122 )
		goto tr165;
	goto tr81;
case 112:
	switch( (*p) ) {
		case 34: goto tr82;
		case 39: goto tr75;
		case 60: goto tr83;
		case 62: goto tr166;
		case 92: goto tr84;
	}
	if ( 97 <= (*p) && (*p) <= 122 )
		goto tr165;
	goto tr81;
case 113:
	switch( (*p) ) {
		case 34: goto tr82;
		case 39: goto tr75;
		case 60: goto tr83;
		case 92: goto tr84;
		case 115: goto tr167;
	}
	goto tr81;
case 114:
	switch( (*p) ) {
		case 34: goto tr82;
		case 39: goto tr75;
		case 60: goto tr83;
		case 92: goto tr84;
		case 105: goto tr168;
	}
	goto tr81;
case 115:
	switch( (*p) ) {
		case 34: goto tr82;
		case 39: goto tr75;
		case 58: goto tr169;
		case 60: goto tr83;
		case 92: goto tr84;
	}
	goto tr81;
case 116:
	switch( (*p) ) {
		case 34: goto tr82;
		case 39: goto tr75;
		case 60: goto tr83;
		case 92: goto tr84;
	}
	if ( 97 <= (*p) && (*p) <= 122 )
		goto tr170;
	goto tr81;
case 117:
	switch( (*p) ) {
		case 32: goto tr171;
		case 34: goto tr82;
		case 39: goto tr75;
		case 60: goto tr83;
		case 62: goto tr172;
		case 92: goto tr84;
	}
	if ( (*p) > 13 ) {
		if ( 97 <= (*p) && (*p) <= 122 )
			goto tr170;
	} else if ( (*p) >= 9 )
		goto tr171;
	goto tr81;
case 312:
	switch( (*p) ) {
		case 34: goto tr407;
		case 39: goto tr408;
		case 60: goto tr409;
		case 92: goto tr410;
	}
	goto tr406;
case 118:
	switch( (*p) ) {
		case 32: goto tr173;
		case 34: goto tr174;
		case 39: goto tr137;
		case 60: goto tr83;
		case 92: goto tr84;
	}
	if ( 9 <= (*p) && (*p) <= 13 )
		goto tr173;
	goto tr81;
case 119:
	switch( (*p) ) {
		case 32: goto tr175;
		case 34: goto tr111;
		case 39: goto tr112;
		case 45: goto tr176;
		case 47: goto tr177;
		case 60: goto tr113;
		case 62: goto tr178;
		case 92: goto tr114;
		case 95: goto tr176;
	}
	if ( (*p) < 65 ) {
		if ( 9 <= (*p) && (*p) <= 13 )
			goto tr175;
	} else if ( (*p) > 90 ) {
		if ( 97 <= (*p) && (*p) <= 122 )
			goto tr176;
	} else
		goto tr176;
	goto tr109;
case 120:
	switch( (*p) ) {
		case 32: goto tr175;
		case 34: goto tr111;
		case 39: goto tr112;
		case 45: goto tr176;
		case 47: goto tr177;
		case 60: goto tr113;
		case 92: goto tr114;
		case 95: goto tr176;
	}
	if ( (*p) < 65 ) {
		if ( 9 <= (*p) && (*p) <= 13 )
			goto tr175;
	} else if ( (*p) > 90 ) {
		if ( 97 <= (*p) && (*p) <= 122 )
			goto tr176;
	} else
		goto tr176;
	goto tr109;
case 121:
	switch( (*p) ) {
		case 34: goto tr111;
		case 39: goto tr112;
		case 45: goto tr176;
		case 60: goto tr113;
		case 61: goto tr179;
		case 92: goto tr114;
		case 95: goto tr176;
	}
	if ( (*p) < 65 ) {
		if ( 48 <= (*p) && (*p) <= 57 )
			goto tr176;
	} else if ( (*p) > 90 ) {
		if ( 97 <= (*p) && (*p) <= 122 )
			goto tr176;
	} else
		goto tr176;
	goto tr109;
case 122:
	switch( (*p) ) {
		case 34: goto tr111;
		case 39: goto tr112;
		case 47: goto tr180;
		case 60: goto tr113;
		case 92: goto tr114;
		case 101: goto tr181;
	}
	goto tr109;
case 123:
	switch( (*p) ) {
		case 34: goto tr111;
		case 39: goto tr112;
		case 60: goto tr113;
		case 92: goto tr114;
		case 101: goto tr182;
	}
	goto tr109;
case 124:
	switch( (*p) ) {
		case 34: goto tr183;
		case 39: goto tr174;
		case 60: goto tr113;
		case 92: goto tr114;
	}
	goto tr109;
case 125:
	switch( (*p) ) {
		case 32: goto tr184;
		case 34: goto tr111;
		case 39: goto tr112;
		case 45: goto tr185;
		case 47: goto tr186;
		case 60: goto tr113;
		case 62: goto tr187;
		case 92: goto tr114;
		case 95: goto tr185;
	}
	if ( (*p) < 65 ) {
		if ( 9 <= (*p) && (*p) <= 13 )
			goto tr184;
	} else if ( (*p) > 90 ) {
		if ( 97 <= (*p) && (*p) <= 122 )
			goto tr185;
	} else
		goto tr185;
	goto tr109;
case 126:
	switch( (*p) ) {
		case 32: goto tr184;
		case 34: goto tr111;
		case 39: goto tr112;
		case 45: goto tr185;
		case 47: goto tr186;
		case 60: goto tr113;
		case 92: goto tr114;
		case 95: goto tr185;
	}
	if ( (*p) < 65 ) {
		if ( 9 <= (*p) && (*p) <= 13 )
			goto tr184;
	} else if ( (*p) > 90 ) {
		if ( 97 <= (*p) && (*p) <= 122 )
			goto tr185;
	} else
		goto tr185;
	goto tr109;
case 127:
	switch( (*p) ) {
		case 34: goto tr111;
		case 39: goto tr112;
		case 45: goto tr185;
		case 60: goto tr113;
		case 61: goto tr188;
		case 92: goto tr114;
		case 95: goto tr185;
	}
	if ( (*p) < 65 ) {
		if ( 48 <= (*p) && (*p) <= 57 )
			goto tr185;
	} else if ( (*p) > 90 ) {
		if ( 97 <= (*p) && (*p) <= 122 )
			goto tr185;
	} else
		goto tr185;
	goto tr109;
case 128:
	switch( (*p) ) {
		case 32: goto tr189;
		case 34: goto tr183;
		case 39: goto tr190;
		case 60: goto tr113;
		case 92: goto tr114;
	}
	if ( 9 <= (*p) && (*p) <= 13 )
		goto tr189;
	goto tr109;
case 129:
	switch( (*p) ) {
		case 32: goto tr191;
		case 34: goto tr38;
		case 39: goto tr30;
		case 45: goto tr192;
		case 47: goto tr193;
		case 60: goto tr39;
		case 62: goto tr158;
		case 92: goto tr40;
		case 95: goto tr192;
	}
	if ( (*p) < 65 ) {
		if ( 9 <= (*p) && (*p) <= 13 )
			goto tr191;
	} else if ( (*p) > 90 ) {
		if ( 97 <= (*p) && (*p) <= 122 )
			goto tr192;
	} else
		goto tr192;
	goto tr37;
case 130:
	switch( (*p) ) {
		case 32: goto tr191;
		case 34: goto tr38;
		case 39: goto tr30;
		case 45: goto tr192;
		case 47: goto tr193;
		case 60: goto tr39;
		case 92: goto tr40;
		case 95: goto tr192;
	}
	if ( (*p) < 65 ) {
		if ( 9 <= (*p) && (*p) <= 13 )
			goto tr191;
	} else if ( (*p) > 90 ) {
		if ( 97 <= (*p) && (*p) <= 122 )
			goto tr192;
	} else
		goto tr192;
	goto tr37;
case 131:
	switch( (*p) ) {
		case 34: goto tr38;
		case 39: goto tr30;
		case 45: goto tr192;
		case 60: goto tr39;
		case 61: goto tr194;
		case 92: goto tr40;
		case 95: goto tr192;
	}
	if ( (*p) < 65 ) {
		if ( 48 <= (*p) && (*p) <= 57 )
			goto tr192;
	} else if ( (*p) > 90 ) {
		if ( 97 <= (*p) && (*p) <= 122 )
			goto tr192;
	} else
		goto tr192;
	goto tr37;
case 132:
	switch( (*p) ) {
		case 34: goto tr38;
		case 39: goto tr30;
		case 47: goto tr195;
		case 60: goto tr39;
		case 92: goto tr40;
		case 101: goto tr196;
	}
	goto tr37;
case 133:
	switch( (*p) ) {
		case 34: goto tr38;
		case 39: goto tr30;
		case 60: goto tr39;
		case 92: goto tr40;
		case 101: goto tr197;
	}
	goto tr37;
case 134:
	switch( (*p) ) {
		case 34: goto tr198;
		case 39: goto tr198;
		case 60: goto tr39;
		case 92: goto tr40;
	}
	goto tr37;
case 135:
	switch( (*p) ) {
		case 32: goto tr199;
		case 34: goto tr38;
		case 39: goto tr30;
		case 45: goto tr200;
		case 47: goto tr193;
		case 60: goto tr39;
		case 62: goto tr158;
		case 92: goto tr40;
		case 95: goto tr200;
	}
	if ( (*p) < 65 ) {
		if ( 9 <= (*p) && (*p) <= 13 )
			goto tr199;
	} else if ( (*p) > 90 ) {
		if ( 97 <= (*p) && (*p) <= 122 )
			goto tr200;
	} else
		goto tr200;
	goto tr37;
case 136:
	switch( (*p) ) {
		case 32: goto tr199;
		case 34: goto tr38;
		case 39: goto tr30;
		case 45: goto tr200;
		case 47: goto tr193;
		case 60: goto tr39;
		case 92: goto tr40;
		case 95: goto tr200;
	}
	if ( (*p) < 65 ) {
		if ( 9 <= (*p) && (*p) <= 13 )
			goto tr199;
	} else if ( (*p) > 90 ) {
		if ( 97 <= (*p) && (*p) <= 122 )
			goto tr200;
	} else
		goto tr200;
	goto tr37;
case 137:
	switch( (*p) ) {
		case 34: goto tr38;
		case 39: goto tr30;
		case 45: goto tr200;
		case 60: goto tr39;
		case 61: goto tr201;
		case 92: goto tr40;
		case 95: goto tr200;
	}
	if ( (*p) < 65 ) {
		if ( 48 <= (*p) && (*p) <= 57 )
			goto tr200;
	} else if ( (*p) > 90 ) {
		if ( 97 <= (*p) && (*p) <= 122 )
			goto tr200;
	} else
		goto tr200;
	goto tr37;
case 138:
	switch( (*p) ) {
		case 32: goto tr202;
		case 34: goto tr198;
		case 39: goto tr198;
		case 60: goto tr39;
		case 92: goto tr40;
	}
	if ( 9 <= (*p) && (*p) <= 13 )
		goto tr202;
	goto tr37;
case 139:
	switch( (*p) ) {
		case 34: goto tr38;
		case 39: goto tr30;
		case 60: goto tr39;
		case 62: goto tr161;
		case 92: goto tr40;
	}
	goto tr37;
case 140:
	switch( (*p) ) {
		case 34: goto tr38;
		case 39: goto tr30;
		case 60: goto tr39;
		case 92: goto tr40;
		case 115: goto tr203;
	}
	goto tr37;
case 141:
	switch( (*p) ) {
		case 34: goto tr38;
		case 39: goto tr30;
		case 60: goto tr39;
		case 92: goto tr40;
		case 105: goto tr204;
	}
	goto tr37;
case 142:
	switch( (*p) ) {
		case 34: goto tr38;
		case 39: goto tr30;
		case 58: goto tr205;
		case 60: goto tr39;
		case 92: goto tr40;
	}
	goto tr37;
case 143:
	switch( (*p) ) {
		case 34: goto tr38;
		case 39: goto tr30;
		case 60: goto tr39;
		case 92: goto tr40;
	}
	if ( 97 <= (*p) && (*p) <= 122 )
		goto tr206;
	goto tr37;
case 144:
	switch( (*p) ) {
		case 34: goto tr38;
		case 39: goto tr30;
		case 60: goto tr39;
		case 62: goto tr207;
		case 92: goto tr40;
	}
	if ( 97 <= (*p) && (*p) <= 122 )
		goto tr206;
	goto tr37;
case 145:
	switch( (*p) ) {
		case 34: goto tr38;
		case 39: goto tr30;
		case 60: goto tr39;
		case 92: goto tr40;
		case 115: goto tr208;
	}
	goto tr37;
case 146:
	switch( (*p) ) {
		case 34: goto tr38;
		case 39: goto tr30;
		case 60: goto tr39;
		case 92: goto tr40;
		case 105: goto tr209;
	}
	goto tr37;
case 147:
	switch( (*p) ) {
		case 34: goto tr38;
		case 39: goto tr30;
		case 58: goto tr210;
		case 60: goto tr39;
		case 92: goto tr40;
	}
	goto tr37;
case 148:
	switch( (*p) ) {
		case 34: goto tr38;
		case 39: goto tr30;
		case 60: goto tr39;
		case 92: goto tr40;
	}
	if ( 97 <= (*p) && (*p) <= 122 )
		goto tr211;
	goto tr37;
case 149:
	switch( (*p) ) {
		case 32: goto tr212;
		case 34: goto tr38;
		case 39: goto tr30;
		case 60: goto tr39;
		case 62: goto tr213;
		case 92: goto tr40;
	}
	if ( (*p) > 13 ) {
		if ( 97 <= (*p) && (*p) <= 122 )
			goto tr211;
	} else if ( (*p) >= 9 )
		goto tr212;
	goto tr37;
case 150:
	switch( (*p) ) {
		case 32: goto tr214;
		case 34: goto tr138;
		case 39: goto tr183;
		case 60: goto tr39;
		case 92: goto tr40;
	}
	if ( 9 <= (*p) && (*p) <= 13 )
		goto tr214;
	goto tr37;
case 151:
	switch( (*p) ) {
		case 32: goto tr215;
		case 34: goto tr110;
		case 39: goto tr119;
		case 45: goto tr216;
		case 47: goto tr217;
		case 60: goto tr127;
		case 62: goto tr218;
		case 92: goto tr128;
		case 95: goto tr216;
	}
	if ( (*p) < 65 ) {
		if ( 9 <= (*p) && (*p) <= 13 )
			goto tr215;
	} else if ( (*p) > 90 ) {
		if ( 97 <= (*p) && (*p) <= 122 )
			goto tr216;
	} else
		goto tr216;
	goto tr126;
case 152:
	switch( (*p) ) {
		case 32: goto tr215;
		case 34: goto tr110;
		case 39: goto tr119;
		case 45: goto tr216;
		case 47: goto tr217;
		case 60: goto tr127;
		case 92: goto tr128;
		case 95: goto tr216;
	}
	if ( (*p) < 65 ) {
		if ( 9 <= (*p) && (*p) <= 13 )
			goto tr215;
	} else if ( (*p) > 90 ) {
		if ( 97 <= (*p) && (*p) <= 122 )
			goto tr216;
	} else
		goto tr216;
	goto tr126;
case 153:
	switch( (*p) ) {
		case 34: goto tr110;
		case 39: goto tr119;
		case 45: goto tr216;
		case 60: goto tr127;
		case 61: goto tr219;
		case 92: goto tr128;
		case 95: goto tr216;
	}
	if ( (*p) < 65 ) {
		if ( 48 <= (*p) && (*p) <= 57 )
			goto tr216;
	} else if ( (*p) > 90 ) {
		if ( 97 <= (*p) && (*p) <= 122 )
			goto tr216;
	} else
		goto tr216;
	goto tr126;
case 154:
	switch( (*p) ) {
		case 32: goto tr220;
		case 34: goto tr190;
		case 39: goto tr138;
		case 60: goto tr127;
		case 92: goto tr128;
	}
	if ( 9 <= (*p) && (*p) <= 13 )
		goto tr220;
	goto tr126;
case 155:
	switch( (*p) ) {
		case 34: goto tr110;
		case 39: goto tr119;
		case 60: goto tr127;
		case 62: goto tr221;
		case 92: goto tr128;
	}
	goto tr126;
case 313:
	switch( (*p) ) {
		case 34: goto tr412;
		case 39: goto tr413;
		case 60: goto tr414;
		case 92: goto tr415;
	}
	goto tr411;
case 156:
	switch( (*p) ) {
		case 34: goto tr223;
		case 39: goto tr224;
		case 60: goto tr225;
		case 92: goto tr226;
	}
	goto tr222;
case 157:
	switch( (*p) ) {
		case 32: goto tr227;
		case 39: goto tr62;
		case 45: goto tr228;
		case 47: goto tr87;
		case 60: goto tr63;
		case 62: goto tr45;
		case 92: goto tr64;
		case 95: goto tr228;
	}
	if ( (*p) < 65 ) {
		if ( 9 <= (*p) && (*p) <= 13 )
			goto tr227;
	} else if ( (*p) > 90 ) {
		if ( 97 <= (*p) && (*p) <= 122 )
			goto tr228;
	} else
		goto tr228;
	goto tr61;
case 158:
	switch( (*p) ) {
		case 32: goto tr227;
		case 39: goto tr62;
		case 45: goto tr228;
		case 47: goto tr87;
		case 60: goto tr63;
		case 92: goto tr64;
		case 95: goto tr228;
	}
	if ( (*p) < 65 ) {
		if ( 9 <= (*p) && (*p) <= 13 )
			goto tr227;
	} else if ( (*p) > 90 ) {
		if ( 97 <= (*p) && (*p) <= 122 )
			goto tr228;
	} else
		goto tr228;
	goto tr61;
case 159:
	switch( (*p) ) {
		case 39: goto tr62;
		case 45: goto tr228;
		case 60: goto tr63;
		case 61: goto tr229;
		case 92: goto tr64;
		case 95: goto tr228;
	}
	if ( (*p) < 65 ) {
		if ( 48 <= (*p) && (*p) <= 57 )
			goto tr228;
	} else if ( (*p) > 90 ) {
		if ( 97 <= (*p) && (*p) <= 122 )
			goto tr228;
	} else
		goto tr228;
	goto tr61;
case 160:
	switch( (*p) ) {
		case 32: goto tr230;
		case 34: goto tr222;
		case 39: goto tr231;
		case 60: goto tr63;
		case 92: goto tr64;
	}
	if ( 9 <= (*p) && (*p) <= 13 )
		goto tr230;
	goto tr61;
case 161:
	switch( (*p) ) {
		case 32: goto tr232;
		case 39: goto tr62;
		case 45: goto tr233;
		case 47: goto tr105;
		case 60: goto tr63;
		case 62: goto tr106;
		case 92: goto tr64;
		case 95: goto tr233;
	}
	if ( (*p) < 65 ) {
		if ( 9 <= (*p) && (*p) <= 13 )
			goto tr232;
	} else if ( (*p) > 90 ) {
		if ( 97 <= (*p) && (*p) <= 122 )
			goto tr233;
	} else
		goto tr233;
	goto tr61;
case 162:
	switch( (*p) ) {
		case 32: goto tr232;
		case 39: goto tr62;
		case 45: goto tr233;
		case 47: goto tr105;
		case 60: goto tr63;
		case 92: goto tr64;
		case 95: goto tr233;
	}
	if ( (*p) < 65 ) {
		if ( 9 <= (*p) && (*p) <= 13 )
			goto tr232;
	} else if ( (*p) > 90 ) {
		if ( 97 <= (*p) && (*p) <= 122 )
			goto tr233;
	} else
		goto tr233;
	goto tr61;
case 163:
	switch( (*p) ) {
		case 39: goto tr62;
		case 45: goto tr233;
		case 60: goto tr63;
		case 61: goto tr234;
		case 92: goto tr64;
		case 95: goto tr233;
	}
	if ( (*p) < 65 ) {
		if ( 48 <= (*p) && (*p) <= 57 )
			goto tr233;
	} else if ( (*p) > 90 ) {
		if ( 97 <= (*p) && (*p) <= 122 )
			goto tr233;
	} else
		goto tr233;
	goto tr61;
case 164:
	switch( (*p) ) {
		case 32: goto tr235;
		case 34: goto tr236;
		case 39: goto tr223;
		case 60: goto tr63;
		case 92: goto tr64;
	}
	if ( 9 <= (*p) && (*p) <= 13 )
		goto tr235;
	goto tr61;
case 165:
	switch( (*p) ) {
		case 34: goto tr231;
		case 39: goto tr237;
		case 60: goto tr238;
		case 92: goto tr239;
	}
	goto tr236;
case 166:
	switch( (*p) ) {
		case 32: goto tr240;
		case 34: goto tr62;
		case 45: goto tr241;
		case 47: goto tr78;
		case 60: goto tr70;
		case 62: goto tr34;
		case 92: goto tr71;
		case 95: goto tr241;
	}
	if ( (*p) < 65 ) {
		if ( 9 <= (*p) && (*p) <= 13 )
			goto tr240;
	} else if ( (*p) > 90 ) {
		if ( 97 <= (*p) && (*p) <= 122 )
			goto tr241;
	} else
		goto tr241;
	goto tr69;
case 167:
	switch( (*p) ) {
		case 32: goto tr240;
		case 34: goto tr62;
		case 45: goto tr241;
		case 47: goto tr78;
		case 60: goto tr70;
		case 92: goto tr71;
		case 95: goto tr241;
	}
	if ( (*p) < 65 ) {
		if ( 9 <= (*p) && (*p) <= 13 )
			goto tr240;
	} else if ( (*p) > 90 ) {
		if ( 97 <= (*p) && (*p) <= 122 )
			goto tr241;
	} else
		goto tr241;
	goto tr69;
case 168:
	switch( (*p) ) {
		case 34: goto tr62;
		case 45: goto tr241;
		case 60: goto tr70;
		case 61: goto tr242;
		case 92: goto tr71;
		case 95: goto tr241;
	}
	if ( (*p) < 65 ) {
		if ( 48 <= (*p) && (*p) <= 57 )
			goto tr241;
	} else if ( (*p) > 90 ) {
		if ( 97 <= (*p) && (*p) <= 122 )
			goto tr241;
	} else
		goto tr241;
	goto tr69;
case 169:
	switch( (*p) ) {
		case 32: goto tr243;
		case 34: goto tr224;
		case 39: goto tr236;
		case 60: goto tr70;
		case 92: goto tr71;
	}
	if ( 9 <= (*p) && (*p) <= 13 )
		goto tr243;
	goto tr69;
case 170:
	switch( (*p) ) {
		case 32: goto tr244;
		case 34: goto tr62;
		case 45: goto tr245;
		case 47: goto tr122;
		case 60: goto tr70;
		case 62: goto tr123;
		case 92: goto tr71;
		case 95: goto tr245;
	}
	if ( (*p) < 65 ) {
		if ( 9 <= (*p) && (*p) <= 13 )
			goto tr244;
	} else if ( (*p) > 90 ) {
		if ( 97 <= (*p) && (*p) <= 122 )
			goto tr245;
	} else
		goto tr245;
	goto tr69;
case 171:
	switch( (*p) ) {
		case 32: goto tr244;
		case 34: goto tr62;
		case 45: goto tr245;
		case 47: goto tr122;
		case 60: goto tr70;
		case 92: goto tr71;
		case 95: goto tr245;
	}
	if ( (*p) < 65 ) {
		if ( 9 <= (*p) && (*p) <= 13 )
			goto tr244;
	} else if ( (*p) > 90 ) {
		if ( 97 <= (*p) && (*p) <= 122 )
			goto tr245;
	} else
		goto tr245;
	goto tr69;
case 172:
	switch( (*p) ) {
		case 34: goto tr62;
		case 45: goto tr245;
		case 60: goto tr70;
		case 61: goto tr246;
		case 92: goto tr71;
		case 95: goto tr245;
	}
	if ( (*p) < 65 ) {
		if ( 48 <= (*p) && (*p) <= 57 )
			goto tr245;
	} else if ( (*p) > 90 ) {
		if ( 97 <= (*p) && (*p) <= 122 )
			goto tr245;
	} else
		goto tr245;
	goto tr69;
case 173:
	switch( (*p) ) {
		case 32: goto tr247;
		case 34: goto tr237;
		case 39: goto tr222;
		case 60: goto tr70;
		case 92: goto tr71;
	}
	if ( 9 <= (*p) && (*p) <= 13 )
		goto tr247;
	goto tr69;
case 174:
	switch( (*p) ) {
		case 34: goto tr62;
		case 60: goto tr70;
		case 62: goto tr248;
		case 92: goto tr71;
	}
	goto tr69;
case 314:
	switch( (*p) ) {
		case 34: goto tr403;
		case 60: goto tr417;
		case 92: goto tr418;
	}
	goto tr416;
case 175:
	switch( (*p) ) {
		case 34: goto tr62;
		case 60: goto tr70;
		case 62: goto tr249;
		case 92: goto tr71;
	}
	goto tr69;
case 315:
	switch( (*p) ) {
		case 34: goto tr403;
		case 60: goto tr417;
		case 92: goto tr418;
	}
	goto tr416;
case 176:
	switch( (*p) ) {
		case 34: goto tr231;
		case 39: goto tr237;
		case 47: goto tr250;
		case 60: goto tr238;
		case 92: goto tr239;
		case 101: goto tr251;
	}
	goto tr236;
case 177:
	switch( (*p) ) {
		case 34: goto tr231;
		case 39: goto tr237;
		case 60: goto tr238;
		case 92: goto tr239;
		case 101: goto tr252;
	}
	goto tr236;
case 178:
	switch( (*p) ) {
		case 34: goto tr253;
		case 39: goto tr254;
		case 60: goto tr238;
		case 92: goto tr239;
	}
	goto tr236;
case 179:
	switch( (*p) ) {
		case 32: goto tr255;
		case 34: goto tr231;
		case 39: goto tr237;
		case 45: goto tr256;
		case 47: goto tr257;
		case 60: goto tr238;
		case 62: goto tr187;
		case 92: goto tr239;
		case 95: goto tr256;
	}
	if ( (*p) < 65 ) {
		if ( 9 <= (*p) && (*p) <= 13 )
			goto tr255;
	} else if ( (*p) > 90 ) {
		if ( 97 <= (*p) && (*p) <= 122 )
			goto tr256;
	} else
		goto tr256;
	goto tr236;
case 180:
	switch( (*p) ) {
		case 32: goto tr255;
		case 34: goto tr231;
		case 39: goto tr237;
		case 45: goto tr256;
		case 47: goto tr257;
		case 60: goto tr238;
		case 92: goto tr239;
		case 95: goto tr256;
	}
	if ( (*p) < 65 ) {
		if ( 9 <= (*p) && (*p) <= 13 )
			goto tr255;
	} else if ( (*p) > 90 ) {
		if ( 97 <= (*p) && (*p) <= 122 )
			goto tr256;
	} else
		goto tr256;
	goto tr236;
case 181:
	switch( (*p) ) {
		case 34: goto tr231;
		case 39: goto tr237;
		case 45: goto tr256;
		case 60: goto tr238;
		case 61: goto tr258;
		case 92: goto tr239;
		case 95: goto tr256;
	}
	if ( (*p) < 65 ) {
		if ( 48 <= (*p) && (*p) <= 57 )
			goto tr256;
	} else if ( (*p) > 90 ) {
		if ( 97 <= (*p) && (*p) <= 122 )
			goto tr256;
	} else
		goto tr256;
	goto tr236;
case 182:
	switch( (*p) ) {
		case 32: goto tr259;
		case 34: goto tr253;
		case 39: goto tr260;
		case 60: goto tr238;
		case 92: goto tr239;
	}
	if ( 9 <= (*p) && (*p) <= 13 )
		goto tr259;
	goto tr236;
case 183:
	switch( (*p) ) {
		case 32: goto tr261;
		case 34: goto tr82;
		case 39: goto tr75;
		case 45: goto tr262;
		case 47: goto tr157;
		case 60: goto tr83;
		case 62: goto tr158;
		case 92: goto tr84;
		case 95: goto tr262;
	}
	if ( (*p) < 65 ) {
		if ( 9 <= (*p) && (*p) <= 13 )
			goto tr261;
	} else if ( (*p) > 90 ) {
		if ( 97 <= (*p) && (*p) <= 122 )
			goto tr262;
	} else
		goto tr262;
	goto tr81;
case 184:
	switch( (*p) ) {
		case 32: goto tr261;
		case 34: goto tr82;
		case 39: goto tr75;
		case 45: goto tr262;
		case 47: goto tr157;
		case 60: goto tr83;
		case 92: goto tr84;
		case 95: goto tr262;
	}
	if ( (*p) < 65 ) {
		if ( 9 <= (*p) && (*p) <= 13 )
			goto tr261;
	} else if ( (*p) > 90 ) {
		if ( 97 <= (*p) && (*p) <= 122 )
			goto tr262;
	} else
		goto tr262;
	goto tr81;
case 185:
	switch( (*p) ) {
		case 34: goto tr82;
		case 39: goto tr75;
		case 45: goto tr262;
		case 60: goto tr83;
		case 61: goto tr263;
		case 92: goto tr84;
		case 95: goto tr262;
	}
	if ( (*p) < 65 ) {
		if ( 48 <= (*p) && (*p) <= 57 )
			goto tr262;
	} else if ( (*p) > 90 ) {
		if ( 97 <= (*p) && (*p) <= 122 )
			goto tr262;
	} else
		goto tr262;
	goto tr81;
case 186:
	switch( (*p) ) {
		case 32: goto tr264;
		case 34: goto tr265;
		case 39: goto tr253;
		case 60: goto tr83;
		case 92: goto tr84;
	}
	if ( 9 <= (*p) && (*p) <= 13 )
		goto tr264;
	goto tr81;
case 187:
	switch( (*p) ) {
		case 32: goto tr266;
		case 34: goto tr223;
		case 39: goto tr224;
		case 45: goto tr267;
		case 47: goto tr268;
		case 60: goto tr225;
		case 62: goto tr218;
		case 92: goto tr226;
		case 95: goto tr267;
	}
	if ( (*p) < 65 ) {
		if ( 9 <= (*p) && (*p) <= 13 )
			goto tr266;
	} else if ( (*p) > 90 ) {
		if ( 97 <= (*p) && (*p) <= 122 )
			goto tr267;
	} else
		goto tr267;
	goto tr222;
case 188:
	switch( (*p) ) {
		case 32: goto tr266;
		case 34: goto tr223;
		case 39: goto tr224;
		case 45: goto tr267;
		case 47: goto tr268;
		case 60: goto tr225;
		case 92: goto tr226;
		case 95: goto tr267;
	}
	if ( (*p) < 65 ) {
		if ( 9 <= (*p) && (*p) <= 13 )
			goto tr266;
	} else if ( (*p) > 90 ) {
		if ( 97 <= (*p) && (*p) <= 122 )
			goto tr267;
	} else
		goto tr267;
	goto tr222;
case 189:
	switch( (*p) ) {
		case 34: goto tr223;
		case 39: goto tr224;
		case 45: goto tr267;
		case 60: goto tr225;
		case 61: goto tr269;
		case 92: goto tr226;
		case 95: goto tr267;
	}
	if ( (*p) < 65 ) {
		if ( 48 <= (*p) && (*p) <= 57 )
			goto tr267;
	} else if ( (*p) > 90 ) {
		if ( 97 <= (*p) && (*p) <= 122 )
			goto tr267;
	} else
		goto tr267;
	goto tr222;
case 190:
	switch( (*p) ) {
		case 34: goto tr223;
		case 39: goto tr224;
		case 47: goto tr270;
		case 60: goto tr225;
		case 92: goto tr226;
		case 101: goto tr271;
	}
	goto tr222;
case 191:
	switch( (*p) ) {
		case 34: goto tr223;
		case 39: goto tr224;
		case 60: goto tr225;
		case 92: goto tr226;
		case 101: goto tr272;
	}
	goto tr222;
case 192:
	switch( (*p) ) {
		case 34: goto tr273;
		case 39: goto tr265;
		case 60: goto tr225;
		case 92: goto tr226;
	}
	goto tr222;
case 193:
	switch( (*p) ) {
		case 32: goto tr274;
		case 34: goto tr223;
		case 39: goto tr224;
		case 45: goto tr275;
		case 47: goto tr276;
		case 60: goto tr225;
		case 62: goto tr142;
		case 92: goto tr226;
		case 95: goto tr275;
	}
	if ( (*p) < 65 ) {
		if ( 9 <= (*p) && (*p) <= 13 )
			goto tr274;
	} else if ( (*p) > 90 ) {
		if ( 97 <= (*p) && (*p) <= 122 )
			goto tr275;
	} else
		goto tr275;
	goto tr222;
case 194:
	switch( (*p) ) {
		case 32: goto tr274;
		case 34: goto tr223;
		case 39: goto tr224;
		case 45: goto tr275;
		case 47: goto tr276;
		case 60: goto tr225;
		case 92: goto tr226;
		case 95: goto tr275;
	}
	if ( (*p) < 65 ) {
		if ( 9 <= (*p) && (*p) <= 13 )
			goto tr274;
	} else if ( (*p) > 90 ) {
		if ( 97 <= (*p) && (*p) <= 122 )
			goto tr275;
	} else
		goto tr275;
	goto tr222;
case 195:
	switch( (*p) ) {
		case 34: goto tr223;
		case 39: goto tr224;
		case 45: goto tr275;
		case 60: goto tr225;
		case 61: goto tr277;
		case 92: goto tr226;
		case 95: goto tr275;
	}
	if ( (*p) < 65 ) {
		if ( 48 <= (*p) && (*p) <= 57 )
			goto tr275;
	} else if ( (*p) > 90 ) {
		if ( 97 <= (*p) && (*p) <= 122 )
			goto tr275;
	} else
		goto tr275;
	goto tr222;
case 196:
	switch( (*p) ) {
		case 32: goto tr278;
		case 34: goto tr273;
		case 39: goto tr279;
		case 60: goto tr225;
		case 92: goto tr226;
	}
	if ( 9 <= (*p) && (*p) <= 13 )
		goto tr278;
	goto tr222;
case 197:
	switch( (*p) ) {
		case 32: goto tr280;
		case 34: goto tr82;
		case 39: goto tr75;
		case 45: goto tr281;
		case 47: goto tr148;
		case 60: goto tr83;
		case 62: goto tr149;
		case 92: goto tr84;
		case 95: goto tr281;
	}
	if ( (*p) < 65 ) {
		if ( 9 <= (*p) && (*p) <= 13 )
			goto tr280;
	} else if ( (*p) > 90 ) {
		if ( 97 <= (*p) && (*p) <= 122 )
			goto tr281;
	} else
		goto tr281;
	goto tr81;
case 198:
	switch( (*p) ) {
		case 32: goto tr280;
		case 34: goto tr82;
		case 39: goto tr75;
		case 45: goto tr281;
		case 47: goto tr148;
		case 60: goto tr83;
		case 92: goto tr84;
		case 95: goto tr281;
	}
	if ( (*p) < 65 ) {
		if ( 9 <= (*p) && (*p) <= 13 )
			goto tr280;
	} else if ( (*p) > 90 ) {
		if ( 97 <= (*p) && (*p) <= 122 )
			goto tr281;
	} else
		goto tr281;
	goto tr81;
case 199:
	switch( (*p) ) {
		case 34: goto tr82;
		case 39: goto tr75;
		case 45: goto tr281;
		case 60: goto tr83;
		case 61: goto tr282;
		case 92: goto tr84;
		case 95: goto tr281;
	}
	if ( (*p) < 65 ) {
		if ( 48 <= (*p) && (*p) <= 57 )
			goto tr281;
	} else if ( (*p) > 90 ) {
		if ( 97 <= (*p) && (*p) <= 122 )
			goto tr281;
	} else
		goto tr281;
	goto tr81;
case 200:
	switch( (*p) ) {
		case 32: goto tr283;
		case 34: goto tr254;
		case 39: goto tr273;
		case 60: goto tr83;
		case 92: goto tr84;
	}
	if ( 9 <= (*p) && (*p) <= 13 )
		goto tr283;
	goto tr81;
case 201:
	switch( (*p) ) {
		case 32: goto tr284;
		case 34: goto tr231;
		case 39: goto tr237;
		case 45: goto tr285;
		case 47: goto tr286;
		case 60: goto tr238;
		case 62: goto tr178;
		case 92: goto tr239;
		case 95: goto tr285;
	}
	if ( (*p) < 65 ) {
		if ( 9 <= (*p) && (*p) <= 13 )
			goto tr284;
	} else if ( (*p) > 90 ) {
		if ( 97 <= (*p) && (*p) <= 122 )
			goto tr285;
	} else
		goto tr285;
	goto tr236;
case 202:
	switch( (*p) ) {
		case 32: goto tr284;
		case 34: goto tr231;
		case 39: goto tr237;
		case 45: goto tr285;
		case 47: goto tr286;
		case 60: goto tr238;
		case 92: goto tr239;
		case 95: goto tr285;
	}
	if ( (*p) < 65 ) {
		if ( 9 <= (*p) && (*p) <= 13 )
			goto tr284;
	} else if ( (*p) > 90 ) {
		if ( 97 <= (*p) && (*p) <= 122 )
			goto tr285;
	} else
		goto tr285;
	goto tr236;
case 203:
	switch( (*p) ) {
		case 34: goto tr231;
		case 39: goto tr237;
		case 45: goto tr285;
		case 60: goto tr238;
		case 61: goto tr287;
		case 92: goto tr239;
		case 95: goto tr285;
	}
	if ( (*p) < 65 ) {
		if ( 48 <= (*p) && (*p) <= 57 )
			goto tr285;
	} else if ( (*p) > 90 ) {
		if ( 97 <= (*p) && (*p) <= 122 )
			goto tr285;
	} else
		goto tr285;
	goto tr236;
case 204:
	switch( (*p) ) {
		case 32: goto tr288;
		case 34: goto tr279;
		case 39: goto tr254;
		case 60: goto tr238;
		case 92: goto tr239;
	}
	if ( 9 <= (*p) && (*p) <= 13 )
		goto tr288;
	goto tr236;
case 205:
	switch( (*p) ) {
		case 34: goto tr231;
		case 39: goto tr237;
		case 60: goto tr238;
		case 62: goto tr289;
		case 92: goto tr239;
	}
	goto tr236;
case 316:
	switch( (*p) ) {
		case 34: goto tr420;
		case 39: goto tr421;
		case 60: goto tr422;
		case 92: goto tr423;
	}
	goto tr419;
case 206:
	switch( (*p) ) {
		case 34: goto tr82;
		case 39: goto tr75;
		case 60: goto tr83;
		case 62: goto tr290;
		case 92: goto tr84;
	}
	goto tr81;
case 207:
	switch( (*p) ) {
		case 34: goto tr223;
		case 39: goto tr224;
		case 60: goto tr225;
		case 62: goto tr291;
		case 92: goto tr226;
	}
	goto tr222;
case 317:
	switch( (*p) ) {
		case 34: goto tr412;
		case 39: goto tr413;
		case 60: goto tr414;
		case 92: goto tr415;
	}
	goto tr411;
case 208:
	switch( (*p) ) {
		case 34: goto tr223;
		case 39: goto tr224;
		case 60: goto tr225;
		case 92: goto tr226;
		case 115: goto tr292;
	}
	goto tr222;
case 209:
	switch( (*p) ) {
		case 34: goto tr223;
		case 39: goto tr224;
		case 60: goto tr225;
		case 92: goto tr226;
		case 105: goto tr293;
	}
	goto tr222;
case 210:
	switch( (*p) ) {
		case 34: goto tr223;
		case 39: goto tr224;
		case 58: goto tr294;
		case 60: goto tr225;
		case 92: goto tr226;
	}
	goto tr222;
case 211:
	switch( (*p) ) {
		case 34: goto tr223;
		case 39: goto tr224;
		case 60: goto tr225;
		case 92: goto tr226;
	}
	if ( 97 <= (*p) && (*p) <= 122 )
		goto tr295;
	goto tr222;
case 212:
	switch( (*p) ) {
		case 34: goto tr223;
		case 39: goto tr224;
		case 60: goto tr225;
		case 62: goto tr296;
		case 92: goto tr226;
	}
	if ( 97 <= (*p) && (*p) <= 122 )
		goto tr295;
	goto tr222;
case 213:
	switch( (*p) ) {
		case 34: goto tr223;
		case 39: goto tr224;
		case 60: goto tr225;
		case 92: goto tr226;
		case 115: goto tr297;
	}
	goto tr222;
case 214:
	switch( (*p) ) {
		case 34: goto tr223;
		case 39: goto tr224;
		case 60: goto tr225;
		case 92: goto tr226;
		case 105: goto tr298;
	}
	goto tr222;
case 215:
	switch( (*p) ) {
		case 34: goto tr223;
		case 39: goto tr224;
		case 58: goto tr299;
		case 60: goto tr225;
		case 92: goto tr226;
	}
	goto tr222;
case 216:
	switch( (*p) ) {
		case 34: goto tr223;
		case 39: goto tr224;
		case 60: goto tr225;
		case 92: goto tr226;
	}
	if ( 97 <= (*p) && (*p) <= 122 )
		goto tr300;
	goto tr222;
case 217:
	switch( (*p) ) {
		case 32: goto tr301;
		case 34: goto tr223;
		case 39: goto tr224;
		case 60: goto tr225;
		case 62: goto tr302;
		case 92: goto tr226;
	}
	if ( (*p) > 13 ) {
		if ( 97 <= (*p) && (*p) <= 122 )
			goto tr300;
	} else if ( (*p) >= 9 )
		goto tr301;
	goto tr222;
case 218:
	switch( (*p) ) {
		case 32: goto tr303;
		case 34: goto tr223;
		case 39: goto tr224;
		case 45: goto tr304;
		case 47: goto tr305;
		case 60: goto tr225;
		case 62: goto tr306;
		case 92: goto tr226;
		case 95: goto tr304;
	}
	if ( (*p) < 65 ) {
		if ( 9 <= (*p) && (*p) <= 13 )
			goto tr303;
	} else if ( (*p) > 90 ) {
		if ( 97 <= (*p) && (*p) <= 122 )
			goto tr304;
	} else
		goto tr304;
	goto tr222;
case 219:
	switch( (*p) ) {
		case 32: goto tr303;
		case 34: goto tr223;
		case 39: goto tr224;
		case 45: goto tr304;
		case 47: goto tr305;
		case 60: goto tr225;
		case 92: goto tr226;
		case 95: goto tr304;
	}
	if ( (*p) < 65 ) {
		if ( 9 <= (*p) && (*p) <= 13 )
			goto tr303;
	} else if ( (*p) > 90 ) {
		if ( 97 <= (*p) && (*p) <= 122 )
			goto tr304;
	} else
		goto tr304;
	goto tr222;
case 220:
	switch( (*p) ) {
		case 34: goto tr223;
		case 39: goto tr224;
		case 45: goto tr304;
		case 60: goto tr225;
		case 61: goto tr307;
		case 92: goto tr226;
		case 95: goto tr304;
	}
	if ( (*p) < 65 ) {
		if ( 48 <= (*p) && (*p) <= 57 )
			goto tr304;
	} else if ( (*p) > 90 ) {
		if ( 97 <= (*p) && (*p) <= 122 )
			goto tr304;
	} else
		goto tr304;
	goto tr222;
case 221:
	switch( (*p) ) {
		case 32: goto tr308;
		case 34: goto tr309;
		case 39: goto tr310;
		case 60: goto tr225;
		case 92: goto tr226;
	}
	if ( 9 <= (*p) && (*p) <= 13 )
		goto tr308;
	goto tr222;
case 222:
	switch( (*p) ) {
		case 32: goto tr311;
		case 34: goto tr111;
		case 39: goto tr112;
		case 45: goto tr312;
		case 47: goto tr177;
		case 60: goto tr113;
		case 62: goto tr178;
		case 92: goto tr114;
		case 95: goto tr312;
	}
	if ( (*p) < 65 ) {
		if ( 9 <= (*p) && (*p) <= 13 )
			goto tr311;
	} else if ( (*p) > 90 ) {
		if ( 97 <= (*p) && (*p) <= 122 )
			goto tr312;
	} else
		goto tr312;
	goto tr109;
case 223:
	switch( (*p) ) {
		case 32: goto tr311;
		case 34: goto tr111;
		case 39: goto tr112;
		case 45: goto tr312;
		case 47: goto tr177;
		case 60: goto tr113;
		case 92: goto tr114;
		case 95: goto tr312;
	}
	if ( (*p) < 65 ) {
		if ( 9 <= (*p) && (*p) <= 13 )
			goto tr311;
	} else if ( (*p) > 90 ) {
		if ( 97 <= (*p) && (*p) <= 122 )
			goto tr312;
	} else
		goto tr312;
	goto tr109;
case 224:
	switch( (*p) ) {
		case 34: goto tr111;
		case 39: goto tr112;
		case 45: goto tr312;
		case 60: goto tr113;
		case 61: goto tr313;
		case 92: goto tr114;
		case 95: goto tr312;
	}
	if ( (*p) < 65 ) {
		if ( 48 <= (*p) && (*p) <= 57 )
			goto tr312;
	} else if ( (*p) > 90 ) {
		if ( 97 <= (*p) && (*p) <= 122 )
			goto tr312;
	} else
		goto tr312;
	goto tr109;
case 225:
	switch( (*p) ) {
		case 32: goto tr314;
		case 34: goto tr315;
		case 39: goto tr316;
		case 60: goto tr113;
		case 92: goto tr114;
	}
	if ( 9 <= (*p) && (*p) <= 13 )
		goto tr314;
	goto tr109;
case 226:
	switch( (*p) ) {
		case 32: goto tr317;
		case 34: goto tr111;
		case 39: goto tr112;
		case 45: goto tr318;
		case 47: goto tr186;
		case 60: goto tr113;
		case 62: goto tr187;
		case 92: goto tr114;
		case 95: goto tr318;
	}
	if ( (*p) < 65 ) {
		if ( 9 <= (*p) && (*p) <= 13 )
			goto tr317;
	} else if ( (*p) > 90 ) {
		if ( 97 <= (*p) && (*p) <= 122 )
			goto tr318;
	} else
		goto tr318;
	goto tr109;
case 227:
	switch( (*p) ) {
		case 32: goto tr317;
		case 34: goto tr111;
		case 39: goto tr112;
		case 45: goto tr318;
		case 47: goto tr186;
		case 60: goto tr113;
		case 92: goto tr114;
		case 95: goto tr318;
	}
	if ( (*p) < 65 ) {
		if ( 9 <= (*p) && (*p) <= 13 )
			goto tr317;
	} else if ( (*p) > 90 ) {
		if ( 97 <= (*p) && (*p) <= 122 )
			goto tr318;
	} else
		goto tr318;
	goto tr109;
case 228:
	switch( (*p) ) {
		case 34: goto tr111;
		case 39: goto tr112;
		case 45: goto tr318;
		case 60: goto tr113;
		case 61: goto tr319;
		case 92: goto tr114;
		case 95: goto tr318;
	}
	if ( (*p) < 65 ) {
		if ( 48 <= (*p) && (*p) <= 57 )
			goto tr318;
	} else if ( (*p) > 90 ) {
		if ( 97 <= (*p) && (*p) <= 122 )
			goto tr318;
	} else
		goto tr318;
	goto tr109;
case 229:
	switch( (*p) ) {
		case 32: goto tr320;
		case 34: goto tr321;
		case 39: goto tr309;
		case 60: goto tr113;
		case 92: goto tr114;
	}
	if ( 9 <= (*p) && (*p) <= 13 )
		goto tr320;
	goto tr109;
case 230:
	switch( (*p) ) {
		case 32: goto tr322;
		case 34: goto tr231;
		case 39: goto tr237;
		case 45: goto tr323;
		case 47: goto tr324;
		case 60: goto tr238;
		case 62: goto tr325;
		case 92: goto tr239;
		case 95: goto tr323;
	}
	if ( (*p) < 65 ) {
		if ( 9 <= (*p) && (*p) <= 13 )
			goto tr322;
	} else if ( (*p) > 90 ) {
		if ( 97 <= (*p) && (*p) <= 122 )
			goto tr323;
	} else
		goto tr323;
	goto tr236;
case 231:
	switch( (*p) ) {
		case 32: goto tr322;
		case 34: goto tr231;
		case 39: goto tr237;
		case 45: goto tr323;
		case 47: goto tr324;
		case 60: goto tr238;
		case 92: goto tr239;
		case 95: goto tr323;
	}
	if ( (*p) < 65 ) {
		if ( 9 <= (*p) && (*p) <= 13 )
			goto tr322;
	} else if ( (*p) > 90 ) {
		if ( 97 <= (*p) && (*p) <= 122 )
			goto tr323;
	} else
		goto tr323;
	goto tr236;
case 232:
	switch( (*p) ) {
		case 34: goto tr231;
		case 39: goto tr237;
		case 45: goto tr323;
		case 60: goto tr238;
		case 61: goto tr326;
		case 92: goto tr239;
		case 95: goto tr323;
	}
	if ( (*p) < 65 ) {
		if ( 48 <= (*p) && (*p) <= 57 )
			goto tr323;
	} else if ( (*p) > 90 ) {
		if ( 97 <= (*p) && (*p) <= 122 )
			goto tr323;
	} else
		goto tr323;
	goto tr236;
case 233:
	switch( (*p) ) {
		case 32: goto tr327;
		case 34: goto tr316;
		case 39: goto tr328;
		case 60: goto tr238;
		case 92: goto tr239;
	}
	if ( 9 <= (*p) && (*p) <= 13 )
		goto tr327;
	goto tr236;
case 234:
	switch( (*p) ) {
		case 32: goto tr329;
		case 34: goto tr110;
		case 39: goto tr119;
		case 45: goto tr330;
		case 47: goto tr141;
		case 60: goto tr127;
		case 62: goto tr142;
		case 92: goto tr128;
		case 95: goto tr330;
	}
	if ( (*p) < 65 ) {
		if ( 9 <= (*p) && (*p) <= 13 )
			goto tr329;
	} else if ( (*p) > 90 ) {
		if ( 97 <= (*p) && (*p) <= 122 )
			goto tr330;
	} else
		goto tr330;
	goto tr126;
case 235:
	switch( (*p) ) {
		case 32: goto tr329;
		case 34: goto tr110;
		case 39: goto tr119;
		case 45: goto tr330;
		case 47: goto tr141;
		case 60: goto tr127;
		case 92: goto tr128;
		case 95: goto tr330;
	}
	if ( (*p) < 65 ) {
		if ( 9 <= (*p) && (*p) <= 13 )
			goto tr329;
	} else if ( (*p) > 90 ) {
		if ( 97 <= (*p) && (*p) <= 122 )
			goto tr330;
	} else
		goto tr330;
	goto tr126;
case 236:
	switch( (*p) ) {
		case 34: goto tr110;
		case 39: goto tr119;
		case 45: goto tr330;
		case 60: goto tr127;
		case 61: goto tr331;
		case 92: goto tr128;
		case 95: goto tr330;
	}
	if ( (*p) < 65 ) {
		if ( 48 <= (*p) && (*p) <= 57 )
			goto tr330;
	} else if ( (*p) > 90 ) {
		if ( 97 <= (*p) && (*p) <= 122 )
			goto tr330;
	} else
		goto tr330;
	goto tr126;
case 237:
	switch( (*p) ) {
		case 32: goto tr332;
		case 34: goto tr310;
		case 39: goto tr321;
		case 60: goto tr127;
		case 92: goto tr128;
	}
	if ( 9 <= (*p) && (*p) <= 13 )
		goto tr332;
	goto tr126;
case 238:
	switch( (*p) ) {
		case 32: goto tr333;
		case 34: goto tr110;
		case 39: goto tr119;
		case 45: goto tr334;
		case 47: goto tr217;
		case 60: goto tr127;
		case 62: goto tr218;
		case 92: goto tr128;
		case 95: goto tr334;
	}
	if ( (*p) < 65 ) {
		if ( 9 <= (*p) && (*p) <= 13 )
			goto tr333;
	} else if ( (*p) > 90 ) {
		if ( 97 <= (*p) && (*p) <= 122 )
			goto tr334;
	} else
		goto tr334;
	goto tr126;
case 239:
	switch( (*p) ) {
		case 32: goto tr333;
		case 34: goto tr110;
		case 39: goto tr119;
		case 45: goto tr334;
		case 47: goto tr217;
		case 60: goto tr127;
		case 92: goto tr128;
		case 95: goto tr334;
	}
	if ( (*p) < 65 ) {
		if ( 9 <= (*p) && (*p) <= 13 )
			goto tr333;
	} else if ( (*p) > 90 ) {
		if ( 97 <= (*p) && (*p) <= 122 )
			goto tr334;
	} else
		goto tr334;
	goto tr126;
case 240:
	switch( (*p) ) {
		case 34: goto tr110;
		case 39: goto tr119;
		case 45: goto tr334;
		case 60: goto tr127;
		case 61: goto tr335;
		case 92: goto tr128;
		case 95: goto tr334;
	}
	if ( (*p) < 65 ) {
		if ( 48 <= (*p) && (*p) <= 57 )
			goto tr334;
	} else if ( (*p) > 90 ) {
		if ( 97 <= (*p) && (*p) <= 122 )
			goto tr334;
	} else
		goto tr334;
	goto tr126;
case 241:
	switch( (*p) ) {
		case 32: goto tr336;
		case 34: goto tr328;
		case 39: goto tr315;
		case 60: goto tr127;
		case 92: goto tr128;
	}
	if ( 9 <= (*p) && (*p) <= 13 )
		goto tr336;
	goto tr126;
case 242:
	switch( (*p) ) {
		case 34: goto tr110;
		case 39: goto tr119;
		case 60: goto tr127;
		case 62: goto tr291;
		case 92: goto tr128;
	}
	goto tr126;
case 243:
	switch( (*p) ) {
		case 34: goto tr231;
		case 39: goto tr237;
		case 60: goto tr238;
		case 62: goto tr337;
		case 92: goto tr239;
	}
	goto tr236;
case 318:
	switch( (*p) ) {
		case 34: goto tr420;
		case 39: goto tr421;
		case 60: goto tr422;
		case 92: goto tr423;
	}
	goto tr419;
case 244:
	switch( (*p) ) {
		case 34: goto tr111;
		case 39: goto tr112;
		case 60: goto tr113;
		case 62: goto tr338;
		case 92: goto tr114;
	}
	goto tr109;
case 319:
	switch( (*p) ) {
		case 34: goto tr420;
		case 39: goto tr421;
		case 60: goto tr422;
		case 92: goto tr423;
	}
	goto tr419;
case 245:
	switch( (*p) ) {
		case 34: goto tr111;
		case 39: goto tr112;
		case 60: goto tr113;
		case 62: goto tr289;
		case 92: goto tr114;
	}
	goto tr109;
case 246:
	switch( (*p) ) {
		case 34: goto tr223;
		case 39: goto tr224;
		case 60: goto tr225;
		case 62: goto tr339;
		case 92: goto tr226;
	}
	goto tr222;
case 320:
	switch( (*p) ) {
		case 34: goto tr412;
		case 39: goto tr413;
		case 60: goto tr414;
		case 92: goto tr415;
	}
	goto tr411;
case 247:
	switch( (*p) ) {
		case 32: goto tr340;
		case 34: goto tr260;
		case 39: goto tr265;
		case 60: goto tr225;
		case 92: goto tr226;
	}
	if ( 9 <= (*p) && (*p) <= 13 )
		goto tr340;
	goto tr222;
case 248:
	switch( (*p) ) {
		case 34: goto tr223;
		case 39: goto tr224;
		case 60: goto tr225;
		case 62: goto tr221;
		case 92: goto tr226;
	}
	goto tr222;
case 249:
	switch( (*p) ) {
		case 34: goto tr231;
		case 39: goto tr237;
		case 60: goto tr238;
		case 62: goto tr338;
		case 92: goto tr239;
	}
	goto tr236;
case 250:
	switch( (*p) ) {
		case 34: goto tr231;
		case 39: goto tr237;
		case 60: goto tr238;
		case 92: goto tr239;
		case 115: goto tr341;
	}
	goto tr236;
case 251:
	switch( (*p) ) {
		case 34: goto tr231;
		case 39: goto tr237;
		case 60: goto tr238;
		case 92: goto tr239;
		case 105: goto tr342;
	}
	goto tr236;
case 252:
	switch( (*p) ) {
		case 34: goto tr231;
		case 39: goto tr237;
		case 58: goto tr343;
		case 60: goto tr238;
		case 92: goto tr239;
	}
	goto tr236;
case 253:
	switch( (*p) ) {
		case 34: goto tr231;
		case 39: goto tr237;
		case 60: goto tr238;
		case 92: goto tr239;
	}
	if ( 97 <= (*p) && (*p) <= 122 )
		goto tr344;
	goto tr236;
case 254:
	switch( (*p) ) {
		case 34: goto tr231;
		case 39: goto tr237;
		case 60: goto tr238;
		case 62: goto tr345;
		case 92: goto tr239;
	}
	if ( 97 <= (*p) && (*p) <= 122 )
		goto tr344;
	goto tr236;
case 255:
	switch( (*p) ) {
		case 34: goto tr231;
		case 39: goto tr237;
		case 60: goto tr238;
		case 92: goto tr239;
		case 115: goto tr346;
	}
	goto tr236;
case 256:
	switch( (*p) ) {
		case 34: goto tr231;
		case 39: goto tr237;
		case 60: goto tr238;
		case 92: goto tr239;
		case 105: goto tr347;
	}
	goto tr236;
case 257:
	switch( (*p) ) {
		case 34: goto tr231;
		case 39: goto tr237;
		case 58: goto tr348;
		case 60: goto tr238;
		case 92: goto tr239;
	}
	goto tr236;
case 258:
	switch( (*p) ) {
		case 34: goto tr231;
		case 39: goto tr237;
		case 60: goto tr238;
		case 92: goto tr239;
	}
	if ( 97 <= (*p) && (*p) <= 122 )
		goto tr349;
	goto tr236;
case 259:
	switch( (*p) ) {
		case 32: goto tr350;
		case 34: goto tr231;
		case 39: goto tr237;
		case 60: goto tr238;
		case 62: goto tr351;
		case 92: goto tr239;
	}
	if ( (*p) > 13 ) {
		if ( 97 <= (*p) && (*p) <= 122 )
			goto tr349;
	} else if ( (*p) >= 9 )
		goto tr350;
	goto tr236;
case 260:
	switch( (*p) ) {
		case 39: goto tr62;
		case 60: goto tr63;
		case 62: goto tr352;
		case 92: goto tr64;
	}
	goto tr61;
case 321:
	switch( (*p) ) {
		case 39: goto tr403;
		case 60: goto tr404;
		case 92: goto tr405;
	}
	goto tr402;
case 261:
	switch( (*p) ) {
		case 39: goto tr62;
		case 60: goto tr63;
		case 62: goto tr133;
		case 92: goto tr64;
	}
	goto tr61;
case 262:
	switch( (*p) ) {
		case 34: goto tr111;
		case 39: goto tr112;
		case 60: goto tr113;
		case 92: goto tr114;
		case 115: goto tr353;
	}
	goto tr109;
case 263:
	switch( (*p) ) {
		case 34: goto tr111;
		case 39: goto tr112;
		case 60: goto tr113;
		case 92: goto tr114;
		case 105: goto tr354;
	}
	goto tr109;
case 264:
	switch( (*p) ) {
		case 34: goto tr111;
		case 39: goto tr112;
		case 58: goto tr355;
		case 60: goto tr113;
		case 92: goto tr114;
	}
	goto tr109;
case 265:
	switch( (*p) ) {
		case 34: goto tr111;
		case 39: goto tr112;
		case 60: goto tr113;
		case 92: goto tr114;
	}
	if ( 97 <= (*p) && (*p) <= 122 )
		goto tr356;
	goto tr109;
case 266:
	switch( (*p) ) {
		case 34: goto tr111;
		case 39: goto tr112;
		case 60: goto tr113;
		case 62: goto tr357;
		case 92: goto tr114;
	}
	if ( 97 <= (*p) && (*p) <= 122 )
		goto tr356;
	goto tr109;
case 267:
	switch( (*p) ) {
		case 34: goto tr111;
		case 39: goto tr112;
		case 60: goto tr113;
		case 92: goto tr114;
		case 115: goto tr358;
	}
	goto tr109;
case 268:
	switch( (*p) ) {
		case 34: goto tr111;
		case 39: goto tr112;
		case 60: goto tr113;
		case 92: goto tr114;
		case 105: goto tr359;
	}
	goto tr109;
case 269:
	switch( (*p) ) {
		case 34: goto tr111;
		case 39: goto tr112;
		case 58: goto tr360;
		case 60: goto tr113;
		case 92: goto tr114;
	}
	goto tr109;
case 270:
	switch( (*p) ) {
		case 34: goto tr111;
		case 39: goto tr112;
		case 60: goto tr113;
		case 92: goto tr114;
	}
	if ( 97 <= (*p) && (*p) <= 122 )
		goto tr361;
	goto tr109;
case 271:
	switch( (*p) ) {
		case 32: goto tr362;
		case 34: goto tr111;
		case 39: goto tr112;
		case 60: goto tr113;
		case 62: goto tr363;
		case 92: goto tr114;
	}
	if ( (*p) > 13 ) {
		if ( 97 <= (*p) && (*p) <= 122 )
			goto tr361;
	} else if ( (*p) >= 9 )
		goto tr362;
	goto tr109;
case 272:
	switch( (*p) ) {
		case 32: goto tr364;
		case 34: goto tr145;
		case 39: goto tr174;
		case 60: goto tr113;
		case 92: goto tr114;
	}
	if ( 9 <= (*p) && (*p) <= 13 )
		goto tr364;
	goto tr109;
case 273:
	switch( (*p) ) {
		case 34: goto tr110;
		case 39: goto tr119;
		case 60: goto tr127;
		case 92: goto tr128;
		case 115: goto tr365;
	}
	goto tr126;
case 274:
	switch( (*p) ) {
		case 34: goto tr110;
		case 39: goto tr119;
		case 60: goto tr127;
		case 92: goto tr128;
		case 105: goto tr366;
	}
	goto tr126;
case 275:
	switch( (*p) ) {
		case 34: goto tr110;
		case 39: goto tr119;
		case 58: goto tr367;
		case 60: goto tr127;
		case 92: goto tr128;
	}
	goto tr126;
case 276:
	switch( (*p) ) {
		case 34: goto tr110;
		case 39: goto tr119;
		case 60: goto tr127;
		case 92: goto tr128;
	}
	if ( 97 <= (*p) && (*p) <= 122 )
		goto tr368;
	goto tr126;
case 277:
	switch( (*p) ) {
		case 34: goto tr110;
		case 39: goto tr119;
		case 60: goto tr127;
		case 62: goto tr369;
		case 92: goto tr128;
	}
	if ( 97 <= (*p) && (*p) <= 122 )
		goto tr368;
	goto tr126;
case 278:
	switch( (*p) ) {
		case 34: goto tr110;
		case 39: goto tr119;
		case 60: goto tr127;
		case 92: goto tr128;
		case 115: goto tr370;
	}
	goto tr126;
case 279:
	switch( (*p) ) {
		case 34: goto tr110;
		case 39: goto tr119;
		case 60: goto tr127;
		case 92: goto tr128;
		case 105: goto tr371;
	}
	goto tr126;
case 280:
	switch( (*p) ) {
		case 34: goto tr110;
		case 39: goto tr119;
		case 58: goto tr372;
		case 60: goto tr127;
		case 92: goto tr128;
	}
	goto tr126;
case 281:
	switch( (*p) ) {
		case 34: goto tr110;
		case 39: goto tr119;
		case 60: goto tr127;
		case 92: goto tr128;
	}
	if ( 97 <= (*p) && (*p) <= 122 )
		goto tr373;
	goto tr126;
case 282:
	switch( (*p) ) {
		case 32: goto tr374;
		case 34: goto tr110;
		case 39: goto tr119;
		case 60: goto tr127;
		case 62: goto tr375;
		case 92: goto tr128;
	}
	if ( (*p) > 13 ) {
		if ( 97 <= (*p) && (*p) <= 122 )
			goto tr373;
	} else if ( (*p) >= 9 )
		goto tr374;
	goto tr126;
case 283:
	switch( (*p) ) {
		case 34: goto tr24;
		case 60: goto tr25;
		case 62: goto tr249;
		case 92: goto tr26;
	}
	goto tr22;
case 284:
	switch( (*p) ) {
		case 32: goto tr376;
		case 34: goto tr81;
		case 39: goto tr82;
		case 60: goto tr63;
		case 92: goto tr64;
	}
	if ( 9 <= (*p) && (*p) <= 13 )
		goto tr376;
	goto tr61;
case 285:
	switch( (*p) ) {
		case 34: goto tr62;
		case 60: goto tr70;
		case 92: goto tr71;
		case 115: goto tr377;
	}
	goto tr69;
case 286:
	switch( (*p) ) {
		case 34: goto tr62;
		case 60: goto tr70;
		case 92: goto tr71;
		case 105: goto tr378;
	}
	goto tr69;
case 287:
	switch( (*p) ) {
		case 34: goto tr62;
		case 58: goto tr379;
		case 60: goto tr70;
		case 92: goto tr71;
	}
	goto tr69;
case 288:
	switch( (*p) ) {
		case 34: goto tr62;
		case 60: goto tr70;
		case 92: goto tr71;
	}
	if ( 97 <= (*p) && (*p) <= 122 )
		goto tr380;
	goto tr69;
case 289:
	switch( (*p) ) {
		case 34: goto tr62;
		case 60: goto tr70;
		case 62: goto tr381;
		case 92: goto tr71;
	}
	if ( 97 <= (*p) && (*p) <= 122 )
		goto tr380;
	goto tr69;
case 290:
	switch( (*p) ) {
		case 34: goto tr62;
		case 60: goto tr70;
		case 92: goto tr71;
		case 115: goto tr382;
	}
	goto tr69;
case 291:
	switch( (*p) ) {
		case 34: goto tr62;
		case 60: goto tr70;
		case 92: goto tr71;
		case 105: goto tr383;
	}
	goto tr69;
case 292:
	switch( (*p) ) {
		case 34: goto tr62;
		case 58: goto tr384;
		case 60: goto tr70;
		case 92: goto tr71;
	}
	goto tr69;
case 293:
	switch( (*p) ) {
		case 34: goto tr62;
		case 60: goto tr70;
		case 92: goto tr71;
	}
	if ( 97 <= (*p) && (*p) <= 122 )
		goto tr385;
	goto tr69;
case 294:
	switch( (*p) ) {
		case 32: goto tr386;
		case 34: goto tr62;
		case 60: goto tr70;
		case 62: goto tr387;
		case 92: goto tr71;
	}
	if ( (*p) > 13 ) {
		if ( 97 <= (*p) && (*p) <= 122 )
			goto tr385;
	} else if ( (*p) >= 9 )
		goto tr386;
	goto tr69;
case 295:
	switch( (*p) ) {
		case 60: goto tr1;
		case 62: goto tr388;
	}
	goto tr0;
case 322:
	if ( (*p) == 60 )
		goto tr425;
	goto tr424;
case 296:
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
case 297:
	switch( (*p) ) {
		case 39: goto tr24;
		case 45: goto tr42;
		case 60: goto tr44;
		case 61: goto tr389;
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
case 298:
	switch( (*p) ) {
		case 32: goto tr390;
		case 34: goto tr37;
		case 39: goto tr38;
		case 60: goto tr44;
		case 92: goto tr46;
	}
	if ( 9 <= (*p) && (*p) <= 13 )
		goto tr390;
	goto tr23;
case 299:
	switch( (*p) ) {
		case 34: goto tr24;
		case 60: goto tr25;
		case 92: goto tr26;
		case 115: goto tr391;
	}
	goto tr22;
case 300:
	switch( (*p) ) {
		case 34: goto tr24;
		case 60: goto tr25;
		case 92: goto tr26;
		case 105: goto tr392;
	}
	goto tr22;
case 301:
	switch( (*p) ) {
		case 34: goto tr24;
		case 58: goto tr393;
		case 60: goto tr25;
		case 92: goto tr26;
	}
	goto tr22;
case 302:
	switch( (*p) ) {
		case 34: goto tr24;
		case 60: goto tr25;
		case 92: goto tr26;
	}
	if ( 97 <= (*p) && (*p) <= 122 )
		goto tr394;
	goto tr22;
case 303:
	switch( (*p) ) {
		case 34: goto tr24;
		case 60: goto tr25;
		case 62: goto tr395;
		case 92: goto tr26;
	}
	if ( 97 <= (*p) && (*p) <= 122 )
		goto tr394;
	goto tr22;
case 304:
	switch( (*p) ) {
		case 34: goto tr24;
		case 60: goto tr25;
		case 92: goto tr26;
		case 115: goto tr396;
	}
	goto tr22;
case 305:
	switch( (*p) ) {
		case 34: goto tr24;
		case 60: goto tr25;
		case 92: goto tr26;
		case 105: goto tr397;
	}
	goto tr22;
case 306:
	switch( (*p) ) {
		case 34: goto tr24;
		case 58: goto tr398;
		case 60: goto tr25;
		case 92: goto tr26;
	}
	goto tr22;
case 307:
	switch( (*p) ) {
		case 34: goto tr24;
		case 60: goto tr25;
		case 92: goto tr26;
	}
	if ( 97 <= (*p) && (*p) <= 122 )
		goto tr399;
	goto tr22;
case 308:
	switch( (*p) ) {
		case 32: goto tr400;
		case 34: goto tr24;
		case 60: goto tr25;
		case 62: goto tr401;
		case 92: goto tr26;
	}
	if ( (*p) > 13 ) {
		if ( 97 <= (*p) && (*p) <= 122 )
			goto tr399;
	} else if ( (*p) >= 9 )
		goto tr400;
	goto tr22;
	}

	tr0: cs = 0; goto f0;
	tr9: cs = 0; goto f2;
	tr424: cs = 0; goto f9;
	tr1: cs = 1; goto f1;
	tr425: cs = 1; goto f11;
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
	tr16: cs = 14; goto f0;
	tr17: cs = 15; goto f0;
	tr21: cs = 16; goto f0;
	tr20: cs = 16; goto f6;
	tr22: cs = 17; goto f0;
	tr395: cs = 17; goto f2;
	tr25: cs = 18; goto f1;
	tr27: cs = 19; goto f0;
	tr26: cs = 20; goto f0;
	tr400: cs = 21; goto f3;
	tr30: cs = 21; goto f7;
	tr31: cs = 22; goto f0;
	tr32: cs = 23; goto f0;
	tr36: cs = 24; goto f0;
	tr35: cs = 24; goto f6;
	tr37: cs = 25; goto f0;
	tr207: cs = 25; goto f2;
	tr59: cs = 26; goto f3;
	tr38: cs = 26; goto f7;
	tr23: cs = 27; goto f0;
	tr54: cs = 27; goto f2;
	tr44: cs = 28; goto f1;
	tr47: cs = 29; goto f0;
	tr46: cs = 30; goto f0;
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
	tr61: cs = 41; goto f0;
	tr96: cs = 41; goto f2;
	tr402: cs = 41; goto f9;
	tr62: cs = 42; goto f7;
	tr403: cs = 42; goto f10;
	tr65: cs = 43; goto f0;
	tr66: cs = 44; goto f0;
	tr68: cs = 45; goto f0;
	tr67: cs = 45; goto f6;
	tr69: cs = 46; goto f0;
	tr381: cs = 46; goto f2;
	tr416: cs = 46; goto f9;
	tr70: cs = 47; goto f1;
	tr417: cs = 47; goto f11;
	tr72: cs = 48; goto f0;
	tr71: cs = 49; goto f0;
	tr418: cs = 49; goto f9;
	tr75: cs = 50; goto f7;
	tr408: cs = 50; goto f10;
	tr76: cs = 51; goto f0;
	tr77: cs = 52; goto f0;
	tr80: cs = 53; goto f0;
	tr79: cs = 53; goto f6;
	tr81: cs = 54; goto f0;
	tr166: cs = 54; goto f2;
	tr406: cs = 54; goto f9;
	tr82: cs = 55; goto f7;
	tr407: cs = 55; goto f10;
	tr85: cs = 56; goto f0;
	tr86: cs = 57; goto f0;
	tr63: cs = 58; goto f1;
	tr404: cs = 58; goto f11;
	tr89: cs = 59; goto f0;
	tr64: cs = 60; goto f0;
	tr405: cs = 60; goto f9;
	tr91: cs = 61; goto f0;
	tr92: cs = 62; goto f0;
	tr93: cs = 63; goto f0;
	tr94: cs = 64; goto f0;
	tr95: cs = 65; goto f0;
	tr90: cs = 66; goto f0;
	tr97: cs = 67; goto f0;
	tr98: cs = 68; goto f0;
	tr99: cs = 69; goto f0;
	tr100: cs = 70; goto f0;
	tr101: cs = 71; goto f3;
	tr111: cs = 71; goto f7;
	tr103: cs = 72; goto f0;
	tr104: cs = 73; goto f0;
	tr108: cs = 74; goto f0;
	tr107: cs = 74; goto f6;
	tr109: cs = 75; goto f0;
	tr357: cs = 75; goto f2;
	tr112: cs = 76; goto f7;
	tr115: cs = 77; goto f0;
	tr116: cs = 78; goto f0;
	tr118: cs = 79; goto f0;
	tr117: cs = 79; goto f6;
	tr386: cs = 80; goto f3;
	tr119: cs = 80; goto f7;
	tr120: cs = 81; goto f0;
	tr121: cs = 82; goto f0;
	tr125: cs = 83; goto f0;
	tr124: cs = 83; goto f6;
	tr126: cs = 84; goto f0;
	tr369: cs = 84; goto f2;
	tr110: cs = 85; goto f7;
	tr129: cs = 86; goto f0;
	tr130: cs = 87; goto f0;
	tr132: cs = 88; goto f0;
	tr131: cs = 88; goto f6;
	tr43: cs = 89; goto f0;
	tr127: cs = 90; goto f1;
	tr134: cs = 91; goto f0;
	tr128: cs = 92; goto f0;
	tr137: cs = 93; goto f7;
	tr139: cs = 94; goto f0;
	tr140: cs = 95; goto f0;
	tr144: cs = 96; goto f0;
	tr143: cs = 96; goto f6;
	tr171: cs = 97; goto f3;
	tr145: cs = 97; goto f7;
	tr146: cs = 98; goto f0;
	tr147: cs = 99; goto f0;
	tr83: cs = 100; goto f1;
	tr409: cs = 100; goto f11;
	tr151: cs = 101; goto f0;
	tr84: cs = 102; goto f0;
	tr410: cs = 102; goto f9;
	tr154: cs = 103; goto f7;
	tr155: cs = 104; goto f0;
	tr156: cs = 105; goto f0;
	tr160: cs = 106; goto f0;
	tr159: cs = 106; goto f6;
	tr157: cs = 107; goto f0;
	tr153: cs = 108; goto f0;
	tr162: cs = 109; goto f0;
	tr163: cs = 110; goto f0;
	tr164: cs = 111; goto f0;
	tr165: cs = 112; goto f0;
	tr152: cs = 113; goto f0;
	tr167: cs = 114; goto f0;
	tr168: cs = 115; goto f0;
	tr169: cs = 116; goto f0;
	tr170: cs = 117; goto f0;
	tr173: cs = 118; goto f0;
	tr150: cs = 118; goto f6;
	tr174: cs = 119; goto f7;
	tr175: cs = 120; goto f0;
	tr176: cs = 121; goto f0;
	tr113: cs = 122; goto f1;
	tr180: cs = 123; goto f0;
	tr114: cs = 124; goto f0;
	tr362: cs = 125; goto f3;
	tr183: cs = 125; goto f7;
	tr184: cs = 126; goto f0;
	tr185: cs = 127; goto f0;
	tr189: cs = 128; goto f0;
	tr188: cs = 128; goto f6;
	tr190: cs = 129; goto f7;
	tr191: cs = 130; goto f0;
	tr192: cs = 131; goto f0;
	tr39: cs = 132; goto f1;
	tr195: cs = 133; goto f0;
	tr40: cs = 134; goto f0;
	tr212: cs = 135; goto f3;
	tr198: cs = 135; goto f7;
	tr199: cs = 136; goto f0;
	tr200: cs = 137; goto f0;
	tr202: cs = 138; goto f0;
	tr201: cs = 138; goto f6;
	tr193: cs = 139; goto f0;
	tr197: cs = 140; goto f0;
	tr203: cs = 141; goto f0;
	tr204: cs = 142; goto f0;
	tr205: cs = 143; goto f0;
	tr206: cs = 144; goto f0;
	tr196: cs = 145; goto f0;
	tr208: cs = 146; goto f0;
	tr209: cs = 147; goto f0;
	tr210: cs = 148; goto f0;
	tr211: cs = 149; goto f0;
	tr214: cs = 150; goto f0;
	tr194: cs = 150; goto f6;
	tr374: cs = 151; goto f3;
	tr138: cs = 151; goto f7;
	tr215: cs = 152; goto f0;
	tr216: cs = 153; goto f0;
	tr220: cs = 154; goto f0;
	tr219: cs = 154; goto f6;
	tr217: cs = 155; goto f0;
	tr222: cs = 156; goto f0;
	tr296: cs = 156; goto f2;
	tr411: cs = 156; goto f9;
	tr223: cs = 157; goto f7;
	tr412: cs = 157; goto f10;
	tr227: cs = 158; goto f0;
	tr228: cs = 159; goto f0;
	tr230: cs = 160; goto f0;
	tr229: cs = 160; goto f6;
	tr231: cs = 161; goto f7;
	tr420: cs = 161; goto f10;
	tr232: cs = 162; goto f0;
	tr233: cs = 163; goto f0;
	tr235: cs = 164; goto f0;
	tr234: cs = 164; goto f6;
	tr236: cs = 165; goto f0;
	tr345: cs = 165; goto f2;
	tr419: cs = 165; goto f9;
	tr237: cs = 166; goto f7;
	tr421: cs = 166; goto f10;
	tr240: cs = 167; goto f0;
	tr241: cs = 168; goto f0;
	tr243: cs = 169; goto f0;
	tr242: cs = 169; goto f6;
	tr224: cs = 170; goto f7;
	tr413: cs = 170; goto f10;
	tr244: cs = 171; goto f0;
	tr245: cs = 172; goto f0;
	tr247: cs = 173; goto f0;
	tr246: cs = 173; goto f6;
	tr122: cs = 174; goto f0;
	tr78: cs = 175; goto f0;
	tr238: cs = 176; goto f1;
	tr422: cs = 176; goto f11;
	tr250: cs = 177; goto f0;
	tr239: cs = 178; goto f0;
	tr423: cs = 178; goto f9;
	tr253: cs = 179; goto f7;
	tr255: cs = 180; goto f0;
	tr256: cs = 181; goto f0;
	tr259: cs = 182; goto f0;
	tr258: cs = 182; goto f6;
	tr260: cs = 183; goto f7;
	tr261: cs = 184; goto f0;
	tr262: cs = 185; goto f0;
	tr264: cs = 186; goto f0;
	tr263: cs = 186; goto f6;
	tr265: cs = 187; goto f7;
	tr266: cs = 188; goto f0;
	tr267: cs = 189; goto f0;
	tr225: cs = 190; goto f1;
	tr414: cs = 190; goto f11;
	tr270: cs = 191; goto f0;
	tr226: cs = 192; goto f0;
	tr415: cs = 192; goto f9;
	tr273: cs = 193; goto f7;
	tr274: cs = 194; goto f0;
	tr275: cs = 195; goto f0;
	tr278: cs = 196; goto f0;
	tr277: cs = 196; goto f6;
	tr279: cs = 197; goto f7;
	tr280: cs = 198; goto f0;
	tr281: cs = 199; goto f0;
	tr283: cs = 200; goto f0;
	tr282: cs = 200; goto f6;
	tr254: cs = 201; goto f7;
	tr284: cs = 202; goto f0;
	tr285: cs = 203; goto f0;
	tr288: cs = 204; goto f0;
	tr287: cs = 204; goto f6;
	tr286: cs = 205; goto f0;
	tr148: cs = 206; goto f0;
	tr276: cs = 207; goto f0;
	tr272: cs = 208; goto f0;
	tr292: cs = 209; goto f0;
	tr293: cs = 210; goto f0;
	tr294: cs = 211; goto f0;
	tr295: cs = 212; goto f0;
	tr271: cs = 213; goto f0;
	tr297: cs = 214; goto f0;
	tr298: cs = 215; goto f0;
	tr299: cs = 216; goto f0;
	tr300: cs = 217; goto f0;
	tr301: cs = 218; goto f3;
	tr315: cs = 218; goto f7;
	tr303: cs = 219; goto f0;
	tr304: cs = 220; goto f0;
	tr308: cs = 221; goto f0;
	tr307: cs = 221; goto f6;
	tr309: cs = 222; goto f7;
	tr311: cs = 223; goto f0;
	tr312: cs = 224; goto f0;
	tr314: cs = 225; goto f0;
	tr313: cs = 225; goto f6;
	tr316: cs = 226; goto f7;
	tr317: cs = 227; goto f0;
	tr318: cs = 228; goto f0;
	tr320: cs = 229; goto f0;
	tr319: cs = 229; goto f6;
	tr350: cs = 230; goto f3;
	tr321: cs = 230; goto f7;
	tr322: cs = 231; goto f0;
	tr323: cs = 232; goto f0;
	tr327: cs = 233; goto f0;
	tr326: cs = 233; goto f6;
	tr328: cs = 234; goto f7;
	tr329: cs = 235; goto f0;
	tr330: cs = 236; goto f0;
	tr332: cs = 237; goto f0;
	tr331: cs = 237; goto f6;
	tr310: cs = 238; goto f7;
	tr333: cs = 239; goto f0;
	tr334: cs = 240; goto f0;
	tr336: cs = 241; goto f0;
	tr335: cs = 241; goto f6;
	tr141: cs = 242; goto f0;
	tr324: cs = 243; goto f0;
	tr186: cs = 244; goto f0;
	tr177: cs = 245; goto f0;
	tr305: cs = 246; goto f0;
	tr340: cs = 247; goto f0;
	tr269: cs = 247; goto f6;
	tr268: cs = 248; goto f0;
	tr257: cs = 249; goto f0;
	tr252: cs = 250; goto f0;
	tr341: cs = 251; goto f0;
	tr342: cs = 252; goto f0;
	tr343: cs = 253; goto f0;
	tr344: cs = 254; goto f0;
	tr251: cs = 255; goto f0;
	tr346: cs = 256; goto f0;
	tr347: cs = 257; goto f0;
	tr348: cs = 258; goto f0;
	tr349: cs = 259; goto f0;
	tr105: cs = 260; goto f0;
	tr87: cs = 261; goto f0;
	tr182: cs = 262; goto f0;
	tr353: cs = 263; goto f0;
	tr354: cs = 264; goto f0;
	tr355: cs = 265; goto f0;
	tr356: cs = 266; goto f0;
	tr181: cs = 267; goto f0;
	tr358: cs = 268; goto f0;
	tr359: cs = 269; goto f0;
	tr360: cs = 270; goto f0;
	tr361: cs = 271; goto f0;
	tr364: cs = 272; goto f0;
	tr179: cs = 272; goto f6;
	tr136: cs = 273; goto f0;
	tr365: cs = 274; goto f0;
	tr366: cs = 275; goto f0;
	tr367: cs = 276; goto f0;
	tr368: cs = 277; goto f0;
	tr135: cs = 278; goto f0;
	tr370: cs = 279; goto f0;
	tr371: cs = 280; goto f0;
	tr372: cs = 281; goto f0;
	tr373: cs = 282; goto f0;
	tr33: cs = 283; goto f0;
	tr376: cs = 284; goto f0;
	tr88: cs = 284; goto f6;
	tr74: cs = 285; goto f0;
	tr377: cs = 286; goto f0;
	tr378: cs = 287; goto f0;
	tr379: cs = 288; goto f0;
	tr380: cs = 289; goto f0;
	tr73: cs = 290; goto f0;
	tr382: cs = 291; goto f0;
	tr383: cs = 292; goto f0;
	tr384: cs = 293; goto f0;
	tr385: cs = 294; goto f0;
	tr18: cs = 295; goto f0;
	tr41: cs = 296; goto f0;
	tr42: cs = 297; goto f0;
	tr390: cs = 298; goto f0;
	tr389: cs = 298; goto f6;
	tr29: cs = 299; goto f0;
	tr391: cs = 300; goto f0;
	tr392: cs = 301; goto f0;
	tr393: cs = 302; goto f0;
	tr394: cs = 303; goto f0;
	tr28: cs = 304; goto f0;
	tr396: cs = 305; goto f0;
	tr397: cs = 306; goto f0;
	tr398: cs = 307; goto f0;
	tr399: cs = 308; goto f0;
	tr60: cs = 310; goto f4;
	tr45: cs = 310; goto f5;
	tr133: cs = 310; goto f8;
	tr213: cs = 311; goto f4;
	tr158: cs = 311; goto f5;
	tr161: cs = 311; goto f8;
	tr172: cs = 312; goto f4;
	tr149: cs = 312; goto f5;
	tr290: cs = 312; goto f8;
	tr375: cs = 313; goto f4;
	tr218: cs = 313; goto f5;
	tr221: cs = 313; goto f8;
	tr387: cs = 314; goto f4;
	tr123: cs = 314; goto f5;
	tr248: cs = 314; goto f8;
	tr401: cs = 315; goto f4;
	tr34: cs = 315; goto f5;
	tr249: cs = 315; goto f8;
	tr178: cs = 316; goto f5;
	tr289: cs = 316; goto f8;
	tr142: cs = 317; goto f5;
	tr291: cs = 317; goto f8;
	tr351: cs = 318; goto f4;
	tr325: cs = 318; goto f5;
	tr337: cs = 318; goto f8;
	tr363: cs = 319; goto f4;
	tr187: cs = 319; goto f5;
	tr338: cs = 319; goto f8;
	tr302: cs = 320; goto f4;
	tr306: cs = 320; goto f5;
	tr339: cs = 320; goto f8;
	tr102: cs = 321; goto f4;
	tr106: cs = 321; goto f5;
	tr352: cs = 321; goto f8;
	tr15: cs = 322; goto f4;
	tr19: cs = 322; goto f5;
	tr388: cs = 322; goto f8;

f0:
#line 250 "parser.rl"
	{
    //printf( "[%c:%d],", *p, cs );
    switch( cs ){
    case 0: /* non matching state */
      if( parser->prev_state != 12 && parser->prev_state != 7 ){ /* states following a possible end state for a tag */
        if( parser->echobuffer && parser->echobuffer_index != -1 ){
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
#line 250 "parser.rl"
	{
    //printf( "[%c:%d],", *p, cs );
    switch( cs ){
    case 0: /* non matching state */
      if( parser->prev_state != 12 && parser->prev_state != 7 ){ /* states following a possible end state for a tag */
        if( parser->echobuffer && parser->echobuffer_index != -1 ){
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
#line 123 "parser.rl"
	{
    parser->mark = p;
    //debug_string( "begin", p, 1 );
  }
	goto _again;
f9:
#line 250 "parser.rl"
	{
    //printf( "[%c:%d],", *p, cs );
    switch( cs ){
    case 0: /* non matching state */
      if( parser->prev_state != 12 && parser->prev_state != 7 ){ /* states following a possible end state for a tag */
        if( parser->echobuffer && parser->echobuffer_index != -1 ){
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
#line 127 "parser.rl"
	{
//    printf( "finish\n" );
  }
	goto _again;
f3:
#line 250 "parser.rl"
	{
    //printf( "[%c:%d],", *p, cs );
    switch( cs ){
    case 0: /* non matching state */
      if( parser->prev_state != 12 && parser->prev_state != 7 ){ /* states following a possible end state for a tag */
        if( parser->echobuffer && parser->echobuffer_index != -1 ){
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
#line 132 "parser.rl"
	{
    parser->tag_text = parser->mark+1;
    parser->tag_text_length = p - (parser->mark+1);
    parser->mark = p;
  }
	goto _again;
f8:
#line 250 "parser.rl"
	{
    //printf( "[%c:%d],", *p, cs );
    switch( cs ){
    case 0: /* non matching state */
      if( parser->prev_state != 12 && parser->prev_state != 7 ){ /* states following a possible end state for a tag */
        if( parser->echobuffer && parser->echobuffer_index != -1 ){
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
#line 139 "parser.rl"
	{
    /* trim the tag text */
    ltrim_pointer( &(parser->tag_text), p, &(parser->tag_text_length) );
    rtrim_pointer( &(parser->tag_text), p, &(parser->tag_text_length) );

    /* send the start tag and end tag message */
    parser->start_tag_handler( data, parser->tag_text, parser->tag_text_length, parser->attributes, parser->user_data );
    parser->end_tag_handler( data, parser->tag_text, parser->tag_text_length, parser->user_data );

    /* mark the position */
    parser->tag_text = NULL;
    parser->tag_text_length = 0;
    parser->mark = p;

    /* clear out the echo buffer */
    esi_parser_echobuffer_clear( parser );
  }
	goto _again;
f5:
#line 250 "parser.rl"
	{
    //printf( "[%c:%d],", *p, cs );
    switch( cs ){
    case 0: /* non matching state */
      if( parser->prev_state != 12 && parser->prev_state != 7 ){ /* states following a possible end state for a tag */
        if( parser->echobuffer && parser->echobuffer_index != -1 ){
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
#line 158 "parser.rl"
	{
    /* trim tag text */
    ltrim_pointer( &(parser->tag_text), p, &(parser->tag_text_length) );
    rtrim_pointer( &(parser->tag_text), p, &(parser->tag_text_length) );
    
    /* send the start and end tag message */
    parser->start_tag_handler( data, parser->tag_text, parser->tag_text_length, parser->attributes, parser->user_data );

    parser->tag_text = NULL;
    parser->tag_text_length = 0;
    parser->mark = p;
    
    /* clear out the echo buffer */
    esi_parser_echobuffer_clear( parser );
  }
	goto _again;
f6:
#line 250 "parser.rl"
	{
    //printf( "[%c:%d],", *p, cs );
    switch( cs ){
    case 0: /* non matching state */
      if( parser->prev_state != 12 && parser->prev_state != 7 ){ /* states following a possible end state for a tag */
        if( parser->echobuffer && parser->echobuffer_index != -1 ){
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
#line 175 "parser.rl"
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
#line 250 "parser.rl"
	{
    //printf( "[%c:%d],", *p, cs );
    switch( cs ){
    case 0: /* non matching state */
      if( parser->prev_state != 12 && parser->prev_state != 7 ){ /* states following a possible end state for a tag */
        if( parser->echobuffer && parser->echobuffer_index != -1 ){
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
#line 189 "parser.rl"
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
    if( parser->attributes ){
      parser->last->next = attr;
      parser->last = attr;
    }
    else{
      parser->last = parser->attributes = attr;
    }
  }
	goto _again;
f4:
#line 250 "parser.rl"
	{
    //printf( "[%c:%d],", *p, cs );
    switch( cs ){
    case 0: /* non matching state */
      if( parser->prev_state != 12 && parser->prev_state != 7 ){ /* states following a possible end state for a tag */
        if( parser->echobuffer && parser->echobuffer_index != -1 ){
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
#line 218 "parser.rl"
	{

    parser->tag_text = parser->mark;
    parser->tag_text_length = p - parser->mark;

    parser->mark = p;

    ltrim_pointer( &(parser->tag_text), p, &(parser->tag_text_length) );
    rtrim_pointer( &(parser->tag_text), p, &(parser->tag_text_length) );

    parser->start_tag_handler( data, parser->tag_text, parser->tag_text_length, NULL, parser->user_data );

    esi_parser_echobuffer_clear( parser );
  }
	goto _again;
f2:
#line 250 "parser.rl"
	{
    //printf( "[%c:%d],", *p, cs );
    switch( cs ){
    case 0: /* non matching state */
      if( parser->prev_state != 12 && parser->prev_state != 7 ){ /* states following a possible end state for a tag */
        if( parser->echobuffer && parser->echobuffer_index != -1 ){
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
#line 234 "parser.rl"
	{
    /* offset by 2 to account for the </ characters */
    parser->tag_text = parser->mark+2;
    parser->tag_text_length = p - (parser->mark+2);

    parser->mark = p;
    
    ltrim_pointer( &(parser->tag_text), p, &(parser->tag_text_length) );
    rtrim_pointer( &(parser->tag_text), p, &(parser->tag_text_length) );
    
    parser->end_tag_handler( data, parser->tag_text, parser->tag_text_length, parser->user_data );

    esi_parser_echobuffer_clear( parser );
  }
	goto _again;
f11:
#line 250 "parser.rl"
	{
    //printf( "[%c:%d],", *p, cs );
    switch( cs ){
    case 0: /* non matching state */
      if( parser->prev_state != 12 && parser->prev_state != 7 ){ /* states following a possible end state for a tag */
        if( parser->echobuffer && parser->echobuffer_index != -1 ){
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
#line 123 "parser.rl"
	{
    parser->mark = p;
    //debug_string( "begin", p, 1 );
  }
#line 127 "parser.rl"
	{
//    printf( "finish\n" );
  }
	goto _again;
f10:
#line 250 "parser.rl"
	{
    //printf( "[%c:%d],", *p, cs );
    switch( cs ){
    case 0: /* non matching state */
      if( parser->prev_state != 12 && parser->prev_state != 7 ){ /* states following a possible end state for a tag */
        if( parser->echobuffer && parser->echobuffer_index != -1 ){
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
#line 189 "parser.rl"
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
    if( parser->attributes ){
      parser->last->next = attr;
      parser->last = attr;
    }
    else{
      parser->last = parser->attributes = attr;
    }
  }
#line 127 "parser.rl"
	{
//    printf( "finish\n" );
  }
	goto _again;

_again:
	if ( ++p != pe )
		goto _resume;
	_out: {}
	}
#line 434 "parser.rl"

  parser->cs = cs;

  if( cs != esi_start && cs != 0 ){

    /* reached the end and we're not at a termination point save the buffer as overflow */
    if( !parser->overflow_data ){
      // recompute mark, tag_text, attr_key, and attr_value since they all exist within overflow_data
      int mark_offset = compute_offset( parser->mark, data );
      int tag_text_offset = compute_offset( parser->tag_text, data );
      int attr_key_offset = compute_offset( parser->attr_key, data );
      int attr_value_offset = compute_offset( parser->attr_value, data );
      //debug_string( "mark before move", parser->mark, 1 );

      parser->overflow_data = (char*)malloc( sizeof( char ) * length );
      memcpy( parser->overflow_data, data, length );
      parser->overflow_data_size = length;
//      printf( "allocate overflow data: %ld\n", parser->overflow_data_size );

      // in our new memory space mark will now be
      parser->mark = ( mark_offset >= 0 ) ? parser->overflow_data + mark_offset : NULL;
      parser->tag_text = ( tag_text_offset >= 0 ) ? parser->overflow_data + tag_text_offset : NULL;
      parser->attr_key = ( attr_key_offset >= 0 ) ? parser->overflow_data + attr_key_offset : NULL;
      parser->attr_value = ( attr_value_offset >= 0 ) ? parser->overflow_data + attr_value_offset : NULL;
      //if( parser->mark ){ debug_string( "mark after  move", parser->mark, 1 ); } else { printf( "mark is now empty\n" ); }
    }

  }else if( parser->overflow_data ){
    /* dump the overflow buffer execution ended at a final state */
    free( parser->overflow_data );
    parser->overflow_data = NULL;
    parser->overflow_data_size = 0;
  }

  return cs;
}
int esi_parser_finish( ESIParser *parser )
{
  
#line 5268 "parser.c"
#line 473 "parser.rl"
  return 0;
}

void esi_parser_start_tag_handler( ESIParser *parser, start_tag_cb callback )
{
  parser->start_tag_handler = callback;
}

void esi_parser_end_tag_handler( ESIParser *parser, end_tag_cb callback )
{
  parser->end_tag_handler = callback;
}
