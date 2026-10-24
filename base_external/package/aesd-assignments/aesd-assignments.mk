################################################################################
#
# aesd-assignments
#
################################################################################

AESD_ASSIGNMENTS_VERSION = 1.0

# Correct path based on your actual directory structure:
# buildroot/
# ../base_external/package/aesd-assignments
AESD_ASSIGNMENTS_SITE = $(TOPDIR)/../base_external/package/aesd-assignments
AESD_ASSIGNMENTS_SITE_METHOD = local

define AESD_ASSIGNMENTS_BUILD_CMDS
    $(MAKE) CC="$(TARGET_CC)" -C $(@D)/server
endef

define AESD_ASSIGNMENTS_INSTALL_TARGET_CMDS
    $(INSTALL) -D -m 0755 $(@D)/server/aesdsocket $(TARGET_DIR)/usr/bin/aesdsocket
    $(INSTALL) -D -m 0755 $(@D)/server/aesdsocket-start-stop \
        $(TARGET_DIR)/etc/init.d/S99aesdsocket
endef

$(eval $(generic-package))
