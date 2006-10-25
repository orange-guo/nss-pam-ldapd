/*
   Copyright (C) 1997-2005 Luke Howard
   This file is part of the nss_ldap library.
   Contributed by Luke Howard, <lukeh@padl.com>, 1997.

   The nss_ldap library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   The nss_ldap library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with the nss_ldap library; see the file COPYING.LIB.  If not,
   write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.

   $Id$
*/

#include "config.h"

#include <sys/socket.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <arpa/nameser.h>
#include <resolv.h>
#ifdef HAVE_LBER_H
#include <lber.h>
#endif
#ifdef HAVE_LDAP_H
#include <ldap.h>
#endif
#if defined(HAVE_THREAD_H)
#include <thread.h>
#elif defined(HAVE_PTHREAD_H)
#include <pthread.h>
#endif
#ifdef INET6
#include <resolv/mapv4v6addr.h>
#endif

#include "ldap-nss.h"
#include "util.h"

#ifndef MAXALIASES
#define MAXALIASES 35
#endif

static struct ent_context *hosts_context = NULL;

static enum nss_status
_nss_ldap_parse_host (LDAPMessage * e,
                      struct ldap_state * pvt,
                      void *result, char *buffer, size_t buflen,
                      int af)
{
  /* this code needs reviewing. XXX */
  struct hostent *host = (struct hostent *) result;
  enum nss_status stat;
#ifdef INET6
  char addressbuf[sizeof ("ffff:ffff:ffff:ffff:ffff:ffff:255.255.255.255") *
                  MAXALIASES];
#else
  char addressbuf[sizeof ("255.255.255.255") * MAXALIASES];
#endif
  char *p_addressbuf = addressbuf;
  char **addresses = NULL;
  size_t addresslen = sizeof (addressbuf);
  size_t addresscount = 0;
  char **host_addresses = NULL;
  int i;

  *addressbuf = *buffer = '\0';

  stat = _nss_ldap_assign_attrval (e, ATM (LM_HOSTS, cn), &host->h_name,
                                   &buffer, &buflen);
  if (stat != NSS_STATUS_SUCCESS)
    return stat;

  stat =
    _nss_ldap_assign_attrvals (e, ATM (LM_HOSTS, cn), host->h_name,
                               &host->h_aliases, &buffer, &buflen, NULL);
  if (stat != NSS_STATUS_SUCCESS)
    return stat;

  stat =
    _nss_ldap_assign_attrvals (e, AT (ipHostNumber), NULL, &addresses,
                               &p_addressbuf, &addresslen, &addresscount);
  if (stat != NSS_STATUS_SUCCESS)
    return stat;
  if (addresscount == 0)
    return NSS_STATUS_NOTFOUND;

#ifdef INET6
  if (af == AF_INET6)
    {
      if (bytesleft (buffer, buflen, char *) <
          (size_t) ((addresscount + 1) * IN6ADDRSZ))
          return NSS_STATUS_TRYAGAIN;
    }
  else
    {
      if (bytesleft (buffer, buflen, char *) <
          (size_t) ((addresscount + 1) * INADDRSZ))
          return NSS_STATUS_TRYAGAIN;
    }
#else
  if (bytesleft (buffer, buflen, char *) <
      (size_t) ((addresscount + 1) * INADDRSZ))
      return NSS_STATUS_TRYAGAIN;
#endif

  align (buffer, buflen, char *);
  host_addresses = (char **) buffer;
  host->h_addr_list = host_addresses;
  host_addresses[addresscount] = NULL;

  buffer += (addresscount + 1) * sizeof (char *);
  buflen -= (addresscount + 1) * sizeof (char *);
#ifdef INET6
  host->h_addrtype = 0;
  host->h_length = 0;
#else
  host->h_addrtype = AF_INET;
  host->h_length = INADDRSZ;
#endif

  for (i = 0; i < (int) addresscount; i++)
    {
#ifdef INET6
      char *addr = addresses[i];
      char entdata[16];
      /* from glibc NIS parser. Thanks, Uli. */

      if (af == AF_INET && inet_pton (AF_INET, addr, entdata) > 0)
        {
          if (_res.options & RES_USE_INET6)
            {
              map_v4v6_address ((char *) entdata,
                                (char *) entdata);
              host->h_addrtype = AF_INET6;
              host->h_length = IN6ADDRSZ;
            }
          else
            {
              host->h_addrtype = AF_INET;
              host->h_length = INADDRSZ;
            }
        }
      else if (af == AF_INET6
               && inet_pton (AF_INET6, addr, entdata) > 0)
        {
          host->h_addrtype = AF_INET6;
          host->h_length = IN6ADDRSZ;
        }
      else
        /* Illegal address: ignore line.  */
        continue;

#else
      unsigned long haddr;
      haddr = inet_addr (addresses[i]);
#endif

      if (buflen < (size_t) host->h_length)
        return NSS_STATUS_TRYAGAIN;

#ifdef INET6
      memcpy (buffer, entdata, host->h_length);
      *host_addresses = buffer;
      buffer += host->h_length;
      buflen -= host->h_length;
#else
      memcpy (buffer, &haddr, INADDRSZ);
      *host_addresses = buffer;
      buffer += INADDRSZ;
      buflen -= INADDRSZ;
#endif

      host_addresses++;
      *host_addresses = NULL;
    }

#ifdef INET6
  /* if host->h_addrtype is not changed, this entry does not
     have the right IP address.  */
  if (host->h_addrtype == 0)
    return NSS_STATUS_NOTFOUND;
#endif

  return NSS_STATUS_SUCCESS;
}

static enum nss_status
_nss_ldap_parse_hostv4 (LDAPMessage * e,
                        struct ldap_state * pvt,
                        void *result, char *buffer, size_t buflen)
{
  return _nss_ldap_parse_host (e, pvt, result, buffer, buflen,
                               AF_INET);
}

#ifdef INET6
static enum nss_status
_nss_ldap_parse_hostv6 (LDAPMessage * e,
                        struct ldap_state * pvt,
                        void *result, char *buffer, size_t buflen)
{
  return _nss_ldap_parse_host (e, pvt, result, buffer, buflen,
                               AF_INET6);
}
#endif

enum nss_status _nss_ldap_gethostbyname2_r (const char *name, int af, struct hostent * result,
                            char *buffer, size_t buflen, int *errnop,
                            int *h_errnop)
{
  enum nss_status status;
  struct ldap_args a;

  LA_INIT (a);
  LA_STRING (a) = name;
  LA_TYPE (a) = LA_TYPE_STRING;

  status = _nss_ldap_getbyname (&a,
                                result,
                                buffer,
                                buflen,
                                errnop,
                                _nss_ldap_filt_gethostbyname,
                                LM_HOSTS,
#ifdef INET6
                                (af == AF_INET6) ?
                                _nss_ldap_parse_hostv6 :
#endif
                                _nss_ldap_parse_hostv4);

  MAP_H_ERRNO (status, *h_errnop);

  return status;
}

enum nss_status _nss_ldap_gethostbyname_r (const char *name, struct hostent * result,
                           char *buffer, size_t buflen, int *errnop,
                           int *h_errnop)
{
  return _nss_ldap_gethostbyname2_r (name,
#ifdef INET6
                                     (_res.options & RES_USE_INET6) ?
                                     AF_INET6 :
#endif
                                     AF_INET, result, buffer, buflen,
                                     errnop, h_errnop);
}

enum nss_status _nss_ldap_gethostbyaddr_r (struct in_addr * addr, int len, int type,
                           struct hostent * result, char *buffer,
                           size_t buflen, int *errnop, int *h_errnop)
{
  enum nss_status status;
  struct ldap_args a;

  /* if querying by IPv6 address, make sure the address is "normalized" --
   * it should contain no leading zeros and all components of the address.
   * still we can't fit an IPv6 address in an int, so who cares for now.
   */

  LA_INIT (a);
  LA_STRING (a) = inet_ntoa (*addr);
  LA_TYPE (a) = LA_TYPE_STRING;

  status = _nss_ldap_getbyname (&a,
                                result,
                                buffer,
                                buflen,
                                errnop,
                                _nss_ldap_filt_gethostbyaddr,
                                LM_HOSTS,
#ifdef INET6
                                (type == AF_INET6) ?
                                _nss_ldap_parse_hostv6 :
#endif
                                _nss_ldap_parse_hostv4);

  MAP_H_ERRNO (status, *h_errnop);

  return status;
}

enum nss_status _nss_ldap_sethostent (void)
{
  LOOKUP_SETENT (hosts_context);
}

enum nss_status _nss_ldap_endhostent (void)
{
  LOOKUP_ENDENT (hosts_context);
}

enum nss_status _nss_ldap_gethostent_r (struct hostent * result, char *buffer, size_t buflen,
                        int *errnop, int *h_errnop)
{
  enum nss_status status;

  status = _nss_ldap_getent (&hosts_context,
                             result,
                             buffer,
                             buflen,
                             errnop,
                             _nss_ldap_filt_gethostent, LM_HOSTS,
#ifdef INET6
                             (_res.options & RES_USE_INET6) ?
                             _nss_ldap_parse_hostv6 :
#endif
                             _nss_ldap_parse_hostv4);

  MAP_H_ERRNO (status, *h_errnop);

  return status;
}