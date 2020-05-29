/* * Copyright (C) 2020 The Android Open Source Project
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

#include "chre_cross_validator_wifi_manager.h"

#include <stdio.h>
#include <cinttypes>
#include <cstring>

#include "chre/util/nanoapp/callbacks.h"
#include "chre/util/nanoapp/log.h"
#include "chre_cross_validation_wifi.nanopb.h"
#include "chre_test_common.nanopb.h"

#define LOG_TAG "ChreCrossValidatorWifi"

#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))

namespace chre {

namespace cross_validator_wifi {

// Fake scan monitor cookie which is not used
constexpr uint32_t kScanMonitoringCookie = 0;

void Manager::handleEvent(uint32_t senderInstanceId, uint16_t eventType,
                          const void *eventData) {
  switch (eventType) {
    case CHRE_EVENT_MESSAGE_FROM_HOST:
      handleMessageFromHost(
          senderInstanceId,
          static_cast<const chreMessageFromHostData *>(eventData));
      break;
    case CHRE_EVENT_WIFI_ASYNC_RESULT:
      handleWifiAsyncResult(static_cast<const chreAsyncResult *>(eventData));
    default:
      LOGE("Unknown message type %" PRIu16 "received when handling event",
           eventType);
  }
}

void Manager::handleMessageFromHost(uint32_t senderInstanceId,
                                    const chreMessageFromHostData *hostData) {
  if (senderInstanceId != CHRE_INSTANCE_ID) {
    LOGE("Incorrect sender instance id: %" PRIu32, senderInstanceId);
  } else {
    mCrossValidatorState.hostEndpoint = hostData->hostEndpoint;
    switch (hostData->messageType) {
      case chre_cross_validation_wifi_MessageType_STEP_START: {
        pb_istream_t stream = pb_istream_from_buffer(
            static_cast<const pb_byte_t *>(
                const_cast<const void *>(hostData->message)),
            hostData->messageSize);
        chre_cross_validation_wifi_StepStartCommand stepStartCommand;
        if (!pb_decode(&stream,
                       chre_cross_validation_wifi_StepStartCommand_fields,
                       &stepStartCommand)) {
          LOGE("Error decoding StepStartCommand");
        } else {
          handleStepStartMessage(stepStartCommand);
        }
        break;
      }
      default:
        LOGE("Unknown message type %" PRIu32 " for host message",
             hostData->messageType);
    }
  }
}

void Manager::handleStepStartMessage(
    chre_cross_validation_wifi_StepStartCommand stepStartCommand) {
  chre_test_common_TestResult testResult;
  switch (stepStartCommand.step) {
    case chre_cross_validation_wifi_Step_INIT:
      testResult = makeTestResultProtoMessage(
          false, "Received StepStartCommand for INIT step");
      break;
    case chre_cross_validation_wifi_Step_SETUP:
      if (!chreWifiConfigureScanMonitorAsync(true /* enable */,
                                             &kScanMonitoringCookie)) {
        LOGE("chreWifiConfigureScanMonitorAsync() failed");
        testResult =
            makeTestResultProtoMessage(false, "setupWifiScanMonitoring failed");
        encodeAndSendMessageToHost(static_cast<void *>(&testResult),
                                   chre_test_common_TestResult_fields);
      } else {
        LOGD("chreWifiConfigureScanMonitorAsync() succeeded");
      }
      break;
    case chre_cross_validation_wifi_Step_VALIDATE:
      break;
  }
  mStep = stepStartCommand.step;
}

bool Manager::encodeErrorMessage(pb_ostream_t *stream,
                                 const pb_field_t * /*field*/,
                                 void *const *arg) {
  const char *str = static_cast<const char *>(const_cast<const void *>(*arg));
  size_t len = strlen(str);
  return pb_encode_string(stream, reinterpret_cast<const pb_byte_t *>(str),
                          len);
}

chre_test_common_TestResult Manager::makeTestResultProtoMessage(
    bool success, const char *errMessage) {
  // TODO(b/154271547): Move this implementation into
  // common/shared/send_message.cc::sendTestResultToHost
  chre_test_common_TestResult testResult =
      chre_test_common_TestResult_init_default;
  testResult.has_code = true;
  testResult.code = success ? chre_test_common_TestResult_Code_PASSED
                            : chre_test_common_TestResult_Code_FAILED;
  if (!success && errMessage != nullptr) {
    testResult.errorMessage = {.funcs = {.encode = encodeErrorMessage},
                               .arg = const_cast<char *>(errMessage)};
  }
  return testResult;
}

void Manager::encodeAndSendMessageToHost(const void *message,
                                         const pb_field_t *fields) {
  size_t encodedSize;
  if (!pb_get_encoded_size(&encodedSize, fields, message)) {
    LOGE("Could not get encoded size of test result message");
  } else {
    pb_byte_t *buffer = static_cast<pb_byte_t *>(chreHeapAlloc(encodedSize));
    if (buffer == nullptr) {
      LOG_OOM();
    } else {
      pb_ostream_t ostream = pb_ostream_from_buffer(buffer, encodedSize);
      if (!pb_encode(&ostream, fields, message)) {
        LOGE("Could not encode data proto message");
      } else if (!chreSendMessageToHostEndpoint(
                     static_cast<void *>(buffer), encodedSize,
                     chre_cross_validation_wifi_MessageType_STEP_RESULT,
                     mCrossValidatorState.hostEndpoint,
                     heapFreeMessageCallback)) {
        LOGE("Could not send message to host");
      }
    }
  }
}

void Manager::handleWifiAsyncResult(const chreAsyncResult *result) {
  LOGI("handleWifiAsyncResult method");
  chre_test_common_TestResult testResult;
  if (result->requestType == CHRE_WIFI_REQUEST_TYPE_CONFIGURE_SCAN_MONITOR) {
    if (mStep != chre_cross_validation_wifi_Step_SETUP) {
      testResult = makeTestResultProtoMessage(
          false, "Received scan monitor result event when step is not SETUP");
    } else {
      if (result->success) {
        LOGD("Wifi scan monitoring setup successfully");
        testResult = makeTestResultProtoMessage(true);
      } else {
        LOGE("Wifi scan monitoring setup failed async w/ error code %" PRIu8
             ".",
             result->errorCode);
        testResult = makeTestResultProtoMessage(
            false, "Wifi scan monitoring setup failed async.");
      }
      encodeAndSendMessageToHost(static_cast<void *>(&testResult),
                                 chre_test_common_TestResult_fields);
    }
  } else {
    testResult = makeTestResultProtoMessage(
        false, "Unknown chre async result type received");
  }
}

}  // namespace cross_validator_wifi

}  // namespace chre