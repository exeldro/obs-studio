/*
simulcast
Copyright (C) 2023-2023 John R. Bradley <jocbrad@twitch.tv>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program. If not, see <https://www.gnu.org/licenses/>
*/

#include "simulcast-plugin.h"
#include "common.h"
#include "global-service.h"
#include "simulcast-dock-widget.h"

#include <QAction>
#include <QMainWindow>
#include <obs.hpp>
#include <obs-module.h>
#include <obs-frontend-api.h>
#include <util/util.hpp>

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("simulcast", "en-US")
OBS_MODULE_AUTHOR("John R. Bradley")
const char *obs_module_name(void)
{
	return "simulcast";
}
const char *obs_module_description(void)
{
	return obs_module_text("Simulcast.Plugin.Description");
}

void register_service();
void register_settings_window(SimulcastDockWidget *dock,
			      obs_data_t *plugin_config);

static void obs_event_handler(obs_frontend_event event,
			      SimulcastDockWidget *simulcastWidget)
{
	if (event == obs_frontend_event::OBS_FRONTEND_EVENT_EXIT) {
		simulcastWidget->SaveConfig();
	} else if (event == obs_frontend_event::
				    OBS_FRONTEND_EVENT_PROFILE_CHANGED ||
		   event == obs_frontend_event::
				    OBS_FRONTEND_EVENT_FINISHED_LOADING) {
		simulcastWidget->LoadConfig();
		if (event ==
		    obs_frontend_event::OBS_FRONTEND_EVENT_FINISHED_LOADING) {
			simulcastWidget->CheckPromptToMakeDockVisible();
		}
	} else if (event ==
		   obs_frontend_event::OBS_FRONTEND_EVENT_PROFILE_RENAMED) {
		simulcastWidget->ProfileRenamed();
	} else if (event ==
		   obs_frontend_event::OBS_FRONTEND_EVENT_PROFILE_LIST_CHANGED) {
		simulcastWidget->PruneDeletedProfiles();
	}
}

bool obs_module_load(void)
{
	blog(LOG_INFO, "Loading module simulcast (%s)", SIMULCAST_VERSION);

	if (obs_get_version() < MAKE_SEMANTIC_VERSION(29, 1, 0))
		return false;

	register_service();

	auto mainWindow = (QMainWindow *)obs_frontend_get_main_window();
	if (mainWindow == nullptr)
		return false;

	auto plugin_config_file = BPtr<char>{obs_module_file("plugin.json")};
	if (!plugin_config_file) {
		blog(LOG_WARNING, "Could not find 'plugin.json'");
		return false;
	}

	OBSDataAutoRelease plugin_config =
		obs_data_create_from_json_file(plugin_config_file);

	OBSDataAutoRelease dock_config =
		obs_data_get_obj(plugin_config, "dock");
	if (!dock_config || !obs_data_has_user_value(dock_config, "id") ||
	    !obs_data_has_user_value(dock_config, "title")) {
		blog(LOG_ERROR, "Invalid dock config");
		return false;
	}

	QMetaObject::invokeMethod(mainWindow, []() {
		GetGlobalService().setCurrentThreadAsDefault();
	});

	auto dock = new SimulcastDockWidget(mainWindow);

	register_settings_window(dock, OBSDataAutoRelease{obs_data_get_obj(
					       plugin_config, "settings")});

	obs_frontend_add_dock_by_id(obs_data_get_string(dock_config, "id"),
				    obs_data_get_string(dock_config, "title"),
				    dock);

	// Parent is set by `obs_frontend_add_dock_by_id`, so we need to call this externally/later
	dock->SetParentStyleSheet(dock_config);

	obs_frontend_add_event_callback(
		[](enum obs_frontend_event event, void *private_data) {
			obs_event_handler(event,
					  static_cast<SimulcastDockWidget *>(
						  private_data));
		},
		dock);

	return true;
}

void obs_module_unload()
{
	blog(LOG_INFO, "Unloading module");
}
