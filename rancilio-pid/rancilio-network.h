#ifndef _rancilio_network_h
#define _rancilio_network_h

bool InitNetworking();
void checkWifi(bool);
void HandleOTA();
void InitOTA();

extern bool forceOffline;

#endif // _connection_H
