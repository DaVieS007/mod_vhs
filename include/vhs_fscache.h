#define FS_CACHE_FOUND 1
#define FS_CACHE_EXPIRED 2
#define FS_CACHE_NOT_FOUND 3
#define FS_CACHE_NO_CACHE 0
#define MAX_HOSTNAME_LEN 255 // DONT FORGET TO ADJUST struct mod_vhs_request_ss->*
#define MAX_CACHE_FILE_LEN 300
#define CLEANUP_PROBABILITY 5 // WILL ONLY VERIFY IF CLEANUP IS NEEDED 1/CLEANUP_PROBABILITY TIMES set_cache() IS CALLED, SET TO 1 TO EXECUTE CLEANUP AT EACH CALL

/** GET_MTIME **/
apr_time_t get_mtime(request_rec *r, const char *path)
{
	apr_status_t rv;
	apr_finfo_t finfo;

	rv = apr_stat(&finfo, path, APR_FINFO_MTIME, r->pool);
	if(rv == APR_SUCCESS)
	{
		return finfo.mtime;
	}
	else
	{
		// The file does not exists or cannot be accessed
		return 0;
	}
}
/** GET_MTIME **/

/** GET_SIZE **/
size_t get_size(request_rec *r, const char *path)
{
	apr_status_t rv;
	apr_finfo_t finfo;

	rv = apr_stat(&finfo, path, APR_FINFO_SIZE, r->pool);
	if(rv == APR_SUCCESS)
	{
		return finfo.size;
	}
	else
	{
		// The file does not exists or cannot be accessed
		return 0;
	}
}
/** GET_SIZE **/


/** IS_HOSTNAME_VALID **/
char is_hostname_valid(const char *hostname)
{
	char ret = 1, found = 0;
	size_t i, i2, len, len2;

	char allowed_chars[] = "QWERTZUIOPASDFGHJKLYXCVBNMqwertzuiopasdfghjklyxcvbnm-:_.0123456789[]";

	len = strlen(hostname);
	len2 = strlen(allowed_chars);

	/** AVOID BUFFER OVERFLOW **/
	if(len > MAX_HOSTNAME_LEN)
	{
		return 0;
	}
	/** AVOID BUFFER OVERFLOW **/

	// The hostname cannot start with these chars
	if(hostname[0] == ':' || hostname[0] == '_' || hostname[0] == '.' || hostname[0] == '[' || hostname[0] == ']') {
		return 0;
	}

	for(i = 0; i < len; i++)
	{
		found = 0;
		for(i2 = 0; i2 < len2; i2++)
		{
			if(hostname[i] == allowed_chars[i2])
			{
				found = 1;
			}
		}

		if(!found)
		{
			ret = 0;
			break;			
		}
	}

	return ret;
}
/** IS_HOSTNAME_VALID **/

/** FS_CACHE_CLEANUP **/
void fs_cache_cleanup(request_rec *r, vhs_config_rec *vhr)
{
	char need_cleanup = 1;
	apr_status_t rv, lock;
	apr_dir_t *dir;

	if(!vhr->fscache_dir)
	{
		ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, r, "[VHS::FSCACHE] Invalid cache directory: %s", vhr->fscache_dir);
		return;
	}

	rv = apr_dir_open(&dir, vhr->fscache_dir, r->pool);
	if(rv != APR_SUCCESS)
	{
		// The cache directory doesnt exists and is created with permission 0700
		apr_dir_make_recursive(vhr->fscache_dir, APR_UREAD|APR_UWRITE|APR_UEXECUTE, r->pool);
		ap_log_rerror(APLOG_MARK, APLOG_DEBUG, 0, r, "[VHS::FSCACHE] Cache directory %s has been created", vhr->fscache_dir);
		need_cleanup = 0; //EMPTY folder does not require cleanup
	} else {
		rv = apr_dir_close(dir);
		if(rv != APR_SUCCESS)
		{
			char errbuf[256];
			ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, r, "[VHS::FSCACHE] Could not close the cache directory %s: %s", vhr->fscache_dir, apr_strerror(rv, errbuf, sizeof(errbuf)));
		}
	}

	// Only check if the cleanup is needed 1/CLEANUP_PROBABILITY times to limit the load and the risk to hit a race condition on servers with many concurrent requests
#if CLEANUP_PROBABILITY > 1
	srand(r->connection->id);
	if((rand() % (CLEANUP_PROBABILITY)) == (CLEANUP_PROBABILITY - 1)) {
		ap_log_rerror(APLOG_MARK, APLOG_DEBUG, 0, r, "[VHS::FSCACHE] Cleaning up cache");
	} else {
		ap_log_rerror(APLOG_MARK, APLOG_DEBUG, 0, r, "[VHS::FSCACHE] Not cleaning up cache");
		return;
	}
#endif

	char cache_file[MAX_CACHE_FILE_LEN];
	char cleanup_file[MAX_CACHE_FILE_LEN];
	char data[32];
	apr_time_t mtime = 0;

	cleanup_file[0] = 0x00;
	sprintf(cleanup_file, "%s/.cleanup", vhr->fscache_dir);

	data[0] = 0x00;
	sprintf(data, "%i", (int)apr_time_now());


	/** ACQUIRE LOCK **/
	apr_file_t *f;
	rv = apr_file_open(&f, cleanup_file, APR_FOPEN_CREATE|APR_WRITE, APR_UREAD|APR_UWRITE, r->pool);
	if(rv != APR_SUCCESS)
	{
		char errbuf[256];
		ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, r, "[VHS::FSCACHE] Could not open file %s for write: %s, skipping cleanup", cleanup_file, apr_strerror(rv, errbuf, sizeof(errbuf)));
		return;
	}
	else
	{
		lock = apr_file_lock(f, APR_FLOCK_EXCLUSIVE);
		if(lock != APR_SUCCESS)
		{
			ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, r, "Cannot acquire exclusive lock for %s on fs_cache_cleanup(), skipping cleanup", cleanup_file);
			apr_file_close(f);
			return;
		}

		/** CHECK MTIME TO DO THE CLEANUP **/

		mtime = get_mtime(r, cleanup_file);
		if(mtime + vhr->fscache_expire >= apr_time_now())
		{
			need_cleanup = 0;
		}

		/** CHECK MTIME TO DO THE CLEANUP **/


		if(need_cleanup > 0)
		{
			apr_finfo_t dirent;

			rv = apr_dir_open(&dir, vhr->fscache_dir, r->pool);
			if(rv == APR_SUCCESS)
			{
				// The cache directory exists
				while ((apr_dir_read(&dirent, APR_FINFO_DIRENT|APR_FINFO_TYPE|APR_FINFO_NAME, dir)) == APR_SUCCESS)
				{
					// Loop on each files on the cache directory to verify if they are expired and should be removed
			  		if(!strcmp(dirent.name, ".") || !strcmp(dirent.name, "..") || !strcmp(dirent.name, ".cleanup"))
			  		{
			  			continue;
			  		}

					cache_file[0] = 0x00;
					sprintf(cache_file, "%s/%s", vhr->fscache_dir, dirent.name);

			  		mtime = get_mtime(r, cache_file);
			  		if(mtime + vhr->fscache_expire < apr_time_now())
			  		{
			  			/** DELETE FILE **/
			  			apr_file_remove(cache_file, r->pool);
			  			/** DELETE FILE **/
						ap_log_rerror(APLOG_MARK, APLOG_DEBUG, 0, r, "CLEANUP: %s", cache_file);
			  		}
			  	}

				rv = apr_dir_close(dir);
				if(rv != APR_SUCCESS)
				{
					char errbuf[256];
					ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, r, "[VHS::FSCACHE] Could not close the cache directory %s: %s", vhr->fscache_dir, apr_strerror(rv, errbuf, sizeof(errbuf)));
				}

				// Update of the .cleanup file modification time
				rv = apr_file_mtime_set(cleanup_file, apr_time_now(), r->pool);
				if(rv != APR_SUCCESS)
				{
					char errbuf[256];
					ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, r, "[VHS::FSCACHE] Could not update the %s file modification time : %s", cleanup_file, apr_strerror(rv, errbuf, sizeof(errbuf)));
				}
			}

			apr_size_t nbytes = sizeof(data);
			apr_file_write(f, data, &nbytes);
		}

		lock = apr_file_unlock(f);
		if(lock != APR_SUCCESS)
		{
			ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, r, "Cannot release exclusive lock for %s fs_cache_cleanup()", cache_file);
		}
		apr_file_close(f);
	}		
	/** ACQUIRE LOCK **/
}
/** FS_CACHE_CLEANUP **/


/** SET_CACHE **/
void set_cache(request_rec *r, vhs_config_rec *vhr, const char *hostname, mod_vhs_request_t *reqc)
{
#ifdef HAVE_MPM_ITK_SUPPORT
	if (vhr->itk_enable) {
		extern AP_DECLARE_DATA int ap_has_irreversibly_setuid;
		if(ap_has_irreversibly_setuid) {
			ap_log_rerror(APLOG_MARK, APLOG_NOTICE, 0, r, "[VHS::REJECT] Cannot set cache once the setuid() has been done by mpm_itk");
			return;
		}
	}
#endif /* HAVE_MPM_ITK_SUPPORT */

	/** DIRECTORY TRAVERSAL PROTECTION && NULLPOINT DEREFERENCE PROTECTION **/
	if(!hostname || !strlen(hostname) || !is_hostname_valid(hostname))
	{
		ap_log_rerror(APLOG_MARK, APLOG_NOTICE, 0, r, "[VHS::REJECT] Invalid hostname passed to set_cache(): %s", hostname);
		return;
	}
	/** DIRECTORY TRAVERSAL PROTECTION && NULLPOINT DEREFERENCE PROTECTION **/

	fs_cache_cleanup(r, vhr);

	/** PREPARE_CACHE **/
	char cache_file[MAX_CACHE_FILE_LEN];
	apr_status_t lock, rv;
	cache_file[0] = 0x00;
	sprintf(cache_file, "%s/%s", vhr->fscache_dir, hostname);
	/** PREPARE_CACHE **/

	if(reqc == NULL)
	{
		// The VHost is not in the database
		apr_file_t *f;
		rv = apr_file_open(&f, cache_file, APR_FOPEN_CREATE|APR_WRITE, APR_UREAD|APR_UWRITE, r->pool);
		if(rv != APR_SUCCESS)
		{
			char errbuf[256];
			ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, r, "[VHS::FSCACHE] Could not open file %s for write: %s", cache_file, apr_strerror(rv, errbuf, sizeof(errbuf)));
		}
		else
		{
			lock = apr_file_lock(f, APR_FLOCK_EXCLUSIVE);
			if(lock != APR_SUCCESS)
			{
				ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, r, "Cannot acquire exclusive lock for %s on set_cache()", cache_file);
			}
			// The cache file is empty for VHosts that are not on the database
			apr_file_trunc(f, 0);
			lock = apr_file_unlock(f);
			if(lock != APR_SUCCESS)
			{
				ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, r, "Cannot release exclusive lock for %s on set_cache()", cache_file);
			}
			apr_file_close(f);
		}
	}
	else
	{
		/** CACHE_STRUCT **/
		mod_vhs_request_ss sstr;
		sstr.vhost_found = 0;
		memset(sstr.name,0,sizeof(sstr.name));
		memset(sstr.admin,0,sizeof(sstr.admin));
		memset(sstr.docroot,0,sizeof(sstr.docroot));
		memset(sstr.uid,0,sizeof(sstr.uid));
		memset(sstr.gid,0,sizeof(sstr.gid));
		memset(sstr.phpoptions,0,sizeof(sstr.phpoptions));
		memset(sstr.associateddomain,0,sizeof(sstr.associateddomain));
		/** CACHE_STRUCT **/

		sstr.vhost_found = reqc->vhost_found;
		strcpy(sstr.name,reqc->name);
		if(reqc->admin)
		{
			strcpy(sstr.admin,reqc->admin);
		}
		if(reqc->docroot)
		{
			strcpy(sstr.docroot,reqc->docroot);
		}
		if(reqc->uid)
		{
			strcpy(sstr.uid,reqc->uid);
		}
		if(reqc->gid)
		{
			strcpy(sstr.gid,reqc->gid);
		}
		if(reqc->phpoptions)
		{
			strcpy(sstr.phpoptions,reqc->phpoptions);
		}
		if(reqc->associateddomain)
		{
			strcpy(sstr.associateddomain,reqc->associateddomain);
		}

		apr_file_t *f;
		rv = apr_file_open(&f, cache_file, APR_FOPEN_CREATE|APR_WRITE, APR_UREAD|APR_UWRITE, r->pool);
		if(rv != APR_SUCCESS)
		{
			char errbuf[256];
			ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, r, "[VHS::FSCACHE] Could not open file %s for write: %s", cache_file, apr_strerror(rv, errbuf, sizeof(errbuf)));
		}
		else
		{
			lock = apr_file_lock(f, APR_FLOCK_EXCLUSIVE);
			if(lock != APR_SUCCESS)
			{
				ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, r, "Cannot acquire exclusive lock for %s on set_cache()", cache_file);
			}
			apr_size_t nbytes = sizeof(sstr);
			apr_file_write(f, &sstr, &nbytes);
			lock = apr_file_unlock(f);
			if(lock != APR_SUCCESS)
			{
				ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, r, "Cannot release exclusive lock for %s on set_cache()", cache_file);
			}
			apr_file_close(f);
		}
	}
}
/** SET_CACHE **/

/** GET_CACHE **/
int get_cache(request_rec *r, vhs_config_rec *vhr, const char *hostname, mod_vhs_request_t *reqc)
{
#ifdef HAVE_MPM_ITK_SUPPORT
	if (vhr->itk_enable) {
		extern AP_DECLARE_DATA int ap_has_irreversibly_setuid;
		if(ap_has_irreversibly_setuid) {
			ap_log_rerror(APLOG_MARK, APLOG_NOTICE, 0, r, "[VHS::REJECT] Cannot get cache once the setuid() has been done by mpm_itk");
			return FS_CACHE_NO_CACHE;
		}
	}
#endif /* HAVE_MPM_ITK_SUPPORT */

	/** DIRECTORY TRAVERSAL PROTECTION && NULLPOINT DEREFERENCE PROTECTION **/
	if(!hostname || !strlen(hostname) || !is_hostname_valid(hostname))
	{
		ap_log_rerror(APLOG_MARK, APLOG_NOTICE, 0, r, "[VHS::REJECT] Invalid hostname passed to get_cache(): %s", hostname);
		return FS_CACHE_NO_CACHE;
	}
	/** DIRECTORY TRAVERSAL PROTECTION && NULLPOINT DEREFERENCE PROTECTION **/

	/** PREPARE_CACHE **/
	char cache_file[MAX_CACHE_FILE_LEN];
	apr_status_t lock, rv;
	cache_file[0] = 0x00;
	int empty = 0;

	sprintf(cache_file, "%s/%s", vhr->fscache_dir, hostname);
	/** PREPARE_CACHE **/

	apr_time_t mtime = get_mtime(r, cache_file);

	/** CHECK IF AVAILABLE **/
	if(mtime != 0)
	{
		/** CACHE_STRUCT **/
		mod_vhs_request_ss sstr;
		sstr.vhost_found = 0;
		memset(sstr.name,0,sizeof(sstr.name));
		memset(sstr.admin,0,sizeof(sstr.admin));
		memset(sstr.docroot,0,sizeof(sstr.docroot));
		memset(sstr.uid,0,sizeof(sstr.uid));
		memset(sstr.gid,0,sizeof(sstr.gid));
		memset(sstr.phpoptions,0,sizeof(sstr.phpoptions));
		memset(sstr.associateddomain,0,sizeof(sstr.associateddomain));
		/** CACHE_STRUCT **/

		/** CHECK FILE_SIZE **/
		apr_off_t tsize = get_size(r, cache_file);
		if(tsize != 0 && tsize != sizeof(sstr))
		{
			ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, r, "[VHS::FSCACHE] File is invalid: %s", cache_file);
			return FS_CACHE_NO_CACHE;
		}
		/** CHECK FILE_SIZE **/

		apr_file_t *f;
		rv = apr_file_open(&f, cache_file, APR_READ, APR_UREAD|APR_UWRITE, r->pool);
		if(rv != APR_SUCCESS)
		{
			char errbuf[256];
			ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, r, "[VHS::FSCACHE] Could not open file %s for read: %s", cache_file, apr_strerror(rv, errbuf, sizeof(errbuf)));
			return FS_CACHE_NO_CACHE;
		}
		else
		{
			lock = apr_file_lock(f, APR_FLOCK_SHARED);
			if(lock != APR_SUCCESS)
			{
				ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, r, "Cannot acquire shared lock for %s on get_cache()", cache_file);
			}
			apr_size_t nbytes = sizeof(sstr);
			apr_file_read(f, &sstr, &nbytes);
			if(nbytes == 0)
			{
				empty = 1;
			}
			lock = apr_file_unlock(f);
			if(lock != APR_SUCCESS)
			{
				ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, r, "Cannot release shared lock for %s on get_cache()", cache_file);
			}
			apr_file_close(f);
		}

		apr_time_t ct = apr_time_now() - vhr->fscache_timeout;
		if(empty)
		{
			if(mtime < ct)
			{
				// Cache is empty and expired
				return FS_CACHE_NO_CACHE;
			}
			else
			{
				// Cache is empty (eg. VHost not in the database)
				return FS_CACHE_NOT_FOUND;
			}
		}

		reqc->vhost_found = sstr.vhost_found;

		char *r_name;
		char *r_admin;
		char *r_docroot;
		char *r_uid;
		char *r_gid;
		char *r_phpoptions;
		char *r_associateddomain;

		r_name = (char*)malloc(strlen(sstr.name) + 1);
		memset(r_name,0,strlen(sstr.name) + 1);

		r_admin = (char*)malloc(strlen(sstr.admin) + 1);
		memset(r_admin,0,strlen(sstr.admin) + 1);

		r_docroot = (char*)malloc(strlen(sstr.docroot) + 1);
		memset(r_docroot,0,strlen(sstr.docroot) + 1);

		r_uid = (char*)malloc(strlen(sstr.uid) + 1);
		memset(r_uid,0,strlen(sstr.uid) + 1);

		r_gid = (char*)malloc(strlen(sstr.gid) + 1);
		memset(r_gid,0,strlen(sstr.gid) + 1);

		r_phpoptions = (char*)malloc(strlen(sstr.phpoptions) + 1);
		memset(r_phpoptions,0,strlen(sstr.phpoptions) + 1);

		r_associateddomain = (char*)malloc(strlen(sstr.associateddomain) + 1);
		memset(r_associateddomain,0,strlen(sstr.associateddomain) + 1);


		strcpy(r_name,sstr.name);
		strcpy(r_admin,sstr.admin);
		strcpy(r_docroot,sstr.docroot);
		strcpy(r_uid,sstr.uid);
		strcpy(r_gid,sstr.gid);
		strcpy(r_phpoptions,sstr.phpoptions);
		strcpy(r_associateddomain,sstr.associateddomain);

		reqc->name = r_name;
		reqc->admin = r_admin;
		reqc->docroot = r_docroot;
		reqc->uid = r_uid;
		reqc->gid = r_gid;
		reqc->phpoptions = r_phpoptions;
		reqc->associateddomain = r_associateddomain;


		if(mtime < ct)
		{
			return FS_CACHE_EXPIRED;
		}
		else
		{
			return FS_CACHE_FOUND;
		}
	}
	else
	{
		return FS_CACHE_NO_CACHE;
	}
	/** CHECK IF AVAILABLE **/
}
/** GET_CACHE **/
