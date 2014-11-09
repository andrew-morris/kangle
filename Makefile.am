SUBDIRS=src extworker module/webdav 
#module/vhs_mysql module/vhs_sqlite module/whm/whmdso/vhsnode
sysconf_DATA=
EXTRA_DIST=module/access 
wwwdir=$(prefix)/www
sysdir=$(prefix)/webadmin
docdir=$(prefix)/doc
vardir=$(prefix)/var
incdir=$(prefix)/include
etcdir=$(prefix)/etc
dist_sysconf_DATA=$(sysconf_DATA) etc/config.xml 
dist_www_DATA=www/index.html 
dist_sys_DATA=webadmin/kangle.css webadmin/core.whm webadmin/logo.gif
dist_var_DATA=
dist_etc_DATA=etc/lang.xml etc/lang_en.xml etc/lang_zh.xml etc/config-default.xml 
dist_inc_DATA=src/httpext.h src/httpfilt.h src/ksapi.h module/whm/whm.h
#dist_doc_DATA=doc/zh_CN/*.html doc/zh_CN/*.css
install-dist_sysconfDATA:
	@$(NORMAL_INSTALL)
	test -z "$(sysconfdir)" || $(mkdir_p) "$(DESTDIR)$(sysconfdir)"
	@list='$(dist_sysconf_DATA)'; for p in $$list; do \
	  if test -f "$$p"; then d=; else d="$(srcdir)/"; fi; \
	  f=`echo $$p | sed -e 's|^.*/||'`;\
	  if test -f "$(DESTDIR)$(sysconfdir)/$$f" ; then echo "$(DESTDIR)$(sysconfdir)/$$f exsit.."; \
	  else echo "$(INSTALL_DATA) '$$d$$p' '$(DESTDIR)$(sysconfdir)/$$f'"; \
	  $(INSTALL_DATA) "$$d$$p" "$(DESTDIR)$(sysconfdir)/$$f"; fi;\
	done
