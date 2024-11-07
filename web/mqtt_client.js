// DOM Elements
const mqttUrl = document.getElementById('mqtt_url');
const mqttUsername = document.getElementById('mqtt_user');
const mqttPassword = document.getElementById('mqtt_pass');
const mqttStatus = document.getElementById('mqtt_status');
const mqttConnect = document.getElementById('mqtt_connect');
const homeId = document.getElementById('home_id');
const gatewayId = document.getElementById('gateway_id');
const gatewayStatus = document.getElementById('gateway_status');
const gatewayConnect = document.getElementById('gateway_connect');
const joinLoader = document.getElementById('join_loader');
const joinDeviceInfo = document.getElementById('join_device_info');
const joinPermit = document.getElementById('join_permit');
const deviceId = document.getElementById('device_id');
const endpointId = document.getElementById('endpoint_id');
const switchState = document.getElementById('switch_state');
const toggleSwitch = document.getElementById('toggle_switch');

var mattClient;
var mqttPubOption;
var gatewayOnline = false;
var topicRpcRequest;
var topicRpcResponse;
var topicDeviceState;
var topicJoinNewDeivec;
var topicGatewayStatus;
const client_id = "test_client";

mqttConnect.addEventListener('click', mqtt_connect);
gatewayConnect.addEventListener('click', gateway_connect);
joinPermit.addEventListener('click', join_permit);
toggleSwitch.addEventListener('click', toggle_switch);

function mqtt_connect() {
  mqttStatus.innerHTML = 'Connecting...';
  const url = mqttUrl.value;
  const options = {
    clean: true, // Clean session
    protocolVersion: 5,
    connectTimeout: 4000,
    clientId: client_id,
    username: mqttUsername.value,
    password: mqttPassword.value,
  };

  mqttPubOption = {
    qos: 2,
    properties: {
      responseTopic: client_id,
      correlationData: "123",
    }
  }

  mattClient = mqtt.connect(url, options);
  console.log("mqtt connect with url:", url);
  mattClient.on('connect', () => {
    mqttStatus.innerHTML = 'Connected';
  })

  mattClient.on('disconnect', function (err) {
    mqttStatus.innerHTML = 'Disconnected';
  })

  mattClient.on('error', (err) => {
    console.log("mqtt error:", err)
    mqttStatus.innerHTML = 'Error' + err;
  })

  // Receive messages
  mattClient.on('message', (topic, message) => {
    console.log('Received message on topic ' + topic + ': ' + message.toString());
    console.log("topic:", topic);
    if (topic === topicGatewayStatus) {
      onGatewayStatusChange(message.toString());
    } else if (topic === topicJoinNewDeivec) {
      onJoinDeviceEvent(JSON.parse(message.toString()));
    } else if (topic.toString().startsWith(topicDeviceState)) {
      const parts = topic.toString().split("/");
      onDeviceStateEvent(parts[3], parts[4],JSON.parse(message.toString()));
    }
    else if (topic === topicRpcResponse) {
    }
  })
}

function onGatewayStatusChange(status) {
  switch (status) {
    case "online":
      gatewayOnline = true;
      gatewayStatus.innerHTML = 'Online';
      break;
    case "offline":
      gatewayOnline = false;
      gatewayStatus.innerHTML = 'Offline';
      break;
  }
}

function onJoinDeviceEvent(deviceInfo) {
  joinLoader.style.display = "none";
  joinDeviceInfo.style.display = "block";

  if (deviceInfo) {
    joinDeviceInfo.innerHTML = "Device ID: " + deviceInfo.device_id + "<br/>";
    deviceInfo.endpoints.forEach(endpoint => {
      joinDeviceInfo.innerHTML += `Endpoint(${endpoint.id}) : ${endpoint.description} <br/>`;
    });
  }
}

function onDeviceStateEvent(devId, endpId,event) {
  console.log("onDeviceStateEvent:", event);
  if (event && devId == deviceId.value && endpId == endpointId.value) {
    switchState.innerHTML = event.state.onOff ? "on" : "off";
  }
}

function checkMqttConnection() {
  if (!mattClient || !mattClient.connected) {
    window.alert("Please connect to MQTT server first!");
    return false;
  }

  return true;
}

function gateway_connect() {
  if (checkMqttConnection()) {
    topicRpcRequest = `/${homeId.value}/RPCREQ/${gatewayId.value}`
    topicRpcResponse = `/${homeId.value}/RPCRES/${client_id}`
    topicDeviceState = `/${homeId.value}/DEVSTATE`
    topicJoinNewDeivec = `/${homeId.value}/DEVJOIN/${gatewayId.value}`
    topicGatewayStatus = `/${homeId.value}/EVENTS/${gatewayId.value}/STATUS`

    // Subscribe gateway stuts topic
    mattClient.subscribe(topicGatewayStatus);

    // Subscribe device state topic
    mattClient.subscribe(topicDeviceState + "/+/+");

    // Subscribe rpc reponse topic
    mattClient.subscribe(topicRpcRequest);
  }
}

function join_permit() {
  if (checkMqttConnection()) {
    const rpcReq = {
      method: "SetJoinPermit",
      params: { state: "enable", timeout: 60 }
    }
    // Subscribe join new device topic
    mattClient.subscribe(topicJoinNewDeivec);
    // Publish join permit request
    mattClient.publish(topicRpcRequest, JSON.stringify(rpcReq), mqttPubOption);

    joinLoader.style.display = "block";
    joinDeviceInfo.style.display = "none";

    setTimeout(() => {
      joinLoader.style.display = "none";
      joinDeviceInfo.style.display = "block";
      mattClient.unsubscribe(topicJoinNewDeivec);
    }, 60 * 1000);
  }
}

function toggle_switch() {
  if (checkMqttConnection()) {
    const state = switchState.value == "on";
    const rpcReq = {
      method: "SetDeviceState",
      params: {
        device_id: deviceId.value,
        endpoint: +endpointId.value,
        type: 6,
        state: { "onOff": !state },
      }
    }
    mattClient.publish(topicRpcRequest, JSON.stringify(rpcReq), mqttPubOption);
    switchState.value = !state ? "on" : "off";
  }
}