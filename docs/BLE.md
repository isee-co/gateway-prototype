# BLE Configuration of Gateway

The gateway should check its configuration status at boot time. If WiFi and MQTT server configuration has not been set yet, Bluetooth will be activated and configuration service will be ready to work.

***Note:*** The configuration service is designed based on BLE 5 features.

***Note:*** The device name should be "zigbee-gateway".

## 1. Configuration Service

This service is a [GATT](https://en.wikipedia.org/wiki/GATT) service with two characteristics with the following information

| Type | Description | UUID | Access type |
|------|-------------|------|-------------|
| Service| Configuration | 0x5501 | Primary |
| Characteristic | Events | 0xCC02 | Notify |
| Characteristic | JSON RPC | 0xCC03 | Notify & Write |

### 1.1 Events Characteristic

This characteristic is responsible for sending events notification to the client device. The events will be sent with JSON format and the following template.

***Event template:***
```json
{
// event type is one of the event type in the table below
"type": "event type",
// event name is one of the event name in the table below
"event": "event name",
// fialure reason only if event type is failed
"reason": "fialure reason"
}
```


| Event Type |  Value |
|------------|--------|
| WIFI |  INIT / CONNCTIND / CONNECTED / DISCONNECTED / FAILED |
| MQTT |  INIT / CONNECTED / DISCONNECTED / FAILED |

### 1.2 JSON RPC Specification

The client sends a method execution request to the device by writing to this characteristic and receives the result as a notification.

***Request template:***
```json
{  
"id": 123,
// name of method should be call
"method": "method name",
// parameters should be passed to the method
"params": {} // object or string
}
```

***Response template:***
```json
{
"id": 123,
// method call is success or not.
"success": true,
// method returned result, if success
"data": {},
// message of occurred error
"error": "error message"
}
```

## 2. RPC Methods

### 2.1 Set WiFi Configuration

This method is used to set wifi configuration.

***Method name:*** SetWifiConfig

***Parameters:***
```json
{
"ssid": "wifi ssid",
"pass": "wifi password"
}
```

### 2.2 Set Home ID

This method is used to set home id.

***Method name:*** SetHomeId

***Parameters:***
```
"home id" // home id should be url safe base64 string 
```

### 2.3 Set MQTT Configuration

This method is used to set mqtt configuration.

***Method name:*** SetMqttConfig

***Parameters:***
```json
{
// mqtt server hostname or ip
"host": "hostname",
"port": 1883,
"user": "username",
"pass": "password"
}
