#ifndef BLE_H
#define BLE_H

#include <stdbool.h>

//is laptop listening? 
extern bool is_laptop_subscribed;

//compact setup for advertising and connecting for main
void initialize_bluetooth(void);

#endif