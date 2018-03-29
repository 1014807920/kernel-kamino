/* A Bison parser, made by GNU Bison 3.0.4.  */

/* Bison implementation for Yacc-like parsers in C

   Copyright (C) 1984, 1989-1990, 2000-2015 Free Software Foundation, Inc.

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

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
#define YYBISON_VERSION "3.0.4"

/* Skeleton name.  */
#define YYSKELETON_NAME "yacc.c"

/* Pure parsers.  */
#define YYPURE 1

/* Push parsers.  */
#define YYPUSH 0

/* Pull parsers.  */
#define YYPULL 1


/* Substitute the variable and function names.  */
#define yyparse         parse_events_parse
#define yylex           parse_events_lex
#define yyerror         parse_events_error
#define yydebug         parse_events_debug
#define yynerrs         parse_events_nerrs


/* Copy the first part of user declarations.  */
#line 7 "util/parse-events.y" /* yacc.c:339  */


#define YYDEBUG 1

#include <linux/compiler.h>
#include <linux/list.h>
#include <linux/types.h>
#include "util.h"
#include "parse-events.h"
#include "parse-events-bison.h"

#define ABORT_ON(val) \
do { \
	if (val) \
		YYABORT; \
} while (0)

#define ALLOC_LIST(list) \
do { \
	list = malloc(sizeof(*list)); \
	ABORT_ON(!list);              \
	INIT_LIST_HEAD(list);         \
} while (0)

static inc_group_count(struct list_head *list,
		       struct parse_events_evlist *data)
{
	/* Count groups only have more than 1 members */
	if (!list_is_last(list->next, list))
		data->nr_groups++;
}


#line 106 "util/parse-events-bison.c" /* yacc.c:339  */

# ifndef YY_NULLPTR
#  if defined __cplusplus && 201103L <= __cplusplus
#   define YY_NULLPTR nullptr
#  else
#   define YY_NULLPTR 0
#  endif
# endif

/* Enabling verbose error messages.  */
#ifdef YYERROR_VERBOSE
# undef YYERROR_VERBOSE
# define YYERROR_VERBOSE 1
#else
# define YYERROR_VERBOSE 0
#endif

/* In a future release of Bison, this section will be replaced
   by #include "parse-events-bison.h".  */
#ifndef YY_PARSE_EVENTS_UTIL_PARSE_EVENTS_BISON_H_INCLUDED
# define YY_PARSE_EVENTS_UTIL_PARSE_EVENTS_BISON_H_INCLUDED
/* Debug traces.  */
#ifndef YYDEBUG
# define YYDEBUG 0
#endif
#if YYDEBUG
extern int parse_events_debug;
#endif

/* Token type.  */
#ifndef YYTOKENTYPE
# define YYTOKENTYPE
  enum yytokentype
  {
    PE_START_EVENTS = 258,
    PE_START_TERMS = 259,
    PE_VALUE = 260,
    PE_VALUE_SYM_HW = 261,
    PE_VALUE_SYM_SW = 262,
    PE_RAW = 263,
    PE_TERM = 264,
    PE_EVENT_NAME = 265,
    PE_NAME = 266,
    PE_BPF_OBJECT = 267,
    PE_BPF_SOURCE = 268,
    PE_MODIFIER_EVENT = 269,
    PE_MODIFIER_BP = 270,
    PE_NAME_CACHE_TYPE = 271,
    PE_NAME_CACHE_OP_RESULT = 272,
    PE_PREFIX_MEM = 273,
    PE_PREFIX_RAW = 274,
    PE_PREFIX_GROUP = 275,
    PE_ERROR = 276,
    PE_PMU_EVENT_PRE = 277,
    PE_PMU_EVENT_SUF = 278,
    PE_KERNEL_PMU_EVENT = 279
  };
#endif

/* Value type.  */
#if ! defined YYSTYPE && ! defined YYSTYPE_IS_DECLARED

union YYSTYPE
{
#line 87 "util/parse-events.y" /* yacc.c:355  */

	char *str;
	u64 num;
	struct list_head *head;
	struct parse_events_term *term;
	struct tracepoint_name {
		char *sys;
		char *event;
	} tracepoint_name;

#line 182 "util/parse-events-bison.c" /* yacc.c:355  */
};

typedef union YYSTYPE YYSTYPE;
# define YYSTYPE_IS_TRIVIAL 1
# define YYSTYPE_IS_DECLARED 1
#endif

/* Location type.  */
#if ! defined YYLTYPE && ! defined YYLTYPE_IS_DECLARED
typedef struct YYLTYPE YYLTYPE;
struct YYLTYPE
{
  int first_line;
  int first_column;
  int last_line;
  int last_column;
};
# define YYLTYPE_IS_DECLARED 1
# define YYLTYPE_IS_TRIVIAL 1
#endif



int parse_events_parse (void *_data, void *scanner);

#endif /* !YY_PARSE_EVENTS_UTIL_PARSE_EVENTS_BISON_H_INCLUDED  */

/* Copy the second part of user declarations.  */

#line 212 "util/parse-events-bison.c" /* yacc.c:358  */

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
#else
typedef signed char yytype_int8;
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
# elif ! defined YYSIZE_T
#  include <stddef.h> /* INFRINGES ON USER NAME SPACE */
#  define YYSIZE_T size_t
# else
#  define YYSIZE_T unsigned int
# endif
#endif

#define YYSIZE_MAXIMUM ((YYSIZE_T) -1)

#ifndef YY_
# if defined YYENABLE_NLS && YYENABLE_NLS
#  if ENABLE_NLS
#   include <libintl.h> /* INFRINGES ON USER NAME SPACE */
#   define YY_(Msgid) dgettext ("bison-runtime", Msgid)
#  endif
# endif
# ifndef YY_
#  define YY_(Msgid) Msgid
# endif
#endif

#ifndef YY_ATTRIBUTE
# if (defined __GNUC__                                               \
      && (2 < __GNUC__ || (__GNUC__ == 2 && 96 <= __GNUC_MINOR__)))  \
     || defined __SUNPRO_C && 0x5110 <= __SUNPRO_C
#  define YY_ATTRIBUTE(Spec) __attribute__(Spec)
# else
#  define YY_ATTRIBUTE(Spec) /* empty */
# endif
#endif

#ifndef YY_ATTRIBUTE_PURE
# define YY_ATTRIBUTE_PURE   YY_ATTRIBUTE ((__pure__))
#endif

#ifndef YY_ATTRIBUTE_UNUSED
# define YY_ATTRIBUTE_UNUSED YY_ATTRIBUTE ((__unused__))
#endif

#if !defined _Noreturn \
     && (!defined __STDC_VERSION__ || __STDC_VERSION__ < 201112)
# if defined _MSC_VER && 1200 <= _MSC_VER
#  define _Noreturn __declspec (noreturn)
# else
#  define _Noreturn YY_ATTRIBUTE ((__noreturn__))
# endif
#endif

/* Suppress unused-variable warnings by "using" E.  */
#if ! defined lint || defined __GNUC__
# define YYUSE(E) ((void) (E))
#else
# define YYUSE(E) /* empty */
#endif

#if defined __GNUC__ && 407 <= __GNUC__ * 100 + __GNUC_MINOR__
/* Suppress an incorrect diagnostic about yylval being uninitialized.  */
# define YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN \
    _Pragma ("GCC diagnostic push") \
    _Pragma ("GCC diagnostic ignored \"-Wuninitialized\"")\
    _Pragma ("GCC diagnostic ignored \"-Wmaybe-uninitialized\"")
# define YY_IGNORE_MAYBE_UNINITIALIZED_END \
    _Pragma ("GCC diagnostic pop")
#else
# define YY_INITIAL_VALUE(Value) Value
#endif
#ifndef YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
# define YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
# define YY_IGNORE_MAYBE_UNINITIALIZED_END
#endif
#ifndef YY_INITIAL_VALUE
# define YY_INITIAL_VALUE(Value) /* Nothing. */
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
#    if ! defined _ALLOCA_H && ! defined EXIT_SUCCESS
#     include <stdlib.h> /* INFRINGES ON USER NAME SPACE */
      /* Use EXIT_SUCCESS as a witness for stdlib.h.  */
#     ifndef EXIT_SUCCESS
#      define EXIT_SUCCESS 0
#     endif
#    endif
#   endif
#  endif
# endif

# ifdef YYSTACK_ALLOC
   /* Pacify GCC's 'empty if-body' warning.  */
#  define YYSTACK_FREE(Ptr) do { /* empty */; } while (0)
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
#  if (defined __cplusplus && ! defined EXIT_SUCCESS \
       && ! ((defined YYMALLOC || defined malloc) \
             && (defined YYFREE || defined free)))
#   include <stdlib.h> /* INFRINGES ON USER NAME SPACE */
#   ifndef EXIT_SUCCESS
#    define EXIT_SUCCESS 0
#   endif
#  endif
#  ifndef YYMALLOC
#   define YYMALLOC malloc
#   if ! defined malloc && ! defined EXIT_SUCCESS
void *malloc (YYSIZE_T); /* INFRINGES ON USER NAME SPACE */
#   endif
#  endif
#  ifndef YYFREE
#   define YYFREE free
#   if ! defined free && ! defined EXIT_SUCCESS
void free (void *); /* INFRINGES ON USER NAME SPACE */
#   endif
#  endif
# endif
#endif /* ! defined yyoverflow || YYERROR_VERBOSE */


#if (! defined yyoverflow \
     && (! defined __cplusplus \
         || (defined YYLTYPE_IS_TRIVIAL && YYLTYPE_IS_TRIVIAL \
             && defined YYSTYPE_IS_TRIVIAL && YYSTYPE_IS_TRIVIAL)))

/* A type that is properly aligned for any stack member.  */
union yyalloc
{
  yytype_int16 yyss_alloc;
  YYSTYPE yyvs_alloc;
  YYLTYPE yyls_alloc;
};

/* The size of the maximum gap between one aligned stack and the next.  */
# define YYSTACK_GAP_MAXIMUM (sizeof (union yyalloc) - 1)

/* The size of an array large to enough to hold all stacks, each with
   N elements.  */
# define YYSTACK_BYTES(N) \
     ((N) * (sizeof (yytype_int16) + sizeof (YYSTYPE) + sizeof (YYLTYPE)) \
      + 2 * YYSTACK_GAP_MAXIMUM)

# define YYCOPY_NEEDED 1

/* Relocate STACK from its old location to the new one.  The
   local variables YYSIZE and YYSTACKSIZE give the old and new number of
   elements in the stack, and YYPTR gives the new location of the
   stack.  Advance YYPTR to a properly aligned location for the next
   stack.  */
# define YYSTACK_RELOCATE(Stack_alloc, Stack)                           \
    do                                                                  \
      {                                                                 \
        YYSIZE_T yynewbytes;                                            \
        YYCOPY (&yyptr->Stack_alloc, Stack, yysize);                    \
        Stack = &yyptr->Stack_alloc;                                    \
        yynewbytes = yystacksize * sizeof (*Stack) + YYSTACK_GAP_MAXIMUM; \
        yyptr += yynewbytes / sizeof (*yyptr);                          \
      }                                                                 \
    while (0)

#endif

#if defined YYCOPY_NEEDED && YYCOPY_NEEDED
/* Copy COUNT objects from SRC to DST.  The source and destination do
   not overlap.  */
# ifndef YYCOPY
#  if defined __GNUC__ && 1 < __GNUC__
#   define YYCOPY(Dst, Src, Count) \
      __builtin_memcpy (Dst, Src, (Count) * sizeof (*(Src)))
#  else
#   define YYCOPY(Dst, Src, Count)              \
      do                                        \
        {                                       \
          YYSIZE_T yyi;                         \
          for (yyi = 0; yyi < (Count); yyi++)   \
            (Dst)[yyi] = (Src)[yyi];            \
        }                                       \
      while (0)
#  endif
# endif
#endif /* !YYCOPY_NEEDED */

/* YYFINAL -- State number of the termination state.  */
#define YYFINAL  41
/* YYLAST -- Last index in YYTABLE.  */
#define YYLAST   117

/* YYNTOKENS -- Number of terminals.  */
#define YYNTOKENS  32
/* YYNNTS -- Number of nonterminals.  */
#define YYNNTS  26
/* YYNRULES -- Number of rules.  */
#define YYNRULES  66
/* YYNSTATES -- Number of states.  */
#define YYNSTATES  111

/* YYTRANSLATE[YYX] -- Symbol number corresponding to YYX as returned
   by yylex, with out-of-bounds checking.  */
#define YYUNDEFTOK  2
#define YYMAXUTOK   279

#define YYTRANSLATE(YYX)                                                \
  ((unsigned int) (YYX) <= YYMAXUTOK ? yytranslate[YYX] : YYUNDEFTOK)

/* YYTRANSLATE[TOKEN-NUM] -- Symbol number corresponding to TOKEN-NUM
   as returned by yylex, without out-of-bounds checking.  */
static const yytype_uint8 yytranslate[] =
{
       0,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,    25,    30,     2,    29,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,    26,     2,
       2,    31,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,    27,     2,    28,     2,     2,     2,     2,
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
       5,     6,     7,     8,     9,    10,    11,    12,    13,    14,
      15,    16,    17,    18,    19,    20,    21,    22,    23,    24
};

#if YYDEBUG
  /* YYRLINE[YYN] -- Source line where rule number YYN was defined.  */
static const yytype_uint16 yyrline[] =
{
       0,   100,   100,   102,   104,   112,   121,   130,   132,   135,
     143,   146,   155,   165,   174,   176,   179,   192,   195,   202,
     204,   205,   206,   207,   208,   209,   210,   211,   214,   225,
     235,   253,   274,   276,   279,   292,   305,   315,   325,   336,
     347,   358,   369,   381,   398,   416,   428,   436,   447,   458,
     469,   479,   486,   496,   508,   517,   526,   535,   544,   553,
     561,   569,   577,   577,   579,   579,   579
};
#endif

#if YYDEBUG || YYERROR_VERBOSE || 0
/* YYTNAME[SYMBOL-NUM] -- String name of the symbol SYMBOL-NUM.
   First, the terminals, then, starting at YYNTOKENS, nonterminals.  */
static const char *const yytname[] =
{
  "$end", "error", "$undefined", "PE_START_EVENTS", "PE_START_TERMS",
  "PE_VALUE", "PE_VALUE_SYM_HW", "PE_VALUE_SYM_SW", "PE_RAW", "PE_TERM",
  "PE_EVENT_NAME", "PE_NAME", "PE_BPF_OBJECT", "PE_BPF_SOURCE",
  "PE_MODIFIER_EVENT", "PE_MODIFIER_BP", "PE_NAME_CACHE_TYPE",
  "PE_NAME_CACHE_OP_RESULT", "PE_PREFIX_MEM", "PE_PREFIX_RAW",
  "PE_PREFIX_GROUP", "PE_ERROR", "PE_PMU_EVENT_PRE", "PE_PMU_EVENT_SUF",
  "PE_KERNEL_PMU_EVENT", "','", "':'", "'{'", "'}'", "'/'", "'-'", "'='",
  "$accept", "start", "start_events", "groups", "group", "group_def",
  "events", "event", "event_mod", "event_name", "event_def", "event_pmu",
  "value_sym", "event_legacy_symbol", "event_legacy_cache",
  "event_legacy_mem", "event_legacy_tracepoint", "tracepoint_name",
  "event_legacy_numeric", "event_legacy_raw", "event_bpf_file",
  "start_terms", "event_config", "event_term", "sep_dc", "sep_slash_dc", YY_NULLPTR
};
#endif

# ifdef YYPRINT
/* YYTOKNUM[NUM] -- (External) token number corresponding to the
   (internal) symbol number NUM (which must be that of a token).  */
static const yytype_uint16 yytoknum[] =
{
       0,   256,   257,   258,   259,   260,   261,   262,   263,   264,
     265,   266,   267,   268,   269,   270,   271,   272,   273,   274,
     275,   276,   277,   278,   279,    44,    58,   123,   125,    47,
      45,    61
};
# endif

#define YYPACT_NINF -38

#define yypact_value_is_default(Yystate) \
  (!!((Yystate) == (-38)))

#define YYTABLE_NINF -1

#define yytable_value_is_error(Yytable_value) \
  0

  /* YYPACT[STATE-NUM] -- Index in YYTABLE of the portion describing
     STATE-NUM.  */
static const yytype_int8 yypact[] =
{
      62,     8,    22,    17,    -1,   -38,   -38,   -38,    56,    30,
     -38,   -38,     4,    35,    18,    66,    31,   -38,    57,   -38,
      68,   -38,   -38,    77,   -38,   -38,    58,   -38,    66,   -38,
      66,    67,    66,    66,   -38,   -38,    64,    69,   -38,    72,
     -38,   -38,    93,    47,   -38,    88,    31,     0,    90,    85,
      59,    80,   -38,   -38,    61,   -38,     8,    91,   -38,   -38,
      22,   -38,   -38,   -38,    22,   -38,   -38,    -4,    40,    22,
     -38,   -38,    65,   -38,   -17,    78,    76,    92,   103,   -38,
      66,    31,   -38,   -38,   -38,   -38,    25,    50,   -38,   -38,
     -38,   -38,   -38,   -38,   -38,   -38,    98,    94,    66,    84,
     -38,   -38,   -38,   -38,   -38,   -38,   -38,    97,   -38,    66,
     -38
};

  /* YYDEFACT[STATE-NUM] -- Default reduction number in state STATE-NUM.
     Performed when YYTABLE does not specify something else to do.  Zero
     means the default is an error.  */
static const yytype_uint8 yydefact[] =
{
       0,     0,     0,     0,     0,    32,    33,    48,     0,     0,
      49,    50,    38,     0,     0,    63,     0,     2,     4,     7,
      10,     8,    15,    17,    19,    20,    66,    21,    63,    23,
      63,    43,    63,    63,    27,    58,    61,    57,     3,    51,
      53,     1,     0,     0,    18,     0,     0,     0,     0,     0,
      63,     0,    62,    30,     0,    14,     0,     0,    16,    65,
      64,    35,    22,    24,     0,    25,    26,     0,     0,     0,
      47,    46,     0,    29,     0,     0,    37,    62,     0,    42,
      63,     0,    12,     5,     6,     9,     0,     0,    60,    59,
      55,    56,    54,    52,    11,    28,     0,     0,    63,    63,
      31,    13,    34,    44,    45,    36,    41,    62,    40,    63,
      39
};

  /* YYPGOTO[NTERM-NUM].  */
static const yytype_int8 yypgoto[] =
{
     -38,   -38,   -38,   -38,    60,   -38,    71,     2,   -38,   -38,
     105,   -38,   -38,   -38,   -38,   -38,   -38,   -38,   -38,   -38,
     -38,   -38,   -37,    45,   -28,   -38
};

  /* YYDEFGOTO[NTERM-NUM].  */
static const yytype_int8 yydefgoto[] =
{
      -1,     3,    17,    18,    19,    20,    54,    55,    22,    23,
      24,    25,    26,    27,    28,    29,    30,    31,    32,    33,
      34,    38,    39,    40,    53,    61
};

  /* YYTABLE[YYPACT[STATE-NUM]] -- What to do in state STATE-NUM.  If
     positive, shift that token.  If negative, reduce the rule whose
     number is the opposite.  If YYTABLE_NINF, syntax error.  */
static const yytype_uint8 yytable[] =
{
      62,    88,    63,    21,    65,    66,    35,    89,    69,    36,
      74,    37,    95,     4,     5,     6,     7,    41,     8,     9,
      10,    11,    79,    86,    12,    42,    13,    87,    35,    73,
      14,    36,    15,    37,    49,    16,     4,     5,     6,     7,
      50,     8,    43,    10,    11,    90,    91,    12,    51,    13,
      69,    92,   100,    14,   102,    15,    45,    46,    84,    47,
      48,     4,     5,     6,     7,     1,     2,    43,    10,    11,
     106,   108,    12,    45,    13,    69,    47,    48,    14,   103,
      15,   110,    56,   101,    59,    77,    81,    60,    78,    82,
      81,    58,    52,    94,    57,    67,    64,    69,    70,    71,
      68,    75,    76,    80,    96,    85,    97,    98,    99,   104,
     107,   105,   109,    44,    93,     0,    83,    72
};

static const yytype_int8 yycheck[] =
{
      28,     5,    30,     1,    32,    33,     6,    11,    25,     9,
      47,    11,    29,     5,     6,     7,     8,     0,    10,    11,
      12,    13,    50,    60,    16,    26,    18,    64,     6,    29,
      22,     9,    24,    11,    30,    27,     5,     6,     7,     8,
       5,    10,    11,    12,    13,     5,     6,    16,    30,    18,
      25,    11,    80,    22,    29,    24,    26,    27,    56,    29,
      30,     5,     6,     7,     8,     3,     4,    11,    12,    13,
      98,    99,    16,    26,    18,    25,    29,    30,    22,    29,
      24,   109,    25,    81,    26,    26,    25,    29,    29,    28,
      25,    14,    26,    28,    26,    31,    29,    25,     5,    11,
      31,    11,    17,    23,    26,    14,    30,    15,     5,    11,
      26,    17,    15,     8,    69,    -1,    56,    46
};

  /* YYSTOS[STATE-NUM] -- The (internal number of the) accessing
     symbol of state STATE-NUM.  */
static const yytype_uint8 yystos[] =
{
       0,     3,     4,    33,     5,     6,     7,     8,    10,    11,
      12,    13,    16,    18,    22,    24,    27,    34,    35,    36,
      37,    39,    40,    41,    42,    43,    44,    45,    46,    47,
      48,    49,    50,    51,    52,     6,     9,    11,    53,    54,
      55,     0,    26,    11,    42,    26,    27,    29,    30,    30,
       5,    30,    26,    56,    38,    39,    25,    26,    14,    26,
      29,    57,    56,    56,    29,    56,    56,    31,    31,    25,
       5,    11,    38,    29,    54,    11,    17,    26,    29,    56,
      23,    25,    28,    36,    39,    14,    54,    54,     5,    11,
       5,     6,    11,    55,    28,    29,    26,    30,    15,     5,
      56,    39,    29,    29,    11,    17,    56,    26,    56,    15,
      56
};

  /* YYR1[YYN] -- Symbol number of symbol that rule YYN derives.  */
static const yytype_uint8 yyr1[] =
{
       0,    32,    33,    33,    34,    35,    35,    35,    35,    36,
      36,    37,    37,    38,    38,    39,    40,    40,    41,    41,
      42,    42,    42,    42,    42,    42,    42,    42,    43,    43,
      43,    43,    44,    44,    45,    45,    46,    46,    46,    47,
      47,    47,    47,    48,    48,    49,    49,    50,    51,    52,
      52,    53,    54,    54,    55,    55,    55,    55,    55,    55,
      55,    55,    56,    56,    57,    57,    57
};

  /* YYR2[YYN] -- Number of symbols on the right hand side of rule YYN.  */
static const yytype_uint8 yyr2[] =
{
       0,     2,     2,     2,     1,     3,     3,     1,     1,     3,
       1,     4,     3,     3,     1,     1,     2,     1,     2,     1,
       1,     1,     2,     1,     2,     2,     2,     1,     4,     3,
       2,     4,     1,     1,     4,     2,     5,     3,     1,     7,
       5,     5,     3,     1,     4,     5,     3,     3,     1,     1,
       1,     1,     3,     1,     3,     3,     3,     1,     1,     3,
       3,     1,     1,     0,     1,     1,     0
};


#define yyerrok         (yyerrstatus = 0)
#define yyclearin       (yychar = YYEMPTY)
#define YYEMPTY         (-2)
#define YYEOF           0

#define YYACCEPT        goto yyacceptlab
#define YYABORT         goto yyabortlab
#define YYERROR         goto yyerrorlab


#define YYRECOVERING()  (!!yyerrstatus)

#define YYBACKUP(Token, Value)                                  \
do                                                              \
  if (yychar == YYEMPTY)                                        \
    {                                                           \
      yychar = (Token);                                         \
      yylval = (Value);                                         \
      YYPOPSTACK (yylen);                                       \
      yystate = *yyssp;                                         \
      goto yybackup;                                            \
    }                                                           \
  else                                                          \
    {                                                           \
      yyerror (&yylloc, _data, scanner, YY_("syntax error: cannot back up")); \
      YYERROR;                                                  \
    }                                                           \
while (0)

/* Error token number */
#define YYTERROR        1
#define YYERRCODE       256


/* YYLLOC_DEFAULT -- Set CURRENT to span from RHS[1] to RHS[N].
   If N is 0, then set CURRENT to the empty location which ends
   the previous symbol: RHS[0] (always defined).  */

#ifndef YYLLOC_DEFAULT
# define YYLLOC_DEFAULT(Current, Rhs, N)                                \
    do                                                                  \
      if (N)                                                            \
        {                                                               \
          (Current).first_line   = YYRHSLOC (Rhs, 1).first_line;        \
          (Current).first_column = YYRHSLOC (Rhs, 1).first_column;      \
          (Current).last_line    = YYRHSLOC (Rhs, N).last_line;         \
          (Current).last_column  = YYRHSLOC (Rhs, N).last_column;       \
        }                                                               \
      else                                                              \
        {                                                               \
          (Current).first_line   = (Current).last_line   =              \
            YYRHSLOC (Rhs, 0).last_line;                                \
          (Current).first_column = (Current).last_column =              \
            YYRHSLOC (Rhs, 0).last_column;                              \
        }                                                               \
    while (0)
#endif

#define YYRHSLOC(Rhs, K) ((Rhs)[K])


/* Enable debugging if requested.  */
#if YYDEBUG

# ifndef YYFPRINTF
#  include <stdio.h> /* INFRINGES ON USER NAME SPACE */
#  define YYFPRINTF fprintf
# endif

# define YYDPRINTF(Args)                        \
do {                                            \
  if (yydebug)                                  \
    YYFPRINTF Args;                             \
} while (0)


/* YY_LOCATION_PRINT -- Print the location on the stream.
   This macro was not mandated originally: define only if we know
   we won't break user code: when these are the locations we know.  */

#ifndef YY_LOCATION_PRINT
# if defined YYLTYPE_IS_TRIVIAL && YYLTYPE_IS_TRIVIAL

/* Print *YYLOCP on YYO.  Private, do not rely on its existence. */

YY_ATTRIBUTE_UNUSED
static unsigned
yy_location_print_ (FILE *yyo, YYLTYPE const * const yylocp)
{
  unsigned res = 0;
  int end_col = 0 != yylocp->last_column ? yylocp->last_column - 1 : 0;
  if (0 <= yylocp->first_line)
    {
      res += YYFPRINTF (yyo, "%d", yylocp->first_line);
      if (0 <= yylocp->first_column)
        res += YYFPRINTF (yyo, ".%d", yylocp->first_column);
    }
  if (0 <= yylocp->last_line)
    {
      if (yylocp->first_line < yylocp->last_line)
        {
          res += YYFPRINTF (yyo, "-%d", yylocp->last_line);
          if (0 <= end_col)
            res += YYFPRINTF (yyo, ".%d", end_col);
        }
      else if (0 <= end_col && yylocp->first_column < end_col)
        res += YYFPRINTF (yyo, "-%d", end_col);
    }
  return res;
 }

#  define YY_LOCATION_PRINT(File, Loc)          \
  yy_location_print_ (File, &(Loc))

# else
#  define YY_LOCATION_PRINT(File, Loc) ((void) 0)
# endif
#endif


# define YY_SYMBOL_PRINT(Title, Type, Value, Location)                    \
do {                                                                      \
  if (yydebug)                                                            \
    {                                                                     \
      YYFPRINTF (stderr, "%s ", Title);                                   \
      yy_symbol_print (stderr,                                            \
                  Type, Value, Location, _data, scanner); \
      YYFPRINTF (stderr, "\n");                                           \
    }                                                                     \
} while (0)


/*----------------------------------------.
| Print this symbol's value on YYOUTPUT.  |
`----------------------------------------*/

static void
yy_symbol_value_print (FILE *yyoutput, int yytype, YYSTYPE const * const yyvaluep, YYLTYPE const * const yylocationp, void *_data, void *scanner)
{
  FILE *yyo = yyoutput;
  YYUSE (yyo);
  YYUSE (yylocationp);
  YYUSE (_data);
  YYUSE (scanner);
  if (!yyvaluep)
    return;
# ifdef YYPRINT
  if (yytype < YYNTOKENS)
    YYPRINT (yyoutput, yytoknum[yytype], *yyvaluep);
# endif
  YYUSE (yytype);
}


/*--------------------------------.
| Print this symbol on YYOUTPUT.  |
`--------------------------------*/

static void
yy_symbol_print (FILE *yyoutput, int yytype, YYSTYPE const * const yyvaluep, YYLTYPE const * const yylocationp, void *_data, void *scanner)
{
  YYFPRINTF (yyoutput, "%s %s (",
             yytype < YYNTOKENS ? "token" : "nterm", yytname[yytype]);

  YY_LOCATION_PRINT (yyoutput, *yylocationp);
  YYFPRINTF (yyoutput, ": ");
  yy_symbol_value_print (yyoutput, yytype, yyvaluep, yylocationp, _data, scanner);
  YYFPRINTF (yyoutput, ")");
}

/*------------------------------------------------------------------.
| yy_stack_print -- Print the state stack from its BOTTOM up to its |
| TOP (included).                                                   |
`------------------------------------------------------------------*/

static void
yy_stack_print (yytype_int16 *yybottom, yytype_int16 *yytop)
{
  YYFPRINTF (stderr, "Stack now");
  for (; yybottom <= yytop; yybottom++)
    {
      int yybot = *yybottom;
      YYFPRINTF (stderr, " %d", yybot);
    }
  YYFPRINTF (stderr, "\n");
}

# define YY_STACK_PRINT(Bottom, Top)                            \
do {                                                            \
  if (yydebug)                                                  \
    yy_stack_print ((Bottom), (Top));                           \
} while (0)


/*------------------------------------------------.
| Report that the YYRULE is going to be reduced.  |
`------------------------------------------------*/

static void
yy_reduce_print (yytype_int16 *yyssp, YYSTYPE *yyvsp, YYLTYPE *yylsp, int yyrule, void *_data, void *scanner)
{
  unsigned long int yylno = yyrline[yyrule];
  int yynrhs = yyr2[yyrule];
  int yyi;
  YYFPRINTF (stderr, "Reducing stack by rule %d (line %lu):\n",
             yyrule - 1, yylno);
  /* The symbols being reduced.  */
  for (yyi = 0; yyi < yynrhs; yyi++)
    {
      YYFPRINTF (stderr, "   $%d = ", yyi + 1);
      yy_symbol_print (stderr,
                       yystos[yyssp[yyi + 1 - yynrhs]],
                       &(yyvsp[(yyi + 1) - (yynrhs)])
                       , &(yylsp[(yyi + 1) - (yynrhs)])                       , _data, scanner);
      YYFPRINTF (stderr, "\n");
    }
}

# define YY_REDUCE_PRINT(Rule)          \
do {                                    \
  if (yydebug)                          \
    yy_reduce_print (yyssp, yyvsp, yylsp, Rule, _data, scanner); \
} while (0)

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
#ifndef YYINITDEPTH
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
static YYSIZE_T
yystrlen (const char *yystr)
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
static char *
yystpcpy (char *yydest, const char *yysrc)
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

/* Copy into *YYMSG, which is of size *YYMSG_ALLOC, an error message
   about the unexpected token YYTOKEN for the state stack whose top is
   YYSSP.

   Return 0 if *YYMSG was successfully written.  Return 1 if *YYMSG is
   not large enough to hold the message.  In that case, also set
   *YYMSG_ALLOC to the required number of bytes.  Return 2 if the
   required number of bytes is too large to store.  */
static int
yysyntax_error (YYSIZE_T *yymsg_alloc, char **yymsg,
                yytype_int16 *yyssp, int yytoken)
{
  YYSIZE_T yysize0 = yytnamerr (YY_NULLPTR, yytname[yytoken]);
  YYSIZE_T yysize = yysize0;
  enum { YYERROR_VERBOSE_ARGS_MAXIMUM = 5 };
  /* Internationalized format string. */
  const char *yyformat = YY_NULLPTR;
  /* Arguments of yyformat. */
  char const *yyarg[YYERROR_VERBOSE_ARGS_MAXIMUM];
  /* Number of reported tokens (one for the "unexpected", one per
     "expected"). */
  int yycount = 0;

  /* There are many possibilities here to consider:
     - If this state is a consistent state with a default action, then
       the only way this function was invoked is if the default action
       is an error action.  In that case, don't check for expected
       tokens because there are none.
     - The only way there can be no lookahead present (in yychar) is if
       this state is a consistent state with a default action.  Thus,
       detecting the absence of a lookahead is sufficient to determine
       that there is no unexpected or expected token to report.  In that
       case, just report a simple "syntax error".
     - Don't assume there isn't a lookahead just because this state is a
       consistent state with a default action.  There might have been a
       previous inconsistent state, consistent state with a non-default
       action, or user semantic action that manipulated yychar.
     - Of course, the expected token list depends on states to have
       correct lookahead information, and it depends on the parser not
       to perform extra reductions after fetching a lookahead from the
       scanner and before detecting a syntax error.  Thus, state merging
       (from LALR or IELR) and default reductions corrupt the expected
       token list.  However, the list is correct for canonical LR with
       one exception: it will still contain any token that will not be
       accepted due to an error action in a later state.
  */
  if (yytoken != YYEMPTY)
    {
      int yyn = yypact[*yyssp];
      yyarg[yycount++] = yytname[yytoken];
      if (!yypact_value_is_default (yyn))
        {
          /* Start YYX at -YYN if negative to avoid negative indexes in
             YYCHECK.  In other words, skip the first -YYN actions for
             this state because they are default actions.  */
          int yyxbegin = yyn < 0 ? -yyn : 0;
          /* Stay within bounds of both yycheck and yytname.  */
          int yychecklim = YYLAST - yyn + 1;
          int yyxend = yychecklim < YYNTOKENS ? yychecklim : YYNTOKENS;
          int yyx;

          for (yyx = yyxbegin; yyx < yyxend; ++yyx)
            if (yycheck[yyx + yyn] == yyx && yyx != YYTERROR
                && !yytable_value_is_error (yytable[yyx + yyn]))
              {
                if (yycount == YYERROR_VERBOSE_ARGS_MAXIMUM)
                  {
                    yycount = 1;
                    yysize = yysize0;
                    break;
                  }
                yyarg[yycount++] = yytname[yyx];
                {
                  YYSIZE_T yysize1 = yysize + yytnamerr (YY_NULLPTR, yytname[yyx]);
                  if (! (yysize <= yysize1
                         && yysize1 <= YYSTACK_ALLOC_MAXIMUM))
                    return 2;
                  yysize = yysize1;
                }
              }
        }
    }

  switch (yycount)
    {
# define YYCASE_(N, S)                      \
      case N:                               \
        yyformat = S;                       \
      break
      YYCASE_(0, YY_("syntax error"));
      YYCASE_(1, YY_("syntax error, unexpected %s"));
      YYCASE_(2, YY_("syntax error, unexpected %s, expecting %s"));
      YYCASE_(3, YY_("syntax error, unexpected %s, expecting %s or %s"));
      YYCASE_(4, YY_("syntax error, unexpected %s, expecting %s or %s or %s"));
      YYCASE_(5, YY_("syntax error, unexpected %s, expecting %s or %s or %s or %s"));
# undef YYCASE_
    }

  {
    YYSIZE_T yysize1 = yysize + yystrlen (yyformat);
    if (! (yysize <= yysize1 && yysize1 <= YYSTACK_ALLOC_MAXIMUM))
      return 2;
    yysize = yysize1;
  }

  if (*yymsg_alloc < yysize)
    {
      *yymsg_alloc = 2 * yysize;
      if (! (yysize <= *yymsg_alloc
             && *yymsg_alloc <= YYSTACK_ALLOC_MAXIMUM))
        *yymsg_alloc = YYSTACK_ALLOC_MAXIMUM;
      return 1;
    }

  /* Avoid sprintf, as that infringes on the user's name space.
     Don't have undefined behavior even if the translation
     produced a string with the wrong number of "%s"s.  */
  {
    char *yyp = *yymsg;
    int yyi = 0;
    while ((*yyp = *yyformat) != '\0')
      if (*yyp == '%' && yyformat[1] == 's' && yyi < yycount)
        {
          yyp += yytnamerr (yyp, yyarg[yyi++]);
          yyformat += 2;
        }
      else
        {
          yyp++;
          yyformat++;
        }
  }
  return 0;
}
#endif /* YYERROR_VERBOSE */

/*-----------------------------------------------.
| Release the memory associated to this symbol.  |
`-----------------------------------------------*/

static void
yydestruct (const char *yymsg, int yytype, YYSTYPE *yyvaluep, YYLTYPE *yylocationp, void *_data, void *scanner)
{
  YYUSE (yyvaluep);
  YYUSE (yylocationp);
  YYUSE (_data);
  YYUSE (scanner);
  if (!yymsg)
    yymsg = "Deleting";
  YY_SYMBOL_PRINT (yymsg, yytype, yyvaluep, yylocationp);

  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  YYUSE (yytype);
  YY_IGNORE_MAYBE_UNINITIALIZED_END
}




/*----------.
| yyparse.  |
`----------*/

int
yyparse (void *_data, void *scanner)
{
/* The lookahead symbol.  */
int yychar;


/* The semantic value of the lookahead symbol.  */
/* Default value used for initialization, for pacifying older GCCs
   or non-GCC compilers.  */
YY_INITIAL_VALUE (static YYSTYPE yyval_default;)
YYSTYPE yylval YY_INITIAL_VALUE (= yyval_default);

/* Location data for the lookahead symbol.  */
static YYLTYPE yyloc_default
# if defined YYLTYPE_IS_TRIVIAL && YYLTYPE_IS_TRIVIAL
  = { 1, 1, 1, 1 }
# endif
;
YYLTYPE yylloc = yyloc_default;

    /* Number of syntax errors so far.  */
    int yynerrs;

    int yystate;
    /* Number of tokens to shift before error messages enabled.  */
    int yyerrstatus;

    /* The stacks and their tools:
       'yyss': related to states.
       'yyvs': related to semantic values.
       'yyls': related to locations.

       Refer to the stacks through separate pointers, to allow yyoverflow
       to reallocate them elsewhere.  */

    /* The state stack.  */
    yytype_int16 yyssa[YYINITDEPTH];
    yytype_int16 *yyss;
    yytype_int16 *yyssp;

    /* The semantic value stack.  */
    YYSTYPE yyvsa[YYINITDEPTH];
    YYSTYPE *yyvs;
    YYSTYPE *yyvsp;

    /* The location stack.  */
    YYLTYPE yylsa[YYINITDEPTH];
    YYLTYPE *yyls;
    YYLTYPE *yylsp;

    /* The locations where the error started and ended.  */
    YYLTYPE yyerror_range[3];

    YYSIZE_T yystacksize;

  int yyn;
  int yyresult;
  /* Lookahead token as an internal (translated) token number.  */
  int yytoken = 0;
  /* The variables used to return semantic value and location from the
     action routines.  */
  YYSTYPE yyval;
  YYLTYPE yyloc;

#if YYERROR_VERBOSE
  /* Buffer for error messages, and its allocated size.  */
  char yymsgbuf[128];
  char *yymsg = yymsgbuf;
  YYSIZE_T yymsg_alloc = sizeof yymsgbuf;
#endif

#define YYPOPSTACK(N)   (yyvsp -= (N), yyssp -= (N), yylsp -= (N))

  /* The number of symbols on the RHS of the reduced rule.
     Keep to zero when no symbol should be popped.  */
  int yylen = 0;

  yyssp = yyss = yyssa;
  yyvsp = yyvs = yyvsa;
  yylsp = yyls = yylsa;
  yystacksize = YYINITDEPTH;

  YYDPRINTF ((stderr, "Starting parse\n"));

  yystate = 0;
  yyerrstatus = 0;
  yynerrs = 0;
  yychar = YYEMPTY; /* Cause a token to be read.  */
  yylsp[0] = yylloc;
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
        YYLTYPE *yyls1 = yyls;

        /* Each stack pointer address is followed by the size of the
           data in use in that stack, in bytes.  This used to be a
           conditional around just the two extra args, but that might
           be undefined if yyoverflow is a macro.  */
        yyoverflow (YY_("memory exhausted"),
                    &yyss1, yysize * sizeof (*yyssp),
                    &yyvs1, yysize * sizeof (*yyvsp),
                    &yyls1, yysize * sizeof (*yylsp),
                    &yystacksize);

        yyls = yyls1;
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
        YYSTACK_RELOCATE (yyss_alloc, yyss);
        YYSTACK_RELOCATE (yyvs_alloc, yyvs);
        YYSTACK_RELOCATE (yyls_alloc, yyls);
#  undef YYSTACK_RELOCATE
        if (yyss1 != yyssa)
          YYSTACK_FREE (yyss1);
      }
# endif
#endif /* no yyoverflow */

      yyssp = yyss + yysize - 1;
      yyvsp = yyvs + yysize - 1;
      yylsp = yyls + yysize - 1;

      YYDPRINTF ((stderr, "Stack size increased to %lu\n",
                  (unsigned long int) yystacksize));

      if (yyss + yystacksize - 1 <= yyssp)
        YYABORT;
    }

  YYDPRINTF ((stderr, "Entering state %d\n", yystate));

  if (yystate == YYFINAL)
    YYACCEPT;

  goto yybackup;

/*-----------.
| yybackup.  |
`-----------*/
yybackup:

  /* Do appropriate processing given the current state.  Read a
     lookahead token if we need one and don't already have one.  */

  /* First try to decide what to do without reference to lookahead token.  */
  yyn = yypact[yystate];
  if (yypact_value_is_default (yyn))
    goto yydefault;

  /* Not known => get a lookahead token if don't already have one.  */

  /* YYCHAR is either YYEMPTY or YYEOF or a valid lookahead symbol.  */
  if (yychar == YYEMPTY)
    {
      YYDPRINTF ((stderr, "Reading a token: "));
      yychar = yylex (&yylval, &yylloc, scanner);
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
      if (yytable_value_is_error (yyn))
        goto yyerrlab;
      yyn = -yyn;
      goto yyreduce;
    }

  /* Count tokens shifted since error; after three, turn off error
     status.  */
  if (yyerrstatus)
    yyerrstatus--;

  /* Shift the lookahead token.  */
  YY_SYMBOL_PRINT ("Shifting", yytoken, &yylval, &yylloc);

  /* Discard the shifted token.  */
  yychar = YYEMPTY;

  yystate = yyn;
  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  *++yyvsp = yylval;
  YY_IGNORE_MAYBE_UNINITIALIZED_END
  *++yylsp = yylloc;
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
     '$$ = $1'.

     Otherwise, the following line sets YYVAL to garbage.
     This behavior is undocumented and Bison
     users should not rely upon it.  Assigning to YYVAL
     unconditionally makes the parser a bit smaller, and it avoids a
     GCC warning that YYVAL may be used uninitialized.  */
  yyval = yyvsp[1-yylen];

  /* Default location.  */
  YYLLOC_DEFAULT (yyloc, (yylsp - yylen), yylen);
  YY_REDUCE_PRINT (yyn);
  switch (yyn)
    {
        case 4:
#line 105 "util/parse-events.y" /* yacc.c:1646  */
    {
	struct parse_events_evlist *data = _data;

	parse_events_update_lists((yyvsp[0].head), &data->list);
}
#line 1479 "util/parse-events-bison.c" /* yacc.c:1646  */
    break;

  case 5:
#line 113 "util/parse-events.y" /* yacc.c:1646  */
    {
	struct list_head *list  = (yyvsp[-2].head);
	struct list_head *group = (yyvsp[0].head);

	parse_events_update_lists(group, list);
	(yyval.head) = list;
}
#line 1491 "util/parse-events-bison.c" /* yacc.c:1646  */
    break;

  case 6:
#line 122 "util/parse-events.y" /* yacc.c:1646  */
    {
	struct list_head *list  = (yyvsp[-2].head);
	struct list_head *event = (yyvsp[0].head);

	parse_events_update_lists(event, list);
	(yyval.head) = list;
}
#line 1503 "util/parse-events-bison.c" /* yacc.c:1646  */
    break;

  case 9:
#line 136 "util/parse-events.y" /* yacc.c:1646  */
    {
	struct list_head *list = (yyvsp[-2].head);

	ABORT_ON(parse_events__modifier_group(list, (yyvsp[0].str)));
	(yyval.head) = list;
}
#line 1514 "util/parse-events-bison.c" /* yacc.c:1646  */
    break;

  case 11:
#line 147 "util/parse-events.y" /* yacc.c:1646  */
    {
	struct list_head *list = (yyvsp[-1].head);

	inc_group_count(list, _data);
	parse_events__set_leader((yyvsp[-3].str), list);
	(yyval.head) = list;
}
#line 1526 "util/parse-events-bison.c" /* yacc.c:1646  */
    break;

  case 12:
#line 156 "util/parse-events.y" /* yacc.c:1646  */
    {
	struct list_head *list = (yyvsp[-1].head);

	inc_group_count(list, _data);
	parse_events__set_leader(NULL, list);
	(yyval.head) = list;
}
#line 1538 "util/parse-events-bison.c" /* yacc.c:1646  */
    break;

  case 13:
#line 166 "util/parse-events.y" /* yacc.c:1646  */
    {
	struct list_head *event = (yyvsp[0].head);
	struct list_head *list  = (yyvsp[-2].head);

	parse_events_update_lists(event, list);
	(yyval.head) = list;
}
#line 1550 "util/parse-events-bison.c" /* yacc.c:1646  */
    break;

  case 16:
#line 180 "util/parse-events.y" /* yacc.c:1646  */
    {
	struct list_head *list = (yyvsp[-1].head);

	/*
	 * Apply modifier on all events added by single event definition
	 * (there could be more events added for multiple tracepoint
	 * definitions via '*?'.
	 */
	ABORT_ON(parse_events__modifier_event(list, (yyvsp[0].str), false));
	(yyval.head) = list;
}
#line 1566 "util/parse-events-bison.c" /* yacc.c:1646  */
    break;

  case 18:
#line 196 "util/parse-events.y" /* yacc.c:1646  */
    {
	ABORT_ON(parse_events_name((yyvsp[0].head), (yyvsp[-1].str)));
	free((yyvsp[-1].str));
	(yyval.head) = (yyvsp[0].head);
}
#line 1576 "util/parse-events-bison.c" /* yacc.c:1646  */
    break;

  case 28:
#line 215 "util/parse-events.y" /* yacc.c:1646  */
    {
	struct parse_events_evlist *data = _data;
	struct list_head *list;

	ALLOC_LIST(list);
	ABORT_ON(parse_events_add_pmu(data, list, (yyvsp[-3].str), (yyvsp[-1].head)));
	parse_events__free_terms((yyvsp[-1].head));
	(yyval.head) = list;
}
#line 1590 "util/parse-events-bison.c" /* yacc.c:1646  */
    break;

  case 29:
#line 226 "util/parse-events.y" /* yacc.c:1646  */
    {
	struct parse_events_evlist *data = _data;
	struct list_head *list;

	ALLOC_LIST(list);
	ABORT_ON(parse_events_add_pmu(data, list, (yyvsp[-2].str), NULL));
	(yyval.head) = list;
}
#line 1603 "util/parse-events-bison.c" /* yacc.c:1646  */
    break;

  case 30:
#line 236 "util/parse-events.y" /* yacc.c:1646  */
    {
	struct parse_events_evlist *data = _data;
	struct list_head *head;
	struct parse_events_term *term;
	struct list_head *list;

	ALLOC_LIST(head);
	ABORT_ON(parse_events_term__num(&term, PARSE_EVENTS__TERM_TYPE_USER,
					(yyvsp[-1].str), 1, &(yylsp[-1]), NULL));
	list_add_tail(&term->list, head);

	ALLOC_LIST(list);
	ABORT_ON(parse_events_add_pmu(data, list, "cpu", head));
	parse_events__free_terms(head);
	(yyval.head) = list;
}
#line 1624 "util/parse-events-bison.c" /* yacc.c:1646  */
    break;

  case 31:
#line 254 "util/parse-events.y" /* yacc.c:1646  */
    {
	struct parse_events_evlist *data = _data;
	struct list_head *head;
	struct parse_events_term *term;
	struct list_head *list;
	char pmu_name[128];
	snprintf(&pmu_name, 128, "%s-%s", (yyvsp[-3].str), (yyvsp[-1].str));

	ALLOC_LIST(head);
	ABORT_ON(parse_events_term__num(&term, PARSE_EVENTS__TERM_TYPE_USER,
					&pmu_name, 1, &(yylsp[-3]), NULL));
	list_add_tail(&term->list, head);

	ALLOC_LIST(list);
	ABORT_ON(parse_events_add_pmu(data, list, "cpu", head));
	parse_events__free_terms(head);
	(yyval.head) = list;
}
#line 1647 "util/parse-events-bison.c" /* yacc.c:1646  */
    break;

  case 34:
#line 280 "util/parse-events.y" /* yacc.c:1646  */
    {
	struct parse_events_evlist *data = _data;
	struct list_head *list;
	int type = (yyvsp[-3].num) >> 16;
	int config = (yyvsp[-3].num) & 255;

	ALLOC_LIST(list);
	ABORT_ON(parse_events_add_numeric(data, list, type, config, (yyvsp[-1].head)));
	parse_events__free_terms((yyvsp[-1].head));
	(yyval.head) = list;
}
#line 1663 "util/parse-events-bison.c" /* yacc.c:1646  */
    break;

  case 35:
#line 293 "util/parse-events.y" /* yacc.c:1646  */
    {
	struct parse_events_evlist *data = _data;
	struct list_head *list;
	int type = (yyvsp[-1].num) >> 16;
	int config = (yyvsp[-1].num) & 255;

	ALLOC_LIST(list);
	ABORT_ON(parse_events_add_numeric(data, list, type, config, NULL));
	(yyval.head) = list;
}
#line 1678 "util/parse-events-bison.c" /* yacc.c:1646  */
    break;

  case 36:
#line 306 "util/parse-events.y" /* yacc.c:1646  */
    {
	struct parse_events_evlist *data = _data;
	struct list_head *list;

	ALLOC_LIST(list);
	ABORT_ON(parse_events_add_cache(list, &data->idx, (yyvsp[-4].str), (yyvsp[-2].str), (yyvsp[0].str)));
	(yyval.head) = list;
}
#line 1691 "util/parse-events-bison.c" /* yacc.c:1646  */
    break;

  case 37:
#line 316 "util/parse-events.y" /* yacc.c:1646  */
    {
	struct parse_events_evlist *data = _data;
	struct list_head *list;

	ALLOC_LIST(list);
	ABORT_ON(parse_events_add_cache(list, &data->idx, (yyvsp[-2].str), (yyvsp[0].str), NULL));
	(yyval.head) = list;
}
#line 1704 "util/parse-events-bison.c" /* yacc.c:1646  */
    break;

  case 38:
#line 326 "util/parse-events.y" /* yacc.c:1646  */
    {
	struct parse_events_evlist *data = _data;
	struct list_head *list;

	ALLOC_LIST(list);
	ABORT_ON(parse_events_add_cache(list, &data->idx, (yyvsp[0].str), NULL, NULL));
	(yyval.head) = list;
}
#line 1717 "util/parse-events-bison.c" /* yacc.c:1646  */
    break;

  case 39:
#line 337 "util/parse-events.y" /* yacc.c:1646  */
    {
	struct parse_events_evlist *data = _data;
	struct list_head *list;

	ALLOC_LIST(list);
	ABORT_ON(parse_events_add_breakpoint(list, &data->idx,
					     (void *) (yyvsp[-5].num), (yyvsp[-1].str), (yyvsp[-3].num)));
	(yyval.head) = list;
}
#line 1731 "util/parse-events-bison.c" /* yacc.c:1646  */
    break;

  case 40:
#line 348 "util/parse-events.y" /* yacc.c:1646  */
    {
	struct parse_events_evlist *data = _data;
	struct list_head *list;

	ALLOC_LIST(list);
	ABORT_ON(parse_events_add_breakpoint(list, &data->idx,
					     (void *) (yyvsp[-3].num), NULL, (yyvsp[-1].num)));
	(yyval.head) = list;
}
#line 1745 "util/parse-events-bison.c" /* yacc.c:1646  */
    break;

  case 41:
#line 359 "util/parse-events.y" /* yacc.c:1646  */
    {
	struct parse_events_evlist *data = _data;
	struct list_head *list;

	ALLOC_LIST(list);
	ABORT_ON(parse_events_add_breakpoint(list, &data->idx,
					     (void *) (yyvsp[-3].num), (yyvsp[-1].str), 0));
	(yyval.head) = list;
}
#line 1759 "util/parse-events-bison.c" /* yacc.c:1646  */
    break;

  case 42:
#line 370 "util/parse-events.y" /* yacc.c:1646  */
    {
	struct parse_events_evlist *data = _data;
	struct list_head *list;

	ALLOC_LIST(list);
	ABORT_ON(parse_events_add_breakpoint(list, &data->idx,
					     (void *) (yyvsp[-1].num), NULL, 0));
	(yyval.head) = list;
}
#line 1773 "util/parse-events-bison.c" /* yacc.c:1646  */
    break;

  case 43:
#line 382 "util/parse-events.y" /* yacc.c:1646  */
    {
	struct parse_events_evlist *data = _data;
	struct parse_events_error *error = data->error;
	struct list_head *list;

	ALLOC_LIST(list);
	if (error)
		error->idx = (yylsp[0]).first_column;

	if (parse_events_add_tracepoint(list, &data->idx, (yyvsp[0].tracepoint_name).sys, (yyvsp[0].tracepoint_name).event,
					error, NULL))
		return -1;

	(yyval.head) = list;
}
#line 1793 "util/parse-events-bison.c" /* yacc.c:1646  */
    break;

  case 44:
#line 399 "util/parse-events.y" /* yacc.c:1646  */
    {
	struct parse_events_evlist *data = _data;
	struct parse_events_error *error = data->error;
	struct list_head *list;

	ALLOC_LIST(list);
	if (error)
		error->idx = (yylsp[-3]).first_column;

	if (parse_events_add_tracepoint(list, &data->idx, (yyvsp[-3].tracepoint_name).sys, (yyvsp[-3].tracepoint_name).event,
					error, (yyvsp[-1].head)))
		return -1;

	(yyval.head) = list;
}
#line 1813 "util/parse-events-bison.c" /* yacc.c:1646  */
    break;

  case 45:
#line 417 "util/parse-events.y" /* yacc.c:1646  */
    {
	char sys_name[128];
	struct tracepoint_name tracepoint;

	snprintf(&sys_name, 128, "%s-%s", (yyvsp[-4].str), (yyvsp[-2].str));
	tracepoint.sys = &sys_name;
	tracepoint.event = (yyvsp[0].str);

	(yyval.tracepoint_name) = tracepoint;
}
#line 1828 "util/parse-events-bison.c" /* yacc.c:1646  */
    break;

  case 46:
#line 429 "util/parse-events.y" /* yacc.c:1646  */
    {
	struct tracepoint_name tracepoint = {(yyvsp[-2].str), (yyvsp[0].str)};

	(yyval.tracepoint_name) = tracepoint;
}
#line 1838 "util/parse-events-bison.c" /* yacc.c:1646  */
    break;

  case 47:
#line 437 "util/parse-events.y" /* yacc.c:1646  */
    {
	struct parse_events_evlist *data = _data;
	struct list_head *list;

	ALLOC_LIST(list);
	ABORT_ON(parse_events_add_numeric(data, list, (u32)(yyvsp[-2].num), (yyvsp[0].num), NULL));
	(yyval.head) = list;
}
#line 1851 "util/parse-events-bison.c" /* yacc.c:1646  */
    break;

  case 48:
#line 448 "util/parse-events.y" /* yacc.c:1646  */
    {
	struct parse_events_evlist *data = _data;
	struct list_head *list;

	ALLOC_LIST(list);
	ABORT_ON(parse_events_add_numeric(data, list, PERF_TYPE_RAW, (yyvsp[0].num), NULL));
	(yyval.head) = list;
}
#line 1864 "util/parse-events-bison.c" /* yacc.c:1646  */
    break;

  case 49:
#line 459 "util/parse-events.y" /* yacc.c:1646  */
    {
	struct parse_events_evlist *data = _data;
	struct parse_events_error *error = data->error;
	struct list_head *list;

	ALLOC_LIST(list);
	ABORT_ON(parse_events_load_bpf(data, list, (yyvsp[0].str), false));
	(yyval.head) = list;
}
#line 1878 "util/parse-events-bison.c" /* yacc.c:1646  */
    break;

  case 50:
#line 470 "util/parse-events.y" /* yacc.c:1646  */
    {
	struct parse_events_evlist *data = _data;
	struct list_head *list;

	ALLOC_LIST(list);
	ABORT_ON(parse_events_load_bpf(data, list, (yyvsp[0].str), true));
	(yyval.head) = list;
}
#line 1891 "util/parse-events-bison.c" /* yacc.c:1646  */
    break;

  case 51:
#line 480 "util/parse-events.y" /* yacc.c:1646  */
    {
	struct parse_events_terms *data = _data;
	data->terms = (yyvsp[0].head);
}
#line 1900 "util/parse-events-bison.c" /* yacc.c:1646  */
    break;

  case 52:
#line 487 "util/parse-events.y" /* yacc.c:1646  */
    {
	struct list_head *head = (yyvsp[-2].head);
	struct parse_events_term *term = (yyvsp[0].term);

	ABORT_ON(!head);
	list_add_tail(&term->list, head);
	(yyval.head) = (yyvsp[-2].head);
}
#line 1913 "util/parse-events-bison.c" /* yacc.c:1646  */
    break;

  case 53:
#line 497 "util/parse-events.y" /* yacc.c:1646  */
    {
	struct list_head *head = malloc(sizeof(*head));
	struct parse_events_term *term = (yyvsp[0].term);

	ABORT_ON(!head);
	INIT_LIST_HEAD(head);
	list_add_tail(&term->list, head);
	(yyval.head) = head;
}
#line 1927 "util/parse-events-bison.c" /* yacc.c:1646  */
    break;

  case 54:
#line 509 "util/parse-events.y" /* yacc.c:1646  */
    {
	struct parse_events_term *term;

	ABORT_ON(parse_events_term__str(&term, PARSE_EVENTS__TERM_TYPE_USER,
					(yyvsp[-2].str), (yyvsp[0].str), &(yylsp[-2]), &(yylsp[0])));
	(yyval.term) = term;
}
#line 1939 "util/parse-events-bison.c" /* yacc.c:1646  */
    break;

  case 55:
#line 518 "util/parse-events.y" /* yacc.c:1646  */
    {
	struct parse_events_term *term;

	ABORT_ON(parse_events_term__num(&term, PARSE_EVENTS__TERM_TYPE_USER,
					(yyvsp[-2].str), (yyvsp[0].num), &(yylsp[-2]), &(yylsp[0])));
	(yyval.term) = term;
}
#line 1951 "util/parse-events-bison.c" /* yacc.c:1646  */
    break;

  case 56:
#line 527 "util/parse-events.y" /* yacc.c:1646  */
    {
	struct parse_events_term *term;
	int config = (yyvsp[0].num) & 255;

	ABORT_ON(parse_events_term__sym_hw(&term, (yyvsp[-2].str), config));
	(yyval.term) = term;
}
#line 1963 "util/parse-events-bison.c" /* yacc.c:1646  */
    break;

  case 57:
#line 536 "util/parse-events.y" /* yacc.c:1646  */
    {
	struct parse_events_term *term;

	ABORT_ON(parse_events_term__num(&term, PARSE_EVENTS__TERM_TYPE_USER,
					(yyvsp[0].str), 1, &(yylsp[0]), NULL));
	(yyval.term) = term;
}
#line 1975 "util/parse-events-bison.c" /* yacc.c:1646  */
    break;

  case 58:
#line 545 "util/parse-events.y" /* yacc.c:1646  */
    {
	struct parse_events_term *term;
	int config = (yyvsp[0].num) & 255;

	ABORT_ON(parse_events_term__sym_hw(&term, NULL, config));
	(yyval.term) = term;
}
#line 1987 "util/parse-events-bison.c" /* yacc.c:1646  */
    break;

  case 59:
#line 554 "util/parse-events.y" /* yacc.c:1646  */
    {
	struct parse_events_term *term;

	ABORT_ON(parse_events_term__str(&term, (int)(yyvsp[-2].num), NULL, (yyvsp[0].str), &(yylsp[-2]), &(yylsp[0])));
	(yyval.term) = term;
}
#line 1998 "util/parse-events-bison.c" /* yacc.c:1646  */
    break;

  case 60:
#line 562 "util/parse-events.y" /* yacc.c:1646  */
    {
	struct parse_events_term *term;

	ABORT_ON(parse_events_term__num(&term, (int)(yyvsp[-2].num), NULL, (yyvsp[0].num), &(yylsp[-2]), &(yylsp[0])));
	(yyval.term) = term;
}
#line 2009 "util/parse-events-bison.c" /* yacc.c:1646  */
    break;

  case 61:
#line 570 "util/parse-events.y" /* yacc.c:1646  */
    {
	struct parse_events_term *term;

	ABORT_ON(parse_events_term__num(&term, (int)(yyvsp[0].num), NULL, 1, &(yylsp[0]), NULL));
	(yyval.term) = term;
}
#line 2020 "util/parse-events-bison.c" /* yacc.c:1646  */
    break;


#line 2024 "util/parse-events-bison.c" /* yacc.c:1646  */
      default: break;
    }
  /* User semantic actions sometimes alter yychar, and that requires
     that yytoken be updated with the new translation.  We take the
     approach of translating immediately before every use of yytoken.
     One alternative is translating here after every semantic action,
     but that translation would be missed if the semantic action invokes
     YYABORT, YYACCEPT, or YYERROR immediately after altering yychar or
     if it invokes YYBACKUP.  In the case of YYABORT or YYACCEPT, an
     incorrect destructor might then be invoked immediately.  In the
     case of YYERROR or YYBACKUP, subsequent parser actions might lead
     to an incorrect destructor call or verbose syntax error message
     before the lookahead is translated.  */
  YY_SYMBOL_PRINT ("-> $$ =", yyr1[yyn], &yyval, &yyloc);

  YYPOPSTACK (yylen);
  yylen = 0;
  YY_STACK_PRINT (yyss, yyssp);

  *++yyvsp = yyval;
  *++yylsp = yyloc;

  /* Now 'shift' the result of the reduction.  Determine what state
     that goes to, based on the state we popped back to and the rule
     number reduced by.  */

  yyn = yyr1[yyn];

  yystate = yypgoto[yyn - YYNTOKENS] + *yyssp;
  if (0 <= yystate && yystate <= YYLAST && yycheck[yystate] == *yyssp)
    yystate = yytable[yystate];
  else
    yystate = yydefgoto[yyn - YYNTOKENS];

  goto yynewstate;


/*--------------------------------------.
| yyerrlab -- here on detecting error.  |
`--------------------------------------*/
yyerrlab:
  /* Make sure we have latest lookahead translation.  See comments at
     user semantic actions for why this is necessary.  */
  yytoken = yychar == YYEMPTY ? YYEMPTY : YYTRANSLATE (yychar);

  /* If not already recovering from an error, report this error.  */
  if (!yyerrstatus)
    {
      ++yynerrs;
#if ! YYERROR_VERBOSE
      yyerror (&yylloc, _data, scanner, YY_("syntax error"));
#else
# define YYSYNTAX_ERROR yysyntax_error (&yymsg_alloc, &yymsg, \
                                        yyssp, yytoken)
      {
        char const *yymsgp = YY_("syntax error");
        int yysyntax_error_status;
        yysyntax_error_status = YYSYNTAX_ERROR;
        if (yysyntax_error_status == 0)
          yymsgp = yymsg;
        else if (yysyntax_error_status == 1)
          {
            if (yymsg != yymsgbuf)
              YYSTACK_FREE (yymsg);
            yymsg = (char *) YYSTACK_ALLOC (yymsg_alloc);
            if (!yymsg)
              {
                yymsg = yymsgbuf;
                yymsg_alloc = sizeof yymsgbuf;
                yysyntax_error_status = 2;
              }
            else
              {
                yysyntax_error_status = YYSYNTAX_ERROR;
                yymsgp = yymsg;
              }
          }
        yyerror (&yylloc, _data, scanner, yymsgp);
        if (yysyntax_error_status == 2)
          goto yyexhaustedlab;
      }
# undef YYSYNTAX_ERROR
#endif
    }

  yyerror_range[1] = yylloc;

  if (yyerrstatus == 3)
    {
      /* If just tried and failed to reuse lookahead token after an
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
                      yytoken, &yylval, &yylloc, _data, scanner);
          yychar = YYEMPTY;
        }
    }

  /* Else will try to reuse lookahead token after shifting the error
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

  yyerror_range[1] = yylsp[1-yylen];
  /* Do not reclaim the symbols of the rule whose action triggered
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
  yyerrstatus = 3;      /* Each real token shifted decrements this.  */

  for (;;)
    {
      yyn = yypact[yystate];
      if (!yypact_value_is_default (yyn))
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

      yyerror_range[1] = *yylsp;
      yydestruct ("Error: popping",
                  yystos[yystate], yyvsp, yylsp, _data, scanner);
      YYPOPSTACK (1);
      yystate = *yyssp;
      YY_STACK_PRINT (yyss, yyssp);
    }

  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  *++yyvsp = yylval;
  YY_IGNORE_MAYBE_UNINITIALIZED_END

  yyerror_range[2] = yylloc;
  /* Using YYLLOC is tempting, but would change the location of
     the lookahead.  YYLOC is available though.  */
  YYLLOC_DEFAULT (yyloc, yyerror_range, 2);
  *++yylsp = yyloc;

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

#if !defined yyoverflow || YYERROR_VERBOSE
/*-------------------------------------------------.
| yyexhaustedlab -- memory exhaustion comes here.  |
`-------------------------------------------------*/
yyexhaustedlab:
  yyerror (&yylloc, _data, scanner, YY_("memory exhausted"));
  yyresult = 2;
  /* Fall through.  */
#endif

yyreturn:
  if (yychar != YYEMPTY)
    {
      /* Make sure we have latest lookahead translation.  See comments at
         user semantic actions for why this is necessary.  */
      yytoken = YYTRANSLATE (yychar);
      yydestruct ("Cleanup: discarding lookahead",
                  yytoken, &yylval, &yylloc, _data, scanner);
    }
  /* Do not reclaim the symbols of the rule whose action triggered
     this YYABORT or YYACCEPT.  */
  YYPOPSTACK (yylen);
  YY_STACK_PRINT (yyss, yyssp);
  while (yyssp != yyss)
    {
      yydestruct ("Cleanup: popping",
                  yystos[*yyssp], yyvsp, yylsp, _data, scanner);
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
  return yyresult;
}
#line 581 "util/parse-events.y" /* yacc.c:1906  */


void parse_events_error(YYLTYPE *loc, void *data,
			void *scanner __maybe_unused,
			char const *msg __maybe_unused)
{
	parse_events_evlist_error(data, loc->last_column, "parser error");
}
