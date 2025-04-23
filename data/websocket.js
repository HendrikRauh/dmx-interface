const gateway = `ws://${window.location.hostname}/ws`;
let ws;

let callbacks = {};

export function initWebSocket() {
    if (ws) return;

    ws = new WebSocket(gateway);

    ws.onopen = () => {
        console.info("WebSocket connection opened");
    };

    ws.onclose = (event) => {
        console.info("WebSocket connection closed, reason:", event.reason);
        ws = null;
    };

    ws.onerror = (event) => {
        console.warn("WebSocket encountered error, closing socket.", event);
        ws.close();
        ws = null;
    };

    ws.onmessage = (event) => {
        const message = JSON.parse(event.data);
        console.log("received websocket data", message);
        if (message.type in callbacks) {
            callbacks[message.type](message.data);
        }
    };
}

export function registerCallback(type, callback) {
    callbacks[type] = callback;
}
