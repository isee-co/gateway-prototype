# MQTT Connection

> **Note:** The gateway must use a mqtt client that supports MQTT v5 Features such as User Properties, Retain Messages, etc.

***Requirement parameters:***

***Gateway_ID:***: every gateway must have a unique 64-bit serial number. you must encode it in UrlBase64 without padding (11 characters).

***Client_ID:***: same as Gateway_ID.

***Home_ID:***: defined in the setup duration.

***Username:***: defined in the setup duration.

***Password:***: defined in the setup duration.

***Connection URL:***: defined in the setup duration.

***Connection Port:***: defined in the setup duration.


# 1. MQTT RPC

The gateway should subscribe to the ***Request topic***, invoke the requested method with the sent parameters, and then publish the result in the ***Response topic***.

| #        | topic                                          | type      | Qos | Retain | User Properties*                                                                                                    |
|----------|------------------------------------------------|-----------|:---:|:------:|---------------------------------------------------------------------------------------------------------------------|
| Request  | / ${Home_ID} / RPC_REQ / ${Gateway_ID}         | Subscribe |  2  | false  | { "Response topic": "***Requester_ID***", <br> "Correlation data": 123 // sequential request id } |
| Response | / ${Home_ID} / RPC_RES / ${***Requester_ID***} | Publish   |  1  | false  | { "Correlation data": 123 // same as request id }             |

> ***User Properties** are the user-defined properties that allow users to add their metadata to MQTT v5 messages and transmit additional user-defined information to expand more application scenarios.for more details, please refer to [User Properties](https://www.emqx.com/en/blog/mqtt5-user-properties). and [Request/Response](https://www.emqx.com/en/blog/mqtt5-request-response) pattern.

***Request body:***
```javascript
{
    // name of method should be call
    "method": "method name",
    // parameters should be passed to the method
    "params": {}
}
```

***Response body:***
```javascript
{
  // method call is success or not. 
  "success": true,
  // method returned result, if success
  "data": {},
  // message of occurred error
  "error": "error message"
}
```

***Example:***

```javascript
request: { "method": "setJoinPermit", "params": { "state": "enable", "timeout": 10 } }
        
response: { "success": true, "data": { "state": "enabled", "timeout": 10 } }
```

## 1.1 RPC Methods

### 1.1.1 Set Join Permit 

This method is used to enable or disable permit join. when the permit join is enabled, the gateway will accept the join request from the device for specific amount of time (default 10 seconds), and then reject the join request.

***Method name:*** setJoinPermit

***Parameters:***

```javascript
{
  // this parameter is required, and should be "enable" or "disable"
  "state": "enable" | "disable",
  // this parameter is optional, default value is 10 and set allowed time for joining.
  "timeout": 10,
  // this parameter is optional, and allow joining via a specific device id.
  "device_id": "device_id",
  // this parameter is optional, and set installation code of the device in zigbee 3.0 standard.
  "install_code": "code"
}
``` 
***Response data:***

```javascript
{ "state": "enabled" | "disabled", "timeout": 10 }
```

### 1.1.2 Set Device State

This method is used to set device state. the gateway must process the received status and send the necessary commands to the device.

***Method name:*** setDevState

***Parameters:***

```javascript
{
  "device_id": uint64,
  "endpoint_id": uint8,
  // device state type, see on Device state section
  "type": enum8,
  // device state object
  "state": DeviceState
}
```

***Response data:***

```javascript
{} // empty object
```

### 1.1.3 Get Device State

This method is used to get device state. the gateway must read attributes from the device based on the specified ***cluster***.then map to ***DeviceState*** and send to requester client. 

***Method name:*** getDevState

***Parameters:***

```javascript
{
  "device_id": uint64,
  "endpoint_id": uint8,
  // device state type, see on Device state section
  "type": enum8
}
```

***Response data:***

```javascript
{
  // device state object
  "state": DeviceState
}
```

### 1.1.4 Set Log Level

This method is used to set log level.

***Method name:*** setLogLevel

***Parameters:***

```javascript
{ "level": "debug" | "info" | "warn" | "error" }
```

***Response data:***

```javascript
{ "level": "debug" | "info" | "warn" | "error" }
```

## 1.2 Device State


***Device state type:***

| Enum value | Type description |
|------------|------------------|
| 0x06       | On/Off Switch    |
| 0x08       | Dimmable Light   |
| 0x0102     | window Covering  |
| 0x0204     | AC Controller    |

### 1.1.4.1 On/Off Switch

```javascript
{ "onOff": bool }
```

### 1.1.4.2 Dimmable Light

```javascript
{ "brightness": 0 - 100 }
```

### 1.1.4.3 window Covering

```javascript
{ 
  // Percentage of cover opening
  "percent": 0 - 100 
}
```

### 1.1.4.4 AC Controller

```javascript
{
  "power": bool,
  // Temperature in Celsius
  "temp": 15 - 40,
  // Curent temperature of room, this parameter only exists on response
  "room_temp": floot,
  // operation mode: auto | cool | heat
  "op_mode": "auto" | "cool" | "heat",
  "fan_speed": "auto" | "low" | "medium" | "high"
}
```

# 2. Events topics

## 2.1 Device state

The gateway must collect received state from the device.then map to ***DeviceState*** and publish to below topic with ***Qos:*** 0 and ***Retain:*** true.

| topic                                   | type    | Qos | Retain | User Properties |
|-----------------------------------------|---------|-----|--------|-----------------|
| / ${Home_ID} / DEV_STATE / ${Device_ID} | Publish | 1   | true   |                 |

***Message body:***
```javascript
{
  "timestamp": uint64,
  // device state type, see on Device state section
  "type": enum8,
  // device state object
  "state": DeviceState
}
```

## 2.2 Device join

When permit join is enabled, the gateway processes the received join request from the device. If joining is successful, the gateway will discover the device description and all endpoint descriptions, then publish the information. Otherwise, it will publish a failure response.

| topic                                   | type    | Qos | Retain | User Properties |
|-----------------------------------------|---------|-----|--------|-----------------|
| / ${Home_ID} / DEV_JOIN / ${Gateway_ID} | Publish | 1   | false  |                 |

***Success message:***

```javascript
{
  // zigbee ieee 64 bit address
  "device_id": uint64,
  "device_info": {
    "model": string,
    "vendor": string,
    "description": string,
    "power_source": string,
    "sw_version": string,
    "hw_version": string,
  },
  "endpoints": [
    {
      "id": uint8,
      "description": string,
      "type": enum8
    }
  ]
}
```

***Failure message:***

```javascript
{
  // zigbee ieee 64 bit address
  "device_id": uint64,
  "reason": string
}
```

## 2.3 Logs

The gateway should publish the device logs to the below topic.

| topic                               | type    | Qos | Retain | User Properties |
|-------------------------------------|---------|-----|--------|-----------------|
| / ${Home_ID} / LOGS / ${Gateway_ID} | Publish | 0   | false  |                 |
