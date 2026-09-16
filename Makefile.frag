# Expand JS includes before EM_JS stringification, without expanding JS identifiers
# as C macros. PHP substitutes srcdir/builddir when installing this fragment.
VRZNO_JS_UNITS = vrzno vrzno_array vrzno_fetch vrzno_functions vrzno_object
VRZNO_JS_HEADERS = $(addprefix $(builddir)/generated/,$(addsuffix _js.h,$(VRZNO_JS_UNITS)))
VRZNO_JS_DEPS = $(VRZNO_JS_HEADERS:.h=.d)

define VRZNO_JS_OBJECT_RULE
$(builddir)/$(1).lo: $(builddir)/generated/$(1)_js.h
endef
$(foreach unit,$(VRZNO_JS_UNITS),$(eval $(call VRZNO_JS_OBJECT_RULE,$(unit))))

# Group both outputs to recover lost dependency files and serialize parallel users.
$(builddir)/generated/%_js.h $(builddir)/generated/%_js.d &: $(srcdir)/%_js.h.in $(srcdir)/Makefile.frag
	@mkdir -p "$(builddir)/generated"
	@set -eu; \
		header="$(builddir)/generated/$*_js.h"; \
		deps="$(builddir)/generated/$*_js.d"; \
		trap 'rm -f "$$header.tmp" "$$deps.tmp"' 0 1 2 3 15; \
		$(CC) -E -P -CC -fdirectives-only -x c \
			-MMD -MP -MF "$$deps.tmp" -MQ "$$header" -MQ "$$deps" \
			"$<" -o "$$header.tmp"; \
		mv "$$header.tmp" "$$header"; \
		mv "$$deps.tmp" "$$deps"

# Cleaning must work without JS inputs or an installed compiler.
ifeq ($(filter clean distclean clean-vrzno-js,$(MAKECMDGOALS)),)
include $(VRZNO_JS_DEPS)
endif

.PHONY: clean-vrzno-js
clean: clean-vrzno-js
clean-vrzno-js:
	rm -f $(VRZNO_JS_HEADERS) $(VRZNO_JS_DEPS) $(addsuffix .tmp,$(VRZNO_JS_HEADERS) $(VRZNO_JS_DEPS))
