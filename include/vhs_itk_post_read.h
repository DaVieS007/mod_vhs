/**
 * @version $Id$
 * @brief This function is called after reading the client request, before any other phase.
 * This function sets the UID and GID if apache2-mpm-itk is enabled.
 * @remark This is an "ap_hook_post_read_request" - read Apache API Documentation
 * @author Rene Kanzler <rk (at) cosmomill (dot) de>
 * @author Xavier Beaudouin <kiwi (at) oav (dot) net>
 * @param r structure of the current client request
 * @return OK or HTTP_INTERNAL_SERVER_ERROR
 */

#include "http_connection.h" // for ap_lingering_close() ##

#ifndef vhs_itk_post_read_H
#define vhs_itk_post_read_H

static int vhs_itk_post_read(request_rec *r)
{
	uid_t libhome_uid;
	gid_t libhome_gid;
	int vhost_found_by_request = DECLINED;
	
  	vhs_config_rec *vhr = (vhs_config_rec *) ap_get_module_config(r->server->module_config, &vhs_module);
	ap_log_rerror(APLOG_MARK, APLOG_DEBUG, 0, r, "vhs_itk_post_read: BEGIN ***");

	mod_vhs_request_t *reqc = (mod_vhs_request_t *) ap_get_module_config(r->request_config, &vhs_module);	
	if (!reqc) {
		ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, r->server, "vhs_itk_post_read: variable reqc does not already exists.... creating !");
		reqc = (mod_vhs_request_t *)apr_pcalloc(r->pool, sizeof(mod_vhs_request_t));
		reqc->vhost_found = VH_VHOST_INFOS_NOT_YET_REQUESTED;
		ap_set_module_config(r->request_config, &vhs_module, reqc);
	}
	
	
	vhs_connection_conf *conn_conf = (vhs_connection_conf *) ap_get_module_config(r->connection->conn_config, &vhs_module);
	if (!conn_conf) {
		ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, r->server, "vhs_itk_post_read: variable conn_conf does not already exists.... creating !");
		conn_conf = (vhs_connection_conf *) apr_pcalloc(r->connection->pool, sizeof(vhs_connection_conf));
		ap_set_module_config(r->connection->conn_config, &vhs_module, conn_conf);
	}

	// Force the re-fetch of informations on the database if the previous query on the same connection was for a different virtualhost ##
	if(conn_conf->name && strcmp(conn_conf->name, r->hostname) != 0) {
		// The connection was previously used for a different virtualhost ##
		ap_log_rerror(APLOG_MARK, APLOG_DEBUG, 0, r, "vhs_itk_post_read: Reusing connection for a different virtualhost (was %s, now %s)", conn_conf->name, r->hostname);
		conn_conf->docroot = NULL;
		conn_conf->name = NULL;
		conn_conf->vhost_found = VH_VHOST_INFOS_NOT_YET_REQUESTED;
	}

	if(!r->hostname) {
		// No "Host" header has been passed (eg. HTTPS without SNI), no need to check the database/cache ##
		ap_log_rerror(APLOG_MARK, APLOG_DEBUG, 0, r, "vhs_itk_post_read: Empty 'Host:' header");
		conn_conf->vhost_found = VH_VHOST_INFOS_NOT_FOUND;
	}

	/* Can we re-use a previous result from our conn_conf? 
	*
	* Note that if a conn_conf->docroot exists we are within the same connection,
	* so this request is guaranteed to be to the same IP address. 
	*/
	if (conn_conf->docroot) {
		// YES - we do not need to execute a query. Use the docroot we saved in conn_conf ##
		vhost_found_by_request = OK;
		reqc->vhost_found = VH_VHOST_INFOS_FOUND;
		ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, r->server, "vhs_itk_post_read: Using previous connection query, setting docroot to: '%s'", conn_conf->docroot);
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
	} else if(conn_conf->vhost_found == VH_VHOST_INFOS_NOT_FOUND) {
		// No need to re-query the database if the virtualhost was previously not found on it ##
		ap_log_rerror(APLOG_MARK, APLOG_DEBUG, 0, r, "vhs_itk_post_read: The virtualhost %s was previously not found in the database, not re-fetching informations from the database", conn_conf->name);
		reqc->vhost_found = conn_conf->vhost_found;
	} else {
		// No - we do need to execute a query ##
		vhost_found_by_request = DECLINED;
		vhost_found_by_request = vhs_retrieve_vhost(r, vhr, r->hostname, reqc); // Get the stuff from Mod DBD or LDAP ##
	}
		
	if (vhost_found_by_request == OK) {
		libhome_uid = atoi(reqc->uid);
		libhome_gid = atoi(reqc->gid);
	}
	else {
		if (vhr->lamer_mode) {
			ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, r->server, "vhs_itk_post_read: Lamer friendly mode engaged");
			if ((strncasecmp(r->hostname, "www.", 4) == 0) && (strlen(r->hostname) > 4)) {
				char           *lhost;
				lhost = apr_pstrdup(r->pool, r->hostname + 5 - 1);
				ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, r->server, "vhs_itk_post_read: Found a lamer for %s -> %s", r->hostname, lhost);

				// Trying to get vhost information ##
				if(conn_conf->vhost_found != VH_VHOST_INFOS_NOT_FOUND) {
					// No need to re-query the database if the virtualhost was previously not found on it ##
					vhost_found_by_request = vhs_retrieve_vhost(r, vhr, lhost, reqc); // Get the stuff from Mod DBD or LDAP ##
				}
	
				if (vhost_found_by_request == OK) {
					libhome_uid = atoi(reqc->uid);
					libhome_gid = atoi(reqc->gid);
					ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, r->server, "vhs_itk_post_read: lamer for %s -> %s has itk uid='%d' itk gid='%d'", r->hostname, lhost, libhome_uid, libhome_gid);
				} else {
					libhome_uid = vhr->itk_defuid;
					libhome_gid = vhr->itk_defgid;
					ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, r->server, "vhs_itk_post_read: no lamer found for %s set default itk uid='%d' itk gid='%d'", r->hostname, libhome_uid, libhome_gid);
				}
			} else { /* if ((strncasecmp(r->hostname, "www.", 4) == 0) && (strlen(r->hostname) > 4)) */
				libhome_uid = vhr->itk_defuid;
				libhome_gid = vhr->itk_defgid;
				ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, r->server, "vhs_itk_post_read: no lamer found for %s set default itk uid='%d' itk gid='%d'", r->hostname, libhome_uid, libhome_gid);
			}
		} else { /* if (vhr->lamer_mode) */
			libhome_uid = vhr->itk_defuid;
			libhome_gid = vhr->itk_defgid;
		}
	}

	/* If ITK support is not enabled, then don't process request */
	if (vhr->itk_enable) {
		module *mpm_itk_module = ap_find_linked_module("mpm_itk.c");
		
		if (mpm_itk_module == NULL) {
			ap_log_error(APLOG_MARK, APLOG_ERR, 0, r->server, "vhs_itk_post_read: mpm_itk.c is not loaded");
			return HTTP_INTERNAL_SERVER_ERROR;
		}
		
		itk_per_dir_conf *cfg = (itk_per_dir_conf *) ap_get_module_config(r->per_dir_config, mpm_itk_module);
		
		ap_log_rerror(APLOG_MARK, APLOG_DEBUG, 0, r, "vhs_itk_post_read: itk uid='%d' itk gid='%d' itk username='%s' before change", cfg->uid, cfg->gid, cfg->username);

		if ((libhome_uid == -1 || libhome_gid == -1)) { 
			cfg->uid = vhr->itk_defuid;
			cfg->gid = vhr->itk_defgid;
			cfg->username = vhr->itk_defusername;
		} else {
			extern AP_DECLARE_DATA int ap_has_irreversibly_setuid;
			// If ITK already dropped the privileges for this connection and this virtualhost needs yet another UID/GID, it will close the connection so the client can open a new connection for this query ##
			if (ap_has_irreversibly_setuid && (cfg->uid != libhome_uid || cfg->gid != libhome_gid)) {
				/* Most likely a case of switching uid/gid within a persistent
				* connection; the RFCs allow us to just close the connection
				* at anytime, so we excercise our right. :-)
				*/
				ap_log_error(APLOG_MARK, APLOG_WARNING, 0, NULL, "vhs_itk_post_read: Cannot change uid/gid within a persistant connection (from %d/%d to %d/%d), closing connection.", cfg->uid, cfg->gid, libhome_uid, libhome_gid);
				ap_lingering_close(r->connection);
				exit(0);
			}

			cfg->uid = libhome_uid;
			cfg->gid = libhome_gid;

			/* set the username - otherwise MPM-ITK will not work */
			apr_status_t rv = 0;
			rv = apr_uid_name_get(&cfg->username, libhome_uid, r->pool);
			if(rv != APR_SUCCESS) {
				ap_log_rerror(APLOG_MARK, APLOG_WARNING, 0, r, "vhs_itk_post_read: Cannot determine username for uid %d", libhome_uid);
				// Setting the username as #uid as a fallback ##
				cfg->username = apr_psprintf(r->pool, "#%d", libhome_uid);
			}
		}
#ifdef HAVE_MPM_ITK_PASS_USERNAME
		// The user executing the request is set as a variable that can be used on CustomLog (%{VH_USERNAME}) or in CGI/SSI scripts ##
		apr_table_set(r->subprocess_env, "VH_USERNAME", cfg->username);
#endif
		ap_log_rerror(APLOG_MARK, APLOG_DEBUG, 0, r, "vhs_itk_post_read: itk uid='%d' itk gid='%d' itk username='%s' after change", cfg->uid, cfg->gid, cfg->username);
	}
	ap_log_rerror(APLOG_MARK, APLOG_DEBUG, 0, r, "vhs_itk_post_read: END ***");
	
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
	/* to avoid to re-query the database if the virtualhost was not found */
	conn_conf->vhost_found = reqc->vhost_found ? reqc->vhost_found : 0;

	return OK;
}

#endif
