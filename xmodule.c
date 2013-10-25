#include <xorg-server.h>
#include <xf86Module.h>
#include <stdlib.h>
#include <xf86.h>
#include <scrnintstr.h>
#include <string.h>

static XF86ModuleVersionInfo vgaswitch_version_info =
{
	"vgaswitch",
	MODULEVENDORSTRING,
	MODINFOSTRING1,
	MODINFOSTRING2,
	XORG_VERSION_CURRENT,
	0, 0, 1,
	ABI_CLASS_EXTENSION,
	ABI_EXTENSION_VERSION,
	MOD_CLASS_EXTENSION,
	{0, 0, 0, 0}
};

struct vgaswitch_dev {
	GDevRec base;
	xf86EnterVTProc *savedEnterVT;
	CloseScreenProcPtr savedCloseScreen;
};
struct vgaswitch_dev vgaswitch_fake_dev;
extern _X_EXPORT int xf86NumScreens;
extern _X_EXPORT int xf86NumGPUScreens;
extern _X_EXPORT ScrnInfoPtr *xf86GPUScreens;

static Bool vgaswitch_enter_vt(ScrnInfoPtr info)
{
	system("/sbin/vga_switch -g start");
	info->EnterVT = vgaswitch_fake_dev.savedEnterVT;
	Bool r = info->EnterVT(info);
	vgaswitch_fake_dev.savedEnterVT = info->EnterVT;
	info->EnterVT = vgaswitch_enter_vt;
	system("/sbin/vga_switch -g stop");
	return r;
}

static Bool vgaswitch_close_screen(ScreenPtr pScreen)
{
	system("/sbin/vga_switch -g start");
	pScreen->CloseScreen = vgaswitch_fake_dev.savedCloseScreen;
	Bool r = pScreen->CloseScreen(pScreen);
	vgaswitch_fake_dev.savedCloseScreen = pScreen->CloseScreen;
	pScreen->CloseScreen = vgaswitch_close_screen;
	system("/bin/sh -c 'sleep 15; /sbin/vga_switch -g stop;' &");
	return r;
}
static void vgaswitch_entity_init(pointer data, OSTimePtr timeout, pointer readmask)
{
	(void)timeout;(void)readmask;(void)data;

	int i;

	for (i = 0; i < screenInfo.numScreens; i++) {
		ScreenPtr pScreen = screenInfo.screens[i];
		ScrnInfoPtr scrn = xf86ScreenToScrn(pScreen);
		if (strcmp("radeon", scrn->driverName) == 0) {
			vgaswitch_fake_dev.savedEnterVT = scrn->EnterVT;
			scrn->EnterVT = vgaswitch_enter_vt;

			vgaswitch_fake_dev.savedCloseScreen = scrn->pScreen->CloseScreen;
			scrn->pScreen->CloseScreen = vgaswitch_close_screen;
		}
	}

	for (i = 0; i < screenInfo.numGPUScreens; i++) {
		ScreenPtr pScreen = screenInfo.gpuscreens[i];
		ScrnInfoPtr scrn = xf86ScreenToScrn(pScreen);
		if (strcmp("radeon", scrn->driverName) == 0) {
			vgaswitch_fake_dev.savedEnterVT = scrn->EnterVT;
			scrn->EnterVT = vgaswitch_enter_vt;

			vgaswitch_fake_dev.savedCloseScreen = scrn->pScreen->CloseScreen;
			scrn->pScreen->CloseScreen = vgaswitch_close_screen;
		}
	}

	RemoveBlockAndWakeupHandlers(vgaswitch_entity_init, NULL, NULL);

}

static void vgaswitch_claim_entity(void)
{
	RegisterBlockAndWakeupHandlers(vgaswitch_entity_init, NULL, NULL);
}

extern _X_EXPORT void xf86BusProbe(void);

static pointer vgaswitch_setup(pointer Module, pointer Options, int *major, int *minor)
{
	(void)Module; (void)Options; (void)major; (void)minor;
	static Bool loaded = FALSE;

	if (!loaded)
	{
		loaded = TRUE;
		system("/sbin/vga_switch -g start");
		xf86BusProbe();
		vgaswitch_claim_entity();
		system("/bin/sh -c 'sleep 60; /sbin/vga_switch -g stop;' &");
	}

	return (pointer)1;
}

_X_EXPORT XF86ModuleData vgaswitchModuleData =
{
	&vgaswitch_version_info,
	vgaswitch_setup,
	NULL
};
