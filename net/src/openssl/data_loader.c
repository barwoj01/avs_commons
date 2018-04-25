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

#include <avs_commons_config.h>

#define MODULE_NAME avs_net_data_loader
#include <x_log_config.h>

#include "common.h"
#include "data_loader.h"

#include "../api.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <avsystem/commons/utils.h>

VISIBILITY_SOURCE_BEGIN

static int password_cb(char *buf, int num, int rwflag, void *userdata) {
    if (!userdata) {
        buf[0] = '\0';
        return 0;
    }
    int retval = snprintf(buf, (size_t) num, "%s", (const char *) userdata);
    (void) rwflag;
    return (retval < 0 || retval >= num) ? -1 : retval;
}

static inline void setup_password_callback(SSL_CTX *ctx, const char *password) {
    SSL_CTX_set_default_passwd_cb_userdata(
            ctx,
            /* const_cast */ (void *) (intptr_t) password);
    SSL_CTX_set_default_passwd_cb(ctx, password_cb);
}

static int load_ca_certs_from_paths(SSL_CTX *ctx,
                                    const char *file,
                                    const char *path) {
    LOG(DEBUG, "CA certificate <file=%s, path=%s>: going to load",
        file ? file : "(null)", path ? path : "(null)");

    if (file) {
        // Try DER.
        if (SSL_CTX_use_certificate_file(ctx, file, SSL_FILETYPE_ASN1) == 1) {
            return 0;
        }
        // Try PEM.
        if (SSL_CTX_use_certificate_file(ctx, file, SSL_FILETYPE_PEM) == 1) {
            return 0;
        }
        log_openssl_error();
        return -1;
    } else {
        if (!SSL_CTX_load_verify_locations(ctx, NULL, path)) {
            log_openssl_error();
            return -1;
        }
        if (!SSL_CTX_set_default_verify_paths(ctx)) {
            log_openssl_error();
            return -1;
        }
    }
    return 0;
}

typedef enum {
    ENCODING_UNKNOWN,
    ENCODING_PEM,
    ENCODING_DER
} encoding_t;

static encoding_t detect_cert_encoding(const char *buffer, size_t len) {
    static const char *pem_prefix = "-----BEGIN ";
    if (len < strlen(pem_prefix)) {
        // clearly not PEM and too short to be DER
        return ENCODING_UNKNOWN;
    } else if (!strncmp(buffer, pem_prefix, strlen(pem_prefix))) {
        return ENCODING_PEM;
    } else {
        return ENCODING_DER;
    }
}

// NOTE: This function exists only because OpenSSL does not seem to have a
// method of loading in-buffer PEM encoded certificates.
static X509 *parse_cert(const void *buffer, const size_t len) {
    X509 *cert = NULL;
    switch (detect_cert_encoding((const char *) buffer, len)) {
    case ENCODING_PEM: {
        // Convert PEM to DER.
        BIO *bio = BIO_new_mem_buf((void *) (intptr_t) buffer, len);
        if (!bio) {
            return NULL;
        }
        cert = PEM_read_bio_X509(bio, NULL, NULL, NULL);
        BIO_free(bio);
        break;
    }
    case ENCODING_DER: {
        const unsigned char *data = (unsigned char *)buffer;
        cert = d2i_X509(NULL, &data, (int) len);
        break;
    }
    deault:
        LOG(ERROR, "unknown in-memory certificate format");
        break;
    }
    return cert;
}

static int load_ca_cert_from_buffer(SSL_CTX *ctx,
                                    const void *buffer,
                                    const size_t len) {
    X509 *cert = parse_cert(buffer, len);
    if (!cert) {
        return -1;
    }
    X509_STORE *store = SSL_CTX_get_cert_store(ctx);
    if (!store || !X509_STORE_add_cert(store, cert)) {
        log_openssl_error();
        X509_free(cert);
        return -1;
    }
    return 0;
}

int _avs_net_openssl_load_ca_certs(SSL_CTX *ctx,
                                   const avs_net_trusted_cert_info_t *info) {
    setup_password_callback(ctx, NULL);

    switch (info->desc.source) {
    case AVS_NET_DATA_SOURCE_FILE:
        return load_ca_certs_from_paths(ctx, info->desc.info.file.filename,
                                        NULL);
    case AVS_NET_DATA_SOURCE_PATH:
        return load_ca_certs_from_paths(ctx, NULL, info->desc.info.path.path);
    case AVS_NET_DATA_SOURCE_BUFFER:
        return load_ca_cert_from_buffer(ctx, info->desc.info.buffer.buffer,
                                        info->desc.info.buffer.buffer_size);
    default:
        assert(0 && "invalid data source");
        return -1;
    }
    return 0;
}

static int load_client_cert_from_file(SSL_CTX *ctx,
                                      const char *filename) {
    // Try DER.
    if (SSL_CTX_use_certificate_file(ctx, filename, SSL_FILETYPE_ASN1) == 1) {
        return 0;
    }
    // Try PEM.
    if (SSL_CTX_use_certificate_file(ctx, filename, SSL_FILETYPE_PEM) == 1) {
        return 0;
    }
    log_openssl_error();
    return -1;
}

static int
load_client_cert_from_buffer(SSL_CTX *ctx, const void *buffer, size_t len) {
    X509 *cert = parse_cert(buffer, len);
    if (!cert) {
        return -1;
    }
    if (SSL_CTX_use_certificate(ctx, cert) != 1) {
        X509_free(cert);
        log_openssl_error();
        return -1;
    }
    return 0;
}

int _avs_net_openssl_load_client_cert(SSL_CTX *ctx,
                                      const avs_net_client_cert_info_t *info) {
    setup_password_callback(ctx, NULL);

    switch (info->desc.source) {
    case AVS_NET_DATA_SOURCE_FILE:
        return load_client_cert_from_file(ctx, info->desc.info.file.filename);
    case AVS_NET_DATA_SOURCE_BUFFER:
        return load_client_cert_from_buffer(ctx, info->desc.info.buffer.buffer,
                                            info->desc.info.buffer.buffer_size);
    default:
        assert(0 && "invalid data source");
        return -1;
    }
    return 0;
}

static int load_client_key_from_file(SSL_CTX *ctx,
                                     const char *filename,
                                     const char *password) {
    setup_password_callback(ctx, password);

    // Try PEM.
    if (SSL_CTX_use_PrivateKey_file(ctx, filename, SSL_FILETYPE_PEM) == 1) {
        return 0;
    }
    // Try DER.
    if (SSL_CTX_use_PrivateKey_file(ctx, filename, SSL_FILETYPE_ASN1) == 1) {
        return 0;
    }
    log_openssl_error();
    return -1;
}

static int load_client_key_from_buffer(SSL_CTX *ctx,
                                       const void *buffer,
                                       size_t len,
                                       const char *password) {
    setup_password_callback(ctx, password);

    /**
     * We support EC keys only at the moment, as OpenSSL does not seem to have
     * a method for auto-detection of key type.
     */
    if (SSL_CTX_use_PrivateKey_ASN1(EVP_PKEY_EC, ctx,
                                    (unsigned char *) (intptr_t) buffer,
                                    (long) len) == 1) {
        return 0;
    }
    log_openssl_error();
    return -1;
}

int _avs_net_openssl_load_client_key(SSL_CTX *ctx,
                                     const avs_net_client_key_info_t *info) {
    switch (info->desc.source) {
    case AVS_NET_DATA_SOURCE_FILE:
        return load_client_key_from_file(ctx, info->desc.info.file.filename,
                                         info->desc.info.file.password);
    case AVS_NET_DATA_SOURCE_BUFFER:
        return load_client_key_from_buffer(ctx, info->desc.info.buffer.buffer,
                                           info->desc.info.buffer.buffer_size,
                                           info->desc.info.buffer.password);
    default:
        assert(0 && "invalid data source");
        return -1;
    }
    return 0;
}

#ifdef AVS_UNIT_TESTING
#include "test/data_loader.c"
#endif
