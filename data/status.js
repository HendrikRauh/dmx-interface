import { data } from "./load-data.js";
import { initWebSocket, registerCallback } from "./websocket.js";

const statusDialog = document.querySelector(".dialog-status");
const expandButton = document.querySelector(".expand-status");

expandButton.addEventListener("click", () => {
    statusDialog.showModal();
});

registerCallback("status", setStatus);
initWebSocket();

function setStatus(status) {
    setValue("model", status.chip.model);
    setValue("mac", formatMac(status.chip.mac));
    setValue("sdk-version", status.sdkVersion);

    setValue("rssi", status.connection.signalStrength);
    const icon = selectConnectionIcon(status.connection.signalStrength);
    document.querySelectorAll(".connection-icon").forEach((img) => {
        img.src = `/icons/${icon}`;
    });

    setValue("cpu-freq", status.chip.cpuFreqMHz);
    setValue("cpu-cycle-count", status.chip.cycleCount);
    setValue("cpu-temp", status.chip.tempC);

    const usedHeap = status.heap.total - status.heap.free;
    setValue("heap-used", formatBytes(usedHeap));
    setValue("heap-total", formatBytes(status.heap.total));
    setValue(
        "heap-percentage",
        Math.round((usedHeap / status.heap.total) * 100)
    );

    const usedPsram = status.psram.total - status.psram.free;
    setValue("psram-used", formatBytes(usedPsram));
    setValue("psram-total", formatBytes(status.psram.total));
    setValue(
        "psram-percentage",
        Math.round((usedPsram / status.psram.total) * 100)
    );

    setValue("uptime", parseDuration(status.uptime));

    setValue("hash", parseHash(status.sketch.md5));
}

function setValue(className, value) {
    document.querySelectorAll("." + className).forEach((element) => {
        element.innerText = value;
    });
}

function parseDuration(ms) {
    const date = new Date(ms);
    const time =
        date.getUTCHours().toString().padStart(2, "0") +
        ":" +
        date.getUTCMinutes().toString().padStart(2, "0") +
        " h";
    const days = Math.floor(date.getTime() / (1000 * 60 * 60 * 24));

    return days !== 0 ? `${days} Tage, ${time}` : time;
}

function parseHash(hash) {
    return hash.toUpperCase().substring(0, 16);
}

function formatBytes(bytes) {
    const units = ["Bytes", "KB", "MB", "GB"];

    let value = bytes;
    let index = 0;
    while (value >= 1000) {
        value /= 1000;
        index++;
    }

    return `${Math.round(value * 10) / 10} ${units[index]}`;
}

function formatMac(decimalMac) {
    const octets = decimalMac.toString(16).toUpperCase().match(/../g) || [];
    return octets.reverse().join(":");
}

function selectConnectionIcon(signalStrength) {
    // access point
    if (data.connection == 1) {
        return "hotspot.svg";
    }

    // ethernet
    if (data.connection == 2) {
        return "lan.svg";
    }

    // station
    if (signalStrength >= -50) {
        return "signal4.svg";
    }
    if (signalStrength >= -60) {
        return "signal3.svg";
    }
    if (signalStrength >= -70) {
        return "signal2.svg";
    }
    return "signal1.svg";
}
