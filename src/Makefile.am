include $(top_srcdir)/Makefile.shared

bin_PROGRAMS=					\
	nautilus				\
	nautilus-autorun-software		\
	nautilus-connect-server			\
	$(NULL)

libexec_PROGRAMS=				\
	nautilus-convert-metadata		\
	$(NULL)

INCLUDES =							\
	-I$(top_srcdir) 					\
	-I$(top_srcdir)/cut-n-paste-code			\
	-I$(top_builddir)/libnautilus-private 			\
	$(BASE_CFLAGS) 						\
	$(COMMON_CFLAGS)					\
	$(NAUTILUS_CFLAGS)					\
	$(WARNING_CFLAGS)					\
	$(EXIF_CFLAGS)						\
	$(EXEMPI_CFLAGS)                                        \
	-DDATADIR=\""$(datadir)"\" 				\
	-DLIBDIR=\""$(libdir)"\" 				\
	-DLIBEXECDIR=\""$(libexecdir)"\" 			\
	-DNAUTILUS_DATADIR=\""$(datadir)/nautilus"\" 		\
	-DUIDIR=\""$(datadir)/nautilus/ui"\" 		\
	-DNAUTILUS_PIXMAPDIR=\""$(datadir)/pixmaps/nautilus"\" 	\
	-DPREFIX=\""$(prefix)"\" 				\
	-DSYSCONFDIR=\""$(sysconfdir)"\" 			\
	-DVERSION="\"$(VERSION)\"" 				\
	$(DISABLE_DEPRECATED_CFLAGS)                            \
	$(NULL)

LDADD =\
	$(top_builddir)/libnautilus-private/libnautilus-private.la  \
	$(BASE_LIBS) \
	$(COMMON_LIBS) \
	$(NAUTILUS_LIBS) \
	$(CORE_LIBS) \
	$(EXIF_LIBS) \
	$(EXEMPI_LIBS) \
	$(POPT_LIBS) \
	$(NULL)

BUILT_SOURCES = \
	nautilus-src-marshal.c \
	nautilus-src-marshal.h \
	$(NULL)

nautilus_SOURCES = \
	nautilus-actions.h			\
	nautilus-application.c			\
	nautilus-application.h			\
	nautilus-audio-mime-types.h		\
	nautilus-bookmark-list.c		\
	nautilus-bookmark-list.h		\
	nautilus-bookmarks-window.c		\
	nautilus-bookmarks-window.h		\
	nautilus-connect-server-dialog.c	\
	nautilus-connect-server-dialog.h	\
	nautilus-connect-server-dialog-nonmain.c \
	nautilus-connect-server-operation.c	\
	nautilus-connect-server-operation.h	\
	nautilus-desktop-icon-view.c		\
	nautilus-desktop-icon-view.h		\
	nautilus-desktop-item-properties.c	\
	nautilus-desktop-item-properties.h	\
	nautilus-desktop-window.c		\
	nautilus-desktop-window.h		\
	nautilus-error-reporting.c		\
	nautilus-error-reporting.h		\
	nautilus-file-management-properties.c	\
	nautilus-file-management-properties.h	\
	nautilus-floating-bar.c			\
	nautilus-floating-bar.h			\
	nautilus-icon-view.c			\
	nautilus-icon-view.h			\
	nautilus-icon-view-container.c		\
	nautilus-icon-view-container.h		\
	nautilus-image-properties-page.c	\
	nautilus-image-properties-page.h	\
	nautilus-list-model.c			\
	nautilus-list-model.h			\
	nautilus-list-view-private.h 		\
	nautilus-list-view.c			\
	nautilus-list-view.h 			\
	nautilus-location-bar.c         	\
	nautilus-location-bar.h			\
	nautilus-location-entry.c               \
	nautilus-location-entry.h               \
	nautilus-main.c 			\
	nautilus-mime-actions.c 		\
	nautilus-mime-actions.h 		\
	nautilus-navigation-action.c		\
	nautilus-navigation-action.h		\
	nautilus-navigation-state.c		\
	nautilus-navigation-state.h		\
	nautilus-notebook.c			\
	nautilus-notebook.h			\
	nautilus-pathbar.c			\
	nautilus-pathbar.h			\
	nautilus-places-sidebar.c		\
	nautilus-places-sidebar.h		\
	nautilus-previewer.c			\
	nautilus-previewer.h			\
	nautilus-progress-info-widget.c		\
	nautilus-progress-info-widget.h		\
	nautilus-progress-ui-handler.c		\
	nautilus-progress-ui-handler.h		\
	nautilus-properties-window.c		\
	nautilus-properties-window.h		\
	nautilus-query-editor.c			\
	nautilus-query-editor.h			\
	nautilus-search-bar.c			\
	nautilus-search-bar.h			\
	nautilus-self-check-functions.c 	\
	nautilus-self-check-functions.h 	\
	nautilus-toolbar.c			\
	nautilus-toolbar.h			\
	nautilus-tree-sidebar.c			\
	nautilus-tree-sidebar.h			\
	nautilus-tree-sidebar-model.c		\
	nautilus-tree-sidebar-model.h		\
	nautilus-trash-bar.c			\
	nautilus-trash-bar.h			\
	nautilus-view.c				\
	nautilus-view.h				\
	nautilus-view-dnd.c			\
	nautilus-view-dnd.h			\
	nautilus-view-factory.c 		\
	nautilus-view-factory.h 		\
	nautilus-window-bookmarks.c		\
	nautilus-window-bookmarks.h		\
	nautilus-window-manage-views.c		\
	nautilus-window-manage-views.h		\
	nautilus-window-menus.c         	\
	nautilus-window-pane.c	\
	nautilus-window-pane.h	\
	nautilus-window-private.h		\
	nautilus-window-slot.c			\
	nautilus-window-slot.h			\
	nautilus-window-slot-dnd.c		\
	nautilus-window-slot-dnd.h		\
	nautilus-window.c			\
	nautilus-window.h			\
	nautilus-window-types.h			\
	nautilus-x-content-bar.c		\
	nautilus-x-content-bar.h		\
	$(NULL)

EMPTY_VIEW_SOURCES = \
	nautilus-empty-view.c 			\
	nautilus-empty-view.h

if ENABLE_EMPTY_VIEW
nautilus_SOURCES += $(EMPTY_VIEW_SOURCES)
endif

nodist_nautilus_SOURCES = \
	$(BUILT_SOURCES) \
	$(NULL)

nautilus-src-marshal.list: $(nautilus_SOURCES) Makefile.am
	$(AM_V_GEN)( cd $(srcdir) && \
	sed -n -e 's/.*nautilus_src_marshal_\([[:upper:][:digit:]]*__[[:upper:][:digit:]_]*\).*/\1/p' \
	$(nautilus_SOURCES) ) \
	| sed -e 's/__/:/' -e 'y/_/,/' | sort -u > $@.tmp
	@if cmp -s $@.tmp $@; then \
		rm $@.tmp; \
	else \
		mv $@.tmp $@; \
	fi

%-marshal.c: %-marshal.list Makefile
	$(AM_V_GEN)echo "#include \"nautilus-src-marshal.h\"" > $@ && \
	$(GLIB_GENMARSHAL) --body --prefix=$(subst -,_,$*)_marshal $< >> $*-marshal.c

%-marshal.h: %-marshal.list Makefile
	$(AM_V_GEN)$(GLIB_GENMARSHAL) --header --prefix=$(subst -,_,$*)_marshal $< > $*-marshal.h

nautilus_autorun_software_SOURCES= 			\
	nautilus-autorun-software.c			\
	$(NULL)

nautilus_connect_server_SOURCES= \
	nautilus-bookmark-list.c		\
	nautilus-bookmark-list.h		\
	nautilus-connect-server-dialog.c	\
	nautilus-connect-server-dialog.h	\
	nautilus-connect-server-dialog-main.c	\
	nautilus-connect-server-operation.c     \
	nautilus-connect-server-operation.h     \
	$(NULL)

nautilus_convert_metadata_SOURCES= \
	nautilus-convert-metadata.c	\
	$(NULL)

TESTS=check-nautilus

@INTLTOOL_SERVER_RULE@

uidir = $(datadir)/nautilus/ui
ui_DATA = 					\
	nautilus-shell-ui.xml			\
	nautilus-file-management-properties.ui	\
	nautilus-bookmarks-window.ui		\
	nautilus-desktop-icon-view-ui.xml	\
	nautilus-directory-view-ui.xml		\
	nautilus-icon-view-ui.xml		\
	nautilus-list-view-ui.xml		\
	nautilus-toolbar-ui.xml			\
	$(NULL)

CLEANFILES = \
	nautilus-src-marshal.list		\
	$(BUILT_SOURCES)			\
	$(server_DATA)				\
	$(NULL)

EXTRA_DIST =					\
	nautilus-src-marshal.list		\
	$(server_in_files)			\
	$(ui_DATA)				\
	check-nautilus				\
	$(NULL)

dist-hook:
	cd $(distdir); rm -f $(CLEANFILES)
