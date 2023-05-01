#ifndef _connection_H
#define _connection_H

extern bool forceOffline;

void InitWifi(bool);
void checkWifi(bool);
void HandleOTA();
void InitOTA();

extern void disableBlynkTemporary();

#endif // _connection_H
