# FreeSWITCH mod_ringback - 在 FreeSWITCH 源码树中编译
# 将此目录复制到 FreeSWITCH 的 src/mod/applications/ 下
# 并在 modules.conf 中添加 applications/mod_ringback

include $(switch_srcdir)/build/modmake.rules

LOCAL_CFLAGS += -I.
LOCAL_LDFLAGS += -lm

LOCAL_OBJS = mod_ringback.lo

mod_ringback.la: $(LOCAL_OBJS)
	$(LINK) -rpath $(libdir) -module -avoid-version -no-undefined $(LOCAL_OBJS) -lm
	$(CORE_APP_LINK)
