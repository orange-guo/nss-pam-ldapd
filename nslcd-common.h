/*
   nslcd-common.h - helper macros for reading and writing in
                    protocol streams

   Copyright (C) 2006 West Consulting
   Copyright (C) 2006 Arthur de Jong

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with this library; if not, write to the Free
   Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston,
   MA 02110-1301 USA
*/

#ifndef _NSLCD_COMMON_H
#define _NSLCD_COMMON_H 1

/* WRITE marcos, used for writing data, on write error they will
   call the ERROR_OUT_WRITEERROR macro
   these macros may require the availability of the following
   variables:
   int32_t tmpint32; - temporary variable
   */

#ifdef DEBUG_PROT
/* define a debugging macro to output logging */
#include <string.h>
#include <errno.h>
#define DEBUG_PRINT(fmt,arg) \
  fprintf(stderr,"%s:%d:%s: " fmt"\n",__FILE__,__LINE__,__PRETTY_FUNCTION__,arg);
#else /* DEBUG_PROT */
/* define an empty debug macro to disable logging */
#define DEBUG_PRINT(fmt,arg)
#endif /* not DEBUG_PROT */

#define WRITE(fp,ptr,size) \
  DEBUG_PRINT("WRITE       : var="__STRING(ptr)" size=%d",(int)size); \
  if (fwrite(ptr,size,1,fp)<1) \
  { \
    DEBUG_PRINT("WRITE       : var="__STRING(ptr)" error: %s",strerror(errno)); \
    ERROR_OUT_WRITEERROR(fp); \
  }

#define WRITE_TYPE(fp,field,type) \
  WRITE(fp,&(field),sizeof(type))

#define WRITE_INT32(fp,i) \
  DEBUG_PRINT("WRITE_INT32 : var="__STRING(i)" int32=%d",(int)i); \
  tmpint32=(int32_t)(i); \
  WRITE_TYPE(fp,tmpint32,int32_t)

#define WRITE_STRING(fp,str) \
  DEBUG_PRINT("WRITE_STRING: var="__STRING(str)" string=\"%s\"",str); \
  WRITE_INT32(fp,strlen(str)); \
  if (tmpint32>0) \
    { WRITE(fp,str,tmpint32); }

#define WRITE_FLUSH(fp) \
  if (fflush(fp)<0) \
  { \
    DEBUG_PRINT("WRITE_FLUSH : error: %s",strerror(errno)); \
    ERROR_OUT_WRITEERROR(fp); \
  }

#define WRITE_STRINGLIST_NUM(fp,arr,num) \
  /* write number of strings */ \
  DEBUG_PRINT("WRITE_STRLST: var="__STRING(arr)" num=%d",(int)num); \
  WRITE_INT32(fp,num); \
  /* write strings */ \
  for (tmp2int32=0;tmp2int32<(num);tmp2int32++) \
  { \
    WRITE_STRING(fp,(arr)[tmp2int32]); \
  }

#define WRITE_STRINGLIST_NULLTERM(fp,arr) \
  /* first determin length of array */ \
  for (tmp3int32=0;(arr)[tmp3int32]!=NULL;tmp3int32++) \
    /*noting*/ ; \
  /* write number of strings */ \
  DEBUG_PRINT("WRITE_STRLST: var="__STRING(arr)" num=%d",(int)tmp3int32); \
  WRITE_TYPE(fp,tmp3int32,int32_t); \
  /* write strings */ \
  for (tmp2int32=0;tmp2int32<tmp3int32;tmp2int32++) \
  { \
    WRITE_STRING(fp,(arr)[tmp2int32]); \
  }

/* READ macros, used for reading data, on read error they will
   call the ERROR_OUT_READERROR or ERROR_OUT_BUFERROR macro
   these macros may require the availability of the following
   variables:
   int32_t tmpint32; - temporary variable
   char *buffer;     - pointer to a buffer for reading strings
   size_t buflen;    - the size of the buffer
   size_t bufptr;    - the current position in the buffer
   */

#define READ(fp,ptr,size) \
  if (fread(ptr,size,1,fp)<1) \
  { \
    DEBUG_PRINT("READ       : var="__STRING(ptr)" error: %s",strerror(errno)); \
    ERROR_OUT_READERROR(fp); \
  } \
  DEBUG_PRINT("READ       : var="__STRING(ptr)" size=%d",(int)size);

#define READ_TYPE(fp,field,type) \
  READ(fp,&(field),sizeof(type))

#define READ_INT32(fp,i) \
  READ_TYPE(fp,tmpint32,int32_t); \
  i=tmpint32; \
  DEBUG_PRINT("READ_INT32 : var="__STRING(i)" int32=%d",(int)i);

/* current position in the buffer */
#define BUF_CUR \
  (buffer+bufptr)

/* check that the buffer has sz bytes left in it */
#define BUF_CHECK(fp,sz) \
  if ((bufptr+(size_t)(sz))>buflen) \
  { \
    DEBUG_PRINT("READ       : buffer error: %d bytes missing",((sz)-(buflen))); \
    ERROR_OUT_BUFERROR(fp); \
  } /* will not fit */

/* move the buffer pointer */
#define BUF_SKIP(sz) \
  bufptr+=(size_t)(sz);

/* read string in the buffer (using buffer, buflen and bufptr)
   and store the actual location of the string in field */
#define READ_STRING_BUF(fp,field) \
  /* read the size of the string */ \
  READ_TYPE(fp,tmpint32,int32_t); \
  DEBUG_PRINT("READ_STRING: var="__STRING(field)" strlen=%d",tmpint32); \
  /* check if read would fit */ \
  BUF_CHECK(fp,tmpint32+1); \
  /* read string from the stream */ \
  if (tmpint32>0) \
    { READ(fp,BUF_CUR,(size_t)tmpint32); } \
  /* null-terminate string in buffer */ \
  BUF_CUR[tmpint32]='\0'; \
  DEBUG_PRINT("READ_STRING: var="__STRING(field)" string=\"%s\"",BUF_CUR); \
  /* prepare result */ \
  (field)=BUF_CUR; \
  BUF_SKIP(tmpint32+1);

/* read a string from the stream dynamically allocating memory
   for the string (don't forget to call free() later on) */
#define READ_STRING_ALLOC(fp,field) \
  /* read the size of the string */ \
  READ_TYPE(fp,tmpint32,int32_t); \
  /* allocate memory */ \
  (field)=(char *)malloc((size_t)(tmpint32+1)); \
  if ((field)==NULL) \
  { \
    DEBUG_PRINT("READ_STRING: var="__STRING(field)" malloc() error: %s",strerror(errno)); \
    ERROR_OUT_ALLOCERROR(fp); \
  } /* problem allocating */ \
  /* read string from the stream */ \
  if (tmpint32>0) \
    { READ(fp,name,(size_t)tmpint32); } \
  /* null-terminate string */ \
  (name)[tmpint32]='\0'; \
  DEBUG_PRINT("READ_STRING: var="__STRING(field)" string=\"%s\"",(name));

/* read an array from a stram and store the length of the
   array in num (size for the array is allocated) */
#define READ_STRINGLIST_NUM(fp,arr,num) \
  /* read the number of entries */ \
  READ_INT32(fp,(num)); \
  DEBUG_PRINT("READ_STRLST: var="__STRING(arr)" num=%d",(int)(num)); \
  /* allocate room for *char[num] */ \
  tmpint32*=sizeof(char *); \
  BUF_CHECK(fp,tmpint32); \
  (arr)=(char **)BUF_CUR; \
  BUF_SKIP(tmpint32); \
  for (tmp2int32=0;tmp2int32<(num);tmp2int32++) \
  { \
    READ_STRING_BUF(fp,(arr)[tmp2int32]); \
  }

/* read an array from a stram and store it as a null-terminated
   array list (size for the array is allocated) */
#define READ_STRINGLIST_NULLTERM(fp,arr) \
  /* read the number of entries */ \
  READ_TYPE(fp,tmp3int32,int32_t); \
  DEBUG_PRINT("READ_STRLST: var="__STRING(arr)" num=%d",(int)tmp3int32); \
  /* allocate room for *char[num+1] */ \
  tmp2int32=(tmp3int32+1)*sizeof(char *); \
  BUF_CHECK(fp,tmp2int32); \
  (arr)=(char **)BUF_CUR; \
  BUF_SKIP(tmp2int32); \
  /* read all entries */ \
  for (tmp2int32=0;tmp2int32<tmp3int32;tmp2int32++) \
  { \
    READ_STRING_BUF(fp,(arr)[tmp2int32]); \
  } \
  /* set last entry to NULL */ \
  (arr)[tmp2int32]=NULL;

/* skip a number of bytes foreward
   Note that this macro modifies the sz variable */
#define SKIP(fp,sz) \
  DEBUG_PRINT("READ       : skip %d bytes",(int)(sz)); \
  /* read (skip) the specified number of bytes */ \
  while ((sz)-->0) \
    if (fgetc(fp)==EOF) \
    { \
      DEBUG_PRINT("READ       : skip error: %s",strerror(errno)); \
      ERROR_OUT_READERROR(fp); \
    }

/* read a string from the stream but don't do anything with the result */
#define SKIP_STRING(fp) \
  /* read the size of the string */ \
  READ_TYPE(fp,tmpint32,int32_t); \
  DEBUG_PRINT("READ_STRING: skip %d bytes",(int)tmpint32); \
  /* read (skip) the specified number of bytes */ \
  SKIP(fp,tmpint32);

/* skip a loop of strings */
#define SKIP_STRINGLIST(fp) \
  /* read the number of entries */ \
  READ_TYPE(fp,tmp3int32,int32_t); \
  DEBUG_PRINT("READ_STRLST: skip %d strings",(int)tmp3int32); \
  /* read all entries */ \
  for (tmp2int32=0;tmp2int32<tmp3int32;tmp2int32++) \
  { \
    SKIP_STRING(fp); \
  }

#endif /* not _NSLCD_COMMON_H */
