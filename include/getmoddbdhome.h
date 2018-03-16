/**
 * @version $Id$
 * @brief get vHost configuration from mod_dbd
 * @author Xavier Beaudouin <kiwi (at) oav (dot) net>
 * @param r structure of the current client request
 * @param vhr vhs module server configuration structure
 * @param hostname the given hostname used as part of the memcache key to identify which value belongs to which hostname
 * @param reqc current request configuration structure
 * @return DECLINED or OK
 */

#ifndef getmoddbdhome_H
#define getmoddbdhome_H

#ifdef HAVE_FSCACHE_SUPPORT
#include "vhs_fscache.h"
#endif

int getmoddbdhome(request_rec *r, vhs_config_rec *vhr, const char *hostname, mod_vhs_request_t *reqc)
{
	const char *host = 0;
#ifdef HAVE_FSCACHE_SUPPORT
	char from_cache = 0;
#endif
	char lookup_err = 0;
	char got_result = 0;

	if (!vhr->enable) {
		return DECLINED;
	}

#ifdef HAVE_FSCACHE_SUPPORT
	/** QUERY CACHE **/
	if(vhr->fscache_enable) {
		from_cache = get_cache(r, vhr, hostname, reqc);
		if(from_cache == FS_CACHE_FOUND)
		{
			//ap_log_rerror(APLOG_MARK, APLOG_NOTICE, 0, r, "[VHS::RESOLV]: %s (Cached)",hostname);
			apr_pool_userdata_set(reqc, VH_KEY, apr_pool_cleanup_null, r->pool);
			return OK;
		}
		else if(from_cache == FS_CACHE_EXPIRED)
		{
			//ap_log_rerror(APLOG_MARK, APLOG_NOTICE, 0, r, "[VHS::RESOLV]: %s (Cached-Expired)",hostname);
		}
		else if(from_cache == FS_CACHE_NOT_FOUND)
		{
			/* the vhost is in the cache but not in the database */
			reqc->vhost_found = VH_VHOST_INFOS_NOT_FOUND;
			//ap_log_rerror(APLOG_MARK, APLOG_NOTICE, 0, r, "[VHS::RESOLV]: %s (Cached-NonExists)",hostname);
			return DECLINED;
		}
		else
		{
			//ap_log_rerror(APLOG_MARK, APLOG_NOTICE, 0, r, "[VHS::RESOLV]: %s (Non-Cached)",hostname);
		}
	}
	/** QUERY CACHE **/
#endif

	apr_status_t rv = 0;
	ap_dbd_t *dbd;
	apr_dbd_prepared_t *statement;
	apr_dbd_results_t *res = NULL;
	apr_dbd_row_t *row = NULL;

	ap_log_rerror(APLOG_MARK, APLOG_DEBUG, 0, r, "getmoddbdhome --------------------------------------------");

	if (vhr->query_label == NULL) {
		ap_log_rerror(APLOG_MARK, APLOG_NOTICE, 0, r, "getmoddbdhome: No VhostDBDQuery has been specified");
		lookup_err = 1;
	}

	if(lookup_err == 0)	
	{
		host = hostname; 
		ap_log_rerror(APLOG_MARK, APLOG_DEBUG, 0, r, "getmoddbdhome: search for vhost: '%s'", host);
		
		dbd = vhost_dbd_acquire_fn(r);
		if (dbd == NULL) {
			ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, r, "getmoddbdhome: Failed to acquire database connection to look up host '%s'", host);
			lookup_err = 1;
		}		
	}
	
	if(lookup_err == 0)	
	{
		statement = apr_hash_get(dbd->prepared, vhr->query_label, APR_HASH_KEY_STRING);
		if (statement == NULL) {
			ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, r, "getmoddbdhome: A prepared statement could not be found for ""VhostDBDQuery with the label '%s'", vhr->query_label);
			lookup_err = 1;
		}
	}
	
	if(lookup_err == 0)
	{
		ap_log_rerror(APLOG_MARK, APLOG_DEBUG, 0, r, "getmoddbdhome: execute query with label='%s'", vhr->query_label);
	
		/* execute the query of a statement and parameter host */
		if ((rv = apr_dbd_pvselect(dbd->driver, r->pool, dbd->handle, &res, statement, 0, host, NULL))) {
			ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, r, "getmoddbdhome: Unable to execute SQL statement: %s", apr_dbd_error(dbd->driver, dbd->handle, rv));
			lookup_err = 1;
		}
	}
	
	if(lookup_err == 0)
	{
		ap_log_rerror(APLOG_MARK, APLOG_DEBUG, 0, r, "getmoddbdhome: apr_dbd_get_row return : %d", rv);
		
		if ((rv = apr_dbd_get_row(dbd->driver, r->pool, res, &row, -1))) {
			ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, r, "No found results for host '%s' in database", host);
			got_result = 0;
		}
		else
		{
			got_result = 1;
		}
	}

	if(lookup_err == 0)	
	{
		/* requete dbd ok */
		ap_log_rerror(APLOG_MARK, APLOG_DEBUG, 0, r, "getmoddbdhome: dbd is ok");
		
		/* "SELECT ServerName, ServerAdmin, DocumentRoot, suexec_uid, suexec_gid, php_env, associateddomain, isalias FROM ", vhr->dbd_table_name, " WHERE ServerName = %s AND active = 'yes'" */
		/* servername */
		if(got_result == 1)
		{
			/* the VHost is in the database */
			reqc->name = apr_pstrdup(r->pool, apr_dbd_get_entry(dbd->driver, row, 0));
			/* enforcing lowercase to avoid cache miss and other potential issues if the ServerName column contains uppercase chars */
			ap_str_tolower(reqc->name);

			ap_log_rerror(APLOG_MARK, APLOG_DEBUG, 0, r, "getmoddbdhome: server_name='%s'", reqc->name);
			
			/* email admin server */
			reqc->admin = apr_pstrdup(r->pool, apr_dbd_get_entry(dbd->driver, row, 1));
			ap_log_rerror(APLOG_MARK, APLOG_DEBUG, 0, r, "getmoddbdhome: server_admin='%s'", reqc->admin);
			
			/* document root */
			reqc->docroot = apr_pstrdup(r->pool, apr_dbd_get_entry(dbd->driver, row, 2));
			ap_log_rerror(APLOG_MARK, APLOG_DEBUG, 0, r, "getmoddbdhome: docroot=%s", reqc->docroot);
			
			/* suexec UID */
			reqc->uid = apr_pstrdup(r->pool, apr_dbd_get_entry(dbd->driver, row, 3));
			ap_log_rerror(APLOG_MARK, APLOG_DEBUG, 0, r, "getmoddbdhome: uid=%s", reqc->uid);
			
			/* suexec GID */
			reqc->gid = apr_pstrdup(r->pool, apr_dbd_get_entry(dbd->driver, row, 4));
			ap_log_rerror(APLOG_MARK, APLOG_DEBUG, 0, r, "getmoddbdhome: gid=%s", reqc->gid);
			
			/* phpopt_fromdb / options PHP */
			reqc->phpoptions = apr_pstrdup(r->pool, apr_dbd_get_entry(dbd->driver, row, 5));
			ap_log_rerror(APLOG_MARK, APLOG_DEBUG, 0, r, "getmoddbdhome: phpoptions=%s", reqc->phpoptions);
			
			/* associate domain */
			reqc->associateddomain = apr_pstrdup(r->pool, apr_dbd_get_entry(dbd->driver, row, 6));
			ap_log_rerror(APLOG_MARK, APLOG_DEBUG, 0, r, "getmoddbdhome: associateddomain=%s", reqc->associateddomain);
			
			/* the vhost has been found, set vhost_found to VH_VHOST_INFOS_FOUND */
			reqc->vhost_found = VH_VHOST_INFOS_FOUND;

			apr_pool_userdata_set(reqc, VH_KEY, apr_pool_cleanup_null, r->pool);
#ifdef HAVE_FSCACHE_SUPPORT
			if(vhr->fscache_enable) {
				set_cache(r, vhr, hostname, reqc);
			}
#endif
			return OK;			
		}
		else
		{
			/* the VHost is not in the database */
#ifdef HAVE_FSCACHE_SUPPORT
			if(vhr->fscache_enable) {
				set_cache(r, vhr, hostname, NULL);
			}
#endif
			return DECLINED;
		}
	}
#ifdef HAVE_FSCACHE_SUPPORT
	else if(vhr->fscache_enable && from_cache != FS_CACHE_NO_CACHE)
	{
		ap_log_rerror(APLOG_MARK, APLOG_NOTICE, 0, r, "[VHS::FSCACHE] There was no luck with lookup, but we have a cache: %s",hostname);
		apr_pool_userdata_set(reqc, VH_KEY, apr_pool_cleanup_null, r->pool);
		return OK;
	}
	else if(vhr->fscache_enable)
	{
		ap_log_rerror(APLOG_MARK, APLOG_NOTICE, 0, r, "[VHS::FSCACHE] There was no luck with lookup: %s",hostname);
		return DECLINED;
	}
#endif

	return DECLINED;
}

#endif
