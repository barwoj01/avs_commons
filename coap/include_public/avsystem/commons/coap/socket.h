/*
 * Copyright 2017 AVSystem <avsystem@avsystem.com>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef AVS_COMMONS_COAP_SOCKET_H
#define AVS_COMMONS_COAP_SOCKET_H

#include <avsystem/commons/coap/msg.h>
#include <avsystem/commons/coap/tx_params.h>

#ifdef __cplusplus
extern "C" {
#endif

#define AVS_COAP_SOCKET_ERR_TIMEOUT       (-0x5E1)
#define AVS_COAP_SOCKET_ERR_MSG_MALFORMED (-0x5E2)
#define AVS_COAP_SOCKET_ERR_NETWORK       (-0x5E3)
#define AVS_COAP_SOCKET_ERR_MSG_TOO_LONG  (-0x5E4)
#define AVS_COAP_SOCKET_ERR_DUPLICATE     (-0x5E5)
#define AVS_COAP_SOCKET_ERR_MSG_WAS_PING  (-0x5E6)

typedef struct avs_coap_socket avs_coap_socket_t;

int avs_coap_socket_create(avs_coap_socket_t **sock,
                           avs_net_abstract_socket_t *backend,
                           size_t msg_cache_size);

int avs_coap_socket_close(avs_coap_socket_t *sock);

void avs_coap_socket_cleanup(avs_coap_socket_t **sock);

/**
 * @returns 0 on success, a negative value in case of error:
 * - AVS_COAP_SOCKET_ERR_TIMEOUT if the socket timeout expired, but message
 *   could not be sent
 * - AVS_COAP_SOCKET_ERR_MSG_TOO_LONG when the message to be sent was too big
 *   for the socket
 * - AVS_COAP_SOCKET_ERR_NETWORK in case of other error on a layer below the
 *   application layer
 * - AVS_COAP_SOCKET_ERR_DUPLICATE in case duplicate request was received
 *   and then handled by response cache
 * - AVS_COAP_SOCKET_ERR_MSG_WAS_PING in case a ping was received and then
 *   handled by the socket layer
 */
int avs_coap_socket_send(avs_coap_socket_t *sock, const avs_coap_msg_t *msg);

/**
 * @returns 0 on success, a negative value in case of error:
 * - AVS_COAP_SOCKET_ERR_TIMEOUT if the socket timeout expired, but no message
 *   was received
 * - AVS_COAP_SOCKET_ERR_MSG_MALFORMED when a packet was successfully
 *   received, but it was not a correct CoAP message
 * - AVS_COAP_SOCKET_ERR_MSG_TOO_LONG when the buffer was too small to receive
 *   the packet in its entirety
 * - AVS_COAP_SOCKET_ERR_NETWORK in case of other error on a layer below the
 *   application layer
 **/
int avs_coap_socket_recv(avs_coap_socket_t *sock,
                         avs_coap_msg_t *out_msg,
                         size_t msg_capacity);

int avs_coap_socket_get_recv_timeout(avs_coap_socket_t *sock);
void avs_coap_socket_set_recv_timeout(avs_coap_socket_t *sock, int timeout_ms);

const avs_coap_tx_params_t *
avs_coap_socket_get_tx_params(avs_coap_socket_t *sock);
void avs_coap_socket_set_tx_params(avs_coap_socket_t *sock,
                                   const avs_coap_tx_params_t *tx_params);

avs_net_abstract_socket_t *avs_coap_socket_get_backend(avs_coap_socket_t *sock);

void avs_coap_socket_set_backend(avs_coap_socket_t *sock,
                                 avs_net_abstract_socket_t *backend);

/**
 * Auxiliary functions for sending simple messages.
 * @{
 */

/**
 * Sends an Empty message with given values of @p msg_type and @p msg_id.
 */
int avs_coap_send_empty(avs_coap_socket_t *socket,
                        avs_coap_msg_type_t msg_type,
                        uint16_t msg_id);

/**
 * Responds with error specified as @p error_code to the message @p msg.
 */
void avs_coap_send_error(avs_coap_socket_t *socket,
                         const avs_coap_msg_t *msg,
                         uint8_t error_code);

/**
 * Responds with a Service Unavailable messages, with Max-Age option set to
 * @p retry_after_ms converted to seconds.
 */
void avs_coap_send_service_unavailable(avs_coap_socket_t *socket,
                                       const avs_coap_msg_t *msg,
                                       int32_t retry_after_ms);

/** @} */

#ifdef __cplusplus
}
#endif

#endif // AVS_COMMONS_COAP_SOCKET_H
