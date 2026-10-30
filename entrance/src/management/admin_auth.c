#include "management/admin_auth.h"

#include <stdio.h>
#include <string.h>

#include "management/time_utils.h"

static void copy_text(char *destination, size_t size, const char *source)
{
    if (destination == NULL || size == 0) {
        return;
    }

    if (source == NULL) {
        destination[0] = '\0';
        return;
    }

    snprintf(destination, size, "%s", source);
}

static bool equals_ignore_case(const char *left, const char *right)
{
    if (left == NULL || right == NULL) {
        return false;
    }

    while (*left != '\0' && *right != '\0') {
        char a = *left;
        char b = *right;

        if (a >= 'A' && a <= 'Z') {
            a = (char)(a - 'A' + 'a');
        }
        if (b >= 'A' && b <= 'Z') {
            b = (char)(b - 'A' + 'a');
        }

        if (a != b) {
            return false;
        }

        left++;
        right++;
    }

    return *left == '\0' && *right == '\0';
}

static bool constant_time_equal(const uint8_t *left, const uint8_t *right, size_t length)
{
    uint8_t difference = 0;

    for (size_t i = 0; i < length; i++) {
        difference |= (uint8_t)(left[i] ^ right[i]);
    }

    return difference == 0;
}

static void fallback_random(void *context, uint8_t *buffer, size_t length)
{
    (void)context;

    static uint64_t state = 0;

    if (state == 0) {
        state = (uint64_t)(uintptr_t)buffer ^ (uint64_t)time(NULL) ^ 0x9e3779b97f4a7c15ull;
    }

    for (size_t i = 0; i < length; i++) {
        state ^= state << 13;
        state ^= state >> 7;
        state ^= state << 17;
        buffer[i] = (uint8_t)(state >> 24);
    }
}

static void fill_random(const admin_auth_t *auth, uint8_t *buffer, size_t length)
{
    if (auth->random_bytes != NULL) {
        auth->random_bytes(auth->random_context, buffer, length);
    } else {
        fallback_random(NULL, buffer, length);
    }
}

bool admin_auth_hash_password(const uint8_t *salt,
                              size_t salt_length,
                              const char *password,
                              uint32_t iterations,
                              uint8_t hash[ADMIN_AUTH_HASH_SIZE])
{
    if (salt == NULL || password == NULL || hash == NULL || iterations == 0) {
        return false;
    }

    sha256_context_t context;

    sha256_init(&context);
    sha256_update(&context, salt, salt_length);
    sha256_update(&context, password, strlen(password));
    sha256_final(&context, hash);

    for (uint32_t round = 1; round < iterations; round++) {
        sha256_init(&context);
        sha256_update(&context, hash, ADMIN_AUTH_HASH_SIZE);
        sha256_update(&context, salt, salt_length);
        sha256_update(&context, password, strlen(password));
        sha256_final(&context, hash);
    }

    return true;
}

bool admin_auth_password_acceptable(const char *password, char *err, size_t err_size)
{
    if (err != NULL && err_size > 0) {
        err[0] = '\0';
    }

    if (password == NULL) {
        if (err != NULL && err_size > 0) {
            snprintf(err, err_size, "password is missing");
        }
        return false;
    }

    const size_t length = strlen(password);

    if (length < ADMIN_AUTH_PASSWORD_MIN || length > ADMIN_AUTH_PASSWORD_MAX) {
        if (err != NULL && err_size > 0) {
            snprintf(err, err_size, "password must be %d-%d characters",
                     ADMIN_AUTH_PASSWORD_MIN, ADMIN_AUTH_PASSWORD_MAX);
        }
        return false;
    }

    bool has_letter = false;
    bool has_digit = false;

    for (size_t i = 0; i < length; i++) {
        const char value = password[i];

        if ((value >= 'a' && value <= 'z') || (value >= 'A' && value <= 'Z')) {
            has_letter = true;
        } else if (value >= '0' && value <= '9') {
            has_digit = true;
        }
    }

    if (!has_letter || !has_digit) {
        if (err != NULL && err_size > 0) {
            snprintf(err, err_size, "password must contain a letter and a digit");
        }
        return false;
    }

    return true;
}

void admin_auth_set_random_source(admin_auth_t *auth,
                                  void (*random_bytes)(void *context, uint8_t *buffer, size_t length),
                                  void *context)
{
    if (auth == NULL) {
        return;
    }

    auth->random_bytes = random_bytes;
    auth->random_context = context;
}

admin_auth_result_t admin_auth_provision(admin_auth_t *auth,
                                         const char *username,
                                         const char *password,
                                         int64_t now)
{
    if (auth == NULL || username == NULL || username[0] == '\0' || password == NULL) {
        return ADMIN_AUTH_ERR_INVALID_ARGUMENT;
    }

    char error[64];

    if (!admin_auth_password_acceptable(password, error, sizeof(error))) {
        return ADMIN_AUTH_ERR_WEAK_PASSWORD;
    }

    admin_credentials_t *credentials = &auth->credentials;

    memset(credentials, 0, sizeof(*credentials));
    copy_text(credentials->username, sizeof(credentials->username), username);
    credentials->iterations = ADMIN_AUTH_ITERATIONS;
    fill_random(auth, credentials->salt, sizeof(credentials->salt));

    if (!admin_auth_hash_password(credentials->salt,
                                  sizeof(credentials->salt),
                                  password,
                                  credentials->iterations,
                                  credentials->password_hash)) {
        return ADMIN_AUTH_ERR_INVALID_ARGUMENT;
    }

    credentials->provisioned = true;
    auth->authenticated = false;
    auth->last_activity = now;
    auth->failed_attempts = 0;
    auth->locked_until = 0;

    return ADMIN_AUTH_OK;
}

void admin_auth_init(admin_auth_t *auth, int64_t now)
{
    if (auth == NULL) {
        return;
    }

    /* Keep a previously configured entropy source across initialisation. */
    void (*random_bytes)(void *context, uint8_t *buffer, size_t length) = auth->random_bytes;
    void *random_context = auth->random_context;

    memset(auth, 0, sizeof(*auth));
    auth->random_bytes = random_bytes;
    auth->random_context = random_context;
    auth->last_activity = now;

    admin_auth_provision(auth,
                         ADMIN_AUTH_DEFAULT_USERNAME,
                         ADMIN_AUTH_DEFAULT_PASSWORD,
                         now);
}

static void clear_expired_lockout(admin_auth_t *auth, int64_t now)
{
    if (auth->locked_until > 0 && now >= auth->locked_until) {
        auth->locked_until = 0;
        auth->failed_attempts = 0;
    }
}

admin_auth_result_t admin_auth_login(admin_auth_t *auth,
                                     const char *username,
                                     const char *password,
                                     int64_t now)
{
    if (auth == NULL || username == NULL || password == NULL) {
        return ADMIN_AUTH_ERR_INVALID_ARGUMENT;
    }

    if (!auth->credentials.provisioned) {
        return ADMIN_AUTH_ERR_CREDENTIALS;
    }

    if (admin_auth_lockout_remaining(auth, now) > 0) {
        return ADMIN_AUTH_ERR_LOCKED;
    }

    clear_expired_lockout(auth, now);

    uint8_t candidate[ADMIN_AUTH_HASH_SIZE];

    if (!admin_auth_hash_password(auth->credentials.salt,
                                  sizeof(auth->credentials.salt),
                                  password,
                                  auth->credentials.iterations,
                                  candidate)) {
        return ADMIN_AUTH_ERR_INVALID_ARGUMENT;
    }

    const bool username_matches =
        equals_ignore_case(username, auth->credentials.username);
    const bool password_matches =
        constant_time_equal(candidate, auth->credentials.password_hash, ADMIN_AUTH_HASH_SIZE);

    if (username_matches && password_matches) {
        auth->authenticated = true;
        auth->last_activity = now;
        auth->failed_attempts = 0;
        auth->successful_logins++;
        return ADMIN_AUTH_OK;
    }

    auth->authenticated = false;
    auth->failed_attempts++;

    if (auth->failed_attempts >= ADMIN_AUTH_MAX_FAILED_ATTEMPTS) {
        auth->locked_until = now + ADMIN_AUTH_LOCKOUT_SECONDS;
        auth->failed_attempts = 0;
        return ADMIN_AUTH_ERR_LOCKED;
    }

    return ADMIN_AUTH_ERR_CREDENTIALS;
}

bool admin_auth_session_valid(admin_auth_t *auth, int64_t now)
{
    if (auth == NULL || !auth->authenticated) {
        return false;
    }

    const int64_t idle = now - auth->last_activity;

    if (idle < 0 || idle >= ADMIN_AUTH_SESSION_TIMEOUT_SECONDS) {
        auth->authenticated = false;
        return false;
    }

    return true;
}

void admin_auth_touch(admin_auth_t *auth, int64_t now)
{
    if (auth != NULL && admin_auth_session_valid(auth, now)) {
        auth->last_activity = now;
    }
}

void admin_auth_logout(admin_auth_t *auth)
{
    if (auth != NULL) {
        auth->authenticated = false;
    }
}

int64_t admin_auth_session_remaining(const admin_auth_t *auth, int64_t now)
{
    if (auth == NULL || !auth->authenticated) {
        return 0;
    }

    const int64_t idle = now - auth->last_activity;

    if (idle < 0 || idle >= ADMIN_AUTH_SESSION_TIMEOUT_SECONDS) {
        return 0;
    }

    return ADMIN_AUTH_SESSION_TIMEOUT_SECONDS - idle;
}

int64_t admin_auth_lockout_remaining(const admin_auth_t *auth, int64_t now)
{
    if (auth == NULL || auth->locked_until <= 0) {
        return 0;
    }

    if (now >= auth->locked_until) {
        return 0;
    }

    return auth->locked_until - now;
}

bool admin_auth_uses_default_password(const admin_auth_t *auth)
{
    if (auth == NULL || !auth->credentials.provisioned) {
        return true;
    }

    uint8_t candidate[ADMIN_AUTH_HASH_SIZE];

    if (!admin_auth_hash_password(auth->credentials.salt,
                                  sizeof(auth->credentials.salt),
                                  ADMIN_AUTH_DEFAULT_PASSWORD,
                                  auth->credentials.iterations,
                                  candidate)) {
        return true;
    }

    return constant_time_equal(candidate, auth->credentials.password_hash, ADMIN_AUTH_HASH_SIZE);
}

admin_auth_result_t admin_auth_change_password(admin_auth_t *auth,
                                               const char *old_password,
                                               const char *new_password,
                                               int64_t now)
{
    if (auth == NULL || old_password == NULL || new_password == NULL) {
        return ADMIN_AUTH_ERR_INVALID_ARGUMENT;
    }

    if (!admin_auth_session_valid(auth, now)) {
        return ADMIN_AUTH_ERR_NOT_AUTHENTICATED;
    }

    uint8_t current[ADMIN_AUTH_HASH_SIZE];

    if (!admin_auth_hash_password(auth->credentials.salt,
                                  sizeof(auth->credentials.salt),
                                  old_password,
                                  auth->credentials.iterations,
                                  current)) {
        return ADMIN_AUTH_ERR_INVALID_ARGUMENT;
    }

    if (!constant_time_equal(current, auth->credentials.password_hash, ADMIN_AUTH_HASH_SIZE)) {
        return ADMIN_AUTH_ERR_CREDENTIALS;
    }

    char error[64];

    if (!admin_auth_password_acceptable(new_password, error, sizeof(error))) {
        return ADMIN_AUTH_ERR_WEAK_PASSWORD;
    }

    if (strcmp(old_password, new_password) == 0) {
        return ADMIN_AUTH_ERR_WEAK_PASSWORD;
    }

    char username[ADMIN_AUTH_USERNAME_MAX];

    copy_text(username, sizeof(username), auth->credentials.username);

    admin_auth_result_t result = admin_auth_provision(auth, username, new_password, now);

    if (result == ADMIN_AUTH_OK) {
        auth->authenticated = true;
        auth->last_activity = now;
    }

    return result;
}
