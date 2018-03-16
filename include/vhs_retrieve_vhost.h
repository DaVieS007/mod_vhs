/**
 * @version $Id$
 * @brief fetch vHost data either from DBD, LDAP or memcached and store fetched data to memcached
 * @author Rene Kanzler <rk (at) cosmomill (dot) de>
 * @param r structure of the current client request
 * @param vhr vhs module server configuration structure
 * @param hostname the given hostname used as part of the memcache key to identify which value belongs to which hostname
 * @param reqc current request configuration structure
 * @return always OK
 */
 
#ifndef vhs_retrieve_vhost_H
#define vhs_retrieve_vhost_H

int vhs_retrieve_vhost(request_rec *r, vhs_config_rec *vhr, const char *hostname, mod_vhs_request_t *reqc)
{
	int vhost_found_by_request = DECLINED;


#ifdef HAVE_MEMCACHE_SUPPORT
		#include "vhs_memcache_get.h"
		vhost_found_by_request = vhs_memcache_get(r, vhr, (char *) hostname, reqc);
		if (vhost_found_by_request == OK) {
			return vhost_found_by_request;
		} else if (vhost_found_by_request == DECLINED && reqc->vhost_found == VH_VHOST_INFOS_NOT_FOUND) {
			return vhost_found_by_request;
		} else {
#endif /* HAVE_MEMCACHE_SUPPORT */	

			#ifdef HAVE_LDAP_SUPPORT
				vhost_found_by_request = getldaphome(r, vhr, (char *) hostname, reqc);
			#endif

			#ifdef HAVE_MOD_DBD_SUPPORT
				vhost_found_by_request = getmoddbdhome(r, vhr, (char *) hostname, reqc);
			#endif

#ifdef HAVE_MEMCACHE_SUPPORT
			#include "vhs_memcache_set.h"
			#include "vhs_memcache_prepare.h"
			
			if (vhost_found_by_request == OK) {
				vhs_memcache_prepare(r, vhr, hostname, reqc);
				return vhost_found_by_request;
			} else {
				// store all requests in memcache so we must not hammer database/LDAP for useless requests ## 
				ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, r->server, "vhs_retrieve_vhost: store unknown server_name: '%s' in memcache", hostname);
				vhs_memcache_set(r, vhr, hostname, "name", "NOT_IN_DATABASE");
				return vhost_found_by_request;
			}
#endif /* HAVE_MEMCACHE_SUPPORT */

			return vhost_found_by_request;

#ifdef HAVE_MEMCACHE_SUPPORT
		}
#endif /* HAVE_MEMCACHE_SUPPORT */		
}

#endif

