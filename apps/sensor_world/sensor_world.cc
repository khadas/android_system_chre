/*
 * Copyright (C) 2016 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <chre.h>
#include <cinttypes>

#include "chre/util/macros.h"
#include "chre/util/nanoapp/log.h"
#include "chre/util/nanoapp/sensor.h"
#include "chre/util/time.h"

#define LOG_TAG "[SensorWorld]"

#ifdef CHRE_NANOAPP_INTERNAL
namespace chre {
namespace {
#endif  // CHRE_NANOAPP_INTERNAL

namespace {

struct SensorState {
  const uint8_t type;
  uint32_t handle;
  bool isInitialized;
  bool enable;
  uint64_t interval;  // nsec
  uint64_t latency;  // nsec
  chreSensorInfo info;
};

SensorState sensors[] = {
  { .type = CHRE_SENSOR_TYPE_ACCELEROMETER,
    .enable = true,
    .interval = Milliseconds(80).toRawNanoseconds(),
    .latency = Seconds(4).toRawNanoseconds(),
  },
  { .type = CHRE_SENSOR_TYPE_INSTANT_MOTION_DETECT,
    .enable = false,
  },
  { .type = CHRE_SENSOR_TYPE_STATIONARY_DETECT,
    .enable = false,
  },
  { .type = CHRE_SENSOR_TYPE_GYROSCOPE,
    .enable = true,
    .interval = Milliseconds(80).toRawNanoseconds(),
    .latency = Seconds(4).toRawNanoseconds(),
  },
  { .type = CHRE_SENSOR_TYPE_GEOMAGNETIC_FIELD,
    .enable = true,
    .interval = Milliseconds(80).toRawNanoseconds(),
    .latency = Seconds(4).toRawNanoseconds(),
  },
  { .type = CHRE_SENSOR_TYPE_PRESSURE,
    .enable = true,
    .interval = Milliseconds(200).toRawNanoseconds(),
    .latency = Seconds(4).toRawNanoseconds(),
  },
  { .type = CHRE_SENSOR_TYPE_LIGHT,
    .enable = true,
    .interval = Milliseconds(200).toRawNanoseconds(),
    .latency = 0,
  },
  { .type = CHRE_SENSOR_TYPE_PROXIMITY,
    .enable = true,
    .interval = Milliseconds(200).toRawNanoseconds(),
    .latency = 0,
  },
  { .type = CHRE_SENSOR_TYPE_ACCELEROMETER_TEMPERATURE,
    .enable = true,
    .interval = Seconds(2).toRawNanoseconds(),
    .latency = 0,
  },
  { .type = CHRE_SENSOR_TYPE_GYROSCOPE_TEMPERATURE,
    .enable = true,
    .interval = Seconds(2).toRawNanoseconds(),
    .latency = 0,
  },
  { .type = CHRE_SENSOR_TYPE_UNCALIBRATED_ACCELEROMETER,
    .enable = true,
    .interval = Milliseconds(80).toRawNanoseconds(),
    .latency = Seconds(4).toRawNanoseconds(),
  },
  { .type = CHRE_SENSOR_TYPE_UNCALIBRATED_GYROSCOPE,
    .enable = true,
    .interval = Milliseconds(80).toRawNanoseconds(),
    .latency = Seconds(4).toRawNanoseconds(),
  },
  { .type = CHRE_SENSOR_TYPE_UNCALIBRATED_GEOMAGNETIC_FIELD,
    .enable = true,
    .interval = Milliseconds(80).toRawNanoseconds(),
    .latency = Seconds(4).toRawNanoseconds(),
  },
};

// Helpers for testing InstantMotion and StationaryDetect
enum class MotionMode {
  Instant,
  Stationary,
};

// Storage to help access InstantMotion and StationaryDetect sensor handle and
// info
static size_t motionSensorIndices[2];
static MotionMode motionMode = MotionMode::Instant;

size_t getMotionSensorIndex() {
  motionMode = (motionMode == MotionMode::Instant) ?
      MotionMode::Stationary : MotionMode::Instant;
  return motionSensorIndices[static_cast<size_t>(motionMode)];
}

} // namespace

bool nanoappStart() {
  LOGI("App started on platform ID %" PRIx64, chreGetPlatformId());

  for (size_t i = 0; i < ARRAY_SIZE(sensors); i++) {
    SensorState& sensor = sensors[i];
    sensor.isInitialized = chreSensorFindDefault(sensor.type, &sensor.handle);
    LOGI("Sensor %d initialized: %s with handle %" PRIu32,
         i, sensor.isInitialized ? "true" : "false", sensor.handle);

    if (sensor.type == CHRE_SENSOR_TYPE_INSTANT_MOTION_DETECT) {
      motionSensorIndices[static_cast<size_t>(MotionMode::Instant)] = i;
    } else if (sensor.type == CHRE_SENSOR_TYPE_STATIONARY_DETECT) {
      motionSensorIndices[static_cast<size_t>(MotionMode::Stationary)] = i;
    }

    if (sensor.isInitialized) {
      // Get sensor info
      chreSensorInfo& info = sensor.info;
      bool infoStatus = chreGetSensorInfo(sensor.handle, &info);
      if (infoStatus) {
        LOGI("SensorInfo: %s, Type=%" PRIu8 " OnChange=%d"
             " OneShot=%d minInterval=%" PRIu64 "nsec",
             info.sensorName, info.sensorType, info.isOnChange,
             info.isOneShot, info.minInterval);
      } else {
        LOGE("chreGetSensorInfo failed");
      }

      // Subscribe to sensors
      if (sensor.enable) {
        float odrHz = 1e9 / sensor.interval;
        float latencySec = sensor.latency / 1e9;
        bool status = chreSensorConfigure(sensor.handle,
            CHRE_SENSOR_CONFIGURE_MODE_CONTINUOUS, sensor.interval,
            sensor.latency);
        LOGI("Requested data: odr %f Hz, latency %f sec, %s",
             odrHz, latencySec, status ? "success" : "failure");
      }
    }
  }

  return true;
}

void nanoappHandleEvent(uint32_t senderInstanceId,
                        uint16_t eventType,
                        const void *eventData) {
  switch (eventType) {
    case CHRE_EVENT_SENSOR_ACCELEROMETER_DATA:
    case CHRE_EVENT_SENSOR_UNCALIBRATED_ACCELEROMETER_DATA:
    case CHRE_EVENT_SENSOR_GYROSCOPE_DATA:
    case CHRE_EVENT_SENSOR_UNCALIBRATED_GYROSCOPE_DATA:
    case CHRE_EVENT_SENSOR_GEOMAGNETIC_FIELD_DATA:
    case CHRE_EVENT_SENSOR_UNCALIBRATED_GEOMAGNETIC_FIELD_DATA: {
      const auto *ev = static_cast<const chreSensorThreeAxisData *>(eventData);
      const auto header = ev->header;
      const auto *data = ev->readings;

      float x = 0, y = 0, z = 0;
      for (size_t i = 0; i < header.readingCount; i++) {
        x += data[i].v[0];
        y += data[i].v[1];
        z += data[i].v[2];
      }
      x /= header.readingCount;
      y /= header.readingCount;
      z /= header.readingCount;

      LOGI("%s, %d samples: %f %f %f",
           getSensorNameForEventType(eventType), header.readingCount, x, y, z);
      break;
    }

    case CHRE_EVENT_SENSOR_PRESSURE_DATA:
    case CHRE_EVENT_SENSOR_LIGHT_DATA:
    case CHRE_EVENT_SENSOR_ACCELEROMETER_TEMPERATURE_DATA:
    case CHRE_EVENT_SENSOR_GYROSCOPE_TEMPERATURE_DATA: {
      const auto *ev = static_cast<const chreSensorFloatData *>(eventData);
      const auto header = ev->header;

      float v = 0;
      for (size_t i = 0; i < header.readingCount; i++) {
        v += ev->readings[i].value;
      }
      v /= header.readingCount;

      LOGI("%s, %d samples: %f",
           getSensorNameForEventType(eventType), header.readingCount, v);
      break;
    }

    case CHRE_EVENT_SENSOR_PROXIMITY_DATA: {
      const auto *ev = static_cast<const chreSensorByteData *>(eventData);
      const auto header = ev->header;
      const auto reading = ev->readings[0];

      LOGI("%s, %d samples: isNear %d, invalid %d",
           getSensorNameForEventType(eventType), header.readingCount,
           reading.isNear, reading.invalid);

      // Enable InstantMotion and StationaryDetect alternatively on near->far.
      if (reading.isNear == 0) {
        size_t motionSensorIndex = getMotionSensorIndex();
        bool status = chreSensorConfigure(sensors[motionSensorIndex].handle,
            CHRE_SENSOR_CONFIGURE_MODE_ONE_SHOT,
            CHRE_SENSOR_INTERVAL_DEFAULT,
            CHRE_SENSOR_LATENCY_DEFAULT);
        LOGI("Requested %s: %s", sensors[motionSensorIndex].info.sensorName,
              status ? "success" : "failure");
      }
      break;
    }

    case CHRE_EVENT_SENSOR_INSTANT_MOTION_DETECT_DATA:
    case CHRE_EVENT_SENSOR_STATIONARY_DETECT_DATA: {
      const auto *ev = static_cast<const chreSensorOccurrenceData *>(eventData);
      const auto header = ev->header;

      LOGI("%s, %d samples",
           getSensorNameForEventType(eventType), header.readingCount);
      break;
    }

    default:
      LOGW("Unhandled event %d", eventType);
      break;
  }
}

void nanoappEnd() {
  LOGI("Stopped");
}

#ifdef CHRE_NANOAPP_INTERNAL
}  // anonymous namespace
}  // namespace chre

#include "chre/util/nanoapp/app_id.h"
#include "chre/platform/static_nanoapp_init.h"

CHRE_STATIC_NANOAPP_INIT(SensorWorld, chre::kSensorWorldAppId, 0);
#endif  // CHRE_NANOAPP_INTERNAL
