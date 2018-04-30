/**
 * @version $Id$
 * @brief Used for redirect subsystem when a hostname is not found
 * @author Xavier Beaudouin <kiwi (at) oav (dot) net>
 * @param r structure of the current client request
 * @param vhr vhs module server configuration structure
 * @return DECLINED or HTTP_MOVED_TEMPORARILY
 */

#ifndef vhs_redirect_stuff_H
#define vhs_redirect_stuff_H

static int vhs_redirect_stuff(request_rec * r, vhs_config_rec * vhr)
{
	if (vhr->default_host) {
		apr_table_setn(r->headers_out, "Location", vhr->default_host);
		ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, r->server, "redirect_stuff: using a redirect to %s for %s", vhr->default_host, r->hostname);
		return HTTP_MOVED_TEMPORARILY;
	}
	/* Failsafe */
	if (vhr->log_notfound) {
		ap_log_error(APLOG_MARK, APLOG_ALERT, 0, r->server, "redirect_stuff: no host found (non HTTP/1.1 request, no default set) %s", r->hostname);
	}
	return DECLINED;
}

#endif
