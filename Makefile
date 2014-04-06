
CFLAGS = -g3 -fPIC -Wall

all_DEPS = \
	libnsdp.a \
	nsdp_client \

nsdp_client_DEPS = \
	nsdp_client.o \
	libnsdp.a \

nsdp_client_LDFLAGS = \
	-L. \

nsdp_client_LIBS = \
	-lnsdp -levent \

libnsdp.a_DEPS = \
	nsdp_socket_posix.o \
	nsdp_iface_sysfs.o \
	nsdp_packet.o \
	nsdp_property.o \
	nsdp_property_types.o \
	nsdp_properties.o \

all: $(all_DEPS)

clean:
	rm -f *.o *.so

all_FLAGS = CPPFLAGS CFLAGS CXXFLAGS LDFLAGS LIBS

define goal_flag
$(1) $($(1)_DEPS): $(2) += $($(1)_$(2))
endef

define goal
$(1): $($(1)_DEPS) Makefile
$(foreach flag,$(all_FLAGS),$(eval $(call goal_flag,$(1),$(flag))))
endef

$(foreach dep,$(all_DEPS),$(eval $(call goal,$(dep))))

%.o: %.c Makefile
	$(CROSS_COMPILE)$(CC) -o $@ -c $< $(CPPFLAGS) $(CFLAGS)

%.so:
	$(CROSS_COMPILE)$(CC) -o $@ -shared $(filter %.o,$($*.so_DEPS)) $(LDFLAGS) $(LIBS)

%.a:
	rm -f $@
	$(CROSS_COMPILE)$(AR) rs $@ $(filter %.o,$($*.a_DEPS))

%:
	$(CROSS_COMPILE)$(CC) -o $@ $(filter %.o,$($*_DEPS)) $(LDFLAGS) $(LIBS)

.PHONY: all clean
