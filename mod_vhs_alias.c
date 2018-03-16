/**
 * @version $Id$
 * @brief Loosely based on mod_alias.c
 */

#include "include/mod_vhs.h"

extern module AP_MODULE_DECLARE_DATA vhs_module;

 void * create_alias_dir_config(apr_pool_t * p, char *d)
{
	alias_dir_conf *a = (alias_dir_conf *) apr_pcalloc(p, sizeof(alias_dir_conf));
	a->redirects = apr_array_make(p, 2, sizeof(alias_entry));
	return a;
}

 void * merge_alias_dir_config(apr_pool_t * p, void *basev, void *overridesv)
{
	alias_dir_conf *a = (alias_dir_conf *) apr_pcalloc(p, sizeof(alias_dir_conf));
	alias_dir_conf *base = (alias_dir_conf *) basev;
	alias_dir_conf *overrides = (alias_dir_conf *) overridesv;
	a->redirects = apr_array_append(p, overrides->redirects, base->redirects);
	return a;
}

 const char * add_alias_internal(cmd_parms * cmd, void *dummy, const char *f, const char *r, int use_regex)
{
	server_rec     *s = cmd->server;
	vhs_config_rec *conf = ap_get_module_config(s->module_config,
						    &vhs_module);
	alias_entry    *new = apr_array_push(conf->aliases);
	alias_entry    *entries = (alias_entry *) conf->aliases->elts;
	int		i;

	/* XX r can NOT be relative to DocumentRoot here... compat bug. */

	if (use_regex) {
#ifdef DEBIAN
		new->regexp = ap_pregcomp(cmd->pool, f, AP_REG_EXTENDED);
#else
		new->regexp = ap_pregcomp(cmd->pool, f, REG_EXTENDED);
#endif /* DEBIAN */
		if (new->regexp == NULL)
			return "Regular expression could not be compiled.";
		new->real = r;
	} else {
		/*
		 * XXX This may be optimized, but we must know that new->real
		 * exists.  If so, we can dir merge later, trusing new->real
		 * and just canonicalizing the remainder.  Not till I finish
		 * cleaning out the old ap_canonical stuff first.
		 */
		new->real = r;
	}
	new->fake = f;
	new->handler = cmd->info;

	/*
	 * check for overlapping (Script)Alias directives and throw a warning
	 * if found one
	 */
	if (!use_regex) {
		for (i = 0; i < conf->aliases->nelts - 1; ++i) {
			alias_entry    *p = &entries[i];

			if ((!p->regexp && alias_matches(f, p->fake) > 0)
			    || (p->regexp && !ap_regexec(p->regexp, f, 0, NULL, 0))) {
				ap_log_error(APLOG_MARK, APLOG_WARNING, 0, cmd->server,
					     "The %s directive in %s at line %d will probably "
				"never match because it overlaps an earlier "
					     "%sAlias%s.",
				   cmd->cmd->name, cmd->directive->filename,
					     cmd->directive->line_num,
					     p->handler ? "Script" : "",
					     p->regexp ? "Match" : "");
				break;	/* one warning per alias should be
					 * sufficient */
			}
		}
	}
	return NULL;
}

 const char * add_alias(cmd_parms * cmd, void *dummy, const char *f, const char *r)
{
	return add_alias_internal(cmd, dummy, f, r, 0);
}

 const char * add_alias_regex(cmd_parms * cmd, void *dummy, const char *f, const char *r)
{
	return add_alias_internal(cmd, dummy, f, r, 1);
}

 const char * add_redirect_internal(cmd_parms * cmd, alias_dir_conf * dirconf,
		                                  const char *arg1, const char *arg2,
		                                  const char *arg3, int use_regex)
{
	alias_entry    *new;
	server_rec     *s = cmd->server;
	vhs_config_rec *serverconf = ap_get_module_config(s->module_config,
							  &vhs_module);
	int		status = (int)(long)cmd->info;
#if APR_MAJOR_VERSION > 0
	ap_regex_t     *r = NULL;
#else
#ifdef DEBIAN
	ap_regex_t     *r = NULL;
#else
	regex_t        *r = NULL;
#endif				/* DEBIAN */
#endif
	const char     *f = arg2;
	const char     *url = arg3;

	if (!strcasecmp(arg1, "gone"))
		status = HTTP_GONE;
	else if (!strcasecmp(arg1, "permanent"))
		status = HTTP_MOVED_PERMANENTLY;
	else if (!strcasecmp(arg1, "temp"))
		status = HTTP_MOVED_TEMPORARILY;
	else if (!strcasecmp(arg1, "seeother"))
		status = HTTP_SEE_OTHER;
	else if (apr_isdigit(*arg1))
		status = atoi(arg1);
	else {
		f = arg1;
		url = arg2;
	}

	if (use_regex) {
#ifdef DEBIAN
		r = ap_pregcomp(cmd->pool, f, AP_REG_EXTENDED);
#else
		r = ap_pregcomp(cmd->pool, f, REG_EXTENDED);
#endif /* DEBIAN */
		if (r == NULL)
			return "Regular expression could not be compiled.";
	}
	if (ap_is_HTTP_REDIRECT(status)) {
		if (!url)
			return "URL to redirect to is missing";
		if (!use_regex && !ap_is_url(url))
			return "Redirect to non-URL";
	} else {
		if (url)
			return "Redirect URL not valid for this status";
	}

	if (cmd->path)
		new = apr_array_push(dirconf->redirects);
	else
		new = apr_array_push(serverconf->redirects);

	new->fake = f;
	new->real = url;
	new->regexp = r;
	new->redir_status = status;
	return NULL;
}


 const char * add_redirect(cmd_parms * cmd, void *dirconf,
	                             const char *arg1, const char *arg2,
	                             const char *arg3)
{
	return add_redirect_internal(cmd, dirconf, arg1, arg2, arg3, 0);
}

 const char * add_redirect2(cmd_parms * cmd, void *dirconf,
	                              const char *arg1, const char *arg2)
{
	return add_redirect_internal(cmd, dirconf, arg1, arg2, NULL, 0);
}

 const char * add_redirect_regex(cmd_parms * cmd, void *dirconf,
		                               const char *arg1, const char *arg2,
		                               const char *arg3)
{
	return add_redirect_internal(cmd, dirconf, arg1, arg2, arg3, 1);
}

 int alias_matches(const char *uri, const char *alias_fakename)
{
	const char     *aliasp = alias_fakename, *urip = uri;

	while (*aliasp) {
		if (*aliasp == '/') {
			/*
			 * any number of '/' in the alias matches any number
			 * in the supplied URI, but there must be at least
			 * one...
			 */
			if (*urip != '/')
				return 0;

			do {
				++aliasp;
			} while (*aliasp == '/');
			do {
				++urip;
			} while (*urip == '/');
		} else {
			/* Other characters are compared literally */
			if (*urip++ != *aliasp++)
				return 0;
		}
	}

	/* Check last alias path component matched all the way */

	if (aliasp[-1] != '/' && *urip != '\0' && *urip != '/')
		return 0;

	/*
	 * Return number of characters from URI which matched (may be greater
	 * than length of alias, since we may have matched doubled slashes)
	 */

	return urip - uri;
}

 char * try_alias_list(request_rec * r, apr_array_header_t * aliases,
	                         int doesc, int *status)
{
	alias_entry    *entries = (alias_entry *) aliases->elts;
#ifdef DEBIAN
	ap_regmatch_t	regm [AP_MAX_REG_MATCH];
#else
	regmatch_t	regm [AP_MAX_REG_MATCH];
#endif  /* DEBIAN */
	char           *found = NULL;
	int		i;

	for (i = 0; i < aliases->nelts; ++i) {
		alias_entry    *p = &entries[i];
		int		l;

		if (p->regexp) {
			if (!ap_regexec(p->regexp, r->uri, AP_MAX_REG_MATCH, regm, 0)) {
				if (p->real) {
					found = ap_pregsub(r->pool, p->real, r->uri,
						    AP_MAX_REG_MATCH, regm);
					if (found && doesc) {
						apr_uri_t	uri;
						apr_uri_parse(r->pool, found, &uri);
						/*
						 * Do not escape the query
						 * string or fragment.
						 */
						found = apr_uri_unparse(r->pool, &uri,
						     APR_URI_UNP_OMITQUERY);
						found = ap_escape_uri(r->pool, found);
						if (uri.query) {
							found = apr_pstrcat(r->pool, found, "?",
							   uri.query, NULL);
						}
						if (uri.fragment) {
							found = apr_pstrcat(r->pool, found, "#",
							uri.fragment, NULL);
						}
					}
				} else {
					/* need something non-null */
					found = apr_pstrdup(r->pool, "");
				}
			}
		} else {
			l = alias_matches(r->uri, p->fake);

			if (l > 0) {
				if (doesc) {
					char           *escurl;
					escurl = ap_os_escape_path(r->pool, r->uri + l, 1);

					found = apr_pstrcat(r->pool, p->real, escurl, NULL);
				} else
					found = apr_pstrcat(r->pool, p->real, r->uri + l, NULL);
			}
		}

		if (found) {
			if (p->handler) {	/* Set handler, and leave a
						 * note for mod_cgi */
				r->handler = p->handler;
				apr_table_setn(r->notes, "alias-forced-type", r->handler);
			}
			/*
			 * XXX This is as SLOW as can be, next step, we
			 * optimize and merge to whatever part of the found
			 * path was already canonicalized.  After I finish
			 * eliminating os canonical. Better fail test for
			 * ap_server_root_relative needed here.
			 */
			if (!doesc) {
				found = ap_server_root_relative(r->pool, found);
			}
			if (found) {
				*status = p->redir_status;
			}
			return found;
		}
	}

	return NULL;
}

 int fixup_redir(request_rec * r)
{
	void           *dconf = r->per_dir_config;
	alias_dir_conf *dirconf =
	(alias_dir_conf *) ap_get_module_config(dconf, &vhs_module);
	char           *ret;
	int		status;

	/* It may have changed since last time, so try again */

	if ((ret = try_alias_list(r, dirconf->redirects, 1, &status)) != NULL) {
		if (ap_is_HTTP_REDIRECT(status)) {
			if (ret[0] == '/') {
				char           *orig_target = ret;

				ret = ap_construct_url(r->pool, ret, r);
				ap_log_rerror(APLOG_MARK, APLOG_DEBUG, 0, r,
				"incomplete redirection target of '%s' for "
					      "URI '%s' modified to '%s'",
					      orig_target, r->uri, ret);
			}
			if (!ap_is_url(ret)) {
				status = HTTP_INTERNAL_SERVER_ERROR;
				ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, r,
					    "cannot redirect '%s' to '%s'; "
					      "target is not a valid absoluteURI or abs_path",
					      r->uri, ret);
			} else {
				/*
				 * append requested query only, if the config
				 * didn't supply its own.
				 */
				if (r->args && !ap_strchr(ret, '?')) {
					ret = apr_pstrcat(r->pool, ret, "?", r->args, NULL);
				}
				apr_table_setn(r->headers_out, "Location", ret);
			}
		}
		return status;
	}
	return DECLINED;
}
