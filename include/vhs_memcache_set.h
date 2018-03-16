/**
 * @version $Id$
 * @brief sets a value by key on the memcached server
 * @author Rene Kanzler <rk (at) cosmomill (dot) de>
 * @param r structure of the current client request
 * @param vhr vhs module server configuration structure
 * @param hostname the given hostname used as part of the memcache key to identify which value belongs to which hostname
 * @param key a key descriptor used as part of the memcache key to identify which value belongs to which vHost parameter e.g. docroot, admin, server...
 * @param val the value 
 * @return always OK
 */
 
#ifndef vhs_memcache_set_H
#define vhs_memcache_set_H

int vhs_memcache_set(request_rec *r, vhs_config_rec *vhr, const char *hostname, char *key, char *val)
{
	apr_memcache_t *memctxt; //memcache context provided by mod_memcache ##
	char *memcache_key;
	char *memcache_val;
	apr_status_t rv = 0;
	int memcache_lifetime;

	memctxt = ap_memcache_client(r->server);
    if (memctxt == NULL) {
		vhr->memcache_enable = 0;
	}
	
	if (vhr->memcache_enable) {
		memcache_val = val;
		
		// create memcache key with md5 of hostname ##
		char md5hostname[100];
		apr_md5_encode(hostname, "", md5hostname, sizeof md5hostname);
		memcache_key = apr_pstrcat(r->pool, "vhs_", vhr->memcache_instance, "_", key, "_", md5hostname, NULL);
		
		if (vhr->memcache_lifetime == NULL) {
			memcache_lifetime = DEFAULT_MEMCACHED_LIFETIME;
		} else {
			memcache_lifetime = atoi(vhr->memcache_lifetime);
		}

		//ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, r->server, "vhs_memcache_set: memcache_lifetime: '%d'", memcache_lifetime);
		ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, r->server, "vhs_memcache_set: memcache insert: '%s' -> '%s'", memcache_key, memcache_val);

		rv = apr_memcache_set(memctxt, memcache_key, memcache_val, strlen(memcache_val), memcache_lifetime, 0);
		if (rv != APR_SUCCESS) {
			ap_log_error(APLOG_MARK, APLOG_ERR, 0, r->server, "vhs_memcache_set: memcache error setting key '%s' with %d bytes of data", memcache_key, (int) strlen(memcache_val));
		}
	}

	return OK;
}
#endif

