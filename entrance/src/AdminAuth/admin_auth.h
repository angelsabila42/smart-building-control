#ifndef ADMIN_AUTH_H
#define ADMIN_AUTH_H

#include <stdbool.h>

void admin_auth_init(void);
bool admin_login(const char *pin);
void admin_logout(void);
bool admin_is_authenticated(void);

#endif
