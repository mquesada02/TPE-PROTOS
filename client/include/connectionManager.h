#ifndef TPE_PROTOS_CONNECTIONMANAGER_H
#define TPE_PROTOS_CONNECTIONMANAGER_H

#include "selector.h"

int setupLeekerSocket(const char *service, const char **errmsg);

struct Tracker * setupTrackerSocket(const char *ip, const char *port, const char **errmsg);

void leekerHandler(struct selector_key *key);

#endif //TPE_PROTOS_CONNECTIONMANAGER_H
