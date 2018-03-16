/**
 * @version $Id$
 * @brief set the fields (config params) inside the vhs module server configuration structure
 * @author Xavier Beaudouin <kiwi (at) oav (dot) net>
 * @param parms structure of the invoked command - read Apache API Documentation
 * @param mconfig not used
 * @param arg the given configuration parameter form mod_vhs configuration options
 * @return NULL
 */

#ifndef set_field_H
#define set_field_H

static const char * set_field(cmd_parms * parms, void *mconfig, const char *arg)
{
	int		pos = (uintptr_t) parms->info;
	vhs_config_rec *vhr = (vhs_config_rec *) ap_get_module_config(parms->server->module_config, &vhs_module);

	switch (pos) {

#ifdef HAVE_MOD_DBD_SUPPORT
	case 0:
		vhr->query_label = apr_pstrdup(parms->pool, arg);
		
		//ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, parms->server, "set_field: Query Label='%s' arg='%s' for server: '%s' line: %d", vhr->query_label, arg, parms->server->defn_name, parms->server->defn_line_number);
		
		/* code repris de mod_authn_dbd pour preparer la connection et requete a la base. */
		if (vhost_dbd_prepare_fn == NULL) {
			vhost_dbd_prepare_fn = APR_RETRIEVE_OPTIONAL_FN(ap_dbd_prepare);
				if (vhost_dbd_prepare_fn == NULL) {
					return "You must load mod_dbd to enable VhostDBD functions";
			}
			vhost_dbd_acquire_fn = APR_RETRIEVE_OPTIONAL_FN(ap_dbd_acquire);
		}
		break;
#endif /* HAVE_MOD_DBD_SUPPORT */

	case 1:
		vhr->path_prefix = apr_pstrdup(parms->pool, arg);
		break;
	case 2:
		vhr->default_host = apr_pstrdup(parms->pool, arg);
		break;

#ifdef HAVE_MOD_PHP_SUPPORT
	case 3:
		vhr->openbdir_path = apr_pstrdup(parms->pool, arg);
		break;
#endif /* HAVE_MOD_PHP_SUPPORT */

#ifdef HAVE_LDAP_SUPPORT
	case 4:
		vhr->ldap_binddn = apr_pstrdup(parms->pool, arg);
		break;

	case 5:
		vhr->ldap_bindpw = apr_pstrdup(parms->pool, arg);
		break;

	case 6:
	    if (strcmp(arg, "never") == 0 || strcasecmp(arg, "off") == 0) {
	        vhr->ldap_deref = never;
	        vhr->ldap_have_deref = 1;
	    }
	    else if (strcmp(arg, "searching") == 0) {
	        vhr->ldap_deref = searching;
	        vhr->ldap_have_deref = 1;
	    }
	    else if (strcmp(arg, "finding") == 0) {
	        vhr->ldap_deref = finding;
	        vhr->ldap_have_deref = 1;
	    }
	    else if (strcmp(arg, "always") == 0 || strcasecmp(arg, "on") == 0) {
	        vhr->ldap_deref = always;
	        vhr->ldap_have_deref = 1;
	    }
	    else {
	        return "Unrecognized value for vhs_DAPAliasDereference directive";
	    }
		break;
#endif /* HAVE_LDAP_SUPPORT */

#ifdef HAVE_MOD_SUPHP_SUPPORT
	case 10:
		vhr->suphp_config_path = apr_pstrdup(parms->pool, arg);
		break;
#endif /* HAVE_MOD_SUPHP_SUPPRT */

#ifdef HAVE_MEMCACHE_SUPPORT
	case 11:
		vhr->memcache_instance = apr_pstrdup(parms->pool, arg);
		break;
	case 12:
		vhr->memcache_lifetime = apr_pstrdup(parms->pool, arg);
		break;
#endif /* HAVE_MEMCACHE_SUPPORT */
#ifdef HAVE_FSCACHE_SUPPORT
	case 13:
		vhr->fscache_dir = apr_pstrdup(parms->pool, arg);
		break;
	case 14:
		vhr->fscache_timeout = apr_time_from_sec(atoi(arg));
		break;
	case 15:
		vhr->fscache_expire = apr_time_from_sec(atoi(arg));
		break;
#endif /* HAVE_FSCACHE_SUPPORT */

	}
	return NULL;
}

#endif

