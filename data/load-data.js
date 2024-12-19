import {
    showLoadingScreen,
    showError,
    hideLoadingScreen,
} from "./loading-screen.js";

const form = document.querySelector("form.config");

export async function loadData(timeout = null) {
    const req = await fetch("/config", {
        method: "GET",
        signal: timeout !== null ? AbortSignal.timeout(timeout) : undefined,
    });
    if (!req.ok) {
        throw new Error(`Response status: ${req.status}`);
    }

    const json = await req.json();
    console.log(json);
    return json;
}

export function writeDataToInput(data) {
    console.log("write data");
    for (const [key, value] of Object.entries(data)) {
        const element = document.querySelector(`[name=${key}]`);
        console.log(key, element);

        if (element.type === "checkbox") {
            element.checked = value;
        } else {
            element.value = value;
        }
    }
    // send "change" event
    form.dispatchEvent(new Event("change", { bubbles: true }));
}

showLoadingScreen("Konfiguration wird geladen...");
try {
    const data = await loadData();
    hideLoadingScreen();
    writeDataToInput(data);
} catch (error) {
    console.log(error.message);
    showError("Die Konfiguration konnte nicht geladen werden.");
}
