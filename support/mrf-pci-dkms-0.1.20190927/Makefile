#/usr/bin/make
SRC = $(DESTDIR)/usr/src
SHARE = $(DESTDIR)/usr/share/$(NAME)-dkms
UDEV_RULES = $(DESTDIR)/lib/udev/rules.d

all:

clean:

install:

#source tree
ifeq ("$(wildcard $(NAME)-$(VERSION))", "$(NAME)-$(VERSION)")
	install -d "$(SRC)"
	cp -a $(NAME)-$(VERSION) $(SRC)
	chmod 644 -R "$(SRC)/$(NAME)-$(VERSION)"
endif

#tarball, possibly with binaries
ifeq ("$(wildcard $(NAME)-$(VERSION).dkms.tar.gz)", "$(NAME)-$(VERSION).dkms.tar.gz")
	install -d "$(SHARE)"
	install -m 644 $(NAME)-$(VERSION).dkms.tar.gz "$(SHARE)"
endif

#udev rules
ifeq ("$(wildcard $(NAME)-$(VERSION))/60-mrf-pci.rules", "$(NAME)-$(VERSION)/60-mrf-pci.rules")
	install -d "$(UDEV_RULES)"
	install -m 644 "$(NAME)-$(VERSION)/60-mrf-pci.rules" "$(UDEV_RULES)"
endif

#postinst, only if we are supporting legacy mode
ifeq ("$(wildcard common.postinst)", "common.postinst")
	install -d "$(SHARE)"
	install -m 755 $(PREFIX)/usr/lib/dkms/common.postinst $(SHARE)/postinst
endif
