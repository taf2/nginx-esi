/** 
 * Copyright (c) 2008 Todd A. Fisher
 */
%%{
  machine esi_common_parser;

  # valid attribute key characters
  attr_key = [a-zA-Z_\-]+ [a-zA-Z_\-0-9]*;

  # attribute values can be contained within either double or single quotes, when single quoted then double quotes are valid
  # when double quoted then single quotes are valid
  attr_valueq1 = ('"' ("\\\""| [^"])* '"');
  attr_valueq2 = ("'" ("\\\'"| [^'])* "'");

  # an attribute value is either single or double quoted 
  attr_value = ( attr_valueq1 | attr_valueq2 );

  # inline tags e.g. <esi:comment text='some useless text'/> or <esi:include src='/foo/'/>, etc...
  # block tags e.g. <esi:try><esi:attempt></esi:attempt></esi:try>
  # NOTE: not verifying the tag nesting
  esi_tags = (
    start: (
      # first match block start tag with no attributes
      '<esi:' [a-z]+ '>' @block_start_tag -> final |
      # next match either an inline or block tag with attributes
      '<esi:' [a-z]+ space @see_start_tag -> start_attribute |
      # finally match a closing block tag
      '</esi:' [a-z]+ '>' @block_end_tag
    ),
    start_attribute: (
      # match the attribute key up to the begining = allowing for arbitrary whitespace before the start of the key
      space* attr_key '=' @see_attribute_key -> end_attribute |
      # match the end of an inline tag, triggers start and end callbacks
      space* '/>' @see_end_tag -> final |
      # matching the end of a block tag, triggers start callback
      '>' @see_block_start_with_attributes -> final
    ),
    end_attribute: (
      # match the attribute value and return to matching attribute start
      space* attr_value @see_attribute_value -> start_attribute
    )
  ) >begin %finish;

  main := ((/.*/)? @echo (esi_tags) )*;
}%%
