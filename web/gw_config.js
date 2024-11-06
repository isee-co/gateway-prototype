// DOM Elements
const bleConnect = document.getElementById('ble_connect');
const bleDisconnect = document.getElementById('ble_disconnect');
const bleState = document.getElementById('ble_state');
const wifiSsid = document.getElementById('wifi_ssid');
const wifiPassword = document.getElementById('wifi_password');
const wifiSave = document.getElementById('wifi_save');
const homeId = document.getElementById('home_id');
const homeIdSave = document.getElementById('home_id_save');
const mqttServer = document.getElementById('mqtt_server');
const mqttPort = document.getElementById('mqtt_port');
const mqttUsername = document.getElementById('mqtt_username');
const mqttPassword = document.getElementById('mqtt_password');
const mqttSave = document.getElementById('mqtt_save');
const textLogs = document.getElementById('logs');

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
bleConnect.addEventListener('click', (event) => {
    if (isWebBluetoothEnabled()) {
        connectToDevice();
    }
});

// Disconnect Button
bleDisconnect.addEventListener('click', disconnectDevice);

// wifi config Save
wifiSave.addEventListener('click', (event) => {
    const config = { ssid: wifiSsid.value, pass: wifiPassword.value, }
    const request = { id: 123, method: 'SetWifiConfig', payload: config }
    sendRpcRequest(JSON.stringify(request));
});

// Home ID Save
homeIdSave.addEventListener('click', (event) => {
    const request = { id: 123, method: 'SetHomeId', payload: homeId.value }
    sendRpcRequest(JSON.stringify(request));
});

// mqtt config Save
mqttSave.addEventListener('click', (event) => {
    const config = {
        server: mqttServer.value,
        port: mqttPort.value,
        username: mqttUsername.value,
        password: mqttPassword.value
    }
    const request = { id: 123, method: 'SetMqttConfig', payload: config }
    sendRpcRequest(JSON.stringify(request));
});

// Check if BLE is available in your Browser
async function isWebBluetoothEnabled() {
    if (!navigator.bluetooth) {
        console.log('Web Bluetooth API is not available in this browser!');
        bleState.innerHTML = "Web Bluetooth API is not available in this browser/device!";
        return false
    }
    var availability = await navigator.bluetooth.getAvailability();
    if (!availability) {
        console.log('Web Bluetooth adapter is not available in this browser!');
        bleState.innerHTML = "Web Bluetooth adapter is not available in this browser/device!";
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
            bleConnect.style.display = "none";
            bleDisconnect.style.display = "inline-block";
            bleState.innerHTML = 'Connected to device ' + device.name;
            bleState.style.color = "#24af37";
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
            characteristic.startNotifications();
        })
        .catch(error => {
            console.log('Error: ', error);
        })
}

function onDisconnected(event) {
    bleConnect.style.display = "inline-block";
    bleDisconnect.style.display = "none";
    console.log('Device Disconnected:', event.target.device.name);
    bleState.innerHTML = "Device disconnected";
    bleState.style.color = "#d13a30";

    connectToDevice();
}

function handleCharNotification(event) {
    const newValueReceived = new TextDecoder().decode(event.target.value);
    const source = event.target.uuid == bleRpcChar.uuid ? "RpcRes" : "Event";
    setTimeout(() => {
        textLogs.innerHTML += `${getTime()} ${source}:  ${newValueReceived} \n`;
    }, 100);
}

function sendRpcRequest(req) {
    if (bleServer && bleServer.connected) {
        var enc = new TextEncoder();
        const data = enc.encode(req);
        bleRpcChar.writeValue(data).then(() => {
            textLogs.innerHTML += `${getTime()} RpcReq:  ${req} \n`;
        }).catch(error => {
            textLogs.innerHTML += `${getTime()} Error:  ${error} \n`;
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
