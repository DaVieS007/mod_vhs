/**
 * @version $Id$
 * @brief vhs module
 * @author Xavier Beaudouin <kiwi (at) oav (dot) net>
 */

#include "include/mod_vhs.h"
#include "include/vhs_redirect_stuff.h" // Used for redirect subsystem when a hostname is not found ##


#ifdef HAVE_MOD_DBD_SUPPORT
	static ap_dbd_t *(*vhost_dbd_acquire_fn)(request_rec*) = NULL;
	static void (*vhost_dbd_prepare_fn)(server_rec*, const char*, const char*) = NULL;
	#define VH_KEY "mod_vhs"
	#include "include/getmoddbdhome.h" // Get the stuff from Mod DBD ##
#endif /* HAVE_MOD_DBD_SUPPORT */

#ifdef HAVE_LDAP_SUPPORT
	#define FILTER_LENGTH MAX_STRING_LEN
	#include "include/getldaphome.h" // Get the stuff from LDAP ##
	#include "include/mod_vhs_ldap_parse_url.h" // parse LDAP url function ##
#endif /* HAVE_LDAP_SUPPORT */

#include "include/vhs_retrieve_vhost.h" // Get the stuff from Mod DBD or LDAP ##

#ifdef HAVE_MOD_SUPHP_SUPPORT
	#include "include/vhs_suphp.h" // include suPHP functions ##
#endif /* HAVE_MOD_SUPHP_SUPPORT  */

#ifdef HAVE_MOD_PHP_SUPPORT
	#include "include/vhs_php_config.h" // include vhs_php_config function ##
#endif /* HAVE_MOD_PHP_SUPPORT */

/*
 * Let's start coding
 */
module AP_MODULE_DECLARE_DATA vhs_module;

/**
 * @brief Apache per server config structure
 * @remark Allocates and initializes memory for configuration data
 * @param p memory pool object
 * @param s The server_rec structure is one of the key components of the Apache API. One is created for every virtual host.
 * @return pointer to the created vhs module server configuration structure
 */
static void * vhs_create_server_config(apr_pool_t * p, server_rec * s)
{
	vhs_config_rec *vhr = (vhs_config_rec *) apr_pcalloc(p, sizeof(vhs_config_rec));

	/*
	 * Pre default the module is not enabled
	 */
	vhr->enable 		= 0;

	/*
	 * From mod_alias.c
	 */
	vhr->aliases		= apr_array_make(p, 20, sizeof(alias_entry));
	vhr->redirects		= apr_array_make(p, 20, sizeof(alias_entry));

#ifdef HAVE_LDAP_SUPPORT
	vhr->ldap_binddn	= NULL;
	vhr->ldap_bindpw	= NULL;
	vhr->ldap_have_url	= 0;
	vhr->ldap_have_deref	= 0;
	vhr->ldap_deref		= always;
	vhr->ldap_set_filter	= 0;
#endif /* HAVE_LDAP_SUPPORT */

#ifdef HAVE_MPM_ITK_SUPPORT
	vhr->itk_enable 	= 0;
#endif /* HAVE_MPM_ITK_SUPPORT */

#ifdef HAVE_MOD_DBD_SUPPORT
	vhr->query_label		= NULL;
#endif /* HAVE_MOD_DBD_SUPPORT */

#ifdef HAVE_MEMCACHE_SUPPORT
	vhr->memcache_enable	= 0;
#endif /* HAVE_MEMCACHE_SUPPORT */

#ifdef HAVE_FSCACHE_SUPPORT
	vhr->fscache_enable	= 1;
	vhr->fscache_dir	= "/tmp/vhs_cache";		// FS CACHE PATH
	vhr->fscache_timeout	= apr_time_from_sec(60);	// QUERY DATABASE AGAIN IF TIMEOUT EXPIRED ; IF THE DATABASE DOESNT ANSWER, IT WILL RETURN THE VALUE FROM FS CACHE
	vhr->fscache_expire	= apr_time_from_sec(3600);	// DELETE FILES HAVING MTIME OLDER THAN AN HOUR
#endif /* HAVE_FSCACHE_SUPPORT */
	return (void *)vhr;
}


/**
 * @brief Apache merge per server config structures
 * @remark Merges the data stored for each vHost with data stored for the master server.
 * The function may not modify any of it's arguments.
 * @param p memory pool object
 * @param parentv 
 * @param childv
 * @return pointer to the created vhs module server configuration structure containing the merged values.
 */
static void * vhs_merge_server_config(apr_pool_t * p, void *parentv, void *childv)
{
	vhs_config_rec *parent	= (vhs_config_rec *) parentv;
	vhs_config_rec *child	= (vhs_config_rec *) childv;
	vhs_config_rec *conf	= (vhs_config_rec *) apr_pcalloc(p, sizeof(vhs_config_rec));

	conf->enable 		= (child->enable ? child->enable : parent->enable);
	conf->path_prefix 	= (child->path_prefix ? child->path_prefix : parent->path_prefix);
	conf->default_host 	= (child->default_host ? child->default_host : parent->default_host);
	conf->lamer_mode 	= (child->lamer_mode ? child->lamer_mode : parent->lamer_mode);
	conf->log_notfound 	= (child->log_notfound ? child->log_notfound : parent->log_notfound);

#ifdef HAVE_MOD_PHP_SUPPORT
	#ifdef OLD_PHP
		conf->safe_mode 	= (child->safe_mode ? child->safe_mode : parent->safe_mode);
	#endif /* OLD_PHP */
	conf->open_basedir 		= (child->open_basedir ? child->open_basedir : parent->open_basedir);
	conf->display_errors 	= (child->display_errors ? child->display_errors : parent->display_errors);
	conf->append_basedir 	= (child->append_basedir ? child->append_basedir : parent->append_basedir);
	conf->openbdir_path 	= (child->openbdir_path ? child->openbdir_path : parent->openbdir_path);
	conf->phpopt_fromdb 	= (child->phpopt_fromdb ? child->phpopt_fromdb : parent->phpopt_fromdb);
#endif /* HAVE_MOD_PHP_SUPPORT */

#ifdef HAVE_MOD_SUPHP_SUPPORT
    conf->suphp_config_path = (child->suphp_config_path ? child->suphp_config_path : parent->suphp_config_path);
#endif /* HAVE_MOD_SUPHP_SUPPORT */

#ifdef HAVE_MPM_ITK_SUPPORT
	conf->itk_enable = (child->itk_enable ? child->itk_enable : parent->itk_enable);
	conf->itk_defuid = (child->itk_defuid ? child->itk_defuid : parent->itk_defuid);
	conf->itk_defgid = (child->itk_defgid ? child->itk_defgid : parent->itk_defgid);
	conf->itk_defusername = (child->itk_defusername ? child->itk_defusername : parent->itk_defusername);
#endif /* HAVE_MPM_ITK_SUPPORT */

#ifdef HAVE_LDAP_SUPPORT
    if (child->ldap_have_url) {
    	conf->ldap_have_url   = child->ldap_have_url;
    	conf->ldap_url        = child->ldap_url;
    	conf->ldap_host       = child->ldap_host;
    	conf->ldap_port       = child->ldap_port;
    	conf->ldap_basedn     = child->ldap_basedn;
    	conf->ldap_scope      = child->ldap_scope;
    	conf->ldap_filter     = child->ldap_filter;
    	conf->ldap_secure     = child->ldap_secure;
		conf->ldap_set_filter = child->ldap_set_filter;
    } else {
		conf->ldap_have_url   = parent->ldap_have_url;
		conf->ldap_url        = parent->ldap_url;
		conf->ldap_host       = parent->ldap_host;
		conf->ldap_port       = parent->ldap_port;
		conf->ldap_basedn     = parent->ldap_basedn;
		conf->ldap_scope      = parent->ldap_scope;
		conf->ldap_filter     = parent->ldap_filter;
		conf->ldap_secure     = parent->ldap_secure;
		conf->ldap_set_filter = parent->ldap_set_filter;
    }
    if (child->ldap_have_deref) {
    	conf->ldap_have_deref = child->ldap_have_deref;
    	conf->ldap_deref      = child->ldap_deref;
    } else {
    	conf->ldap_have_deref = parent->ldap_have_deref;
    	conf->ldap_deref      = parent->ldap_deref;
    }

    conf->ldap_binddn = (child->ldap_binddn ? child->ldap_binddn : parent->ldap_binddn);
    conf->ldap_bindpw = (child->ldap_bindpw ? child->ldap_bindpw : parent->ldap_bindpw);
#endif /* HAVE_LDAP_SUPPORT */
	
#ifdef HAVE_MOD_DBD_SUPPORT
	conf->query_label = (child->query_label ? child->query_label : parent->query_label);
#endif /* HAVE_MOD_DBD_SUPPORT */

#ifdef HAVE_MEMCACHE_SUPPORT
	conf->memcache_enable = (child->memcache_enable ? child->memcache_enable : parent->memcache_enable);
	conf->memcache_instance = (child->memcache_instance ? child->memcache_instance : parent->memcache_instance);
	conf->memcache_lifetime = (child->memcache_lifetime ? child->memcache_lifetime : parent->memcache_lifetime);
#endif /* HAVE_MEMCACHE_SUPPORT */

#ifdef HAVE_FSCACHE_SUPPORT
	// Shouldnt the parent and the child be the other way around?
	conf->fscache_enable	= (parent->fscache_enable == 0 ? parent->fscache_enable : child->fscache_enable);
	conf->fscache_dir	= ((parent->fscache_dir && parent->fscache_dir[0] == '/') ? parent->fscache_dir : child->fscache_dir);
	conf->fscache_timeout	= (parent->fscache_timeout ? parent->fscache_timeout : child->fscache_timeout);
	conf->fscache_expire	= (parent->fscache_expire ? parent->fscache_expire : child->fscache_expire);
#endif /* HAVE_FSCACHE_SUPPORT */

	conf->aliases   = apr_array_append(p, child->aliases, parent->aliases);
	conf->redirects = apr_array_append(p, child->redirects, parent->redirects);

	return conf;
}

#include "include/set_field.h" // Set the fields inside the conf struct ##
#include "include/set_flag.h" // setting flags ##
#include "include/vhs_init_handler.h" // init handler ##

#ifdef HAVE_MPM_ITK_SUPPORT
	#include "include/vhs_itk_post_read.h" // configure MPM-ITK ##

static const char *vhs_itk_assign_default_user_id (cmd_parms *parms, void *ptr, const char *user_name, const char *group_name)
{
	/* Defines a specific User and Group for VHosts not found on the database (default is nobody/nogroup), code based on assign_user_id() from mpm-itk */
	const char *err = ap_check_cmd_context(parms, NOT_IN_HTACCESS);
	if (err) {
		return err;
	}

	vhs_config_rec *vhr = (vhs_config_rec *) ap_get_module_config(parms->server->module_config, &vhs_module);
	vhr->itk_defuid = ap_uname2id(user_name);
	vhr->itk_defgid = ap_gname2id(group_name);
	vhr->itk_defusername = apr_pstrdup(parms->pool, user_name);

	ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, NULL, "vhs_itk_assign_default_user_id:  itk_defuid:%d, itk_defgid:%d, itk_defusername:%s", vhr->itk_defuid, vhr->itk_defgid, vhr->itk_defusername);
	return NULL;
}
#endif /* HAVE_MPM_ITK_SUPPORT  */

#include "include/vhs_translate_name.h" // send the right path to the end user uppon a request ##

#ifdef HAVE_MEMCACHE_SUPPORT
#include "include/vhs_memcache_status_hook.h" // Extend the Apache server-status with memcached stats ##

/**
 * @brief Handle mod_status status page generation
 * @remark This is an "ap_hook_pre_config" - read Apache API Documentation
 * @param pconf memory pool object
 * @param plog memory pool object
 * @param ptemp memory pool object
 * @return always OK
 */
static int vhs_memcache_pre_config(apr_pool_t *pconf, apr_pool_t *plog, apr_pool_t *ptemp)
{
    // Register to handle mod_status status page generation ##
	APR_OPTIONAL_HOOK(ap, status_hook, vhs_memcache_status_hook, NULL, NULL, APR_HOOK_MIDDLE);
	return OK;
}
#endif /* HAVE_MEMCACHE_SUPPORT */

/**
 * @brief Stuff for register the module
 * @remark The array of "command_rec" structures is passed to the httpd core
 * by vhs module to declare a new configuration directive.
 * The command_table field holds a pointer to a command_rec structure, which
 * describes each of the module's configuration directives in detail and
 * points back to configuration callbacks to process the directives.
 */
static const command_rec vhs_commands[] = {
	AP_INIT_FLAG( "EnableVHS", set_flag, (void *)5, RSRC_CONF, "Enable VHS module"),

	AP_INIT_TAKE1("vhs_Path_Prefix", set_field, (void *)1, RSRC_CONF, "Set path prefix."),
	AP_INIT_TAKE1("vhs_Default_Host", set_field, (void *)2, RSRC_CONF, "Set default host if HTTP/1.1 is not used."),
	AP_INIT_FLAG( "vhs_Lamer", set_flag, (void *)0, RSRC_CONF, "Enable Lamer Friendly mode"),
	AP_INIT_FLAG( "vhs_LogNotFound", set_flag, (void *)7, RSRC_CONF, "Log on error log when host or path is not found."),

#ifdef HAVE_MOD_PHP_SUPPORT
	#ifdef OLD_PHP
		AP_INIT_FLAG( "vhs_PHPsafe_mode", set_flag, (void *)1, RSRC_CONF, "Enable PHP Safe Mode"),
	#endif /* OLD_PHP */
	AP_INIT_FLAG( "vhs_PHPopen_basedir", set_flag, (void *)2, RSRC_CONF, "Set PHP open_basedir to path"),
	AP_INIT_FLAG( "vhs_PHPopt_fromdb", set_flag, (void *)3, RSRC_CONF, "Gets PHP options from db/libhome"),
	AP_INIT_FLAG( "vhs_PHPdisplay_errors", set_flag, (void *)4, RSRC_CONF, "Enable PHP display_errors"),
	AP_INIT_FLAG( "vhs_append_open_basedir", set_flag, (void *)6, RSRC_CONF, "Append homedir path to PHP open_basedir to vhs_open_basedir_path."),
	AP_INIT_TAKE1("vhs_open_basedir_path", set_field, (void *)3, RSRC_CONF, "The default PHP open_basedir path."),
#endif /* HAVE_MOD_PHP_SUPPORT */

#ifdef HAVE_MOD_SUPHP_SUPPORT
	AP_INIT_TAKE1( "vhs_suphp_config_path", set_field, (void *)10, RSRC_CONF, "The SuPHP configuration path for the user"),
#endif /* HAVE_MOD_SUPHP_SUPPORT */

#ifdef HAVE_MPM_ITK_SUPPORT
	AP_INIT_FLAG("vhs_itk_enable", set_flag, (void *)8, RSRC_CONF, "Enable MPM-ITK support"),
	AP_INIT_TAKE2("vhs_itk_AssignDefaultUserID", vhs_itk_assign_default_user_id, NULL, RSRC_CONF|ACCESS_CONF, "Set the default user and group for VHosts not defined on the database."),
#endif /* HAVE_MPM_ITK_SUPPORT */

	AP_INIT_TAKE2( "vhs_Alias", add_alias, NULL, RSRC_CONF, "a fakename and a realname"),
	AP_INIT_TAKE2( "vhs_ScriptAlias", add_alias, "cgi-script", RSRC_CONF, "a fakename and a realname"),
	AP_INIT_TAKE23("vhs_Redirect", add_redirect, (void *)HTTP_MOVED_TEMPORARILY, OR_FILEINFO,
						"an optional status, then document to be redirected and "
						"destination URL"),
	AP_INIT_TAKE2( "vhs_AliasMatch", add_alias_regex, NULL, RSRC_CONF, "a regular expression and a filename"),
	AP_INIT_TAKE2( "vhs_ScriptAliasMatch", add_alias_regex, "cgi-script", RSRC_CONF, "a regular expression and a filename"),
	AP_INIT_TAKE23("vhs_RedirectMatch", add_redirect_regex, (void *)HTTP_MOVED_TEMPORARILY, OR_FILEINFO,
						"an optional status, then a regular expression and "
						"destination URL"),
	AP_INIT_TAKE2( "vhs_RedirectTemp", add_redirect2, (void *)HTTP_MOVED_TEMPORARILY, OR_FILEINFO,
						"a document to be redirected, then the destination URL"),
	AP_INIT_TAKE2( "vhs_RedirectPermanent", add_redirect2, (void *)HTTP_MOVED_PERMANENTLY, OR_FILEINFO,
						"a document to be redirected, then the destination URL"),
#ifdef HAVE_LDAP_SUPPORT
	AP_INIT_TAKE1( "vhs_LDAPBindDN",set_field, (void *)4, RSRC_CONF,
						"DN to use to bind to LDAP server. If not provided, will do an anonymous bind."),
	AP_INIT_TAKE1( "vhs_LDAPBindPassword",set_field, (void *)5, RSRC_CONF,
						"Password to use to bind LDAP server. If not provider, will do an anonymous bind."),
	AP_INIT_TAKE1( "vhs_LDAPDereferenceAliases",set_field, (void *)6, RSRC_CONF,
						"Determines how aliases are handled during a search. Can be one of the"
			            		"values \"never\", \"searching\", \"finding\", or \"always\"."
						"Defaults to always."),
	
	AP_INIT_FLAG(  "vhs_LDAPSetFilter", set_flag, (void *)9, RSRC_CONF, "Use filter given in LDAPUrl"),

	AP_INIT_TAKE1( "vhs_LDAPUrl",mod_vhs_ldap_parse_url, NULL, RSRC_CONF,
						"URL to define LDAP connection in form ldap://host[:port]/basedn[?attrib[?scope[?filter]]]."),
#endif /* HAVE_LDAP_SUPPORT */

#ifdef HAVE_MEMCACHE_SUPPORT
    AP_INIT_TAKE1("vhs_MemcachedInstance", set_field, (void *)11, RSRC_CONF, "Name of the mod_vhs instance (used by Memcache)"),
	AP_INIT_FLAG("vhs_Memcached", set_flag, (void *)10, RSRC_CONF, "Set to On/Off to use memcached."),
	AP_INIT_TAKE1("vhs_MemcachedLifeTime", set_field, (void *)12, RSRC_CONF, "Lifetime (in seconds) associated with stored objects in memcache daemon(s). Default is 600 s."),
#endif /* HAVE_MEMCACHE_SUPPORT */

#ifdef HAVE_MOD_DBD_SUPPORT
	AP_INIT_TAKE1("vhs_DBD_SQL_Label", set_field, (void *)0, RSRC_CONF, "DBDPrepareSQL label name used to construct SQL request to fetch vhost for host"),
#endif /* HAVE_MOD_DBD_SUPPORT */

#ifdef HAVE_FSCACHE_SUPPORT
	AP_INIT_FLAG( "vhs_FScache", set_flag, (void *)11, RSRC_CONF, "Enable VHS file cache"),
	AP_INIT_TAKE1("vhs_FSCachePath", set_field, (void*)13, RSRC_CONF, "Path for the VHS file cache"),
	AP_INIT_TAKE1("vhs_FSCacheTimeout", set_field, (void*)14, RSRC_CONF, "Timeout for the cache entries, the database is re-queried after the delay and it will use the cache values if the database is not available"),
	AP_INIT_TAKE1("vhs_FSCacheExpire", set_field, (void*)15, RSRC_CONF, "Expiration for the cache entries, the cache file is removed"),
#endif

	{NULL}
};

/**
 * @brief Register mod_vhs functions
 * @remark This is a pointer to a function that details the hooks that vhs module handles.
 * by this module to declare a new configuration directive.
 * @param p memory pool object
 */
static void register_hooks(apr_pool_t * p)
{
	/* Modules that have to be loaded before mod_vhs */
	static const char *const aszPre[] =
	{"mod_userdir.c", "mod_vhost_alias.c", NULL};
	/* Modules that have to be loaded after mod_vhs */
	static const char *const aszSucc[] =
	{"mod_php.c", "mod_suphp.c", NULL};
	
#ifdef HAVE_MPM_ITK_SUPPORT
	static const char * const aszSuc_itk[]= {"mpm_itk.c",NULL };
	ap_hook_post_read_request(vhs_itk_post_read, NULL, aszSuc_itk, APR_HOOK_REALLY_FIRST);
#endif /* HAVE_MPM_ITK_SUPPORT */
	
	ap_hook_post_config(vhs_init_handler, NULL, NULL, APR_HOOK_MIDDLE);
	
#ifdef HAVE_MEMCACHE_SUPPORT
    ap_hook_pre_config(vhs_memcache_pre_config, NULL, NULL, APR_HOOK_MIDDLE);
#endif /* HAVE_MEMCACHE_SUPPORT */

	ap_hook_translate_name(vhs_translate_name, aszPre, aszSucc, APR_HOOK_FIRST);
	ap_hook_fixups(fixup_redir, NULL, NULL, APR_HOOK_MIDDLE);

#ifdef HAVE_MOD_SUPHP_SUPPORT
	ap_hook_handler(vhs_suphp_handler, NULL, aszSucc, APR_HOOK_FIRST);
#endif /* HAVE_MOD_SUPHP_SUPPORT */

#ifdef HAVE_LDAP_SUPPORT
	ap_hook_optional_fn_retrieve(ImportULDAPOptFn,NULL,NULL,APR_HOOK_MIDDLE);
#endif /* HAVE_LDAP_SUPPORT */
}

/**
 * @brief The global list of "ap_listen_rec" (Apache's listeners record) structures.
 */
AP_DECLARE_DATA module vhs_module = {
	STANDARD20_MODULE_STUFF,
	create_alias_dir_config,		/**< create per-directory config structure */
	merge_alias_dir_config,			/**< merge per-directory config structures */
	vhs_create_server_config,		/**< create per-server config structure */
	vhs_merge_server_config,		/**< merge per-server config structures */
	vhs_commands,					/**< command apr_table_t */
	register_hooks					/**< register hooks */
};
