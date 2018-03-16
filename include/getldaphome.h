/**
 * @version $Id$
 * @brief get vHost configuration from LDAP
 * @author Xavier Beaudouin <kiwi (at) oav (dot) net>
 * @param r structure of the current client request
 * @param vhr vhs module server configuration structure
 * @param hostname the given hostname used as part of the memcache key to identify which value belongs to which hostname
 * @param reqc current request configuration structure
 * @return DECLINED or OK
 */

#ifndef getldaphome_H
#define getldaphome_H

int getldaphome(request_rec *r, vhs_config_rec *vhr, char *hostname, mod_vhs_request_t *reqc)
{
	/* LDAP associated variable and stuff */
	const char 		**vals = NULL;
	char 			filtbuf[FILTER_LENGTH];
    int 			result = 0;
    const char 		*dn = NULL;
    util_ldap_connection_t 	*ldc = NULL;
    int 			failures = 0;
	
  	ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, r->server, "getldaphome() called.");

	start_over:

	if (vhr->ldap_host) {
	ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, r->server, "util_ldap_connection_find(r,%s,%d,%s,%s,%d,%d);",vhr->ldap_host, vhr->ldap_port, vhr->ldap_binddn, vhr->ldap_bindpw, vhr->ldap_deref, vhr->ldap_secure);
    		ldc = util_ldap_connection_find(r, vhr->ldap_host, vhr->ldap_port, vhr->ldap_binddn,
						vhr->ldap_bindpw, vhr->ldap_deref, vhr->ldap_secure);
	ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, r->server, "util_ldap_connection_find();");
    	} else {
        	ap_log_rerror(APLOG_MARK, APLOG_WARNING|APLOG_NOERRNO, 0, r,
                      		"translate: no vhr->host - weird...?");
        	return DECLINED;
    	}

	ap_log_rerror(APLOG_MARK, APLOG_DEBUG|APLOG_NOERRNO, 0, r, "translating %s", r->uri);

	if (vhr->ldap_set_filter) {
    		apr_snprintf(filtbuf, FILTER_LENGTH, "%s=%s", vhr->ldap_filter, hostname);
	} else {
    		apr_snprintf(filtbuf, FILTER_LENGTH, "(&(%s)(|(apacheServerName=%s)(apacheServerAlias=%s)))", vhr->ldap_filter, hostname, hostname);
	}
	ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, r->server, "filtbuf = %s",filtbuf);
    	result = util_ldap_cache_getuserdn(r, ldc, vhr->ldap_url, vhr->ldap_basedn, vhr->ldap_scope, ldap_attributes, filtbuf, &dn, &vals);
    	util_ldap_connection_close(ldc);

    	/* sanity check - if server is down, retry it up to 5 times */
	if (result == LDAP_SERVER_DOWN) {
    		if (failures++ <= 5) {
    			goto start_over;
        	}
    	}

    if ((result == LDAP_NO_SUCH_OBJECT)) {
     	ap_log_rerror(APLOG_MARK, APLOG_WARNING|APLOG_NOERRNO, 0, r,
    		          "virtual host %s not found",
    		          hostname);
	return DECLINED;
    }

    /* handle bind failure */
    if (result != LDAP_SUCCESS) {
       ap_log_rerror(APLOG_MARK, APLOG_WARNING|APLOG_NOERRNO, 0, r,
                     "translate failed; virtual host %s; URI %s [%s]",
    		         hostname, r->uri, ldap_err2string(result));
	return DECLINED;
    }


  int i = 0;
  while (ldap_attributes[i]) {
	if (strcasecmp (ldap_attributes[i], "apacheServerName") == 0) {
	  reqc->name = apr_pstrdup (r->pool, vals[i]);
	}
	else if (strcasecmp (ldap_attributes[i], "apacheServerAdmin") == 0) {
	  reqc->admin = apr_pstrdup (r->pool, vals[i]);
	}
	else if (strcasecmp (ldap_attributes[i], "apacheDocumentRoot") == 0) {
	  reqc->docroot = apr_pstrdup (r->pool, vals[i]);
	}
	else if (strcasecmp (ldap_attributes[i], "homeDirectory") == 0) {
	  reqc->docroot = apr_pstrdup (r->pool, vals[i]);
	}
	else if (strcasecmp (ldap_attributes[i], "apachePhpopts") == 0) {
	  reqc->phpoptions = apr_pstrdup (r->pool, vals[i]);
	}
	else if (strcasecmp (ldap_attributes[i], "apacheSuexecUid") == 0) {
	  reqc->uid = apr_pstrdup(r->pool, vals[i]);
	}
	else if (strcasecmp (ldap_attributes[i], "apacheSuexecGid") == 0) {
	  reqc->gid = apr_pstrdup(r->pool, vals[i]);
	}
	else if (strcasecmp (ldap_attributes[i], "associatedDomain") == 0) {
	  reqc->associateddomain = apr_pstrdup(r->pool, vals[i]);
	}
	i++;
  }
    
  reqc->vhost_found = VH_VHOST_INFOS_FOUND;

  /* If we have a document root then we can honnor the resquest */
  if (reqc->docroot == NULL) {
	return DECLINED;
  }

  /* We have a document root so, this is ok */
  return OK;
}

#endif

