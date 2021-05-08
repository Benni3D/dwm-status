#ifndef DWM_STATUS_CONFIG_H
#define DWM_STATUS_CONFIG_H

// how many seconds between each refresh (default: 0.3s)
const float refresh_delay = 1.0f / 3.0f;

// how many seconds between each network check (default: 20s)
const unsigned network_delay = 20;

// how many times xsetroot has to fail to cause dwm-status to exit (default: 5)
const unsigned max_fails = 5;

// name of the battery (see /sys/class/power_supply)
#define BAT_NAME "BAT0"

// date format (see date(1))
const char* const date_format = "%a, %F %T";

// coinbase API URL
#define COINBASE_API "https://api.coinbase.com/v2/prices/ETH-USD/spot"

// ethereum update delay (default: 300s/5min)
const int ether_delay = 300;

#endif /* DWM_STATUS_CONFIG_H */
