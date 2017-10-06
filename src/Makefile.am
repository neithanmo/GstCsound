lib_LTLIBRARIES = libgstcsound.la
includedir = /usr/local/include/

libgstcsound_includedir = $(includedir)gstreamer-1.0/gst/csound
libgstcsound_include_HEADERS = \
	gstcsoundsrc.h \
	gstcsoundsink.h \
	gstcsoundfilter.h


# sources used to compile this plug-in
libgstcsound_la_SOURCES = gstcsoundfilter.c plugin.c gstcsoundsrc.c gstcsoundsink.c

# compiler and linker flags used to compile this plugin, set in configure.ac
libgstcsound_la_CFLAGS = $(GST_CFLAGS) $(CSOUND_CFLAGS)
libgstcsound_la_LIBADD = $(GST_LIBS) $(CSOUND_LIBS)
libgstcsound_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS) $(CSOUND_LIBS)
libgstcsound_la_LIBTOOLFLAGS = --tag=disable-static

# headers we need but don't want installed
#noinst_HEADERS = plugin.h
