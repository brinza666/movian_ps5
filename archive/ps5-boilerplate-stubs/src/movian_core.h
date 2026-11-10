/*
 * Movian Core Header
 * Declares functions from libm7.so that we need to call
 * from the PS5 native port
 */

#ifndef MOVIAN_CORE_H
#define MOVIAN_CORE_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Application core */
const char *app_dataroot(void);
void free_c_data(void *ptr);

/* Duktape JS engine - exported from libm7.so */
/* These are the duk_* functions we analyzed via REA */

/* Video/audio would be implemented natively for PS5 */

/* Crypto functions (from libm7.so exports) */
int AES128_ECB_encrypt(const uint8_t *key, const uint8_t *in, uint8_t *out);
int AES128_ECB_decrypt_reference(const uint8_t *key, const uint8_t *in, uint8_t *out);
void MD5Update(void *ctx, const void *data, size_t len);
void MD4Update(void *ctx, const void *data, size_t len);

/* SMB2 network functions (from libm7.so exports) */
int smb2_init_context(void);
int smb2_connect_share(void *ctx, const char *host, const char *share);
int smb2_listdir(void *share_ctx, const char *path);

/* Initialize Movian core (replaces JNI_OnLoad) */
int movian_core_init(void);

/* Shutdown Movian core */
void movian_core_shutdown(void);

#ifdef __cplusplus
}
#endif

#endif /* MOVIAN_CORE_H */
