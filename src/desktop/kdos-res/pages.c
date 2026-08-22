/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   KD's Homebrew Linux Distro
 * ---------------------------------
 */

/*
 * The registry. One row per page, in sidebar order.
 *
 * A page with a NULL draw is one that has not landed yet and says so on the
 * screen rather than drawing an empty frame that looks finished.
 */

#include <stdio.h>
#include <string.h>

#include "res.h"

void res_page_placeholder(int x, int y, int w, int h, const char *name)
{
	(void)name;
	(void)h;
	ktui_draw_text(x + 2, y + 1, w - 4,
		       "This page has no reader yet.", KT_DIM, KT_BG, 0);
}


const ResPage RES_PAGES[RP_NPAGES] = {
	{ "applications", "Applications", "applications-system", res_app_prepare, res_app_headline, res_draw_apps, res_app_click, res_app_wheel, res_app_key, res_app_motion, res_app_release },
	{ "processes",    "Processes",    "utilities-terminal",  res_procs_prepare, res_proc_headline, res_draw_procs, res_procs_click, res_procs_wheel, res_procs_key, res_procs_motion, res_procs_release },
	{ "cpu",          "CPU",          "cpu",                 NULL, res_cpu_headline, res_draw_cpu, NULL, NULL, NULL, NULL, NULL },
	{ "memory",       "Memory",       "media-flash",         NULL, res_mem_headline, res_draw_mem, NULL, NULL, NULL, NULL, NULL },
	{ "gpu",          "GPU",          "video-display",       res_gpu_prepare, res_gpu_headline, res_draw_gpu, NULL, NULL, NULL, NULL, NULL },
	{ "drives",       "Drives",       "drive-harddisk",      res_drive_prepare, res_drive_headline, res_draw_drives, res_drive_click, res_drive_wheel, res_drive_key, res_drive_motion, NULL },
	{ "network",      "Network",      "network-wired",       res_net_prepare, res_net_headline, res_draw_net, res_net_click, res_net_wheel, res_net_key, res_net_motion, NULL },
	{ "batteries",    "Batteries",    "battery",             res_batt_prepare, res_batt_headline, res_draw_batt, NULL, NULL, NULL, NULL, NULL },
	{ "energy",       "Energy",       "speedometer",         res_energy_prepare, res_energy_headline, res_draw_energy, NULL, NULL, res_energy_key, NULL, NULL },
};
