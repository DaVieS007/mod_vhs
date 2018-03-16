/**
 * @version $Id$
 * @brief This function will configure on the fly the php like php.ini will do.
 * @author Xavier Beaudouin <kiwi (at) oav (dot) net>
 * @param r structure of the current client request
 * @param vhr vhs module server configuration structure
 * @param path the path to the DocumentRoot
 * @param passwd the given php parameters
 */

#ifndef vhs_php_config_H
#define vhs_php_config_H

static void vhs_php_config(request_rec * r, vhs_config_rec * vhr, char *path, char *passwd)
{
	/*
	 * Some Basic PHP stuff, thank to Igor Popov module
	 */
	apr_table_set(r->subprocess_env, "PHP_DOCUMENT_ROOT", path);
	zend_alter_ini_entry("doc_root", sizeof("doc_root"), path, strlen(path), 4, 1);

#ifdef OLD_PHP
	/*
	 * vhs_PHPsafe_mode support
	 */
	if (vhr->safe_mode) {
		ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, r->server, "vhs_php_config: PHP safe_mode engaged");
		zend_alter_ini_entry("safe_mode", 10, "1", 1, 4, 16);
	} else {
		ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, r->server, "vhs_php_config: PHP safe_mode inactive, defaulting to php.ini values");
	}
#endif /* OLD_PHP */

	/*
	 * vhs_PHPopen_baserdir    \ vhs_append_open_basedir |  support
	 * vhs_open_basedir_path   /
	 */
	if (vhr->open_basedir) {
		if (vhr->append_basedir && vhr->openbdir_path) {
			/*
			 * There is a default open_basedir path and
			 * configuration allow appending them
			 */
			char *obasedir_path;

			if (vhr->path_prefix) {
				obasedir_path = apr_pstrcat(r->pool, vhr->openbdir_path, ":", vhr->path_prefix, path, NULL);
			} else {
				obasedir_path = apr_pstrcat(r->pool, vhr->openbdir_path, ":", path, NULL);
			}
			zend_alter_ini_entry("open_basedir", 13, obasedir_path, strlen(obasedir_path), 4, 16);
			ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, r->server, "vhs_php_config: PHP open_basedir set to %s (appending mode)", obasedir_path);
		} else {
			zend_alter_ini_entry("open_basedir", 13, path, strlen(path), 4, 16);
			ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, r->server, "vhs_php_config: PHP open_basedir set to %s", path);
		}
	} else {
		ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, r->server, "vhs_php_config: PHP open_basedir inactive defaulting to php.ini values");
	}

	/*
	 * vhs_PHPdisplay_errors support
	 */
	if (vhr->display_errors) {
		ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, r->server, "vhs_php_config: PHP display_errors engaged");
		zend_alter_ini_entry("display_errors", 10, "1", 1, 4, 16);
	} else {
		ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, r->server, "vhs_php_config: PHP display_errors inactive defaulting to php.ini values");
	}

	/*
	 * vhs_PHPopt_fromdb
	 */
	if (vhr->phpopt_fromdb) {
		ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, r->server, "vhs_php_config: PHP from DB engaged");
		char           *retval;
		char           *state;
		char           *myphpoptions;

		myphpoptions = apr_pstrdup(r->pool, passwd);
		ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, r->server, "vhs_php_config: DB => %s", myphpoptions);

		if ((ap_strchr(myphpoptions, ';') != NULL) && (ap_strchr(myphpoptions, '=') != NULL)) {
			/* Getting values for PHP there so we can proceed */

			retval = apr_strtok(myphpoptions, ";", &state);
			while (retval != NULL) {
				char           *key = NULL;
				char           *val = NULL;
				char           *strtokstate = NULL;

				key = apr_strtok(retval, "=", &strtokstate);
				val = apr_strtok(NULL, "=", &strtokstate);
				ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, r->server, "vhs_php_config: Zend PHP Stuff => %s => %s", key, val);
				//zend_alter_ini_entry(key, strlen(key) + 1, val, strlen(val), 4, 16); // if safe mode is enabled this does not work ##
				zend_alter_ini_entry(key, strlen(key) + 1, val, strlen(val), 4, 1);
				retval = apr_strtok(NULL, ";", &state);
			}
		}
		else {
			ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, r->server, "vhs_php_config: no PHP stuff found.");
		}
	}
}

#endif

