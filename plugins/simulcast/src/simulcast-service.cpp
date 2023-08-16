#include <obs.hpp>
#include <obs-module.h>
#include <obs-service.h>

#include <util/dstr.hpp>

#include "plugin-macros.generated.h"

struct simulcast_service {
	char *server, *key;
};

static simulcast_service *cast(void *data)
{
	return static_cast<simulcast_service *>(data);
}

static void simulcast_service_update(void *data, obs_data_t *settings)
{
	auto service = cast(data);

	bfree(service->server);
	bfree(service->key);

	service->server = bstrdup(obs_data_get_string(settings, "server"));
	service->key = bstrdup(obs_data_get_string(settings, "key"));
}

static void simulcast_service_destroy(void *data)
{
	auto service = cast(data);

	bfree(service->server);
	bfree(service->key);
	bfree(service);
}

static void *simulcast_service_create(obs_data_t *settings,
				      obs_service_t * /*obs_service*/)
{
	auto service = cast(bzalloc(sizeof(simulcast_service)));
	simulcast_service_update(service, settings);
	return service;
}

static obs_properties_t *simulcast_service_properties(void * /*data*/)
{
	obs_properties_t *ppts = obs_properties_create();

	obs_properties_add_text(ppts, "server", "URL", OBS_TEXT_DEFAULT);

	obs_properties_add_text(ppts, "key",
				obs_module_text("Service.StreamKey"),
				OBS_TEXT_PASSWORD);

	return ppts;
}

static const char *simulcast_service_url(void *data)
{
	return cast(data)->server;
}

static const char *simulcast_service_key(void *data)
{
	return cast(data)->key;
}

static const char *simulcast_service_username(void * /*data*/)
{
	return nullptr;
}

static const char *simulcast_service_password(void * /*data*/)
{
	return nullptr;
}

#define RTMPS_PREFIX "rtmps://"
#define FTL_PREFIX "ftl://"
#define SRT_PREFIX "srt://"
#define RIST_PREFIX "rist://"

static const char *simulcast_service_get_protocol(void *data)
{
	auto service = cast(data);

	if (strncmp(service->server, RTMPS_PREFIX, strlen(RTMPS_PREFIX)) == 0)
		return "RTMPS";

	if (strncmp(service->server, FTL_PREFIX, strlen(FTL_PREFIX)) == 0)
		return "FTL";

	if (strncmp(service->server, SRT_PREFIX, strlen(SRT_PREFIX)) == 0)
		return "SRT";

	if (strncmp(service->server, RIST_PREFIX, strlen(RIST_PREFIX)) == 0)
		return "RIST";

	return "RTMP";
}

static void simulcast_service_apply_settings(void *data,
					     obs_data_t *video_settings,
					     obs_data_t *audio_settings)
{
	auto service = cast(data);
	const char *protocol = simulcast_service_get_protocol(service);
	bool has_mpegts = false;
	bool is_rtmp = false;
	if (strcmp(protocol, "SRT") == 0 || strcmp(protocol, "RIST") == 0)
		has_mpegts = true;
	if (strcmp(protocol, "RTMP") == 0 || strcmp(protocol, "RTMPS") == 0)
		is_rtmp = true;
	if (!is_rtmp && video_settings != NULL)
		obs_data_set_bool(video_settings, "repeat_headers", true);
	if (has_mpegts && audio_settings != NULL)
		obs_data_set_bool(audio_settings, "set_to_ADTS", true);
}

static const char *simulcast_service_get_connect_info(void *data, uint32_t type)
{
	switch ((enum obs_service_connect_info)type) {
	case OBS_SERVICE_CONNECT_INFO_SERVER_URL:
		return simulcast_service_url(data);
	case OBS_SERVICE_CONNECT_INFO_STREAM_ID:
		return simulcast_service_key(data);
	case OBS_SERVICE_CONNECT_INFO_USERNAME:
		return simulcast_service_username(data);
	case OBS_SERVICE_CONNECT_INFO_PASSWORD:
		return simulcast_service_password(data);
	case OBS_SERVICE_CONNECT_INFO_ENCRYPT_PASSPHRASE: {
		const char *protocol = simulcast_service_get_protocol(data);

		if ((strcmp(protocol, "SRT") == 0))
			return simulcast_service_password(data);
		else if ((strcmp(protocol, "RIST") == 0))
			return simulcast_service_key(data);

		break;
	}
	case OBS_SERVICE_CONNECT_INFO_BEARER_TOKEN:
		return NULL;
	}

	return NULL;
}

static bool simulcast_service_can_try_to_connect(void *data)
{
	auto service = cast(data);

	return (service->server != NULL && service->server[0] != '\0');
}

void register_service()
{
	obs_service_info info{};
	info.id = "simulcast_service";
	info.get_name = [](void *) {
		return obs_module_text("Service.Name");
	};
	info.create = simulcast_service_create;
	info.destroy = simulcast_service_destroy;
	info.update = simulcast_service_update;
	info.get_properties = simulcast_service_properties;
	info.get_protocol = simulcast_service_get_protocol;
	info.get_url = simulcast_service_url;
	info.get_key = simulcast_service_key;
	info.get_connect_info = simulcast_service_get_connect_info;
	info.get_username = simulcast_service_username;
	info.get_password = simulcast_service_password;
	info.apply_encoder_settings = simulcast_service_apply_settings;
	info.can_try_to_connect = simulcast_service_can_try_to_connect;

	obs_register_service(&info);
}
