################################################################################
# aesdsocket
################################################################################

AESDSOCKET_VERSION = 1.0
AESDSOCKET_SITE = $(TOPDIR)/../assignments-3-and-later-patricereneeemery/server
AESDSOCKET_SITE_METHOD = local

define AESDSOCKET_BUILD_CMDS
    $(MAKE) -C $(@D)
endef

define AESDSOCKET_INSTALL_TARGET_CMDS
    $(INSTALL) -D -m 0755 $(@D)/aesdsocket $(TARGET_DIR)/usr/bin/aesdsocket
    $(INSTALL) -D -m 0755 $(AESDSOCKET_SITE)/aesdsocket-start-stop \
        $(TARGET_DIR)/etc/init.d/S99aesdsocket
endef

$(eval $(generic-package))
