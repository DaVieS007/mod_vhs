/**
 * @version $Id$
 * @brief gat a value by key from the memcached server
 * @author Rene Kanzler <rk (at) cosmomill (dot) de>
 * @param r structure of the current client request
 * @param vhr vhs module server configuration structure
 * @param hostname the given hostname used as part of the memcache key to identify which value belongs to which hostname
 * @param reqc current request configuration structure
 * @return DECLINED or OK
 */
 
#ifndef vhs_memcache_get_H
#define vhs_memcache_get_H

int vhs_memcache_get(request_rec *r, vhs_config_rec *vhr, const char *hostname, mod_vhs_request_t *reqc)
{
	apr_memcache_t *memctxt; //memcache context provided by mod_memcache ##
	char *memcache_key;
	char *memcache_val;
	apr_status_t rv = 0;
	apr_size_t len;
	
	ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, r->server, "vhs_memcache_get ********************************************");
                                                                              
	memctxt = ap_memcache_client(r->server);
    if (memctxt == NULL) {
		vhr->memcache_enable = 0;
	}
	
	if (vhr->memcache_enable) {
		// create name key for memcache request with md5 of hostname ##
		char md5hostname[100];
		apr_md5_encode(hostname, "", md5hostname, sizeof md5hostname);
		memcache_key = apr_pstrcat(r->pool, "vhs_", vhr->memcache_instance, "_name_", md5hostname, NULL);
		
		rv = apr_memcache_getp(memctxt, r->pool, memcache_key, &memcache_val, &len, NULL);

		if (rv != APR_SUCCESS) {
			ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, r->server, "vhs_memcache_get: server_name: '%s' unknown in memcache", hostname);
			return DECLINED;
		} 
		
		if (apr_strnatcmp(memcache_val, "NOT_IN_DATABASE") == OK) {
			reqc->vhost_found = VH_VHOST_INFOS_NOT_FOUND;
			ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, r->server, "vhs_memcache_get: Memcache Hit ---> server_name: '%s' in memcache but not in database", hostname);
			return DECLINED;
		} else {
			/* servername */
			reqc->name = memcache_val;
			ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, r->server, "vhs_memcache_get: Memcache Hit ---> server_name: '%s' known in memcache", memcache_val);
		
			/* email admin server */
			memcache_key = apr_pstrcat(r->pool, "vhs_", vhr->memcache_instance, "_admin_", md5hostname, NULL);
			rv = apr_memcache_getp(memctxt, r->pool, memcache_key, &memcache_val, &len, NULL);
			if (rv == APR_SUCCESS) {
				reqc->admin = memcache_val;
				ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, r->server, "vhs_memcache_get: server_admin: '%s' known in memcache", memcache_val);
			} else {
				ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, r->server, "vhs_memcache_get: server_admin unknown in memcache");
			}
			
			/* document root */
			memcache_key = apr_pstrcat(r->pool, "vhs_", vhr->memcache_instance, "_docroot_", md5hostname, NULL);
			rv = apr_memcache_getp(memctxt, r->pool, memcache_key, &memcache_val, &len, NULL);
			if (rv == APR_SUCCESS) {
				reqc->docroot = memcache_val;
				ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, r->server, "vhs_memcache_get: docroot: '%s' known in memcache", memcache_val);
			} else {
				ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, r->server, "vhs_memcache_get: docroot unknown in memcache");
			}
			
			/* suexec UID */
			memcache_key = apr_pstrcat(r->pool, "vhs_", vhr->memcache_instance, "_uid_", md5hostname, NULL);
			rv = apr_memcache_getp(memctxt, r->pool, memcache_key, &memcache_val, &len, NULL);
			if (rv == APR_SUCCESS) {
				reqc->uid = memcache_val;
				ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, r->server, "vhs_memcache_get: uid: '%s' known in memcache", memcache_val);
			} else {
				ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, r->server, "vhs_memcache_get: uid unknown in memcache");
			}
			
			/* suexec GID */
			memcache_key = apr_pstrcat(r->pool, "vhs_", vhr->memcache_instance, "_gid_", md5hostname, NULL);
			rv = apr_memcache_getp(memctxt, r->pool, memcache_key, &memcache_val, &len, NULL);
			if (rv == APR_SUCCESS) {
				reqc->gid = memcache_val;
				ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, r->server, "vhs_memcache_get: gid: '%s' known in memcache", memcache_val);
			} else {
				ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, r->server, "vhs_memcache_get: gid unknown in memcache");
			}
			
			/* phpopt_fromdb / options PHP */
			memcache_key = apr_pstrcat(r->pool, "vhs_", vhr->memcache_instance, "_phpoptions_", md5hostname, NULL);
			rv = apr_memcache_getp(memctxt, r->pool, memcache_key, &memcache_val, &len, NULL);
			if (rv == APR_SUCCESS) {
				reqc->phpoptions = memcache_val;
				ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, r->server, "vhs_memcache_get: phpoptions: '%s' known in memcache", memcache_val);
			} else {
				ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, r->server, "vhs_memcache_get: phpoptions unknown in memcache");
			}
			
			/* associate domain */
			memcache_key = apr_pstrcat(r->pool, "vhs_", vhr->memcache_instance, "_associateddomain_", md5hostname, NULL);
			rv = apr_memcache_getp(memctxt, r->pool, memcache_key, &memcache_val, &len, NULL);
			if (rv == APR_SUCCESS) {
				reqc->associateddomain = memcache_val;
				ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, r->server, "vhs_memcache_get: associateddomain: '%s' known in memcache", memcache_val);
			} else {
				ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, r->server, "vhs_memcache_get: associateddomain unknown in memcache");
			}
			
			/* the vhost has been found, set vhost_found to VH_VHOST_INFOS_FOUND */
			reqc->vhost_found = VH_VHOST_INFOS_FOUND;
	
			apr_pool_userdata_set(reqc, VH_KEY, apr_pool_cleanup_null, r->pool);
		
			return OK;
		}
	}

	return DECLINED;
}
#endif

