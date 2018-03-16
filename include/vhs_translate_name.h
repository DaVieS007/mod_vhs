/**
 * @version $Id$
 * @brief This function translate the URI into a filename.
 * Send the right path to the end user uppon a request.
 * @remark This is an "ap_hook_translate_name" - read Apache API Documentation
 * @author Xavier Beaudouin <kiwi (at) oav (dot) net>
 * @author Rene Kanzler <rk (at) cosmomill (dot) de>
 * @param r structure of the current client request
 * @return DECLINED, OK or call "vhs_redirect_stuff" function
 */
 
#ifndef vhs_translate_name_H
#define vhs_translate_name_H

static int vhs_translate_name(request_rec * r)
{
	vhs_config_rec     	*vhr  = (vhs_config_rec *)     ap_get_module_config(r->server->module_config, &vhs_module);
	core_server_config 	*conf = (core_server_config *) ap_get_module_config(r->server->module_config, &core_module);

	const char     		*host = 0;
	/* mod_alias like functions */
	char           		*ret = 0;
	int			   		status = 0;

	/* Stuff */
	char           *ptr = 0;
	int vhost_found_by_request = DECLINED;

	ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, r->server, "vhs_translate_name: BEGIN ***");

	/* If VHS is not enabled, then don't process request */
	if (!vhr->enable) {
		ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, r->server, "vhs_translate_name: VHS Disabled ");
		return DECLINED;
	}

	mod_vhs_request_t *reqc = (mod_vhs_request_t *) ap_get_module_config(r->request_config, &vhs_module);	
	if (!reqc) {
		ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, r->server, "vhs_translate_name: variable reqc does not already exists.... creating !");
		reqc = (mod_vhs_request_t *)apr_pcalloc(r->pool, sizeof(mod_vhs_request_t));
		reqc->vhost_found = VH_VHOST_INFOS_NOT_YET_REQUESTED;
		ap_set_module_config(r->request_config, &vhs_module, reqc);
	}
	
	vhs_connection_conf *conn_conf = (vhs_connection_conf *) ap_get_module_config(r->connection->conn_config, &vhs_module);
	if (!conn_conf) {
		ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, r->server, "vhs_translate_name: variable conn_conf does not already exists.... creating !");
		conn_conf = (vhs_connection_conf *) apr_pcalloc(r->connection->pool, sizeof(vhs_connection_conf));
		ap_set_module_config(r->connection->conn_config, &vhs_module, conn_conf);
	}
		
	// Force the re-fetch of informations on the database if the previous query on the same connection was for a different virtualhost ##
	if(conn_conf->name && strcmp(conn_conf->name, r->hostname) != 0) {
		// The connection was previously used for a different virtualhost ##
		ap_log_rerror(APLOG_MARK, APLOG_DEBUG, 0, r, "vhs_translate_name: Reusing connection for a different virtualhost (was %s, now %s)", conn_conf->name, r->hostname);
		conn_conf->docroot = NULL;
		conn_conf->name = NULL;
		conn_conf->vhost_found = VH_VHOST_INFOS_NOT_YET_REQUESTED;
	}

	if(!r->hostname) {
		// No "Host" header has been passed (eg. HTTPS without SNI), no need to check the database/cache ##
		ap_log_rerror(APLOG_MARK, APLOG_DEBUG, 0, r, "vhs_translate_name: Empty 'Host:' header");
		conn_conf->vhost_found = VH_VHOST_INFOS_NOT_FOUND;
	}

#ifdef HAVE_LDAP_SUPPORT
	/* If we don't have LDAP Url module is disabled */
	if (!vhr->ldap_have_url) {
		ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, r->server, "vhs_translate_name: VHS Disabled - No LDAP URL ");
		return DECLINED;
	}
	ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, r->server, "vhs_translate_name: VHS Enabled (LDAP).");
#endif /* HAVE_LDAP_SUPPORT */

#ifdef HAVE_MOD_DBD_SUPPORT
	ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, r->server, "vhs_translate_name: VHS Enabled (Mod DBD).");
#endif /* HAVE_MOD_DBD_SUPPORT */

	/* Handle alias stuff */
	if ((ret = try_alias_list(r, vhr->redirects, 1, &status)) != NULL) {
		if (ap_is_HTTP_REDIRECT(status)) {
			/* include QUERY_STRING if any */
			if (r->args) {
				ret = apr_pstrcat(r->pool, ret, "?", r->args, NULL);
			}
			apr_table_setn(r->headers_out, "Location", ret);
		}
		return status;
	}
	if ((ret = try_alias_list(r, vhr->aliases, 0, &status)) != NULL) {
		/* The URI is matching a vhs_Alias or vhs_ScriptAlias directive */
		r->filename = ret;
		if (reqc->admin) {
			r->server->server_admin = apr_pstrcat(r->connection->pool, reqc->admin, NULL);
		} else {
			r->server->server_admin = apr_pstrcat(r->connection->pool, "webmaster@", r->hostname, NULL);
		}
		return OK;
	}
	/* Avoid handling request that don't start with '/' with an exception for "OPTIONS *" */
	if ( (r->method_number != M_OPTIONS && r->uri[0] != '/' && r->uri[0] != '\0' ) || (r->method_number == M_OPTIONS && r->uri[0] != '*' && r->uri[0] != '/' && r->uri[0] != '\0') ) {
		ap_log_error(APLOG_MARK, APLOG_ALERT, 0, r->server, "vhs_translate_name: declined URI %s on hostname %s using method %s : No leading `/'", r->uri, r->hostname, r->method);
		return DECLINED;
	}
	if (!(host = apr_table_get(r->headers_in, "Host"))) {
		conn_conf->vhost_found = VH_VHOST_INFOS_NOT_FOUND;
		if(!conn_conf->name) {
			// Used to verify if queries on the same concurrent connection are for the same virtualhost ##
			conn_conf->name = r->hostname ? apr_pstrdup(r->connection->pool, r->hostname) : NULL;
		}
		return vhs_redirect_stuff(r, vhr);
	}
	if ((ptr = ap_strchr(host, ':'))) {
		*ptr = '\0';
	}

	// Query the database only if the informations for the virtualhost hasnt been already fetched previously on this connection ##
	if (reqc->vhost_found == VH_VHOST_INFOS_NOT_YET_REQUESTED && conn_conf->vhost_found != VH_VHOST_INFOS_NOT_FOUND)
	  {
		ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, r->server, "vhs_translate_name: looking for %s", host);
		
		/* Can we re-use a previous result from our conn_conf? 
		*
		* Note that if a conn_conf->docroot exists we are within the same connection,
		* so this request is guaranteed to be to the same IP address. 
		*/
		if (conn_conf->docroot) {
			// YES - we do not need to execute a query. Use the docroot we saved in conn_conf ##
			vhost_found_by_request = OK;
			reqc->vhost_found = VH_VHOST_INFOS_FOUND;
			ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, r->server, "vhs_translate_name: Using previous connection query, setting docroot to: '%s'", conn_conf->docroot);
			// Set connection values to request values ##
			/* servername */
			reqc->name = conn_conf->name;
			/* email admin server */
			reqc->admin = conn_conf->admin;
			/* document root */
			reqc->docroot = conn_conf->docroot;
			/* suexec UID */
			reqc->uid = conn_conf->uid;
			/* suexec GID */
			reqc->gid = conn_conf->gid;
			/* phpopt_fromdb / options PHP */
			reqc->phpoptions = conn_conf->phpoptions;
			/* associate domain */
			reqc->associateddomain = conn_conf->associateddomain;
		} else {
			// No - we do need to execute a query ##
			vhost_found_by_request = DECLINED;
			vhost_found_by_request = vhs_retrieve_vhost(r, vhr, (char *) host, reqc); // Get the stuff from Mod DBD or LDAP ##
		}
		
		if (vhost_found_by_request != OK) { 
		/*
		 * The vhost has not been found
		 * Trying to get lamer mode or not
		 */
		if (vhr->lamer_mode) {
			ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, r->server, "vhs_translate_name: Lamer friendly mode engaged");
			if ((strncasecmp(host, "www.", 4) == 0) && (strlen(host) > 4)) {
				char           *lhost;
				lhost = apr_pstrdup(r->pool, host + 5 - 1);
				ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, r->server, "vhs_translate_name: Found a lamer for %s -> %s", host, lhost);

				// Trying to get vhost information ##
				vhost_found_by_request = vhs_retrieve_vhost(r, vhr, lhost, reqc); // Get the stuff from Mod DBD or LDAP ##
		
				if (vhost_found_by_request != OK) {
					if (vhr->log_notfound) {
						ap_log_error(APLOG_MARK, APLOG_NOTICE, 0, r->server, "vhs_translate_name: no host found in database for %s (lamer %s)", host, lhost);
					}
					conn_conf->vhost_found = VH_VHOST_INFOS_NOT_FOUND;
					if(!conn_conf->name) {
						// Used to verify if queries on the same concurrent connection are for the same virtualhost ##
						conn_conf->name = r->hostname ? apr_pstrdup(r->connection->pool, r->hostname) : NULL;
					}
					return vhs_redirect_stuff(r, vhr);
				}
			}
		} else {
			if (vhr->log_notfound) {
			  ap_log_error(APLOG_MARK, APLOG_NOTICE, 0, r->server, "vhs_translate_name: no host found in database for %s (lamer mode not enabled)", host);
			}
			conn_conf->vhost_found = VH_VHOST_INFOS_NOT_FOUND;
			if(!conn_conf->name) {
				// Used to verify if queries on the same concurrent connection are for the same virtualhost ##
				conn_conf->name = r->hostname ? apr_pstrdup(r->connection->pool, r->hostname) : NULL;
			}
			return vhs_redirect_stuff(r, vhr);
		}
	}
	  }
	else 
	  {
		ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, r->server, "vhs_translate_name: Request to the backend has already been done (vhs_itk_post_read()) !");
		if (reqc->vhost_found == VH_VHOST_INFOS_NOT_FOUND || conn_conf->vhost_found == VH_VHOST_INFOS_NOT_FOUND)
		  vhost_found_by_request = DECLINED; /* the request has already be done and vhost was not found */
		else
		  vhost_found_by_request = OK; /* the request has already be done and vhost was found */
	  }

	if (vhost_found_by_request == OK)
	  ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, r->server, "vhs_translate_name: path found in database for %s is %s", host, reqc->docroot);
	else
	  {
		if (vhr->log_notfound) {
			ap_log_error(APLOG_MARK, APLOG_NOTICE, 0, r->server, "vhs_translate_name: no path found in database for %s (normal)", host);
		}
		conn_conf->vhost_found = VH_VHOST_INFOS_NOT_FOUND;
		if(!conn_conf->name) {
			// Used to verify if queries on the same concurrent connection are for the same virtualhost ##
			conn_conf->name = r->hostname ? apr_pstrdup(r->connection->pool, r->hostname) : NULL;
		}
		return vhs_redirect_stuff(r, vhr);
	}

#ifdef WANT_VH_HOST
	apr_table_set(r->subprocess_env, "VH_HOST", host);
#endif /* WANT_VH_HOST */

	apr_table_set(r->subprocess_env, "VH_GECOS", reqc->associateddomain ? reqc->associateddomain : "");
	/* Do we have handle vhr_Path_Prefix here ? */
	if (vhr->path_prefix) {
		apr_table_set(r->subprocess_env, "VH_PATH", apr_pstrcat(r->pool, vhr->path_prefix, reqc->docroot, NULL));
		apr_table_set(r->subprocess_env, "SERVER_ROOT", apr_pstrcat(r->pool, vhr->path_prefix, reqc->docroot, NULL));
	} else {
		apr_table_set(r->subprocess_env, "VH_PATH", reqc->docroot);
		apr_table_set(r->subprocess_env, "SERVER_ROOT", reqc->docroot);
	}

	if (reqc->admin) {
		r->server->server_admin = apr_pstrcat(r->connection->pool, reqc->admin, NULL);
	} else {
		r->server->server_admin = apr_pstrcat(r->connection->pool, "webmaster@", r->hostname, NULL);
	}
	r->server->server_hostname = apr_pstrcat(r->connection->pool, host, NULL);
	r->parsed_uri.path = apr_pstrcat(r->pool, vhr->path_prefix ? vhr->path_prefix : "", reqc->docroot, r->parsed_uri.path, NULL);
	r->parsed_uri.hostname = r->server->server_hostname;
	r->parsed_uri.hostinfo = r->server->server_hostname;

	/* document_root */
	if (vhr->path_prefix) {
		conf->ap_document_root = apr_pstrcat(r->pool, vhr->path_prefix, reqc->docroot, NULL);
	} else {
		conf->ap_document_root = apr_pstrcat(r->pool, reqc->docroot, NULL);
	}

	/* if directory exist */
	if (!ap_is_directory(r->pool, reqc->docroot)) {
		ap_log_error(APLOG_MARK, APLOG_ALERT, 0, r->server,
		"vhs_translate_name: homedir '%s' is not dir at all", reqc->docroot);
		return DECLINED;
	}
	r->filename = apr_psprintf(r->pool, "%s%s%s", vhr->path_prefix ? vhr->path_prefix : "", reqc->docroot, r->uri);

	/* Avoid getting two // in filename */
	ap_no2slash(r->filename);

	ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, r->server, "vhs_translate_name: translated http://%s%s to file %s", host, r->uri, r->filename);

#ifdef HAVE_MOD_PHP_SUPPORT
	vhs_php_config(r, vhr, reqc->docroot, reqc->phpoptions);
#endif /* HAVE_MOD_PHP_SUPPORT */

#ifdef HAVE_MOD_SUPHP_SUPPORT
	vhs_suphp_config(r, vhr, reqc->docroot, reqc->uid, reqc->gid);
#endif /* HAVE_MOD_SUPHP_SUPPORT */

	// set values from database request to connection configuration ##
	/* servername */
	conn_conf->name = reqc->name ? apr_pstrdup(r->connection->pool, reqc->name) : NULL;
	/* email admin server */
	conn_conf->admin = reqc->admin ? apr_pstrdup(r->connection->pool, reqc->admin) : NULL;
	/* document root */
	conn_conf->docroot = reqc->docroot ? apr_pstrdup(r->connection->pool, reqc->docroot) : NULL;
	/* suexec UID */
	conn_conf->uid = reqc->uid ? apr_pstrdup(r->connection->pool, reqc->uid) : NULL;
	/* suexec GID */
	conn_conf->gid = reqc->gid ? apr_pstrdup(r->connection->pool, reqc->gid) : NULL;
	/* phpopt_fromdb / options PHP */
	conn_conf->phpoptions = reqc->phpoptions ? apr_pstrdup(r->connection->pool, reqc->phpoptions) : NULL;
	/* associate domain */
	conn_conf->associateddomain = reqc->associateddomain ? apr_pstrdup(r->connection->pool, reqc->associateddomain) : NULL;
	// used to avoid to re-check the virtualhost in the next queries if it was not found at first ##
	conn_conf->vhost_found = reqc->vhost_found ? reqc->vhost_found : 0;
	
	ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, r->server, "vhs_translate_name: END ***");
	return OK;
}

#endif
