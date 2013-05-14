if !CROSS_COMPILING

man_MANS = \
    cf-agent.8 \
    cf-execd.8 \
    cf-key.8 \
    cf-monitord.8 \
    cf-promises.8 \
    cf-runagent.8 \
    cf-serverd.8

#if HAVE_NOVA
#if MONGO
#  man_MANS += cf-hub.8
#endif
#endif

endif

.PRECIOUS: ../../%/%

../../%/.doc-stamp:
	echo $@
	$(MAKE) -C $(dir $@) $(AM_MAKEFLAGS)
	touch $@

%.8 : ../../%/.doc-stamp
	$(AM_V_GEN)$(dir $<)$(notdir $(patsubst %/,%,$(dir $<))) -M > $@
