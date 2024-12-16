import { data } from "./load-data.js";

const networkDropdown = document.querySelector("#select-network");
const refreshButton = document.querySelector("#refresh-networks");
const refreshIcon = refreshButton.querySelector("img");

let isLoading = false;

refreshButton.addEventListener("click", async () => {
    // check if interface is connected via WiFi
    if (data.connection == 0 || data.connection == 1) {
        alert(
            "Beim WLAN-Scan wird die Verbindung hardwarebedingt kurzzeitig" +
                "unterbrochen.\n" +
                "Möglicherweise muss das Interface neu verbunden werden."
        );
    }
    updateNetworks();
});

// check if connected via WiFi-Station
if (data.connection === 0) {
    // show currently connected wifi
    insertNetworks([data.ssid]);
}

function insertNetworks(networks) {
    networkDropdown.textContent = ""; // clear dropdown

    for (const ssid of networks) {
        const option = document.createElement("option");
        option.value = ssid;
        option.innerText = ssid;
        networkDropdown.appendChild(option);
    }
}

async function loadNetworks() {
    if (isLoading) return;

    isLoading = true;
    refreshButton.classList.remove("error-bg");
    refreshIcon.classList.add("spinning");

    try {
        const res = await fetch("/networks", {
            method: "GET",
        });

        if (!res.ok) {
            throw Error(`response status: ${res.status}`);
        }

        const networks = await res.json();

        refreshIcon.classList.remove("spinning");
        isLoading = false;
        // remove duplicate values
        return Array.from(new Set(networks));
    } catch (e) {
        refreshIcon.classList.remove("spinning");
        refreshButton.classList.add("error-bg");
        isLoading = false;
        return [];
    }
}

async function updateNetworks() {
    const networks = await loadNetworks();
    if (networks) {
        insertNetworks(["", ...networks], true);
    }
}
