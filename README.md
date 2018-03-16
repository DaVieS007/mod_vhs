# mod_vhs 1.2 / *(mod_vhs)* - Virtual Hosting module
## This is a fork from mod_vhs 1.1 (abandoned) by **Xavier Beaudouin**

### mod_vhs is an Apache 2.4 (apache2-mpm-itk) module which allows dynamically serving content without any file-based configuration.

### Special Thanks for Jean Weisbuch who actively maintained this module

#### Configuration Variables

 - **EnableVHS**: Enable or Disable mod_vhs. Per default VHS is NOT enabled.

 - **vhs_LogNotFound:** Enable or Disable logging into error log when mod_vhs cannot find the hostname in database.
	Per default this option is disabled.

 - **vhs_Path_Prefix:** Sets an optional location to prefix translations by. 
	This option is not required.

 - **vhs_Default_Host:** Sets the default host to use if a non-HTTP/1.1 request was received. 
	This option is not required and usually won't do anything because the Apache Web Server by default catches these errors.
	Since we use internaly HTTP Rediect you MUST use an URI.

 - **vhs_Lamer:** boolean (On / Off), default to Off.
	If set, this will enable "lamer mode" option to allow people that always add an "www." to web 
	addresses to be magicaly handled by this module.

 - **vhs_PHPsafe_mode:** boolean (On / Off), default Off.
	If set, and if you have compiled mod_vhs with PHP
	support it will set safe_mode to on for the 
	virtual host otherwise php.ini defaults will be used.
	Notice this code is added only uppon compilation the -DOLD_PHP.
	This code is not maintained anymore.

 - **vhs_PHPopen_basedir:** boolean (On / Off), default Off.
	If set, and if you have compiled mod_vhs with PHP
	support, it will set open_basedir to the home 
	directory given by libhome/mod_vhs otherwise php.ini
	defaults will be used.

 - **vhs_open_basedir_path:** string. The open_basedir path that will be used to
	be set when vhs_PHPopen_basedir is set AND vhs_append_open_basedir.

 - **vhs_append_open_basedir:** boolean (On / Off), default Off.
	Is set, mod_vhs will append the homedir path intovhs_open_basedir_path.
	NOTE :	mod_vhs open_basedir option will OVERRIDE php.ini settings.

 - **vhs_PHPdisplay_errors:** boolean (On / Off), default Off.
	If set, and if you have compiled mod_vhs with PHP
	support, it will enable display_errors to be shown,
	otherwise php.ini defaults will be used.

 - **vhs_PHPopt_fromdb:** boolean (On / Off), default Off,
	If set, and if you have compiled mod_vhs with PHP
	support, it will alter PHP configuration using
	libhome passwd unused field. See README.phpopt
	for more detailed example.

vhs_Alias, vhs_ScriptAlias, vhs_ScriptAliasMatch, vhs_RedirectTemp and
vhs_RedirectPermanent :	mod_alias 100% compatible options inside mod_vhs.
Please see http://httpd.apache.org/docs-2.0/mod_alias.html
for more informations.

mod_ldap and mod_dbd stuff
Since version 1.1.0 or newer, we stop tu use libhome as backend
and we use internal Apache stuff.
For LDAP we use mod_ldap that have internal caching system
For SQL we use mod_dbd
Because of these change, mod_vhs 1.1.0 is compatible ONLY
with Apache 2.2.x

LDAP Only directives (only readed when mod_vhs builded with LDAP)
See README.LDAP for detailed informations.

vhs_LDAPBindDN
vhs_LDAPBindPassword
An optional DN used to bind to the server when searching for
entries. If not provied, mod_vhs will use an anonymous bind.

An bind password to use in conjonction with the bind DN. Note
that the bind password is probably sensitive data, and should be
properly protected. You should only use the vhs_LDAPBindDN and
vhs_LDAPBindPassword if you absolutely need them to search the 
directory

vhs_LDAPDereferenceAliases
This directive specifies when mod_vhs will de-references aliases
during LDAP operations. The default is always.

vhs_LDAPUrl
A RFC 2255 URL which specifies the LDAP search parameters to use.
The syntax of the URL is :
ldap://host:port/basedn?attribute?scope?filter

ldap
For regular ldap, use the string ldap. For secure LDAP, use
ldaps instead. Secure LDAP is only available is Apache was
linked to an LDAP library with SSL support

host:port
The name/port of the LDAP server (default to localhost:389 for
ldap, and localhost:636 for ldaps). To specify multiple, redundant 
LDAP server, just list all servers, separated by spaces. mod_vhs
will try connecting to each servers in turn, until it make a 
successfull connection.

Once a connection has been made to server, that connection remains
active for the life of the httpd process, or until the LDAP server
goes down.

If the LDAP server goes down and break the existing connection,
mod_vhs will attends to re-connect, starting with the primary 
server, and trying each redundant server in turn. Note that is
different than a true round-robin search.

basedn
The DN of the branch of the directory where all searches start from.
At the very last, this must be the top of you directory tree, but
could also specifiy a subtree in the directory.

attribue
The attribute to search for. Don't change search attribute in
mod_vhs or you will break the module.

scope
The scope of the search. Can be either "one" or "sub". Not that
scope of "base" is also supported by RFC 2255, but not supported
in this module. If the scope is not provided, or if "base" scope
is specified, the default is to use a scope of "sub".

filter
A valid LDAP search filer. If not provided, default to 
(|(apacheServername=vhost)(apacheServerAlias=vhost)). Filter are
limited to approximately 8000 caracters (the definition of
MAX_STRING_LEN in the Apache source code). This should be
more than sufficient for any application. Don't changer filter
unless you know what your are doing.

vhs_LDAPSetFilter	Use the filter given ing vhs_LDAPUrl instead of the compiled one.
See README.LDAP for more informations


mod_dbd support (when compiled with DBD support)

vhs_DBD_SQL_Label	The name used for the DBDPrepareSQL label to get data from DBD.
mod_vhs use the following SQL column names to get it's values.

ServerName -> "ServerName" of "VirtualHost" e.g. foo.bar
ServerAdmin -> "ServerAdmin" of "VirtualHost" e.g. webmaster@foo.bar
DocumentRoot -> "DocumentRoot" of "VirtualHost" e.g. var/www/htdocs
suexec_uid -> user ID (UID) either for apache2-mpm-itk or suPHP
suexec_gid -> group ID (GID) either for apache2-mpm-itk or suPHP
php_env -> PHP options e.g. open_basedir=/tmp/:/var/www/;upload_tmp_dir=/var/www/tmp;session.save_path=/var/www/tmp;upload_max_filesize=200M;post_max_size=200M;
associateddomain -> the value of VH_GECOS HTTP header, normally this can be the "ServerName"

A sample query: 
SELECT ServerName, 
ServerAdmin, 
DocumentRoot, 
suexec_uid, 
suexec_gid, 
php_env, 
associateddomain
FROM mod_vhs_table 
WHERE ServerName = %s 

If you have different column names you can use something like this:
SELECT vHost AS ServerName, 
admin AS ServerAdmin, 
docRoot AS DocumentRoot, 
uidNumber AS suexec_uid, 
gidNumber AS suexec_gid, 
SetEnv AS php_env,
vHost AS associateddomain
FROM my_vhost_table 
WHERE vHost = %s 
AND IsDeactivated = '0'

Note: "%s" is the hostname requested by the client


apache2-mpm-itk support (when compiled with apache2-mpm-itk support)

vhs_itk_enable: boolean (On / Off), default Off.	
When on, it changes UID/GID to the one given either on LDAP or DBD


memcached support (when compiled with memcached support)

vhs_Memcached: boolean (On / Off), default Off.
When on, results from DBD/LDAP requests are cached.

vhs_MemcachedInstance: string. Name of the mod_vhs instance.
Use a short, but descriptive name. e.g. super-server

vhs_MemcachedLifeTime: interger. Lifetime (in seconds) associated with stored
objects in memcache daemon(s). Default is 600 s.


Notes about vhs_PHP* values :

All this options *will* override the default PHP values you have set into php.ini. 

Home page of this module: https://devel.npulse.net/npulse-public/mod_vhs