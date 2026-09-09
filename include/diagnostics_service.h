#ifndef DIAGNOSTICS_SERVICE_H_
#define DIAGNOSTICS_SERVICE_H_

#define DIAGNOSTICS_SERVICE_PERIOD_MS 100U
#define DIAGNOSTICS_SERVICE_STACK_SIZE 1024
#define DIAGNOSTICS_SERVICE_PRIORITY 3

/** Start once, after diagnostics_init() and the supervised services' initialization. */
void diagnostics_service_init(void);

#endif /* DIAGNOSTICS_SERVICE_H_ */
