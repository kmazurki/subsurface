// SPDX-License-Identifier: GPL-2.0
#include "divelog.h"
#include "divelist.h"
#include "divesite.h"
#include "device.h"
#include "errorhelper.h"
#include "filterpreset.h"
#include "trip.h"

struct divelog divelog;

// We can't use smart pointers, since this is used from C
// and it would be bold to presume that std::unique_ptr<>
// and a plain pointer have the same memory layout.
divelog::divelog() :
	dives(new dive_table),
	trips(new trip_table),
	sites(new dive_site_table),
	devices(new device_table),
	filter_presets(new filter_preset_table),
	autogroup(false)
{
	*dives = empty_dive_table;
	*trips = empty_trip_table;
	*sites = empty_dive_site_table;
}

divelog::~divelog()
{
	clear_dive_table(dives);
	clear_trip_table(trips);
	clear_dive_site_table(sites);
	delete dives;
	delete trips;
	delete sites;
	delete devices;
	delete filter_presets;
}

divelog::divelog(divelog &&log) :
	dives(new dive_table),
	trips(new trip_table),
	sites(new dive_site_table),
	devices(new device_table),
	filter_presets(new filter_preset_table)
{
	*dives = empty_dive_table;
	*trips = empty_trip_table;
	*sites = empty_dive_site_table;
	move_dive_table(log.dives, dives);
	move_trip_table(log.trips, trips);
	move_dive_site_table(log.sites, sites);
	*devices = std::move(*log.devices);
	*filter_presets = std::move(*log.filter_presets);
}

struct divelog &divelog::operator=(divelog &&log)
{
	move_dive_table(log.dives, dives);
	move_trip_table(log.trips, trips);
	move_dive_site_table(log.sites, sites);
	*devices = std::move(*log.devices);
	*filter_presets = std::move(*log.filter_presets);
	return *this;
}

/* this implements the mechanics of removing the dive from the
 * dive log and the trip, but doesn't deal with updating dive trips, etc */
void delete_single_dive(struct divelog *log, int idx)
{
	if (idx < 0 || idx > log->dives->nr) {
		report_info("Warning: deleting unexisting dive with index %d", idx);
		return;
	}
	struct dive *dive = log->dives->dives[idx];
	remove_dive_from_trip(dive, log->trips);
	unregister_dive_from_dive_site(dive);
	delete_dive_from_table(log->dives, idx);
}

void divelog::clear()
{
	while (dives->nr > 0)
		delete_single_dive(this, dives->nr - 1);
	while (sites->nr)
		delete_dive_site(get_dive_site(0, sites), sites);
	if (trips->nr != 0) {
		report_info("Warning: trip table not empty in divelog::clear()!");
		trips->nr = 0;
	}
	clear_device_table(devices);
	filter_presets->clear();
}

extern "C" void clear_divelog(struct divelog *log)
{
	log->clear();
}
