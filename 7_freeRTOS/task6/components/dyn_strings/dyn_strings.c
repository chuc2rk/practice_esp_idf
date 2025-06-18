/*
 * Copyright https://github.com/kotyara12
 *
 * 
 */

#include "dyn_strings.h"
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

char * malloc_string(const char *source) 
{
  if (source) {
    uint32_t len = strlen(source);
    char *ret = (char*)malloc(len+1);
    if (ret != NULL) {
      memset(ret, 0, len+1);
      strcpy(ret, source);
      return ret;
    }
  };
  return NULL;
}

char * malloc_stringf(const char *format, ...) 
{
  char *ret = NULL;
  if (format != NULL) {
    /*
     * va_list declares variables to hold argument list
     * va_start initializes args1 with the variable arguments
     * va_copy makes a backup copy in args2 (needed because we'll use the args twice)
     */
    va_list args1, args2;
    va_start(args1, format);
    va_copy(args2, args1);

    /*
     * vsnprintf(NULL, 0, ...) is a trick to calculate needed string length
     * Passing NULL and 0 makes it return required buffer size without writing
     * You don’t need to call va_arg manually in your code 
     * because vsnprintf does it for you based on the format specifiers in the format string.
     * vsnprintf automatically fetches the arguments from va_list and uses them to format the string. 
     */
    int len = vsnprintf(NULL, 0, format, args1);
    va_end(args1);

    /*
     * allocate memory for string  
     */
    if (len > 0) {
      ret = (char*)malloc(len+1);
      if (ret != NULL) {
        memset(ret, 0, len+1);
        vsnprintf(ret, len+1, format, args2);
      };
    };
    va_end(args2);
  };
  return ret;
}
