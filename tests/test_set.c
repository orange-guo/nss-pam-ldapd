/*
   test_set.c - simple test for the set module
   This file is part of the nss-pam-ldapd library.

   Copyright (C) 2008, 2009 Arthur de Jong

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with this library; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
   02110-1301 USA
*/

#include "config.h"

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdlib.h>

#include "common/set.h"
#include "compat/attrs.h"

/* the main program... */
int main(int UNUSED(argc),char UNUSED(*argv[]))
{
  SET *set;
  const char **list;
  int i;

  /* initialize */
  set=set_new();

  /* store some entries */
  set_add(set,"key1");
  set_add(set,"key2");
  set_add(set,"key3");
  set_add(set,"KEY2");

  /* check set contents */
  assert(set_contains(set,"KeY1"));
  assert(set_contains(set,"kEy2"));
  assert(set_contains(set,"KeY3"));
  assert(!set_contains(set,"key4"));

  /* loop over set contents */
  list=set_tolist(set);
  for (i=0;list[i]!=NULL;i++)
  {
    assert( (strcasecmp(list[i],"key1")==0) ||
            (strcasecmp(list[i],"key2")==0) ||
            (strcasecmp(list[i],"key3")==0) );
  }

  /* free set */
  set_free(set);
  free(list);

  return 0;
}
