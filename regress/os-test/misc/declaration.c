/*
 * Copyright (c) 2025, 2026 Jonas 'Sortie' Termansen.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * declaration.c
 * Parse machine readable .api files.
 */

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum type
{
	TYPE_DEFINITION,
	TYPE_FUNCTION,
	TYPE_GENERIC,
	TYPE_EXTERNAL,
	TYPE_SYMBOLIC_CONSTANT,
	TYPE_ENUMERATION,
	TYPE_ENUMERATION_MEMBER,
	TYPE_UNION,
	TYPE_UNION_MEMBER,
	TYPE_STRUCTURE,
	TYPE_STRUCTURE_MEMBER,
	TYPE_TYPE,
	TYPE_EXPRESSION,
	TYPE_INCLUDE,
	TYPE_NAMESPACE,
	TYPE_COUNT,
	TYPE_FIRST = TYPE_DEFINITION,
};

const char* type_names[] =
{
        [TYPE_DEFINITION] = "define",
        [TYPE_FUNCTION] = "function",
        [TYPE_GENERIC] = "generic",
        [TYPE_EXTERNAL] = "external",
        [TYPE_SYMBOLIC_CONSTANT] = "symbolic_constant",
        [TYPE_ENUMERATION] = "enum",
        [TYPE_ENUMERATION_MEMBER] = "enum_member",
        [TYPE_UNION] = "union",
        [TYPE_UNION_MEMBER] = "union_member",
        [TYPE_STRUCTURE] = "struct",
        [TYPE_STRUCTURE_MEMBER] = "struct_member",
        [TYPE_TYPE] = "typedef",
        [TYPE_EXPRESSION] = "expression",
        [TYPE_INCLUDE] = "include",
        [TYPE_NAMESPACE] = "namespace",
};

struct declaration
{
	int type_mask;
	char* name;
	char* sig;
	char* parent;
	char* options;
	bool optional;
	bool incomplete;
};

#define REQUIRED_TYPE(type) (1 << (type))
#define OPTIONAL_TYPE(type) (1 << ((type) + TYPE_COUNT))

static bool is_identifier(char c)
{
	return ('a' <= c && c <= 'z') || ('A' <= c && c <= 'Z') ||
	       ('0' <= c && c <= '9') || c == '_';
}

static bool next_is_token(const char* input, const char* token)
{
	return !strncmp(input, token, strlen(token)) &&
	       (!input[strlen(token)] ||
	        isspace((unsigned char) input[strlen(token)]) ||
	        (is_identifier(input[strlen(token)])) !=
	         is_identifier(token[0]));
}

struct declaration* parse_declaration(const char* input)
{
	struct declaration* declaration = calloc(1, sizeof(struct declaration));
	if ( !declaration )
		abort();
	while ( isspace((unsigned char) input[0]) )
		input++;
	if ( input[0] == '[' )
	{
		input++;
		size_t length = 0;
		while ( input[length] && input[length] != ']' )
			length++;
		if ( input[length] != ']' )
			abort();
		if ( !(declaration->options = strndup(input, length)) )
			abort();
		input += length + 1;
		while ( isspace((unsigned char) input[0]) )
			input++;
	}
	if ( next_is_token(input, "optional") )
	{
		declaration->optional = true;
		input += strlen("optional");
		while ( isspace((unsigned char) input[0]) )
			input++;
	}
	if ( next_is_token(input, "incomplete") )
	{
		declaration->incomplete = true;
		input += strlen("incomplete");
		while ( isspace((unsigned char) input[0]) )
			input++;
	}
	if ( next_is_token(input, "parent") )
	{
		input += strlen("parent");
		while ( isspace((unsigned char) input[0]) )
			input++;
		size_t length = 0;
		if ( next_is_token(input, "struct") )
			length += strlen("struct");
		else if ( next_is_token(input, "union") )
			length += strlen("union");
		while ( isspace((unsigned char) input[length]) )
			length++;
		while ( input[length] && !isspace((unsigned char) input[length]) )
			length++;
		if ( !length )
			abort();
		if ( !(declaration->parent = strndup(input, length)) )
			abort();
		input += length;
		while ( isspace((unsigned char) input[0]) )
			input++;
	}
	for ( enum type type = TYPE_FIRST; type < TYPE_COUNT; type++ )
	{
		if ( !strncmp(input, "maybe_", strlen("maybe_")) &&
		     next_is_token(input + strlen("maybe_"), type_names[type]) )
		{
			input += strlen("maybe_") + strlen(type_names[type]);
			declaration->type_mask |= OPTIONAL_TYPE(type);
		}
		else if ( next_is_token(input, type_names[type]) )
		{
			input += strlen(type_names[type]);
			declaration->type_mask |= REQUIRED_TYPE(type);
		}
		while ( isspace((unsigned char) input[0]) )
			input++;
	}
	if ( !declaration->type_mask )
		abort();
	size_t length = 0;
	while ( input[length] && !isspace((unsigned char) input[length]) &&
	        input[length] != ';' && input[length] != ':' )
		length++;
	if ( !length )
		abort();
	if ( !(declaration->name = strndup(input, length)) )
		abort();
	input += length;
	while ( isspace((unsigned char) input[0]) )
		input++;
	if ( input[0] == ':' )
	{
		input++;
		while ( isspace((unsigned char) input[0]) )
			input++;
		length = 0;
		while ( input[length] && input[length] != ';' )
			length++;
		if ( input[length] != ';' )
			abort();
		if ( !(declaration->sig = strndup(input, length)) )
			abort();
		input += length;
	}
	else if ( !(declaration->sig = strdup(declaration->name)) )
		abort();
	if ( strcmp(input, ";") != 0 )
		abort();
	return declaration;
}

void output_declaration(struct declaration* declaration, FILE* fp)
{
	if ( declaration->options )
		fprintf(fp, "[%s] ", declaration->options);
	if ( declaration->optional )
		fprintf(fp, "optional ");
	if ( declaration->incomplete )
		fprintf(fp, "incomplete ");
	if ( declaration->parent )
		fprintf(fp, "parent %s ", declaration->parent);
	const char* s = "";
	for ( enum type type = TYPE_FIRST; type < TYPE_COUNT; type++ )
	{
		if ( declaration->type_mask & OPTIONAL_TYPE(type) )
			fprintf(fp, "%smaybe_%s", s, type_names[type]),
			s = " ";
		else if ( declaration->type_mask & REQUIRED_TYPE(type) )
			fprintf(fp, "%s%s", s, type_names[type]),
			s = " ";
	}
	if ( declaration->type_mask == REQUIRED_TYPE(TYPE_STRUCTURE) ||
	     declaration->type_mask == REQUIRED_TYPE(TYPE_UNION) )
		fprintf(fp, " %s;\n", declaration->name);
	else if ( declaration->type_mask == REQUIRED_TYPE(TYPE_NAMESPACE) )
		fprintf(fp, " %s;\n", declaration->sig);
	else
	{
	     if ( strcmp(declaration->name, declaration->sig) != 0 )
			fprintf(fp, " %s:", declaration->name);
		fprintf(fp, " %s;\n", declaration->sig);
	}
}
