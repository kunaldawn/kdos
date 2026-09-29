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
 * `draw` is the one field every page fills; the input handlers are NULL on a
 * page that takes none, and page.c tests each before it calls.
 */

#include "res.h"

const ResPage RES_PAGES[RP_NPAGES] = {
	{ "applications", "Applications", "applications-system", res_app_prepare, res_app_headline, res_draw_apps, res_app_click, res_app_wheel, res_app_key, res_app_motion, res_app_release, NULL },
	{ "processes",    "Processes",    "system-run",  res_procs_prepare, res_proc_headline, res_draw_procs, res_procs_click, res_procs_wheel, res_procs_key, res_procs_motion, res_procs_release, NULL },
	{ "cpu",          "CPU",          "cpu",                 NULL, res_cpu_headline, res_draw_cpu, NULL, NULL, res_cpu_key, NULL, NULL, res_cpu_figure },
	{ "memory",       "Memory",       "media-flash",         NULL, res_mem_headline, res_draw_mem, NULL, NULL, res_mem_key, NULL, NULL, res_mem_figure },
	{ "gpu",          "GPU",          "video-display",       res_gpu_prepare, res_gpu_headline, res_draw_gpu, NULL, NULL, NULL, NULL, NULL, res_gpu_figure },
	{ "drives",       "Drives",       "drive-harddisk",      res_drive_prepare, res_drive_headline, res_draw_drives, res_drive_click, res_drive_wheel, res_drive_key, res_drive_motion, NULL, NULL },
	{ "network",      "Network",      "network-wired",       res_net_prepare, res_net_headline, res_draw_net, res_net_click, res_net_wheel, res_net_key, res_net_motion, NULL, NULL },
	{ "batteries",    "Batteries",    "battery",             res_batt_prepare, res_batt_headline, res_draw_batt, NULL, NULL, NULL, NULL, NULL, res_batt_figure },
	{ "energy",       "Energy",       "speedometer",         res_energy_prepare, res_energy_headline, res_draw_energy, NULL, NULL, res_energy_key, NULL, NULL, NULL },
	{ "sensors",      "Sensors",      "temperature-symbolic", res_sensor_prepare, res_sensor_headline, res_draw_sensors, NULL, NULL, NULL, NULL, NULL, res_sensor_figure },
	{ "boxes",        "Boxes",        "package",   res_box_prepare, res_box_headline, res_draw_boxes, res_box_click, res_box_wheel, res_box_key, res_box_motion, res_box_release, NULL },
};
