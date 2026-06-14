/******************************************************************************
 * Copyright (c) 2026 Jaroslav Hensl <emulator@emulace.cz>                    *
 *                                                                            *
 * Permission is hereby granted, free of charge, to any person obtaining a    *
 * copy of this software and associated documentation files (the "Software"), *
 * to deal in the Software without restriction, including without limitation  *
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,   *
 * and/or sell copies of the Software, and to permit persons to whom the      *
 * Software is furnished to do so, subject to the following conditions:       *
 *                                                                            *
 * The above copyright notice and this permission notice shall be included in *
 * all copies or substantial portions of the Software.                        *
 *                                                                            *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR *
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,   *
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL    *
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER *
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING    *
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER        *
 * DEALINGS IN THE SOFTWARE.                                                  *
 ******************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const char header[] = 
	"format MS COFF\n\n"
	"section '.text' code readable executable\n\n";
	
const char foot[] = 
	"section '.data' data readable writeable\n";

void write_fn(FILE *out, const char *func_name, const char *link_name)
{
	fprintf(out,
		//"extrn '__imp__%s' as %s:dword\n"
		"extrn '__imp__%s' as %s:dword\n"
		"public _hook_%s\n\n",
		link_name, func_name, func_name
	);
	fprintf(out,
		"_hook_%s:\n"
		"    jmp [%s]\n\n",
		func_name, func_name
	);
}

void write_stub(FILE *out, const char *func_name, int stack_args)
{
	fprintf(out,
		"public _hook_%s\n", func_name
	);
	
	if(stack_args != 0)
	{
		fprintf(out,
			"_hook_%s:\n"
			"  ret %d\n\n",
			func_name, stack_args);
	}
	else
	{
		fprintf(out,
			"_hook_%s:\n"
			"  ret\n\n",
			func_name);
	}
}

#define NAME_MAX 128

#define F_IGNORE  1
#define F_HASBODY 2
#define F_STUB    4
#define F_DATA    8
#define F_REDIRECT 16

typedef struct func_list
{
	char name[NAME_MAX];
	char linkname[NAME_MAX];
	int  params;
	unsigned flags;
	struct func_list *next;
} func_list_t;

typedef struct lib_list
{
	char file[NAME_MAX];
	char base[NAME_MAX];
	struct func_list *func;
	struct lib_list *next;
} lib_list_t;

lib_list_t *libs = NULL;

#define FSM_LIB 0
#define FSM_FUNC 1
#define FSM_FUNC_PARAM 2

static char buf[4096];
static char buf_param[4096];

unsigned parse_flags()
{
	unsigned f = 0;
	
	if(strstr(buf_param, "IGNORE") != NULL)    f |= F_IGNORE;
	if(strstr(buf_param, "HASBODY") != NULL)   f |= F_HASBODY;
	if(strstr(buf_param, "STUB") != NULL)      f |= F_STUB;
	if(strstr(buf_param, "DATA") != NULL)      f |= F_DATA;
	if(strstr(buf_param, "REDIRECT") != NULL)  f |= F_REDIRECT;
	
	return f;
}

void parse_list(const char *filename)
{
	FILE *fr = fopen(filename, "rb");
	if(fr)
	{
		int fsm = FSM_LIB;
		int buflen = 0;
		int buf_param_len = 0;
		int c;
		
		do
		{
			c = fgetc(fr);
			switch(c)
			{
				case EOF:
				case '\n':
					if(buflen != 0)
					{
						buf[buflen] = '\0';
						buf_param[buf_param_len] = '\0';
						if(fsm == FSM_LIB)
						{
							lib_list_t *ll = malloc(sizeof(lib_list_t));
							strcpy(ll->file, buf);
							
							char *dot = strrchr(buf, '.');
							if(dot != NULL)
							{
								size_t s = dot - buf;
								memcpy(ll->base, buf, s);
								ll->base[s] = '\0';
							}
							
							ll->next = libs;
							ll->func = NULL;
							libs = ll;
						}
						else if(fsm == FSM_FUNC || fsm == FSM_FUNC_PARAM)
						{
							if(libs == NULL)
							{
								printf("no lib\b");
							}
							else
							{
								unsigned flags = parse_flags();
								
								if((flags & F_IGNORE) == 0)
								{
									func_list_t *fl = malloc(sizeof(func_list_t));
									strcpy(fl->linkname, buf);
	
									char *at = strchr(fl->linkname, '@');
									if(at != NULL)
									{
										size_t name_len = at - fl->linkname;
										memcpy(fl->name, fl->linkname, name_len);
										fl->name[name_len] = '\0';
										fl->params = atoi(at+1);
									}
									else
									{
										strcpy(fl->name, fl->linkname);
										fl->params = 0;
									}
									
									//strcpy(fl->linkname, buf);
									fl->flags = flags;
									fl->next = libs->func;
									libs->func = fl;
								} // !IGNORE
							}
						}
					}
					buflen = 0;
					buf_param_len = 0;
					fsm = FSM_LIB;
					break;
				case '\r':
					/* ignore */
					break;
				case ' ':
				case '\t':
					if(buflen == 0)
					{
						fsm = FSM_FUNC;
					}
					else
					{
						if(fsm == FSM_LIB)
						{
							buf[buflen++] = c;
						}
						else if(fsm == FSM_FUNC)
						{
							fsm = FSM_FUNC_PARAM;
						}
						else if(fsm == FSM_FUNC_PARAM)
						{
							buf_param[buf_param_len++] = c;
						}
					}
					break;
				default:
					if(fsm == FSM_FUNC_PARAM)
					{
						buf_param[buf_param_len++] = c;
					}
					else
					{
						buf[buflen++] = c;
					}
					break;
			}
		} while(c != EOF);
		
		fclose(fr);
	}
	else
	{
		printf("cannot open %s\n", filename);
	}
}

int main(int argc, char *argv[])
{
	if(argc < 2)
	{
		printf("usage: %s <list.txt>\n", argv[0]);
		return EXIT_FAILURE;
	}
	
	parse_list(argv[1]);
	
	FILE *fw = stdout;
	if(argc > 2)
	{
		fw = fopen(argv[2], "wb");
	}
	
	fputs(header, fw);
	
	lib_list_t *a;
	func_list_t *b;
	for(a = libs; a != NULL; a = a->next)
	{
		//printf("LIB: %s\n", a->file);
		for(b = a->func; b != NULL; b = b->next)
		{
			//printf("%s\n", b->linkname);
			if((b->flags & F_STUB) != 0)
			{
				write_stub(fw, b->name, b->params);
			}
			else if((b->flags & (F_HASBODY | F_REDIRECT | F_DATA)) == 0)
			{
				write_fn(fw, b->name, b->linkname);
			}
		}
	}
	
	fputs(foot, fw);
	
	if(fw != stdout)
	{
		fclose(fw);
	}
	
	FILE *def = NULL;
	if(argc > 3)
	{
		def = fopen(argv[3], "wb");
		if(def)
		{
			fputs("EXPORTS\n", def);
			for(a = libs; a != NULL; a = a->next)
			{
				for(b = a->func; b != NULL; b = b->next)
				{
					if((b->flags & (F_HASBODY | F_DATA | F_REDIRECT)) == 0)
					{
						fprintf(def, "\t%s = hook_%s\n", b->name, b->name);
					}
					else if((b->flags & (F_DATA | F_REDIRECT)) == (F_DATA | F_REDIRECT))
					{
						fprintf(def, "\t%s = %s.%s DATA\n", b->name, a->base, b->name);
					}
					else if((b->flags & F_REDIRECT) != 0)
					{
						fprintf(def, "\t%s = %s.%s\n", b->name, a->base, b->name);
					}
				}
			}
			fclose(def);
		}
	}
	
	return EXIT_SUCCESS;
}
