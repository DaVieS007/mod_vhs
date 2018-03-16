/**
 * @version $Id$
 * @brief store values from a previous DBD/LDAP query to the memcached server
 * @author Rene Kanzler <rk (at) cosmomill (dot) de>
 * @param r structure of the current client request
 * @param vhr vhs module server configuration structure
 * @param hostname the given hostname used as part of the memcache key to identify which value belongs to which hostname
 * @param reqc current request configuration structure
 * @return always OK
 */

#ifndef vhs_memcache_prepare_H
#define vhs_memcache_prepare_H

int vhs_memcache_prepare(request_rec *r, vhs_config_rec *vhr, const char *hostname, mod_vhs_request_t *reqc)
{
	if (vhr->memcache_enable) {
		
		ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, r->server, "vhs_memcache_prepare ********************************************");
    	
		// Set memcache key and value ##
		/* servername */
		if (reqc->name) {
			vhs_memcache_set(r, vhr, hostname, "name", reqc->name);
		}
		/* email admin server */
		if (reqc->admin) {
			vhs_memcache_set(r, vhr, hostname, "admin", reqc->admin);
		}
		/* document root */
		if (reqc->docroot) {
			vhs_memcache_set(r, vhr, hostname, "docroot", reqc->docroot);
		}
		/* suexec UID */
		if (reqc->uid) {
			vhs_memcache_set(r, vhr, hostname, "uid", reqc->uid);
		}
		/* suexec GID */
		if (reqc->gid) {
			vhs_memcache_set(r, vhr, hostname, "gid", reqc->gid);
		}
		/* phpopt_fromdb / options PHP */
		if (reqc->phpoptions) {
			vhs_memcache_set(r, vhr, hostname, "phpoptions", reqc->phpoptions);
		}
		/* associate domain */
		if (reqc->associateddomain) {
			vhs_memcache_set(r, vhr, hostname, "associateddomain", reqc->associateddomain);
		}
	}

	return OK;
}
#endif

