/**
 * @version $Id$
 * @brief vhs module init handler
 * @remark If you hook a function into this phase it would 
 * be run when the server is initialized.
 * Functions that need to be started in this phase is usualy
 * one that sets a part of the outgoing Server header.
 * @author Xavier Beaudouin <kiwi (at) oav (dot) net>
 * @param pconf memory pool object
 * @param plog memory pool object
 * @param ptemp memory pool object
 * @param s The server_rec structure is one of the key components of the Apache API. One is created for every virtual host.
 * @return OK or HTTP_INTERNAL_SERVER_ERROR
 */

#ifndef vhs_init_handler_H
#define vhs_init_handler_H

static int vhs_init_handler(apr_pool_t * pconf, apr_pool_t * plog, apr_pool_t * ptemp, server_rec * s)
{

#ifdef HAVE_LDAP_SUPPORT
	/* make sure that mod_ldap (util_ldap) is loaded */
	if (ap_find_linked_module("util_ldap.c") == NULL) {
		ap_log_error(APLOG_MARK, APLOG_ERR|APLOG_NOERRNO, 0, s,
			"Module mod_ldap missing. Mod_ldap (aka. util_ldap) "
			"must be loaded in order for mod_vhs to function properly");
		return HTTP_INTERNAL_SERVER_ERROR;
	}
#endif /* HAVE_LDAP_SUPPORT */

	ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, s, "loading version %s.", VH_VERSION);
	ap_add_version_component(pconf, VH_VERSION);

#ifdef HAVE_MPM_ITK_SUPPORT
	unsigned short int itk_enable = 1;
	server_rec *sp;

	module *mpm_itk_module = ap_find_linked_module("mpm_itk.c");
	if (mpm_itk_module == NULL) {
		/* the mpm_itk module is not loaded */
		itk_enable = 0;
	}

	for (sp = s; sp; sp = sp->next) {
		vhs_config_rec *vhr = (vhs_config_rec *) ap_get_module_config(sp->module_config, &vhs_module);

		if (vhr->itk_enable) {
			if (!itk_enable) {
				vhr->itk_enable = 0;
				ap_log_error(APLOG_MARK, APLOG_ERR, 0, sp, "vhs_init_handler: vhs_itk_enable is set but mpm_itk.c is not loaded");
			} else {
				/* defining default values if vhs_itk_AssignDefaultUserID hasnt been called */
				if(!vhr->itk_defuid) {
					vhr->itk_defuid = 65534;
				}
				if(!vhr->itk_defgid) {
					vhr->itk_defgid = 65534;
				}
				if(!vhr->itk_defusername) {
					vhr->itk_defusername = "nobody";
				}

				ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, sp, "vhs_init_handler: default itk uid='%d' default itk gid='%d' default itk username='%s'", vhr->itk_defuid, vhr->itk_defgid, vhr->itk_defusername);
			}
		}
	}
#endif /* HAVE_MPM_ITK_SUPPORT  */

	return OK;
}

#endif
