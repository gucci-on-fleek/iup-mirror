/** \file
 * \brief String Utilities
 *
 * See Copyright Notice in "iup.h"
 */

 
#include <string.h>  
#include <stdlib.h>  
#include <stdio.h>  
#include <limits.h>
#include <stdarg.h>

#include "iup_str.h"


/* Line breaks can be:
\n - UNIX
\r - Mac
\r\n - DOS/Windows
*/

#define iStrIsDigit(_c) (_c>='0' && _c<='9')

#define iStrUpper(_c)  ((_c >= 'a' && _c <= 'z')? (_c - 'a') + 'A': _c)

#define iStrLower(_c)  ((_c >= 'A' && _c <= 'Z')? (_c - 'A') + 'a': _c)

#define IUP_STR_EQUAL(str1, str2)      \
{                                      \
  if (str1 == str2)                    \
    return 1;                          \
                                       \
  if (!str1 || !str2)                  \
    return 0;                          \
                                       \
  while(*str1 && *str2 &&              \
        SF(*str1) == SF(*str2))        \
  {                                    \
    EXTRAINC(str1);                    \
    EXTRAINC(str2);                    \
    str1++;                            \
    str2++;                            \
  }                                    \
                                       \
  /* check also for terminator */      \
  if (*str1 == *str2) return 1;        \
}

int iupStrEqual(const char* str1, const char* str2) 
{
#define EXTRAINC(_x) (_x)
#define SF(_x) (_x)
  IUP_STR_EQUAL(str1, str2);
#undef SF
#undef EXTRAINC
  return 0;
}

int iupStrEqualPartial(const char* str1, const char* str2) 
{
#define EXTRAINC(_x) (_x)
#define SF(_x) (_x)
  IUP_STR_EQUAL(str1, str2);
#undef SF
#undef EXTRAINC
  if (*str2 == 0) 
    return 1;  /* if second string is at terminator, then it is partially equal */
  return 0;
}

int iupStrEqualNoCase(const char* str1, const char* str2) 
{
#define EXTRAINC(_x) (_x)
#define SF(_x) iStrLower(_x)
  IUP_STR_EQUAL(str1, str2);
#undef SF
#undef EXTRAINC
  return 0;
}

int iupStrEqualNoCasePartial(const char* str1, const char* str2) 
{
#define EXTRAINC(_x) (_x)
#define SF(_x) iStrLower(_x)
  IUP_STR_EQUAL(str1, str2);
#undef SF
#undef EXTRAINC
  if (*str2 == 0) 
    return 1;  /* if second string is at terminator, then it is partially equal */
  return 0;
}

int iupStrEqualNoCaseNoSpace(const char* str1, const char* str2) 
{
#define EXTRAINC(_x) { if (*_x == ' ') _x++; }  /* also ignore spaces */
#define SF(_x) iStrLower(_x)
  IUP_STR_EQUAL(str1, str2);
#undef SF
#undef EXTRAINC
  return 0;
}

int iupStrFalse(const char* str)
{
  if (!str || str[0]==0) return 0;
  if (str[0]=='0' && str[1]==0) return 1;
  if (iupStrEqualNoCase(str, "NO")) return 1;
  if (iupStrEqualNoCase(str, "OFF")) return 1;
  return 0;
}

int iupStrBoolean(const char* str)
{
  if (!str || str[0]==0) return 0;
  if (str[0]=='1' && str[1]==0) return 1;
  if (iupStrEqualNoCase(str, "YES")) return 1;
  if (iupStrEqualNoCase(str, "ON")) return 1;
  return 0;
}

void iupStrUpper(char* dstr, const char* sstr)
{
  for (; *sstr; sstr++, dstr++)
    *dstr = (char)iStrUpper(*sstr);
  *dstr = 0;
}

void iupStrLower(char* dstr, const char* sstr)
{
  for (; *sstr; sstr++, dstr++)
    *dstr = (char)iStrLower(*sstr);
  *dstr = 0;
}

int iupStrHasSpace(const char* str)
{
  while(*str)
  {
    if (*str == ' ')
      return 1;
    str++;
  }
  return 0;
}

char *iupStrDup(const char *str)
{
  if (str)
  {
    int size = strlen(str)+1;
    char *newstr = malloc(size);
    if (newstr) memcpy(newstr, str, size);
    return newstr;
  }
  return NULL;
}

const char* iupStrNextLine(const char* str, int *len)
{
  *len = 0;

  while(*str!=0 && *str!='\n' && *str!='\r') 
  {
    (*len)++;
    str++;
  }

  if (*str=='\r' && *(str+1)=='\n')   /* DOS line end */
    return str+2;
  else if (*str=='\n' || *str=='\r')   /* UNIX or MAC line end */
    return str+1;
  else 
    return str;  /* no next line */
}

const char* iupStrNextValue(const char* str, int str_len, int *len, char sep)
{
  *len = 0;

  while(*str!=0 && *str!=sep && *len<str_len) 
  {
    (*len)++;
    str++;
  }

  if (*str==sep)
    return str+1;
  else 
    return str;  /* no next value */
}

int iupStrLineCount(const char* str)
{
  int num_lin = 1;

  if (!str)
    return num_lin;

  while(*str != 0)
  {
    while(*str!=0 && *str!='\n' && *str!='\r')
      str++;

    if (*str=='\r' && *(str+1)=='\n')   /* DOS line end */
    {
      num_lin++;
      str+=2;
    }
    else if (*str=='\n' || *str=='\r')   /* UNIX or MAC line end */
    {
      num_lin++;
      str++;
    }
  }

  return num_lin;
}

int iupStrCountChar(const char *str, char c)
{
  int n;
  if (!str) return 0;
  for (n=0; *str; str++)
  {
    if (*str==c)
      n++;
  }
  return n;
}

void iupStrCopyN(char* dst_str, int dst_max_size, const char* src_str)
{
  int size = strlen(src_str)+1;
  if (size > dst_max_size) size = dst_max_size;
  memcpy(dst_str, src_str, size-1);
  dst_str[size-1] = 0;
}

char* iupStrDupUntil(char **str, char c)
{
  char *p_str,*new_str;
  if (!str || *str==NULL)
    return NULL;

  p_str=strchr(*str,c);
  if (!p_str) 
    return NULL;
  else
  {
    int i;
    int sl = (int)(p_str - (*str));

    new_str = (char *)malloc(sl + 1);
    if (!new_str) return NULL;

    for (i = 0; i < sl; ++i)
      new_str[i] = (*str)[i];

    new_str[sl] = 0;

    *str = p_str+1;
    return new_str;
  }
}

static char *iStrDupUntilNoCase(char **str, char sep)
{
  char *p_str,*new_str;
  if (!str || *str==NULL)
    return NULL;

  p_str=strchr(*str,sep); /* usually the lower case is enough */
  if (!p_str && (iStrUpper(sep)!=sep)) 
    p_str=strchr(*str, iStrUpper(sep));  /* but check also for upper case */

  /* if both fail, then abort */
  if (!p_str) 
    return NULL;
  else
  {
    int i;
    int sl=(int)(p_str - (*str));

    new_str = (char *) malloc (sl + 1);
    if (!new_str) return NULL;

    for (i = 0; i < sl; ++i)
      new_str[i] = (*str)[i];

    new_str[sl] = 0;

    *str = p_str+1;
    return new_str;
  }
}

char *iupStrGetLargeMem(int *size)
{
#define LARGE_MAX_BUFFERS 10
#define LARGE_SIZE SHRT_MAX
  static char buffers[LARGE_MAX_BUFFERS][LARGE_SIZE];
  static int buffers_index = -1;
  char* ret_str;

  /* init buffers array */
  if (buffers_index == -1)
  {
    int i;

    memset(buffers, 0, sizeof(char*)*LARGE_MAX_BUFFERS);
    buffers_index = 0;

    /* clear all memory only once */
    for (i=0; i<LARGE_MAX_BUFFERS; i++)
      memset(buffers[i], 0, sizeof(char)*LARGE_SIZE);
  }

  /* DON'T clear memory everytime because the buffer is too large */
  ret_str = buffers[buffers_index];
  ret_str[0] = 0;

  buffers_index++;
  if (buffers_index == LARGE_MAX_BUFFERS)
    buffers_index = 0;

  if (size) *size = LARGE_SIZE;
  return ret_str;
#undef LARGE_MAX_BUFFERS
#undef LARGE_SIZE 
}

static char* iupStrGetSmallMem(void)
{
#define SMALL_MAX_BUFFERS 100
#define SMALL_SIZE 80  /* maximum for iupStrReturnFloat */
  static char buffers[SMALL_MAX_BUFFERS][SMALL_SIZE];
  static int buffers_index = -1;
  char* ret_str;

  /* init buffers array */
  if (buffers_index == -1)
  {
    memset(buffers, 0, sizeof(char*)*SMALL_MAX_BUFFERS);
    buffers_index = 0;
  }

  /* always clear memory before returning a new buffer */
  memset(buffers[buffers_index], 0, SMALL_SIZE);
  ret_str = buffers[buffers_index];

  buffers_index++;
  if (buffers_index == SMALL_MAX_BUFFERS)
    buffers_index = 0;

  return ret_str;
#undef SMALL_MAX_BUFFERS
#undef SMALL_SIZE 
}

char *iupStrGetMemory(int size)
{
#define MAX_BUFFERS 50
  static char* buffers[MAX_BUFFERS];
  static int buffers_sizes[MAX_BUFFERS];
  static int buffers_index = -1;

  int i;

  if (size == -1) /* Frees memory */
  {
    buffers_index = -1;
    for (i = 0; i < MAX_BUFFERS; i++)
    {
      if (buffers[i]) 
      {
        free(buffers[i]);
        buffers[i] = NULL;
      }
      buffers_sizes[i] = 0;
    }
    return NULL;
  }
  else
  {
    char* ret_str;

    /* init buffers array */
    if (buffers_index == -1)
    {
      memset(buffers, 0, sizeof(char*)*MAX_BUFFERS);
      memset(buffers_sizes, 0, sizeof(int)*MAX_BUFFERS);
      buffers_index = 0;
    }

    /* first alocation */
    if (!(buffers[buffers_index]))
    {
      buffers_sizes[buffers_index] = size+1;
      buffers[buffers_index] = (char*)malloc(buffers_sizes[buffers_index]);
    }
    else if (buffers_sizes[buffers_index] < size+1)  /* reallocate if necessary */
    {
      buffers_sizes[buffers_index] = size+1;
      buffers[buffers_index] = (char*)realloc(buffers[buffers_index], buffers_sizes[buffers_index]);
    }

    /* always clear memory before returning a new buffer */
    memset(buffers[buffers_index], 0, buffers_sizes[buffers_index]);
    ret_str = buffers[buffers_index];

    buffers_index++;
    if (buffers_index == MAX_BUFFERS)
      buffers_index = 0;

    return ret_str;
  }
#undef MAX_BUFFERS
}

char* iupStrReturnStrf(const char* format, ...)
{
  char* value = iupStrGetMemory(1024);
  va_list arglist;
  va_start(arglist, format);
  vsnprintf(value, 1024, format, arglist);
  va_end(arglist);
  return value;
}

char* iupStrReturnStr(const char* str)
{
  if (str)
  {
    int size = strlen(str)+1;
    char* ret_str = iupStrGetMemory(size);
    memcpy(ret_str, str, size);
    return ret_str;
  }
  else
    return NULL;
}

char* iupStrReturnBoolean(int b)
{
  if (b)
    return "YES";
  else
    return "NO";
}

char* iupStrReturnChecked(int check)
{
  if (check == -1)
    return "NOTDEF";
  else if (check)
    return "ON";
  else
    return "OFF";
}

char* iupStrReturnInt(int i)
{
  char* str = iupStrGetSmallMem();  /* 20 */
  sprintf(str, "%d", i);
  return str;
}

char* iupStrReturnFloat(float f)
{
  char* str = iupStrGetSmallMem();  /* 80 */
  sprintf(str, "%.9f", f);  /* maximum float precision */
  return str;
}

char* iupStrReturnRGB(unsigned char r, unsigned char g, unsigned char b)
{
  char* str = iupStrGetSmallMem();  /* 3*20 */
  sprintf(str, "%d %d %d", (int)r, (int)g, (int)b);
  return str;
}

char* iupStrReturnStrStr(const char *str1, const char *str2, char sep)
{
  if (str1 || str2)
  {
    char* ret_str;
    int size1=0, size2=0;
    if (str1) size1 = strlen(str1);
    if (str2) size2 = strlen(str2);
    ret_str = iupStrGetMemory(size1+size2+2);
    if (str1 && size1) memcpy(ret_str, str1, size1);
    ret_str[size1] = sep;
    if (str2 && size2) memcpy(ret_str+size1+1, str2, size2);
    ret_str[size1+1+size2] = 0;
    return ret_str;
  }
  else
    return NULL;
}

char* iupStrReturnIntInt(int i1, int i2, char sep)
{
  char* str = iupStrGetSmallMem();  /* 2*20 */
  sprintf(str, "%d%c%d", i1, sep, i2);
  return str;
}

int iupStrToRGB(const char *str, unsigned char *r, unsigned char *g, unsigned char *b)
{
  unsigned int ri = 0, gi = 0, bi = 0;
  if (!str) return 0;
  if (str[0]=='#')
  {
    str++;
    if (sscanf(str, "%2X%2X%2X", &ri, &gi, &bi) != 3) return 0;
  }
  else
  {
    if (sscanf(str, "%u %u %u", &ri, &gi, &bi) != 3) return 0;
  }
  if (ri > 255 || gi > 255 || bi > 255) return 0;
  *r = (unsigned char)ri;
  *g = (unsigned char)gi;
  *b = (unsigned char)bi;
  return 1;
}

int iupStrToInt(const char *str, int *i)
{
  if (!str) return 0;
  if (sscanf(str, "%d", i) != 1) return 0;
  return 1;
}

int iupStrToIntInt(const char *str, int *i1, int *i2, char sep)
{
  if (!str) return 0;
                         
  if (iStrLower(*str) == sep) /* no first value */
  {
    str++; /* skip separator */
    if (sscanf(str, "%d", i2) != 1) return 0;
    return 1;
  }
  else 
  {
    char* p_str = iStrDupUntilNoCase((char**)&str, sep);
    
    if (!p_str)   /* no separator means no second value */
    {        
      if (sscanf(str, "%d", i1) != 1) return 0;
      return 1;
    }
    else if (*str==0)  /* separator exists, but second value empty, also means no second value */
    {        
      int ret = sscanf(p_str, "%d", i1);
      free(p_str);
      if (ret != 1) return 0;
      return 1;
    }
    else
    {
      int ret = 0;
      if (sscanf(p_str, "%d", i1) == 1) ret++;
      if (sscanf(str, "%d", i2) == 1) ret++;
      free(p_str);
      return ret;
    }
  }
}

int iupStrToFloat(const char *str, float *f)
{
  if (!str) return 0;
  if (sscanf(str, "%f", f) != 1) return 0;
  return 1;
}

int iupStrToFloatFloat(const char *str, float *f1, float *f2, char sep)
{
  if (!str) return 0;

  if (iStrLower(*str) == sep) /* no first value */
  {
    str++; /* skip separator */
    if (sscanf(str, "%f", f2) != 1) return 0;
    return 1;
  }
  else 
  {
    char* p_str = iStrDupUntilNoCase((char**)&str, sep);
    
    if (!p_str)   /* no separator means no second value */
    {        
      if (sscanf(str, "%f", f1) != 1) return 0;
      return 1;
    }
    else if (*str==0)    /* separator exists, but second value empty, also means no second value */
    {        
      int ret = sscanf(p_str, "%f", f1);
      free(p_str);
      if (ret != 1) return 0;
      return 1;
    }
    else
    {
      int ret = 0;
      if (sscanf(p_str, "%f", f1) != 1) ret++;
      if (sscanf(str, "%f", f2) != 1) ret++;
      free(p_str);
      return ret;
    }
  }
}

int iupStrToStrStr(const char *str, char *str1, char *str2, char sep)
{
  if (!str) return 0;

  if (iStrLower(*str) == sep) /* no first value */
  {
    str++; /* skip separator */
    strcpy(str2, str);
    return 1;
  }
  else 
  {
    char* p_str = iStrDupUntilNoCase((char**)&str, sep);
    
    if (!p_str)   /* no separator means no second value */
    {        
      strcpy(str1, str);
      return 1;
    }
    else if (*str==0)    /* separator exists, but second value empty, also means no second value */
    {        
      strcpy(str1, p_str);
      free(p_str);
      return 1;
    }
    else
    {
      strcpy(str1, p_str);
      strcpy(str2, str);
      free(p_str);
      return 2;
    }
  }
}

char* iupStrFileGetPath(const char *file_name)
{
  /* Starts at the last character */
  int len = strlen(file_name) - 1;
  while (len != 0)
  {
    if (file_name[len] == '\\' || file_name[len] == '/')
    {
      len++;
      break;
    }

    len--;
  }                                                          
  if (len == 0)
    return NULL;

  {
    char* path = malloc(len+1);
    memcpy(path, file_name, len);
    path[len] = 0;

    return path;
  }
}

char* iupStrFileGetTitle(const char *file_name)
{
  /* Starts at the last character */
  int len = strlen(file_name);
  int offset = len - 1;
  while (offset != 0)
  {
    if (file_name[offset] == '\\' || file_name[offset] == '/')
    {
      offset++;
      break;
    }

    offset--;
  }

  {
    int title_size = len - offset + 1;
    char* file_title = malloc(title_size);
    memcpy(file_title, file_name + offset, title_size);
    return file_title;
  }
}

char* iupStrFileGetExt(const char *file_name)
{
  /* Starts at the last character */
  int len = strlen(file_name);
  int offset = len - 1;
  while (offset != 0)
  {
    /* if found a path separator stop. */
    if (file_name[offset] == '\\' || file_name[offset] == '/')
      return NULL;

    if (file_name[offset] == '.')
    {
      offset++;
      break;
    }

    offset--;
  }

  if (offset == 0) 
    return NULL;

  {
    int ext_size = len - offset + 1;
    char* file_ext = (char*)malloc(ext_size);
    memcpy(file_ext, file_name + offset, ext_size);
    return file_ext;
  }
}

char* iupStrFileMakeFileName(const char* path, const char* title)
{
  int size_path = strlen(path);
  int size_title = strlen(title);
  char *filename = malloc(size_path + size_title + 2);
  memcpy(filename, path, size_path);

  if (path[size_path-1] != '/')
  {
    filename[size_path] = '/';
    size_path++;
  }

  memcpy(filename+size_path, title, size_title);
  filename[size_path+size_title] = 0;

  return filename;
}

void iupStrFileNameSplit(const char* filename, char *path, char *title)
{
  int i, n = strlen(filename);

  /* Look for last folder separator and split title from path */
  for (i=n-1;i>=0; i--)
  {
    if (filename[i] == '\\' || filename[i] == '/') 
    {
      if (path)
      {
        strncpy(path, filename, i+1);
        path[i+1] = 0;
      }

      if (title)
      {
        strcpy(title, filename+i+1);
        title[n-i] = 0;
      }

      return;
    }
  }
}

int iupStrReplace(char* str, char src, char dst)
{
  int i = 0;
  while (*str)
  {
    if (*str == src)
    {
      *str = dst;
      i++;
    }
    str++;
  }
  return i;
}

void iupStrToUnix(char* str)
{
  char* pstr = str;

  if (!str) return;
  
  while (*str)
  {
    if (*str == '\r')
    {
      if (*(str+1) != '\n')  /* MAC line end */
        *pstr++ = '\n';
      str++;
    }
    else
      *pstr++ = *str++;
  }
  
  *pstr = *str;
}

void iupStrToMac(char* str)
{
  char* pstr = str;

  if (!str) return;
  
  while (*str)
  {
    if (*str == '\r')
    {
      if (*(++str) == '\n')  /* DOS line end */
        str++;
      *pstr++ = '\r';
    }
    else if (*str == '\n')  /* UNIX line end */
    {
      str++;
      *pstr++ = '\r';
    }
    else
      *pstr++ = *str++;
  }
  
  *pstr = *str;
}

char* iupStrToDos(const char* str)
{
	char *auxstr, *newstr;
	int num_lin;

  if (!str) return NULL;

  num_lin = iupStrLineCount(str);
  if (num_lin == 1)
    return (char*)str;

	newstr = malloc(num_lin + strlen(str) + 1);
  auxstr = newstr;
	while(*str)
	{
		if (*str == '\r' && *(str+1)=='\n')  /* DOS line end */
    {
      *auxstr++ = *str++;
      *auxstr++ = *str++;
    }
    else if (*str == '\r')   /* MAC line end */
    {
		  *auxstr++ = *str++;
			*auxstr++ = '\n';
    }
    else if (*str == '\n')  /* UNIX line end */
    {
			*auxstr++ = '\r';
		  *auxstr++ = *str++;
    }
    else
		  *auxstr++ = *str++;
	}
	*auxstr = 0;

	return newstr;	
}

#define IUP_ISRESERVED(_c) (_c=='\n' || _c=='\r' || _c=='\t')

char* iupStrConvertToC(const char* str)
{
  char* new_str, *pnstr;
  const char* pstr = str;
  int len, count=0;
  while(*pstr)
  {
    if (IUP_ISRESERVED(*pstr))
      count++;
    pstr++;
  }
  if (!count)
    return (char*)str;

  len = pstr-str;
  new_str = malloc(len+count+1);
  pstr = str;
  pnstr = new_str;
  while(*pstr)
  {
    if (IUP_ISRESERVED(*pstr))
    {
      *pnstr = '\\';
      pnstr++;

      switch(*pstr)
      {
      case '\n':
        *pnstr = 'n';
        break;
      case '\r':
        *pnstr = 'r';
        break;
      case '\t':
        *pnstr = 't';
        break;
      }
    }
    else
      *pnstr = *pstr;

    pnstr++;
    pstr++;
  }
  *pnstr = 0;
  return new_str;
}

char* iupStrProcessMnemonic(const char* str, char *c, int action)
{
  int i = 0, found = 0;
  char* new_str, *orig_str = (char*)str;

  if (!str) 
    return NULL;

  if (!strchr(str, '&'))
    return (char*)str;

  new_str = malloc(strlen(str)+1);
  while (*str)
  {
    if (*str == '&')
    {
      if (*(str+1) == '&') /* remove & from the string, add next & to the string */
      {
        found = -1;

        str++;
        new_str[i++] = *str;
      }
      else if (found!=1) /* mnemonic found */
      {
        found = 1;

        if (action == 1) /* replace & by c */
          new_str[i++] = *c;
        else if (action == -1)  /* remove & and return in c */
          *c = *(str+1);  /* next is mnemonic */
        /* else -- only remove & */
      }
    }
    else
    {
      new_str[i++] = *str;
    }

    str++;
  }
  new_str[i] = 0;

  if (found==0)
  {
    free(new_str);
    return orig_str;
  }

  return new_str;
}

int iupStrFindMnemonic(const char* str)
{
  int c = 0, found = 0;

  if (!str) 
    return 0;

  if (!strchr(str, '&'))
    return 0;

  while (*str)
  {
    if (*str == '&')
    {
      if (*(str+1) == '&')
      {
        found = -1;
        str++;
      }
      else if (found!=1) /* mnemonic found */
      {
        found = 1;
        c = *(str+1);  /* next is mnemonic */
      }
    }

    str++;
  }

  if (found==0)
    return 0;
  else
    return c;
}

#define ISTR_UTF8_BASE(_x)  (_x & 0xC0)  /* 11XX XXXX */
#define ISTR_UTF8_COUNT(_x) ((_x & 0xE0)? 3: ((_x & 0xF0)? 4: 2 ))
                            /* 1110 XXXX */  /* 1111 0XXX */  /* 110X XXXX */
#define ISTR_UTF8_SEQ(_x)   (_x & 0x80)  /* 10XX XXXX */

static void iStrFixPosUTF8(const char* value, int *start, int *end)
{
  int p = 0, i = 0, find = 0;
  while(value[i])
  {
    if (find==0 && p == *start)
    {
      *start = i;
      find = 1;
    }
    if (find==1 && p == *end)
    {
      *end = i;
      return;
    }

    if (ISTR_UTF8_BASE(value[i+1]) != 0x80)  /* is 1o byte of a sequence or is 1 byte only */
      p++;
    i++;
  }
}

void iupStrRemove(char* value, int start, int end, int dir, int utf8)
{
  if (start == end) 
  {
    if (dir==1)
      end++;
    else if (start == 0) /* there is nothing to remove before */
      return;
    else
      start--;
  }

  if (utf8)
    iStrFixPosUTF8(value, &start, &end);

  value += start;
  end -= start;
  while (*value)
  {
    *value = *(value+end);
    value++;
  }
}

char* iupStrInsert(const char* value, const char* insert_value, int start, int end, int utf8)
{
  char* new_value = (char*)value;
  int insert_len = strlen(insert_value);
  int len = strlen(value);

  if (utf8)
    iStrFixPosUTF8(value, &start, &end);

  if (end==start || insert_len > end-start)
  {
    new_value = malloc(len - (end-start) + insert_len + 1);
    memcpy(new_value, value, start);
    memcpy(new_value+start, insert_value, insert_len);
    memcpy(new_value+start+insert_len, value+end, len-end+1);
  }
  else
  {
    memcpy(new_value+start, insert_value, insert_len);
    memcpy(new_value+start+insert_len, value+end, len-end+1);
  }

  return new_value;
}

static unsigned char* ANSI_map = NULL;

static void iStrInitANSI_map(void)
{
  static char m[256];

  /* these characters are sorted in the same order as Excel would sort them */

  m[  0]=  0;  m[  1]=  1; m[  2]=  2; m[  3]=  3; m[  4]=  4; m[  5]=  5; m[  6]=  6; m[  7]=  7;  m[  8]=  8; m[  9]=  9; m[ 10]= 10; m[ 11]= 11; m[ 12]= 12; m[ 13]= 13; m[ 14]= 14; m[ 15]= 15; 
  m[ 16]= 16;  m[ 17]= 17; m[ 18]= 18; m[ 19]= 19; m[ 20]= 20; m[ 21]= 21; m[ 22]= 22; m[ 23]= 23;  m[ 24]= 24; m[ 25]= 25; m[ 26]= 26; m[ 27]= 27; m[ 28]= 28; m[ 29]= 29; m[ 30]= 30; m[ 31]= 31; 
  m['\'']= 32; m['-']= 33; m['�']= 34; m['�']= 35; m[' ']= 36; m['!']= 37; m['"']= 38; m['#']= 39;  m['$']= 40; m['%']= 41; m['&']= 42; m['(']= 43; m[')']= 44; m['*']= 45; m[',']= 46; m['.']= 47; 
  m['/']= 48;  m[':']= 49; m[';']= 50; m['?']= 51; m['@']= 52; m['[']= 53; m[']']= 54; m['^']= 55;  m['�']= 56; m['_']= 57; m['`']= 58; m['{']= 59; m['|']= 60; m['}']= 61; m['~']= 62; m['�']= 63; 
  m['�']= 64;  m['�']= 65; m['�']= 66; m['�']= 67; m['�']= 68; m['�']= 69; m['�']= 70; m['�']= 71;  m['�']= 72; m['�']= 73; m['�']= 74; m['�']= 75; m['�']= 76; m['�']= 77; m['�']= 78; m['�']= 79; 
  m['�']= 80;  m['�']= 81; m['�']= 82; m['�']= 83; m['+']= 84; m['<']= 85; m['=']= 86; m['>']= 87;  m['�']= 88; m['�']= 89; m['�']= 90; m['�']= 91; m['�']= 92; m['�']= 93; m['�']= 94; m['�']= 95; 
  m['�']= 96;  m['�']= 97; m['�']= 98; m['�']= 99; m['�']=100; m['�']=101; m['�']=102; m['�']=103;  m['�']=104; m['�']=105; m['0']=106; m['�']=107; m['�']=108; m['�']=109; m['1']=110; m['�']=111; 
  m['2']=112;  m['�']=113; m['3']=114; m['�']=115; m['4']=116; m['5']=117; m['6']=118; m['7']=119;  m['8']=120; m['9']=121; m['a']=122; m['A']=123; m['�']=124; m['�']=125; m['�']=126; m['�']=127; 
  m['�']=128;  m['�']=129; m['�']=130; m['�']=131; m['�']=132; m['�']=133; m['�']=134; m['�']=135;  m['�']=136; m['�']=137; m['�']=138; m['b']=139; m['B']=140; m['c']=141; m['C']=142; m['�']=143; 
  m['�']=144;  m['d']=145; m['D']=146; m['�']=147; m['�']=148; m['e']=149; m['E']=150; m['�']=151;  m['�']=152; m['�']=153; m['�']=154; m['�']=155; m['�']=156; m['�']=157; m['�']=158; m['f']=159; 
  m['F']=160;  m['�']=161; m['g']=162; m['G']=163; m['h']=164; m['H']=165; m['i']=166; m['I']=167;  m['�']=168; m['�']=169; m['�']=170; m['�']=171; m['�']=172; m['�']=173; m['�']=174; m['�']=175; 
  m['j']=176;  m['J']=177; m['k']=178; m['K']=179; m['l']=180; m['L']=181; m['m']=182; m['M']=183;  m['n']=184; m['N']=185; m['�']=186; m['�']=187; m['o']=188; m['O']=189; m['�']=190; m['�']=191; 
  m['�']=192;  m['�']=193; m['�']=194; m['�']=195; m['�']=196; m['�']=197; m['�']=198; m['�']=199;  m['�']=200; m['�']=201; m['�']=202; m['�']=203; m['�']=204; m['p']=205; m['P']=206; m['q']=207; 
  m['Q']=208;  m['r']=209; m['R']=210; m['s']=211; m['S']=212; m['�']=213; m['�']=214; m['�']=215;  m['t']=216; m['T']=217; m['�']=218; m['�']=219; m['�']=220; m['u']=221; m['U']=222; m['�']=223; 
  m['�']=224;  m['�']=225; m['�']=226; m['�']=227; m['�']=228; m['�']=229; m['�']=230; m['v']=231;  m['V']=232; m['w']=233; m['W']=234; m['x']=235; m['X']=236; m['y']=237; m['Y']=238; m['�']=239; 
  m['�']=240;  m['�']=241; m['�']=242; m['z']=243; m['Z']=244; m['�']=245; m['�']=246; m['\\']=247; m[127]=248; m[129]=249; m[141]=250; m[143]=251; m[144]=252; m[157]=253; m[160]=254; m[173]=255; 

  ANSI_map = (unsigned char*)m;
}

/*
The Alphanum Algorithm is an improved sorting algorithm for strings
containing numbers.  Instead of sorting numbers in ASCII order like a
standard sort, this algorithm sorts numbers in numeric order.

The Alphanum Algorithm is discussed at http://www.DaveKoelle.com/alphanum.html

This implementation is Copyright (c) 2008 Dirk Jagdmann <doj@cubic.org>.
It is a cleanroom implementation of the algorithm and not derived by
other's works. In contrast to the versions written by Dave Koelle this
source code is distributed with the libpng/zlib license.

This software is provided 'as-is', without any express or implied
warranty. In no event will the authors be held liable for any damages
arising from the use of this software.

Permission is granted to anyone to use this software for any purpose,
including commercial applications, and to alter it and redistribute it
freely, subject to the following restrictions:

    1. The origin of this software must not be misrepresented; you
       must not claim that you wrote the original software. If you use
       this software in a product, an acknowledgment in the product
       documentation would be appreciated but is not required.

    2. Altered source versions must be plainly marked as such, and
       must not be misrepresented as being the original software.

    3. This notice may not be removed or altered from any source
       distribution. */

/* The following code is based on the "alphanum.hpp" code 
   downloaded from Dave Koelle and implemented by Dirk Jagdmann.

   It was altered to be compiled in C and simplified to IUP needs.
*/

int iupStrCompare(const char *l, const char *r, int casesensitive, int utf8)
{
  enum mode_t { STRING, NUMBER } mode=STRING;

  if (!ANSI_map)
    iStrInitANSI_map();

  while(*l && *r)
  {
    if (mode == STRING)
    {
      while((*l) && (*r))
      {
        int diff;
        char l_char = *l, 
             r_char = *r;

        /* check if this are digit characters */
        int l_digit = iStrIsDigit(l_char), 
            r_digit = iStrIsDigit(r_char);

        /* if both characters are digits, we continue in NUMBER mode */
        if(l_digit && r_digit)
        {
          mode = NUMBER;
          break;
        }

        /* if only the left character is a digit, we have a result */
        if(l_digit) return -1;

        /* if only the right character is a digit, we have a result */
        if(r_digit) return +1;

        /* compute the difference of both characters */
        diff = ANSI_map[l_char] - ANSI_map[r_char];

        /* if they differ we have a result */
        if(diff != 0) return diff;

        /* otherwise process the next characters */
        ++l;
        ++r;
      }
    }
    else /* mode==NUMBER */
    {
      unsigned long r_int;
      long diff;

      /* get the left number */
      unsigned long l_int=0;
      while(*l && iStrIsDigit(*l))
      {
        /* TODO: this can overflow */
        l_int = l_int*10 + *l-'0';
        ++l;
      }

      /* get the right number */
      r_int=0;
      while(*r && iStrIsDigit(*r))
      {
        /* TODO: this can overflow */
        r_int = r_int*10 + *r-'0';
        ++r;
      }

      /* if the difference is not equal to zero, we have a comparison result */
      diff = l_int-r_int;
      if (diff != 0)
        return (int)diff;

      /* otherwise we process the next substring in STRING mode */
      mode=STRING;
    }
  }

  if (*r) return -1;
  if (*l) return +1;
  return 0;
}
