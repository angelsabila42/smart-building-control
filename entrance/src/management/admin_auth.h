#ifndef MANAGEMENT_ADMIN_AUTH_H
#define MANAGEMENT_ADMIN_AUTH_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "management/sha256.h"

#define ADMIN_AUTH_USERNAME_MAX 32
#define ADMIN_AUTH_PASSWORD_MIN 8
#define ADMIN_AUTH_PASSWORD_MAX 64
#define ADMIN_AUTH_SALT_SIZE 16
#define ADMIN_AUTH_HASH_SIZE SHA256_DIGEST_SIZE
#define ADMIN_AUTH_ITERATIONS 5000

#define ADMIN_AUTH_MAX_FAILED_ATTEMPTS 3
#define ADMIN_AUTH_LOCKOUT_SECONDS 60
#define ADMIN_AUTH_SESSION_TIMEOUT_SECONDS 300

#define ADMIN_AUTH_DEFAULT_USERNAME "admin"
#define ADMIN_AUTH_DEFAULT_PASSWORD "ChangeMe123"

/*
 * Administrator authentication for the management interface.
 *
 * Passwords are never stored in clear text.  A per-device salt and repeated
 * SHA-256 hashing are used (a deliberately simple key-derivation suitable for
 * a prototype; PBKDF2/HMAC would be the next step for production).  Sessions
 * expire after a period of inactivity and repeated failures lock the account
 * temporarily.  Commands in serial_menu.c use admin_auth_session_valid().
 */

typedef struct {
    char username[ADMIN_AUTH_USERNAME_MAX];
    uint8_t salt[ADMIN_AUTH_SALT_SIZE];
    uint8_t password_hash[ADMIN_AUTH_HASH_SIZE];
    uint32_t iterations;
    bool provisioned;
} admin_credentials_t;

typedef struct {
    admin_credentials_t credentials;
    bool authenticated;
    int64_t last_activity;
    int failed_attempts;
    int64_t locked_until;
    uint32_t successful_logins;
    void (*random_bytes)(void *context, uint8_t *buffer, size_t length);
    void *random_context;
} admin_auth_t;

typedef enum {
    ADMIN_AUTH_OK = 0,
    ADMIN_AUTH_ERR_CREDENTIALS,
    ADMIN_AUTH_ERR_LOCKED,
    ADMIN_AUTH_ERR_WEAK_PASSWORD,
    ADMIN_AUTH_ERR_NOT_AUTHENTICATED,
    ADMIN_AUTH_ERR_INVALID_ARGUMENT
} admin_auth_result_t;

/* Creates the default administrator account. */
void admin_auth_init(admin_auth_t *auth, int64_t now);

/* Optional entropy source; defaults to a best-effort software fallback. */
void admin_auth_set_random_source(admin_auth_t *auth,
                                  void (*random_bytes)(void *context, uint8_t *buffer, size_t length),
                                  void *context);

/* Replaces the account (used by the default setup and NVS restore). */
admin_auth_result_t admin_auth_provision(admin_auth_t *auth,
                                         const char *username,
                                         const char *password,
                                         int64_t now);

admin_auth_result_t admin_auth_login(admin_auth_t *auth,
                                     const char *username,
                                     const char *password,
                                     int64_t now);

/* True for an authenticated session that has not timed out. */
bool admin_auth_session_valid(admin_auth_t *auth, int64_t now);

/* Extends an active session; called after each authorised command. */
void admin_auth_touch(admin_auth_t *auth, int64_t now);

void admin_auth_logout(admin_auth_t *auth);

/* Seconds until the session expires, or 0 when not authenticated. */
int64_t admin_auth_session_remaining(const admin_auth_t *auth, int64_t now);

/* Seconds until a lockout ends, or 0 when not locked. */
int64_t admin_auth_lockout_remaining(const admin_auth_t *auth, int64_t now);

admin_auth_result_t admin_auth_change_password(admin_auth_t *auth,
                                               const char *old_password,
                                               const char *new_password,
                                               int64_t now);

/* Password policy: 8-64 characters with at least one letter and one digit. */
bool admin_auth_password_acceptable(const char *password, char *err, size_t err_size);

/* Fills salt/iterations/hash for a password; exposed for tests and NVS import. */
bool admin_auth_hash_password(const uint8_t *salt,
                              size_t salt_length,
                              const char *password,
                              uint32_t iterations,
                              uint8_t hash[ADMIN_AUTH_HASH_SIZE]);

/* True when the account still uses the shipped default password. */
bool admin_auth_uses_default_password(const admin_auth_t *auth);

#endif /* MANAGEMENT_ADMIN_AUTH_H */
