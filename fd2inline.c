/******************************************************************************
 *
 * fd2sfd -- forked from fd2inline 1.38
 *
 * Should be able to parse CBM fd files and generate SFD files for futher
 * processing. Works as a filter.
 *
 * Based on fd2inline by Wolfgang Baron, Rainer F. Trunz, Kamil Iskra,
 * Ralph Schmidt, Emmanuel Lesueur and Martin Blom.
 *
 * fd2sfd by Martin Blom.
 *
 *****************************************************************************/

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/******************************************************************************
 * The program has a few sort of class definitions, which are the result of
 * object oriented thinking, to be imlemented in plain C. I just haven't
 * had the time to learn C++ or install the compiler. The design does however
 * improve robustness, which allows the source to be used over and over again.
 * if you use this code, please leave a little origin note.
 ******************************************************************************/

static const char version_str[]="$VER: fd2sfd " VERSION " (" DATE ")\r\n";

/******************************************************************************
 * These are general definitions including types for defining registers etc.
 ******************************************************************************/

#ifdef DEBUG
#define DBP(a) a
#else /* !DEBUG */
#define DBP(a)
#endif /* !DEBUG */

#if (defined(__GNUC__) || defined(__SASC)) && 0
#define INLINE __inline /* Gives 20% *larger* executable with GCC?! */
#else
#define INLINE
#endif

#define REGS 16	 /* d0=0,...,a7=15 */
#define FDS 1000

typedef enum
{
   d0, d1, d2, d3, d4, d5, d6, d7, a0, a1, a2, a3, a4, a5, a6, a7, illegal
} regs;

typedef unsigned char shortcard;

typedef enum { false, nodef, real_error } Error;

static int Quiet = 0;

static char BaseName[64], BaseNamU[64], BaseNamL[64], BaseNamC[64];
static char Buffer[512];

static const char *LibExcTable[]=
{
   "BattClockBase",	   "Node",
   "BattMemBase",	   "Node",
   "ConsoleDevice",	   "Device",
   "DiskBase",		   "DiskResource",
   "DOSBase",		   "DosLibrary",
   "SysBase",		   "ExecBase",
   "ExpansionBase",	   "ExpansionBase",
   "GfxBase",		   "GfxBase",
   "InputBase",		   "Device",
   "IntuitionBase",	   "IntuitionBase",
   "LocaleBase",	   "LocaleBase",
   "MathIeeeDoubBasBase",  "MathIEEEBase",
   "MathIeeeDoubTransBase","MathIEEEBase",
   "MathIeeeSingBasBase",  "MathIEEEBase",
   "MathIeeeSingTransBase","MathIEEEBase",
   "MiscBase",		   "Node",
   "PotgoBase",		   "Node",
   "RamdriveDevice",	   "Device",
   "RealTimeBase",	   "RealTimeBase",
   "RexxSysBase",	   "RxsLib",
   "TimerBase",		   "Device",
   "UtilityBase",	   "UtilityBase"
};
static const char *StdLib; /* global lib-name ptr */

#define CLASS		"class"
#define DEVICE		"device"
#define GADGET		"gadget"
#define IMAGE		"image"
#define RESOURCE	"resource"

static const char *TypeTable[] =
{
   "ARexxBase",		CLASS,
   "DTClassBase",	CLASS,
   "RequesterBase",	CLASS,
   "WindowBase",	CLASS,

   "AHIBase",		DEVICE,
   "ConsoleDevice",	DEVICE,
   "InputBase",		DEVICE,
   "RamdriveDevice",	DEVICE,
   "TimerBase",		DEVICE,

   "BGUIPaletteBase",	GADGET,
   "ButtonBase",	GADGET,
   "CheckBoxBase",	GADGET,
   "ChooserBase",	GADGET,
   "ColorWheelBase",	GADGET,
   "ClickTabBase",	GADGET,
   "DateBrowserBase",	GADGET,
   "FuelGaugeBase",	GADGET,
   "GetFileBase",	GADGET,
   "GetFontBase",	GADGET,
   "GetScreenModeBase",	GADGET,
   "IntegerBase",	GADGET,
   "LayoutBase",	GADGET,
   "ListBrowserBase",	GADGET,
   "PaletteBase",	GADGET,
   "PopCycleBase",	GADGET,
   "RadioButtonBase",	GADGET,
   "ScrollerBase",	GADGET,
   "SliderBase",	GADGET,
   "SpaceBase",		GADGET,
   "SpeedBarBase",	GADGET,
   "StringBase",	GADGET,
   "TextEditorBase",	GADGET,
   "TextFieldBase",	GADGET,
   "VirtualBase",	GADGET,

   "BevelBase",		IMAGE,
   "BitMapBase",	IMAGE,
   "DrawListBase",	IMAGE,
   "GlyphBase",		IMAGE,
   "LabelBase",		IMAGE,
   "PenMapBase",	IMAGE,

   "BattClockBase",	RESOURCE,
   "BattMemBase",	RESOURCE,
   "CardResource",	RESOURCE,
   "DiskBase",		RESOURCE,
   "KeymapBase",	RESOURCE,
   "MiscBase",		RESOURCE,
   "PotgoBase"		RESOURCE,
};


/*******************************************
 * just some support functions, no checking
 *******************************************/

char*
NewString(char** new, const char* old)
{
   const char *high;
   unsigned long len;

   while (*old && (*old==' ' || *old=='\t'))
      old++;
   len=strlen(old);
   for (high=old+len-1; high>=old && (*high==' ' || *high=='\t'); high--);
   high++;
   len=high-old;
   *new=malloc(1+len);
   if (*new)
   {
      strncpy(*new, old, len);
      (*new)[len]='\0';
   }
   else
      fprintf(stderr, "No mem for string\n");
   return *new;
}

static INLINE void
illparams(const char* funcname)
{
   fprintf(stderr, "%s: illegal Parameters\n", funcname);
}

static INLINE const char*
RegStr(regs reg)
{
   static const char *aosregs[]=
   {
      "d0", "d1", "d2", "d3", "d4", "d5", "d6", "d7",
      "a0", "a1", "a2", "a3", "a4", "a5", "a6", "a7", "illegal"
   };

   if (reg>illegal)
      reg=illegal;
   if ((int)reg<(int)d0)
      reg=d0;
   return aosregs[reg];
}


static INLINE 

/******************************************************************************
 *    StrNRBrk
 *
 * searches string in from position at downwards, as long as in does not
 * contain any character in not.
 *
 ******************************************************************************/

const char*
StrNRBrk(const char* in, const char* not, const char* at)
{
   const char *chcheck;
   Error ready;

   chcheck=""; /* if at<in, the result will be NULL */
   for (ready=false; ready==false && at>=in;)
   {
      for (chcheck=not; *chcheck && *chcheck != *at; chcheck++);
      if (*chcheck)
	 ready=real_error;
      else
	 at--;
   }
   DBP(fprintf(stderr, "{%c}", *chcheck));
   return *chcheck ? at : NULL;
}

/*
  Our own "strupr", since it is a non-standard function.
*/
void
StrUpr(char* str)
{
   while (*str)
   {
      *str=toupper(*str);
      str++;
   }
}

int
MatchGlob( char* glob, char* str )
{
  while( *glob )
  {
    char c = *glob++;

    switch( c )
    {
      case '?':
	if( *str == 0 )
	{
	  return 0;
	}
	break;

      case '\\':
	c = *glob++;
	if( c == 0 || *str != c )
	{
	  return 0;
	}
	break;
	
      case '*':
	if( *glob == 0 )
	{
	  return 1;
	}

	while( *str )
	{
	  if( MatchGlob( glob, str ) )
	  {
	    return 1;
	  }
	  ++str;
	}
	return 0;

      default:
	if( *str != c )
	{
	  return 0;
	}
	break;
    }

    ++str;
  }

  return *str == 0;
}

const char*
SkipWSAndComments( const char* ptr )
{
   for(;;)
   {
      while (*ptr && (*ptr==' ' || *ptr=='\t'))
	 ptr++;
      if (ptr[0] == '/' && ptr[1] == '*' ) 
      {
	 ptr+=2;
	 while (ptr[0] && ! (ptr[-2] == '*' && ptr[-1] == '/') )
	    ptr++;
      }
      else
	 break;
   }

   return ptr;
}

/******************************************************************************
 *    CLASS fdFile
 *
 * stores a file with a temporary buffer (static length, sorry), a line number,
 * an offset (used for library offsets and an error field.
 * When there's no error, line will contain line #lineno and offset will be
 * the last offset set by the interpretation of the last line. If there's been
 * no ##bias line, this field assumes a bias of 30, which is the standard bias.
 * It is assumed offsets are always negative.
 ******************************************************************************/

#define fF_BUFSIZE 1024

/* all you need to know about an fdFile you parse */

typedef enum {FD_PRIVATE=1} fdflags;

typedef struct
{
   FILE*         file;	      /* the file we're reading from	  */
   char	         line[fF_BUFSIZE]; /* the current line		  */
   unsigned long lineno;      /* current line number		  */
   long	         offset;      /* current fd offset (-bias)	  */
   Error         error;	      /* is everything o.k.		  */
   fdflags       flags;	      /* for ##private			  */
} fdFile;

fdFile*
fF_ctor	       (const char* fname);
static void
fF_dtor	       (fdFile* obj);
static void
fF_SetError    (fdFile* obj, Error error);
static void
fF_SetOffset   (fdFile* obj, long at);
Error
fF_readln      (fdFile* obj);
static Error
fF_GetError    (const fdFile* obj);
static long
fF_GetOffset   (const fdFile* obj);
char*
fF_FuncName    (fdFile* obj); /* return name or null */
static void
fF_SetFlags    (fdFile* obj, fdflags flags);
static fdflags
fF_GetFlags    (const fdFile* obj);

static INLINE void
fF_dtor(fdFile* obj)
{
  fclose(obj->file);
  free(obj);
}

static INLINE void
fF_SetError(fdFile* obj, Error error)
{
   if (obj)
      obj->error=error;
   else
      illparams("fF_SetError");
}

#define FUNCTION_GAP (6)

static INLINE void
fF_SetOffset(fdFile* obj, long at)
{
   if (obj)
      obj->offset= at;
   else
      illparams("fFSetOffset");
}

static INLINE void
fF_SetFlags(fdFile* obj, fdflags flags)
{
  if (obj)
    obj->flags=flags;
  else
    illparams("fF_SetFlags");
}

fdFile*
fF_ctor(const char* fname)
{
   fdFile *result;

   if (fname)
   {
      result=malloc(sizeof(fdFile));
      if (result)
      {
	 result->file=fopen(fname, "r");
	 if (result->file)
	 {
	    result->lineno=0;
	    fF_SetOffset(result, -30);
	    fF_SetError(result, false);
	    fF_SetFlags(result, 0);
	    result->line[0]='\0';
	 }
	 else
	 {
	    free(result);
	    result=NULL;
	 }
      }
   }
   else
   {
      result=NULL;
      illparams("fF_ctor");
   }
   return result;
}

Error
fF_readln(fdFile* obj)
{
   char *low, *bpoint;
   long glen,  /* the length we read until now */
   len;	       /* the length of the last segment */

   if (obj)
   {
      low=obj->line;
      glen=0;

      for (;;)
      {
	 obj->lineno++;
	 if (!fgets(low, fF_BUFSIZE-1-glen, obj->file))
	 {
	    fF_SetError(obj, real_error);
	    obj->line[0]='\0';
	    return real_error;
	 }
	 if (low==strpbrk(low, "*#/"))
	 {
	    DBP(fprintf(stderr, "in# %s\n", obj->line));
	    return false;
	 }
	 low=(char*) SkipWSAndComments(low);
	 len=strlen(low);
	 bpoint=low+len-1;
	 while (len && isspace(*bpoint))
	 {
	    bpoint--;
	    len--;
	 }
	 if (*bpoint==';' || *bpoint==')')
	 {
	    DBP(fprintf(stderr, "\nin: %s\n", obj->line));
	    return false;
	 }
	 glen+=len;
	 low+=len;
	 if (glen>=fF_BUFSIZE-10) /* somewhat pessimistic? */
	 {
	    fF_SetError(obj, real_error);
	    fprintf(stderr, "Line %lu too long.\n", obj->lineno);
	    return real_error;
	 }
	 DBP(fprintf(stderr, "+"));
      }
   }
   illparams("fF_readln");
   return real_error;
}

static INLINE Error
fF_GetError(const fdFile* obj)
{
   if (obj)
      return obj->error;
   illparams("fF_GetError");
   return real_error;
}

static INLINE long
fF_GetOffset(const fdFile* obj)
{
   if (obj)
      return obj->offset;
   illparams("fF_GetOffset");
   return -1;
}

/******************************************************************************
 * fF_FuncName
 *
 * checks if it can find a function-name and return it's address, or NULL
 * if the current line does not seem to contain one. The return value will
 * be a pointer into a malloced buffer, thus the caller will have to free().
 ******************************************************************************/

char*
fF_FuncName(fdFile* obj)
{
   const char *lower;
   const char *upper;
   char *buf;
   long obraces;  /* count of open braces */
   Error ready;	  /* ready with searching */

   if (!obj || fF_GetError(obj)==real_error)
   {
      illparams("fF_FuncName");
      return NULL;
   }
   lower=obj->line;
   /* lcs: Skip whitespaces AND comments */
   lower=SkipWSAndComments(lower);
   if (!*lower || (!isalpha(*lower) && *lower!='_'))
   {
      fF_SetError(obj, nodef);
      return NULL;
   }

   while (*lower)
   {
      if (!isalnum(*lower) && !isspace(*lower) && *lower!='*' && *lower!=','
      && *lower!='.' && *lower!=';' && *lower!='(' && *lower!=')' &&
      *lower!='[' && *lower!=']' && *lower!='_' && *lower!='\\')
      {
	 fF_SetError(obj, nodef);
	 return NULL;
      }
      lower++;
   }

   lower=NULL;
   buf=NULL;

   if (obj && fF_GetError(obj)==false)
   {
      if ((upper=strrchr(obj->line, ')'))!=0)
      {
	 DBP(fprintf(stderr, "end:%s:", upper));

	 for (obraces=1, ready=false; ready==false; upper=lower)
	 {
	    lower=StrNRBrk(obj->line, "()", --upper);
	    if (lower)
	    {
	       switch (*lower)
	       {
		  case ')':
		     obraces++;
		     DBP(fprintf(stderr, " )%ld%s", obraces, lower));
		     break;
		  case '(':
		     obraces--;
		     DBP(fprintf(stderr, " (%ld%s", obraces, lower));
		     if (!obraces)
			ready=nodef;
		     break;
		  default:
		     fprintf(stderr, "Faulty StrNRBrk\n");
	       }
	    }
	    else
	    {
	       fprintf(stderr, "'(' or ')' expected in line %lu.\n",
		  obj->lineno);
	       ready=real_error;
	    }
	 }
	 if (ready==nodef) /* we found the matching '(' */
	 {
	    long newlen;
	    const char* name;

	    upper--;

	    while (upper>=obj->line && (*upper==' ' || *upper=='\t'))
	       upper--;

	    lower=StrNRBrk(obj->line, " \t*)", upper);

	    if (!lower)
	       lower=obj->line;
	    else
	       lower++;

	    for (name=lower; name<=upper; name++)
	       if (!isalnum(*name) && *name!='_')
	       {
		  fF_SetError(obj, nodef);
		  return NULL;
	       }

	    newlen=upper-lower+1;
	    buf=malloc(newlen+1);

	    if (buf)
	    {
	       strncpy(buf, lower, newlen);
	       buf[newlen]='\0';
	    }
	    else
	       fprintf(stderr, "No mem for fF_FuncName");
	 }
      }
   }
   else
      illparams("fF_FuncName");
   return buf;
}

static INLINE fdflags
fF_GetFlags(const fdFile* obj)
{
   if (obj)
      return obj->flags;
   illparams("fF_GetFlags");
   return 0;
}

/*********************
 *    CLASS fdDef    *
 *********************/

typedef struct
{
   char* name;
   char* type;
   long	 offset;
   regs	 reg[REGS];
   char* param[REGS];
   char* proto[REGS];
   regs	 funcpar; /* number of argument that has type "pointer to function" */
   int   private;
   int   base;
   int   cfunction;
} fdDef;

fdDef*
fD_ctor		  (void);
void
fD_dtor		  (fdDef* obj);
static void
fD_NewName	  (fdDef* obj, const char* newname);
void
fD_NewParam	  (fdDef* obj, shortcard at, const char* newstr);
int
fD_NewProto	  (fdDef* obj, shortcard at, char* newstr);
static void
fD_NewReg	  (fdDef* obj, shortcard at, regs reg);
static void
fD_NewType	  (fdDef* obj, const char* newstr);
static void
fD_SetOffset	  (fdDef* obj, long off);
static void
fD_SetBase	  (fdDef* obj, int base);
static void
fD_SetCFunction	  (fdDef* obj, int cfunc);
static void
fD_SetPrivate	  (fdDef* obj, int priv);
Error
fD_parsefd	  (fdDef* obj, char** comment_ptr, fdFile* infile);
Error
fD_parsepr	  (fdDef* obj, fdFile* infile);
static const char*
fD_GetName	  (const fdDef* obj);
static long
fD_GetOffset	  (const fdDef* obj);
static const char*
fD_GetParam	  (const fdDef* obj, shortcard at);
static int
fD_GetPrivate	  (const fdDef* obj);
static regs
fD_GetReg	  (const fdDef* obj, shortcard at);
static const char*
fD_GetRegStr	  (const fdDef* obj, shortcard at);
static const char*
fD_GetType	  (const fdDef* obj);
static shortcard
fD_ParamNum	  (const fdDef* obj);
static shortcard
fD_ProtoNum	  (const fdDef* obj);
static shortcard
fD_RegNum	  (const fdDef* obj);
int
fD_cmpName	  (const void* big, const void* small);
void
fD_write	  (FILE* outfile, const fdDef* obj, int alias);
static shortcard
fD_GetFuncParNum  (const fdDef* obj);
static void
fD_SetFuncParNum  (fdDef* obj, shortcard at);

static fdDef **defs;
static fdDef **arrdefs;
static char  **arrcmts;
static long fds;

static char *fD_nostring="";

fdDef*
fD_ctor(void)
{
   fdDef *result;
   regs count;

   result=malloc(sizeof(fdDef));

   if (result)
   {
      result->name=fD_nostring;
      result->type=fD_nostring;
      result->funcpar=illegal;
      result->private=0;

      for (count=d0; count<illegal; count++ )
      {
	 result->reg[count]=illegal;
	 result->param[count]=fD_nostring; /* if (!strlen) dont't free() */
	 result->proto[count]=fD_nostring;
      }
   }
   return result;
}

/* free all resources and make the object as illegal as possible */

void
fD_dtor(fdDef* obj)
{
   regs count;

   if (obj)
   {
      if (!obj->name)
	 fprintf(stderr, "fD_dtor: null name");
      else
	 if (obj->name!=fD_nostring)
	    free(obj->name);

      if (!obj->type)
	 fprintf(stderr, "fD_dtor: null type");
      else
	 if (obj->type!=fD_nostring)
	    free(obj->type);

      obj->name=obj->type=NULL;

      for (count=d0; count<illegal; count++)
      {
	 obj->reg[count]=illegal;

	 if (!obj->param[count])
	    fprintf(stderr, "fD_dtor: null param");
	 else
	    if (obj->param[count]!=fD_nostring)
	       free(obj->param[count]);

	 if (!obj->proto[count])
	    fprintf(stderr, "fD_dtor: null proto");
	 else
	    if (obj->proto[count]!=fD_nostring)
	       free(obj->proto[count]);

	 obj->param[count]=obj->proto[count]=NULL;
      }

      free(obj);
   }
   else
      fprintf(stderr, "fd_dtor(NULL)\n");
}

static INLINE void
fD_NewName(fdDef* obj, const char* newname)
{
   if (obj && newname)
   {
      if (obj->name && obj->name!=fD_nostring)
	 free(obj->name);
      if (!NewString(&obj->name, newname))
	 obj->name=fD_nostring;
   }
   else
      illparams("fD_NewName");
}

void
fD_NewParam(fdDef* obj, shortcard at, const char* newstr)
{
   char *pa;

   if (newstr && obj && at<illegal)
   {
      pa=obj->param[at];

      if (pa && pa!=fD_nostring)
	 free(pa);

      while (*newstr==' ' || *newstr=='\t')
	 newstr++;

      if (NewString(&pa, newstr))
      {
	 obj->param[at]=pa;
      }
      else
	 obj->param[at]=fD_nostring;
   }
   else
      illparams("fD_NewParam");
}

/* get first free *reg or illegal */

static INLINE shortcard
fD_RegNum(const fdDef* obj)
{
   shortcard count;

   if (obj)
   {
      for (count=d0; count<illegal && obj->reg[count]!=illegal; count++);
      return count;
   }
   else
   {
      illparams("fD_RegNum");
      return illegal;
   }
}

static INLINE void
fD_NewReg(fdDef* obj, shortcard at, regs reg)
{
   if (obj && at<illegal && (int)reg>=(int)d0 && reg<=illegal)
      obj->reg[at]=reg;
   else
      illparams("fD_NewReg");
}

static INLINE regs
fD_GetReg(const fdDef* obj, shortcard at)
{
   if (obj && at<illegal)
      return obj->reg[at];
   else
   {
      illparams("fD_GetReg");
      return illegal;
   }
}

static INLINE shortcard
fD_GetFuncParNum(const fdDef* obj)
{
   if (obj)
      return (shortcard)obj->funcpar;
   else
   {
      illparams("fD_GetFuncParNum");
      return illegal;
   }
}

static INLINE void
fD_SetFuncParNum(fdDef* obj, shortcard at)
{
   if (obj && at<illegal)
      obj->funcpar=at;
   else
      illparams("fD_SetFuncParNum");
}

int
fD_NewProto(fdDef* obj, shortcard at, char* newstr)
{
   char *pr;

   if (newstr && obj && at<illegal)
   {
      char *t, arr[200]; /* I hope 200 will be enough... */
      int numwords=1;
      pr=obj->proto[at];

      if (pr && pr!=fD_nostring)
	 free(pr);

      while (*newstr==' ' || *newstr=='\t')
	 newstr++; /* Skip leading spaces */

      t=arr;
      while ((*t++=*newstr)!=0)
      {
	 /* Copy the rest, counting number of words */
	 if ((*newstr==' ' || *newstr=='\t') && newstr[1] && newstr[1]!=' ' &&
	 newstr[1]!='\t')
	    numwords++;
	 newstr++;
      }

      t=arr+strlen(arr)-1;
      while (*t==' ' || *t=='\t')
	 t--;
      t[1]='\0'; /* Get rid of tailing spaces */

      if (at!=fD_GetFuncParNum(obj))
      {
	 if (numwords>1) /* One word - must be type */
	    if (*t!='*')
	    {
	       /* '*' on the end - no parameter name used */
	       while (*t!=' ' && *t!='\t' && *t!='*')
		  t--;
	       t++;
	       if (strcmp(t, "char") && strcmp(t, "short") && strcmp(t, "int")
	       && strcmp(t, "long") && strcmp(t, "APTR"))
	       {
		  /* Not one of applicable keywords - must be parameter name.
		     Get rid of it. */
		  t--;
		  while (*t==' ' || *t=='\t')
		     t--;
		  t[1]='\0';
	       }
	    }
      }
      else
      {
	 /* Parameter of type "pointer to function". */
	 char *end;
	 t=strchr(arr, '(');
	 t++;
	 while (*t==' ' || *t=='\t')
	    t++;
	 if (*t!='*')
	    return 1;
	 t++;
	 end=strchr(t, ')');
	 memmove(t+2, end, strlen(end)+1);
	 *t='%';
	 t[1]='s';
      }

      if (NewString(&pr, arr))
      {
	 obj->proto[at]=pr;
	 while (*pr==' ' || *pr=='\t')
	    pr++;
	 if (!strcasecmp(pr, "double"))
	 {
	    /* "double" needs two data registers */
	    int count, regs=fD_RegNum(obj);
	    for (count=at+1; count<regs; count++)
	       fD_NewReg(obj, count, fD_GetReg(obj, count+1));
	 }
      }
      else
	 obj->proto[at]=fD_nostring;
   }
   else
      illparams("fD_NewProto");

   return 0;
}

static INLINE void
fD_NewType(fdDef* obj, const char* newtype)
{
   if (obj && newtype)
   {
      if (obj->type && obj->type!=fD_nostring)
	 free(obj->type);
      if (!NewString(&obj->type, newtype))
	 obj->type=fD_nostring;
   }
   else
      illparams("fD_NewType");
}

static INLINE void
fD_SetOffset(fdDef* obj, long off)
{
   if (obj)
      obj->offset=off;
   else
      illparams("fD_SetOffset");
}

static INLINE void
fD_SetBase(fdDef* obj, int base)
{
   if (obj)
      obj->base=base;
   else
      illparams("fD_SetPrivate");
}

static INLINE void
fD_SetCFunction(fdDef* obj, int cfunc)
{
   if (obj)
      obj->cfunction=cfunc;
   else
      illparams("fD_SetPrivate");
}

static INLINE void
fD_SetPrivate(fdDef* obj, int priv)
{
   if (obj)
      obj->private=priv;
   else
      illparams("fD_SetPrivate");
}

static INLINE const char*
fD_GetName(const fdDef* obj)
{
   if (obj && obj->name)
      return obj->name;
   else
   {
      illparams("fD_GetName");
      return fD_nostring;
   }
}

static INLINE long
fD_GetOffset(const fdDef* obj)
{
   if (obj)
      return obj->offset;
   else
   {
      illparams("fD_GetOffset");
      return 0;
   }
}

static INLINE int
fD_GetPrivate(const fdDef* obj)
{
   if (obj)
      return obj->private;
   else
   {
      illparams("fD_GetPrivate");
      return 0;
   }
}

static INLINE const char*
fD_GetProto(const fdDef* obj, shortcard at)
{
   if (obj && at<illegal && obj->proto[at])
      return obj->proto[at];
   else
   {
      illparams("fD_GetProto");
      return fD_nostring;
   }
}

static INLINE const char*
fD_GetParam(const fdDef* obj, shortcard at)
{
   if (obj && at<illegal && obj->param[at])
      return obj->param[at];
   else
   {
      illparams("fD_GetParam");
      return fD_nostring;
   }
}

static INLINE const char*
fD_GetRegStr(const fdDef* obj, shortcard at)
{
   if (obj && at<illegal)
      return RegStr(obj->reg[at]);
   else
   {
      illparams("fD_GetReg");
      return RegStr(illegal);
   }
}

static INLINE const char*
fD_GetType(const fdDef* obj)
{
   if (obj && obj->type)
      return obj->type;
   else
   {
      illparams("fD_GetType");
      return fD_nostring;
   }
}

/* get first free param or illegal */

static INLINE shortcard
fD_ParamNum(const fdDef* obj)
{
   shortcard count;

   if (obj)
   {
      for (count=d0; count<illegal && obj->param[count]!=fD_nostring;
      count++);
      return count;
   }
   else
   {
      illparams("fD_ParamNum");
      return illegal;
   }
}

static INLINE shortcard
fD_ProtoNum(const fdDef* obj)
{
   shortcard count;

   if (obj)
   {
      for (count=d0; count<illegal && obj->proto[count]!=fD_nostring;
      count++);
      return count;
   }
   else
   {
      illparams("fD_ProtoNum");
      return illegal;
   }
}

/******************************************************************************
 *    fD_parsefd
 *
 *  parse the current line. Needs to copy input, in order to insert \0's
 *  RETURN
 *    fF_GetError(infile):
 * false = read a definition.
 * nodef = not a definition on line (so try again)
 * error = real error
 ******************************************************************************/

Error
fD_parsefd(fdDef* obj, char** comment_ptr, fdFile* infile)
{
   enum parse_info { name, params, regs, ready } parsing;
   char *buf, *bpoint, *bnext;
   unsigned long index;

   if (obj && infile && fF_GetError(infile)==false)
   {
      parsing=name;

      if (!NewString(&buf, infile->line))
      {
	 fprintf(stderr, "No mem for line %lu\n", infile->lineno);
	 fF_SetError(infile, real_error);
      }
      bpoint=buf; /* so -Wall keeps quiet */

      /* try to parse the line until there's an error or we are done */

      while (parsing!=ready && fF_GetError(infile)==false)
      {
	 switch (parsing)
	 {
	    case name:
	       switch (buf[0])
	       {
		  case '#':
		     if (strncmp("##base", buf, 6)==0)
		     {
			bnext=buf+6;
			while (*bnext==' ' || *bnext=='\t' || *bnext=='_')
			   bnext++;
			strcpy(BaseName, bnext);
			BaseName[strlen(BaseName)-1]='\0';
		     }
		     else
			if (strncmp("##bias", buf, 6)==0)
			{
			   if (!sscanf(buf+6, "%ld", &infile->offset))
			   {
			      fprintf(stderr, "Illegal ##bias in line %lu: %s\n",
				 infile->lineno, infile->line);
			      fF_SetError(infile, real_error);
			      break; /* avoid nodef */
			   }
			   else
			   {
			      if (fF_GetOffset(infile)>0)
				 fF_SetOffset(infile, -fF_GetOffset(infile));
			      DBP(fprintf(stderr, "set offset to %ld\n",
				 fF_GetOffset(infile)));
			   }
			}
			else
			{
			   if (strncmp("##private", buf, 9)==0)
			      fF_SetFlags(infile, fF_GetFlags(infile) |
				 FD_PRIVATE);
			   else if (strncmp("##public", buf, 8)==0)
			      fF_SetFlags(infile, fF_GetFlags(infile) &
				 ~FD_PRIVATE);
			}
		     /* try again somewhere else */
		     fF_SetError(infile, nodef);
			break;

		  case '*':
		  {
		    size_t olen = *comment_ptr ? strlen(*comment_ptr) : 0;
		    size_t clen = strlen(buf) + 1 + olen;
		    
		    *comment_ptr = realloc(*comment_ptr, clen);

		    if (olen ==0) {
		      **comment_ptr = 0;
		    }

		    strcat(*comment_ptr, buf);
		    DBP(fprintf(stderr, "Comments: %s", *comment_ptr));

		    /* try again somewhere else */
		     fF_SetError(infile, nodef);
			break;
		  }

		  default:
		     /* assume a regular line here */
		     fD_SetPrivate( obj,
				    (fF_GetFlags(infile) & FD_PRIVATE) != 0);
		     parsing=name; /* switch (parsing) */
		     for (index=0; buf[index] && buf[index]!='('; index++);

		     if (!buf[index])
		     {
			/* oops, no fd ? */
			fprintf(stderr, "Not an fd, line %lu: %s\n",
			   infile->lineno, buf /* infile->line */);
			fF_SetError(infile, nodef);
		     } /* maybe next time */
		     else
		     {
			buf[index]=0;

			fD_NewName(obj, buf);
			fD_SetOffset(obj, fF_GetOffset(infile));

			bpoint=buf+index+1;
			parsing=params; /* continue the loop */
		     }
	       }
	       break;

	    case params:
	    {
	       char *bptmp; /* needed for fD_NewParam */

	       /* look for parameters now */

	       for (bnext = bpoint; *bnext && *bnext!=',' && *bnext!=')';
	       bnext++);

	       if (*bnext)
	       {
		  bptmp=bpoint;

		  if (*bnext == ')')
		  {
		     if (bnext[1] != '(')
		     {
			fprintf(stderr, "Registers expected in line %lu: %s\n",
			   infile->lineno, infile->line);
			fF_SetError(infile, nodef);
		     }
		     else
		     {
			parsing=regs;
			bpoint=bnext+2;
		     }
		  }
		  else
		     bpoint = bnext+1;

		  /* terminate string and advance to next item */

		  *bnext='\0';
		  if (*bptmp)
		     fD_NewParam(obj, fD_ParamNum(obj), bptmp);
	       }
	       else
	       {
		  fF_SetError(infile, nodef);
		  fprintf(stderr, "Param expected in line %lu: %s\n",
		     infile->lineno, infile->line);
	       }
	       break;  /* switch parsing */
	    }

	    case regs:
	       /* look for parameters now */

	       for (bnext=bpoint; *bnext && *bnext!='/' && *bnext!=',' &&
	       *bnext!=')'; bnext++);

	       if (*bnext)
	       {
		  if (')'==*bnext)
		  {
		     /* wow, we've finished */
		     fF_SetOffset(infile, fF_GetOffset(infile)-FUNCTION_GAP);
		     parsing=ready;
		  }
		  *bnext = '\0';

		  bpoint[0]=tolower(bpoint[0]);

		  if ((bpoint[0]=='d' || bpoint[0]=='a') && bpoint[1]>='0' &&
		  bpoint[1]<='8' && bnext==bpoint+2)
		     fD_NewReg(obj, fD_RegNum(obj),
			bpoint[1]-'0'+(bpoint[0]=='a'? 8 : 0));
		  else
		     if (bnext!=bpoint)
		     {
		        if (!strcasecmp(bpoint, "base"))
			{
			   fD_SetBase(obj,1);
			}
			else if (!strcasecmp(bpoint, "sysv"))
			{
			   fD_SetCFunction(obj,1);
			}
		        else
			{
			   /* it is when our function is void */
			   fprintf(stderr, "Illegal register %s in line %ld\n",
				   bpoint, infile->lineno);
			   fF_SetError(infile, nodef);
			}
		     }
		  bpoint = bnext+1;
	       }
	       else
	       {
		  fF_SetError(infile, nodef);
		  fprintf(stderr, "Reg expected in line %lu\n",
		     infile->lineno);
	       }
	       break; /* switch parsing */

	    case ready:
	       fprintf(stderr, "Internal error, use another compiler.\n");
	       break;
	 }
      }

      free(buf);
      return fF_GetError(infile);
   }
   else
   {
      illparams("fD_parsefd");
      return real_error;
   }
}


Error
fD_parsepr(fdDef* obj, fdFile* infile)
{
   char	 *buf;	  /* a copy of infile->line		       */
   char	 *bpoint, /* cursor in buf			       */
	 *bnext,  /* looking for the end		       */
	 *lowarg; /* beginning of this argument		       */
   long	 obraces; /* count of open braces		       */
   regs	 count,	  /* count parameter number		       */
	 args;	  /* the number of arguments for this function */

   if (!(obj && infile && fF_GetError(infile)==false))
   {
      illparams("fD_parsepr");
      fF_SetError(infile, real_error);
      return real_error;
   }
   if (!NewString(&buf, SkipWSAndComments(infile->line))) //lcs
   {
      fprintf(stderr, "No mem for fD_parsepr\n");
      fF_SetError(infile, real_error);
      return real_error;
   }
   fF_SetError(infile, false);
   bpoint=strchr(buf, '(');
   while (--bpoint>=buf && strstr(bpoint, fD_GetName(obj))!=bpoint);
   if (bpoint>=buf)
   {
      while (--bpoint >= buf && (*bpoint==' ' || *bpoint=='\t'));
      *++bpoint='\0';

      fD_NewType(obj, buf);

      while (bpoint && *bpoint++!='('); /* one beyond '(' */

      lowarg=bpoint;
      obraces=0;

      for (count=0, args=fD_ParamNum(obj); count<args; bpoint=bnext+1)
      {
	 while (*bpoint && (*bpoint==' ' || *bpoint=='\t')) /* ignore spaces */
	    bpoint++;

	 bnext=strpbrk(bpoint, "(),");

	 if (bnext)
	 {
	    switch (*bnext)
	    {
	       case '(':
		  if (!obraces)
		  {
		     fD_SetFuncParNum(obj, count);
		  }
		  obraces++;
		  DBP(fprintf(stderr, "< (%ld%s >", obraces, bnext));
		  break;

	       case ')':
		  if (obraces)
		  {
		     DBP(fprintf(stderr, "< )%ld%s >", obraces, bnext));
		     obraces--;
		  }
		  else
		  {
		     *bnext='\0';
		     DBP(fprintf(stderr, "< )0> [LAST PROTO=%s]", lowarg));
		     if (fD_NewProto(obj, count, lowarg))
			fprintf(stderr, "Parser confused in line %ld\n",
			      infile->lineno);
		     lowarg=bnext+1;

		     if (count!=args-1)
		     {
			DBP(fprintf(stderr, "%s needs %u arguments and got %u.\n",
			   fD_GetName(obj), args, count+1));
			fF_SetError(infile, nodef);
		     }
		     count++;
		  }
		  break;

	       case ',':
		  if (!obraces)
		  {
		     *bnext='\0';
		     DBP(fprintf(stderr, " [PROTO=%s] ", lowarg));
		     if (fD_NewProto(obj, count, lowarg))
			fprintf(stderr, "Parser confused in line %ld\n",
			      infile->lineno);
		     lowarg=bnext+1;
		     count++;
		  }
		  break;

	       default:
		  fprintf(stderr, "Faulty strpbrk in line %lu.\n",
		     infile->lineno);
	    }
	 }
	 else
	 {
	    DBP(fprintf(stderr, "Faulty argument %u in line %lu.\n", count+1,
	       infile->lineno));
	    count=args; /* this will effectively quit the for loop */
	    fF_SetError(infile, nodef);
	 }
      }
      if (fD_ProtoNum(obj)!=fD_ParamNum(obj))
	 fF_SetError(infile, nodef);
   }
   else
   {
      fprintf(stderr, "fD_parsepr was fooled in line %lu\n", infile->lineno);
      fprintf(stderr, "function , definition %s.\n",
	 /* fD_GetName(obj),*/ infile->line);
      fF_SetError(infile, nodef);
   }

   free(buf);

   return fF_GetError(infile);
}

int
fD_cmpName(const void* big, const void* small) /* for qsort and bsearch */
{
   return strcmp(fD_GetName(*(fdDef**)big), fD_GetName(*(fdDef**)small));
}

static const char *TagExcTable[]=
{
   "BuildEasyRequestArgs", "BuildEasyRequest",
   "DoDTMethodA",	   "DoDTMethod",
   "DoGadgetMethodA",	   "DoGadgetMethod",
   "EasyRequestArgs",	   "EasyRequest",
   "MUI_MakeObjectA",	   "MUI_MakeObject",
   "MUI_RequestA",	   "MUI_Request",
   "PrintDTObjectA",	   "PrintDTObject",
   "RefreshDTObjectA",     "RefreshDTObjects",
   "UMSVLog",		   "UMSLog",
   "VFWritef",		   "FWritef",
   "VFPrintf",		   "FPrintf",
   "VPrintf",		   "Printf",
};

const char*
getvarargsfunction(const fdDef * obj)
{
   unsigned int count;
   const char *name = fD_GetName(obj);
    
   for (count=0; count<sizeof TagExcTable/sizeof TagExcTable[0]; count+=2)
   {
      if (strcmp(name, TagExcTable[count])==0)
      {
	 return TagExcTable[count+1];
      }
   }
   return(NULL);
}

const char*
taggedfunction(const fdDef* obj)
{
   shortcard numargs=fD_ParamNum(obj);
   unsigned int count;
   int aos_tagitem;
   const char *name=fD_GetName(obj);
   static char newname[200];  /* Hope will be enough... static because used
				 out of the function. */
   const char *lastarg;
   static const char *TagExcTable2[]=
   {
      "ApplyTagChanges",
      "CloneTagItems",
      "FindTagItem",
      "FreeTagItems",
      "GetTagData",
      "PackBoolTags",
      "PackStructureTags",
      "RefreshTagItemClones",
      "UnpackStructureTags",
   };

   if (!numargs)
      return NULL;

   for (count=0; count<sizeof TagExcTable/sizeof TagExcTable[0]; count+=2)
      if (strcmp(name, TagExcTable[count])==0)
	 return NULL;
// lcs	 return TagExcTable[count+1];

   for (count=0; count<sizeof TagExcTable2/sizeof TagExcTable2[0]; count++)
      if (strcmp(name, TagExcTable2[count])==0)
	 return NULL;

   lastarg=fD_GetProto(obj, numargs-1);
   if (strncmp(lastarg, "const", 5)==0 || strncmp(lastarg, "CONST", 5)==0)
      lastarg+=5;
   while (*lastarg==' ' || *lastarg=='\t')
      lastarg++;
   if (strncmp(lastarg, "struct", 6))
      return NULL;
   lastarg+=6;
   while (*lastarg==' ' || *lastarg=='\t')
      lastarg++;
   aos_tagitem=1;
   if (strncmp(lastarg, "TagItem", 7))
      return NULL;
   lastarg+=(aos_tagitem ? 7 : 11);
   while (*lastarg==' ' || *lastarg=='\t')
      lastarg++;
   if (strcmp(lastarg, "*"))
      return NULL;

   strcpy(newname, name);
   if (newname[strlen(newname)-1]=='A')
      newname[strlen(newname)-1]='\0';
   else
      if (strlen(newname)>7 && !strcmp(newname+strlen(newname)-7, "TagList"))
	 strcpy(newname+strlen(newname)-4, "s");
      else
	 strcat(newname, "Tags");
   return newname;
}

const char*
aliasfunction(const char* name)
{
   static const char *AliasTable[]=
   {
      "AllocDosObject", "AllocDosObjectTagList",
      "CreateNewProc",	"CreateNewProcTagList",
      "NewLoadSeg",	"NewLoadSegTagList",
      "System",		"SystemTagList",
   };
   unsigned int count;
   for (count=0; count<sizeof AliasTable/sizeof AliasTable[0]; count++)
      if (strcmp(name, AliasTable[count])==0)
	 return AliasTable[count+(count%2 ? -1 : 1)];
   return NULL;
}

static INLINE void
fD_PrintRegs(FILE* outfile, const fdDef* obj)
{
   int count;
   int numregs=fD_RegNum(obj);

   if (obj->cfunction)
   {
      if (obj->base)
      {
	 fprintf(outfile, "base,");
      }
      fprintf(outfile, "sysv");
   }
   if (numregs>0)
   {
      for (count=d0; count<numregs-1; count++)
	 fprintf(outfile, "%s,", fD_GetRegStr(obj, count));
      fprintf(outfile, "%s", fD_GetRegStr(obj, count));
   }
}

void
fD_write(FILE* outfile, const fdDef* obj,int alias)
{
   static int bias = -1;
   static int priv = -1;
   shortcard count, numargs;
   const char *tagname, *varname, *name, *rettype;
   int vd=0, a45=0, d7=0;

   DBP(fprintf(stderr, "func %s\n", fD_GetName(obj)));

   numargs=fD_RegNum(obj);

   if (!numargs)
     numargs=fD_ParamNum(obj);
   else if (fD_ParamNum(obj) != numargs && !Quiet)
     fprintf(stderr, "Warning: %s gets %d params and %d regs!\n",
	     fD_GetName(obj), fD_ParamNum(obj), numargs);
     

   if ((rettype=fD_GetType(obj))==fD_nostring)
   {
      if (!Quiet && !fD_GetPrivate(obj))
	 fprintf(stderr, "Warning: %s has no prototype.\n", fD_GetName(obj));
      rettype = "ULONG";
   }
   if (!strcasecmp(rettype, "void"))
      vd = 1; /* set flag */
   for (count=d0; count<numargs; count++)
   {
      const char *reg=fD_GetRegStr(obj, count);
      if (strcmp(reg, "a4")==0 || strcmp(reg, "a5")==0)
      {
	 if (!a45)
	    a45=(strcmp(reg, "a4") ? 5 : 4); /* set flag */
	 else /* Security check */
	    if (!Quiet)
	       fprintf(stderr, "Warning: both a4 and a5 are used. "
		       "This is not supported!\n");
      }
      if (strcmp(reg, "d7")==0) /* Used only when a45!=0 */
	 d7=1;
   }
   
   if (a45 && d7) /* Security check */
      if (!Quiet)
	 fprintf(stderr, "Warning: d7 and a4 or a5 are used. This is not "
		 "supported!\n");

   name=fD_GetName(obj);

   if (fD_ProtoNum(obj)!=numargs)
   {
      if (!Quiet)
	 fprintf(stderr, "%s gets %d fd args and %d proto%s.\n", name, numargs,
		 fD_ProtoNum(obj), fD_ProtoNum(obj)!= 1 ? "s" : "");
      for (count=d0; count<numargs; count++)
      {
	 if (fD_GetReg(obj, count) != illegal &&
	     fD_GetProto(obj, count) == fD_nostring)
	 {
	    fD_NewProto((fdDef*)obj, count, "ULONG");
	 }
      }

//      return;
   }

   if (bias != fD_GetOffset(obj))
   {
      bias = fD_GetOffset(obj);
      fprintf(outfile, "==bias %d\n", -bias);
   }

   if (priv != fD_GetPrivate(obj))
   {
      priv = fD_GetPrivate(obj);
      fprintf(outfile, "==%s\n", priv ? "private" : "public");
   }
   
   fprintf(outfile, "%s %s(", rettype, name);
   
   if (numargs>0)
   {
      for (count=d0; count<numargs; count++)
      {
	 if (strchr(fD_GetProto(obj, count),'%'))
	    sprintf(Buffer, fD_GetProto(obj, count), fD_GetParam(obj, count));
	 else
	    sprintf(Buffer, "%s %s",
		    fD_GetProto(obj, count), fD_GetParam(obj, count));

	 // Workaround varargs in FD file (sysv)
	 if (strcmp(fD_GetParam(obj, count), "...")==0)
	   sprintf(Buffer, "...");
	 
	 if (count<numargs-1)
	    fprintf(outfile, "%s, ", Buffer);
	 else
	    fprintf(outfile, "%s", Buffer);
      }
   }

   fprintf(outfile, ") (");

   fD_PrintRegs(outfile, obj);

   fprintf(outfile, ")\n");

   if (alias)
   {
      return;
   }
   
   if ((tagname=aliasfunction(fD_GetName(obj)))!=0)
   {
      fdDef *objnc=(fdDef*)obj;
      objnc->name=(char*)tagname;

      fprintf(outfile, "==alias\n");
      fD_write(outfile, objnc, 1);

      objnc->name=(char*)name;
   }

   if ((tagname=taggedfunction(obj))!=0)
   {
      fprintf(outfile, "==varargs\n");

      fprintf(outfile, "%s %s(", rettype, tagname);
   
      if (numargs>0)
      {
	 for (count=d0; count<numargs; count++)
	 {
	    if (strchr(fD_GetProto(obj, count),'%'))
	       sprintf(Buffer, fD_GetProto(obj, count),
		       fD_GetParam(obj, count));
	    else
	       sprintf(Buffer, "%s %s",
		       fD_GetProto(obj, count), fD_GetParam(obj, count));
	    if (count<numargs-1)
	       fprintf(outfile, "%s, ", Buffer);
	    else
	       fprintf(outfile, "Tag %s, ...", fD_GetParam(obj, count));
	 }
      }

      fprintf(outfile, ") (");

      fD_PrintRegs(outfile, obj);

      fprintf(outfile, ")\n");
   }

   if ((varname = getvarargsfunction(obj)) != 0)
   {
      fprintf(outfile, "==varargs\n");

      fprintf(outfile, "%s %s(", rettype, varname);
   
      if (numargs>0)
      {
	 for (count=d0; count<numargs; count++)
	 {
	    if (strchr(fD_GetProto(obj, count),'%'))
	       sprintf(Buffer, fD_GetProto(obj, count),
		       fD_GetParam(obj, count));
	    else
	       sprintf(Buffer, "%s %s",
		       fD_GetProto(obj, count), fD_GetParam(obj, count));
	    if (count<numargs-1)
	       fprintf(outfile, "%s, ", Buffer);
	    else
	       fprintf(outfile, "...");
	 }
      }

      fprintf(outfile, ") (");

      fD_PrintRegs(outfile, obj);

      fprintf(outfile, ")\n");
   }

   if (strcmp(name, "DoPkt")==0)
   {
      fdDef *objnc=(fdDef*)obj;
      char newname[7]="DoPkt0";
      objnc->name=newname;
      for (count=2; count<7; count++)
      {
	 regs reg=objnc->reg[count];
	 char *proto=objnc->proto[count];
	 objnc->reg[count]=illegal;
	 objnc->proto[count]=fD_nostring;
	 fprintf(outfile,"==alias\n");
	 fD_write(outfile, objnc, 1);
	 objnc->reg[count]=reg;
	 objnc->proto[count]=proto;
	 newname[5]++;
      }
      objnc->name=(char*)name;
   }

   bias -= FUNCTION_GAP;
}

int
varargsfunction(const char* proto, const char* funcname)
{
   const char *end=proto+strlen(proto)-1;
   while (isspace(*end))
      end--;
   if (*end--==';')
   {
      while (isspace(*end))
	 end--;
      if (*end--==')')
      {
	 while (isspace(*end))
	    end--;
	 if (!strncmp(end-2, "...", 3))
	 {
	    /* Seems to be a varargs function. Check if it will be recognized
	       as "tagged". */
	    unsigned int count;
	    char fixedname[200]; /* Hope will be enough... */
	    fdDef *tmpdef;

	    for (count=0; count<sizeof TagExcTable/sizeof TagExcTable[0];
	    count+=2)
	       if (strcmp(funcname, TagExcTable[count+1])==0)
		  return 1;

	    if (!(tmpdef=fD_ctor()))
	    {
	       fprintf(stderr, "No mem for FDs\n");
	       exit(EXIT_FAILURE);
	    }

	    strcpy(fixedname, funcname);
	    if (strlen(funcname)>4 &&
	    !strcmp(funcname+strlen(funcname)-4, "Tags"))
	    {
	       /* Might be either nothing or "TagList". */
	       fixedname[strlen(fixedname)-4]='\0';
	       fD_NewName(tmpdef, fixedname);
	       if (bsearch(&tmpdef, arrdefs, fds, sizeof arrdefs[0],
	       fD_cmpName))
		  return 1;

	       strcat(fixedname, "TagList");
	       fD_NewName(tmpdef, fixedname);
	       if (bsearch(&tmpdef, arrdefs, fds, sizeof arrdefs[0],
	       fD_cmpName))
		  return 1;
	    }
	    else
	    {
	       strcat(fixedname, "A");
	       fD_NewName(tmpdef, fixedname);
	       if (bsearch(&tmpdef, arrdefs, fds, sizeof arrdefs[0],
	       fD_cmpName))
		  return 1;
	    }
	 }
      }
   }
   return 0;
}

int
ishandleddifferently(const char* proto, const char* funcname)
{
   /* First check if this is a vararg call? */
   if (varargsfunction(proto, funcname))
      return 1;

   /* It might be a dos.library "alias" name. */
   if (aliasfunction(funcname))
      return 1;

   /* It might be one from dos.library/DoPkt() family. */
   if (strlen(funcname)==6 && !strncmp(funcname, "DoPkt", 5) &&
   funcname[5]>='0' && funcname[6]<='4')
      return 1;

   /* Finally, it can be intuition.library/ReportMouse1(). */
   return !strcmp(funcname, "ReportMouse1");
}

void
printusage(const char* exename)
{
   fprintf(stderr,
      "Usage: %s [options] fd-file clib-file [[-o] output-file]\n"
      "Options:\n"

      "--quiet\t\t\tDon't display warnings\n"
      "--version\t\tPrint version number and exit\n\n"
	   , exename);
}

void output_proto(FILE* outfile)
{
   fprintf(outfile,
      "/* Automatically generated header! Do not edit! */\n\n"
      "#ifndef PROTO_%s_H\n"
      "#define PROTO_%s_H\n\n"
      "#include <clib/%s_protos.h>\n\n"
      "#ifndef _NO_INLINE\n"
      "#ifdef __GNUC__\n"
      "#include <inline/%s.h>\n"
      "#endif /* __GNUC__ */\n"
      "#endif /* !_NO_INLINE */\n\n",
      BaseNamU, BaseNamU, BaseNamL, BaseNamL);

   if (BaseName[0])
      fprintf(outfile,
	 "#ifndef __NOLIBBASE__\n"
	 "extern struct %s *\n"
	 "#ifdef __CONSTLIBBASEDECL__\n"
	 "__CONSTLIBBASEDECL__\n"
	 "#endif /* __CONSTLIBBASEDECL__ */\n"
	 "%s;\n"
	 "#endif /* !__NOLIBBASE__ */\n\n",
	 StdLib, BaseName);

   fprintf(outfile,
      "#endif /* !PROTO_%s_H */\n", BaseNamU);
}

/******************************************************************************/

int
main(int argc, char** argv)
{
   fdDef *tmpdef,	/* a dummy to contain the name to look for */
	 *founddef;	/* the fdDef for which we found a prototype */
   fdFile *myfile;
   char *tmpstr;
   FILE *clib;
   FILE *outfile;
   int   closeoutfile=0;
   int   rc = EXIT_FAILURE;
   char *fdfilename=0, *clibfilename=0, *outfilename=0;
   const char* type = "library";

   int count;
   Error lerror;

   for (count=1; count<argc; count++)
   {
      char *option=argv[count];
      if (*option=='-')
      {
	 option++;
	 if (strcmp(option, "o")==0)
	 {
	    if (count==argc-1 || outfilename)
	    {
	       printusage(argv[0]);
	       return EXIT_FAILURE;
	    }
	    if (strcmp(argv[++count], "-"))
	       outfilename=argv[count];
	 }
	 else
	 {
	    if (*option=='-') /* Accept GNU-style '--' options */
	       option++;

	    if (strcmp(option, "quiet") == 0)
	       Quiet = 1;
	    else if (strcmp(option, "version")==0)
	    {
	       fprintf(stderr, "fd2sfd version " VERSION "\n");
	       return EXIT_SUCCESS;
	    }
	    /* Unknown option */
	    else
	    {
	       printusage(argv[0]);
	       return EXIT_FAILURE;
	    }
	 }
      }
      else
      {
	 /* One of the filenames */
	 if (!fdfilename)
	    fdfilename=option;
	 else if (!clibfilename)
	    clibfilename=option;
	 else if (!outfilename)
	    outfilename=option;
	 else
	 {
	    printusage(argv[0]);
	    return EXIT_FAILURE;
	 }
      }
   }

   if (!fdfilename || !clibfilename)
   {
      printusage(argv[0]);
      return EXIT_FAILURE;
   }

   if (!(arrdefs=malloc(FDS*sizeof(fdDef*))))
   {
      fprintf(stderr, "No mem for FDs\n");
      rc = EXIT_FAILURE;
      goto quit;
   }
   if (!(arrcmts=malloc(FDS*sizeof(char*))))
   {
      fprintf(stderr, "No mem for FD comments\n");
      rc = EXIT_FAILURE;
      goto quit;
   }
   for (count=0; count<FDS; count++)
   {
      arrdefs[count]=NULL;
      arrcmts[count]=NULL;
   }

   if (!(myfile=fF_ctor(fdfilename)))
   {
      fprintf(stderr, "Couldn't open file '%s'.\n", fdfilename);
      rc = EXIT_FAILURE;
      goto quit;
   }

   lerror=false;

   for (count=0; count<FDS && lerror==false; count++)
   {
      if (!(arrdefs[count]=fD_ctor()))
      {
	 fprintf(stderr, "No mem for FDs\n" );
	 rc = EXIT_FAILURE;
	 goto quit;
      }
      do
      {
	 if ((lerror=fF_readln(myfile))==false)
	 {
	    fF_SetError(myfile, false);
	    lerror=fD_parsefd(arrdefs[count], &arrcmts[count], myfile);
	 }
      }
      while (lerror==nodef);
   }
   if (count<FDS)
   {
      count--;
      fD_dtor(arrdefs[count]);
      arrdefs[count]=NULL;
      arrcmts[count]=NULL;
   }
   fds=count;

   /* Make a copy before we sort, since we need to process the
      definitions in bias order, not lexical order */

   if (!(defs=malloc(fds*sizeof(fdDef*))))
   {
      fprintf(stderr, "No mem for FDs\n");
      rc = EXIT_FAILURE;
      goto quit;
   }

   bcopy(arrdefs,defs,fds*sizeof(fdDef*));
   
   qsort(arrdefs, count, sizeof arrdefs[0], fD_cmpName);

   if (BaseName[0])
   {
      unsigned int count2;
      StdLib="Library";

      for (count2=0; count2<sizeof LibExcTable/sizeof LibExcTable[0]; count2+=2)
	 if (strcmp(BaseName, LibExcTable[count2])==0)
	 {
	    StdLib=LibExcTable[count2+1];
	    break;
	 }
   }

   fF_dtor(myfile);

   if (!(myfile=fF_ctor(clibfilename)))
   {
     fprintf(stderr, "Couldn't open file '%s'.\n", clibfilename);
     rc = EXIT_FAILURE;
     goto quit;
   }

   if (!(tmpdef=fD_ctor()))
   {
     fprintf(stderr, "No mem for FDs\n");
     rc = EXIT_FAILURE;
     goto quit;
   }

   for (lerror=false; lerror==false || lerror==nodef;)
     if ((lerror=fF_readln(myfile))==false)
     {
       fF_SetError(myfile, false); /* continue even on errors */
       tmpstr=fF_FuncName(myfile);

       if (tmpstr)
       {
	 fdDef **res;
	 fD_NewName(tmpdef, tmpstr);
	 res=(fdDef**)bsearch(&tmpdef, arrdefs, fds, sizeof arrdefs[0],
			      fD_cmpName);

	 if (res)
	 {
	   founddef=*res;
	   DBP(fprintf(stderr, "found (%s).\n", fD_GetName(founddef)));
	   fF_SetError(myfile, false);
	   lerror=fD_parsepr(founddef, myfile);
	 }
	 else
	   if (!ishandleddifferently(myfile->line, tmpstr))
	     if (!Quiet)
	       fprintf(stderr, "Don't know what to do with <%s> in line %lu.\n",
		       tmpstr, myfile->lineno);
	 free(tmpstr);
       }
     }

   fD_dtor(tmpdef);

   fF_dtor(myfile);

   if (strlen(fdfilename)>7 &&
   !strcmp(fdfilename+strlen(fdfilename)-7, "_lib.fd"))
   {
      char *str=fdfilename+strlen(fdfilename)-8;
      while (str!=fdfilename && str[-1]!='/' && str[-1]!=':')
	 str--;
//lcs      strncpy(BaseNamL, str, strlen(str)-7);
      strncpy(BaseNamU, str, strlen(str)-7);
      BaseNamU[strlen(str)-7]='\0';
      strcpy(BaseNamL, BaseNamU);
      strcpy(BaseNamC, BaseNamU);
   }
   else
   {
      strcpy(BaseNamU, BaseName);
      if (strlen(BaseNamU)>4 && strcmp(BaseNamU+strlen(BaseNamU)-4, "Base")==0)
	 BaseNamU[strlen(BaseNamU)-4]='\0';
      strcpy(BaseNamL, BaseNamU);
      strcpy(BaseNamC, BaseNamU);
   }
   StrUpr(BaseNamU);
   BaseNamC[0]=toupper(BaseNamC[0]);

   if (BaseName[0])
   {
      unsigned int count2;

      if (strlen(fdfilename)>6 &&
	  !strcmp(fdfilename+strlen(fdfilename)-6, "_gc.fd"))
	 type = GADGET;
      else if (strlen(fdfilename)>6 &&
	       !strcmp(fdfilename+strlen(fdfilename)-6, "_ic.fd"))
	 type = IMAGE;
      else if (strlen(fdfilename)>6 &&
	       !strcmp(fdfilename+strlen(fdfilename)-6, "_cl.fd"))
	 type = CLASS;
      else
      {
	 for (count2=0; count2<sizeof TypeTable/sizeof TypeTable[0]; count2+=2)
	 {
	    if (strcmp(BaseName, TypeTable[count2])==0)
	    {
	       type=TypeTable[count2+1];
	       break;
	    }
	 }
      }
   }

   if (outfilename)
   {
      if (!(outfile=fopen(outfilename, "w")))
      {
	 fprintf(stderr, "Couldn't open output file.\n");
	 rc = EXIT_FAILURE;
	 goto quit;
      }
      else
      {
	 closeoutfile=1;
      }
   }
   else
      outfile=stdout;

   fprintf(outfile, "* This SFD file was automatically generated by fd2sfd from\n");
   fprintf(outfile, "* %s and\n", fdfilename);
   fprintf(outfile, "* %s.\n", clibfilename);
	   
   if (BaseName[0])
   {
      fprintf(outfile, "==base _%s\n", BaseName);
      fprintf(outfile, "==basetype struct %s *\n", StdLib);
   }

   if (BaseName[0])
   {
      fprintf(outfile, "==libname %s.%s\n",
	      strcmp(BaseNamL, "cardres") == 0 ? "card" : BaseNamL, type);
   }

   clib = fopen( clibfilename, "r" );

   if (clib == NULL)
   {
      fprintf(stderr, "Couldn't open file '%s'.\n", clibfilename);
   }
   else
   {
      char* buffer = malloc(1024);

      if (buffer == NULL)
      {
	 fprintf(stderr, "No memory for line buffer.\n " );
      }
      else
      {
	 int got_exec_types = 0;
	 int got_utility_tagitem = 0;
	 
	 while (fgets(buffer, 1023, clib) != NULL)
	 {
	    int i = 0;

	    while (buffer[i] == ' ' || buffer[i] == '\t') ++i;

	    if( buffer[ i ] == '#') /* Pre-processor instruction */ 
	    {
	       ++i;

	       while (buffer[i] == ' ' || buffer[i] == '\t') ++i;

	       if (strncmp(buffer+i, "include",7) == 0)
	       {
		  char  start = 0;
		  char  end   = 0;
		  char* inc;

		  i += 7;

 		  while (buffer[i] == ' ' || buffer[i] == '\t') ++i;

		  start = buffer[i];
		  
		  if (start== '"' )
		     end = '"';
		  else if (start == '<')
		     end = '>';
		  else
		  {
		     fprintf(stderr, "Bad #include line\n");
		  }

		  ++i;

		  inc = buffer+i;

		  while (buffer[i] != end && buffer[i] != 0) ++i;
		  buffer[i] = 0;

		  if (strncmp(inc, "proto/", 6) &&
		      strncmp(inc, "pragma/", 7) &&
		      strncmp(inc, "ppcinline/", 10) &&
                      strncmp(inc, "ppcpragma/", 10) &&
		      strncmp(inc, "ppcproto/", 9) &&
		      strncmp(inc, "inline/", 7) &&
		      strncmp(inc, "stormprotos/", 12) )
		  {
		    fprintf(outfile, "==include %c%s%c\n", start, inc, end );
		  }

		  if (!strcmp(inc,"exec/types.h"))
		    got_exec_types = 1;
		  else if (!strcmp(inc,"utility/tagitem.h"))
		    got_utility_tagitem = 1;
	       }
	    }
	    else if (!strncmp(buffer+i,"typedef",7))
	    {
	       char* td;

	       i += 7;
		 
	       while (buffer[i] == ' ' || buffer[i] == '\t') ++i;

	       td = buffer+i;
	       
	       while (buffer[i] != ';' && buffer[i] != 0) ++i;
	       buffer[i] = 0;

	       fprintf(outfile, "* Unofficial extension on next line\n");
	       fprintf(outfile, "==typedef %s\n", td);
	    }
	 }

	 // We always need these (for basic types like ULONG and Tag)
	 if (!got_exec_types)
	   fprintf(outfile, "==include <exec/types.h>\n");

	 if (!got_utility_tagitem)
	   fprintf(outfile, "==include <utility/tagitem.h>\n");
	 
	 free(buffer);
      }
      
      fclose(clib);
   }

   for (count=0; count<fds && defs[count]; count++)
   {
      DBP(fprintf(stderr, "outputting %ld...\n", count));
      if (arrcmts[count])
      {
	fprintf(outfile, "%s", arrcmts[count]);
      }
      fD_write(outfile, defs[count], 0);
   }

   fprintf(outfile, "==end\n");

   rc = EXIT_SUCCESS;
  quit:
   for (count=0; count<FDS && arrdefs[count]; count++)
      fD_dtor(arrdefs[count]);
   for (count=0; count<FDS && arrcmts[count]; count++)
      free(arrcmts[count]);

   free(defs);
   free(arrdefs);
   free(arrcmts);

   if (closeoutfile)
   {
      fclose(outfile);
   }

   return rc;
}
