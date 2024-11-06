// DOM Elements
const cfgBleConnect = document.getElementById('cfg_ble_connect');
const cfgBleDisconnect = document.getElementById('cfg_ble_disconnect');
const cfgBleState = document.getElementById('cfg_ble_state');
const cfgWifiSsid = document.getElementById('cfg_wifi_ssid');
const cfgWifiPass = document.getElementById('cfg_wifi_pass');
const cfgWifiSave = document.getElementById('cfg_wifi_save');
const cfgHomeId = document.getElementById('cfg_home_id');
const cfgHomeIdSave = document.getElementById('cfg_home_id_save');
const cfgMqttServer = document.getElementById('cfg_mqtt_server');
const cfgMqttPort = document.getElementById('cfg_mqtt_port');
const cfgMqttUser = document.getElementById('cfg_mqtt_user');
const cfgMqttPass = document.getElementById('cfg_mqtt_pass');
const cfgMqttSave = document.getElementById('cfg_mqtt_save');
const cfgTextLogs = document.getElementById('cfg_logs');

//Define BLE Device Specs
const BLE_DEVICE_NAME = 'Zigbee-Gateway';
const BLE_SERVICE_ID = 0x5501;
const BLE_EVENTS_CHAR_ID = 0xCC02;
const BLE_RPC_CHAR_ID = 0xCC03;


//Global Variables to Handle Bluetooth
var bleServer;
var bleService;
var bleRpcChar;
var bleEventsChar;

// Connect Button (search for BLE Devices only if BLE is available)
cfgBleConnect.addEventListener('click', (event) => {
    if (isWebBluetoothEnabled()) {
        connectToDevice();
    }
});

// Disconnect Button
cfgBleDisconnect.addEventListener('click', disconnectDevice);

// wifi config Save
cfgWifiSave.addEventListener('click', (event) => {
    const config = { ssid: cfgWifiSsid.value, pass: cfgWifiPass.value, }
    const request = { id: 123, method: 'SetWifiConfig', payload: config }
    sendRpcRequest(JSON.stringify(request));
});

// Home ID Save
cfgHomeIdSave.addEventListener('click', (event) => {
    const request = { id: 123, method: 'SetHomeId', payload: cfgHomeId.value }
    sendRpcRequest(JSON.stringify(request));
});

// mqtt config Save
cfgMqttSave.addEventListener('click', (event) => {
    const config = {
        host: cfgMqttServer.value,
        port: +cfgMqttPort.value,
        user: mqttUsername.value,
        pass: mqttPassword.value
    }
    const request = { id: 123, method: 'SetMqttConfig', payload: config }
    sendRpcRequest(JSON.stringify(request));
});

// Check if BLE is available in your Browser
async function isWebBluetoothEnabled() {
    if (!navigator.bluetooth) {
        console.log('Web Bluetooth API is not available in this browser!');
        cfgBleState.innerHTML = "Web Bluetooth API is not available in this browser/device!";
        return false
    }
    var availability = await navigator.bluetooth.getAvailability();
    if (!availability) {
        console.log('Web Bluetooth adapter is not available in this browser!');
        cfgBleState.innerHTML = "Web Bluetooth adapter is not available in this browser/device!";
        return false
    }
    console.log('Web Bluetooth API supported in this browser.');
    return true
}

// Connect to BLE Device and Enable Notifications
function connectToDevice() {
    console.log('Initializing Bluetooth...');
    navigator.bluetooth.requestDevice({
        filters: [{ name: BLE_DEVICE_NAME }],
        optionalServices: [BLE_SERVICE_ID]
    })
        .then(device => {
            console.log('Device Selected:', device.name);
            cfgBleConnect.style.display = "none";
            cfgBleDisconnect.style.display = "inline-block";
            cfgBleState.innerHTML = 'Connected to device ' + device.name;
            cfgBleState.style.color = "#24af37";
            device.addEventListener('gattserverdisconnected', onDisconnected);
            return device.gatt.connect();
        })
        .then(gattServer => {
            bleServer = gattServer;
            console.log("Connected to GATT Server");
            return bleServer.getPrimaryService(BLE_SERVICE_ID);
        })
        .then(service => {
            bleService = service;
            console.log("Service discovered:", service.uuid);
            return service.getCharacteristic(BLE_RPC_CHAR_ID);
        })
        .then(characteristic => {
            console.log("Characteristic discovered:", characteristic.uuid);
            bleRpcChar = characteristic;
            characteristic.addEventListener('characteristicvaluechanged', handleCharNotification);
            characteristic.startNotifications();
            return bleService.getCharacteristic(BLE_EVENTS_CHAR_ID);
        })
        .then(characteristic => {
            console.log("Characteristic discovered:", characteristic.uuid);
            bleEventsChar = characteristic;
            characteristic.addEventListener('characteristicvaluechanged', handleCharNotification);
            setTimeout(() => {
                characteristic.startNotifications();
            }, 200)
        })
        .catch(error => {
            console.log('Error: ', error);
        })
}

function onDisconnected(event) {
    cfgBleConnect.style.display = "inline-block";
    cfgBleDisconnect.style.display = "none";
    console.log('Device Disconnected:', event.target.device.name);
    cfgBleState.innerHTML = "Device disconnected";
    cfgBleState.style.color = "#d13a30";

    connectToDevice();
}

function handleCharNotification(event) {
    const newValueReceived = new TextDecoder().decode(event.target.value);
    const source = event.target.uuid == bleRpcChar.uuid ? "RpcRes" : "Event";
    setTimeout(() => {
        cfgTextLogs.innerHTML += `${getTime()} ${source}:  ${newValueReceived} \n`;
    }, 100);
}

function sendRpcRequest(req) {
    if (bleServer && bleServer.connected) {
        var enc = new TextEncoder();
        const data = enc.encode(req);
        bleRpcChar.writeValue(data).then(() => {
            cfgTextLogs.innerHTML += `${getTime()} RpcReq:\t${req} \n`;
        }).catch(error => {
            cfgTextLogs.innerHTML += `${getTime()} Error:\t${error} \n`;
        });
    } else {
        console.error("Bluetooth is not connected. Cannot write to characteristic.")
        window.alert("Bluetooth is not connected. Cannot write to characteristic. \n Connect to BLE first!")
    }
}

function disconnectDevice() {
    console.log("Disconnect Device.");
    if (bleServer && bleServer.connected) {
        if (bleRpcChar) {
            bleRpcChar.stopNotifications()
                .then(() => {
                    console.log("Notifications Stopped");
                    return bleServer.disconnect();
                })
                .then(() => {
                    console.log("Device Disconnected");
                    bleStateContainer.innerHTML = "Device Disconnected";
                    bleStateContainer.style.color = "#d13a30";

                })
                .catch(error => {
                    console.log("An error occurred:", error);
                });
        } else {
            console.log("No characteristic found to disconnect.");
        }
    } else {
        // Throw an error if Bluetooth is not connected
        console.error("Bluetooth is not connected.");
        window.alert("Bluetooth is not connected.")
    }
}

function getTime() {
    const currentTime = new Date();
    const options = { hour: '2-digit', minute: '2-digit', second: '2-digit', hour12: false };
    const formattedTime = currentTime.toLocaleTimeString([], options);
    return formattedTime;
}
