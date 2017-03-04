# Note: plugindir is set in configure

##############################################################################
# TODO: change libgstplugin.la to something else, e.g. libmysomething.la     #
##############################################################################
plugin_LTLIBRARIES = libgstcsound.la

##############################################################################
# TODO: for the next set of variables, name the prefix if you named the .la, #
#  e.g. libmysomething.la => libmysomething_la_SOURCES                       #
#                            libmysomething_la_CFLAGS                        #
#                            libmysomething_la_LIBADD                        #
#                            libmysomething_la_LDFLAGS                       #
##############################################################################

## Plugin 1

# sources used to compile this plug-in
libgstcsound_la_SOURCES = gstcsoundfilter.c plugin.c gstcsoundfilter.h gstcsoundsrc.c gstcsoundsrc.h gstcsoundsink.c gstcsoundsink.h

# compiler and linker flags used to compile this plugin, set in configure.ac
libgstcsound_la_CFLAGS = $(GST_CFLAGS) $(CSOUND_CFLAGS)
libgstcsound_la_LIBADD = $(GST_LIBS) $(CSOUND_LIBS)
libgstcsound_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS) $(CSOUND_LIBS)
libgstcsound_la_LIBTOOLFLAGS = --tag=disable-static

## Plugin 2 (audio filter example)


# headers we need but don't want installed
noinst_HEADERS = gstcsoundfilter.h gstcsoundsrc.h gstcsoundsink.h
