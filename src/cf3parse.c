/* A Bison parser, made by GNU Bison 2.3.  */

/* Skeleton implementation for Bison's Yacc-like parsers in C

   Copyright (C) 1984, 1989, 1990, 2000, 2001, 2002, 2003, 2004, 2005, 2006
   Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.  */

/* As a special exception, you may create a larger work that contains
   part or all of the Bison parser skeleton and distribute that work
   under terms of your choice, so long as that work isn't itself a
   parser generator using the skeleton or a modified version thereof
   as a parser skeleton.  Alternatively, if you modify or redistribute
   the parser skeleton itself, you may (at your option) remove this
   special exception, which will cause the skeleton and the resulting
   Bison output files to be licensed under the GNU General Public
   License without this special exception.

   This special exception was added by the Free Software Foundation in
   version 2.2 of Bison.  */

/* C LALR(1) parser skeleton written by Richard Stallman, by
   simplifying the original so-called "semantic" parser.  */

/* All symbols defined below should begin with yy or YY, to avoid
   infringing on user name space.  This should be done even for local
   variables, as they might otherwise be expanded by user macros.
   There are some unavoidable exceptions within include files to
   define necessary library symbols; they are noted "INFRINGES ON
   USER NAME SPACE" below.  */

/* Identify Bison output.  */
#define YYBISON 1

/* Bison version.  */
#define YYBISON_VERSION "2.3"

/* Skeleton name.  */
#define YYSKELETON_NAME "yacc.c"

/* Pure parsers.  */
#define YYPURE 0

/* Using locations.  */
#define YYLSP_NEEDED 0



/* Tokens.  */
#ifndef YYTOKENTYPE
# define YYTOKENTYPE
   /* Put the tokens into the symbol table, so that GDB and other debuggers
      know about them.  */
   enum yytokentype {
     ID = 258,
     QSTRING = 259,
     CLASS = 260,
     CATEGORY = 261,
     BUNDLE = 262,
     BODY = 263,
     ASSIGN = 264,
     ARROW = 265,
     NAKEDVAR = 266
   };
#endif
/* Tokens.  */
#define ID 258
#define QSTRING 259
#define CLASS 260
#define CATEGORY 261
#define BUNDLE 262
#define BODY 263
#define ASSIGN 264
#define ARROW 265
#define NAKEDVAR 266




/* Copy the first part of user declarations.  */
#line 2 "cf3parse.y"


/*******************************************************************/
/*                                                                 */
/*  PARSER for cfengine 3                                          */
/*                                                                 */
/*******************************************************************/

#include "cf3.defs.h"
#include "cf3.extern.h"

extern char *yytext;



/* Enabling traces.  */
#ifndef YYDEBUG
# define YYDEBUG 0
#endif

/* Enabling verbose error messages.  */
#ifdef YYERROR_VERBOSE
# undef YYERROR_VERBOSE
# define YYERROR_VERBOSE 1
#else
# define YYERROR_VERBOSE 0
#endif

/* Enabling the token table.  */
#ifndef YYTOKEN_TABLE
# define YYTOKEN_TABLE 0
#endif

#if ! defined YYSTYPE && ! defined YYSTYPE_IS_DECLARED
typedef int YYSTYPE;
# define yystype YYSTYPE /* obsolescent; will be withdrawn */
# define YYSTYPE_IS_DECLARED 1
# define YYSTYPE_IS_TRIVIAL 1
#endif



/* Copy the second part of user declarations.  */


/* Line 216 of yacc.c.  */
#line 143 "y.tab.c"

#ifdef short
# undef short
#endif

#ifdef YYTYPE_UINT8
typedef YYTYPE_UINT8 yytype_uint8;
#else
typedef unsigned char yytype_uint8;
#endif

#ifdef YYTYPE_INT8
typedef YYTYPE_INT8 yytype_int8;
#elif (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
typedef signed char yytype_int8;
#else
typedef short int yytype_int8;
#endif

#ifdef YYTYPE_UINT16
typedef YYTYPE_UINT16 yytype_uint16;
#else
typedef unsigned short int yytype_uint16;
#endif

#ifdef YYTYPE_INT16
typedef YYTYPE_INT16 yytype_int16;
#else
typedef short int yytype_int16;
#endif

#ifndef YYSIZE_T
# ifdef __SIZE_TYPE__
#  define YYSIZE_T __SIZE_TYPE__
# elif defined size_t
#  define YYSIZE_T size_t
# elif ! defined YYSIZE_T && (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
#  include <stddef.h> /* INFRINGES ON USER NAME SPACE */
#  define YYSIZE_T size_t
# else
#  define YYSIZE_T unsigned int
# endif
#endif

#define YYSIZE_MAXIMUM ((YYSIZE_T) -1)

#ifndef YY_
# if YYENABLE_NLS
#  if ENABLE_NLS
#   include <libintl.h> /* INFRINGES ON USER NAME SPACE */
#   define YY_(msgid) dgettext ("bison-runtime", msgid)
#  endif
# endif
# ifndef YY_
#  define YY_(msgid) msgid
# endif
#endif

/* Suppress unused-variable warnings by "using" E.  */
#if ! defined lint || defined __GNUC__
# define YYUSE(e) ((void) (e))
#else
# define YYUSE(e) /* empty */
#endif

/* Identity function, used to suppress warnings about constant conditions.  */
#ifndef lint
# define YYID(n) (n)
#else
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static int
YYID (int i)
#else
static int
YYID (i)
    int i;
#endif
{
  return i;
}
#endif

#if ! defined yyoverflow || YYERROR_VERBOSE

/* The parser invokes alloca or malloc; define the necessary symbols.  */

# ifdef YYSTACK_USE_ALLOCA
#  if YYSTACK_USE_ALLOCA
#   ifdef __GNUC__
#    define YYSTACK_ALLOC __builtin_alloca
#   elif defined __BUILTIN_VA_ARG_INCR
#    include <alloca.h> /* INFRINGES ON USER NAME SPACE */
#   elif defined _AIX
#    define YYSTACK_ALLOC __alloca
#   elif defined _MSC_VER
#    include <malloc.h> /* INFRINGES ON USER NAME SPACE */
#    define alloca _alloca
#   else
#    define YYSTACK_ALLOC alloca
#    if ! defined _ALLOCA_H && ! defined _STDLIB_H && (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
#     include <stdlib.h> /* INFRINGES ON USER NAME SPACE */
#     ifndef _STDLIB_H
#      define _STDLIB_H 1
#     endif
#    endif
#   endif
#  endif
# endif

# ifdef YYSTACK_ALLOC
   /* Pacify GCC's `empty if-body' warning.  */
#  define YYSTACK_FREE(Ptr) do { /* empty */; } while (YYID (0))
#  ifndef YYSTACK_ALLOC_MAXIMUM
    /* The OS might guarantee only one guard page at the bottom of the stack,
       and a page size can be as small as 4096 bytes.  So we cannot safely
       invoke alloca (N) if N exceeds 4096.  Use a slightly smaller number
       to allow for a few compiler-allocated temporary stack slots.  */
#   define YYSTACK_ALLOC_MAXIMUM 4032 /* reasonable circa 2006 */
#  endif
# else
#  define YYSTACK_ALLOC YYMALLOC
#  define YYSTACK_FREE YYFREE
#  ifndef YYSTACK_ALLOC_MAXIMUM
#   define YYSTACK_ALLOC_MAXIMUM YYSIZE_MAXIMUM
#  endif
#  if (defined __cplusplus && ! defined _STDLIB_H \
       && ! ((defined YYMALLOC || defined malloc) \
	     && (defined YYFREE || defined free)))
#   include <stdlib.h> /* INFRINGES ON USER NAME SPACE */
#   ifndef _STDLIB_H
#    define _STDLIB_H 1
#   endif
#  endif
#  ifndef YYMALLOC
#   define YYMALLOC malloc
#   if ! defined malloc && ! defined _STDLIB_H && (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
void *malloc (YYSIZE_T); /* INFRINGES ON USER NAME SPACE */
#   endif
#  endif
#  ifndef YYFREE
#   define YYFREE free
#   if ! defined free && ! defined _STDLIB_H && (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
void free (void *); /* INFRINGES ON USER NAME SPACE */
#   endif
#  endif
# endif
#endif /* ! defined yyoverflow || YYERROR_VERBOSE */


#if (! defined yyoverflow \
     && (! defined __cplusplus \
	 || (defined YYSTYPE_IS_TRIVIAL && YYSTYPE_IS_TRIVIAL)))

/* A type that is properly aligned for any stack member.  */
union yyalloc
{
  yytype_int16 yyss;
  YYSTYPE yyvs;
  };

/* The size of the maximum gap between one aligned stack and the next.  */
# define YYSTACK_GAP_MAXIMUM (sizeof (union yyalloc) - 1)

/* The size of an array large to enough to hold all stacks, each with
   N elements.  */
# define YYSTACK_BYTES(N) \
     ((N) * (sizeof (yytype_int16) + sizeof (YYSTYPE)) \
      + YYSTACK_GAP_MAXIMUM)

/* Copy COUNT objects from FROM to TO.  The source and destination do
   not overlap.  */
# ifndef YYCOPY
#  if defined __GNUC__ && 1 < __GNUC__
#   define YYCOPY(To, From, Count) \
      __builtin_memcpy (To, From, (Count) * sizeof (*(From)))
#  else
#   define YYCOPY(To, From, Count)		\
      do					\
	{					\
	  YYSIZE_T yyi;				\
	  for (yyi = 0; yyi < (Count); yyi++)	\
	    (To)[yyi] = (From)[yyi];		\
	}					\
      while (YYID (0))
#  endif
# endif

/* Relocate STACK from its old location to the new one.  The
   local variables YYSIZE and YYSTACKSIZE give the old and new number of
   elements in the stack, and YYPTR gives the new location of the
   stack.  Advance YYPTR to a properly aligned location for the next
   stack.  */
# define YYSTACK_RELOCATE(Stack)					\
    do									\
      {									\
	YYSIZE_T yynewbytes;						\
	YYCOPY (&yyptr->Stack, Stack, yysize);				\
	Stack = &yyptr->Stack;						\
	yynewbytes = yystacksize * sizeof (*Stack) + YYSTACK_GAP_MAXIMUM; \
	yyptr += yynewbytes / sizeof (*yyptr);				\
      }									\
    while (YYID (0))

#endif

/* YYFINAL -- State number of the termination state.  */
#define YYFINAL  8
/* YYLAST -- Last index in YYTABLE.  */
#define YYLAST   80

/* YYNTOKENS -- Number of terminals.  */
#define YYNTOKENS  18
/* YYNNTS -- Number of nonterminals.  */
#define YYNNTS  44
/* YYNRULES -- Number of rules.  */
#define YYNRULES  76
/* YYNRULES -- Number of states.  */
#define YYNSTATES  105

/* YYTRANSLATE(YYLEX) -- Bison symbol number corresponding to YYLEX.  */
#define YYUNDEFTOK  2
#define YYMAXUTOK   266

#define YYTRANSLATE(YYX)						\
  ((unsigned int) (YYX) <= YYMAXUTOK ? yytranslate[YYX] : YYUNDEFTOK)

/* YYTRANSLATE[YYLEX] -- Bison symbol number corresponding to YYLEX.  */
static const yytype_uint8 yytranslate[] =
{
       0,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
      12,    13,     2,     2,    14,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,    17,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,    15,     2,    16,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     1,     2,     3,     4,
       5,     6,     7,     8,     9,    10,    11
};

#if YYDEBUG
/* YYPRHS[YYN] -- Index of the first RHS symbol of rule number YYN in
   YYRHS.  */
static const yytype_uint8 yyprhs[] =
{
       0,     0,     3,     4,     6,     8,    11,    16,    22,    27,
      33,    35,    37,    39,    41,    45,    47,    51,    52,    54,
      55,    60,    62,    65,    67,    69,    70,    75,    77,    80,
      82,    84,    86,    89,    90,    96,    98,   101,   103,   105,
     107,   110,   112,   113,   120,   121,   126,   128,   132,   133,
     137,   139,   141,   143,   145,   147,   149,   151,   155,   157,
     161,   162,   164,   166,   168,   170,   172,   174,   177,   178,
     183,   185,   189,   190,   192,   194,   196
};

/* YYRHS -- A `-1'-separated list of the rules' RHS.  */
static const yytype_int8 yyrhs[] =
{
      19,     0,    -1,    -1,    20,    -1,    21,    -1,    20,    21,
      -1,    22,    24,    25,    29,    -1,    22,    24,    25,    26,
      29,    -1,    23,    24,    25,    33,    -1,    23,    24,    25,
      26,    33,    -1,     7,    -1,     8,    -1,     3,    -1,     3,
      -1,    12,    27,    13,    -1,    28,    -1,    28,    14,    27,
      -1,    -1,     3,    -1,    -1,    15,    30,    31,    16,    -1,
      32,    -1,    31,    32,    -1,    43,    -1,    40,    -1,    -1,
      15,    34,    35,    16,    -1,    36,    -1,    35,    36,    -1,
      49,    -1,    37,    -1,    38,    -1,    37,    38,    -1,    -1,
      50,     9,    51,    39,    17,    -1,    41,    -1,    40,    41,
      -1,    49,    -1,    42,    -1,    44,    -1,    42,    44,    -1,
       6,    -1,    -1,    56,    10,    51,    45,    47,    17,    -1,
      -1,    56,    46,    47,    17,    -1,    48,    -1,    47,    14,
      48,    -1,    -1,    50,     9,    51,    -1,     5,    -1,     3,
      -1,     3,    -1,     4,    -1,    11,    -1,    52,    -1,    57,
      -1,    15,    53,    16,    -1,    54,    -1,    54,    14,    53,
      -1,    -1,     3,    -1,     4,    -1,    11,    -1,    57,    -1,
       3,    -1,     4,    -1,    55,    58,    -1,    -1,    12,    59,
      60,    13,    -1,    61,    -1,    60,    14,    61,    -1,    -1,
       3,    -1,     4,    -1,    11,    -1,    57,    -1
};

/* YYRLINE[YYN] -- source line where rule number YYN was defined.  */
static const yytype_uint16 yyrline[] =
{
       0,    21,    21,    22,    26,    27,    31,    32,    33,    34,
      38,    48,    56,    64,    71,    77,    78,    79,    83,    92,
      91,   118,   119,   123,   124,   129,   128,   149,   150,   154,
     155,   159,   160,   167,   164,   204,   205,   209,   210,   214,
     215,   219,   237,   232,   262,   261,   289,   290,   291,   295,
     314,   321,   330,   337,   344,   351,   357,   366,   372,   373,
     374,   378,   384,   390,   396,   405,   412,   420,   428,   427,
     453,   454,   455,   459,   466,   473,   480
};
#endif

#if YYDEBUG || YYERROR_VERBOSE || YYTOKEN_TABLE
/* YYTNAME[SYMBOL-NUM] -- String name of the symbol SYMBOL-NUM.
   First, the terminals, then, starting at YYNTOKENS, nonterminals.  */
static const char *const yytname[] =
{
  "$end", "error", "$undefined", "ID", "QSTRING", "CLASS", "CATEGORY",
  "BUNDLE", "BODY", "ASSIGN", "ARROW", "NAKEDVAR", "'('", "')'", "','",
  "'{'", "'}'", "';'", "$accept", "specification", "blocks", "block",
  "bundle", "body", "typeid", "blockid", "usearglist", "aitems", "aitem",
  "bundlebody", "@1", "statements", "statement", "bodybody", "@2",
  "bodyattribs", "bodyattrib", "selections", "selection", "@3",
  "classpromises", "classpromise", "promises", "category", "promise", "@4",
  "@5", "constraints", "constraint", "class", "id", "rval", "list",
  "litems", "litem", "functionid", "promiser", "usefunction",
  "givearglist", "@6", "gaitems", "gaitem", 0
};
#endif

# ifdef YYPRINT
/* YYTOKNUM[YYLEX-NUM] -- Internal token number corresponding to
   token YYLEX-NUM.  */
static const yytype_uint16 yytoknum[] =
{
       0,   256,   257,   258,   259,   260,   261,   262,   263,   264,
     265,   266,    40,    41,    44,   123,   125,    59
};
# endif

/* YYR1[YYN] -- Symbol number of symbol that rule YYN derives.  */
static const yytype_uint8 yyr1[] =
{
       0,    18,    19,    19,    20,    20,    21,    21,    21,    21,
      22,    23,    24,    25,    26,    27,    27,    27,    28,    30,
      29,    31,    31,    32,    32,    34,    33,    35,    35,    36,
      36,    37,    37,    39,    38,    40,    40,    41,    41,    42,
      42,    43,    45,    44,    46,    44,    47,    47,    47,    48,
      49,    50,    51,    51,    51,    51,    51,    52,    53,    53,
      53,    54,    54,    54,    54,    55,    56,    57,    59,    58,
      60,    60,    60,    61,    61,    61,    61
};

/* YYR2[YYN] -- Number of symbols composing right hand side of rule YYN.  */
static const yytype_uint8 yyr2[] =
{
       0,     2,     0,     1,     1,     2,     4,     5,     4,     5,
       1,     1,     1,     1,     3,     1,     3,     0,     1,     0,
       4,     1,     2,     1,     1,     0,     4,     1,     2,     1,
       1,     1,     2,     0,     5,     1,     2,     1,     1,     1,
       2,     1,     0,     6,     0,     4,     1,     3,     0,     3,
       1,     1,     1,     1,     1,     1,     1,     3,     1,     3,
       0,     1,     1,     1,     1,     1,     1,     2,     0,     4,
       1,     3,     0,     1,     1,     1,     1
};

/* YYDEFACT[STATE-NAME] -- Default rule to reduce with in state
   STATE-NUM when YYTABLE doesn't specify something else to do.  Zero
   means the default is an error.  */
static const yytype_uint8 yydefact[] =
{
       2,    10,    11,     0,     3,     4,     0,     0,     1,     5,
      12,     0,     0,    13,     0,     0,    17,    19,     0,     6,
      25,     0,     8,    18,     0,    15,     0,     7,     0,     9,
      14,    17,    66,    50,    41,     0,    21,    24,    35,    38,
      23,    39,    37,    44,    51,     0,    27,    30,    31,    29,
       0,    16,    20,    22,    36,    40,     0,    48,    26,    28,
      32,     0,    52,    53,    54,    60,    42,    55,     0,    56,
       0,    46,     0,    33,    61,    62,    63,     0,    58,    64,
      48,    68,    67,     0,    45,     0,     0,    57,    60,     0,
      72,    47,    49,    34,    59,    43,    73,    74,    75,    76,
       0,    70,    69,     0,    71
};

/* YYDEFGOTO[NTERM-NUM].  */
static const yytype_int8 yydefgoto[] =
{
      -1,     3,     4,     5,     6,     7,    11,    14,    18,    24,
      25,    19,    26,    35,    36,    22,    28,    45,    46,    47,
      48,    86,    37,    38,    39,    40,    41,    80,    57,    70,
      71,    42,    50,    66,    67,    77,    78,    68,    43,    69,
      82,    90,   100,   101
};

/* YYPACT[STATE-NUM] -- Index in YYTABLE of the portion describing
   STATE-NUM.  */
#define YYPACT_NINF -66
static const yytype_int8 yypact[] =
{
      40,   -66,   -66,     5,    40,   -66,    13,    13,   -66,   -66,
     -66,    29,    29,   -66,     0,    18,    32,   -66,    24,   -66,
     -66,    30,   -66,   -66,    42,    39,    37,   -66,    41,   -66,
     -66,    32,   -66,   -66,   -66,     3,   -66,    45,   -66,    50,
     -66,   -66,   -66,    46,   -66,     1,   -66,    54,   -66,   -66,
      49,   -66,   -66,   -66,   -66,   -66,     7,    54,   -66,   -66,
     -66,     7,    47,   -66,   -66,    10,   -66,   -66,    48,   -66,
      17,   -66,    52,   -66,    47,   -66,   -66,    51,    55,   -66,
      54,   -66,   -66,    54,   -66,     7,    53,   -66,    10,    23,
      25,   -66,   -66,   -66,   -66,   -66,    47,   -66,   -66,   -66,
      38,   -66,   -66,    25,   -66
};

/* YYPGOTO[NTERM-NUM].  */
static const yytype_int8 yypgoto[] =
{
     -66,   -66,   -66,    58,   -66,   -66,    56,    59,    57,    33,
     -66,    60,   -66,   -66,    31,    44,   -66,   -66,    28,   -66,
      21,   -66,   -66,    43,   -66,   -66,    35,   -66,   -66,    -5,
      -7,   -25,   -56,   -59,   -66,   -11,   -66,   -66,   -66,   -65,
     -66,   -66,   -66,   -24
};

/* YYTABLE[YYPACT[STATE-NUM]].  What to do in state STATE-NUM.  If
   positive, shift that token.  If negative, reduce the rule which
   number is the opposite.  If zero, do what YYDEFACT says.
   If YYTABLE_NINF, syntax error.  */
#define YYTABLE_NINF -66
static const yytype_int8 yytable[] =
{
      79,    72,    73,    49,    44,     8,    33,    32,    33,    34,
      62,    63,    16,    74,    75,    17,    10,    58,    64,    52,
      49,    76,    65,    79,    72,    99,    92,    72,    96,    97,
      16,    83,    13,    20,    84,    23,    98,    83,    99,    17,
      95,    32,    33,    34,    44,    20,    33,     1,     2,    32,
      33,   102,   103,    31,    32,    30,    56,    44,    61,   -65,
      81,    85,     9,    12,    51,    29,    53,    87,    60,    88,
      93,    15,    21,    59,    55,    89,    91,    94,    27,   104,
      54
};

static const yytype_uint8 yycheck[] =
{
      65,    57,    61,    28,     3,     0,     5,     4,     5,     6,
       3,     4,    12,     3,     4,    15,     3,    16,    11,    16,
      45,    11,    15,    88,    80,    90,    85,    83,     3,     4,
      12,    14,     3,    15,    17,     3,    11,    14,   103,    15,
      17,     4,     5,     6,     3,    15,     5,     7,     8,     4,
       5,    13,    14,    14,     4,    13,    10,     3,     9,    12,
      12,     9,     4,     7,    31,    21,    35,    16,    47,    14,
      17,    12,    15,    45,    39,    80,    83,    88,    18,   103,
      37
};

/* YYSTOS[STATE-NUM] -- The (internal number of the) accessing
   symbol of state STATE-NUM.  */
static const yytype_uint8 yystos[] =
{
       0,     7,     8,    19,    20,    21,    22,    23,     0,    21,
       3,    24,    24,     3,    25,    25,    12,    15,    26,    29,
      15,    26,    33,     3,    27,    28,    30,    29,    34,    33,
      13,    14,     4,     5,     6,    31,    32,    40,    41,    42,
      43,    44,    49,    56,     3,    35,    36,    37,    38,    49,
      50,    27,    16,    32,    41,    44,    10,    46,    16,    36,
      38,     9,     3,     4,    11,    15,    51,    52,    55,    57,
      47,    48,    50,    51,     3,     4,    11,    53,    54,    57,
      45,    12,    58,    14,    17,     9,    39,    16,    14,    47,
      59,    48,    51,    17,    53,    17,     3,     4,    11,    57,
      60,    61,    13,    14,    61
};

#define yyerrok		(yyerrstatus = 0)
#define yyclearin	(yychar = YYEMPTY)
#define YYEMPTY		(-2)
#define YYEOF		0

#define YYACCEPT	goto yyacceptlab
#define YYABORT		goto yyabortlab
#define YYERROR		goto yyerrorlab


/* Like YYERROR except do call yyerror.  This remains here temporarily
   to ease the transition to the new meaning of YYERROR, for GCC.
   Once GCC version 2 has supplanted version 1, this can go.  */

#define YYFAIL		goto yyerrlab

#define YYRECOVERING()  (!!yyerrstatus)

#define YYBACKUP(Token, Value)					\
do								\
  if (yychar == YYEMPTY && yylen == 1)				\
    {								\
      yychar = (Token);						\
      yylval = (Value);						\
      yytoken = YYTRANSLATE (yychar);				\
      YYPOPSTACK (1);						\
      goto yybackup;						\
    }								\
  else								\
    {								\
      yyerror (YY_("syntax error: cannot back up")); \
      YYERROR;							\
    }								\
while (YYID (0))


#define YYTERROR	1
#define YYERRCODE	256


/* YYLLOC_DEFAULT -- Set CURRENT to span from RHS[1] to RHS[N].
   If N is 0, then set CURRENT to the empty location which ends
   the previous symbol: RHS[0] (always defined).  */

#define YYRHSLOC(Rhs, K) ((Rhs)[K])
#ifndef YYLLOC_DEFAULT
# define YYLLOC_DEFAULT(Current, Rhs, N)				\
    do									\
      if (YYID (N))                                                    \
	{								\
	  (Current).first_line   = YYRHSLOC (Rhs, 1).first_line;	\
	  (Current).first_column = YYRHSLOC (Rhs, 1).first_column;	\
	  (Current).last_line    = YYRHSLOC (Rhs, N).last_line;		\
	  (Current).last_column  = YYRHSLOC (Rhs, N).last_column;	\
	}								\
      else								\
	{								\
	  (Current).first_line   = (Current).last_line   =		\
	    YYRHSLOC (Rhs, 0).last_line;				\
	  (Current).first_column = (Current).last_column =		\
	    YYRHSLOC (Rhs, 0).last_column;				\
	}								\
    while (YYID (0))
#endif


/* YY_LOCATION_PRINT -- Print the location on the stream.
   This macro was not mandated originally: define only if we know
   we won't break user code: when these are the locations we know.  */

#ifndef YY_LOCATION_PRINT
# if YYLTYPE_IS_TRIVIAL
#  define YY_LOCATION_PRINT(File, Loc)			\
     fprintf (File, "%d.%d-%d.%d",			\
	      (Loc).first_line, (Loc).first_column,	\
	      (Loc).last_line,  (Loc).last_column)
# else
#  define YY_LOCATION_PRINT(File, Loc) ((void) 0)
# endif
#endif


/* YYLEX -- calling `yylex' with the right arguments.  */

#ifdef YYLEX_PARAM
# define YYLEX yylex (YYLEX_PARAM)
#else
# define YYLEX yylex ()
#endif

/* Enable debugging if requested.  */
#if YYDEBUG

# ifndef YYFPRINTF
#  include <stdio.h> /* INFRINGES ON USER NAME SPACE */
#  define YYFPRINTF fprintf
# endif

# define YYDPRINTF(Args)			\
do {						\
  if (yydebug)					\
    YYFPRINTF Args;				\
} while (YYID (0))

# define YY_SYMBOL_PRINT(Title, Type, Value, Location)			  \
do {									  \
  if (yydebug)								  \
    {									  \
      YYFPRINTF (stderr, "%s ", Title);					  \
      yy_symbol_print (stderr,						  \
		  Type, Value); \
      YYFPRINTF (stderr, "\n");						  \
    }									  \
} while (YYID (0))


/*--------------------------------.
| Print this symbol on YYOUTPUT.  |
`--------------------------------*/

/*ARGSUSED*/
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static void
yy_symbol_value_print (FILE *yyoutput, int yytype, YYSTYPE const * const yyvaluep)
#else
static void
yy_symbol_value_print (yyoutput, yytype, yyvaluep)
    FILE *yyoutput;
    int yytype;
    YYSTYPE const * const yyvaluep;
#endif
{
  if (!yyvaluep)
    return;
# ifdef YYPRINT
  if (yytype < YYNTOKENS)
    YYPRINT (yyoutput, yytoknum[yytype], *yyvaluep);
# else
  YYUSE (yyoutput);
# endif
  switch (yytype)
    {
      default:
	break;
    }
}


/*--------------------------------.
| Print this symbol on YYOUTPUT.  |
`--------------------------------*/

#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static void
yy_symbol_print (FILE *yyoutput, int yytype, YYSTYPE const * const yyvaluep)
#else
static void
yy_symbol_print (yyoutput, yytype, yyvaluep)
    FILE *yyoutput;
    int yytype;
    YYSTYPE const * const yyvaluep;
#endif
{
  if (yytype < YYNTOKENS)
    YYFPRINTF (yyoutput, "token %s (", yytname[yytype]);
  else
    YYFPRINTF (yyoutput, "nterm %s (", yytname[yytype]);

  yy_symbol_value_print (yyoutput, yytype, yyvaluep);
  YYFPRINTF (yyoutput, ")");
}

/*------------------------------------------------------------------.
| yy_stack_print -- Print the state stack from its BOTTOM up to its |
| TOP (included).                                                   |
`------------------------------------------------------------------*/

#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static void
yy_stack_print (yytype_int16 *bottom, yytype_int16 *top)
#else
static void
yy_stack_print (bottom, top)
    yytype_int16 *bottom;
    yytype_int16 *top;
#endif
{
  YYFPRINTF (stderr, "Stack now");
  for (; bottom <= top; ++bottom)
    YYFPRINTF (stderr, " %d", *bottom);
  YYFPRINTF (stderr, "\n");
}

# define YY_STACK_PRINT(Bottom, Top)				\
do {								\
  if (yydebug)							\
    yy_stack_print ((Bottom), (Top));				\
} while (YYID (0))


/*------------------------------------------------.
| Report that the YYRULE is going to be reduced.  |
`------------------------------------------------*/

#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static void
yy_reduce_print (YYSTYPE *yyvsp, int yyrule)
#else
static void
yy_reduce_print (yyvsp, yyrule)
    YYSTYPE *yyvsp;
    int yyrule;
#endif
{
  int yynrhs = yyr2[yyrule];
  int yyi;
  unsigned long int yylno = yyrline[yyrule];
  YYFPRINTF (stderr, "Reducing stack by rule %d (line %lu):\n",
	     yyrule - 1, yylno);
  /* The symbols being reduced.  */
  for (yyi = 0; yyi < yynrhs; yyi++)
    {
      fprintf (stderr, "   $%d = ", yyi + 1);
      yy_symbol_print (stderr, yyrhs[yyprhs[yyrule] + yyi],
		       &(yyvsp[(yyi + 1) - (yynrhs)])
		       		       );
      fprintf (stderr, "\n");
    }
}

# define YY_REDUCE_PRINT(Rule)		\
do {					\
  if (yydebug)				\
    yy_reduce_print (yyvsp, Rule); \
} while (YYID (0))

/* Nonzero means print parse trace.  It is left uninitialized so that
   multiple parsers can coexist.  */
int yydebug;
#else /* !YYDEBUG */
# define YYDPRINTF(Args)
# define YY_SYMBOL_PRINT(Title, Type, Value, Location)
# define YY_STACK_PRINT(Bottom, Top)
# define YY_REDUCE_PRINT(Rule)
#endif /* !YYDEBUG */


/* YYINITDEPTH -- initial size of the parser's stacks.  */
#ifndef	YYINITDEPTH
# define YYINITDEPTH 200
#endif

/* YYMAXDEPTH -- maximum size the stacks can grow to (effective only
   if the built-in stack extension method is used).

   Do not make this value too large; the results are undefined if
   YYSTACK_ALLOC_MAXIMUM < YYSTACK_BYTES (YYMAXDEPTH)
   evaluated with infinite-precision integer arithmetic.  */

#ifndef YYMAXDEPTH
# define YYMAXDEPTH 10000
#endif



#if YYERROR_VERBOSE

# ifndef yystrlen
#  if defined __GLIBC__ && defined _STRING_H
#   define yystrlen strlen
#  else
/* Return the length of YYSTR.  */
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static YYSIZE_T
yystrlen (const char *yystr)
#else
static YYSIZE_T
yystrlen (yystr)
    const char *yystr;
#endif
{
  YYSIZE_T yylen;
  for (yylen = 0; yystr[yylen]; yylen++)
    continue;
  return yylen;
}
#  endif
# endif

# ifndef yystpcpy
#  if defined __GLIBC__ && defined _STRING_H && defined _GNU_SOURCE
#   define yystpcpy stpcpy
#  else
/* Copy YYSRC to YYDEST, returning the address of the terminating '\0' in
   YYDEST.  */
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static char *
yystpcpy (char *yydest, const char *yysrc)
#else
static char *
yystpcpy (yydest, yysrc)
    char *yydest;
    const char *yysrc;
#endif
{
  char *yyd = yydest;
  const char *yys = yysrc;

  while ((*yyd++ = *yys++) != '\0')
    continue;

  return yyd - 1;
}
#  endif
# endif

# ifndef yytnamerr
/* Copy to YYRES the contents of YYSTR after stripping away unnecessary
   quotes and backslashes, so that it's suitable for yyerror.  The
   heuristic is that double-quoting is unnecessary unless the string
   contains an apostrophe, a comma, or backslash (other than
   backslash-backslash).  YYSTR is taken from yytname.  If YYRES is
   null, do not copy; instead, return the length of what the result
   would have been.  */
static YYSIZE_T
yytnamerr (char *yyres, const char *yystr)
{
  if (*yystr == '"')
    {
      YYSIZE_T yyn = 0;
      char const *yyp = yystr;

      for (;;)
	switch (*++yyp)
	  {
	  case '\'':
	  case ',':
	    goto do_not_strip_quotes;

	  case '\\':
	    if (*++yyp != '\\')
	      goto do_not_strip_quotes;
	    /* Fall through.  */
	  default:
	    if (yyres)
	      yyres[yyn] = *yyp;
	    yyn++;
	    break;

	  case '"':
	    if (yyres)
	      yyres[yyn] = '\0';
	    return yyn;
	  }
    do_not_strip_quotes: ;
    }

  if (! yyres)
    return yystrlen (yystr);

  return yystpcpy (yyres, yystr) - yyres;
}
# endif

/* Copy into YYRESULT an error message about the unexpected token
   YYCHAR while in state YYSTATE.  Return the number of bytes copied,
   including the terminating null byte.  If YYRESULT is null, do not
   copy anything; just return the number of bytes that would be
   copied.  As a special case, return 0 if an ordinary "syntax error"
   message will do.  Return YYSIZE_MAXIMUM if overflow occurs during
   size calculation.  */
static YYSIZE_T
yysyntax_error (char *yyresult, int yystate, int yychar)
{
  int yyn = yypact[yystate];

  if (! (YYPACT_NINF < yyn && yyn <= YYLAST))
    return 0;
  else
    {
      int yytype = YYTRANSLATE (yychar);
      YYSIZE_T yysize0 = yytnamerr (0, yytname[yytype]);
      YYSIZE_T yysize = yysize0;
      YYSIZE_T yysize1;
      int yysize_overflow = 0;
      enum { YYERROR_VERBOSE_ARGS_MAXIMUM = 5 };
      char const *yyarg[YYERROR_VERBOSE_ARGS_MAXIMUM];
      int yyx;

# if 0
      /* This is so xgettext sees the translatable formats that are
	 constructed on the fly.  */
      YY_("syntax error, unexpected %s");
      YY_("syntax error, unexpected %s, expecting %s");
      YY_("syntax error, unexpected %s, expecting %s or %s");
      YY_("syntax error, unexpected %s, expecting %s or %s or %s");
      YY_("syntax error, unexpected %s, expecting %s or %s or %s or %s");
# endif
      char *yyfmt;
      char const *yyf;
      static char const yyunexpected[] = "syntax error, unexpected %s";
      static char const yyexpecting[] = ", expecting %s";
      static char const yyor[] = " or %s";
      char yyformat[sizeof yyunexpected
		    + sizeof yyexpecting - 1
		    + ((YYERROR_VERBOSE_ARGS_MAXIMUM - 2)
		       * (sizeof yyor - 1))];
      char const *yyprefix = yyexpecting;

      /* Start YYX at -YYN if negative to avoid negative indexes in
	 YYCHECK.  */
      int yyxbegin = yyn < 0 ? -yyn : 0;

      /* Stay within bounds of both yycheck and yytname.  */
      int yychecklim = YYLAST - yyn + 1;
      int yyxend = yychecklim < YYNTOKENS ? yychecklim : YYNTOKENS;
      int yycount = 1;

      yyarg[0] = yytname[yytype];
      yyfmt = yystpcpy (yyformat, yyunexpected);

      for (yyx = yyxbegin; yyx < yyxend; ++yyx)
	if (yycheck[yyx + yyn] == yyx && yyx != YYTERROR)
	  {
	    if (yycount == YYERROR_VERBOSE_ARGS_MAXIMUM)
	      {
		yycount = 1;
		yysize = yysize0;
		yyformat[sizeof yyunexpected - 1] = '\0';
		break;
	      }
	    yyarg[yycount++] = yytname[yyx];
	    yysize1 = yysize + yytnamerr (0, yytname[yyx]);
	    yysize_overflow |= (yysize1 < yysize);
	    yysize = yysize1;
	    yyfmt = yystpcpy (yyfmt, yyprefix);
	    yyprefix = yyor;
	  }

      yyf = YY_(yyformat);
      yysize1 = yysize + yystrlen (yyf);
      yysize_overflow |= (yysize1 < yysize);
      yysize = yysize1;

      if (yysize_overflow)
	return YYSIZE_MAXIMUM;

      if (yyresult)
	{
	  /* Avoid sprintf, as that infringes on the user's name space.
	     Don't have undefined behavior even if the translation
	     produced a string with the wrong number of "%s"s.  */
	  char *yyp = yyresult;
	  int yyi = 0;
	  while ((*yyp = *yyf) != '\0')
	    {
	      if (*yyp == '%' && yyf[1] == 's' && yyi < yycount)
		{
		  yyp += yytnamerr (yyp, yyarg[yyi++]);
		  yyf += 2;
		}
	      else
		{
		  yyp++;
		  yyf++;
		}
	    }
	}
      return yysize;
    }
}
#endif /* YYERROR_VERBOSE */


/*-----------------------------------------------.
| Release the memory associated to this symbol.  |
`-----------------------------------------------*/

/*ARGSUSED*/
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static void
yydestruct (const char *yymsg, int yytype, YYSTYPE *yyvaluep)
#else
static void
yydestruct (yymsg, yytype, yyvaluep)
    const char *yymsg;
    int yytype;
    YYSTYPE *yyvaluep;
#endif
{
  YYUSE (yyvaluep);

  if (!yymsg)
    yymsg = "Deleting";
  YY_SYMBOL_PRINT (yymsg, yytype, yyvaluep, yylocationp);

  switch (yytype)
    {

      default:
	break;
    }
}


/* Prevent warnings from -Wmissing-prototypes.  */

#ifdef YYPARSE_PARAM
#if defined __STDC__ || defined __cplusplus
int yyparse (void *YYPARSE_PARAM);
#else
int yyparse ();
#endif
#else /* ! YYPARSE_PARAM */
#if defined __STDC__ || defined __cplusplus
int yyparse (void);
#else
int yyparse ();
#endif
#endif /* ! YYPARSE_PARAM */



/* The look-ahead symbol.  */
int yychar;

/* The semantic value of the look-ahead symbol.  */
YYSTYPE yylval;

/* Number of syntax errors so far.  */
int yynerrs;



/*----------.
| yyparse.  |
`----------*/

#ifdef YYPARSE_PARAM
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
int
yyparse (void *YYPARSE_PARAM)
#else
int
yyparse (YYPARSE_PARAM)
    void *YYPARSE_PARAM;
#endif
#else /* ! YYPARSE_PARAM */
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
int
yyparse (void)
#else
int
yyparse ()

#endif
#endif
{
  
  int yystate;
  int yyn;
  int yyresult;
  /* Number of tokens to shift before error messages enabled.  */
  int yyerrstatus;
  /* Look-ahead token as an internal (translated) token number.  */
  int yytoken = 0;
#if YYERROR_VERBOSE
  /* Buffer for error messages, and its allocated size.  */
  char yymsgbuf[128];
  char *yymsg = yymsgbuf;
  YYSIZE_T yymsg_alloc = sizeof yymsgbuf;
#endif

  /* Three stacks and their tools:
     `yyss': related to states,
     `yyvs': related to semantic values,
     `yyls': related to locations.

     Refer to the stacks thru separate pointers, to allow yyoverflow
     to reallocate them elsewhere.  */

  /* The state stack.  */
  yytype_int16 yyssa[YYINITDEPTH];
  yytype_int16 *yyss = yyssa;
  yytype_int16 *yyssp;

  /* The semantic value stack.  */
  YYSTYPE yyvsa[YYINITDEPTH];
  YYSTYPE *yyvs = yyvsa;
  YYSTYPE *yyvsp;



#define YYPOPSTACK(N)   (yyvsp -= (N), yyssp -= (N))

  YYSIZE_T yystacksize = YYINITDEPTH;

  /* The variables used to return semantic value and location from the
     action routines.  */
  YYSTYPE yyval;


  /* The number of symbols on the RHS of the reduced rule.
     Keep to zero when no symbol should be popped.  */
  int yylen = 0;

  YYDPRINTF ((stderr, "Starting parse\n"));

  yystate = 0;
  yyerrstatus = 0;
  yynerrs = 0;
  yychar = YYEMPTY;		/* Cause a token to be read.  */

  /* Initialize stack pointers.
     Waste one element of value and location stack
     so that they stay on the same level as the state stack.
     The wasted elements are never initialized.  */

  yyssp = yyss;
  yyvsp = yyvs;

  goto yysetstate;

/*------------------------------------------------------------.
| yynewstate -- Push a new state, which is found in yystate.  |
`------------------------------------------------------------*/
 yynewstate:
  /* In all cases, when you get here, the value and location stacks
     have just been pushed.  So pushing a state here evens the stacks.  */
  yyssp++;

 yysetstate:
  *yyssp = yystate;

  if (yyss + yystacksize - 1 <= yyssp)
    {
      /* Get the current used size of the three stacks, in elements.  */
      YYSIZE_T yysize = yyssp - yyss + 1;

#ifdef yyoverflow
      {
	/* Give user a chance to reallocate the stack.  Use copies of
	   these so that the &'s don't force the real ones into
	   memory.  */
	YYSTYPE *yyvs1 = yyvs;
	yytype_int16 *yyss1 = yyss;


	/* Each stack pointer address is followed by the size of the
	   data in use in that stack, in bytes.  This used to be a
	   conditional around just the two extra args, but that might
	   be undefined if yyoverflow is a macro.  */
	yyoverflow (YY_("memory exhausted"),
		    &yyss1, yysize * sizeof (*yyssp),
		    &yyvs1, yysize * sizeof (*yyvsp),

		    &yystacksize);

	yyss = yyss1;
	yyvs = yyvs1;
      }
#else /* no yyoverflow */
# ifndef YYSTACK_RELOCATE
      goto yyexhaustedlab;
# else
      /* Extend the stack our own way.  */
      if (YYMAXDEPTH <= yystacksize)
	goto yyexhaustedlab;
      yystacksize *= 2;
      if (YYMAXDEPTH < yystacksize)
	yystacksize = YYMAXDEPTH;

      {
	yytype_int16 *yyss1 = yyss;
	union yyalloc *yyptr =
	  (union yyalloc *) YYSTACK_ALLOC (YYSTACK_BYTES (yystacksize));
	if (! yyptr)
	  goto yyexhaustedlab;
	YYSTACK_RELOCATE (yyss);
	YYSTACK_RELOCATE (yyvs);

#  undef YYSTACK_RELOCATE
	if (yyss1 != yyssa)
	  YYSTACK_FREE (yyss1);
      }
# endif
#endif /* no yyoverflow */

      yyssp = yyss + yysize - 1;
      yyvsp = yyvs + yysize - 1;


      YYDPRINTF ((stderr, "Stack size increased to %lu\n",
		  (unsigned long int) yystacksize));

      if (yyss + yystacksize - 1 <= yyssp)
	YYABORT;
    }

  YYDPRINTF ((stderr, "Entering state %d\n", yystate));

  goto yybackup;

/*-----------.
| yybackup.  |
`-----------*/
yybackup:

  /* Do appropriate processing given the current state.  Read a
     look-ahead token if we need one and don't already have one.  */

  /* First try to decide what to do without reference to look-ahead token.  */
  yyn = yypact[yystate];
  if (yyn == YYPACT_NINF)
    goto yydefault;

  /* Not known => get a look-ahead token if don't already have one.  */

  /* YYCHAR is either YYEMPTY or YYEOF or a valid look-ahead symbol.  */
  if (yychar == YYEMPTY)
    {
      YYDPRINTF ((stderr, "Reading a token: "));
      yychar = YYLEX;
    }

  if (yychar <= YYEOF)
    {
      yychar = yytoken = YYEOF;
      YYDPRINTF ((stderr, "Now at end of input.\n"));
    }
  else
    {
      yytoken = YYTRANSLATE (yychar);
      YY_SYMBOL_PRINT ("Next token is", yytoken, &yylval, &yylloc);
    }

  /* If the proper action on seeing token YYTOKEN is to reduce or to
     detect an error, take that action.  */
  yyn += yytoken;
  if (yyn < 0 || YYLAST < yyn || yycheck[yyn] != yytoken)
    goto yydefault;
  yyn = yytable[yyn];
  if (yyn <= 0)
    {
      if (yyn == 0 || yyn == YYTABLE_NINF)
	goto yyerrlab;
      yyn = -yyn;
      goto yyreduce;
    }

  if (yyn == YYFINAL)
    YYACCEPT;

  /* Count tokens shifted since error; after three, turn off error
     status.  */
  if (yyerrstatus)
    yyerrstatus--;

  /* Shift the look-ahead token.  */
  YY_SYMBOL_PRINT ("Shifting", yytoken, &yylval, &yylloc);

  /* Discard the shifted token unless it is eof.  */
  if (yychar != YYEOF)
    yychar = YYEMPTY;

  yystate = yyn;
  *++yyvsp = yylval;

  goto yynewstate;


/*-----------------------------------------------------------.
| yydefault -- do the default action for the current state.  |
`-----------------------------------------------------------*/
yydefault:
  yyn = yydefact[yystate];
  if (yyn == 0)
    goto yyerrlab;
  goto yyreduce;


/*-----------------------------.
| yyreduce -- Do a reduction.  |
`-----------------------------*/
yyreduce:
  /* yyn is the number of a rule to reduce with.  */
  yylen = yyr2[yyn];

  /* If YYLEN is nonzero, implement the default value of the action:
     `$$ = $1'.

     Otherwise, the following line sets YYVAL to garbage.
     This behavior is undocumented and Bison
     users should not rely upon it.  Assigning to YYVAL
     unconditionally makes the parser a bit smaller, and it avoids a
     GCC warning that YYVAL may be used uninitialized.  */
  yyval = yyvsp[1-yylen];


  YY_REDUCE_PRINT (yyn);
  switch (yyn)
    {
        case 2:
#line 21 "cf3parse.y"
    { yyerror("Something defined outside of a block or missing punctuation in input"); }
    break;

  case 10:
#line 39 "cf3parse.y"
    {
                          DebugBanner("Bundle");
                          P.block = "bundle";
                          P.rval = NULL;
                          P.currentRlist = NULL;
                          }
    break;

  case 11:
#line 49 "cf3parse.y"
    {
                          DebugBanner("Body");
                          P.block = "body";
                          }
    break;

  case 12:
#line 56 "cf3parse.y"
    {
                          P.blocktype = P.currentid;;
                          Debug("Found block type %s for %s\n",P.blocktype,P.block);
                          P.useargs = NULL;
                          }
    break;

  case 13:
#line 64 "cf3parse.y"
    {
                          P.blockid = P.currentid;
                          Debug("Found identifier %s for %s\n",P.currentid,P.block);
                          }
    break;

  case 18:
#line 84 "cf3parse.y"
    {
                          AppendRlist(&(P.useargs),P.currentid,CF_SCALAR);
                          free(P.currentid);
                          }
    break;

  case 19:
#line 92 "cf3parse.y"
    {
                       if (RelevantBundle(THIS_AGENT,P.blocktype))
                          {
                          Debug("We a compiling everything here\n");
                          INSTALL_SKIP = false;                          
                          }
                       else if (strcmp(THIS_AGENT,P.blocktype) != 0)
                          {
                          Debug("This is for a different agent\n");
                          INSTALL_SKIP = true;
                          }
                       
                       P.currentbundle = AppendBundle(&BUNDLES,P.blockid,P.blocktype,P.useargs); 
                       P.useargs = NULL;
                       P.currenttype = NULL;
                       /* The append functions gobble these strdups, so no memory leak */
                       }
    break;

  case 20:
#line 111 "cf3parse.y"
    {
                       INSTALL_SKIP = false;
                       Debug("End promise bundle\n\n");
                       }
    break;

  case 25:
#line 129 "cf3parse.y"
    {
                        P.currentbody = AppendBody(&BODIES,P.blockid,P.blocktype,P.useargs);
                          
                        P.useargs = NULL;
                        P.currenttype = NULL;
                        P.currentid = NULL;

                        /* The append functions gobble these strdups, so no memory leak */
                        Debug("Starting block\n");
                        }
    break;

  case 26:
#line 143 "cf3parse.y"
    {
                        Debug("End promise body\n");
                        }
    break;

  case 33:
#line 167 "cf3parse.y"
    { char *contextid = NULL;
 
                        CheckSelection(P.blocktype,P.blockid,P.lval,P.rval,P.rtype);

                        if (P.currentclasses == NULL)
                           {
                           AppendConstraint(&((P.currentbody)->conlist),P.lval,P.rval,P.rtype,"any",P.isbody);
                           }
                        else
                           {
                           AppendConstraint(&((P.currentbody)->conlist),P.lval,P.rval,P.rtype,P.currentclasses,P.isbody);
                           }

                        if (strcmp(P.blockid,"control") == 0 && strcmp(P.blocktype,"common") == 0)
                           {
                           if (strcmp(P.lval,"inputs") == 0)
                              {
                              if (IsDefinedClass(P.currentclasses))
                                 {
                                 if (VINPUTLIST == NULL)
                                    {
                                    VINPUTLIST = P.rval;
                                    }
                                 else
                                    {
                                    yyerror("Redefinition of input list (broken promise)");
                                    }
                                 }
                              }
                           }

                        P.rval = NULL;
                        }
    break;

  case 41:
#line 220 "cf3parse.y"
    {
                         Debug("\n* Begin new promise type category %s in function \n\n",P.currenttype);
                                                 
                         if (strcmp(P.block,"bundle") == 0)
                            {
                            struct SubTypeSyntax ss = CheckSubType(P.blocktype,P.currenttype);
                            P.currentstype = AppendSubType(P.currentbundle,P.currenttype);
                            }
                         }
    break;

  case 42:
#line 237 "cf3parse.y"
    {
                        if (P.currentclasses == NULL)
                           {
                           P.currentpromise = AppendPromise(P.currentstype,P.promiser,P.rval,P.rtype,"any",P.blockid,P.blocktype);
                           }
                        else
                           {
                           P.currentpromise = AppendPromise(P.currentstype,P.promiser,P.rval,P.rtype,P.currentclasses,P.blockid,P.blocktype);
                           }
                        }
    break;

  case 43:
#line 249 "cf3parse.y"
    {
                        Debug("End implicit promise %s\n\n",P.promiser);
                        P.currentid = NULL;
                        P.currentRlist = NULL;
                        P.currentstring = NULL;
                        P.promiser = NULL;
                        P.promisee = NULL;
                        /* reset argptrs etc*/
                        }
    break;

  case 44:
#line 262 "cf3parse.y"
    {
                        if (P.currentclasses == NULL)
                           {
                           P.currentpromise = AppendPromise(P.currentstype,P.promiser,NULL,CF_NOPROMISEE,"any",P.blockid,P.blocktype);
                           }
                        else
                           {
                           P.currentpromise = AppendPromise(P.currentstype,P.promiser,NULL,CF_NOPROMISEE,P.currentclasses,P.blockid,P.blocktype);
                           }
                        }
    break;

  case 45:
#line 274 "cf3parse.y"
    {
                        Debug("End full promise with promisee %s\n\n",P.promiser);

                        /* Don't free these */
                        P.currentid = NULL;
                        P.currentRlist = NULL;
                        P.currentstring = NULL;
                        P.promiser = NULL;
                        P.promisee = NULL;
                         /* reset argptrs etc*/
                        }
    break;

  case 49:
#line 298 "cf3parse.y"
    { struct SubTypeSyntax ss;
                          char *contextid = NULL;

                        if (!INSTALL_SKIP)
                           {
                           ss = CheckSubType(P.blocktype,P.currenttype);                           
                           CheckConstraint(P.currenttype,P.blockid,P.lval,P.rval,P.rtype,ss);                           
                           AppendConstraint(&(P.currentpromise->conlist),P.lval,P.rval,P.rtype,"any",P.isbody);
                           P.rval = NULL;
                           P.lval = NULL;
                           P.currentRlist = NULL;
                           }  
                        }
    break;

  case 50:
#line 315 "cf3parse.y"
    {
                         Debug("  New class context \'%s\' :: \n\n",P.currentclasses);
                         }
    break;

  case 51:
#line 322 "cf3parse.y"
    {
                         P.lval = P.currentid;
                         P.currentRlist = NULL;
                         Debug("Recorded LVAL %s\n",P.lval);
                         }
    break;

  case 52:
#line 331 "cf3parse.y"
    {
                         P.rval = P.currentid;
                         P.rtype = CF_SCALAR;
                         P.isbody = true;
                         Debug("Recorded IDRVAL %s\n",P.rval);
                         }
    break;

  case 53:
#line 338 "cf3parse.y"
    {
                         P.rval = P.currentstring;
                         P.rtype = CF_SCALAR;
                         P.isbody = false;
                         Debug("Recorded scalarRVAL %s\n",P.rval);
                         }
    break;

  case 54:
#line 345 "cf3parse.y"
    {
                         P.rval = P.currentstring;
                         P.rtype = CF_SCALAR;
                         P.isbody = false;
                         Debug("Recorded saclarvariableRVAL %s\n",P.rval);
                         }
    break;

  case 55:
#line 352 "cf3parse.y"
    {
                         P.rval = P.currentRlist;
                         P.isbody = false;
                         P.rtype = CF_LIST;
                         }
    break;

  case 56:
#line 358 "cf3parse.y"
    {
                         P.isbody = false;
                         P.rval = P.currentfncall[P.arg_nesting+1];
                         P.rtype = CF_FNCALL;
                         }
    break;

  case 61:
#line 379 "cf3parse.y"
    {
                          AppendRlist((struct Rlist **)&P.currentRlist,P.currentid,CF_SCALAR);
                          free(P.currentid);
                          }
    break;

  case 62:
#line 385 "cf3parse.y"
    {
                          AppendRlist((struct Rlist **)&P.currentRlist,(void *)P.currentstring,CF_SCALAR);
                          free(P.currentstring);
                          }
    break;

  case 63:
#line 391 "cf3parse.y"
    {
                          AppendRlist((struct Rlist **)&P.currentRlist,(void *)P.currentstring,CF_SCALAR);
                          free(P.currentstring);
                          }
    break;

  case 64:
#line 397 "cf3parse.y"
    {
                          Debug("Install function call as list item from level %d\n",P.arg_nesting+1);
                          AppendRlist((struct Rlist **)&P.currentRlist,(void *)P.currentfncall[P.arg_nesting+1],CF_FNCALL);
                          DeleteRvalItem(P.currentfncall[P.arg_nesting+1],CF_FNCALL);
                          }
    break;

  case 65:
#line 406 "cf3parse.y"
    {
                          Debug("Found function identifier %s\n",P.currentid);
                          }
    break;

  case 66:
#line 413 "cf3parse.y"
    {
                          P.promiser = P.currentstring; //strdup(P.currentstring);
                          Debug("Promising object name \'%s\'\n",P.promiser);
                          }
    break;

  case 67:
#line 421 "cf3parse.y"
    {
                         Debug("Finished with function call, now at level %d\n\n",P.arg_nesting);
                         }
    break;

  case 68:
#line 428 "cf3parse.y"
    {
                           if (++P.arg_nesting > CF_MAX_NESTING)
                              {
                              yyerror("Nesting of functions is deeper than recommended");
                              }
                           P.currentfnid[P.arg_nesting] = P.currentid; //strdup(P.currentid);
                           Debug("Start FnCall %s args level %d\n",P.currentfnid[P.arg_nesting],P.arg_nesting);
                           }
    break;

  case 69:
#line 439 "cf3parse.y"
    {
                           Debug("End args level %d\n",P.arg_nesting);
                           P.currentfncall[P.arg_nesting] = NewFnCall(P.currentfnid[P.arg_nesting],P.giveargs[P.arg_nesting]);
                           P.giveargs[P.arg_nesting] = NULL;
                           
                           P.currentid = NULL;
                           free(P.currentfnid[P.arg_nesting]);
                           P.currentfnid[P.arg_nesting] = NULL;
                           P.arg_nesting--;
                           }
    break;

  case 73:
#line 460 "cf3parse.y"
    {
                          /* currently inside a use function */
                          AppendRlist(&P.giveargs[P.arg_nesting],P.currentid,CF_SCALAR);
                          free(P.currentid);
                          }
    break;

  case 74:
#line 467 "cf3parse.y"
    {
                          /* currently inside a use function */
                          AppendRlist(&P.giveargs[P.arg_nesting],P.currentstring,CF_SCALAR);
                          free(P.currentstring);
                          }
    break;

  case 75:
#line 474 "cf3parse.y"
    {
                          /* currently inside a use function */
                          AppendRlist(&P.giveargs[P.arg_nesting],P.currentstring,CF_SCALAR);
                          free(P.currentstring);
                          }
    break;

  case 76:
#line 481 "cf3parse.y"
    {
                          /* Careful about recursion */
                          AppendRlist(&P.giveargs[P.arg_nesting],(void *)P.currentfncall[P.arg_nesting+1],CF_FNCALL);
                          DeleteRvalItem(P.currentfncall[P.arg_nesting+1],CF_FNCALL);
                          }
    break;


/* Line 1267 of yacc.c.  */
#line 1835 "y.tab.c"
      default: break;
    }
  YY_SYMBOL_PRINT ("-> $$ =", yyr1[yyn], &yyval, &yyloc);

  YYPOPSTACK (yylen);
  yylen = 0;
  YY_STACK_PRINT (yyss, yyssp);

  *++yyvsp = yyval;


  /* Now `shift' the result of the reduction.  Determine what state
     that goes to, based on the state we popped back to and the rule
     number reduced by.  */

  yyn = yyr1[yyn];

  yystate = yypgoto[yyn - YYNTOKENS] + *yyssp;
  if (0 <= yystate && yystate <= YYLAST && yycheck[yystate] == *yyssp)
    yystate = yytable[yystate];
  else
    yystate = yydefgoto[yyn - YYNTOKENS];

  goto yynewstate;


/*------------------------------------.
| yyerrlab -- here on detecting error |
`------------------------------------*/
yyerrlab:
  /* If not already recovering from an error, report this error.  */
  if (!yyerrstatus)
    {
      ++yynerrs;
#if ! YYERROR_VERBOSE
      yyerror (YY_("syntax error"));
#else
      {
	YYSIZE_T yysize = yysyntax_error (0, yystate, yychar);
	if (yymsg_alloc < yysize && yymsg_alloc < YYSTACK_ALLOC_MAXIMUM)
	  {
	    YYSIZE_T yyalloc = 2 * yysize;
	    if (! (yysize <= yyalloc && yyalloc <= YYSTACK_ALLOC_MAXIMUM))
	      yyalloc = YYSTACK_ALLOC_MAXIMUM;
	    if (yymsg != yymsgbuf)
	      YYSTACK_FREE (yymsg);
	    yymsg = (char *) YYSTACK_ALLOC (yyalloc);
	    if (yymsg)
	      yymsg_alloc = yyalloc;
	    else
	      {
		yymsg = yymsgbuf;
		yymsg_alloc = sizeof yymsgbuf;
	      }
	  }

	if (0 < yysize && yysize <= yymsg_alloc)
	  {
	    (void) yysyntax_error (yymsg, yystate, yychar);
	    yyerror (yymsg);
	  }
	else
	  {
	    yyerror (YY_("syntax error"));
	    if (yysize != 0)
	      goto yyexhaustedlab;
	  }
      }
#endif
    }



  if (yyerrstatus == 3)
    {
      /* If just tried and failed to reuse look-ahead token after an
	 error, discard it.  */

      if (yychar <= YYEOF)
	{
	  /* Return failure if at end of input.  */
	  if (yychar == YYEOF)
	    YYABORT;
	}
      else
	{
	  yydestruct ("Error: discarding",
		      yytoken, &yylval);
	  yychar = YYEMPTY;
	}
    }

  /* Else will try to reuse look-ahead token after shifting the error
     token.  */
  goto yyerrlab1;


/*---------------------------------------------------.
| yyerrorlab -- error raised explicitly by YYERROR.  |
`---------------------------------------------------*/
yyerrorlab:

  /* Pacify compilers like GCC when the user code never invokes
     YYERROR and the label yyerrorlab therefore never appears in user
     code.  */
  if (/*CONSTCOND*/ 0)
     goto yyerrorlab;

  /* Do not reclaim the symbols of the rule which action triggered
     this YYERROR.  */
  YYPOPSTACK (yylen);
  yylen = 0;
  YY_STACK_PRINT (yyss, yyssp);
  yystate = *yyssp;
  goto yyerrlab1;


/*-------------------------------------------------------------.
| yyerrlab1 -- common code for both syntax error and YYERROR.  |
`-------------------------------------------------------------*/
yyerrlab1:
  yyerrstatus = 3;	/* Each real token shifted decrements this.  */

  for (;;)
    {
      yyn = yypact[yystate];
      if (yyn != YYPACT_NINF)
	{
	  yyn += YYTERROR;
	  if (0 <= yyn && yyn <= YYLAST && yycheck[yyn] == YYTERROR)
	    {
	      yyn = yytable[yyn];
	      if (0 < yyn)
		break;
	    }
	}

      /* Pop the current state because it cannot handle the error token.  */
      if (yyssp == yyss)
	YYABORT;


      yydestruct ("Error: popping",
		  yystos[yystate], yyvsp);
      YYPOPSTACK (1);
      yystate = *yyssp;
      YY_STACK_PRINT (yyss, yyssp);
    }

  if (yyn == YYFINAL)
    YYACCEPT;

  *++yyvsp = yylval;


  /* Shift the error token.  */
  YY_SYMBOL_PRINT ("Shifting", yystos[yyn], yyvsp, yylsp);

  yystate = yyn;
  goto yynewstate;


/*-------------------------------------.
| yyacceptlab -- YYACCEPT comes here.  |
`-------------------------------------*/
yyacceptlab:
  yyresult = 0;
  goto yyreturn;

/*-----------------------------------.
| yyabortlab -- YYABORT comes here.  |
`-----------------------------------*/
yyabortlab:
  yyresult = 1;
  goto yyreturn;

#ifndef yyoverflow
/*-------------------------------------------------.
| yyexhaustedlab -- memory exhaustion comes here.  |
`-------------------------------------------------*/
yyexhaustedlab:
  yyerror (YY_("memory exhausted"));
  yyresult = 2;
  /* Fall through.  */
#endif

yyreturn:
  if (yychar != YYEOF && yychar != YYEMPTY)
     yydestruct ("Cleanup: discarding lookahead",
		 yytoken, &yylval);
  /* Do not reclaim the symbols of the rule which action triggered
     this YYABORT or YYACCEPT.  */
  YYPOPSTACK (yylen);
  YY_STACK_PRINT (yyss, yyssp);
  while (yyssp != yyss)
    {
      yydestruct ("Cleanup: popping",
		  yystos[*yyssp], yyvsp);
      YYPOPSTACK (1);
    }
#ifndef yyoverflow
  if (yyss != yyssa)
    YYSTACK_FREE (yyss);
#endif
#if YYERROR_VERBOSE
  if (yymsg != yymsgbuf)
    YYSTACK_FREE (yymsg);
#endif
  /* Make sure YYID is used.  */
  return YYID (yyresult);
}


#line 487 "cf3parse.y"


/*****************************************************************/

void yyerror(s)

char *s;

{ char *sp = yytext;

if (sp == NULL)
   {
   fprintf (stderr, "%s:%s:%d,%d: %s, near token \'NULL\'\n",VPREFIX,P.filename,P.line_no,P.line_pos,s);
   }
else if (*sp == '\"' && strlen(sp) > 1)
   {
   sp++;
   }

fprintf (stderr, "%s:%s:%d,%d: %s, near token \'%s\'\n",VPREFIX,P.filename,P.line_no,P.line_pos,s,sp);

ERRORCOUNT++;

if (ERRORCOUNT > 10)
   {
   FatalError("Too many errors");
   }
}

/*****************************************************************/

/* EOF */

