/**
 * @version $Id$
 * @brief set the flags (config params) inside the vhs module server configuration structure
 * @author Xavier Beaudouin <kiwi (at) oav (dot) net>
 * @param parms structure of the invoked command - read Apache API Documentation
 * @param mconfig not used
 * @param flag the given configuration parameter form mod_vhs configuration options (On / Off)
 * @return NULL
 */
 
#ifndef set_flag_H
#define set_flag_H

static const char * set_flag(cmd_parms * parms, void *mconfig, int flag)
{
	int		pos = (uintptr_t)parms->info;
	vhs_config_rec *vhr = (vhs_config_rec *) ap_get_module_config(parms->server->module_config, &vhs_module);

	/*	VH_AP_LOG_ERROR(APLOG_MARK, APLOG_DEBUG, 0, parms->server,
		     "set_flag:Flag='%d' for server: '%s' for pos='%d' line: %d",
		     flag, parms->server->defn_name, pos, parms->server->defn_line_number ); */
	switch (pos) {
	case 0:
		if (flag) {
			vhr->lamer_mode = 1;
		} else {
			vhr->lamer_mode = 0;
		}
		break;

#ifdef HAVE_MOD_PHP_SUPPORT

	#ifdef OLD_PHP
	case 1:
		if (flag) {
			vhr->safe_mode = 1;
		} else {
			vhr->safe_mode = 0;
		}
		break;
	#endif /* OLD_PHP */

	case 2:
		if (flag) {
			vhr->open_basedir = 1;
		} else {
			vhr->open_basedir = 0;
		}
		break;
	case 3:
		if (flag) {
			vhr->phpopt_fromdb = 1;
		} else {
			vhr->phpopt_fromdb = 0;
		}
		break;
	case 4:
		if (flag) {
			vhr->display_errors = 1;
		} else {
			vhr->display_errors = 0;
		}
		break;
#endif /* HAVE_MOD_PHP_SUPPORT */

	case 5:
		if (flag) {
			vhr->enable = 1;
		} else {
			vhr->enable = 0;
		}
		break;

#ifdef HAVE_MOD_PHP_SUPPORT
	case 6:
		if (flag) {
			vhr->append_basedir = 1;
		} else {
			vhr->append_basedir = 0;
		}
		break;
#endif /* HAVE_MOD_PHP_SUPPORT */

	case 7:
		if (flag) {
			vhr->log_notfound = 1;
		} else {
			vhr->log_notfound = 0;
		}
		break;

#ifdef HAVE_MPM_ITK_SUPPORT
	case 8:
		if (flag) {
			vhr->itk_enable = 1;
		} else {
			vhr->itk_enable = 0;
		}
		break;
#endif /* HAVE_MPM_ITK_SUPPORT  */

#ifdef HAVE_LDAP_SUPPORT
	case 9:
		if (flag) {
			vhr->ldap_set_filter = 1;
		} else {
			vhr->ldap_set_filter = 0;
		}
		break;
#endif /* HAVE_LDAP_SUPPORT */

#ifdef HAVE_MEMCACHE_SUPPORT
	case 10:
		if (flag) {
			vhr->memcache_enable = 1;
		} else {
			vhr->memcache_enable = 0;
		}
		break;
#endif /* HAVE_MEMCACHE_SUPPORT */

#ifdef HAVE_FSCACHE_SUPPORT
	case 11:
		if (flag) {
			vhr->fscache_enable = 1;
		} else {
			vhr->fscache_enable = 0;
		}
		break;
#endif /* HAVE_FSCACHE_SUPPORT */

	}
	return NULL;
}

#endif
