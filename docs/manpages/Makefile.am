if !CROSS_COMPILING

man_MANS = \
    cf-agent.8 \
    cf-execd.8 \
    cf-key.8 \
    cf-know.8 \
    cf-monitord.8 \
    cf-promises.8 \
    cf-report.8 \
    cf-runagent.8 \
    cf-serverd.8

if HAVE_NOVA
if MONGO
  man_MANS += cf-hub.8
endif
endif

endif

.PRECIOUS: $(top_builddir)/src/%

$(top_builddir)/src/%:
	$(MAKE) -C $(top_builddir)/src $(AM_MAKEFLAGS) $<

%.8 : $(top_builddir)/src/%
	$(AM_V_GEN)$< -M > $@
