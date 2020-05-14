#include "openbsd_compat_config.h"

#include <string.h> // strcmp
#include <sys/errno.h> // ENOTSUP
#include <IOKit/IOLib.h> // IOMalloc, IOFree

#include "sdmmcvar.h" // sdmmc_attach_args, sdmmc_softc_original

#define UTL_THIS_CLASS ""
#include "util.h" // UTL_CHK_PTR, UTL_ERR

// See: https://github.com/openbsd/src/blob/master/sys/kern/subr_autoconf.c

extern struct cfattach sdmmc_ca;
extern struct cfdriver sdmmc_cd;
extern struct cfdriver rtsx_cd;

/// Array of cfdata to be matched
/// TODO: should be this named cfdata as defined in device.h?
static struct cfdata my_cfdata[2] = {
	{ &sdmmc_ca, &sdmmc_cd }, // index 0 must be sdmmc (see config_activate_chilren())
	//{ nullptr, &rtsx_cd },
};

/// Performs indirect configuration of physical devices by iterating over all potential children, calling the given
/// function func for each one.
void *config_search(cfmatch_t fn, struct device *parent, void *aux)
{
	for (int i = 0; i < sizeof(my_cfdata) / sizeof(my_cfdata[0]); i++) {
		UTL_DEBUG_LOOP("Trying my_cfdata[%d]...", i);
		// cf_attach may be null
		if (!my_cfdata[i].cf_attach)
			continue;
		UTL_DEBUG_LOOP("Calling my_cfdata[%d].ca_match()...", i);
		if (my_cfdata[i].cf_attach->ca_match(parent, my_cfdata + i, aux)) {
			// return first match found
			return &my_cfdata[i];
		}

	}
	return nullptr;
}

/**
 * Attach a device (allocate memory for device)
 */
struct device *config_attach(struct device *parent, void *match, void *aux, cfprint_t print) {
	struct device *ret;
	struct cfdata *this_cfdata = (struct cfdata *) match;
	UTL_CHK_PTR(this_cfdata, nullptr);
	UTL_CHK_PTR(this_cfdata->cf_attach, nullptr);

	int32_t devSize = (int32_t) this_cfdata->cf_attach->ca_devsize;
	// allocate memory (TODO: check that it is freed on detach)
	UTL_DEBUG_MEM("Allocating %d bytes...", devSize);
	ret = (struct device *) IOMalloc(devSize);
	UTL_CHK_PTR(ret, nullptr);
	UTL_DEBUG_MEM("Memory allocated => " RTSX_PTR_FMT, RTSX_PTR_FMT_VAR(ret));
	bzero(ret, devSize);
	ret->dv_flags = DVF_ACTIVE; // used by deactivate

	// copy name (the only used member?)
	UTL_CHK_PTR(this_cfdata->cf_driver, nullptr);
	UTL_CHK_PTR(this_cfdata->cf_driver->cd_name, nullptr);

	// set dv_xname
	strlcpy(ret->dv_xname, this_cfdata->cf_driver->cd_name , sizeof(ret->dv_xname));
	ret->dv_cfdata = this_cfdata; // used by config_detach
	ret->dv_parent = parent; // parent device (is it used?)

	// call attach (and activate?)
	UTL_CHK_PTR(this_cfdata->cf_attach->ca_attach, nullptr);
	UTL_DEBUG_FUN("Calling ca_attach...");
	this_cfdata->cf_attach->ca_attach(parent, (struct device *) ret, aux);

	return (struct device *) ret;
}

/// This function is called from:
///  - rtsx_attach, passing a struct sdmmcbus_attach_args as aux argument
///  - sdmmc_io_attach, passing a struct sdmmc_attach_args as aux argument,
///                             a print function, and
///                             a submatch function (sdmmc_submatch)
struct device *config_found_sm(struct device *parent, void *aux, cfprint_t print, cfmatch_t submatch)
{
	UTL_DEBUG_FUN("START");

	UTL_CHK_PTR(parent, nullptr);

	if (strncmp(parent->dv_xname, "sdmmc", sizeof(parent->dv_xname)) == 0) {
		UTL_ERR("sdmmc should not be calling this (io funcion attached?)");
	}
	// This is very similar to OpenBSD code:
	auto match = (struct cfdata *) config_search(submatch, parent, aux);
	if (match) {
		UTL_DEBUG_DEF("Match found. Calling config_attach...");
		return config_attach(parent, match, aux, print);
	} else {
		UTL_ERR("Config not found. Returning null.");
		return nullptr;
	}
}

/// Calls deactivate, detach, and frees memory
int config_detach(struct device *dev, int flags)
{
	int ret = 0;
	UTL_CHK_PTR(dev, EINVAL);
	UTL_CHK_PTR(dev->dv_cfdata, EINVAL);
	UTL_CHK_PTR(dev->dv_cfdata->cf_attach, EINVAL);

	struct cfattach *cf_attach = dev->dv_cfdata->cf_attach;

	UTL_DEBUG_DEF("Detaching %s device", dev->dv_xname);

	// call deactivate
	ret = config_deactivate(dev);
	if (ret == 0) {
		// call detach
		if (cf_attach && cf_attach->ca_detach) {
			UTL_DEBUG_FUN("Calling detach function...");
			ret = cf_attach->ca_detach(dev, flags);
			UTL_DEBUG_FUN("Detach function returns");
		} else {
			UTL_ERR("Null detach function not supported yet!");
			ret = ENOTSUP;
		}
	}

	// TODO: Check OpenBSD code. It does more processing here...
	
	uint32_t devSize = (uint32_t) cf_attach->ca_devsize;
	UTL_DEBUG_MEM("Freeing %d bytes at " RTSX_PTR_FMT, devSize, RTSX_PTR_FMT_VAR(dev));
	IOFree(dev, devSize);
	return ret;
}

int config_activate_children(struct device *dev, int act) {
	// See subr_autoconf for implementation (not difficult, but do not need a complex implementation)
	// The best would be to implement dv_list, but for now we don't need it
	// For our case, calling activate() on sdmmc when rtsx_softc calls this method is enough
	UTL_CHK_PTR(dev, EINVAL);

	if (strcmp(dev->dv_xname, "rtsx") == 0) {
		auto sdmmc = (struct sdmmc_softc *) ((struct rtsx_softc *)dev)->sdmmc;
		if (!sdmmc)
			return ENOTSUP;
		// activate sdmmc!
		// TODO: This is hardcoded... change!
		auto activate = my_cfdata[0].cf_attach->ca_activate;
		if (activate) {
			activate(&sdmmc->sc_dev, act);
		} else {
			config_activate_children(&sdmmc->sc_dev, act);
		}
	} else if (strcmp(dev->dv_xname, "sdmmc") == 0) {
		// nothing to do
		return 0;
	}
	// otherwise, ignore (nothing to do)
	return 0;
}

int config_deactivate(struct device *dev)
{
	int rv = 0, oflags = dev->dv_flags;

	if (dev->dv_flags & DVF_ACTIVE) {
		dev->dv_flags &= ~DVF_ACTIVE;
		rv = config_suspend(dev, DVACT_DEACTIVATE);
		if (rv)
			dev->dv_flags = oflags;
	}
	return rv;
}

void config_pending_incr(void) {
	return; // nothing to do?
}
void config_pending_decr(void) {
	return; // nothing to do?
}

int config_suspend(struct device *dev, int act)
{
	struct cfattach *ca = dev->dv_cfdata->cf_attach;
	int r;

	// device_ref(dev);
	if (ca->ca_activate)
		r = (*ca->ca_activate)(dev, act);
	else
		r = config_activate_children(dev, act);
	// device_unref(dev);
	return (r);
}
