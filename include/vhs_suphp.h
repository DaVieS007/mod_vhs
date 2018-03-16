/**
 * @version $Id$
 * @brief suPHP functions and configuration structure
 * @author Xavier Beaudouin <kiwi (at) oav (dot) net>
 */

/**
 * @brief suPHP configuration structure
 */
 
typedef struct {
	int			engine;			/**< Status of suPHP_Engine */
	char		*php_config;
	int			cmode;			/**< Server of directory configuration? */
	char		*target_user;
	char		*target_group;
	apr_table_t	*handlers;
} suphp_conf;

#ifndef vhs_suphp_handler_H
#define vhs_suphp_handler_H

/**
 * @brief This function will be called when a request reaches the content generation phase.
 * This function handles the suPHP stuff.
 * @remark This is an "ap_hook_handler" - read Apache API Documentation
 * @param r structure of the current client request
 * @return DECLINED or HTTP_INTERNAL_SERVER_ERROR
 */
 
static int vhs_suphp_handler(request_rec *r)
{
	module *suphp_module = ap_find_linked_module("mod_suphp.c");

	if (suphp_module == NULL) {
		ap_log_error(APLOG_MARK, APLOG_ERR, 0, r->server, "vhs_suphp_handler: mod_suphp.c is not loaded");
		return HTTP_INTERNAL_SERVER_ERROR;
	}

	suphp_conf *cfg    = (suphp_conf *)ap_get_module_config(r->server->module_config, suphp_module);
	suphp_conf *dircfg = (suphp_conf *)ap_get_module_config(r->per_dir_config, suphp_module);

	if (cfg == NULL)
		return HTTP_INTERNAL_SERVER_ERROR;

	dircfg->engine       = cfg->engine;
	dircfg->php_config   = cfg->php_config;
	dircfg->target_user  = cfg->target_user;
	dircfg->target_group = cfg->target_group;

	ap_set_module_config(r->per_dir_config, suphp_module, dircfg);

	return DECLINED;
}

#endif


#ifndef vhs_suphp_config_H
#define vhs_suphp_config_H

/**
 * @brief This function is called by "vhs_translate_name".
 * This function will configure php with parameters form the users php.ini.
 * @param r structure of the current client request
 * @param vhr vhs module server configuration structure
 * @param path the path to the DocumentRoot
 * @param uid the given UID
 * @param uid the given GID
 * @return
 */
 
// XXX: to test
static void vhs_suphp_config(request_rec *r, vhs_config_rec *vhr, char *path, char *uid, char *gid)
{
  /* Path to the suPHP config file per user */
	char *transformedPath = NULL;
	char *transformedUid = NULL;
	char *transformedGid = NULL;

	if (vhr->suphp_config_path) {
		if ((strstr(vhr->suphp_config_path,"%s")!=NULL) && (uid!=NULL) && (gid!=NULL))
			transformedPath = apr_psprintf(r->pool, vhr->suphp_config_path, uid);
		else
			transformedPath = vhr->suphp_config_path;
	} else {
		transformedPath = path;
	}

	ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, r->server, "vhs_suphp_config: suPHP_config_dir set to %s", transformedPath);

	module *suphp_module = ap_find_linked_module("mod_suphp.c");

	if (suphp_module == NULL) {
		ap_log_error(APLOG_MARK, APLOG_ERR, 0, r->server, "vhs_suphp_config: mod_suphp.c is not loaded");
		return;
	}

	suphp_conf *cfg = (suphp_conf *)ap_get_module_config(r->server->module_config, suphp_module);
	if ( cfg == NULL )
		ap_log_error(APLOG_MARK, APLOG_ERR, 0, r->server, "vhs_suphp_config: suPHP_config_dir is NULL");

	// TODO: is this used ? 
	//       warning passwd var doesn't exist anymore
	//cfg->engine       = (strstr(passwd,"engine=Off") == NULL);
	cfg->php_config   = apr_pstrdup(r->pool,transformedPath);

	transformedUid    = apr_psprintf(r->pool, "#%d", (int) uid);
	cfg->target_user  = apr_pstrdup(r->pool,transformedUid);

	transformedGid    = apr_psprintf(r->pool, "#%d", (int) gid);
	cfg->target_group = apr_pstrdup(r->pool,transformedGid);

	ap_set_module_config(r->server->module_config, suphp_module, cfg);
}

#endif

