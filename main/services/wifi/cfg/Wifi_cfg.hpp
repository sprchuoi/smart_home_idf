#ifndef __WIFI_TYPEDEF_HPP__
#define __WIFI_TYPEDEF_HPP__

#define WIFI_SSID_MAX_LEN     32
#define WIFI_PASS_MAX_LEN     64


typedef struct {
    char ssid[WIFI_SSID_MAX_LEN];
    char password[WIFI_PASS_MAX_LEN];
} WifiConfigInfo_st; 



#endif // __WIFI_TYPEDEF_HPP__