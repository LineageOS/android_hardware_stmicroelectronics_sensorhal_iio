/*
 * STMicroelectronics Sysfs SensorBase Class
 *
 * Copyright 2022 Billy Laws
 *
 * Licensed under the Apache License, Version 2.0 (the "License").
 */

#define _BSD_SOURCE

#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "Light.h"

static float ReadIntegrationTimeS(const char *sysfs_path) {
	char integ_time_filename[DEVICE_IIO_MAX_FILENAME_LEN];
	if (snprintf(integ_time_filename, DEVICE_IIO_MAX_FILENAME_LEN,
			   "%s/%s", sysfs_path, "in_illuminance_integration_time") < 0)
		return 0.001f;

	FILE *fp = fopen(integ_time_filename, "r");
	if (fp == nullptr)
		return 0.001f;

	float integ_time_s{};
	fscanf(fp, "%f", &integ_time_s);
	fclose(fp);

	if (integ_time_s < 0.001f)
		return 0.001f;

	return integ_time_s;
}

Light::Light(HWSensorBaseCommonData *data,
			const char *name, int handle,
			float power_consumption)
	: SensorBase(name, handle, SENSOR_TYPE_LIGHT)
{
	memcpy(&common_data, data, sizeof(common_data));

	sensor_t_data.power = power_consumption;
	sensor_t_data.stringType = SENSOR_STRING_TYPE_LIGHT;
	sensor_t_data.flags |= SENSOR_FLAG_ON_CHANGE_MODE;
	sensor_t_data.resolution = 1.0f;
	sensor_t_data.maxRange = CONFIG_ST_HAL_ALS_RANGE;
#if (CONFIG_ST_HAL_ANDROID_VERSION >= ST_HAL_MARSHMALLOW_VERSION)
	sensor_t_data.flags &= ~DATA_INJECTION_MASK;
#endif /* CONFIG_ST_HAL_ANDROID_VERSION */

	// Configure delay range
	sensor_t_data.minDelay = (int32_t)(ReadIntegrationTimeS(
				common_data.device_iio_sysfs_path) * 1000);
	sensor_t_data.maxDelay = 999; // Maxmimum value allowed by nanosleep
	poll_period_ns = sensor_t_data.minDelay * 1000000;
}

int Light::Enable(int handle, bool enable, bool lock_en_mutex)
{
	if (lock_en_mutex)
		pthread_mutex_lock(&enable_mutex);

	bool old_status = GetStatus(false);
	bool old_status_no_handle = GetStatusExcludeHandle(handle);

	int err = SensorBase::Enable(handle, enable, false);
	if (err < 0)
		goto unlock_mutex;

	if (enable && !old_status)
		sensor_global_enable = android::elapsedRealtimeNano();
	else if (!enable && !old_status_no_handle)
		sensor_global_disable = android::elapsedRealtimeNano();

	if (sensor_t_data.handle == handle) {
		if (enable)
			sensor_my_enable = android::elapsedRealtimeNano();
		else
			sensor_my_disable = android::elapsedRealtimeNano();
	}

unlock_mutex:
	if (lock_en_mutex)
		pthread_mutex_unlock(&enable_mutex);

	return err;
}

int Light::SetDelay(int __attribute__((unused))handle,
			   int64_t period_ns,
			   int64_t __attribute__((unused))timeout,
			   bool __attribute__((unused))lock_en_mutex)
{
	// Clamp to min/max
	if ((period_ns / 1000) < sensor_t_data.minDelay)
		period_ns = sensor_t_data.minDelay * 1000;

	if ((period_ns / 1000) > sensor_t_data.maxDelay)
		period_ns = sensor_t_data.maxDelay * 1000;

	poll_period_ns = period_ns;
	return 0;
}

void Light::ThreadDataTask()
{
	SensorBaseData sensor_data{};
	char illum_filename[DEVICE_IIO_MAX_FILENAME_LEN];

	int err = snprintf(illum_filename, DEVICE_IIO_MAX_FILENAME_LEN,
			   "%s/%s", common_data.device_iio_sysfs_path, "in_illuminance_input");
	if (err < 0)
		return;

	unsigned int illum_val{};

	// Poll forever
	while (true) {
		struct timespec ts = {
			.tv_sec = 0,
			.tv_nsec = (long)poll_period_ns,
		};

		nanosleep(&ts, &ts);

		FILE *fp = fopen(illum_filename, "r");
		if (fp == nullptr)
			continue;

		unsigned int new_illum_val{};
		fscanf(fp, "%u", &new_illum_val);
		fclose(fp);

		// Only report new data on value change
		if (new_illum_val == illum_val)
			continue;

		illum_val = new_illum_val;

		sensor_data.pollrate_ns = poll_period_ns;
		sensor_data.flush_event_handle = -1;

		sensor_event.timestamp = sensor_data.timestamp = android::elapsedRealtimeNano();
		sensor_event.light = illum_val;

		SensorBase::WriteDataToPipe(poll_period_ns);
		SensorBase::ProcessData(&sensor_data);
	}
}
