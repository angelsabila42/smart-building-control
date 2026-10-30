#include <string.h>

#include "test_util.h"

static void fixed_random(void *context, uint8_t *buffer, size_t length)
{
    static uint8_t counter = 0;
    const uint8_t offset = (uint8_t)(context != NULL ? *(const int *)context : 0);

    counter++;

    for (size_t i = 0; i < length; i++) {
        buffer[i] = (uint8_t)(offset + counter + i);
    }
}

static void prepare(admin_auth_t *auth)
{
    int seed = 7;

    memset(auth, 0, sizeof(*auth));
    admin_auth_set_random_source(auth, fixed_random, &seed);
    admin_auth_init(auth, 1000);
}

void run_auth_tests(void)
{
    admin_auth_t auth;
    char error[96];

    prepare(&auth);
    CHECK_STR(auth.credentials.username, "admin");
    CHECK(auth.credentials.provisioned);
    CHECK(!auth.authenticated);
    CHECK(admin_auth_uses_default_password(&auth));
    CHECK(!admin_auth_session_valid(&auth, 1000));

    CHECK_INT(admin_auth_login(&auth, "admin", "ChangeMe123", 1000), ADMIN_AUTH_OK);
    CHECK(auth.authenticated);
    CHECK(admin_auth_session_valid(&auth, 1000));
    CHECK(admin_auth_session_valid(&auth, 1299));
    CHECK(!admin_auth_session_valid(&auth, 1300));
    CHECK(!auth.authenticated);
    CHECK_INT(admin_auth_session_remaining(&auth, 1000), 0);

    prepare(&auth);
    CHECK_INT(admin_auth_login(&auth, "ADMIN", "ChangeMe123", 1000), ADMIN_AUTH_OK);
    CHECK_INT(admin_auth_session_remaining(&auth, 1000), 300);
    admin_auth_touch(&auth, 1200);
    CHECK_INT(admin_auth_session_remaining(&auth, 1200), 300);
    CHECK(admin_auth_session_valid(&auth, 1499));
    CHECK(!admin_auth_session_valid(&auth, 1500));

    admin_auth_logout(&auth);
    CHECK(!admin_auth_session_valid(&auth, 1200));

    /* Wrong username and password. */
    prepare(&auth);
    CHECK_INT(admin_auth_login(&auth, "someone", "ChangeMe123", 1000),
              ADMIN_AUTH_ERR_CREDENTIALS);
    CHECK_INT(auth.failed_attempts, 1);
    CHECK_INT(admin_auth_login(&auth, "admin", "wrong-password", 1001),
              ADMIN_AUTH_ERR_CREDENTIALS);
    CHECK_INT(auth.failed_attempts, 2);
    CHECK_INT(admin_auth_login(&auth, "admin", "wrong-password", 1002),
              ADMIN_AUTH_ERR_LOCKED);
    CHECK_INT(admin_auth_lockout_remaining(&auth, 1002), 60);

    /* Correct credentials are refused while locked. */
    CHECK_INT(admin_auth_login(&auth, "admin", "ChangeMe123", 1061), ADMIN_AUTH_ERR_LOCKED);
    CHECK_INT(admin_auth_login(&auth, "admin", "ChangeMe123", 1062), ADMIN_AUTH_OK);
    CHECK_INT(auth.failed_attempts, 0);
    CHECK_INT(admin_auth_lockout_remaining(&auth, 1062), 0);

    /* Password policy. */
    CHECK(!admin_auth_password_acceptable("short", error, sizeof(error)));
    CHECK(!admin_auth_password_acceptable("allletters", error, sizeof(error)));
    CHECK(!admin_auth_password_acceptable("12345678", error, sizeof(error)));
    CHECK(!admin_auth_password_acceptable(NULL, error, sizeof(error)));
    CHECK(admin_auth_password_acceptable("Secret123", error, sizeof(error)));
    CHECK_STR(error, "");

    /* Changing the password. */
    prepare(&auth);
    CHECK_INT(admin_auth_change_password(&auth, "ChangeMe123", "NewPass123", 1000),
              ADMIN_AUTH_ERR_NOT_AUTHENTICATED);

    CHECK_INT(admin_auth_login(&auth, "admin", "ChangeMe123", 1000), ADMIN_AUTH_OK);
    CHECK_INT(admin_auth_change_password(&auth, "wrong", "NewPass123", 1000),
              ADMIN_AUTH_ERR_CREDENTIALS);
    CHECK_INT(admin_auth_change_password(&auth, "ChangeMe123", "short", 1000),
              ADMIN_AUTH_ERR_WEAK_PASSWORD);
    CHECK_INT(admin_auth_change_password(&auth, "ChangeMe123", "ChangeMe123", 1000),
              ADMIN_AUTH_ERR_WEAK_PASSWORD);
    CHECK_INT(admin_auth_change_password(&auth, "ChangeMe123", "NewPass123", 1000),
              ADMIN_AUTH_OK);
    CHECK(admin_auth_session_valid(&auth, 1000));
    CHECK(!admin_auth_uses_default_password(&auth));
    CHECK_STR(auth.credentials.username, "admin");

    admin_auth_logout(&auth);
    CHECK_INT(admin_auth_login(&auth, "admin", "ChangeMe123", 1001),
              ADMIN_AUTH_ERR_CREDENTIALS);
    CHECK_INT(admin_auth_login(&auth, "admin", "NewPass123", 1002), ADMIN_AUTH_OK);

    /* Provisioning rejects weak passwords and changes the salt. */
    prepare(&auth);
    CHECK_INT(admin_auth_provision(&auth, "root", "weak", 1000),
              ADMIN_AUTH_ERR_WEAK_PASSWORD);
    CHECK_INT(admin_auth_provision(&auth, "root", "RootPass123", 1000), ADMIN_AUTH_OK);
    CHECK_STR(auth.credentials.username, "root");
    CHECK_INT(admin_auth_login(&auth, "admin", "ChangeMe123", 1000),
              ADMIN_AUTH_ERR_CREDENTIALS);
    CHECK_INT(admin_auth_login(&auth, "root", "RootPass123", 1001), ADMIN_AUTH_OK);

    /* Hash derivation is deterministic for the same salt. */
    const uint8_t salt[16] = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16 };
    uint8_t first[ADMIN_AUTH_HASH_SIZE];
    uint8_t second[ADMIN_AUTH_HASH_SIZE];

    CHECK(admin_auth_hash_password(salt, sizeof(salt), "Example123", 100, first));
    CHECK(admin_auth_hash_password(salt, sizeof(salt), "Example123", 100, second));
    CHECK(memcmp(first, second, sizeof(first)) == 0);

    uint8_t third[ADMIN_AUTH_HASH_SIZE];

    CHECK(admin_auth_hash_password(salt, sizeof(salt), "Example124", 100, third));
    CHECK(memcmp(first, third, sizeof(first)) != 0);

    /* The salted hash must not be a plain SHA-256 of the password. */
    uint8_t plain[SHA256_DIGEST_SIZE];

    sha256("Example123", strlen("Example123"), plain);
    CHECK(memcmp(first, plain, sizeof(plain)) != 0);

    /* Logging a password change out and back in keeps the new password. */
    prepare(&auth);
    CHECK_INT(admin_auth_login(&auth, "admin", "ChangeMe123", 2000), ADMIN_AUTH_OK);
    CHECK_INT(admin_auth_change_password(&auth, "ChangeMe123", "Another123", 2000),
              ADMIN_AUTH_OK);
    CHECK_INT(admin_auth_login(&auth, "admin", "Another123", 2000), ADMIN_AUTH_OK);
}
