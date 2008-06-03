/**
 * Copyright (c) 2007 Todd A. Fisher
 * You can redistribute it and/or modify it under the same terms as Mozilla Public License 1.1.
 */
#ifndef ESI_PARSER_H
#define ESI_PARSER_H
#include <sys/types.h>

char *esi_strndup( const char *str, size_t len );

/* 
 * ESI Attribute is a single attribute with name and value
 *
 * e.g. for an esi include tag:
 *
 *  <esi:include src='/foo/bar/' timeout='10'/>
 *
 * 2 attributes would be allocated
 *
 * attrs[0]->name => 'src'
 * attrs[0]->value => '/foo/bar/'
 *
 * attrs[1]->name => 'timeout'
 * attrs[1]->value => '10'
 *
 * */
typedef struct _ESIAttr {
  char *name;
  char *value;
  struct _ESIAttr *next;
}ESIAttribute;

ESIAttribute *esi_attribute_new( const char *name, size_t name_length, const char *value, size_t value_length );
ESIAttribute *esi_attribute_copy( ESIAttribute *attribute );
void esi_attribute_free( ESIAttribute *attribute );

typedef void (*esi_start_tag_cb)(const void *data,
                             const char *name_start,
                             size_t name_length,
                             ESIAttribute *attributes,
                             void *user_data);

typedef void (*esi_end_tag_cb)(const void *data,
                           const char *name_start,
                           size_t name_length,
                           void *user_data);

typedef void (*esi_output_cb)(const void *data,
                          size_t length,
                          void *user_data);

typedef struct _ESIParser {
  int cs;
  int prev_state;

  void *user_data;

  const char *mark;
  size_t overflow_data_size; /* current size of the overflow data buffer */
  char *overflow_data; /* overflow buffer if execute finishes and we are not in a final state store the parse data */

  size_t echobuffer_allocated; /* amount of memory allocated for the echobuffer */
  size_t echobuffer_index; /* current write position of the last echo'ed character */
  char *echobuffer; /* echo buffer if the parse state is not 0 we store the characters here */

  const char *tag_text;  /* start pointer in data */
  size_t tag_text_length; /* length from tag_text within data */

  const char *attr_key; /* start pointer in data */
  size_t attr_key_length;

  const char *attr_value; /* start pointer in data */
  size_t attr_value_length;

  ESIAttribute *attributes, *last;

  esi_start_tag_cb start_tag_handler;
  esi_end_tag_cb end_tag_handler;
  esi_output_cb output_handler;

} ESIParser;

/* create a new Edge Side Include Parser */
ESIParser *esi_parser_new();
void esi_parser_free( ESIParser *parser );

/* initialize the parser */
int esi_parser_init( ESIParser *parser );

/* 
 * send a chunk of data to the parser, the internal parser state is returned
 */
int esi_parser_execute( ESIParser *parser, const char *data, size_t length );
/*
 * let the parser know that it has reached the end and it should flush any remaining data to the desired output device
 */
int esi_parser_finish( ESIParser *parser );

/* 
 * setup a callback to execute when a new esi: start tag is encountered
 * this will fire for all block tags e.g. <esi:try>, <esi:attempt> and also
 * inline tags <esi:inline src='cache-key'/> <esi:include src='dest'/>
 */
void esi_parser_start_tag_handler( ESIParser *parser, esi_start_tag_cb callback );

void esi_parser_end_tag_handler( ESIParser *parser, esi_end_tag_cb callback );

/* setup a callback to recieve data ready for output */
void esi_parser_output_handler( ESIParser *parser, esi_output_cb output_handler );


#endif
