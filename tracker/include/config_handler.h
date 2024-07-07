#ifndef TPE_PDC_CONFIG_HANDLER_H
#define TPE_PDC_CONFIG_HANDLER_H

int init_config_socket(char *ip, int port, const char **err_msg);

void accept_connection(struct selector_key *key);

void close_connection(struct selector_key *key);

void close_config_sockets(void);

#endif //TPE_PDC_CONFIG_HANDLER_H
